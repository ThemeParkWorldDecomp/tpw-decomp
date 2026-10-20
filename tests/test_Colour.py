"""Unit v5L21, __sinit_Colour_cpp 0x1001a104 (bullfrog, PROGRAM "bullfrog symbols shared"), against libtpw_unit_Colour.dylib
(src/bfl_Colour.cpp, tests/unit_Colour.cpp).

The static initialiser runs on the PowerPC over random garbage in the 13 records (0x10073f38..0x1007406c, 0x18 bytes each); the
host's records hold what its constant initialisation produced. The 78 words are compared in address order.
No callees, no fused operations.
"""
import ctypes as C
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_Colour"
FN = 0x1001A104
FIRST = 0x10073F38
COUNT = 13


def setup(runner):
    pass


def ppc(e, case):
    e.write(FIRST, struct.pack(">%dI" % (6 * COUNT), *case["garbage"]))
    T.call_ppc(e, FN, [])
    return {"formats": list(struct.unpack(">%dI" % (6 * COUNT), e.read(FIRST, 0x18 * COUNT)))}


def host(lib, case):
    f = lib.tpw_colour_formats
    f.restype, f.argtypes = None, [C.POINTER(C.c_uint32)]
    out = (C.c_uint32 * (6 * COUNT))()
    f(out)
    return {"formats": list(out)}


def edges():
    yield {"garbage": [0] * (6 * COUNT)}
    yield {"garbage": [0xFFFFFFFF] * (6 * COUNT)}
    yield {"garbage": [0xDEADBEEF] * (6 * COUNT)}


def rnd(r):
    return {"garbage": [r.getrandbits(32) for _ in range(6 * COUNT)]}


TESTS = [FnTest("__sinit_Colour_cpp", ppc, host, edge=edges, rnd=rnd)]
