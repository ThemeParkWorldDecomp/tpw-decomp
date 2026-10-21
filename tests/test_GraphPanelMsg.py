"""Unit v5894, the park status graph panel's message handler, against libtpw_unit_GraphPanelMsg.dylib (tests/unit_GraphPanelMsg.cpp), main app
("SimTheme Park"), src/ParkInfoLines_GraphPanelMsg.cpp:
  GraphPanelMsg 0x101ac044

A case is a message and the globals (flags, range). Callees outside the unit are fakes on both sides that record their arguments:
GetParkAnalyser (answers a fixed address), the two MaxAnalyserValue (the history's offset from that address, the value in and the range;
the value out comes from the case's queue), SetupGraph (with the rectangle's four shorts), SetXRange, SetYRange, DrawMeGraph,
GetClientRect (writes the rectangle from the queue) and CallDefaultWindowHandler. CoolRect's constructor and destructor run for real on
the PowerPC side and from src/UI_Base.cpp on the host.

What is compared: the answer, the event log, the flags and the range. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_GraphPanelMsg"
MASK = 0xFFFFFFFF

FN = 0x101ac044
F_ANALYSER = 0x1013ebc8
F_MAXI = 0x101f5e3c
F_MAXF = 0x101f5f2c
F_SETUP = 0x101f6480
F_SETX = 0x101fd798
F_SETY = 0x101fd7d8
F_DRAW = 0x101f66c0
F_CLIENT = 0x101dba4c
F_DEFAULT = 0x101de318

ADDR = dict(grapher=0x10394a28, range=0x10394a88, flags=0x102abbac, lines=0x101abb90, world=0x1037ebfc)   # lines: ParkInfoLines, the code word of the TVector 0x10267ca8 the Mac passes to DrawMeGraph
HOST_LAYOUT = [4]
WINDOW = 0x30100000
ABASE = 0x30000000
WORLD_PTR = 0x30200000


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update({FN})


def stub_analyser(e):
    e.cl.rec("GetParkAnalyser", (1,))
    e.set_r(3, ABASE)


def mk_max(name):
    def f(e):
        L = e.cl
        L.rec(name, (s32(e.r(3) - ABASE), s32(e.u32(e.r(4))), s32(e.r(5))))
        e.write(e.r(4), struct.pack(">I", L.ret("Max") & MASK))
    return f


def stub_setup(e):
    L = e.cl
    rc = e.r(4)
    sh = struct.unpack(">hhhh", e.read(rc + 4, 8))
    L.rec("SetupGraph", (L.lab(e.r(3)),) + sh + tuple(s32(e.r(i)) for i in range(5, 11)))


def mk_range(name):
    def f(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    return f


def stub_draw(e):
    L = e.cl
    L.rec("DrawMeGraph", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.u32(e.r(5)))))     # the callback: the function its TVector holds


def stub_client(e):
    L = e.cl
    L.rec("GetClientRect", (L.lab(e.r(3)),))
    vals = [L.ret(k) & 0xFFFF for k in ("RL", "RT", "RR", "RB")]
    e.write(e.r(4) + 4, struct.pack(">HHHH", *vals))
    e.set_r(3, 0)


def stub_default(e):
    L = e.cl
    L.rec("Default", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("Default") & MASK)


STUBS.internal(F_ANALYSER, stub_analyser)
STUBS.internal(F_MAXI, mk_max("MaxI"))
STUBS.internal(F_MAXF, mk_max("MaxF"))
STUBS.internal(F_SETUP, stub_setup)
STUBS.internal(F_SETX, mk_range("SetXRange"))
STUBS.internal(F_SETY, mk_range("SetYRange"))
STUBS.internal(F_DRAW, stub_draw)
STUBS.internal(F_CLIENT, stub_client)
STUBS.internal(F_DEFAULT, stub_default)


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
        self.L = e.cl = CL.PpcLog({ADDR["grapher"]: 1, ADDR["lines"]: 2, WINDOW: 3}, case["queues"])
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

    def call(self, msg, a, b):
        r3, _, _ = T.call_ppc(self.e, FN, [WINDOW, msg & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("grapher"), 2: self.g("lines"), 3: WINDOW}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gpm_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, msg, a, b):
        return self.lib.tpw_gpm_call(WINDOW, s32(msg), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_gpm_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "call").restype, getattr(lib, p + "call").argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("flags"), [case["flags"]])
    s.words(s.g("range"), [case["range"]])
    s.words(s.g("world"), [WORLD_PTR])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "flags": s.rd_word(s.g("flags")), "range": s.rd_word(s.g("range"))}


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


B = 0x4a68bc


def mkcase(r, msg, a, b, flags=None, rng=None):
    return {
        "args": (msg, a, b),
        "flags": r.randrange(32) if flags is None else flags,
        "range": r.choice([0, 1, 2, 2, 3, -1, 0, 1]) if rng is None else rng,
        "queues": {
            "Max": [r.choice([0, 1, 7, 100, -5, r.randrange(-1000, 100000)]) for _ in range(r.randrange(1, 6))],
            "RL": [r.randrange(-40000, 40000)], "RT": [r.randrange(-40000, 40000)],
            "RR": [r.randrange(-40000, 40000)], "RB": [r.randrange(-40000, 40000)],
            "Default": [r.choice([0, 1, -1, r.randrange(0, 1 << 32)])],
        },
    }


def edges():
    r = random.Random(5894)
    yield mkcase(r, 0x15, 0, 0, 0, 0)
    for flags in (0, 1, 2, 4, 8, 0x10, 0x1f):
        for rng in (0, 1, 2, 3, -1):
            yield mkcase(r, 0x15, r.randrange(100), 0, flags, rng)
    for a in range(B - 2, B + 8):
        for b in (1, 0, 2, -1):
            for flags in (0, 0x1f, 0x15):
                yield mkcase(r, 0x100, a, b, flags)
    for a in range(0x4a68c0, 0x4a68c8):
        for b in (1, 0, 2):
            for rng in (0, 1, 2, 3, -1):
                yield mkcase(r, 0x101, a, b, None, rng)
    for m in (0, 0x14, 0x16, 0xff, 0x102, 0x103, 0x800, -1):
        yield mkcase(r, m, B, 1)
    for m in (0x100, 0x101):
        yield mkcase(r, m, 0x7fffffff, 1)
        yield mkcase(r, m, -0x7fffffff, 1)


def rnd(r):
    m = r.choice([0x15, 0x100, 0x100, 0x101, 0x101, r.randrange(0, 0x900)])
    a = r.choice([B + r.randrange(-1, 8), 0x4a68c0 + r.randrange(0, 7), r.randrange(0, 1 << 32)])
    return mkcase(r, m, a, r.choice([0, 1, 1, 2, -1]))


TESTS = [
    FnTest("GraphPanelMsg", ppc, host, edge=edges, rnd=rnd),
]
