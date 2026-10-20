"""Unit v5568, cmp_nocase(const wstring&, const wstring&, bool), against libtpw_unit_cmp_nocase.dylib (tests/unit_cmp_nocase.cpp), main app:
  cmp_nocase 0x10210e68

A case is two wide strings and a flag. Each string is a word pointing to a representation (length, capacity, refs, data pointer) with the
characters in a separate block; the upper-case map (__wupper_map, 256 halfwords) is written before the call. Built twice by ONE piece of code
(emulator memory big-endian, host memory little-endian with 8-byte pointers). Compared: the answer. No callees, no fused operations.
Not tested: NULL strings (the original reads them unconditionally).
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest
import calllog as CL

SHIM_LIBRARY = "unit_cmp_nocase"
MASK = 0xFFFFFFFF
FN = 0x10210E68
OWN = {FN}
MAP_ADDR = 0x103BD0A0
HOST_LAYOUT = [16, 24]

STUBS = CL.Stubs()
STUBS.real.update(OWN)

ALPHABET = [0x61, 0x41, 0x62, 0x42, 0x7A, 0x5A, 0x5F, 0xE9, 0xC9, 0x00, 0xFF, 0x100, 0x101, 0x1000, 0xFFFF, 0x20]


def setup(runner):
    T.install_std_stubs(runner.emu)


def table():
    t = list(range(256))
    for c in range(0x61, 0x7B):
        t[c] = c - 0x20
    t[0xE9] = 0xC9
    t[0x5F] = 0x200       # maps above 0xff
    t[0xFF] = 0x0178
    t[0x00] = 0x0001
    return t


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def make(self, chars):
        """A string object holding chars; returns its address."""
        data = self.alloc(2 * len(chars) + 4)
        self.write(data, b"".join(struct.pack(self.fmt[0] + "H", c) for c in chars))
        rep = self.alloc(32)
        self.words(rep, [len(chars), len(chars) + 1, 0])
        self.ptr(rep + self.dataofs, data)
        obj = self.alloc(8)
        self.ptr(obj, rep)
        return obj


class Ppc(Side):
    fmt = ">I"
    dataofs = 0xC

    def __init__(self, e, case):
        self.e = e
        self.saved = STUBS.activate(e)
        e.write(0x10261DEC, struct.pack(">I", MAP_ADDR))     # the TOC slot of the data import (unrelocated in the image)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return MAP_ADDR

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a, v):
        self.words(a, [v])

    def call(self, a, b, flag):
        r3, _, _ = T.call_ppc(self.e, FN, [a, b, flag])
        return T.s32(r3)


class Host(Side):
    fmt = "<I"
    dataofs = 0x10

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cmpn_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cmpn_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def call(self, a, b, flag):
        return T.s32(self.lib.tpw_cmpn_cmp(a, b, flag))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cmpn_addr.restype, lib.tpw_cmpn_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cmpn_alloc.restype, lib.tpw_cmpn_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cmpn_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cmpn_cmp.restype, lib.tpw_cmpn_cmp.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cmpn_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.write(s.g("map"), b"".join(struct.pack(s.fmt[0] + "H", v) for v in table()))
        a = s.make(case["a"])
        b = s.make(case["b"])
        return {"answer": s.call(a, b, case["flag"])}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(a, b, flag):
    return {"a": list(a), "b": list(b), "flag": flag}


def edges():
    S = lambda t: [ord(c) for c in t]
    pairs = [
        ("", ""), ("a", ""), ("", "a"), ("abc", "ABC"), ("abc", "abd"), ("abd", "abc"), ("ab", "abc"), ("abc", "ab"),
        ("ABC", "abcd"), ("abcd", "ABC"), ("_", "a"), ("a", "_"), ("z", "Z"),
    ]
    for x, y in pairs:
        for f in (0, 1):
            yield mk(S(x), S(y), f)
    for x in (0x100, 0x101, 0x1000, 0xFFFF, 0xE9, 0xC9, 0xFF, 0xFF, 0, 0x5F, 0x178):
        for y in (0x100, 0x101, 0x1000, 0xE9, 0xC9, 0xFF, 0x178, 0x41, 0):
            yield mk([x], [y], 0)
    yield mk([0x61, 0x62], [0x41, 0x62, 0x63], 1)
    yield mk([0x61, 0x62, 0x63], [0x41, 0x62], 1)
    yield mk([0x61], [0x41], 1)


def rnd(r):
    la, lb = r.randrange(0, 6), r.randrange(0, 6)
    a = [r.choice(ALPHABET) for _ in range(la)]
    if r.random() < 0.5:
        b = [r.choice([c, c ^ 0x20, c]) if c < 0x80 else c for c in a][:lb]
        b += [r.choice(ALPHABET) for _ in range(max(0, lb - len(b)))]
    else:
        b = [r.choice(ALPHABET) for _ in range(lb)]
    return mk(a, b, r.randrange(2))


TESTS = [
    FnTest("cmp_nocase", ppc, host, edge=edges, rnd=rnd),
]
