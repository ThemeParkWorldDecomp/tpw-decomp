"""Unit v5296, the keytable actions, against libtpw_unit_enter_hoarding_mode.dylib (tests/unit_enter_hoarding_mode.cpp), main app
("SimTheme Park"), src/ui_Postcard_panel_win_ticket_1_show_person_history_info_enter_hoarding_mode.cpp:
  win_ticket_7 0x1014f16c, win_ticket_8 0x1014f208, win_ticket_9 0x1014f2a4, enter_hoarding_mode 0x1014f494,
  force_ride_breakdown 0x1014f7e0, park_attraction_value 0x1014fa2c

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory at the Mac's addresses, host library
memory); the same code reads both back. The callees outside the unit (constructors, EarnedTicket, AssignMouseMode, GetThingId, the assert,
IsBrokenDown, RequestBreakdown, GetFunnyTime..., CTRACE) are fakes on both sides that record their arguments and answer from the case.

The world of a case: gMasterSave, gMouse and its two words, the pointed thing of gIntrface, the thing array slots 0 and id, the world
pointer, and the queues. What is compared: the answer, those globals (as null / the preset object / a new one) and the event log.

No fused operations. Not tested: nothing skipped; thing ids are below 64 (the host table is larger, the Mac's has room).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_enter_hoarding_mode"
MASK = 0xFFFFFFFF

FN = dict(win_ticket_7=0x1014F16C, win_ticket_8=0x1014F208, win_ticket_9=0x1014F2A4, enter_hoarding_mode=0x1014F494,
          force_ride_breakdown=0x1014F7E0, park_attraction_value=0x1014FA2C)
SIZES = {0x1014F16C: 120, 0x1014F208: 120, 0x1014F2A4: 120, 0x1014F494: 200, 0x1014F7E0: 176, 0x1014FA2C: 252}
OWN = set(SIZES)

CALLEES = dict(
    CFSGlobalMasterSave=0x101694DC, EarnedTicket=0x1016A2A0, CMMPathDraw=0x1007E6E4, CMMDefault=0x1007EA78, AssignMouseMode=0x1007E2E0,
    GetThingId=0x1012E1F8, GAME_ASSERTMSG=0x1000DDE8, IsBrokenDown=0x1010C878, RequestBreakdown=0x1010D90C, GetFunnyTime=0x101127D0,
    GetFunnyTimeSinceGameTurn=0x10112C80, GetParkAnalyser=0x1013EBC8, GetTotalParkAttractionPoints=0x100E5B1C,
    GetThreeMostDominantThoughts=0x100E5DC8, CTRACE=0x1006FCBC)

ADDR = dict(master=0x10380A8C, mouse=0x102DC1F4, w1e0=0x102DC1E0, w1e4=0x102DC1E4, intrface=0x102DC1AC, world=0x1037EBFC,
            slots=0x1034CBFC, worldref=0x102A460C)
HOST_LAYOUT = [0x2A0, 16, None]      # mParkTime, mThingUnderPointer (the Mac's is 8; host pointers are 8 bytes), stride (host sizeof)

# fixed fake objects (never dereferenced): address -> label
WORLD, ANALYSER, THING, OBJECT, MASTER, MOUSE = 0x20001000, 0x20004000, 0x20005000, 0x20006000, 0x20007000, 0x20008000
LABELS = {WORLD: 1, ANALYSER: 2, THING: 3, OBJECT: 4, MASTER: 5, MOUSE: 6}
HOST_STRIDE = [0]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def S(name, spec, ret=None):
    STUBS.internal(CALLEES[name], CL.recorder(name, spec, ret))


S("CFSGlobalMasterSave", "N")
S("EarnedTicket", "Lii")
S("CMMPathDraw", "Ni")
S("CMMDefault", "N")
S("AssignMouseMode", "LL")
S("GAME_ASSERTMSG", "is")
S("IsBrokenDown", "L", "b")
S("RequestBreakdown", "L")
S("GetTotalParkAttractionPoints", "L", "i")
_ctr_a = CL.recorder("CTRACE", "isi")
_ctr_b = CL.recorder("CTRACE", "isiii")


def stub_ctrace(e):
    (_ctr_a if e.cstr(e.r(4)).startswith("Park") else _ctr_b)(e)


STUBS.internal(CALLEES["CTRACE"], stub_ctrace)


def stub_thing_id(e):
    L = e.cl
    L.rec("GetThingId", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetThingId") & 0xFFFF))


STUBS.internal(CALLEES["GetThingId"], stub_thing_id)


def stub_funny(e):
    L = e.cl
    off = s32(e.r(4) - WORLD)
    L.rec("GetFunnyTime", (off, e.r(5), e.r(6), e.r(7), e.r(8), e.r(9), e.r(10), e.u32(e.r(1) + 0x38), e.u32(e.r(1) + 0x3C)))


def stub_funny_turn(e):
    L = e.cl
    L.rec("GetFunnyTimeSinceGameTurn", (s32(e.r(4) - WORLD), e.r(5)))


def stub_analyser(e):
    L = e.cl
    L.rec("GetParkAnalyser", (L.lab(e.r(3)),))
    e.set_r(3, ANALYSER)


def stub_thoughts(e):
    L = e.cl
    L.rec("GetThreeMostDominantThoughts", (L.lab(e.r(3)),))
    for i, reg in enumerate((4, 5, 6)):
        e.write(e.r(reg), bytes([L.ret("thought%d" % i) & 0xFF]))


STUBS.internal(CALLEES["GetFunnyTime"], stub_funny)
STUBS.internal(CALLEES["GetFunnyTimeSinceGameTurn"], stub_funny_turn)
STUBS.internal(CALLEES["GetParkAnalyser"], stub_analyser)
STUBS.internal(CALLEES["GetThreeMostDominantThoughts"], stub_thoughts)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def ptr(self, a, v):
        """A pointer field: 4 bytes on the Mac, 8 on the host (the whole field is written, so no stale high half)."""
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]


class Ppc(Side):
    fmt = ">I"
    stride = 20
    thing_off = 8
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog(dict(LABELS), case["queues"])
        self.saved = STUBS.activate(e)
        self.write(ADDR["worldref"], struct.pack(">I", ADDR["world"]))

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op):
        r3, _, _ = T.call_ppc(self.e, FN[op], [])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    thing_off = 16
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.stride = HOST_STRIDE[0]
        CL.host_begin(lib, {v: k for k, v in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_eh_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        return getattr(self.lib, "tpw_eh_" + op)() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, "tpw_eh_addr").restype, getattr(lib, "tpw_eh_addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, "tpw_eh_layout").argtypes = [C.POINTER(C.c_int64)]
    for op in FN:
        getattr(lib, "tpw_eh_" + op).restype = C.c_uint32
    out = (C.c_int64 * 32)()
    n = lib.tpw_eh_layout(out)
    assert list(out)[:2] == HOST_LAYOUT[:2], list(out)[:n]
    HOST_STRIDE[0] = out[2]
    lib.bound = True


def build(s, case):
    s.ptr(s.g("master"), case["master"])
    s.ptr(s.g("mouse"), case["mouse"])
    s.words(s.g("w1e0"), [case["w1e0"]])
    s.words(s.g("w1e4"), [case["w1e4"]])
    s.ptr(s.g("intrface") + s.thing_off, case["thing"])
    s.ptr(s.g("world"), WORLD)
    slots = s.g("slots")
    s.ptr(slots, case["slot0"])
    s.ptr(slots + case["id"] * s.stride, case["slotid"])


def kind(v):
    return 0 if v == 0 else (LABELS.get(v, 100))


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "master": kind(s.rd_ptr(s.g("master"))), "mouse": kind(s.rd_ptr(s.g("mouse"))),
            "w1e0": s.rd_word(s.g("w1e0")), "w1e4": s.rd_word(s.g("w1e4"))}


def make(op):
    def run(s, case):
        try:
            build(s, case)
            return observe(s, case, s.call(op))
        finally:
            s.done()

    return (lambda e, case: run(Ppc(e, case), case)), (lambda lib, case: run(Host(lib, case), case))


class Case(dict):
    def __repr__(self):
        return "m%x mo%x t%x s0%x id%d sid%x" % (self["master"], self["mouse"], self["thing"], self["slot0"], self["id"], self["slotid"])


def mk(r, **kw):
    c = Case(master=r.choice([0, MASTER]), mouse=r.choice([0, MOUSE]), w1e0=r.randrange(1 << 32), w1e4=r.randrange(1 << 32),
             thing=r.choice([0, THING]), slot0=r.choice([0, OBJECT]), id=r.choice([0, 1, 2, 5, 63, r.randrange(64)]),
             slotid=r.choice([0, OBJECT]))
    c.update(kw)
    c["queues"] = {"GetThingId": [c["id"]], "IsBrokenDown": [r.choice([0, 1, 2])],
                   "GetTotalParkAttractionPoints": [r.choice([0, 1, -1, 12345, r.randrange(-(1 << 31), 1 << 31)])],
                   "thought0": [r.randrange(256)], "thought1": [r.randrange(256)], "thought2": [r.choice([0x17, r.randrange(256)])]}
    return c


def edges(seed, grid):
    def gen():
        r = random.Random(seed)
        for kw in grid:
            yield mk(r, **kw)
    return gen


def rnd(r):
    return mk(r)


def tests():
    out = []
    for name in FN:
        p, h = make(name)
        if name.startswith("win_ticket"):
            grid = [dict(master=0), dict(master=MASTER)]
        elif name == "enter_hoarding_mode":
            grid = [dict(mouse=0), dict(mouse=MOUSE), dict(mouse=0, w1e0=5, w1e4=6)]
        elif name == "force_ride_breakdown":
            grid = [dict(thing=0)] + [dict(thing=THING, id=i, slot0=a, slotid=b, queues=None) for i in (0, 1, 63) for a in (0, OBJECT)
                                      for b in (0, OBJECT)]
        else:
            grid = [dict(), dict(world=1)]
        if name == "force_ride_breakdown":
            def gen(grid=grid):
                r = random.Random(77)
                for kw in grid:
                    kw = dict(kw)
                    kw.pop("queues", None)
                    for broken in (0, 1):
                        c = mk(r, **kw)
                        c["queues"]["IsBrokenDown"] = [broken]
                        yield c
            out.append(FnTest(name, p, h, edge=gen, rnd=rnd))
        else:
            grid = [{k: v for k, v in kw.items() if k != "world"} for kw in grid]
            out.append(FnTest(name, p, h, edge=edges(len(name), grid), rnd=rnd))
    return out


TESTS = tests()
