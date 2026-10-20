"""Unit v5265, seven CAdvisorScoring staff scorers, against libtpw_unit_CAdvisorScoring__ADVISOR_STAFF_HIRE_GUARDS_3.dylib
(tests/unit_CAdvisorScoring__ADVISOR_STAFF_HIRE_GUARDS_3.cpp), main app, src/AdvisorScoring_CAdvisorScoring__ADVISOR_STAFF_HIRE_GUARDS_3.cpp:
  _ADVISOR_STAFF_HIRE_GUARDS_1 0x10011d4c, _GUARDS_2 0x10011e64, _GUARDS_3 0x10011f54, _HIRE_ENTERTAINERS_1 0x10012090,
  _HIRE_RESEARCHERS_2 0x10012328, _SET_MECHANIC_AREA 0x1001260c, _SET_HANDYMAN_AREA 0x100128a4

A case is a scoring object (balance values at their Mac offsets) and a world with an analyser, built by one piece of Python code on both
memories. Every callee outside the unit (CWorld::IsParkClosed, GetParkAnalyser, the CParkAnalyser readers, CParkTime::GetFunnyTimeSinceSessionStart)
is a fake on both sides recording its arguments and answering from the case's queues. The 64-bit divide at 0x10237e1c runs as the original.

The world of a case: the balance values the functions read, the global gGameWorld, the queues. Compared: the answer and the event log.
No fused operations. Not tested: a mechanic/handyman staff count of 0 with a limit of 0 or less (a divide by zero, undefined on the hardware).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_STAFF_HIRE_GUARDS_3"
MASK = 0xFFFFFFFF

FNS = {  # name: (index, address, size)
    "CAdvisorScoring::_ADVISOR_STAFF_HIRE_GUARDS_3": (0, 0x10011F54, 248),
    "CAdvisorScoring::_ADVISOR_STAFF_HIRE_GUARDS_2": (1, 0x10011E64, 172),
    "CAdvisorScoring::_ADVISOR_STAFF_HIRE_ENTERTAINERS_1": (2, 0x10012090, 212),
    "CAdvisorScoring::_ADVISOR_STAFF_HIRE_GUARDS_1": (3, 0x10011D4C, 212),
    "CAdvisorScoring::_ADVISOR_STAFF_HIRE_RESEARCHERS_2": (4, 0x10012328, 244),
    "CAdvisorScoring::_ADVISOR_STAFF_SET_MECHANIC_AREA": (5, 0x1001260C, 248),
    "CAdvisorScoring::_ADVISOR_STAFF_SET_HANDYMAN_AREA": (6, 0x100128A4, 248),
}
OWN = {a for (_, a, _) in FNS.values()}

GAME_WORLD_REF, GAME_WORLD = 0x102A460C, 0x1037EBFC
HOST_LAYOUT = [8]
BAL_OFFSETS = [0xF0, 0xF4, 0xF8, 0xFC, 0x100, 0x104, 0x108, 0x10C, 0x110, 0x11C, 0x12C, 0x130, 0x134, 0x138, 0x144, 0x148, 0x14C, 0x150, 0x630]
DIV = 0x1792F8648000
LABEL_WORLD, LABEL_ANALYSER = 1, 2


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def fake(addr, name, key, args=lambda e: ()):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)),) + tuple(args(e)))
        e.set_r(3, L.ret(key) & MASK)
    STUBS.internal(addr, f)


fake(0x1013FF7C, "IsParkClosed", "closed")
fake(0x100E7188, "GetDaysSinceLastStaffHired", "days")
fake(0x100E588C, "GetNumDevelopedCells", "cells", lambda e: (s32(e.r(4)),))
fake(0x100E8144, "NumberOfStaffType", "staff", lambda e: (s32(e.r(4)),))
fake(0x100E86A4, "NumberOfStaffTypeWithPatrolArea", "patrol", lambda e: (s32(e.r(4)),))
fake(0x100E7930, "MisbehavingVisitorsLastMonth", "mis", lambda e: (e.r(4) & 0xFF,))
fake(0x100E7E98, "PercentageOfVisitorsWithHappinessUnder", "pct", lambda e: (s32(e.r(4)),))
fake(0x100E5944, "HowBusyIsPark", "busy")
fake(0x100E9E70, "TypesOfRidesLeftToResearch", "left")
fake(0x100E9998, "TypesOfRidesAvailable", "avail", lambda e: (e.r(4) & 0xFF,))
fake(0x100EA298, "TypesOfParticularObjectTypeInPark", "inpark", lambda e: (s32(e.r(4)), e.r(5) & 0xFF))


def stub_world_analyser(e):
    e.cl.rec("GetParkAnalyser", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.adv_analyser)


STUBS.internal(0x1013EBC8, stub_world_analyser)


def stub_funny(e):
    """CParkTime::GetFunnyTimeSinceSessionStart() const: the TbTimeDiff comes back through r3, this is r4."""
    L = e.cl
    L.rec("GetFunnyTimeSinceSessionStart", (e.r(4) - e.adv_world,))
    e.write(e.r(3), struct.pack(">II", L.ret("tickhi") & MASK, L.ret("ticklo") & MASK))


STUBS.internal(0x10112B14, stub_funny)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.world = e.alloc(0x400)
        self.analyser = e.alloc(0x40)
        e.adv_world, e.adv_analyser = self.world, self.analyser
        self.L = e.cl = CL.PpcLog({self.world: LABEL_WORLD, self.analyser: LABEL_ANALYSER}, case["queues"])
        self.saved = STUBS.activate(e)
        self.obj = e.alloc(0x700)
        self.words(GAME_WORLD_REF, [GAME_WORLD])
        self.words(GAME_WORLD, [self.world])

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def bal(self, off, value):
        self.words(self.obj + off, [value])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, idx):
        _, addr, _ = [v for v in FNS.values() if v[0] == idx][0]
        r3, _, _ = T.call_ppc(self.e, addr, [self.obj])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.world = lib.tpw_adv_alloc(0x400)
        self.analyser = lib.tpw_adv_alloc(0x40)
        self.obj = lib.tpw_adv_alloc(0x4000)
        lib.tpw_adv_setup(self.world, self.analyser)
        CL.host_begin(lib, {LABEL_WORLD: self.world, LABEL_ANALYSER: self.analyser}, case["queues"])

    def done(self):
        pass

    def bal(self, off, value):
        self.words(self.lib.tpw_adv_slot(self.obj, off), [value])

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, idx):
        return self.lib.tpw_adv_call(idx, self.obj) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_adv_alloc.restype, lib.tpw_adv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_adv_setup.restype, lib.tpw_adv_setup.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_adv_slot.restype, lib.tpw_adv_slot.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_adv_call.restype, lib.tpw_adv_call.argtypes = C.c_int32, [C.c_int32, C.c_uint64]
    lib.tpw_adv_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_adv_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        for off, v in case["bal"].items():
            s.bal(off, v)
        return {"answer": s32(s.call(case["idx"])), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def small(r, lo, hi):
    """Mostly a small number (so limits meet their values), now and then any 32-bit number (wrapping products)."""
    k = r.random()
    if k < 0.08:
        return r.choice([0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32)])
    return r.randint(lo, hi)


def ticks_for(r):
    q = r.randint(-2, 6)
    rem = r.choice([0, DIV - 1, r.randrange(DIV)])
    t = q * DIV + rem
    if r.random() < 0.1:
        t = r.randrange(-(1 << 62), 1 << 62)
    return t & 0xFFFFFFFFFFFFFFFF


def make(r, idx):
    bal = {}
    for off in BAL_OFFSETS:
        bal[off] = small(r, -2, 5) & MASK
    staff = r.randint(0, 5)
    q = {
        "closed": [1 if r.random() < 0.15 else 0],
        "days": [small(r, -1, 5) & MASK],
        "cells": [small(r, -1, 8) & MASK],
        "staff": [staff],
        "patrol": [small(r, -2, 8) & MASK],
        "mis": [small(r, 0, 4) & MASK],
        "pct": [r.choice([28, 29, 30, 31, 32, r.randint(-2, 100)]) & MASK],
        "busy": [small(r, -1, 6) & MASK],
        "left": [r.choice([0, 0, 1, 2])],
        "avail": [r.randint(0, 3)],
        "inpark": [r.randint(0, 3)],
    }
    if r.random() < 0.4:
        q["inpark"] = list(q["avail"])
    if r.random() < 0.3:
        q["days"] = [bal[0x630]]
    if r.random() < 0.3:
        q["cells"] = [bal[0x108]]
    if idx in (5, 6):
        lim = 0x12C if idx == 5 else 0x144
        bal[lim] = r.randint(1, 4)          # a limit of 0 or less with no staff divides by zero
        if r.random() < 0.5:
            q["staff"] = [bal[lim] + r.randint(0, 2)]
        thr = 0x130 if idx == 5 else 0x148
        if r.random() < 0.4:
            bal[thr] = r.choice([0, 50, 100, 200, 25, 75])
        if r.random() < 0.5:
            n = q["staff"][0] or 1
            q["patrol"] = [r.randint(0, n)]
        t = ticks_for(r)
        q["tickhi"], q["ticklo"] = [t >> 32], [t & MASK]
        lo = 0x138 if idx == 5 else 0x150
        if r.random() < 0.5:
            bal[lo] = (s32((t if t < (1 << 63) else t - (1 << 64)) // 1 // DIV if False else 0) + r.randint(-1, 1)) & MASK
            qq = (t if t < (1 << 63) else t - (1 << 64))
            qd = int(qq / DIV) if abs(qq) < (1 << 52) else qq // DIV
            bal[lo] = (qd + r.randint(-1, 1)) & MASK
    else:
        q["tickhi"], q["ticklo"] = [0], [0]
    return {"idx": idx, "bal": bal, "queues": q}


def gen(seed, idx, n):
    def edges():
        r = random.Random(seed)
        for _ in range(n):
            yield make(r, idx)
    return edges


def rnd(idx):
    return lambda r: make(r, idx)


TESTS = [FnTest(name, ppc, host, edge=gen(1000 + i, i, 400), rnd=rnd(i)) for name, (i, _, _) in FNS.items()]
