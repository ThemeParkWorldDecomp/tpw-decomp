"""Unit v5303, the main menu's creation and removal, against libtpw_unit_HideMainMenu.dylib (tests/unit_HideMainMenu.cpp), main app
("SimTheme Park"), src/game/ui/ui_mainmenu.cpp:
  CreateMainMenu 0x101fc05c, HideMainMenu 0x101fc140

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees (the two menu makers, RemoveSlideoutPanel,
CGame::Pause and Unpause, the timer system's pause and unpause, UIMenu::Show, Hide and the destructor with its delete flag) are fakes on both
sides that record their arguments; the makers set gMainMenu to the case's menu.

The world of a case: gMainMenu (NULL or the menu), the pause byte 0x10396a54, gGame's word +0x3c, the argument. What is compared: the
globals, gGame's word and the event log. No fused operations. Not tested: HideMainMenu with gMainMenu NULL (the host's delete of NULL
does not call the destructor the original calls, a Mac crash anyway).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_HideMainMenu"
MASK = 0xFFFFFFFF

FN_CREATE, FN_HIDE = 0x101FC05C, 0x101FC140
SIZES = {FN_CREATE: 188, FN_HIDE: 136}
OWN = set(SIZES)

ADDR = dict(gGame=0x1037F134, flag=0x10396A54, menu=0x10396A58)
HOST_F3C = [0]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def make_stub(name, spec):
    def stub(e):
        L = e.cl
        vals = []
        for kind, reg in spec:
            if kind == "L":
                vals.append(L.lab(e.r(reg)))
            elif kind == "h":
                vals.append(T.s16(e.r(reg)))
            elif kind == "b":
                vals.append(e.r(reg) & 0xFF)
            else:
                vals.append(T.s32(e.r(reg)))
        L.rec(name, tuple(vals))
    return stub


def maker(name):
    def stub(e):
        e.cl.rec(name, ())
        e.write(ADDR["menu"], (e.menu_addr & MASK).to_bytes(4, "big"))
    return stub


STUBS.internal(0x101FBCD0, maker("InitialiseLobbyMenu"))
STUBS.internal(0x101FB760, maker("InitialiseMainMenu"))
STUBS.internal(0x101A83D8, make_stub("RemoveSlideoutPanel", [("h", 3)]))
STUBS.internal(0x10149904, make_stub("Pause", [("L", 3), ("i", 4), ("b", 5)]))
STUBS.internal(0x101499D0, make_stub("Unpause", [("L", 3)]))
STUBS.internal(0x101D8B64, make_stub("UI_PauseTimerSystem", []))
STUBS.internal(0x101D8BA0, make_stub("UI_UnPauseTimerSystem", []))
STUBS.internal(0x1018B0E0, make_stub("Show", [("L", 3), ("i", 4)]))
STUBS.internal(0x1018B1FC, make_stub("Hide", [("L", 3)]))
def stub_dtor(e):
    """~UIMenu with the delete flag: the destructor, then (flag > 0) the operator delete the host's real delete logs."""
    L = e.cl
    obj, flag = L.lab(e.r(3)), T.s32(e.r(4))
    L.rec("dtor", (obj, flag))
    if flag > 0:
        L.rec("delete", (obj,))


STUBS.internal(0x1018AC8C, stub_dtor)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(int(w & MASK).to_bytes(4, self.order) for w in ws))

    def rd_word(self, a):
        return int.from_bytes(self.read(a, 4), self.order)


class Ppc(Side):
    order = "big"
    f3c = 0x3C

    def __init__(self, e, case):
        self.e = e
        self.menu = e.alloc(0x14)
        e.menu_addr = self.menu
        self.L = e.cl = CL.PpcLog({ADDR["gGame"]: 1, self.menu: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def create(self, a):
        T.call_ppc(self.e, FN_CREATE, [a & MASK])

    def hide(self):
        T.call_ppc(self.e, FN_HIDE, [])


class Host(Side):
    order = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.f3c = HOST_F3C[0]
        self.menu = lib.tpw_hm_newmenu()
        CL.host_begin(lib, {1: self.g("gGame"), 2: self.menu}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_hm_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def create(self, a):
        self.lib.tpw_hm_create(T.s32(a))

    def hide(self):
        self.lib.tpw_hm_hide()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_hm_addr.restype, lib.tpw_hm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_hm_newmenu.restype, lib.tpw_hm_newmenu.argtypes = C.c_uint64, []
    lib.tpw_hm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_hm_create.restype, lib.tpw_hm_create.argtypes = None, [C.c_int32]
    lib.tpw_hm_hide.restype, lib.tpw_hm_hide.argtypes = None, []
    out = (C.c_int64 * 8)()
    n = lib.tpw_hm_layout(out)
    HOST_F3C[0] = list(out)[:n][0]
    lib.bound = True


def build(s, case):
    mv = s.menu if case["has_menu"] else 0
    s.write(s.g("menu"), mv.to_bytes(4 if s.order == "big" else 8, s.order))
    s.write(s.g("flag"), bytes([case["flag"]]))
    s.words(s.g("gGame") + s.f3c, [case["f3c"]])


def execute(s, case):
    if case["op"] == "create":
        s.create(case["a"])
    else:
        s.hide()


def observe(s, case):
    m = s.rd_word(s.g("menu")) if s.order == "big" else int.from_bytes(s.read(s.g("menu"), 8), "little")
    return {"menu": 0 if m == 0 else (2 if m == s.menu else -1),
            "flag": s.read(s.g("flag"), 1),
            "f3c": s.rd_word(s.g("gGame") + s.f3c),
            "log": s.log()}


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op, has_menu):
    return {"op": op, "queues": {}, "has_menu": has_menu, "flag": r.choice([0, 1, 2, 255]),
            "f3c": r.choice([0, 1, 2, MASK, 0x80000000]), "a": r.choice([0, 1, -1, 5, 0x80000000])}


def create_edges():
    r = random.Random(5303)
    for has_menu in (False, True):
        for f3c in (0, 1, 2, MASK):
            for a in (0, 1, -1, 5):
                for flag in (0, 1):
                    c = base(r, "create", has_menu)
                    c.update(f3c=f3c, a=a, flag=flag)
                    yield c


def create_random(r):
    return base(r, "create", r.random() < 0.7)


def hide_edges():
    r = random.Random(5304)
    for f3c in (0, 1, 2, MASK):
        for flag in (0, 1, 2, 255):
            c = base(r, "hide", True)
            c.update(f3c=f3c, flag=flag)
            yield c


def hide_random(r):
    return base(r, "hide", True)


TESTS = [
    FnTest("CreateMainMenu", ppc, host, edge=create_edges, rnd=create_random),
    FnTest("HideMainMenu", ppc, host, edge=hide_edges, rnd=hide_random),
]
