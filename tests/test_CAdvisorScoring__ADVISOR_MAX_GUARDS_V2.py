"""Unit v5108, the eight CAdvisorScoring::_ADVISOR_MAX_* scorers, against libtpw_unit_CAdvisorScoring__ADVISOR_MAX_GUARDS_V2.dylib
(tests/unit_CAdvisorScoring__ADVISOR_MAX_GUARDS_V2.cpp), main app, src/game/advisor/AdvisorScoring.cpp:
  _ADVISOR_MAX_HANDYMEN_V2 0x10016e68, _ADVISOR_MAX_ENTERTAINERS 0x10016f2c, _ADVISOR_MAX_ENTERTAINERS_V2 0x10016fe8,
  _ADVISOR_MAX_MECHANICS_V2 0x10017168, _ADVISOR_MAX_GUARDS 0x10017230, _ADVISOR_MAX_GUARDS_V2 0x100172e4,
  _ADVISOR_MAX_RESEARCHERS 0x100173a8, _ADVISOR_MAX_RESEARCHERS_V2 0x10017460; unit v5L21: _ADVISOR_MAX_MECHANICS 0x100170b0 and
  _ADVISOR_BOAT_ARRIVED 0x10017ae4 (the table's value at 0x40c; no call)

A case is a world and a call. The world is built by one piece of Python code in both memories. CWorld::GetParkAnalyser and
CParkAnalyser::NumberOfStaffType are fakes on both sides that record their arguments; the count answers from the case's queue.

The world: gGameWorld, the five staff-limit slots of gMainBalance (Mac +0x2a8..+0x2b8) and the scoring object's slots (Mac +0x38c..+0x3c4 and +0x40c),
small values so that the equalities and the thresholds are hit. Compared: the answer and the event log. No fused operations.
Not tested: a NULL gGameWorld (the original reads it unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_MAX_GUARDS_V2"
MASK = 0xFFFFFFFF

# name: (address, size, which, model)
FUNCS = {
    "_ADVISOR_MAX_HANDYMEN_V2": (0x10016E68, 132, 0, 5, "_ADVISOR_MAX_HANDYMEN_V2__15CAdvisorScoringFv"),
    "_ADVISOR_MAX_ENTERTAINERS": (0x10016F2C, 120, 1, 6, "_ADVISOR_MAX_ENTERTAINERS__15CAdvisorScoringFv"),
    "_ADVISOR_MAX_ENTERTAINERS_V2": (0x10016FE8, 132, 2, 6, "_ADVISOR_MAX_ENTERTAINERS_V2__15CAdvisorScoringFv"),
    "_ADVISOR_MAX_MECHANICS_V2": (0x10017168, 132, 3, 4, "_ADVISOR_MAX_MECHANICS_V2__15CAdvisorScoringFv"),
    "_ADVISOR_MAX_GUARDS": (0x10017230, 120, 4, 7, "_ADVISOR_MAX_GUARDS__15CAdvisorScoringFv"),
    "_ADVISOR_MAX_GUARDS_V2": (0x100172E4, 132, 5, 7, "_ADVISOR_MAX_GUARDS_V2__15CAdvisorScoringFv"),
    "_ADVISOR_MAX_RESEARCHERS": (0x100173A8, 120, 6, 8, "_ADVISOR_MAX_RESEARCHERS__15CAdvisorScoringFv"),
    "_ADVISOR_MAX_RESEARCHERS_V2": (0x10017460, 132, 7, 8, "_ADVISOR_MAX_RESEARCHERS_V2__15CAdvisorScoringFv"),
    "_ADVISOR_MAX_MECHANICS": (0x100170B0, 120, 8, 4, "_ADVISOR_MAX_MECHANICS__15CAdvisorScoringFv"),
    "_ADVISOR_BOAT_ARRIVED": (0x10017AE4, 8, 9, 0, "_ADVISOR_BOAT_ARRIVED__15CAdvisorScoringFv"),
}
OWN = {a for a, *_ in FUNCS.values()}

GET_ANALYSER = 0x1013EBC8
NUMBER_OF_STAFF = 0x100E8144
MAIN_BALANCE = 0x102B4568
GAME_WORLD = 0x1037EBFC

BAL_OFFS = [0x2A8, 0x2AC, 0x2B0, 0x2B4, 0x2B8]
OBJ_OFFS = list(range(0x38C, 0x3C8, 4)) + [0x40C]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_get_analyser(e):
    L = e.cl
    L.rec("GetParkAnalyser", (L.lab(e.r(3)),))
    e.set_r(3, L.analyser)


def stub_number_of_staff(e):
    L = e.cl
    L.rec("NumberOfStaffType", (L.lab(e.r(3)), T.s32(e.r(4))))
    e.set_r(3, L.ret("Staff") & MASK)


STUBS.internal(GET_ANALYSER, stub_get_analyser)
STUBS.internal(NUMBER_OF_STAFF, stub_number_of_staff)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.world = e.alloc(16)
        self.analyser = e.alloc(16)
        self.L = e.cl = CL.PpcLog({self.world: 1, self.analyser: 2}, case["queues"])
        self.L.analyser = self.analyser
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_world(self):
        self.words(GAME_WORLD, [self.world])

    def set_bal(self, off, v):
        self.words(MAIN_BALANCE + off, [v])

    def new_obj(self):
        self.obj = self.e.alloc(0x410)

    def set_obj(self, off, v):
        self.words(self.obj + off, [v])

    def call(self, name):
        addr, _, _, _, label = FUNCS[name]
        r3, _, _ = T.call_ppc(self.e, addr,[self.obj])
        return T.s32(r3)


class Host(Side):
    fmt = "<I"
    layout = None

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.world = lib.tpw_adv_alloc(16)
        self.analyser = lib.tpw_adv_addr(b"analyser")
        CL.host_begin(lib, {1: self.world, 2: self.analyser}, case["queues"])

    def done(self):
        pass

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_world(self):
        self.write(self.lib.tpw_adv_addr(b"world"), struct.pack("<Q", self.world))

    def set_bal(self, off, v):
        self.words64(self.lib.tpw_adv_addr(b"values") + ((off - 8) // 4) * 8, v)

    def words64(self, a, v):
        self.write(a, struct.pack("<Q", v & MASK))

    def new_obj(self):
        self.obj = self.lib.tpw_adv_alloc(Host.layout[2] + 64)

    def set_obj(self, off, v):
        self.words64(self.obj + Host.layout[0] + ((off - 0x14) // 4) * 8, v)

    def call(self, name):
        return T.s32(self.lib.tpw_adv_call(FUNCS[name][2], self.obj))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for n, res, args in (("addr", C.c_uint64, [C.c_char_p]), ("alloc", C.c_uint64, [C.c_uint32]),
                         ("layout", C.c_int32, [C.POINTER(C.c_int64)]), ("call", C.c_int32, [C.c_int32, C.c_uint64])):
        f = getattr(lib, "tpw_adv_" + n)
        f.restype, f.argtypes = res, args
    out = (C.c_int64 * 8)()
    n = lib.tpw_adv_layout(out)
    Host.layout = list(out)[:n]
    assert Host.layout[1] == 8, Host.layout
    lib.bound = True


def build(s, case):
    s.set_world()
    for off, v in zip(BAL_OFFS, case["bal"]):
        s.set_bal(off, v)
    s.new_obj()
    for off, v in zip(OBJ_OFFS, case["obj"]):
        s.set_obj(off, v)


def run(s, case):
    try:
        build(s, case)
        return {"answer": s.call(case["fn"]), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def small(r):
    return r.choice([0, 1, 2, 3, -1, 5, 100])


def make_case(r, fn, tie=None):
    bal = [small(r) for _ in BAL_OFFS]
    obj = [r.choice([0, 1, 2, 3, -1, 50, 77]) for _ in OBJ_OFFS]
    count = r.choice([0, 1, 2, 3, 5, -1])
    if tie is not None:
        # make the compared slots equal / the count equal to the balance value, now and then
        which = r.randrange(3)
        bi = {0: 0, 1: 1, 2: 2, 3: 3, 4: 4}
        bal_i = [i for i, o in enumerate(BAL_OFFS)]
        if which == 0:
            for bo in range(len(BAL_OFFS)):
                oi = r.randrange(len(OBJ_OFFS))
                obj[oi] = bal[bo]
        elif which == 1:
            count = bal[r.randrange(len(BAL_OFFS))]
        else:
            for bo in range(len(BAL_OFFS)):
                obj[OBJ_OFFS.index(0x398)] = bal[2]
                obj[OBJ_OFFS.index(0x3B0)] = bal[3]
                obj[OBJ_OFFS.index(0x3A4)] = bal[1]
                obj[OBJ_OFFS.index(0x38C)] = bal[0]
                obj[OBJ_OFFS.index(0x3BC)] = bal[4]
            count = bal[r.randrange(len(BAL_OFFS))]
    return {"fn": fn, "bal": bal, "obj": obj, "queues": {"Staff": [count]}}


def edges_for(fn):
    def gen():
        r = random.Random(sum(map(ord, fn)))
        for i in range(120):
            yield make_case(r, fn, tie=i % 3 if i % 4 else None)
    return gen


def rnd_for(fn):
    def gen(r):
        return make_case(r, fn, tie=r.randrange(3) if r.random() < 0.5 else None)
    return gen


TESTS = [FnTest(name, ppc, host, edge=edges_for(name), rnd=rnd_for(name)) for name in FUNCS]
