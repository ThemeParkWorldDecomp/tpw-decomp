"""Unit v5715, the anaglyph key action, against libtpw_unit_AnaglyphToggle.dylib (tests/unit_AnaglyphToggle.cpp), main app
("SimTheme Park"), src/keytables_enable_AnaglyphToggle.cpp:
  AnaglyphToggle 0x1014fce4

A case is the display flags word (gei_Display +0, 0x103bc8d0) written on both sides; the function is called and the answer and the flags are
compared. No callees, no fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_AnaglyphToggle"
MASK = 0xFFFFFFFF
FN = 0x1014fce4
FLAGS = 0x103bc8d0


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ana_addr.restype, lib.tpw_ana_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ana_toggle.restype, lib.tpw_ana_toggle.argtypes = C.c_uint32, []
    lib.bound = True


import calllog as CL

STUBS = CL.Stubs()
STUBS.real.add(FN)


def ppc(e, case):
    FLAGS = e.u32(0x1026067c)      # the import slot holds the resolved address of gei_Display
    e.write(FLAGS, struct.pack(">I", case["flags"] & MASK))
    saved = STUBS.activate(e)
    try:
        r3, _, _ = T.call_ppc(e, FN, [])
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"answer": r3 & MASK, "flags": struct.unpack(">I", e.read(FLAGS, 4))[0]}


def host(lib, case):
    bind(lib)
    a = lib.tpw_ana_addr(b"flags")
    C.memmove(a, struct.pack("<I", case["flags"] & MASK), 4)
    r = lib.tpw_ana_toggle() & MASK
    return {"answer": r, "flags": struct.unpack("<I", C.string_at(a, 4))[0]}


def edges():
    for f in (0, 4, 8, 0xc, 0xFFFFFFFF, 0xFFFFFFF3, 0xFFFFFFF7, 0xFFFFFFFB, 0x10, 0x14, 0x18, 0x1c, 0x410100):
        yield {"flags": f}


def rnd(r):
    return {"flags": r.getrandbits(32)}


TESTS = [
    FnTest("AnaglyphToggle", ppc, host, edge=edges, rnd=rnd),
]
