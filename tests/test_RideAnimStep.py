"""Differential tests of the ride frame callback, all-channel reset, hoardings and stats.
The file-record builders, time controls and fused-rounding hook are copied from RideAnimTick;
the two-header geometry builder extends the matching RideAnimChange builders. SetupAnimationFlags
and ResetAnimTimeStats run as translated code on the host and original code on the PPC.
AnimateRideSet and the three untranslated helpers record every argument on both sides.
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_RideAnimStep"

MASK = 0xFFFFFFFF
TPGLOBALS = 0x103B74C8        # +0x400c engine flags, +0x4010 clock, +0x4014 step, +0x4018 fixed clock
NAN_BITS = 0x7FC00000


# ------------------------------------------------------------------ small helpers


def ws(e, address, *words):
    """Writes 32-bit words, big-endian, to the emulator's memory."""
    e.write(address, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def fb(x):
    """The bit pattern of a single-precision value (a NaN as one pattern)."""
    if x != x:
        return NAN_BITS
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bf(bits):
    """The single-precision value of a bit pattern."""
    return struct.unpack(">f", struct.pack(">I", bits & MASK))[0]


def canon(bits):
    """A word with every NaN pattern made one (the processors differ in the NaNs they make)."""
    bits &= MASK
    return NAN_BITS if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def f32(x):
    return T.f32(x)


class Globals:
    """Keeps the original's globals a case writes and puts them back when it is over."""

    def __init__(self, e):
        self.e = e
        self.kept = []

    def keep(self, address, size):
        self.kept.append((address, self.e.read(address, size)))

    def close(self):
        for address, data in reversed(self.kept):
            self.e.write(address, data)


LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Unicorn (QEMU 5.0) computes fmadds, fmsubs, fnmadds and fnmsubs in double precision and
    rounds the result to single, which can differ by one unit in the last place from the
    processor's single rounding of the exact result. A code hook looks at each instruction
    before it runs and rewrites its target register just before the next one, with the C
    library's fmaf (one rounding). ranges: the (begin, end) address ranges of the functions
    that use fused instructions (AnimateNode, DoWorldTransformsForEmbeddedData, Combine...)."""

    changed = 0    # results the correction replaced (all cases so far), for the notes
    seen = 0       # fused instructions it looked at

    def __init__(self, e, ranges):
        self.e, self.pending, self.hooks, self.ranges = e, None, [], ranges

    def __enter__(self):
        for begin, end in self.ranges:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            target, value = self.pending
            self.pending = None
            corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
            if fpr(e, target) != corrected and not (value != value and fpr_double(e, target) != fpr_double(e, target)):
                HardwareRounding.changed += 1
            uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        HardwareRounding.seen += 1
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        for x in (a, c, b):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        subtract = kind in (28, 30)
        value = float(LIBM.fmaf(a, c, -b if subtract else b))
        if kind in (30, 31):
            value = -value
        self.pending = (rd, value)


def function_range(e, address):
    """(begin, end) of the function at `address` in the program's function table."""
    for f in e.functions:
        if f["entry"] == address:
            return address, address + f["size"]
    raise KeyError(hex(address))


# ------------------------------------------------------------------ the two sides' memory


class Ref:
    """A pointer field's value: the address of the object `name` (None: null)."""

    def __init__(self, name, offset=0):
        self.name, self.offset = name, offset


class Obj:
    """An object of the case: a size, a label for the call log (-1: the next free one) and the
    words to write. sub(offset, label): a label for a place inside it (the records of an array)."""

    def __init__(self, size, label=-1):
        self.size, self.label, self.writes, self.subs = size, label, [], []

    def w(self, offset, fmt, *values):
        self.writes.append((offset, fmt, values))
        return self

    def sub(self, offset, label=-1):
        self.subs.append((offset, label))
        return self


AUTO_LABEL = 100    # the first of the labels that objects get without asking for one


class PpcSide:
    """The PowerPC's memory: absolute addresses, big-endian words."""

    def __init__(self, e, log):
        self.e, self.log, self.addr = e, log, {}

    def new(self, name, size, label):
        self.addr[name] = self.e.alloc(size, b"\xa5" * size)
        if label:
            self.log.labels[self.addr[name]] = label

    def absolute(self, name):
        return self.addr[name] if name is not None else 0

    pointer = absolute

    def name_place(self, name, offset, label):
        self.log.labels[self.addr[name] + offset] = label

    def w(self, name, offset, fmt, *values):
        self.e.write(self.addr[name] + offset, struct.pack(">" + fmt, *values))

    def r(self, name, offset, fmt):
        return struct.unpack(">" + fmt, self.e.read(self.addr[name] + offset, struct.calcsize(">" + fmt)))

    def label(self, field):
        return self.log.lab(field) & MASK


class HostSide:
    """The host's memory: arena blocks, little-endian words, Ptr32 offsets from the arena's base."""

    def __init__(self, lib):
        self.lib, self.addr = lib, {}
        lib.tpw_ras_base.restype = C.c_uint64
        lib.tpw_ras_alloc.restype = C.c_uint64
        lib.tpw_ras_alloc.argtypes = [C.c_uint32]
        lib.tpw_cl_lab.argtypes = [C.c_void_p]
        lib.tpw_cl_lab.restype = C.c_int32
        lib.tpw_ras_alloc(16)
        self.base = lib.tpw_ras_base()

    def new(self, name, size, label):
        a = self.lib.tpw_ras_alloc(size)
        C.memset(a, 0xA5, size)
        self.addr[name] = a
        if label:
            self.lib.tpw_cl_label(label, C.c_void_p(a))

    def absolute(self, name):
        return self.addr[name] if name is not None else 0

    def pointer(self, name):
        return self.addr[name] - self.base if name is not None else 0

    def name_place(self, name, offset, label):
        self.lib.tpw_cl_label(label, C.c_void_p(self.addr[name] + offset))

    def w(self, name, offset, fmt, *values):
        data = struct.pack("<" + fmt, *values)
        C.memmove(self.addr[name] + offset, data, len(data))

    def r(self, name, offset, fmt):
        return struct.unpack("<" + fmt, C.string_at(self.addr[name] + offset, struct.calcsize("<" + fmt)))

    def label(self, field):
        return (self.lib.tpw_cl_lab(C.c_void_p(self.base + field)) if field else 0) & MASK


def build(side, objs):
    """Allocates the objects (poisoned with 0xa5, labelled) and writes their fields."""
    auto = AUTO_LABEL
    for name, o in objs.items():
        label = o.label
        if label == -1:
            label, auto = auto, auto + 1
        side.new(name, o.size, label)
        for offset, sub_label in o.subs:
            if sub_label == -1:
                sub_label, auto = auto, auto + 1
            side.name_place(name, offset, sub_label)
    for name, o in objs.items():
        for offset, fmt, values in o.writes:
            side.w(name, offset, fmt, *[(side.pointer(v.name) + v.offset if v.name is not None else 0) if isinstance(v, Ref) else (0 if v is None else v) for v in values])


def dump(side, items):
    """The words of the objects that a function may have changed. items: (name, offset, kind,
    count) with kind I (32-bit words), H (16-bit), F (floats: NaNs equal, signed zeros not) or P
    (pointer fields, as labels)."""
    out = []
    for name, offset, kind, n in items:
        if kind == "I":
            out += list(side.r(name, offset, "%dI" % n))
        elif kind == "H":
            out += list(side.r(name, offset, "%dH" % n))
        elif kind == "F":
            out += [float("nan") if v != v else v for v in side.r(name, offset, "%df" % n)]
        elif kind == "P":
            out += [side.label(v) for v in side.r(name, offset, "%dI" % n)]
        else:
            raise ValueError(kind)
    return out


CONTROL_DUMP = [(0, "I", 3), (0xC, "F", 1), (0x10, "I", 3), (0x1C, "F", 2), (0x24, "I", 3), (0x30, "F", 1), (0x34, "P", 1)]


def control_dump(name):
    """All fourteen words of a time control (floats as floats, the animation it applied as a label)."""
    return [(name, offset, kind, n) for offset, kind, n in CONTROL_DUMP]



CONTROLS = 4
ENTRIES = 3
VERTEX_BLOCKS = 3
UV_BLOCKS = 3
SCRATCH_WORDS = 8


def node_record(r=None, flags=0, f50=0, matrix=None, vcount=0, f5e=0, verts=None, uvs=None, box=None):
    """A full node (0xa0 bytes): flags, its own number, the matrix, vertex and texture coordinate counts,
    the vertices (36 floats: three sets of four) and texture coordinates (24 floats), the box."""
    return {"flags": flags, "f50": f50, "matrix": matrix or [float(i) for i in range(16)], "vcount": vcount,
            "f5e": f5e, "verts": verts, "uvs": uvs, "box": box or [float(i) for i in range(6)]}


def mini_record(flags=0, f50=0, matrix=None):
    return {"flags": flags, "f50": f50, "matrix": matrix or [float(i) for i in range(16)]}


def header_record(flags=0, nodes=None, minis=None, textures=None):
    return {"flags": flags, "nodes": nodes if nodes is not None else [node_record()], "minis": minis or [],
            "textures": textures or []}


def anim_record(flags=0, f04=0, f08=30, nas=None, disable=None, has_disable=True):
    """An animation header: flags, the frames it runs (f04 to f08), its node animations (dicts with flags,
    the number f14 of the node they move and the node record they point at: ("n", k) or ("m", k) of the
    set's header) and the numbers of the nodes it disables."""
    return {"flags": flags, "f04": f04, "f08": f08, "nas": nas or [], "disable": disable or [],
            "has_disable": has_disable}


def na_record(flags=0, f14=0, node=("n", 0)):
    return {"flags": flags, "f14": f14, "node": node}


def change_control_words(flags=0, kind=0xC, index=0, rate=1.0, start=0, current=0, last=0, length=0.0, frame=0.0,
                         qkind=0xC, qindex=0, qflags=0, qspeed=0.0):
    """The fourteen words of a time control (floats as bit patterns); the last, the animation applied
    last, is left as poison."""
    return [flags & MASK, kind & MASK, index & MASK, fb(rate), start & MASK, current & MASK, last & MASK,
            fb(length), fb(frame), qkind & MASK, qindex & MASK, qflags & MASK, fb(qspeed), 0xA5A5A5A5]


def group_record(flags=3, count=ENTRIES, entries=None):
    return (flags, count, entries if entries is not None else [0] * ENTRIES)


def world_record(hdr=None, thdr=None, anims=None, groups=None, controls=None, set_flags=0, time_count=CONTROLS,
                 f14=0xA5A5A5A5, mesh_flags=0, type_mesh_flags=0, matrix=None, scratch=None, globals_=(0, 1000, 2000)):
    """One case's world. groups: twelve (flags, count, entries) with entries animation numbers."""
    hdr = hdr or header_record()
    return {"hdr": hdr, "thdr": thdr or header_record(nodes=[node_record(matrix=[float(-i) for i in range(16)])
                                                              for _ in hdr["nodes"]],
                                                       minis=[mini_record() for _ in hdr["minis"]]),
            "anims": anims if anims is not None else [anim_record()],
            "groups": groups or [group_record() for _ in range(12)],
            "controls": controls or [change_control_words() for _ in range(CONTROLS)], "set_flags": set_flags & MASK,
            "time_count": time_count, "f14": f14, "mesh_flags": mesh_flags & MASK,
            "type_mesh_flags": type_mesh_flags & MASK, "matrix": matrix,
            "scratch": scratch or [0xA5A5A5A5] * SCRATCH_WORDS, "globals": globals_}


def header_objs(objs, tag, h):
    """The header, its nodes (an array), mini nodes (an array), textures and the vertex and texture
    coordinate blocks of each node."""
    nodes, minis, textures = h["nodes"], h["minis"], h["textures"]
    header = Obj(0xB8)
    header.w(0x30, "I", h["flags"]).w(0x36, "H", len(textures)).w(0x42, "H", len(nodes) + len(minis))
    header.w(0x44, "H", len(nodes)).w(0x50, "I", Ref(None)).w(0x70, "I", Ref(None)).w(0x74, "I", Ref(None))
    if textures:
        records = Obj(8 * len(textures))
        for i, (first, second) in enumerate(textures):
            records.w(8 * i, "HH", first, second)
        objs[tag + ".tex"] = records
        header.w(0x50, "I", Ref(tag + ".tex"))
    if nodes:
        array = Obj(0xA0 * len(nodes))
        for k, n in enumerate(nodes):
            base = 0xA0 * k
            array.w(base, "I", n["flags"]).w(base + 0x10, "16f", *n["matrix"]).w(base + 0x50, "H", n["f50"])
            array.w(base + 0x58, "H", n["vcount"]).w(base + 0x5E, "H", n["f5e"]).w(base + 0x78, "6f", *n["box"])
            if n["verts"] is not None:
                objs["%s.v%d" % (tag, k)] = Obj(0x30 * VERTEX_BLOCKS).w(0, "%df" % (12 * VERTEX_BLOCKS), *n["verts"])
            array.w(base + 0x60, "I", Ref("%s.v%d" % (tag, k) if n["verts"] is not None else None))
            if n["uvs"] is not None:
                objs["%s.u%d" % (tag, k)] = Obj(0x20 * UV_BLOCKS).w(0, "%df" % (8 * UV_BLOCKS), *n["uvs"])
            array.w(base + 0x68, "I", Ref("%s.u%d" % (tag, k) if n["uvs"] is not None else None))
        objs[tag + ".nodes"] = array
        header.w(0x70, "I", Ref(tag + ".nodes"))
    if minis:
        array = Obj(0x58 * len(minis))
        for k, n in enumerate(minis):
            array.w(0x58 * k, "I", n["flags"]).w(0x58 * k + 0x10, "16f", *n["matrix"]).w(0x58 * k + 0x50, "H", n["f50"])
        objs[tag + ".minis"] = array
        header.w(0x74, "I", Ref(tag + ".minis"))
    objs[tag] = header


def node_ref(spec):
    """The Ref to a node record of the set's header: ("n", k) a full node, ("m", k) a mini node."""
    kind, k = spec
    return Ref("H.nodes", 0xA0 * k) if kind == "n" else Ref("H.minis", 0x58 * k)


def anim_objs(objs, i, a):
    """One animation header with its node animations (an array of 0x40 byte records) and its list of
    nodes to disable."""
    anim = Obj(0x48)
    anim.w(0, "I", a["flags"]).w(4, "I", a["f04"] & MASK).w(8, "I", a["f08"] & MASK).w(0x12, "H", len(a["nas"]))
    anim.w(0x1A, "H", len(a["disable"])).w(0x2C, "I", Ref(None)).w(0x38, "I", Ref(None))
    if a["nas"]:
        array = Obj(0x40 * len(a["nas"]))
        for k, na in enumerate(a["nas"]):
            array.w(0x40 * k + 4, "I", na["flags"]).w(0x40 * k + 0x14, "H", na["f14"])
            array.w(0x40 * k + 0x3C, "I", node_ref(na["node"]))
        objs["A%d.nas" % i] = array
        anim.w(0x2C, "I", Ref("A%d.nas" % i))
    if a["has_disable"]:
        listing = Obj(2 * max(len(a["disable"]), 1))
        if a["disable"]:
            listing.w(0, "%dH" % len(a["disable"]), *a["disable"])
        objs["A%d.dis" % i] = listing
        anim.w(0x38, "I", Ref("A%d.dis" % i))
    objs["A%d" % i] = anim


def world_objs(w):
    objs = {}
    header_objs(objs, "H", w["hdr"])
    header_objs(objs, "T", w["thdr"])
    for i, a in enumerate(w["anims"]):
        anim_objs(objs, i, a)
    controls = Obj(0x38 * CONTROLS)
    for c, words in enumerate(w["controls"]):
        controls.w(0x38 * c, "13I", *words[:13])
        controls.w(0x38 * c + 0x34, "I", Ref("A%d" % words[13]) if words[13] is not None else None)
    objs["controls"] = controls
    objs["scratch"] = Obj(4 * SCRATCH_WORDS).w(0, "%dI" % SCRATCH_WORDS, *w["scratch"])
    return objs


def world_numbers(w, side):
    """The list the host's BuildWorld reads (see tests/unit_RideAnimChange.cpp)."""
    n = [w["set_flags"], w["time_count"], w["f14"], w["mesh_flags"], w["type_mesh_flags"], side.absolute("H"),
         side.absolute("T"), side.absolute("controls"), side.absolute("scratch"), 1 if w["matrix"] else 0]
    n += [fb(x) for x in (w["matrix"] or [0.0] * 16)]
    for flags, count, entries in w["groups"]:
        n += [flags & MASK, count & MASK]
        n += [side.absolute("A%d" % a) if a is not None else 0 for a in entries]
    return n


def ppc_set(e, side, w):
    """Lays the set, the type set, their meshes, the entries of the groups and the matrix out as the Mac
    does; the addresses come back for the arguments and the results."""
    block = b"\xa5"
    set_, type_set = e.alloc(0x100, block * 0x100), e.alloc(0x100, block * 0x100)
    mesh, type_mesh, matrix = e.alloc(0x40, block * 0x40), e.alloc(0x40, block * 0x40), e.alloc(0x40, block * 0x40)
    side.log.labels.update({set_: 10, type_set: 11, mesh: 20, type_mesh: 21, matrix: 30})
    ws(e, set_, type_set, w["set_flags"], mesh)
    e.write(set_ + 0xE, struct.pack(">H", w["time_count"]))
    ws(e, set_ + 0x10, side.addr["controls"], w["f14"])
    for k, (flags, count, entries) in enumerate(w["groups"]):
        records = e.alloc(8 * ENTRIES, block * (8 * ENTRIES))
        ws(e, set_ + 0x18 + 12 * k, flags, count, records)
        for j, a in enumerate(entries):
            ws(e, records + 8 * j, j, side.addr["A%d" % a] if a is not None else 0)
    ws(e, set_ + 0xC8, matrix if w["matrix"] else 0)
    if w["matrix"]:
        ws(e, matrix, *[fb(x) for x in w["matrix"]])
    ws(e, type_set + 8, type_mesh)
    ws(e, mesh, w["mesh_flags"], side.addr["H"])
    ws(e, type_mesh, w["type_mesh_flags"], side.addr["T"])
    ws(e, mesh + 0x28, side.addr["H"] if w["embedded"] else 0)
    ws(e, set_ + 0xB0, side.addr["H"] if w["hoarding_mesh"] else 0,
       side.addr["T"] if w["hoarding_state"] else 0, fb(w["level"]), fb(w["speed"]))
    ws(e, side.addr["H"] + 0xB4, set_ if w["present"] else 0)
    return {"set": set_, "mesh": mesh, "type_mesh": type_mesh}


CONTROL_DUMP = [(0, "I", 3), (0xC, "F", 1), (0x10, "I", 3), (0x1C, "F", 2), (0x24, "I", 3), (0x30, "F", 1), (0x34, "P", 1)]


def world_dump_items(w):
    """Every word of the world a set function may change: the scratch buffer, the controls, both headers'
    flags, every node and mini node (flags, matrix, box), the vertices and texture coordinates, the textures."""
    items = [("scratch", 0, "I", SCRATCH_WORDS)]
    for c in range(CONTROLS):
        items += [("controls", 0x38 * c + offset, kind, n) for offset, kind, n in CONTROL_DUMP]
    for tag, h in (("H", w["hdr"]), ("T", w["thdr"])):
        items.append((tag, 0x30, "I", 1))
        for k, n in enumerate(h["nodes"]):
            base = 0xA0 * k
            items += [(tag + ".nodes", base, "I", 1), (tag + ".nodes", base + 0x10, "F", 16),
                      (tag + ".nodes", base + 0x78, "F", 6)]
            if n["verts"] is not None:
                items.append(("%s.v%d" % (tag, k), 0, "F", 12 * VERTEX_BLOCKS))
            if n["uvs"] is not None:
                items.append(("%s.u%d" % (tag, k), 0, "F", 8 * UV_BLOCKS))
        for k in range(len(h["minis"])):
            items += [(tag + ".minis", 0x58 * k, "I", 1), (tag + ".minis", 0x58 * k + 0x10, "F", 16)]
        for i in range(len(h["textures"])):
            items.append((tag + ".tex", 8 * i, "H", 2))
    return items



def rfloat(r):
    """A float for a matrix, vertex or box: mostly moderate, some zeros, infinities and a quiet NaN."""
    k = r.random()
    if k < 0.85:
        return f32(r.uniform(-100.0, 100.0))
    return r.choice([0.0, -0.0, 1.0, -1.0, 0.5, math.inf, -math.inf, math.nan, 1e-30, 3e38])


def rand_node(r):
    return node_record(
        flags=r.choice([0, 0x10, 0x80000000, 0x80000010, 0x40000, 0xFFFFFFFF, r.getrandbits(32)]),
        f50=r.randrange(0, 8), matrix=[rfloat(r) for _ in range(16)], vcount=r.choice([0, 1, 3, 4, 5, 8, 11, 12]),
        f5e=r.choice([0, 1, 4, 5, 8, 12]),
        verts=[rfloat(r) for _ in range(12 * VERTEX_BLOCKS)] if r.random() > 0.15 else None,
        uvs=[rfloat(r) for _ in range(8 * UV_BLOCKS)] if r.random() > 0.15 else None,
        box=[rfloat(r) for _ in range(6)])


def rand_header(r, nodes, minis):
    return header_record(
        flags=r.choice([0, 4, 0x80000, 0x80004, 0xFFF7FFFF, r.getrandbits(32)]),
        nodes=[rand_node(r) for _ in range(nodes)],
        minis=[mini_record(r.choice([0, 0x10, 0x80000000, 0x80000010, r.getrandbits(32)]), r.randrange(0, 8),
                           [rfloat(r) for _ in range(16)]) for _ in range(minis)],
        textures=[(r.choice([0, 0x40, 0x400, 0x440, r.getrandbits(16)]), r.choice([0, 1, 0xFFFF, r.getrandbits(16)]))
                  for _ in range(r.randrange(0, 4))])


NA_FLAGS = [0, 1, 8, 0x80, 0x200, 0x289, 0x1000, 0x10000, 0x11000, 0x20000, 0x1289, 0x11289]


def rand_anim(r, nodes, minis):
    """An animation of a set with `nodes` full nodes and `minis` mini nodes. A node animation that moves a mini
    node never has the flags that read past a mini node's 0x58 bytes (0x1000, 0x10000)."""
    count = nodes + minis
    nas = []
    for _ in range(r.choice([0, 1, 1, 2, 3])):
        f14 = r.randrange(count)
        flags = r.choice(NA_FLAGS + [r.getrandbits(32)])
        if f14 >= nodes:
            flags &= ~0x11000
        target = r.randrange(count)
        nas.append(na_record(flags, f14, ("n", target) if target < nodes else ("m", target - nodes)))
    start = r.choice([0, 0, 5, 10])
    has_disable = r.random() > 0.15
    return anim_record(flags=r.choice([0, 2, 2, 3, 0xFFFFFFFF, r.getrandbits(32)]), f04=start,
                       f08=start + r.choice([0, 1, 30, 100, 300, 7]) if r.random() > 0.05 else start - 3,
                       nas=nas, disable=[r.randrange(count) for _ in range(r.choice([0, 1, 2]))] if has_disable else [],
                       has_disable=has_disable)


def rand_control(r):
    length = r.choice([0.0, 5.0, 10.0, 30.0, 100.0, f32(r.uniform(0, 50)), math.nan])
    base = 0.0 if math.isnan(length) else length
    return change_control_words(
        flags=r.choice([0, 0, 1, 2, 4, 6, 8, 0x10, 0x14, 0x20, 0x40, 0x41, 0x5F, r.getrandbits(32)]),
        kind=r.choice([0xC, 0xC, 0, 1, 2, 3, 5, 11, r.randrange(12)]), index=r.randrange(ENTRIES),
        rate=r.choice([1.0, 0.5, 2.0, f32(r.uniform(0.1, 3)), 0.0, -1.0, math.nan]),
        start=r.choice([0, 500, 999, 1000, 1001, 2000, r.getrandbits(32)]),
        current=r.choice([0, 1000, 3000, r.getrandbits(32)]), last=r.choice([0, 1000, r.getrandbits(32)]),
        length=length, frame=r.choice([0.0, base, base / 2, base + 1, f32(r.uniform(0, 120)), math.nan, T.rfloat(r)]),
        qkind=r.choice([0xC, 0xC, 0, 1, 5]), qindex=r.randrange(ENTRIES),
        qflags=r.choice([0, 1, 2, 4, r.getrandbits(32)]), qspeed=r.choice([0.0, 1.0, -0.0, 2.5, T.rfloat(r)]))


def rand_world(r, small=False):
    nodes, minis = (1, 0) if small else (r.randrange(1, 5), r.randrange(0, 3))
    anims = [rand_anim(r, nodes, minis) for _ in range(1 if small else r.randrange(1, 4))]
    groups = [(r.choice([3, 3, 1, 2, 0, r.getrandbits(32)]), r.choice([0, 1, 2, 3, 3]),
               [r.randrange(len(anims)) for _ in range(ENTRIES)]) for _ in range(12)]
    return world_record(
        hdr=rand_header(r, nodes, minis), thdr=rand_header(r, nodes, minis), anims=anims, groups=groups,
        controls=[rand_control(r) for _ in range(CONTROLS)],
        set_flags=r.choice([0, 0, 8, 0x8000, 0x1000000, 0x100000, 0x100008, 0x1008, 0x8008, r.getrandbits(32)]),
        time_count=r.choice([0, 1, 2, 3, 4]), f14=r.choice([0xA5A5A5A5, 0, 1, 5]),
        mesh_flags=r.choice([0, 1, 4, 5, r.getrandbits(32)]), type_mesh_flags=r.choice([0, 1, r.getrandbits(32)]),
        matrix=[rfloat(r) for _ in range(16)] if r.random() > 0.2 else None,
        scratch=[r.choice([0, 0xA5A5A5A5, r.getrandbits(32)]) for _ in range(SCRATCH_WORDS)],
        globals_=(r.choice([0, 1, 8, 9, 0x10, 0x11, r.getrandbits(32)]),
                  r.choice([0, 1000, 2000, 5000, r.getrandbits(32)]), r.choice([0, 1000, 5000, r.getrandbits(32)])))




CALLBACK, PROCESS_STEP, RESET_ALL, HOARDINGS, STATS = 0x10069A98, 0x1005B24C, 0x100C54EC, 0x100619D4, 0x100C7930
ENTRIES_BY_OP = [CALLBACK, PROCESS_STEP, RESET_ALL, HOARDINGS, STATS]


def make_stubs():
    """Record every argument of the external helpers. The reset and SetupAnimationFlags remain real."""
    stubs = CL.Stubs()
    stubs.real.update(ENTRIES_BY_OP + [0x100C5E18, 0x10237CD8])

    def animate(e):
        set_ = e.r(3)
        e.cl.rec("AnimateRideSet", [e.cl.lab(set_), T.s32(e.r(4)), e.r(5), e.u32(set_ + 4)])
        ws(e, set_ + 4, (e.u32(set_ + 4) & e.cl.ret("AnimateAnd", MASK)) | e.cl.ret("AnimateOr", 0))

    def heads(e):
        e.cl.rec("TickRideHeads", [e.cl.lab(e.u32(e.r(3))), e.cl.lab(e.r(3))])

    def enable(e):
        e.cl.rec("EnableRideHoardings", [e.cl.lab(e.r(3)), T.s32(e.r(4)), e.u32(e.r(3) + 4)])

    def level(e):
        e.cl.rec("SetRideHoardingLevel", [e.cl.lab(e.r(3)), fb(e.f(1)), e.u32(e.r(3) + 4)])

    stubs.internal(0x100C74D4, animate)
    stubs.internal(0x10200460, heads)
    stubs.internal(0x10061060, enable)
    stubs.internal(0x10061268, level)
    return stubs


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    runner.emu.u16 = lambda a: struct.unpack(">H", runner.emu.read(a, 2))[0]
    STUBS = make_stubs()


def case_world(w=None, operation=2, animate=1, present=True, embedded=False, hoarding_mesh=True,
               hoarding_state=True, level=0.5, speed=0.1, step=0.25, selected=0, outputs=7, queues=None):
    """A single operation on the common two-mesh world, with independent hoarding gates."""
    w = w or world_record()
    for control in w["controls"]:
        if control[13] == 0xA5A5A5A5:
            control[13] = None
    w.update(operation=operation, animate=animate, present=present, embedded=embedded,
             hoarding_mesh=hoarding_mesh, hoarding_state=hoarding_state, level=level, speed=speed,
             step=step, selected=selected, outputs=outputs)
    return {"world": w, "objs": world_objs(w), "queues": queues or {}}


def ppc_run(e, case):
    """Call an original function and compare all written state, optional outputs and call order."""
    w = case["world"]
    kept = Globals(e)
    kept.keep(TPGLOBALS + 0x400C, 0x10)
    kept.keep(0x102C7990, 4)
    saved = STUBS.activate(e)
    try:
        e.cl = CL.PpcLog({}, case["queues"])
        side = PpcSide(e, e.cl)
        build(side, case["objs"])
        records = ppc_set(e, side, w)
        e.cl.labels[records["set"] + 8] = 40
        ws(e, TPGLOBALS + 0x400C, w["globals"][0], w["globals"][1], fb(w["step"]), w["globals"][2])
        ws(e, 0x102C7990, side.addr["scratch"])
        outputs = e.alloc(12, struct.pack(">III", 0x12345678, 0x23456789, 0x3456789A))
        operation = w["operation"]
        if operation == 0:
            args = [side.addr["H"]]
        elif operation == 1:
            args = [records["set"], w["animate"]]
        elif operation == 3:
            args = [records["set"], T.F(w["step"])]
        elif operation == 4:
            args = [records["set"]] + [outputs + 4 * i if w["outputs"] & (1 << i) else 0 for i in range(3)]
            args.append(w["selected"])
        else:
            args = [records["set"]]
        with HardwareRounding(e, [(HOARDINGS, HOARDINGS + 216)]):
            answer = T.call_ppc(e, ENTRIES_BY_OP[operation], args)[0]
        set_ = records["set"]
        result = [answer if operation == 4 else 0, e.u32(set_ + 4), e.u32(set_ + 0x14),
                  e.u32(records["mesh"]), e.u32(records["type_mesh"])]
        result += [e.u32(TPGLOBALS + offset) for offset in (0x400C, 0x4010, 0x4018)]
        result += [canon(e.u32(set_ + 0xB8)), canon(e.u32(set_ + 0xBC))]
        result += list(struct.unpack(">3I", e.read(outputs, 12)))
        result += [canon(e.u32(TPGLOBALS + 0x4014))]
        return {"result": result, "state": dump(side, world_dump_items(w)), "log": e.cl.log}
    finally:
        CL.Stubs.deactivate(e, saved)
        kept.close()


def host_run(lib, case):
    """Build the same file records and native objects, then report the same state."""
    lib.tpw_ras_reset()
    CL.host_begin(lib, {}, case["queues"])
    side = HostSide(lib)
    build(side, case["objs"])
    w = case["world"]
    numbers = world_numbers(w, side)
    numbers += [w["operation"], w["animate"], w["present"], w["embedded"], w["hoarding_mesh"], w["hoarding_state"],
                fb(w["level"]), fb(w["speed"]), fb(w["step"]), *w["globals"], w["selected"], w["outputs"]]
    data = (C.c_int64 * len(numbers))(*numbers)
    out = (C.c_int64 * 14)()
    lib.tpw_ras_run.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_ras_run(data, out)
    result = list(out)
    for i in (8, 9, 13):
        result[i] = canon(result[i])
    return {"result": result, "state": dump(side, world_dump_items(w)), "log": CL.host_end(lib)}


def reset_world(mask=0x11289, set_flags=0, header_flags=0, engine_flags=0, control_flags=4, kind=0,
                group_flags=3, vcount=5, f5e=5, vertices=True, source_vertices=True, uvs=True, source_uvs=True):
    """Two full nodes and one mini node, different source geometry and every texture gate."""
    h = header_record(flags=header_flags, nodes=[node_record(flags=0xF40010, vcount=vcount, f5e=f5e,
                      verts=[float(i + 1) for i in range(36)] if vertices else None,
                      uvs=[float(i + 2) for i in range(24)] if uvs else None), node_record(flags=0x80F40010)],
                      minis=[mini_record(flags=0xF40010)], textures=[(0, 1), (0x40, 0), (0x40, 1), (0x440, 7)])
    t = header_record(nodes=[node_record(matrix=[float(-i - 1) for i in range(16)], vcount=8, f5e=8,
                      verts=[float(-i - 3) for i in range(36)] if source_vertices else None,
                      uvs=[float(-i - 4) for i in range(24)] if source_uvs else None,
                      box=[float(-i - 5) for i in range(6)]), node_record()], minis=[mini_record(matrix=[float(-i - 1) for i in range(16)])])
    old = anim_record(flags=2, nas=[na_record(mask, 0), na_record(0x289, 2, ("m", 0))], disable=[1, 2])
    other = anim_record()
    control = change_control_words(flags=control_flags, kind=kind)
    control[13] = 0
    controls = [control] + [change_control_words() for _ in range(CONTROLS - 1)]
    return world_record(hdr=h, thdr=t, anims=[old, other], controls=controls, set_flags=set_flags,
                        groups=[group_record(flags=group_flags, entries=[1, 1, 1]) for _ in range(12)],
                        time_count=1, f14=1, globals_=(engine_flags, 1000, 2000))


def reset_edges():
    """Each reset mask, pointer gate, count boundary, flag gate, and cross-control ordering."""
    cases = []
    for mask in (0, 1, 8, 0x80, 0x200, 0x1000, 0x10000, 0x11289):
        for count in (0, 1, 3, 4, 5, 8):
            for gates in range(16):
                w = reset_world(mask=mask, vcount=count, f5e=count, vertices=bool(gates & 1),
                                source_vertices=bool(gates & 2), uvs=bool(gates & 4), source_uvs=bool(gates & 8))
                cases.append(case_world(w))
    for sf in (0, 8, 0x100000, 0x100008):
        for hf in (0, 4):
            for ef in (0, 1, 8, 9):
                for cf in (0, 4, 0x20, 0x24):
                    cases.append(case_world(reset_world(set_flags=sf, header_flags=hf, engine_flags=ef, control_flags=cf)))
    for kind in (0, 1, 11, 12):
        for gf in (0, 1, 2, 3, 4):
            cases.append(case_world(reset_world(kind=kind, group_flags=gf)))
    # triage 2026-10-13a (doubtful 3): three active controls on the same node, each with its own group entry, in all six orders
    import itertools
    for order in itertools.permutations(range(3)):
        for count in (2, 3):
            w = reset_world()
            w["anims"] += [anim_record(flags=2, nas=[na_record(0x1000, 0)]), anim_record(nas=[na_record(0x10000, 0)]),
                           anim_record(flags=2, nas=[na_record(0x289, 0)]), anim_record(nas=[na_record(0x11000, 0)])]
            w["groups"][1] = group_record(entries=[3, 3, 3])
            w["groups"][2] = group_record(entries=[5, 5, 5])
            second = change_control_words(flags=4, kind=1)
            second[13] = 2
            third = change_control_words(flags=4, kind=2)
            third[13] = 4
            first = w["controls"][0]
            w["controls"][0], w["controls"][1], w["controls"][2] = [(first, second, third)[i] for i in order]
            w["time_count"] = count
            cases.append(case_world(w))
    # Source and working headers select full versus mini nodes independently.
    w = reset_world(mask=0x289)
    w["thdr"] = header_record(nodes=[node_record(matrix=[float(-i - 7) for i in range(16)]) for _ in range(3)])
    cases.append(case_world(w))
    w = reset_world(mask=0x289)
    w["thdr"] = header_record(nodes=[node_record()], minis=[mini_record(), mini_record()])
    cases.append(case_world(w))
    # Later assignment replaces an earlier mask; reverse order changes the result.
    for reverse in (False, True):
        for count in range(5):
            w = reset_world()
            w["anims"] += [anim_record(flags=2, nas=[na_record(0x1000, 0)]),
                           anim_record(nas=[na_record(0x10000, 0)])]
            w["groups"][1] = group_record(entries=[3, 3, 3])
            second = change_control_words(flags=4, kind=1)
            second[13] = 2
            w["controls"][1] = second
            if reverse:
                w["controls"][0], w["controls"][1] = w["controls"][1], w["controls"][0]
            w["time_count"] = count
            cases.append(case_world(w))
    return cases


def reset_random(r):
    w = rand_world(r)
    for control in w["controls"]:
        control[13] = r.choice([None] + list(range(len(w["anims"]))))
    return case_world(w)


def process_edges(operation=1):
    cases = []
    for animate in (0, 1, -1, 2):
        for flags in (0, 0x800000, 0x880000, 0x3000, 0x1000, 0x2000, 0x20, 0x60, 0xA0,
                      0x806020, 0x803000, 0x980000,
                      # triage 2026-10-13a: the neighbouring bits of the 0x800000 gate and of the 0x80000, 0x3000 and 0x20 tests
                      0x1, 0x80001, 0x880001, 0x2, 0x400000, 0x1000000, 0x7FFFFF, 0xFF7FFFFF, 0xFFFFFFFF, 0x4000,
                      0x800001, 0x800002, 0x1000, 0x21):
            for active in (0, 1):
                for embedded in (False, True):
                    w = reset_world(set_flags=flags)
                    w["f14"] = active
                    cases.append(case_world(w, operation=operation, animate=animate, embedded=embedded))
    if operation == 0:
        cases.append(case_world(reset_world(), operation=0, present=False))
    # The external animation changes flags before the hoarding gate is read.
    # triage 2026-10-13a (doubtful 1): the external animation clears 0x200000 (or sets it); the callback's OR comes after the call
    for sf in (0, 0x20, 0x200000, 0x220000):
        for or_flags, and_flags in ((0, MASK ^ 0x200000), (0x200000, MASK ^ 0x200000), (0, MASK ^ 0x20 ^ 0x200000)):
            cases.append(case_world(reset_world(set_flags=sf), operation=operation,
                                    queues={"AnimateOr": [or_flags], "AnimateAnd": [and_flags]}))
    for or_flags, and_flags in ((0x60, MASK), (0xA0, MASK), (0, MASK ^ 0x20)):
        cases.append(case_world(reset_world(set_flags=0x20), operation=operation,
                                queues={"AnimateOr": [or_flags], "AnimateAnd": [and_flags]}))
    return cases


def process_random(r, operation=1):
    case = reset_random(r)
    w = case["world"]
    w.update(operation=operation, animate=r.choice([0, 1, -1, 2]), embedded=r.choice([False, True]),
             present=r.random() > 0.1, hoarding_mesh=r.choice([False, True]), hoarding_state=r.choice([False, True]),
             level=f32(r.uniform(-0.5, 1.5)), speed=f32(r.uniform(-2, 2)), step=f32(r.uniform(-2, 2)))
    return case


def hoarding_edges():
    cases = []
    for mesh in (False, True):
        for state in (False, True):
            for flags in (0, 0x40, 0x80, 0xC0, 0xE0):
                for level in (-0.5, -0.0, 0.0, 0.5, 1.0, 1.5, math.nan, math.inf, -math.inf):
                    for speed, step in ((0, 1), (1, 0), (0.5, 1), (-0.5, 1), (1, 2), (-1, 2),
                                        (math.inf, 0), (math.nan, 1)):
                        cases.append(case_world(world_record(set_flags=flags), operation=3, hoarding_mesh=mesh,
                                                hoarding_state=state, level=level, speed=speed, step=step))
    # The fused result differs from a multiply followed by an add, inside the unclamped interval.
    cases.append(case_world(world_record(set_flags=0x40), operation=3, level=bf(0x3F000001),
                            speed=bf(0x3F000001), step=bf(0xBF7FFFFF)))
    return cases


def hoarding_random(r):
    return case_world(world_record(set_flags=r.choice([0, 0x40, 0x80, 0xC0, 0xE0, r.getrandbits(32)])),
                      operation=3, hoarding_mesh=r.random() > 0.1, hoarding_state=r.random() > 0.1,
                      level=rfloat(r), speed=rfloat(r), step=rfloat(r))


def stats_case(length, frame, flags=1, selected=0, outputs=7):
    controls = [change_control_words(flags=flags ^ i, kind=11 - i, index=0x12345678 + i,
                                    length=length, frame=frame) for i in range(CONTROLS)]
    return case_world(world_record(controls=controls), operation=4, selected=selected, outputs=outputs)


def stats_edges():
    cases = []
    values = [(0, 0), (1, 0), (0, 1), (0.03, 0), (0.029, 0), (30, 0), (100, 99.9),
              (64424508, 0), (64424512, 0), (128849008, 0), (128849016, 0), (128849024, 0),
              (math.inf, 0), (-math.inf, 0), (math.nan, 0), (math.inf, math.inf), (0, math.nan),
              (1e-30, 0), (0, -1), (1e10, -1e10), (1e20, 1e20), (1e20, 0)]
    for outputs in range(8):
        for selected in range(4):
            for length, frame in values:
                for flags in (0, 1):
                    cases.append(stats_case(length, frame, flags, selected, outputs))
    return cases


def stats_random(r):
    return stats_case(rfloat(r), rfloat(r), r.getrandbits(32), r.randrange(4), r.randrange(8))


TESTS = [
    FnTest("AnimateRideCallback", ppc_run, host_run, edge=lambda: process_edges(0), rnd=lambda r: process_random(r, 0)),
    FnTest("ProcessRideAnim", ppc_run, host_run, edge=process_edges, rnd=process_random),
    FnTest("ResetRideAnimationAllChannels", ppc_run, host_run, edge=reset_edges, rnd=reset_random),
    FnTest("HandleActiveHoardings", ppc_run, host_run, edge=hoarding_edges, rnd=hoarding_random),
    FnTest("GetCurrentAnimStats", ppc_run, host_run, edge=stats_edges, rnd=stats_random),
]
