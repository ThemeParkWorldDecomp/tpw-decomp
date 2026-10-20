"""Unit v5192, CTrackCell::CalculateNeighbours, against libtpw_unit_CTrackCell_CalculateNeighbours.dylib
(tests/unit_CTrackCell_CalculateNeighbours.cpp), main app ("SimTheme Park"), src/TrackCell_flags_CTrackCell_CalculateNeighbours.cpp:
  CTrackCell::CalculateNeighbours 0x100a0bd4

A case is a world and a call, built twice by one piece of Python code (emulator memory at the Mac's addresses, host library memory). The callees
outside the unit (GetNeighbouringCell, IsTrack, NumberOfNeighbours, ModifyNeighbours) are fakes on both sides that record their arguments
and answer from the case's queues. Cells are named in the log by the word at +0x1c (1 the cell, 2 the neighbour).

The world of a case: the globals (flag 0x102e485c, the last offsets, the parent ids), the four compass offsets (written on the PowerPC side
only: the static initialisers do not run; the host library defines them), the cell and one neighbour cell (type, parent id). What is
compared: the event log and the two last-offset globals. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CTrackCell_CalculateNeighbours"
MASK = 0xFFFFFFFF

FN = 0x100A0BD4
SIZES = {FN: 1732}
OWN = set(SIZES)

GET_NEIGH, IS_TRACK, NUM_NEIGH, MOD_NEIGH = 0x1012FCB8, 0x100A3574, 0x10081744, 0x10081608

ADDR = dict(f485c=0x102E485C, e288=0x1029E288, e28c=0x1029E28C, db68=0x102DDB68,
            g290=0x1029E290, g292=0x1029E292, g294=0x1029E294, g296=0x1029E296,
            g298=0x1029E298, g29a=0x1029E29A, g29c=0x1029E29C, g29e=0x1029E29E)
HW = ["db68", "g290", "g292", "g294", "g296", "g298", "g29a", "g29c", "g29e"]
DELTAS = {0x1034C284: (1, 0), 0x1034C25C: (0, 1), 0x1034C234: (-1, 0), 0x1034C2AC: (0, -1)}
HOST_LAYOUT = [8, 0x10, 0x28]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def tag(e, p):
    return e.u32(p + 0x1C)


def stub_get(e):
    L = e.cl
    d = e.r(4)
    L.rec("GetNeighbouringCell", (tag(e, e.r(3)), s32(e.u32(d)), s32(e.u32(d + 4)), s32(e.r(5))))
    e.set_r(3, e.nbr if L.ret("neigh") else 0)


def stub_is_track(e):
    L = e.cl
    L.rec("IsTrack", (tag(e, e.r(3)),))
    e.set_r(3, L.ret("istrack") & MASK)


def stub_nn(e):
    L = e.cl
    L.rec("NumberOfNeighbours", (tag(e, e.r(3)),))
    e.set_r(3, L.ret("nn") & MASK)


def stub_mod(e):
    L = e.cl
    L.rec("ModifyNeighbours", (tag(e, e.r(3)), e.r(4) & 0xFF))


STUBS.internal(GET_NEIGH, stub_get)
STUBS.internal(IS_TRACK, stub_is_track)
STUBS.internal(NUM_NEIGH, stub_nn)
STUBS.internal(MOD_NEIGH, stub_mod)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def hw(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


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

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def init_deltas(self):
        for a, (x, y) in DELTAS.items():
            self.words(a, [x, y, 0])

    def setnbr(self, n):
        self.e.nbr = n

    def call(self, t, mode, a, b):
        T.call_ppc(self.e, FN, [t, mode & MASK, a & MASK, b & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cn_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cn_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def init_deltas(self):
        pass

    def setnbr(self, n):
        self.nbr = n

    def call(self, t, mode, a, b):
        self.lib.tpw_cn_calc(t, self.nbr, s32(mode), s32(a), s32(b))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cn_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cn_calc.restype = None
    lib.tpw_cn_calc.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("f485c"), [case["f485c"]])
    s.words(s.g("e288"), [case["e288"]])
    s.words(s.g("e28c"), [case["e28c"]])
    for k in HW:
        s.hw(s.g(k), case[k])
    s.init_deltas()
    t = s.alloc(0x40)
    n = s.alloc(0x40)
    s.words(t + 0x1C, [1])
    s.words(n + 0x1C, [2])
    s.words(n + 8, [case["ntype"]])
    s.hw(n + 0x10, case["par"])
    s.setnbr(n)
    return t


def observe(s):
    return {"log": s.log(), "e288": s.rd_word(s.g("e288")), "e28c": s.rd_word(s.g("e28c"))}


def run(s, case):
    try:
        t = build(s, case)
        s.call(t, *case["args"])
        return observe(s)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


VALS = [0, 2, -2, 0x80, 1, 3, -1]


def seq(r, good, bad, p):
    return [good(r) if r.random() < p else bad(r) for _ in range(16)]


def base(r, mode, a, b):
    c = {"args": [mode, a, b]}
    c["f485c"] = r.choice([0, 0, 1])
    c["e288"] = r.choice(VALS) & MASK
    c["e28c"] = r.choice(VALS) & MASK
    for k in HW:
        c[k] = r.choice([0, 1, 2])
    c["par"] = r.choice([0, 1, 2])
    c["ntype"] = 0x19 if r.random() < 0.7 else r.choice([0, 0xB, 0xC, 0x11])
    c["queues"] = {
        "neigh": seq(r, lambda r: 1, lambda r: 0, 0.85),
        "istrack": seq(r, lambda r: r.choice([1, 2]), lambda r: 0, 0.85),
        "nn": seq(r, lambda r: r.choice([0, 1, 3]), lambda r: r.choice([4, 5, 9]), 0.8),
    }
    return c


def edges():
    r = random.Random(5192)
    for mode in [-1, 0, 0xA] + list(range(0xB, 0x1C)) + [0x40]:
        for a, b in [(2, 2), (-2, -2), (2, -2), (-2, 2), (0, 0), (2, 0), (0, -2)]:
            c = base(r, mode, a, b)
            c["f485c"] = 0
            yield c
        for e288, e28c in [(2, 2), (-2, -2), (0x80, 2), (2, 0x80), (-2, 2)]:
            c = base(r, mode, 5, 7)
            c["f485c"] = 1
            c["e288"], c["e28c"] = e288 & MASK, e28c & MASK
            yield c


def rnd(r):
    return base(r, r.choice(list(range(-1, 0x1D))), r.choice(VALS), r.choice(VALS))


TESTS = [
    FnTest("CTrackCell::CalculateNeighbours", ppc, host, edge=edges, rnd=rnd),
]
