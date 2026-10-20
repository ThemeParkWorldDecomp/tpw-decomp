"""Unit v5238, what a person thinks of a ride, against libtpw_unit_CPerson_TooPricey.dylib (tests/unit_CPerson_TooPricey.cpp), main app
("SimTheme Park"), src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_TooPricey.cpp:
  CPerson::TooPricey 0x10119eec, CPerson::ExtraExcitementEffects 0x10119c0c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the ride's price, record, chance, prize, control block and excitement, the person's preferred excitement, the world's park analyser
and its OfferOpinionOn) are fakes on both sides that record their arguments and answer from the case's queues. GetDBInfo answers block A
the first time and block B afterwards (the original keeps the first answer in a register and re-asks for the type).

The world of a case: a person block (floats at +0x19c, +0x1a4, +0x1a8, +0x1b0, the word at +0x1a0), the four balance words of gMainBalance
(+0x24 to +0x30), the ride records and the control block, gGameWorld. What is compared: the answer, the whole person block, the event log.

No fused operations (the floats are single adds and compares). Not tested: a balance divisor of 0 (divw by 0 is undefined on the hardware),
floats outside the int range, infinite or NaN in the fields that go through fctiwz (TooPricey's four, ExtraExcitementEffects' +0x1a8) (Unicorn and the hardware answer differently).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPerson_TooPricey"
MASK = 0xFFFFFFFF

FN_PRICEY, FN_EXTRA = 0x10119EEC, 0x10119C0C
SIZES = {FN_PRICEY: 1092, FN_EXTRA: 672}
OWN = set(SIZES)

C_PRICE, C_DBINFO, C_CHANCE, C_PRIZE, C_CONTROL = 0x1010F4F4, 0x10107CD4, 0x1010FBB8, 0x1010F52C, 0x10110580
C_EXCITE, C_PREFERRED, C_ANALYSER, C_OFFER = 0x1010CFB0, 0x1011B4B4, 0x1013EBC8, 0x100E4360

OBJ, WORLD, ANA = 0x20001000, 0x20002000, 0x20003000
ADDR = dict(gMainBalance=0x102B4568, gGameWorld=0x1037EBFC)
HOST_LAYOUT = [12]
PERSON = 0x240
FLOAT_OFFS = (0x19C, 0x1A4, 0x1A8, 0x1B0)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk_obj_stub(name, key):
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)),))
        e.set_r(3, L.ret(key) & MASK)
    return stub


def stub_dbinfo(e):
    L = e.cl
    L.rec("GetDBInfo", (L.lab(e.r(3)),))
    e.set_r(3, L.blockA if L.dbn == 0 else L.blockB)
    L.dbn += 1


def stub_control(e):
    L = e.cl
    L.rec("GetControl", (L.lab(e.r(3)),))
    e.set_r(3, L.control)


def stub_excite(e):
    L = e.cl
    L.rec("GetExcitementLevel", (L.lab(e.r(3)), e.r(4) & 0xFF))
    e.set_r(3, L.ret("GetExcitementLevel") & MASK)


def stub_preferred(e):
    L = e.cl
    L.rec("GetPreferredExcitement", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetPreferredExcitement") & MASK)


def stub_analyser(e):
    L = e.cl
    L.rec("GetParkAnalyser", (L.lab(e.r(3)),))
    e.set_r(3, ANA)


def stub_offer(e):
    L = e.cl
    L.rec("OfferOpinionOn", (L.lab(e.r(3)), s32(e.r(4)), e.r(5) & MASK))


STUBS.internal(C_PRICE, mk_obj_stub("GetPricePerUse", "GetPricePerUse"))
STUBS.internal(C_DBINFO, stub_dbinfo)
STUBS.internal(C_CHANCE, mk_obj_stub("GetChanceOfWinning", "GetChanceOfWinning"))
STUBS.internal(C_PRIZE, mk_obj_stub("GetPrizeValue", "GetPrizeValue"))
STUBS.internal(C_CONTROL, stub_control)
STUBS.internal(C_EXCITE, stub_excite)
STUBS.internal(C_PREFERRED, stub_preferred)
STUBS.internal(C_ANALYSER, stub_analyser)
STUBS.internal(C_OFFER, stub_offer)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({OBJ: 1, WORLD: 2, ANA: 3}, case["queues"])
        self.L.dbn = 0
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

    def setptrs(self, a, b, c):
        self.L.blockA, self.L.blockB, self.L.control = a, b, c

    def call(self, fn, person):
        r3, _, _ = T.call_ppc(self.e, fn, [person, OBJ])
        return r3 & 0xFF if fn == FN_PRICEY else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: OBJ, 2: WORLD, 3: ANA}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_t_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_t_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setptrs(self, a, b, c):
        self.lib.tpw_t_set(a, b, c)

    def call(self, fn, person):
        f = self.lib.tpw_t_pricey if fn == FN_PRICEY else self.lib.tpw_t_extra
        return f(person, OBJ) & 0xFF if fn == FN_PRICEY else (f(person, OBJ) and 0)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_t_addr.restype, lib.tpw_t_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_t_alloc.restype, lib.tpw_t_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_t_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_t_set.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_t_pricey.restype, lib.tpw_t_pricey.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    lib.tpw_t_extra.restype, lib.tpw_t_extra.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_t_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    p = s.alloc(PERSON)
    s.words(p + 0x19C, [case["f19c"]])
    s.words(p + 0x1A0, [case["w1a0"]])
    s.words(p + 0x1A4, [case["f1a4"]])
    s.words(p + 0x1A8, [case["f1a8"]])
    s.words(p + 0x1B0, [case["f1b0"]])
    s.words(s.g("gMainBalance") + 0x24, case["bal"])
    s.words(s.g("gGameWorld"), [WORLD])
    a, b, c = s.alloc(0x7C0), s.alloc(0x7C0), s.alloc(0x10)
    for blk, fields in ((a, case["dbA"]), (b, case["dbB"])):
        for off, v in fields.items():
            s.words(blk + off, [v])
    s.words(c + 0xC, [case["ctrl"]])
    s.setptrs(a, b, c)
    return p


def run(s, case):
    try:
        p = build(s, case)
        answer = s.call(case["fn"], p)
        words = s.rd_words(p, PERSON // 4)
        for off in FLOAT_OFFS:
            words[off // 4] = canon_f(words[off // 4])
        return {"answer": answer, "log": s.log(), "person": words}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ------------------------------------------------------------------------------------------------------------ case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x3F800000, 0xBF800000, fb(100.0), fb(99.5), fb(-0.5), fb(0.5)]


def rfree(r):
    """A float field read only by fadds and compares: anything."""
    k = r.random()
    if k < 0.15:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-30, 130))
    return fb(r.uniform(-1000, 1000))


def rint_f(r):
    """A float field that goes through fctiwz: finite and inside the int range."""
    k = r.random()
    if k < 0.2:
        return r.choice([0, fb(100.0), fb(99.9), fb(255.0), fb(256.0), fb(-1.0), fb(-0.5), fb(0.5), fb(1000.0), 0x80000000])
    if k < 0.7:
        return fb(r.uniform(0, 110))
    return fb(r.uniform(-1000, 1000))


def garbage(r, v):
    return (v & 0xFF) | (r.choice([0, 0, 0x100, 0x5A00, 0xABCD00, 0xFF000000]))


def base(r, fn):
    c = {"fn": fn, "f19c": rfree(r), "w1a0": r.choice([0, 1, 5, 50, 100, 300, r.randrange(0, 600), 0xFFFFFFFF]),
         "f1a4": rint_f(r), "f1a8": rint_f(r), "f1b0": rfree(r),
         "bal": [r.randrange(-60, 60), r.randrange(-60, 60), r.randrange(-60, 60), r.choice([1, 2, 3, 5, 10, 17, 50, -3, -1, 255, 300])]}
    c["dbA"], c["dbB"], c["ctrl"] = {}, {}, r.choice([0, -50, -100, -101, 10, 100, r.randrange(-200, 200)])
    return c


def extra_case(r, l1=None, pref=None, l2=None, l3=None, **over):
    c = base(r, FN_EXTRA)
    l1 = r.choice([0, 1, 5, 40, 128, 255, r.randrange(0, 256)]) if l1 is None else l1
    l2 = l1 if l2 is None else l2
    l3 = l2 if l3 is None else l3
    pref = r.randrange(0, 256) if pref is None else pref
    if l1 and r.random() < 0.5:
        pref = max(0, min(255, l2 + r.choice([-45, -40, -39, -28, -15, -14, -5, -4, 0, 4, 5, 14, 15, 39, 40, 45])))
    c["queues"] = {"GetExcitementLevel": [garbage(r, l1), garbage(r, l2), garbage(r, l3)], "GetPreferredExcitement": [garbage(r, pref)]}
    c.update(over)
    return c


def pricey_case(r, **over):
    c = base(r, FN_PRICEY)
    c["f19c"], c["f1b0"] = rint_f(r), rint_f(r)   # all four floats go through fctiwz here
    def small(): return r.choice([0, 1, 10, 50, 100, r.randrange(0, 300), r.randrange(-50, 400)])
    c["dbA"] = {0x140: small(), 0x144: small(), 0x148: small(), 0x14C: small(), 0x150: small(),
                0x158: r.choice([0, 1, 2, 3, 4, 5, -1, 6]), 0x15C: r.choice([0, 1, 2, 3, 4, -1]), 0x7A8: r.choice([0, 1, 2, 3, 0x7FFFFFFF])}
    c["dbB"] = {0x7A8: r.choice([0, 1, 1, 2, 2, 3, -1]), 0x158: 99, 0x15C: 99}
    price = r.choice([0, 1, 5, 20, 80, 100, 120, r.randrange(1, 400), r.randrange(1, 100000), 0xFFFFFFFF])
    c["queues"] = {"GetPricePerUse": [price, r.randrange(0, 1000)],
                   "GetChanceOfWinning": [garbage(r, r.choice([0, 1, 50, 100, 255, r.randrange(0, 256)]))],
                   "GetPrizeValue": [r.choice([0, 1, 100, 1000, r.randrange(0, 100000), 0x7FFFFFFF, 0xFFFFFFFF])]}
    c.update(over)
    return c


def extra_edges():
    r = random.Random(23801)
    for d in (0, 4, 5, 14, 15, 39, 40, 41, 200):
        for sign in (1, -1):
            yield extra_case(r, l1=100, l2=100, pref=100 + sign * d)
    yield extra_case(r, l1=0)
    yield extra_case(r, l1=0x100 | 0)   # low byte 0 with garbage above is a masked zero
    for amount in (100, 99, 0, -1, -200, 1, 50):
        for f in (fb(0.0), fb(100.0), fb(50.0), fb(-1.0), fb(0.5), fb(-0.5), fb(99.5)):
            for pref in (100, 108, 130):
                yield extra_case(r, l1=100, pref=pref, f19c=f, f1b0=f, bal=[amount, amount, amount, 1])
    for f1a8 in (0, fb(100.0), fb(80.0), fb(79.0), fb(255.0), fb(256.0), fb(-1.0), fb(19.0), fb(20.0)):
        for lvl in (1, 19, 20, 255):
            yield extra_case(r, l1=lvl, f1a8=f1a8, f1b0=fb(r.uniform(0, 100)), bal=[1, 1, 1, r.choice([1, 2, 3, 7])])


def pricey_edges():
    r = random.Random(23802)
    yield pricey_case(r, queues={"GetPricePerUse": [0, 0], "GetChanceOfWinning": [0], "GetPrizeValue": [0]})
    for t in (0, 1, 2, 3, -1):
        for k in (0, 1, 2, 3, 4, 5, -1):
            for m in (0, 1, 2, 3, -1):
                c = pricey_case(r)
                c["dbA"][0x158], c["dbA"][0x15C] = k, m
                c["dbB"][0x7A8] = t
                c["dbA"][0x7A8] = 3 - t
                c["queues"]["GetPricePerUse"] = [r.choice([1, 3, 30]), 0]
                yield c
    for price in (1, 50, 100, 115, 116, 117, 200, 0xFFFFFFFF):
        for w in (0, price - 1, price, price + 1):
            c = pricey_case(r, w1a0=w & MASK)
            c["dbB"][0x7A8] = 1
            c["queues"]["GetPricePerUse"] = [price, 0]
            yield c


def extra_random(r):
    return extra_case(r)


def pricey_random(r):
    return pricey_case(r)


TESTS = [
    FnTest("CPerson::ExtraExcitementEffects", ppc, host, edge=extra_edges, rnd=extra_random),
    FnTest("CPerson::TooPricey", ppc, host, edge=pricey_edges, rnd=pricey_random),
]
