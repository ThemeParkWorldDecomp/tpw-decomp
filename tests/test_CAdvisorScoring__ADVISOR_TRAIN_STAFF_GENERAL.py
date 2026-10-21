"""Unit v5282, eight advisor scorers, against libtpw_unit_CAdvisorScoring__ADVISOR_TRAIN_STAFF_GENERAL.dylib
(tests/unit_CAdvisorScoring__ADVISOR_TRAIN_STAFF_GENERAL.cpp), main app ("SimTheme Park"),
src/game/advisor/AdvisorScoring.cpp:
  _ADVISOR_TRAIN_STAFF_GENERAL 0x10018f4c, _ADVISOR_SET_PATROLS_GENERAL 0x10018e80, _ADVISOR_HIRE_STAFF_GENERAL 0x10018d90,
  _ADVISOR_NEED_STAFF_ROOM 0x10018c78, _ADVISOR_PRICING_FOOD_POOR_VALUE 0x100192a4, _ADVISOR_PRICING_SHOPS_POOR_VALUE 0x100193ac,
  _ADVISOR_PRICING_RESTAURANT_POOR_VALUE 0x100194b4, _ADVISOR_CONGRAT_COSTUMES 0x100188f4

A case is a world and a call. The scoring object, the world, the analyser and the staff headquarters are built twice by ONE piece of Python
code (emulator memory big-endian, host memory little-endian). The callees (world getters, analyser counts, GetShopWithWorstValue, the park
time, CTRACE) are fakes on both sides that record their arguments and answer from the case's queues.

The world of a case: the balance values the scorers read (Mac offsets 0x4a8..0x4ec of the object), the three "no thing" ids, the
headquarters' flag byte at +0x20, the object's words at +4 and +8, and the queues. Compared: the answer, the call log, the object's
halfword at +4 and word at +8. No fused operations. Not tested: divw of INT_MIN by -1 (undefined on the hardware).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_TRAIN_STAFF_GENERAL"
MASK = 0xFFFFFFFF

FN = {
    "_ADVISOR_TRAIN_STAFF_GENERAL": (0x10018F4C, 128),
    "_ADVISOR_SET_PATROLS_GENERAL": (0x10018E80, 136),
    "_ADVISOR_HIRE_STAFF_GENERAL": (0x10018D90, 172),
    "_ADVISOR_NEED_STAFF_ROOM": (0x10018C78, 216),
    "_ADVISOR_PRICING_FOOD_POOR_VALUE": (0x100192A4, 192),
    "_ADVISOR_PRICING_SHOPS_POOR_VALUE": (0x100193AC, 188),
    "_ADVISOR_PRICING_RESTAURANT_POOR_VALUE": (0x100194B4, 192),
    "_ADVISOR_CONGRAT_COSTUMES": (0x100188F4, 92),
}
OWN = set(a for a, _ in FN.values())

GET_ANALYSER = 0x1013EBC8
GET_HQ = 0x1013E99C
IS_CLOSED = 0x1013FF7C
NUM_STAFF = 0x100E8324
NUM_TIRED = 0x100E84D4
NUM_PATROL = 0x100E86A4
NUM_VISITORS = 0x100E7994
NUM_COSTUMES = 0x100E7BD8
WORST_T = 0x100EAC7C
WORST = 0x100EB504
FUNNY = 0x10112B14
CTRACE = 0x1006FCBC

GAME_WORLD_REF = 0x102A460C
ADDR = dict(none_food=0x1027DF06, none_shops=0x1027DF08, none_rest=0x1027DF0A)
HOST_LAYOUT = [4, 8, 0x2A0, 0x20]
BAL_OFFS = list(range(0x4A8, 0x4F0, 4))
DIV = 0x1792F8648000

CUR = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lab_stub(name, key, ret=True):
    def fn(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)),))
        e.set_r(3, (L.ret(key) if ret else 0) & MASK)
    return fn


def stub_get_analyser(e):
    e.cl.rec("GetParkAnalyser", (e.cl.lab(e.r(3)),))
    e.set_r(3, CUR["an"])


def stub_get_hq(e):
    e.cl.rec("GetStaffHQ", (e.cl.lab(e.r(3)),))
    e.set_r(3, CUR["hq"])


def stub_worst_t(e):
    L = e.cl
    L.rec("WorstT", (L.lab(e.r(3)), s32(e.r(4))))
    e.write(e.r(5), struct.pack(">H", L.ret("id") & 0xFFFF))
    e.set_r(3, L.ret("WorstT") & MASK)


def stub_worst(e):
    L = e.cl
    L.rec("Worst", (L.lab(e.r(3)),))
    e.write(e.r(4), struct.pack(">H", L.ret("id") & 0xFFFF))
    e.set_r(3, L.ret("Worst") & MASK)


def stub_funny(e):
    L = e.cl
    L.rec("Funny", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">II", L.ret("FunnyHi") & MASK, L.ret("FunnyLo") & MASK))


STUBS.internal(GET_ANALYSER, stub_get_analyser)
STUBS.internal(GET_HQ, stub_get_hq)
STUBS.internal(IS_CLOSED, lab_stub("IsParkClosed", "IsParkClosed"))
STUBS.internal(NUM_STAFF, lab_stub("NumberOfStaff", "NumberOfStaff"))
STUBS.internal(NUM_TIRED, lab_stub("Tired", "Tired"))
STUBS.internal(NUM_VISITORS, lab_stub("Visitors", "Visitors"))
STUBS.internal(NUM_COSTUMES, lab_stub("Costumes", "Costumes"))


def stub_patrol(e):
    L = e.cl
    L.rec("Patrol", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("Patrol") & MASK)


STUBS.internal(NUM_PATROL, stub_patrol)
STUBS.internal(WORST_T, stub_worst_t)
STUBS.internal(WORST, stub_worst)
STUBS.internal(FUNNY, stub_funny)
STUBS.internal(CTRACE, CL.recorder("CTRACE", "usi"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def wr_h(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_h(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(0x700)
        self.world = e.alloc(0x400)
        self.an = e.alloc(0x40)
        self.hq = e.alloc(0x68)
        CUR.update(an=self.an, hq=self.hq)
        self.L = e.cl = CL.PpcLog({self.world: 1, self.an: 2, self.hq: 3}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def install(self):
        cell = self.e.u32(GAME_WORLD_REF)
        self.words(cell, [self.world])

    def slot(self, off, v):
        self.words(self.obj + off, [v])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op):
        r3, _, _ = T.call_ppc(self.e, FN[op][0], [self.obj])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_adv_alloc(0x2000)
        self.world = lib.tpw_adv_alloc(0x4000)
        self.an = lib.tpw_adv_alloc(0x40)
        self.hq = lib.tpw_adv_alloc(0x68)
        CL.host_begin(lib, {1: self.world, 2: self.an, 3: self.hq}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_adv_addr(name.encode())

    def install(self):
        self.lib.tpw_adv_install(self.world, self.an, self.hq)

    def slot(self, off, v):
        a = self.lib.tpw_adv_slot(self.obj, off)
        self.write(a, struct.pack("<Q", 0))
        self.write(a, struct.pack("<I", v & MASK))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        return getattr(self.lib, "tpw_adv" + op)(self.obj) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_adv_addr.restype, lib.tpw_adv_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_adv_alloc.restype, lib.tpw_adv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_adv_slot.restype, lib.tpw_adv_slot.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_adv_install.restype, lib.tpw_adv_install.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_adv_layout.argtypes = [C.POINTER(C.c_int64)]
    for op in FN:
        f = getattr(lib, "tpw_adv" + op)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_adv_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.install()
    for off, v in case["bal"].items():
        s.slot(off, v)
    for name, v in case["none"].items():
        s.wr_h(s.g(name), v)
    s.write(s.hq + 0x20, bytes([case["hq"]]))
    s.wr_h(s.obj + 4, case["w4"])
    s.words(s.obj + 8, [case["w8"]])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "w4": s.rd_h(s.obj + 4), "w8": s.rd_words(s.obj + 8, 1)}


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


POOL = [-2, -1, 0, 1, 2, 3, 4, 100, 101]
IDS = [0, 1, 2, 0xFFFF]


def pick(r):
    return r.choice(POOL + [r.randrange(-300, 300), r.randrange(-1000000, 1000000)])


def base(r, op):
    months = r.choice([-1, 0, 1, 2, 3, 4, 100])
    extra = r.choice([0, DIV - 1, r.randrange(0, DIV)])
    ticks = (months * DIV + extra) & 0xFFFFFFFFFFFFFFFF
    q = {
        "IsParkClosed": [r.choice([0, 1, 2])],
        "NumberOfStaff": [pick(r)],
        "Tired": [pick(r)],
        "Patrol": [pick(r) if r.random() < 0.5 else 0],
        "Visitors": [pick(r)],
        "Costumes": [pick(r)],
        "WorstT": [pick(r)],
        "Worst": [pick(r)],
        "id": [r.choice(IDS)],
        "FunnyHi": [ticks >> 32],
        "FunnyLo": [ticks & MASK],
    }
    return {
        "op": op,
        "queues": q,
        "bal": {off: pick(r) for off in BAL_OFFS},
        "none": {n: r.choice(IDS) for n in ADDR},
        "hq": r.choice([0, 0, 1, 0x80, 0xFF]),
        "w4": r.randrange(0, 0x10000),
        "w8": r.randrange(0, 1 << 32),
    }


# (queue key, balance offset) the main comparison of each function
PRIM = {
    "_ADVISOR_TRAIN_STAFF_GENERAL": ("NumberOfStaff", 0x4D0),
    "_ADVISOR_SET_PATROLS_GENERAL": ("NumberOfStaff", 0x4C8),
    "_ADVISOR_HIRE_STAFF_GENERAL": ("Visitors", 0x4C4),
    "_ADVISOR_NEED_STAFF_ROOM": ("Tired", 0x4B8),
    "_ADVISOR_PRICING_FOOD_POOR_VALUE": ("WorstT", 0x4D8),
    "_ADVISOR_PRICING_SHOPS_POOR_VALUE": ("Worst", 0x4E0),
    "_ADVISOR_PRICING_RESTAURANT_POOR_VALUE": ("WorstT", 0x4E8),
    "_ADVISOR_CONGRAT_COSTUMES": ("Costumes", 0x4A8),
}
NONE_OF = {
    "_ADVISOR_PRICING_FOOD_POOR_VALUE": "none_food",
    "_ADVISOR_PRICING_SHOPS_POOR_VALUE": "none_shops",
    "_ADVISOR_PRICING_RESTAURANT_POOR_VALUE": "none_rest",
}


def make_edges(op):
    def edges():
        r = random.Random(sum(map(ord, op)))
        key, off = PRIM[op]
        for x in POOL:
            for lim in POOL:
                for aux in range(4):
                    c = base(r, op)
                    c["queues"][key] = [x]
                    c["bal"][off] = lim
                    c["hq"] = aux & 1
                    c["queues"]["IsParkClosed"] = [aux >> 1]
                    c["queues"]["Patrol"] = [0 if aux & 1 else 3]
                    if op == "_ADVISOR_HIRE_STAFF_GENERAL":
                        c["queues"]["NumberOfStaff"] = [0 if aux & 1 else 2]
                    if op == "_ADVISOR_NEED_STAFF_ROOM":
                        c["queues"]["FunnyHi"] = [0]
                        c["queues"]["FunnyLo"] = [DIV * (2 + (aux & 1)) + 5 & MASK] if False else [0]
                        ticks = DIV * (2 + (aux & 1)) + (aux >> 1)
                        c["queues"]["FunnyHi"] = [ticks >> 32]
                        c["queues"]["FunnyLo"] = [ticks & MASK]
                        c["queues"]["NumberOfStaff"] = [r.choice([0, -1, 1, 3, 7, 100])]
                    if op in NONE_OF:
                        c["none"][NONE_OF[op]] = IDS[aux]
                        c["queues"]["id"] = [IDS[(aux + x) % 4]]
                    yield c
        if op == "_ADVISOR_NEED_STAFF_ROOM":
            for mon in (-1, 0, 1, 2, 3, 4):
                for d in (-1, 0, 1):
                    c = base(r, op)
                    ticks = (mon * DIV + d) & 0xFFFFFFFFFFFFFFFF
                    c["queues"]["FunnyHi"] = [ticks >> 32]
                    c["queues"]["FunnyLo"] = [ticks & MASK]
                    yield c
    return edges


def make_rnd(op):
    return lambda r: base(r, op)


TESTS = [FnTest(op, ppc, host, edge=make_edges(op), rnd=make_rnd(op)) for op in FN]
