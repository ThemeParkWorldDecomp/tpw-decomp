"""Unit v5892, twelve advisor scorers, against libtpw_unit_CAdvisorScoring__ADVISOR_GOLD_TICKET_NEAR_TO_XHA_2.dylib
(tests/unit_CAdvisorScoring__ADVISOR_GOLD_TICKET_NEAR_TO_XHA_2.cpp), main app ("SimTheme Park"),
src/AdvisorScoring_CAdvisorScoring__ADVISOR_GOLD_TICKET_NEAR_TO_XHA_2.cpp:
  _ADVISOR_GOLD_TICKET_NEAR_TO_XHAPPINESS 0x1001b18c, _ADVISOR_CONGRAT_BALLOONS 0x10018854, _ADVISOR_CONGRAT_SEND_POSTCARD 0x1001873c,
  _ADVISOR_CONGRAT_PUBLISH_PARK 0x10018624, _ADVISOR_CONGRAT_MECHANICS_TRAINED 0x100183ac, _ADVISOR_CONGRAT_ENTERTAINERS_TRAINED 0x100182b0,
  _ADVISOR_CONGRAT_HANDYMEN_TRAINED 0x100181b8, _ADVISOR_CONGRAT_RESEARCHERS_TRAINED 0x100180c0, _ADVISOR_CONGRAT_SECURITY_TRAINED 0x10017fc8,
  _ADVISOR_CONGRAT_VISITORS_HAPPY 0x10017edc, _ADVISOR_CONGRAT_STAFF_HAPPY 0x10017df4, _ADVISOR_VISITORS_TOO_SCARED 0x10017c40

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory at the Mac's addresses, big-endian, and the
host library's memory). The callees outside the unit (CWorld::GetParkAnalyser and GetStaffHQ, the analyser's readers, the master save's
constructor and HaveTicket) are fakes on both sides that record their arguments and answer from the case's queues.

The world of a case: the scoring object with the balance values the function reads (Mac offset from this; the host slots are 8 bytes, so the
host address comes from the library), gGameWorld, the master save pointer, the staff HQ block, the main balance slots 467 and 468. Every
threshold is small and the fakes' answers are the threshold plus -1, 0 or +1 (floats: plus -1, -0.5, 0, 0.5, 1, NaN, -0.0), so each compare is
hit on both sides of its boundary. What is compared: the answer, the event log, the halfword at this+4 (the scariest ride's id) and the master
save pointer (null, kept, or made).

No fused operations. Not tested: the size 0x4c given to operator new in the GOLD scorer (the library's operator new is the program's arena one,
not logged on the host); a main balance product past 2^31 (Unicorn's fctiwz saturates differently).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_GOLD_TICKET_NEAR_TO_XHA_2"
MASK = 0xFFFFFFFF

ANALYSER, WORLD, MASTER = 0x10381000, 0x10381200, 0x10381100
HOST_LAYOUT = [8, 2]

ADDR = dict(gGameWorld=0x1037ebfc, gMasterSave=0x10380a8c, mainbal467=0x102b4568 + 0x758, mainbal468=0x102b4568 + 0x75C)

A_GETPARK, A_GETHQ, A_HAPPY, A_STAFFHAPPY = 0x1013ebc8, 0x1013e99c, 0x100e4cd8, 0x100e4ebc
A_VISITORS, A_BALLOONS, A_VALUE, A_STAFFTYPE = 0x100e7278, 0x100e7a94, 0x100e5110, 0x100e8144
A_QUALITY, A_NSTAFF, A_GATES, A_OBJTYPE = 0x100e88a0, 0x100e8324, 0x100e7028, 0x100ea5d8
A_EXCITE, A_SCARY, A_CTOR, A_TICKET = 0x100ed3a0, 0x100ed57c, 0x101694dc, 0x1016a12c

# name, address, size, hq word index (or None), stages (queue key, threshold), answer offset
# a threshold is a balance offset, "m468" (main balance slot 468) or "gold" (the product of the GOLD scorer)
FUNCS = [
    ("_ADVISOR_GOLD_TICKET_NEAR_TO_XHAPPINESS", 0x1001B18C, 304, None,
     [("GetHappinessLevel", "gold"), ("NumberOfVisitorsInPark", "m468"), ("HaveTicket", None)], 0x5D0),
    ("_ADVISOR_CONGRAT_BALLOONS", 0x10018854, 92, None, [("NumberOfVisitorsWithBalloons", 0x4A0)], 0x4A4),
    ("_ADVISOR_CONGRAT_SEND_POSTCARD", 0x1001873C, 208, None,
     [("GetParkValue", 0x490), ("NumberOfVisitorsInPark", 0x498), ("GetHappinessLevel", 0x494)], 0x49C),
    ("_ADVISOR_CONGRAT_PUBLISH_PARK", 0x10018624, 208, None,
     [("GetParkValue", 0x480), ("NumberOfVisitorsInPark", 0x488), ("GetHappinessLevel", 0x484)], 0x48C),
    ("_ADVISOR_CONGRAT_MECHANICS_TRAINED", 0x100183AC, 172, 1,
     [("NumberOfStaffType", 0x474), ("AverageQualityOfStaffType", 0x478)], 0x47C),
    ("_ADVISOR_CONGRAT_ENTERTAINERS_TRAINED", 0x100182B0, 172, 2,
     [("NumberOfStaffType", 0x468), ("AverageQualityOfStaffType", 0x46C)], 0x470),
    ("_ADVISOR_CONGRAT_HANDYMEN_TRAINED", 0x100181B8, 172, 0,
     [("NumberOfStaffType", 0x45C), ("AverageQualityOfStaffType", 0x460)], 0x464),
    ("_ADVISOR_CONGRAT_RESEARCHERS_TRAINED", 0x100180C0, 172, 4,
     [("NumberOfStaffType", 0x450), ("AverageQualityOfStaffType", 0x454)], 0x458),
    ("_ADVISOR_CONGRAT_SECURITY_TRAINED", 0x10017FC8, 172, 3,
     [("NumberOfStaffType", 0x444), ("AverageQualityOfStaffType", 0x448)], 0x44C),
    ("_ADVISOR_CONGRAT_VISITORS_HAPPY", 0x10017EDC, 164, None,
     [("NumberOfVisitorsInPark", 0x438), ("GetHappinessLevel", 0x43C)], 0x440),
    ("_ADVISOR_CONGRAT_STAFF_HAPPY", 0x10017DF4, 164, None,
     [("NumberOfStaff", 0x42C), ("GetStaffHappinessLevel", 0x430)], 0x434),
    ("_ADVISOR_VISITORS_TOO_SCARED", 0x10017C40, 224, None,
     [("GatesOpen", None), ("NumberOfObjectTypeInPark", 0x41C), ("GetAverageExcitementLevelOfAllRides", 0x418)], 0x420),
]
SIZES = {f[1]: f[2] for f in FUNCS}
OWN = set(SIZES)
FLOAT_KEYS = ("GetHappinessLevel", "GetStaffHappinessLevel")


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def reader(key, nargs, is_float=False):
    def stub(e):
        L = e.cl
        L.rec(key, tuple([L.lab(e.r(3))] + [s32(e.r(3 + i)) for i in range(1, nargs)]))
        v = L.ret(key)
        if is_float:
            e.set_f1(bitsf(v))
        else:
            e.set_r(3, v & MASK)
    return stub


def stub_getpark(e):
    e.cl.rec("GetParkAnalyser", (e.cl.lab(e.r(3)),))
    e.set_r(3, ANALYSER)


def stub_gethq(e):
    e.cl.rec("GetStaffHQ", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.hq)


def stub_scary(e):
    L = e.cl
    L.rec("GetScariestRide", (L.lab(e.r(3)),))
    e.write(e.r(4), struct.pack(">H", L.ret("GetScariestRide") & 0xFFFF))
    e.set_r(3, 0)


def stub_ctor(e):
    e.cl.rec("CFSGlobalMasterSave", (e.cl.lab(e.r(3)),))


STUBS.internal(A_GETPARK, stub_getpark)
STUBS.internal(A_GETHQ, stub_gethq)
STUBS.internal(A_HAPPY, reader("GetHappinessLevel", 2, True))
STUBS.internal(A_STAFFHAPPY, reader("GetStaffHappinessLevel", 2, True))
STUBS.internal(A_VISITORS, reader("NumberOfVisitorsInPark", 1))
STUBS.internal(A_BALLOONS, reader("NumberOfVisitorsWithBalloons", 1))
STUBS.internal(A_VALUE, reader("GetParkValue", 2))
STUBS.internal(A_STAFFTYPE, reader("NumberOfStaffType", 2))
STUBS.internal(A_QUALITY, reader("AverageQualityOfStaffType", 2))
STUBS.internal(A_NSTAFF, reader("NumberOfStaff", 1))
STUBS.internal(A_GATES, reader("GatesOpen", 1))
STUBS.internal(A_OBJTYPE, reader("NumberOfObjectTypeInPark", 3))
STUBS.internal(A_EXCITE, reader("GetAverageExcitementLevelOfAllRides", 2))
STUBS.internal(A_SCARY, stub_scary)
STUBS.internal(A_CTOR, stub_ctor)
STUBS.internal(A_TICKET, reader("HaveTicket", 2))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def wr_half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ANALYSER: 1, WORLD: 2, MASTER: 3}, case["queues"])
        self.L.hq = 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def new_obj(self):
        return self.e.alloc(0x700)

    def new_hq(self):
        hq = self.e.alloc(0x80)
        self.L.hq = hq
        return hq

    def slot(self, obj, off):
        return obj + off

    def word4(self, obj):
        return obj + 4

    def hqword(self, hq, i):
        return hq + 0x0C + 4 * i

    def set_ptr(self, name, v):
        self.words(self.g(name), [v])

    def get_ptr(self, name):
        return struct.unpack(">I", self.read(self.g(name), 4))[0]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, i, obj):
        r3, _, _ = T.call_ppc(self.e, FUNCS[i][1], [obj])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: ANALYSER, 2: WORLD, 3: MASTER}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_adv_addr(name.encode())

    def new_obj(self):
        return self.lib.tpw_adv_alloc(self.lib.tpw_adv_objsize())

    def new_hq(self):
        hq = self.lib.tpw_adv_alloc(self.lib.tpw_adv_hqsize())
        self.write(self.g("hq"), struct.pack("<Q", hq))
        return hq

    def slot(self, obj, off):
        return self.lib.tpw_adv_slot(obj, off)

    def word4(self, obj):
        return self.lib.tpw_adv_word4(obj)

    def hqword(self, hq, i):
        return self.lib.tpw_adv_hqword(hq, i)

    def set_ptr(self, name, v):
        self.write(self.g(name), struct.pack("<Q", v))

    def get_ptr(self, name):
        return struct.unpack("<Q", self.read(self.g(name), 8))[0]

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, i, obj):
        return self.lib.tpw_adv_call(i, obj) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_adv_"
    for n, res, args in (("addr", C.c_uint64, [C.c_char_p]), ("alloc", C.c_uint64, [C.c_uint32]),
                         ("objsize", C.c_uint32, []), ("hqsize", C.c_uint32, []),
                         ("slot", C.c_uint64, [C.c_uint64, C.c_int32]), ("word4", C.c_uint64, [C.c_uint64]),
                         ("hqword", C.c_uint64, [C.c_uint64, C.c_int32]), ("call", C.c_uint32, [C.c_int32, C.c_uint64])):
        getattr(lib, p + n).restype, getattr(lib, p + n).argtypes = res, args
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    fi = case["fn"]
    name, addr, size, hqi, stages, ans = FUNCS[fi]
    obj = s.new_obj()
    s.words(s.slot(obj, ans), [case["answer"]])
    for off, w in case["slots"].items():
        s.words(s.slot(obj, off), [w])
    s.wr_half(s.word4(obj), case["word4"])
    s.set_ptr("gGameWorld", WORLD)
    s.set_ptr("gMasterSave", MASTER if case["master"] else 0)
    s.words(s.g("mainbal467"), [case["m467"]])
    s.words(s.g("mainbal468"), [case["m468"]])
    hq = s.new_hq()
    if hqi is not None:
        s.words(s.hqword(hq, hqi), [case["hqword"]])
    case["obj"] = obj


def observe(s, case, answer):
    p = s.get_ptr("gMasterSave")
    return {"answer": answer, "log": s.log(), "word4": s.rd_half(s.word4(case["obj"])),
            "master": 0 if p == 0 else (1 if p == MASTER else 2)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["fn"], case["obj"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SM = [-2, -1, 0, 1, 2, 3]
HDELTA = [-1.0, -0.5, 0.0, 0.5, 1.0]


def case_for(r, fi):
    name, addr, size, hqi, stages, ans = FUNCS[fi]
    c = {"fn": fi, "slots": {}, "queues": {}, "answer": r.choice([r.randrange(-2**31, 2**31), 7, 1, -1]) & MASK,
         "word4": r.randrange(0, 65536), "master": r.choice([0, 1]), "m467": r.choice([-4, -1, 0, 1, 2, 3, 6]) & MASK,
         "m468": r.choice(SM) & MASK, "hqword": r.choice([0x80000000, MASK, 0, 1, 2, 5])}
    mult = r.choice([0.0, 0.5, 1.0, 1.5, 2.0, -1.0, -0.5])
    if fi == 0:
        c["slots"][0x5CC] = fb(mult)
    for key, thr in stages:
        if thr is None:
            if key == "GatesOpen":
                c["queues"][key] = [r.choice([0, 1, 1])]
            else:
                c["queues"][key] = [r.choice([0, 1])]
            continue
        if thr == "m468":
            base = s32(c["m468"])
        elif thr == "gold":
            base = int(s32(c["m467"]) * bitsf_f32(mult))
        else:
            base = r.choice(SM)
            c["slots"][thr] = base & MASK
        if key in FLOAT_KEYS:
            if r.random() < 0.08:
                c["queues"][key] = [r.choice([0x7FC00000, 0x80000000, 0x7F800000, 0xFF800000])]
            else:
                c["queues"][key] = [fb(base + r.choice(HDELTA))]
        else:
            c["queues"][key] = [(base + r.choice([-1, 0, 1, 1])) & MASK]
    if fi == 11:
        c["queues"]["GetScariestRide"] = [r.randrange(0, 65536)]
    return c


def bitsf_f32(x):
    return struct.unpack(">f", struct.pack(">f", x))[0]


def make_test(fi):
    def rnd(r):
        return case_for(r, fi)

    def edges():
        r = random.Random(5000 + fi)
        for _ in range(60):
            yield case_for(r, fi)

    return FnTest(FUNCS[fi][0], ppc, host, edge=edges, rnd=rnd)


TESTS = [make_test(i) for i in range(len(FUNCS))]
