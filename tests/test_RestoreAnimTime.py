"""Unit v5561, RestoreAnimTime, against libtpw_unit_RestoreAnimTime.dylib (tests/unit_RestoreAnimTime.cpp), main app
("SimTheme Park"), src/RestoreAnimTime.cpp:
  RestoreAnimTime 0x1006b6f4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. IsValidAnimation and RestoreAnimTimeStats are
fakes on both sides that record their arguments (the stats fake logs the first word of the header it is given, a marker).

The world of a case: a ride anim set with two groups of two entries (header blocks with markers or NULL), a control filled with random
words, a save block of random words, and the answer queue of IsValidAnimation. What is compared: the answer, the whole control, the log.

No fused operations. Not tested: a valid animation with a kind of 2 or more or an index of 2 or more (the tables hold two of each).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RestoreAnimTime"
MASK = 0xFFFFFFFF

FN = 0x1006B6F4
OWN = {FN: 288}
IS_VALID = 0x100C5D3C
STATS = 0x100C5C68

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def stub_is_valid(e):
    L = e.cl
    L.rec("IsValidAnimation", (e.r(4) & MASK, e.r(5) & MASK))
    e.set_r(3, L.ret("IsValidAnimation") & MASK)


def stub_stats(e):
    L = e.cl
    L.rec("RestoreAnimTimeStats", (e.u32(e.r(4)),))


STUBS.internal(IS_VALID, stub_is_valid)
STUBS.internal(STATS, stub_stats)


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

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def ptr(self, a, value):
        self.write(a, struct.pack(">I", value))

    def make_set(self):
        s = self.alloc(0x100)
        return s, 0x18, 0xC, 8, 8, 4

    def call(self, st, ctl, save):
        r3, _, _ = T.call_ppc(self.e, FN, [st & MASK, ctl & MASK, save & MASK])
        return r3 & 0xFF


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        out = (C.c_int64 * 32)()
        n = lib.tpw_rat_layout(out)
        self.lay = list(out)[:n]

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_rat_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptr(self, a, value):
        self.write(a, struct.pack("<Q", value))

    def make_set(self):
        size, goff, gsize, eoff, esize, aoff = self.lay[:6]
        return self.alloc(size), goff, gsize, eoff, esize, aoff

    def call(self, st, ctl, save):
        return self.lib.tpw_rat_call(st, ctl, save) & 0xFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rat_alloc.restype, lib.tpw_rat_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rat_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rat_call.restype, lib.tpw_rat_call.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rat_layout(out)
    assert list(out)[6:n] == [0x30, 0x2C], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        st, goff, gsize, eoff, esize, aoff = s.make_set()
        for k in range(2):
            entries = s.alloc(2 * esize)
            s.ptr(st + goff + k * gsize + eoff, entries)
            for j in range(2):
                m = case["marks"][k][j]
                if m is not None:
                    h = s.alloc(0x40)
                    s.words(h, [m])
                    s.ptr(entries + j * esize + aoff, h)
        ctl = s.alloc(0x38)
        s.words(ctl, case["ctl"])
        save = s.alloc(0x2C)
        s.words(save, case["save"])
        ans = s.call(st, ctl, save)
        c = s.rd_words(ctl, 14)
        for i in (3, 12):
            c[i] = canon_f(c[i])
        return {"answer": ans, "ctl": c, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, a=None, b=None, flags=None, valid=None):
    small = lambda: r.choice([0, 1])
    a = r.choice([0, 1, 1, 0, 5, 0x80000000, 0xFFFFFFFF]) if a is None else a
    b = r.choice([0, 1, 1, 0, 7, 0xFFFFFFFF]) if b is None else b
    f = r.getrandbits(32) if flags is None else flags
    save = [f, a, b] + [r.getrandbits(32) for _ in range(8)]
    save[6] = r.choice([0, 0x3F800000, 0x7FC00000, r.getrandbits(32) & 0x7F7FFFFF])
    save[10] = r.choice([0, 0x3F800000, 0xFF800000, r.getrandbits(32) & 0x7F7FFFFF])
    ok = a < 2 and b < 2
    v = r.choice([0, 1]) if valid is None else valid
    marks = [[r.choice([None, r.getrandbits(32)]) for _ in range(2)] for _ in range(2)]
    if ok and v and r.random() < 0.8:
        marks[a][b] = r.getrandbits(32) | 1
    if not ok:
        v = 0
    return {"queues": {"IsValidAnimation": [v]}, "save": save, "marks": marks,
            "ctl": [r.getrandbits(32) for _ in range(14)]}


def edges():
    r = random.Random(77)
    for f in (0, 4, 0x14, 0xFFFFFFFB, 0xFFFFFFFF):
        for a, b, v in ((0, 0, 0), (0, 0, 1), (1, 1, 1), (0, 1, 1), (1, 0, 0), (5, 0, 0)):
            c = mk(r, a, b, f, v)
            if v and c["marks"][a][b] is None:
                c["marks"][a][b] = 0x1234
            yield c
        c = mk(r, 1, 0, f, 1)
        c["marks"][1][0] = None
        yield c


def rnd(r):
    return mk(r)


TESTS = [FnTest("RestoreAnimTime", ppc, host, edge=edges, rnd=rnd)]
