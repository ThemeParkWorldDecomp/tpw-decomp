"""Unit v5886, ride placement on the height field, against libtpw_unit_GetRideRequiredYOffset.dylib (tests/unit_GetRideRequiredYOffset.cpp),
main app ("SimTheme Park"), src/game/rides/RidePlacement.cpp:
  GetRideMasterBoundArea 0x101ed058, GetNodeWorldVertPosnsIntoGeomBuffer (node, matrix) 0x101ed17c, GetRideRequiredYOffset 0x101ed29c,
  CorrectRidePosnForRotation 0x101ed5bc, SetRidePositionAndRotation 0x101ed6ac, MoveRide 0x101ed9a0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. CellIsRide, GetRotatedHFieldCoord,
YRotateLMatrixNoTrans and the Disable/EnableMeshDraw pair are fakes on both sides that record their arguments and answer from the case.

The world of a case: the height field header (flags, cell range bytes, height field with its heights, the first node with a matrix), a mesh,
a ride set (flags, ranges, height map, place), the ride table, the sine table, the scratch geometry buffer, a cell block. What is compared:
the answer, the log, and every block the functions may write. Fused operations: fmadds in the matrix product, the bound box, the position
(HardwareRounding over the unit's functions). Not tested: a NULL set or header the original reads unconditionally.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_GetRideRequiredYOffset"
MASK = 0xFFFFFFFF

F_MASTER, F_NODE, F_YOFF, F_CORRECT, F_SETPOS, F_MOVE = 0x101ed058, 0x101ed17c, 0x101ed29c, 0x101ed5bc, 0x101ed6ac, 0x101ed9a0
SIZES = {F_MASTER: 232, F_NODE: 208, F_YOFF: 744, F_CORRECT: 180, F_SETPOS: 676, F_MOVE: 288}
OWN = set(SIZES)

CELL_IS_RIDE = 0x1006d718
GET_ROTATED = 0x10060ab8
YROTATE = 0x100c1698
DISABLE, DISABLE_R, ENABLE, ENABLE_R = 0x100653a4, 0x1006548c, 0x10065178, 0x100650c4

ADDR = dict(hfh=0x102c6c7c, scratch=0x102c7990, sets=0x102c79a4, tpg=0x103b74c8)
MAC_LAY = dict(flags=4, heads=8, fc0=0xC0, fc4=0xC4, hm=0xCC, fe8=0xE8, fec=0xEC, setsize=0x100, mheader=4, mf10=0x10, meshsize=0x40)
FILE_LAY = [0x30, 0x6C, 0x78, 0xA8, 0x10, 0x58, 0x60, 0x18, 0x28]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def cw(ws):
    return [canon_f(w & MASK) for w in ws]


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def ffb(e, i):
    return canon_f(fb(e.f(i)))


def stub_cell(e):
    L = e.cl
    L.rec("CellIsRide", (s32(e.r(3)),))
    e.set_r(3, L.ret("CellIsRide") & MASK)


def stub_rot(e):
    L = e.cl
    L.rec("Rot", (s32(e.r(3)),) + tuple(ffb(e, i) for i in range(1, 7)))
    outx = e.r(10)
    outz = e.u32(e.r(1) + 0x38)
    e.write(outx, struct.pack(">I", L.ret("RotX") & MASK))
    e.write(outz, struct.pack(">I", L.ret("RotZ") & MASK))


def stub_yrot(e):
    L = e.cl
    L.rec("YRot", (L.lab(e.r(3) - 0x10), ffb(e, 1), ffb(e, 2)))


def stub_disable(e):
    e.cl.rec("Disable", (e.cl.lab(e.r(3)),))


def stub_disable_r(e):
    e.cl.rec("DisableR", (e.cl.lab(e.r(3)), e.r(4) >> 16, e.r(4) & 0xFFFF, e.r(5) >> 16, e.r(5) & 0xFFFF))


def stub_enable(e):
    e.cl.rec("Enable", (e.cl.lab(e.r(3)),))


def stub_enable_r(e):
    e.cl.rec("EnableR", (e.cl.lab(e.r(3)), e.r(4) >> 16, e.r(4) & 0xFFFF, e.r(5) >> 16, e.r(5) & 0xFFFF))


for _a, _f in ((CELL_IS_RIDE, stub_cell), (GET_ROTATED, stub_rot), (YROTATE, stub_yrot), (DISABLE, stub_disable),
               (DISABLE_R, stub_disable_r), (ENABLE, stub_enable), (ENABLE_R, stub_enable_r)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def halves(self, a, hs):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", h & 0xFFFF) for h in hs))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"
    lay = MAC_LAY

    def __init__(self, e, case):
        self.e = e
        self.queues = case["queues"]
        self.saved = STUBS.activate(e)

    def begin(self, fixed):
        self.e.cl = self.L = CL.PpcLog({addr: lab for lab, addr in fixed.items()}, self.queues)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def ptr(self, a, v):
        self.words(a, [v])

    def p32(self, v):
        return v

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        with ROUNDING(self.e):
            r3, _, f1 = T.call_ppc(self.e, entry, args)
        return r3 & MASK, fb(f1)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.queues = case["queues"]
        self.lay = lib.lay

    def begin(self, fixed):
        CL.host_begin(self.lib, fixed, self.queues)

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rd_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_rd_alloc(n)

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def p32(self, v):
        return self.lib.tpw_rd_off(v) if v else 0

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rd_addr.restype, lib.tpw_rd_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rd_alloc.restype, lib.tpw_rd_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rd_off.restype, lib.tpw_rd_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_rd_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rd_yoff.restype = C.c_uint32
    lib.tpw_rd_yoff.argtypes = [C.c_int32, C.c_int32, C.c_uint64, C.c_int32, C.c_int32, C.c_uint32, C.c_int32]
    lib.tpw_rd_master.restype, lib.tpw_rd_master.argtypes = C.c_int32, [C.c_uint32, C.c_uint64]
    lib.tpw_rd_node.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_rd_correct.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_uint64, C.c_uint64, C.c_int32, C.c_int32]
    lib.tpw_rd_setpos.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_float, C.c_uint32,
                                  C.c_int32, C.c_int32]
    lib.tpw_rd_move.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rd_layout(out)
    v = list(out)[:n]
    assert v[11:] == FILE_LAY, v
    lib.lay = dict(zip(("flags", "heads", "fc0", "fc4", "hm", "fe8", "fec", "setsize", "mheader", "mf10", "meshsize"), v[:11]))
    lib.bound = True


def call_host(s, op, args, W):
    L = s.lib
    if op == "yoff":
        x, z, has_a, w, h, flags, rot = args
        return L.tpw_rd_yoff(x, z, W["cells"] if has_a else 0, w, h, flags & MASK, rot), 0
    if op == "master":
        return L.tpw_rd_master(args[0], W["box"]) & MASK, 0
    if op == "node":
        L.tpw_rd_node(W["node"], W["mx"])
        return 0, 0
    if op == "correct":
        x, z, rot, w, h = args
        L.tpw_rd_correct(W["ints"], W["ints"] + 4, rot, W["ints"] + 8, W["ints"] + 12, w, h)
        return 0, 0
    if op == "setpos":
        ox, oz, x, z, rot, yb, flags, w, h = args
        L.tpw_rd_setpos(W["set"], ox, oz, x, z, rot, bitsf(yb), flags & MASK, w, h)
        return 0, 0
    x, z, has_a, w, h, flags = args
    L.tpw_rd_move(W["set"], x, z, W["cells"] if has_a else 0, w, h, flags & MASK)
    return 0, 0


def call_ppc_op(s, op, args, W):
    if op == "yoff":
        x, z, has_a, w, h, flags, rot = args
        r3, f1 = s.call(F_YOFF, [x & MASK, z & MASK, W["cells"] if has_a else 0, w & MASK, h & MASK, flags & MASK, rot & MASK])
        return f1, 0
    if op == "master":
        return s.call(F_MASTER, [args[0], W["box"]])[0], 0
    if op == "node":
        s.call(F_NODE, [W["node"], W["mx"]])
        return 0, 0
    if op == "correct":
        x, z, rot, w, h = args
        s.call(F_CORRECT, [W["ints"], W["ints"] + 4, rot & MASK, W["ints"] + 8, W["ints"] + 12, w & MASK, h & MASK])
        return 0, 0
    if op == "setpos":
        ox, oz, x, z, rot, yb, flags, w, h = args
        s.call(F_SETPOS, [W["set"], ox & MASK, oz & MASK, x & MASK, z & MASK, rot & MASK, T.F(bitsf(yb)), flags & MASK, w & MASK, h & MASK])
        return 0, 0
    x, z, has_a, w, h, flags = args
    s.call(F_MOVE, [W["set"], x & MASK, z & MASK, W["cells"] if has_a else 0, w & MASK, h & MASK, flags & MASK])
    return 0, 0


def build(s, case):
    c = case["world"]
    L = s.lay
    W = {}
    for name, n in (("hdr", 0xC0), ("hf", 0x40), ("heights", 256), ("node", 0x80), ("verts", 0x80), ("mesh", L["meshsize"]),
                    ("set", L["setsize"]), ("hm", 0x40), ("cells", 0x400), ("scratch", 0x100), ("box", 0x20), ("ints", 0x20),
                    ("mx", 0x40)):
        W[name] = s.alloc(n)
    s.words(W["hdr"] + 0x30, [c["hflags"]])
    s.words(W["hdr"] + 0x6C, [s.p32(W["hf"])])
    s.words(W["hdr"] + 0x78, [s.p32(W["node"])])
    s.write(W["hdr"] + 0xA8, bytes(c["abytes"]))
    s.words(W["hf"] + 8, c["hff"])
    s.words(W["hf"] + 0x18, [c["W"]])
    s.words(W["hf"] + 0x28, [s.p32(W["heights"])])
    s.words(W["heights"], c["heights"])
    s.words(W["node"] + 0x10, c["nmat"])
    s.halves(W["node"] + 0x58, [c["vcount"]])
    s.words(W["node"] + 0x60, [s.p32(W["verts"])])
    s.words(W["verts"], c["verts"])
    s.words(W["mx"], c["mxw"])
    s.ptr(W["mesh"] + L["mheader"], W["hdr"])
    s.words(W["mesh"] + L["mf10"], [c["mf10"]])
    st = W["set"]
    s.words(st + L["flags"], [c["setflags"]])
    s.ptr(st + L["heads"], W["mesh"])
    s.halves(st + L["fc0"], c["fc0"])
    s.halves(st + L["fc4"], c["fc4"])
    s.ptr(st + L["hm"], W["hm"] if c["hm_present"] else 0)
    s.words(st + L["fe8"], [c["fe8"]])
    s.words(st + L["fec"], [c["fec"]])
    s.words(W["hm"], c["hmw"])
    for j in range(8):
        for i in range(4):
            s.words(W["cells"] + j * 0xA0 + i * 8 + 8, [c["cellw"][j * 4 + i]])
    s.words(W["scratch"], [0xDEADBEEF] * 64)
    s.words(W["box"], [0xCAFEF00D] * 8)
    s.words(W["ints"], c["ints"])
    for k in range(8):
        s.ptr(s.g("sets") + k * (4 if s.fmt == ">I" else 8), st if c["present"][k] else 0)
    s.ptr(s.g("hfh"), W["hdr"])
    s.ptr(s.g("scratch"), W["scratch"])
    t = s.g("tpg")
    s.words(t, [0x1000, c["mask"], c["scale"]])
    s.words(t + 0xC, c["sin"])
    s.begin({1: W["mesh"], 2: W["node"]})
    return W


def observe(s, case, W, answer):
    L = s.lay
    st = W["set"]
    return {
        "answer": answer,
        "log": s.log(),
        "hflags": s.rd_word(W["hdr"] + 0x30),
        "abytes": s.read(W["hdr"] + 0xA8, 4),
        "nmat": cw(s.rd_words(W["node"] + 0x10, 16)),
        "mf10": s.rd_word(W["mesh"] + L["mf10"]),
        "setflags": s.rd_word(st + L["flags"]),
        "fc0": [s.rd_half(st + L["fc0"]), s.rd_half(st + L["fc0"] + 2)],
        "fc4": [s.rd_half(st + L["fc4"]), s.rd_half(st + L["fc4"] + 2)],
        "fe8": s.rd_word(st + L["fe8"]),
        "fec": s.rd_word(st + L["fec"]),
        "scratch": cw(s.rd_words(W["scratch"], 64)),
        "box": cw(s.rd_words(W["box"], 8)),
        "ints": s.rd_words(W["ints"], 8),
    }


def run(s, case):
    try:
        W = build(s, case)
        if isinstance(s, Ppc):
            f, _ = call_ppc_op(s, case["op"], case["args"], W)
        else:
            f, _ = call_host(s, case["op"], case["args"], W)
        return observe(s, case, W, f)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000]


def rf(r, scale=100.0):
    k = r.random()
    if k < 0.06:
        return r.choice(SPECIAL)
    return fb(r.uniform(-scale, scale))


def world(r):
    Wd = r.randrange(0, 8)
    return {
        "hflags": r.getrandbits(32), "abytes": [r.randrange(256) for _ in range(4)],
        "hff": [rf(r, 10) for _ in range(4)], "W": Wd, "heights": [rf(r, 60) for _ in range(64)],
        "nmat": [rf(r, 20) for _ in range(16)], "vcount": r.choice([0, 1, 3, 4, 5, 8, r.randrange(9)]),
        "verts": [rf(r, 20) for _ in range(24)], "mxw": [rf(r, 4) for _ in range(16)],
        "mf10": r.choice([0, 90, 180, 270, r.randrange(-1000, 1000)]),
        "setflags": r.choice([0, 0x4000, r.getrandbits(32)]),
        "fc0": [r.randrange(65536), r.randrange(65536)], "fc4": [r.randrange(65536), r.randrange(65536)],
        "hm_present": r.random() < 0.8, "fe8": r.getrandbits(32), "fec": r.getrandbits(32),
        "hmw": [rf(r, 50) for _ in range(16)], "cellw": [r.randrange(10) for _ in range(32)],
        "ints": [r.randrange(-50, 50) & MASK for _ in range(8)], "present": [r.random() < 0.7 for _ in range(8)],
        "mask": r.choice([63, 63, 31, 0]), "scale": fb(r.uniform(0, 12)), "sin": [rf(r, 1) for _ in range(64)],
    }, Wd


def mk(r, op, args, **over):
    w, Wd = world(r)
    w.update(over)
    return {"op": op, "args": args, "world": w,
            "queues": {"CellIsRide": [r.choice([0, 1]) for _ in range(20)],
                       "RotX": [r.randrange(0, w["W"] + 1) for _ in range(70)],
                       "RotZ": [r.randrange(0, 8) for _ in range(70)]}}


ROTS = [0, 0xB4, 90, 270, 45, 360, -90]
FLAGS = [0, 2, 0x20, 0x22, 0x10, 0x32, 0xFF]


def a_flag(r):
    return r.random() < 0.7


def yoff_args(r, rot=None, has_a=None, w=None, h=None, flags=None):
    return [r.randrange(-20, 100), r.randrange(-20, 100), a_flag(r) if has_a is None else has_a,
            r.choice([1, 2, 3, 4, 0, -1]) if w is None else w, r.choice([1, 2, 3, 4, 0, -1]) if h is None else h,
            r.choice(FLAGS) if flags is None else flags, r.choice(ROTS) if rot is None else rot]


def yoff_edges():
    r = random.Random(11)
    for rot in ROTS:
        for has_a in (False, True):
            for w, h in ((1, 1), (2, 3), (3, 2), (0, 2), (2, 0), (4, 4)):
                for flags in (0, 0x10):
                    yield mk(r, "yoff", yoff_args(r, rot, has_a, w, h, flags))


def yoff_random(r):
    return mk(r, "yoff", yoff_args(r))


def master_edges():
    r = random.Random(12)
    for ride in range(8):
        for hm in (False, True):
            yield mk(r, "master", [ride], hm_present=hm, present=[True] * 8)
    yield mk(r, "master", [3], present=[False] * 8)


def master_random(r):
    return mk(r, "master", [r.randrange(8)])


def node_edges():
    r = random.Random(13)
    for n in (0, 1, 3, 4, 5, 8):
        yield mk(r, "node", [], vcount=n)


def node_random(r):
    return mk(r, "node", [])


def correct_args(r, rot=None):
    return [r.randrange(-50, 50), r.randrange(-50, 50), r.choice([0, 0x5A, 0xB4, 0x10E, 45, r.randrange(-400, 400)]) if rot is None else rot,
            r.randrange(-5, 8), r.randrange(-5, 8)]


def correct_edges():
    r = random.Random(14)
    for rot in (0, 0x5A, 0xB4, 0x10E, 1, 360, -90):
        yield mk(r, "correct", correct_args(r, rot))


def correct_random(r):
    return mk(r, "correct", correct_args(r))


def setpos_args(r, flags=None, rot=None):
    return [r.randrange(-30, 300), r.randrange(-30, 300), r.randrange(-30, 100), r.randrange(-30, 100),
            r.choice(ROTS + [r.randrange(-800, 800)]) if rot is None else rot, rf(r, 50), r.choice(FLAGS) if flags is None else flags,
            r.choice([1, 2, 3, 4, 40000]), r.choice([1, 2, 3, 5, 70000])]


def setpos_edges():
    r = random.Random(15)
    for flags in (0, 2, 0x20, 0x22):
        for rot in ROTS:
            yield mk(r, "setpos", setpos_args(r, flags, rot))
    for mf10 in (0, 359, -1, -359, -360, 720, 1000):
        yield mk(r, "setpos", setpos_args(r, 0x20, 90), mf10=mf10)


def setpos_random(r):
    return mk(r, "setpos", setpos_args(r))


def move_args(r, flags=None):
    return [r.randrange(-20, 100), r.randrange(-20, 100), a_flag(r), r.choice([1, 2, 3, 4]), r.choice([1, 2, 3, 4]),
            r.choice(FLAGS) if flags is None else flags]


def move_edges():
    r = random.Random(16)
    for sf in (0, 0x4000):
        for mf10 in (0, 90, 180, 270, 33):
            for flags in (0, 0x22, 0x32):
                yield mk(r, "move", move_args(r, flags), setflags=sf, mf10=mf10)


def move_random(r):
    return mk(r, "move", move_args(r))


TESTS = [
    FnTest("GetRideRequiredYOffset", ppc, host, edge=yoff_edges, rnd=yoff_random),
    FnTest("GetNodeWorldVertPosnsIntoGeomBuffer", ppc, host, edge=node_edges, rnd=node_random),
    FnTest("GetRideMasterBoundArea", ppc, host, edge=master_edges, rnd=master_random),
    FnTest("CorrectRidePosnForRotation", ppc, host, edge=correct_edges, rnd=correct_random),
    FnTest("SetRidePositionAndRotation", ppc, host, edge=setpos_edges, rnd=setpos_random),
    FnTest("MoveRide", ppc, host, edge=move_edges, rnd=move_random),
]
