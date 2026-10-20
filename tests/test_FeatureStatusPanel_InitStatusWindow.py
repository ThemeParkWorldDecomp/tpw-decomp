"""Unit v5676, FeatureStatusPanel::InitStatusWindow, against libtpw_unit_FeatureStatusPanel_InitStatusWindow.dylib
(tests/unit_FeatureStatusPanel_InitStatusWindow.cpp), main app ("SimTheme Park"), src/FeatureStatusPanel_UpdateStatusWindow_FeatureStatusPanel_InitStatusWindow.cpp:
  FeatureStatusPanel::InitStatusWindow 0x1019e454

A case is a panel whose +0x10 is NULL or a parent block holding four child windows (offset 0x200 * (id - 0x1583)), each with a table
pointer at +0x118 whose slot 0x24 holds a marker. The callees (GetWindow, operator new, the three constructors, SetClientDrawObject,
GetFontTableEntry, SetFont, UIMain_GetString, SetClientDrawText and the virtual SetValue through __ptr_glue) are fakes on both sides that
record their arguments (windows as offsets from the parent); the case's queues decide which children exist, whether new answers NULL, and
the pointers the font and string lookups answer. Compared: the event log. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FeatureStatusPanel_InitStatusWindow"
MASK = 0xFFFFFFFF

FN = 0x1019E454
SIZES = {FN: 596}
OWN = set(SIZES)

GET_WINDOW = 0x101DBE40
OP_NEW = 0x102384B8
SPRITE_CTOR = 0x1018D11C
NUMBER_CTOR = 0x1018D930
CASH_CTOR = 0x1018DE60
SET_DRAW = 0x101DB624
GET_FONT = 0x10180270
SET_FONT = 0x101DA118
GET_STRING = 0x10180468
SET_TEXT = 0x101DE130
PTR_GLUE = 0x1023BAA8
MARK = 0x1234

STUBS = CL.Stubs()
STUBS.real.update(OWN)

state = {"parent": 0}


def rel(a):
    return T.s32((a - state["parent"]) & MASK)


def stub_get_window(e):
    L = e.cl
    state["parent"] = e.r(3)
    wid = T.s32(e.r(4))
    L.rec("GetWindow", (wid,))
    e.set_r(3, (state["parent"] + 0x200 * (wid - 0x1583)) & MASK if L.ret("GetWindow") else 0)


def stub_new(e):
    L = e.cl
    size = e.r(3)
    L.rec("new", (size,))
    e.set_r(3, 0 if L.ret("new") else e.alloc(size))


def stub_set_draw(e):
    e.cl.rec("SetClientDrawObject", (rel(e.r(3)), e.cl.lab(e.r(4))))


def stub_get_font(e):
    e.cl.rec("GetFontTableEntry", (e.r(3),))
    e.set_r(3, e.cl.ret("GetFontTableEntry") & MASK)


def stub_set_font(e):
    e.cl.rec("SetFont", (rel(e.r(3)), T.s32(e.r(4))))


def stub_get_string(e):
    e.cl.rec("UIMain_GetString", (T.s32(e.r(3)),))
    e.set_r(3, e.cl.ret("UIMain_GetString") & MASK)


def stub_set_text(e):
    e.cl.rec("SetClientDrawText", (rel(e.r(3)), T.s32(e.r(4))))


def stub_glue(e):
    # r12 is the table's slot 0x24 contents (the marker); the host fake does not see it
    assert e.r(12) == MARK, hex(e.r(12))
    e.cl.rec("WindowSetValue", (rel(e.r(3)), T.s32(e.r(4))))


for _a, _f in ((GET_WINDOW, stub_get_window), (OP_NEW, stub_new), (SET_DRAW, stub_set_draw), (GET_FONT, stub_get_font),
               (SET_FONT, stub_set_font), (GET_STRING, stub_get_string), (SET_TEXT, stub_set_text), (PTR_GLUE, stub_glue)):
    STUBS.internal(_a, _f)
STUBS.internal(SPRITE_CTOR, CL.recorder("SpriteCtor", "N"))
STUBS.internal(NUMBER_CTOR, CL.recorder("NumberCtor", "N"))
STUBS.internal(CASH_CTOR, CL.recorder("CashCtor", "N"))

HOST_LAYOUT = [0x10]


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, panel):
        T.call_ppc(self.e, FN, [panel])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_fspinit_alloc(n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, panel):
        self.lib.tpw_fspinit_init(panel)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_fspinit_"
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "init").restype, getattr(lib, p + "init").argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        panel = s.alloc(0x30)
        s.words(panel, [0] * 12)
        if case["has"]:
            parent = s.alloc(0xA00)
            vt = s.alloc(0x40)
            s.words(vt + 0x24, [MARK])
            for k in range(1, 5):
                s.words(parent + 0x200 * k + 0x118, [vt])
            s.words(panel + 0x10, [parent])
        s.call(panel)
        return {"log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, has=None, gw=None, nw=None):
    return {"has": r.choice([0, 1, 1, 1]) if has is None else has,
            "queues": {"GetWindow": [r.choice([0, 1, 1]) for _ in range(4)] if gw is None else gw,
                       "new": [r.choice([0, 0, 1]) for _ in range(4)] if nw is None else nw,
                       "GetFontTableEntry": [r.randrange(0x1000, 0x2000) for _ in range(4)],
                       "UIMain_GetString": [r.randrange(0x3000, 0x4000) for _ in range(2)]}}


def edges():
    r = random.Random(676)
    yield mk(r, 0)
    yield mk(r, 1, [1] * 4, [0] * 4)
    yield mk(r, 1, [0] * 4, [0] * 4)
    yield mk(r, 1, [1] * 4, [1] * 4)
    for mask in range(16):
        yield mk(r, 1, [(mask >> i) & 1 for i in range(4)], [0] * 4)
        yield mk(r, 1, [1] * 4, [(mask >> i) & 1 for i in range(4)])


def rnd(r):
    return mk(r)


TESTS = [
    FnTest("FeatureStatusPanel::InitStatusWindow", ppc, host, edge=edges, rnd=rnd),
]
