"""__sinit_tb_printf_opengl_cpp 0x10046d30 against libtpw_unit_sinit_tb_printf_opengl_cpp.dylib.

The global COpenGLPrintf at 0x102b364c is pre-filled with random words, the initialiser runs, and the words 1..3 (opaque), the
text/x/y fields and the __register_global_object event (object, destructor, node as labels) are compared. Host layout of the
last three fields differs from the Mac's (8-byte pointer), so they are read by a host wrapper.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_sinit_tb_printf_opengl_cpp"
MASK = 0xFFFFFFFF
FN = 0x10046D30
OBJ, NODE, DTOR = 0x102B364C, 0x102B3668, 0x10050B68

STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_register(e):
    L = e.cl
    L.rec("register", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))


STUBS.imp("__register_global_object", stub_register)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sinit_addr.restype, lib.tpw_sinit_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sinit_fields.argtypes = [C.POINTER(C.c_uint32)]
    out = (C.c_int64 * 8)()
    n = lib.tpw_sinit_layout(out)
    assert list(out)[:n] == [0x10], list(out)[:n]
    lib.bound = True


def ppc(e, case):
    L = e.cl = CL.PpcLog({OBJ: 1, DTOR: 2, NODE: 3}, {})
    saved = STUBS.activate(e)
    try:
        e.write(OBJ, b"".join(struct.pack(">I", w) for w in case["pre"]))
        T.call_ppc(e, FN, [])
        w = [e.u32(OBJ + 4 * i) for i in range(7)]
        return {"w": w[1:4], "f": w[4:7], "w0": w[0] == case["pre"][0], "log": L.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    bind(lib)
    obj = lib.tpw_sinit_addr(b"obj")
    CL.host_begin(lib, {1: obj, 2: lib.tpw_sinit_addr(b"dtor"), 3: lib.tpw_sinit_addr(b"node")}, {})
    pre = case["pre"]
    C.memmove(obj, struct.pack("<4I", *pre[:4]), 16)
    lib.tpw_sinit_call_pre = None
    lib.tpw_sinit_call()
    w = list(struct.unpack("<4I", C.string_at(obj, 16)))
    f = (C.c_uint32 * 3)()
    lib.tpw_sinit_fields(f)
    # the Mac's fields: text 0x10, x 0x14, y 0x18 -> nonzero flags / values after init
    return {"w": w[1:4], "f": list(f), "w0": w[0] == pre[0], "log": CL.host_end(lib)}


def edges():
    r = random.Random(7)
    yield {"pre": [0] * 7}
    yield {"pre": [MASK] * 7}
    for _ in range(3):
        yield {"pre": [r.getrandbits(32) for _ in range(7)]}


def rnd(r):
    return {"pre": [r.getrandbits(32) for _ in range(7)]}


TESTS = [FnTest("__sinit_tb_printf_opengl_cpp", ppc, host, edge=edges, rnd=rnd)]
