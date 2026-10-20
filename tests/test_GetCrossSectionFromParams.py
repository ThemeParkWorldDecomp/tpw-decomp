"""Unit v5208, GetCrossSectionFromParams 0x1004fd94 against libtpw_unit_GetCrossSectionFromParams.dylib, main app, src/TPConfig_extrude_GetCrossSectionFromParams.cpp.

A case is a type block (table pointer at 0x78, count at 0x7c) with a table of 0x2c-byte entries (eight range floats and an answer word
at 0x20), an orientation block (floats at 4, 8, 0xc) and an angle. The world is built by one piece of code on both memories. Compared:
the answer. No callees, no fused operations. Not tested: a count of 0 (the original then walks past the table).
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_GetCrossSectionFromParams"
MASK = 0xFFFFFFFF
FN = 0x1004FD94


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e):
        self.e = e

    def set_table(self, t, tab, n):
        self.words(t + 0x78, [tab, n])

    def alloc(self, n):
        return self.e.alloc(n)

    def write(self, a, data):
        self.e.write(a, data)

class Host(Side):
    fmt = "<I"

    def __init__(self, lib):
        self.lib = lib
        bind(lib)

    def set_table(self, t, tab, n):
        self.write(t + 0x78, struct.pack("<QI", tab, n)[:8] + b"\0" * 0 )
        self.write(t + 0x80, struct.pack("<I", n))

    def alloc(self, n):
        return self.lib.tpw_gcs_alloc(n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def call(self, t, o, f):
        return self.lib.tpw_gcs_a(t, o, bitsf(f)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gcs_alloc.restype, lib.tpw_gcs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gcs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_gcs_a.restype, lib.tpw_gcs_a.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_float]
    out = (C.c_int64 * 8)()
    n = lib.tpw_gcs_layout(out)
    assert list(out)[:n] == [4, 12, 0x78, 0x80, 0x2c], list(out)[:n]
    lib.bound = True


def build(s, case):
    ents = case["entries"]
    tab = s.alloc(0x2c * len(ents))
    for i, en in enumerate(ents):
        s.words(tab + 0x2c * i, en)
    t = s.alloc(0x100)
    s.set_table(t, tab, len(ents))
    o = s.alloc(0x1c)
    s.words(o + 4, case["orient"])
    return t, o


def ppc(e, case):
    s = Ppc(e)
    t, o = build(s, case)
    # PowerOpen: r3 type, f1 unused (its r4 shadow), r5 orientation, f2 angle
    r3, _, _ = T.call_ppc(e, FN, [t, T.F(0.0), o, T.F(bitsf(case["angle"]))])
    return {"answer": r3 & MASK}


def host(lib, case):
    s = Host(lib)
    t, o = build(s, case)
    return {"answer": s.call(t, o, case["angle"])}


def rf(r):
    k = r.random()
    if k < 0.05:
        return r.choice([0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000])
    return fb(float(r.randint(-4, 4)))


def entry(r, ans=None):
    e = []
    for _ in range(4):
        lo = r.randint(-4, 2)
        hi = lo + r.randint(0, 4)
        if r.random() < 0.05:
            lo, hi = hi, lo
        e += [fb(float(lo)), fb(float(hi))]
    e.append(r.choice([0, 1, 7, 0xFFFFFFFF, r.randrange(1, 1000)]) if ans is None else ans)
    e += [0, 0]
    return e


def case_(r, n=None):
    n = r.randint(1, 5) if n is None else n
    ents = [entry(r) for _ in range(n)]
    c = {"entries": ents, "orient": [rf(r), rf(r), rf(r)], "angle": rf(r)}
    if r.random() < 0.6 and n > 1:
        # aim inside a chosen entry, sometimes on the boundary
        en = r.choice(ents[:-1])
        pick = lambda lo, hi: r.choice([lo, hi, fb((bitsf(lo) + bitsf(hi)) / 2)])
        c["angle"] = pick(en[0], en[1])
        c["orient"] = [pick(en[2], en[3]), pick(en[4], en[5]), pick(en[6], en[7])]
        if r.random() < 0.3:
            k = r.randrange(4)
            if k == 0:
                c["angle"] = fb(bitsf(en[1]) + 1.0)
            else:
                c["orient"][k - 1] = fb(bitsf(en[2 * k + 1]) + 1.0)
    return c


def edges():
    r = random.Random(208)
    for n in (1, 2, 3):
        for _ in range(12):
            yield case_(r, n)
    for _ in range(40):
        yield case_(r)


def rnd(r):
    return case_(r)


TESTS = [FnTest("GetCrossSectionFromParams", ppc, host, edge=edges, rnd=rnd)]
