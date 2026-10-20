"""Unit v5703, LbString_strnzcpy 0x10066200, against libtpw_unit_LbString_strnzcpy.dylib.

A case is a source string and a size; the destination is a 64-byte block pre-filled with 0xAA. strlen and memcpy are the C library's
(stubbed identically). Compared: the answer (low byte), the whole destination block and the source block. No fused operations.
Not tested: none (size 0 is tested with strings short enough for the block).
"""
import ctypes as C
import random

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbString_strnzcpy"
MASK = 0xFFFFFFFF
FN = 0x10066200
DST = 64
SRC = 64


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sz_alloc.restype, lib.tpw_sz_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sz_strnzcpy.restype = C.c_int32
    lib.tpw_sz_strnzcpy.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32]
    lib.bound = True


def ppc(e, case):
    dst = e.alloc(DST)
    src = e.alloc(SRC)
    e.write(dst, bytes([0xAA]) * DST)
    e.write(src, case["src"] + b"\0" + bytes([0x55]) * (SRC - len(case["src"]) - 1))
    r3, _, _ = T.call_ppc(e, FN, [dst, src, case["size"] & MASK])
    return {"answer": r3 & 0xFF, "dst": e.read(dst, DST), "src": e.read(src, SRC)}


def host(lib, case):
    bind(lib)
    dst = lib.tpw_sz_alloc(DST)
    src = lib.tpw_sz_alloc(SRC)
    C.memmove(dst, bytes([0xAA]) * DST, DST)
    data = case["src"] + b"\0" + bytes([0x55]) * (SRC - len(case["src"]) - 1)
    C.memmove(src, data, SRC)
    r = lib.tpw_sz_strnzcpy(dst, src, case["size"] & MASK)
    return {"answer": r & 0xFF, "dst": C.string_at(dst, DST), "src": C.string_at(src, SRC)}


def mk(r, n, size):
    return {"src": bytes(r.randrange(1, 256) for _ in range(n)), "size": size}


def edges():
    r = random.Random(7)
    for n in (0, 1, 2, 5, 10):
        for size in (0, 1, 2, n - 1, n, n + 1, n + 2, n + 3, 40):
            if size >= 0:
                yield mk(r, n, size)
    yield mk(r, 40, 0)
    yield mk(r, 60, 62)


def rnd(r):
    n = r.randrange(0, 40)
    return mk(r, n, r.choice([0, 1, r.randrange(0, 50), n, n + 1, n + 2]))


TESTS = [FnTest("LbString_strnzcpy", ppc, host, edge=edges, rnd=rnd)]
