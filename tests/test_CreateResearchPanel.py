"""Unit v5021, the research panel's creation, against libtpw_unit_CreateResearchPanel.dylib (tests/unit_CreateResearchPanel.cpp), main app
("SimTheme Park"), src/game/ui/ui_researchPanel.cpp:
  CreateResearchPanel 0x101b4f74

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee is a
fake on both sides that records its arguments: MainMenuUp, UIMain_CheckPanel, UIMain_GetString, UI_DialogBox, UIMain_SetPanel, UIMain_UISound,
InterfaceWindow::Build / GetWindow / SetCallbackFunction / setExtraParam / SetFrame / SetDrawObject / SetClientDrawObject / SetFont... , the
scroll bar's SetMinMax, the three draw object constructors (with the texture name they get), operator new (with the Mac's sizes), the font
table, the world's and the analyser's getters, InitAll and the advisor. The windows are fake objects; GetWindow answers the one whose label is
the id's low byte, or NULL as the case's mask says; the virtual call through the vtable slot 0x24 (SetValue) goes through the __ptr_glue fake
on the PowerPC side and the window's SetValue on the host, and records the window and the value.

The world of a case: MainMenuUp's answer, the panel global, the game type word (the guard byte is 1 on the Mac), the world's head halfword,
Build's answer, the analyser's two abilities, the lab's work load, the sound handles, the font index and sentinels in every global the
function writes. What is compared: the event log and all those globals read back.

No fused operations. Not tested: the game type's first-use constructor (the host's function static cannot be reset between cases, so the
guard byte stays 1 and the constructor is not part of the log); an ability maximum of 0 (divw by zero) or -1 with the dividend INT_MIN; NULL
answers of GetWindow for the gauge child, the frame and the bar's inner window (the original calls through them unchecked).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateResearchPanel"
MASK = 0xFFFFFFFF

FN = 0x101B4F74
SIZES = {FN: 2888}
OWN = set(SIZES)

TOKEN = 0x5E700001
SCRIPT = 0x102AC35A

ADDR = dict(gGameWindow=0x103949E4, gGameWorld=0x1037EBFC, gPanel=0x10395980, gBar=0x1039597C, gMax=0x10395974, gCur=0x10395970,
            gGauge=0x1039596C, gMade=0x10395978, gSndA=0x10395968, gSndB=0x10395964, gFont=0x102ACA94, gType=0x102B3AA0,
            gGuard=0x102B3AA4)
CALLBACKS = {0x10267D58: 1, 0x10267D50: 2, 0x10267D48: 3, 0x10267D40: 4, 0x10267D38: 5, 0x10267D30: 6, 0x10267D28: 7}
IDS = [0x10, 0x0E, 0x0F, 0x0D, 0x0C, 0x0B, 0x01, 0x07, 0x08, 0x09, 0x0A]
WORLD_HEAD_PPC, LAB_LOAD_PPC = 0x1DA742, 0x1460
NOBJ = 0x43

OBJ_PPC = {}
ST = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cb_id(a):
    return CALLBACKS.get(a & MASK, 99 if a else 0)


def mesh_id(e, a):
    if (a & MASK) == 0:
        return 0
    t = e.cstr(a)
    if isinstance(t, bytes):
        t = t.decode("latin1")
    return {"effortguage.wct": 1, "bluebar.wct": 2}.get(t, 9)


def stub(addr, name, ret=None):
    def make(fn):
        STUBS.internal(addr, fn)
        return fn
    return make


def simple(addr, name, regs, ret=None):
    """regs: how to read each argument: 'l' label of r3.., 'i' s32, 'u' u32."""
    def fn(e):
        vals = []
        for n, k in enumerate(regs):
            r = e.r(3 + n)
            vals.append(e.cl.lab(r) if k == "l" else s32(r) if k == "i" else (r & MASK))
        e.cl.rec(name, tuple(vals))
        if ret is not None:
            e.set_r(3, ret(e) & MASK)
    STUBS.internal(addr, fn)


simple(0x101FC1EC, "MainMenuUp", "", lambda e: ST["cfg"][1])
simple(0x10180514, "UIMain_CheckPanel", "", lambda e: 0)
simple(0x10180468, "UIMain_GetString", "i", lambda e: 0x70000000 + s32(e.r(3)))
simple(0x101805A0, "UIMain_SetPanel", "l")
simple(0x101802CC, "UIMain_UISound", "i", lambda e: ST["cfg"][5] if e.r(3) == 0x9A else ST["cfg"][6])
simple(0x101D6B44, "SetMinMax", "lii")
simple(0x101DB624, "SetClientDrawObject", "ll")
simple(0x101DB474, "SetDrawObject", "ll")
simple(0x101DCFD0, "setExtraParam", "lii")
simple(0x101DB7DC, "SetFrame", "li")
simple(0x101DA204, "SetFontJust", "lii")
simple(0x101DA2D8, "SetFontWrap", "lii")
simple(0x10180270, "GetFontTableEntry", "u", lambda e: 0x60000000 + (e.r(3) & MASK))
simple(0x101DA118, "SetFont", "lu")
simple(0x101DA3AC, "SetFontColour", "liiii")
simple(0x101DE130, "SetClientDrawText", "lu")
simple(0x101B4000, "InitAll", "")
simple(0x101821CC, "UIMain_AdvisorMessage", "i")
simple(0x1013EBC8, "GetParkAnalyser", "l", lambda e: OBJ_PPC[0x41])
simple(0x1013ECE8, "GetResearchLab", "l", lambda e: OBJ_PPC[0x42])
simple(0x100E8ED8, "MaxResearcherAbility", "l", lambda e: ST["cfg"][3])
simple(0x100E8DD8, "CurrentResearcherAbility", "l", lambda e: ST["cfg"][4])


def stub_dialog(e):
    text, r, g, b, v8, vc = struct.unpack(">IBBBxII", e.read(e.r(3), 16))
    e.cl.rec("UI_DialogBox", (text, (r << 16) | (g << 8) | b, s32(v8), s32(vc), s32(e.r(4)), 1 if e.r(5) else 0, s32(e.r(6)),
                              1 if e.r(7) else 0, s32(e.r(8)), 1 if e.r(9) else 0, 1 if (e.r(10) & 0xFF) else 0))
    e.set_r(3, 0)


STUBS.internal(0x101F1800, stub_dialog)


def stub_build(e):
    e.cl.rec("Build", (e.cl.lab(e.r(3)), 1 if (e.r(4) & MASK) == SCRIPT else 0, cb_id(e.r(5))))
    e.set_r(3, OBJ_PPC[0x30] if ST["cfg"][2] else 0)


STUBS.internal(0x101DEB78, stub_build)


def stub_getwindow(e):
    this, ident = e.r(3), s32(e.r(4))
    e.cl.rec("GetWindow", (e.cl.lab(this), ident))
    low = ident & 0xFF
    null = (ST["cfg"][0] >> (low & 0x1F)) & 1
    if low == 0x10:
        ST["n10"] += 1
        if ST["n10"] > 1:
            null = 0
    e.set_r(3, 0 if null else OBJ_PPC[low])


STUBS.internal(0x101DBE40, stub_getwindow)


def stub_setcb(e):
    e.cl.rec("SetCallbackFunction", (e.cl.lab(e.r(3)), cb_id(e.r(4))))


STUBS.internal(0x101DCBA8, stub_setcb)


def save_toc(e):
    """The glue (an import or a pointer call) keeps the caller's TOC pointer at 0x14(r1); the code reloads r2 from there afterwards."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def stub_new(e):
    save_toc(e)
    size = e.r(3) & MASK
    e.cl.rec("new", (size,))
    e.set_r(3, e.alloc(size))


STUBS.internal(0x102384B8, stub_new)


def ctor(addr, name, data=False):
    rec = CL.recorder(name + "Ctor", "N")
    tail = name + ("Data" if data else "Mesh")

    def fn(e):
        mesh = (e.r(4) & MASK) if data else mesh_id(e, e.r(4))
        rec(e)
        e.cl.rec(tail, (mesh,))
    STUBS.internal(addr, fn)


ctor(0x10188FF8, "VGradient")
ctor(0x101898C8, "HGradient")
ctor(0x1018D11C, "TextSprite", True)


def stub_glue(e):
    save_toc(e)
    e.cl.rec("VCall", (e.cl.lab(e.r(3)), s32(e.r(4)), 1 if (e.r(12) & MASK) == TOKEN else 0))


STUBS.internal(0x1023BAA8, stub_glue)


def setup(runner):
    emu = runner.emu
    T.install_std_stubs(emu)
    vt = T.persist_alloc(emu, 0x40, bytes(0x24) + struct.pack(">I", TOKEN) + bytes(0x18))
    for k in range(1, NOBJ):
        if k < 0x40:
            data = bytes(0x118) + struct.pack(">I", vt) + bytes(0x200 - 0x11C)
            OBJ_PPC[k] = T.persist_alloc(emu, 0x200, data)
        elif k == 0x40:
            OBJ_PPC[k] = T.persist_alloc(emu, 0x1E0000, bytes(0x1E0000))
        elif k == 0x41:
            OBJ_PPC[k] = T.persist_alloc(emu, 0x100, bytes(0x100))
        else:
            OBJ_PPC[k] = T.persist_alloc(emu, 0x1480, bytes(0x1480))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def lbl(self, w):
        return self.rev.get(w, ("raw", w))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    head_off, load_off = WORLD_HEAD_PPC, LAB_LOAD_PPC

    def __init__(self, e, case):
        self.e = e
        fixed = {OBJ_PPC[k]: k for k in range(1, NOBJ)}
        self.rev = dict(fixed)
        self.rev[0] = 0
        self.L = e.cl = CL.PpcLog(fixed, {})
        ST["cfg"] = case["cfg"]
        ST["n10"] = 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def obj(self, k):
        return OBJ_PPC[k]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self):
        T.call_ppc(self.e, FN, [])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        arr = (C.c_int64 * 8)(*case["cfg"])
        lib.tpw_crp_config(arr)
        self.objs = {k: lib.tpw_crp_obj(k) for k in range(1, NOBJ)}
        self.rev = {v: k for k, v in self.objs.items()}
        self.rev[0] = 0
        self.head_off, self.load_off = lib.layout
        CL.host_begin(lib, dict(self.objs), {})

    def done(self):
        pass

    def g(self, name):
        if name == "gGuard":
            return 0
        return self.lib.tpw_crp_addr(name.encode())

    def obj(self, k):
        return self.objs[k]

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_crp_run()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_crp_addr.restype, lib.tpw_crp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_crp_obj.restype, lib.tpw_crp_obj.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_crp_config.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_crp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_crp_run.argtypes = []
    out = (C.c_int64 * 8)()
    n = lib.tpw_crp_layout(out)
    lib.layout = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.ptr(s.g("gGameWindow"), s.obj(0x31))
    s.ptr(s.g("gGameWorld"), s.obj(0x40))
    s.ptr(s.g("gPanel"), s.obj(case["panel"]) if case["panel"] else 0)
    s.ptr(s.g("gBar"), s.obj(case["bar0"]) if case["bar0"] else 0)
    for name, v in zip(("gMax", "gCur", "gGauge", "gSndA", "gSndB"), case["sent"]):
        s.words(s.g(name), [v])
    s.write(s.g("gMade"), bytes([case["made"]]))
    s.words(s.g("gFont"), [case["font"]])
    s.words(s.g("gType"), [case["gtype"]])
    if s.g("gGuard"):
        s.write(s.g("gGuard"), bytes([1]))
    s.half(s.obj(0x40) + s.head_off, case["head"])
    s.words(s.obj(0x42) + s.load_off, [case["load"]])


def observe(s, case):
    out = {"log": s.log()}
    for name in ("gPanel", "gBar"):
        out[name] = s.lbl(s.rd_ptr(s.g(name)))
    for name in ("gMax", "gCur", "gGauge", "gSndA", "gSndB", "gFont", "gType"):
        out[name] = s.rd_word(s.g(name))
    out["gMade"] = s.read(s.g("gMade"), 1)
    out["head"] = struct.unpack(s.fmt[0] + "H", s.read(s.obj(0x40) + s.head_off, 2))[0]
    out["load"] = s.rd_word(s.obj(0x42) + s.load_off)
    return out


def run(s, case):
    try:
        build(s, case)
        s.call()
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def mk(r, **kw):
    c = dict(mask=0, menu=0, panel=0, gtype=0, head=1, buildok=1, mx=50, cur=30, sa=7, sb=9, load=70, font=6, bar0=0)
    c.update(kw)
    c["sent"] = [r.getrandbits(32) for _ in range(5)]
    c["made"] = r.getrandbits(8)
    c["cfg"] = [c["mask"], c["menu"], c["buildok"], c["mx"], c["cur"], c["sa"], c["sb"], 0]
    return c


def edges():
    r = random.Random(5021)
    yield mk(r)
    yield mk(r, menu=1)
    yield mk(r, menu=-7)
    yield mk(r, panel=0x30)
    yield mk(r, panel=0x30, menu=1)
    yield mk(r, gtype=2)
    yield mk(r, gtype=2, head=0)
    yield mk(r, gtype=1)
    yield mk(r, gtype=3)
    yield mk(r, gtype=-1)
    yield mk(r, head=0)
    yield mk(r, head=0x1234)
    yield mk(r, buildok=0)
    for ident in IDS:
        yield mk(r, mask=1 << (ident & 0x1F))
    yield mk(r, mask=sum(1 << (i & 0x1F) for i in IDS))
    yield mk(r, bar0=7)
    for mx, cur, load in ((1, 0, 0x46), (1, 100, 100), (7, -20, 0), (3, 150, -5), (-5, 40, 30), (9, 0x400000, 70), (13, 0x7FFFFFFF, 99),
                          (2, -0x7FFFFFFF, 3), (100, 100, 70), (100, 99, 71), (30, 31, 0x47), (1, 1, 1), (-2, 0, 0)):
        yield mk(r, mx=mx, cur=cur, load=load)
    for font in (0, 1, 6, 7, 0xFFFFFFFF):
        yield mk(r, font=font)
    yield mk(r, sa=-1, sb=0x7FFFFFFF)


def rnd(r):
    k = r.random()
    mask = 0
    if k < 0.5:
        for i in IDS:
            if r.random() < 0.15:
                mask |= 1 << (i & 0x1F)
    mx = r.choice([r.randrange(1, 300), -r.randrange(2, 300), r.randrange(1, 1 << 20)])
    return mk(r, mask=mask, menu=r.choice([0, 0, 0, 0, 1, -1]), panel=r.choice([0, 0, 0, 0x30, 0x31]),
              gtype=r.choice([0, 1, 2, 3, 0, 0]), head=r.choice([0, 1, 5, 0xFFFF]), buildok=r.choice([0, 1, 1, 1]),
              mx=mx, cur=r.choice([r.randrange(-50, 400), r.randrange(-(1 << 31) + 1, 1 << 31)]), sa=T.rint(r), sb=T.rint(r),
              load=r.choice([r.randrange(0, 101), T.rint(r)]), font=r.choice([6, r.randrange(0, 20), r.getrandbits(32)]),
              bar0=r.choice([0, 0, 7]))


TESTS = [
    FnTest("CreateResearchPanel", ppc, host, edge=edges, rnd=rnd),
]
