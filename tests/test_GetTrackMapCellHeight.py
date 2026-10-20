"""Unit v5070, the height of a track piece from its map cell and neighbours, against libtpw_unit_GetTrackMapCellHeight.dylib
(tests/unit_GetTrackMapCellHeight.cpp), main app ("SimTheme Park"), src/GetTrackMapCellHeight.cpp:
  GetTrackMapCellHeight 0x1009d8d0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory (big-endian) for the original, and in the
host library's memory (little-endian) for the translation. The callees outside the unit, GetWorldGSquareHeight (answers a float from the
queue) and CMapCell::GetNeighbouringCell (answers the case's neighbour cell for the offset asked, or NULL), are fakes on both sides that
record every argument.

The world of a case: the cell (id, type), up to three neighbour cells (id, type) or NULL, the height queue. What is compared: the answer's
bits and the event log. No fused operations; the heights stay below 2^31 (fctiwz saturation differs in Unicorn).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_GetTrackMapCellHeight"
MASK = 0xFFFFFFFF

FN = 0x1009D8D0
CALLEE_HEIGHT = 0x1005F160
CALLEE_NEIGH = 0x101036B0

STATE = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_height(e):
    L = e.cl
    L.rec("Height", (s32(e.r(3)), s32(e.r(4)), e.r(5) & MASK))
    e.set_f1(bitsf(L.ret("Height")))


def stub_neigh(e):
    L = e.cl
    x, y = e.s32(e.r(4)), e.s32(e.r(4) + 4)
    L.rec("Neighbour", (1 if e.r(3) == STATE["main"] else 0, x, y, s32(e.r(5))))
    idx = {(0, 1): 0, (1, 0): 1, (1, 1): 2}.get((x, y))
    e.set_r(3, STATE["nb"][idx] if idx is not None else 0)


STUBS.internal(CALLEE_HEIGHT, stub_height)
STUBS.internal(CALLEE_NEIGH, stub_neigh)


def setup(runner):
    T.install_std_stubs(runner.emu)
    for a, x, y in ((0x1034C25C, 0, 1), (0x1034C270, 1, 1), (0x1034C284, 1, 0)):
        runner.emu.write(a, struct.pack(">III", x, y, 0))


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

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def world(self, main, nbs):
        STATE["main"] = main
        STATE["nb"] = nbs

    def call(self, cell, h, x, y):
        _, _, f1 = T.call_ppc(self.e, FN, [cell, T.F(bitsf(h)), x & MASK, y & MASK])
        return canon_f(fb(f1))


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_v5070_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def world(self, main, nbs):
        self.lib.tpw_v5070_world(main, *nbs)

    def call(self, cell, h, x, y):
        return canon_f(fb(self.lib.tpw_v5070_run(cell, bitsf(h), s32(x), s32(y))))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_v5070_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "world").restype, getattr(lib, p + "world").argtypes = None, [C.c_uint64] * 4
    getattr(lib, p + "run").restype, getattr(lib, p + "run").argtypes = C.c_float, [C.c_uint64, C.c_float, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == [0, 8, 0x44], list(out)[:n]
    lib.bound = True


def make_cell(s, spec):
    if spec is None:
        return 0
    cid, typ = spec
    a = s.alloc(0x44)
    s.write(a, struct.pack(s.fmt[0] + "HBB", cid & 0xFFFF, 1, 0))
    s.words(a + 8, [typ])
    return a


def run(s, case):
    try:
        main = make_cell(s, case["main"])
        nbs = [make_cell(s, n) for n in case["nb"]]
        s.world(main, nbs)
        answer = s.call(main, *case["args"])
        return {"answer": answer, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


TYPES = [4, 7, 9, 10, 0xB, 0x10, 0x19]
OTHERS = [0, 1, 2, 3, 5, 6, 8, 0xC, 0xF, 0x11, 0x18, 0x1A, 0x1E, -1, 0x20]
HEIGHTS = [0.0, -0.0, 0.5, 0.9, 1.0, 8.0, 9.0, 9.5, 10.0, 10.5, 11.0, 19.0, 20.0, -0.5, -1.0, -8.0, -9.0, -9.5, -10.0, -10.5, -11.0,
           -19.0, 123.4, 1234.9, 1e6, -1e6, 1e9, -1e9, 16777216.0, 160.0]


def hq(r, n=4):
    return [fb(r.choice(HEIGHTS) if r.random() < 0.5 else r.uniform(-3000, 3000)) for _ in range(n)]


def rcell(r, typ=None):
    return (r.choice([0, 1, 2, 0x7F, 0x80, 0x81, 0x4000, 0x4001, 0xFFFF, r.randrange(0, 0x10000)]),
            typ if typ is not None else r.choice(TYPES + OTHERS))


def case(r, main=None, nb=None, args=None):
    c = {"queues": {"Height": hq(r)}}
    c["main"] = (r.randrange(1, 0x4000), r.choice(TYPES + OTHERS)) if main is None else main
    c["nb"] = [None if r.random() < 0.2 else rcell(r) for _ in range(3)] if nb is None else nb
    c["args"] = args if args is not None else [fb(r.choice(HEIGHTS + [5.0, 77.5])) if r.random() < 0.7 else fb(r.uniform(-500, 500)),
                                              r.choice([0, 1, -1, 127, 128, r.randrange(-300, 300)]), r.choice([0, 1, -1, 127, r.randrange(-300, 300)])]
    return c


def edges():
    r = random.Random(5070)
    for t in TYPES + OTHERS:
        yield case(r, main=(5, t), nb=[None, None, None])
    for t in TYPES + OTHERS:
        for k in range(3):
            nb = [None, None, None]
            nb[k] = (r.randrange(0, 0x10000), t)
            c = case(r, main=(5, 0), nb=nb)
            yield c
            c = case(r, main=(5, 0x19), nb=nb)
            yield c
    for h in HEIGHTS:
        c = case(r, main=(9, 0xB), nb=[None, None, None])
        c["queues"] = {"Height": [fb(h)]}
        yield c
        c = case(r, main=(9, 0), nb=[(0, 4), (1, 4), (0x4000, 7)])
        c["queues"] = {"Height": [fb(h), fb(-h), fb(h + 1)]}
        yield c
    for ids in [(0, 0, 0), (1, 128, 129), (0xFFFF, 0x8000, 0x4001)]:
        yield case(r, main=(3, 7), nb=[(ids[0], 7), (ids[1], 9), (ids[2], 0x10)])


def rnd(r):
    return case(r)


TESTS = [
    FnTest("GetTrackMapCellHeight", ppc, host, edge=edges, rnd=rnd),
]
