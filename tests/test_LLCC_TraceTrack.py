"""Unit v5988, the track tracer, against libtpw_unit_LLCC_TraceTrack.dylib (tests/unit_LLCC_TraceTrack.cpp), main app
("SimTheme Park"), src/game/park/LLCellRide.cpp:
  LLCC_TraceTrack 0x1008ddfc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee (the cell predicates, GetDirection,
ModifyDirection, OPPOSITE, Delete, LLCC_DrawTrackLine, LLCC_BreakTrack, BMP_FreeTrack ...) is a fake on both sides that records its arguments (a
cell as its index in the grid) and answers from the case's queue.

The world of a case: gGameWorld pointing at a block whose cells (a 64 by 64 window of the grid) hold their id (+0), a mode (+8, 0x19 now and
then) and a parent id (+0x10); the word gLLCCDB6C pointing at an object whose word +0x24 is the ride. What is compared: the answer and the
event log (the cells are never written by the fakes). No fused operations. Not tested: a walk that leaves the window (the original reads
whatever lies there).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCC_TraceTrack"
MASK = 0xFFFFFFFF

FN = 0x1008ddfc
SIZES = {FN: 2212}
OWN = set(SIZES)

ADDR = dict(world=0x1037ebfc, obj=0x102ddb6c)
HOST_LAYOUT = [0, 8, 0x10, 0x28]
CELLS = 0x2d8 + 0x110000
LO, HI = 32, 96
CUR = {"base": 0}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cell_index(e):
    return (e.r(3) - CUR["base"]) // 0x28


def cell_queue(addr, name):
    def f(e):
        L = e.cl
        L.rec(name, (cell_index(e),))
        e.set_r(3, L.ret(name) & MASK)
    STUBS.internal(addr, f)


for _a, _n in ((0x100a367c, "IsTrackX"), (0x100a399c, "GetSegmentNumber"), (0x10081930, "GetDirection"), (0x100a3574, "IsTrack"),
               (0x100a3914, "IsCrossRoad"), (0x100a37a0, "IsCorner"), (0x100a3868, "IsStraight"), (0x100816e8, "GetNeighbours"),
               (0x10081968, "NumberOfDirections"), (0x10081744, "NumberOfNeighbours")):
    cell_queue(_a, _n)


def stub_opposite(e):
    d = e.r(3) & 0xFF
    e.cl.rec("OPPOSITE", (d,))
    e.set_r(3, ((d << 4) | (d >> 4)) & 0xFF)


def stub_modify_direction(e):
    e.cl.rec("ModifyDirection", (cell_index(e), e.r(4) & 0xFF))


def stub_delete(e):
    e.cl.rec("Delete", (cell_index(e), s32(e.r(4)), s32(e.r(5)), e.r(6) & 0xFF))


def stub_draw(e):
    e.cl.rec("DrawTrackLine", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), e.s32(e.r(6)), e.s32(e.r(7))))


def stub_break(e):
    e.cl.rec("BreakTrack", (s32(e.r(3)), s32(e.r(4)), e.r(5) & 0xFF))
    e.set_r(3, 0)


def stub_free(e):
    e.cl.rec("BMP_FreeTrack", (s32(e.r(3)),))


STUBS.internal(0x10102500, stub_opposite)
STUBS.internal(0x10081810, stub_modify_direction)
STUBS.internal(0x100a40c4, stub_delete)
STUBS.internal(0x100a30f8, stub_draw)
STUBS.internal(0x100a510c, stub_break)
STUBS.internal(0x100270e0, stub_free)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, a, b, flag):
        r3, _, _ = T.call_ppc(self.e, FN, [a, b, flag & MASK])
        return s32(r3)

    def set_world(self, w):
        self.words(self.g("world"), [w])

    def set_object(self, o):
        self.words(self.g("obj"), [o])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_tt_reset()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_tt_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_tt_alloc(n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, a, b, flag):
        return self.lib.tpw_tt_run(a, b, flag)

    def set_world(self, w):
        self.write(self.g("world"), struct.pack("<Q", w))     # a host pointer is 64 bits

    def set_object(self, o):
        self.words(self.g("obj"), [o])
        self.write(self.g("curobj"), struct.pack("<Q", o))    # the whole pointer beside the Mac's word


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_tt_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "run").restype, getattr(lib, p + "run").argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def cell_fields(seed, x, y):
    h = (x * 73856093 ^ y * 19349663 ^ seed) & MASK
    h2 = h % 100
    mode = 0x19 if h2 < 12 else (0xC, 0x11, 1, 2, 7)[h2 % 5]
    px = 56 + (h >> 8) % 16
    py = 56 + (h >> 12) % 16
    return x + y * 128 + 1, mode, px + py * 128 + 1


def build(s, case):
    size = CELLS + (HI * 128 + HI + 2) * 0x28
    w = s.alloc(size)
    s.base = w + CELLS
    CUR["base"] = s.base
    st = struct.Struct(s.fmt[0] + "H6xI4xH22x")
    for y in range(LO, HI):
        row = b"".join(st.pack(*cell_fields(case["seed"], x, y)) for x in range(LO, HI))
        s.write(s.base + (y * 128 + LO) * 0x28, row)
    s.set_world(w)
    if case["obj"]:
        o = s.alloc(0x40)
        s.words(o + 0x24, [case["ride"]])
        s.set_object(o)
    else:
        s.set_object(0)


def cell_addr(s, x, y):
    return s.base + (x + y * 128) * 0x28


def execute(s, case):
    return s.call(cell_addr(s, *case["from"]), cell_addr(s, *case["to"]), case["flag"])


def run(s, case):
    try:
        build(s, case)
        answer = execute(s, case)
        return {"answer": answer, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

DIRS = [1, 4, 0x10, 0x40]
DELTA = {1: (0, -2), 4: (2, 0), 0x10: (0, 2), 0x40: (-2, 0)}


def make_case(r, flag, pcorner, pcross, ptx, k, pstraight):
    d = r.choice(DIRS)
    sx, sy = r.randrange(56, 72), r.randrange(56, 72)
    to = (sx + k * DELTA[d][0], sy + k * DELTA[d][1])
    seg_f = r.choice([1, 2, 3, -1])
    seg_t = seg_f + r.choice([-1, 0, 0, 1, 1, 2])
    segs = [seg_f, seg_t] + [r.choice([seg_f, seg_t, seg_f + 1, 5]) for _ in range(40)]

    def nz(p, n=40, vals=(0x55, 1, 4, 0x10)):
        return [r.choice(vals) if r.random() < p else 0 for _ in range(n)] + [0]

    track = [1] * (2 * (k + 1)) + [r.randint(0, 1) for _ in range(r.randrange(0, 10))] + [0]
    if r.random() < 0.08:
        track[0] = 0
    q = {
        "IsTrackX": [1 if r.random() < ptx else 0 for _ in range(40)] + [0],
        "GetSegmentNumber": segs,
        "GetDirection": [d if r.random() < 0.85 else r.choice(DIRS) for _ in range(40)],
        "IsTrack": track,
        "IsCrossRoad": nz(pcross),
        "IsCorner": nz(pcorner),
        "IsStraight": nz(pstraight),
        "GetNeighbours": [r.choice([1, 4, 0x10, 0x40, 5, 0x14, 0x50, 0x41, 0x15]) for _ in range(40)],
        "NumberOfDirections": [r.choice([1, 1, 2, 3]) for _ in range(40)],
        "NumberOfNeighbours": [r.choice([0, 1, 1, 2, 3]) for _ in range(40)],
    }
    return {"flag": flag, "from": (sx, sy), "to": to, "queues": q, "seed": r.randrange(1 << 30),
            "obj": r.random() < 0.6, "ride": r.randrange(1 << 32)}


def edges():
    r = random.Random(5988)
    for flag in (0, 1):
        for pcorner in (0, 0.4):
            for pcross in (0, 0.3):
                for ptx in (0, 0.3):
                    for k in (0, 1, 3):
                        yield make_case(r, flag, pcorner, pcross, ptx, k, r.choice([0, 0.5]))
    for flag in (5, 1, 1, 1):
        for k in (0, 2, 4, 6):
            yield make_case(r, flag, 0, 0.1, 0, k, 0.7)


def rnd(r):
    return make_case(r, r.choice([0, 1, 1, 1, 5]), r.choice([0, 0.2, 0.5]), r.choice([0, 0.1, 0.4]), r.choice([0, 0, 0.2]),
                     r.randrange(0, 7), r.choice([0, 0.3, 0.8]))


TESTS = [
    FnTest("LLCC_TraceTrack", ppc, host, edge=edges, rnd=rnd),
]
