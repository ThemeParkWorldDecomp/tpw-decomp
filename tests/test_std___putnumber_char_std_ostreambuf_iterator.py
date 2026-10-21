"""Unit v5068, std::__putnumber<char, ostreambuf_iterator<char>>, against libtpw_unit_std___putnumber_char_std_ostreambuf_iterator.dylib,
src/game/MslSupport.cpp:
  std::__putnumber 0x1020a140

A case is a stream buffer (buffer of 64 bytes, pptr and epptr at chosen offsets, possibly a NULL iterator), an ios (width, flags), a fill
character and prefix and suffix strings. The overflow callee (__ptr_glue 0x1023baa8) is a fake on both sides recording whether it got the
test's streambuf and the character, answering from a queue (-1 makes the iterator NULL). Compared: the returned iterator (NULL, the
streambuf), the buffer bytes, pptr, the ios width, and the call log. No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___putnumber_char_std_ostreambuf_iterator"
MASK = 0xFFFFFFFF

FN = 0x1020A140
SIZES = {FN: 724}
OWN = set(SIZES)
GLUE = 0x1023BAA8
HOST_LAYOUT = [0x14, 0x18, 0x2C, 0x30]
SB_ADDR = [0]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_glue(e):
    L = e.cl
    L.rec("overflow", (1 if e.r(3) == SB_ADDR[0] else 0, e.r(4) & MASK))
    e.set_r(3, L.ret("overflow") & MASK)


STUBS.internal(GLUE, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def p(self, a):
        return a

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, sb, ios, fill, pre, npre, suf, nsuf):
        SB_ADDR[0] = sb
        out = self.e.alloc(4)
        T.call_ppc(self.e, FN, [out, sb, ios, fill, pre, npre, suf, nsuf])
        w = self.rd_words(out, 1)[0]
        return 0 if w == 0 else (1 if w == sb else 2)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def p(self, a):
        return 0 if a == 0 else (a - self.lib.tpw_putnum_base()) & MASK

    def alloc(self, n):
        return self.lib.tpw_putnum_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, sb, ios, fill, pre, npre, suf, nsuf):
        return self.lib.tpw_putnum_run(sb, ios, fill, pre, npre, suf, nsuf)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_putnum_alloc.restype, lib.tpw_putnum_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_putnum_base.restype = C.c_uint64
    lib.tpw_putnum_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_putnum_run.restype = C.c_uint32
    lib.tpw_putnum_run.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_uint64, C.c_int32, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_putnum_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        buf = s.alloc(64)
        s.write(buf, bytes(case["init"]))
        vt = s.alloc(0x40)
        sb = s.alloc(0x20)
        s.words(sb, [s.p(vt)])
        s.words(sb + 0x14, [s.p(buf) + case["p"], s.p(buf) + case["e"]])
        ios = s.alloc(0x40)
        s.words(ios + 0x2C, [case["width"], case["flags"]])
        pre = s.alloc(8)
        s.write(pre, bytes(case["pre"]))
        suf = s.alloc(8)
        s.write(suf, bytes(case["suf"]))
        itr = 0 if case["null"] else sb
        ans = s.call(itr, ios, case["fill"], pre, case["npre"], suf, case["nsuf"])
        return {"answer": ans, "log": s.log(), "buf": s.read(buf, 64), "pptr": s.rd_words(sb + 0x14, 2)[0] - s.p(buf),
                "ios": s.rd_words(ios + 0x2C, 2)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, **kw):
    c = {"init": [r.randrange(256) for _ in range(64)], "p": r.randrange(0, 12), "e": r.randrange(0, 40),
         "width": r.randrange(-2, 14),
         "flags": r.choice([0, 0x10, 0x20, 0x30, 0x80, 0xB0, 0x40, 0x1, 0x2]) | r.choice([0, 0x4, 0x100, 0x8]),
         "fill": r.choice([0x20, 0x30, 0x80, 0xFF, 0, 0x41]), "pre": [r.randrange(256) for _ in range(8)],
         "suf": [r.randrange(256) for _ in range(8)], "npre": r.randrange(0, 5), "nsuf": r.randrange(0, 5),
         "null": r.random() < 0.05,
         "queues": {"overflow": [r.choice([-1, 0, 5, 0x41, 0xFF]) & MASK for _ in range(r.randrange(1, 6))]}}
    c.update(kw)
    return c


def edges():
    r = random.Random(68)
    for fl in (0, 0x10, 0x20, 0x30, 0x80, 0xB0):
        for width in (-1, 0, 3, 4, 5, 9):
            for e in (0, 40):
                for q in ([0], [MASK], [MASK, 0], [0, MASK, 0]):
                    yield mk(r, flags=fl, width=width, e=e, p=r.randrange(0, 4), queues={"overflow": q}, npre=2, nsuf=2)
    yield mk(r, null=True)
    yield mk(r, p=5, e=5, npre=3, nsuf=1, width=0)
    yield mk(r, p=6, e=5, npre=3, nsuf=1, width=6, flags=0x10)
    yield mk(r, p=4, e=5, npre=3, nsuf=0, width=0)
    yield mk(r, npre=0, nsuf=0, width=3)


def rnd(r):
    return mk(r)


TESTS = [
    FnTest("std::__putnumber<char,std::ostreambuf_iterator<char,std::char_traits<char>>>", ppc, host, edge=edges, rnd=rnd),
]
