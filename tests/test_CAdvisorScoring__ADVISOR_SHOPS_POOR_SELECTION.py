"""Unit v5939, nine advisor scorers, against libtpw_unit_CAdvisorScoring__ADVISOR_SHOPS_POOR_SELECTION.dylib
(tests/unit_CAdvisorScoring__ADVISOR_SHOPS_POOR_SELECTION.cpp), main app,
src/AdvisorScoring_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN_CAdvisorScoring__ADVISOR_SHOPS_POOR_SELECTION.cpp:
  _ADVISOR_RIDES_RESEARCH_MORE 0x10013f28, _ADVISOR_RIDES_BUILD_RIDES 0x10014078, _ADVISOR_RIDES_NO_UPGRADES 0x10014160,
  _ADVISOR_RIDES_MORE_UPGRADES 0x100142a4, _ADVISOR_SHOPS_POOR_SELECTION 0x10014558, _ADVISOR_SHOPS_RESEARCH_MORE 0x100146a8,
  _ADVISOR_SHOPS_BUILD_SHOPS 0x100147f8, _ADVISOR_SHOPS_BUILD_MORE_SHOPS 0x1001493c, _ADVISOR_SIDESHOWS_POOR_SELECTION 0x10014aac

A case is a scoring object (its balance values at the Mac offsets, written through each side's own slot layout), a game world pointer and a
call. The world is a fake address on both sides (never read); the analyser, research lab and bank account the getters return are fake
addresses too. Every callee outside the unit (the three world getters, the analyser counts, GetResearchSpeed, GetBalance and
GetFunnyTimeSinceSessionStart) is a fake on both sides that records its arguments (objects as labels) and answers from the case's queues.
The 64-bit division by the glue routine 0x10237e1c runs as the original on the PowerPC side.

What is compared: the answer and the event log. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_SHOPS_POOR_SELECTION"
MASK = 0xFFFFFFFF

# (name, address, size, index in the host wrapper)
FUNCS = [
    ("CAdvisorScoring::_ADVISOR_RIDES_RESEARCH_MORE", 0x10013F28, 268, 0),
    ("CAdvisorScoring::_ADVISOR_RIDES_BUILD_RIDES", 0x10014078, 164, 1),
    ("CAdvisorScoring::_ADVISOR_RIDES_NO_UPGRADES", 0x10014160, 256, 2),
    ("CAdvisorScoring::_ADVISOR_RIDES_MORE_UPGRADES", 0x100142A4, 200, 3),
    ("CAdvisorScoring::_ADVISOR_SHOPS_POOR_SELECTION", 0x10014558, 264, 4),
    ("CAdvisorScoring::_ADVISOR_SHOPS_RESEARCH_MORE", 0x100146A8, 268, 5),
    ("CAdvisorScoring::_ADVISOR_SHOPS_BUILD_SHOPS", 0x100147F8, 256, 6),
    ("CAdvisorScoring::_ADVISOR_SHOPS_BUILD_MORE_SHOPS", 0x1001493C, 296, 7),
    ("CAdvisorScoring::_ADVISOR_SIDESHOWS_POOR_SELECTION", 0x10014AAC, 264, 8),
]
OWN = {a for _, a, _, _ in FUNCS}

WORLD, ANALYSER, LAB, BANK = 0x30000000, 0x7000, 0x7100, 0x7200
LABELS = {WORLD: 1, ANALYSER: 2, LAB: 3, BANK: 4, WORLD + 0x2A0: 5}
ADDR = dict(world=0x1037EBFC)     # gGameWorld; the TOC slot 0x102a460c holds its address

OFFS = [0x22C, 0x230, 0x234, 0x238, 0x23C, 0x240, 0x244, 0x258, 0x25C, 0x260, 0x264, 0x268, 0x26C, 0x270, 0x274, 0x278, 0x27C,
        0x280, 0x284, 0x634]
QUEUE_KEYS = ["GetDaysSinceLastObjectBuilt", "NumberOfObjectTypeInPark", "NumberOfStaffType", "TypesOfParticularObjectTypeInPark",
              "TypesOfShopsAvailable", "TypesOfSideshowsAvailable", "TypesOfUpgradesAvailable", "TypesOfRidesAvailable",
              "TypesOfRidesLeftToResearch", "TypesOfShopsLeftToResearch", "GetResearchSpeed", "GetBalance", "Time"]


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def getter(name, result):
    def fn(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)),))
        e.set_r(3, result)
    return fn


def counter(name, kinds):
    def fn(e):
        L = e.cl
        args = [L.lab(e.r(3))]
        for i, k in enumerate(kinds):
            v = e.r(4 + i)
            args.append(s32(v) if k == "i" else v & 0xFF)
        L.rec(name, tuple(args))
        e.set_r(3, L.ret(name) & MASK)
    return fn


def stub_time(e):
    L = e.cl
    L.rec("GetFunnyTimeSinceSessionStart", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">q", L.ret("Time")))


STUBS.internal(0x1013EBC8, getter("GetParkAnalyser", ANALYSER))
STUBS.internal(0x1013ECE8, getter("GetResearchLab", LAB))
STUBS.internal(0x1013EDFC, getter("GetBankAccount", BANK))
STUBS.internal(0x100E9B8C, counter("TypesOfShopsAvailable", "b"))
STUBS.internal(0x100E9C80, counter("TypesOfSideshowsAvailable", "b"))
STUBS.internal(0x100E9A94, counter("TypesOfUpgradesAvailable", "b"))
STUBS.internal(0x100E9998, counter("TypesOfRidesAvailable", "b"))
STUBS.internal(0x100E9E70, counter("TypesOfRidesLeftToResearch", ""))
STUBS.internal(0x100E9EEC, counter("TypesOfShopsLeftToResearch", ""))
STUBS.internal(0x100E7098, counter("GetDaysSinceLastObjectBuilt", ""))
STUBS.internal(0x100EA5D8, counter("NumberOfObjectTypeInPark", "ii"))
STUBS.internal(0x100E8144, counter("NumberOfStaffType", "i"))
STUBS.internal(0x100EA298, counter("TypesOfParticularObjectTypeInPark", "ib"))
STUBS.internal(0x10122A04, counter("GetResearchSpeed", "i"))
STUBS.internal(0x100F32B4, counter("GetBalance", ""))
STUBS.internal(0x10112B14, stub_time)


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
        self.L = e.cl = CL.PpcLog(dict(LABELS), case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def write(self, a, data):
        self.e.write(a, data)

    def setptr(self, a, v):
        self.words(a, [v])

    def setslot(self, obj, off, w):
        self.words(obj + off, [w])

    def size(self):
        return 0x640

    def log(self):
        return self.L.log

    def call(self, idx, obj):
        addr = FUNCS[idx][1]
        r3, _, _ = T.call_ppc(self.e, addr, [obj & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {v: k for k, v in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_adv_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_adv_alloc(n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def setslot(self, obj, off, w):
        self.write(obj + self.lib.base + 8 * ((off - 0x14) // 4), struct.pack("<I", w & MASK))

    def size(self):
        return self.lib.tpw_adv_size()

    def log(self):
        return CL.host_end(self.lib)

    def call(self, idx, obj):
        return self.lib.tpw_adv_call(idx, obj) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_adv_addr.restype, lib.tpw_adv_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_adv_alloc.restype, lib.tpw_adv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_adv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_adv_size.restype = C.c_int32
    lib.tpw_adv_call.restype, lib.tpw_adv_call.argtypes = C.c_int32, [C.c_int32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_adv_layout(out)
    assert n == 2 and out[1] == 8, list(out)[:n]
    lib.base = out[0]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.setptr(s.g("world"), WORLD)
    obj = s.alloc(s.size())
    for off, w in case["bal"].items():
        s.setslot(obj, off, w)
    return obj


def run(s, case):
    try:
        obj = build(s, case)
        return {"answer": s.call(case["idx"], obj), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SMALL = [0, 0, 1, 1, 2, 3, 5, 8, 100, -1]
TIMES = [0, 1, -1, 863999999999, 864000000000, 864000000001, -864000000000, -863999999999, 5 * 864000000000, 100 * 864000000000,
         25919999999999, 25920000000000, 25920000000001, 3 * 25920000000000, -25920000000000, 2 ** 40 + 12345, 2 ** 62, -(2 ** 62),
         864000000000 * (2 ** 23), 864000000000 * (2 ** 23) - 864000000000 * 7, 25920000000000 * (2 ** 18 + 3),
         -864000000000 * (2 ** 23)]
FLOATS = [0.0, -0.0, 0.5, 1.0, 1.5, 2.0, 3.0, -1.0, float("nan"), float("inf"), 0.25, 1.0000001]


def base(r, idx):
    q = {k: [r.choice(SMALL)] for k in QUEUE_KEYS}
    q["GetDaysSinceLastObjectBuilt"] = [r.choice([-1, 0, 1, 2, 3, 10])]
    q["GetResearchSpeed"] = [r.choice([0, 0, 0, 1, 5])]
    q["Time"] = [r.choice(TIMES) if r.random() < 0.8 else r.randrange(-(2 ** 63), 2 ** 63)]
    q["GetBalance"] = [r.choice([-1, 0, 1, 2, 3, 100, 2 ** 31 - 1, -(2 ** 31)])]
    bal = {}
    for off in OFFS:
        if off in (0x258, 0x27C):
            bal[off] = r.choice([0, 1, 33, 34, 50, 51, 100, 101, 200, 201, 300, 0x80000000, 0xFFFFFFFF]) & MASK
        elif off == 0x270:
            f = r.choice(FLOATS) if r.random() < 0.9 else r.uniform(-4, 4)
            bal[off] = fb(f)
        else:
            bal[off] = r.choice(SMALL + [r.randrange(-3, 8), r.randrange(0, 2 ** 32)]) & MASK
    return {"idx": idx, "queues": q, "bal": bal}


def edges_for(idx):
    def gen():
        r = random.Random(1000 + idx)
        for _ in range(400):
            yield base(r, idx)
    return gen


def random_for(idx):
    return lambda r: base(r, idx)


TESTS = [FnTest(name, ppc, host, edge=edges_for(idx), rnd=random_for(idx)) for name, _, _, idx in FUNCS]
