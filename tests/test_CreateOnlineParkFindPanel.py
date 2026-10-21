"""Unit v5661, the online park find panel, against libtpw_unit_CreateOnlineParkFindPanel.dylib (tests/unit_CreateOnlineParkFindPanel.cpp), main app
("SimTheme Park"), src/game/ui/ui_onlineParkFindPanel.cpp:
  CreateOnlineParkFindPanel 0x101e9ff8, ShowFindListSearching 0x101e9f28

A case is a world and a call. The world is built on both sides: the Mac's globals in emulator memory, the host's through one setter of the
library. Every callee outside the unit (the window script builder, GetWindow, the edit box and list calls, the strings, the buttons,
operator new and the two constructors, the online system's login name, the virtual SetText, FormatString, the advisor) is a fake on both
sides that records its arguments; objects are fixed fake addresses (never dereferenced) with one label each.

The world of a case: whether the find panel and the online system already exist, and the close callback passed in. What is compared: the
event log, the online system, the find panel and the close callback globals afterwards. No fused operations. Not tested: a NULL lobby (the
original reads it unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateOnlineParkFindPanel"
MASK = 0xFFFFFFFF

FN_CREATE, FN_SHOW = 0x101E9FF8, 0x101E9F28
SIZES = {FN_CREATE: 1084, FN_SHOW: 164}
OWN = set(SIZES)

# glue and in-binary callees
BUILD, GETWINDOW, NEW, SPRITE_CTOR, SETCLIENT, GETFONT, SETFONT = 0x101DEB78, 0x101DBE40, 0x102384B8, 0x1018F1D0, 0x101DB624, 0x10180270, 0x101DA118
SETSTATIC, FINDDRAW, SETCURSOR, SYS_CTOR, LOGINNAME, PTRGLUE, CAPTURE = 0x101CD7E0, 0x101CD2D4, 0x101CDAC4, 0x1016F7D0, 0x10171834, 0x1023BAA8, 0x101DCCEC
SETFORMATS, SETCOLTYPE, SETSEL, GETSTRING, MAKEBTN, ADDITEM, FORMAT, SETTITLE, ADVADD, REMOVEALL = (
    0x101D2864, 0x101D2614, 0x101D1EB4, 0x10180468, 0x101818E4, 0x101D3000, 0x10239730, 0x10180794, 0x100A7074, 0x101D3CC8)

ADDR = dict(gLobby=0x102E4888, gLobbyScreens=0x102E49B8, gText=0x1037FF18, gOnlineSystem=0x10380AB0, panel=0x103960C8, closecb=0x103960C4,
            playername=0x103960A4, script=0x102B0C8C, msg=0x10268578, lcformat=0x10268570, lcpost=0x10268568, buf=0x10380C48,
            pool=0x1024F01E)
HOST_LAYOUT = [8]

LOBBYWIN, ADVISOR, TABLE, SYS, SPRITE, PANEL = 0x7A000100, 0x7A000200, 0x7A000300, 0x7A000400, 0x7A000500, 0x7A000600
LIST, COL, FONT, DRAW, NAME, STR = 0x7A000700, 0x7A000800, 0x7A000900, 0x7A000A00, 0x7A000B00, 0x7B000000
MARKER = 0x5E7E7E70
EDIT = None
TBL = None


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def getwindow_result(id_):
    if id_ == 0xE05A:
        return EDIT
    if id_ == 0xE05B:
        return LIST
    if 0x10 <= id_ <= 0x15:
        return COL + (id_ - 0x10) * 0x10
    return 0


def glue(fn):
    """A glue function stores the caller's TOC at 0x14(r1) before it runs; the stand-in does the same."""
    def run(e):
        e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return run


def S(addr, fn):
    STUBS.internal(addr, fn)


def simple(name, nregs_lab=0, ints=0, ret=None):
    """A fake that records its first nregs_lab register args as labels then ints more as signed numbers."""
    def run(e):
        L = e.cl
        args = [L.lab(e.r(3 + i)) for i in range(nregs_lab)] + [s32(e.r(3 + nregs_lab + i)) for i in range(ints)]
        L.rec(name, tuple(args))
        if ret is not None:
            e.set_r(3, ret(e) & MASK)
    return run


S(BUILD, simple("Build", 3, 0, lambda e: PANEL))
S(GETWINDOW, simple("GetWindow", 1, 1, lambda e: getwindow_result(e.r(4))))
S(SETCLIENT, simple("SetClientDrawObject", 2))
S(SETFONT, simple("SetFont", 2))
S(SETSTATIC, simple("SetStaticTextArea", 2, 1))
S(SETCURSOR, simple("SetCursorDrawObject", 2))
S(CAPTURE, simple("CaptureKb", 1, 1))
S(SETFORMATS, simple("SetColumnFormatsAll", 3))
S(SETCOLTYPE, simple("SetColumnType", 1, 2))
S(SETSEL, simple("SetSelectionObject", 2))
S(REMOVEALL, simple("RemoveAllItems", 1))
S(GETFONT, simple("GetFontTableEntry", 0, 1, lambda e: FONT))
S(GETSTRING, simple("GetString", 0, 1, lambda e: STR + s32(e.r(3)) * 4))
S(SETTITLE, lambda e: e.cl.rec("SetTitleText", (e.cl.lab(e.r(3)), s32(e.r(4)), e.cl.lab(e.r(5)))))
S(ADVADD, simple("AdvisorAdd", 1, 2))
S(SPRITE_CTOR, simple("SpriteCtor", 1, 1))
S(SYS_CTOR, simple("SystemCtor", 1))
S(LOGINNAME, simple("LoginName", 1, 0, lambda e: NAME))


def stub_makebtn(e):
    e.cl.rec("MAKECOLUMNBUTTON", (e.cl.lab(e.r(3)), e.r(4) & MASK) + tuple(s32(e.r(i)) for i in range(5, 10)))


S(MAKEBTN, stub_makebtn)


def stub_finddraw(e):
    e.cl.rec("FindDrawObject", tuple(e.read(e.r(3), 8)))
    e.set_r(3, DRAW)


S(FINDDRAW, stub_finddraw)


def stub_additem(e):
    L = e.cl
    w = struct.unpack(">6I", e.read(e.r(4), 24))
    L.rec("AddItem", (L.lab(e.r(3)), w[0], L.lab(w[1]), L.lab(w[2]), w[3], w[4], w[5], s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))
    e.set_r(3, 0)


S(ADDITEM, stub_additem)


def stub_new(e):
    size = e.r(3) & MASK
    e.cl.rec("new", (size,))
    e.set_r(3, SPRITE if size == 0x19C else SYS)


S(NEW, glue(stub_new))


def stub_format(e):
    L = e.cl
    L.rec("FormatString", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK, e.r(6) & MASK))
    e.set_r(3, 0)


S(FORMAT, glue(stub_format))


def stub_ptrglue(e):
    assert e.r(12) == MARKER, hex(e.r(12))
    L = e.cl
    L.rec("SetText", (L.lab(e.r(3)), L.lab(e.r(4))))


S(PTRGLUE, glue(stub_ptrglue))


def setup(runner):
    global EDIT, TBL
    T.install_std_stubs(runner.emu)
    TBL = T.persist_alloc(runner.emu, 0x40, struct.pack(">I", 0) * 11 + struct.pack(">I", MARKER) + bytes(0x10))
    EDIT = T.persist_alloc(runner.emu, 0x140, bytes(0x118) + struct.pack(">I", TBL) + bytes(0x24))


def labels():
    fixed = {LOBBYWIN: 1, ADVISOR: 2, TABLE: 3, SYS: 4, SPRITE: 5, PANEL: 6, EDIT: 7, LIST: 8, FONT: 15, DRAW: 16, NAME: 17}
    for i in range(6):
        fixed[COL + i * 0x10] = 9 + i
    return fixed


MAC_LABELS = {0x103960A4: 18, 0x102B0C8C: 19, 0x10268578: 20, 0x10268570: 21, 0x10268568: 22, 0x10380C48: 23, 0x1024F01E: 24, 0x1024F020: 25}


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = dict(labels())
        fixed.update(MAC_LABELS)
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def world(self, sys, panel, cb):
        lob = self.e.alloc(0x40)
        self.words(lob + 4, [LOBBYWIN])
        self.words(ADDR["gLobby"], [lob])
        scr = self.e.alloc(0x40)
        self.words(scr + 0x14, [ADVISOR])
        self.words(ADDR["gLobbyScreens"], [scr])
        self.words(ADDR["gText"] + 0x564, [TABLE])
        self.words(ADDR["gOnlineSystem"], [sys])
        self.words(ADDR["panel"], [panel])
        self.words(ADDR["closecb"], [cb])

    def state(self):
        return [self.e.u32(ADDR["gOnlineSystem"]), self.e.u32(ADDR["panel"]), self.e.u32(ADDR["closecb"])]

    def create(self, cb):
        T.call_ppc(self.e, FN_CREATE, [cb & MASK])

    def show(self):
        T.call_ppc(self.e, FN_SHOW, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        g = lambda n: lib.tpw_fp_addr(n.encode())
        fixed = {v: k for k, v in labels().items()}
        fixed.update({18: g("playername"), 19: g("script"), 20: g("msg"), 21: g("lcformat"), 22: g("lcpost"), 23: g("buf"),
                      24: g("pool"), 25: g("pool") + 2})
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def log(self):
        return CL.host_end(self.lib)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def world(self, sys, panel, cb):
        self.lib.tpw_fp_world(LOBBYWIN, ADVISOR, TABLE, sys, panel, cb, EDIT)

    def state(self):
        out = (C.c_uint64 * 3)()
        self.lib.tpw_fp_state(out)
        return [v & MASK for v in out]

    def create(self, cb):
        self.lib.tpw_fp_create(cb & MASK)

    def show(self):
        self.lib.tpw_fp_show()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_fp_addr.restype, lib.tpw_fp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fp_alloc.restype, lib.tpw_fp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fp_world.argtypes = [C.c_uint64] * 7
    lib.tpw_fp_state.argtypes = [C.POINTER(C.c_uint64)]
    lib.tpw_fp_create.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_fp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.world(case["sys"], case["panel"], case["cb"])
        if case["op"] == "create":
            s.create(case["arg"])
        else:
            s.show()
        return {"log": s.log(), "state": s.state()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, op, sys=None, panel=None, cb=None, arg=None):
    return {"op": op, "queues": {}, "sys": r.choice([0, SYS]) if sys is None else sys,
            "panel": r.choice([0, PANEL]) if panel is None else panel,
            "cb": r.choice([0, r.randrange(1, 1 << 32)]) if cb is None else cb,
            "arg": r.choice([0, r.randrange(1, 1 << 32)]) if arg is None else arg}


def create_edges():
    r = random.Random(5661)
    for sys in (0, SYS):
        for panel in (0, PANEL):
            for arg in (0, 0x12345678):
                yield mk(r, "create", sys, panel, 0x55, arg)


def create_random(r):
    return mk(r, "create")


def show_edges():
    r = random.Random(5662)
    for panel in (0, PANEL):
        for sys in (0, SYS):
            yield mk(r, "show", sys, panel, 0)


def show_random(r):
    return mk(r, "show")


TESTS = [
    FnTest("CreateOnlineParkFindPanel", ppc, host, edge=create_edges, rnd=create_random),
    FnTest("ShowFindListSearching", ppc, host, edge=show_edges, rnd=show_random),
]
