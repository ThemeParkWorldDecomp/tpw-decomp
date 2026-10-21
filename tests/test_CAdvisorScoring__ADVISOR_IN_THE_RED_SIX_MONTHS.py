"""Unit v5252, eight advisor scorers, against libtpw_unit_CAdvisorScoring__ADVISOR_IN_THE_RED_SIX_MONTHS.dylib
(tests/unit_CAdvisorScoring__ADVISOR_IN_THE_RED_SIX_MONTHS.cpp), main app ("SimTheme Park"),
src/game/advisor/AdvisorScoring.cpp:
  _ADVISOR_IN_THE_RED_SIX_MONTHS 0x10016420, _ADVISOR_TAKE_LOAN 0x1001636c, _ADVISOR_IN_THE_RED_THREE_MONTHS 0x10016524,
  _ADVISOR_IN_THE_RED_MONTH_LEFT 0x10016630, _ADVISOR_WEALTH_HIGH 0x10016160, _ADVISOR_GENERAL_LAX_SECURITY 0x10015f68,
  _ADVISOR_LOTS_HANDYMAN 0x100168e8, _ADVISOR_LOTS_ENTERTAINERS 0x100169b4

A case is a scoring object whose balance values (at the Mac's offsets from the object) are set, a world pointer in gGameWorld and
a call. The callees (CWorld::GetBankAccount/GetParkAnalyser, CBankAccount::GetBalance/AnyLoanAvailable/BeenInTheRedFor/
GetNumberOfLoansBought, CParkAnalyser::MisbehavingVisitorsLastMonth/NumberOfObjectTypeInPark/NumberOfStaffType) are fakes on both
sides that record their arguments and answer from the case. The runtime's 64-bit divide 0x10237e1c runs as the original on the
PowerPC side. Compared: the answer and the event log. No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_IN_THE_RED_SIX_MONTHS"
MASK = 0xFFFFFFFF

FNS = {
    "_ADVISOR_IN_THE_RED_SIX_MONTHS": (0x10016420, 188),
    "_ADVISOR_TAKE_LOAN": (0x1001636C, 120),
    "_ADVISOR_IN_THE_RED_THREE_MONTHS": (0x10016524, 196),
    "_ADVISOR_IN_THE_RED_MONTH_LEFT": (0x10016630, 188),
    "_ADVISOR_WEALTH_HIGH": (0x10016160, 124),
    "_ADVISOR_GENERAL_LAX_SECURITY": (0x10015F68, 136),
    "_ADVISOR_LOTS_HANDYMAN": (0x100168E8, 140),
    "_ADVISOR_LOTS_ENTERTAINERS": (0x100169B4, 140),
}
OWN = {a for a, _ in FNS.values()}
HOST_NAME = {n: "tpw_adv_" + n[len("_ADVISOR_"):] for n in FNS}

GET_BANK, GET_BALANCE, ANY_LOAN, BEEN_RED, NUM_LOANS = 0x1013EDFC, 0x100F32B4, 0x100F3EE0, 0x100F3490, 0x100F3BD0
GET_ANALYSER, MISB, OBJ_TYPE, STAFF = 0x1013EBC8, 0x100E7930, 0x100EA5D8, 0x100E8144
WORLD, BANK, ANALYSER = 0x4000, 0x5000, 0x6000
G_WORLD = 0x1037EBFC
DIVISOR = 0x1792F8648000
OFFS = [0x314, 0x318, 0x330, 0x334, 0x340, 0x344, 0x348, 0x34C, 0x360, 0x364, 0x368, 0x36C]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lab(e, v):
    return e.cl.lab(v)


def mk(name, spec):
    def f(e):
        L = e.cl
        L.rec(name, tuple(spec(e, L)))
        return L
    return f


def s_get_bank(e):
    e.cl.rec("GetBankAccount", (e.cl.lab(e.r(3)),))
    e.set_r(3, BANK)


def s_get_balance(e):
    e.cl.rec("GetBalance", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("GetBalance") & MASK)


def s_any_loan(e):
    e.cl.rec("AnyLoanAvailable", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("AnyLoanAvailable") & MASK)


def s_been_red(e):
    e.cl.rec("BeenInTheRedFor", (e.cl.lab(e.r(4)),))
    hi, lo = e.cl.ret("RedHi") & MASK, e.cl.ret("RedLo") & MASK
    e.write(e.r(3), struct.pack(">II", hi, lo))


def s_num_loans(e):
    e.cl.rec("GetNumberOfLoansBought", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("GetNumberOfLoansBought") & MASK)


def s_get_analyser(e):
    e.cl.rec("GetParkAnalyser", (e.cl.lab(e.r(3)),))
    e.set_r(3, ANALYSER)


def s_misb(e):
    e.cl.rec("MisbehavingVisitorsLastMonth", (e.cl.lab(e.r(3)), e.r(4) & 0xFF))
    e.set_r(3, e.cl.ret("Misbehaving") & MASK)


def s_obj_type(e):
    e.cl.rec("NumberOfObjectTypeInPark", (e.cl.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5))))
    e.set_r(3, e.cl.ret("ObjectTypes") & MASK)


def s_staff(e):
    e.cl.rec("NumberOfStaffType", (e.cl.lab(e.r(3)), T.s32(e.r(4))))
    e.set_r(3, e.cl.ret("StaffType") & MASK)


for _a, _f in ((GET_BANK, s_get_bank), (GET_BALANCE, s_get_balance), (ANY_LOAN, s_any_loan), (BEEN_RED, s_been_red),
               (NUM_LOANS, s_num_loans), (GET_ANALYSER, s_get_analyser), (MISB, s_misb), (OBJ_TYPE, s_obj_type),
               (STAFF, s_staff)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({WORLD: 1, BANK: 2, ANALYSER: 3}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def gworld(self):
        return G_WORLD

    def alloc_obj(self):
        return self.e.alloc(0x800)

    def bal_addr(self, obj, off):
        return obj + off

    def write(self, a, d):
        self.e.write(a, d)

    def log(self):
        return self.L.log

    def call(self, name, obj):
        r3, _, _ = T.call_ppc(self.e, FNS[name][0], [obj])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: WORLD, 2: BANK, 3: ANALYSER}, case["queues"])

    def done(self):
        pass

    def gworld(self):
        return self.lib.tpw_adv_addr(b"gGameWorld")

    def alloc_obj(self):
        return self.lib.tpw_adv_alloc(self.lib.size)

    def bal_addr(self, obj, off):
        return obj + self.lib.base + ((off - 0x18) // 4) * self.lib.stride

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, name, obj):
        return getattr(self.lib, HOST_NAME[name])(obj) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, "tpw_adv_addr").restype, getattr(lib, "tpw_adv_addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, "tpw_adv_alloc").restype, getattr(lib, "tpw_adv_alloc").argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_adv_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in FNS:
        f = getattr(lib, HOST_NAME[n])
        f.restype, f.argtypes = C.c_int32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_adv_layout(out)
    lib.base, lib.stride, lib.size = out[0], out[1], out[2]
    assert lib.stride == 8 and lib.base % 8 == 0
    lib.bound = True


def build(s, case):
    obj = s.alloc_obj()
    for off, v in case["bal"].items():
        s.words(s.bal_addr(obj, off), [v])
    s.words(s.gworld(), [WORLD])
    return obj


def run(s, case):
    try:
        obj = build(s, case)
        return {"answer": s.call(case["fn"], obj), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def pick(r, vals):
    return r.choice(vals)


def make(r, fn, months=None, balance=None, loan=None, ticks_off=None):
    bal = {off: r.choice([0, 1, -1, 5, 100, 0x7FFFFFFF, -0x80000000, r.randint(-1000, 1000)]) & MASK for off in OFFS}
    b330 = T.s32(bal[0x330])
    gb = r.choice([-5, -1, 0, 1, 7, b330, T.s32(b330 + 1), T.s32(b330 - 1), r.randint(-100000, 100000)]) if balance is None else balance
    if months is None:
        months = r.choice([-1, 0, 1, 2, 3, 4, 5, 6, 7, 100])
    if ticks_off is None:
        ticks_off = r.choice([0, DIVISOR - 1, 1, r.randrange(DIVISOR)])
    ticks = months * DIVISOR + (ticks_off if months >= 0 else -ticks_off)
    if r.random() < 0.05:
        ticks = r.randrange(-(1 << 63), 1 << 63)
    ticks &= (1 << 64) - 1
    b314 = T.s32(bal[0x314])
    misb = r.choice([b314, T.s32(b314 + 1), T.s32(b314 - 1), 0, 3, r.randint(-50, 50)])
    obj = r.choice([0, 1, 3, 10, -2, r.randint(-5, 20)])
    lim_off = 0x360 if fn == "_ADVISOR_LOTS_HANDYMAN" else 0x368
    limit = T.s32(T.s32(bal[lim_off]) + obj)
    staff = r.choice([limit, T.s32(limit + 1), T.s32(limit - 1), r.randint(-5, 20)])
    q = {
        "GetBalance": [gb & MASK],
        "AnyLoanAvailable": [r.choice([0, 1]) if loan is None else loan],
        "RedHi": [ticks >> 32],
        "RedLo": [ticks & MASK],
        "GetNumberOfLoansBought": [r.choice([0, 0, 1, 2, MASK, 0x80000000])],
        "Misbehaving": [misb & MASK],
        "ObjectTypes": [obj & MASK],
        "StaffType": [staff & MASK],
    }
    return {"fn": fn, "bal": bal, "queues": q}


def edges_for(fn):
    def gen():
        r = random.Random(sum(map(ord, fn)))
        for months in (-1, 0, 2, 3, 4, 5, 6, 7):
            for balance in (-1, 0, 1):
                for loan in (0, 1):
                    for off in (0, DIVISOR - 1):
                        yield make(r, fn, months, balance, loan, off)
        for _ in range(60):
            yield make(r, fn)
    return gen


def rnd_for(fn):
    return lambda r: make(r, fn)


TESTS = [FnTest(n, ppc, host, edge=edges_for(n), rnd=rnd_for(n)) for n in FNS]
