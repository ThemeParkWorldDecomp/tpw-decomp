"""Unit v5134, ui_ErrorDisplay.cpp's static initialiser, against libtpw_unit_sinit_ui_ErrorDisplay_cpp.dylib
(tests/unit_sinit_ui_ErrorDisplay_cpp.cpp), main app, src/sinit_ui_ErrorDisplay_cpp.cpp:
  __sinit_ui_ErrorDisplay_cpp 0x101f499c

A case is a call. The deque constructor, the copy, tear_down, operator delete, the mutex constructor and __register_global_object are
fakes on both sides recording their arguments (globals as labels). The deque constructor fake writes a per-case word at +0x10 of the
temporary (the block table pointer the original tests before calling operator delete); the mutex constructor answers a per-case
value that the second registration receives. Compared: the event log. Not tested: nothing skipped; no fused operations.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_ui_ErrorDisplay_cpp"
MASK = 0xFFFFFFFF

FN = 0x101F499C
SIZES = {FN: 168}
OWN = set(SIZES)

CTOR, COPY, TEARDOWN, DELETE, MUTEX_CTOR, REGISTER = 0x101F5158, 0x101F4D14, 0x101F4A74, 0x10238218, 0x10200CA4, 0x10239940

ADDR = dict(queue=0x10396454, mutex=0x10396444, qdtor=0x102685E0, mdtor=0x102686A8, qnode=0x10396470, mnode=0x10396448)
LABELS = dict(queue=1, mutex=2, qdtor=3, mdtor=4, qnode=5, mnode=6)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s_ctor(e):
    e.cl.rec("DequeCtor", ())
    e.write(e.r(3) + 0x10, struct.pack(">I", e.cl.ret("tmpword") & MASK))


def s_copy(e):
    e.cl.rec("DequeCopy", (e.cl.lab(e.r(3)), e.u32(e.r(4) + 0x10)))


def s_tear(e):
    e.cl.rec("TearDown", (e.u32(e.r(3) + 0x10),))


def save_toc(e):
    """The glue's cross-TOC call saves r2 at 0x14(r1); the caller reloads it afterwards."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def s_delete(e):
    save_toc(e)
    e.cl.rec("OperatorDelete", (e.r(3) & MASK,))


def s_mutex(e):
    e.cl.rec("MutexCtor", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("mutexret") & MASK)


def s_register(e):
    save_toc(e)
    L = e.cl
    L.rec("Register", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))


STUBS.internal(CTOR, s_ctor)
STUBS.internal(COPY, s_copy)
STUBS.internal(TEARDOWN, s_tear)
STUBS.internal(DELETE, s_delete)
STUBS.internal(MUTEX_CTOR, s_mutex)
STUBS.internal(REGISTER, s_register)


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc(e, case):
    e.cl = CL.PpcLog({ADDR[k]: LABELS[k] for k in ADDR}, case["queues"])
    saved = STUBS.activate(e)
    try:
        T.call_ppc(e, FN, [])
        return {"log": e.cl.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sinit_addr.restype, lib.tpw_sinit_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sinit_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sinit_run.restype = None
    lib.bound = True


def host(lib, case):
    bind(lib)
    CL.host_begin(lib, {LABELS[k]: lib.tpw_sinit_addr(k.encode()) for k in ADDR}, case["queues"])
    lib.tpw_sinit_run()
    return {"log": CL.host_end(lib)}


def mk(tmp, mut):
    return {"queues": {"tmpword": [tmp], "mutexret": [mut]}}


def edges():
    for tmp in (0, 1, 0x12345670, 0xFFFFFFF0):
        for mut in (0, 0x77, 0x80000000):
            yield mk(tmp, mut)


def rnd(r):
    return mk(r.choice([0, r.getrandbits(32)]), r.choice([0, r.getrandbits(32)]))


TESTS = [
    FnTest("__sinit_ui_ErrorDisplay_cpp", ppc, host, edge=edges, rnd=rnd),
]
