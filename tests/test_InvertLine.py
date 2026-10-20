"""Unit v5757, the text formatter's line helpers, against libtpw_unit_InvertLine.dylib (tests/unit_InvertLine.cpp),
src/ltms_textformatter_rest.cpp and src/ltms_textformatter_rest_InvertLine.cpp:
  InvertLine 0x10006c3c, IsHebInLine 0x10006bb4, GetNextNonDelimiter 0x10006b04, GetNextDelimiter 0x10006a68

A case is an array of 16-byte CCharacterInfo records (the words random or taken from space, comma and the Hebrew range edges) and a
range [first, last) of record indexes. The array is built twice by one piece of code (emulator memory big-endian, host little-endian).
Pointers are compared as record indexes (-1 for NULL). Compared: the answer and the whole array afterwards. No callees, no fused
operations. Not tested: nothing excluded except ranges whose byte length is not a multiple of 16.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_InvertLine"
MASK = 0xFFFFFFFF

INVERT, HEB, NEXTNON, NEXTDEL = 0x10006c3c, 0x10006bb4, 0x10006b04, 0x10006a68
SIZES = {INVERT: 124, HEB: 68, NEXTNON: 100, NEXTDEL: 84}
OWN = set(SIZES)
ENTRY = {"invert": INVERT, "heb": HEB, "nextnon": NEXTNON, "nextdel": NEXTDEL}
HOST_LAYOUT = [0, 8, 16]
SPECIAL = [0x20, 0x2c, 0x5b0, 0x5b1, 0x5ea, 0x5eb, 0x41, 0, 0x1f, 0x21, 0x2b, 0x2d, 0xffff]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, {})
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def call(self, op, a, b):
        r3, _, _ = T.call_ppc(self.e, ENTRY[op], [a & MASK, b & MASK])
        return 0 if op == "invert" else r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_il_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def call(self, op, a, b):
        L = self.lib
        if op == "invert":
            L.tpw_il_invert(a, b)
            return 0
        if op == "heb":
            return L.tpw_il_heb(a, b)
        if op == "nextnon":
            return L.tpw_il_nextnon(a, b)
        return L.tpw_il_nextdel(a, b)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_il_alloc.restype, lib.tpw_il_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_il_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_il_invert.restype, lib.tpw_il_invert.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_il_heb.restype, lib.tpw_il_heb.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    lib.tpw_il_nextnon.restype, lib.tpw_il_nextnon.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64]
    lib.tpw_il_nextdel.restype, lib.tpw_il_nextdel.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_il_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case, op):
    try:
        n = len(case["chars"])
        base = s.alloc(16 * n + 16)
        for i, (ch, w) in enumerate(case["chars"]):
            s.write(base + 16 * i, struct.pack(s.fmt[0] + "HHBBHIHH", ch, (i * 7) & 0xffff, i & 0xff, 0, 0, w, i, 0))
        first, last = case["first"], case["last"]
        ans = s.call(op, base + 16 * first, base + 16 * last)
        if op in ("nextnon", "nextdel"):
            ans = -1 if ans == 0 else (ans - base) // 16
        raw = s.read(base, 16 * n + 16)
        mem = [struct.unpack(s.fmt[0] + "HHBBHIHH", raw[16 * i:16 * i + 16]) for i in range(n + 1)]
        return {"answer": ans, "mem": mem}
    finally:
        s.done()


def mk(op):
    def p(e, case):
        return run(Ppc(e, case), case, op)

    def h(lib, case):
        return run(Host(lib, case), case, op)
    return p, h


def make_case(r, chars, first, last):
    return {"chars": [(c, r.getrandbits(32)) for c in chars], "first": first, "last": last}


def rand_case(r):
    n = r.randrange(0, 9)
    chars = [r.choice(SPECIAL) if r.random() < 0.8 else r.randrange(0, 0x10000) for _ in range(n)]
    if r.random() < 0.4:
        chars = [r.choice([0x20, 0x2c, 0x41]) for _ in range(n)]
    first = r.randrange(0, n + 1)
    last = r.randrange(first, n + 1) if r.random() < 0.9 else r.randrange(0, n + 1)
    return make_case(r, chars, first, last)


def edges():
    r = random.Random(57)
    for ch in SPECIAL:
        for pos in range(4):
            chars = [0x41] * 4
            chars[pos] = ch
            for f in range(5):
                for l in range(5):
                    yield make_case(r, chars, f, l)
    for chars in ([0x20] * 4, [0x2c, 0x20, 0x41, 0x2c], [0x20, 0x41], [0x41, 0x20], [0x20, 0x20, 0x41]):
        for f in range(len(chars) + 1):
            for l in range(len(chars) + 1):
                yield make_case(r, chars, f, l)


TESTS = []
for name, op in (("InvertLine", "invert"), ("IsHebInLine", "heb"), ("GetNextNonDelimiter", "nextnon"), ("GetNextDelimiter", "nextdel")):
    p, h = mk(op)
    TESTS.append(FnTest(name, p, h, edge=edges, rnd=rand_case))
