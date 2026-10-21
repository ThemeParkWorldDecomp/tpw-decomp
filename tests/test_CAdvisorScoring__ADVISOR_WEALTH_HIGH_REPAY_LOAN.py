"""Unit v5896, twelve advisor scorers, against libtpw_unit_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN.dylib
(tests/unit_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN.cpp), main app
("SimTheme Park"), src/game/advisor/AdvisorScoring.cpp:
  _ADVISOR_WEALTH_HIGH_REPAY_LOAN 0x10016218, _ADVISOR_PRICING_TICKET_TOO_CHEAP 0x10015cec, _ADVISOR_WAGES_HIGH 0x10016778,
  _ADVISOR_PRICING_TICKET_TOO_EXPENSIVE 0x10015c14, _ADVISOR_LOTS_MECHANICS 0x10016a84, _ADVISOR_LOTS_SECURITY 0x10016b50,
  _ADVISOR_LOTS_RESEARCHERS 0x10016c1c, _ADVISOR_QUEUE_UNCONNECTED 0x10016cec, _ADVISOR_MAX_HANDYMEN 0x10016db0,
  _ADVISOR_FEATURES_NOT_ENOUGH_TOILETS 0x10015564, _ADVISOR_FEATURES_NO_TOILETS 0x100153e0, _ADVISOR_FEATURES_BUILD_FEATURES 0x10015298

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the world, the bank
account, the park analyser, the park time and the history mean 0x1001c064) are fakes on both sides that record their arguments and answer from
the case's queues. The 64-bit division routine at 0x10237e1c runs as the original.

The world of a case: the scoring object's balance values at the Mac offsets the function reads, mWord4, gMainBalance's word at 0x2a8, the
queue-unconnected constant (0x1027df04) and the queues. What is compared: the answer, mWord4, the two globals and the event log.

No fused operations (the float work is fsubs and fmuls on exact integers). Not tested: a history callee that answers a count with a mean the
host would make differently (the callee is a fake on both sides).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN"
MASK = 0xFFFFFFFF

WORLD, ANALYSER, BANK = 0x30000000, 0x30200000, 0x30400000
OBJ_SIZE = 0x700

ADDR = dict(gw=0x1037ebfc, ref=0x102a460c, mb=0x102b4568 + 0x2a8, noride=0x1027df04)

MONTH = 0x1792F8648000
DAY = 0xC92A69C000

# ---- the callees faked on both sides
CALLEES = {
    "GetBankAccount": 0x1013edfc, "GetBalance": 0x100f32b4, "TotalOutstandingOnLoans": 0x100f43dc, "GetCheapestLoanTaken": 0x100f42e4,
    "OutstandingOnLoan": 0x100f4278, "GetParkAnalyser": 0x1013ebc8, "HaveCompleteSetOfOpinions": 0x100e80a8,
    "GetAverageOpinionOn": 0x100e8018, "NumberOfObjectTypeInPark": 0x100ea5d8, "NumberOfStaffType": 0x100e8144,
    "FindRideWithUnconnectedQueue": 0x100ed240, "IsParkClosed": 0x1013ff7c, "GetAverageToiletQueueLenght": 0x100ed0c0,
    "GatesOpen": 0x100e7028, "NumberOfToiletsInPark": 0x100e8f28, "NumberOfVisitorsNeedingToilet": 0x100e7520,
    "NumberOfVisitorsInPark": 0x100e7278, "TypesOfParticularObjectTypeInPark": 0x100ea298, "GetDaysSinceLastObjectBuilt": 0x100e7098,
    "GetFunnyTimeSinceSessionStart": 0x10112b14, "GetAverageOfLast": 0x1001c064,
}

STUBS = CL.Stubs()


def s32(v):
    return T.s32(v)


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def simple(name, args, key=None, const=None):
    """A fake that records its register arguments (L label, i s32, u u32, b u8) and answers r3 from the queue `key` or a constant."""
    def stub(e):
        L = e.cl
        vals = []
        for i, k in enumerate(args):
            v = e.r(3 + i)
            vals.append(L.lab(v) if k == "L" else s32(v) if k == "i" else (v & MASK) if k == "u" else (v & 0xFF))
        L.rec(name, tuple(vals))
        if const is not None:
            e.set_r(3, const)
        elif key:
            e.set_r(3, L.ret(key) & MASK)
    STUBS.internal(CALLEES[name], stub)


simple("GetBankAccount", "L", const=BANK)
simple("GetBalance", "L", "balance")
simple("TotalOutstandingOnLoans", "L", "total")
simple("GetCheapestLoanTaken", "L", "cheapest")
simple("OutstandingOnLoan", "Lu", "owed")
simple("GetParkAnalyser", "L", const=ANALYSER)
simple("HaveCompleteSetOfOpinions", "Li", "complete")
simple("GetAverageOpinionOn", "Li", "avg")
simple("NumberOfObjectTypeInPark", "Lii", "nobj")
simple("NumberOfStaffType", "Li", "nstaff")
simple("IsParkClosed", "L", "closed")
simple("GatesOpen", "L", "gates")
simple("NumberOfToiletsInPark", "L", "toilets")
simple("NumberOfVisitorsNeedingToilet", "Lb", "needing")
simple("NumberOfVisitorsInPark", "L", "inpark")
simple("TypesOfParticularObjectTypeInPark", "Lib", "types")
simple("GetDaysSinceLastObjectBuilt", "L", "days")


def stub_find(e):
    """FindRideWithUnconnectedQueue(bool): the hidden result (r3), this (r4), any (r5)."""
    L = e.cl
    L.rec("FindRideWithUnconnectedQueue", (L.lab(e.r(4)), e.r(5) & 0xFF))
    e.write(e.r(3), struct.pack(">H", L.ret("unconn") & 0xFFFF))


STUBS.internal(CALLEES["FindRideWithUnconnectedQueue"], stub_find)


def stub_queue(e):
    L = e.cl
    L.rec("GetAverageToiletQueueLenght", (L.lab(e.r(3)),))
    e.set_f1(bitsf(L.ret("queue")))


STUBS.internal(CALLEES["GetAverageToiletQueueLenght"], stub_queue)


def stub_time(e):
    """GetFunnyTimeSinceSessionStart() const: the hidden result (r3), this (r4); the span as two words."""
    L = e.cl
    L.rec("GetFunnyTimeSinceSessionStart", ((e.r(4) - WORLD) & MASK,))
    e.write(e.r(3), struct.pack(">II", L.ret("thi") & MASK, L.ret("tlo") & MASK))


STUBS.internal(CALLEES["GetFunnyTimeSinceSessionStart"], stub_time)


def stub_avg(e):
    """CHistory<int,144>::GetAverageOfLast(int& out, int count): answers the count n and writes the mean of the sum."""
    L = e.cl
    index = (e.r(3) - ANALYSER - 0x1f100) // 0x250
    count = s32(e.r(5))
    L.rec("GetAverageOfLast", (index, count))
    n = s32(L.ret("n3" if index == 3 else "n5"))
    total = L.ret("sum3" if index == 3 else "sum5") - 1000000
    out = int(float(total) / float(n)) if n > 0 else 0
    e.write(e.r(4), struct.pack(">I", out & MASK))
    e.set_r(3, n & MASK)


STUBS.internal(CALLEES["GetAverageOfLast"], stub_avg)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def wr16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd16(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({WORLD: 1, ANALYSER: 2, BANK: 3}, case["queues"])
        self.saved = STUBS.activate(e)
        self.obj = e.alloc(OBJ_SIZE)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def slot(self, off):
        return self.obj + off

    def prep(self):
        self.words(ADDR["gw"], [WORLD])
        self.words(ADDR["ref"], [ADDR["gw"]])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op):
        r3, _, _ = T.call_ppc(self.e, OPS[op]["addr"], [self.obj])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_adv_reset()
        CL.host_begin(lib, {1: self.g("world"), 2: self.g("analyser"), 3: self.g("bank")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_adv_addr(name.encode())

    def slot(self, off):
        return self.lib.tpw_adv_slot(off)

    def prep(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        return getattr(self.lib, "tpw_adv_" + op)() & MASK


HOST_LAYOUT = [4, 0x1f100, 0x250, 0x2a0]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_adv_addr.restype, lib.tpw_adv_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_adv_slot.restype, lib.tpw_adv_slot.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_adv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_adv_reset.argtypes = []
    for op in OPS:
        getattr(lib, "tpw_adv_" + op).restype = C.c_uint32
        getattr(lib, "tpw_adv_" + op).argtypes = []
    out = (C.c_int64 * 32)()
    n = lib.tpw_adv_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    for off, w in case["bal"].items():
        s.words(s.slot(off), [w])
    s.wr16(s.slot(4), case["w4"])
    s.words(s.g("mb"), [case["mb"]])
    s.wr16(s.g("noride"), case["noride"])
    s.prep()


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "w4": s.rd16(s.slot(4)), "mb": s.rd_word(s.g("mb")), "noride": s.rd16(s.g("noride"))}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def pick(*xs):
    return lambda r: r.choice(xs)


def sm(r):
    return r.randrange(-2, 7)


def res(r):
    return r.randrange(1, 100000) * r.choice([1, -1])


def flt(r):
    k = r.random()
    if k < 0.05:
        return 0x7FC00000
    if k < 0.75:
        return fb(r.choice([0.0, -0.0, 0.5, 1.0, 1.5, 2.0, 3.0, -1.0, 0.25]))
    return fb(r.uniform(-4, 4))


def hist_n(r):
    return r.choice([0, 1, 1, 2, 5, -1, 3])


def hist_sum(r):
    return r.randrange(0, 2000000)


U32 = (0, 1, 50, 100, 16777217, 0xFFFFFFFF, 0x80000000, 3000)
THR = pick(-1, 0, 1, 2, 3, 100, 254, 255, 256)
U8 = pick(0, 1, 2, 3, 100, 254, 255)
BOOL = pick(0, 1)

OPS = {
    "WEALTH_HIGH_REPAY_LOAN": dict(
        addr=0x10016218,
        bal={0x338: flt, 0x33c: res},
        q=dict(balance=pick(-100, 0, 50, 100, 16777217, 0x7fffffff, -0x80000000, 3000), total=pick(*U32),
               cheapest=pick(0xFFFFFFFF, 0, 1, 2, 3, 0x80000000), owed=pick(*U32))),
    "PRICING_TICKET_TOO_CHEAP": dict(addr=0x10015cec, bal={0x2f8: THR, 0x2fc: res}, q=dict(complete=BOOL, avg=U8)),
    "PRICING_TICKET_TOO_EXPENSIVE": dict(addr=0x10015c14, bal={0x2f0: THR, 0x2f4: res}, q=dict(complete=BOOL, avg=U8)),
    "WAGES_HIGH": dict(
        addr=0x10016778, time=MONTH,
        bal={0x354: flt, 0x358: sm, 0x35c: res},
        q=dict(n3=hist_n, n5=hist_n, sum3=hist_sum, sum5=hist_sum)),
    "LOTS_MECHANICS": dict(addr=0x10016a84, bal={0x370: sm, 0x374: res}, q=dict(nobj=sm, nstaff=sm)),
    "LOTS_SECURITY": dict(addr=0x10016b50, bal={0x378: sm, 0x37c: res}, q=dict(nobj=sm, nstaff=sm)),
    "LOTS_RESEARCHERS": dict(addr=0x10016c1c, bal={0x380: sm, 0x384: res}, q=dict(nobj=sm, nstaff=sm)),
    "QUEUE_UNCONNECTED": dict(addr=0x10016cec, bal={0x388: res}, q=dict(unconn=pick(0, 1, 2, 0xFFFF, 0x100))),
    "MAX_HANDYMEN": dict(addr=0x10016db0, bal={0x38c: sm, 0x390: res}, q=dict(nstaff=sm)),
    "FEATURES_NOT_ENOUGH_TOILETS": dict(addr=0x10015564, bal={0x2c8: flt}, q=dict(closed=pick(0, 1, -1, 2), queue=flt)),
    "FEATURES_NO_TOILETS": dict(
        addr=0x100153e0,
        bal={0x2b8: pick(0, 1, 2, 255, 256, 257, -1), 0x2bc: pick(-1, 0, 25, 50, 100, 200), 0x2c0: res},
        q=dict(gates=BOOL, toilets=pick(0, 0, 1, -1), needing=pick(0, 1, 2, 5, 50, 100, 0x7fffffff), inpark=pick(-3, -1, 0, 1, 2, 5, 50, 100))),
    "FEATURES_BUILD_FEATURES": dict(
        addr=0x10015298, time=DAY,
        bal={0x634: sm, 0x2b0: res, 0x2b4: res},
        q=dict(types=pick(0, 1, 1, 2), days=sm)),
}


def ticks(r, div):
    k = r.choice([-2, -1, 0, 0, 1, 2, 3, 4, 5, 100, 70000] if div == MONTH else [-2, -1, 0, 1, 2, 3, 100, 5000000, 3000000000])
    rem = r.choice([0, div - 1, r.randrange(div)])
    if k < 0:
        rem = -rem
    return (k * div + rem) & 0xFFFFFFFFFFFFFFFF


def make_case(r, op):
    spec = OPS[op]
    c = {"op": op, "bal": {off: g(r) & MASK for off, g in spec["bal"].items()}, "w4": r.choice([0, 1, 0xFFFF, r.randrange(0x10000)]),
         "mb": r.choice([0, 1, 2, 3, 4, 5, -1, -2]) & MASK if op == "MAX_HANDYMEN" else r.randrange(1 << 32),
         "noride": r.choice([0, 1, 2, 0xFFFF, 0x100])}
    if op == "MAX_HANDYMEN":
        c["mb"] = (c["bal"][0x38c] if r.random() < 0.5 else r.choice([0, 1, 2, 3, 4, 5, -1, -2])) & MASK
    if op == "QUEUE_UNCONNECTED" and r.random() < 0.4:
        c["noride"] = 0
    queues = {k: [g(r) for _ in range(3)] for k, g in spec["q"].items()}
    if "time" in spec:
        t = ticks(r, spec["time"])
        queues["thi"] = [t >> 32]
        queues["tlo"] = [t & MASK]
    c["queues"] = queues
    return c


def tests_for(op):
    def edges():
        r = random.Random(sum(map(ord, op)))
        for _ in range(300):
            yield make_case(r, op)

    return FnTest("CAdvisorScoring::_ADVISOR_" + op, ppc, host, edge=edges, rnd=lambda r: make_case(r, op))


TESTS = [tests_for(op) for op in OPS]
