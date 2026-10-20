"""Unit v5286, NewsPanelMsg and RemoveOnlineLoginPanel against libtpw_unit_NewsPanelMsg.dylib (tests/unit_NewsPanelMsg.cpp), main app,
src/UI_IslandLobby_TextPanelMsg_NewsPanelMsg.cpp:
  NewsPanelMsg 0x101e2f84, RemoveOnlineLoginPanel 0x101e2be4

A case is a world and a call. UI_SendMessage, UIMain_DecideKbCapture, RemoveOnlineNewsPanel, DefaultDialogCallback and the glue
__ptr_glue (the login hook) are fakes on both sides recording their arguments as raw words. The world: the lobby window (gLobby+4), the
news panel global, the login flag word and whether the hook is set. Compared: the answer, the news panel global and the event log.
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_NewsPanelMsg"
MASK = 0xFFFFFFFF

FN_NEWS, FN_LOGIN = 0x101E2F84, 0x101E2BE4
SIZES = {FN_NEWS: 248, FN_LOGIN: 100}
OWN = set(SIZES)

UI_SENDMESSAGE = 0x101C9518
DECIDE_KB = 0x1018116C
REMOVE_NEWS = 0x101E35C4
DEFAULT_CB = 0x101F1448

ADDR = dict(gLobby=0x102E4888, news=0x10395DD8, flag=0x10395D2C, hook=0x10395D28)
TVEC = 0x10395E00      # only compared against r12, never read

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def stub_send(e):
    L = e.cl
    L.rec("UI_SendMessage", (e.r(3), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("UI_SendMessage") & MASK)


def stub_kb(e):
    e.cl.rec("UIMain_DecideKbCapture", ())


def stub_remove(e):
    e.cl.rec("RemoveOnlineNewsPanel", ())


def stub_default(e):
    L = e.cl
    L.rec("DefaultDialogCallback", (e.r(3), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("DefaultDialogCallback") & MASK)


def stub_glue(e):
    assert e.r(12) == TVEC, hex(e.r(12))
    e.cl.rec("hook", ())


STUBS.internal(UI_SENDMESSAGE, stub_send)
STUBS.internal(DECIDE_KB, stub_kb)
STUBS.internal(REMOVE_NEWS, stub_remove)
STUBS.internal(DEFAULT_CB, stub_default)
STUBS.internal(0x1023BAA8, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def world(self, case):
        e = self.e
        blk = e.alloc(0x40)
        e.write(blk + 4, (case["window"] & MASK).to_bytes(4, "big"))
        e.write(ADDR["gLobby"], blk.to_bytes(4, "big"))
        e.write(ADDR["news"], (case["news"] & MASK).to_bytes(4, "big"))
        e.write(ADDR["flag"], (case["flag"] & MASK).to_bytes(4, "big"))
        e.write(ADDR["hook"], (TVEC if case["hook"] else 0).to_bytes(4, "big"))

    def news(self, w, m, a, b):
        r3, _, _ = T.call_ppc(self.e, FN_NEWS, [w & MASK, m & MASK, a & MASK, b & MASK])
        return r3 & MASK

    def login(self, arg):
        T.call_ppc(self.e, FN_LOGIN, [arg & MASK])
        return 0

    def getnews(self):
        return self.e.u32(ADDR["news"])

    def log(self):
        return self.L.log


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def world(self, case):
        l = self.lib
        l.tpw_nm_setptrs(case["window"] & MASK, case["news"] & MASK)
        C.c_int32.from_address(l.tpw_nm_addr(b"flag")).value = s32(case["flag"])
        l.tpw_nm_sethook(1 if case["hook"] else 0)

    def news(self, w, m, a, b):
        return self.lib.tpw_nm_news(w & MASK, s32(m), s32(a), s32(b)) & MASK

    def login(self, arg):
        self.lib.tpw_nm_login(s32(arg))
        return 0

    def getnews(self):
        return self.lib.tpw_nm_getnews()

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_nm_addr.restype, lib.tpw_nm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_nm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_nm_sethook.argtypes = [C.c_int32]
    lib.tpw_nm_setptrs.argtypes = [C.c_uint32, C.c_uint32]
    lib.tpw_nm_getnews.restype = C.c_uint32
    lib.tpw_nm_news.restype, lib.tpw_nm_news.argtypes = C.c_uint32, [C.c_uint32, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_nm_login.argtypes = [C.c_int32]
    out = (C.c_int64 * 8)()
    n = lib.tpw_nm_layout(out)
    assert n == 1   # mWindow's host offset differs from the Mac's 4 (vptr), the shim sets it itself
    lib.bound = True


def run(s, case):
    try:
        s.world(case)
        if case["op"] == "news":
            ans = s.news(*case["args"])
        else:
            ans = s.login(case["args"][0])
        return {"answer": ans, "log": s.log(), "news": s.getnews()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    return {"op": op, "window": r.choice([0, 0x1000, r.randrange(1 << 20)]), "news": r.choice([0, 0x2000, r.randrange(1 << 20)]),
            "flag": r.choice([0, 0x3000, -1, r.randrange(1 << 20)]), "hook": r.choice([0, 1]),
            "queues": {"UI_SendMessage": [r.randrange(100)], "DefaultDialogCallback": [r.choice([0, 1, -5, r.randrange(1000)]) & MASK]}}


MSGS = [0, 1, 0x13, 0x14, 0x15, 0x16, 0xFF, 0x100, 0x101, -1, 0x7FFFFFFF, -0x80000000]


def news_case(r, m=None, a=None):
    c = base(r, "news")
    m = r.choice(MSGS) if m is None else m
    a = r.choice([0, -1, 1, 5, r.randrange(-5, 5)]) if a is None else a
    c["args"] = [r.choice([0, 0x4000, r.randrange(1 << 20)]), m, a, r.choice([0, 1, -1, r.randrange(100)])]
    return c


def news_edges():
    r = random.Random(5286)
    for m in MSGS:
        for a in (0, -1, 1):
            yield news_case(r, m, a)


def news_random(r):
    return news_case(r)


def login_case(r, arg=None):
    c = base(r, "login")
    c["args"] = [r.choice([0, 1, -1, r.randrange(1, 100)]) if arg is None else arg]
    return c


def login_edges():
    r = random.Random(77)
    for arg in (0, 1, -1, 2):
        for _ in range(4):
            yield login_case(r, arg)


def login_random(r):
    return login_case(r)


TESTS = [
    FnTest("NewsPanelMsg", ppc, host, edge=news_edges, rnd=news_random),
    FnTest("RemoveOnlineLoginPanel", ppc, host, edge=login_edges, rnd=login_random),
]
