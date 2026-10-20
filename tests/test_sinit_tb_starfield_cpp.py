"""Unit v5283, the static initialiser of tb_starfield.cpp, against libtpw_unit_sinit_tb_starfield_cpp.dylib
(tests/unit_sinit_tb_starfield_cpp.cpp), main app, src/tb_switch_sinit_tb_starfield_cpp.cpp:
  __sinit_tb_starfield_cpp 0x1003266c

A case fills the 0x1c bytes of gStarfield with a junk byte, calls the initialiser and compares the seven fields (pointers as low
32 bits) and the call log. __register_global_object is a fake on both sides recording (object, destructor, node) as labels.
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_sinit_tb_starfield_cpp"
MASK = 0xFFFFFFFF

FN = 0x1003266C
OBJ, DTOR, NODE = 0x10173158, 0x10050750, 0x10173174
JUNK = [0x00, 0xFF, 0xA5, 0x5A, 0x01]

STUBS = CL.Stubs()
STUBS.real.add(FN)
STUBS.imp("__register_global_object", CL.recorder("__register_global_object", "LLL"))


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc(e, case):
    e.cl = CL.PpcLog({OBJ: 1, DTOR: 2, NODE: 3}, {})
    saved = STUBS.activate(e)
    try:
        j = case["junk"] * 0x01010101
        e.write(OBJ, struct.pack(">7I", *([j] * 7)))
        T.call_ppc(e, FN, [])
        return {"fields": list(struct.unpack(">7I", e.read(OBJ, 28))), "log": e.cl.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    lib.tpw_ss_addr.restype, lib.tpw_ss_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ss_junk.argtypes = [C.c_uint32]
    lib.tpw_ss_read.argtypes = [C.POINTER(C.c_int64)]
    g = lambda n: lib.tpw_ss_addr(n.encode())
    CL.host_begin(lib, {1: g("obj"), 2: g("dtor"), 3: g("node")}, {})
    lib.tpw_ss_junk(case["junk"])
    lib.tpw_ss_run()
    out = (C.c_int64 * 7)()
    lib.tpw_ss_read(out)
    return {"fields": [v & MASK for v in out], "log": CL.host_end(lib)}


def edges():
    for j in JUNK:
        yield {"junk": j}


def rnd(r):
    return {"junk": r.choice(JUNK)}


TESTS = [FnTest("__sinit_tb_starfield_cpp", ppc, host, edge=edges, rnd=rnd)]
