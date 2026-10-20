"""Unit v5073, the finance graph panel message handler, against libtpw_unit_FinanceGraphPanelMsg.dylib (tests/unit_FinanceGraphPanelMsg.cpp), main app
("SimTheme Park"), src/FinanceGraphPanelMsg.cpp:
  FinanceGraphPanelMsg 0x1019f5e4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (CoolRect ctor and dtor,
GetClientRect, GetMaxYValue, GetMinYValue, SetupGraph, DrawMeGraph, RemoveFinancePanelPanel, GetWindow, SetState, SetXRange, SetYRange,
CallDefaultWindowHandler) are fakes on both sides that record their arguments; window pointers are opaque numbers and GetWindow answers a
number computed from the parent and the id, so the chain of calls is compared.

The world of a case: the flag word, the mode word, the panel pointer, the window and the three message numbers, the answer queues.
What is compared: the answer, the flag and mode words, the event log.

No fused operations. Not tested: nothing is skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FinanceGraphPanelMsg"
MASK = 0xFFFFFFFF

FN = 0x1019F5E4
SIZES = {FN: 2552}
OWN = set(SIZES)

ADDR = dict(flags=0x102A9EE4, mode=0x10394900, panel=0x10394904)
GRAPHER, KISS = 0x10394894, 0x10267BA0
HOST_LAYOUT = [4, 4]

CALLEES = dict(
    ctor=0x101C7448, dtor=0x101C74E0, client=0x101DBA4C, maxy=0x1019EF1C, miny=0x1019F0BC, setup=0x101F6480,
    draw=0x101F66C0, remove=0x101A0C8C, getwin=0x101DBE40, setstate=0x101CB1D0, setx=0x101FD798, sety=0x101FD7D8,
    default=0x101DE318,
)


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st(key, fn):
    STUBS.internal(CALLEES[key], fn)


def lab(e, r):
    return e.cl.lab(r)


st("ctor", lambda e: e.cl.rec("CoolRect", ()))
st("dtor", lambda e: e.cl.rec("~CoolRect", ()))
st("client", lambda e: (e.cl.rec("GetClientRect", (s32(e.r(3)), lab(e, e.r(4)))), e.set_r(3, 0)))
st("maxy", lambda e: e.set_r(3, e.cl.ret("GetMaxYValue") & MASK))
st("miny", lambda e: e.set_r(3, e.cl.ret("GetMinYValue") & MASK))
st("setup", lambda e: e.cl.rec("SetupGraph", (lab(e, e.r(3)), lab(e, e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7)),
                                              s32(e.r(8)), s32(e.r(9)), s32(e.r(10)))))
st("draw", lambda e: e.cl.rec("DrawMeGraph", (s32(e.r(3)), lab(e, e.r(4)), lab(e, e.r(5)))))
st("remove", lambda e: e.cl.rec("RemoveFinancePanelPanel", ()))
st("setx", lambda e: e.cl.rec("SetXRange", (lab(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)))))
st("sety", lambda e: e.cl.rec("SetYRange", (lab(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)))))
st("default", lambda e: (e.cl.rec("CallDefaultWindowHandler", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)))),
                         e.set_r(3, e.cl.ret("Default") & MASK)))


def stub_getwin(e):
    this, ident = e.r(3) & MASK, e.r(4) & MASK
    e.cl.rec("GetWindow", (s32(this), s32(ident)))
    e.set_r(3, (0x60000000 + ((this * 31 + ident) & 0x0FFFFFFF)) & MASK)


def stub_setstate(e):
    e.cl.rec("SetState", (s32(e.r(3)), T.s8(e.r(4)), T.s8(e.r(5))))
    e.set_r(3, 0)


st("getwin", stub_getwin)
st("setstate", stub_setstate)


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
        self.L = e.cl = CL.PpcLog({GRAPHER: 1, KISS: 2}, case["queues"])
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

    def call(self, w, m, a, b):
        r3, _, _ = T.call_ppc(self.e, FN, [w & MASK, m & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.addr("grapher"), 2: self.addr("kiss")}, case["queues"])

    def addr(self, name):
        return self.lib.tpw_fin_addr(name.encode())

    def done(self):
        pass

    def g(self, name):
        return self.addr(name)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, w, m, a, b):
        return self.lib.tpw_fin_msg(w & MASK, s32(m), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_fin_addr.restype, lib.tpw_fin_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fin_alloc.restype, lib.tpw_fin_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fin_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fin_msg.restype, lib.tpw_fin_msg.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_fin_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("flags"), [case["flags"]])
    s.words(s.g("mode"), [case["mode"]])
    s.words(s.g("panel"), [case["panel"]])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "flags": s.rd_words(s.g("flags"), 1), "mode": s.rd_words(s.g("mode"), 1)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(*case["args"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


IDS = [-2, -1, 0, 0x12270, 0x12272, 0x12273, 0x12274, 0x12275, 0x12276, 0x12277, 0x12278, 0x12279, 0x1227A, 0x1227B, 0x1227C,
       0x1227D, 0x1227E, 0x1227F, 0x12280, 0x7FFFFFFF]
MSGS = [0, 0x14, 0x15, 0x16, 0xFF, 0x100, 0x101, 0x102, 0x103, -1]
FLAGS = [0, 1, 2, 3, 4, 0x10, 0x40, 0x80, 0xFC, 0xFF, 0x100, 0xFFFFFFFF, 0x80000001]


def case_of(r, m, a, b, flags=None):
    return {
        "args": [0x50000000 + r.randrange(0, 0x1000) * 16, m, a, b],
        "flags": r.choice(FLAGS) if flags is None else flags,
        "mode": r.choice([0, 1, 2, 3, -1, 5]),
        "panel": 0x40000000 + r.randrange(0, 0x1000) * 16,
        "queues": {"GetMaxYValue": [r.randrange(-100, 1000)], "GetMinYValue": [r.randrange(-1000, 100)],
                   "Default": [r.choice([0, 1, -1, r.randrange(0, 1 << 20)])]},
    }


def edges():
    r = random.Random(73)
    for a in IDS:
        for b in (0, 1, 2, -1):
            for f in (0, 3, 0xFC, 0x41, 0x80000000):
                yield case_of(r, 0x100, a, b, f)
    for a in IDS:
        for b in (0, 1, 2):
            for mode in (0, 1, 2, 3):
                c = case_of(r, 0x101, a, b)
                c["mode"] = mode
                yield c
    for m in MSGS:
        for b in (0, 1):
            yield case_of(r, m, 0x12277, b)


def rnd(r):
    m = r.choice([0x100, 0x100, 0x101, 0x15, r.choice(MSGS)])
    return case_of(r, m, r.choice(IDS), r.choice([0, 1, 1, 2, -1]))


TESTS = [
    FnTest("FinanceGraphPanelMsg", ppc, host, edge=edges, rnd=rnd),
]
