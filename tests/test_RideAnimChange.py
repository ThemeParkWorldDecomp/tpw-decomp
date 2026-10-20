"""Differential test for src/RideAnimChange.cpp (IsValidAnimation, SetupAnimationFlags,
ResetRideAnimationSingleChannel, SetNewAnim, SetCurRideAnim, InterpolateCMorphData,
InterpolateCMorphBoundData and RADecompressMorph), against libtpw_unit_RideAnimChange.dylib.

The eight functions run as the original on the PowerPC side, and so does ResetAnimTimeStats (0x100c5a90,
src/RideAnimInit.cpp on the host), which SetNewAnim calls for the control's clock words. The one other
callee, the matrix form of EnableMeshDraw, is a fake on both sides that records its arguments and the
matrix it is given. tests/unit_RideAnimChange.cpp is the host's half, the Stubs below the PowerPC's.

A case is a world: a ride set (twelve groups of animation entries, up to three time controls), the type's
set that its mSelf names, the two meshes' headers with their nodes, mini nodes, vertices, texture
coordinates and textures, the animations (node animations, lists of nodes to disable), the scratch
geometry buffer and the engine globals. The mesh file's records have the file's layout on both sides, so
one description builds them for the two (Side classes: the PowerPC side writes big-endian words and
absolute addresses, the host little-endian words and arena offsets, as the Ptr32 fields hold them). The
ride set and the mesh have native pointers on the host and the Mac's layout on the PowerPC side; the host
builds them from a list of numbers that the PowerPC side lays out from the same world. After a call the
answer, the call log, the set's words and every word of the world the function could have changed are
compared (floats as floats: NaNs equal, signed zeros not).

The morph functions get a compressed morph (key data with frame tables, vertex tables and 10-bit fields
packed three to a word) and a node with three sets of four vertices and a bounding box.

Unicorn computes fmadds in double precision and rounds twice; HardwareRounding (one code hook per fused
instruction, as in test_RideAnimTick.py) gives each the processor's single rounding.
"""
import ctypes as C
import math
import pathlib
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_RideAnimChange"

MASK = 0xFFFFFFFF
TPGLOBALS = 0x103B74C8        # +0x400c engine flags, +0x4010 clock, +0x4018 fixed clock
SCRATCH_GLOBAL = 0x102C7990   # gScratchGeomBuffer: the pointer to the scratch geometry buffer

IS_VALID, SETUP_FLAGS, RESET_CHANNEL = 0x100C5D3C, 0x100C5E18, 0x100C5054
SET_NEW, SET_CUR = 0x100C60FC, 0x100C6670
INTERP_DATA, INTERP_BOUND, DECOMPRESS = 0x100C390C, 0x100C3CAC, 0x100C40DC
RESET_TIME = 0x100C5A90
ENABLE_MESH_MATRIX = 0x10065200
NAN_BITS = 0x7FC00000

ENTRIES, CONTROLS, SCRATCH_WORDS = 3, 3, 16   # entries per group, time controls built, scratch words built
VERTEX_BLOCKS, UV_BLOCKS = 3, 3               # sets of four vertices and of texture coordinates each node has


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
    that use fused instructions (the three morph functions)."""

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
    """A pointer field's value: the address of the object `name` plus `offset` (None name: null)."""

    def __init__(self, name, offset=0):
        self.name, self.offset = name, offset


class Obj:
    """An object of the case: a size, a label for the call log (0: none) and the words to write."""

    def __init__(self, size, label=0):
        self.size, self.label, self.writes = size, label, []

    def w(self, offset, fmt, *values):
        self.writes.append((offset, fmt, values))
        return self


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

    def pointer(self, ref):
        return self.addr[ref.name] + ref.offset if ref.name is not None else 0

    def w(self, name, offset, fmt, *values):
        self.e.write(self.addr[name] + offset, struct.pack(">" + fmt, *values))

    def r(self, name, offset, fmt):
        return struct.unpack(">" + fmt, self.e.read(self.addr[name] + offset, struct.calcsize(">" + fmt)))


class HostSide:
    """The host's memory: arena blocks, little-endian words, Ptr32 offsets from the arena's base."""

    def __init__(self, lib):
        self.lib, self.addr = lib, {}
        lib.tpw_rac_base.restype = C.c_uint64
        lib.tpw_rac_alloc.restype = C.c_uint64
        lib.tpw_rac_alloc.argtypes = [C.c_uint32]
        lib.tpw_cl_lab.argtypes = [C.c_void_p]
        lib.tpw_cl_lab.restype = C.c_int32
        lib.tpw_rac_alloc(16)
        self.base = lib.tpw_rac_base()

    def new(self, name, size, label):
        a = self.lib.tpw_rac_alloc(size)
        C.memset(a, 0xA5, size)
        self.addr[name] = a
        if label:
            self.lib.tpw_cl_label(label, C.c_void_p(a))

    def absolute(self, name):
        return self.addr[name] if name is not None else 0

    def pointer(self, ref):
        return self.addr[ref.name] + ref.offset - self.base if ref.name is not None else 0

    def w(self, name, offset, fmt, *values):
        data = struct.pack("<" + fmt, *values)
        C.memmove(self.addr[name] + offset, data, len(data))

    def r(self, name, offset, fmt):
        return struct.unpack("<" + fmt, C.string_at(self.addr[name] + offset, struct.calcsize("<" + fmt)))


def build(side, objs):
    """Allocates the objects (poisoned with 0xa5) and writes their fields."""
    for name, o in objs.items():
        side.new(name, o.size, o.label)
    for name, o in objs.items():
        for offset, fmt, values in o.writes:
            side.w(name, offset, fmt, *[side.pointer(v) if isinstance(v, Ref) else v for v in values])


def dump(side, items):
    """The words of the objects that a function may have changed. items: (name, offset, kind,
    count) with kind I (32-bit words), H (16-bit), F (floats: NaNs equal, signed zeros not)."""
    out = []
    for name, offset, kind, n in items:
        if kind == "I":
            out += list(side.r(name, offset, "%dI" % n))
        elif kind == "H":
            out += list(side.r(name, offset, "%dH" % n))
        elif kind == "F":
            out += [float("nan") if v != v else v for v in side.r(name, offset, "%df" % n)]
        else:
            raise ValueError(kind)
    return out


# ------------------------------------------------------------------ the PowerPC's fakes


def words_at(e, address, n):
    return list(struct.unpack(">" + "I" * n, e.read(address, 4 * n)))


def make_stubs():
    """The fake of the matrix form of EnableMeshDraw and the C library's modf, on the PowerPC side. The
    fake records the event the host's fake of the same name records."""
    S = CL.Stubs()
    S.real.update([IS_VALID, SETUP_FLAGS, RESET_CHANNEL, SET_NEW, SET_CUR, INTERP_DATA, INTERP_BOUND, DECOMPRESS,
                   RESET_TIME])

    def enable_mesh(e):
        mesh, matrix = e.r(3), e.r(4)
        e.cl.rec("EnableMeshDraw", [e.cl.lab(mesh) & MASK, e.cl.lab(matrix) & MASK, T.s32(e.r(5)), e.u32(mesh)])
        if matrix:
            words = [canon(w) for w in words_at(e, matrix, 16)]
            e.cl.rec("Matrix", words[:8])
            e.cl.rec("Matrix", words[8:])
        ws(e, mesh, e.u32(mesh) | 0x11)

    S.internal(ENABLE_MESH_MATRIX, enable_mesh)

    def modf(e):
        # RADecompressMorph takes the frame's integer part and does nothing with it.
        fraction, whole = math.modf(e.f(1))
        e.write(e.r(5), struct.pack(">d", whole))
        e.set_f1(fraction)

    S.imp("modf", modf)
    return S


STUBS = None


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    STUBS = make_stubs()


# ------------------------------------------------------------------ running a case on both sides


def ppc_run(e, case, entry, args, results, ranges=()):
    """Builds the case's objects, calls `entry` and returns what `results(side, r3)` gives with the
    call log."""
    g = Globals(e)
    g.keep(TPGLOBALS + 0x400C, 0x14)
    g.keep(SCRATCH_GLOBAL, 4)
    saved = STUBS.activate(e)
    try:
        e.cl = CL.PpcLog({}, case.get("queues"))
        side = PpcSide(e, e.cl)
        build(side, case["objs"])
        engine, f4010, f4018 = case["world"]["globals"] if "world" in case else (0, 0, 0)
        ws(e, TPGLOBALS + 0x400C, engine, f4010)
        ws(e, TPGLOBALS + 0x4018, f4018)
        if "scratch" in side.addr:
            ws(e, SCRATCH_GLOBAL, side.addr["scratch"])
        with HardwareRounding(e, ranges):
            r3 = T.call_ppc(e, entry, args(side))[0]
        return {"result": results(side, r3), "log": e.cl.log}
    finally:
        CL.Stubs.deactivate(e, saved)
        g.close()


def host_run(lib, case, function, numbers, results):
    """The host's half of ppc_run for the set functions: `numbers(side)` is the list the function reads."""
    lib.tpw_rac_reset()
    CL.host_begin(lib, {}, case.get("queues"))
    side = HostSide(lib)
    build(side, case["objs"])
    engine, f4010, f4018 = case["world"]["globals"]
    lib.tpw_rac_set_globals(engine, f4010, f4018)
    words = numbers(side)
    block = (C.c_int64 * len(words))(*words)
    out = (C.c_int64 * 8)()
    fn = getattr(lib, function)
    fn.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    fn.restype = C.c_int32
    fn(block, out)
    return {"result": results(side, out), "log": CL.host_end(lib)}


# ------------------------------------------------------------------ the world: a ride set and its meshes


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


def control_words(flags=0, kind=0xC, index=0, rate=1.0, start=0, current=0, last=0, length=0.0, frame=0.0,
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
            "controls": controls or [control_words() for _ in range(CONTROLS)], "set_flags": set_flags & MASK,
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
        controls.w(0x38 * c, "14I", *words)
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
    return {"set": set_, "mesh": mesh, "type_mesh": type_mesh}


CONTROL_DUMP = [(0, "I", 3), (0xC, "F", 1), (0x10, "I", 3), (0x1C, "F", 2), (0x24, "I", 3), (0x30, "F", 1), (0x34, "I", 1)]


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


def set_case(world, **args):
    return dict(args, world=world, objs=world_objs(world), queues={})


def ppc_set_function(e, case, entry, arguments, answered=True):
    """Runs a function that takes the world's set. arguments(side, holder) gives the call's arguments."""
    w = case["world"]
    holder = {}

    def args(side):
        holder.update(ppc_set(e, side, w))
        return arguments(side, holder)

    def results(side, r3):
        answer = r3 if answered and not case.get("via_void") else (MASK if case.get("via_void") else 0)
        return [answer & MASK, e.u32(holder["set"] + 4), e.u32(holder["set"] + 0x14), e.u32(holder["mesh"]),
                e.u32(holder["type_mesh"]), e.u32(TPGLOBALS + 0x400C), e.u32(TPGLOBALS + 0x4010),
                e.u32(TPGLOBALS + 0x4018)] + dump(side, world_dump_items(w))

    return ppc_run(e, case, entry, args, results)


def host_set_function(lib, case, function, numbers):
    w = case["world"]

    def results(side, out):
        return [out[i] & MASK for i in range(8)] + dump(side, world_dump_items(w))

    return host_run(lib, case, function, lambda side: world_numbers(w, side) + numbers(side), results)


# ------------------------------------------------------------------ IsValidAnimation


def ppc_is_valid(e, case):
    def arguments(side, holder):
        cell = e.alloc(4, b"\xa5" * 4)
        ws(e, cell, holder["set"])
        return [cell, case["kind"], case["index"]]

    return ppc_set_function(e, case, IS_VALID, arguments)


def host_is_valid(lib, case):
    return host_set_function(lib, case, "tpw_rac_is_valid", lambda side: [case["kind"], case["index"]])


# ------------------------------------------------------------------ SetupAnimationFlags


def anim_address(side, number):
    return side.absolute("A%d" % number) if number is not None else 0


def ppc_setup_flags(e, case):
    return ppc_set_function(e, case, SETUP_FLAGS,
                            lambda side, holder: [anim_address(side, case["anim"]), holder["mesh"]], answered=False)


def host_setup_flags(lib, case):
    return host_set_function(lib, case, "tpw_rac_setup_flags", lambda side: [anim_address(side, case["anim"])])


# ------------------------------------------------------------------ ResetRideAnimationSingleChannel


def ppc_reset_channel(e, case):
    return ppc_set_function(
        e, case, RESET_CHANNEL,
        lambda side, holder: [holder["set"], anim_address(side, case["anim"]), anim_address(side, case["other"])],
        answered=False)


def host_reset_channel(lib, case):
    return host_set_function(lib, case, "tpw_rac_reset_channel",
                             lambda side: [anim_address(side, case["anim"]), anim_address(side, case["other"])])


# ------------------------------------------------------------------ SetNewAnim and SetCurRideAnim


def ppc_set_new(e, case):
    return ppc_set_function(
        e, case, SET_NEW,
        lambda side, holder: [holder["set"], case["kind"], case["index"], side.addr["controls"] + 0x38 * case["control"],
                              case["flags"], T.F(case["speed"])])


def host_set_new(lib, case):
    return host_set_function(
        lib, case, "tpw_rac_set_new_anim",
        lambda side: [case["kind"], case["index"], side.absolute("controls") + 0x38 * case["control"], case["flags"],
                      fb(case["speed"]), int(bool(case.get("via_void")))])


def ppc_set_cur(e, case):
    return ppc_set_function(
        e, case, SET_CUR,
        lambda side, holder: [holder["set"], case["kind"], case["index"], case["flags"], T.F(case["speed"]),
                              case["control"]])


def host_set_cur(lib, case):
    return host_set_function(
        lib, case, "tpw_rac_set_cur_anim",
        lambda side: [case["kind"], case["index"], case["flags"], fb(case["speed"]), case["control"],
                      int(bool(case.get("via_void")))])


# ------------------------------------------------------------------ the morph functions


def pack_word(x, y, z, low=0):
    """A compressed vertex: three 10-bit fields at the top of a word (the two bits below are not read)."""
    return ((x & 0x3FF) << 22) | ((y & 0x3FF) << 12) | ((z & 0x3FF) << 2) | (low & 3)


def key_record(frames, n, indices, words, f10=0):
    """A key data: the frame numbers (a row of the vertex table for each), the vertex count n of a row, the
    table of vertex numbers (n 16-bit numbers: a set of four is the number shifted right by 2, the lane
    its low two bits), the vertex words (rows of n) and the current row."""
    return {"frames": frames, "n": n, "indices": indices, "words": words, "f10": f10}


def morph_record(flags=0, offset=(0.0, 0.0, 0.0), scale=(1.0, 1.0, 1.0), keys=None):
    return {"flags": flags, "offset": list(offset), "scale": list(scale), "keys": keys or []}


def morph_case(morph, node_flags=0, box=None, positions=None, frame=0.0, t=0.0, add=0, key=0, a=0):
    case = {"morph": morph, "node_flags": node_flags & MASK, "box": box or [float(i) for i in range(6)],
            "positions": positions or [float(i) / 4 for i in range(12 * VERTEX_BLOCKS)], "frame": frame, "t": t,
            "add": add, "key": key, "a": a, "queues": {}}
    case["objs"] = morph_objs(case)
    return case


PAD_ROWS = 3   # rows of poison after the last row of a key's vertex table (reads past the last frame land there)


def morph_objs(c):
    m = c["morph"]
    objs = {}
    morph = Obj(0x2C).w(0, "H", m["flags"]).w(2, "H", len(m["keys"])).w(0xC, "I", Ref("keys"))
    morph.w(0x14, "3f", *m["offset"]).w(0x20, "3f", *m["scale"])
    objs["morph"] = morph
    keys = Obj(0x14 * max(len(m["keys"]), 1))
    for i, k in enumerate(m["keys"]):
        base = 0x14 * i
        keys.w(base, "HH", len(k["frames"]), k["n"]).w(base + 4, "I", Ref("idx%d" % i))
        keys.w(base + 8, "I", Ref("fr%d" % i)).w(base + 0xC, "I", Ref("w%d" % i)).w(base + 0x10, "I", k["f10"])
        indices = Obj(2 * max(len(k["indices"]), 1))
        if k["indices"]:
            indices.w(0, "%dH" % len(k["indices"]), *k["indices"])
        objs["idx%d" % i] = indices
        frames = Obj(2 * (len(k["frames"]) + PAD_ROWS))
        if k["frames"]:
            frames.w(0, "%dH" % len(k["frames"]), *k["frames"])
        objs["fr%d" % i] = frames
        words = Obj(4 * (len(k["words"]) + PAD_ROWS * max(k["n"], 2)))
        if k["words"]:
            words.w(0, "%dI" % len(k["words"]), *k["words"])
        objs["w%d" % i] = words
    objs["keys"] = keys
    objs["pos"] = Obj(4 * len(c["positions"])).w(0, "%df" % len(c["positions"]), *c["positions"])
    objs["node"] = Obj(0xA0).w(0, "I", c["node_flags"]).w(0x60, "I", Ref("pos")).w(0x78, "6f", *c["box"])
    return objs


def morph_dump_items(c):
    items = [("node", 0, "I", 1), ("node", 0x78, "F", 6), ("pos", 0, "F", len(c["positions"]))]
    items += [("keys", 0x14 * i + 0x10, "I", 1) for i in range(len(c["morph"]["keys"]))]
    return items


MORPH_RANGES = [(0x100C390C, 0x100C390C + 832), (0x100C3CAC, 0x100C3CAC + 976), (0x100C40DC, 0x100C40DC + 1196)]


def ppc_morph(entry, arguments):
    """A runner of a morph function on the PowerPC: arguments(side, case) gives the call's arguments."""
    def run(e, case):
        return ppc_run(e, case, entry, lambda side: arguments(side, case),
                       lambda side, r3: dump(side, morph_dump_items(case)), MORPH_RANGES)
    return run


def host_morph(function, argtypes, arguments):
    """The host's half: arguments(side, case) gives the call's ctypes arguments."""
    def run(lib, case):
        lib.tpw_rac_reset()
        CL.host_begin(lib, {}, {})
        side = HostSide(lib)
        build(side, case["objs"])
        fn = getattr(lib, function)
        fn.argtypes = argtypes
        fn.restype = None
        fn(*arguments(side, case))
        return {"result": dump(side, morph_dump_items(case)), "log": CL.host_end(lib)}
    return run


def key_address(side, case):
    return side.addr["keys"] + 0x14 * case["key"]


MORPH_ARGTYPES = [C.c_uint64, C.c_uint64, C.c_uint64, C.c_uint32, C.c_int32]
ppc_interpolate_data = ppc_morph(
    INTERP_DATA, lambda s, c: [s.addr["morph"], s.addr["pos"], key_address(s, c), T.F(c["t"]), c["add"]])
host_interpolate_data = host_morph(
    "tpw_rac_interpolate_data", MORPH_ARGTYPES,
    lambda s, c: [s.addr["morph"], s.addr["pos"], key_address(s, c), fb(c["t"]), c["add"]])
ppc_interpolate_bound = ppc_morph(
    INTERP_BOUND, lambda s, c: [s.addr["morph"], s.addr["node"] + 0x78, key_address(s, c), T.F(c["t"]), c["add"]])
host_interpolate_bound = host_morph(
    "tpw_rac_interpolate_bound", MORPH_ARGTYPES,
    lambda s, c: [s.addr["morph"], s.addr["node"] + 0x78, key_address(s, c), fb(c["t"]), c["add"]])
ppc_decompress = ppc_morph(
    DECOMPRESS, lambda s, c: [s.addr["node"], s.addr["morph"], T.F(c["frame"]), c["a"], c["add"]])
host_decompress = host_morph(
    "tpw_rac_decompress", [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32, C.c_int32],
    lambda s, c: [s.addr["node"], s.addr["morph"], fb(c["frame"]), c["a"], c["add"]])


# ------------------------------------------------------------------ random worlds


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
                       nas=nas, disable=[r.randrange(count) for _ in range(r.choice([0, 1, 2, 4]))] if has_disable else [],
                       has_disable=has_disable)


def rand_control(r):
    length = r.choice([0.0, 5.0, 10.0, 30.0, 100.0, f32(r.uniform(0, 50)), math.nan])
    base = 0.0 if math.isnan(length) else length
    return control_words(
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
        time_count=r.choice([0, 1, 2, 3, 3]), f14=r.choice([0xA5A5A5A5, 0, 1, 5]),
        mesh_flags=r.choice([0, 1, 4, 5, r.getrandbits(32)]), type_mesh_flags=r.choice([0, 1, r.getrandbits(32)]),
        matrix=[rfloat(r) for _ in range(16)] if r.random() > 0.2 else None,
        scratch=[r.choice([0, 0xA5A5A5A5, r.getrandbits(32)]) for _ in range(SCRATCH_WORDS)],
        globals_=(r.choice([0, 1, 8, 9, 0x10, 0x11, r.getrandbits(32)]),
                  r.choice([0, 1000, 2000, 5000, r.getrandbits(32)]), r.choice([0, 1000, 5000, r.getrandbits(32)])))


def make_playing(r, w, number):
    """Makes control `number` run an animation that exists and has not run out (the queueing case)."""
    kinds = [k for k, (flags, count, _) in enumerate(w["groups"]) if flags & 3 and count > 0]
    if not kinds:
        return
    kind = r.choice(kinds)
    count = w["groups"][kind][1]
    length = r.choice([10.0, 30.0, 100.0, f32(r.uniform(5, 200))])
    w["controls"][number] = control_words(
        flags=r.choice([0, 1, 0x10, 0x40, 0x41, 0x20]), kind=kind, index=r.randrange(min(count, ENTRIES)),
        rate=r.choice([1.0, 0.5, 2.0]), start=r.choice([0, 1000]), current=r.choice([0, 1500]),
        last=r.getrandbits(32), length=length, frame=r.choice([0.0, length / 2, length, f32(r.uniform(0, length))]),
        qkind=r.choice([0xC, 0xC, 2]), qindex=r.randrange(ENTRIES), qflags=r.choice([0, 1]),
        qspeed=r.choice([0.0, 1.0]))


# ------------------------------------------------------------------ the set functions: cases


def is_valid_random(r):
    return set_case(rand_world(r, small=True), kind=r.choice(list(range(12)) + [0xC, 0xC]),
                    index=r.choice([0, 1, 2, 3, 4, 0xFFFFFFFF, 0x80000000, r.getrandbits(32)]))


def is_valid_edges():
    for flags in (0, 1, 2, 3, 0x80000000, 0x80000002, 0xFFFFFFFC):
        for count in (0, 1, 3, 0xFFFFFFFF, 0x80000000):
            world = world_record(groups=[group_record(flags, count) for _ in range(12)])
            for kind in (0, 5, 11, 0xC):
                for index in (0, 2, 3, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000):
                    yield set_case(world, kind=kind, index=index)


def setup_flags_random(r):
    w = rand_world(r)
    w["mesh_flags"] = r.choice([0, 4, 4, 5, 1, r.getrandbits(32)])
    return set_case(w, anim=r.randrange(len(w["anims"])))


def setup_flags_edges():
    nodes = [node_record(flags=f) for f in (0, 0x80000000, 0x10, 0x80000010)]
    minis = [mini_record(f, 4 + i) for i, f in enumerate((0, 0x80000000))]
    for mesh_flags in (0, 4):
        for disable in ([], [0], [1, 2], [3, 4, 5], [4, 5, 0, 5], [7], [2, 2]):
            for has_disable in (True, False):
                for na_flags in (0, 0x1000, 0x10000, 0x289, 0x1, 0x8, 0x80, 0x200, 0x11289):
                    anim = anim_record(nas=[na_record(na_flags, 0, ("n", 1)), na_record(0x1000, 1, ("m", 0))],
                                       disable=disable, has_disable=has_disable)
                    world = world_record(hdr=header_record(nodes=nodes, minis=minis),
                                         thdr=header_record(nodes=nodes, minis=minis), anims=[anim],
                                         mesh_flags=mesh_flags)
                    yield set_case(world, anim=0)


def reset_channel_random(r):
    w = rand_world(r)
    return set_case(w, anim=r.randrange(len(w["anims"])),
                    other=None if r.random() < 0.2 else r.randrange(len(w["anims"])))


def reset_channel_edges():
    textures = [(0x40, 5), (0, 7), (0x440, 9)]
    minis = [mini_record(0x10, 3), mini_record(0x80000010, 4)]
    type_minis = [mini_record(matrix=[-float(i) - 20 for i in range(16)]) for _ in range(2)]

    def pair(present_here, present_there, count):
        """A node of the set's mesh and the node of the type's mesh it is reset from, with (or without) vertices
        and texture coordinates, and `count` of each."""
        mine = node_record(flags=0x10, vcount=count, f5e=count, box=[1.0 + i for i in range(6)],
                           verts=[float(i) for i in range(36)] if present_here else None,
                           uvs=[float(50 + i) for i in range(24)] if present_here else None)
        there = node_record(matrix=[-float(i) for i in range(16)], vcount=count, f5e=count,
                            box=[7.0 + i for i in range(6)],
                            verts=[float(100 + i) for i in range(36)] if present_there else None,
                            uvs=[float(200 + i) for i in range(24)] if present_there else None)
        return mine, there

    for set_flags in (0, 8, 0x100000, 0x100008):
        for header_flags in (0, 4):
            for engine in (0, 1, 8, 9):
                for na_flags in (0x289, 0x1000, 0x10000, 0x11289, 0, 1):
                    pairs = [pair(True, True, 5), pair(True, True, 0), pair(False, True, 12)]
                    anim = anim_record(flags=2, nas=[na_record(na_flags, 0), na_record(0x289, 1), na_record(0x289, 3)],
                                       disable=[0, 1, 4])
                    other = anim_record(nas=[na_record(0x289, 3), na_record(0x1000, 0)])
                    world = world_record(
                        hdr=header_record(flags=header_flags, nodes=[m for m, _ in pairs], minis=minis,
                                          textures=textures),
                        thdr=header_record(nodes=[t for _, t in pairs], minis=type_minis), anims=[anim, other],
                        set_flags=set_flags, globals_=(engine, 0, 0))
                    for other_number in (None, 1):
                        yield set_case(world, anim=0, other=other_number)
    # the copies of the vertices and texture coordinates: each pair of (here, there) present or not, each count
    for here in (True, False):
        for there in (True, False):
            for count in (0, 1, 4, 5, 12):
                mine, other_node = pair(here, there, count)
                anim = anim_record(nas=[na_record(0x11000, 0)])
                world = world_record(hdr=header_record(nodes=[mine]), thdr=header_record(nodes=[other_node]),
                                     anims=[anim])
                yield set_case(world, anim=0, other=None)
    # (an animation with flag 2 and the textures; no node animations; no disabled list)
    for flags in (0, 2):
        pairs = [pair(True, True, 5), pair(True, True, 0), pair(False, True, 12)]
        world = world_record(hdr=header_record(nodes=[m for m, _ in pairs], minis=minis, textures=textures),
                             thdr=header_record(nodes=[t for _, t in pairs], minis=type_minis),
                             anims=[anim_record(flags=flags, has_disable=False), anim_record(disable=[0])])
        yield set_case(world, anim=0, other=None)
        yield set_case(world, anim=1, other=0)


SPEEDS = [1.0, 0.5, 2.0, 0.0, -1.0, math.inf, math.nan, -0.0, 1e-3]


def set_new_random(r):
    w = rand_world(r)
    control = r.randrange(CONTROLS)
    if r.random() < 0.5:
        make_playing(r, w, control)
    return set_case(w, kind=r.choice(list(range(12)) + [0xC, 0xD, 0xE, 0xC, 0xD, 0xE, r.randrange(12)]),
                    index=r.randrange(ENTRIES), control=control,
                    flags=r.choice([0, 1, 2, 3, 4, 5, 8, 9, 0xC, 0xF, r.getrandbits(32)]),
                    speed=r.choice(SPEEDS + [T.rfloat(r)]), via_void=r.random() < 0.1)


def set_cur_random(r):
    w = rand_world(r)
    control = r.randrange(CONTROLS)
    if r.random() < 0.55:
        make_playing(r, w, control)
    return set_case(w, kind=r.choice(list(range(12)) + [0xC, 0xD, 0xE, 0xC, r.randrange(12)]),
                    index=r.randrange(ENTRIES), control=control,
                    flags=r.choice([0, 1, 2, 3, 4, 5, 8, 9, 0xC, 0x10, r.getrandbits(32)]),
                    speed=r.choice(SPEEDS + [T.rfloat(r)]), via_void=r.random() < 0.1)


def change_world(set_flags=0, controls=None, mesh_flags=0, matrix=True, time_count=CONTROLS, globals_=(0, 1000, 2000),
                 header_flags=0):
    """A world for the animation changes: two animations of a mesh with two nodes and a mini node, the
    twelve groups on them (group 4 has no entries, group 5 has one, group 6 only flag 2), a type mesh to
    reset from."""
    nodes = [node_record(flags=0x10, vcount=5, f5e=5, verts=[float(i) for i in range(36)],
                         uvs=[float(i) for i in range(24)]) for _ in range(2)]
    type_nodes = [node_record(matrix=[-float(i) for i in range(16)], vcount=5, f5e=5,
                              verts=[float(100 + i) for i in range(36)], uvs=[float(200 + i) for i in range(24)])
                  for _ in range(2)]
    anims = [anim_record(flags=2, f04=0, f08=30, nas=[na_record(0x289, 0), na_record(0x11000, 1, ("n", 0))],
                         disable=[0, 2]),
             anim_record(f04=5, f08=105, nas=[na_record(0x1000, 1), na_record(0x8, 2, ("m", 0))], disable=[1])]
    groups = [group_record(3, 3, [0, 1, 0]) for _ in range(12)]
    groups[4] = group_record(0, 3, [0, 1, 0])
    groups[5] = group_record(1, 1, [1, 0, 1])
    groups[6] = group_record(2, 3, [1, 1, 0])
    return world_record(
        hdr=header_record(flags=header_flags, nodes=nodes, minis=[mini_record(0x10, 2)]),
        thdr=header_record(nodes=type_nodes, minis=[mini_record(matrix=[-float(i) - 20 for i in range(16)])]),
        anims=anims, groups=groups, controls=controls or [control_words() for _ in range(CONTROLS)],
        set_flags=set_flags, mesh_flags=mesh_flags, time_count=time_count,
        matrix=[float(i) for i in range(16)] if matrix else None, globals_=globals_)


def sample_controls():
    """Controls to start animations on: idle, playing, run past its length, at its length, with no length
    known, with a queued animation, held, with the clock flag, with a rate."""
    playing = dict(kind=1, index=0, length=30.0, frame=10.0, start=100, current=1100, last=1100)
    return [
        control_words(),
        control_words(**playing),
        control_words(**dict(playing, frame=40.0)),
        control_words(**dict(playing, frame=30.0)),
        control_words(**dict(playing, frame=math.nan)),
        control_words(**dict(playing, qkind=2, qindex=1, qflags=4, qspeed=2.0)),
        control_words(flags=2, **playing),
        control_words(flags=0x40, **dict(playing, kind=3)),
        control_words(flags=0x41, **dict(playing, kind=5, index=2, rate=0.5)),
        control_words(**dict(playing, kind=4)),
        control_words(**dict(playing, kind=6, index=1)),
        control_words(flags=4, **dict(playing, frame=40.0, rate=2.0)),
        control_words(flags=0x14, **playing),
    ]


def set_new_edges():
    for number, control in enumerate(sample_controls()):
        for set_flags in (0, 0x8000, 0x1000000, 0x1008000):
            controls = [control, control_words(kind=2, index=1, length=30.0, frame=3.0), control_words()]
            for time_count in (0, 3):
                world = change_world(set_flags=set_flags, controls=controls, time_count=time_count,
                                     header_flags=0x80000 if time_count == 0 else 0)
                for kind in (0, 1, 4, 5, 6, 0xC, 0xD, 0xE):
                    for index in (0, 2):
                        for flags in (0, 1, 4, 8, 0xC, 0xF):
                            yield set_case(world, kind=kind, index=index, control=0, flags=flags, speed=1.5)
    # the other controls, the speeds
    for speed in SPEEDS:
        world = change_world(controls=sample_controls()[1:4])
        for control in (0, 1, 2):
            yield set_case(world, kind=2, index=1, control=control, flags=0, speed=speed)
            yield set_case(dict(world), kind=2, index=1, control=control, flags=0, speed=speed, via_void=True)


def set_cur_edges():
    for number, control in enumerate(sample_controls()):
        for set_flags in (0, 8, 0x8000, 0x1008, 0x1000000):
            for mesh_flags in (0, 1):
                controls = [control, control_words(kind=2, index=1, length=30.0, frame=3.0), control_words()]
                world = change_world(set_flags=set_flags, controls=controls, mesh_flags=mesh_flags,
                                     matrix=number % 3 != 0)
                for kind in (0, 1, 4, 5, 6, 0xC, 0xD, 0xE):
                    for index in (0, 2):
                        for flags in (0, 1, 2, 3, 4, 8):
                            yield set_case(world, kind=kind, index=index, control=0, flags=flags, speed=1.5)
    for speed in SPEEDS:
        world = change_world(controls=sample_controls()[1:4])
        for control in (0, 1, 2):
            yield set_case(world, kind=2, index=1, control=control, flags=0, speed=speed)
            yield set_case(dict(world), kind=2, index=1, control=control, flags=0, speed=speed, via_void=True)


# ------------------------------------------------------------------ the morph functions: cases


def rand_field(r):
    return r.choice([0, -1, 1, -512, 511, r.randrange(-512, 512), r.randrange(-512, 512)])


def rand_word(r):
    return pack_word(rand_field(r), rand_field(r), rand_field(r), r.randrange(4))


def rand_frames(r, rows):
    frames = [r.choice([0, 0, 1, 5]) + r.randrange(0, 4)]
    for _ in range(rows - 1):
        frames.append(frames[-1] + r.choice([0, 1, 5, 10, 10, 30]))
    return frames


def rand_key(r, n, rows, f10=None):
    """A key data of `rows` frames and `n` vertices (numbers below the twelve a node's sets hold)."""
    frames = rand_frames(r, rows)
    return key_record(frames, n, [r.randrange(4 * VERTEX_BLOCKS) for _ in range(n)],
                      [rand_word(r) for _ in range(rows * n)],
                      f10=r.choice([0, 0, rows - 1, r.randrange(rows)]) if f10 is None else f10)


def rand_morph(r, moving=None, dup=None):
    """A morph: the bounds key (two words a row), the shared vertices key when flag 2 is set, then the
    keys that move."""
    dup = r.random() < 0.4 if dup is None else dup
    moving = r.choice([0, 1, 1, 2, 3]) if moving is None else moving
    keys = [rand_key(r, 2, r.randrange(2, 5))]
    if dup:
        keys.append(key_record([0, 1], r.randrange(1, 7), [], [], f10=r.choice([0, 1])))
        keys[-1]["indices"] = [r.randrange(4 * VERTEX_BLOCKS) for _ in range(keys[-1]["n"])]
        keys[-1]["words"] = [rand_word(r) for _ in range(keys[-1]["n"])]
    keys += [rand_key(r, r.randrange(1, 7), r.randrange(2, 5)) for _ in range(moving)]
    return morph_record(flags=(2 if dup else 0) | r.choice([0, 0, 1, 4, 0x8000]),
                        offset=[r.choice([0.0, f32(r.uniform(-5, 5)), f32(r.uniform(-100, 100))]) for _ in range(3)],
                        scale=[r.choice([1.0, 0.0, f32(r.uniform(0, 3)), f32(r.uniform(-3, 3)), 0.01]) for _ in range(3)],
                        keys=keys)


def last_frame(m):
    return max(max(k["frames"]) for k in m["keys"])


def rand_frame_for(r, m):
    top = last_frame(m)
    return r.choice([f32(r.uniform(-3, top + 3)), f32(r.uniform(0, top)), float(r.choice(m["keys"])["frames"][0]),
                     float(r.choice(m["keys"])["frames"][-1]), float(r.randrange(0, top + 2)), 0.0, math.nan,
                     f32(r.uniform(0, 30000))])


def random_node_state(r):
    return dict(node_flags=r.choice([0, 0, 0x800000, 0x10000, 0x810000, r.getrandbits(32)]),
                box=[rfloat(r) for _ in range(6)], positions=[rfloat(r) for _ in range(12 * VERTEX_BLOCKS)])


def decompress_random(r):
    m = rand_morph(r)
    return morph_case(m, frame=rand_frame_for(r, m), add=r.choice([0, 0, 1]), a=r.getrandbits(32), **random_node_state(r))


def interpolate_random(r):
    m = rand_morph(r)
    return morph_case(m, t=r.choice([0.0, 1.0, 0.5, f32(r.uniform(0, 1)), f32(r.uniform(-2, 3)), T.rfloat(r), math.nan,
                                     math.inf]),
                      add=r.choice([0, 1]), key=r.randrange(len(m["keys"])), **random_node_state(r))


def decompress_edges():
    for dup in (False, True):
        for moving in (0, 1, 2):
            keys = [key_record([0, 10, 30], 2, [0, 0], [pack_word(-5, 7, 9), pack_word(5, -7, 3), pack_word(1, 2, 3),
                                                          pack_word(4, 5, 6), pack_word(-1, -2, -3), pack_word(9, 8, 7)])]
            if dup:
                keys.append(key_record([0, 1], 3, [0, 5, 11], [pack_word(1, 1, 1), pack_word(-2, 3, 4), pack_word(511, -512, 0)]))
            for _ in range(moving):
                keys.append(key_record([0, 10, 30], 3, [1, 4, 6], [pack_word(i, -i, 2 * i) for i in range(9)], f10=0))
            m = morph_record(flags=2 if dup else 0, offset=(0.5, -0.25, 1.0), scale=(0.125, 0.5, 2.0), keys=keys)
            for node_flags in (0, 0x800000, 0x10000):
                for add in (0, 1):
                    for frame in (-4.0, 0.0, 5.0, 10.0, 12.5, 29.0, 30.0, 31.0, 40.0, math.nan):
                        yield morph_case(m, node_flags=node_flags, add=add, frame=frame)


def interpolate_edges():
    key = key_record([0, 10, 30], 4, [0, 1, 5, 11], [pack_word(i - 3, 2 - i, i * 7 - 40) for i in range(12)], f10=0)
    bound = key_record([0, 10, 30], 2, [0, 0], [pack_word(-5, 7, 9), pack_word(5, -7, 3), pack_word(1, 2, 3),
                                                  pack_word(4, 5, 6), pack_word(-1, -2, -3), pack_word(9, 8, 7)])
    m = morph_record(offset=(0.5, -0.25, 1.0), scale=(0.125, 0.5, 2.0), keys=[bound, key])
    for t in (0.0, 1.0, 0.25, 0.75, -1.0, 2.0, math.nan, math.inf, -math.inf, -0.0):
        for add in (0, 1):
            for number in (0, 1):
                for f10 in (0, 1, 2):
                    mm = morph_record(offset=m["offset"], scale=m["scale"],
                                      keys=[dict(k, f10=f10) for k in m["keys"]])
                    yield morph_case(mm, t=t, add=add, key=number)


# ------------------------------------------------------------------ real morphs

ROOT = pathlib.Path(__file__).resolve().parents[2]       # theme-park-world-mac (this folder's worktree is a sibling of decomp)
WADS = ROOT / "extracted" / "wads"
REAL_MORPH_FILES = [
    "lobby.wad.d/terrain/Bfly_YELLm1.MD2", "lobby.wad.d/terrain/Fan_gateM2.MD2", "lobby.wad.d/terrain/Batm1.MD2",
    "lobby.wad.d/terrain/Jun_isleM1.MD2", "lobby.wad.d/terrain/Fan_gateM1.MD2",
    "levels/hallow/rides/haunt.wad.d/hauntB.MD2", "levels/hallow/rides/haunt.wad.d/hauntR.MD2",
    "levels/hallow/rides/ghostshp.wad.d/ghostshpm5.MD2", "levels/hallow/rides/ghostshp.wad.d/ghostshpi.MD2",
    "levels/hallow/rides/ghostshp.wad.d/ghostshpb2.MD2", "levels/hallow/rides/ghostshp.wad.d/ghostshpc.MD2",
    "levels/hallow/rides/ghostshp.wad.d/ghostshpl1.MD2",
]


def read_morph(data, offset):
    """The morph set at a file offset of a mesh (little-endian) as a morph_record, the vertex words rewritten
    the way the loader does (src/m3d2_load.cpp: the file's three 10-bit fields to the top of the word)."""
    flags, count = struct.unpack_from("<HH", data, offset)
    keys_at = struct.unpack_from("<I", data, offset + 0xC)[0]
    offset_xyz = struct.unpack_from("<3f", data, offset + 0x14)
    scale_xyz = struct.unpack_from("<3f", data, offset + 0x20)
    keys = []
    for i in range(count):
        a, n, idx_at, frames_at, words_at_, f10 = struct.unpack_from("<HHIIII", data, keys_at + 0x14 * i)
        words = []
        for w in struct.unpack_from("<%dI" % (a * n), data, words_at_):
            words.append(pack_word(w & 0x3FF, (w >> 10) & 0x3FF, (w >> 20) & 0x3FF))
        keys.append(key_record(list(struct.unpack_from("<%dH" % a, data, frames_at)), n,
                               list(struct.unpack_from("<%dH" % n, data, idx_at)), words, f10=f10))
    return morph_record(flags=flags, offset=offset_xyz, scale=scale_xyz, keys=keys)


def real_morphs():
    """(file, node animation, morph) for the compressed morphs of the real meshes named above (the first two
    of each file that has them); a file that is not there is skipped."""
    out = []
    for name in REAL_MORPH_FILES:
        path = WADS / name
        if not path.exists():
            print("test_RideAnimChange: %s is not there; its morphs are skipped" % path)
            continue
        data = path.read_bytes()
        anim = struct.unpack_from("<I", data, 0x98)[0]
        count = struct.unpack_from("<H", data, anim + 0x12)[0]
        nas = struct.unpack_from("<I", data, anim + 0x2C)[0]
        found = 0
        for i in range(count):
            flags = struct.unpack_from("<I", data, nas + 0x40 * i + 4)[0]
            morph = struct.unpack_from("<I", data, nas + 0x40 * i + 0x28)[0]
            if morph and flags & 0x1000 and not flags & 0x4000 and found < 2:
                out.append((name, i, read_morph(data, morph)))
                found += 1
    return out


def script_frames(m):
    """Frames across the morph's frame range: from the first frame to the last and a little past it."""
    low = min(k["frames"][0] for k in m["keys"][:1])
    high = max(max(k["frames"]) for k in m["keys"])
    frames = [float(low)] + [f32(low + (high - low) * i / 6.0) for i in range(1, 7)] + [float(high), float(high) + 0.5]
    return frames


def script_case(m, add, positions=None):
    blocks = max(max(k["indices"] or [0]) for k in m["keys"]) // 4 + 1
    case = morph_case(m, positions=positions or [f32(0.25 * i) for i in range(12 * blocks)], add=add)
    case["script"] = [(f, add) for f in script_frames(m)]
    return case


REAL = []   # the real morphs, read once


def real_cache():
    if not REAL:
        REAL.extend(real_morphs())
    return REAL


def decompress_script_edges():
    for name, number, m in real_cache():
        for add in (0, 1):
            yield script_case(m, add)


def decompress_script_random(r):
    """A script of random frames over a real morph (or, with no real file, over a made-up one)."""
    m = r.choice(real_cache())[2] if real_cache() else rand_morph(r)
    case = script_case(m, r.choice([0, 0, 1]))
    top = last_frame(m)
    case["script"] = [(r.choice([f32(r.uniform(-3, top + 3)), f32(r.uniform(0, top)), float(r.randrange(0, top + 2))]),
                       case["add"]) for _ in range(r.randrange(1, 6))]
    return case


def ppc_decompress_script(e, case):
    """A script of calls on one node and morph; the state is read back after each."""
    saved = STUBS.activate(e)
    try:
        e.cl = CL.PpcLog({}, {})
        side = PpcSide(e, e.cl)
        build(side, case["objs"])
        steps = []
        with HardwareRounding(e, MORPH_RANGES):
            for frame, add in case["script"]:
                T.call_ppc(e, DECOMPRESS, [side.addr["node"], side.addr["morph"], T.F(frame), 0, add])
                steps.append(dump(side, morph_dump_items(case)))
        return {"result": steps, "log": e.cl.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host_decompress_script(lib, case):
    lib.tpw_rac_reset()
    CL.host_begin(lib, {}, {})
    side = HostSide(lib)
    build(side, case["objs"])
    fn = lib.tpw_rac_decompress
    fn.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32, C.c_int32]
    fn.restype = None
    steps = []
    for frame, add in case["script"]:
        fn(side.addr["node"], side.addr["morph"], fb(frame), 0, add)
        steps.append(dump(side, morph_dump_items(case)))
    return {"result": steps, "log": CL.host_end(lib)}


# ------------------------------------------------------------------ the tests


TESTS = [
    FnTest("IsValidAnimation", ppc_is_valid, host_is_valid, edge=is_valid_edges, rnd=is_valid_random),
    FnTest("SetupAnimationFlags", ppc_setup_flags, host_setup_flags, edge=setup_flags_edges, rnd=setup_flags_random),
    FnTest("ResetRideAnimationSingleChannel", ppc_reset_channel, host_reset_channel, edge=reset_channel_edges,
           rnd=reset_channel_random),
    FnTest("SetNewAnim", ppc_set_new, host_set_new, edge=set_new_edges, rnd=set_new_random),
    FnTest("SetCurRideAnim", ppc_set_cur, host_set_cur, edge=set_cur_edges, rnd=set_cur_random),
    FnTest("InterpolateCMorphData", ppc_interpolate_data, host_interpolate_data, edge=interpolate_edges,
           rnd=interpolate_random),
    FnTest("InterpolateCMorphBoundData", ppc_interpolate_bound, host_interpolate_bound, edge=interpolate_edges,
           rnd=interpolate_random),
    FnTest("RADecompressMorph", ppc_decompress, host_decompress, edge=decompress_edges, rnd=decompress_random),
    FnTest("RADecompressMorph (real morphs)", ppc_decompress_script, host_decompress_script,
           edge=decompress_script_edges, rnd=decompress_script_random),
]
