"""Unit v5256, eight advisor scorers, against libtpw_unit_CAdvisorScoring__ADVISOR_CONGRAT_PRICING_DRINKS__2.dylib
(tests/unit_CAdvisorScoring__ADVISOR_CONGRAT_PRICING_DRINKS__2.cpp), main app ("SimTheme Park"),
src/game/advisor/AdvisorScoring.cpp:
  _ADVISOR_CONGRAT_PRICING_DRINKS_GREAT_SATISFACTION 0x1001a4e8, _ADVISOR_PRICING_SIDESHOWS_POOR_SATISFACTION 0x10019c24,
  _ADVISOR_PRICING_DRINKS_POOR_SATISFACTION 0x10019b10, _ADVISOR_PRICING_RESTAURANT_POOR_SATISFACTION 0x100199f8,
  _ADVISOR_GOLD_TICKET_NEAR_TO_XPEEPS 0x1001b054, _ADVISOR_PRICING_SHOPS_POOR_SATISFACTION 0x100198ec,
  _ADVISOR_PRICING_FOOD_POOR_SATISFACTION 0x100197dc, _ADVISOR_PRICING_SIDESHOWS_POOR_VALUE 0x100196d0

A case is a scoring object (balance values at the Mac's offsets), the "none" id globals, and a call. The world, the analyser and the
master save are fixed fake addresses; GetParkAnalyser, the analyser getters, NumberOfVisitorsSinceBeginning, CTRACE, the master save
constructor and HaveTicket are fakes on both sides that record their arguments and answer from the case's queues.
Compared: the answer, the event log, the object's words at 4 and 8, the none global, the master save pointer (0 / existing / new).
No fused operations; the gold ticket scorer runs under HardwareRounding (fctiwz). Not tested: gold ticket products beyond 2^31
(fctiwz saturates differently in Unicorn).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from unicorn import UC_HOOK_CODE
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_CONGRAT_PRICING_DRINKS__2"
MASK = 0xFFFFFFFF

WORLD, ANALYSER, OLDSAVE = 0x1000, 0x2000, 0x3000
GETPARK, BESTSAT, WORSTSAT_T, WORSTSAT, SIDESAT, SIDEVAL = 0x1013ebc8, 0x100eaed4, 0x100eb0b4, 0x100eb970, 0x100ec228, 0x100ebdbc
CTRACE, VISITORS, OPNEW, MSCTOR, HAVETICKET = 0x1006fcbc, 0x100e7994, 0x102384b8, 0x101694dc, 0x1016a12c

ADDR = dict(gGameWorldRef=0x102a460c, gMasterSave=0x10380a8c, mainValues=0x102b4568 + 8,
            s_sideshows_value=0x1027df0e, s_food_sat=0x1027df10, s_shops_sat=0x1027df12, s_restaurant_sat=0x1027df14,
            s_drinks_sat=0x1027df16, s_sideshows_sat=0x1027df18, s_drinks_great=0x1027df2a)
HOST_LAYOUT = [32, 8, 16, 4, 8]
MAC_VALUES = 0x14       # mValues in the scoring object; 4 bytes a slot (host: 8)

# name, address, size, callee event, type arg (None for none), sentinel, limit slot, text (None: great)
SAT = [
    ("_ADVISOR_CONGRAT_PRICING_DRINKS_GREAT_SATISFACTION", 0x1001a4e8, 160, "BestSat", 4, "s_drinks_great", 341, None),
    ("_ADVISOR_PRICING_SIDESHOWS_POOR_SATISFACTION", 0x10019c24, 188, "SideSat", None, "s_sideshows_sat", 323, "sideshows poor satisfaction (%d)\n"),
    ("_ADVISOR_PRICING_DRINKS_POOR_SATISFACTION", 0x10019b10, 192, "WorstSatT", 4, "s_drinks_sat", 321, "drinks poor satisfaction (%d)\n"),
    ("_ADVISOR_PRICING_RESTAURANT_POOR_SATISFACTION", 0x100199f8, 192, "WorstSatT", 3, "s_restaurant_sat", 319, "restaurant poor satisfaction (%d)\n"),
    ("_ADVISOR_PRICING_SHOPS_POOR_SATISFACTION", 0x100198ec, 188, "WorstSat", None, "s_shops_sat", 317, "shops poor satisfaction (%d)\n"),
    ("_ADVISOR_PRICING_FOOD_POOR_SATISFACTION", 0x100197dc, 192, "WorstSatT", 2, "s_food_sat", 315, "Food poor satisfaction (%d)\n"),
    ("_ADVISOR_PRICING_SIDESHOWS_POOR_VALUE", 0x100196d0, 188, "SideVal", None, "s_sideshows_value", 313, "sideshows poor value (%d)\n"),
]
GOLD = ("_ADVISOR_GOLD_TICKET_NEAR_TO_XPEEPS", 0x1001b054, 236)
OWN = {a: s for _, a, s, *_ in SAT}
OWN[GOLD[1]] = GOLD[2]

class ROUNDING(HardwareRounding):
    """HardwareRounding aimed at the gold ticket scorer (Unicorn's fctiwz differs from the hardware)."""

    def __enter__(self):
        begin = GOLD[1]
        self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + GOLD[2] - 1))



_KEPT = {}


class Rounded:
    """One ROUNDING hook per emulator, kept for the whole run (a hook added and removed per case exhausts Unicorn's hook slots)."""

    def __init__(self, e):
        self.e = e

    def __enter__(self):
        if id(self.e) not in _KEPT:
            r = ROUNDING(self.e)
            r.__enter__()
            _KEPT[id(self.e)] = r
        return self

    def __exit__(self, *args):
        return False


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def stub_getpark(e):
    e.cl.rec("GetParkAnalyser", (e.cl.lab(e.r(3)),))
    e.set_r(3, ANALYSER)


def shop_stub(name, id_reg, typed):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), s32(e.r(4))) if typed else (L.lab(e.r(3)),))
        e.write(e.r(id_reg), struct.pack(">H", L.ret("id") & 0xFFFF))
        e.set_r(3, L.ret("val") & MASK)
    return f


def stub_visitors(e):
    e.cl.rec("Visitors", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("val") & MASK)


def stub_have(e):
    e.cl.rec("HaveTicket", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, e.cl.ret("have") & 0xFF)


def stub_new(e):
    e.set_r(3, e.alloc(max(e.r(3), 4)))


STUBS.internal(GETPARK, stub_getpark)
STUBS.internal(BESTSAT, shop_stub("BestSat", 5, True))
STUBS.internal(WORSTSAT_T, shop_stub("WorstSatT", 5, True))
STUBS.internal(WORSTSAT, shop_stub("WorstSat", 4, False))
STUBS.internal(SIDESAT, shop_stub("SideSat", 4, False))
STUBS.internal(SIDEVAL, shop_stub("SideVal", 4, False))
STUBS.internal(VISITORS, stub_visitors)
STUBS.internal(HAVETICKET, stub_have)
STUBS.internal(OPNEW, stub_new)
STUBS.internal(CTRACE, CL.recorder("CTRACE", "usi"))
STUBS.internal(MSCTOR, CL.recorder("ctor", "N"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    stride = 4

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({WORLD: 1, ANALYSER: 2, OLDSAVE: 3}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def setptr(self, a, v):
        self.words(a, [v])

    def getptr(self, a):
        return self.rd_words(a, 1)[0]

    def world(self):
        cell = self.alloc(4)
        self.words(cell, [WORLD])
        self.words(ADDR["gGameWorldRef"], [cell])

    def new_obj(self):
        return self.alloc(0x700)

    def slot(self, obj, i, v):
        self.words(obj + MAC_VALUES + 4 * i, [v])

    def main_slot(self, i, v):
        self.words(ADDR["mainValues"] + 4 * i, [v])

    def call(self, addr, obj):
        with Rounded(self.e):
            r3, _, _ = T.call_ppc(self.e, addr, [obj])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    stride = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: WORLD, 2: ANALYSER, 3: OLDSAVE}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_adv_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_adv_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def getptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def world(self):
        pass

    def new_obj(self):
        return self.alloc(0x2000)

    def slot(self, obj, i, v):
        self.words(obj + 32 + 8 * i, [v, 0])

    def main_slot(self, i, v):
        self.words(self.g("mainValues") + 8 * i, [v, 0])

    def call(self, addr, obj):
        return getattr(self.lib, "tpw_adv_" + NAMES[addr][len("_ADVISOR_"):])(obj) & MASK


NAMES = {a: n for n, a, *_ in SAT}
NAMES[GOLD[1]] = GOLD[0]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, "tpw_adv_addr").restype, getattr(lib, "tpw_adv_addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, "tpw_adv_alloc").restype, getattr(lib, "tpw_adv_alloc").argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_adv_layout.argtypes = [C.POINTER(C.c_int64)]
    for name in NAMES.values():
        f = getattr(lib, "tpw_adv_" + name[len("_ADVISOR_"):])
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_adv_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.world()
    obj = s.new_obj()
    for i, v in case["slots"].items():
        s.slot(obj, i, v)
    for i, v in case.get("main", {}).items():
        s.main_slot(i, v)
    s.write(obj + 4, struct.pack(s.fmt[0] + "H", case["w4"]))
    s.words(obj + 8, [case["w8"]])
    for name, v in case["none"].items():
        s.write(s.g(name), struct.pack(s.fmt[0] + "H", v))
    s.setptr(s.g("gMasterSave"), OLDSAVE if case.get("save") else 0)
    return obj


def observe(s, case, obj, answer):
    ms = s.getptr(s.g("gMasterSave"))
    return {"answer": answer, "log": s.log(),
            "w4": struct.unpack(s.fmt[0] + "H", s.read(obj + 4, 2))[0], "w8": s.rd_words(obj + 8, 1),
            "none": [struct.unpack(s.fmt[0] + "H", s.read(s.g(n), 2))[0] for n in sorted(case["none"])],
            "save": 0 if ms == 0 else (1 if ms == OLDSAVE else 2)}


def run(s, case):
    try:
        obj = build(s, case)
        return observe(s, case, obj, s.call(case["addr"], obj))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rint(r):
    return r.choice([0, 1, -1, 2, 100, -100, r.randrange(-1000, 1000), r.randrange(-2**31, 2**31)]) & MASK


def sat_case(r, fn, figure=None, hit=None):
    name, addr, size, ev, ty, sent, slot, text = fn
    lim = r.choice([0, 1, -1, r.randrange(-500, 500)])
    mult = rint(r)
    if figure is None:
        figure = r.choice([lim - 1, lim, lim + 1, r.randrange(-1000, 1000), r.randrange(-2**31, 2**31)])
    ident = r.choice([0, 1, 0xFFFF, r.randrange(0x10000)])
    none = ident if (hit if hit is not None else r.random() < 0.3) else (ident + 1) & 0xFFFF
    return {"addr": addr, "queues": {"val": [figure & MASK], "id": [ident]},
            "slots": {slot: lim & MASK, slot + 1: mult}, "none": {sent: none},
            "w4": r.randrange(0x10000), "w8": r.randrange(1 << 32)}


def sat_edges(fn):
    def gen():
        r = random.Random(fn[1])
        slot = fn[6]
        for d in (-1, 0, 1):
            for hit in (False, True):
                c = sat_case(r, fn, hit=hit)
                lim = s32(c["slots"][slot])
                c["queues"]["val"] = [(lim + d) & MASK]
                yield c
        for hit in (False, True):
            yield sat_case(r, fn, figure=-2**31, hit=hit)
            yield sat_case(r, fn, figure=2**31 - 1, hit=hit)
    return gen


def sat_random(fn):
    return lambda r: sat_case(r, fn)


def gold_case(r, edge=None):
    bal = r.choice([0, 1, -1, 100, r.randrange(-10**6, 10**6), 10**7, -10**7])
    mult = r.choice([0.0, 1.0, -1.0, 0.5, 2.0, r.uniform(-50, 50), r.uniform(0, 5)])
    t = int(T.f32(T.f32(bal) * T.f32(mult)))
    visitors = r.choice([t - 1, t, t + 1, r.randrange(-1000, 1000), r.randrange(0, 10**6)])
    return {"addr": GOLD[1], "queues": {"val": [visitors & MASK], "have": [r.choice([0, 1])]},
            "slots": {364: fb(mult), 365: rint(r)}, "main": {466: bal & MASK}, "none": {},
            "save": r.choice([0, 1]), "w4": r.randrange(0x10000), "w8": r.randrange(1 << 32)}


def gold_edges():
    r = random.Random(0x1001b054)
    for bal, mult, vis in ((10, 1.0, 11), (10, 1.0, 10), (10, 1.0, 9), (-10, -1.0, 11), (-10, -1.0, 10), (-10, -1.0, 9), (0, 1.0, 0), (0, 1.0, 1),
                           (7, 0.5, 4), (7, 0.5, 3), (7, 0.5, 2), (-10, 1.0, -9), (-10, 1.0, -10), (-10, 1.0, -11), (10, -1.0, -9), (10, -1.0, -10),
                           (-7, 0.5, -3), (-7, 0.5, -4)):
        c = gold_case(r)
        c["main"] = {466: bal & MASK}
        c["slots"][364] = fb(mult)
        c["queues"]["val"] = [vis & MASK]
        yield c
    for _ in range(40):
        yield gold_case(r)


TESTS = [FnTest(fn[0], ppc, host, edge=sat_edges(fn), rnd=sat_random(fn)) for fn in SAT]
TESTS.append(FnTest(GOLD[0], ppc, host, edge=gold_edges, rnd=gold_case))
