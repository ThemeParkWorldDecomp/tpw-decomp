"""Unit v5910, the status panel's handlers, against libtpw_unit_DefaultStatusPanelMsg.dylib (tests/unit_DefaultStatusPanelMsg.cpp), main app
("SimTheme Park"), src/ui_StatusPanel_SaveTags_UIStatusPanel_GetNextObject_ViewWindowCallback_DefaultStatusPanelMsg.cpp:
  UIStatusPanel::DisplayStatusText 0x10193af4, DefaultStatusPanelMsg 0x10193c78, UIStatusPanel::InitEditBox 0x10193dcc,
  TwoEditsMsg 0x10193fac (host name TwoEditsMsgStatus: the park-name handler owns the plain name)

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory at the Mac's addresses, host memory in the
arena). Every callee outside the unit is a fake on both sides recording its arguments: the InterfaceWindow methods, UI_SendMessage and
PostMessage, the panel's navigation, the sign setters, the default handlers, and the panel's second table (reached through __ptr_glue; the
fake records the slot and the panel).

The world: five blocks (window, ride object, panel, two more windows), the panel's fields (state at +4, windows at +0xc and +0x18, the
second table at +0x20 with slots 0xc 0x10 0x24 0x28 0x44), the thing global. Compared: the answer, the panel's words, the event log.

No fused operations. Not tested: a NULL window at +0xc in InitEditBox (the original reads through it unchecked).
"""
import ctypes as C
import itertools
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DefaultStatusPanelMsg"
MASK = 0xFFFFFFFF

FN_DISPLAY, FN_DEFAULT, FN_INIT, FN_TWO = 0x10193AF4, 0x10193C78, 0x10193DCC, 0x10193FAC
SIZES = {FN_DISPLAY: 128, FN_DEFAULT: 276, FN_INIT: 124, FN_TWO: 548}
OWN = set(SIZES)

ADDR = dict(thing=0x103947A4, bufA=0x10394782, bufB=0x10394764)
HOST_LAYOUT = [4, 0xC, 0x18, 0x1C, 0x20]
SLOTS = (0x44, 0x24, 0x28, 0x10, 0xC)
TITLE_TVECTOR = 0x10267968

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def ident(a):
    return 1 if (a & MASK) == ADDR["bufA"] else 2 if (a & MASK) == ADDR["bufB"] else 0


def stub(addr, name, f, ret=None):
    def fn(e):
        L = e.cl
        L.rec(name, tuple(f(e, L)))
        if ret is not None:
            e.set_r(3, ret(e, L) & MASK)
    STUBS.internal(addr, fn)


def obj_ret(e, L):
    idx = s32(e.r(4))
    return L.objs.get({0: 2, 1: 3, 6: 3}.get(idx, 0), 0)


stub(0x101DD030, "getExtraParam", lambda e, L: (L.lab(e.r(3)), s32(e.r(4))), obj_ret)
stub(0x101DBE40, "GetWindow", lambda e, L: (L.lab(e.r(3)), s32(e.r(4))), lambda e, L: L.objs[5])
stub(0x101DCCEC, "CaptureKb", lambda e, L: (L.lab(e.r(3)), s32(e.r(4))))
stub(0x101DCBA8, "SetCallbackFunction", lambda e, L: (L.lab(e.r(3)), 1 if e.r(4) == TITLE_TVECTOR else 0))
stub(0x101DCFD0, "setExtraParam", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), L.lab(e.r(5))), lambda e, L: 0)
stub(0x101DC1EC, "Show", lambda e, L: (L.lab(e.r(3)), s32(e.r(4))), lambda e, L: 0)
stub(0x101DE318, "CallDefault", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))), lambda e, L: L.ret("default"))
stub(0x101C9518, "UI_SendMessage", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))), lambda e, L: 0)
stub(0x101C9400, "UI_PostMessage", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))), lambda e, L: 0)
stub(0x10182304, "EnableAllKeyTables", lambda e, L: ())
stub(0x1018116C, "DecideKbCapture", lambda e, L: ())
stub(0x101945C4, "EditNamePanelGone", lambda e, L: (L.lab(e.r(3)),))
stub(0x10192C28, "GetNextObject", lambda e, L: (L.lab(e.r(3)),))
stub(0x10192DE0, "GetPreviousObject", lambda e, L: (L.lab(e.r(3)),))
stub(0x10110948, "SetSignNameA", lambda e, L: (L.lab(e.r(3)), ident(e.r(4))))
stub(0x10110B10, "SetSignNameB", lambda e, L: (L.lab(e.r(3)), ident(e.r(4))))
stub(0x101EEB9C, "RegenerateRideSign", lambda e, L: (L.lab(e.r(3)), ident(e.r(4)), ident(e.r(5))))
stub(0x1018493C, "UIMain_DefaultPanelMsg", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))), lambda e, L: L.ret("dpm"))
stub(0x10180794, "UIMain_SetTitleText", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), 1 if e.r(5) == 0x1024DE12 else 0))
stub(0x10181018, "UIMain_SetObjectStateText", lambda e, L: (L.lab(e.r(3)), s32(e.r(4))))
stub(0x1023BAA8, "slot", lambda e, L: (L.slotmap.get(e.r(12), -1), L.lab(e.r(3))))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def norm(self, w):
        return self.back.get(w, w)

    def make_objs(self):
        self.blocks = [self.alloc(0x40) for _ in range(5)]      # label 1 window, 2 object, 3 panel, 4 and 5 windows
        self.back = {self.ptr(b): -(i + 1) for i, b in enumerate(self.blocks)}
        self.win, self.obj, self.panel, self.wa, self.wb = self.blocks


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.cl = None
        self.make_objs()
        self.L = e.cl = CL.PpcLog({b: i + 1 for i, b in enumerate(self.blocks)}, case["queues"])
        self.L.objs = {i + 1: b for i, b in enumerate(self.blocks)}
        self.L.slotmap = {}
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ptr(self, a):
        return a

    def alloc(self, n):
        return self.e.alloc(n)

    def slotblock(self, slot):
        b = self.e.alloc(8)
        self.L.slotmap[b] = slot
        return b

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_objs()
        lib.tpw_dsp_setobjs(*self.blocks[1:])
        lib.tpw_dsp_setobjs(self.obj, self.panel, self.wa, self.wb)
        CL.host_begin(lib, {i + 1: b for i, b in enumerate(self.blocks)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_dsp_addr(name.encode())

    def ptr(self, a):
        return self.lib.tpw_dsp_off(a)

    def alloc(self, n):
        return self.lib.tpw_dsp_alloc(n)

    def slotblock(self, slot):
        b = self.alloc(8)
        self.write(b, struct.pack("<Q", self.lib.tpw_dsp_slotfn(slot)))
        return b

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        lib = self.lib
        if fn == FN_DISPLAY:
            lib.tpw_dsp_display(args[0])
            return 0
        if fn == FN_INIT:
            lib.tpw_dsp_init(args[0], s32(args[1]))
            return 0
        f = lib.tpw_dsp_default if fn == FN_DEFAULT else lib.tpw_dsp_two
        return f(args[0], s32(args[1]), s32(args[2]), s32(args[3])) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, i32, u32 = C.c_uint64, C.c_int32, C.c_uint32
    lib.tpw_dsp_addr.restype, lib.tpw_dsp_addr.argtypes = u64, [C.c_char_p]
    lib.tpw_dsp_alloc.restype, lib.tpw_dsp_alloc.argtypes = u64, [u32]
    lib.tpw_dsp_off.restype, lib.tpw_dsp_off.argtypes = u32, [u64]
    lib.tpw_dsp_slotfn.restype, lib.tpw_dsp_slotfn.argtypes = u64, [i32]
    lib.tpw_dsp_setobjs.argtypes = [u64] * 4
    lib.tpw_dsp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_dsp_display.argtypes = [u64]
    lib.tpw_dsp_init.argtypes = [u64, i32]
    lib.tpw_dsp_default.restype, lib.tpw_dsp_default.argtypes = i32, [u64, i32, i32, i32]
    lib.tpw_dsp_two.restype, lib.tpw_dsp_two.argtypes = i32, [u64, i32, i32, i32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_dsp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    vt = s.alloc(0x50)
    for slot in SLOTS:
        s.words(vt + slot, [s.ptr(s.slotblock(slot))])
    s.words(s.panel, [case["mode"], case["state"], case["f08"], s.ptr(s.wa) if case["wa"] else 0, 0, 0,
                      s.ptr(s.wb) if case["wb"] else 0, case["f1c"], s.ptr(vt)])
    s.words(s.g("thing"), [s.obj & MASK if case["thing"] else 0])


def execute(s, case):
    fn = case["fn"]
    if fn == FN_DISPLAY:
        s.call(fn, [s.panel])           # void: r3 is left over
        return 0
    if fn == FN_INIT:
        s.call(fn, [s.panel, case["id"]])
        return 0
    return s.call(fn, [s.win, case["m"], case["a"], case["b"]])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "panel": [s.norm(w) for w in s.rd_words(s.panel, 8)],
            "thing": s.rd_words(s.g("thing"), 1)[0] != 0}


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


def base(r, fn):
    return {"fn": fn, "queues": {"default": [r.randrange(-5, 5)], "dpm": [r.randrange(-5, 5)]},
            "mode": r.randrange(0, 6), "state": r.choice([0, 1, 2, r.randrange(-3, 9)]), "f08": r.randrange(0, 99),
            "wa": r.random() < 0.7, "wb": r.random() < 0.7, "f1c": r.randrange(0, 99), "thing": r.random() < 0.5,
            "id": r.randrange(-3, 0x50000), "m": 0, "a": 0, "b": r.randrange(-3, 99)}


def make(fn, msgs, avals):
    def one(r, m=None, a=None, thing=None):
        c = base(r, fn)
        c["m"] = r.choice(msgs) if m is None else m
        c["a"] = r.choice(avals) if a is None else a
        if thing is not None:
            c["thing"] = thing
        return c

    def edges():
        r = random.Random(fn)
        for m, a, t in itertools.product(msgs, avals, (False, True)):
            yield one(r, m, a, t)

    return edges, lambda r: one(r)


DEF_MSGS = [0x14, 0x1000A, 0x10009, 0x1000B, 0x13, 0x15, 0, -1]
DEF_A = [0x2600, 0x2800, 0x27FF, 0x2601, 0x2801, 0, -1]
TWO_MSGS = [0x802, 0x803, 0x804, 0x100, 0x14, 0x101, 0x13, 0x805, 0x801, 0, 0x15, -1]
TWO_A = [0x3943C, 0x3943D, 0x3943B, -1, -2, 0, 5]

d_edges, d_rnd = make(FN_DEFAULT, DEF_MSGS, DEF_A)
t_edges, t_rnd = make(FN_TWO, TWO_MSGS, TWO_A)


def disp_edges():
    r = random.Random(1)
    for t, wb, st in itertools.product((False, True), (False, True), (0, 1, 2, -1)):
        c = base(r, FN_DISPLAY)
        c.update(thing=t, wb=wb, state=st)
        yield c


def disp_rnd(r):
    return base(r, FN_DISPLAY)


def init_edges():
    r = random.Random(2)
    for i in (0, 1, -1, 0x3943C, 0x7FFFFFFF):
        c = base(r, FN_INIT)
        c.update(id=i, wa=True)
        yield c


def init_rnd(r):
    c = base(r, FN_INIT)
    c["wa"] = True
    return c


TESTS = [
    FnTest("UIStatusPanel::DisplayStatusText", ppc, host, edge=disp_edges, rnd=disp_rnd),
    FnTest("DefaultStatusPanelMsg", ppc, host, edge=d_edges, rnd=d_rnd),
    FnTest("UIStatusPanel::InitEditBox", ppc, host, edge=init_edges, rnd=init_rnd),
    FnTest("TwoEditsMsg", ppc, host, edge=t_edges, rnd=t_rnd),
]
