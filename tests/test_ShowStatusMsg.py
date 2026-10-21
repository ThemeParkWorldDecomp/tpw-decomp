"""Unit v5525, the show-status panel's message handler, against libtpw_unit_ShowStatusMsg.dylib (tests/unit_ShowStatusMsg.cpp), main app
("SimTheme Park"), src/game/ui/ui_showStatus.cpp:
  ShowStatusMsg 0x101c2708

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory for the original, host memory for the
translation). Every callee is a fake on both sides that records its arguments (labels for pointers): the panel methods, the status panel
base methods, CreateAllItemsPanel, UI_IterateThingList (mask and whether the callback is _applytoall), the animation timer, the advisor,
DefaultStatusPanelMsg (answer from the case) and the virtual call through the panel's table at +0x20 slot 0x1c.

The world: the panel (words 0x24, 0x28, 0x2c random, table at 0x20), the thing global. Compared: the answer, the panel's first 12 words
and the event log. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ShowStatusMsg"
MASK = 0xFFFFFFFF

FN = 0x101c2708
PANEL = 0x10395a58
THING_GLOBAL = 0x103947a4
APPLYTOALL_TV = 0x10267eb0

ADDR = dict(panel=PANEL, thing=THING_GLOBAL)
HOST_LAYOUT = [0x24]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.add(FN)


def rec(addr, name, spec="", ret=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret))


rec(0x10181570, "TickRidePreview")
rec(0x1018227c, "CancelAdvisorMessage", "i")
rec(0x101dd5e0, "setAnimTimer", "Lii", "h")
rec(0x101dd6b8, "startAnimTimer", "Li")
rec(0x101c2abc, "Remove", "L")
rec(0x101c1848, "UpdateStatusWindow", "L")
rec(0x10192c28, "GetNextObject", "L")
rec(0x10192de0, "GetPreviousObject", "L")
rec(0x10193138, "EraseObject", "L")
rec(0x101938d8, "Move", "L")
rec(0x10193010, "CloseObject", "Li")
rec(0x101983c4, "CreateAllItemsPanel", "iL")
rec(0x10193c78, "DefaultStatusPanelMsg", "Liii", "i")


def stub_iterate(e):
    e.cl.rec("IterateThingList", (s32(e.r(3)), 1 if (e.r(4) & MASK) == APPLYTOALL_TV else 0))


def stub_glue(e):
    e.cl.rec("Slot", (e.cl.lab(e.r(3)),))


STUBS.internal(0x101f7db0, stub_iterate)
STUBS.internal(0x1023baa8, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def prep(self):
        self.thing = self.alloc(16)
        self.win = self.alloc(16)
        self.table = self.alloc(0x40)
        self.block = self.alloc(16)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.prep()
        self.L = e.cl = CL.PpcLog({PANEL: 1, self.win: 2, self.thing: 3}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def off(self, a):
        return a

    def setthing(self):
        self.words(THING_GLOBAL, [self.thing])

    def fnwords(self):
        return [0x10000000, 0]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, m, a, b):
        r3, _, _ = T.call_ppc(self.e, FN, [self.win, m & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.prep()
        CL.host_begin(lib, {1: self.g("panel"), 2: self.win, 3: self.thing}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ss_addr(name.encode())

    def off(self, a):
        return self.lib.tpw_ss_off(a)

    def setthing(self):
        self.lib.tpw_ss_setthing(self.thing)

    def fnwords(self):
        f = self.g("slotfn")
        return [f & MASK, f >> 32]

    def alloc(self, n):
        return self.lib.tpw_ss_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, m, a, b):
        return self.lib.tpw_ss_msg(self.win, s32(m), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ss_addr.restype, lib.tpw_ss_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ss_alloc.restype, lib.tpw_ss_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ss_off.restype, lib.tpw_ss_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_ss_setthing.argtypes = [C.c_uint64]
    lib.tpw_ss_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ss_msg.restype, lib.tpw_ss_msg.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ss_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.setthing()
    s.words(s.block, s.fnwords())
    s.words(s.table + 0x1c, [s.off(s.block)])
    p = s.g("panel")
    s.words(p, [0] * 12)
    s.words(p + 0x20, [s.off(s.table)] + case["fields"])


def run(s, case):
    try:
        build(s, case)
        ans = s.call(*case["args"])
        return {"answer": ans, "log": s.log(), "panel": s.rd_words(s.g("panel") + 0x24, 3)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


MSGS = [0x15, 0x10, 5, 0x14, 0x1e, 0x100, 0x800]
OTHER = [0, 1, 4, 6, 0xf, 0x11, 0x13, 0x16, 0x1d, 0x1f, 0xff, 0x101, 0x7ff, 0x801, -1]
A100 = [-4, -3, -2, -1, 0, 1, 0xa088, 0xa089, 0xa08a, 0xa08b, 0xa08c, 0xa08d, 0xa08e, 0xa08f, 0xa090, 0xa0a7, 0xa0a8,
        0xa0a9, 0xa0aa, 0xa0ab, 0x7fffffff, -0x80000000]
A800 = [0xa08c, 0xa08d, 0xa08e, 0xa08f, 0xa090, 0xa091, 0xa092, 0, -1]
A10 = [0x80080, 0x8007f, 0x80081, 0, -1]


def mk(r, m, a, b):
    return {"args": [m, a, b],
            "queues": {"setAnimTimer": [r.choice([0, 1, -1, 0x10000])], "DefaultStatusPanelMsg": [r.choice([0, 1, -5, 77])]},
            "fields": [r.randrange(1 << 32) for _ in range(3)]}


def edges():
    r = random.Random(5525)
    for m in OTHER:
        yield mk(r, m, r.choice(A100), r.choice([0, 1]))
    for t in (0, 1):
        yield mk(r, 0x15, 0, t)
    for a in A10:
        yield mk(r, 0x10, a, 0)
    for m in (5, 0x14, 0x1e):
        yield mk(r, m, 0, 0)
    for a in A100:
        for b in (0, 1, 2, -1):
            yield mk(r, 0x100, a, b)
    for a in A800:
        yield mk(r, 0x800, a, r.randrange(1 << 32))


def rnd(r):
    m = r.choice(MSGS + OTHER)
    pool = A100 + A800 + A10
    a = r.choice(pool) if r.random() < 0.8 else r.randrange(-0x80000000, 0x80000000)
    b = r.choice([0, 1, 2, -1, r.randrange(1 << 32)])
    return mk(r, m, a, b)


TESTS = [
    FnTest("ShowStatusMsg", ppc, host, edge=edges, rnd=rnd),
]
