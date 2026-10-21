"""Unit v5771, the main menu object's Show, Hide and destructor, against libtpw_unit_UIMenu_Show.dylib (tests/unit_UIMenu_Show.cpp), main
app ("SimTheme Park"), src/game/ui/UIMenu.cpp:
  UIMenu::Show 0x1018b0e0, UIMenu::Hide 0x1018b1fc, UIMenu::~UIMenu 0x1018ac8c

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian for the original, host
memory little-endian for the translation). The callees outside the unit (InterfaceWindow::Show, link, setZ, CaptureKb, UnCaptureKb,
unlink, UI_GetHighestZ, UIMain_CheckPanel, UIMain_DecideKbCapture, UI_SendMessage, operator delete) are fakes on both sides that record
their arguments; the queues "Z" and "Msg" give the answers of UI_GetHighestZ and UI_SendMessage.

The world of a case: the menu object (three free words, the shown word, the window pointer), the menu window and the root window
(gUIRootWindow). What is compared: the answer (the destructor's, as a label), the menu's words, the window pointer, the event log.

No fused operations. Not tested: nothing is left out (Hide with a NULL window is run: the original calls Show on it unchecked).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UIMenu_Show"
MASK = 0xFFFFFFFF

FN_SHOW, FN_HIDE, FN_DTOR = 0x1018B0E0, 0x1018B1FC, 0x1018AC8C
SIZES = {FN_SHOW: 248, FN_HIDE: 120, FN_DTOR: 112}
OWN = set(SIZES)

C_WSHOW, C_ZHIGH, C_LINK, C_SETZ, C_CAPT = 0x101DC1EC, 0x101CA880, 0x101DDF20, 0x101DD8BC, 0x101DCCEC
C_UNLINK, C_UNCAPT, C_CHECK, C_DECIDE = 0x101DE084, 0x101DCDB0, 0x10180514, 0x1018116C
C_SEND, C_DELETE = 0x101C9518, 0x10238218

ADDR = dict(root=0x10395C24)
HOST_LAYOUT = [0x0C, 0x10, 0x18]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lb(e, a):
    return e.cl.lab(a)


STUBS.internal(C_WSHOW, lambda e: e.cl.rec("Show", (lb(e, e.r(3)), T.s16(e.r(4)))))
STUBS.internal(C_UNLINK, lambda e: e.cl.rec("unlink", (lb(e, e.r(3)),)))
STUBS.internal(C_LINK, lambda e: e.cl.rec("link", (lb(e, e.r(3)), lb(e, e.r(4)), T.s8(e.r(5)), T.s8(e.r(6)))))
STUBS.internal(C_SETZ, lambda e: e.cl.rec("setZ", (lb(e, e.r(3)), T.s16(e.r(4)))))
STUBS.internal(C_CAPT, lambda e: e.cl.rec("CaptureKb", (lb(e, e.r(3)), T.s32(e.r(4)))))
STUBS.internal(C_UNCAPT, lambda e: e.cl.rec("UnCaptureKb", (lb(e, e.r(3)), T.s32(e.r(4)))))
STUBS.internal(C_CHECK, lambda e: (e.cl.rec("UIMain_CheckPanel", ()), e.set_r(3, 0)))
STUBS.internal(C_DECIDE, lambda e: e.cl.rec("UIMain_DecideKbCapture", ()))


def stub_z(e):
    L = e.cl
    L.rec("UI_GetHighestZ", (lb(e, e.r(3)), lb(e, e.r(4))))
    e.set_r(3, L.ret("Z") & MASK)


def stub_send(e):
    L = e.cl
    L.rec("UI_SendMessage", (lb(e, e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))))
    e.set_r(3, L.ret("Msg") & MASK)


STUBS.internal(C_ZHIGH, stub_z)
STUBS.internal(C_SEND, stub_send)
STUBS.internal(C_DELETE, lambda e: e.cl.rec("delete", (lb(e, e.r(3)),)))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def lab_of(self, p):
        return self.labels.get(p, -1) if p else 0


class Ppc(Side):
    fmt = ">I"
    psz = 4

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.saved = STUBS.activate(e)

    def begin(self, fixed):
        self.labels = {a: n for n, a in fixed.items()}
        self.L = self.e.cl = CL.PpcLog(dict(self.labels), self.case["queues"])

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

    def setptr(self, a, p):
        self.words(a, [p])

    def rdptr(self, a):
        return self.rd_words(a, 1)[0]

    def log(self):
        return self.L.log

    def call(self, op, menu, arg):
        if op == "show":
            T.call_ppc(self.e, FN_SHOW, [menu, arg & MASK])
            return 0
        if op == "hide":
            T.call_ppc(self.e, FN_HIDE, [menu])
            return 0
        r3, _, _ = T.call_ppc(self.e, FN_DTOR, [menu, arg & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)

    def begin(self, fixed):
        self.labels = {a: n for n, a in fixed.items()}
        CL.host_begin(self.lib, fixed_inv(fixed), self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_menu_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_menu_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setptr(self, a, p):
        self.write(a, struct.pack("<Q", p))

    def rdptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, menu, arg):
        if op == "show":
            self.lib.tpw_menu_show(menu, T.s32(arg))
            return 0
        if op == "hide":
            self.lib.tpw_menu_hide(menu)
            return 0
        return self.lib.tpw_menu_dtor(menu, T.s32(arg))


def fixed_inv(fixed):
    return dict(fixed)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_menu_addr.restype, lib.tpw_menu_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_menu_alloc.restype, lib.tpw_menu_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_menu_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_menu_show.argtypes = [C.c_uint64, C.c_int32]
    lib.tpw_menu_hide.argtypes = [C.c_uint64]
    lib.tpw_menu_dtor.restype, lib.tpw_menu_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_menu_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.menu = s.alloc(0x18)
    s.win = s.alloc(0x40)
    s.root = s.alloc(0x40)
    s.other = s.alloc(0x40)
    s.begin({1: s.menu, 2: s.win, 3: s.root, 4: s.other})
    s.words(s.menu, case["words"] + [case["shown"]])
    s.setptr(s.menu + 0x10, s.win if case["haswin"] else 0)
    s.setptr(s.g("root"), s.root)


def observe(s, case, answer):
    return {"answer": s.lab_of(answer) if case["op"] == "dtor" else 0, "log": s.log(),
            "words": s.rd_words(s.menu, 4), "window": s.lab_of(s.rdptr(s.menu + 0x10)),
            "root": s.lab_of(s.rdptr(s.g("root")))}


def run(s, case):
    try:
        build(s, case)
        menu = 0 if case.get("nullmenu") else s.menu
        return observe(s, case, s.call(case["op"], menu, case["arg"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


ZS = [0, 1, -1, 5, 0x7FFE, 0x7FFF, 0x8000, 0xFFFF, 0x12345, -0x8001, -32768, 100]
ARGS = [0, 1, -1, 5, 0x10000, -0x10000, 0x7FFF, 0x8000, 0xFFFF, 0x18000, -0x8000, -0x7FFF, 0x80000000]


def base(r, op, shown=None, haswin=None, arg=None, z=None):
    c = {"op": op, "words": [r.getrandbits(32) for _ in range(3)],
         "shown": r.choice([0, 1, 0xFFFFFFFF, 7]) if shown is None else shown,
         "haswin": r.choice([0, 1]) if haswin is None else haswin,
         "arg": (r.choice(ARGS) if r.random() < 0.7 else r.getrandbits(32)) if arg is None else arg,
         "queues": {"Z": [r.choice(ZS) if z is None else z], "Msg": [r.getrandbits(32)]}}
    return c


def show_edges():
    r = random.Random(11)
    for shown in (0, 1, 0xFFFFFFFF):
        for haswin in (0, 1):
            for arg in (0, 1, -1, 5):
                for z in (0, 0x7FFE, 0x7FFF, -1):
                    yield base(r, "show", shown, haswin, arg, z)


def show_random(r):
    return base(r, "show")


def hide_edges():
    r = random.Random(12)
    for shown in (0, 1, 0xFFFFFFFF):
        for haswin in (0, 1):
            yield base(r, "hide", shown, haswin)


def hide_random(r):
    return base(r, "hide")


def dtor_edges():
    r = random.Random(13)
    for haswin in (0, 1):
        for arg in (0, 1, -1, 0x7FFF, 0x8000, 0x10001, 0xFFFF, 0x18000, 0x80000000):
            yield base(r, "dtor", None, haswin, arg)
    c = base(r, "dtor", None, 1, 1)
    c["nullmenu"] = True
    yield c
    c = base(r, "dtor", None, 0, 0)
    c["nullmenu"] = True
    yield c


def dtor_random(r):
    c = base(r, "dtor")
    if r.random() < 0.1:
        c["nullmenu"] = True
    return c


TESTS = [
    FnTest("UIMenu::Show", ppc, host, edge=show_edges, rnd=show_random),
    FnTest("UIMenu::Hide", ppc, host, edge=hide_edges, rnd=hide_random),
    FnTest("UIMenu::~UIMenu", ppc, host, edge=dtor_edges, rnd=dtor_random),
]
