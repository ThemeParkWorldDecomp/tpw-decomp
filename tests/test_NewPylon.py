"""Unit v5091, the pylon records and the pylon stack's meshes, against libtpw_unit_NewPylon.dylib (tests/unit_NewPylon.cpp), main app
("SimTheme Park"), src/TPCoaster_loaders_NewPylon.cpp:
  NewPylon 0x1003d610, UpdatePylonStackMeshes 0x1003d958

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers) for the translation; the same code reads both back. The
callees outside the unit are fakes on both sides that record their arguments: MReleaseAllocF (the log shows the Mac's 0x48 for the host
record's size), GetRideRequiredYOffset (answers a float from the queue "Y"), 0x10237cd8 (a float to an integer, answers the queue "Cell"),
FlagCollisionUpdateTrackAroundBrace, AnimateRideSet and DoMeshSkirtDeform.

The world of NewPylon: the editor's coaster (its count, list head, tail and preview slot), the height field's size and row stride, the point or
none, the flags. What is compared: the new record field by field (pointers as object names), the coaster's four words, the old tail's next
pointer, the call log. The world of UpdatePylonStackMeshes: the pylon with up to 5 braces, up to 3 ride sets (mesh, header, node and mini
node arrays, root node, head table), the pylon control's flags and node indices. What is compared: every node and mini node word of every
set, the written node words, the root words, the call log.

No fused operations. Not tested: a NULL set, mesh or header (read unconditionally), a brace index past 2, a node index past the arrays (the
tests keep the index below 8 = the arrays' size), a NULL coaster (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_NewPylon"
MASK = 0xFFFFFFFF

FN_NEW, FN_UPD = 0x1003D610, 0x1003D958
SIZES = {FN_NEW: 500, FN_UPD: 1416}
OWN = set(SIZES)

CALLEE_ALLOC = 0x10064BD4
CALLEE_Y = 0x101ED29C
CALLEE_CELL = 0x10237CD8
CALLEE_FLAG = 0x10040FA4
CALLEE_ANIM = 0x100C74D4
CALLEE_DEFORM = 0x1006E930

ADDR = dict(inst=0x102B68B0, hh=0x102C6C64, hw=0x102C6C68, hdr=0x102C6C7C, sets=0x102C79A4, skip=0x102B68A0)

NAMES = ["set_heads", "mesh_header", "mesh_f28", "mesh_root", "tab_entries", "ent_size", "ent_rec", "rec_y", "br_flags", "br_f50",
         "br_w70", "br_data", "data_f20", "lp_control", "lp_y", "lp_count", "lp_braces", "ctl_fc4", "ctl_f18", "ctl_f1c", "ctl_f20",
         "ctl_f24", "ctl_f28", "hdr_nodecount", "hdr_f46", "hdr_hfield", "hdr_nodes", "hdr_mini", "hdr_f78", "hf_f18", "inst_f40",
         "inst_pylons", "inst_tail", "inst_preview", "pr_number", "pr_flags", "pr_index", "pr_cellx", "pr_cellz", "pr_instance",
         "pr_control", "pr_x", "pr_y", "pr_z", "pr_prev", "pr_next", "pr_size"]
MAC = dict(zip(NAMES, [8, 4, 0x28, 0x2C, 4, 0x14, 4, 0x34, 8, 0x50, 0x70, 0x100, 0x20, 0x18, 0x20, 0x30, 0x34, 0xC4, 0x18, 0x1C, 0x20,
                       0x24, 0x28, 0x44, 0x46, 0x6C, 0x70, 0x74, 0x78, 0x18, 0x40, 0x4C, 0x50, 0x68, 0, 4, 8, 0xC, 0x10, 0x14, 0x18,
                       0x1C, 0x20, 0x24, 0x2C, 0x28, 0x48]))
# the host offsets that must equal the Mac's (file layouts and the control's words)
SAME = ["br_flags", "br_f50", "data_f20", "ctl_fc4", "ctl_f18", "ctl_f1c", "ctl_f20", "ctl_f24", "ctl_f28", "hdr_nodecount", "hdr_f46",
        "hdr_hfield", "hdr_nodes", "hdr_mini", "hdr_f78", "hf_f18", "pr_number", "pr_flags", "pr_index", "pr_cellx", "pr_cellz"]
HOST = {}

SETS_N = 3
NODE_WORDS, MINI_WORDS, F78_WORDS, ROOT_WORDS = 8 * 0xA0 // 4, 8 * 0x58 // 4, 24, 4

OBJ_NAMES = ["inst", "head", "tail", "ctl", "pt", "hdr", "hf", "prev0", "pctl", "pyl"]
OBJ_NAMES += ["b%d" % i for i in range(5)] + ["d%d" % i for i in range(5)]
for _k in ("set", "mesh", "hdrS", "nodes", "mini", "f78", "root", "tab", "ent", "recA", "recB"):
    OBJ_NAMES += ["%s%d" % (_k, i) for i in range(SETS_N)]
LABEL = {n: i + 1 for i, n in enumerate(OBJ_NAMES)}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_alloc(e):
    size = e.r(3)
    e.cl.rec("MReleaseAllocF", (size, e.r(4)))
    a = e.alloc(size)
    e.write(a, bytes(size))
    e.set_r(3, a)


def stub_y(e):
    L = e.cl
    L.rec("GetRideRequiredYOffset", (s32(e.r(3)), s32(e.r(4)), L.lab(e.r(5)), s32(e.r(6)), s32(e.r(7)), e.r(8) & MASK, s32(e.r(9))))
    e.set_f1(bitsf(L.ret("Y")))


def stub_cell(e):
    L = e.cl
    L.rec("FloatToCell", (canon_f(fb(e.f(1))),))
    e.set_r(3, L.ret("Cell") & MASK)


def stub_flag(e):
    e.cl.rec("FlagCollision", (e.cl.lab(e.r(3)),))


def stub_anim(e):
    e.cl.rec("AnimateRideSet", (e.cl.lab(e.r(3)), s32(e.r(4)), e.r(5) & MASK))


def stub_deform(e):
    L = e.cl
    L.rec("DoMeshSkirtDeform", (L.lab(e.r(3)), L.lab(e.r(4)), canon_f(fb(e.f(1))), e.r(6) & MASK))


STUBS.internal(CALLEE_ALLOC, stub_alloc)
STUBS.internal(CALLEE_Y, stub_y)
STUBS.internal(CALLEE_CELL, stub_cell)
STUBS.internal(CALLEE_FLAG, stub_flag)
STUBS.internal(CALLEE_ANIM, stub_anim)
STUBS.internal(CALLEE_DEFORM, stub_deform)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def w16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def objects(self):
        self.objs = {n: self.alloc(0x800) for n in OBJ_NAMES}
        self.inv = {a: n for n, a in self.objs.items()}

    def nm(self, v):
        return self.inv.get(v, "?" if v else 0)

    def o(self, name):
        """The address of an object, or 0 for None."""
        return self.objs[name] if name else 0


class Ppc(Side):
    fmt = ">I"
    psz = 4
    off = MAC

    def __init__(self, e, case):
        self.e = e
        self.objects()
        self.L = e.cl = CL.PpcLog({a: LABEL[n] for n, a in self.objs.items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def wp(self, a, v):
        self.write(a, struct.pack(">I", v))

    def rp(self, a):
        return struct.unpack(">I", self.read(a, 4))[0]

    def p32(self, v):
        return v

    def call_new(self, x, z, flags, unused, ctl, pt):
        r3, _, _ = T.call_ppc(self.e, FN_NEW, [x & MASK, z & MASK, flags & MASK, unused & MASK, ctl, pt])
        return r3 & MASK

    def call_upd(self, ctl, pyl, a, b):
        T.call_ppc(self.e, FN_UPD, [ctl, pyl, a & MASK, b & MASK])


class Host(Side):
    fmt = "<I"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = HOST
        self.objects()
        CL.host_begin(lib, {LABEL[n]: a for n, a in self.objs.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_np_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_np_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def wp(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rp(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def p32(self, v):
        return self.lib.tpw_np_off(v)

    def call_new(self, x, z, flags, unused, ctl, pt):
        return self.lib.tpw_np_new(x & MASK, z & MASK, flags & MASK, s32(unused), ctl, pt)

    def call_upd(self, ctl, pyl, a, b):
        self.lib.tpw_np_upd(ctl, pyl, s32(a), s32(b))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_np_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "off").restype, getattr(lib, p + "off").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "new").restype = C.c_uint64
    getattr(lib, p + "new").argtypes = [C.c_uint32, C.c_uint32, C.c_uint32, C.c_int32, C.c_uint64, C.c_uint64]
    getattr(lib, p + "upd").restype = None
    getattr(lib, p + "upd").argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32]
    out = (C.c_int64 * 64)()
    n = getattr(lib, p + "layout")(out)
    assert n == len(NAMES), n
    HOST.update(dict(zip(NAMES, list(out)[:n])))
    for k in SAME:
        assert HOST[k] == MAC[k], (k, HOST[k], MAC[k])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_new(s, c):
    o, H = s.objs, s.off
    s.words(s.g("hw"), [c["hw"]])
    s.words(s.g("hh"), [c["hh"]])
    s.wp(s.g("inst"), o["inst"])
    s.wp(s.g("hdr"), o["hdr"])
    I = o["inst"]
    s.words(I + H["inst_f40"], [c["count"]])
    s.wp(I + H["inst_pylons"], s.o(c["head"]))
    s.wp(I + H["inst_tail"], s.o(c["tail"]))
    s.wp(I + H["inst_preview"], s.o(c["old_preview"]))
    s.words(o["hdr"] + H["hdr_hfield"], [s.p32(o["hf"])])
    s.words(o["hf"] + H["hf_f18"], [c["stride"]])
    s.words(o["pt"], c["pt"] or [0, 0, 0])


def build_upd(s, c):
    o, H = s.objs, s.off
    s.words(s.g("skip"), [c["skip"]])
    s.wp(s.g("hdr"), o["hdr"])
    for i in range(SETS_N):
        s.wp(s.g("sets") + i * s.psz, o["set%d" % i])
    ctl = o["ctl"]
    s.words(ctl, [c["cflags"]])
    for k in ("f18", "f1c", "f20", "f24", "f28"):
        s.words(ctl + H["ctl_" + k], [c["c_" + k]])
    s.words(o["pctl"] + H["ctl_fc4"], [c["fc4"]])
    pyl = o["pyl"]
    s.wp(pyl + H["lp_control"], o["pctl"])
    s.words(pyl + H["lp_y"], [c["pylon_y"]])
    s.words(pyl + H["lp_count"], [c["n"]])
    for i, b in enumerate(c["braces"]):
        bo = o["b%d" % i]
        s.wp(pyl + H["lp_braces"] + i * s.psz, bo)
        s.words(bo + H["br_flags"], [b["flags"]])
        s.words(bo + H["br_f50"], [b["f50"]])
        s.words(bo + H["br_w70"], [b["w70"]])
        s.wp(bo + H["br_data"], o["d%d" % i])
        s.words(o["d%d" % i] + H["data_f20"], [b["d20"]])
    for i, st in enumerate(c["sets"]):
        S = lambda k: o["%s%d" % (k, i)]
        s.wp(S("set") + H["set_heads"], S("mesh"))
        s.wp(S("mesh") + H["mesh_header"], S("hdrS"))
        s.wp(S("mesh") + H["mesh_f28"], S("tab"))
        s.wp(S("mesh") + H["mesh_root"], S("root") if st["has_root"] else 0)
        h = S("hdrS")
        s.w16(h + H["hdr_nodecount"], st["count"])
        s.w16(h + H["hdr_f46"], st["f46"])
        s.words(h + H["hdr_nodes"], [s.p32(S("nodes"))])
        s.words(h + H["hdr_mini"], [s.p32(S("mini"))])
        s.words(h + H["hdr_f78"], [s.p32(S("f78"))])
        s.words(S("nodes"), st["nodes"])
        s.words(S("mini"), st["mini"])
        s.words(S("f78"), st["f78"])
        s.words(S("root"), st["root"])
        s.wp(S("tab") + H["tab_entries"], S("ent"))
        for k, ch in enumerate(st["entries"]):
            rec = 0 if ch == 0 else (S("recA") if ch == 1 else S("recB"))
            s.wp(S("ent") + k * H["ent_size"] + H["ent_rec"], rec)
        s.words(S("recA") + H["rec_y"], [st["ya"]])
        s.words(S("recB") + H["rec_y"], [st["yb"]])


def execute(s, c):
    o = s.objs
    if c["op"] == "new":
        r = s.call_new(c["x"], c["z"], c["flags"], c["unused"], s.o(c["ctl"]), o["pt"] if c["pt"] else 0)
        if r:
            s.inv[r] = "new"
        return r
    s.call_upd(o["ctl"], o["pyl"], c["a"], c["b"])
    return 0


def observe_new(s, c, r):
    H, o = s.off, s.objs
    I = o["inst"]
    out = {"log": s.log(), "answer": s.nm(r)}
    out["inst"] = [s.rd_word(I + H["inst_f40"]), s.nm(s.rp(I + H["inst_pylons"])), s.nm(s.rp(I + H["inst_tail"])),
                   s.nm(s.rp(I + H["inst_preview"]))]
    out["tail_next"] = s.nm(s.rp(o["tail"] + H["pr_next"]))
    if r:
        w = lambda k: s.rd_word(r + H[k])
        out["rec"] = [w("pr_number"), w("pr_flags"), w("pr_index"), w("pr_cellx"), w("pr_cellz"), s.nm(s.rp(r + H["pr_instance"])),
                      s.nm(s.rp(r + H["pr_control"])), canon_f(w("pr_x")), canon_f(w("pr_y")), canon_f(w("pr_z")),
                      s.nm(s.rp(r + H["pr_prev"])), s.nm(s.rp(r + H["pr_next"]))]
    return out


def observe_upd(s, c, r):
    o = s.objs
    out = {"log": s.log()}
    for i in range(SETS_N):
        out["nodes%d" % i] = s.rd_words(o["nodes%d" % i], NODE_WORDS)
        out["mini%d" % i] = s.rd_words(o["mini%d" % i], MINI_WORDS)
        out["f78_%d" % i] = s.rd_words(o["f78%d" % i], F78_WORDS)
        out["root%d" % i] = s.rd_words(o["root%d" % i], ROOT_WORDS)
    return out


def run(s, c):
    try:
        if c["op"] == "new":
            build_new(s, c)
            return observe_new(s, c, execute(s, c))
        build_upd(s, c)
        r = execute(s, c)
        return observe_upd(s, c, r)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def new_case(r, flags=None, point=None, head="r", tail="r", x=None, z=None, cells=None):
    hw, hh = r.randrange(1, 40), r.randrange(1, 40)
    if cells is None:
        pool = [-3, -1, 0, 1, hw - 1, hw, hw + 5, hh - 1, hh, hh + 4, r.randrange(-100, 100)]
        cells = [r.choice(pool), r.choice(pool)]
    big = [0, 1, 5, 0xFFFFFFFF, 0x80000000, 16777217, 0x7FFFFFFF, r.randrange(0, 300), r.randrange(0, 300)]
    c = {"op": "new", "queues": {"Cell": [c_ & MASK for c_ in cells], "Y": [rf(r)]}, "hw": hw, "hh": hh, "stride": r.randrange(0, 300),
         "count": r.choice([0, 1, 2, 7, 0xFFFFFFFF]),
         "head": r.choice([None, "head"]) if head == "r" else head,
         "tail": r.choice([None, "tail", "head"]) if tail == "r" else tail,
         "old_preview": r.choice([None, "prev0"]),
         "x": r.choice(big) if x is None else x, "z": r.choice(big) if z is None else z, "unused": r.choice([0, 1, -1]),
         "ctl": r.choice([None, "ctl", "ctl"]),
         "flags": (r.choice([0, 4, 5, 8, 12, 1, 0xFFFFFFFF, r.getrandbits(32)]) if flags is None else flags)}
    c["pt"] = ([rf(r), rf(r), rf(r)] if r.random() < 0.6 else None) if point is None else (point if point != "none" else None)
    return c


def new_edges():
    r = random.Random(5091)
    for flags in (0, 4, 1, 5, 0xFFFFFFFF):
        for point in ("none", None):
            for head, tail in ((None, None), ("head", None), ("head", "tail"), (None, "tail")):
                yield new_case(r, flags=flags, point=point if point == "none" else [fb(55.5), fb(-2.0), fb(123.25)], head=head, tail=tail)
    for cells in ([-1, -1], [0, 0], [4, 4], [100, 100], [-5, 100], [100, -5]):
        yield new_case(r, flags=0, point=[fb(1.0), fb(2.0), fb(3.0)], cells=cells)
    for x in (0, 1, 0xFFFFFFFF, 0x80000000, 16777217, 33554435):
        for z in (0, 0xFFFFFFFF, 16777217, 7):
            yield new_case(r, flags=0, point="none", x=x, z=z)


def new_random(r):
    return new_case(r)


def upd_case(r, n=None, skip=None, a=None, b=None, cflags=None):
    n = r.choice([0, 1, 1, 2, 3, 4, 5]) if n is None else n
    pyl_bits = [0x10, 0x400, 0x800, 0x1000, 0x2000]
    cf = sum(bit for bit in pyl_bits if r.random() < 0.5) | (r.getrandbits(4) & 0xF) if cflags is None else cflags
    c = {"op": "upd", "queues": {}, "skip": r.choice([0, 0, 1]) if skip is None else skip,
         "a": r.choice([0, 1, -1]) if a is None else a, "b": r.choice([0, 1, 2]) if b is None else b, "cflags": cf,
         "c_f18": r.randrange(4), "c_f1c": r.randrange(5), "c_f20": r.randrange(5), "c_f24": r.randrange(5), "c_f28": r.randrange(5),
         "fc4": rf(r), "pylon_y": rf(r), "n": n}
    c["braces"] = [{"flags": (r.getrandbits(32) & ~0x2000) | (0x2000 if r.random() < 0.3 else 0), "w70": r.randrange(SETS_N),
                    "f50": rf(r), "d20": rf(r)} for _ in range(5)]
    c["sets"] = [{"count": r.randrange(9), "f46": r.randrange(4), "has_root": r.random() < 0.8,
                  "nodes": [r.getrandbits(32) for _ in range(NODE_WORDS)], "mini": [r.getrandbits(32) for _ in range(MINI_WORDS)],
                  "f78": [r.getrandbits(32) for _ in range(F78_WORDS)], "root": [r.getrandbits(32) for _ in range(ROOT_WORDS)],
                  "entries": [r.randrange(3) for _ in range(4)], "ya": rf(r), "yb": rf(r)} for _ in range(SETS_N)]
    return c


def upd_edges():
    r = random.Random(5092)
    for skip, a in ((0, 0), (1, 0), (1, 1), (0, 1)):
        for b in (0, 1):
            for n in (0, 1, 2, 3, 5):
                for cf in (0, 0x10, 0x400, 0x800, 0x1000, 0x2000, 0x3C10, 0x3C00):
                    yield upd_case(r, n=n, skip=skip, a=a, b=b, cflags=cf)


def upd_random(r):
    return upd_case(r)


TESTS = [
    FnTest("NewPylon", ppc, host, edge=new_edges, rnd=new_random),
    FnTest("UpdatePylonStackMeshes", ppc, host, edge=upd_edges, rnd=upd_random),
]
