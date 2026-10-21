"""Unit v5750, TbRect::Intersects 0x1002a06c (bullfrog), and unit v5L16, TbRect::Normalise 0x1002a1d4, against
libtpw_unit_TbRect_Intersects.dylib (src/bullfrog/LbRect.cpp).

Intersects: two rectangles are built in each memory (four signed words each); the answer (low byte) and both rectangles are compared.
Normalise: one rectangle and the word after it; the five words are compared. No callees, no fused operations.
"""
import ctypes as C
import random

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_TbRect_Intersects"
MASK = 0xFFFFFFFF
FN = 0x1002A06C
FN_NORMALISE = 0x1002A1D4
HOST_LAYOUT = [4, 8, 12, 16]


def setup(runner):
    pass


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rect_alloc.restype, lib.tpw_rect_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rect_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rect_intersects.restype, lib.tpw_rect_intersects.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    lib.tpw_rect_normalise.restype, lib.tpw_rect_normalise.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rect_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def ppc(e, case):
    a = e.alloc(16)
    b = e.alloc(16)
    e.write(a, b"".join((w & MASK).to_bytes(4, "big") for w in case["a"]))
    e.write(b, b"".join((w & MASK).to_bytes(4, "big") for w in case["b"]))
    r3, _, _ = T.call_ppc(e, FN, [a, b])
    return {"answer": r3 & 0xFF, "a": e.read(a, 16), "b": e.read(b, 16)}


def host(lib, case):
    bind(lib)
    a = lib.tpw_rect_alloc(16)
    b = lib.tpw_rect_alloc(16)
    C.memmove(a, b"".join((w & MASK).to_bytes(4, "little") for w in case["a"]), 16)
    C.memmove(b, b"".join((w & MASK).to_bytes(4, "little") for w in case["b"]), 16)
    ans = lib.tpw_rect_intersects(a, b) & 0xFF
    # host memory is little-endian: re-express as big-endian bytes for comparison
    def be(p):
        return b"".join(int.from_bytes(C.string_at(p + 4 * i, 4), "little").to_bytes(4, "big") for i in range(4))
    return {"answer": ans, "a": be(a), "b": be(b)}


VALS = [0, 1, -1, 5, 6, 4, 10, 11, 9, -2 ** 31, 2 ** 31 - 1]


def edges():
    # boundaries on each of the four compares: equal, one below, one above
    for v in VALS:
        for w in VALS:
            yield {"a": [0, 0, 5, 5], "b": [v, 0, w, 5]}
            yield {"a": [0, 0, 5, 5], "b": [0, v, 5, w]}
            yield {"a": [v, 0, w, 5], "b": [0, 0, 5, 5]}
            yield {"a": [0, v, 5, w], "b": [0, 0, 5, 5]}
    for x in (4, 5, 6, -1, 0, 1):
        for y in (4, 5, 6, -1, 0, 1):
            yield {"a": [0, 0, 5, 5], "b": [x, y, x + 5, y + 5]}


def rnd(r):
    def v():
        return r.choice([r.randrange(-10, 10), r.randrange(-2 ** 31, 2 ** 31)])
    return {"a": [v() for _ in range(4)], "b": [v() for _ in range(4)]}


# ---- Normalise: the rectangle and a guard word after it


def n_ppc(e, case):
    a = e.alloc(20)
    e.write(a, b"".join((w & MASK).to_bytes(4, "big") for w in case["r"] + [0x5A5A5A5A]))
    T.call_ppc(e, FN_NORMALISE, [a])
    return {"r": [T.s32(int.from_bytes(e.read(a + 4 * i, 4), "big")) for i in range(5)]}


def n_host(lib, case):
    bind(lib)
    a = lib.tpw_rect_alloc(20)
    C.memmove(a, b"".join((w & MASK).to_bytes(4, "little") for w in case["r"] + [0x5A5A5A5A]), 20)
    lib.tpw_rect_normalise(a)
    return {"r": [T.s32(int.from_bytes(C.string_at(a + 4 * i, 4), "little")) for i in range(5)]}


def n_edges():
    # each pair below, equal and above (signed: the extremes and a sign change), the other pair any way round
    pairs = [(0, 0), (1, 0), (0, 1), (-1, 0), (0, -1), (5, 5), (6, 5), (5, 6), (-2 ** 31, 2 ** 31 - 1), (2 ** 31 - 1, -2 ** 31),
             (-2 ** 31, -2 ** 31), (2 ** 31 - 1, 2 ** 31 - 1), (-5, 3), (3, -5)]
    for x in pairs:
        for y in pairs:
            yield {"r": [x[0], y[0], x[1], y[1]]}


def n_rnd(r):
    def v():
        return r.choice([r.randrange(-10, 10), r.randrange(-2 ** 31, 2 ** 31)])
    return {"r": [v() for _ in range(4)]}


TESTS = [FnTest("TbRect::Intersects", ppc, host, edge=edges, rnd=rnd),
         FnTest("TbRect::Normalise", n_ppc, n_host, edge=n_edges, rnd=n_rnd)]
