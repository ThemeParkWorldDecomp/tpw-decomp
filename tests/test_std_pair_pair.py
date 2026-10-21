"""Unit v5719, the theme map pair's copy constructor, against libtpw_unit_std_pair_pair.dylib (tests/unit_std_pair_pair.cpp), main app,
src/game/MslSupport.cpp:
  std::pair<const TbDynamicStringTemplate<char>, CFSGlobalParkData*>::pair 0x1016f080

A case: a source pair and a destination pair in fresh memory (the Mac's record pointer at offset 0x14, the host's at its own offset). The key
copy constructor (0x10238308) is a fake on both sides recording (this, source) as labels. Compared: the log, the destination's record pointer,
the source's record pointer. No fused operations. Not tested: the returned this (the host declaration is void).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_pair_pair"
MASK = 0xFFFFFFFF

FN_PAIR = 0x1016F080
SIZES = {FN_PAIR: 72}
KEY_COPY = 0x10238308
MAC_SECOND = 0x14

STUBS = CL.Stubs()
STUBS.real.update(SIZES)


def stub_key_copy(e):
    L = e.cl
    L.rec("KeyCopy", (L.lab(e.r(3)), L.lab(e.r(4))))


STUBS.internal(KEY_COPY, stub_key_copy)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    fmt = ">I"
    off = MAC_SECOND

    def __init__(self, e, case):
        self.e = e
        self.dst = e.alloc(0x40)
        self.src = e.alloc(0x40)
        self.L = e.cl = CL.PpcLog({self.dst: 1, self.src: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def write(self, a, d):
        self.e.write(a, d)

    def read(self, a, n):
        return self.e.read(a, n)

    def log(self):
        return self.L.log

    def call(self):
        with_ = T.call_ppc(self.e, FN_PAIR, [self.dst, self.src])
        return with_[0] & MASK


class Host:
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = HOST_OFF[0]
        self.dst = lib.tpw_pair_alloc(0x40)
        self.src = lib.tpw_pair_alloc(0x40)
        CL.host_begin(lib, {1: self.dst, 2: self.src}, case["queues"])

    def done(self):
        pass

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def read(self, a, n):
        return C.string_at(a, n)

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_pair_copy(self.dst, self.src)
        return self.dst


HOST_OFF = [0]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_pair_alloc.restype, lib.tpw_pair_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pair_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pair_copy.restype, lib.tpw_pair_copy.argtypes = None, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_pair_layout(out)
    HOST_OFF[0] = out[0]
    assert n == 2 and out[0] >= 4, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.write(s.dst + s.off, struct.pack(s.fmt, case["dst"]))
        s.write(s.src + s.off, struct.pack(s.fmt, case["src"]))
        s.call()
        return {"log": s.log(),
                "dst": struct.unpack(s.fmt, s.read(s.dst + s.off, 4))[0],
                "src": struct.unpack(s.fmt, s.read(s.src + s.off, 4))[0]}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make(src, dst):
    return {"queues": {}, "src": src & MASK, "dst": dst & MASK}


def edges():
    for src, dst in ((0, 0), (1, 0), (0xFFFFFFFF, 0), (0x12345678, 0xDEADBEEF), (0, 0xFFFFFFFF), (0x80000000, 1)):
        yield make(src, dst)


def rnd(r):
    return make(r.getrandbits(32), r.getrandbits(32))


TESTS = [
    FnTest("std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>::pair", ppc, host, edge=edges, rnd=rnd),
]
