"""Unit v5524, CalcNumTrackSamples against libtpw_unit_CalcNumTrackSamples.dylib (tests/unit_CalcNumTrackSamples.cpp), main app,
src/DestroyCoasterPylons_CalcNumTrackSamples.cpp:
  CalcNumTrackSamples 0x1005162c

A case is a brace (number, count, optional previous brace) and a call. The world is built by one piece of Python code on both sides.
GetTrackPosition, DistanceBetweenPoints, FloatToCell and IncrementTrackPosnControl are fakes on both sides that record what they read
(braces by their number word) and answer from the case's queues; Increment adds 1 to the index and clears the control's second brace
when its queue answers 0.

What is compared: the answer, the total written through the out pointer, and the event log. Fused operation: the fmadds, under
HardwareRounding. Not tested: a NULL brace (read unconditionally).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CalcNumTrackSamples"
MASK = 0xFFFFFFFF

FN = 0x1005162C
SIZES = {FN: 260}
OWN = set(SIZES)

C_GTP, C_DIST, C_INC, C_CELL = 0x100514F0, 0x100C3198, 0x10051300, 0x10237CD8
HOST_LAYOUT = [104]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def num(e, p):
    return e.u32(p) if p else 0


def stub_gtp(e):
    L = e.cl
    c = e.r(3)
    L.rec("GetTrackPosition", (e.u32(c), num(e, e.u32(c + 4)), e.u32(c + 8), num(e, e.u32(c + 12)), fb(e.f(1))))
    p = e.r(5)
    e.write(p, struct.pack(">3I", L.ret("GTP") & MASK, L.ret("GTP") & MASK, L.ret("GTP") & MASK))


def stub_dist(e):
    L = e.cl
    a, b = e.r(3), e.r(4)
    L.rec("Distance", struct.unpack(">6I", e.read(a, 12) + e.read(b, 12)))
    e.set_f1(bitsf(L.ret("Dist")))


def stub_cell(e):
    L = e.cl
    q = struct.unpack(">Q", struct.pack(">d", e.f(1)))[0]
    L.rec("FloatToCell", (q >> 32, q & MASK))
    e.set_r(3, L.ret("Cell") & MASK)


def stub_inc(e):
    L = e.cl
    c = e.r(3)
    L.rec("Increment", (e.u32(c), num(e, e.u32(c + 4)), e.u32(c + 8), num(e, e.u32(c + 12))))
    e.write(c, struct.pack(">I", (e.u32(c) + 1) & MASK))
    if L.ret("Inc") == 0:
        e.write(c + 12, struct.pack(">I", 0))


STUBS.internal(C_GTP, stub_gtp)
STUBS.internal(C_DIST, stub_dist)
STUBS.internal(C_CELL, stub_cell)
STUBS.internal(C_INC, stub_inc)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = CL.Stubs.activate(STUBS, e) if False else STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def brace(self, number, prev, count):
        b = self.e.alloc(0xA0)
        s = self.e.alloc(16)
        self.e.write(s, struct.pack(">I", count))
        self.e.write(b, struct.pack(">I", number))
        self.e.write(b + 0x64, struct.pack(">I", prev))
        self.e.write(b + 0x90, struct.pack(">I", s))
        return b

    def buf(self, v):
        a = self.e.alloc(8)
        self.e.write(a, struct.pack(">I", v))
        return a

    def rd(self, a):
        return struct.unpack(">I", self.e.read(a, 4))[0]

    def log(self):
        return self.L.log

    def call(self, b, f, out):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN, [b, T.F(f), out])
        return r3 & MASK


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def brace(self, number, prev, count):
        return self.lib.tpw_ccn_brace(number, prev, count)

    def buf(self, v):
        a = self.lib.tpw_ccn_alloc(8)
        C.memmove(a, struct.pack("<I", v), 4)
        return a

    def rd(self, a):
        return struct.unpack("<I", C.string_at(a, 4))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, b, f, out):
        return self.lib.tpw_ccn_run(b, f, out) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ccn_alloc.restype, lib.tpw_ccn_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ccn_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ccn_brace.restype, lib.tpw_ccn_brace.argtypes = C.c_uint64, [C.c_uint32, C.c_uint64, C.c_uint32]
    lib.tpw_ccn_run.restype, lib.tpw_ccn_run.argtypes = C.c_uint32, [C.c_uint64, C.c_float, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ccn_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        prev = s.brace(2, 0, case["countB"]) if case["hasPrev"] else 0
        b = s.brace(1, prev, case["count"])
        out = s.buf(case["total"])
        ans = s.call(b, case["step"], out)
        return {"answer": ans, "total": s.rd(out), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


SPECIAL = [0, 0x80000000, 0x7F800000, 0x3F800000, 0xBF800000, 0x00000001]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    return fb(r.uniform(-300, 300))


def mk(r, count=None, has_prev=None, steps=None, step=None):
    n = r.randrange(0, 5) if steps is None else steps
    c = {
        "count": r.choice([0, 1, 2, 3]) if count is None else count,
        "countB": r.choice([0, 1, 2]),
        "hasPrev": r.choice([True, False]) if has_prev is None else has_prev,
        "total": rf(r),
        "step": bitsf(rf(r)) if step is None else step,
    }
    # the first fake answer of Inc decides whether the loop goes on: n more passes after the first
    c["queues"] = {
        "GTP": [rf(r) for _ in range(3 * (2 * n + 3))],
        "Dist": [rf(r) for _ in range(n + 2)],
        "Cell": [r.randrange(-5, 20) for _ in range(n + 2)],
        "Inc": [1] * n + [0],
    }
    c["step"] = c["step"] if isinstance(c["step"], float) else bitsf(c["step"])
    return c


def edges():
    r = random.Random(5524)
    for count in (0, 1, 2, 3):
        for hp in (False, True):
            for steps in (0, 1, 3):
                yield mk(r, count, hp, steps)
    for st in (0.0, -0.0, 1.0, 0.5, -2.0, 3.0):
        yield mk(r, 2, True, 2, st)


def rnd(r):
    return mk(r)


TESTS = [FnTest("CalcNumTrackSamples", ppc, host, edge=edges, rnd=rnd)]
