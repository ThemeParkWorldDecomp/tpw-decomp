"""Unit v5128, the online globe's city list callback, against libtpw_unit_PostGetCityListCallbackFunction.dylib
(tests/unit_PostGetCityListCallbackFunction.cpp), main app ("SimTheme Park"), src/lobby_globe_close_PostGetCityListCallbackFunction.cpp:
  PostGetCityListCallbackFunction 0x100ad6dc

A case is an error code, a context object and a list pointer (NULL or a block). The context is built in emulator memory with a vtable whose
slots 0x30 and 0x34 hold marker values; the glue __ptr_glue (0x1023baa8) is faked and records which marker r12 held, with r3 and r4. On the
host the context is an object with two recording virtual functions. Compared: the event log (which slot, the context and, for slot 0x30,
the list). No fused operations. Not tested: a NULL context (the original dereferences it unconditionally).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_PostGetCityListCallbackFunction"
MASK = 0xFFFFFFFF

FN = 0x100ad6dc
PTR_GLUE = 0x1023baa8
MARK_RECEIVED, MARK_FAILED = 0xA0A0, 0xB0B0

STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_glue(e):
    L = e.cl
    r12 = e.r(12) & MASK
    if r12 == MARK_RECEIVED:
        L.rec("Received", (L.lab(e.r(3)), L.lab(e.r(4))))
    elif r12 == MARK_FAILED:
        L.rec("Failed", (L.lab(e.r(3)),))
    else:
        L.rec("Unknown", (r12,))


STUBS.internal(PTR_GLUE, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        vt = e.alloc(0x40)
        e.write(vt + 0x30, MARK_RECEIVED.to_bytes(4, "big") + MARK_FAILED.to_bytes(4, "big"))
        self.ctx = e.alloc(16)
        e.write(self.ctx, vt.to_bytes(4, "big"))
        self.lst = e.alloc(16) if case["list"] else 0
        fixed = {self.ctx: 1}
        if self.lst:
            fixed[self.lst] = 2
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def run(self, case):
        try:
            T.call_ppc(self.e, FN, [case["error"] & MASK, self.ctx, self.lst])
            return {"log": self.L.log}
        finally:
            CL.Stubs.deactivate(self.e, self.saved)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_pgcl_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "call").restype, getattr(lib, p + "call").argtypes = None, [C.c_int32, C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == [1], list(out)[:n]
    lib.bound = True


def ppc(e, case):
    return Ppc(e, case).run(case)


def host(lib, case):
    bind(lib)
    ctx = lib.tpw_pgcl_addr(b"screen")
    lst = lib.tpw_pgcl_alloc(16) if case["list"] else 0
    fixed = {1: ctx}
    if lst:
        fixed[2] = lst
    CL.host_begin(lib, fixed, case["queues"])
    lib.tpw_pgcl_call(T.s32(case["error"]), ctx, lst)
    return {"log": CL.host_end(lib)}


def mk(error, lst):
    return {"error": error & MASK, "list": lst, "queues": {}}


def edges():
    for err in (0, 1, -1, 2, 0x7FFFFFFF, 0x80000000, 0x10000, 0x100):
        for lst in (False, True):
            yield mk(err, lst)


def rnd(r):
    return mk(r.choice([0, 0, 1, -1, r.getrandbits(32)]), r.random() < 0.7)


TESTS = [
    FnTest("PostGetCityListCallbackFunction", ppc, host, edge=edges, rnd=rnd),
]
