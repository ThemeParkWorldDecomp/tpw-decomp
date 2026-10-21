"""Unit v5118, the advisor's staff scorers, against libtpw_unit_CAdvisorScoring__ADVISOR_STAFF_ENTERTAINER_COVER.dylib
(tests/unit_CAdvisorScoring__ADVISOR_STAFF_ENTERTAINER_COVER.cpp), main app ("SimTheme Park"),
src/game/advisor/AdvisorScoring.cpp:
  _ADVISOR_STAFF_GUARD_BUILD_CAMERA 0x10012b0c, _ADVISOR_STAFF_SET_GUARD_AREA 0x10012bec, _ADVISOR_STAFF_SET_ENTERTAINER_AREA 0x10012dd4,
  _ADVISOR_STAFF_ENTERTAINER_COVERAGE 0x10012f18, _ADVISOR_STAFF_TRAIN_MECHANICS 0x10013044, _ADVISOR_STAFF_TRAIN_HANDYMEN 0x10013144,
  _ADVISOR_STAFF_TRAIN_GUARDS 0x10013244, _ADVISOR_STAFF_TRAIN_ENTERTAINERS 0x10013340

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the world's
GetParkAnalyser and GetStaffHQ, the analyser's staff queries, the park time's GetFunnyTimeSinceSessionStart) are fakes on both sides that
record their arguments and answer from the case's queues. The runtime's 64-bit divide (0x10237e1c) runs as the original on the PowerPC side.

The world of a case: gGameWorld (the world is a small block), the scoring object's balance values (written by their Mac offsets; the host's
slots are 8 bytes, so the host offset is computed), the staff headquarters' training words, the fakes' answers. What is compared: the answer
and the event log. Nothing else is written by the functions.

No fused operations. Not tested: a staff count of 0 reaching the divide of the SET_*_AREA functions (divw by 0 is undefined on the hardware).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_STAFF_ENTERTAINER_COVER"
MASK = 0xFFFFFFFF

# name, address, size
FUNCS = [
    ("CAdvisorScoring::_ADVISOR_STAFF_ENTERTAINER_COVERAGE", 0x10012F18, 224, "_ADVISOR_STAFF_ENTERTAINER_COVERAGE__15CAdvisorScoringFv"),
    ("CAdvisorScoring::_ADVISOR_STAFF_TRAIN_MECHANICS", 0x10013044, 184, "_ADVISOR_STAFF_TRAIN_MECHANICS__15CAdvisorScoringFv"),
    ("CAdvisorScoring::_ADVISOR_STAFF_SET_ENTERTAINER_AREA", 0x10012DD4, 248, "_ADVISOR_STAFF_SET_ENTERTAINER_AREA__15CAdvisorScoringFv"),
    ("CAdvisorScoring::_ADVISOR_STAFF_TRAIN_HANDYMEN", 0x10013144, 184, "_ADVISOR_STAFF_TRAIN_HANDYMEN__15CAdvisorScoringFv"),
    ("CAdvisorScoring::_ADVISOR_STAFF_SET_GUARD_AREA", 0x10012BEC, 248, "_ADVISOR_STAFF_SET_GUARD_AREA__15CAdvisorScoringFv"),
    ("CAdvisorScoring::_ADVISOR_STAFF_TRAIN_GUARDS", 0x10013244, 184, "_ADVISOR_STAFF_TRAIN_GUARDS__15CAdvisorScoringFv"),
    ("CAdvisorScoring::_ADVISOR_STAFF_GUARD_BUILD_CAMERA", 0x10012B0C, 148, "_ADVISOR_STAFF_GUARD_BUILD_CAMERA__15CAdvisorScoringFv"),
    ("CAdvisorScoring::_ADVISOR_STAFF_TRAIN_ENTERTAINERS", 0x10013340, 184, "_ADVISOR_STAFF_TRAIN_ENTERTAINERS__15CAdvisorScoringFv"),
]
OWN = {a for _, a, _, _ in FUNCS}

GET_ANALYSER, NUM_TYPE, NUM_AREA, PCT, AVG, MISB, GET_HQ, FUNNY = (
    0x1013EBC8, 0x100E8144, 0x100E86A4, 0x100EE924, 0x100E88A0, 0x100E7930, 0x1013E99C, 0x10112B14)

ADDR = dict(gameworld=0x1037EBFC, ref=0x102A460C)
DIV = 0x1792F8648000
SLOT_OFFS = list(range(0x15C, 0x1B4, 4))

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def _an(e, name, spec_args):
    L = e.cl
    L.rec(name, spec_args)
    e.set_r(3, L.ret(name) & MASK)


STUBS.internal(GET_ANALYSER, lambda e: (e.cl.rec("GetParkAnalyser", (e.cl.lab(e.r(3)),)), e.set_r(3, e.cl.an)))
STUBS.internal(GET_HQ, lambda e: (e.cl.rec("GetStaffHQ", (e.cl.lab(e.r(3)),)), e.set_r(3, e.cl.hq)))
STUBS.internal(NUM_TYPE, lambda e: _an(e, "NumberOfStaffType", (e.cl.lab(e.r(3)), T.s32(e.r(4)))))
STUBS.internal(NUM_AREA, lambda e: _an(e, "NumberOfStaffTypeWithPatrolArea", (e.cl.lab(e.r(3)), T.s32(e.r(4)))))
STUBS.internal(PCT, lambda e: _an(e, "PercentageOfAreaPatrolledBy", (e.cl.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)))))
STUBS.internal(AVG, lambda e: _an(e, "AverageQualityOfStaffType", (e.cl.lab(e.r(3)), T.s32(e.r(4)))))
STUBS.internal(MISB, lambda e: _an(e, "MisbehavingVisitorsLastMonth", (e.cl.lab(e.r(3)), e.r(4) & 0xFF)))


def stub_funny(e):
    """CParkTime::GetFunnyTimeSinceSessionStart() const: the 8 byte result goes through the hidden pointer in r3, this is r4."""
    L = e.cl
    L.rec("FunnyTime", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">II", L.ret("ticksHi") & MASK, L.ret("ticksLo") & MASK))


STUBS.internal(FUNNY, stub_funny)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.world = e.alloc(0x40)
        self.an = e.alloc(0x40)
        self.hq = e.alloc(0x80)
        self.this = e.alloc(0x400)
        self.L = e.cl = CL.PpcLog({self.world: 1, self.world + 0x2A0: 2, self.an: 3, self.hq: 4}, case["queues"])
        self.L.an, self.L.hq = self.an, self.hq
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def slot(self, off, v):
        self.words(self.this + off, [v])

    def trainword(self, i, v):
        self.words(self.hq + 0xC + 4 * i, [v])

    def setworld(self):
        self.words(ADDR["ref"], [ADDR["gameworld"]])
        self.words(ADDR["gameworld"], [self.world])

    def call(self, op):
        r3, _, _ = T.call_ppc(self.e, FUNCS[op][1], [self.this])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.world = lib.tpw_stf_alloc(0x40)
        self.an = lib.tpw_stf_alloc(0x40)
        self.hq = lib.tpw_stf_alloc(0x400)
        self.this = lib.tpw_stf_alloc(0x1000)
        lib.tpw_stf_setptrs(self.an, self.hq)
        CL.host_begin(lib, {1: self.world, 2: self.world + 0x2A0, 3: self.an, 4: self.hq}, case["queues"])

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def slot(self, off, v):
        self.words(self.this + self.lib.layout[0] + 2 * (off - 0x14), [v])

    def trainword(self, i, v):
        self.words(self.hq + self.lib.layout[1] + 4 * i, [v])

    def setworld(self):
        self.write(self.lib.tpw_stf_addr(b"gameworld"), struct.pack("<Q", self.world))

    def call(self, op):
        return getattr(self.lib, "tpw_stf_%d" % op)(self.this) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_stf_addr.restype, lib.tpw_stf_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_stf_alloc.restype, lib.tpw_stf_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_stf_setptrs.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_stf_layout.argtypes = [C.POINTER(C.c_int64)]
    for i in range(8):
        f = getattr(lib, "tpw_stf_%d" % i)
        f.restype, f.argtypes = C.c_int32, [C.c_uint64]
    out = (C.c_int64 * 8)()
    n = lib.tpw_stf_layout(out)
    lib.layout = list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.setworld()
        for off, v in case["slots"].items():
            s.slot(off, v)
        for i, v in enumerate(case["train"]):
            s.trainword(i, v)
        return {"answer": T.s32(s.call(case["op"])), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk_case(r, op):
    slots = {}
    for off in SLOT_OFFS:
        slots[off] = r.choice([r.randrange(-3, 8), r.randrange(0, 130), r.randrange(-300, 300)])
    for off in (0x170, 0x188):
        slots[off] = r.randrange(-2, 4)
    for off in (0x164, 0x17C):
        slots[off] = r.randrange(-1, 7)
    area = op in (2, 4)
    count = r.randrange(1, 8) if area else r.choice([0, 1, 2, 5])
    k = r.randrange(-2, 4)
    t = k * DIV + r.choice([0, DIV - 1, 1, r.randrange(DIV)])
    t &= (1 << 64) - 1
    q = {
        "NumberOfStaffType": [count],
        "NumberOfStaffTypeWithPatrolArea": [r.randrange(0, 9)],
        "PercentageOfAreaPatrolledBy": [r.choice([r.randrange(0, 101), r.randrange(-5, 140)])],
        "AverageQualityOfStaffType": [r.choice([r.randrange(0, 101), r.randrange(-5, 140)])],
        "MisbehavingVisitorsLastMonth": [r.randrange(-3, 130)],
        "ticksHi": [t >> 32],
        "ticksLo": [t & MASK],
    }
    train = [r.choice([0, 0, 1, -1, 5, r.randrange(-3, 4)]) for _ in range(5)]
    return {"op": op, "queues": q, "slots": slots, "train": train}


def make_test(op):
    def rnd(r):
        return mk_case(r, op)

    def edges():
        r = random.Random(100 + op)
        for _ in range(400):
            yield mk_case(r, op)

    return FnTest(FUNCS[op][0], ppc, host, edge=edges, rnd=rnd)


TESTS = [make_test(op) for op in range(8)]
