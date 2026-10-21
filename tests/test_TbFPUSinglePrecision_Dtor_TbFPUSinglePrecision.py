"""Unit v5740, the TbFPUSinglePrecision destructor, against libtpw_unit_TbFPUSinglePrecision_Dtor_TbFPUSinglePrecision.dylib
(tests/unit_TbFPUSinglePrecision_Dtor_TbFPUSinglePrecision.cpp), bullfrog, src/bullfrog/WinScrn.cpp:
  TbFPUSinglePrecision::~TbFPUSinglePrecision 0x1006ab38

The world: an object of 12 bytes with mSaved, mSet, mClear (offsets 4, 6, 8), or a NULL this. TbFPUFlag::Reset (0x1001611c) and
operator delete (0x1006b2d4) are fakes on both sides: Reset logs the three fields it sees, delete logs a "delete" event. The
hidden flag in r4 is a short (extsh), with garbage in the upper half. Compared: whether this is returned (non-NULL) and the log.
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_TbFPUSinglePrecision_Dtor_TbFPUSinglePrecision"
MASK = 0xFFFFFFFF

FN = 0x1006ab38
RESET = 0x1001611C
DELETE = 0x1006B2D4

STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_reset(e):
    a = e.r(3)
    e.cl.rec("Reset", (e.u32(a + 4) >> 16, e.u32(a + 4) & 0xFFFF, e.u32(a + 8) >> 16))


def stub_delete(e):
    e.cl.rec("delete", ())


STUBS.internal(RESET, stub_reset)
STUBS.internal(DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc(e, case):
    e.cl = CL.PpcLog({}, {})
    saved = STUBS.activate(e)
    try:
        if case["has"]:
            a = e.alloc(16)
            e.write(a, (0x10073A84).to_bytes(4, "big") + ((case["saved"] << 16) | case["set"]).to_bytes(4, "big")
                    + (case["clear"] << 16).to_bytes(4, "big"))
        else:
            a = 0
        r3, _, _ = T.call_ppc(e, FN, [a, case["flag"]])
        return {"answer": 1 if (r3 & MASK) else 0, "log": e.cl.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    f = lib.tpw_fpusp_dtor
    f.restype, f.argtypes = C.c_uint32, [C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_int32]
    CL.host_begin(lib, {}, {})
    ans = f(case["has"], case["saved"], case["set"], case["clear"], case["flag"] & MASK if case["flag"] < 0x80000000 else case["flag"] - (1 << 32))
    return {"answer": ans, "log": CL.host_end(lib)}


def mk(r, flag, has=1):
    return {"has": has, "flag": flag, "saved": r.randrange(65536), "set": r.randrange(65536), "clear": r.randrange(65536)}


def edges():
    r = random.Random(7)
    for flag in (0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0xFFFF0000, 0xFFFF0001, 0x00018000):
        yield mk(r, flag)
    yield mk(r, 1, 0)
    yield mk(r, 0, 0)


def rnd(r):
    return mk(r, r.choice([0, 1, r.randrange(1 << 32)]), r.choice([1, 1, 1, 0]))


TESTS = [FnTest("TbFPUSinglePrecision::~TbFPUSinglePrecision", ppc, host, edge=edges, rnd=rnd)]
