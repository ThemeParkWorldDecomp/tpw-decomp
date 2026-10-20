"""Unit v5973, six advisor scorers, against libtpw_unit_CAdvisorScoring__ADVISOR_RIDES_POOR_SELECTION_2.dylib
(tests/unit_CAdvisorScoring__ADVISOR_RIDES_POOR_SELECTION_2.cpp), main app ("SimTheme Park"),
src/AdvisorScoring_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN_CAdvisorScoring__ADVISOR_RIDES_POOR_SELECTION_2.cpp:
  _ADVISOR_RIDES_POOR_SELECTION 0x10013dd8, _ADVISOR_SIDESHOWS_RESEARCH_MORE 0x10014c00, _ADVISOR_SIDESHOWS_BUILD_SIDESHOWS 0x10014d54,
  _ADVISOR_FEATURES_RESEARCH_MORE 0x10015168, _ADVISOR_HYPE_NEWPARK 0x10017528, _ADVISOR_BIG_BUSLOAD_ARRIVED 0x10017b2c

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory at the Mac's addresses, host memory
little-endian). The park analyser, its history, the research lab, the park time, the map and CHistory::GetValue / HowFarBack are fakes
on both sides that record their arguments and answer from the case's queues (the 64-bit division 0x10237e1c runs as the original).

The world of a case: gGameWorld, the balance values at the Mac offsets of the scoring object and the fakes' queues. What is compared: the
answer, the object's word at +8 (written by BIG_BUSLOAD_ARRIVED) and the event log. No fused operations. Not tested: a division of
INT_MIN by -1 in HYPE_NEWPARK (undefined on the hardware).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_RIDES_POOR_SELECTION_2"
MASK = 0xFFFFFFFF

FNS = {
    "rides": (0x10013dd8, 264), "sres": (0x10014c00, 268), "sbuild": (0x10014d54, 256),
    "fres": (0x10015168, 232), "hype": (0x10017528, 336), "bus": (0x10017b2c, 208),
}
OWN = set(a for a, _ in FNS.values())

C_PARKANALYSER = 0x1013ebc8
C_RESEARCHLAB = 0x1013ece8
C_RIDES_AVAIL = 0x100e9998
C_DAYS = 0x100e7098
C_NUMOBJ = 0x100ea5d8
C_TP = 0x100ea298
C_STAFF = 0x100e8144
C_SHOWS_LEFT = 0x100e9f68
C_SHOWS_AVAIL = 0x100e9c80
C_FEAT_LEFT = 0x100e9fe8
C_FEAT_AVAIL = 0x100e9d78
C_SPEED = 0x10122a04
C_TIME = 0x10112b14
C_HOWFAR = 0x1001c010
C_GETVALUE = 0x1001bf6c
C_BUSPOS = 0x10101a50

ADDR = dict(gGameWorld=0x1037ebfc)
HOST_LAYOUT = [0x2a0, 0x216d8, 0x2d8]

CUR = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk(name, addr, nargs=0, ret=None, key=None):
    """A fake that records (name, lab(r3), the next nargs-1 arguments) and answers r3 from queue `key` (a format with the first argument)."""
    def stub(e):
        L = e.cl
        args = [s32(e.r(4 + i)) for i in range(nargs)]
        L.rec(name, tuple([L.lab(e.r(3))] + args))
        if ret is not None:
            k = ret % tuple(args[:1]) if "%" in ret else ret
            e.set_r(3, L.ret(k) & MASK)
    STUBS.internal(addr, stub)


def stub_analyser(e):
    e.cl.rec("GetParkAnalyser", (e.cl.lab(e.r(3)),))
    e.set_r(3, CUR["A"])


def stub_lab(e):
    e.cl.rec("GetResearchLab", (e.cl.lab(e.r(3)),))
    e.set_r(3, CUR["R"])


def stub_time(e):
    L = e.cl
    L.rec("GetFunnyTime", (L.lab(e.r(4)),))
    hi, lo = L.ret("TicksHi") & MASK, L.ret("TicksLo") & MASK
    e.write(e.r(3), struct.pack(">II", hi, lo))


def stub_getvalue(e):
    L = e.cl
    back = s32(e.r(5))
    L.rec("GetValue", (L.lab(e.r(3)), back))
    e.write(e.r(4), struct.pack(">I", L.ret("GV:%d" % back) & MASK))
    e.set_r(3, L.ret("GVok:%d" % back) & 1)


def stub_bus(e):
    L = e.cl
    L.rec("GetBusStopPos", (L.lab(e.r(4)), s32(e.r(5))))
    e.write(e.r(3), struct.pack(">H", L.ret("BusPos") & 0xFFFF))


STUBS.internal(C_PARKANALYSER, stub_analyser)
STUBS.internal(C_RESEARCHLAB, stub_lab)
STUBS.internal(C_TIME, stub_time)
STUBS.internal(C_GETVALUE, stub_getvalue)
STUBS.internal(C_BUSPOS, stub_bus)
mk("TypesOfRidesAvailable", C_RIDES_AVAIL, 1, "RidesAvail")
mk("GetDaysSinceLastObjectBuilt", C_DAYS, 0, "Days")
mk("NumberOfObjectTypeInPark", C_NUMOBJ, 2, "NumObj")
mk("TypesOfParticularObjectTypeInPark", C_TP, 2, "TP:%d")
mk("NumberOfStaffType", C_STAFF, 1, "Staff")
mk("TypesOfSideshowsLeftToResearch", C_SHOWS_LEFT, 0, "ShowsLeft")
mk("TypesOfSideshowsAvailable", C_SHOWS_AVAIL, 1, "ShowsAvail")
mk("TypesOfFeaturesLeftToResearch", C_FEAT_LEFT, 0, "FeatLeft")
mk("TypesOfFeaturesAvailable", C_FEAT_AVAIL, 1, "FeatAvail")
mk("GetResearchSpeed", C_SPEED, 1, "Speed")
mk("HowFarBack", C_HOWFAR, 0, "HowFar")


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.W = e.alloc(0x400)
        self.A = e.alloc(0x21c48)
        self.R = e.alloc(0x40)
        self.obj = e.alloc(0x700)
        CUR.update(W=self.W, A=self.A, R=self.R)
        self.L = e.cl = CL.PpcLog({self.W: 1, self.A: 2, self.A + 0x216d8: 3, self.R: 4, self.W + 0x2a0: 5, self.W + 0x2d8: 6},
                                  case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def set_world(self):
        self.words(ADDR["gGameWorld"], [self.W])

    def set_bal(self, off, v):
        self.words(self.obj + off, [v])

    def word8(self):
        return self.rd_word(self.obj + 8)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op):
        r3, _, _ = T.call_ppc(self.e, FNS[op][0], [self.obj])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.W = lib.tpw_v5973_alloc(0x400)
        self.A = lib.tpw_v5973_alloc(0x21c48)
        self.R = lib.tpw_v5973_alloc(0x40)
        self.obj = lib.tpw_v5973_newthis()
        lib.tpw_v5973_setup(self.W, self.A, self.R)
        CL.host_begin(lib, {1: self.W, 2: self.A, 3: self.A + 0x216d8, 4: self.R, 5: self.W + 0x2a0, 6: self.W + 0x2d8}, case["queues"])

    def done(self):
        pass

    def set_world(self):
        self.lib.tpw_v5973_setworld(self.W)

    def set_bal(self, off, v):
        self.lib.tpw_v5973_setbal(self.obj, off, s32(v))

    def word8(self):
        return self.lib.tpw_v5973_word8(self.obj) & MASK

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        return getattr(self.lib, "tpw_v5973_" + op)(self.obj) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_v5973_"
    for n in ("alloc",):
        getattr(lib, p + n).restype, getattr(lib, p + n).argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_v5973_newthis.restype, lib.tpw_v5973_newthis.argtypes = C.c_uint64, []
    lib.tpw_v5973_setup.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_v5973_setworld.argtypes = [C.c_uint64]
    lib.tpw_v5973_setbal.argtypes = [C.c_uint64, C.c_int32, C.c_int32]
    lib.tpw_v5973_word8.restype, lib.tpw_v5973_word8.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_v5973_layout.argtypes = [C.POINTER(C.c_int64)]
    for op in FNS:
        getattr(lib, p + op).restype, getattr(lib, p + op).argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_v5973_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.set_world()
    for off, v in case["bal"].items():
        s.set_bal(off, v)


def run(s, case):
    try:
        build(s, case)
        ans = s.call(case["op"])
        return {"answer": ans, "w8": s.word8(), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

SMALL = [0, 1, 2, 3, 4, 5, 6, 7, 10, 25, 100, -1, -2, 0x7FFFFFFF, -0x80000000]


def case(op, bal, **q):
    return {"op": op, "bal": bal, "queues": {k: (v if isinstance(v, list) else [v]) for k, v in q.items()}}


def rs(r):
    return r.choice(SMALL) if r.random() < 0.3 else r.randrange(0, 8)


def ticks(r):
    k = r.random()
    day = 864000000000
    if k < 0.2:
        return r.choice([0, 1, day - 1, day, day + 1, -1, -day, -day - 1, 30 * day - 1, 30 * day, 30 * day + 1, 2 ** 62, -2 ** 63, 2 ** 63 - 1])
    if k < 0.7:
        return r.randrange(-3 * 30 * day, 12 * 30 * day)
    return r.randrange(-2 ** 63, 2 ** 63)


def tq(t):
    t &= (1 << 64) - 1
    return {"TicksHi": t >> 32, "TicksLo": t & MASK}


def rides(r):
    a = rs(r)
    t = rs(r)
    return case("rides", {0x634: rs(r), 0x224: rs(r), 0x220: r.choice([0, 25, 26, 24, 100, -1, rs(r)]), 0x228: 777},
                RidesAvail=a, Days=rs(r), NumObj=rs(r), **{"TP:0": t})


def rides_edges(r):
    for a in (0, 4):
        for d, b in ((3, 3), (2, 3), (4, 3)):
            for n, bn in ((5, 5), (4, 5), (6, 5)):
                for t, p in ((1, 25), (1, 26), (1, 24), (-1, 25), (0, 0)):
                    yield case("rides", {0x634: b, 0x224: bn, 0x220: p, 0x228: 777}, RidesAvail=a, Days=d, NumObj=n, **{"TP:0": t})


def sres(r):
    return case("sres", {0x288: r.choice([5, 0, -9])}, Staff=r.choice([0, 1, 3]), ShowsLeft=r.choice([0, 1, 5]),
                Speed=r.choice([0, 0, 1, 5]), ShowsAvail=rs(r), **{"TP:2": r.choice([0, 1, 2, 3, 3])})


def sres_edges(r):
    for s in (0, 3):
        for t in (0, 2, 3):
            for l in (0, 5):
                for sp in (0, 1):
                    for av in (2, 3):
                        yield case("sres", {0x288: 321}, Staff=s, ShowsLeft=l, Speed=sp, ShowsAvail=av, **{"TP:2": t})


def fres(r):
    return case("fres", {0x2ac: r.choice([5, 0, -9])}, Staff=r.choice([0, 1, 3]), FeatLeft=r.choice([0, 1, 5]),
                FeatAvail=rs(r), **{"TP:3": r.choice([0, 1, 2, 3, 3])})


def fres_edges(r):
    for s in (0, 3):
        for t in (0, 2, 3):
            for l in (0, 5):
                for av in (2, 3):
                    yield case("fres", {0x2ac: 321}, Staff=s, FeatLeft=l, FeatAvail=av, **{"TP:3": t})


def sbuild(r):
    return case("sbuild", {0x634: rs(r), 0x28c: r.choice([5, -7, 1000]), 0x290: r.choice([0, 1, 3, -2, 0x10000])},
                Days=rs(r), **{"TP:1": r.choice([0, 1, 4]), "TP:2": r.choice([0, 0, 1, 4])}, **tq(ticks(r)))


def sbuild_edges(r):
    day = 864000000000
    for t1 in (0, 2):
        for d, b in ((3, 3), (2, 3)):
            for t2 in (0, 1):
                for tk in (0, day - 1, day, 5 * day + 7, -day, -1, 2 ** 62):
                    yield case("sbuild", {0x634: b, 0x28c: 100, 0x290: 11}, Days=d, **{"TP:1": t1, "TP:2": t2}, **tq(tk))


def hype(r):
    return case("hype", {0x3c8: r.choice([0, 1, 3, 12, -1, rs(r)]), 0x3cc: r.choice([0, 10, 50, 99, -1, rs(r)]), 0x3d0: 456},
                HowFar=r.choice([0, 1, 2, 5, -1]), **{"GV:0": r.choice([0, 50, 90, r.randrange(-200, 2000), rs(r)]),
                "GV:1": r.choice([0, 100, 1000, r.randrange(-200, 2000), rs(r)]), "GVok:0": r.choice([1, 1, 0]), "GVok:1": r.choice([1, 1, 0])},
                **tq(ticks(r)))


def hype_edges(r):
    month = 25920000000000
    for h in (1, 2):
        for tk, b in ((0, 1), (month - 1, 1), (month, 1), (month, 2), (-1, 0)):
            for ok0, ok1 in ((0, 1), (1, 0), (1, 1)):
                for v0, v1 in ((50, 100), (0, 0), (100, 100), (101, 100), (90, 100), (89, 100), (-5, 100), (5, -100), (5, 0)):
                    for lim in (10, 9, 11):
                        yield case("hype", {0x3c8: b, 0x3cc: lim, 0x3d0: 456}, HowFar=h,
                                   **{"GV:0": v0, "GV:1": v1, "GVok:0": ok0, "GVok:1": ok1}, **tq(tk))


def bus(r):
    return case("bus", {0x410: rs(r), 0x414: r.choice([5, -3])}, HowFar=r.choice([0, 1, 5, -1]),
                BusPos=r.randrange(0, 0x10000), **{"GV:0": rs(r), "GVok:0": r.choice([1, 1, 0])})


def bus_edges(r):
    for h in (0, 1, -1):
        for ok in (0, 1):
            for v, b in ((5, 5), (6, 5), (4, 5), (-3, -4), (-4, -3)):
                yield case("bus", {0x410: b, 0x414: 99}, HowFar=h, BusPos=0xABCD, **{"GV:0": v, "GVok:0": ok})


def gen(fn):
    return lambda: fn(random.Random(5973))


TESTS = []
for name, op, ed, rn in (
        ("CAdvisorScoring::_ADVISOR_RIDES_POOR_SELECTION", "rides", rides_edges, rides),
        ("CAdvisorScoring::_ADVISOR_SIDESHOWS_RESEARCH_MORE", "sres", sres_edges, sres),
        ("CAdvisorScoring::_ADVISOR_SIDESHOWS_BUILD_SIDESHOWS", "sbuild", sbuild_edges, sbuild),
        ("CAdvisorScoring::_ADVISOR_FEATURES_RESEARCH_MORE", "fres", fres_edges, fres),
        ("CAdvisorScoring::_ADVISOR_HYPE_NEWPARK", "hype", hype_edges, hype),
        ("CAdvisorScoring::_ADVISOR_BIG_BUSLOAD_ARRIVED", "bus", bus_edges, bus)):
    TESTS.append(FnTest(name, ppc, host, edge=gen(ed), rnd=rn))
