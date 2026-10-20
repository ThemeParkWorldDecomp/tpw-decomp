"""Unit v5756, std::operator< on two nocase wide strings, against libtpw_unit_lt__3stdFRCQ23std58basic_stringRCQ23std58basic_s.dylib
(tests/unit_lt__3stdFRCQ23std58basic_stringRCQ23std58basic_s.cpp), main app, src/lt__3stdFRCQ23std58basic_stringRCQ23std58basic_s.cpp:
  __lt<nocase wide string> 0x10225d2c

The callees size (0x10226134), begin (0x102260c8) and compare (0x10225a5c) are fakes on both sides that record their arguments (objects as
labels 1 = a, 2 = b) and answer from the case's queues: Size (b first, then a), Begin, Compare. Compared: the answer and the event log
(order, objects, pos 0, n1, the begin pointer, n2). No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_lt__3stdFRCQ23std58basic_stringRCQ23std58basic_s"
MASK = 0xFFFFFFFF

FN = 0x10225D2C
SIZES = {FN: 140}
CALLEE_SIZE, CALLEE_BEGIN, CALLEE_COMPARE = 0x10226134, 0x102260C8, 0x10225A5C

STUBS = CL.Stubs()
STUBS.real.update(SIZES)

OBJ = {}


def stub_size(e):
    L = e.cl
    L.rec("Size", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("Size") & MASK)


def stub_begin(e):
    L = e.cl
    L.rec("Begin", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("Begin") & MASK)


def stub_compare(e):
    L = e.cl
    L.rec("Compare", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK, e.r(7) & MASK))
    e.set_r(3, L.ret("Compare") & MASK)


STUBS.internal(CALLEE_SIZE, stub_size)
STUBS.internal(CALLEE_BEGIN, stub_begin)
STUBS.internal(CALLEE_COMPARE, stub_compare)


def setup(runner):
    T.install_std_stubs(runner.emu)
    OBJ["A"] = T.persist_alloc(runner.emu, 16)
    OBJ["B"] = T.persist_alloc(runner.emu, 16)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lt_addr.restype, lib.tpw_lt_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lt_less.restype, lib.tpw_lt_less.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    lib.bound = True


def ppc(e, case):
    L = e.cl = CL.PpcLog({OBJ["A"]: 1, OBJ["B"]: 2}, case["queues"])
    saved = STUBS.activate(e)
    try:
        r3, _, _ = T.call_ppc(e, FN, [OBJ["A"], OBJ["B"]])
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"answer": r3 & MASK, "log": L.log}


def host(lib, case):
    bind(lib)
    a, b = lib.tpw_lt_addr(b"A"), lib.tpw_lt_addr(b"B")
    CL.host_begin(lib, {1: a, 2: b}, case["queues"])
    ans = lib.tpw_lt_less(a, b) & MASK
    return {"answer": ans, "log": CL.host_end(lib)}


def mk(r, cmp=None):
    cmpv = r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, r.randrange(-1000, 1000)]) if cmp is None else cmp
    return {"queues": {"Size": [r.choice([0, 1, 5, r.randrange(0, 1000)]), r.choice([0, 1, 7, r.randrange(0, 1000)])],
                       "Begin": [r.choice([0x1000, 0x20000, r.randrange(0, 1 << 32)])],
                       "Compare": [cmpv & MASK]}}


def edges():
    r = random.Random(5756)
    for c in (0, 1, -1, 0x7FFFFFFF, -0x80000000, 2, -2):
        yield mk(r, c)


def rnd(r):
    return mk(r)


TESTS = [
    FnTest("__lt<w,19nocase_wchar_traits,Q23std12allocator<w>>__3stdFRCQ23std58basic_string<w,19nocase_wchar_traits,Q23std12allocator<w>>RCQ23std58basic_string<w,19nocase_wchar_traits,Q23std12allocator<w>>",
           ppc, host, edge=edges, rnd=rnd),
]
