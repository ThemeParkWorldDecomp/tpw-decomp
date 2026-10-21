"""Unit v5754, TakeLoanOK 0x101a3ee0, against libtpw_unit_TakeLoanOK.dylib (tests/unit_TakeLoanOK.cpp), main app, src/game/ui/ui_loansPanel.cpp.

A case is the globals gGameWorld and gLoansPanel (small fake pointer values, never dereferenced), the loan parameter and the answers of the
four callees (GetBankAccount, ApplyForLoan, GetWindow, FillLenderList), each a fake on both sides recording its arguments as raw values.
What is compared: the answer (1) and the event log. No fused operations. Not tested: a NULL gLoansPanel (the original calls through it).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_TakeLoanOK"
MASK = 0xFFFFFFFF

FN = 0x101a3ee0
OWN = {FN}
GET_BANK, APPLY, GET_WINDOW, FILL = 0x1013edfc, 0x100f3db8, 0x101dbe40, 0x101a4a38
ADDR = dict(world=0x1037ebfc, panel=0x10394980)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_get_bank(e):
    e.cl.rec("GetBankAccount", (e.r(3),))
    e.set_r(3, e.cl.ret("GetBankAccount") & MASK)


def stub_apply(e):
    e.cl.rec("ApplyForLoan", (e.r(3), e.r(4)))
    e.set_r(3, e.cl.ret("ApplyForLoan") & 0xFF)


def stub_get_window(e):
    e.cl.rec("GetWindow", (e.r(3), T.s32(e.r(4))))
    e.set_r(3, e.cl.ret("GetWindow") & MASK)


def stub_fill(e):
    e.cl.rec("FillLenderList", (e.r(3),))


STUBS.internal(GET_BANK, stub_get_bank)
STUBS.internal(APPLY, stub_apply)
STUBS.internal(GET_WINDOW, stub_get_window)
STUBS.internal(FILL, stub_fill)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def put(self, name, v):
        self.e.write(ADDR[name], (v & MASK).to_bytes(4, "big"))

    def log(self):
        return self.L.log

    def call(self, p):
        r3, _, _ = T.call_ppc(self.e, FN, [p & MASK])
        return r3 & MASK


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def put(self, name, v):
        a = self.lib.tpw_tlo_addr(name.encode())
        C.memmove(a, (v & MASK).to_bytes(4, "little") + b"\0\0\0\0", 8)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, p):
        return self.lib.tpw_tlo_run(p & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_tlo_addr.restype, lib.tpw_tlo_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_tlo_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_tlo_run.restype, lib.tpw_tlo_run.argtypes = C.c_uint32, [C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_tlo_layout(out)
    assert list(out)[:n] == [], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.put("world", case["world"])
        s.put("panel", case["panel"])
        ans = s.call(case["param"])
        return {"answer": ans, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make(r, param=None):
    return {
        "world": r.choice([0x5000, 0x5010, r.randrange(1, 0x10000)]),
        "panel": r.choice([0x7000, r.randrange(1, 0x10000)]),
        "param": r.choice([0, 1, 2, 0xFFFFFFFF, r.randrange(0, 100)]) if param is None else param,
        "queues": {"GetBankAccount": [r.choice([0x6000, r.randrange(1, 0x10000)])],
                   "ApplyForLoan": [r.choice([0, 1])],
                   "GetWindow": [r.choice([0, 0x8000, r.randrange(1, 0x10000)])]},
    }


def edges():
    r = random.Random(5754)
    for p in (0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
        yield make(r, p)


def rnd(r):
    return make(r)


TESTS = [FnTest("TakeLoanOK", ppc, host, edge=edges, rnd=rnd)]
