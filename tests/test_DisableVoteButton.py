"""Unit v5766, two helpers of the online panel, against libtpw_unit_DisableVoteButton.dylib (tests/unit_DisableVoteButton.cpp), main app
("SimTheme Park"), src/game/ui/ui_onlinePanel.cpp:
  DisableVoteButton 0x101e6224, EarmuffsButtonDown 0x101e6168

A case is a world and a call. The world is built twice by ONE piece of Python code: four fake windows (tags 1..4) chained by a word at +0x200
(GetWindow answers it), tag at +0x204; gOnlinePanel is window 1 or NULL. Callees are fakes on both sides: InterfaceWindow::GetWindow
(logs tag, id), InterfaceButton::SetState (tag, state, redraw) and the virtual Enable reached through __ptr_glue (tag, argument).

What is compared: the event log and gOnlinePanel. No fused operations. Not tested: a NULL child (the original dereferences it), flags other
than 0 and 1 (a bool).
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DisableVoteButton"
MASK = 0xFFFFFFFF

FN_DIS, FN_EAR = 0x101E6224, 0x101E6168
SIZES = {FN_DIS: 132, FN_EAR: 144}
OWN = set(SIZES)
GET_WINDOW, SET_STATE, PTR_GLUE = 0x101DBE40, 0x101CB1D0, 0x1023BAA8
ADDR = dict(gOnlinePanel=0x10395E98)
HOST_LAYOUT = []
CUR = {}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_get_window(e):
    this = e.r(3)
    e.cl.rec("GetWindow", (e.u32(this + 0x204), T.s32(e.r(4))))
    e.set_r(3, CUR["wins"].get(e.u32(this + 0x200), 0))


def stub_set_state(e):
    e.cl.rec("SetState", (e.u32(e.r(3) + 0x204), T.s8(e.r(4)), T.s8(e.r(5))))
    e.set_r(3, 0)


def stub_ptr_glue(e):
    assert e.r(12) == CUR["desc"], "glue descriptor"
    e.cl.rec("Enable", (e.u32(e.r(3) + 0x204), T.s16(e.r(4))))
    e.set_r(3, 0)


STUBS.internal(GET_WINDOW, stub_get_window)
STUBS.internal(SET_STATE, stub_set_state)
STUBS.internal(PTR_GLUE, stub_ptr_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, {})
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

    def init_windows(self, wins):
        tbl, desc = self.alloc(0x40), self.alloc(8)
        CUR["desc"] = desc
        self.words(tbl + 0x1C, [desc])
        CUR["wins"] = {i + 1: w for i, w in enumerate(wins)}
        for w in wins:
            self.words(w + 0x118, [tbl])

    def set_panel(self, w):
        self.words(self.g("gOnlinePanel"), [w])

    def get_panel(self):
        return self.rd_word(self.g("gOnlinePanel")) != 0

    def call(self, op, flag):
        fn = FN_DIS if op == "dis" else FN_EAR
        T.call_ppc(self.e, fn, [flag])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_dvb_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_dvb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def init_windows(self, wins):
        for w in wins:
            self.lib.tpw_dvb_vptr(w)
        for i, w in enumerate(wins):
            self.lib.tpw_dvb_register(i + 1, w)

    def set_panel(self, w):
        self.write(self.g("gOnlinePanel"), struct.pack("<Q", w))

    def get_panel(self):
        return struct.unpack("<Q", self.read(self.g("gOnlinePanel"), 8))[0] != 0

    def call(self, op, flag):
        (self.lib.tpw_dvb_disable if op == "dis" else self.lib.tpw_dvb_earmuffs)(flag)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_dvb_"
    lib.tpw_dvb_addr.restype, lib.tpw_dvb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_dvb_alloc.restype, lib.tpw_dvb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_dvb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_dvb_vptr.argtypes = [C.c_uint64]
    lib.tpw_dvb_register.argtypes = [C.c_uint32, C.c_uint64]
    lib.tpw_dvb_disable.argtypes = [C.c_uint32]
    lib.tpw_dvb_earmuffs.argtypes = [C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_dvb_layout(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.bound = True


def run(s, case):
    try:
        wins = [s.alloc(0x220) for _ in range(4)]
        for i, w in enumerate(wins):
            s.words(w + 0x200, [i + 2 if i < 3 else 0, i + 1])   # child's tag, own tag
        s.init_windows(wins)
        s.set_panel(wins[0] if case["panel"] else 0)
        s.call(case["op"], case["flag"])
        return {"log": s.log(), "panel": s.get_panel()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(op, flag, panel):
    return {"op": op, "flag": flag, "panel": panel, "queues": {}}


def edges(op):
    def gen():
        for panel in (True, False):
            for flag in (0, 1):
                yield mk(op, flag, panel)
    return gen


def rnd(op):
    return lambda r: mk(op, r.choice([0, 1]), r.choice([True, True, False]))


TESTS = [
    FnTest("DisableVoteButton", ppc, host, edge=edges("dis"), rnd=rnd("dis")),
    FnTest("EarmuffsButtonDown", ppc, host, edge=edges("ear"), rnd=rnd("ear")),
]
