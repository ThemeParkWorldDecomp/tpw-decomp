"""Unit v5106, the advisor's gold ticket scorers and the tooltip tutorial scorer, against
libtpw_unit_CAdvisorScoring__ADVISOR_GOLD_TICKET_AMOUNT_VISI.dylib (tests/unit_CAdvisorScoring__ADVISOR_GOLD_TICKET_AMOUNT_VISI.cpp), main app,
src/AdvisorScoring_CAdvisorScoring__ADVISOR_GOLD_TICKET_AMOUNT_VISI.cpp:
  _ADVISOR_TUTORIAL_EXPLAIN_TOOLTIPS 0x1001b604, _ADVISOR_GOLD_TICKET_EXPLANATION 0x1001b6d0,
  _ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_a/b/c 0x1001b8ec 0x1001b9d0 0x1001bab4,
  _ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_a/b/c 0x1001bb8c 0x1001bcc0 0x1001bdf4

A case is a scoring object (balance values at Mac offsets 0x5e8..0x628), the main balance value at 0x754 and the answers of the callees. The
callees (CWorld::IsParkClosed, CWorld::GetParkAnalyser, NumberOfVisitorsInPark, PercentageOfTotalObjectsInPark, GetFunnyTimeSinceSessionStart,
CHistory<int,5>::HowFarBack) are fakes on both sides recording their arguments (the world and analyser are constant addresses, never
dereferenced; the park time and the history are checked by their offset from them). The 64-bit divide of the tooltip scorer (0x10237e1c) runs
as the original on the PowerPC side. What is compared: the answer and the event log. No fused operations. Not tested: a visitor percentage
whose multiplication by 100 is INT_MIN with a divisor of -1 (the hardware is undefined there).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_GOLD_TICKET_AMOUNT_VISI"
MASK = 0xFFFFFFFF

FNS = [  # (name, address, size, which)
    ("CAdvisorScoring::_ADVISOR_TUTORIAL_EXPLAIN_TOOLTIPS", 0x1001B604, 128, 0),
    ("CAdvisorScoring::_ADVISOR_GOLD_TICKET_EXPLANATION", 0x1001B6D0, 92, 1),
    ("CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_a", 0x1001B8EC, 136, 2),
    ("CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_b", 0x1001B9D0, 136, 3),
    ("CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_c", 0x1001BAB4, 124, 4),
    ("CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_a", 0x1001BB8C, 228, 5),
    ("CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_b", 0x1001BCC0, 228, 6),
    ("CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_c", 0x1001BDF4, 228, 7),
]
OWN = {a for _, a, _, _ in FNS}

IS_PARK_CLOSED, GET_ANALYSER, NUM_VISITORS, PERCENT, FUNNY_TIME, HOW_FAR_BACK, DIV64 = (
    0x1013FF7C, 0x1013EBC8, 0x100E7278, 0x100EA194, 0x10112B14, 0x1001C010, 0x10237E1C)
WORLD, ANALYSER = 0x20000000, 0x20100000
ADDR = dict(gGameWorld=0x1037EBFC, gGameWorldRef=0x102A460C, mb754=0x102B4568 + 0x754)
HOST_LAYOUT = [0x216D8, 0x2A0]
SLOT_OFFSETS = list(range(0x5E8, 0x62C, 4))


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.add(DIV64)


def stub_is_park_closed(e):
    L = e.cl
    L.rec("IsParkClosed", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("IsParkClosed") & MASK)


def stub_get_analyser(e):
    L = e.cl
    L.rec("GetParkAnalyser", (L.lab(e.r(3)),))
    e.set_r(3, ANALYSER)


def stub_visitors(e):
    L = e.cl
    L.rec("NumberOfVisitorsInPark", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("NumberOfVisitorsInPark") & MASK)


def stub_percent(e):
    L = e.cl
    L.rec("PercentageOfTotalObjectsInPark", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("PercentageOfTotalObjectsInPark") & MASK)


def stub_funny(e):
    L = e.cl
    L.rec("GetParkTime", ()) if False else None
    L.rec("GetFunnyTimeSinceSessionStart", (s32(e.r(4) - WORLD),))
    ticks = L.ret("GetFunnyTimeSinceSessionStart") & 0xFFFFFFFFFFFFFFFF
    e.write(e.r(3), struct.pack(">Q", ticks))


def stub_how_far_back(e):
    L = e.cl
    L.rec("HowFarBack", (s32(e.r(3) - (ANALYSER + 0x216D8)),))
    e.set_r(3, L.ret("HowFarBack") & MASK)


STUBS.internal(IS_PARK_CLOSED, stub_is_park_closed)
STUBS.internal(GET_ANALYSER, stub_get_analyser)
STUBS.internal(NUM_VISITORS, stub_visitors)
STUBS.internal(PERCENT, stub_percent)
STUBS.internal(FUNNY_TIME, stub_funny)
STUBS.internal(HOW_FAR_BACK, stub_how_far_back)


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
        self.L = e.cl = CL.PpcLog({WORLD: 1, ANALYSER: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def world(self):
        self.words(ADDR["gGameWorld"], [WORLD])
        self.words(ADDR["gGameWorldRef"], [ADDR["gGameWorld"]])

    def obj(self, slots):
        o = self.e.alloc(0x700)
        for off, v in slots.items():
            self.words(o + off, [v])
        return o

    def call(self, which, entry, o):
        r3, _, _ = T.call_ppc(self.e, entry, [o])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("world"), 2: self.g("analyser")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gt_addr(name.encode())

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def world(self):
        self.words(self.g("gGameWorld"), [self.g("world")])

    def obj(self, slots):
        o = self.lib.tpw_gt_object()
        for off, v in slots.items():
            self.lib.tpw_gt_slot(o, off, s32(v))
        return o

    def call(self, which, entry, o):
        return self.lib.tpw_gt_run(which, o) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gt_addr.restype, lib.tpw_gt_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gt_object.restype, lib.tpw_gt_object.argtypes = C.c_uint64, []
    lib.tpw_gt_slot.restype, lib.tpw_gt_slot.argtypes = None, [C.c_uint64, C.c_int32, C.c_int32]
    lib.tpw_gt_run.restype, lib.tpw_gt_run.argtypes = C.c_uint32, [C.c_int32, C.c_uint64]
    lib.tpw_gt_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gt_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def run(s, case, which, entry):
    try:
        s.world()
        s.words(s.g("mb754"), [case["total"]])
        o = s.obj(case["slots"])
        answer = s.call(which, entry, o)
        return {"answer": answer, "log": s.log()}
    finally:
        s.done()


def make_test(name, entry, which, gen):
    def ppc(e, case):
        return run(Ppc(e, case), case, which, entry)

    def host(lib, case):
        return run(Host(lib, case), case, which, entry)

    def edges():
        r = random.Random(entry)
        for i in range(120):
            yield gen(r, i, True)

    def rnd(r):
        return gen(r, r.randrange(1000), False)

    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


DAY = 864000000000


def near(r, p, special):
    """A threshold near the compared value p: the boundary on both sides, or any small number."""
    k = r.random()
    if k < 0.6 or special:
        return p + r.choice([-1, 0, 1])
    return r.randrange(-5, 15)


def base_case(r):
    slots = {off: r.choice([r.randrange(-5, 15), r.randrange(-5, 105), 100, 0]) for off in SLOT_OFFSETS}
    # amounts are distinguishable
    for off in (0x5F0, 0x5F4, 0x600, 0x608, 0x610, 0x618, 0x620, 0x628):
        slots[off] = r.randrange(-1000, 100000) * 7 + 1
    return {"queues": {"IsParkClosed": [1 if r.random() < 0.25 else 0]}, "slots": slots, "total": 0}


def gen_tooltips(r, i, special):
    c = base_case(r)
    d = r.randrange(-3, 12)
    k = r.random()
    if k < 0.15:
        t = r.randrange(-(1 << 62), 1 << 62)
    else:
        t = d * DAY + r.choice([0, 1, -1, DAY - 1, r.randrange(0, DAY)])
    c["queues"]["GetFunnyTimeSinceSessionStart"] = [t]
    q = t // DAY
    q = int(t / DAY) if abs(t) < (1 << 52) else q
    q32 = s32(q if t >= 0 else -((-t) // DAY))
    c["slots"][0x5E8] = near(r, q32, special) if r.random() < 0.8 else r.randrange(-5, 5)
    c["slots"][0x5EC] = near(r, q32, special) if r.random() < 0.8 else r.randrange(-5, 15)
    return c


def gen_explanation(r, i, special):
    c = base_case(r)
    c["queues"]["HowFarBack"] = [r.choice([0, 1, 1, 2, 5, 50, -1])]
    return c


def gen_rides(which):
    def gen(r, i, special):
        c = base_case(r)
        p = r.choice([r.randrange(-3, 110), r.randrange(0, 100)])
        c["queues"]["PercentageOfTotalObjectsInPark"] = [p]
        lo, hi = {0: (0x5FC, 0x604), 1: (0x604, 0x60C), 2: (0x60C, 0x60C)}[which]
        c["slots"][lo] = near(r, p, special)
        c["slots"][hi] = near(r, p, special)
        return c
    return gen


def gen_visitors(which):
    thr = {0: 0x614, 1: 0x61C, 2: 0x624}[which]

    def gen(r, i, special):
        c = base_case(r)
        total = r.choice([0, 1, 2, 3, 7, 10, 50, 100, 200, -1, -5, 1000, r.randrange(-300, 300)])
        c["total"] = total
        v1 = r.choice([0, 1, 5, 50, 99, 100, 101, 150, 300, -3, r.randrange(-50, 400)])
        v2 = r.choice([v1, v1, r.randrange(-50, 400)])
        c["queues"]["NumberOfVisitorsInPark"] = [v1, v2]
        if total != 0 and not (total == -1 and s32(v1 * 100) == -(1 << 31)):
            def div(a, b):
                a = s32(a * 100)
                q = abs(a) // abs(b)
                return s32(-q if (a < 0) != (b < 0) else q)
            p1 = div(v1, total)
            p2 = 100 if p1 > 100 else div(v2, total)
            c["slots"][thr] = near(r, p2, special)
        else:
            c["slots"][thr] = r.randrange(-5, 15)
        return c
    return gen


GENS = [gen_tooltips, gen_explanation, gen_rides(0), gen_rides(1), gen_rides(2), gen_visitors(0), gen_visitors(1), gen_visitors(2)]

TESTS = [make_test(name, addr, which, GENS[which]) for name, addr, size, which in FNS]
