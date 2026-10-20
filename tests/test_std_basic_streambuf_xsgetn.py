"""Unit v5147, std::basic_streambuf<char>::xsgetn and xsputn, against libtpw_unit_std_basic_streambuf_xsgetn.dylib
(tests/unit_std_basic_streambuf_xsgetn.cpp), main app, src/std_basic_streambuf_xsgetn.cpp:
  xsgetn 0x10070974, xsputn 0x10070808

A case builds a streambuf object (get and put area pointers into a 128-byte block), a source or destination block and a call. The world is
built by one piece of Python code in both memories (pointer fields are 4 bytes big-endian on the Mac, 8 bytes little-endian on the host).
The virtual uflow (slot 0x2c) and overflow (slot 0x38) are fakes on both sides recording their calls and answering from queues.

Compared: the answer, the event log, the four area pointers (relative to the block), the area block and the other block, all bytes.
No fused operations. Not tested: the per-character fast path inside the loops (gptr < egptr after the bulk copy): unreachable, because the
bulk copy leaves the pointers equal whenever characters remain.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_streambuf_xsgetn"
MASK = 0xFFFFFFFF

FN_GET, FN_PUT = 0x10070974, 0x10070808
SIZES = {FN_GET: 276, FN_PUT: 280}
OWN = set(SIZES)
PTR_GLUE = 0x1023BAA8
UF_CODE, OV_CODE = 0x10070001, 0x10070002
MAC_OFF = {"gptr": 0x8, "egptr": 0xC, "pptr": 0x14, "epptr": 0x18}
HOST_OFF = {"gptr": 16, "egptr": 24, "pptr": 40, "epptr": 48}
HOST_LAYOUT = [16, 24, 40, 48, 64]
FIELDS = ("gptr", "egptr", "pptr", "epptr")

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_glue(e):
    L = e.cl
    code = e.u32(e.r(12))
    if code == UF_CODE:
        L.rec("Uflow", (0,))
        e.set_r(3, L.ret("Uflow") & MASK)
    elif code == OV_CODE:
        L.rec("Overflow", (e.r(4) & MASK,))
        e.set_r(3, L.ret("Overflow") & MASK)
    else:
        raise AssertionError("unknown virtual %x" % code)


STUBS.internal(PTR_GLUE, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def setp(self, o, f, v):
        self.write(o + self.OFF[f], struct.pack(self.pfmt, v))

    def getp(self, o, f):
        return struct.unpack(self.pfmt, self.read(o + self.OFF[f], self.psize))[0]


class Ppc(Side):
    pfmt, psize, OFF = ">I", 4, MAC_OFF
    objsize = 0x40

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def vtable(self, obj):
        d = self.e.alloc(16)
        self.write(d, struct.pack(">IIII", UF_CODE, 0, OV_CODE, 0))
        vt = self.e.alloc(0x40)
        self.write(vt + 0x2C, struct.pack(">I", d))
        self.write(vt + 0x38, struct.pack(">I", d + 8))
        self.write(obj, struct.pack(">I", vt))

    def call(self, op, obj, buf, count):
        r3, _, _ = T.call_ppc(self.e, FN_GET if op == "get" else FN_PUT, [obj, buf, count & MASK])
        return T.s32(r3)


class Host(Side):
    pfmt, psize, OFF = "<Q", 8, HOST_OFF
    objsize = 64

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_sbio_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def vtable(self, obj):
        pass

    def call(self, op, obj, buf, count):
        f = self.lib.tpw_sbio_xsgetn if op == "get" else self.lib.tpw_sbio_xsputn
        return f(obj, buf, T.s32(count))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sbio_alloc.restype, lib.tpw_sbio_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sbio_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("tpw_sbio_xsgetn", "tpw_sbio_xsputn"):
        getattr(lib, n).restype, getattr(lib, n).argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sbio_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        obj = s.alloc(s.objsize)
        s.vtable(obj)
        area = s.alloc(128)
        other = s.alloc(128)
        s.write(area, bytes(case["area"]))
        s.write(other, bytes(case["other"]))
        gp = case["gp"]
        pp = case["pp"]
        s.setp(obj, "gptr", area + gp)
        s.setp(obj, "egptr", area + gp + case["ga"])
        s.setp(obj, "pptr", area + pp)
        s.setp(obj, "epptr", area + pp + case["pa"])
        ans = s.call(case["op"], obj, other + 16, case["count"])
        # xsgetn: area = get area, other = destination; xsputn: area = put area, other = source
        ptrs = [s.getp(obj, f) - area for f in FIELDS]
        return {"answer": ans, "log": s.log(), "ptrs": ptrs, "area": s.read(area, 128), "other": s.read(other, 128)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


UQ = [[-1], [0x41, 0x42, -1], [0x80, 0xFF, 0x1FF, 0x41], [0xFFFFFF80, 0x7F, 0x100], [0, 0x30, 0x31, 0x32, 0x33, -1]]
OQ = [[-1], [0x41, 0x42, -1], [0, 0, 0x100], [0x7F, 0x80, -2, -1], [0x30]]


def mk(r, op, count, avail, qi):
    c = {"op": op, "count": count, "area": [r.randrange(256) for _ in range(128)],
         "other": [r.randrange(256) for _ in range(128)], "gp": r.randrange(32, 49), "pp": r.randrange(32, 49),
         "ga": r.randrange(-5, 41), "pa": r.randrange(-5, 41),
         "queues": {"Uflow": r.choice(UQ), "Overflow": r.choice(OQ)}}
    if op == "get":
        c["ga"] = avail
        c["queues"]["Uflow"] = UQ[qi % len(UQ)]
    else:
        c["pa"] = avail
        c["queues"]["Overflow"] = OQ[qi % len(OQ)]
    return c


def edges(op):
    def gen():
        r = random.Random(5147 if op == "get" else 5148)
        for count in (0, -1, -3, 1, 2, 5, 40):
            for avail in (-5, -1, 0, 1, 2, 5, 40):
                for qi in range(5):
                    yield mk(r, op, count, avail, qi)
    return gen


def rnd(op):
    def gen(r):
        return mk(r, op, r.randrange(-3, 41), r.randrange(-5, 41), r.randrange(5))
    return gen


TESTS = [
    FnTest("std::basic_streambuf<char,std::char_traits<char>>::xsgetn", ppc, host, edge=edges("get"), rnd=rnd("get")),
    FnTest("std::basic_streambuf<char,std::char_traits<char>>::xsputn", ppc, host, edge=edges("put"), rnd=rnd("put")),
]
