"""Unit v5102, the ride list panel's info window and help line, against libtpw_unit_ChangeInfoWindow.dylib (tests/unit_ChangeInfoWindow.cpp), main app
("SimTheme Park"), src/game/ui/ui_ridepanel.cpp:
  ChangeInfoWindow 0x101b7108, UpdateHelpText 0x101b6d18

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (InterfaceWindow::GetWindow,
Show and GetClientRect, UI_GetMouseXY, GetObjectControl, GetBankAccount, GetBalance, GetRideInfo, HasBeenBought, CanIAfford, TbITable::FormatString
and SetHelpTextString) are fakes on both sides that record their arguments (the text of FormatString and SetHelpTextString as its first six code
units, the tag of the two FormatString calls that carry one as its magic, id and text) and answer from the case's queues. CoolRect (constructor,
PointIn, destructor) runs for real on both sides.

The world of a case: the panel and world pointers, the two string tables (fake pointers, never read), gHelpTextState, gRideListF88, the ride record's
golden ticket cost and ride type string words, the control's upgrade cost, and the queues (the windows that exist, the client rectangle, the mouse,
the balance, the golden ticket answers). What is compared: the log, gHelpTextState and gRideListF88.

No fused operations. Not tested: a NULL panel, a NULL window 0x1f8 or a NULL control (the original reads them unconditionally); a negative id other
than -1 and -2 (the original then passes a buffer it never wrote to SetHelpTextString).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ChangeInfoWindow"
MASK = 0xFFFFFFFF

FN_CHANGE, FN_UPDATE = 0x101B7108, 0x101B6D18
SIZES = {FN_CHANGE: 1024, FN_UPDATE: 924}
OWN = set(SIZES)

GETWINDOW, SHOW, GETCLIENTRECT, MOUSEXY = 0x101DBE40, 0x101DC1EC, 0x101DBA4C, 0x101CA900
GETOBJCTRL, GETBANK, GETBALANCE, GETRIDEINFO = 0x100FA0B8, 0x1013EDFC, 0x100F32B4, 0x10154868
HASBOUGHT, CANAFFORD, SETHELP, FORMATSTRING = 0x100FBFEC, 0x100FC0C8, 0x1018C694, 0x10239730

ADDR = dict(panel=0x10395998, state=0x102ACEE0, f88=0x10395988, world=0x1037EBFC, text7=0x1037FF18 + 0x568,
            text21=0x1037FF18 + 0x5A0, ridedb=0x1037F6C8)
HOST_LAYOUT = [4, 8]

# the fake pointers (the same numbers on both sides; none is ever read)
PANEL, WORLD, BANK, TABA, TABB = 0x30000100, 0x30100000, 0x30200000, 0x30000010, 0x30000020
CTRLMGR, GOLDEN = WORLD + 0x1D943C, WORLD + 0x2C0
WIN_IDS = list(range(0x1ED, 0x1F9))


def win(i):
    return 0x30001000 + i * 16


LABELS = {PANEL: 1, WORLD: 2, CTRLMGR: 3, GOLDEN: 4, BANK: 5, TABA: 6, TABB: 7}
for _i in WIN_IDS:
    LABELS[win(_i)] = _i

CTX = {}


def s32(v):
    return T.s32(v)


def units(e, a):
    """The first six code units of a UTF-16 string, zero after its end."""
    out, ended = [], False
    for h in struct.unpack(">6H", e.read(a, 12)):
        if ended:
            h = 0
        if h == 0:
            ended = True
        out.append(h)
    return tuple(out)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_getwindow(e):
    L = e.cl
    wid = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), wid))
    e.set_r(3, win(wid) if L.ret("GetWindow:%d" % wid) else 0)


def st_show(e):
    L = e.cl
    L.rec("Show", (L.lab(e.r(3)), T.s16(e.r(4))))
    e.set_r(3, 0)


def st_clientrect(e):
    L = e.cl
    L.rec("GetClientRect", (L.lab(e.r(3)),))
    vals = [L.ret(k) & 0xFFFF for k in ("rectL", "rectT", "rectR", "rectB")]
    e.write(e.r(4) + 4, struct.pack(">HHHH", *vals))
    e.set_r(3, 0)


def st_mouse(e):
    L = e.cl
    L.rec("UI_GetMouseXY", ())
    e.write(e.r(3), struct.pack(">H", L.ret("mouseX") & 0xFFFF))
    e.write(e.r(4), struct.pack(">H", L.ret("mouseY") & 0xFFFF))


def st_objctrl(e):
    L = e.cl
    L.rec("GetObjectControl", (L.lab(e.r(3)), e.r(4) >> 16, s32(e.r(5))))
    e.set_r(3, CTX["ctrl"])


def st_bank(e):
    L = e.cl
    L.rec("GetBankAccount", (L.lab(e.r(3)),))
    e.set_r(3, BANK)


def st_balance(e):
    L = e.cl
    L.rec("GetBalance", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetBalance") & MASK)


def st_rideinfo(e):
    L = e.cl
    L.rec("GetRideInfo", (L.lab(e.r(3)), e.r(4) >> 16))
    e.set_r(3, CTX["ride"])


def st_bought(e):
    L = e.cl
    L.rec("HasBeenBought", (L.lab(e.r(3)), e.r(4) >> 16))
    e.set_r(3, 1 if L.ret("HasBeenBought") else 0)


def st_afford(e):
    L = e.cl
    L.rec("CanIAfford", (L.lab(e.r(3)), e.r(4) >> 16))
    e.set_r(3, 1 if L.ret("CanIAfford") else 0)


def st_format(e):
    L = e.cl
    this, out, size, fid = e.r(3), e.r(4), e.r(5), e.r(6)
    text = "T%x" % (fid & 0xFFF)
    e.write(out, b"".join(struct.pack(">H", ord(c)) for c in text) + b"\0\0")
    # the glue saves the caller's TOC pointer where the caller reloads it from
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
    if this == TABA and fid in (0x96, 0x94):
        tag = e.r(7)
        L.rec("FormatString", (L.lab(this), size, fid, e.u32(tag), e.u32(tag + 8)) + units(e, e.u32(tag + 12)))
    else:
        L.rec("FormatString", (L.lab(this), size, fid))
    e.set_r(3, 0)


def st_sethelp(e):
    e.cl.rec("SetHelpTextString", units(e, e.r(3)))


for _a, _f in ((GETWINDOW, st_getwindow), (SHOW, st_show), (GETCLIENTRECT, st_clientrect), (MOUSEXY, st_mouse),
               (GETOBJCTRL, st_objctrl), (GETBANK, st_bank), (GETBALANCE, st_balance), (GETRIDEINFO, st_rideinfo),
               (HASBOUGHT, st_bought), (CANAFFORD, st_afford), (SETHELP, st_sethelp)):
    STUBS.internal(_a, _f)
STUBS.internal(FORMATSTRING, st_format)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({**LABELS, ADDR["ridedb"]: 8}, case["queues"])
        self.ctrl = e.alloc(64)
        self.ride = e.alloc(0x100)
        CTX["ctrl"], CTX["ride"] = self.ctrl, self.ride
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "ctrlcost":
            return self.ctrl + 4
        if name == "ride46":
            return self.ride + 0xC4
        if name == "ride33":
            return self.ride + 0x90
        return ADDR[name]

    def ptr(self, a, v):
        self.words(a, [v])

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_change(self, kind):
        return T.call_ppc(self.e, FN_CHANGE, [kind & MASK])[0] & MASK

    def call_update(self, ident, junk):
        tag = self.alloc(4)
        self.words(tag, [(ident << 16) | junk])
        return T.call_ppc(self.e, FN_UPDATE, [tag])[0] & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = {lab: addr for addr, lab in LABELS.items()}
        fixed[8] = self.g("ridedb")
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ciw_addr(name.encode())

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def alloc(self, n):
        return self.lib.tpw_ciw_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_change(self, kind):
        self.lib.tpw_ciw_change(s32(kind))
        return 0

    def call_update(self, ident, junk):
        self.lib.tpw_ciw_update(ident)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ciw_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ciw_change.restype, lib.tpw_ciw_change.argtypes = None, [C.c_int32]
    lib.tpw_ciw_update.restype, lib.tpw_ciw_update.argtypes = None, [C.c_uint32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.ptr(s.g("panel"), PANEL)
    s.ptr(s.g("world"), WORLD)
    s.ptr(s.g("text7"), TABA)
    s.ptr(s.g("text21"), TABB)
    s.words(s.g("state"), [case["state"]])
    s.half(s.g("f88"), case["f88"])
    s.words(s.g("ride46"), [case["cost"]])
    s.words(s.g("ride33"), [case["rtype"]])
    s.words(s.g("ctrlcost"), [case["upg"]])


def execute(s, case):
    if case["op"] == "change":
        return s.call_change(case["type"])
    return s.call_update(case["id"], case["junk"])


def observe(s, case, answer):
    return {"log": s.log(), "state": s.rd_words(s.g("state"), 1), "f88": s.rd_half(s.g("f88"))}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def base(r, op):
    q = {"GetWindow:%d" % i: [1] for i in WIN_IDS}
    q.update(mouseX=[0], mouseY=[0], rectL=[0], rectT=[0], rectR=[10], rectB=[10], GetBalance=[0], HasBeenBought=[0], CanIAfford=[0])
    return {"op": op, "queues": q, "state": -1, "f88": 0, "cost": 0, "rtype": 0, "upg": 0, "type": 0, "id": 1, "junk": 0}


# ---- ChangeInfoWindow
def change_case(r, kind, present=None):
    c = base(r, "change")
    c["type"] = kind
    for i in WIN_IDS:
        c["queues"]["GetWindow:%d" % i] = [1 if (present is None or i in present) and (r.random() < 0.8 or present is not None) else 0]
    if present is None and r.random() < 0.5:
        for i in WIN_IDS:
            c["queues"]["GetWindow:%d" % i] = [1]
    return c


def change_edges():
    r = random.Random(5102)
    for kind in (-5, -1, 0, 1, 2, 3, 4, 5, 0x10000, 0x7FFFFFFF, -0x80000000):
        yield change_case(r, kind, set(WIN_IDS))
        yield change_case(r, kind, set(WIN_IDS) - {0x1ED})
    for kind in (0, 1, 2, 3):
        for i in WIN_IDS:
            yield change_case(r, kind, set(WIN_IDS) - {i})
        yield change_case(r, kind, {0x1ED})


def change_random(r):
    return change_case(r, r.choice([-2, -1, 0, 1, 2, 3, 4, 5, r.randrange(-1000, 1000)]))


# ---- UpdateHelpText
def update_case(r, ident=None, inside=None):
    c = base(r, "update")
    c["id"] = r.choice([0, 1, 0x7FFF, 0xFFFF, 0xFFFE, r.randrange(0, 0x8000), r.randrange(0, 0x8000)]) if ident is None else ident
    c["junk"] = r.randrange(0, 0x10000)
    left, top = r.randrange(-50, 50), r.randrange(-50, 50)
    right, bottom = left + r.randrange(0, 100), top + r.randrange(0, 100)
    if inside is None:
        inside = r.random() < 0.8
    if inside:
        mx = r.choice([left, right, r.randrange(left, right + 1)])
        my = r.choice([top, bottom, r.randrange(top, bottom + 1)])
    else:
        mx = r.choice([left - 1, right + 1, r.randrange(-200, 200)])
        my = r.choice([top - 1, bottom + 1, r.randrange(-200, 200)])
        if r.random() < 0.5:
            mx = r.randrange(left, right + 1)
            my = r.choice([top - 1, bottom + 1, top - 100])
    q = c["queues"]
    q.update(rectL=[left], rectT=[top], rectR=[right], rectB=[bottom], mouseX=[mx], mouseY=[my])
    balance = r.choice([0, -5, 1000, r.randrange(-100000, 100000), 0x7FFFFFFF])
    q["GetBalance"] = [balance & MASK]
    c["upg"] = r.choice([balance - 1, balance, balance + 1, r.randrange(-100000, 100000), 0]) & MASK
    c["cost"] = r.choice([0, 0, 1, -1, r.randrange(-100000, 100000)]) & MASK
    c["rtype"] = r.choice([0, 1, 0x96, 0xFFF, 0x1000, r.randrange(0, 1 << 32)])
    q["HasBeenBought"] = [r.choice([0, 1])]
    q["CanIAfford"] = [r.choice([0, 1])]
    # the state the call will compute, so that a match with the globals is common
    signed = c["id"] - 0x10000 if c["id"] & 0x8000 else c["id"]
    if signed >= 0:
        cost = T.s32(c["cost"])
        if cost != 0 and not q["HasBeenBought"][0]:
            want = 0 if q["CanIAfford"][0] else 1
        else:
            want = 2 if T.s32(c["upg"]) > T.s32(balance & MASK) else 3
    else:
        want = 4
    c["state"] = r.choice([want, want, -1, 0, 1, 2, 3, 4]) & MASK
    c["f88"] = r.choice([c["id"], c["id"], r.randrange(0, 0x10000), 0])
    return c


def update_edges():
    r = random.Random(61020)
    for inside in (False, True):
        for ident in (0, 1, 0x7FFF, 0xFFFF, 0xFFFE, 5):
            for _ in range(6):
                yield update_case(r, ident, inside)
    for _ in range(40):
        yield update_case(r)


def update_random(r):
    return update_case(r)


TESTS = [
    FnTest("ChangeInfoWindow", ppc, host, edge=change_edges, rnd=change_random),
    FnTest("UpdateHelpText", ppc, host, edge=update_edges, rnd=update_random),
]
