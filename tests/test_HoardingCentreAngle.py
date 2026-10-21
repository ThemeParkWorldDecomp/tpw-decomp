"""Unit v5646, HoardingCentreAngle 0x1005f8c4, against libtpw_unit_HoardingCentreAngle.dylib, src/game/park/TPHoardings.cpp.

A case is (x, y, side, halfWidth, halfHeight). The callees atan2 and fmod (glue imports) are the C library's on both sides (math.atan2,
math.fmod on the PowerPC fake; std:: on the host). Compared: the float answer's bits. No fused operations. Not tested: a side outside
0..3 (the original then uses unset registers).
"""
import ctypes as C
import random
import struct
import math

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_HoardingCentreAngle"
MASK = 0xFFFFFFFF
FN = 0x1005F8C4

STUBS = CL.Stubs()
STUBS.real.add(FN)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon(b):
    return 0x7FC00000 if (b & 0x7F800000) == 0x7F800000 and (b & 0x7FFFFF) else b


def atan2_stub(e):
    e.set_f1(math.atan2(e.f(1), e.f(2)))


def fmod_stub(e):
    x, y = e.f(1), e.f(2)
    try:
        r = math.fmod(x, y)
    except ValueError:
        r = float("nan")
    e.set_f1(r)


STUBS.imp("atan2", atan2_stub)
STUBS.imp("fmod", fmod_stub)


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc(e, case):
    e.cl = CL.PpcLog({}, {})
    saved = STUBS.activate(e)
    try:
        x, y, side, w, h = case
        _, _, f1 = T.call_ppc(e, FN, [x & MASK, y & MASK, side & MASK, T.F(bitsf(w)), T.F(bitsf(h))])
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"answer": canon(fb(f1))}


def host(lib, case):
    f = lib.tpw_hca_call
    f.restype = C.c_float
    f.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_float, C.c_float]
    x, y, side, w, h = case
    r = f(T.s32(x), T.s32(y), side, bitsf(w), bitsf(h))
    return {"answer": canon(fb(r))}


def rf(r):
    k = r.random()
    if k < 0.05:
        return r.choice([0, 0x80000000, 0x3F800000, 0xBF800000, 0x3F000000])
    if k < 0.7:
        return fb(r.uniform(-20, 20))
    return fb(r.uniform(-1000, 1000))


def edges():
    r = random.Random(77)
    for side in range(4):
        for x, y in ((0, 0), (1, 1), (-1, -1), (5, -7), (100000, -100000)):
            yield (x, y, side, fb(0.0), fb(0.0))
            yield (x, y, side, fb(x + 0.5), fb(y + 0.5))
            yield (x, y, side, rf(r), rf(r))
        yield (3, 4, side, fb(3.5), fb(4.0))
        yield (3, 4, side, fb(3.0), fb(4.5))


def rnd(r):
    return (r.randrange(-50, 50), r.randrange(-50, 50), r.randrange(4), rf(r), rf(r))


TESTS = [FnTest("HoardingCentreAngle", ppc, host, edge=edges, rnd=rnd)]
