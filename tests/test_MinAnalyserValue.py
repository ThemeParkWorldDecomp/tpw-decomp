"""Unit v5135, against libtpw_unit_MinAnalyserValue.dylib (tests/unit_MinAnalyserValue.cpp), main app, src/game/ui/ui_graph.cpp:
  MinAnalyserValue 0x101f6030, MaxAnalyserValue (float) 0x101f5f2c, MaxAnalyserValue (int) 0x101f5e3c,
  std::__cdeque<errorthingy*>::iterator::operator+= 0x101f5adc

A case is a world and a call, built by one piece of Python code in both memories. The CHistory members and the cursor -= are fakes on both
sides recording their arguments; their answers come from the case's queues. Compared: the value word, the cursor words, the answer
(is it the cursor) and the call log. Float entries are kept inside the int range (fctiwz saturation differs in the emulator).
No fused operations. Not tested: float entries beyond int range and NaN (emulator fctiwz differs).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_MinAnalyserValue"
MASK = 0xFFFFFFFF

F_MIN, F_MAXF, F_MAXI, F_ADD = 0x101F6030, 0x101F5F2C, 0x101F5E3C, 0x101F5ADC
SIZES = {F_MIN: 184, F_MAXF: 200, F_MAXI: 180, F_ADD: 152}
OWN = set(SIZES)

LOWEST_I, HIGHEST_F, HIGHEST_I, VALUE_I, VALUE_F, SUB = 0x101F6B0C, 0x101F68F8, 0x101F6A2C, 0x1001C2A8, 0x101F6854, 0x101F5BDC
HOST_LAYOUT = [0x250, 12, 8]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk_hist(name, fbits_arg):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)),) + ((fb(e.f(1)),) if fbits_arg else (s32(e.r(4)),)) + (s32(e.r(5)),))
        e.set_r(3, L.ret(name) & MASK)
    return f


def mk_value(name):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), s32(e.r(5))))
        e.write(e.r(4), struct.pack(">I", L.ret(name) & MASK))
        e.set_r(3, 1)
    return f


def stub_sub(e):
    L = e.cl
    L.rec("CursorSub", (L.lab(e.r(3)), s32(e.r(4))))


STUBS.internal(LOWEST_I, mk_hist("GetLowestI", False))
STUBS.internal(HIGHEST_I, mk_hist("GetHighestI", False))
STUBS.internal(HIGHEST_F, mk_hist("GetHighestF", True))
STUBS.internal(VALUE_I, mk_value("GetValueI"))
STUBS.internal(VALUE_F, mk_value("GetValueF"))
STUBS.internal(SUB, stub_sub)


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
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, a, b, c=None):
        entry = {"min": F_MIN, "maxf": F_MAXF, "maxi": F_MAXI, "add": F_ADD}[op]
        args = [a, b & MASK] if c is None else [a, b, c & MASK]
        r3, _, _ = T.call_ppc(self.e, entry, args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_mav_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, a, b, c=None):
        if op == "add":
            return self.lib.tpw_mav_add(a, s32(b))
        getattr(self.lib, "tpw_mav_" + op)(a, b, s32(c))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_mav_addr.restype, lib.tpw_mav_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_mav_alloc.restype, lib.tpw_mav_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_mav_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("min", "maxf", "maxi"):
        f = getattr(lib, "tpw_mav_" + n)
        f.restype, f.argtypes = None, [C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_mav_add.restype, lib.tpw_mav_add.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_mav_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run_hist(s, case):
    try:
        h = s.alloc(0x250)
        v = s.alloc(4)
        s.words(v, [case["value"]])
        s.call(case["op"], h, v, case["range"])
        return {"log": s.log(), "value": s.rd_words(v, 1)}
    finally:
        s.done()


def run_add(s, case):
    try:
        d = s.alloc(16)
        s.words(d, case["cd"])
        c = s.alloc(12)
        s.words(c, [d, case["idx"], case["slot"]])
        ret = s.call("add", c, case["count"])
        return {"log": s.log(), "cursor": s.rd_words(c, 3)[1:], "cd": s.rd_words(d, 4), "self": ret == c}
    finally:
        s.done()


def mk(fn):
    return (lambda e, case: fn(Ppc(e, case), case)), (lambda lib, case: fn(Host(lib, case), case))


def hist_case(r, op, rng=None, idx=None):
    base = r.choice([0, 1, -1, 5, -5, 1000, -1000, 0x7FFFFFFF, -0x80000000])
    if rng is None:
        rng = r.choice([-1, 0, 1, 2, 3, 4, 100, -100, r.randrange(-20, 20)])
    if idx is None:
        idx = r.choice([-1, 0, 3, 143, r.randrange(-1, 144)])
    ent = base + r.choice([-1, 0, 1, 2, -2]) if abs(base) < 0x7FFFFF00 else r.choice([0, 7, -7])
    if op == "maxf":
        key = "GetValueF"
        ev = fb(float(ent) + r.choice([0.0, 0.0, 0.5, -0.5, 0.99, -0.99]))
        if abs(ent) > 1000000:
            ev = fb(float(r.choice([0, 3, -3])))
    else:
        key = "GetValueI"
        ev = ent & MASK
    nm = {"min": "GetLowestI", "maxi": "GetHighestI", "maxf": "GetHighestF"}[op]
    return {"op": op, "range": rng, "value": base & MASK, "queues": {nm: [idx], key: [ev]}}


def hist_edges(op):
    def g():
        r = random.Random(11)
        for rng in (-1, 0, 1, 2, 3, 4, 100, -100):
            for idx in (-1, 0, 7):
                for _ in range(4):
                    yield hist_case(r, op, rng, idx)
    return g


def hist_rand(op):
    return lambda r: hist_case(r, op)


def add_case(r, count=None):
    cap = r.randrange(1, 20)
    first = r.randrange(0, cap + 5)
    idx = r.randrange(0, 30)
    if count is None:
        k = r.random()
        if k < 0.4:
            count = cap - first - idx + r.choice([-1, 0, 1])
            if count < 0:
                count = r.randrange(0, 25)
        elif k < 0.5:
            count = -r.randrange(1, 10)
        else:
            count = r.randrange(0, 25)
    if r.random() < 0.05:
        first = 0xFFFFFFF0 + r.randrange(0, 16)
    return {"cd": [cap, r.randrange(0, 30), first, r.randrange(0, 1 << 32) & ~3], "idx": idx, "slot": r.randrange(0, 1 << 32) & ~3,
            "count": count, "queues": {}}


def add_edges():
    r = random.Random(5)
    for count in (-1, -7, 0, 1, 2, 5, 100):
        for _ in range(3):
            yield add_case(r, count)
    for _ in range(30):
        yield add_case(r)


def add_rand(r):
    return add_case(r)


TESTS = [
    FnTest("MinAnalyserValue", *mk(run_hist), edge=hist_edges("min"), rnd=hist_rand("min")),
    FnTest("MaxAnalyserValue", *mk(run_hist), edge=hist_edges("maxf"), rnd=hist_rand("maxf")),
    FnTest("MaxAnalyserValue", *mk(run_hist), edge=hist_edges("maxi"), rnd=hist_rand("maxi")),
    FnTest("std::__cdeque<errorthingy*,std::allocator<errorthingy*>>::iterator::operator+=", *mk(run_add), edge=add_edges, rnd=add_rand),
]
