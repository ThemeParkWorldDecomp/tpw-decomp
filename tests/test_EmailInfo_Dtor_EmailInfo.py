"""Unit v5711, EmailInfo::~EmailInfo, against libtpw_unit_EmailInfo_Dtor_EmailInfo.dylib (tests/unit_EmailInfo_Dtor_EmailInfo.cpp), main app,
src/game/online/email.cpp:
  EmailInfo::~EmailInfo 0x10203aec

A case is an object (or NULL) and the hidden delete flag. The callees (string destructor 0x1000221c, list clear 0x1017fc04, operator
delete 0x10238218) are fakes on both sides that record the offset of their address argument from the object (and the flag).

What is compared: the returned pointer (this or NULL) and the event log. No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_EmailInfo_Dtor_EmailInfo"
MASK = 0xFFFFFFFF

FN = 0x10203AEC
SIZES = {FN: 148}
OWN = set(SIZES)
STRDTOR, LISTCLEAR, OPDELETE = 0x1000221C, 0x1017FC04, 0x10238218
HOST_LAYOUT = [4, 8, 0x14]

STUBS = CL.Stubs()
STUBS.real.update(OWN)
THIS = [0]


def stub_strdtor(e):
    e.cl.rec("StringDtor", ((e.r(3) - THIS[0]) & MASK, T.s16(e.r(4))))


def stub_listclear(e):
    e.cl.rec("ListClear", ((e.r(3) - THIS[0]) & MASK,))


def stub_delete(e):
    e.cl.rec("OperatorDelete", ((e.r(3) - THIS[0]) & MASK,))


STUBS.internal(STRDTOR, stub_strdtor)
STUBS.internal(LISTCLEAR, stub_listclear)
STUBS.imp("operator delete", stub_delete) if False else STUBS.internal(OPDELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ei_alloc.restype, lib.tpw_ei_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ei_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ei_dtor.restype, lib.tpw_ei_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ei_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def ppc(e, case):
    e.cl = CL.PpcLog({}, {})
    saved = STUBS.activate(e)
    try:
        this = e.alloc(0x40) if case["obj"] else 0
        THIS[0] = this
        r3, _, _ = T.call_ppc(e, FN, [this, case["flag"] & MASK])
        return {"same": (r3 & MASK) == this, "log": e.cl.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    bind(lib)
    CL.host_begin(lib, {}, {})
    this = lib.tpw_ei_alloc(0x40) if case["obj"] else 0
    r = lib.tpw_ei_dtor(this, T.s32(case["flag"]))
    return {"same": r == this, "log": CL.host_end(lib)}


def edges():
    for obj in (0, 1):
        for flag in (0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0xFFFE, 0x10000, 0x10001, 0xFFFF0000, 0x00018000):
            yield {"obj": obj, "flag": flag}


def rnd(r):
    return {"obj": r.choice([0, 1, 1, 1]), "flag": r.choice([r.randrange(0, 1 << 32), r.randrange(0, 4), 0xFFFF])}


TESTS = [
    FnTest("EmailInfo::~EmailInfo", ppc, host, edge=edges, rnd=rnd),
]
