"""Unit v5536, LLCC_DrawKartBlock 0x10089cfc, against libtpw_unit_LLCC_DrawKartBlock.dylib (tests/unit_LLCC_DrawKartBlock.cpp), main app
("SimTheme Park"), src/game/park/LLCellRide.cpp.

A case is a call. The callees CMap::IsXYValid, LLCC_SystemGetRideTrack, ADD_DIRECTION and CMapCell::Modify are fakes on both sides that
record their arguments (Modify's cell as its index from the map base) and answer from the case's queues. gGameWorld points at a small
block on both sides (cells are only addressed, never read).

What is compared: the answer and the event log. No fused operations. Not tested: a NULL world (the Mac does the arithmetic anyway).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCC_DrawKartBlock"
MASK = 0xFFFFFFFF

FN = 0x10089CFC
ADDR = dict(world=0x1037EBFC)
HOST_LAYOUT = [0x44]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.add(FN)


def st_valid(e):
    e.cl.rec("IsXYValid", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, e.cl.ret("IsXYValid") & MASK)


def st_track(e):
    e.cl.rec("GetRideTrack", ())
    e.set_r(3, e.cl.ret("GetRideTrack") & MASK)


def st_adddir(e):
    a, b = e.r(3) & 0xFF, e.r(4) & 0xFF
    e.cl.rec("ADD_DIRECTION", (a, b))
    e.set_r(3, (a + b * 3) & 0xFF)


def st_modify(e):
    world = e.u32(ADDR["world"])
    index = s32(e.r(3) - (world + 0x2D8)) // 0x44
    e.cl.rec("Modify", (index, s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, e.cl.ret("Modify") & MASK)


STUBS.internal(0x101012F4, st_valid)
STUBS.internal(0x10091AD0, st_track)
STUBS.internal(0x1010254C, st_adddir)
STUBS.internal(0x10097538, st_modify)


def setup(runner):
    T.install_std_stubs(runner.emu)


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

    def call(self, args):
        r3, _, _ = T.call_ppc(self.e, FN, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_kb_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_kb_alloc(n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, args):
        return self.lib.tpw_kb_draw(*[s32(a) for a in args[:5]], args[5] & 0xFF) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_kb_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    f = getattr(lib, p + "draw")
    f.restype, f.argtypes = C.c_uint32, [C.c_int32] * 5 + [C.c_uint32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        w = s.alloc(16)
        s.words(s.g("world"), [w])
        ans = s.call(case["args"])
        return {"answer": ans, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(x, y, a, b, c, d, valid, track, modify):
    return {"queues": {"IsXYValid": list(valid), "GetRideTrack": [track], "Modify": list(modify)}, "args": [x, y, a, b, c, d]}


def edges():
    r = random.Random(5536)
    for track in (0, 1, 2, 3, -1):
        for valid in ([1, 1, 1, 1], [0], [1, 0], [1, 1, 0], [1, 1, 1, 0]):
            for fa, fb in ((0, 0), (1, 0), (0, 1), (1, 1), (-1, 0), (0, -5)):
                for modify in ([1], [0], [1, 0], [1, 1, 0], [1, 1, 1, 0]):
                    yield mk(r.randrange(0, 126), r.randrange(0, 126), r.randrange(-3, 3), fa, fb, r.randrange(256), valid, track, modify)
    for x, y in ((0, 0), (127, 127), (-1, -1), (0, -1), (-1, 0), (510, 510), (65535, 3), (3, 600), (-200, 700)):
        for track in (1, 2):
            for fb in (0, 1):
                yield mk(x, y, 0, 0, fb, 0xF5, [1], track, [1])


def rnd(r):
    return mk(r.randrange(-5, 140), r.randrange(-5, 700), r.randrange(-2, 3), r.choice([0, 0, 1, -1]), r.choice([0, 1]),
              r.randrange(256), [r.choice([1, 1, 1, 0]) for _ in range(4)], r.choice([0, 1, 1, 2, 2, 3]),
              [r.choice([0, 1, 1]) for _ in range(4)])


TESTS = [
    FnTest("LLCC_DrawKartBlock", ppc, host, edge=edges, rnd=rnd),
]
