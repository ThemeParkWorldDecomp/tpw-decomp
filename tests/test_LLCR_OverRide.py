"""Unit v5033, LLCR_OverRide against libtpw_unit_LLCR_OverRide.dylib (tests/unit_LLCR_OverRide.cpp), main app, src/LLCR_OverRide.cpp:
  LLCR_OverRide 0x10089210

The balance words at +0x57c, +0x580, +0x584 of gMainBalance (0x102b4568) and the world (gGameWorld, 0x1037ebfc) with its halfword at +0x1da732 are
written on both sides. The callee 0x1008905c is a fake on both sides recording the two ints whose addresses it gets and writing a queued halfword.
Compared: the output halfword, the event log, the balance words. No fused operations. Not tested: nothing.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCR_OverRide"
MASK = 0xFFFFFFFF

FN = 0x10089210
CALLEE = 0x1008905C
ADDR = dict(balance=0x102B4568, world=0x1037EBFC)
HOST_LAYOUT = [8]
WORLD_SIZE = 0x1DA740

STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_callee(e):
    L = e.cl
    out = e.r(3)
    x, y = e.s32(e.r(4)), e.s32(e.r(5))
    L.rec("Override", (x, y))
    e.write(out, struct.pack(">H", L.ret("Override") & 0xFFFF))


STUBS.internal(CALLEE, stub_callee)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

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

    def g(self, name):
        return ADDR[name]

    def bal(self, off):
        return ADDR["balance"] + off

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, out, x, y):
        T.call_ppc(self.e, FN, [out & MASK, x & MASK, y & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_llcr_addr(name.encode())

    def bal(self, off):
        """The host address of the balance word at the Mac offset (BalanceValue is 8 bytes here, mValues at 16)."""
        return self.g("balance") + 16 + 8 * ((off - 8) // 4)

    def alloc(self, n):
        return self.lib.tpw_llcr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, out, x, y):
        self.lib.tpw_llcr_a(out, x, y)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_llcr_addr.restype, lib.tpw_llcr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_llcr_alloc.restype, lib.tpw_llcr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_llcr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_llcr_a.restype, lib.tpw_llcr_a.argtypes = None, [C.c_uint64, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_llcr_layout(out)
    assert list(out)[:n] == [16], list(out)[:n]   # the host object has an 8-byte vptr: mValues at 16, the Mac's at 8
    lib.bound = True


def build(s, case):
    s.words(s.bal(0x57C), [case["lo"]])
    s.words(s.bal(0x580), [case["row"]])
    s.words(s.bal(0x584), [case["hi"]])
    w = s.alloc(WORLD_SIZE)
    s.write(w + 0x1DA730, struct.pack(s.fmt[0] + "HH", 0x1111, case["wword"]))
    s.write(s.g("world"), struct.pack(">I", w) if s.fmt[0] == ">" else struct.pack("<Q", w))   # a native pointer on the host
    out = s.alloc(8)
    s.write(out, struct.pack(s.fmt[0] + "HH", 0xABCD, 0x1234))
    return out


def run(s, case):
    try:
        out = build(s, case)
        s.call(out, case["x"], case["y"])
        return {"out": struct.unpack(s.fmt[0] + "HH", s.read(out, 4)), "log": s.log(),
                "bal": [s.rd_word(s.bal(o)) for o in (0x57C, 0x580, 0x584)]}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, lo, hi, row, x, y):
    return {"lo": lo, "hi": hi, "row": row, "x": x, "y": y, "wword": r.getrandbits(16),
            "queues": {"Override": [r.getrandbits(16)]}}


def edges():
    r = random.Random(33)
    for lo, hi, row in ((10, 20, 5), (0, 0, 0), (-5, 3, -2), (0x7FFFFFFF, 0x7FFFFFFF, 1), (-0x80000000, -0x80000000, 1)):
        xs = {lo - 2, lo - 1, lo, lo + 1, hi, hi + 1, hi + 2, 0, -1}
        for x in sorted(xs):
            for y in (row, row + 1, row - 1):
                yield mk(r, lo, hi, row, T.s32(x), T.s32(y))


def rnd(r):
    lo = r.randrange(-20, 40)
    hi = lo + r.randrange(-3, 15)
    row = r.randrange(-5, 10)
    return mk(r, lo, hi, row, r.randrange(-25, 60), row if r.random() < 0.6 else r.randrange(-8, 12))


TESTS = [FnTest("LLCR_OverRide", ppc, host, edge=edges, rnd=rnd)]
