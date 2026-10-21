"""Unit v5726, ui_playerLogin.cpp's static initialiser, against libtpw_unit_sinit_ui_playerLogin_cpp.dylib
(tests/unit_sinit_ui_playerLogin_cpp.cpp), main app, src/game/ui/ui_playerLogin.cpp:
  __sinit_ui_playerLogin_cpp 0x101afa90

The call has no input. The wide string constructor (0x10238800) and __register_global_object (0x10239940) are fakes on both sides
recording the object, the text (as a byte offset into the data at 0x1024e19e) and the destructor and node labels. What is compared: the
event log. No fused operations. Not tested: the text of the two strings (the fake constructor only records the offset).
"""
import ctypes as C

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_ui_playerLogin_cpp"
MASK = 0xFFFFFFFF

FN = 0x101AFA90
CTOR = 0x10238800
REG = 0x10239940

ADDR = dict(strings=0x1024E19E, a=0x10394AE4, b=0x10394AC4, nodea=0x10394AF8, nodeb=0x10394AD8, dtor=0x103BCBB8)
LABELS = dict(a=1, b=2, nodea=3, nodeb=4, dtor=5)

STUBS = CL.Stubs()
STUBS.real.add(FN)


def save_toc(e):
    """The glue the original calls stores the TOC pointer at 0x14(r1) before it jumps; the caller reloads r2 from there."""
    e.write(e.r(1) + 0x14, (e.r(2) & MASK).to_bytes(4, "big"))


def stub_ctor(e):
    save_toc(e)
    e.cl.rec("ctor", (e.cl.lab(e.r(3)), (e.r(4) - ADDR["strings"]) & MASK))


def stub_reg(e):
    save_toc(e)
    L = e.cl
    L.rec("register", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))


STUBS.internal(CTOR, stub_ctor)
STUBS.internal(REG, stub_reg)


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc(e, case):
    e.cl = CL.PpcLog({ADDR[k]: v for k, v in LABELS.items()}, {})
    saved = STUBS.activate(e)
    try:
        T.call_ppc(e, FN, [])
        return {"log": e.cl.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    lib.tpw_pl_addr.restype, lib.tpw_pl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_pl_layout.argtypes = [C.POINTER(C.c_int64)]
    CL.host_begin(lib, {v: lib.tpw_pl_addr(k.encode()) for k, v in LABELS.items()}, {})
    lib.tpw_pl_sinit()
    return {"log": CL.host_end(lib)}


def edges():
    yield {}


def rnd(r):
    return {}


TESTS = [
    FnTest("__sinit_ui_playerLogin_cpp", ppc, host, edge=edges, rnd=rnd),
]
