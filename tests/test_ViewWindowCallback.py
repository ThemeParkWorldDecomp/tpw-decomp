"""Unit v5753, two status panel window handlers, against libtpw_unit_ViewWindowCallback.dylib (tests/unit_ViewWindowCallback.cpp), main app,
src/game/ui/ui_StatusPanel.cpp:
  ViewWindowCallback 0x10193bac, StatusPanelTitleMsg 0x10193e7c

A case is a world and a call, built by one piece of Python code in both memories. The callees outside the unit (getExtraParam,
GetThingId, UIMain_ZoomToThing, CallDefaultWindowHandler, SetFontColour, UI_TITLE_COLOUR, __ptr_glue) are fakes on both sides that record
their arguments; the windows are blocks with a marker word the fakes log, the panel is a block with a table whose entries 0x34 and 0x40 are
fake functions that log the panel's marker.

The world: window (marker, extra parameter pointer), panel (marker, table), the thing (its id), gStaffRoomPanelThing, the default handler's
answer. Compared: the answer and the event log.

No fused operations. Not tested: a NULL extra parameter (the original calls through it unchecked).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ViewWindowCallback"
MASK = 0xFFFFFFFF

FN_VIEW, FN_TITLE = 0x10193BAC, 0x10193E7C
SIZES = {FN_VIEW: 140, FN_TITLE: 240}
OWN = set(SIZES)

GETEXTRA, GETID, ZOOM, PTRGLUE = 0x101DD030, 0x1012E1F8, 0x10181B00, 0x1023BAA8
CDWH, SETFONT, TITLECOL = 0x101DE318, 0x101DA3AC, 0x1018BD3C

ADDR = dict(thing=0x103947A4)
HOST_LAYOUT = [0x20, 0]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def wmark(e, p):
    return e.u32(p) & MASK


def stub_getextra(e):
    e.cl.rec("getExtraParam", (wmark(e, e.r(3)), T.s16(e.r(4))))
    e.set_r(3, e.u32(e.r(3) + 4) & MASK)


def stub_getid(e):
    ident = struct.unpack(">H", e.read(e.r(4), 2))[0]
    e.cl.rec("GetThingId", (ident,))
    e.write(e.r(3), struct.pack(">H", ident))


def stub_zoom(e):
    e.cl.rec("UIMain_ZoomToThing", ((e.r(3) >> 16) & 0xFFFF,))


def stub_glue(e):
    e.cl.rec("slot", (e.u32(e.r(12)) & MASK, wmark(e, e.r(3) + 0x24)))


def stub_cdwh(e):
    L = e.cl
    L.rec("CallDefaultWindowHandler", (wmark(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("CallDefaultWindowHandler") & MASK)


def stub_setfont(e):
    e.cl.rec("SetFontColour", (wmark(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))


def stub_title(e):
    e.cl.rec("UI_TITLE_COLOUR", (wmark(e, e.r(3)),))


STUBS.internal(GETEXTRA, stub_getextra)
STUBS.internal(GETID, stub_getid)
STUBS.internal(ZOOM, stub_zoom)
STUBS.internal(PTRGLUE, stub_glue)
STUBS.internal(CDWH, stub_cdwh)
STUBS.internal(SETFONT, stub_setfont)
STUBS.internal(TITLECOL, stub_title)


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

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def ptr(self, a):
        return a

    def setptr(self, addr, a):
        self.words(addr, [a])

    def fnblock(self, a, tag, name):
        self.words(a, [tag, 0])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, w, m, a, b):
        r3, _, _ = T.call_ppc(self.e, fn, [w, m & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_vwc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_vwc_alloc(n)

    def ptr(self, a):
        return self.lib.tpw_vwc_off(a)

    def setptr(self, addr, a):
        self.write(addr, struct.pack("<Q", a))

    def fnblock(self, a, tag, name):
        self.write(a, struct.pack("<Q", self.g(name)))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, w, m, a, b):
        f = self.lib.tpw_vwc_view if fn == FN_VIEW else self.lib.tpw_vwc_title
        return f(w, s32(m), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_vwc_addr.restype, lib.tpw_vwc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_vwc_alloc.restype, lib.tpw_vwc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_vwc_off.restype, lib.tpw_vwc_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_vwc_layout.argtypes = [C.POINTER(C.c_int64)]
    for f in (lib.tpw_vwc_view, lib.tpw_vwc_title):
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_vwc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    """Window at the returned address; panel with table; thing."""
    thing = s.alloc(16)
    s.write(thing, struct.pack(s.fmt[0] + "H", case["id"]))
    s.setptr(s.g("thing"), thing)
    fn34, fn40 = s.alloc(16), s.alloc(16)
    s.fnblock(fn34, 0x34, "fn34")
    s.fnblock(fn40, 0x40, "fn40")
    table = s.alloc(0x60)
    s.words(table + 0x34, [s.ptr(fn34)])
    s.words(table + 0x40, [s.ptr(fn40)])
    panel = s.alloc(0x30)
    s.words(panel + 0x20, [s.ptr(table)])
    s.words(panel + 0x24, [case["pmark"]])
    window = s.alloc(0x20)
    s.words(window, [case["wmark"], s.ptr(panel)])
    return window


def run(s, case):
    try:
        w = build(s, case)
        fn = FN_VIEW if case["op"] == "v" else FN_TITLE
        answer = s.call(fn, w, *case["args"])
        return {"answer": answer, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


MSGS = [0x10006, 0x10005, 0x10007, 0x10000, 0x10001, 0x10002, 0x10003, 0, -1, 0x7FFFFFFF, 0x80000000, 0x20006, 6, 0x10006 | 0x100000]


def mk(r, op, m=None, a=None):
    c = {"op": op, "id": r.randrange(0x10000), "pmark": r.randrange(1, 1 << 32), "wmark": r.randrange(1, 1 << 32),
         "queues": {"CallDefaultWindowHandler": [r.choice([0, 1, -1, r.randrange(1 << 32)]) & MASK]}}
    c["args"] = [r.choice(MSGS) if m is None else m, r.choice([0, 1, -1, r.randrange(1 << 32)]) if a is None else a,
                 r.randrange(1 << 32)]
    return c


def edges(op):
    def gen():
        r = random.Random(5753 + ord(op))
        for m in MSGS:
            for a in (0, 1, -1):
                yield mk(r, op, m, a)
    return gen


def rnd(op):
    return lambda r: mk(r, op)


TESTS = [
    FnTest("ViewWindowCallback", ppc, host, edge=edges("v"), rnd=rnd("v")),
    FnTest("StatusPanelTitleMsg", ppc, host, edge=edges("t"), rnd=rnd("t")),
]
