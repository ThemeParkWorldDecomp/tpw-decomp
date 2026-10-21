"""Unit v5657, LbMaths_ArcTan 0x100295f0, against libtpw_unit_LbMaths_ArcTan.dylib (tests/unit_LbMaths_ArcTan.cpp), bullfrog
(src/bullfrog/LbZLib.cpp). Called with two ints; atan is the harness MATH1 import on the PowerPC side and
std::atan on the host. Compared: the answer. Fused operations: fmadd / fnmsub / fctiwz are covered by ROUNDING.
Not tested: y == 0 with x == 0 (NaN to fctiwz, unicorn and hardware differ)."""
import ctypes as C
import math
import random

import tpwtest as T
from tpwtest import FnTest
from unicorn import UC_HOOK_CODE
from test_DynamicMesh import HardwareRounding

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbMaths_ArcTan"
MASK = 0xFFFFFFFF
FN = 0x100295F0
SIZE = 260
KADDR = 0x10074810      # a double set at run time (zero in the dump): angle units per radian
K = 4096.0 / math.pi


class ROUNDING(HardwareRounding):
    def __enter__(self):
        self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=FN, end=FN + SIZE - 1))


import math
import struct

import calllog as CL

STUBS = CL.Stubs()


def atan_glue(e):
    """The import glue saves the TOC pointer at 0x14(r1) before the call; the function reloads r2 from there."""
    e.write(e.r(1) + 0x14, struct.pack(">I", (e.r(31) - 0x5A70) & MASK))
    e.set_f1(math.atan(e.f(1)))


STUBS.imp("atan", atan_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    f = lib.tpw_lbarctan_a
    f.restype, f.argtypes = C.c_uint32, [C.c_int32, C.c_int32]
    lib.bound = True


def ppc(e, case):
    x, y = case["args"]
    e.write(KADDR, struct.pack(">d", K))
    saved = STUBS.activate(e)
    try:
        with ROUNDING(e):
            r3, _, _ = T.call_ppc(e, FN, [x & MASK, y & MASK])
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"answer": r3 & MASK}


def host(lib, case):
    bind(lib)
    x, y = case["args"]
    C.c_double.in_dll(lib, "gLbMathsAtanScale").value = K
    return {"answer": lib.tpw_lbarctan_a(T.s32(x), T.s32(y)) & MASK}


def edges():
    vals = [1, -1, 2, -2, 100, -100, 12345, -12345, 0x7FFFFFFF, -0x80000000, 0]
    for x in vals:
        for y in vals:
            if y == 0 and x == 0:
                continue
            yield {"args": [x, y]}


def rnd(r):
    def v():
        k = r.random()
        if k < 0.5:
            return r.randint(-1000, 1000) or 1
        if k < 0.9:
            return r.randint(-100000, 100000) or 1
        return r.randint(-0x80000000, 0x7FFFFFFF) or 1
    return {"args": [v(), v()]}


TESTS = [FnTest("LbMaths_ArcTan", ppc, host, edge=edges, rnd=rnd)]
