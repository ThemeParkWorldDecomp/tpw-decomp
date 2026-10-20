"""Unit v5577, vector<facet*>::~vector 0x100724d0, against libtpw_unit_std_vector_Dtor_vector.dylib.

A case: a 16 byte vector object (NULL or not) whose word at +8 is a buffer (NULL or not), and the delete flag (signed 16 bit).
operator delete (0x10238218) is a fake on both sides logging which block it got (1 vector, 2 buffer). Compared: the returned
this (as 'is it the vector'), the delete events. No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_vector_Dtor_vector"
FN = 0x100724d0
DELETE = 0x10238218
MASK = 0xFFFFFFFF

CUR = {"vec": 0, "buf": 0}
STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_delete(e):
    p = e.r(3)
    k = 1 if p == CUR["vec"] else (2 if p == CUR["buf"] else 0)
    e.cl.rec("Delete", (k,))


STUBS.internal(DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_vd_alloc.restype, lib.tpw_vd_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_vd_dtor.restype, lib.tpw_vd_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.bound = True


def ppc(e, case):
    vec = e.alloc(16) if case["vec"] else 0
    buf = e.alloc(16) if case["buf"] else 0
    if vec:
        e.write(vec + 8, buf.to_bytes(4, "big"))
    CUR["vec"], CUR["buf"] = vec, buf
    e.cl = CL.PpcLog({}, {})
    saved = STUBS.activate(e)
    try:
        r3, _, _ = T.call_ppc(e, FN, [vec, case["flag"] & 0xFFFF])
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"same": r3 == vec, "log": e.cl.log}


def host(lib, case):
    bind(lib)
    vec = lib.tpw_vd_alloc(16) if case["vec"] else 0
    buf = lib.tpw_vd_alloc(16) if case["buf"] else 0
    if vec:
        C.memmove(vec + 8, buf.to_bytes(8, "little"), 8)
    CL.host_begin(lib, {}, {})
    r = lib.tpw_vd_dtor(vec, case["flag"])
    return {"same": r == vec, "log": CL.host_end(lib)}


def mk(vec, buf, flag):
    return {"vec": vec, "buf": buf, "flag": flag}


def edges():
    for vec in (0, 1):
        for buf in (0, 1):
            for flag in (0, 1, -1, 0x7FFF, -0x8000, 0x10000, 0x18000, 2):
                yield mk(vec, buf, flag)


def rnd(r):
    return mk(r.choice([0, 1, 1]), r.choice([0, 1]), r.choice([0, 1, -1, r.randrange(-40000, 40000)]))


TESTS = [FnTest("std::vector<std::locale::facet*,std::allocator<std::locale::facet*>>::~vector", ppc, host, edge=edges, rnd=rnd)]
