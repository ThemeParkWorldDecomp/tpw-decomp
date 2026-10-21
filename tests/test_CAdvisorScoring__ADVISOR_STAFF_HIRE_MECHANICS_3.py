"""Unit v5136, the advisor's visitor and staff hire scorers, against libtpw_unit_CAdvisorScoring__ADVISOR_STAFF_HIRE_MECHANICS_3.dylib
(tests/unit_CAdvisorScoring__ADVISOR_STAFF_HIRE_MECHANICS_3.cpp), main app, src/game/advisor/AdvisorScoring.cpp:
  _ADVISOR_VISITORS_THIRSTY 0x10011640, _ADVISOR_VISITORS_HUNGRY 0x100116d8, _ADVISOR_STAFF_HIRE_MECHANICS_1 0x1001176c,
  _ADVISOR_STAFF_HIRE_MECHANICS_2 0x10011838, _ADVISOR_STAFF_HIRE_MECHANICS_3 0x10011900, _ADVISOR_STAFF_HIRE_HANDYMEN_1 0x10011a08,
  _ADVISOR_STAFF_HIRE_HANDYMEN_2 0x10011b0c, _ADVISOR_STAFF_HIRE_HANDYMEN_3 0x10011c0c

A case is a scoring object (balance slots written at the Mac offsets on one side, at the host's slot offsets on the other) and a call. The
callees (CWorld::GetParkAnalyser, IsParkClosed and the CParkAnalyser counters) are fakes on both sides that record every argument and answer
from the case's queues. Compared: the answer and the event log. No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_STAFF_HIRE_MECHANICS_3"
MASK = 0xFFFFFFFF

ENTRY = {
    "thirsty": 0x10011640, "hungry": 0x100116d8, "mech1": 0x1001176c, "mech2": 0x10011838,
    "mech3": 0x10011900, "hand1": 0x10011a08, "hand2": 0x10011b0c, "hand3": 0x10011c0c,
}
NAMES = {
    "thirsty": "CAdvisorScoring::_ADVISOR_VISITORS_THIRSTY", "hungry": "CAdvisorScoring::_ADVISOR_VISITORS_HUNGRY",
    "mech1": "CAdvisorScoring::_ADVISOR_STAFF_HIRE_MECHANICS_1", "mech2": "CAdvisorScoring::_ADVISOR_STAFF_HIRE_MECHANICS_2",
    "mech3": "CAdvisorScoring::_ADVISOR_STAFF_HIRE_MECHANICS_3", "hand1": "CAdvisorScoring::_ADVISOR_STAFF_HIRE_HANDYMEN_1",
    "hand2": "CAdvisorScoring::_ADVISOR_STAFF_HIRE_HANDYMEN_2", "hand3": "CAdvisorScoring::_ADVISOR_STAFF_HIRE_HANDYMEN_3",
}
SLOT_CELL = 0x102a460c        # holds the address of gGameWorld
SLOT_BASE = 0x18              # the Mac offset of balance slot 0 in the scoring object
SLOTS = [(off - SLOT_BASE) // 4 for off in range(0xac, 0xf0, 4)] + [(0x630 - SLOT_BASE) // 4]

STUBS = CL.Stubs()


def mk(name, cb=None):
    def stub(e):
        L = e.cl
        args = (L.lab(e.r(3)),) + (cb(e) if cb else ())
        L.rec(name, args)
        e.set_r(3, L.ret(name) & MASK)
    return stub


STUBS.internal(0x1013ebc8, lambda e: (e.cl.rec("GetParkAnalyser", (e.cl.lab(e.r(3)),)), e.set_r(3, e.analyser)))
STUBS.internal(0x1013ff7c, mk("IsParkClosed"))
STUBS.internal(0x100e7188, mk("GetDaysSinceLastStaffHired"))
STUBS.internal(0x100ea5d8, mk("NumberOfObjectTypeInPark", lambda e: (T.s32(e.r(4)), T.s32(e.r(5)))))
STUBS.internal(0x100e8144, mk("NumberOfStaffType", lambda e: (T.s32(e.r(4)),)))
STUBS.internal(0x100e9714, mk("NumberOfBrokenDownRidesInPark"))
STUBS.internal(0x100e9844, mk("NumberOfWornRidesInPark", lambda e: (T.s32(e.r(4)),)))
STUBS.internal(0x100ed738, mk("GetPercentageLitteredCells"))
STUBS.internal(0x100e588c, mk("GetNumDevelopedCells", lambda e: (e.r(4) & MASK,)))
STUBS.internal(0x100e7680, mk("NumberOfHungryVisitors", lambda e: (e.r(4) & 0xFF,)))
STUBS.internal(0x100e77d8, mk("NumberOfThirstyVisitors", lambda e: (e.r(4) & 0xFF,)))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.world = e.alloc(16)
        self.an = e.alloc(16)
        e.analyser = self.an
        self.obj = e.alloc(0x800)
        cell = e.u32(SLOT_CELL)
        self.write(cell, struct.pack(">I", self.world))
        self.L = e.cl = CL.PpcLog({self.world: 1, self.an: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def slot_addr(self, k):
        return self.obj + SLOT_BASE + 4 * k

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op):
        r3, _, _ = T.call_ppc(self.e, ENTRY[op], [self.obj])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.world = lib.tpw_ass_alloc(16)
        self.an = lib.tpw_ass_alloc(16)
        self.obj = lib.tpw_ass_alloc(max(lib.slot_size, 0x800))
        lib.tpw_ass_setup(self.world, self.an)
        CL.host_begin(lib, {1: self.world, 2: self.an}, case["queues"])

    def done(self):
        pass

    def slot_addr(self, k):
        return self.obj + self.lib.slot_off + 8 * k

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        return getattr(self.lib, "tpw_ass_" + op)(self.obj) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ass_alloc.restype, lib.tpw_ass_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ass_setup.restype, lib.tpw_ass_setup.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_ass_layout.argtypes = [C.POINTER(C.c_int64)]
    for op in ENTRY:
        f = getattr(lib, "tpw_ass_" + op)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 8)()
    n = lib.tpw_ass_layout(out)
    assert n == 3 and out[2] == 8, list(out)[:n]
    lib.slot_off, lib.slot_size = out[0], out[1]
    lib.bound = True


def run(s, case):
    try:
        for k, v in zip(SLOTS, case["slots"]):
            s.words(s.slot_addr(k), [v])
        answer = s.call(case["op"])
        return {"answer": answer, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


QUEUE_KEYS = ["IsParkClosed", "GetDaysSinceLastStaffHired", "NumberOfObjectTypeInPark", "NumberOfStaffType",
              "NumberOfBrokenDownRidesInPark", "NumberOfWornRidesInPark", "GetPercentageLitteredCells", "GetNumDevelopedCells",
              "NumberOfHungryVisitors", "NumberOfThirstyVisitors"]
BIG = [0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x10000, 0x1234, -0x1235, 0xFF, 0x100, 0x1FF]


def rv(r, small):
    if r.random() < 0.15:
        return r.choice(BIG)
    return r.randint(-3, 6) if small else r.randint(-1000, 1000)


def mkcase(r, op):
    small = r.random() < 0.7
    slots = [rv(r, small) & MASK for _ in SLOTS]
    queues = {}
    for key in QUEUE_KEYS:
        if key == "IsParkClosed":
            queues[key] = [r.choice([0, 0, 0, 1, 2, -1])]
        elif key == "NumberOfStaffType":
            queues[key] = [rv(r, small) for _ in range(2)] if r.random() < 0.5 else [rv(r, small)]
        else:
            queues[key] = [rv(r, small)]
    return {"op": op, "slots": slots, "queues": queues}


def make(op):
    def edges():
        r = random.Random(sum(map(ord, op)))
        for _ in range(60):
            yield mkcase(r, op)
        # boundaries: the compared values equal their limits
        for v in (-1, 0, 1, 5):
            for closed in (0, 1):
                c = mkcase(r, op)
                c["slots"] = [v & MASK] * len(SLOTS)
                for key in QUEUE_KEYS:
                    c["queues"][key] = [v, v + 1] if key == "NumberOfStaffType" else [v]
                c["queues"]["IsParkClosed"] = [closed]
                yield c
                c2 = mkcase(r, op)
                c2["slots"] = [(v + 1) & MASK] * len(SLOTS)
                for key in QUEUE_KEYS:
                    c2["queues"][key] = [v]
                c2["queues"]["IsParkClosed"] = [closed]
                yield c2

    def rnd(r):
        return mkcase(r, op)

    return FnTest(NAMES[op], ppc, host, edge=edges, rnd=rnd)


TESTS = [make(op) for op in ENTRY]
