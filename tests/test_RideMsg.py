"""Unit v5029, the five research panel bar message functions, against libtpw_unit_RideMsg.dylib (tests/unit_RideMsg.cpp), main app
("SimTheme Park"), src/game/ui/ui_researchPanel.cpp:
  RideMsg 0x101b4bb0, UpgMsg 0x101b4c70, ShopMsg 0x101b4d30, ShowMsg 0x101b4df0, FeatureMsg 0x101b4eb0

A case is a call (window, message, a, b). TankButton and AnimatingPumpMsg are fakes on both sides recording their arguments;
AnimatingPumpMsg answers from the case's queue. Compared: the answer and the event log. No fused operations. Not tested: nothing.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RideMsg"
MASK = 0xFFFFFFFF

FUNCS = [("RideMsg", 0x101b4bb0), ("UpgMsg", 0x101b4c70), ("ShopMsg", 0x101b4d30), ("ShowMsg", 0x101b4df0), ("FeatureMsg", 0x101b4eb0)]
TANK = 0x101b4224
PUMP = 0x101b49d0

STUBS = CL.Stubs()
STUBS.real.update(a for _, a in FUNCS)


def stub_tank(e):
    e.cl.rec("TankButton", (s32(e.r(3)), e.cl.lab(e.r(4)), s32(e.r(5))))


def stub_pump(e):
    L = e.cl
    L.rec("AnimatingPumpMsg", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("AnimatingPumpMsg") & MASK)


def s32(v):
    return T.s32(v)


STUBS.internal(TANK, stub_tank)
STUBS.internal(PUMP, stub_pump)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rmsg_call.restype = C.c_uint32
    lib.tpw_rmsg_call.argtypes = [C.c_int32, C.c_uint32, C.c_int32, C.c_int32, C.c_int32]
    lib.bound = True


def ppc_for(op):
    def ppc(e, case):
        e.cl = CL.PpcLog({}, case["queues"])
        saved = STUBS.activate(e)
        try:
            r3, _, _ = T.call_ppc(e, FUNCS[op][1], [case["w"], case["m"] & MASK, case["a"] & MASK, case["b"] & MASK])
            return {"answer": r3 & MASK, "log": e.cl.log}
        finally:
            CL.Stubs.deactivate(e, saved)
    return ppc


def host_for(op):
    def host(lib, case):
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        ans = lib.tpw_rmsg_call(op, case["w"], s32(case["m"]), s32(case["a"]), s32(case["b"])) & MASK
        return {"answer": ans, "log": CL.host_end(lib)}
    return host


def mk(r, m=None, a=None):
    return {"queues": {"AnimatingPumpMsg": [r.randrange(0, 1 << 32)]},
            "w": r.choice([0, 0x1000, 0x2345678]),
            "m": r.choice([0x100, 0x100, 0, 0xff, 0x101, -1, r.randrange(0, 1 << 32)]) if m is None else m,
            "a": r.choice([0xd2d03, 0xd2d03, 0xd2d02, 0xd2d04, 0, -1, r.randrange(0, 1 << 32)]) if a is None else a,
            "b": r.randrange(0, 1 << 32)}


def edges_for(seed):
    def edges():
        r = random.Random(seed)
        for m in (0x100, 0xff, 0x101, 0, -1):
            for a in (0xd2d03, 0xd2d02, 0xd2d04, 0, -1):
                yield mk(r, m, a)
    return edges


def rnd_for(r):
    return mk(r)


TESTS = [FnTest(name, ppc_for(i), host_for(i), edge=edges_for(i + 1), rnd=rnd_for) for i, (name, _) in enumerate(FUNCS)]
