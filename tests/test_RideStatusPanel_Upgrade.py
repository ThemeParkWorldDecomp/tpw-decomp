"""Unit v5914, the ride status panel's knobs and buttons, against libtpw_unit_RideStatusPanel_Upgrade.dylib
(tests/unit_RideStatusPanel_Upgrade.cpp), main app ("SimTheme Park"),
src/LCPostFormat_RideStatusPanel_UpdateStatusWindow_RideStatusPanel_Upgrade.cpp:
  RideStatusPanel::Upgrade 0x101bbb88, ApplyChanges 0x101bbacc, DoDial 0x101bb9b8, Edit 0x101bb71c, Show 0x101bc0bc,
  UpdateKnobsWindow 0x101bb494

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the bank account, the object's record and set calls, the controls, the mouse and its modes, the ride database, the window calls,
the ops table and the window virtuals reached through __ptr_glue) are fakes on both sides that record their arguments; the pointers the
fakes answer come through the case's queues.

The world of a case: the thing global, the world, gMouse and its two words, the panel (window, ops, figures), the object's fields, the ride
records' slots, the control's upgrade cost, the window tree (children as words at +0x10 of each window block), the balance. What is compared:
the answer, the panel's figures, gMouse (none, the old one or a new one), the two mouse words, gRideStatusShowThing, the event log.

No fused operations. Not tested: a bank balance with a division by zero in UpdateKnobsWindow (the speed bar's high equal to its low: divw by 0
is undefined on the hardware), a missing child the original dereferences, a flag byte other than 0 and 1 for Upgrade (the host takes a bool).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RideStatusPanel_Upgrade"
MASK = 0xFFFFFFFF

FN_UPGRADE, FN_APPLY, FN_DODIAL, FN_EDIT, FN_SHOW, FN_KNOBS = 0x101BBB88, 0x101BBACC, 0x101BB9B8, 0x101BB71C, 0x101BC0BC, 0x101BB494
SIZES = {FN_UPGRADE: 448, FN_APPLY: 136, FN_DODIAL: 228, FN_EDIT: 624, FN_SHOW: 176, FN_KNOBS: 588}
OWN = set(SIZES)

# ---- the callees faked on both sides
A_GETBANK, A_GETBAL, A_REQUP, A_GETDB = 0x1013EDFC, 0x100F32B4, 0x1010C214, 0x10107CD4
A_GETCTRL, A_PATH2, A_PATH1, A_DEFAULT, A_ASSIGN = 0x100FA0B8, 0x1007E744, 0x1007E6E4, 0x1007EA78, 0x1007E2E0
A_SELECT, A_CURRENT, A_NODOUGH = 0x10194768, 0x10088590, 0x10182160
A_SETSPEED, A_SETCAP, A_SETDUR = 0x10108068, 0x10108248, 0x10108104
A_GETRIDE, A_COASTER, A_SETTRACK, A_LEFTUP = 0x10154868, 0x1019C584, 0x10091A5C, 0x10084CA4
A_OPEN, A_ADVISOR, A_GETWIN, A_MINMAX, A_SHOW, A_TITLES, A_GLUE = (0x101935A0, 0x101821CC, 0x101DBE40, 0x101D6B44, 0x101DC1EC, 0x101BAF28,
                                                                  0x1023BAA8)

# ---- the Mac's addresses of the globals
ADDR = dict(thing=0x103947A4, world=0x1037EBFC, mouse=0x102DC1F4, w1e0=0x102DC1E0, w1e4=0x102DC1E4, show=0x1039599C)
SCRIPT_ADDR, CALLBACK_TV = 0x102ACEF0, 0x10267D98

# ---- the Mac's offsets; the host's come from the library (HOST_LAYOUT is the check of them)
MAC_PANEL = dict(win=0x0C, ops=0x20, upg=0x24, f28=0x28, spd=0x2C, cap=0x30, dur=0x34)
MAC_THING = dict(type=0x0E, cond=0x40, lvl=0x4C, spd=0x54, dur=0x58, cap=0x59)
# panel (window, ops, upgrade window, f28, speed, capacity, duration, size), object (type, condition, level, speed, duration, capacity, size),
# ride record (size, slot 0's offset, step), control (upgrade cost, size)
HOST_LAYOUT = [12, 32, 36, 40, 44, 48, 52, 56, 14, 64, 76, 84, 88, 89, 1096, 2824, 24, 8, 4, 32]

# block sizes (Mac, host fills in from the layout)
MARK_SLOT5, MARK_SLOT11, MARK_SETVALUE = 0x1C, 0x34, 0x24
KID_SLOT = {0x3E2D: 0, 0x3E2E: 1, 0x3E2F: 2, 0x3E30: 3, 0x3E31: 4}
WINDOWS = ["wpanel", "wbar", "wdur", "wcap", "c1", "c2", "wk"]
LABELS = dict(panel=1, thing=2, world=3, acct=4, wpanel=10, wbar=11, wdur=12, wcap=13, c1=14, c2=15, wk=16)

BLK = {}   # the PowerPC side's persistent blocks


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_getbank(e):
    L = e.cl
    L.rec("GetBankAccount", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetBankAccount") & MASK)


def stub_getbal(e):
    L = e.cl
    L.rec("GetBalance", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetBalance") & MASK)


def stub_getdb(e):
    L = e.cl
    L.rec("GetDBInfo", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetDBInfo") & MASK)


def stub_getctrl(e):
    L = e.cl
    L.rec("GetObjectControl", (s32(e.r(3) - BLK["world"]), (e.r(4) >> 16) & 0xFFFF, s32(e.r(5))))
    e.set_r(3, L.ret("GetObjectControl") & MASK)


def stub_getride(e):
    L = e.cl
    L.rec("GetRideInfo", ((e.r(4) >> 16) & 0xFFFF,))
    e.set_r(3, L.ret("GetRideInfo") & MASK)


def stub_getwin(e):
    L = e.cl
    this, wid = e.r(3), s32(e.r(4))
    L.rec("GetWindow", (L.lab(this), wid))
    e.set_r(3, e.u32(this + 0x10 + 4 * KID_SLOT[wid]) if wid in KID_SLOT else 0)


def stub_open(e):
    L = e.cl
    L.rec("OpenWindow", (L.lab(e.r(3)), int(e.r(4) == SCRIPT_ADDR), int(e.r(5) == CALLBACK_TV), s32(e.r(6)), s32(e.r(7)), s32(e.r(8))))


def stub_glue(e):
    L = e.cl
    mark = e.u32(e.r(12))
    if mark == MARK_SETVALUE:
        L.rec("SetValue", (L.lab(e.r(3)), s32(e.r(4))))
    elif mark == MARK_SLOT5:
        L.rec("Slot5", (L.lab(e.r(3)),))
    elif mark == MARK_SLOT11:
        L.rec("Slot11", (L.lab(e.r(3)),))
    else:
        raise AssertionError("unknown glue call %x" % mark)


STUBS.internal(A_GETBANK, stub_getbank)
STUBS.internal(A_GETBAL, stub_getbal)
STUBS.internal(A_GETDB, stub_getdb)
STUBS.internal(A_GETCTRL, stub_getctrl)
STUBS.internal(A_GETRIDE, stub_getride)
STUBS.internal(A_GETWIN, stub_getwin)
STUBS.internal(A_OPEN, stub_open)
STUBS.internal(A_GLUE, stub_glue)
STUBS.internal(A_REQUP, CL.recorder("RequestUpgrade", "Li"))
STUBS.internal(A_PATH1, CL.recorder("CMMPathDraw1", "Ni"))
STUBS.internal(A_PATH2, CL.recorder("CMMPathDraw2", "Niw"))
STUBS.internal(A_DEFAULT, CL.recorder("CMMDefault", "N"))
STUBS.internal(A_ASSIGN, CL.recorder("AssignMouseMode", "LL"))
STUBS.internal(A_SELECT, CL.recorder("UI_SetSelectedObject", "L"))
STUBS.internal(A_CURRENT, CL.recorder("LLCM_SetCurrentObject", "b"))
STUBS.internal(A_NODOUGH, CL.recorder("UIMain_NotEnoughDough", ""))
STUBS.internal(A_SETSPEED, CL.recorder("SetOperatingSpeed", "Lu"))
STUBS.internal(A_SETCAP, CL.recorder("SetOperatingCapacity", "Lb"))
STUBS.internal(A_SETDUR, CL.recorder("SetOperatingDuration", "Lb"))
STUBS.internal(A_COASTER, CL.recorder("CoasterEditMode", "L"))
STUBS.internal(A_SETTRACK, CL.recorder("LLCC_SystemSetRideTrack", "i"))
STUBS.internal(A_LEFTUP, CL.recorder("LLCM_LeftMouseButtonUp", "iiHi"))
STUBS.internal(A_ADVISOR, CL.recorder("UIMain_AdvisorMessage", "i"))
STUBS.internal(A_MINMAX, CL.recorder("SetMinMax", "Lii"))
STUBS.internal(A_SHOW, CL.recorder("Show", "Lh"))
STUBS.internal(A_TITLES, CL.recorder("UpdateKnobsTitles", "Li"))


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    BLK["tvs"] = T.persist_alloc(e, 0x20, struct.pack(">IIIIII", MARK_SLOT5, 0, MARK_SLOT11, 0, MARK_SETVALUE, 0))
    BLK["ops"] = T.persist_alloc(e, 0x40)
    BLK["vt"] = T.persist_alloc(e, 0x40)
    for name, size in MAC_SIZES.items():
        BLK[name] = T.persist_alloc(e, size)


MAC_SIZES = dict(panel=0x80, thing=0x100, world=0x20, acct=0x10, info=0x300, info2=0x300, ctrl=0x20, mouse0=0x10)
for _w in WINDOWS:
    MAC_SIZES[_w] = 0x130


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def wr(self, a, n, v):
        self.write(a, struct.pack(self.fmt[0] + {1: "B", 2: "H", 4: "I", 8: "Q"}[n], v & ((1 << (8 * n)) - 1)))

    def rd(self, a, n):
        return struct.unpack(self.fmt[0] + {1: "B", 2: "H", 4: "I", 8: "Q"}[n], self.read(a, n))[0]

    def wptr(self, a, v):
        self.wr(a, self.PW, v)

    def wp32(self, a, v):
        """A 32-bit pointer field of the panel (the Mac's address; the host's arena offset, unit v5L31)."""
        self.wr(a, 4, v)

    def rptr(self, a):
        return self.rd(a, self.PW)

    def prepare(self, case):
        """Fills in the queues with the blocks' addresses (blocks exist before the log starts)."""
        q = case["queues"]
        B = self.B
        q["GetBankAccount"] = [B["acct"]]
        q["GetDBInfo"] = [B["info"]]
        q["GetRideInfo"] = [B["info2"]]
        q["GetObjectControl"] = [B["ctrl"]]


class Ppc(Side):
    fmt = ">I"
    PW = 4
    mac = True

    def __init__(self, e, case):
        self.e = e
        self.B = BLK
        self.pf, self.tf = MAC_PANEL, MAC_THING
        self.prepare(case)
        fixed = {BLK[k]: v for k, v in LABELS.items()}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

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

    def slot_off(self, slot):
        return 0xC + 4 * slot

    def size(self, name):
        return MAC_SIZES[name]

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    PW = 8
    mac = False

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lay = lib.layout
        self.pf = dict(zip(["win", "ops", "upg", "f28", "spd", "cap", "dur"], lay[0:7]))
        self.psize = lay[7]
        self.tf = dict(zip(["type", "cond", "lvl", "spd", "dur", "cap"], lay[8:14]))
        self.tsize, self.rsize, self.rv0, self.rvstep, self.ccost, self.csize = lay[14], lay[15], lay[16], lay[17], lay[18], lay[19]
        sizes = dict(panel=self.psize, thing=self.tsize, world=0x20, acct=0x10, info=self.rsize, info2=self.rsize, ctrl=self.csize, mouse0=0x10)
        for w in WINDOWS:
            sizes[w] = 0x130
        self.sizes = sizes
        if not hasattr(lib, "blocks"):   # allocated once: the arena is small and the blocks' addresses must fit 32 bits
            lib.blocks = {k: lib.tpw_rsp_alloc(v) for k, v in sizes.items()}
        self.B = lib.blocks
        self.prepare(case)
        CL.host_begin(lib, {v: self.B[k] for k, v in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rsp_addr(name.encode())

    def wp32(self, a, v):
        self.wr(a, 4, self.lib.tpw_rsp_off(v))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def slot_off(self, slot):
        return self.rv0 + self.rvstep * slot

    def size(self, name):
        return self.sizes[name]

    def call(self, fn, args):
        f = {FN_UPGRADE: self.lib.tpw_rsp_upgrade, FN_APPLY: self.lib.tpw_rsp_apply, FN_DODIAL: self.lib.tpw_rsp_dodial,
             FN_EDIT: self.lib.tpw_rsp_edit, FN_SHOW: self.lib.tpw_rsp_show, FN_KNOBS: self.lib.tpw_rsp_knobs}[fn]
        r = f(*[a & 0xFFFFFFFFFFFFFFFF if i == 0 or (fn == FN_SHOW and i == 1) else s32(a) for i, a in enumerate(args)])
        return (r or 0) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rsp_addr.restype, lib.tpw_rsp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rsp_alloc.restype, lib.tpw_rsp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rsp_off.restype, lib.tpw_rsp_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_rsp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rsp_upgrade.restype, lib.tpw_rsp_upgrade.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32]
    lib.tpw_rsp_apply.restype, lib.tpw_rsp_apply.argtypes = None, [C.c_uint64]
    lib.tpw_rsp_dodial.restype, lib.tpw_rsp_dodial.argtypes = None, [C.c_uint64, C.c_int32, C.c_int32]
    lib.tpw_rsp_edit.restype, lib.tpw_rsp_edit.argtypes = None, [C.c_uint64, C.c_int32]
    lib.tpw_rsp_show.restype, lib.tpw_rsp_show.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_rsp_knobs.restype, lib.tpw_rsp_knobs.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rsp_layout(out)
    lib.layout = list(out)[:n]
    assert lib.layout == HOST_LAYOUT, lib.layout
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    B = s.B
    for name in list(MAC_SIZES):
        s.write(B[name], bytes(s.size(name)))
    s.wptr(s.g("thing"), B["thing"])
    s.wptr(s.g("world"), B["world"])
    s.wptr(s.g("mouse"), B["mouse0"] if case["mouse"] else 0)
    s.wr(s.g("w1e0"), 4, case["w1e0"])
    s.wr(s.g("w1e4"), 4, case["w1e4"])
    s.wptr(s.g("show"), 0)
    # the panel
    p, pf = B["panel"], s.pf
    s.wp32(p + pf["win"], B["wpanel"] if case["mwin"] else 0)
    s.wp32(p + pf["ops"], BLK["ops"] if s.mac else s.g("ops"))
    s.wr(p + pf["upg"], 4, case["upg"])   # only tested for NULL (a Ptr32 on the host)
    s.wr(p + pf["f28"], 4, case["f28"])
    s.wr(p + pf["spd"], 4, case["pspd"])
    s.wr(p + pf["cap"], 4, case["pcap"])
    s.wr(p + pf["dur"], 4, case["pdur"])
    # the object
    t, tf = B["thing"], s.tf
    s.wr(t + tf["type"], 2, case["type"])
    s.wr(t + tf["cond"], 4, case["cond"])
    s.wr(t + tf["lvl"], 1, case["lvl"])
    s.wr(t + tf["spd"], 4, case["tspd"])
    s.wr(t + tf["dur"], 1, case["tdur"])
    s.wr(t + tf["cap"], 1, case["tcap"])
    # the ride records
    for slot, v in case["info"].items():
        s.wr(B["info"] + s.slot_off(slot), 4, v)
    s.wr(B["info2"] + s.slot_off(36), 4, case["track"])
    s.wr(B["ctrl"] + (4 if s.mac else s.ccost), 4, case["cost"])
    # the window tree: children words at +0x10 + 4 * k
    kids = case["kids"]

    def child(parent, wid, kid):
        s.wptr(B[parent] + 0x10 + s.PW * KID_SLOT[wid], B[kid])

    if kids["bar"]:
        child("wpanel", 0x3E30, "wbar")
        child("wbar", 0x3E2E, "c1")
    if kids["dur"]:
        child("wpanel", 0x3E2F, "wdur")
    if kids["cap"]:
        child("wpanel", 0x3E2D, "wcap")
        child("wcap", 0x3E2E, "c2")
    child("wpanel", 0x3E31, "wk")
    if s.mac:   # the windows' virtual tables and the ops table go through __ptr_glue
        tvs = BLK["tvs"]
        s.wr(BLK["vt"] + 0x24, 4, tvs + 16)
        s.wr(BLK["ops"] + 0x1C, 4, tvs)
        s.wr(BLK["ops"] + 0x34, 4, tvs + 8)
        for w in WINDOWS:
            s.wr(B[w] + 0x118, 4, BLK["vt"])


def pmap(s, v, case):
    """A pointer as 0, the thing, or something else."""
    return 0 if v == 0 else 1 if v == s.B["thing"] else 2


def observe(s, case, answer):
    B, pf = s.B, s.pf
    p = B["panel"]
    mouse = s.rptr(s.g("mouse"))
    return {"answer": answer, "log": s.log(),
            "f28": s.rd(p + pf["f28"], 4), "spd": s.rd(p + pf["spd"], 4), "cap": s.rd(p + pf["cap"], 4), "dur": s.rd(p + pf["dur"], 4),
            "upg": 1 if s.rd(p + pf["upg"], 4) else 0,
            "mouse": 0 if mouse == 0 else 1 if mouse == B["mouse0"] else 2,
            "w1e0": s.rd(s.g("w1e0"), 4), "w1e4": s.rd(s.g("w1e4"), 4),
            "show": pmap(s, s.rptr(s.g("show")), case)}


def execute(s, case):
    B = s.B
    op, a = case["op"], case["args"]
    panel = B["panel"]
    if op == "upgrade":
        return s.call(FN_UPGRADE, [panel, a[0] & MASK, a[1]])
    # the others answer nothing (r3 is whatever the last callee left)
    if op == "apply":
        s.call(FN_APPLY, [panel])
    elif op == "dodial":
        s.call(FN_DODIAL, [panel, a[0] & MASK, a[1] & MASK])
    elif op == "edit":
        s.call(FN_EDIT, [panel, a[0] & MASK])
    elif op == "show":
        s.call(FN_SHOW, [panel, B["thing"]])
    else:
        s.call(FN_KNOBS, [panel])
    return 0


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


def ri(r, a, b):
    return r.randint(a, b)


def base(r, op):
    low = ri(r, -50, 50)
    info = {25: r.choice([0, 0, 1, ri(r, -5, 9)]),
            74: low, 75: low + r.choice([ri(r, 1, 100), ri(r, 1, 100), -ri(r, 1, 60)])}
    cl = ri(r, -40, 40)
    info[70] = cl
    info[71] = cl if r.random() < 0.35 else cl + ri(r, -30, 60) or cl + 1
    info[72], info[73] = ri(r, -9, 30), ri(r, 31, 90)
    for k in range(4):
        info[100 + 16 * k] = ri(r, -300, 300)
        info[104 + 16 * k] = ri(r, -300, 300)
        info[107 + 16 * k] = r.choice([ri(r, 0, 5000), ri(r, -50, 50)])
    cost = r.choice([ri(r, 0, 5000), ri(r, -50, 50)])
    return {"op": op, "queues": {"GetBalance": [cost + r.choice([-1, 0, 1, ri(r, -2000, 2000), 0])]},
            "mouse": r.choice([0, 1]), "w1e0": r.getrandbits(32), "w1e4": r.getrandbits(32), "mwin": 1 if r.random() < 0.9 else 0,
            "upg": r.choice([0, 0, 1]), "f28": r.getrandbits(32), "pspd": ri(r, -20, 300), "pcap": ri(r, -20, 300), "pdur": ri(r, -20, 300),
            "type": r.getrandbits(16), "cond": fb(r.choice([0.0, 0.5, 1.0, 20.0, 20.9, 21.0, 100.0, 256.0, 257.5, -1.5, r.uniform(-5, 40),
                                                            r.uniform(-1000, 1000)])),
            "lvl": ri(r, 0, 3), "tspd": r.getrandbits(32), "tdur": ri(r, 0, 255), "tcap": ri(r, 0, 255),
            "info": info, "track": r.choice([-1, 0, 1, 2, 3, 4, 5, ri(r, -9, 9)]), "cost": cost,
            "kids": {"bar": r.random() < 0.8, "dur": r.random() < 0.8, "cap": r.random() < 0.8}, "args": []}


def case_for(op, r, args=None, **over):
    c = base(r, op)
    if args is None:
        if op == "upgrade":
            args = [r.choice([-1, -2, -3, 0, 1, 2, 0x55, 0xFFFF, 0x12345, ri(r, -3, 200)]), r.choice([0, 0, 1])]
        elif op == "dodial":
            args = [r.choice([0x3E2C, 0x3E2D, 0x3E2E, 0x3E2F, 0x3E30, 0x3E31, 0x3E32, 0, -1, ri(r, 0, 0x4000)]), ri(r, -500, 500)]
        elif op == "edit":
            args = [r.choice([0, 1, 2, -1, ri(r, -5, 5)])]
        else:
            args = []
    c["args"] = args
    c.update(over)
    return c


def maker(op):
    def rnd(r):
        return case_for(op, r)
    return rnd


def edges_upgrade():
    r = random.Random(5914)
    for what in (-1, -2, -3, 0, 1, 5, 0x3E8, 0xFFFF, 0x10000, 0x12345):
        for flag in (0, 1):
            for dbal in (-1, 0, 1):
                c = case_for("upgrade", r, [what, flag])
                cost = c["info"][107 + 16 * (-what)] if what < 0 else c["cost"]
                c["queues"]["GetBalance"] = [cost + dbal]
                yield c
    yield case_for("upgrade", r, [-1, 0], queues={"GetBalance": [-5]})


def edges_apply():
    r = random.Random(5915)
    for unit in (0, 1, -1, 7):
        c = case_for("apply", r)
        c["info"][25] = unit
        yield c


def edges_dodial():
    r = random.Random(5916)
    for a in (0x3E2C, 0x3E2D, 0x3E2E, 0x3E2F, 0x3E30, 0x3E31, 0x3E32, 0, -1, 0x7FFFFFFF, -0x80000000):
        for b in (0, -1, 77, 0x7FFFFFFF):
            yield case_for("dodial", r, [a, b])


def edges_edit():
    r = random.Random(5917)
    for on in (0, 1, -1, 2):
        for track in (-1, 0, 1, 2, 3, 4, 5, 100):
            for mouse in (0, 1):
                c = case_for("edit", r, [on])
                c["track"], c["mouse"] = track, mouse
                yield c


def edges_show():
    r = random.Random(5918)
    for cond in (0.0, 0.5, 0.99, 1.0, 1.5, 19.99, 20.0, 20.5, 21.0, 21.5, 255.0, 256.0, 257.0, 277.0, 278.0, -0.5, -1.0, -235.0, -236.0, 300.0, 1.0e9):
        c = case_for("show", r)
        c["cond"] = fb(cond)
        yield c


def edges_knobs():
    r = random.Random(5919)
    for mwin in (0, 1):
        for bar in (0, 1):
            for dur in (0, 1):
                for cap in (0, 1):
                    for same in (0, 1):
                        for unit in (0, 3):
                            c = case_for("knobs", r)
                            c["mwin"] = mwin
                            c["kids"] = {"bar": bool(bar), "dur": bool(dur), "cap": bool(cap)}
                            if same:
                                c["info"][71] = c["info"][70]
                            elif c["info"][71] == c["info"][70]:
                                c["info"][71] += 1
                            c["info"][25] = unit
                            if c["info"][74] == c["info"][75]:
                                c["info"][75] += 1
                            yield c
    for lvl in range(4):
        c = case_for("knobs", r)
        c["lvl"] = lvl
        if c["info"][74] == c["info"][75]:
            c["info"][75] += 1
        if c["info"][70] == c["info"][71]:
            c["info"][71] += 1
        yield c


def _fix(gen):
    """Random knobs cases must not divide by zero (the speed bar's range is never empty)."""
    def rnd(r):
        c = gen(r)
        if c["info"][74] == c["info"][75]:
            c["info"][75] += 1
        return c
    return rnd


TESTS = [
    FnTest("RideStatusPanel::Upgrade", ppc, host, edge=edges_upgrade, rnd=maker("upgrade")),
    FnTest("RideStatusPanel::ApplyChanges", ppc, host, edge=edges_apply, rnd=maker("apply")),
    FnTest("RideStatusPanel::DoDial", ppc, host, edge=edges_dodial, rnd=maker("dodial")),
    FnTest("RideStatusPanel::Edit", ppc, host, edge=edges_edit, rnd=maker("edit")),
    FnTest("RideStatusPanel::Show", ppc, host, edge=edges_show, rnd=maker("show")),
    FnTest("RideStatusPanel::UpdateKnobsWindow", ppc, host, edge=edges_knobs, rnd=_fix(maker("knobs"))),
]
