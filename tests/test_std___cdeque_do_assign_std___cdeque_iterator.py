"""Unit v5564, std::__cdeque<const void*>::do_assign<errorthing iterator>, against libtpw_unit_std___cdeque_do_assign_std___cdeque_iterator.dylib
(tests/unit_std___cdeque_do_assign_std___cdeque_iterator.cpp), main app, src/std___cdeque_do_assign_std___cdeque_iterator.cpp:
  do_assign 0x101f3278

A case is a world and a call, built twice by one piece of Python code (emulator memory big-endian, host memory little-endian). The callees
(strlen, strcpy, operator new, new[], delete, delete[], __throw) are fakes on both sides that record the same events. The PowerPC __throw
stub sends the function to its epilogue (the original never returns from it); the host fake throws a C++ exception the wrapper catches.

The world: the target cdeque (capacity, size, start, block) with an old block, the source cdeque (capacity, block) with its words, and
the two iterators (cdeque, index, slot). Compared: the target words, its block's words (and whether it is still the old block), the source
block, the event log, the throw flag. No fused operations. Not tested: a count of exactly 0x3fffffff (it would copy a billion words).
"""
import ctypes as C
import random
import struct

from unicorn.ppc_const import UC_PPC_REG_LR

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___cdeque_do_assign_std___cdeque_iterator"
MASK = 0xFFFFFFFF

FN = 0x101f3278
EPILOGUE = 0x101f35a4
G_STRLEN, G_STRCPY, G_NEW, G_NEWA, G_DEL, G_DELA, G_THROW = 0x10238650, 0x10238410, 0x102384b8, 0x10238668, 0x10238218, 0x10238638, 0x10238680
ADDR = dict(vtbl=0x10278b74, dtor=0x10265680)
HOST_LAYOUT = [12, 16]
STATE = {"old": 0}


def txt(x):
    return x.decode("latin-1") if isinstance(x, (bytes, bytearray)) else x


STUBS = CL.Stubs()
STUBS.real.add(FN)


def st_strlen(e):
    s = txt(e.cstr(e.r(3)))
    e.cl.rec("strlen", (), (s,))
    e.set_r(3, len(s))


def st_strcpy(e):
    d, s = e.r(3), txt(e.cstr(e.r(4)))
    e.cl.rec("strcpy", (), (s,))
    e.write(d, s.encode("latin-1") + b"\0")
    e.set_r(3, d)


def st_new(e):
    n = e.r(3)
    e.cl.rec("new", (n,))
    e.set_r(3, e.alloc(max(n, 4)))


def st_newa(e):
    n = e.r(3)
    e.cl.rec("new[]", (n,))
    e.set_r(3, e.alloc(max(n, 4)))


def st_del(e):
    e.cl.rec("delete", (1 if e.r(3) == STATE["old"] else 0,))


def st_dela(e):
    e.cl.rec("delete[]", (e.cl.lab(e.r(3)),))


def st_throw(e):
    L, o = e.cl, e.r(4)
    L.rec("throw", (L.lab(e.u32(o)), e.u32(e.u32(o + 8)), L.lab(e.r(5))), (txt(e.cstr(e.r(3))), txt(e.cstr(e.u32(o + 4)))))
    e.uc.reg_write(UC_PPC_REG_LR, EPILOGUE)


for a, f in ((G_STRLEN, st_strlen), (G_STRCPY, st_strcpy), (G_NEW, st_new), (G_NEWA, st_newa), (G_DEL, st_del), (G_DELA, st_dela),
             (G_THROW, st_throw)):
    def with_toc(e, f=f):
        # the glue stub saves the TOC register in the caller's frame; the function reloads it from 0x14(r1) after every call
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        f(e)

    STUBS.internal(a, with_toc)


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n))) if n else []

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def full(self, a):
        return a

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["vtbl"]: 2, ADDR["dtor"]: 3}, {})
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, args):
        r3, _, _ = T.call_ppc(self.e, FN, [a & MASK for a in args])
        return 0 if self.e.u32 is None else self.threw

    threw = 0


class Host(Side):
    fmt = "<I"

    def full(self, a):
        return (self.T & ~MASK) | (a & MASK)

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {2: self.g("vtbl"), 3: self.g("dtor")}, {})

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cdassign_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cdassign_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, args):
        return self.lib.tpw_cdassign_do(*[a & MASK for a in args])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cdassign_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "do").restype, getattr(lib, p + "do").argtypes = C.c_uint32, [C.c_uint32] * 7
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    """Target T (4 words) with the old block O, source S (4 words) with its block B."""
    t0 = case["t_cap"]
    s.T = s.alloc(16)
    s.O = s.alloc(max(4 * t0, 4)) if case["t_block"] else 0
    s.S = s.alloc(16)
    s.B = s.alloc(4 * case["s_cap"])
    s.words(s.B, case["s_words"])
    if s.O:
        s.words(s.O, case["t_words"])
    s.words(s.T, [t0, case["t_size"], case["t_start"], s.O])
    s.words(s.S, [case["s_cap"], 0, 0, s.B])
    STATE["old"] = s.O


def execute(s, case):
    first = [s.S, case["i0"], s.B + 4 * case["slot"]]
    last = [s.S, (case["i0"] + case["count"]) & MASK, s.B]
    return s.call([s.T] + first + last)


def observe(s, case, threw):
    t = s.rd_words(s.T, 4)
    cap = t[0]
    block = t[3]
    if block == 0:
        kind, words = "null", []
    elif block == s.O & MASK:
        kind, words = "old", s.rd_words(s.full(block), cap)
    else:
        kind, words = "new", s.rd_words(s.full(block), cap)
    return {"threw": threw, "log": s.log(), "t": t[:3], "kind": kind, "block": words, "src": s.rd_words(s.B, case["s_cap"])}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    s = Ppc(e, case)
    s.threw = 0
    try:
        build(s, case)
        T.call_ppc(e, FN, [a & MASK for a in ([s.T] + [s.S, case["i0"], s.B + 4 * case["slot"]] +
                                              [s.S, (case["i0"] + case["count"]) & MASK, s.B])])
        threw = 1 if any(x[0] == "throw" for x in s.log()) else 0
        return observe(s, case, threw)
    finally:
        s.done()


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, count, t_cap=None, s_cap=None, t_block=True):
    t_cap = r.randrange(0, 12) if t_cap is None else t_cap
    s_cap = r.randrange(1, 9) if s_cap is None else s_cap
    return {"t_cap": t_cap, "t_block": t_block or t_cap > 0, "t_size": r.getrandbits(32), "t_start": r.getrandbits(32),
            "t_words": [r.getrandbits(32) for _ in range(max(t_cap, 1))], "s_cap": s_cap,
            "s_words": [r.getrandbits(32) for _ in range(s_cap)], "slot": r.randrange(0, s_cap),
            "i0": r.choice([0, 5, r.getrandbits(32), MASK - 2]), "count": count}


def edges():
    r = random.Random(5564)
    for t in (0, 1, 4):
        for d in (-1, 0, 1):
            c = t + d
            if c >= 0:
                yield mk(r, c, t_cap=t)
    yield mk(r, 0, t_cap=0, t_block=False)
    yield mk(r, 3, t_cap=0, t_block=False)
    yield mk(r, 20, t_cap=2, s_cap=3)
    yield mk(r, 7, t_cap=9, s_cap=1)
    yield mk(r, 0x40000000)
    yield mk(r, MASK)
    yield mk(r, 0x80000000, t_cap=3)


def rnd(r):
    return mk(r, r.choice([0, 1, r.randrange(0, 30)]), t_block=r.random() < 0.8)


TESTS = [
    FnTest("std::__cdeque<const_void*,std::allocator<const_void*>>::do_assign<std::__cdeque<errorthingy*,std::allocator<errorthingy*>>::iterator>",
           ppc, host, edge=edges, rnd=rnd),
]
