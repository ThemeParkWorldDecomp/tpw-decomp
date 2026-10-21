"""Unit v5523, std::ios_base::setstate, against libtpw_unit_std_ios_base_setstate.dylib (tests/unit_std_ios_base_setstate.cpp), main app,
src/game/MslSupport.cpp:
  std::ios_base::setstate 0x102077c0

A case is an ios block (stream buffer pointer 0x24, state 0x34, exception mask 0x38) and the bits to add. The block is built twice by
one piece of code. The only callee outside the unit is __throw, a fake on both sides that records the type string, the object it is
given (vtable, destructor vector, the count and the message copy). strlen, strcpy and the operators are the real/standard ones.

What is compared: the state and mask words, the answer-less call, and the event log. No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_ios_base_setstate"
MASK = 0xFFFFFFFF

FN = 0x102077C0
VTBL, DTORS, TYPE = 0x1029D084, 0x102661D0, 0x10250098
HOST_LAYOUT = [0x24, 0x34, 0x38]
TYPE_TEXT = b"!std::exception!!std::ios_base::failure!!"

STUBS = CL.Stubs()


def stub_throw(e):
    L = e.cl
    typ = e.cstr(e.r(3))
    if isinstance(typ, str):
        typ = typ.encode()
    obj = e.r(4)
    msg = e.r(obj + 4) if False else e.u32(obj + 4)
    cnt = e.u32(obj + 8)
    text = bytes(e.cstr(msg), "latin1") if isinstance(e.cstr(msg), str) else bytes(e.cstr(msg))
    text = (text + b"\0").ljust(28, b"\0")[:28]
    words = [int.from_bytes(text[i:i + 4], "big") for i in range(0, 28, 4)]
    L.rec("__throw", tuple([1 if typ == TYPE_TEXT else 0, 1 if e.u32(obj) == VTBL else 0, 1 if e.r(5) == DTORS else 0,
                            e.u32(cnt)] + words))


STUBS.imp("__throw", stub_throw)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


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

    def call(self, ios, bits):
        T.call_ppc(self.e, FN, [ios & MASK, bits & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_setstate_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, ios, bits):
        self.lib.tpw_setstate_call(ios, T.s32(bits))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_setstate_alloc.restype, lib.tpw_setstate_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_setstate_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_setstate_call.restype, lib.tpw_setstate_call.argtypes = None, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_setstate_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        ios = s.alloc(0x40)
        s.words(ios + 0x24, [case["sb"]])
        s.words(ios + 0x34, [case["state"], case["mask"]])
        s.call(ios, case["bits"])
        return {"words": s.rd_words(ios, 16), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(sb, state, mask, bits):
    return {"queues": {}, "sb": sb, "state": state, "mask": mask, "bits": bits}


def edges():
    for sb in (0, 0x1000):
        for state in (0, 2, 5):
            for mask in (0, 1, 2, 4, 6, 0x80000000, MASK):
                for bits in (0, 1, 2, 4, 8, 0x80000000):
                    yield mk(sb, state, mask, bits)


def rnd(r):
    return mk(r.choice([0, 0x2000, r.randrange(1, MASK)]), r.choice([0, r.randrange(0, 16), r.getrandbits(32)]),
              r.choice([0, r.randrange(0, 16), r.getrandbits(32)]), r.choice([0, r.randrange(0, 16), r.getrandbits(32)]))


TESTS = [
    FnTest("std::ios_base::setstate", ppc, host, edge=edges, rnd=rnd),
]
