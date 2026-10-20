"""Unit v5724, the std::basic_ifstream<wchar_t> constructor 0x10221e20, against libtpw_unit_std_basic_ifstream_basic_ifstream_2.dylib
(tests/unit_std_basic_ifstream_basic_ifstream_2.cpp), src/std_basic_ifstream_basic_ifstream_2.cpp:
  std::basic_ifstream<wchar_t,std::char_traits<wchar_t>>::basic_ifstream 0x10221e20

The world: an object block and an external virtual-base block (its first word junk), this[0] pointing at the external block. The callees
(basic_istream and basic_filebuf constructors) are fakes on both sides recording their arguments as tags (object, filebuf part, NULL).
Compared: the answer, the log, the virtual-base pointer, the words the constructor writes through it, the vtable word at +8.
No fused operations. Not tested: a NULL this[0] with a zero flag (the original writes through it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_ifstream_basic_ifstream_2"
MASK = 0xFFFFFFFF

FN = 0x10221E20
SIZES = {FN: 180}
OWN = set(SIZES)
ISTREAM = 0x102266E4
FILEBUF = 0x10226EF4

ADDR = dict(vtB=0x102B1E40)
HOST_LAYOUT = [0, 8, 0x10, 0x38, 0x80, 0x40]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def tag(e, a):
    o = e.obj
    return 0 if a == o else 1 if a == o + 0xC else 2 if a == 0 else 3


def stub_istream(e):
    e.cl.rec("IstreamCtor", (tag(e, e.r(3)), T.s32(e.r(4)), tag(e, e.r(5))))


def stub_filebuf(e):
    e.cl.rec("FilebufCtor", (tag(e, e.r(3)), tag(e, e.r(4))))


STUBS.internal(ISTREAM, stub_istream)
STUBS.internal(FILEBUF, stub_filebuf)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def pw(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rp(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]

    def rps(self, a):
        return struct.unpack(self.pfmt.lower(), self.read(a, self.psz))[0]


class Ppc(Side):
    pfmt = ">I"
    psz = 4
    ios, voff = 0x5C, 0x44

    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, d):
        self.e.write(a, d)

    def log(self):
        return self.e.cl.log

    def call(self, obj, flag):
        self.e.obj = obj
        r3, _, _ = T.call_ppc(self.e, FN, [obj, flag & MASK])
        return 1 if r3 & MASK == obj else 0


class Host(Side):
    pfmt = "<Q"
    psz = 8
    ios, voff = 0x38, 0x40

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ifs2_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ifs2_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, obj, flag):
        return self.lib.tpw_ifs2_ctor(obj, T.s32(flag))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ifs2_addr.restype, lib.tpw_ifs2_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ifs2_alloc.restype, lib.tpw_ifs2_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ifs2_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ifs2_ctor.restype, lib.tpw_ifs2_ctor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ifs2_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        obj = s.alloc(0x100)
        ext = s.alloc(0x60)
        s.pw(ext, case["junk"])
        s.pw(ext + s.voff, case["junk2"])
        s.pw(obj, ext)
        s.pw(obj + 8, case["junk3"])
        ans = s.call(obj, case["flag"])
        vb = s.rp(obj)
        vbt = "ios" if vb == obj + s.ios else "ext" if vb == ext else vb
        first = s.rp(vb)
        off = s.rps(vb + s.voff) - (obj + s.ios - vb)
        vt = s.rp(obj + 8)
        return {"answer": ans, "log": s.log(), "vb": vbt,
                "first": "B+c" if first == s.g("vtB") + 0xC else first,
                "off": off, "vt": "B" if vt == s.g("vtB") else vt,
                "extfirst": s.rp(ext) if vb != ext else "same"}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


FLAGS = [0, 1, -1, 0x10000, 0x8000, 0xFFFF, 0x7FFF, 0x10001, 2, -0x8000]


def mk(r, flag):
    return {"queues": {}, "flag": flag & MASK, "junk": r.choice([0, r.getrandbits(32)]),
            "junk2": r.getrandbits(32), "junk3": r.choice([0, r.getrandbits(32)])}


def edges():
    r = random.Random(5724)
    for f in FLAGS:
        yield mk(r, f)


def rnd(r):
    return mk(r, r.choice(FLAGS + [r.getrandbits(32)]))


TESTS = [
    FnTest("std::basic_ifstream<wchar_t,std::char_traits<wchar_t>>::basic_ifstream", ppc, host, edge=edges, rnd=rnd),
]
