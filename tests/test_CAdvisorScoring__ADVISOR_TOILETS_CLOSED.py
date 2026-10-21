"""Unit v5040, eight advisor scorers, against libtpw_unit_CAdvisorScoring__ADVISOR_TOILETS_CLOSED.dylib
(tests/unit_CAdvisorScoring__ADVISOR_TOILETS_CLOSED.cpp), main app ("SimTheme Park"), src/game/advisor/AdvisorScoring.cpp, src/game/advisor/Advisor.cpp:
  _ADVISOR_TOILETS_CLOSED 0x1001aa20, _ADVISOR_STAFFROOM_CLOSED 0x1001aafc, _ADVISOR_SIDESHOWS_CLOSED 0x1001a934,
  _ADVISOR_LOTS_OF_PRANKS 0x1001abdc, _ADVISOR_SHOPS_CLOSED 0x1001a84c, _ADVISOR_RIDES_CLOSED 0x1001a728,
  _ADVISOR_CONGRAT_PRICING_SIDESHOWS_GREAT_SATISFACTION 0x1001a5e4, _ADVISOR_GOLD_TICKET_NEAR_TO_FIRST_ONE 0x1001aef8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses for the
original, and in the host library's memory for the translation. The callees (CWorld::GetParkAnalyser, the CParkAnalyser queries,
CGoldenTicketControl::HowManyTickets) are fakes on both sides that record their arguments and answer from the case's queues.

The world of a case: the scoring object (balance values at the Mac offsets 0x570..0x5bc, the host maps an offset to its own slot), the
object's halfword 4 and word 8, gGameWorld / gGameWorldRef, the "no sideshow" id 0x1027df2c, gMainBalance's word 0x750. Compared: the
answer, the object's halfword 4 and word 8, the event log. No fused operations (the gold ticket limit is fmuls then fctiwz, values kept
below 2^31). Not tested: a product beyond 2^31 (Unicorn's fctiwz differs from the hardware).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_TOILETS_CLOSED"
MASK = 0xFFFFFFFF

FUNCS = [
    ("_ADVISOR_TOILETS_CLOSED", 0x1001aa20, 156),
    ("_ADVISOR_STAFFROOM_CLOSED", 0x1001aafc, 156),
    ("_ADVISOR_SIDESHOWS_CLOSED", 0x1001a934, 168),
    ("_ADVISOR_LOTS_OF_PRANKS", 0x1001abdc, 96),
    ("_ADVISOR_SHOPS_CLOSED", 0x1001a84c, 168),
    ("_ADVISOR_RIDES_CLOSED", 0x1001a728, 228),
    ("_ADVISOR_CONGRAT_PRICING_SIDESHOWS_GREAT_SATISFACTION", 0x1001a5e4, 156),
    ("_ADVISOR_GOLD_TICKET_NEAR_TO_FIRST_ONE", 0x1001aef8, 180),
]
OWN = {a for _, a, _ in FUNCS}

ADDR = dict(gw=0x1037ebfc, gwref=0x102a460c, none=0x1027df2c, main=0x102b4568)
WORLD_SIZE, ANAL_SIZE = 0x400, 0x40
WORLD = {}


def s32(v):
    return T.s32(v)


def tdiv(a, b):
    q = abs(a) // abs(b)
    return -q if (a < 0) != (b < 0) else q


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def fake(addr, name, nargs=0, key=None):
    def f(e):
        L = e.cl
        L.rec(name, tuple([L.lab(e.r(3))] + [s32(e.r(4 + i)) for i in range(nargs)]))
        e.set_r(3, L.ret(key or name) & MASK)
    STUBS.internal(addr, f)


def stub_get_analyser(e):
    L = e.cl
    L.rec("GetParkAnalyser", (L.lab(e.r(3)),))
    e.set_r(3, WORLD["anal"])


STUBS.internal(0x1013ebc8, stub_get_analyser)
fake(0x100e8f28, "NumberOfToiletsInPark")
fake(0x100e9070, "NumberOfClosedToiletsInPark")
fake(0x100e91b0, "NumberOfStaffRoomsInPark")
fake(0x100e92dc, "NumberOfClosedStaffRoomsInPark")
fake(0x100ea5d8, "NumberOfObjectTypeInPark", 2)
fake(0x100ea748, "NumberOfClosedObjectTypeInPark", 1)
fake(0x100e7028, "GatesOpen")
fake(0x100e7994, "NumberOfVisitorsSinceBeginning")
fake(0x100fbdf8, "HowManyTickets")


def stub_misbehaving(e):
    L = e.cl
    L.rec("MisbehavingVisitorsLastMonth", (L.lab(e.r(3)), e.r(4) & 0xFF))
    e.set_r(3, L.ret("MisbehavingVisitorsLastMonth") & MASK)


def stub_best(e):
    L = e.cl
    L.rec("GetSideshowWithBestSatisfaction", (L.lab(e.r(3)),))
    e.write(e.r(4), struct.pack(">H", L.ret("BestId") & 0xFFFF))
    e.set_r(3, L.ret("GetSideshowWithBestSatisfaction") & MASK)


STUBS.internal(0x100e7930, stub_misbehaving)
STUBS.internal(0x100ec014, stub_best)


def setup(runner):
    T.install_std_stubs(runner.emu)
    WORLD["world"] = T.persist_alloc(runner.emu, WORLD_SIZE)
    WORLD["anal"] = T.persist_alloc(runner.emu, ANAL_SIZE)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        w, a = WORLD["world"], WORLD["anal"]
        self.L = e.cl = CL.PpcLog({w: 1, a: 2, w + 0x2c0: 3}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def off(self, m):
        return m

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def set_world(self):
        self.words(ADDR["gw"], [WORLD["world"]])
        self.words(ADDR["gwref"], [ADDR["gw"]])

    def set_main(self, v):
        self.words(ADDR["main"] + 0x750, [v])

    def log(self):
        return self.L.log

    def call(self, fn, obj):
        r3, _, _ = T.call_ppc(self.e, FUNCS[fn][1], [obj])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.world = lib.tpw_cadv_alloc(WORLD_SIZE)
        self.anal = lib.tpw_cadv_alloc(ANAL_SIZE)
        CL.host_begin(lib, {1: self.world, 2: self.anal, 3: self.world + 0x2c0}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cadv_addr(name.encode())

    def off(self, m):
        w4, w8, vals = self.lib.lay[:3]
        if m == 4:
            return w4
        if m == 8:
            return w8
        return vals + 2 * (m - 0x14)    # a BalanceValue is 8 bytes on the host (a union with a pointer), 4 on the Mac

    def alloc(self, n):
        return self.lib.tpw_cadv_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def set_world(self):
        self.lib.tpw_cadv_set_world(self.world, self.anal)

    def set_main(self, v):
        self.lib.tpw_cadv_set_main(s32(v))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, obj):
        return self.lib.tpw_cadv_call(fn, obj) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cadv_addr.restype, lib.tpw_cadv_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cadv_alloc.restype, lib.tpw_cadv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cadv_set_world.restype, lib.tpw_cadv_set_world.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_cadv_set_main.restype, lib.tpw_cadv_set_main.argtypes = None, [C.c_int32]
    lib.tpw_cadv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cadv_call.restype, lib.tpw_cadv_call.argtypes = C.c_int32, [C.c_int32, C.c_uint64]
    out = (C.c_int64 * 8)()
    n = lib.tpw_cadv_layout(out)
    lay = list(out)[:n]
    assert lay[0] < lay[1] < lay[2] and lay[2] + 8 * 393 <= lay[3] <= OBJ, lay
    lib.lay = lay
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

OBJ = 0x1000


def build(s, case):
    s.set_world()
    obj = s.alloc(OBJ)
    s.obj = obj
    for m, v in case["slots"].items():
        s.words(obj + s.off(m), [v])
    s.half(obj + s.off(4), case["w4"])
    s.words(obj + s.off(8), [case["w8"]])
    s.half(s.g("none"), case["none"])
    s.set_main(case["main"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "w4": s.rd_half(s.obj + s.off(4)), "w8": s.rd_word(s.obj + s.off(8))}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["fn"], s.obj))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

# fn: (total key, closed key, limit offset, points offset)
CLOSED = {
    0: ("NumberOfToiletsInPark", "NumberOfClosedToiletsInPark", 0x598, 0x59c),
    1: ("NumberOfStaffRoomsInPark", "NumberOfClosedStaffRoomsInPark", 0x5a0, 0x5a4),
    2: ("NumberOfObjectTypeInPark", "NumberOfClosedObjectTypeInPark", 0x590, 0x594),
    4: ("NumberOfObjectTypeInPark", "NumberOfClosedObjectTypeInPark", 0x588, 0x58c),
    5: ("NumberOfObjectTypeInPark", "NumberOfClosedObjectTypeInPark", 0x580, 0x584),
}
ALLKEYS = ["NumberOfToiletsInPark", "NumberOfClosedToiletsInPark", "NumberOfStaffRoomsInPark", "NumberOfClosedStaffRoomsInPark",
           "NumberOfObjectTypeInPark", "NumberOfClosedObjectTypeInPark", "GatesOpen", "MisbehavingVisitorsLastMonth",
           "NumberOfVisitorsSinceBeginning", "GetSideshowWithBestSatisfaction", "BestId", "HowManyTickets"]


def rpts(r):
    return r.choice([0, 1, -1, 7, 100, -250, r.randrange(-2000, 2000), r.randrange(-(1 << 31), 1 << 31)])


def rlimit(r, value, dl):
    return value + dl if dl is not None else r.choice([0, -1, 5, 50, r.randrange(-10, 120)])


def make(r, fn, dl=None, total=None, extra=None):
    """One case of function fn. dl: the offset of the compared limit to the value (None: a random limit)."""
    q = {k: [0] for k in ALLKEYS}
    slots = {}
    c = {"fn": fn, "queues": q, "slots": slots, "w4": r.randrange(0x10000), "w8": r.randrange(1 << 32),
         "none": r.choice([0, 1, 0x1234, 0xFFFF]), "main": 0}
    if fn in CLOSED:
        tk, ck, lo, po = CLOSED[fn]
        t = total if total is not None else r.choice([0, 1, 2, 3, 5, 10, 50, 100, r.randrange(1, 400)])
        cl = r.choice([0, 1, t, t + 1, r.randrange(0, 12), r.randrange(0, t + 3)])
        q[tk], q[ck] = [t], [cl]
        pct = tdiv(cl * 100, t) if t else 0
        slots[lo], slots[po] = rlimit(r, pct, dl), rpts(r)
        if fn == 5:
            q["GatesOpen"] = [1 if r.random() < 0.8 else r.choice([0, 1])]
            slots[0x57c] = r.choice([t - 1, t, t + 1, 0, r.randrange(-3, 12)])
            if extra is not None:
                q["GatesOpen"], slots[0x57c] = [extra[0]], t + extra[1]
    elif fn == 3:
        v = r.choice([0, 1, 5, -3, r.randrange(-50, 500)])
        q["MisbehavingVisitorsLastMonth"] = [v]
        slots[0x5a8], slots[0x5ac] = rlimit(r, v, dl), rpts(r)
    elif fn == 6:
        sat = r.choice([0, 1, 50, -5, r.randrange(-100, 1000)])
        q["GetSideshowWithBestSatisfaction"] = [sat]
        bid = r.choice([c["none"], c["none"], r.randrange(0x10000)])
        if extra is not None:
            bid = c["none"] if extra else (c["none"] + 1) & 0xFFFF
        q["BestId"] = [bid]
        slots[0x570], slots[0x574] = rlimit(r, sat, dl), rpts(r) % 20000 - 10000 if r.random() < 0.7 else rpts(r)
    else:
        mainv = r.choice([0, 1, -1, 100, 1000, r.randrange(-100000, 100000)])
        fac = T.f32(r.choice([0.0, 1.0, 0.5, 2.5, -1.0, r.uniform(-3, 3), r.uniform(0, 0.1)]))
        prod = T.f32(T.f32(float(mainv)) * fac)
        lim = int(prod)
        vis = lim + dl if dl is not None else r.choice([0, 1, -1, lim, r.randrange(0, 100000)])
        q["NumberOfVisitorsSinceBeginning"] = [vis]
        q["HowManyTickets"] = [r.choice([0, 0, 1, 2, -1, 5]) if extra is None else extra]
        c["main"] = mainv
        slots[0x5b8] = T.f32_bits(fac)
        slots[0x5bc] = rpts(r)
    return c


def edges(fn, seed):
    def gen():
        r = random.Random(seed)
        for dl in (-1, 0, 1):
            if fn in CLOSED:
                for t in (0, 1, 2, 7, 100):
                    for _ in range(2):
                        yield make(r, fn, dl, t)
                if fn == 5:
                    for g in (0, 1):
                        for md in (-1, 0, 1):
                            yield make(r, fn, dl, 10, (g, md))
            elif fn == 6:
                for none in (True, False):
                    yield make(r, fn, dl, extra=none)
            elif fn == 7:
                for tk in (-1, 0, 1, 3):
                    yield make(r, fn, dl, extra=tk)
            else:
                yield make(r, fn, dl)
    return gen


def rnd(fn):
    return lambda r: make(r, fn)


TESTS = [FnTest(name, ppc, host, edge=edges(i, 100 + i), rnd=rnd(i)) for i, (name, _, _) in enumerate(FUNCS)]
