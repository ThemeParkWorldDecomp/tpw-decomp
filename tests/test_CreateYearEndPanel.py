"""Unit v5913, CreateYearEndPanel, against libtpw_unit_CreateYearEndPanel.dylib (tests/unit_CreateYearEndPanel.cpp), main app
("SimTheme Park"), src/game/ui/ui_yearEndPanel.cpp:
  CreateYearEndPanel 0x101c68d0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee
outside the unit (UIMain_CheckPanel, Build, SetPanel, GetString, SetTitleText, GetWindow, operator new and the three constructors,
SetClientDrawObject, GetFontTableEntry, SetFont, SetClientDrawText, GetParkAnalyser, CHistory::GetValue, the virtual SetValue through
__ptr_glue, SetFontJust) is a fake on both sides that records its arguments and answers from the case.

The world of a case: gGameWindow, gGameWorld, the panel global (preset or not), the queues (Build's answer, each GetWindow id's answer,
the history values). What is compared: the event log (every call, in order, with labels for pointers, history offsets and the values
set) and the panel global. No fused operations. Not tested: a NULL gGameWindow or gGameWorld (the original reads through them; the
fakes would not notice).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateYearEndPanel"
MASK = 0xFFFFFFFF

FN = 0x101C68D0
SIZES = {FN: 2792}
OWN = set(SIZES)

CHECKPANEL, BUILD, SETPANEL, GETSTRING, SETTITLE = 0x10180514, 0x101DEB78, 0x101805A0, 0x10180468, 0x10180794
GETWINDOW, NEW, SPRITE, SETCDO, FONTENTRY = 0x101DBE40, 0x102384B8, 0x1018D11C, 0x101DB624, 0x10180270
SETFONT, SETTEXT, ANALYSER, GETVALUE, CASH = 0x101DA118, 0x101DE130, 0x1013EBC8, 0x1001C2A8, 0x1018DE60
NUMBER, PTRGLUE, FONTJUST = 0x1018D930, 0x1023BAA8, 0x101DA204

ADDR = dict(gwin=0x103949E4, world=0x1037EBFC, panel=0x10395B70, script=0x102AF524)
MSG_TVECTOR = 0x10268078
HOST_LAYOUT = [0x250]

CHILD_IDS = [0xADAE, 0xADAF, 0xADA9, 0xADA3, 0xADA4, 0xADA5, 0xADB0, 0xADA8, 0xADAC, 0xADA2, 0xADAD, 0xADA7, 0xADAA, 0xADA6, 0xADAB,
             0xADB1, 0xADB2]
# labels: 1 game window, 2 panel, 3 inner window, 4 analyser, 5 world, 6 other panel, 10+i children, 30+i strings, 40/41 fonts 6/8
LABELS = [1, 2, 3, 4, 5, 6] + [10 + i for i in range(len(CHILD_IDS))] + [30 + i for i in range(8)] + [40, 41]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = [None]


def rec(name, vals):
    CUR[0].L.rec(name, tuple(vals))


def lab(v):
    return CUR[0].L.lab(v & MASK)


def st_check(e):
    rec("CheckPanel", ())
    e.set_r(3, CUR[0].L.ret("CheckPanel") & MASK)


def st_build(e):
    rec("Build", (lab(e.r(3)), int((e.r(4) & MASK) == ADDR["script"]), int((e.r(5) & MASK) == MSG_TVECTOR)))
    e.set_r(3, CUR[0].addr(CUR[0].L.ret("Build")))


def st_setpanel(e):
    rec("SetPanel", (lab(e.r(3)),))


def st_getstring(e):
    rec("GetString", (s32(e.r(3)),))
    i = s32(e.r(3)) - 0xBA
    e.set_r(3, CUR[0].addr(30 + i) if 0 <= i < 8 else 0)


def st_settitle(e):
    rec("SetTitleText", (lab(e.r(3)), s32(e.r(4)), lab(e.r(5))))


def st_font(e):
    rec("Font", (e.r(3) & MASK,))
    k = e.r(3) & MASK
    e.set_r(3, CUR[0].addr(40) if k == 6 else CUR[0].addr(41) if k == 8 else 0)


def st_getwindow(e):
    i = s32(e.r(4))
    rec("GetWindow", (lab(e.r(3)), i))
    e.set_r(3, CUR[0].addr(CUR[0].L.ret("GW:%d" % i)))


def st_setcdo(e):
    rec("SetClientDrawObject", (lab(e.r(3)), lab(e.r(4))))


def st_setfont(e):
    rec("SetFont", (lab(e.r(3)), lab(e.r(4))))


def st_settext(e):
    rec("SetClientDrawText", (lab(e.r(3)), lab(e.r(4))))


def st_fontjust(e):
    rec("SetFontJust", (lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def st_analyser(e):
    rec("GetParkAnalyser", (lab(e.r(3)),))
    e.set_r(3, CUR[0].addr(4))


def st_getvalue(e):
    rec("GetValue", (s32((e.r(3) & MASK) - CUR[0].addr(4)), s32(e.r(5))))
    e.write(e.r(4) & MASK, struct.pack(">I", CUR[0].L.ret("GV") & MASK))


def save_toc(e):
    """The glue stores the caller's TOC at 0x14(r1); the code reloads r2 from there afterwards."""
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))


def st_new(e):
    save_toc(e)
    rec("new", (e.r(3) & MASK,))
    e.set_r(3, e.alloc(e.r(3) & MASK))


def ctor(name):
    def fn(e):
        CUR[0].L.rec(name, (CUR[0].L.newlab(e.r(3) & MASK),))
    return fn


def st_glue(e):
    # the virtual SetValue: this in r3, the value in r4 (r12 holds the TVector)
    save_toc(e)
    rec("SetValue", (lab(e.r(3)), s32(e.r(4))))


for a, f in ((CHECKPANEL, st_check), (BUILD, st_build), (SETPANEL, st_setpanel), (GETSTRING, st_getstring), (SETTITLE, st_settitle),
             (GETWINDOW, st_getwindow), (NEW, st_new), (SPRITE, ctor("SpriteCtor")), (SETCDO, st_setcdo), (FONTENTRY, st_font),
             (SETFONT, st_setfont), (SETTEXT, st_settext), (ANALYSER, st_analyser), (GETVALUE, st_getvalue), (CASH, ctor("CashCtor")),
             (NUMBER, ctor("NumberCtor")), (FONTJUST, st_fontjust), (PTRGLUE, st_glue)):
    STUBS.internal(a, f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    fmt = None

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        """A pointer global: 4 bytes on the Mac, 8 on the host."""
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def pword(self, a, value):
        self.write(a, struct.pack(self.pfmt, value))

    def make_objs(self):
        self.objs = {l: self.alloc(0x140) for l in LABELS}
        self.back = {a: l for l, a in self.objs.items()}
        table = self.alloc(0x40)
        for l, a in self.objs.items():
            self.words(a + 0x118, [table])   # the table the original reads the SetValue vector from (+0x24)

    def addr(self, label):
        return self.objs.get(label, 0)

    def label_of(self, a):
        return self.back.get(a, 0 if a == 0 else -1)


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make_objs()
        self.L = e.cl = CL.PpcLog(dict(self.back), case["queues"])
        CUR[0] = self
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

    def call(self):
        T.call_ppc(self.e, FN, [])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_objs()
        for l, a in self.objs.items():
            lib.tpw_yep_setlabel(l, a)
        CL.host_begin(lib, dict(self.objs), case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_yep_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_yep_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_yep_run()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_yep_addr.restype, lib.tpw_yep_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_yep_alloc.restype, lib.tpw_yep_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_yep_setlabel.restype, lib.tpw_yep_setlabel.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_yep_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_yep_run.restype, lib.tpw_yep_run.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_yep_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.pword(s.g("gwin"), s.addr(1))
    s.pword(s.g("world"), s.addr(5))
    s.pword(s.g("panel"), s.addr(6) if case["preset"] else 0)


def run(s, case):
    try:
        build(s, case)
        s.call()
        return {"log": s.log(), "panel": s.label_of(s.rd_word(s.g("panel")))}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make_case(r, preset=False, build_ok=True, inner_ok=True, missing=()):
    q = {"CheckPanel": [r.choice([0, 1])], "Build": [2 if build_ok else 0], "GW:%d" % 0xADA1: [3 if inner_ok else 0]}
    for i, cid in enumerate(CHILD_IDS):
        q["GW:%d" % cid] = [0 if i < 7 and ((missing != "rand" and i in missing) or (missing == "rand" and r.random() < 0.3)) else 10 + i]
    q["GV"] = [r.choice([0, 1, -1, r.randrange(-100000, 100000), 0x7FFFFFFF, -0x80000000]) & MASK for _ in range(40)]
    return {"queues": q, "preset": preset}


def edges():
    r = random.Random(913)
    yield make_case(r, preset=True)
    yield make_case(r, build_ok=False)
    yield make_case(r, inner_ok=False)
    yield make_case(r)
    for i in range(7):   # only the seven text windows may be missing: the cash and number ones are read through (+0x118)
        yield make_case(r, missing=(i,))


def rnd(r):
    return make_case(r, preset=r.random() < 0.1, build_ok=r.random() < 0.9, inner_ok=r.random() < 0.9,
                     missing="rand" if r.random() < 0.3 else ())


TESTS = [
    FnTest("CreateYearEndPanel", ppc, host, edge=edges, rnd=rnd),
]
