"""Unit v5738, LbMaths_Random, against libtpw_unit_LbMaths_Random.dylib (tests/unit_LbMaths_Random.cpp), src/AudioPlaceHolder_LbMaths_Random.cpp:
  LbMaths_Random 0x10018cf4

A case is a seed word in memory (built through Side.words) and a call with its address. RotateRight<long> (0x10018d6c) is a fake on both
sides recording its two arguments. Compared: the answer, the seed word after the call, the event log. No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_LbMaths_Random"
MASK = 0xFFFFFFFF

FN = 0x10018CF4
SIZES = {FN: 80}
OWN = set(SIZES)
ROTATE = 0x10018D6C

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_rotate(e):
    L = e.cl
    L.rec("RotateRight", (T.s32(e.r(3)), T.s32(e.r(4))))
    e.set_r(3, L.ret("RotateRight") & MASK)


STUBS.internal(ROTATE, stub_rotate)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

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

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, p):
        r3, _, _ = T.call_ppc(self.e, FN, [p])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_lbr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, p):
        return self.lib.tpw_lbr_random(p) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lbr_alloc.restype, lib.tpw_lbr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lbr_random.restype, lib.tpw_lbr_random.argtypes = C.c_uint32, [C.c_uint64]
    lib.bound = True


def run(s, case):
    try:
        p = s.alloc(8)
        s.words(p, [case["seed"]])
        ans = s.call(p)
        return {"answer": ans, "seed": s.rd_word(p), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, seed):
    return {"seed": seed & MASK, "queues": {"RotateRight": [r.randrange(1 << 32)]}}


def edges():
    r = random.Random(738)
    for x in (0, 1, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x19660D, 0x3C6EF35F, 0xC391 , 0x12345678, 0xDEADBEEF):
        yield mk(r, x)


def rnd(r):
    return mk(r, r.randrange(1 << 32))


TESTS = [
    FnTest("LbMaths_Random", ppc, host, edge=edges, rnd=rnd),
]
