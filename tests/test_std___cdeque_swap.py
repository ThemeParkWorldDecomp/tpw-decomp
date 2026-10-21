"""Unit v5105, std::__cdeque<const void*>::swap 0x101f2668, against libtpw_unit_std___cdeque_swap.dylib, main app, src/game/MslSupport.cpp.

A case is two 16-byte blocks (four words each) and a call swap(a, b); b may be the same block as a. Compared: both blocks after the call.
No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___cdeque_swap"
MASK = 0xFFFFFFFF
FN = 0x101F2668
HOST_LAYOUT = [4, 8, 12, 16]


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cdq_alloc.restype, lib.tpw_cdq_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cdq_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cdq_swap.restype, lib.tpw_cdq_swap.argtypes = None, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cdq_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def ppc(e, case):
    a = e.alloc(16)
    b = a if case["same"] else e.alloc(16)
    e.write(a, struct.pack(">4I", *case["a"]))
    if not case["same"]:
        e.write(b, struct.pack(">4I", *case["b"]))
    T.call_ppc(e, FN, [a, b])
    return {"a": list(struct.unpack(">4I", e.read(a, 16))), "b": list(struct.unpack(">4I", e.read(b, 16)))}


def host(lib, case):
    bind(lib)
    a = lib.tpw_cdq_alloc(16)
    b = a if case["same"] else lib.tpw_cdq_alloc(16)
    C.memmove(a, struct.pack("<4I", *case["a"]), 16)
    if not case["same"]:
        C.memmove(b, struct.pack("<4I", *case["b"]), 16)
    lib.tpw_cdq_swap(a, b)
    return {"a": list(struct.unpack("<4I", C.string_at(a, 16))), "b": list(struct.unpack("<4I", C.string_at(b, 16)))}


def mk(r, same):
    return {"same": same, "a": [r.getrandbits(32) for _ in range(4)], "b": [r.getrandbits(32) for _ in range(4)]}


def edges():
    r = random.Random(5105)
    yield mk(r, True)
    yield mk(r, False)
    yield {"same": False, "a": [0, 0, 0, 0], "b": [MASK] * 4}
    yield {"same": False, "a": [1, 2, 3, 4], "b": [1, 2, 3, 4]}


def rnd(r):
    return mk(r, r.random() < 0.2)


TESTS = [
    FnTest("std::__cdeque<const_void*,std::allocator<const_void*>>::swap", ppc, host, edge=edges, rnd=rnd),
]
