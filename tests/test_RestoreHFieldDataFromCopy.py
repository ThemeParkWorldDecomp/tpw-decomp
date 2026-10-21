"""Unit v5287, RestoreHFieldDataFromCopy 0x1005c050, against libtpw_unit_RestoreHFieldDataFromCopy.dylib
(tests/unit_RestoreHFieldDataFromCopy.cpp), main app, src/game/park/TPHField.cpp.

A case is a header (or NULL) with a height field (or NULL) holding a quad table (or NULL) and a quad count, and a copy field with its own
quad table (or NULL). The world is built twice by one piece of Python (emulator memory big-endian, host memory little-endian, host pointers
as arena offsets). memcpy runs for real on both sides. Compared: both quad tables, the field words, and the guard bytes after the
destination table. Not tested: a NULL copy (the original reads it unconditionally when the header chain is valid).
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RestoreHFieldDataFromCopy"
MASK = 0xFFFFFFFF
FN = 0x1005C050
P = "tpw_rhf_"


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd(self, a, n):
        return self.read(a, n)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e):
        self.e = e

    def alloc(self, n):
        return self.e.alloc(n)

    def ptr(self, a):
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, d):
        self.e.write(a, d)

    def call(self, h, c):
        T.call_ppc(self.e, FN, [h & MASK, c & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib):
        self.lib = lib
        bind(lib)

    def alloc(self, n):
        return getattr(self.lib, P + "alloc")(n)

    def ptr(self, a):
        return getattr(self.lib, P + "off")(a)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def call(self, h, c):
        getattr(self.lib, P + "call")(h, c)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, P + "alloc").restype, getattr(lib, P + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, P + "call").restype, getattr(lib, P + "call").argtypes = None, [C.c_uint64, C.c_uint64]
    getattr(lib, P + "off").restype, getattr(lib, P + "off").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, P + "layout").argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = getattr(lib, P + "layout")(out)
    assert list(out)[:n] == [0x6C, 6, 0x2C], list(out)[:n]
    lib.bound = True


def run(s, case):
    qa, qb = case["dst"], case["src"]      # quad tables: None or bytes
    n = case["count"]
    size = 0x30
    # destination table is allocated with guard bytes after it
    dst = src = 0
    if qa is not None:
        dst = s.alloc(len(qa) + 16)
        s.write(dst, qa + b"\xAA" * 16)
    if qb is not None:
        src = s.alloc(len(qb) + 16)
        s.write(src, qb + b"\xBB" * 16)
    field = s.alloc(size) if case["field"] else 0
    if field:
        s.write(field + 4, struct.pack(s.fmt[0] + "HH", case["hc"], n & 0xFFFF))   # mHeightCount, mQuadCount
        s.words(field + 0x2C, [s.ptr(dst) if dst else 0])
    copy = s.alloc(size)
    s.words(copy + 0x2C, [s.ptr(src) if src else 0])
    hdr = s.alloc(0xB8) if case["hdr"] else 0
    if hdr:
        s.words(hdr + 0x6C, [s.ptr(field) if field else 0])
    s.call(hdr, copy)
    return {
        "dst": s.read(dst, len(qa) + 16) if dst else None,
        "src": s.read(src, len(qb) + 16) if src else None,
        "field": struct.unpack(s.fmt[0] + "IHH", s.read(field, 8)) if field else None,
    }


def ppc(e, case):
    return run(Ppc(e), case)


def host(lib, case):
    return run(Host(lib), case)


def mk(r, hdr=True, field=True, dst=True, src=True, count=None):
    n = r.choice([0, 1, 2, 5, 17, 64, 100]) if count is None else count
    nbytes = max(n * 4, 4) + r.choice([0, 0, 8])
    return {
        "hdr": hdr, "field": field, "hc": r.randrange(0, 0x100),
        "count": n,
        "dst": bytes(r.randrange(256) for _ in range(nbytes)) if dst else None,
        "src": bytes(r.randrange(256) for _ in range(nbytes)) if src else None,
    }


def edges():
    r = random.Random(5287)
    yield mk(r, hdr=False)
    yield mk(r, field=False)
    yield mk(r, dst=False)
    yield mk(r, src=False)
    yield mk(r, count=0)
    yield mk(r, count=1)
    yield mk(r, count=3)
    yield mk(r, count=0xFFFF - 0xFF00)
    yield mk(r, count=0x8000)


def rnd(r):
    k = r.random()
    if k < 0.1:
        return mk(r, hdr=False)
    if k < 0.2:
        return mk(r, field=False)
    if k < 0.3:
        return mk(r, dst=False)
    if k < 0.4:
        return mk(r, src=False)
    return mk(r)


TESTS = [
    FnTest("RestoreHFieldDataFromCopy", ppc, host, edge=edges, rnd=rnd),
]
