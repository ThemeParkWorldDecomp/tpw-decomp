"""Unit v5941, std::basic_string<char> replace 0x10002d20 and init 0x10072e6c, against libtpw_unit_std_basic_string_replace_2.dylib
(tests/unit_std_basic_string_replace_2.cpp), main app ("SimTheme Park"), src/game/MslSupport.cpp.

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian at the Mac's addresses,
host memory little-endian). The callees outside the unit (strlen, strcpy, new, new[], delete, memcpy, memset, __throw) are fakes on
both sides that record the same events; memmove is real on both sides. memset skips a fill above 16 MiB on both sides.

The world of a case: the string cell (one word, the address of a representation len/cap/refs/data), the representation with a random
buffer, the three terminator globals set to nonzero values. What is compared: the final representation (len, cap, refs, the buffer
bytes the call may have changed), the old representation, which representation the cell holds (the original or a new one) and the
event log (including the exception object the throw paths hand to __throw: names, vtable id, destructor table id, message, counter).

No fused operations. Reachable throw paths: pos out of range (continues with pos > len and n1 = len - pos, which keeps every offset
inside the buffer) and the length error with n2 of 0xfffffffe or 0xffffffff (offsets wrap in 32 bits). Not tested: init with
n = 0xffffffff (the throw is followed by a 4 GiB fill and a store at data + 0xffffffff, both of which fault on either side), and
the other length error inputs (they fill or store out of the buffer).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_string_replace_2"
MASK = 0xFFFFFFFF

FN_REPLACE, FN_INIT = 0x10002D20, 0x10072E6C
SIZES = {FN_REPLACE: 1176, FN_INIT: 516}
OWN = set(SIZES)

G_STRLEN, G_NEWA, G_NEW, G_DELETE, G_STRCPY, G_THROW = 0x10238650, 0x10238668, 0x102384B8, 0x10238218, 0x10238410, 0x10238680
G_MEMCPY, G_MEMSET, G_MEMMOVE = 0x102386B0, 0x10238560, 0x102386C8

ADDR = dict(vtblA=0x10278BA0, vtblB=0x10278BB0, vtblC=0x10278BC0, vtblLE=0x10278B74, dtorOOR=0x10265698, dtorLE=0x10265680,
            nulCopy=0x10278BD0, nulPlace=0x10278BD1, nulInit=0x1029D295)
VTBL_IDS = {ADDR["vtblA"]: 1, ADDR["vtblB"]: 2, ADDR["vtblC"]: 3, ADDR["vtblLE"]: 4}
DTOR_IDS = {ADDR["dtorOOR"]: 1, ADDR["dtorLE"]: 2}
HOST_LAYOUT = [0, 4, 8, 16, 8]


def H(b):
    acc = 0
    for x in b:
        acc = (acc * 131 + x) & MASK
    return acc


def cbytes(e, a):
    x = e.cstr(a)
    return x.encode("latin1") if isinstance(x, str) else bytes(x)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_strlen(e):
    n = len(cbytes(e, e.r(3)))
    e.cl.rec("strlen", (n,))
    e.set_r(3, n)


def st_alloc(name):
    def f(e):
        n = e.r(3)
        e.cl.rec(name, (n,))
        e.set_r(3, e.alloc(4 if n == 0 else n))
    return f


def st_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def st_strcpy(e):
    d, s = e.r(3), e.r(4)
    b = cbytes(e, s)
    e.cl.rec("strcpy", (H(b),))
    e.write(d, b + b"\0")


def st_memcpy(e):
    d, s, n = e.r(3), e.r(4), e.r(5)
    e.cl.rec("memcpy", (n,))
    if n:
        e.write(d, e.read(s, n))


def st_memset(e):
    d, c, n = e.r(3), e.r(4) & 0xFF, e.r(5)
    e.cl.rec("memset", (c, n))
    if 0 < n <= 0x1000000:
        e.write(d, bytes([c]) * n)


def st_memmove(e):
    d, s, n = e.r(3), e.r(4), e.r(5)
    if n:
        e.write(d, e.read(s, n))


def st_throw(e):
    names, obj, dtor = e.r(3), e.r(4), e.r(5)
    vt = VTBL_IDS.get(e.u32(obj), 0)
    msg = cbytes(e, e.u32(obj + 4))
    count = e.u32(e.u32(obj + 8))
    e.cl.rec("throw", (H(cbytes(e, names)), vt, DTOR_IDS.get(dtor, 0), H(msg), count))


def glue(fn):
    """A glue import saves the caller's TOC at 0x14(r1); the code reloads r2 from there after the call."""
    def f(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return f


STUBS.internal(G_STRLEN, glue(st_strlen))
STUBS.internal(G_NEWA, glue(st_alloc("newa")))
STUBS.internal(G_NEW, glue(st_alloc("new")))
STUBS.internal(G_DELETE, glue(st_delete))
STUBS.internal(G_STRCPY, glue(st_strcpy))
STUBS.internal(G_MEMCPY, glue(st_memcpy))
STUBS.internal(G_MEMSET, glue(st_memset))
STUBS.internal(G_MEMMOVE, glue(st_memmove))
STUBS.internal(G_THROW, glue(st_throw))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    DATA = 0xC

    def __init__(self, e, case):
        self.e = e
        self.case = case

    def start(self, fixed):
        self.L = self.e.cl = CL.PpcLog({a: n for n, a in fixed.items()}, self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if hasattr(self, "saved"):
            CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def set_ptr(self, a, v):
        self.words(a, [v])

    def get_ptr(self, a):
        return self.rd_words(a, 1)[0]

    def log(self):
        return self.L.log

    def call(self, entry, args):
        T.call_ppc(self.e, entry, [a & MASK for a in args])


class Host(Side):
    fmt = "<I"
    DATA = 0x10

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)

    def start(self, fixed):
        CL.host_begin(self.lib, fixed, self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bsr2_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_bsr2_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def set_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def get_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        if entry == FN_REPLACE:
            self.lib.tpw_bsr2_replace(args[0], args[1] & MASK, args[2] & MASK, args[3] & MASK, args[4] & 0xFF)
        else:
            self.lib.tpw_bsr2_init(args[0], args[1] & MASK, args[2] & 0xFF)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bsr2_addr.restype, lib.tpw_bsr2_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bsr2_alloc.restype, lib.tpw_bsr2_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bsr2_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bsr2_replace.restype, lib.tpw_bsr2_replace.argtypes = None, [C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint32, C.c_int32]
    lib.tpw_bsr2_init.restype, lib.tpw_bsr2_init.argtypes = None, [C.c_uint64, C.c_uint32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_bsr2_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    for k, name in (("nul_copy", "nulCopy"), ("nul_place", "nulPlace"), ("nul_init", "nulInit")):
        s.write(s.g(name), bytes([case[k]]))
    cell = s.alloc(8)
    world = {"cell": cell, "rep": 0, "data": 0}
    if case["op"] == "replace":
        rep = s.alloc(0x20)
        data = s.alloc(case["cap"] + 1)
        s.write(data, bytes(case["data"]))
        s.words(rep, [case["len"], case["cap"], case["refs"]])
        s.set_ptr(rep + s.DATA, data)
        s.set_ptr(cell, rep)
        world.update(rep=rep, data=data)
    fixed = {1: world["rep"], 2: world["data"]} if world["rep"] else {}
    s.start(fixed)
    return world


def execute(s, case, world):
    if case["op"] == "replace":
        s.call(FN_REPLACE, [world["cell"], case["pos"], case["n1"], case["n2"], case["c"]])
    else:
        s.call(FN_INIT, [world["cell"], case["n"], case["c"], 0])


def observe(s, case, world):
    final = s.get_ptr(world["cell"])
    out = {"same_rep": final == world["rep"], "log": s.log()}
    ln, cap, refs = s.rd_words(final, 3)
    out["final"] = (ln, cap, refs)
    n = cap + 1 if final == world["rep"] else ln + 1
    out["bytes"] = s.read(s.get_ptr(final + s.DATA), min(n, 4096))
    if world["rep"]:
        out["old"] = s.rd_words(world["rep"], 3)
    return out


def run(s, case):
    try:
        world = build(s, case)
        execute(s, case, world)
        return observe(s, case, world)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases


def nuls(r, c):
    c["nul_copy"], c["nul_place"], c["nul_init"] = r.randrange(1, 256), r.randrange(1, 256), r.randrange(1, 256)


def rep_case(r, ln, cap, refs, pos, n1, n2, c=None):
    cs = {"op": "replace", "queues": {}, "len": ln, "cap": cap, "refs": refs, "pos": pos, "n1": n1, "n2": n2,
          "c": r.randrange(256) if c is None else c, "data": [r.randrange(256) for _ in range(cap + 1)]}
    nuls(r, cs)
    return cs


def rep_random(r):
    k = r.random()
    ln = r.randrange(0, 13)
    cap = ln + r.choice([0, 0, 1, 2, 5, 9])
    refs = r.choice([0, 1, 1, 2, 3])
    if k < 0.15 and cap > ln:                       # pos out of range, offsets stay inside the buffer
        pos = r.randrange(ln + 1, cap + 1)
        return rep_case(r, ln, cap, refs, pos, (ln - pos) & MASK, r.randrange(0, 4))
    if k < 0.3:                                      # the length error with n2 of -1 or -2
        ln = r.randrange(5, 14)
        cap = ln + r.choice([0, 3])
        pos = r.randrange(2, ln - 2)
        n1 = r.randrange(0, ln - pos - 1)
        return rep_case(r, ln, cap, refs, pos, n1, r.choice([0xFFFFFFFE, 0xFFFFFFFF]))
    pos = r.randrange(0, ln + 1)
    n1 = r.choice([0, r.randrange(0, ln + 3), 0xFFFFFFFF, ln, ln - pos])
    return rep_case(r, ln, cap, refs, pos, n1 & MASK, r.choice([0, 1, 2, r.randrange(0, 12)]))


def rep_edges():
    r = random.Random(941)
    for refs in (0, 1, 2):
        # in place, shrink / grow within cap / grow past cap, with and without fill
        yield rep_case(r, 5, 8, refs, 2, 1, 3)
        yield rep_case(r, 5, 5, refs, 2, 1, 1)
        yield rep_case(r, 5, 5, refs, 2, 1, 2)
        yield rep_case(r, 5, 8, refs, 0, 5, 0)
        yield rep_case(r, 5, 8, refs, 5, 0, 3)
        yield rep_case(r, 0, 0, refs, 0, 0, 0)
        yield rep_case(r, 0, 3, refs, 0, 4, 3)
        yield rep_case(r, 6, 6, refs, 6, 7, 0)
        yield rep_case(r, 6, 6, refs, 3, 0xFFFFFFFF, 2)
        yield rep_case(r, 6, 9, refs, 4, 0xFFFFFFFF, 0, 0x80)
        yield rep_case(r, 6, 9, refs, 7, (6 - 7) & MASK, 1)          # pos > len, n1 = len - pos
        yield rep_case(r, 6, 9, refs, 9, (6 - 9) & MASK, 0)
        yield rep_case(r, 6, 9, refs, 7, (6 - 7) & MASK, 2)           # grows past cap in the copy path
        yield rep_case(r, 8, 8, refs, 3, 1, 0xFFFFFFFF)               # the length error, n2 = -1
        yield rep_case(r, 8, 11, refs, 2, 3, 0xFFFFFFFE)              # n2 = -2
        yield rep_case(r, 9, 9, refs, 4, 0, 0xFFFFFFFE)
    for _ in range(4):
        yield rep_random(r)


def init_case(r, n, c=None):
    cs = {"op": "init", "queues": {}, "n": n, "c": r.randrange(256) if c is None else c}
    nuls(r, cs)
    return cs


def init_edges():
    r = random.Random(942)
    for n in (0, 1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 100):
        yield init_case(r, n)
    yield init_case(r, 3, 0)
    yield init_case(r, 3, 0xFF)


def init_random(r):
    return init_case(r, r.choice([r.randrange(0, 40), r.randrange(0, 600)]))


TESTS = [
    FnTest("std::basic_string<char,std::char_traits<char>,std::allocator<char>>::replace", ppc, host, edge=rep_edges, rnd=rep_random),
    FnTest("std::basic_string<char,std::char_traits<char>,std::allocator<char>>::init", ppc, host, edge=init_edges, rnd=init_random),
]
