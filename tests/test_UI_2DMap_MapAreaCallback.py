"""Unit v5751, UI_2DMap_MapAreaCallback 0x1022bff8, against libtpw_unit_UI_2DMap_MapAreaCallback.dylib
(tests/unit_UI_2DMap_MapAreaCallback.cpp), src/UpdateSatellite_UI_2DMap_MapAreaCallback.cpp.

The function forwards its four arguments to CUI2DMap::MapAreaCallback on gUI2DMap (0x1039742c). The callee is a fake on both
sides recording (this label, window label, message, a, b). Compared: the event log. No fused operations. Not tested: nothing.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UI_2DMap_MapAreaCallback"
MASK = 0xFFFFFFFF

FN = 0x1022BFF8
CALLEE = 0x1022D0AC
GMAP = 0x1039742C

STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_callee(e):
    L = e.cl
    L.rec("MapAreaCallback", (L.lab(e.r(3)), L.lab(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), T.s32(e.r(7))))


STUBS.internal(CALLEE, stub_callee)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_mac_addr.restype, lib.tpw_mac_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_mac_alloc.restype, lib.tpw_mac_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_mac_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_mac_call.restype, lib.tpw_mac_call.argtypes = None, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_mac_layout(out)
    assert list(out)[:n] == []
    lib.bound = True


def ppc(e, case):
    w = e.alloc(64) if case["win"] else 0
    fixed = {GMAP: 1}
    if w:
        fixed[w] = 2
    e.cl = CL.PpcLog(fixed, {})
    saved = STUBS.activate(e)
    try:
        T.call_ppc(e, FN, [w, case["m"] & MASK, case["a"] & MASK, case["b"] & MASK])
        return {"log": e.cl.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    bind(lib)
    g = lib.tpw_mac_addr(b"gUI2DMap")
    w = lib.tpw_mac_alloc(64) if case["win"] else 0
    CL.host_begin(lib, {1: g, 2: w} if w else {1: g}, {})
    lib.tpw_mac_call(w, T.s32(case["m"]), T.s32(case["a"]), T.s32(case["b"]))
    return {"log": CL.host_end(lib)}


def mk(win, m, a, b):
    return {"win": win, "m": m, "a": a, "b": b}


def edges():
    for win in (0, 1):
        for v in (0, 1, -1, 0x7FFFFFFF, -0x80000000):
            yield mk(win, v, 5, -7)
            yield mk(win, 3, v, 9)
            yield mk(win, 3, 4, v)


def rnd(r):
    return mk(r.choice([0, 1]), T.rint(r), T.rint(r), T.rint(r))


TESTS = [FnTest("UI_2DMap_MapAreaCallback", ppc, host, edge=edges, rnd=rnd)]
