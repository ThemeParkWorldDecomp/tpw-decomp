"""Unit v5313, UI_SwitchGameWindow and UI_SetGameWindowCursor, against libtpw_unit_UI_SwitchGameWindow.dylib
(tests/unit_UI_SwitchGameWindow.cpp), main app, src/ui_mainpanel2_window_bumhole_UI_SwitchGameWindow.cpp:
  UI_SwitchGameWindow 0x101a8cd8, UI_SetGameWindowCursor 0x101a8c58

A case is a world and a call. The world is built twice by one piece of Python code (emulator memory for the original, host memory for the
translation). The callees outside the unit (CWorld::IsPlayingWinningAnim, UI_SendMessage, CGameType::CGameType, ChatGoFirstPerson,
ChatBackToThirdPerson, UIMain_DecideKbCapture, InterfaceWindow::setCursor) are fakes on both sides that record their arguments.

The world of a case: the world's state word (+0x1da738), the winning-animation answer, the game and first-person windows (or NULL),
the game type object and its guard byte, the cursor global. What is compared: the event log, the game type, its guard and the cursor
global. No fused operations. Not tested: a NULL gGameWorld (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UI_SwitchGameWindow"
MASK = 0xFFFFFFFF

FN_SWITCH, FN_CURSOR = 0x101A8CD8, 0x101A8C58
SIZES = {FN_SWITCH: 240, FN_CURSOR: 80}
OWN = set(SIZES)

CALLEES = {
    "IsPlayingWinningAnim": 0x1013BDD0,
    "UI_SendMessage": 0x101C9518,
    "CGameType": 0x1016F5D8,
    "ChatGoFirstPerson": 0x101E5968,
    "ChatBackToThirdPerson": 0x101E5BA0,
    "UIMain_DecideKbCapture": 0x1018116C,
    "setCursor": 0x101DD7F0,
}

ADDR = dict(gGameWindow=0x103949E4, gFirstPersonWindow=0x103949E0, gGameWorld=0x1037EBFC, gGameWindowCursor=0x103949B0,
            gameType=0x102B3AA0, gameTypeGuard=0x102B3AA4)
STATE_OFF = 0x1DA738
MAC_WORLD_SIZE = 0x1DA740
HOST_LAYOUT = [STATE_OFF + 0, None]       # the second entry (sizeof CWorld) is not fixed; checked loosely below

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(CALLEES["IsPlayingWinningAnim"], CL.recorder("IsPlayingWinningAnim", "", "b"))
STUBS.internal(CALLEES["UI_SendMessage"], CL.recorder("UI_SendMessage", "Liii"))
STUBS.internal(CALLEES["CGameType"], CL.recorder("CGameType", "L"))
STUBS.internal(CALLEES["ChatGoFirstPerson"], CL.recorder("ChatGoFirstPerson", ""))
STUBS.internal(CALLEES["ChatBackToThirdPerson"], CL.recorder("ChatBackToThirdPerson", ""))
STUBS.internal(CALLEES["UIMain_DecideKbCapture"], CL.recorder("UIMain_DecideKbCapture", ""))
STUBS.internal(CALLEES["setCursor"], CL.recorder("setCursor", "Lh"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    guard_pad = 0

    def __init__(self, e, case):
        self.e = e
        self.w1 = e.alloc(16)
        self.w2 = e.alloc(16)
        self.L = e.cl = CL.PpcLog({self.w1: 1, self.w2: 2, ADDR["gameType"]: 3}, case["queues"])
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

    def world(self):
        return self.alloc(MAC_WORLD_SIZE), STATE_OFF

    def call_switch(self, first):
        T.call_ppc(self.e, FN_SWITCH, [first & MASK])

    def call_cursor(self, cursor):
        T.call_ppc(self.e, FN_CURSOR, [cursor & MASK])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    guard_pad = 7       # the host's guard variable is 8 bytes (Itanium ABI: byte 0 is the flag)

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.w1 = self.alloc(16)
        self.w2 = self.alloc(16)
        CL.host_begin(lib, {1: self.w1, 2: self.w2, 3: self.g("gameType")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_usw_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_usw_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def world(self):
        return self.alloc(self.size), self.off

    def call_switch(self, first):
        self.lib.tpw_usw_switch(T.s32(first))

    def call_cursor(self, cursor):
        self.lib.tpw_usw_cursor(T.s32(cursor))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_usw_"
    lib.tpw_usw_addr.restype, lib.tpw_usw_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_usw_alloc.restype, lib.tpw_usw_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_usw_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_usw_switch.argtypes = [C.c_int32]
    lib.tpw_usw_cursor.argtypes = [C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_usw_layout(out)
    Host.off, Host.size = out[0], out[1]
    lib.bound = True


def build(s, case):
    w, off = s.world()
    s.words(w + off, [case["state"]])
    s.ptr(s.g("gGameWorld"), w)
    s.ptr(s.g("gGameWindow"), s.w1 if case["gw"] else 0)
    s.ptr(s.g("gFirstPersonWindow"), s.w2 if case["fp"] else 0)
    s.words(s.g("gameType"), [case["gtype"]])
    s.write(s.g("gameTypeGuard"), bytes([case["guard"]]) + b"\0" * s.guard_pad)
    s.words(s.g("gGameWindowCursor"), [case["oldcur"]])


def execute(s, case):
    if case["op"] == "sw":
        s.call_switch(case["arg"])
    else:
        s.call_cursor(case["arg"])


def observe(s, case):
    return {"log": s.log(), "gtype": s.rd_word(s.g("gameType")), "guard": s.read(s.g("gameTypeGuard"), 1)[0],
            "cursor": s.rd_word(s.g("gGameWindowCursor"))}


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


def base(r, op):
    return {"op": op, "queues": {"IsPlayingWinningAnim": [r.choice([0, 1])]}, "state": r.choice([0, 1, 2, 3, 4, 5, 0x104]),
            "gw": r.choice([0, 1, 1]), "fp": r.choice([0, 1, 1]), "gtype": r.choice([0, 1, 1, 2, 0xFFFFFFFF]),
            "guard": r.choice([0, 1]), "oldcur": r.randrange(0, 1 << 32), "arg": 0}


def sw_case(r, **kw):
    c = base(r, "sw")
    c["arg"] = r.choice([0, 1, 2, -1, r.randrange(-100, 100)]) & MASK
    c.update(kw)
    if "win" in kw:
        c["queues"] = {"IsPlayingWinningAnim": [kw["win"]]}
    return c


def sw_edges():
    r = random.Random(5313)
    for state in (4, 1):
        for win in (0, 1):
            for gtype in (0, 1, 2):
                for guard in (0, 1):
                    for arg in (0, 1, 2, MASK):
                        yield sw_case(r, state=state, win=win, gtype=gtype, guard=guard, arg=arg, gw=1, fp=1)
    yield sw_case(r, state=0, win=0, gtype=1, guard=1, arg=0, gw=0, fp=0)
    yield sw_case(r, state=0, win=0, gtype=1, guard=1, arg=1, gw=0, fp=0)


def sw_random(r):
    return sw_case(r)


def cur_case(r, **kw):
    c = base(r, "cur")
    c["arg"] = r.choice([0, 1, -1, 0x7FFF, -0x8000, r.randrange(-0x8000, 0x8000)]) & MASK
    c["arg"] = T.s16(c["arg"]) & MASK
    c.update(kw)
    return c


def cur_edges():
    r = random.Random(8888)
    for gw in (0, 1):
        for arg in (0, 1, 0xFFFFFFFF, 0x7FFF, 0xFFFF8000):
            yield cur_case(r, gw=gw, arg=arg)


def cur_random(r):
    return cur_case(r)


TESTS = [
    FnTest("UI_SwitchGameWindow", ppc, host, edge=sw_edges, rnd=sw_random),
    FnTest("UI_SetGameWindowCursor", ppc, host, edge=cur_edges, rnd=cur_random),
]
