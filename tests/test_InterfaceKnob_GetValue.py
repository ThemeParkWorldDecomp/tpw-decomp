"""Unit v51111, the small members of the interface controls and panels, against libtpw_unit_InterfaceKnob_GetValue.dylib
(tests/unit_InterfaceKnob_GetValue.cpp), main app ("SimTheme Park"), src/game/ui/InterfaceBar.cpp, src/game/ui/InterfaceImage.cpp, src/game/ui/InterfaceKnob.cpp, src/game/ui/InterfaceLightPanel.cpp, src/game/ui/InterfaceTextBox.cpp, src/game/ui/InterfaceVaryBox.cpp, src/game/ui/InterfaceWindow.cpp, src/game/ui/UI_Base.cpp, src/game/ui/ui_progressBar.cpp, src/game/ui/ui_upgradeStatus.cpp, src/game/ui/ui_yearEndPanel.cpp:
  InterfaceKnob::GetValue 0x101d0354, SetValue 0x101d02e8, ~InterfaceKnob 0x101d0194, InterfaceImage::GetValue 0x101d0054,
  InterfaceLightPanel::~InterfaceLightPanel 0x101d097c, GetValue 0x101d0b5c, UI_HelpTextEnabled 0x101cab08,
  InterfaceBar::GetValue 0x101c8968, InterfaceTextBox::DrawFunction 0x101d80f8, InterfaceVaryBox::~InterfaceVaryBox 0x101d8d74,
  SetInc 0x101d8ee8, SetValue 0x101d8f2c, GetValue 0x101d8f84, operator- 0x101d8fc0, RemoveYearEndPanel 0x101c73e4,
  UpgradeStatusPanel::Remove 0x101c66e0, UpdateWindow 0x101c6304, DeInitWindow 0x101c62c8, DeInitStatusWindow 0x101c6090,
  InterfaceWindow::CaptureKeys 0x101dce4c, stopAnimTimer 0x101dd754, ProgBarMsg 0x101dff64.

A case is a world and a call. The objects are 0x400-byte blocks built by ONE piece of Python per function: on the PowerPC side in emulator
memory with the Mac's offsets (big-endian), on the host side as the host's own objects (the library's wrapper sets the members by name
and answers what the call changed). The callees outside the unit are fakes on both sides that record the same events: the knob's and the
vary box's ChangeValue, the base window destructor, operator delete, UIStatusPanel::CloseWindow, UI_SendMessage, InterfaceTimer::Stop,
CallDefaultWindowHandler and the virtual calls through __ptr_glue (WhatAmI, the panel's three slots, the window's SetText).

What is compared: the answer, the members and globals the call writes (as labels where they are pointers) and the event log.

No fused operations. Not tested: the vtable pointer a destructor stores (the host's is its own; the Mac's constant is not observable
there), the deleting half of a destructor (the wrapper records the delete the Mac's code calls when the hidden short flag is above 0),
a timer id outside 0..3 or 0x80080..0x80083 (the original indexes past its table), a NULL text window in UpdateWindow (the original
calls through it unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InterfaceKnob_GetValue"
MASK = 0xFFFFFFFF

# ---- the unit's own functions
FN = {
    "InterfaceKnob::GetValue": 0x101d0354, "InterfaceKnob::SetValue": 0x101d02e8, "InterfaceKnob::~InterfaceKnob": 0x101d0194,
    "InterfaceImage::GetValue": 0x101d0054, "InterfaceLightPanel::~InterfaceLightPanel": 0x101d097c,
    "InterfaceLightPanel::GetValue": 0x101d0b5c, "UI_HelpTextEnabled": 0x101cab08, "InterfaceBar::GetValue": 0x101c8968,
    "InterfaceTextBox::DrawFunction": 0x101d80f8, "InterfaceVaryBox::~InterfaceVaryBox": 0x101d8d74,
    "InterfaceVaryBox::SetInc": 0x101d8ee8, "InterfaceVaryBox::SetValue": 0x101d8f2c, "InterfaceVaryBox::GetValue": 0x101d8f84,
    "InterfaceVaryBox::operator-": 0x101d8fc0, "RemoveYearEndPanel": 0x101c73e4, "UpgradeStatusPanel::Remove": 0x101c66e0,
    "UpgradeStatusPanel::UpdateWindow": 0x101c6304, "UpgradeStatusPanel::DeInitWindow": 0x101c62c8,
    "UpgradeStatusPanel::DeInitStatusWindow": 0x101c6090, "InterfaceWindow::CaptureKeys": 0x101dce4c,
    "InterfaceWindow::stopAnimTimer": 0x101dd754, "ProgBarMsg": 0x101dff64,
}
SIZES = {0x101d0354: 8, 0x101d02e8: 60, 0x101d0194: 104, 0x101d0054: 8, 0x101d097c: 104, 0x101d0b5c: 8, 0x101cab08: 36,
         0x101c8968: 8, 0x101d80f8: 4, 0x101d8d74: 104, 0x101d8ee8: 20, 0x101d8f2c: 36, 0x101d8f84: 8, 0x101d8fc0: 128,
         0x101c73e4: 56, 0x101c66e0: 36, 0x101c6304: 120, 0x101c62c8: 4, 0x101c6090: 4, 0x101dce4c: 44, 0x101dd754: 100,
         0x101dff64: 60}
OWN = set(SIZES)

# ---- the callees faked on both sides
KNOB_CHANGE, VARY_CHANGE, WIN_DTOR, OP_DELETE = 0x101d0448, 0x101d9080, 0x101d9ba4, 0x10238218
CLOSE_WINDOW, SEND_MESSAGE, TIMER_STOP, DEFAULT_HANDLER, PTR_GLUE = 0x10193774, 0x101c9518, 0x101d8428, 0x101de318, 0x1023baa8
# the table entries the virtual calls load (never executed: __ptr_glue is faked)
TOK_WHAT, TOK_3C, TOK_SETTEXT, TOK_20, TOK_1C = 0x7001, 0x7002, 0x7003, 0x7004, 0x7005

# ---- the Mac's addresses of the globals
ADDR = dict(gUIHelpText=0x102af928, gYearEndPanel=0x10395b70, gUIKeyCapture=0x10395c08, gUIKeyboardWindow=0x10395c10,
            gUIProgressBar=0x10395c6c)
# ---- the host's offsets and sizes the module relies on (UIStatusPanelFields.mF1C, .mVtable, sizeof(captureKeyList))
HOST_LAYOUT = [0x1c, 0x20, 4]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def stub_knob_change(e):
    """InterfaceKnob::ChangeValue(long): records, then leaves the displayed angle the queue gives."""
    L = e.cl
    L.rec("KnobChange", (L.lab(e.r(3)), s32(e.r(4))))
    w32(e, e.r(3) + 0x148, L.ret("angle"))


def stub_vary_change(e):
    L = e.cl
    L.rec("VaryChange", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("vc") & MASK)


def stub_win_dtor(e):
    L = e.cl
    L.rec("~InterfaceWindow", (L.lab(e.r(3)), s32(e.r(4))))


def stub_delete(e):
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)),))


def stub_close(e):
    L = e.cl
    L.rec("CloseWindow", (L.lab(e.r(3)),))


def stub_send(e):
    L = e.cl
    L.rec("SendMessage", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("SendMessage") & MASK)


def stub_timer_stop(e):
    L = e.cl
    L.rec("TimerStop", (L.lab(e.r(3)),))


def stub_default(e):
    L = e.cl
    L.rec("Default", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("dflt") & MASK)


def stub_glue(e):
    """__ptr_glue: the virtual call through the table entry in r12 (the entry is a token here)."""
    L = e.cl
    tok = e.r(12)
    if tok == TOK_WHAT:
        L.rec("WhatAmI", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("WhatAmI") & MASK)
    elif tok == TOK_3C:
        L.rec("Slot3c", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("Slot3c") & MASK)
    elif tok == TOK_SETTEXT:
        L.rec("SetText", (L.lab(e.r(3)), e.r(4)))
        e.set_r(3, 1)
    elif tok == TOK_20:
        L.rec("Slot20", (L.lab(e.r(3)),))
    elif tok == TOK_1C:
        L.rec("Slot1c", (L.lab(e.r(3)),))
    else:
        raise AssertionError("unknown virtual token %#x" % tok)


for _addr, _fn in ((KNOB_CHANGE, stub_knob_change), (VARY_CHANGE, stub_vary_change), (WIN_DTOR, stub_win_dtor),
                   (OP_DELETE, stub_delete), (CLOSE_WINDOW, stub_close), (SEND_MESSAGE, stub_send),
                   (TIMER_STOP, stub_timer_stop), (DEFAULT_HANDLER, stub_default), (PTR_GLUE, stub_glue)):
    STUBS.internal(_addr, _fn)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides

NOBJ = 5


class Ppc:
    """One case on the PowerPC side: five 0x400-byte blocks, the fakes active, a fresh log."""

    def __init__(self, e, case):
        self.e = e
        self.objs = [e.alloc(0x400) for _ in range(NOBJ)]
        self.m = {a: i + 1 for i, a in enumerate(self.objs)}
        self.L = e.cl = CL.PpcLog(dict(self.m), case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def w(self, a, *ws):
        for i, v in enumerate(ws):
            w32(self.e, a + 4 * i, v)

    def rd(self, a):
        return self.e.u32(a)

    def lbl(self, v):
        return self.m.get(v, 0 if v == 0 else -1)

    def call(self, name, *args):
        return T.call_ppc(self.e, FN[name], [a & MASK for a in args])[0] & MASK

    def log(self):
        return self.L.log


class Host:
    """The same on the host library."""

    def __init__(self, lib, case):
        bind(lib)
        self.lib = lib
        self.objs = [lib.tpw_ikgv_alloc(0x400) for _ in range(NOBJ)]
        self.m = {a: i + 1 for i, a in enumerate(self.objs)}
        CL.host_begin(lib, {i + 1: a for i, a in enumerate(self.objs)}, case["queues"])

    def done(self):
        pass

    def lbl(self, v):
        return self.m.get(v, 0 if v == 0 else -1)

    def hcall(self, op, *args):
        a = (C.c_int64 * 16)(*args)
        out = (C.c_int64 * 8)()
        self.lib.tpw_ikgv_call(op, a, out)
        return list(out)

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ikgv_alloc.restype, lib.tpw_ikgv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ikgv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ikgv_call.restype, lib.tpw_ikgv_call.argtypes = None, [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ikgv_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run_with(side_cls, name_fn):
    def go(x, case):
        s = side_cls(x, case)
        try:
            res = name_fn(s, case)
            res["log"] = s.log()
            return res
        finally:
            s.done()
    return go


# ---------------------------------------------------------------------------------------------------------------- the cases


def ri(r):
    if r.random() < 0.3:
        return r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, 2, 100, 0x1000])
    return s32(r.getrandbits(32))


def case(r, **kw):
    c = {"queues": {"angle": [ri(r)], "vc": [ri(r)], "SendMessage": [ri(r)], "dflt": [ri(r)],
                    "WhatAmI": [r.choice([11, 12, 13]), r.choice([11, 12, 13])], "Slot3c": [r.randrange(1, 0x7FFFFFFF)]},
         "v": ri(r), "junk": [r.getrandbits(32) for _ in range(28)]}
    c["queues"].update(kw.pop("queues", {}))
    c.update(kw)
    return c


def test(name, p, h, edges, rnd):
    """A FnTest from the shared PowerPC and host bodies; `edges` is a list of (kwargs) applied to the random base."""
    def edge():
        r = random.Random(sum(map(ord, name)))
        for kw in edges:
            yield case(r, **kw)
    return FnTest(name, run_with(Ppc, p), run_with(Host, h), edge=edge, rnd=lambda r: case(r, **rnd(r)))


TESTS = []

# ---- the getters
for _name, _off, _op in (("InterfaceKnob::GetValue", 0x158, 0), ("InterfaceImage::GetValue", 0x13C, 1),
                         ("InterfaceLightPanel::GetValue", 0x138, 2), ("InterfaceBar::GetValue", 0x140, 3),
                         ("InterfaceVaryBox::GetValue", 0x140, 4)):
    def _p(s, c, _name=_name, _off=_off):
        o = s.objs[0]
        s.w(o + 0x100, *c["junk"])
        s.w(o + _off, c["v"])
        return {"answer": s.call(_name, o)}

    def _h(s, c, _op=_op):
        return {"answer": s.hcall(_op, s.objs[0], c["v"])[0] & MASK}

    TESTS.append(test(_name, _p, _h, [{"v": v} for v in (0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x12345678)], lambda r: {}))


# ---- InterfaceKnob::SetValue
def knob_set_p(s, c):
    o = s.objs[0]
    s.w(o + 0x144, c["drag"])
    s.w(o + 0x148, c["angle0"])
    r = s.call("InterfaceKnob::SetValue", o, c["v"])
    return {"answer": r, "drag": s.rd(o + 0x144), "angle": s.rd(o + 0x148)}


def knob_set_h(s, c):
    out = s.hcall(5, s.objs[0], c["v"], c["drag"], c["angle0"])
    return {"answer": out[0] & MASK, "drag": out[1] & MASK, "angle": out[2] & MASK}


TESTS.append(test("InterfaceKnob::SetValue", knob_set_p, knob_set_h,
                  [{"drag": 0, "angle0": 0, "queues": {"angle": [0]}}, {"drag": 5, "angle0": 7, "queues": {"angle": [7]}},
                   {"drag": 5, "angle0": 7, "queues": {"angle": [-1]}}, {"v": -1, "drag": -3, "angle0": 3, "queues": {"angle": [0x7FFFFFFF]}}],
                  lambda r: {"drag": ri(r), "angle0": ri(r)}))


# ---- the destructors
def dtor_test(name, op):
    def p(s, c):
        o = 0 if c["null"] else s.objs[0]
        if o:
            s.w(o + 0x118, 0xDEADBEEF)
        return {"answer": s.call(name, o, c["flag"]) == o}

    def h(s, c):
        o = 0 if c["null"] else s.objs[0]
        return {"answer": bool(s.hcall(op, o, c["flag"])[0])}

    edges = [{"null": n, "flag": f} for n in (False, True) for f in (0, 1, -1, 2, 0x7FFF, 0x8000, -0x8000, 0x10000, 0x10001)]
    return test(name, p, h, edges, lambda r: {"null": r.random() < 0.15, "flag": r.choice([0, 1, -1, 2, 0x10001, 0x8000, ri(r)])})


TESTS.append(dtor_test("InterfaceKnob::~InterfaceKnob", 6))
TESTS.append(dtor_test("InterfaceLightPanel::~InterfaceLightPanel", 7))
TESTS.append(dtor_test("InterfaceVaryBox::~InterfaceVaryBox", 8))


# ---- the vary box
def set_inc_p(s, c):
    o, src = s.objs[0], s.objs[1]
    s.w(src, c["v"])
    s.w(o + 0x148, c["old"])
    s.call("InterfaceVaryBox::SetInc", o, 0 if c["null"] else src)
    return {"inc": s.rd(o + 0x148)}


def set_inc_h(s, c):
    return {"inc": s.hcall(9, s.objs[0], c["v"], 1 if c["null"] else 0, c["old"])[0] & MASK}


TESTS.append(test("InterfaceVaryBox::SetInc", set_inc_p, set_inc_h,
                  [{"null": n, "v": v, "old": o} for n in (False, True) for v, o in ((0, 5), (-1, 0), (7, 7), (0x7FFFFFFF, -4))],
                  lambda r: {"null": r.random() < 0.2, "old": ri(r)}))


def vary_set_p(s, c):
    return {"answer": s.call("InterfaceVaryBox::SetValue", s.objs[0], c["v"])}


def vary_set_h(s, c):
    return {"answer": s.hcall(10, s.objs[0], c["v"])[0] & MASK}


TESTS.append(test("InterfaceVaryBox::SetValue", vary_set_p, vary_set_h,
                  [{"v": 0, "queues": {"vc": [0]}}, {"v": -5, "queues": {"vc": [1]}}, {"v": 3, "queues": {"vc": [-1]}}],
                  lambda r: {}))


def vary_minus_p(s, c):
    a = s.objs[0]
    b = a if c["same"] else s.objs[1]
    vt = s.e.alloc(0x40)
    s.w(vt + 0x14, TOK_WHAT)
    s.w(a + 0x118, vt)
    s.w(b + 0x118, vt)
    s.w(a + 0x140, c["v"])
    s.w(b + 0x140, c["v2"])
    return {"answer": s.call("InterfaceVaryBox::operator-", a, b)}


def vary_minus_h(s, c):
    a = s.objs[0]
    b = a if c["same"] else s.objs[1]
    return {"answer": s.hcall(11, a, b, c["v"], c["v2"])[0] & MASK}


_wh = lambda x, y: {"WhatAmI": [x, y], "angle": [0], "vc": [0], "SendMessage": [0], "dflt": [0], "Slot3c": [1]}
TESTS.append(test("InterfaceVaryBox::operator-", vary_minus_p, vary_minus_h,
                  [{"same": False, "v": 10, "v2": 3, "queues": _wh(12, 12)}, {"same": False, "v": 3, "v2": 10, "queues": _wh(12, 12)},
                   {"same": False, "v": 10, "v2": 3, "queues": _wh(12, 11)}, {"same": False, "v": 10, "v2": 3, "queues": _wh(11, 12)},
                   {"same": True, "v": 4, "v2": 9, "queues": _wh(12, 12)},
                   {"same": False, "v": -0x80000000, "v2": 1, "queues": _wh(5, 5)},
                   {"same": False, "v": 0x7FFFFFFF, "v2": -1, "queues": _wh(5, 5)}],
                  lambda r: {"same": r.random() < 0.1, "v2": ri(r)}))


# ---- the upgrade status panel
def update_p(s, c):
    panel, win = s.objs[0], s.objs[1]
    table, vt = s.e.alloc(0x100), s.e.alloc(0x100)
    s.w(panel + 0x1C, win)
    s.w(panel + 0x20, table)
    s.w(table + 0x3C, TOK_3C)
    s.w(table + 0x20, TOK_20)
    s.w(table + 0x1C, TOK_1C)
    s.w(win + 0x118, vt)
    s.w(vt + 0x2C, TOK_SETTEXT)
    s.call("UpgradeStatusPanel::UpdateWindow", panel)
    return {}


def update_h(s, c):
    s.hcall(12, s.objs[0], s.objs[1])
    return {}


TESTS.append(test("UpgradeStatusPanel::UpdateWindow", update_p, update_h, [{}, {"queues": {"Slot3c": [1]}}, {"queues": {"Slot3c": [0x7FFFFFF0]}}],
                  lambda r: {}))


def plain_test(name, op):
    def p(s, c):
        s.w(s.objs[0] + 0x100, *c["junk"])
        s.call(name, s.objs[0])
        return {}

    def h(s, c):
        s.hcall(op, s.objs[0])
        return {}

    return test(name, p, h, [{}], lambda r: {})


TESTS.append(plain_test("UpgradeStatusPanel::Remove", 13))
TESTS.append(plain_test("UpgradeStatusPanel::DeInitWindow", 14))
TESTS.append(plain_test("UpgradeStatusPanel::DeInitStatusWindow", 15))
TESTS.append(plain_test("InterfaceTextBox::DrawFunction", 16))


# ---- RemoveYearEndPanel
def year_p(s, c):
    o = 0 if c["null"] else s.objs[0]
    s.w(ADDR["gYearEndPanel"], o)
    s.call("RemoveYearEndPanel")
    return {"panel": s.lbl(s.rd(ADDR["gYearEndPanel"]))}


def year_h(s, c):
    o = 0 if c["null"] else s.objs[0]
    return {"panel": s.lbl(s.hcall(17, o)[0])}


TESTS.append(test("RemoveYearEndPanel", year_p, year_h, [{"null": False}, {"null": True}], lambda r: {"null": r.random() < 0.3}))


# ---- UI_HelpTextEnabled
def help_p(s, c):
    o = s.objs[0]
    s.w(ADDR["gUIHelpText"], o)
    s.w(o, ((c["en"] & 0xFFFF) << 16) | (c["junk"][0] & 0xFFFF))
    return {"answer": s.call("UI_HelpTextEnabled") & 0xFF}


def help_h(s, c):
    return {"answer": s.hcall(18, s.objs[0], c["en"])[0] & 0xFF}


TESTS.append(test("UI_HelpTextEnabled", help_p, help_h, [{"en": e} for e in (0, 1, -1, 2, 0x100, 0x8000, 0x7FFF, 0xFF00, -0x8000)],
                  lambda r: {"en": r.choice([0, 1, -1, r.getrandbits(16)])}))


# ---- InterfaceWindow::CaptureKeys
def keys_args(s, c):
    win, other, keys, oldkeys = s.objs[0], s.objs[1], s.objs[2], s.objs[3]
    kbd = (0, win, other)[c["kbd"]]
    return win, (0 if c["nokeys"] else keys), kbd, (oldkeys if c["oldkeys"] else 0), (other if c["oldwin"] else 0)


def keys_p(s, c):
    win, keys, kbd, oldkeys, oldwin = keys_args(s, c)
    s.w(ADDR["gUIKeyboardWindow"], kbd)
    s.w(ADDR["gUIKeyCapture"], oldkeys, oldwin)
    r = s.call("InterfaceWindow::CaptureKeys", win, keys)
    return {"answer": r, "keys": s.lbl(s.rd(ADDR["gUIKeyCapture"])), "window": s.lbl(s.rd(ADDR["gUIKeyCapture"] + 4))}


def keys_h(s, c):
    out = s.hcall(19, *keys_args(s, c))
    return {"answer": out[0] & MASK, "keys": s.lbl(out[1]), "window": s.lbl(out[2])}


TESTS.append(test("InterfaceWindow::CaptureKeys", keys_p, keys_h,
                  [{"kbd": k, "nokeys": n, "oldkeys": 1, "oldwin": 1} for k in (0, 1, 2) for n in (False, True)] +
                  [{"kbd": 1, "nokeys": False, "oldkeys": 0, "oldwin": 0}],
                  lambda r: {"kbd": r.randrange(3), "nokeys": r.random() < 0.2, "oldkeys": r.randrange(2), "oldwin": r.randrange(2)}))


# ---- ProgBarMsg
def prog_args(s, c):
    prog = (0, s.objs[0], s.objs[1])[c["prog"]]
    return s.objs[0], c["msg"], c["a"], c["b"], prog


def prog_p(s, c):
    win, msg, a, b, prog = prog_args(s, c)
    s.w(ADDR["gUIProgressBar"], prog)
    r = s.call("ProgBarMsg", win, msg, a, b)
    return {"answer": r, "bar": s.lbl(s.rd(ADDR["gUIProgressBar"]))}


def prog_h(s, c):
    out = s.hcall(20, *prog_args(s, c))
    return {"answer": out[0] & MASK, "bar": s.lbl(out[1])}


TESTS.append(test("ProgBarMsg", prog_p, prog_h,
                  [{"msg": m, "a": 5, "b": -7, "prog": p} for m in (0x14, 0x13, 0x15, 0, -1, 0x10014) for p in (0, 1, 2)],
                  lambda r: {"msg": r.choice([0x14, 0x14, 0x13, 0x15, ri(r)]), "a": ri(r), "b": ri(r), "prog": r.randrange(3)}))


# ---- InterfaceWindow::stopAnimTimer
def stop_args(s, c):
    timers = [s.objs[1 + i] if c["have"][i] else 0 for i in range(4)]
    return [s.objs[0]] + timers + [c["id"]]


def stop_p(s, c):
    args = stop_args(s, c)
    for i in range(4):
        s.w(args[0] + 0x120 + 4 * i, args[1 + i])
    return {"answer": s.call("InterfaceWindow::stopAnimTimer", args[0], args[5])}


def stop_h(s, c):
    return {"answer": s.hcall(21, *stop_args(s, c))[0] & MASK}


def stop_id(r):
    return r.randrange(4) + r.choice([0, 0x80080])


TESTS.append(test("InterfaceWindow::stopAnimTimer", stop_p, stop_h,
                  [{"id": i + b, "have": h} for i in range(4) for b in (0, 0x80080) for h in ([True] * 4, [False] * 4, [i % 2 == 0] * 2 + [i % 2 == 1] * 2)],
                  lambda r: {"id": stop_id(r), "have": [r.random() < 0.6 for _ in range(4)]}))
