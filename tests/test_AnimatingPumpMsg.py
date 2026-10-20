"""Unit v5197, AnimatingPumpMsg 0x101b49d0, against libtpw_unit_AnimatingPumpMsg.dylib (tests/unit_AnimatingPumpMsg.cpp), main app
("SimTheme Park"), src/CreateResearchPanel_RideMsg_AnimatingPumpMsg.cpp.

A case is a call with a fake window address (never dereferenced). The callees getExtraParam, setExtraParam, GetWindow, SetFrame,
IveGotTicks and CallDefaultWindowHandler are fakes on both sides that record every argument; getExtraParam, IveGotTicks and the default
handler answer from the case's queues. Compared: the answer and the whole event log.

No fused operations. Not tested: a mode above 3 (the original then reads the caller's r29, an uninitialised register).
"""
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_AnimatingPumpMsg"
MASK = 0xFFFFFFFF

FN = 0x101B49D0
SIZES = {FN: 420}
OWN = set(SIZES)

GEP, SETP, GETWIN, SETFRAME, TICKS, DEFH = 0x101DD030, 0x101DCFD0, 0x101DBE40, 0x101DB7DC, 0x101CAC3C, 0x101DE318
WIN = 0x6000
SUB = 0x7000

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def stub_get(e):
    L = e.cl
    idx = T.s16(e.r(4))
    L.rec("get", (L.lab(e.r(3)), idx))
    e.set_r(3, L.ret("gep%d" % idx) & MASK)


def stub_set(e):
    L = e.cl
    L.rec("set", (L.lab(e.r(3)), T.s16(e.r(4)), s32(e.r(5))))
    e.set_r(3, 0)


def stub_getwin(e):
    L = e.cl
    L.rec("getwin", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, SUB)


def stub_setframe(e):
    L = e.cl
    L.rec("setframe", (L.lab(e.r(3)), s32(e.r(4))))


def stub_ticks(e):
    L = e.cl
    L.rec("ticks", ())
    e.set_r(3, L.ret("ticks") & MASK)


def stub_default(e):
    L = e.cl
    L.rec("default", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("default") & MASK)


STUBS.internal(GEP, stub_get)
STUBS.internal(SETP, stub_set)
STUBS.internal(GETWIN, stub_getwin)
STUBS.internal(SETFRAME, stub_setframe)
STUBS.internal(TICKS, stub_ticks)
STUBS.internal(DEFH, stub_default)


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc(e, case):
    e.cl = L = CL.PpcLog({WIN: 1, SUB: 2}, case["queues"])
    saved = STUBS.activate(e)
    try:
        r3, _, _ = T.call_ppc(e, FN, [WIN] + [x & MASK for x in case["args"]])
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"answer": r3 & MASK, "log": L.log}


def host(lib, case):
    if not getattr(lib, "bound", False):
        import ctypes as C
        lib.tpw_apm_call.restype = C.c_uint32
        lib.tpw_apm_call.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
        lib.bound = True
    CL.host_begin(lib, {1: WIN, 2: SUB}, case["queues"])
    r = lib.tpw_apm_call(WIN, *[s32(x) for x in case["args"]])
    return {"answer": r & MASK, "log": CL.host_end(lib)}


def mk(r, msg, p0, p2, p3):
    return {"args": [msg, r.choice([0, 1, -5, 77]), r.choice([0, 3, -1])],
            "queues": {"gep0": [p0 & MASK], "gep1": [r.randrange(1 << 32)], "gep2": [p2 & MASK], "gep3": [p3],
                       "ticks": [r.randrange(1 << 32)], "default": [r.randrange(1 << 32)]}}


def build(r, msg=None, p0=None, p2=None, p3=None):
    msg = r.choice([0x1e, 0x1e, 0x1e, 0, 0x1f, 1]) if msg is None else msg
    p3 = r.choice([0, 1, 2, 3]) if p3 is None else p3
    lim = {0: 0x20, 1: 0x20, 2: 0x30, 3: 0x40}[p3]
    p2 = r.choice([0, 1, 2, 0x8000, 0x10000, -1, r.randrange(-1 << 31, 1 << 31), r.randrange(0, 0x40000)]) if p2 is None else p2
    p0 = r.choice([0, r.randrange(0, 1 << 32), (lim << 16) - 2, (lim << 16) - 3, (lim << 16) - 1, lim << 16,
                   ((lim - 1) << 16) + 0xFFFF, r.randrange(0, lim << 16)]) if p0 is None else p0
    return mk(r, msg, p0, p2, p3)


def edges():
    r = random.Random(5197)
    yield build(r, 0, 0, 0, 1)
    yield build(r, 0x1f, 0, 0, 1)
    for mode in (0, 1, 2, 3):
        lim = {0: 0x20, 1: 0x20, 2: 0x30, 3: 0x40}[mode]
        for p0 in (0, (lim << 16) - 2, (lim << 16) - 1, ((lim - 1) << 16) + 0xFFFE, ((lim - 1) << 16) + 0xFFFF, lim << 16,
                   0x80000000, 0x7FFFFFFF):
            for p2 in (0, 1, 2, -1, 0x7FFFFFFF, 0x80000000):
                yield build(r, 0x1e, p0, p2, mode)


def rnd(r):
    return build(r)


TESTS = [
    FnTest("AnimatingPumpMsg", ppc, host, edge=edges, rnd=rnd),
]
