"""Unit v5271, FSOnlineCityServerConnectionCallback 0x101733b0 (src/game/online/FS_Online.cpp), against
libtpw_unit_FSOnlineCityServerConnectionCallback.dylib (tests/unit_FSOnlineCityServerConnectionCallback.cpp).

A case is gOnlineSystem set (an object) or NULL and four arguments. The constructor and CityServerConnectionCallback are fakes on both
sides that record their arguments (the object as a label; a new object is label 100). Compared: the event log and whether gOnlineSystem is
set afterwards. No fused operations. Not tested: nothing excluded (operator new never fails).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FSOnlineCityServerConnectionCallback"
MASK = 0xFFFFFFFF

FN = 0x101733B0
SIZES = {FN: 124}
OWN = set(SIZES)
CTOR = 0x1016F7D0
CALLBACK = 0x101727F8
GLOBAL = 0x10380AB0

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(CTOR, CL.recorder("ctor", "N"))
STUBS.internal(CALLBACK, CL.recorder("Callback", "Liiuu"))


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cs_set.argtypes = [C.c_uint32]
    lib.tpw_cs_get.restype = C.c_uint32
    lib.tpw_cs_call.argtypes = [C.c_int32, C.c_int32, C.c_uint32, C.c_uint32]
    lib.bound = True


def ppc(e, case):
    e.cl = CL.PpcLog({}, {})
    saved = STUBS.activate(e)
    try:
        obj = e.alloc(16) if case["existing"] else 0
        e.write(GLOBAL, obj.to_bytes(4, "big"))
        a, b, c, d = case["args"]
        T.call_ppc(e, FN, [a & MASK, b & MASK, c, d])
        return {"log": e.cl.log, "set": int(e.u32(GLOBAL) != 0)}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    bind(lib)
    CL.host_begin(lib, {}, {})
    lib.tpw_cs_set(case["existing"])
    a, b, c, d = case["args"]
    lib.tpw_cs_call(T.s32(a), T.s32(b), c, d)
    return {"log": CL.host_end(lib), "set": int(lib.tpw_cs_get())}


def mk(r, existing=None):
    return {"existing": r.choice([0, 1]) if existing is None else existing,
            "args": [r.choice([0, 1, -1, 0x80000000, r.getrandbits(32)]), r.choice([0, -1, r.getrandbits(32)]),
                     r.choice([0, r.getrandbits(32)]), r.choice([0, r.getrandbits(32)])]}


def edges():
    r = random.Random(5271)
    for ex in (0, 1):
        for _ in range(3):
            yield mk(r, ex)


def rnd(r):
    return mk(r)


TESTS = [
    FnTest("FSOnlineCityServerConnectionCallback", ppc, host, edge=edges, rnd=rnd),
]
