"""Unit v5692, TPGlobals::~TPGlobals 0x10233378, against libtpw_unit_TPGlobals_Dtor_TPGlobals.dylib
(tests/unit_TPGlobals_Dtor_TPGlobals.cpp), main app, src/TPGlobals_Dtor_TPGlobals.cpp.

A case is an object (zeroed 0x4c90 bytes, or NULL) and the hidden delete flag (a short in r4). The callees outside the unit,
the char and wide string destructors (0x102382f0, 0x10238710) and operator delete (0x10238218), are fakes on both sides that
record the member offset (this - object), so the log proves the 159 destructors run in the same order. The PowerPC stub also
logs a "bad" event when a member destructor is not called with the flag -1 (the host cannot pass one; never expected).
Delete is logged as ("delete", 0) when the object is the one deleted (the stub checks this).

What is compared: the answer (this or NULL) and the event log. No fused operations. Not tested: nothing skipped; the NULL check
and the flag test belong to the deleting variant, which the host wrapper reproduces around the C++ destructor.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_TPGlobals_Dtor_TPGlobals"
MASK = 0xFFFFFFFF

FN = 0x10233378
SIZES = {FN: 2624}
OWN = set(SIZES)
DT_C, DT_W, OP_DELETE = 0x102382F0, 0x10238710, 0x10238218
SIZE = 0x4C90

HOST_LAYOUT = [16424, 21480, 32, 32, 21512]    # the host's member offsets and string sizes (the Mac's are 0x4024, 0x4c7c, 20)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk_dtor(name):
    def stub(e):
        L = e.cl
        off = (e.r(3) - L.base - 0x4024)
        L.rec(name, (off // 20 if off % 20 == 0 else -1000,))
        if s32(e.r(4)) != -1:
            L.rec("bad", (s32(e.r(4)),))
    return stub


def s32(v):
    return T.s32(v)


def stub_delete(e):
    L = e.cl
    L.rec("delete", (0 if (e.r(3) & MASK) == L.base else 1,))


STUBS.internal(DT_C, mk_dtor("dtor_c"))
STUBS.internal(DT_W, mk_dtor("dtor_w"))
STUBS.internal(OP_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_tpg_alloc.restype, lib.tpw_tpg_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_tpg_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_tpg_dtor.restype, lib.tpw_tpg_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_tpg_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def ppc(e, case):
    L = e.cl = CL.PpcLog({}, {})
    saved = STUBS.activate(e)
    try:
        obj = e.alloc(SIZE) if case["obj"] else 0
        L.base = obj
        r3, _, _ = T.call_ppc(e, FN, [obj, case["flag"] & MASK])
        return {"same": 1 if (r3 & MASK) == obj else 0, "null": 1 if (r3 & MASK) == 0 else 0, "log": L.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    bind(lib)
    CL.host_begin(lib, {}, {})
    obj = lib.tpw_tpg_alloc(SIZE) if case["obj"] else 0
    r = lib.tpw_tpg_dtor(obj, s32(case["flag"]))
    return {"same": 1 if r == obj else 0, "null": 1 if r == 0 else 0, "log": CL.host_end(lib)}


FLAGS = [-1, 0, 1, 2, 0x7FFF, 0x8000, 0x10000, 0x10001, -0x8000, 0xFFFF]


def edges():
    for obj in (1, 0):
        for f in FLAGS:
            yield {"obj": obj, "flag": f, "queues": {}}


def rnd(r):
    return {"obj": r.choice([1, 1, 1, 0]), "flag": r.choice(FLAGS + [r.randrange(-70000, 70000)]), "queues": {}}


TESTS = [
    FnTest("TPGlobals::~TPGlobals", ppc, host, edge=edges, rnd=rnd),
]
