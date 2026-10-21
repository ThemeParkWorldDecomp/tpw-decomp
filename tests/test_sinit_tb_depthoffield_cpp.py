"""Unit v5705, the tb_depthoffield.cpp static initialiser, against libtpw_unit_sinit_tb_depthoffield_cpp.dylib, engine program,
src/engine/tb_depthoffield.cpp:
  __sinit_tb_depthoffield_cpp 0x10006864

A case pre-fills the ten-float table at 0x10052b08 with random words, calls the initialiser and compares all ten words. sin and cos
are the C library on both sides. No fused operations. Not tested: nothing.
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_sinit_tb_depthoffield_cpp"
MASK = 0xFFFFFFFF

FN = 0x10006864
TABLE = 0x10052B08
HOST_LAYOUT = [40]


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_dof_addr.restype, lib.tpw_dof_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_dof_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_dof_run.restype, lib.tpw_dof_run.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_dof_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def ppc(e, case):
    e.write(TABLE, b"".join(struct.pack(">I", w) for w in case["fill"]))
    T.call_ppc(e, FN, [])
    return {"table": list(struct.unpack(">10I", e.read(TABLE, 40)))}


def host(lib, case):
    bind(lib)
    a = lib.tpw_dof_addr(b"table")
    C.memmove(a, b"".join(struct.pack("<I", w) for w in case["fill"]), 40)
    lib.tpw_dof_run()
    return {"table": list(struct.unpack("<10I", C.string_at(a, 40)))}


def edges():
    yield {"fill": [0] * 10}
    yield {"fill": [MASK] * 10}


def rnd(r):
    return {"fill": [r.getrandbits(32) for _ in range(10)]}


TESTS = [
    FnTest("__sinit_tb_depthoffield_cpp", ppc, host, edge=edges, rnd=rnd),
]
