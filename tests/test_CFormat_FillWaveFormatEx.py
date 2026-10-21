"""Unit v5274, CFormat::FillWaveFormatEx 0x100021b0, against libtpw_unit_CFormat_FillWaveFormatEx.dylib
(tests/unit_CFormat_FillWaveFormatEx.cpp), sound library, src/sound/FileAccess.cpp.

A case is a CFormat object (0x18 bytes of fields plus the stream info pointer) and optionally a stream info block, built twice by one
piece of code: big-endian in emulator memory, little-endian on the host. No callees, no fused operations. Compared: the 0x18 bytes of
the object after the call (the pointer is compared as present or absent), and the info block. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_CFormat_FillWaveFormatEx"

FN = 0x100021B0
HOST_LAYOUT = [4, 6, 8, 12, 16, 18, 20, 24, 4, 6, 7]


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    en = ">"
    ptr = ">I"

    def __init__(self, e):
        self.e = e

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, d):
        self.e.write(a, d)

    def call(self, obj):
        T.call_ppc(self.e, FN, [obj])


class Host:
    en = "<"
    ptr = "<Q"

    def __init__(self, lib):
        self.lib = lib
        bind(lib)

    def alloc(self, n):
        return self.lib.tpw_cfmt_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def call(self, obj):
        self.lib.tpw_cfmt_fill(obj)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cfmt_alloc.restype, lib.tpw_cfmt_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cfmt_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cfmt_fill.restype, lib.tpw_cfmt_fill.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cfmt_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    en = s.en
    obj = s.alloc(0x20)
    info = s.alloc(8)
    head = struct.pack(en + "BBHHHIIHHH", case["a"], case["b"], case["c"], case["filled"], case["chan"], case["rate"],
                       case["avg"], case["align"], case["bits"], case["extra"])
    s.write(obj, head + b"\0\0")
    s.write(obj + 0x18, struct.pack(s.ptr, info if case["has_info"] else 0))
    s.write(info, struct.pack(en + "IHBB", case["pad"], case["irate"], case["ibits"], case["iflags"]))
    s.call(obj)
    return {"obj": struct.unpack(en + "BBHHHIIHHH", s.read(obj, 0x16)), "info": struct.unpack(en + "IHBB", s.read(info, 8)),
            "ptr_set": struct.unpack(s.ptr, s.read(obj + 0x18, struct.calcsize(s.ptr)))[0] == info}


def ppc(e, case):
    return run(Ppc(e), case)


def host(lib, case):
    return run(Host(lib), case)


def mk(r, **kw):
    c = {"a": r.choice([0, 1, 2, 8, 16, 0x80, 0xFF, r.randrange(256)]), "b": r.choice([0, 8, 16, 0xFF, r.randrange(256)]),
         "c": r.choice([0, 1, 0x5622, 0xAC44, 0xFFFF, r.randrange(65536)]),
         "filled": r.choice([0, 0, 1, 0xFFFF, r.randrange(65536)]),
         "chan": r.randrange(65536), "rate": r.getrandbits(32), "avg": r.getrandbits(32), "align": r.randrange(65536),
         "bits": r.randrange(65536), "extra": r.randrange(65536), "has_info": r.random() < 0.5, "pad": r.getrandbits(32),
         "irate": r.choice([0, 22050, 44100, 0xFFFF, r.randrange(65536)]), "ibits": r.choice([0, 8, 16, 0xFF, r.randrange(256)]),
         "iflags": r.choice([0, 1, 2, 3, 0x80, 0xFF, r.randrange(256)])}
    c.update(kw)
    return c


def edges():
    r = random.Random(5274)
    for filled in (0, 1, 0xFFFF, 0x100):
        for has in (False, True):
            for fl in (0, 1, 2, 0xFF):
                yield mk(r, filled=filled, has_info=has, iflags=fl)
    for a, b, c in ((0xFF, 0xFF, 0xFFFF), (2, 16, 44100), (0, 0, 0), (0xFF, 0x80, 0xFFFF)):
        yield mk(r, filled=0, has_info=False, a=a, b=b, c=c)
    yield mk(r, filled=0, has_info=True, iflags=1, ibits=0xFF, irate=0xFFFF)
    yield mk(r, filled=0, has_info=False, a=0xFF, b=0xFF, c=0xFFFF, rate=0xFFFFFFFF)


def rnd(r):
    return mk(r)


TESTS = [
    FnTest("CFormat::FillWaveFormatEx", ppc, host, edge=edges, rnd=rnd),
]
