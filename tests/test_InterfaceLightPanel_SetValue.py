"""Unit v5770, InterfaceLightPanel::SetValue 0x101d0a50, against libtpw_unit_InterfaceLightPanel_SetValue.dylib
(tests/unit_InterfaceLightPanel_SetValue.cpp), src/ui_whatami_InterfaceLightPanel_SetValue.cpp.

A case is an object with an old mValue and a call with a new value. The callees InterfaceWindow::GetWindow (answers from a queue),
SetFrame and SetValue are fakes on both sides that record their arguments. Compared: the answer, the object's mValue, the event log.
No fused operations. Not tested: nothing.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InterfaceLightPanel_SetValue"
MASK = 0xFFFFFFFF

FN = 0x101D0A50
SIZES = {FN: 216}
GETWINDOW, SETFRAME, SETVALUE = 0x101DBE40, 0x101DB7DC, 0x101DFE84
MAC_OFF = 0x138
HOST = {}

STUBS = CL.Stubs()
STUBS.real.update(SIZES)


def stub_getwindow(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), T.s32(e.r(4))))
    e.set_r(3, 0x50000000 if L.ret("GetWindow") else 0)


def stub_setframe(e):
    L = e.cl
    L.rec("SetFrame", (L.lab(e.r(3)), T.s32(e.r(4))))


def stub_setvalue(e):
    L = e.cl
    L.rec("SetValue", (L.lab(e.r(3)), T.s32(e.r(4))))
    e.set_r(3, 0)


STUBS.internal(GETWINDOW, stub_getwindow)
STUBS.internal(SETFRAME, stub_setframe)
STUBS.internal(SETVALUE, stub_setvalue)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lp_alloc.restype, lib.tpw_lp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lp_set_value.restype, lib.tpw_lp_set_value.argtypes = C.c_int32, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lp_layout(out)
    assert n == 2
    HOST["off"], HOST["size"] = out[0], out[1]
    lib.bound = True


def ppc(e, case):
    e.cl = CL.PpcLog({}, case["queues"])
    saved = STUBS.activate(e)
    try:
        obj = e.alloc(0x200)
        e.write(obj + MAC_OFF, struct.pack(">I", case["old"] & MASK))
        r3, _, _ = T.call_ppc(e, FN, [obj, case["new"] & MASK])
        return {"answer": r3 & MASK, "mValue": struct.unpack(">I", e.read(obj + MAC_OFF, 4))[0], "log": e.cl.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    bind(lib)
    CL.host_begin(lib, {}, case["queues"])
    obj = lib.tpw_lp_alloc(0x200)
    C.memmove(obj + HOST["off"], struct.pack("<I", case["old"] & MASK), 4)
    r = lib.tpw_lp_set_value(obj, T.s32(case["new"]))
    mv = struct.unpack("<I", C.string_at(obj + HOST["off"], 4))[0]
    return {"answer": r & MASK, "mValue": mv, "log": CL.host_end(lib)}


def mk(r, old, new):
    return {"old": old & MASK, "new": new & MASK, "queues": {"GetWindow": [r.choice([0, 1]) for _ in range(40)]}}


def edges():
    r = random.Random(77)
    vals = [0, MASK, 0x7FFFFFFF, 0x80000000, 1, 0x40000000, 0x55555555, 0x2AAAAAAA]
    for o in vals:
        for n in vals:
            c = mk(r, o, n)
            yield c
            c = mk(r, o, n)
            c["queues"] = {"GetWindow": [1]}
            yield c
            c = mk(r, o, n)
            c["queues"] = {"GetWindow": [0]}
            yield c


def rnd(r):
    return mk(r, r.getrandbits(32), r.getrandbits(32))


TESTS = [FnTest("InterfaceLightPanel::SetValue", ppc, host, edge=edges, rnd=rnd)]
