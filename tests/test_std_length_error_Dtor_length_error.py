"""Unit v5749, std::length_error::~length_error, against libtpw_unit_std_length_error_Dtor_length_error.dylib
(tests/unit_std_length_error_Dtor_length_error.cpp), program "ltms symbols shared", src/ltms_textformatter_std_length_error_Dtor_length_error.cpp:
  std::length_error::~length_error 0x10008108

A case is an object (or NULL) and the hidden delete flag. The callees outside the unit, the message's destructor (0x10008a10) and
operator delete (0x1000f694), are fakes on both sides that record their arguments (the object and the member at +4 as labels).

What is compared: the answer (this), which vtable the object ends with (the PowerPC TOC +0x104c, +0x103c or +0x102c against the
host's stand-in arrays) and the event log. No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_std_length_error_Dtor_length_error"
MASK = 0xFFFFFFFF

FN = 0x10008108
RC_DTOR = 0x10008a10
OP_DELETE = 0x1000f694
TOC_KIND = {0x102c: "length", 0x103c: "logic", 0x104c: "exception"}

TOC = [0]
STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_rc(e):
    L = e.cl
    TOC[0] = e.r(2)    # r2 inside the call is the TOC (the delete path reloads it from the stack, junk after a fake)
    L.rec("RcDtor", (L.lab(e.r(3)), T.s16(e.r(4))))


def stub_delete(e):
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)),))


STUBS.internal(RC_DTOR, stub_rc)
STUBS.internal(OP_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(16) if case["obj"] else 0
        self.L = e.cl = CL.PpcLog({self.obj: 1, self.obj + 4: 2} if self.obj else {}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def call(self, flag):
        r3, _, _ = T.call_ppc(self.e, FN, [self.obj, flag & MASK])
        toc = TOC[0]
        word = self.e.u32(self.obj) if self.obj else 0
        kind = TOC_KIND.get((word - toc) & MASK, "other" if self.obj else "none")
        return (1 if r3 & MASK == self.obj and self.obj else 0 if r3 == 0 else 2), kind

    def log(self):
        return self.L.log


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_lerr_alloc(16) if case["obj"] else 0
        CL.host_begin(lib, {1: self.obj, 2: self.obj + 8} if self.obj else {}, case["queues"])
        self.vt = {lib.tpw_lerr_addr(k.encode()): k for k in ("length", "logic", "exception")}

    def done(self):
        pass

    def call(self, flag):
        r = self.lib.tpw_lerr_dtor(self.obj, T.s32(flag))
        word = self.lib.tpw_lerr_vptr(self.obj) if self.obj else 0
        kind = self.vt.get(word, "other" if self.obj else "none")
        return (1 if r == self.obj and self.obj else 0 if r == 0 else 2), kind

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lerr_addr.restype, lib.tpw_lerr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lerr_alloc.restype, lib.tpw_lerr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lerr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lerr_dtor.restype, lib.tpw_lerr_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_lerr_vptr.restype, lib.tpw_lerr_vptr.argtypes = C.c_uint64, [C.c_uint64]
    out = (C.c_int64 * 8)()
    n = lib.tpw_lerr_layout(out)
    assert list(out)[:n] == [8], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        answer, kind = s.call(case["flag"])
        return {"answer": answer, "vtable": kind, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make(obj, flag):
    return {"obj": obj, "flag": flag & MASK, "queues": {}}


def edges():
    for obj in (0, 1):
        for flag in (0, 1, -1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x18000, 0xFFFF8000, 0x7FFF0000):
            yield make(obj, flag)


def rnd(r):
    return make(r.choice([0, 1, 1, 1]), r.choice([r.randrange(0, 1 << 32), r.randrange(-3, 4), r.randrange(0, 1 << 16)]))


TESTS = [
    FnTest("std::length_error::~length_error", ppc, host, edge=edges, rnd=rnd),
]
