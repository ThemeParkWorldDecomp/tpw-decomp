"""Unit v5704, sAdvisorStore::~sAdvisorStore 0x10032358, against libtpw_unit_sAdvisorStore_Dtor_sAdvisorStore.dylib, main app,
src/TPAdvisor_model_sAdvisorStore_Dtor_sAdvisorStore.cpp.

The original is called with a NULL or a built object (the CLight part holds valid pointers so its vtable stores land in blocks)
and the hidden short flag. The only callee, operator delete, is a fake on both sides recording "delete"; it runs when the
flag (low 16 bits, signed) is positive. Compared: the answer (this) and the event log. Not compared: the PowerPC object's
vtable stores (the host object has native pointers and a compiler-made CLight destructor). No fused operations.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sAdvisorStore_Dtor_sAdvisorStore"
MASK = 0xFFFFFFFF
FN = 0x10032358
OPERATOR_DELETE = 0x10238218

STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


STUBS.internal(OPERATOR_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc(e, case):
    L = e.cl = CL.PpcLog({}, {})
    saved = STUBS.activate(e)
    try:
        this = 0
        if case["obj"]:
            this = e.alloc(0x100)
            b1, b2, q = e.alloc(0x80), e.alloc(0x20), e.alloc(0x20)
            e.write(this + 0x48, b1.to_bytes(4, "big"))
            e.write(this + 0x58, q.to_bytes(4, "big"))
            e.write(b1, b2.to_bytes(4, "big"))
        r3, _, _ = T.call_ppc(e, FN, [this, case["flag"] & MASK])
        return {"answer": r3 & MASK == this, "log": L.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    lib.tpw_sas_alloc.restype, lib.tpw_sas_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sas_dtor.restype, lib.tpw_sas_dtor.argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    CL.host_begin(lib, {}, {})
    this = lib.tpw_sas_alloc(0x100) if case["obj"] else 0
    r = lib.tpw_sas_dtor(this, case["flag"])
    return {"answer": r == (this & MASK), "log": CL.host_end(lib)}


FLAGS = [0, 1, -1, 0x7FFF, 0x8000, 0x10000, 0x10001, -0x8000, 2]


def edges():
    for obj in (0, 1):
        for f in FLAGS:
            yield {"obj": obj, "flag": f}


def rnd(r):
    return {"obj": r.choice([0, 1, 1]), "flag": r.choice(FLAGS + [r.randrange(-70000, 70000)])}


TESTS = [FnTest("sAdvisorStore::~sAdvisorStore", ppc, host, edge=edges, rnd=rnd)]
