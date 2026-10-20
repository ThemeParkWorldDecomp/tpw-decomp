"""The online park find panel's message function and the online panel builder (unit v5541), against libtpw_unit_OnlineParkFindPanelMsg.dylib
(tests/unit_OnlineParkFindPanelMsg.cpp), main app ("SimTheme Park"), src/OnlineParkFindPanelMsg.cpp:
  OnlineParkFindPanelMsg 0x101e93b0, CreateOnlinePanel 0x101e8f08

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit (the window methods, the list and edit box methods, the constructors, FindDrawObject, GetFontTableEntry, UDebug, the park list, the
other panel functions and the virtual SetText of the edit window) is a fake on both sides that records its arguments and answers from the
case's queues (Build, CreateObject, GetState, GetData, CallDefault). Pointers are logged as labels: 1 to 39 are the 39 window objects, 60 to
62 the online system, its park list and the lobby, 70 and on the scripts, texts and callbacks.

The world of a case: the globals the functions read (root window, online system, lobby, close callback, float constants, font index) and the
globals they write (given random old values). What is compared: the answer, every global written (pointers as labels), the event log.

No fused operations. Not tested: a NULL lobby window (the original calls CaptureKb on it through the fake; the host cannot call a method on
NULL), GetWindow answering NULL where the original uses the result unconditionally (SelectAll, SetCallbackFunction, Show), a NULL root window.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_OnlineParkFindPanelMsg"
MASK = 0xFFFFFFFF

FN_MSG, FN_CREATE = 0x101E93B0, 0x101E8F08
SIZES = {FN_MSG: 700, FN_CREATE: 916}
OWN = set(SIZES)

SETTEXT_CODE = 0x1018A270      # the code the fake window vtable's slot 0x2c points to (an unused function of the binary)
CLOSECB_CODE = 0x1018A2A8      # the code of the close callback's TVector

ADDR = dict(
    gOnlineSystem=0x10380AB0, gLobby=0x102E4888, gUIRootWindow=0x10395C24, gOnlinePanel=0x10395E98, gOnlineWindow=0x10395E94,
    gOnlineWindowE8c=0x10395E8C, gOnlineWindowE90=0x10395E90, gOnlineContactsBusy=0x10395E7C, gOnlineCursorState=0x10395E64,
    gOnlinePanelShown=0x10395E7A, gOnlineByte78=0x10395E78, gOnlineWord88=0x10395E88, gOnlineEditFontIndex=0x102B0C50,
    gGameWindowCentreX=0x102B2B5C, gGameWindowCentreY=0x102B2B58, gGameWindowHalfWidth=0x10278A28, gGameWindowHalfHeight=0x10278A2C,
    gScreenCentreX=0x102B2B64, gScreenCentreY=0x102B2B60, gViewCentreX=0x102B2B6C, gViewCentreY=0x102B2B68,
    gFindPanelWord=0x103960C8, gFindPanelCloseCallback=0x103960C4, gFindPanelPlayerName=0x103960A4, gOnlineEditText=0x10395E9C,
    gOnlineEmptyHelp=0x1024EF54, gOnlinePanelScript=0x102B0834, gOnlineUnderlayScriptA=0x102B0A22, gOnlineUnderlayScriptB=0x102B0B5E,
    OnlinePanelMsg=0x10268560, OnlinePanelSubPanelMsg=0x10268558, ContactListHandler=0x10268538, OnlineEditMsg=0x10268550,
    ButtonsMenuMsg=0x10268528, ButtonsUnderlayMsg=0x10268530, ContactsLCFormat=0x10268520, ContactsLCPostFormat=0x10268518,
    MessagesLCFormat=0x10268510, MessagesLCPostFormat=0x10268508, FindParksPostGetPlayerParksCallbackFn=0x10268580,
    GeneralParkDownloadCompleteCallback=0x10268590,
)
# the labels of the named blocks and data (windows are 1 to 39)
LABELS = {
    "sys": 60, "list": 61, "lobby": 62, "draw": 76, "font": 77, "closecb": 92,
    "gOnlinePanelScript": 70, "gOnlineUnderlayScriptA": 71, "gOnlineUnderlayScriptB": 72, "gOnlineEmptyHelp": 73, "gOnlineEditText": 74,
    "gFindPanelPlayerName": 75,
    "OnlinePanelMsg": 80, "OnlinePanelSubPanelMsg": 81, "ContactListHandler": 82, "OnlineEditMsg": 83, "ButtonsMenuMsg": 84,
    "ButtonsUnderlayMsg": 85, "ContactsLCFormat": 86, "ContactsLCPostFormat": 87, "MessagesLCFormat": 88, "MessagesLCPostFormat": 89,
    "FindParksPostGetPlayerParksCallbackFn": 90, "GeneralParkDownloadCompleteCallback": 91,
}
HOST_LAYOUT = [360, 8]      # the host's own offsets of mParksByPlayer and mWindow (native pointers); the Mac's are 0xec and 4, written in Ppc.link
FLOATS = ["gGameWindowCentreX", "gGameWindowCentreY", "gGameWindowHalfWidth", "gGameWindowHalfHeight",
          "gScreenCentreX", "gScreenCentreY", "gViewCentreX", "gViewCentreY"]
INTS = ["gOnlineContactsBusy", "gOnlineCursorState", "gOnlineWord88", "gFindPanelWord", "gOnlineEditFontIndex"]
BYTES = ["gOnlinePanelShown", "gOnlineByte78"]
PTRS = ["gOnlinePanel", "gOnlineWindow", "gOnlineWindowE8c", "gOnlineWindowE90", "gOnlineSystem", "gUIRootWindow", "gLobby"]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.add(0x1023BAA8)      # __ptr_glue runs as the original


def conv(L, c, v):
    if c == "L":
        return L.lab(v)
    if c == "i":
        return s32(v)
    if c == "u":
        return v & MASK
    if c == "h":
        return T.s16(v)
    if c == "b":
        return v & 0xFF
    raise ValueError(c)


def fake(addr, name, spec="", ret=None, then=None):
    """A recording fake: spec has one code per argument register from r3 (L label, i s32, u u32, h s16, b u8)."""
    def f(e):
        L = e.cl
        L.rec(name, tuple(conv(L, c, e.r(3 + i)) for i, c in enumerate(spec)))
        if then is not None:
            then(e)
        if ret is not None:
            e.set_r(3, ret(e) & MASK)
    STUBS.internal(addr, f)


def queue(key):
    return lambda e: e.cl.ret(key)


def get_window(e):
    i = e.r(4) & MASK
    return 0 if i == 0x7777 else e.objs[1 + i % 31]


def get_data(e):
    e.write(e.r(5), struct.pack(">I", e.cl.ret("GetData") & MASK))


fake(0x101DEB78, "Build", "LLL", lambda e: e.objs[e.cl.ret("Build")] if e.cl.ret_peek_ok else 0) if False else None


def build_ret(e):
    k = e.cl.ret("Build")
    return e.objs[k] if k else 0


def create_ret(e):
    k = e.cl.ret("CreateObject")
    return e.objs[k] if k else 0


fake(0x101DEB78, "Build", "LLL", build_ret)
fake(0x101DBE40, "GetWindow", "Li", get_window)
fake(0x101DCBA8, "SetCallbackFunction", "LL")
fake(0x101D2864, "SetColumnFormatsAll", "LLL")
fake(0x101D2614, "SetColumnType", "Lhi")
fake(0x101D1EB4, "SetSelectionObject", "LL")
fake(0x101CC148, "SetCurrent", "Li", lambda e: 0)
fake(0x101DB624, "SetClientDrawObject", "LL")
fake(0x10180270, "GetFontTableEntry", "u", lambda e: e.font)
fake(0x101DA118, "SetFont", "LL")
fake(0x101CD7E0, "SetStaticTextArea", "LLi", lambda e: 0)
fake(0x101CDAC4, "SetCursorDrawObject", "LL")
fake(0x101DB884, "GetPosition", "LLh", lambda e: 0)
fake(0x101DC1EC, "Show", "Lh", lambda e: 0)
fake(0x101DE084, "unlink", "L")
fake(0x101DC750, "CreateObject", "Liiihhhh", create_ret)
fake(0x1018C7A4, "MoveHelpText", "hh")
fake(0x101DCCEC, "CaptureKb", "Li")
fake(0x101794E8, "GetList", "LLLL")
fake(0x101E9F28, "ShowFindListSearching")
fake(0x101EA468, "RemoveOnlineParkFindPanel")
fake(0x101D41F0, "GetData", "Li", lambda e: 0, get_data)
fake(0x101EABB4, "CreateOnlineParkInfoPanel", "iLLLL")
fake(0x101CB048, "GetState", "L", queue("GetState"))
fake(0x101CB1D0, "SetState", "Lbb", lambda e: 0)
fake(0x101CE8A4, "SelectAll", "L")
fake(0x101C9400, "UI_PostMessage", "Liii", lambda e: 0)
fake(0x101DE318, "CallDefault", "Liii", queue("CallDefault"))
fake(SETTEXT_CODE, "SetText", "LL", lambda e: 1)
fake(CLOSECB_CODE, "CloseCb")
fake(0x101C74E0, "~CoolRect", "L")

STUBS.internal(0x101CD2D4, CL.recorder("FindDrawObject", "s", lambda e, log: e.draw))
STUBS.internal(0x10000E40, CL.recorder("UDebug", "s"))
STUBS.internal(0x101C7448, CL.recorder("CoolRect", "N"))
STUBS.internal(0x1018F1D0, CL.recorder("InterfaceCursorPosTextObjectSprite", "Nu"))

_sys_ctor = CL.recorder("CFSOnlineSystem", "N")


def stub_sys_ctor(e):
    this = e.r(3)
    _sys_ctor(e)
    e.write(this + 0xEC, struct.pack(">I", e.listaddr))
    e.set_r(3, this)


STUBS.internal(0x1016F7D0, stub_sys_ctor)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def name_of(self, p):
        """A pointer as a label: 0 NULL, a known block or window its number, anything else -1."""
        return 0 if p == 0 else self.known.get(p, -1)


class Ppc(Side):
    fmt = ">I"
    pw = ">I"

    def __init__(self, e, case):
        self.e = e
        objs = [0] + [e.alloc(0x200) for _ in range(1, 40)]
        vt, tv = e.alloc(0x40), e.alloc(8)
        e.write(tv, struct.pack(">II", SETTEXT_CODE, 0))
        e.write(vt + 0x2C, struct.pack(">I", tv))
        for o in objs[1:]:
            e.write(o + 0x118, struct.pack(">I", vt))
        self.objs = objs
        self.blocks = dict(sys=e.alloc(0x404), list=e.alloc(0x40), lobby=e.alloc(0x40), draw=e.alloc(16), font=e.alloc(16), closecb=e.alloc(8))
        e.write(self.blocks["closecb"], struct.pack(">II", CLOSECB_CODE, 0))
        e.objs, e.draw, e.font, e.listaddr = objs, self.blocks["draw"], self.blocks["font"], self.blocks["list"]
        fixed = {objs[i]: i for i in range(1, 40)}
        for name, label in LABELS.items():
            fixed[self.blocks[name] if name in self.blocks else ADDR[name]] = label
        self.known = dict(fixed)
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def obj(self, i):
        return self.objs[i]

    def blk(self, name):
        return self.blocks[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def setp(self, a, v):
        self.write(a, struct.pack(">I", v))

    def getp(self, a):
        return self.rd_word(a)

    def link(self, case):
        self.write(self.blocks["sys"] + 0xEC, struct.pack(">I", self.blocks["list"]))
        self.write(self.blocks["lobby"] + 4, struct.pack(">I", self.objs[case["lobwin"]]))

    def log(self):
        return self.L.log

    def call_msg(self, window, m, a, b):
        r3, _, _ = T.call_ppc(self.e, FN_MSG, [window, m & MASK, a & MASK, b & MASK])
        return r3 & MASK

    def call_create(self):
        T.call_ppc(self.e, FN_CREATE, [])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.addr = lib.tpw_opfp_addr
        fixed = {i: self.addr(b"obj%d" % i) for i in range(1, 40)}
        for name, label in LABELS.items():
            fixed[label] = self.addr(name.encode())
        self.known = {a: l for l, a in fixed.items()}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.addr(name.encode())

    def obj(self, i):
        return self.addr(b"obj%d" % i)

    def blk(self, name):
        return self.addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_opfp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setp(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def getp(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def link(self, case):
        self.lib.tpw_opfp_link_sys()
        self.lib.tpw_opfp_link_lobby(self.obj(case["lobwin"]))

    def log(self):
        return CL.host_end(self.lib)

    def call_msg(self, window, m, a, b):
        return self.lib.tpw_opfp_msg(window, s32(m), s32(a), s32(b)) & MASK

    def call_create(self):
        self.lib.tpw_opfp_create()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_opfp_addr.restype, lib.tpw_opfp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_opfp_alloc.restype, lib.tpw_opfp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_opfp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_opfp_link_sys.argtypes = []
    lib.tpw_opfp_link_lobby.argtypes = [C.c_uint64]
    lib.tpw_opfp_msg.restype, lib.tpw_opfp_msg.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_opfp_create.argtypes = []
    out = (C.c_int64 * 32)()
    n = lib.tpw_opfp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.link(case)
    s.setp(s.g("gUIRootWindow"), s.obj(39))
    s.setp(s.g("gLobby"), s.blk("lobby"))
    s.setp(s.g("gOnlineSystem"), s.blk("sys") if case["sysmode"] else 0)
    s.setp(s.g("gFindPanelCloseCallback"), s.blk("closecb") if case["closecb"] else 0)
    for name in ("gOnlinePanel", "gOnlineWindow", "gOnlineWindowE8c", "gOnlineWindowE90"):
        s.setp(s.g(name), s.obj(case[name]) if case[name] else 0)
    for name in INTS:
        s.words(s.g(name), [case[name]])
    for name in FLOATS:
        s.words(s.g(name), [case[name]])
    for name in BYTES:
        s.write(s.g(name), bytes([case[name]]))


def execute(s, case):
    if case["op"] == "msg":
        m, a, b = case["args"]
        return s.call_msg(s.obj(case["window"]), m, a, b)
    return s.call_create()


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log()}
    for name in PTRS:
        out[name] = s.name_of(s.getp(s.g(name)))
    for name in INTS + FLOATS:
        w = s.rd_word(s.g(name))
        out[name] = canon_f(w) if name in FLOATS else w
    for name in BYTES:
        out[name] = s.read(s.g(name), 1)[0]
    return out


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


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


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice([0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x3F800000, 0x00000001])
    return fb(r.uniform(-300, 300))


def base(r, op):
    c = {"op": op, "window": r.randrange(1, 31), "lobwin": r.randrange(1, 39), "sysmode": r.choice([0, 1]), "closecb": r.choice([0, 1])}
    c["queues"] = {
        "Build": [r.randrange(1, 39), r.randrange(1, 39), r.randrange(1, 39)],
        "CreateObject": [r.randrange(1, 39)],
        "GetState": [r.choice([0, 1, -1, 5, 127, -128])],
        "GetData": [r.choice([0, -1, 0x1234, 0x7FFFFFFF, -2, r.randrange(1, 1 << 30)])],
        "CallDefault": [r.choice([0, 1, -1, r.randrange(-1000, 1000)])],
    }
    for name in ("gOnlinePanel", "gOnlineWindow", "gOnlineWindowE8c", "gOnlineWindowE90"):
        c[name] = r.choice([0, r.randrange(1, 39)])
    c["gOnlineContactsBusy"] = r.choice([0, 1, -1, r.randrange(-5, 5)])
    c["gOnlineCursorState"] = r.choice([0, 1, 2, 3, r.randrange(-5, 9)])
    c["gOnlineWord88"] = r.choice([0, 1, -1, r.randrange(-100, 100)])
    c["gFindPanelWord"] = r.choice([0, 1, -1, r.randrange(-100, 100)])
    c["gOnlineEditFontIndex"] = r.choice([0, 7, 1, 20, 0xFFFFFFFF, r.randrange(0, 40)])
    c["gOnlinePanelShown"] = r.choice([0, 1, 0xFF])
    c["gOnlineByte78"] = r.choice([0, 1, 0xFF])
    for name in FLOATS:
        c[name] = rf(r)
    return c


MSGS = [0x100, 0x14, 0x13, 0x15, 0x16, 0x1F, 0x20, 0, -1, 0xFF, 0x101, 0x400, 0x7FF, 0x801, 0x802, 0x803, 0x1000B, 0x1000A, 0x1000C,
        0xFFFF, 0x80000000]
AS = [0xE059, -2, 0xE058, 0xE05A, 0, -1, 5, 0x1B, 0x1A, 0x1C, 0x7777, 0x12345678, 0xE05B, -3]
BS = [1, 0, 2, -1, 3, 0x7FFFFFFF]


def msg_case(r, m=None, a=None, b=None):
    c = base(r, "msg")
    m = r.choice(MSGS) if m is None else m
    a = r.choice(AS) if a is None else a
    b = r.choice(BS) if b is None else b
    if m == 0x802 and a == 0x7777:
        a = 7
    c["args"] = [m, a, b]
    return c


def msg_edges():
    r = random.Random(541)
    for m in MSGS:
        for a in AS:
            for b in (1, 0, -1):
                if m in (0x100, 0x400, 0x802, 0x1000B) or b == 1:
                    yield msg_case(r, m, a, b)
    for sysmode in (0, 1):
        for cb in (0, 1):
            for b in BS:
                for a in (0xE059, -2):
                    c = msg_case(r, 0x100, a, b)
                    c["sysmode"], c["closecb"] = sysmode, cb
                    yield c
    for data in (0, -1, 0x1234, 0x7FFFFFFF, -2):
        for b in (-1, 0, 3):
            for a in (5, 0x7777, 0xE059):
                c = msg_case(r, 0x400, a, b)
                c["queues"]["GetData"] = [data]
                yield c
    for st in (0, 1, -1, 5, 127, -128):
        for a in (5, 0xE059, 0xE05A):
            c = msg_case(r, 0x802, a, 0)
            c["queues"]["GetState"] = [st]
            yield c
    for cb in (0, 1):
        for a in (0x1B, 0x1A):
            c = msg_case(r, 0x1000B, a, 0)
            c["closecb"] = cb
            yield c


def msg_random(r):
    return msg_case(r)


def create_case(r):
    c = base(r, "create")
    c["args"] = [0, 0, 0]
    return c


def create_edges():
    r = random.Random(542)
    for first in (0, 1, 5, 38):
        for panel in (0, 1):
            c = create_case(r)
            c["queues"]["Build"] = [first, r.randrange(1, 39), r.randrange(1, 39)]
            c["gOnlinePanel"] = panel
            yield c
    for ctr in ((0, 0, 0, 0), (0x7FC00000, 0x80000000, 0x3F800000, 0xFF800000)):
        c = create_case(r)
        for n, v in zip(("gScreenCentreX", "gScreenCentreY", "gViewCentreX", "gViewCentreY"), ctr):
            c[n] = v
        yield c


def create_random(r):
    return create_case(r)


TESTS = [
    FnTest("OnlineParkFindPanelMsg", ppc, host, edge=msg_edges, rnd=msg_random),
    FnTest("CreateOnlinePanel", ppc, host, edge=create_edges, rnd=create_random),
]
