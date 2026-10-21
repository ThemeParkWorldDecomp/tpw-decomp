"""Unit v51067, twenty small online UI functions, against libtpw_unit_OnlineParkListPostGetAllParksInCityCallbackFn.dylib
(tests/unit_OnlineParkListPostGetAllParksInCityCallbackFn.cpp), main app ("SimTheme Park"),
src/game/ui/ui_cityLobbyController.cpp, src/game/ui/ui_onlineCityInfoOverlay.cpp, src/game/ui/ui_onlineLogin.cpp, src/game/ui/ui_onlineNewsPanel.cpp, src/game/ui/ui_onlineOutbox.cpp, src/game/ui/ui_onlinePanel.cpp, src/game/ui/ui_onlineParkFindPanel.cpp, src/game/ui/ui_onlineParkInfoPanel.cpp, src/game/ui/ui_onlineParkListPanel.cpp:
  OnlineParkListPostGetAllParksInCityCallbackFn 0x101ec444, RemoveOnlineParkInfoPanel 0x101eba64, FindParksPostGetPlayerParksCallbackFn
  0x101e9324, RemoveOnlinePanel 0x101e92c4, FillContactList 0x101e82dc, MessagesLCPostFormat 0x101e7cf4, MessagesLCFormat 0x101e7c18,
  OutboxCallback 0x101e4124, __sinit_ui_onlineNewsPanel_cpp 0x101e3628, RemoveOnlineNewsPanel 0x101e35c4, __sinit_ui_onlineLogin_cpp
  0x101e2c78, __sinit_ui_onlineCityInfoOverlay_cpp 0x101e2450, Hide/Show/RemoveOnlineCityInfoOverlay 0x101e23e8 / 0x101e2380 / 0x101e2314,
  CityInfoOverlayMsg 0x101e1ee8, Hide/Show/RemoveCityLobbyController 0x101e08c0 / 0x101e0858 / 0x101e07f0,
  ListParkOnlineParkInfoPanelCancelCallback 0x101e0388.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation (pointer globals are 4 bytes on the Mac, 8 on the host). Every
callee outside the unit (UI_SendMessage, ClearAll, the fill functions, GetWindow, RemoveAllItems, the window setters, operator new and the
constructors, the font calls, the string constructor and __register_global_object, the virtual calls through __ptr_glue) is a fake on both
sides that records its arguments (objects as labels).

The world of a case: the window globals (set or NULL), the online control, the screens, the font, the mail words, queues for the fakes'
answers. What is compared: the answer, the event log, the pointer globals (as labels) and the two mail words.

No fused operations. Not tested: a screen height of 0 in MessagesLCFormat (the divide by zero is undefined on the host).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_OnlineParkListPostGetAllParksInCityCallbackFn"
MASK = 0xFFFFFFFF

# name, address, size, op number of the host's tpw_ov_call
FUNCS = [
    ("OnlineParkListPostGetAllParksInCityCallbackFn", 0x101ec444, 56),
    ("RemoveOnlineParkInfoPanel", 0x101eba64, 76),
    ("FindParksPostGetPlayerParksCallbackFn", 0x101e9324, 48),
    ("RemoveOnlinePanel", 0x101e92c4, 56),
    ("FillContactList", 0x101e82dc, 152),
    ("MessagesLCPostFormat", 0x101e7cf4, 168),
    ("MessagesLCFormat", 0x101e7c18, 180),
    ("OutboxCallback", 0x101e4124, 20),
    ("__sinit_ui_onlineNewsPanel_cpp", 0x101e3628, 60),
    ("RemoveOnlineNewsPanel", 0x101e35c4, 56),
    ("__sinit_ui_onlineLogin_cpp", 0x101e2c78, 64),
    ("__sinit_ui_onlineCityInfoOverlay_cpp", 0x101e2450, 64),
    ("HideOnlineCityInfoOverlay", 0x101e23e8, 56),
    ("ShowOnlineCityInfoOverlay", 0x101e2380, 56),
    ("RemoveOnlineCityInfoOverlay", 0x101e2314, 56),
    ("CityInfoOverlayMsg", 0x101e1ee8, 60),
    ("HideCityLobbyController", 0x101e08c0, 56),
    ("ShowCityLobbyController", 0x101e0858, 56),
    ("RemoveCityLobbyController", 0x101e07f0, 56),
    ("ListParkOnlineParkInfoPanelCancelCallback", 0x101e0388, 56),
]
OWN = set(a for _, a, _ in FUNCS)

ADDR = dict(gLobbyScreens=0x102e49b8, gOnlineParkInfoPanel=0x103961e4, gOnlinePanel=0x10395e98, gOnlineNewsPanel=0x10395dd8,
            gOnlineCityOverlay=0x10395d04, gCityLobbyWindow=0x10395c70, gOnlineControl=0x10396d28, gContactsFontIndex=0x102b0c4c,
            gUIRealScreenHeight=0x10395bba, gMailStatusA=0x10395e04, gMailStatusB=0x10395dfc)
PTRS = [("gOnlineParkInfoPanel", "pi"), ("gOnlinePanel", "op"), ("gOnlineNewsPanel", "np"), ("gOnlineCityOverlay", "co"),
        ("gCityLobbyWindow", "cl"), ("gOnlineControl", "ctl")]
LAB = dict(pi=1, op=2, np=3, co=4, cl=5, adv=6, onl=7, ctl=8, font=9, win=10, parks=11, ctx=12)
NAMES = list(LAB)
HOST_LAYOUT = []

CUR = {"glue": "GetHeight", "font": 0}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def st_operator_new(e):
    L = e.cl
    L.rec("OperatorNew", (e.r(3),))
    e.set_r(3, e.alloc(e.r(3)) & MASK)


def st_ptr_glue(e):
    L = e.cl
    name = CUR["glue"]
    L.rec(name, (L.lab(e.r(3)),))
    if name == "GetHeight":
        e.set_r(3, L.ret("GetHeight") & MASK)


def st_set_callback(e):
    L = e.cl
    L.rec("SetCallbackFunction", (L.lab(e.r(3)), 1 if e.r(4) == 0x10268540 else 0))


def st_register(e):
    L = e.cl
    L.rec("RegisterGlobal", (L.lab(e.r(3)), 1 if e.r(4) == 0x103bcbb8 else 0, L.lab(e.r(5))))


def glue(fn):
    """The Mac's glue saves the caller's TOC at 0x14(r1); the caller reloads r2 from there."""
    def wrapped(e):
        e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2)))
        fn(e)
    return wrapped


STUBS.internal(0x101c9518, CL.recorder("UI_SendMessage", "Liii"))
STUBS.internal(0x100a7128, CL.recorder("ClearAll", "L"))
STUBS.internal(0x101ec1d4, CL.recorder("FillParkList", "L"))
STUBS.internal(0x101ec840, CL.recorder("RemoveOnlineParkListPanel", ""))
STUBS.internal(0x101e9bf0, CL.recorder("FillOnlineParkFindList", "L"))
STUBS.internal(0x101dbe40, CL.recorder("GetWindow", "Li", ret=lambda e, log: (e.r(3) + 0x100) & MASK))
STUBS.internal(0x101d3cc8, CL.recorder("RemoveAllItems", "L"))
STUBS.internal(0x102384b8, glue(st_operator_new))
STUBS.internal(0x10213870, CL.recorder("COnlineControlCtor", "N"))
STUBS.internal(0x1021bf30, CL.recorder("UpdateUserList", "L"))
STUBS.internal(0x1018d11c, CL.recorder("SpriteCtor", "Ni"))
STUBS.internal(0x101d98d8, CL.recorder("WindowCtor", "NLiihhhh"))
STUBS.internal(0x101db624, CL.recorder("SetClientDrawObject", "LL"))
STUBS.internal(0x10180270, CL.recorder("GetFontTableEntry", "u", ret=lambda e, log: CUR["font"]))
STUBS.internal(0x101da118, CL.recorder("SetFont", "LL"))
STUBS.internal(0x101da3ac, CL.recorder("SetFontColour", "Liiii"))
STUBS.internal(0x101dcba8, st_set_callback)
STUBS.internal(0x101de318, CL.recorder("CallDefault", "Liii", ret="i"))
STUBS.internal(0x1023baa8, glue(st_ptr_glue))
STUBS.internal(0x10238800, glue(CL.recorder("StringCtor", "NL")))
STUBS.internal(0x10239940, glue(st_register))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def make_objects(self):
        self.o = {n: self.alloc(0x400) for n in NAMES}
        # the Mac's virtual calls read these tables (nothing on the host reads them)
        self.words(self.o["onl"], [self.alloc(0x80)])
        self.words(self.o["font"] + 0x10, [self.alloc(0x80)])

    def i16(self, name, v):
        self.write(self.g(name), struct.pack(self.fmt[0] + "h", v))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make_objects()
        fixed = {self.o[n]: LAB[n] for n in NAMES}
        fixed.update({0x1024ee76 + 2: 20, 0x1024ee5e + 2: 21, 0x1024ee86: 22, 0x10395df0: 31, 0x10395dc8: 32, 0x10395d1c: 33})
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)
        CUR["glue"] = case["glue"]
        CUR["font"] = self.o["font"]
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

    def pset(self, name, v):
        self.words(self.g(name), [v])

    def pget(self, name):
        return self.L.lab(self.rd_word(self.g(name)))

    def set_screens(self):
        blk = self.alloc(0x20)
        self.words(blk + 8, [self.o["onl"]])
        self.words(blk + 0x14, [self.o["adv"]])
        self.words(self.g("gLobbyScreens"), [blk])

    def call(self, idx, args):
        r3, _, _ = T.call_ppc(self.e, FUNCS[idx][1], [a & MASK for a in args])
        return r3 & MASK

    def label_of(self, v):
        return self.L.lab(v)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_objects()
        fixed = {LAB[n]: self.o[n] for n in NAMES}
        fixed.update({20: self.g("loginText1"), 21: self.g("cityText1"), 22: self.g("newsText"), 31: self.g("newsNode"),
                      32: self.g("loginNode"), 33: self.g("cityNode")})
        CL.host_begin(lib, fixed, case["queues"])
        CUR["glue"] = case["glue"]

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ov_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ov_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def pset(self, name, v):
        self.write(self.g(name), struct.pack("<Q", v))

    def pget(self, name):
        return self.lib.tpw_ov_lab(struct.unpack("<Q", self.read(self.g(name), 8))[0])

    def set_screens(self):
        self.lib.tpw_ov_set(0, self.o["adv"], self.o["onl"])

    def call(self, idx, args):
        self.lib.tpw_ov_set(1, self.o["font"], 0)
        a = list(args) + [0] * 4
        return self.lib.tpw_ov_call(idx, a[0], a[1], a[2], a[3])

    def label_of(self, v):
        return v


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ov_addr.restype, lib.tpw_ov_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ov_alloc.restype, lib.tpw_ov_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ov_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ov_set.restype, lib.tpw_ov_set.argtypes = None, [C.c_int32, C.c_uint64, C.c_uint64]
    lib.tpw_ov_lab.restype, lib.tpw_ov_lab.argtypes = C.c_int64, [C.c_uint64]
    lib.tpw_ov_call.restype = C.c_int64
    lib.tpw_ov_call.argtypes = [C.c_int32, C.c_uint64, C.c_uint64, C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ov_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    for gname, oname in PTRS:
        s.pset(gname, s.o[oname] if case["g"][gname] else 0)
    s.set_screens()
    s.words(s.g("gMailStatusA"), [case["mailA"]])
    s.words(s.g("gMailStatusB"), [case["mailB"]])
    s.words(s.g("gContactsFontIndex"), [case["fontIndex"]])
    s.i16("gUIRealScreenHeight", case["height"])


def resolve(s, a):
    return s.o[a] if isinstance(a, str) else a


def execute(s, case):
    r = s.call(case["idx"], [resolve(s, a) for a in case["args"]])
    if case["idx"] == 6:
        return s.label_of(r)
    return r & MASK if case["idx"] == 15 else 0


def observe(s, case, answer):
    d = {"answer": answer, "log": s.log(), "mailA": s.rd_word(s.g("gMailStatusA")), "mailB": s.rd_word(s.g("gMailStatusB"))}
    for gname, _ in PTRS:
        d[gname] = s.pget(gname)
    return d


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

WORDS = [0, 1, 2, 4, 0x14, 0x13, 0x15, 0x7FFFFFFF, 0x80000000, MASK, 0xFFFFFFFE, 0x12345678]
HEIGHTS = [1, 2, 17, 0x7FFF, 0x8000, 0x12345, -1, -17, 0, 0xFFFF, 0x18000]
SCREENS = [1, -1, 2, 600, 480, 0x7FFF, -0x8000, 1200, 7, -300]


def glue_of(idx):
    return "Slot30" if idx == 19 else "GetHeight"


def mk(r, idx, args=(), flags=None, **kw):
    if flags is None:
        flags = r.random() < 0.7
    c = {"idx": idx, "args": list(args), "glue": glue_of(idx),
         "g": {n: (flags if isinstance(flags, bool) else r.random() < 0.7) for n, _ in PTRS},
         "mailA": r.choice(WORDS) if r.random() < 0.5 else r.getrandbits(32), "mailB": r.getrandbits(32),
         "fontIndex": r.choice(WORDS), "height": r.choice(SCREENS),
         "queues": {"GetHeight": [r.choice(HEIGHTS) & MASK if r.random() < 0.8 else r.randrange(-40, 200) & MASK],
                    "CallDefault": [r.choice(WORDS)]}}
    c.update(kw)
    return c


def args_for(r, idx):
    if idx in (0, 2):
        return [r.choice([0, 1, MASK, 2, r.getrandbits(32)]), "ctx", "parks"]
    if idx == 5:
        return ["win"]
    if idx == 7:
        return [r.choice(WORDS + [r.getrandbits(32)]), r.choice(WORDS + [r.getrandbits(32)])]
    if idx == 15:
        return ["win", r.choice([0x14, 0x13, 0x15, 0, MASK, r.getrandbits(32)]), r.choice(WORDS), r.choice(WORDS)]
    return []


def edges(idx):
    def gen():
        r = random.Random(1000 + idx)
        for flags in (True, False):
            for i in range(3):
                yield mk(r, idx, args_for(r, idx), flags=flags)
        if idx in (0, 2):
            for err in (0, 1, MASK, 0x100):
                yield mk(r, idx, [err, "ctx", "parks"])
        if idx == 15:
            for msg in (0x14, 0x13, 0x15, 0, 0x114):
                for flags in (True, False):
                    yield mk(r, idx, ["win", msg, 5, 6], flags=flags)
        if idx == 7:
            for a, b in ((0, 0), (MASK, MASK), (0x80000000, 0x7FFFFFFF)):
                yield mk(r, idx, [a, b])
        if idx == 6:
            for h in HEIGHTS:
                for sh in (1, -1, 7, 600, 0x7FFF, -0x8000):
                    c = mk(r, idx)
                    c["queues"]["GetHeight"] = [h & MASK]
                    c["height"] = sh
                    yield c
    return gen


def randoms(idx):
    def gen(r):
        return mk(r, idx, args_for(r, idx))
    return gen


TESTS = [FnTest(name, ppc, host, edge=edges(i), rnd=randoms(i)) for i, (name, _, _) in enumerate(FUNCS)]
