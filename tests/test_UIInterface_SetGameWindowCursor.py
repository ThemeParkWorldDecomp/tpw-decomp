"""Unit v5574, UIInterface::SetGameWindowCursor and PostLevelLoad, against libtpw_unit_UIInterface_SetGameWindowCursor.dylib
(tests/unit_UIInterface_SetGameWindowCursor.cpp), main app, src/game/ui/UIInterface.cpp:
  UIInterface::SetGameWindowCursor 0x101efa54, UIInterface::PostLevelLoad 0x101f0124

A case is a world and a call. Callees (UI_SetGameWindowCursor, InterfaceWindow::setCursor, UI_ClearSelected, UI_BankruptAfterLoad,
UIMain_EnableAllKeyTables, CParkTime::GetFunnyTime, UI_MainPanelDateChange) are fakes on both sides that record their arguments;
GetFunnyTime also writes seven answers (from the case) through the pointers it gets, so the date arithmetic is checked.

The world of a case: the three window globals (fake non-null pointers, never dereferenced), gGameWorld (a block big enough for the
+0x1da738 mode word), the date block (nine words, random before the call). Compared: the event log, the date block.
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UIInterface_SetGameWindowCursor"
MASK = 0xFFFFFFFF

FN_CURSOR, FN_POST = 0x101EFA54, 0x101F0124
SIZES = {FN_CURSOR: 128, FN_POST: 300}
OWN = set(SIZES)

UI_SETCURSOR, WIN_SETCURSOR, CLEARSEL, BANKRUPT, ENABLEKEYS, FUNNY, DATECHANGE = (
    0x101A8C58, 0x101DD7F0, 0x10194838, 0x1018710C, 0x10182304, 0x101127D0, 0x101A6234)

ADDR = dict(date=0x1039641C, world=0x1037EBFC, gw=0x103949E4, ow=0x10395E94, op=0x10395E98)
HOST_LAYOUT = [0x14, 36]
WIN_OW, WIN_OP = 0x10001000, 0x10002000
MODE_OFF, TIME_OFF, WORLD_SIZE = 0x1DA738, 0x2A0, 0x1DA73C


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_ui_set(e):
    e.cl.rec("UI_SetGameWindowCursor", (T.s16(e.r(3)),))


def st_win_set(e):
    L = e.cl
    L.rec("setCursor", (L.lab(e.r(3)), T.s16(e.r(4))))


def st_plain(name):
    def f(e):
        e.cl.rec(name, ())
    return f


def st_funny(e):
    L = e.cl
    world = e.u32(ADDR["world"])
    ptrs = [e.r(i) for i in range(5, 11)] + [e.u32(e.r(1) + 0x38), e.u32(e.r(1) + 0x3C)]
    enc = [((p - ADDR["date"]) & MASK) if p else 0xFFFF for p in ptrs]
    L.rec("GetFunnyTime", tuple([(e.r(4) - world) & MASK] + enc))
    for i, p in enumerate(ptrs[:7]):
        e.write(p, struct.pack(">I", L.ret("ft%d" % i) & MASK))


def st_datechange(e):
    e.cl.rec("UI_MainPanelDateChange", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5))))


STUBS.internal(UI_SETCURSOR, st_ui_set)
STUBS.internal(WIN_SETCURSOR, st_win_set)
STUBS.internal(CLEARSEL, st_plain("UI_ClearSelected"))
STUBS.internal(BANKRUPT, st_plain("UI_BankruptAfterLoad"))
STUBS.internal(ENABLEKEYS, st_plain("UIMain_EnableAllKeyTables"))
STUBS.internal(FUNNY, st_funny)
STUBS.internal(DATECHANGE, st_datechange)


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
        self.L = e.cl = CL.PpcLog({WIN_OW: 1, WIN_OP: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc_world(self):
        return self.e.alloc(WORLD_SIZE)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_cursor(self, c):
        T.call_ppc(self.e, FN_CURSOR, [0, c & 0xFFFF])
        return 0

    def call_post(self):
        T.call_ppc(self.e, FN_POST, [0])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: WIN_OW, 2: WIN_OP}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        if name in ("gw", "ow", "op", "world", "date"):
            return self.lib.tpw_uisgwc_addr(name.encode())
        return 0

    def alloc(self, n):
        return self.lib.tpw_uisgwc_alloc(n)

    def alloc_world(self):
        if not getattr(self.lib, "world", 0):
            self.lib.world = self.lib.tpw_uisgwc_alloc(WORLD_SIZE)
        return self.lib.world

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_cursor(self, c):
        self.lib.tpw_uisgwc_cursor(T.s16(c))
        return 0

    def call_post(self):
        self.lib.tpw_uisgwc_post()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_uisgwc_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "cursor").argtypes = [C.c_int16]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("gw"), [case["gw"]])
    s.words(s.g("ow"), [case["ow"]])
    s.words(s.g("op"), [case["op"]])
    w = s.alloc_world()
    s.write(s.g("world"), struct.pack(">I" if s.fmt == ">I" else "<Q", w))   # the host pointer is 64 bits wide
    s.words(w + MODE_OFF, [case["mode"]])
    s.words(s.g("date"), case["date"])
    return w


def execute(s, case):
    if case["fn"] == "cursor":
        return s.call_cursor(case["cursor"])
    return s.call_post()


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "date": s.rd_words(s.g("date"), 9)}


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
    q = {"ft%d" % i: [r.choice([0, 1, 5, 1999, 2026, 0x76C, 0x76D, r.randrange(0, 3000), MASK])] for i in range(7)}
    return {"fn": fn, "queues": q, "gw": 0, "ow": 0, "op": 0, "mode": r.choice([4, 0, 3, 5, MASK, r.randrange(0, 10)]),
            "date": [r.randrange(0, 1 << 32) for _ in range(9)], "cursor": 0}


def cursor_case(r, gw, ow, op, cursor=None):
    c = base(r, "cursor")
    c["gw"], c["ow"], c["op"] = gw, ow, op
    c["cursor"] = r.choice([0, 1, 9, -1, 0x7FFF, -0x8000, r.randrange(-300, 300)]) & 0xFFFF if cursor is None else cursor & 0xFFFF
    return c


def cursor_edges():
    r = random.Random(11)
    for gw in (0, 0x10003000):
        for ow in (0, WIN_OW):
            for op in (0, WIN_OP):
                yield cursor_case(r, gw, ow, op)
    yield cursor_case(r, 0, WIN_OW, WIN_OP, -1)
    yield cursor_case(r, 0, WIN_OW, WIN_OP, 0x8000)


def cursor_random(r):
    return cursor_case(r, r.choice([0, 0x10003000]), r.choice([0, WIN_OW]), r.choice([0, WIN_OP]))


def post_case(r, mode=None):
    c = base(r, "post")
    if mode is not None:
        c["mode"] = mode
    return c


def post_edges():
    r = random.Random(12)
    for mode in (4, 3, 5, 0, MASK, 0x80000004):
        yield post_case(r, mode)


def post_random(r):
    return post_case(r)


TESTS = [
    FnTest("UIInterface::SetGameWindowCursor", ppc, host, edge=cursor_edges, rnd=cursor_random),
    FnTest("UIInterface::PostLevelLoad", ppc, host, edge=post_edges, rnd=post_random),
]
