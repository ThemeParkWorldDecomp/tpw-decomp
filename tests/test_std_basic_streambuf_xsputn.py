"""Unit v5185, basic_streambuf<wchar_t>::xsputn and xsgetn, against libtpw_unit_std_basic_streambuf_xsputn.dylib
(tests/unit_std_basic_streambuf_xsputn.cpp), main app ("SimTheme Park"), src/game/MslSupport.cpp:
  xsputn 0x1021feac, xsgetn 0x10220024

A case is a stream buffer object (vtable with uflow at 0x2c and overflow at 0x38) whose put or get area lies in a 32 character region R,
plus a second buffer X (the source of xsputn, the destination of xsgetn). The world is built by one piece of Python code in emulator memory
(big-endian, 4-byte pointers) and in host memory (little-endian, 8-byte pointers at twice the Mac offsets). Callees: wmemcpy (copies, logs
the count) and the two virtual calls through __ptr_glue (log, answer from the queues "uflow" and "overflow"); both sides log the same.

Compared: the answer, the log, the pointer fields as offsets from R, and the bytes of R and X. No fused operations. Not tested: nothing
excluded (negative counts and reversed areas included); the object's other fields are untouched by the code.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_streambuf_xsputn"
MASK = 0xFFFFFFFF

FN_PUT, FN_GET = 0x1021FEAC, 0x10220024
OWN = {FN_PUT: 292, FN_GET: 288}
PTR_GLUE = 0x1023BAA8
FIELDS = {"gnext": 0x8, "gend": 0xC, "pbeg": 0x10, "pnext": 0x14, "pend": 0x18}
HOST_LAYOUT = [16, 24, 32, 40, 48]
RCH, XCH = 32, 24

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_wmemcpy(e):
    n = e.r(5)
    e.cl.rec("wmemcpy", (n,))
    e.write(e.r(3), e.read(e.r(4), 2 * n))


def stub_ptr_glue(e):
    L = e.cl
    tv = e.r(12)
    if tv == L.tv["uflow"]:
        L.rec("uflow", (1 if e.r(3) == L.obj else 0,))
        e.set_r(3, L.ret("uflow") & MASK)
    else:
        assert tv == L.tv["overflow"], hex(tv)
        L.rec("overflow", (1 if e.r(3) == L.obj else 0, e.r(4)))
        e.set_r(3, L.ret("overflow") & MASK)


STUBS.imp("wmemcpy", stub_wmemcpy)
STUBS.internal(PTR_GLUE, stub_ptr_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def chars(self, a, cs):
        self.write(a, struct.pack(self.fmt + "%dH" % len(cs), *cs))

    def rd_chars(self, a, n):
        return list(struct.unpack(self.fmt + "%dH" % n, self.read(a, 2 * n)))


class Ppc(Side):
    fmt = ">"

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

    def make_obj(self):
        e = self.e
        tvu, tvo, vt, obj = e.alloc(8), e.alloc(8), e.alloc(0x40), e.alloc(0x20)
        e.write(vt + 0x2C, struct.pack(">I", tvu))
        e.write(vt + 0x38, struct.pack(">I", tvo))
        e.write(obj, struct.pack(">I", vt))
        self.L.tv = {"uflow": tvu, "overflow": tvo}
        self.L.obj = obj
        return obj

    def setp(self, obj, name, v):
        self.write(obj + FIELDS[name], struct.pack(">I", v & MASK))

    def getp(self, obj, name):
        return struct.unpack(">I", self.read(obj + FIELDS[name], 4))[0]

    def call(self, fn, obj, buf, n):
        r3, _, _ = T.call_ppc(self.e, fn, [obj, buf, n & MASK])
        return T.s32(r3)


class Host(Side):
    fmt = "<"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_sb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def make_obj(self):
        return self.lib.tpw_sb_newobj()

    def setp(self, obj, name, v):
        self.write(obj + 2 * FIELDS[name], struct.pack("<Q", v))

    def getp(self, obj, name):
        return struct.unpack("<Q", self.read(obj + 2 * FIELDS[name], 8))[0]

    def call(self, fn, obj, buf, n):
        f = self.lib.tpw_sb_xsputn if fn == FN_PUT else self.lib.tpw_sb_xsgetn
        return f(obj, buf, n)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sb_alloc.restype, lib.tpw_sb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sb_newobj.restype, lib.tpw_sb_newobj.argtypes = C.c_uint64, []
    lib.tpw_sb_layout.argtypes = [C.POINTER(C.c_int64)]
    for nm in ("tpw_sb_xsputn", "tpw_sb_xsgetn"):
        f = getattr(lib, nm)
        f.restype, f.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sb_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def make(fn):
    def run(s, case):
        try:
            R, X = s.alloc(2 * RCH), s.alloc(2 * XCH)
            obj = s.make_obj()
            s.chars(R, case["rchars"])
            s.chars(X, case["xchars"])
            a, b = case["a"], case["b"]
            if fn == FN_PUT:
                for nm, v in (("pbeg", 0), ("pnext", a), ("pend", b)):
                    s.setp(obj, nm, R + v)
                buf = X
            else:
                for nm, v in (("gnext", a), ("gend", b)):
                    s.setp(obj, nm, R + v)
                buf = X
            if fn == FN_GET:
                pass
            ans = s.call(fn, obj, R if fn == FN_PUT and False else buf, case["n"])
            res = {"answer": ans, "log": s.log(), "R": s.rd_chars(R, RCH), "X": s.rd_chars(X, XCH)}
            for nm in FIELDS:
                v = s.getp(obj, nm)
                res[nm] = 0 if v == 0 else (v - R) & MASK
            return res
        finally:
            s.done()
    return run


def mk(fn):
    r = make(fn)
    return (lambda e, case: r(Ppc(e, case), case)), (lambda lib, case: r(Host(lib, case), case))


def case(r, a=None, b=None, n=None, qo=None, qu=None, rch=None, xch=None):
    def ch():
        return r.choice([0xFFFF, 0, 0x1234, r.randrange(0x10000), r.randrange(0x10000)])
    if a is None:
        a = r.randrange(0, 20) * 2 + (1 if r.random() < 0.1 else 0)
    if b is None:
        k = r.random()
        b = a + r.choice([0, 2, 4, 6, 8, 12, 16]) if k < 0.7 else r.randrange(0, 41)
        b = min(b, 40)
        if a & 1:      # an odd pointer is only compared, never dereferenced (a 16-bit access there reads different bytes per endianness)
            b = min(b, a)
    if n is None:
        n = r.choice([0, 1, 2, 3, 5, 8, 12, -1, -3, r.randrange(-2, 13)])
    qlist = lambda: [r.choice([0xFFFF, 0x7, 0x1FFFF, 0x12345, 0, 0xABCDFFFF, r.randrange(1 << 32)]) for _ in range(r.randrange(1, 5))]
    if rch is None:
        rch = [ch() for _ in range(RCH)]
    if xch is None:
        xch = [ch() for _ in range(XCH)]
    return {"a": a, "b": b, "n": n, "rchars": rch, "xchars": xch,
            "queues": {"overflow": qo or qlist(), "uflow": qu or qlist()}}


def edges(seed):
    r = random.Random(seed)
    for a, b in ((0, 0), (0, 8), (8, 0), (10, 7), (10, 9), (11, 8), (4, 5), (6, 7), (0, 1), (0, 40), (40, 40), (3, 3), (5, 4)):
        for n in (0, 1, 3, 4, 5, 6, -1, 12):
            yield case(r, a, b, n)
    for q in ([0xFFFF], [1, 0xFFFF], [1, 2, 0xFFFF], [1], [0x1FFFF], [0xABCD0000 | 0xFFFF], [0x10000]):
        yield case(r, 4, 4, 5, qo=q, qu=q)
        yield case(r, 4, 8, 6, qo=q, qu=q)
    full = [0xFFFF] * RCH
    yield case(r, 0, 8, 4, rch=full, xch=full)
    yield case(r, 8, 8, 3, rch=full, xch=full, qo=[1], qu=[1])


def rnd_for():
    return lambda r: case(r)


TESTS = []
for nm, fn, seed in (("std::basic_streambuf<wchar_t,std::char_traits<wchar_t>>::xsputn", FN_PUT, 11),
                     ("std::basic_streambuf<wchar_t,std::char_traits<wchar_t>>::xsgetn", FN_GET, 12)):
    p, h = mk(fn)
    TESTS.append(FnTest(nm, p, h, edge=(lambda s=seed: edges(s)), rnd=rnd_for()))
