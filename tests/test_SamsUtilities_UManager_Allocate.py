"""Unit v5123, SamsUtilities::UManager<UAllocator>::Allocate, against libtpw_unit_SamsUtilities_UManager_Allocate.dylib
(tests/unit_SamsUtilities_UManager_Allocate.cpp), src/TbMpegFilePlayer_SamsUtilities_UManager_Allocate.cpp:
  SamsUtilities::UManager<SamsUtilities::UAllocator>::Allocate 0x1002bc7c

A case is a manager object (one word) holding a stale value and a call. The two callees (0x1002eac4 allocation of 8 bytes, 0x1002ec8c
initialiser) are fakes on both sides that record their arguments; the allocation answers from the case's queue (0 or a word).
Compared: the object's word and the event log (the initialiser runs only for a non-zero answer). No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_SamsUtilities_UManager_Allocate"
MASK = 0xFFFFFFFF

FN_ALLOCATE = 0x1002BC7C
SIZES = {FN_ALLOCATE: 84}
OWN = set(SIZES)
CALLEE_MAKE = 0x1002EAC4
CALLEE_INIT = 0x1002EC8C

HOST_LAYOUT = [0]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_make(e):
    L = e.cl
    L.rec("Make", (T.s32(e.r(3)),))
    e.set_r(3, L.ret("Make") & MASK)


def stub_init(e):
    L = e.cl
    L.rec("Init", (L.lab(e.r(3)),))


STUBS.internal(CALLEE_MAKE, stub_make)
STUBS.internal(CALLEE_INIT, stub_init)


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

    def call(self, obj):
        T.call_ppc(self.e, FN_ALLOCATE, [obj])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_uma_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, obj):
        self.lib.tpw_uma_allocate(obj)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_uma_alloc.restype, lib.tpw_uma_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_uma_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uma_allocate.restype, lib.tpw_uma_allocate.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_uma_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        obj = s.alloc(16)
        s.words(obj, [case["stale"]])
        s.call(obj)
        return {"word": s.rd_word(obj), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(stale, answer):
    return {"stale": stale, "queues": {"Make": [answer]}}


def edges():
    for stale in (0, 0xDEADBEEF):
        for answer in (0, 1, 0x10203040, 0x7FFFFFF0):
            yield mk(stale, answer)


def rnd(r):
    return mk(r.getrandbits(32), r.choice([0, r.randrange(1, 1 << 31)]))


TESTS = [
    FnTest("SamsUtilities::UManager<SamsUtilities::UAllocator>::Allocate", ppc, host, edge=edges, rnd=rnd),
]
