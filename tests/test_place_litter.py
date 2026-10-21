"""Unit v5299, the litter cheats, against libtpw_unit_place_litter.dylib (tests/unit_place_litter.cpp), main app
("SimTheme Park"), src/game/keyboard.cpp:
  place_litter 0x1014dac8, place_bomb 0x1014db74

A case is a world and a call. The world is written twice by ONE piece of Python code: in emulator memory at the Mac's addresses and in
the host library's memory. CWorld::GetRand and CMapCell::PlaceLitter are fakes on both sides recording their arguments (the cell as its
offset from the world pointer); GetRand answers from the case's queue.

The world of a case: gGameWorld (a pointer never dereferenced), gIntrface's halfword at +0x24, gLitterKindCount. What is compared: the
answer and the event log. No fused operations. Not tested: gLitterKindCount == 1 (the original divides by 0).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_place_litter"
MASK = 0xFFFFFFFF

FN_LITTER, FN_BOMB = 0x1014dac8, 0x1014db74
SIZES = {FN_LITTER: 136, FN_BOMB: 92}
OWN = set(SIZES)

GETRAND = 0x1013b1e4
PLACE_LITTER = 0x10103010

ADDR = dict(count=0x102a0b20, intrface=0x102dc1ac, world=0x1037ebfc)
HOST_LAYOUT = [48, 68]
WORLD = 0x20000000


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_getrand(e):
    e.cl.rec("GetRand", (e.r(3),))
    e.set_r(3, e.cl.ret("GetRand") & MASK)


def stub_place(e):
    e.cl.rec("PlaceLitter", (s32(e.r(3) - WORLD), s32(e.r(4))))


STUBS.internal(GETRAND, stub_getrand)
STUBS.internal(PLACE_LITTER, stub_place)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))


class Ppc(Side):
    fmt = ">I"
    f24 = 0x24

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ptr(self, a, v):
        self.words(a, [v])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op):
        return T.call_ppc(self.e, FN_LITTER if op == "litter" else FN_BOMB, [])[0] & MASK


class Host(Side):
    fmt = "<I"
    f24 = 48

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_pl_addr(name.encode())

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        return (self.lib.tpw_pl_litter() if op == "litter" else self.lib.tpw_pl_bomb()) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, "tpw_pl_addr").restype, getattr(lib, "tpw_pl_addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, "tpw_pl_alloc").restype, getattr(lib, "tpw_pl_alloc").argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pl_litter.restype = C.c_uint32
    lib.tpw_pl_bomb.restype = C.c_uint32
    out = (C.c_int64 * 32)()
    n = lib.tpw_pl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.ptr(s.g("world"), WORLD)
    s.half(s.g("intrface") + s.f24, case["id"])
    s.words(s.g("count"), [case["count"]])


def run(s, case):
    try:
        build(s, case)
        answer = s.call(case["op"])
        return {"answer": answer, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


IDS = [0, 1, 2, 0x7FFF, 0x8000, 0xFFFF]
COUNTS = [0, 2, 3, 6, 7, 100, 0xFFFFFFFF, 0x80000000]
RANDS = [0, 1, 5, 6, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]


def mk(op, id, count, rand):
    return {"op": op, "id": id, "count": count, "queues": {"GetRand": [rand]}}


def rid(r):
    return r.choice(IDS + [r.randrange(0x10000)])


def rcount(r):
    c = r.choice(COUNTS + [r.randrange(2, 20)])
    return c


def litter_edges():
    for i in IDS:
        yield mk("litter", i, 6, 12345)
    for c in COUNTS:
        for rd in RANDS:
            yield mk("litter", 1, c, rd)


def litter_random(r):
    return mk("litter", rid(r), rcount(r), r.choice(RANDS + [r.getrandbits(32)]))


def bomb_edges():
    for i in IDS:
        yield mk("bomb", i, 6, 0)


def bomb_random(r):
    return mk("bomb", rid(r), rcount(r), r.getrandbits(32))


TESTS = [
    FnTest("place_litter", ppc, host, edge=litter_edges, rnd=litter_random),
    FnTest("place_bomb", ppc, host, edge=bomb_edges, rnd=bomb_random),
]
