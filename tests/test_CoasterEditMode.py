"""Unit v5246, the coaster edit mode, against libtpw_unit_CoasterEditMode.dylib (tests/unit_CoasterEditMode.cpp), main app
("SimTheme Park"), src/ui_entryPricePanel_CoasterEditMode.cpp:
  CoasterEditMode 0x1019c584, WhichSpecialButton 0x1019c3e8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit (UI_SetSelectedObject, LLCM_SetCurrentObject, RemoveSlideoutPanel, LinkSlideoutPanel, CaptureKb, IsTrackClosed, the three
mouse mode constructors, AssignMouseMode, LLCC_SystemSetRideTrack, SetCoasterEdit, UI_SetGameWindowCursor, GetCoasterEditAvailableButtons,
GetCoasterEditDefaultButtons, WhichCheckButtonsAreDown, GetWindow, SetDrawObject, FindDrawObject) are fakes on both sides that record
their arguments. The virtual calls (the mouse mode's GetMode, slot 0x2c; the window's Enable, vtable at +0x118 slot 0x1c) go through fake
vtables on both sides and are recorded too.

The world of a case: the edit-mode flag, the edited object, the busy byte, the action recorder's recording word, the panel, the mouse
(none or one), the mouse mode (none or a fake answering a queued mode), the two mouse words, two objects with the byte at +0x4c, the
windows' two help words. What is compared: the globals, the help words of the windows 0x18 to 0x21 and the event log.

No fused operations. Not tested: a NULL object for WhichSpecialButton (the original reads the byte at +0x4c unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CoasterEditMode"
MASK = 0xFFFFFFFF

FN_CEM, FN_WSB = 0x1019C584, 0x1019C3E8
SIZES = {FN_CEM: 1308, FN_WSB: 368}
OWN = set(SIZES)

# the callees faked on both sides
UI_SETSEL, LLCM, REMOVE_SLIDE, LINK_SLIDE = 0x10194768, 0x10088590, 0x101A83D8, 0x101A82EC
CAPTURE_KB, IS_CLOSED, PATHDRAW_CT, DEFAULT_CT = 0x101DCCEC, 0x10052850, 0x1007E6E4, 0x1007EA78
COASTER_CT, ASSIGN, RIDE_TRACK, SET_EDIT = 0x1007FD14, 0x1007E2E0, 0x10091A5C, 0x10058758
SET_CURSOR, GET_AVAIL, GET_DEFAULT, WHICH_CHECK = 0x101A8C58, 0x10057278, 0x10057120, 0x1019B120
GET_WINDOW, SET_DRAW, FIND_DRAW = 0x101DBE40, 0x101DB474, 0x101CD2D4
# in-binary addresses used as the targets of the fake virtual functions (stubbed below, never run)
ENABLE_CODE, GETMODE_CODE = 0x1019C378, 0x1019C2DC

ADDR = dict(emode=0x10394830, sel=0x1039482C, busy=0x10394828, panel=0x10394834, mouse=0x102DC1F4, mmode=0x102DC1F0,
            w1e0=0x102DC1E0, w1e4=0x102DC1E4, rec=0x1037EC44 + 0x24)
WIN_IDS = list(range(0x18, 0x22))
NWIN, WSTRIDE = 0x30, 0x140
PERSIST = {}
HOST_OFFS = {}

# labels: 1 panel, 2 object A, 3 object B, 4 draw object, 5 mouse mode, 50 the mouse that exists, 0x200 + id the windows
L_PANEL, L_A, L_B, L_DO, L_MODE, L_MOUSE = 1, 2, 3, 4, 5, 50


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)

STUBS.internal(UI_SETSEL, CL.recorder("UI_SetSelectedObject", "L"))
STUBS.internal(LLCM, CL.recorder("LLCM_SetCurrentObject", "b"))
STUBS.internal(REMOVE_SLIDE, CL.recorder("RemoveSlideoutPanel", "h"))
STUBS.internal(LINK_SLIDE, CL.recorder("LinkSlideoutPanel", "Li"))
STUBS.internal(CAPTURE_KB, CL.recorder("CaptureKb", "Li"))
STUBS.internal(IS_CLOSED, CL.recorder("IsTrackClosed", "L", ret="i"))
STUBS.internal(PATHDRAW_CT, CL.recorder("CMMPathDraw", "Ni"))
STUBS.internal(DEFAULT_CT, CL.recorder("CMMDefault", "N"))
STUBS.internal(COASTER_CT, CL.recorder("CMMCoaster", "N"))
STUBS.internal(ASSIGN, CL.recorder("AssignMouseMode", "LL"))
STUBS.internal(RIDE_TRACK, CL.recorder("LLCC_SystemSetRideTrack", "i"))
STUBS.internal(SET_EDIT, CL.recorder("SetCoasterEdit", "Li"))
STUBS.internal(SET_CURSOR, CL.recorder("UI_SetGameWindowCursor", "h"))
STUBS.internal(WHICH_CHECK, CL.recorder("WhichCheckButtonsAreDown", "u"))
STUBS.internal(SET_DRAW, CL.recorder("SetDrawObject", "LL"))
STUBS.internal(ENABLE_CODE, CL.recorder("Enable", "Lh"))
STUBS.internal(GETMODE_CODE, CL.recorder("GetMode", "L", ret="i"))
STUBS.internal(FIND_DRAW, CL.recorder("FindDrawObject", "s", ret=lambda e, log: e.cl.dobj))


def stub_buttons(name):
    def stub(e):
        L = e.cl
        L.rec(name, ())
        e.write(e.r(3), struct.pack(">I", L.ret(name) & MASK))
    return stub


def stub_get_window(e):
    L = e.cl
    wid = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), wid))
    e.set_r(3, (L.winbase + wid * WSTRIDE) & MASK)


STUBS.internal(GET_AVAIL, stub_buttons("GetCoasterEditAvailableButtons"))
STUBS.internal(GET_DEFAULT, stub_buttons("GetCoasterEditDefaultButtons"))
STUBS.internal(GET_WINDOW, stub_get_window)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    d_enable = T.persist_alloc(e, 8, struct.pack(">II", ENABLE_CODE, 0))
    d_mode = T.persist_alloc(e, 8, struct.pack(">II", GETMODE_CODE, 0))
    wvt, mvt = bytearray(0x40), bytearray(0x40)
    struct.pack_into(">I", wvt, 0x1C, d_enable)
    struct.pack_into(">I", mvt, 0x2C, d_mode)
    PERSIST["wvt"] = T.persist_alloc(e, 0x40, bytes(wvt))
    PERSIST["mvt"] = T.persist_alloc(e, 0x40, bytes(mvt))


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def setup_objects(self):
        self.objs = {None: 0, "A": self.A, "B": self.B}


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    help_off, f90_off = 0x8C, 0x90

    def __init__(self, e, case):
        self.e = e
        self.panel, self.A, self.B = e.alloc(0x40), e.alloc(0x60), e.alloc(0x60)
        self.dobj, self.mode, self.mouse0 = e.alloc(0x10), e.alloc(0x10), e.alloc(0x10)
        self.winbase = e.alloc(NWIN * WSTRIDE)
        fixed = {self.panel: L_PANEL, self.A: L_A, self.B: L_B, self.dobj: L_DO, self.mode: L_MODE, self.mouse0: L_MOUSE}
        for i in WIN_IDS:
            fixed[self.winbase + i * WSTRIDE] = i - 0x18 + 10
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.L.dobj, self.L.winbase = self.dobj, self.winbase
        self.saved = STUBS.activate(e)
        self.setup_objects()

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def win(self, i):
        return self.winbase + i * WSTRIDE

    def init_win(self, i):
        self.words(self.win(i) + 0x118, [PERSIST["wvt"]])

    def init_mode(self):
        self.words(self.mode, [PERSIST["mvt"]])

    def label(self, v):
        return self.L.lab(v)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, obj):
        if op == "cem":
            T.call_ppc(self.e, FN_CEM, [obj & MASK])
        else:
            T.call_ppc(self.e, FN_WSB, [])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_cem_reset()
        self.help_off, self.f90_off = HOST_OFFS["help"], HOST_OFFS["f90"]
        a = lib.tpw_cem_alloc
        self.panel, self.A, self.B, self.mouse0 = a(0x40), a(0x60), a(0x60), a(0x10)
        self.mode, self.dobj = lib.tpw_cem_mode(), lib.tpw_cem_drawobj()
        fixed = {L_PANEL: self.panel, L_A: self.A, L_B: self.B, L_DO: self.dobj, L_MODE: self.mode, L_MOUSE: self.mouse0}
        for i in WIN_IDS:
            fixed[i - 0x18 + 10] = lib.tpw_cem_win(i)
        CL.host_begin(lib, fixed, case["queues"])
        self.setup_objects()

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cem_addr(name.encode())

    def win(self, i):
        return self.lib.tpw_cem_win(i)

    def init_win(self, i):
        pass

    def init_mode(self):
        pass

    def label(self, v):
        return self.lib.tpw_cem_label(v)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, obj):
        if op == "cem":
            self.lib.tpw_cem_CoasterEditMode(obj)
        else:
            self.lib.tpw_cem_WhichSpecialButton()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cem_addr.restype, lib.tpw_cem_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cem_alloc.restype, lib.tpw_cem_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cem_reset.argtypes = []
    lib.tpw_cem_win.restype, lib.tpw_cem_win.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_cem_mode.restype, lib.tpw_cem_mode.argtypes = C.c_uint64, []
    lib.tpw_cem_drawobj.restype, lib.tpw_cem_drawobj.argtypes = C.c_uint64, []
    lib.tpw_cem_label.restype, lib.tpw_cem_label.argtypes = C.c_int64, [C.c_uint64]
    lib.tpw_cem_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cem_CoasterEditMode.argtypes = [C.c_uint64]
    lib.tpw_cem_WhichSpecialButton.argtypes = []
    out = (C.c_int64 * 8)()
    n = lib.tpw_cem_layout(out)
    HOST_OFFS["help"], HOST_OFFS["f90"], HOST_OFFS["size"] = list(out)[:3]
    assert n == 3 and HOST_OFFS["size"] <= 0x400, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("emode"), [case["emode"]])
    s.write(s.g("busy"), bytes([case["busy"]]))
    s.ptr(s.g("sel"), s.objs[case["sel"]])
    s.ptr(s.g("panel"), s.panel)
    s.ptr(s.g("mouse"), s.mouse0 if case["mouse"] else 0)
    s.init_mode()
    s.ptr(s.g("mmode"), s.mode if case["has_mode"] else 0)
    s.words(s.g("w1e0"), [case["w1e0"]])
    s.words(s.g("w1e4"), [case["w1e4"]])
    s.words(s.g("rec"), [case["rec"]])
    s.write(s.A + 0x4C, bytes([case["bA"]]))
    s.write(s.B + 0x4C, bytes([case["bB"]]))
    for i in WIN_IDS:
        s.init_win(i)
        s.words(s.win(i) + s.help_off, [case["help"] + i])
        s.words(s.win(i) + s.f90_off, [case["f90"] + i])


def observe(s, case):
    d = {"log": s.log(),
         "emode": s.rd_word(s.g("emode")),
         "busy": s.read(s.g("busy"), 1),
         "sel": s.label(s.rd_ptr(s.g("sel"))),
         "mouse": s.label(s.rd_ptr(s.g("mouse"))),
         "w1e0": s.rd_word(s.g("w1e0")),
         "w1e4": s.rd_word(s.g("w1e4")),
         "rec": s.rd_word(s.g("rec"))}
    for i in WIN_IDS:
        d["help%d" % i] = s.rd_word(s.win(i) + s.help_off)
        d["f90%d" % i] = s.rd_word(s.win(i) + s.f90_off)
    return d


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"], s.objs[case["arg"]])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    sel = r.choice(["A", "B"]) if op == "wsb" else r.choice([None, "A", "B"])
    return {"op": op, "emode": r.choice([0, 1, 2]), "busy": r.choice([0, 0, 1, 0x80]), "sel": sel,
            "arg": r.choice(["A", "B"]), "rec": r.choice([0, 0, 1, 7]), "mouse": r.choice([0, 1]), "has_mode": r.choice([0, 1]),
            "w1e0": r.getrandbits(32), "w1e4": r.getrandbits(32), "bA": r.choice([0, 1, 2, 0xFF]), "bB": r.choice([0, 1, 3]),
            "help": r.getrandbits(20), "f90": r.getrandbits(20),
            "queues": {"IsTrackClosed": [r.choice([0, 1])], "GetMode": [r.choice([8, 8, 3, 1, 0])],
                       "GetCoasterEditAvailableButtons": [r.getrandbits(10), r.getrandbits(10)],
                       "GetCoasterEditDefaultButtons": [r.getrandbits(32)]}}


def cem_case(r, **kw):
    c = base(r, "cem")
    c.update(kw)
    return c


def cem_edges():
    r = random.Random(5246)
    for emode in (0, 1):
        for same in (0, 1):
            for busy in (0, 1):
                for rec in (0, 1):
                    for closed in (0, 1):
                        for mouse in (0, 1):
                            for mode in (None, 8, 3):
                                c = cem_case(r, emode=emode, busy=busy, rec=rec, mouse=mouse, has_mode=int(mode is not None),
                                             sel="A" if same else r.choice([None, "B"]), arg="A")
                                c["queues"]["IsTrackClosed"] = [closed]
                                c["queues"]["GetMode"] = [mode or 0]
                                yield c
    for bit in range(10):
        for bA in (0, 1):
            c = cem_case(r, emode=0, busy=0, sel=None, arg="A", bA=bA)
            c["queues"]["GetCoasterEditAvailableButtons"] = [1 << bit, 1 << bit]
            yield c
    c = cem_case(r, emode=0, busy=0, sel=None, arg="B", bB=1)
    c["queues"]["GetCoasterEditAvailableButtons"] = [0x3FF, 0x3FF]
    yield c


def cem_random(r):
    return cem_case(r)


def wsb_case(r, **kw):
    c = base(r, "wsb")
    c.update(kw)
    return c


def wsb_edges():
    r = random.Random(1019)
    for byte in (0, 1, 2, 0xFF):
        for buttons in (0, 2, 0x200, 0x202, 0x3FF, 0x1FD):
            c = wsb_case(r, sel="A", bA=byte)
            c["queues"]["GetCoasterEditAvailableButtons"] = [buttons]
            yield c


def wsb_random(r):
    return wsb_case(r)


TESTS = [
    FnTest("CoasterEditMode", ppc, host, edge=cem_edges, rnd=cem_random),
    FnTest("WhichSpecialButton", ppc, host, edge=wsb_edges, rnd=wsb_random),
]
