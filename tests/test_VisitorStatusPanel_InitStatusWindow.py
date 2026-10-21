"""Unit v5521, VisitorStatusPanel::InitStatusWindow, against libtpw_unit_VisitorStatusPanel_InitStatusWindow.dylib
(tests/unit_VisitorStatusPanel_InitStatusWindow.cpp), main app ("SimTheme Park"), src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_visitorStatus.cpp:
  VisitorStatusPanel::InitStatusWindow 0x10142238

The panel is a block with its window at +0x10; the child windows are small blocks whose first word is their id, found by a fake GetWindow.
Every callee (GetWindow, operator new, the constructors, SetClientDrawObject, GetFontTableEntry, SetFont, UIMain_GetString,
SetClientDrawText, UIMain_IndentClient and the virtual SetValue) is a fake on both sides that logs what the real one reads; the objects
the fake operator new makes carry their number in the first word, so the log names them. The queues: "New" (operator new answers NULL, one
value per call) and "Win:14130" (GetWindow of id 0x3732 answers NULL).

Compared: the event log and the panel's window word. No fused operations. Not tested: a NULL child window other than 0x3732 (the original
calls through it unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_VisitorStatusPanel_InitStatusWindow"
MASK = 0xFFFFFFFF

FN = 0x10142238
SIZES = {FN: 2092}
OWN = set(SIZES)

GETWINDOW, NEW, SPRITE, CASH, NUMBER, TIME, GRADIENT = 0x101dbe40, 0x102384b8, 0x1018d11c, 0x1018de60, 0x1018d930, 0x1018edf4, 0x101898c8
SETOBJ, GETFONT, SETFONT, GETSTR, SETTEXT, INDENT, PTRGLUE = (0x101db624, 0x10180270, 0x101da118, 0x10180468, 0x101de130, 0x10180940,
                                                              0x1023baa8)
HOST_LAYOUT = [0x10]   # the panel's window, a Ptr32 (an arena offset) since unit v5L31

STATE = {"wintab": 0, "fonts": 0, "texts": 0, "news": 0}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def wid(e, a):
    return e.u32(a) if a else MASK + 1 - 1 and -1


def mark(e, a):
    return e.u32(a) if a else 0


def wid(e, a):
    return T.s32(e.u32(a)) if a else -1


def st_getwindow(e):
    w, i = e.r(3), T.s32(e.r(4))
    e.cl.rec("GetWindow", (wid(e, w), i))
    if i == 0x3732 and e.cl.ret("Win:%d" % i) != 0:
        e.set_r(3, 0)
    else:
        e.set_r(3, e.u32(STATE["wintab"] + 4 * (i - 0x3724)))


def st_new(e):
    size = e.r(3)
    null = e.cl.ret("New")
    STATE["news"] += 1
    n = STATE["news"]
    e.cl.rec("OperatorNew", (size, 1 if null else 0))
    if null:
        e.set_r(3, 0)
        return
    a = e.alloc(size)
    e.write(a, struct.pack(">I", n))
    e.set_r(3, a)


def ctor(name):
    def f(e):
        e.cl.rec(name, (mark(e, e.r(3)),))
    return f


def st_gradient(e):
    s = e.cstr(e.r(4))
    bs = s if isinstance(s, bytes) else s.encode("latin1")
    h = 0
    for c in bs:
        h = (h * 31 + c) & MASK
    e.cl.rec("GradientCtor", (mark(e, e.r(3)), h))


def st_setobj(e):
    e.cl.rec("SetClientDrawObject", (wid(e, e.r(3)), mark(e, e.r(4))))


def st_getfont(e):
    i = e.r(3)
    e.cl.rec("GetFontTableEntry", (i,))
    e.set_r(3, STATE["fonts"] + 4 * (i & 15))


def st_setfont(e):
    e.cl.rec("SetFont", (wid(e, e.r(3)), mark(e, e.r(4))))


def st_getstr(e):
    i = T.s32(e.r(3))
    e.cl.rec("UIMain_GetString", (i,))
    e.set_r(3, STATE["texts"] + 4 * (i & 255))


def st_settext(e):
    t = e.r(4)
    e.cl.rec("SetClientDrawText", (wid(e, e.r(3)), struct.unpack(">H", e.read(t, 2))[0] if t else -1))


def st_indent(e):
    e.cl.rec("UIMain_IndentClient", (wid(e, e.r(3)), T.s16(e.r(4)), T.s16(e.r(5))))


def st_ptrglue(e):
    e.cl.rec("SetValue", (wid(e, e.r(3)), T.s32(e.r(4))))


for addr, fn in ((GETWINDOW, st_getwindow), (NEW, st_new), (SPRITE, ctor("SpriteCtor")), (CASH, ctor("CashCtor")),
                 (NUMBER, ctor("NumberCtor")), (TIME, ctor("TimeCtor")), (GRADIENT, st_gradient), (SETOBJ, st_setobj),
                 (GETFONT, st_getfont), (SETFONT, st_setfont), (GETSTR, st_getstr), (SETTEXT, st_settext), (INDENT, st_indent),
                 (PTRGLUE, st_ptrglue)):
    def keep_toc(e, fn=fn):
        # the glue calls (operator new, __ptr_glue) save r2 at 0x14(r1) and the caller reloads it afterwards
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    STUBS.internal(addr, keep_toc)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    STATE["wintab"] = T.persist_alloc(e, 64)
    STATE["fonts"] = T.persist_alloc(e, 64, b"".join(struct.pack(">I", 0xF000 + i) for i in range(16)))
    STATE["texts"] = T.persist_alloc(e, 1024, b"".join(struct.pack(">HH", 0x4100 + i, 0) for i in range(256)))


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    psize = 4

    def ptr(self, a, p):
        self.write(a, struct.pack(">I", p & MASK))

    def rd_ptr(self, a):
        return struct.unpack(">I", self.read(a, 4))[0]

    def p32(self, p):
        return p

    def __init__(self, e, case):
        self.e = e
        STATE["news"] = 0
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def wintab(self):
        return STATE["wintab"]

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
    psize = 8

    def ptr(self, a, p):
        self.write(a, struct.pack("<Q", p))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def p32(self, p):
        return p - self.lib.tpw_vsp_arena_base() if p else 0

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_vsp_reset()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def wintab(self):
        return self.lib.tpw_vsp_addr(b"wintab")

    def alloc(self, n):
        return self.lib.tpw_vsp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, panel):
        self.lib.tpw_vsp_InitStatusWindow(panel)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_vsp_addr.restype, lib.tpw_vsp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_vsp_alloc.restype, lib.tpw_vsp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_vsp_arena_base.restype, lib.tpw_vsp_arena_base.argtypes = C.c_uint64, []
    lib.tpw_vsp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_vsp_reset.argtypes = []
    lib.tpw_vsp_InitStatusWindow.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_vsp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    root = s.alloc(16)
    s.words(root, [0xAAAA])
    tab = s.wintab()
    for i in range(16):
        b = s.alloc(0x140)
        s.words(b, [0x3724 + i])
        s.ptr(b + 0x118, s.alloc(0x40))      # the vtable the original reads (+0x118, then slot +0x24) before the SetValue call
        s.ptr(tab + s.psize * i, b)
    panel = s.alloc(0x40)
    s.words(panel + 0x10, [0 if case["nopanel"] else s.p32(root)])
    return panel


def run(s, case):
    try:
        panel = build(s, case)
        s.call(panel)
        return {"log": s.log(), "window": s.rd_words(panel + 0x10, 1)[0] != 0}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, nopanel=False, nowin=0, nulls=()):
    news = [1 if i in nulls else 0 for i in range(20)]
    return {"nopanel": nopanel, "queues": {"New": news, "Win:%d" % 0x3732: [nowin]}}


def edges():
    r = random.Random(5521)
    yield mk(r)
    yield mk(r, nopanel=True)
    yield mk(r, nowin=1)
    for i in range(16):
        yield mk(r, nulls=(i,))
    yield mk(r, nowin=1, nulls=(13,))
    yield mk(r, nulls=tuple(range(20)))


def rnd(r):
    return mk(r, nopanel=r.random() < 0.1, nowin=r.choice([0, 1]), nulls=tuple(i for i in range(20) if r.random() < 0.15))


TESTS = [
    FnTest("VisitorStatusPanel::InitStatusWindow", ppc, host, edge=edges, rnd=rnd),
]
