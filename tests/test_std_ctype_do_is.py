"""Unit v5735, std::ctype<wchar_t>::do_is 0x102208d4, against libtpw_unit_std_ctype_do_is.dylib (tests/unit_std_ctype_do_is.cpp).

A case is a mask, a character and the answer classify gives. classify (glue 0x1023b140) is a recording fake on both sides (object label,
character) answering from the queue. Compared: the bool answer (low byte) and the call log. No fused operations. Not tested: nothing.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_ctype_do_is"
MASK = 0xFFFFFFFF
FN = 0x102208d4
CLASSIFY = 0x1023b140

STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_classify(e):
    L = e.cl
    L.rec("classify", (L.lab(e.r(3)), e.r(4) & 0xFFFF))
    e.set_r(3, L.ret("classify") & MASK)


STUBS.internal(CLASSIFY, stub_classify)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ctype_alloc.restype, lib.tpw_ctype_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ctype_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ctype_do_is.restype, lib.tpw_ctype_do_is.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32, C.c_uint32]
    lib.bound = True


def ppc(e, case):
    e.cl = CL.PpcLog({}, case["queues"])
    saved = STUBS.activate(e)
    try:
        this = e.alloc(16)
        r3, _, _ = T.call_ppc(e, FN, [this, case["mask"], case["c"]])
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"answer": r3 & 0xFF, "log": e.cl.log}


def host(lib, case):
    bind(lib)
    CL.host_begin(lib, {}, case["queues"])
    this = lib.tpw_ctype_alloc(16)
    r = lib.tpw_ctype_do_is(this, case["mask"], case["c"])
    return {"answer": r & 0xFF, "log": CL.host_end(lib)}


def mk(mask, c, ans):
    return {"mask": mask & MASK, "c": c & 0xFFFF, "queues": {"classify": [ans & MASK]}}


def edges():
    for mask, ans in [(0, 0), (0, MASK), (1, 0), (1, 1), (1, 2), (0x80000000, 0x80000000), (0x80000000, 0x7FFFFFFF),
                      (MASK, 1), (MASK, 0), (0x100, 0x1FF), (0x100, 0xFF), (6, 4), (6, 8)]:
        for c in (0, 0x41, 0xFFFF, 0x8000):
            yield mk(mask, c, ans)


def rnd(r):
    mask = r.choice([r.getrandbits(32), 1 << r.randrange(32), 0, r.getrandbits(4)])
    ans = r.choice([r.getrandbits(32), 1 << r.randrange(32), 0, mask, r.getrandbits(4)])
    return mk(mask, r.getrandbits(16), ans)


TESTS = [
    FnTest("std::ctype<wchar_t>::do_is", ppc, host, edge=edges, rnd=rnd),
]
