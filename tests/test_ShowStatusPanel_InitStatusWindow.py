"""Unit v51018, ShowStatusPanel::InitStatusWindow, against libtpw_unit_ShowStatusPanel_InitStatusWindow.dylib
(tests/unit_ShowStatusPanel_InitStatusWindow.cpp), main app, src/ShowStatusPanel_UpdateStatusWindow_ShowStatusPanel_InitStatusWindow.cpp:
  ShowStatusPanel::InitStatusWindow 0x101c0f80

A case is a panel whose window at +0x10 is NULL or a window with sixteen children (ids 0xa092 to 0xa0a1). The world is built by one piece
of Python code in both memories. The callees outside the unit (GetWindow, SetClientDrawObject, SetFont, SetClientDrawText,
GetFontTableEntry, UIMain_GetString, UIMain_IndentClient, operator new, the three constructors and the virtual SetValue) are fakes on
both sides that record their arguments; windows and the font carry fixed labels, new objects take labels 100 on.

What is compared: the event log and the panel's words. No fused operations. Not tested: nothing is left out; the original does not test
GetWindow's answer for NULL, and neither does the fake ever answer NULL.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ShowStatusPanel_InitStatusWindow"
MASK = 0xFFFFFFFF

FN = 0x101C0F80
SIZES = {FN: 2128}
OWN = set(SIZES)

GETWINDOW, SETOBJ, SETFONT, SETTEXT = 0x101DBE40, 0x101DB624, 0x101DA118, 0x101DE130
FONTENTRY, GETSTRING, INDENT = 0x10180270, 0x10180468, 0x10180940
NEW, SPRITE, CASH, GRAD, GLUE = 0x102384B8, 0x1018D11C, 0x1018DE60, 0x101898C8, 0x1023BAA8

ADDR = dict(mesh=0x1024E668)
HOST_LAYOUT = [0x10]
FIRST_ID = 0xA092
KIDS = 16

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_getwindow(e):
    e.cl.rec("GetWindow", (e.cl.lab(e.r(3)), T.s32(e.r(4))))
    e.set_r(3, e.cur.kids[T.s32(e.r(4)) - FIRST_ID] if 0 <= T.s32(e.r(4)) - FIRST_ID < KIDS else 0)


def stub_setobj(e):
    e.cl.rec("SetObj", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def stub_setfont(e):
    e.cl.rec("SetFont", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def stub_settext(e):
    p = e.r(4)
    first = struct.unpack(">H", e.read(p, 2))[0] if p else -1
    e.cl.rec("SetText", (e.cl.lab(e.r(3)), first))


def stub_fontentry(e):
    e.cl.rec("Font", (T.s32(e.r(3)),))
    e.set_r(3, e.cur.font)


def stub_getstring(e):
    i = T.s32(e.r(3))
    e.cl.rec("GetString", (i,))
    p = e.alloc(8)
    e.write(p, struct.pack(">HHH", 0x100 + i, 0x41, 0))
    e.set_r(3, p)


def stub_indent(e):
    def s16(v):
        return T.s16(v)
    e.cl.rec("Indent", (e.cl.lab(e.r(3)), s16(e.r(4)), s16(e.r(5))))


def stub_new(e):
    n = e.r(3) & MASK
    e.cl.rec("New", (n,))
    e.set_r(3, e.alloc(n))


def stub_setvalue(e):
    e.cl.rec("SetValue", (e.cl.lab(e.r(3)), T.s32(e.r(4))))


GRAD_REC = CL.recorder("GradCtor", "N")


def stub_grad(e):
    e.cl.rec("GradMesh", ((e.r(4) & MASK) - ADDR["mesh"],))
    GRAD_REC(e)


STUBS.internal(GETWINDOW, stub_getwindow)
STUBS.internal(SETOBJ, stub_setobj)
STUBS.internal(SETFONT, stub_setfont)
STUBS.internal(SETTEXT, stub_settext)
STUBS.internal(FONTENTRY, stub_fontentry)
STUBS.internal(GETSTRING, stub_getstring)
STUBS.internal(INDENT, stub_indent)
STUBS.internal(NEW, stub_new)
STUBS.internal(SPRITE, CL.recorder("SpriteCtor", "N"))
STUBS.internal(CASH, CL.recorder("CashCtor", "N"))
STUBS.internal(GRAD, stub_grad)
STUBS.internal(GLUE, stub_setvalue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.parent = e.alloc(0x200)
        self.kids = [e.alloc(0x200) for _ in range(KIDS)]
        self.font = e.alloc(16)
        table = e.alloc(0x40)
        for k in self.kids:
            e.write(k + 0x118, struct.pack(">I", table))
        fixed = {self.parent: 1, self.font: 50}
        for i, k in enumerate(self.kids):
            fixed[k] = 2 + i
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        e.cur = self
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def ptr(self, a):
        return a

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, panel):
        T.call_ppc(self.e, FN, [panel & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.parent = lib.tpw_ssp_alloc(0x200)
        fixed = {1: self.parent, 50: lib.tpw_ssp_addr(b"font")}
        for i in range(KIDS):
            fixed[2 + i] = lib.tpw_ssp_addr(b"kid%d" % i)
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def ptr(self, a):
        return self.lib.tpw_ssp_offset(a)

    def alloc(self, n):
        return self.lib.tpw_ssp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, panel):
        self.lib.tpw_ssp_init(panel)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ssp_addr.restype, lib.tpw_ssp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ssp_alloc.restype, lib.tpw_ssp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ssp_offset.restype, lib.tpw_ssp_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_ssp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ssp_init.restype, lib.tpw_ssp_init.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ssp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        panel = s.alloc(0x30)
        s.words(panel, [0x11111111, 0x22222222, 0x33333333, 0x44444444, 0, 0, 0, 0x55555555])
        if case["window"]:
            s.words(panel + 0x10, [s.ptr(s.parent)])
        s.call(panel)
        return {"log": s.log(), "panel": s.rd_words(panel, 12) if not case["window"] else s.rd_words(panel, 4)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def edges():
    for w in (True, False):
        yield {"window": w, "queues": {}}


def rnd(r):
    return {"window": r.random() < 0.9, "queues": {}}


TESTS = [
    FnTest("ShowStatusPanel::InitStatusWindow", ppc, host, edge=edges, rnd=rnd),
]
