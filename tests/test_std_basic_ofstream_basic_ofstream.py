"""Unit v5755, std::basic_ofstream<wchar_t>::basic_ofstream() against libtpw_unit_std_basic_ofstream_basic_ofstream.dylib, main app,
src/std_basic_ofstream_basic_ofstream.cpp:
  std::basic_ofstream<wchar_t,std::char_traits<wchar_t>>::basic_ofstream 0x10221f20

A case is a flag (r4, a short in effect) and a zeroed object plus a separate "other" ios block (the virtual base the caller would have set when
the flag is 0). The two callees (basic_ostream<wchar_t> ctor 0x10226c94, basic_filebuf<wchar_t> ctor 0x10226ef4) are fakes recording their
arguments. Compared: the log, whether the virtual-base pointer is the object's ios part or the other block, the slot word, the ios vtable word
and the stored base offset (each relative to the globals / the object so the Mac and host layouts compare), and the untouched ios part.

No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_ofstream_basic_ofstream"
MASK = 0xFFFFFFFF

FN = 0x10221F20
SIZES = {FN: 180}
OWN = set(SIZES)
OSTREAM = 0x10226C94
FILEBUF = 0x10226EF4

ADDR = dict(gOfsWVtbl=0x102B1F50, gIfsWVtblE=0x102B1F20, gIosBaseVt=0x103BD034)
HOST_LAYOUT = [8, 0x48]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ostream(e):
    L = e.cl
    L.rec("WostreamCtor", (L.lab(e.r(3)), T.s32(e.r(4)), L.lab(e.r(5))))


def stub_filebuf(e):
    L = e.cl
    L.rec("FilebufCtor", (L.lab(e.r(3)), L.lab(e.r(4))))


STUBS.internal(OSTREAM, stub_ostream)
STUBS.internal(FILEBUF, stub_filebuf)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psize = 4
    slot_off, ios_off, base_off = 4, 0x58, 0x44
    fb_off = 8

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(0x200)
        self.other = e.alloc(0x100)
        self.L = e.cl = CL.PpcLog({self.obj: 1, self.obj + 8: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, flag):
        T.call_ppc(self.e, FN, [self.obj, flag & MASK])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psize = 8
    slot_off, ios_off, base_off = 8, 0x38, 0x48
    fb_off = 0x10

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_ofs_alloc(0x200)
        self.other = lib.tpw_ofs_alloc(0x100)
        CL.host_begin(lib, {1: self.obj, 2: self.obj + 0x10}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ofs_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, flag):
        self.lib.tpw_ofs_ctor(self.obj, T.s32(flag))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ofs_addr.restype, lib.tpw_ofs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ofs_alloc.restype, lib.tpw_ofs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ofs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ofs_ctor.restype, lib.tpw_ofs_ctor.argtypes = None, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ofs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def rd_ptr(s, a):
    return struct.unpack(s.pfmt, s.read(a, s.psize))[0]


def build(s, case):
    if not (case["flag"] & 0xFFFF):
        # the caller's virtual base: the stream's first word points to it
        if s.psize == 4:
            s.words(s.obj, [s.other])
        else:
            s.write(s.obj, struct.pack("<Q", s.other))


def observe(s, case):
    s.call(case["flag"])
    mios = rd_ptr(s, s.obj)
    ios = s.obj + s.ios_off
    vt = s.g("gOfsWVtbl")
    return {
        "log": s.log(),
        "mios_is_ios": mios == ios,
        "mios_is_other": mios == s.other,
        "slot": rd_ptr(s, s.obj + s.slot_off) - vt,
        "ios_vt": rd_ptr(s, mios) - vt,
        "base": T.s32(s.rd_word(mios + s.base_off)) - (ios - mios),
        "ios_part": rd_ptr(s, ios) if not (case["flag"] & 0xFFFF) else 0,
    }


def run(s, case):
    try:
        build(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(flag):
    return {"flag": flag & MASK, "queues": {}}


def edges():
    for f in (0, 1, MASK, 0x10000, 0x10001, 0x8000, 0xFFFF, 0x7FFF0000, 0x12340000, 0x12340001):
        yield mk(f)


def rnd(r):
    return mk(r.choice([0, 1, r.getrandbits(32), r.getrandbits(16) << 16, r.getrandbits(16)]))


TESTS = [
    FnTest("std::basic_ofstream<wchar_t,std::char_traits<wchar_t>>::basic_ofstream", ppc, host, edge=edges, rnd=rnd),
]
