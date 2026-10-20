"""Unit v5225, the online news panel, against libtpw_unit_TextPanelMsg.dylib (tests/unit_TextPanelMsg.cpp), main app
("SimTheme Park"), src/UI_IslandLobby_TextPanelMsg.cpp:
  TextPanelMsg 0x101e30b4, CreateOnlineNewsPanel 0x101e3264, CalcScrollBar 0x101e2ce8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee
outside the unit is a fake on both sides that records its arguments (windows, the font and the callbacks as labels, texts as texts):
GetWindow, setZ, Dialog, the sprite and online system constructors, the window setters, GetFontTableEntry, the font height and the
window's SetText and SetValue (virtual calls through a hand made vtable on both sides), GetClientRect, UI_MeasureText (its limit
rectangle too), Show, SetMinMax, SetCurrent, UIMain_GetString, UIMain_SetTitleText, CaptureKb, SetTextOffset and the default handler.

The world of a case: the screen size words, the font index, the inbox flag, the panel and lobby pointers, the online system (none, or
one with two texts; a made one holds the same texts), six windows (the dialog and the five children), the client rectangle, the measured
height, the font height and the answers of SetValue and the default handler. What is compared: the answer, the panel pointer, the inbox
flag, the online system pointer (none, the old one or a made one) and the event log.

No fused operations. Not tested: a screen height of 0 and a font height that makes the scroll step 0 (divw by zero), a NULL panel for
TextPanelMsg and CalcScrollBar (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_TextPanelMsg"
MASK = 0xFFFFFFFF

TEXT, CREATE, CALC = 0x101E30B4, 0x101E3264, 0x101E2CE8
SIZES = {TEXT: 376, CREATE: 820, CALC: 632}
OWN = set(SIZES)

# the callees faked on both sides
GETWIN, SETZ, SPRITE, SETCDO = 0x101DBE40, 0x101DD8BC, 0x1018D11C, 0x101DB624
FONTENT, SETFONT, SETCDT, SETWRAP = 0x10180270, 0x101DA118, 0x101DE130, 0x101DA2D8
SETJUST, SETCB, SETCUR, SETCOL = 0x101DA204, 0x101DCBA8, 0x101CC148, 0x101DA3AC
GETSTR, SETTITLE, CAPKB, DIALOG = 0x10180468, 0x10180794, 0x101DCCEC, 0x101F12C8
SETOFF, CALLDEF, GETCRECT, MEASURE = 0x101DA484, 0x101DE318, 0x101DBA4C, 0x1018B8E4
SHOW, MINMAX, SYSCTOR, COOLCTOR, COOLDTOR = 0x101DC1EC, 0x101D6B44, 0x1016F7D0, 0x101C7448, 0x101C74E0
# code addresses of the virtual calls (in-binary addresses nothing else runs)
CODE_HEIGHT, CODE_SETVALUE, CODE_SETTEXT = 0x101E3600, 0x101E3604, 0x101E3608

ADDR = dict(font=0x102B05C0, script=0x102B0440, showinbox=0x10395DD4, panel=0x10395DD8, lobby=0x102E4888, sys=0x10380AB0,
            height=0x10395BBA, width=0x10395BBC, newsmsg=0x102684C8, textmsg=0x102684C0)
HOST_LAYOUT = [2]

L_WIN0, L_FONT, L_SCRIPT, L_NEWS, L_TEXTCB, L_SYSX = 1, 20, 30, 31, 32, 40


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rcd(addr, name, spec, ret=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret))


def world(e):
    return e.world


rcd(GETWIN, "GetWindow", "Li", lambda e, log: e.world["win"].get(e.r(4) - 0x5B7E, 0))
rcd(SETZ, "setZ", "Lh")
rcd(SPRITE, "Sprite", "Ni")
rcd(SETCDO, "SetClientDrawObject", "LL")
rcd(FONTENT, "GetFontTableEntry", "u", lambda e, log: e.world["font"])
rcd(SETFONT, "SetFont", "LL")
rcd(SETCDT, "SetClientDrawText", "LL")
rcd(SETWRAP, "SetFontWrap", "Lii")
rcd(SETJUST, "SetFontJust", "Lii")
rcd(SETCB, "SetCallbackFunction", "LL")
rcd(SETCUR, "SetCurrent", "Li", lambda e, log: 1)
rcd(SETCOL, "SetFontColour", "Liiii")
rcd(GETSTR, "UIMain_GetString", "i", lambda e, log: e.world["str"][e.r(3) - 0x10B])
rcd(SETTITLE, "UIMain_SetTitleText", "LiS")
rcd(CAPKB, "CaptureKb", "Li")
rcd(DIALOG, "Dialog", "LLi", lambda e, log: e.world["win"][0])
rcd(SETOFF, "SetTextOffset", "Lii")
rcd(CALLDEF, "CallDefaultWindowHandler", "Liii", "i")
rcd(SHOW, "Show", "Lh", lambda e, log: 1)
rcd(MINMAX, "SetMinMax", "Lii")
rcd(CODE_HEIGHT, "GetHeight", "L", "h")
rcd(CODE_SETVALUE, "SetValue", "Li", "h")
rcd(CODE_SETTEXT, "SetText", "LS", lambda e, log: 1)
STUBS.internal(COOLCTOR, lambda e: None)
STUBS.internal(COOLDTOR, lambda e: None)

_sys_ctor = CL.recorder("CFSOnlineSystem", "N")


def stub_sys_ctor(e):
    obj = e.r(3)
    _sys_ctor(e)
    e.write(obj + 0xA0, struct.pack(">I", e.world["ta"]))
    e.write(obj + 0xBC, struct.pack(">I", e.world["tb"]))


STUBS.internal(SYSCTOR, stub_sys_ctor)

_getcrect = CL.recorder("GetClientRect", "L")


def stub_getcrect(e):
    rect = e.r(4)
    _getcrect(e)
    L = e.cl
    e.write(rect + 4, struct.pack(">hhhh", s16(L.ret("crect_l")), s16(L.ret("crect_t")), s16(L.ret("crect_r")), s16(L.ret("crect_b"))))
    e.set_r(3, 1)


def s16(v):
    return T.s16(v)


STUBS.internal(GETCRECT, stub_getcrect)

_measure = CL.recorder("UI_MeasureText", "Sxiiii")


def stub_measure(e):
    rect, out = e.r(4), e.r(9)
    _measure(e)
    L = e.cl
    L.rec("MeasureRect", tuple(e.s32(rect + 4 * i) for i in range(4)) + tuple(e.s32(out + 4 * i) for i in range(4)))
    e.write(out + 12, struct.pack(">I", L.ret("mt_bottom") & MASK))


STUBS.internal(MEASURE, stub_measure)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def chars(self, a, text):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", ord(c)) for c in text))

    def newstr(self, text):
        a = self.alloc(2 * len(text) + 2)
        self.chars(a, text)
        return a

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "h", s16(v)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        w = self.world = make_world(self, case)
        fixed = {w["win"][i]: i + 1 for i in range(6)}
        fixed.update({w["font"]: L_FONT, ADDR["script"]: L_SCRIPT, ADDR["newsmsg"]: L_NEWS, ADDR["textmsg"]: L_TEXTCB,
                      w["sysx"]: L_SYSX})
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def setptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_words(a, 1)[0]

    def newwin(self, z):
        a = self.alloc(0x130)
        vt = self.alloc(0x40)
        tv_value = self.alloc(8)
        tv_text = self.alloc(8)
        self.words(tv_value, [CODE_SETVALUE, 0])
        self.words(tv_text, [CODE_SETTEXT, 0])
        self.words(vt + 0x24, [tv_value])
        self.words(vt + 0x2C, [tv_text])
        self.words(a + 0x118, [vt])
        self.half(a + 0xDC, z)
        return a

    def newfont(self):
        a = self.alloc(0x20)
        table = self.alloc(0x20)
        tv = self.alloc(8)
        self.words(tv, [CODE_HEIGHT, 0])
        self.words(table + 0xC, [tv])
        self.words(a + 0x10, [table])
        return a

    def newsys(self, ta, tb):
        a = self.alloc(0x404)
        self.words(a + 0xA0, [ta])
        self.words(a + 0xBC, [tb])
        return a

    def install(self, w):
        self.e.world = {"win": {i: w["win"][i] for i in range(6)}, "font": w["font"], "str": w["str"], "ta": w["ta"], "tb": w["tb"]}

    def call(self, case):
        w = self.world
        if case["op"] == "text":
            args = [TEXT, [w["win"][1], case["msg"] & MASK, case["a"] & MASK, case["b"] & MASK]]
        else:
            args = [CREATE if case["op"] == "create" else CALC, []]
        r3, _, _ = T.call_ppc(self.e, args[0], args[1])
        return s32(r3) if case["op"] == "text" else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        w = self.world = make_world(self, case)
        fixed = {i + 1: w["win"][i] for i in range(6)}
        fixed.update({L_FONT: w["font"], L_SCRIPT: self.g("script"), L_NEWS: self.g("newsmsg"), L_TEXTCB: self.g("textmsg"),
                      L_SYSX: w["sysx"]})
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_nsp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_nsp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def newwin(self, z):
        return self.lib.tpw_nsp_newwin(s16(z))

    def newfont(self):
        return self.alloc(64)

    def newsys(self, ta, tb):
        return self.lib.tpw_nsp_makesys(ta, tb)

    def install(self, w):
        self.lib.tpw_nsp_world(*w["win"], w["font"], *w["str"], w["ta"], w["tb"])

    def call(self, case):
        w = self.world
        if case["op"] == "text":
            return s32(self.lib.tpw_nsp_textpanelmsg(w["win"][1], s32(case["msg"]), s32(case["a"]), s32(case["b"])))
        if case["op"] == "create":
            self.lib.tpw_nsp_create()
        else:
            self.lib.tpw_nsp_calc()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64 = C.c_uint64
    lib.tpw_nsp_addr.restype, lib.tpw_nsp_addr.argtypes = u64, [C.c_char_p]
    lib.tpw_nsp_alloc.restype, lib.tpw_nsp_alloc.argtypes = u64, [C.c_uint32]
    lib.tpw_nsp_newwin.restype, lib.tpw_nsp_newwin.argtypes = u64, [C.c_int32]
    lib.tpw_nsp_world.restype, lib.tpw_nsp_world.argtypes = None, [u64] * 12
    lib.tpw_nsp_makesys.restype, lib.tpw_nsp_makesys.argtypes = u64, [u64, u64]
    lib.tpw_nsp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_nsp_textpanelmsg.restype, lib.tpw_nsp_textpanelmsg.argtypes = C.c_int32, [u64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_nsp_create.restype, lib.tpw_nsp_create.argtypes = None, []
    lib.tpw_nsp_calc.restype, lib.tpw_nsp_calc.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_nsp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def make_world(s, case):
    w = {}
    w["win"] = [s.newwin(case["z"] if i == 0 else 0x40 + i) for i in range(6)]
    w["font"] = s.newfont()
    w["str"] = [s.newstr(t) for t in case["strs"]]
    w["ta"] = s.newstr(case["texta"])
    w["tb"] = s.newstr(case["textb"])
    w["sysx"] = s.newsys(w["ta"], w["tb"])
    s.install(w)
    return w


def build(s, case):
    w = s.world
    s.half(s.g("height"), case["H"])
    s.half(s.g("width"), case["W"])
    s.words(s.g("font"), [case["font"]])
    s.write(s.g("showinbox"), bytes([case["show"]]))
    s.setptr(s.g("panel"), w["win"][0] if case["panel"] else 0)
    s.setptr(s.g("lobby"), 0x1234 if case["lobby"] else 0)
    s.setptr(s.g("sys"), w["sysx"] if case["sys"] else 0)


def observe(s, case, answer):
    w = s.world
    p = s.rd_ptr(s.g("panel"))
    q = s.rd_ptr(s.g("sys"))
    return {"answer": answer, "log": s.log(), "panel": 0 if p == 0 else (1 if p == w["win"][0] else -1),
            "show": s.read(s.g("showinbox"), 1), "sys": 0 if q == 0 else (1 if q == w["sysx"] else 2)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

LETTERS = "ABCDEFGHxyz"


def rstr(r):
    return "".join(r.choice(LETTERS) for _ in range(r.randrange(0, 6)))


def base(r, op, **kw):
    text_op = op == "text"
    c = {"op": op}
    c["H"] = kw.get("H", r.choice([768, 1536, 600, 1024, 1500, 1, 2, r.randrange(1, 1500)] + ([4000] if text_op else [])))
    c["W"] = kw.get("W", r.choice([1024, 2048, 1, 0, r.randrange(1, 4000)]))
    c["font"] = kw.get("font", r.choice([8, 0, 3, 6, 12, 0xFF, 0x80, 0x7F, r.randrange(256)]))
    c["show"] = kw.get("show", r.choice([0, 1]))
    c["panel"] = kw.get("panel", 1)
    c["lobby"] = kw.get("lobby", 1)
    c["sys"] = kw.get("sys", r.choice([0, 1]))
    c["z"] = kw.get("z", r.choice([0, 1, 100, 0x7FFB, 0x7FFC, 0x7FFF, -1, -4, -5, r.randrange(-300, 300)]))
    c["strs"] = [rstr(r) for _ in range(3)]
    c["texta"], c["textb"] = "a" + rstr(r), "b" + rstr(r)
    l = r.randrange(-1000, 2000)
    t = r.randrange(-500, 1500)
    ch = kw.get("ch", r.choice([r.randrange(-50, 1500), 0, 1, 768]))
    cw = kw.get("cw", r.choice([r.randrange(-50, 3000), 0, 1, 2047, 2048, 2049]))
    c["msg"] = kw.get("msg", 0)
    c["a"], c["b"] = kw.get("a", 0), kw.get("b", 0)
    mt = kw.get("mt", r.randrange(0, 4000))
    c["queues"] = {
        "crect_l": [l], "crect_t": [t], "crect_r": [l + cw], "crect_b": [t + ch],
        "mt_bottom": [mt],
        "GetHeight": [kw.get("fh", r.choice([r.randrange(-3, 100)] if text_op else [r.randrange(1, 60), 1, 13]))],
        "SetValue": [kw.get("sv", r.choice([0, 1, 5, -1, 0x10000, 0x10001]))],
        "CallDefaultWindowHandler": [r.choice([0, 1, -1, r.randrange(-5000, 5000)])],
    }
    return c


def calc_case(r, **kw):
    return base(r, "calc", **kw)


def calc_edges():
    r = random.Random(5225)
    for show in (0, 1):
        for sys in (0, 1):
            yield calc_case(r, show=show, sys=sys)
    for sv in (0, 1, 0x10000, -1):
        yield calc_case(r, sv=sv)
    # the measured height against the window's height, either side of equal
    for ch in (1, 100, 768, 1500):
        for H in (1536, 768, 1000):
            wh = ch
            mt = -(-wh * H // 1536)
            for k in (-2, -1, 0, 1, 2):
                yield calc_case(r, ch=ch, H=H, mt=max(0, mt + k), fh=r.choice([1, 13, 40]))
    for cw in (0, 1, 2047, 2048, 2049, -1, -2048, -2049, -3000):
        yield calc_case(r, cw=cw)
    for ch in (-1, -768, -1536, -2000, 0):
        yield calc_case(r, ch=ch)
    yield calc_case(r, font=0x80)
    yield calc_case(r, font=0xFF)


def calc_random(r):
    return calc_case(r)


def text_case(r, **kw):
    return base(r, "text", **kw)


def text_edges():
    r = random.Random(5226)
    for msg in (0x800, 0x7FF, 0x801, 0x100, 0x102, 0, -1, 0x10005, 0x5B82, 0x1101):
        yield text_case(r, msg=msg, a=1, b=2)
    for a in (0x5B82, 0x5B81, 0x5B83, 0, -1):
        for b in (1, 0, 2, -1, 0x100000001 & MASK):
            for show in (0, 1):
                yield text_case(r, msg=0x101, a=a, b=b, show=show)
    for sys in (0, 1):
        for show in (0, 1):
            yield text_case(r, msg=0x800, b=r.choice([1, 2, -1]), show=show, sys=sys)
    for b in (0, 1, -1, 100, 0x7FFFFF, -0x7FFFFF, 0x12345678, -0x12345678):
        for fh in (0, 1, -1, 13, 2000):
            yield text_case(r, msg=0x800, b=b, fh=fh, H=r.choice([768, 1536, 1, 4000]))
    for H in (1, 3, 600, 1536, 4000):
        yield text_case(r, msg=0x800, b=-77, fh=13, H=H)


def text_random(r):
    msg = r.choice([0x800, 0x101, 0x101, 0x800, r.randrange(0, 0x1000), r.randrange(-3, 0x20000)])
    return text_case(r, msg=msg, a=r.choice([0x5B82, 0x5B81, 0, r.randrange(-100, 0x6000)]),
                     b=r.choice([1, 0, 2, -1, r.randrange(-3000, 3000), r.randrange(-(1 << 31), 1 << 31) // 1000]))


def create_case(r, **kw):
    return base(r, "create", **kw)


def create_edges():
    r = random.Random(5227)
    for panel in (0, 1):
        for lobby in (0, 1):
            for sys in (0, 1):
                yield create_case(r, panel=panel, lobby=lobby, sys=sys, panel_z=0)
    for z in (0, 1, 0x7FFB, 0x7FFC, 0x7FFF, -1, -5, -0x8000):
        yield create_case(r, panel=0, z=z)
    for sv in (0, 1, 0x10000):
        for mt in (0, 3000):
            yield create_case(r, panel=0, sv=sv, mt=mt)


def create_random(r):
    return create_case(r, panel=r.choice([0, 0, 0, 1]), lobby=r.choice([1, 1, 1, 0]))


TESTS = [
    FnTest("TextPanelMsg", ppc, host, edge=text_edges, rnd=text_random),
    FnTest("CreateOnlineNewsPanel", ppc, host, edge=create_edges, rnd=create_random),
    FnTest("CalcScrollBar", ppc, host, edge=calc_edges, rnd=calc_random),
]
