"""Unit v5204, the static initialiser of ui_Postcard.cpp, against libtpw_unit_sinit_ui_Postcard_cpp.dylib, main app:
  __sinit_ui_Postcard_cpp 0x101b30c8

The function takes no input. It constructs eighteen narrow strings and one wide string in a bss block and registers each with
__register_global_object. The string constructors, __register_global_object are fakes on both sides that record the object (as a
label), the text and which destructor glue was passed. Compared: the event log and the bss block (untouched by the fakes).
No fused operations. Not tested: nothing.
"""
import ctypes as C

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_ui_Postcard_cpp"
MASK = 0xFFFFFFFF

FN = 0x101B30C8
BLOB = 0x10395700
BLOB_SIZE = 0x260
CTOR_C, CTOR_W, REG = 0x102382D8, 0x10238800, 0x10239940
DTOR_C, DTOR_W = 0x103BCD38, 0x103BCBB8

# object offsets in the blob that get labels: strings and nodes
OFFSETS = [0x00, 0x14, 0x20, 0x34, 0x40, 0x54] + [0x60 + 0x14 * i for i in range(16)] + [0x1A0 + 0xC * i for i in range(16)]
LABELS = {off: i + 1 for i, off in enumerate(OFFSETS)}

STUBS = CL.Stubs()
STUBS.real.add(FN)
def glue(fn):
    """The real glue saves the TOC pointer in the caller's frame slot 0x14(r1), which the caller reloads."""
    def run(e):
        e.write(e.r(1) + 0x14, (e.r(2) & MASK).to_bytes(4, "big"))
        return fn(e)
    return run


STUBS.internal(CTOR_C, glue(CL.recorder("ctorC", "Ls", ret=lambda e, log: e.r(3))))
STUBS.internal(CTOR_W, glue(CL.recorder("ctorW", "LS", ret=lambda e, log: e.r(3))))


def stub_reg(e):
    L = e.cl
    d = e.r(4)
    L.rec("reg", (L.lab(e.r(3)), 1 if d == DTOR_C else 2 if d == DTOR_W else 0, L.lab(e.r(5))))


STUBS.internal(REG, glue(stub_reg))


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ps_blob.restype = C.c_uint64
    lib.tpw_ps_run.restype = None
    lib.bound = True


def ppc(e, case):
    e.cl = CL.PpcLog({BLOB + off: n for off, n in LABELS.items()}, {})
    saved = STUBS.activate(e)
    try:
        T.call_ppc(e, FN, [])
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"log": e.cl.log}


def host(lib, case):
    bind(lib)
    b = lib.tpw_ps_blob()
    CL.host_begin(lib, {n: b + off for off, n in LABELS.items()}, {})
    lib.tpw_ps_run()
    return {"log": CL.host_end(lib)}


def edges():
    yield {}


TESTS = [
    FnTest("__sinit_ui_Postcard_cpp", ppc, host, edge=edges, rnd=lambda r: {}),
]
