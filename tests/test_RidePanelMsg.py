"""Unit v51005, the ride list panel's message handler, against libtpw_unit_RidePanelMsg.dylib (tests/unit_RidePanelMsg.cpp), main app
("SimTheme Park"), src/game/ui/ui_ridepanel.cpp:
  RidePanelMsg 0x101b7538

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the window lookups, the list control, the bank, the control manager, the ride database, the golden ticket control, the mouse
modes, the string table, the other panels' functions) are fakes on both sides that record their arguments and answer from the case's queues.

The world of a case: the panel's globals (the three ride list words, the type, the sort column, the two words RidePanelMsg only reads),
the mouse globals, the world pointer, the string table slot, the object control's cost and the ride record's two words. What is compared:
the answer, the globals above read back, whether the panel and mouse pointers are none, a known object or a new one, and the event log.

No fused operations. Not tested: a NULL title window in message 0x10 and a NULL object control (the original dereferences them).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RidePanelMsg"
MASK = 0xFFFFFFFF

FN = 0x101B7538
SIZES = {FN: 2084}
OWN = set(SIZES)

ADDR = dict(
    gGameWorld=0x1037EBFC, db=0x1037F6C8, tableslot=0x1037FF18 + 0x564, buf=0x10380C48,
    f8a=0x1039598A, f8c=0x1039598C, f90=0x10395990, type=0x10395994, panelptr=0x10395998,
    sort=0x102ACEE4, pending=0x102ACEE8, help=0x102ACEEA, w1e0=0x102DC1E0, w1e4=0x102DC1E4, gMouse=0x102DC1F4,
)
HOST_LAYOUT = [392, 152, 4]   # the host's BalanceValue is 8 bytes (a union with pointers): slots 46 and 16, then the control's cost

# the persistent objects of the PowerPC side (set up once)
P = {}
LABELS = ["wa", "wb", "panel", "world", "bank", "ctl", "db", "mouse", "table"]   # labels 1..9


def s32(v):
    return T.s32(v)


def hi16(v):
    return (v >> 16) & 0xFFFF


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def win_of(k):
    return {0: 0, 1: P["wa"], 2: P["wb"], 3: P["panel"]}[k]


def reg(addr, name, spec="", ret=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret))


reg(0x101B6500, "InitInfoWindow")
reg(0x101B7108, "ChangeInfoWindow", "i")
reg(0x101B8C28, "RemoveRideListPanel")
reg(0x101815D4, "UIMain_DestroyRidePreview")
reg(0x1018227C, "UIMain_CancelAdvisorMessage", "i")
reg(0x101821CC, "UIMain_AdvisorMessage", "i")
reg(0x10182160, "UIMain_NotEnoughDough")
reg(0x10181570, "UIMain_TickRidePreview")
reg(0x1018493C, "UIMain_DefaultPanelMsg", "Liii", "i")
reg(0x101A1C88, "CreateHireListPanel", "i")
reg(0x101CAC3C, "IveGotTicks", "", "i")
reg(0x101F9268, "UI_IterateThingNext", "iL", "i")
reg(0x101DBE40, "GetWindow", "Li", lambda e, log: win_of(log.ret("GetWindow")))
reg(0x101DD5E0, "setAnimTimer", "Lii", "h")
reg(0x101DD6B8, "startAnimTimer", "Li")
reg(0x101D2220, "UpdateDisplay", "L")
reg(0x1013EDFC, "GetBankAccount", "L", lambda e, log: P["bank"])
reg(0x100F32B4, "GetBalance", "L", "i")
reg(0x1007E744, "CMMPathDraw2", "Niw")
reg(0x1007E6E4, "CMMPathDraw1", "Ni")
reg(0x1007EA78, "CMMDefault", "N")
reg(0x1007E2E0, "AssignMouseMode", "LL")


def st_data(e):
    L = e.cl
    L.rec("GetData", (L.lab(e.r(3)), s32(e.r(4))))
    e.write(e.r(5), struct.pack(">I", L.ret("GetData") & MASK))
    e.set_r(3, 0)


def st_update(name, glob):
    def fn(e):
        L = e.cl
        p = e.r(3)
        L.rec(name, (int.from_bytes(e.read(p, 2), "big"), int(p == ADDR[glob])))
    return fn


def st_control(e):
    L = e.cl
    L.rec("GetObjectControl", ((e.r(3) - P["world"]) & MASK, hi16(e.r(4)), s32(e.r(5))))
    e.set_r(3, P["ctl"])


def st_ticket(name):
    def fn(e):
        L = e.cl
        L.rec(name, ((e.r(3) - P["world"]) & MASK, hi16(e.r(4))))
        e.set_r(3, L.ret(name) & 0xFF)
    return fn


def st_ride(e):
    L = e.cl
    L.rec("GetRideInfo", (L.lab(e.r(3)), hi16(e.r(4))))
    e.set_r(3, P["ride"])


def st_format(e):
    L = e.cl
    t = e.r(7)
    w = [e.u32(t + 4 * i) for i in range(5)]
    L.rec("FormatString", (L.lab(e.r(3)), int(e.r(4) == ADDR["buf"]), e.r(5) & MASK, e.r(6) & MASK, w[0], w[2], s32(w[3]), s32(w[4])))
    e.set_r(3, 0)


def st_labs(e):
    e.set_r(3, abs(s32(e.r(3))) & MASK)


def st_settext(e):
    L = e.cl
    L.rec("SetText", (L.lab(e.r(3)), int(e.r(4) == ADDR["buf"])))
    e.set_r(3, 1)


STUBS.internal(0x101D41F0, st_data)
STUBS.internal(0x101B6D18, st_update("UpdateHelpText", "f8a"))
STUBS.internal(0x101B6A5C, st_update("UpdateInfoWindow", "f8a"))
STUBS.internal(0x101B5F1C, st_update("UpdateViewWindow", "f8c"))
STUBS.internal(0x100FA0B8, st_control)
STUBS.internal(0x100FBFEC, st_ticket("HasBeenBought"))
STUBS.internal(0x100FC0C8, st_ticket("CanIAfford"))
STUBS.internal(0x10154868, st_ride)
STUBS.internal(0x10239730, st_format)
STUBS.internal(0x10239928, st_labs)
STUBS.internal(0x1023BAA8, st_settext)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    for name in ("wa", "wb", "panel"):
        P[name] = T.persist_alloc(e, 0x140)
    for name in ("world", "bank", "table", "mouse"):
        P[name] = T.persist_alloc(e, 64)
    P["ctl"] = T.persist_alloc(e, 64)
    P["ride"] = T.persist_alloc(e, 0x1000)
    vt = T.persist_alloc(e, 0x40)
    for name in ("wa", "wb", "panel"):
        e.write(P[name] + 0x118, struct.pack(">I", vt))
    P["db"] = ADDR["db"]


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def wptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, 8 if self.pfmt[1] == "Q" else 4))[0]

    def g(self, name):
        if name in ADDR:
            return ADDR[name]
        return P[name]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    gold_off, which_off = 0xC4, 0x4C

    def __init__(self, e, case):
        self.e = e
        fixed = {P[n]: i + 1 for i, n in enumerate(LABELS)}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, m, a, b):
        r3, _, _ = T.call_ppc(self.e, FN, [P["panel"], m & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    gold_off, which_off = HOST_LAYOUT[0], HOST_LAYOUT[1]

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.lib.tpw_rpm_addr(b"world")
        fixed = {i + 1: lib.tpw_rpm_addr(n.encode()) for i, n in enumerate(LABELS)}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        if name in ("wa", "wb", "panel", "world", "bank", "ctl", "ride", "table", "mouse", "db"):
            return self.lib.tpw_rpm_addr(name.encode())
        return self.lib.tpw_rpm_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, m, a, b):
        return self.lib.tpw_rpm_msg(self.g("panel"), s32(m), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_rpm_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "msg").restype, getattr(lib, p + "msg").argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.words(s.g("f90"), [case["f90"]])
    s.half(s.g("f8a"), case["f8a"])
    s.half(s.g("f8c"), case["f8c"])
    s.half(s.g("pending"), case["pending"])
    s.half(s.g("help"), case["help"])
    s.words(s.g("type"), [case["type"]])
    s.words(s.g("sort"), [case["sort"]])
    s.words(s.g("w1e0"), [case["w1e0"]])
    s.words(s.g("w1e4"), [case["w1e4"]])
    s.wptr(s.g("gMouse"), s.g("mouse") if case["mouse"] else 0)
    s.wptr(s.g("panelptr"), s.g("panel") if case["panelptr"] else 0)
    s.wptr(s.g("gGameWorld"), s.g("world"))
    s.wptr(s.g("tableslot"), s.g("table"))
    s.words(s.g("ctl") + 4, [case["cost"]])
    s.words(s.g("ride") + s.gold_off, [case["goldcost"]])
    s.words(s.g("ride") + s.which_off, [case["whichui"]])


def category(s, ptr, known):
    if ptr == 0:
        return 0
    return 1 if ptr == s.g(known) else 2


def observe(s, case, answer):
    return {
        "answer": answer, "log": s.log(),
        "f90": s.rd_word(s.g("f90")), "f8a": s.rd_half(s.g("f8a")), "f8c": s.rd_half(s.g("f8c")),
        "sort": s.rd_word(s.g("sort")), "w1e0": s.rd_word(s.g("w1e0")), "w1e4": s.rd_word(s.g("w1e4")),
        "mouse": category(s, s.rptr(s.g("gMouse")), "mouse"), "panelptr": category(s, s.rptr(s.g("panelptr")), "panel"),
        "type": s.rd_word(s.g("type")), "pending": s.rd_half(s.g("pending")), "help": s.rd_half(s.g("help")),
    }


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(*case["args"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SMALL = [0, 1, 2, 0x1234, 0xFFFF, 0x8000]
MSGS = [0x1E, 0x10, 5, 0x15, 0x14, 0x401, 0x100, 0x400, 0x406, 0, 1, 4, 6, 0xF, 0x11, 0x13, 0x16, 0x1D, 0x1F, 0xFF, 0x101, 0x3FF, 0x402,
        0x405, 0x407, 0x1000, -1]
BALANCES = [0, 1, -1, 100, -100, 0x7FFFFFFF, -0x7FFFFFFF, -0x80000000, 5000, -5000]


def base(r):
    return {
        "f90": T.rint(r), "f8a": r.choice(SMALL), "f8c": r.choice(SMALL), "pending": r.choice(SMALL), "help": r.choice(SMALL),
        "type": r.choice([0, 1, 2, 3, -1, 5, 0x7FFFFFFF]), "sort": T.rint(r), "w1e0": T.rint(r), "w1e4": T.rint(r),
        "mouse": r.choice([0, 1]), "panelptr": r.choice([0, 1]),
        "cost": r.choice([0, 100, 5000, -5, T.rint(r)]), "goldcost": r.choice([0, 0, 1, 50, -1]),
        "whichui": r.choice([0, 0, 1, 2, -1]), "args": [0, 0, 0],
        "queues": {
            "GetWindow": [r.choice([0, 1, 1, 2, 3]), r.choice([1, 2, 3])],
            "setAnimTimer": [r.choice([0, 1, -1, 0x10000])],
            "GetBalance": [r.choice(BALANCES)],
            "GetData": [r.choice([-1, -2, -3, 0, 1, 7, 0x12345, 0x8001, 0xFFFF, 0x10000, -0x10000, 0x7FFFFFFF, -0x80000000])],
            "HasBeenBought": [r.choice([0, 1])],
            "CanIAfford": [r.choice([0, 1])],
            "IveGotTicks": [T.rint(r)],
            "UI_IterateThingNext": [r.choice([0, 1])],
            "UIMain_DefaultPanelMsg": [T.rint(r)],
        },
    }


def with_args(r, m, a, b, **kw):
    c = base(r)
    c["args"] = [m, a, b]
    for k, v in kw.items():
        if k in c["queues"]:
            c["queues"][k] = v
        else:
            c[k] = v
    return c


def rand_args(r):
    m = r.choice(MSGS)
    if m == 0x10:
        a = r.choice([0x80080, 0x80080, 0x80081, 0x70080, 0, -1])
    elif m == 0x100:
        a = r.choice([0x1FF, -2, 0x1FE, 0x200, -3, 0, -1, 0x7FFFFFFF])
    else:
        a = r.choice([0x1F8, 5, 0, -1, T.rint(r)])
    b = r.choice([-1, 0, 1, 7, 100, T.rint(r)])
    return m, a, b


def edges():
    r = random.Random(51005)
    for m in MSGS:
        for a in (0, 0x80080, 0x1FF, -2, 0x1F8):
            yield with_args(r, m, a, r.choice([-1, 0, 3]))
    # the tick: every branch
    for f8c, pending in ((1, 1), (1, 2)):
        for ticks, f90 in ((1000, 0), (500, 0), (501, 0), (0x7FFFFFFF, -0x80000000), (0, 0x7FFFFFFF)):
            for f8a, help_ in ((3, 3), (3, 4)):
                yield with_args(r, 0x1E, 0, 0, f8c=f8c, pending=pending, f90=f90, f8a=f8a, help=help_, IveGotTicks=[ticks])
    # the title text: both signs
    for bal in BALANCES:
        for first in (0, 1, 2, 3):
            yield with_args(r, 0x10, 0x80080, 0, GetBalance=[bal], GetWindow=[first, 2])
    # the list: the found window, the id, whether the view changed
    for f8c in (0, 7, 0x1234):
        for data in (0x1234, 0x11234, 0, -1, 0xFFFF):
            for win in (0, 1):
                for b in (-1, 4):
                    yield with_args(r, 0x401, 0x1F8, b, f8c=f8c, GetData=[data], GetWindow=[win, 1])
    # a click: every way through
    for data in (-3, -2, -1, 0, 1, 0x12345, 0x8001):
        for win in (0, 1):
            for b in (-1, 2):
                yield with_args(r, 0x400, 0x1F8, b, GetData=[data], GetWindow=[win, 1])
    for goldcost in (0, 1, -1):
        for bought in (0, 1):
            for afford in (0, 1):
                for cost, bal in ((100, 99), (100, 100), (100, 101), (0, 0), (-5, -5), (-5, -6)):
                    for whichui in (0, 1):
                        for it in (0, 1):
                            for mouse in (0, 1):
                                yield with_args(r, 0x400, 0x1F8, 1, goldcost=goldcost, HasBeenBought=[bought], CanIAfford=[afford],
                                                cost=cost, GetBalance=[bal], whichui=whichui, UI_IterateThingNext=[it], mouse=mouse,
                                                GetData=[0x1234], GetWindow=[1, 1])
    for t in (0, 1, 2, 3, -1):
        for anim in (0, 1):
            yield with_args(r, 0x15, 0, 0, type=t, setAnimTimer=[anim])


def rnd(r):
    m, a, b = rand_args(r)
    return with_args(r, m, a, b)


TESTS = [
    FnTest("RidePanelMsg", ppc, host, edge=edges, rnd=rnd),
]
