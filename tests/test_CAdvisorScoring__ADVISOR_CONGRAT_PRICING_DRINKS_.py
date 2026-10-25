"""Unit v5122, the eight CONGRAT_PRICING great value / great satisfaction scorers, against
libtpw_unit_CAdvisorScoring__ADVISOR_CONGRAT_PRICING_DRINKS_.dylib (tests/unit_CAdvisorScoring__ADVISOR_CONGRAT_PRICING_DRINKS_.cpp), main app
("SimTheme Park"), src/game/advisor/AdvisorScoring.cpp:
  DRINKS_GREAT_VALUE 0x1001a010, SIDESHOWS_GREAT_VALUE 0x1001a104, RESTAURANT_GREAT_VALUE 0x10019f18, SHOPS_GREAT_VALUE 0x10019e28,
  FOOD_GREAT_SATISFACTION 0x1001a1f8, FOOD_GREAT_VALUE 0x10019d34, SHOPS_GREAT_SATISFACTION 0x1001a2f0, RESTAURANT_GREAT_SATISFACTION 0x1001a3e8

A case is a scoring object (limit and multiplier balance values, halfword 4 and word 8), the function's "none" id global and the answers of the
analyser fake (id, figure). The world getter and the analyser calls are fakes on both sides that record their arguments (the shop type too).
The PowerPC object is built at the Mac's offsets; the host's balance slots are passed to the wrapper (slot = (offset - 0x14) / 4).
Compared: the answer, halfword 4, word 8 and the event log. No fused operations. Not tested: nothing skipped.
"""
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_CONGRAT_PRICING_DRINKS_"
import ctypes as C

MASK = 0xFFFFFFFF
ANALYSER = 0x70001000
WORLD = 0x70002000
GETPA = 0x1013ebc8
CALLEES = {0x100eaa24: ("BestValueT", True), 0x100eb294: ("BestValue", False), 0x100eaed4: ("BestSatT", True),
           0x100eb778: ("BestSat", False), 0x100ebb68: ("SideshowValue", False)}
GCELL = 0x102a460c

# idx: (name, address, size, limit offset, none global address)
FNS = [
    ("_ADVISOR_CONGRAT_PRICING_DRINKS_GREAT_VALUE", 0x1001a010, 160, 0x540, 0x1027df20),
    ("_ADVISOR_CONGRAT_PRICING_SIDESHOWS_GREAT_VALUE", 0x1001a104, 156, 0x548, 0x1027df22),
    ("_ADVISOR_CONGRAT_PRICING_RESTAURANT_GREAT_VALUE", 0x10019f18, 160, 0x538, 0x1027df1e),
    ("_ADVISOR_CONGRAT_PRICING_SHOPS_GREAT_VALUE", 0x10019e28, 156, 0x530, 0x1027df1c),
    ("_ADVISOR_CONGRAT_PRICING_FOOD_GREAT_SATISFACTION", 0x1001a1f8, 160, 0x550, 0x1027df24),
    ("_ADVISOR_CONGRAT_PRICING_FOOD_GREAT_VALUE", 0x10019d34, 160, 0x528, 0x1027df1a),
    ("_ADVISOR_CONGRAT_PRICING_SHOPS_GREAT_SATISFACTION", 0x1001a2f0, 156, 0x558, 0x1027df26),
    ("_ADVISOR_CONGRAT_PRICING_RESTAURANT_GREAT_SATISFACTION", 0x1001a3e8, 160, 0x560, 0x1027df28),
]

STUBS = CL.Stubs()
STUBS.real.update(f[1] for f in FNS)


def stub_getpa(e):
    L = e.cl
    L.rec("GetParkAnalyser", (L.lab(e.r(3)),))
    e.set_r(3, ANALYSER)


STUBS.internal(GETPA, stub_getpa)


def make_callee(name, typed):
    def stub(e):
        L = e.cl
        if typed:
            L.rec(name, (L.lab(e.r(3)), T.s32(e.r(4))))
            idp = e.r(5)
        else:
            L.rec(name, (L.lab(e.r(3)),))
            idp = e.r(4)
        e.write(idp, struct.pack(">H", L.ret("id") & 0xFFFF))
        e.set_r(3, L.ret("val") & MASK)
    return stub


for _a, (_n, _t) in CALLEES.items():
    STUBS.internal(_a, make_callee(_n, _t))


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cps_addr.restype, lib.tpw_cps_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cps_call.restype = C.c_uint32
    lib.tpw_cps_call.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)]
    lib.bound = True


def ppc(e, case):
    idx = case["idx"]
    name, addr, size, off, gaddr = FNS[idx]
    e.cl = CL.PpcLog({ANALYSER: 1, WORLD: 2}, case["queues"])
    saved = STUBS.activate(e)
    try:
        cell = e.alloc(4)
        e.write(cell, struct.pack(">I", WORLD))
        e.write(GCELL, struct.pack(">I", cell))
        e.write(gaddr, struct.pack(">H", case["none"]))
        obj = e.alloc(0x600)
        e.write(obj + 4, struct.pack(">H", case["w4"]))
        e.write(obj + 8, struct.pack(">I", case["w8"]))
        e.write(obj + off, struct.pack(">II", case["limit"] & MASK, case["mult"] & MASK))
        r3, _, _ = T.call_ppc(e, addr, [obj])
        w4 = struct.unpack(">H", e.read(obj + 4, 2))[0]
        w8 = struct.unpack(">I", e.read(obj + 8, 4))[0]
        return {"answer": r3 & MASK, "w4": w4, "w8": w8, "log": e.cl.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    bind(lib)
    idx = case["idx"]
    off = FNS[idx][3]
    CL.host_begin(lib, {1: lib.tpw_cps_addr(b"analyser"), 2: lib.tpw_cps_addr(b"world")}, case["queues"])
    g = lib.tpw_cps_addr(b"g%d" % idx)
    C.memmove(g, struct.pack("<H", case["none"]), 2)
    w4, w8 = C.c_uint32(case["w4"]), C.c_uint32(case["w8"])
    slot = (off - 0x14) // 4
    r = lib.tpw_cps_call(idx, slot, T.s32(case["limit"]), T.s32(case["mult"]), C.byref(w4), C.byref(w8))
    return {"answer": r & MASK, "w4": w4.value & 0xFFFF, "w8": w8.value & MASK, "log": CL.host_end(lib)}


def make_case(r, idx, ident=None, val=None, limit=None):
    none = r.randrange(0x10000) if r.random() < 0.5 else r.choice([0, 0xFFFF, 1])
    if ident is None:
        ident = none if r.random() < 0.3 else r.randrange(0x10000)
    elif ident == "none":
        ident = none
    elif ident == "other":
        ident = (none + 1) & 0xFFFF
    if limit is None:
        limit = r.choice([0, -1, 5, r.randrange(-1000, 1000), r.randrange(-2**31, 2**31)])
    if val is None:
        val = r.choice([limit, limit + 1, limit - 1, 0, -1, r.randrange(-1000, 1000), r.randrange(-2**31, 2**31)])
    return {"idx": idx, "none": none, "w4": r.randrange(0x10000), "w8": r.randrange(1 << 32), "limit": limit,
            "mult": r.choice([0, 1, -1, r.randrange(-100, 100), r.randrange(-2**31, 2**31)]),
            "queues": {"id": [ident], "val": [val & MASK]}}


def make_test(idx):
    import random

    def edges():
        r = random.Random(100 + idx)
        for ident in ("none", "other"):
            for lim, val in ((10, 11), (10, 10), (10, 9), (-5, -4), (0x7FFFFFFF, 0x7FFFFFFF), (-2**31, 0x7FFFFFFF), (0x7FFFFFFF, -2**31), (3, 0)):
                yield make_case(r, idx, ident, val, lim)

    def rnd(r):
        return make_case(r, idx)

    return FnTest("CAdvisorScoring::" + FNS[idx][0], ppc, host, edge=edges, rnd=rnd)


TESTS = [make_test(i) for i in range(len(FNS))]
