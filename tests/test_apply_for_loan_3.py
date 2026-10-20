"""Unit v5036, apply_for_loan_3 and apply_for_loan_4 against libtpw_unit_apply_for_loan_3.dylib (tests/unit_apply_for_loan_3.cpp), main app,
src/ui_Postcard_panel_spawn_handyman_apply_for_loan_3.cpp:
  apply_for_loan_3 0x1014e380, apply_for_loan_4 0x1014e41c

Callees GetBankAccount, ApplyForLoan, CTRACE are fakes on both sides recording their arguments; ApplyForLoan answers from the case.
The world of a case: gGameWorld (a pointer). Compared: the answer, the event log. No fused operations.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_apply_for_loan_3"
MASK = 0xFFFFFFFF

FN_3, FN_4 = 0x1014E380, 0x1014E41C
SIZES = {FN_3: 116, FN_4: 116}
OWN = set(SIZES)

GETBANK, APPLY, CTRACE = 0x1013EDFC, 0x100F3DB8, 0x1006FCBC
ADDR = dict(gGameWorld=0x1037EBFC)

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(GETBANK, CL.recorder("GetBankAccount", "L", ret=lambda e, log: 0x1000))
STUBS.internal(APPLY, CL.recorder("ApplyForLoan", "Lu", ret="i"))
STUBS.internal(CTRACE, CL.recorder("CTRACE", "us"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def setworld(self, v):
        self.e.write(ADDR["gGameWorld"], v.to_bytes(4, "big"))

    def call(self, n):
        r3, _, _ = T.call_ppc(self.e, FN_3 if n == 3 else FN_4, [])
        return r3 & MASK

    def log(self):
        return self.e.cl.log


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def setworld(self, v):
        a = self.lib.tpw_afl_addr(b"gGameWorld")
        C.memmove(a, v.to_bytes(8, "little"), 8)

    def call(self, n):
        return (self.lib.tpw_afl_3() if n == 3 else self.lib.tpw_afl_4()) & MASK

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_afl_addr.restype, lib.tpw_afl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_afl_3.restype = C.c_uint32
    lib.tpw_afl_4.restype = C.c_uint32
    lib.bound = True


def run(s, case):
    try:
        s.setworld(0x5000)
        ans = s.call(case["fn"])
        return {"answer": ans, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(fn, ans):
    return {"fn": fn, "queues": {"ApplyForLoan": [ans]}}


def edges3():
    for a in (0, 1, 2, 0x100):
        yield mk(3, a)


def edges4():
    for a in (0, 1, 2, 0x100):
        yield mk(4, a)


TESTS = [
    FnTest("apply_for_loan_3", ppc, host, edge=edges3, rnd=lambda r: mk(3, r.choice([0, 1]))),
    FnTest("apply_for_loan_4", ppc, host, edge=edges4, rnd=lambda r: mk(4, r.choice([0, 1]))),
]
