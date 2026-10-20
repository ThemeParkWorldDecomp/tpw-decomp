"""Unit v5644, SaveRideHeightMap 0x1005e6a0 (src/RideDestroy_LoadFileIntoMemory_SaveRideHeightMap.cpp), against
libtpw_unit_SaveRideHeightMap.dylib (tests/unit_SaveRideHeightMap.cpp).

A case builds a height map (12 header words, width and height as two 16-bit halves at +8, source pointers at +0xc, +0x10, +0x14, any of them
NULL) twice, in emulator memory and in host memory. MReleaseAllocF, MReleaseFreeF, CTRACE, fopen, fwrite and fclose are fakes on both sides
that record their arguments; fwrite also records a hash of the whole block it is given (header halves and words in each side's own byte order,
then the plane bytes), so the written block is compared. fopen and fwrite answer from the case's queues. memcpy is the real one on both sides.

Compared: the answer and the event log. No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SaveRideHeightMap"
MASK = 0xFFFFFFFF

FN = 0x1005E6A0
SIZES = {FN: 524}
OWN = set(SIZES)

ALLOC, FREE, CTRACE = 0x10064BD4, 0x10064C54, 0x1006FCBC
HOST_LAYOUT = [0xC, 0x18, 0x30]

PATHS = [b"ride.hm", b"a", b"dir/some file.bin"]
BLOCK = [0]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_alloc(e):
    L = e.cl
    size, flags = e.r(3) & MASK, e.r(4) & MASK
    L.rec("Alloc", (size, flags))
    BLOCK[0] = e.alloc(max(size, 4))
    e.set_r(3, BLOCK[0])


def stub_free(e):
    e.cl.rec("Free", (1 if (e.r(3) & MASK) == BLOCK[0] else 0,))
    e.set_r(3, 0)


def stub_fwrite(e):
    L = e.cl
    buf, size, count, f = e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK
    data = e.read(buf, size * count)
    h = 0xCBF29CE484222325

    def mix(v):
        nonlocal h
        h = ((h ^ v) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    mix(struct.unpack(">I", data[0:4])[0])
    mix(struct.unpack(">I", data[4:8])[0])
    mix(struct.unpack(">H", data[8:10])[0])
    mix(struct.unpack(">H", data[10:12])[0])
    for i in range(12, 48, 4):
        mix(struct.unpack(">I", data[i:i + 4])[0])
    for b in data[48:]:
        mix(b)
    L.rec("fwrite", (size, count, f, h & 0x3FFFFFFFFFFFFFFF))
    e.set_r(3, L.ret("fwrite") & MASK)


STUBS.internal(ALLOC, stub_alloc)
STUBS.internal(FREE, stub_free)
STUBS.internal(CTRACE, CL.recorder("CTRACE", "uss"))
STUBS.imp("fopen", CL.recorder("fopen", "ss", ret="i"))
STUBS.imp("fwrite", stub_fwrite)
STUBS.imp("fclose", CL.recorder("fclose", "u"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(max(n, 4))

    def ptr(self, a):
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def save(self, m, p):
        r3, _, _ = T.call_ppc(self.e, FN, [m, p])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_srhm_alloc(max(n, 4))

    def ptr(self, a):
        return self.lib.tpw_srhm_off(a)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def save(self, m, p):
        return self.lib.tpw_srhm_save(m, p) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_srhm_alloc.restype, lib.tpw_srhm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_srhm_off.restype, lib.tpw_srhm_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_srhm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_srhm_save.restype, lib.tpw_srhm_save.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_srhm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        w, h = case["w"], case["h"]
        n = w * h
        m = s.alloc(0x30)
        s.words(m, case["words"])
        s.half(m + 8, w)
        s.half(m + 10, h)
        for off, key, size in ((0xC, "cells", 25 * n), (0x10, "p1", n), (0x14, "p2", n)):
            if case[key + "_null"]:
                s.words(m + off, [0])
            else:
                a = s.alloc(size)
                if size:
                    s.write(a, case[key])
                s.words(m + off, [s.ptr(a)])
        p = s.alloc(len(case["path"]) + 1)
        s.write(p, case["path"] + b"\0")
        s.save(m, p)  # declared void in the header (the caller ignores r3), so only the event log is compared
        return {"log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, w=None, h=None, cn=None, p1n=None, p2n=None, fopen_ok=None, written=None):
    if w is None:
        w = r.choice([0, 1, 2, 3, 5, 8, 13])
    if h is None:
        h = r.choice([0, 1, 2, 4, 7])
    n = w * h
    ok = r.random() < 0.8 if fopen_ok is None else fopen_ok
    c = {"w": w, "h": h, "words": [r.getrandbits(32) for _ in range(12)],
         "cells": bytes(r.getrandbits(8) for _ in range(25 * n)),
         "p1": bytes(r.getrandbits(8) for _ in range(n)),
         "p2": bytes(r.getrandbits(8) for _ in range(n)),
         "cells_null": r.random() < 0.15 if cn is None else cn,
         "p1_null": r.random() < 0.15 if p1n is None else p1n,
         "p2_null": r.random() < 0.15 if p2n is None else p2n,
         "path": r.choice(PATHS)}
    wr = (r.choice([0, 1]) if written is None else written)
    c["queues"] = {"fopen": [0x1234 if ok else 0], "fwrite": [wr]}
    return c


def edges():
    r = random.Random(644)
    for fo in (True, False):
        for wr in (0, 1):
            yield mk(r, 3, 4, False, False, False, fo, wr)
            yield mk(r, 0, 5, False, False, False, fo, wr)
            yield mk(r, 5, 0, False, False, False, fo, wr)
            yield mk(r, 2, 2, True, False, False, fo, wr)
            yield mk(r, 2, 2, False, True, False, fo, wr)
            yield mk(r, 2, 2, False, False, True, fo, wr)
            yield mk(r, 1, 1, True, True, True, fo, wr)
    yield mk(r, 0xFFFF, 1, False, False, False, True, 1) if False else mk(r, 200, 3, False, False, False, True, 1)


def rnd(r):
    return mk(r)


TESTS = [
    FnTest("SaveRideHeightMap", ppc, host, edge=edges, rnd=rnd),
]
