"""Unit v5730, SMachineParameters::Return 0x10013c2c, against libtpw_unit_SMachineParameters_Return.dylib
(tests/unit_SMachineParameters_Return.cpp), engine program, src/tb_init_SMachineParameters_Output_SMachineParameters_Return.cpp.

A case is an SMachineParameters object (the four ints and two floats at 0x10..0x24), a 0x20-byte record (flags at 4, limits at
8..0x1c) and a count. The world is written by one piece of code on both sides. Return calls nothing. Compared: the answer and the
whole object and record read back. No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_SMachineParameters_Return"
MASK = 0xFFFFFFFF
FN = 0x10013C2C
HOST_LAYOUT = [0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e):
        self.e = e

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, d):
        self.e.write(a, d)

    def call(self, obj, rec, n):
        r3, _, _ = T.call_ppc(self.e, FN, [obj, rec, n & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib):
        self.lib = lib
        if not getattr(lib, "bound", False):
            lib.tpw_smpr_alloc.restype, lib.tpw_smpr_alloc.argtypes = C.c_uint64, [C.c_uint32]
            lib.tpw_smpr_layout.argtypes = [C.POINTER(C.c_int64)]
            lib.tpw_smpr_return.restype = C.c_uint32
            lib.tpw_smpr_return.argtypes = [C.c_uint64, C.c_uint64, C.c_int32]
            out = (C.c_int64 * 32)()
            n = lib.tpw_smpr_layout(out)
            assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
            lib.bound = True

    def alloc(self, n):
        return self.lib.tpw_smpr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def call(self, obj, rec, n):
        return self.lib.tpw_smpr_return(obj, rec, T.s32(n))


def run(s, case):
    obj = s.alloc(0x28)
    rec = s.alloc(0x20)
    s.words(obj, case["obj"])
    s.words(rec, case["rec"])
    ans = s.call(obj, rec, case["n"])
    return {"answer": ans, "obj": s.rd_words(obj, 10), "rec": s.rd_words(rec, 8)}


def ppc(e, case):
    return run(Ppc(e), case)


def host(lib, case):
    return run(Host(lib), case)


FVALS = [0, 0x80000000, 0x3F800000, 0xBF800000, 0x40000000, 0x7FC00000, 0x7F800000, 0xFF800000, fb(100.0), fb(99.5), fb(-3.0)]
IVALS = [0, 1, -1, 100, 99, 101, 0x7FFFFFFF, -0x80000000, 16777217, 16777216, -16777217]


def fv(r):
    return r.choice(FVALS) if r.random() < 0.6 else fb(r.uniform(-200, 200))


def iv(r):
    return r.choice(IVALS) & MASK if r.random() < 0.6 else r.randrange(-200, 200) & MASK


def make(r, flags=None, n=None, same=0.4):
    obj = [r.randrange(0, 1 << 32), 0, 0, 0, iv(r), fv(r), iv(r), iv(r), iv(r) if r.random() < 0.5 else fb(r.choice([100.0, 99.0, 16777216.0, 16777218.0])), fv(r)]
    rec = [r.randrange(0, 1 << 32), (r.randrange(0, 64) if flags is None else flags) | (r.randrange(0, 1 << 26) << 6 if r.random() < 0.3 else 0),
           obj[4], obj[5], obj[6], obj[7], obj[8] if False else iv(r), obj[9]]
    # fields equal to the object's are the boundaries (equal does not stop); nudge some by one
    for i in (2, 4, 5):
        if r.random() > same:
            rec[i] = iv(r)
    if r.random() > same:
        rec[3] = fv(r)
    if r.random() > same:
        rec[7] = fv(r)
    if r.random() < same:
        rec[6] = r.choice([100, 99, 101, -100, 16777216, 16777217]) & MASK
    else:
        rec[6] = iv(r)
    if r.random() < 0.3:
        obj[8] = fb(float(T.s32(rec[6])) if abs(T.s32(rec[6])) < (1 << 24) else 100.0)
    if r.random() < 0.3:
        rec[r.choice([2, 4, 5])] = (obj[{2: 4, 4: 6, 5: 7}[0] if False else 4] if False else rec[2])
    return {"obj": obj, "rec": rec, "n": r.choice([0, 1, 1, 2, 3, 5, -1]) if n is None else n}


def edges():
    r = random.Random(730)
    for n in (0, -1, 1, 2):
        yield make(r, flags=0, n=n)
    for bit in range(8):
        for _ in range(12):
            yield make(r, flags=(1 << bit) if bit < 6 else 0x3F, n=r.choice([1, 2, 3]))
    for _ in range(30):
        yield make(r, flags=0x3F, n=r.choice([1, 2, 4]), same=0.9)
    # exact equal: every field equal to the object's
    obj = [5, 0, 0, 0, 100, fb(100.0), 100, 100, fb(100.0), fb(100.0)]
    rec = [7, 0x3F, 100, fb(100.0), 100, 100, 100, fb(100.0)]
    yield {"obj": obj, "rec": rec, "n": 3}
    for i, v in ((2, 101), (3, fb(100.5)), (4, 101), (5, 101), (6, 101), (7, fb(100.5))):
        r2 = list(rec)
        r2[i] = v
        yield {"obj": obj, "rec": r2, "n": 3}
    for i, v in ((2, 99), (3, fb(99.5)), (4, 99), (5, 99), (6, 99), (7, fb(99.5))):
        r2 = list(rec)
        r2[i] = v
        yield {"obj": obj, "rec": r2, "n": 3}


def rnd(r):
    return make(r)


TESTS = [FnTest("SMachineParameters::Return", ppc, host, edge=edges, rnd=rnd)]
