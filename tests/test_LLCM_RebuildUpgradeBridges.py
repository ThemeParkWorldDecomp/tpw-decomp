"""Unit v5059, LLCM_CoverUpgrade and LLCM_RebuildUpgradeBridges (the bridge upgrade stretching), against
libtpw_unit_LLCM_RebuildUpgradeBridges.dylib (tests/unit_LLCM_RebuildUpgradeBridges.cpp), main app,
src/LLCellMeshes_LLCM_RebuildUpgradeBridges.cpp:
  LLCM_RebuildUpgradeBridges 0x10082cf0, LLCM_CoverUpgrade 0x100828e4

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian, host memory little-endian).
The callees outside the unit (GetTrackCellEquiv, IsTrackX, GetMapCellEquiv, IsFlag, GetNeighbouringCell, IsTrack, LLCC_DrawTrackLine) are
fakes on both sides that record their arguments (pointers as offsets from the world block) and answer from per-case queues.

The world of a case: gGameWorld pointing at a block with 400 map cells (0x44 bytes from +0x2d8, a half index at +0 and a half id at +0x10, random),
the five compass offsets (initialised on the PowerPC side; fixed on the host side). What is compared: the log, and for CoverUpgrade the two
coordinate words afterwards.

No fused operations. Not tested: negative coordinates (the cell would lie far outside the block the fakes need).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCM_RebuildUpgradeBridges"
MASK = 0xFFFFFFFF

FN_COVER, FN_REBUILD = 0x100828E4, 0x10082CF0
OWN = {FN_COVER, FN_REBUILD}

C_TRACKEQ, C_TRACKX, C_MAPEQ, C_ISFLAG, C_NEIGH, C_ISTRACK, C_DRAW = (
    0x100F48C4, 0x100A367C, 0x100F49A0, 0x100A2918, 0x1012FCB8, 0x100A3574, 0x100A30F8)

ADDR = dict(world=0x1037EBFC)
CONSTS = {0x1034C2AC: (0, -1), 0x1034C284: (1, 0), 0x1034C25C: (0, 1), 0x1034C234: (-1, 0)}
BLOCK = 0x7000
CELLS = 400
CUR = {"base": 0}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def off(e):
    return (e.r(3) - CUR["base"]) & MASK


def ptr_answer(e, key):
    v = e.cl.ret(key) & MASK
    e.set_r(3, (CUR["base"] + v) & MASK if v else 0)


def stub_trackeq(e):
    e.cl.rec("GetTrackCellEquiv", (off(e),))
    ptr_answer(e, "te")


def stub_trackx(e):
    e.cl.rec("IsTrackX", (off(e),))
    e.set_r(3, e.cl.ret("tx") & MASK)


def stub_mapeq(e):
    e.cl.rec("GetMapCellEquiv", (off(e),))
    ptr_answer(e, "me")


def stub_isflag(e):
    e.cl.rec("IsFlag", (off(e), e.r(4) & 0xFFFF))
    e.set_r(3, e.cl.ret("flag") & MASK)


def stub_istrack(e):
    e.cl.rec("IsTrack", (off(e),))
    e.set_r(3, e.cl.ret("trk") & MASK)


def stub_neigh(e):
    d = e.r(4)
    e.cl.rec("GetNeighbouringCell", (off(e), e.s32(d), e.s32(d + 4), s32(e.r(5))))
    ptr_answer(e, "nb")


def stub_draw(e):
    e.cl.rec("DrawTrackLine", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), e.s32(e.r(6)), e.s32(e.r(7))))


STUBS.internal(C_TRACKEQ, stub_trackeq)
STUBS.internal(C_TRACKX, stub_trackx)
STUBS.internal(C_MAPEQ, stub_mapeq)
STUBS.internal(C_ISFLAG, stub_isflag)
STUBS.internal(C_NEIGH, stub_neigh)
STUBS.internal(C_ISTRACK, stub_istrack)
STUBS.internal(C_DRAW, stub_draw)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def setptr(self, a, p):
        self.words(a, [p])

    def init_consts(self):
        for a, (x, y) in CONSTS.items():
            self.words(a, [x, y, 0])

    def log(self):
        return self.L.log

    def cover(self, px, py):
        T.call_ppc(self.e, FN_COVER, [px, py])

    def rebuild(self, x, y):
        T.call_ppc(self.e, FN_REBUILD, [x & MASK, y & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_llcm_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_llcm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setptr(self, a, p):
        self.words(a, [p & MASK, p >> 32])

    def init_consts(self):
        pass

    def log(self):
        return CL.host_end(self.lib)

    def cover(self, px, py):
        self.lib.tpw_llcm_cover(px, py)

    def rebuild(self, x, y):
        self.lib.tpw_llcm_rebuild(s32(x), s32(y))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_llcm_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_llcm_cover.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_llcm_rebuild.argtypes = [C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == [12, 4], list(out)[:n]
    lib.bound = True


def build(s, case):
    base = s.alloc(BLOCK)
    CUR["base"] = base
    s.setptr(s.g("world"), base)
    s.init_consts()
    for k, (idx, ident) in enumerate(case["cells"]):
        a = base + 0x2D8 + k * 0x44
        s.half(a, idx)
        s.half(a + 0x10, ident)
    xy = s.alloc(8)
    s.words(xy, [case["x"], case["y"]])
    return xy


def execute(s, case, xy):
    if case["op"] == "cover":
        s.cover(xy, xy + 4)
    else:
        s.rebuild(case["x"], case["y"])


def run(s, case):
    try:
        xy = build(s, case)
        execute(s, case, xy)
        return {"log": s.log(), "xy": s.rd_words(xy, 2)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def coff(k):
    return 0x2D8 + k * 0x44


def base(r, op, p=0.7, same=False, mask=None, x=None, y=None, tx=None):
    cells = [(r.choice([0, 1, 5, 100, r.randrange(1, 400)]), 1 if same else r.choice([0, 1, 1, 2])) for _ in range(CELLS)]
    n = 6
    def bl():
        return [1 if r.random() < p else 0 for _ in range(n)]
    nb = [coff(r.randrange(CELLS)) if (r.random() < p if mask is None else (mask >> i) & 1) else 0 for i in range(4)]
    q = {
        "te": [coff(r.randrange(CELLS)) for _ in range(2)],
        "tx": [r.choice([0, 1]) if tx is None else tx],
        "me": [coff(r.randrange(CELLS)) for _ in range(n)],
        "flag": bl(),
        "trk": bl(),
        "nb": nb,
    }
    if same:
        q["flag"] = [1]
        q["trk"] = [1]
    return {"op": op, "queues": q, "cells": cells,
            "x": r.randrange(0, 4) if x is None else x, "y": r.randrange(0, 4) if y is None else y}


def edges(op, seed):
    def gen():
        r = random.Random(seed)
        for tx in (0, 1):
            for mask in range(16):
                yield base(r, op, same=True, mask=mask, tx=tx)
            for p in (0.0, 1.0):
                yield base(r, op, p=p, tx=tx)
            yield base(r, op, x=0, y=0, tx=tx)
            yield base(r, op, x=3, y=3, tx=tx)
    return gen


def rnd(op):
    def f(r):
        return base(r, op, p=r.choice([0.3, 0.7, 0.9]), same=r.random() < 0.5)
    return f


TESTS = [
    FnTest("LLCM_CoverUpgrade", ppc, host, edge=edges("cover", 11), rnd=rnd("cover")),
    FnTest("LLCM_RebuildUpgradeBridges", ppc, host, edge=edges("rebuild", 12), rnd=rnd("rebuild")),
]
