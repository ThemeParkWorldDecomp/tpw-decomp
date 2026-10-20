"""Unit v5081, six advisor scorers, against libtpw_unit_CAdvisorScoring__ADVISOR_FEATURES_DIRTY_TOILETS_.dylib
(tests/unit_CAdvisorScoring__ADVISOR_FEATURES_DIRTY_TOILETS_.cpp), main app, src/AdvisorScoring_CAdvisorScoring__ADVISOR_FEATURES_DIRTY_TOILETS_.cpp:
  _ADVISOR_FEATURES_DIRTY_TOILETS_1 0x10015628, _2 0x10015798, _3 0x10015908, _4 0x10015a64,
  _ADVISOR_FEATURES_POOR_SELECTION 0x10015018, _ADVISOR_SIDESHOWS_BUILD_MORE_SIDESHOWS 0x10014ea0

A case is a scoring object and a call. The object's balance values are written by their Mac offset (the host's slots are 8 bytes, the
Mac's 4, so the host offset is computed from the layout); the world pointer, the four toilet ids and the callees' answers come from the case.
The callees (world, analyser, bank account) are fakes on both sides that record their arguments and answer from queues.

What is compared: the answer, mWord4, the other words of the header, the event log. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_FEATURES_DIRTY_TOILETS_"
MASK = 0xFFFFFFFF

FNS = {1: 0x10015628, 2: 0x10015798, 3: 0x10015908, 4: 0x10015a64, 5: 0x10015018, 6: 0x10014ea0}
CALLEES = {
    "IsParkClosed": 0x1013ff7c, "GetParkAnalyser": 0x1013ebc8, "NumberOfStaffType": 0x100e8144,
    "FilthiestToilet": 0x100e9580, "AvgFilthLevelOfToilets": 0x100e9420, "TypesOfFeaturesAvailable": 0x100e9d78,
    "GetDaysSinceLastObjectBuilt": 0x100e7098, "NumberOfObjectTypeInPark": 0x100ea5d8,
    "TypesOfParticularObjectTypeInPark": 0x100ea298, "GetBankAccount": 0x1013edfc, "GetBalance": 0x100f32b4,
}
WORLD, ANALYSER, BANK = 0x30000100, 0x30000200, 0x30000300   # unmapped, never read: only labels
ADDR = dict(id1=0x1027defc, id2=0x1027defe, id3=0x1027df00, id4=0x1027df02)
GLOBAL_REF = 0x102a460c
OFFS = [0x2d0, 0x2d4, 0x2d8, 0x2e0, 0x2e4, 0x2e8, 0x2ec, 0x2a0, 0x2a4, 0x2a8, 0x294, 0x298, 0x29c, 0x634]
HOST = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()


def mk(name, spec):
    def stub(e):
        L = e.cl
        if name == "FilthiestToilet":
            L.rec(name, (L.lab(e.r(4)),))
            e.write(e.r(3), struct.pack(">H", L.ret("FilthId") & 0xFFFF))
            e.write(e.r(5), struct.pack(">I", L.ret("Filth") & MASK))
            return
        if name == "GetParkAnalyser":
            L.rec(name, (L.lab(e.r(3)),))
            e.set_r(3, ANALYSER)
            return
        if name == "GetBankAccount":
            L.rec(name, (L.lab(e.r(3)),))
            e.set_r(3, BANK)
            return
        args = [L.lab(e.r(3))]
        if name == "NumberOfStaffType":
            args.append(s32(e.r(4)))
        elif name == "TypesOfFeaturesAvailable":
            args.append(e.r(4) & 0xFF)
        elif name == "NumberOfObjectTypeInPark":
            args += [s32(e.r(4)), s32(e.r(5))]
        elif name == "TypesOfParticularObjectTypeInPark":
            args += [s32(e.r(4)), e.r(5) & 0xFF]
        L.rec(name, tuple(args))
        key = name
        if name == "TypesOfParticularObjectTypeInPark":
            key = "%s:%d" % (name, s32(e.r(4)))
        e.set_r(3, L.ret(key) & MASK)
    return stub


for _n, _a in CALLEES.items():
    STUBS.internal(_a, mk(_n, ""))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def wr16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd16(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({WORLD: 1, ANALYSER: 2, BANK: 3}, case["queues"])
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

    def put(self, obj, off, word):
        self.words(obj + off, [word])

    def world(self):
        self.words(self.e.u32(GLOBAL_REF), [WORLD])

    def log(self):
        return self.L.log

    def call(self, which, obj):
        r3, _, _ = T.call_ppc(self.e, FNS[which], [obj])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("world"), 2: self.g("analyser"), 3: self.g("bank")}, case["queues"])

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

    def put(self, obj, off, word):
        self.words(obj + HOST["values"] + 8 * ((off - 0x14) // 4), [word])

    def world(self):
        pass

    def log(self):
        return CL.host_end(self.lib)

    def call(self, which, obj):
        return self.lib.tpw_adv_call(which, obj) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_adv_addr.restype, lib.tpw_adv_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_adv_alloc.restype, lib.tpw_adv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_adv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_adv_call.restype, lib.tpw_adv_call.argtypes = C.c_uint32, [C.c_int32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_adv_layout(out)
    values, word4, slot, size = list(out)[:n]
    assert word4 == 4 and slot == 8, list(out)[:n]
    HOST.update(values=values, size=size)
    lib.bound = True


def build(s, case):
    s.world()
    for k in ("id1", "id2", "id3", "id4"):
        s.wr16(s.g(k), case[k])
    obj = s.alloc(0x700 if isinstance(s, Ppc) else HOST["size"] + 16)
    s.words(obj, [case["w0"]])
    s.wr16(obj + 4, case["w4"])
    s.words(obj + 8, [case["w8"]])
    for off, w in case["vals"].items():
        s.put(obj, off, w)
    return obj


def run(s, case):
    try:
        obj = build(s, case)
        answer = s.call(case["which"], obj)
        return {"answer": answer, "log": s.log(), "w0": s.rd_word(obj), "w4": s.rd16(obj + 4), "w8": s.rd_word(obj + 8)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def small(r):
    return r.choice([0, 1, 2, 3, 4, 5, 6, 7, -1, -2, 100, 0x7FFFFFFF, 0x80000000])


def base(r, which):
    q = {
        "IsParkClosed": [1 if r.random() < 0.15 else 0],
        "NumberOfStaffType": [r.choice([-1, 0, 0, 1, 2])],
        "FilthId": [r.choice([0, 1, 2, 0xFFFF, 0x1234])],
        "Filth": [r.randrange(0, 100)],
        "AvgFilthLevelOfToilets": [r.randrange(-2, 9)],
        "TypesOfFeaturesAvailable": [r.choice([0, 1, 2, 3, 5, 7, 0x80000000, 0xFFFFFFFF])],
        "GetDaysSinceLastObjectBuilt": [r.randrange(-2, 8)],
        "NumberOfObjectTypeInPark": [r.randrange(-2, 8)],
        "TypesOfParticularObjectTypeInPark:0": [r.choice([0, 1, 2, 3, 4, 5, -1, 0x7FFFFFFF])],
        "TypesOfParticularObjectTypeInPark:2": [r.choice([0, 1, 2, 3, 4, 5, -1, 0x7FFFFFFF])],
        "TypesOfParticularObjectTypeInPark:3": [r.choice([0, 1, 2, 3, 4, 5, 100, 0x7FFFFFFF, 0x80000000])],
        "GetBalance": [r.randrange(-3, 8)],
    }
    q = {k: [v & MASK if isinstance(v, int) and v < 0 else v for v in vs] for k, vs in q.items()}
    vals = {}
    for off in OFFS:
        vals[off] = r.randrange(-1, 9) & MASK
    if r.random() < 0.3:
        vals[0x2a0] = r.choice([0, 1, 33, 50, 100, 101, 200, 0xFFFFFFFF, 0x80000000])
    vals[0x294] = r.choice([fb(x) for x in (0.0, 0.5, 1.0, 1.5, 2.0, 3.0, -1.0, float("nan"))])
    ids = [r.choice([0, 1, 2, 0xFFFF, 0x1234]) for _ in range(4)]
    return {"which": which, "queues": q, "vals": vals, "id1": ids[0], "id2": ids[1], "id3": ids[2], "id4": ids[3],
            "w0": r.getrandbits(32), "w4": r.getrandbits(16), "w8": r.getrandbits(32)}


def gen(which):
    def edges():
        r = random.Random(100 + which)
        for _ in range(60):
            yield base(r, which)

    def rnd(r):
        return base(r, which)
    return edges, rnd


NAMES = ["_ADVISOR_FEATURES_DIRTY_TOILETS_1", "_ADVISOR_FEATURES_DIRTY_TOILETS_2", "_ADVISOR_FEATURES_DIRTY_TOILETS_3",
         "_ADVISOR_FEATURES_DIRTY_TOILETS_4", "_ADVISOR_FEATURES_POOR_SELECTION", "_ADVISOR_SIDESHOWS_BUILD_MORE_SIDESHOWS"]
TESTS = []
for _i, _n in enumerate(NAMES, 1):
    _e, _r = gen(_i)
    TESTS.append(FnTest("CAdvisorScoring::" + _n, ppc, host, edge=_e, rnd=_r))
