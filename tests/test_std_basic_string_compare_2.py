"""Unit v5146, std::basic_string<char> compare / substring constructor / append, against
libtpw_unit_std_basic_string_compare_2.dylib (tests/unit_std_basic_string_compare_2.cpp), main app
("SimTheme Park"), src/std_basic_string_compare_2.cpp:
  compare 0x10209cac, basic_string(const&, pos, n) 0x101b2918, append(n, c) 0x100731b4

A case is a world and a call. The world is built twice by ONE piece of Python code (Side.words): a string object (one word, the
address of the representation len/cap/refs/data) whose data lives in a 0x1000 byte block of random characters (the data pointer is
block + 0x800). The callees outside the unit are fakes on both sides: operator new and new[] (the size is logged), __throw
(checksums of type name and message, labels of the vtable and destructor vector, the counter value), change_size (length before,
new length, flag; moves the data into a fresh block with spare bytes before it). strlen, strcpy, memcpy, memset and memcmp are
the C library's on both sides (the PowerPC memcmp answers what the host's libc answers).

What is compared: the answer, the event log, the representations (len, cap, refs, data bytes, whether the new string shares the
source's representation or data), the source block, and for append the data block around the end.

No fused operations. Not tested: append with n above 0xfffffffe, or with a sum below 2^32 - 1 that needs a huge block (the
fake would have to allocate gigabytes, memset would write them); the length error is tested where the sum wraps to a small
length (the fakes return after the throw, so the code goes on).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_string_compare_2"
MASK = 0xFFFFFFFF

FN_COMPARE, FN_CTOR, FN_APPEND = 0x10209CAC, 0x101B2918, 0x100731B4
SIZES = {FN_COMPARE: 492, FN_CTOR: 792, FN_APPEND: 408}
OWN = set(SIZES)

NEW, NEWA, MEMCMP, CHANGE, THROW = 0x102384B8, 0x10238668, 0x10239B38, 0x100027B0, 0x10238680

ADDR = dict(vtblA=0x10278BA0, vtblLen=0x10278B74, dtorOor=0x10265698, dtorLen=0x10265680, nul=0x102AC358)
HOST_LAYOUT = [8, 24, 16]

libc = C.CDLL(None)
libc.memcmp.restype = C.c_int
libc.memcmp.argtypes = [C.c_char_p, C.c_char_p, C.c_size_t]


def tob(x):
    return bytes(x) if isinstance(x, (bytes, bytearray)) else x.encode("latin1")


def chk(x):
    b = tob(x)
    h = len(b)
    for v in b:
        h = (h * 31 + v) & MASK
    return h


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_new(e):
    n = e.r(3) & MASK
    e.cl.rec("new", (n,))
    n = max(n, 4)
    a = e.alloc(n)
    e.write(a, bytes(n))
    e.set_r(3, a)


def stub_newa(e):
    n = e.r(3) & MASK
    e.cl.rec("newa", (n,))
    n = max(n, 4)
    a = e.alloc(n)
    e.write(a, bytes(n))
    e.set_r(3, a)


def stub_memcmp(e):
    a, b, n = e.r(3), e.r(4), e.r(5)
    r = 0 if n == 0 else libc.memcmp(e.read(a, n), e.read(b, n), n)
    e.set_r(3, r & MASK)


def stub_change(e):
    s, n, keep = e.r(3), e.r(4) & MASK, e.r(5) & 0xFF
    rep = e.u32(s)
    old = e.u32(rep)
    data = e.u32(rep + 12)
    e.cl.rec("change_size", (old, n, 1 if keep else 0))
    base = e.alloc(n + 0x1000)
    e.write(base, bytes(n + 0x1000))
    cnt = min(old, n)
    e.write(base + 0x800, e.read(data, cnt))
    e.write(rep, struct.pack(">II", n, n))
    e.write(rep + 12, struct.pack(">I", base + 0x800))


def stub_throw(e):
    L = e.cl
    name = e.cstr(e.r(3))
    obj = e.r(4)
    vt, ptr, cnt = e.u32(obj), e.u32(obj + 4), e.u32(obj + 8)
    msg = e.cstr(ptr)
    count = e.u32(cnt) if cnt else 0xFFFFFFFF
    L.rec("throw", (chk(name), L.lab(vt) & MASK if L.lab(vt) >= 0 else -1, L.lab(e.r(5)), chk(msg), count))


def stub_memcpy(e):
    d, src, n = e.r(3), e.r(4), e.r(5)
    if n:
        e.write(d, e.read(src, n))


def stub_memset(e):
    d, c, n = e.r(3), e.r(4) & 0xFF, e.r(5)
    if n:
        e.write(d, bytes([c]) * n)


STUBS.internal(0x102386B0, stub_memcpy)
STUBS.internal(0x10238560, stub_memset)
STUBS.internal(NEW, stub_new)
STUBS.internal(NEWA, stub_newa)
STUBS.internal(MEMCMP, stub_memcmp)
STUBS.internal(CHANGE, stub_change)
STUBS.internal(THROW, stub_throw)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def ptrs(self, a, ws):
        self.write(a, b"".join(struct.pack(self.pfmt, w) for w in ws))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    DOFF = 12

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({0x10278BA0: 1, 0x10278B74: 2, 0x10265698: 3, 0x10265680: 4}, case["queues"])
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

    def call(self, op, *args):
        entry = {"compare": FN_COMPARE, "ctor": FN_CTOR, "append": FN_APPEND}[op]
        return T.call_ppc(self.e, entry, [a & MASK for a in args])[0] & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    DOFF = 16

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("vtblA"), 2: self.g("vtblLen"), 3: self.g("dtorOor"), 4: self.g("dtorLen")},
                      case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sbc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sbc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, *args):
        lib = self.lib
        if op == "append":
            return lib.tpw_sbc_append(args[0], args[1] & MASK, T.s32(args[2]))
        if op == "ctor":
            return lib.tpw_sbc_construct(args[0], args[1], args[2] & MASK, args[3] & MASK)
        return lib.tpw_sbc_compare(args[0], args[1] & MASK, args[2] & MASK, args[3], args[4] & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_sbc_"
    lib.tpw_sbc_addr.restype, lib.tpw_sbc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sbc_alloc.restype, lib.tpw_sbc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sbc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sbc_compare.restype = C.c_uint32
    lib.tpw_sbc_compare.argtypes = [C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint64, C.c_uint32]
    lib.tpw_sbc_construct.restype = C.c_uint64
    lib.tpw_sbc_construct.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32]
    lib.tpw_sbc_append.restype = C.c_uint64
    lib.tpw_sbc_append.argtypes = [C.c_uint64, C.c_uint32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sbc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def mkstring(s, ln, cap, refs, data):
    block = s.alloc(0x1000)
    s.write(block, data)
    rep = s.alloc(32)
    s.words(rep, [ln, cap, refs])
    s.ptrs(rep + s.DOFF, [block + 0x800])
    obj = s.alloc(8)
    s.ptrs(obj, [rep])
    return obj, rep, block


def obs_str(s, obj, back, span):
    rep = s.rd_ptr(obj)
    ln, cap, refs = s.rd_words(rep, 3)
    d = s.rd_ptr(rep + s.DOFF)
    if span is None:
        span = min(cap + 1, 0x100)
    return rep, d, {"len": ln, "cap": cap, "refs": refs, "data": s.read(d - back, back + span)}


def run(s, case):
    try:
        op = case["op"]
        obj, rep, block = mkstring(s, case["len"], case["cap"], case["refs"], case["data"])
        s.write(s.g("nul"), bytes([case["nul"]]))
        if op == "compare":
            sb = s.alloc(0x100)
            s.write(sb, case["s"])
            ans = s.call("compare", obj, case["pos"], case["n1"], sb, case["n2"])
            _, _, src = obs_str(s, obj, 0, 0)
            return {"answer": ans, "log": s.log(), "src": src, "block": s.read(block, 0x1000),
                    "same": s.rd_ptr(obj) == rep}
        if op == "ctor":
            dest = s.alloc(8)
            ret = s.call("ctor", dest, obj, case["pos"], case["n"])
            nrep, nd, new = obs_str(s, dest, 0, None)
            _, _, src = obs_str(s, obj, 0, 0)
            return {"answer": ret == dest, "log": s.log(), "new": new, "shared": nrep == rep, "dshared": nd == block + 0x800,
                    "src": src, "block": s.read(block, 0x1000)}
        ret = s.call("append", obj, case["n"], case["c"])
        r2 = s.rd_ptr(obj)
        ln = s.rd_word(r2)
        _, d, st = obs_str(s, obj, 0x400, min(ln, 0x200) + 0x40) if ln < 0x10000 else obs_str(s, obj, 0x400, 0x40)
        return {"answer": ret == obj, "log": s.log(), "str": st, "same": r2 == rep, "block": s.read(block, 0x1000)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

ALPHA = [b"ab", b"\x7f\x80\x81", bytes(range(256))]
HUGE = [0, 1, 2, 5, 0xFFFFFFFF, 0xFFFFFFFE, 0x80000000]


def mkdata(r):
    al = r.choice(ALPHA)
    return bytes(r.choice(al) for _ in range(0x1000))


def base(r, op, ln):
    return {"op": op, "queues": {}, "len": ln, "cap": r.choice([ln, ln + 3, r.randrange(0, 50)]), "refs": r.choice([0, 0, 1, 2, 9]),
            "data": mkdata(r), "nul": r.choice([0, 0x41, 0xFF])}


def compare_case(r, ln, pos, n1, n2, mode):
    c = base(r, "compare", ln)
    if pos > ln:
        n1, n2 = min(n1, 0x40), min(n2, 0x40)
    src = c["data"][0x800 + pos:0x800 + pos + 0x100]
    if mode == "equal":
        s = src
    elif mode == "diff":
        s = bytearray(src)
        k = r.randrange(0, 8)
        s[k] = (s[k] + r.choice([1, 0x80, 0xFF])) & 0xFF
        s = bytes(s)
    else:
        s = bytes(r.randrange(256) for _ in range(0x100))
    c.update(pos=pos, n1=n1, n2=n2, s=s.ljust(0x100, b"\0")[:0x100])
    return c


def compare_edges():
    r = random.Random(11)
    for ln in (0, 1, 5):
        for pos in (0, 1, ln, ln + 1, ln + 4):
            for n1 in HUGE + [ln, ln + 1]:
                for n2 in (0, 1, ln, ln + 1, 0xFFFFFFFF, 3):
                    yield compare_case(r, ln, pos, n1, n2, r.choice(["equal", "equal", "diff", "rand"]))


def compare_random(r):
    ln = r.choice([0, 1, 2, 5, r.randrange(0, 40)])
    pos = r.choice([0, ln, r.randrange(0, ln + 1), ln + r.randrange(1, 6)])
    return compare_case(r, ln, pos, r.choice(HUGE + [r.randrange(0, 40)]), r.choice(HUGE + [r.randrange(0, 40)]),
                        r.choice(["equal", "diff", "rand"]))


def ctor_case(r, ln, pos, n):
    c = base(r, "ctor", ln)
    if pos > ln:
        n = min(n, 0x40)
    c.update(pos=pos, n=n)
    return c


def ctor_edges():
    r = random.Random(12)
    for ln in (0, 1, 4, 5, 9):
        for pos in (0, 1, ln, ln + 1, ln + 5):
            for n in (0, 1, ln, ln - 1 if ln else 2, ln + 1, 0xFFFFFFFF, 0x20):
                yield ctor_case(r, ln, pos, n & MASK)


def ctor_random(r):
    ln = r.choice([0, 1, 2, 3, 4, 5, r.randrange(0, 40)])
    pos = r.choice([0, 0, ln, r.randrange(0, ln + 1), ln + r.randrange(1, 6)])
    n = r.choice([0, 1, ln, ln + 1, 0xFFFFFFFF, r.randrange(0, 40)])
    return ctor_case(r, ln, pos, n)


def append_case(r, ln, n, ch):
    c = base(r, "append", ln)
    c.update(n=n, c=ch)
    return c


def append_edges():
    r = random.Random(13)
    for ln in (0, 1, 5, 0x30):
        for n in (0, 1, 2, 0x20, 0x100):
            for ch in (0, 0x41, 0x80, 0xFF):
                yield append_case(r, ln, n, ch)
    for k in (1, 2, 0x10, 0x100):
        for m in (0, 1, 0x30):
            yield append_case(r, (1 << 32) - k, k + m, r.randrange(256))


def append_random(r):
    if r.random() < 0.15:
        k = r.randrange(1, 0x101)
        return append_case(r, (1 << 32) - k, k + r.randrange(0, 0x40), r.randrange(256))
    return append_case(r, r.choice([0, 1, 2, r.randrange(0, 60)]), r.choice([0, 1, r.randrange(0, 100)]), r.randrange(256))


TESTS = [
    FnTest("std::basic_string<char,std::char_traits<char>,std::allocator<char>>::compare", ppc, host, edge=compare_edges, rnd=compare_random),
    FnTest("std::basic_string<char,std::char_traits<char>,std::allocator<char>>::basic_string", ppc, host, edge=ctor_edges, rnd=ctor_random),
    FnTest("std::basic_string<char,std::char_traits<char>,std::allocator<char>>::append", ppc, host, edge=append_edges, rnd=append_random),
]
