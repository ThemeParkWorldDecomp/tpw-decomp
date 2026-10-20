"""Unit v5229: the case blind wide string's compare and substring constructor, against libtpw_unit_std_basic_string_compare_3.dylib
(tests/unit_std_basic_string_compare_3.cpp), main app ("SimTheme Park"), src/std_basic_string_compare_3.cpp:
  std::basic_string<wchar_t,nocase_wchar_traits,...>::compare 0x10225a5c, ::basic_string (substring constructor) 0x1022213c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (strlen, new, new[],
strcpy, __throw, wmemcpy, _wcsnicmp, the string's init) are fakes on both sides that record their arguments (texts as a checksum, pointers
into the case's strings as byte offsets) and answer from the case's queues (the new fakes may answer NULL, _wcsnicmp answers any value).

The world of a case: a string object (one pointer to a representation: length, capacity, refs, data pointer) with its characters, and for
compare a second wide string. What is compared: the answer, the source representation, the destination object (shared, new copy with its
fields and characters, or NULL) and the event log. No fused operations. Not tested: nothing skipped (the throw fake returns, so the code
after a failed pos check runs too, as it would after the original's __throw returns).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_string_compare_3"
MASK = 0xFFFFFFFF

FN_CMP, FN_CPY = 0x10225A5C, 0x1022213C
SIZES = {FN_CMP: 496, FN_CPY: 628}
OWN = set(SIZES)

STRLEN, NEWA, NEW, STRCPY, THROW, WMEMCPY, WCSNICMP, INIT = (0x10238650, 0x10238668, 0x102384B8, 0x10238410, 0x10238680, 0x102389E0,
                                                              0x1023A660, 0x1022705C)

ADDR = dict(vtblA=0x10278BA0, vtblB=0x10278BB0, vtblC=0x10278BC0, dtorTV=0x10265698)
HOST_LAYOUT = [16, 24]


def s32(v):
    return T.s32(v)


def textsum(b):
    if isinstance(b, str):
        b = b.encode("latin-1")
    h = 0
    for c in b:
        h = (h * 31 + c) & MASK
    return s32(h)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def off(a, base):
    return s32((a - base) & MASK)


def st_strlen(e):
    s = e.cstr(e.r(3))
    e.cl.rec("strlen", (textsum(s),))
    e.set_r(3, len(s))


def st_newa(e):
    n = e.r(3)
    L = e.cl
    L.rec("newa", (n,))
    e.set_r(3, 0 if L.ret("nullA") else e.alloc(n))


def st_new(e):
    n = e.r(3)
    L = e.cl
    L.rec("new", (n,))
    e.set_r(3, 0 if L.ret("nullN") else e.alloc(n))


def st_strcpy(e):
    d, sp = e.r(3), e.r(4)
    s = e.cstr(sp)
    e.cl.rec("strcpy", (textsum(s),))
    if d:
        e.write(d, (s.encode("latin-1") if isinstance(s, str) else s) + b"\0")


def st_throw(e):
    L = e.cl
    name, obj, tv = e.r(3), e.r(4), e.r(5)
    vt, ptr, cnt = e.u32(obj), e.u32(obj + 4), e.u32(obj + 8)
    L.rec("throw", (textsum(e.cstr(name)), L.lab(vt), textsum(e.cstr(ptr)) if ptr else -1, s32(e.u32(cnt)) if cnt else -1, L.lab(tv)))


def st_wmemcpy(e):
    d, sp, n = e.r(3), e.r(4), e.r(5)
    e.cl.rec("wmemcpy", (off(sp, e.cl.bd), n))
    if d:
        e.write(d, e.read(sp, n * 2))
    e.set_r(3, d)


def st_wcsnicmp(e):
    L = e.cl
    a, b, n = e.r(3), e.r(4), e.r(5)
    L.rec("wcsnicmp", (off(a, L.bd), off(b, L.bs) if b else -1, n))
    e.set_r(3, L.ret("wcsnicmp") & MASK)


def st_init(e):
    L = e.cl
    L.rec("init", (off(e.r(4), L.bd), off(e.r(5), L.bd)))


for _a, _f in ((STRLEN, st_strlen), (NEWA, st_newa), (NEW, st_new), (STRCPY, st_strcpy), (THROW, st_throw), (WMEMCPY, st_wmemcpy),
               (WCSNICMP, st_wcsnicmp), (INIT, st_init)):
    def _wrap(f):
        def g(e):
            e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))   # the glue saves the TOC where the caller reloads it from
            f(e)
        return g
    STUBS.internal(_a, _wrap(_f))


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def chars(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))


class Ppc(Side):
    fmt = ">I"
    dataoff = 0xC

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["vtblA"]: 1, ADDR["vtblB"]: 2, ADDR["vtblC"]: 3, ADDR["dtorTV"]: 4}, case["queues"])
        self.L.bd = self.L.bs = 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def setptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_words(a, 1)[0]

    def set_bases(self, d, sb):
        self.L.bd, self.L.bs = d, sb

    def log(self):
        return self.L.log

    def compare(self, w, pos, n1, n2):
        return s32(T.call_ppc(self.e, FN_CMP, [w["obj"], pos, n1, w["s"], n2])[0])

    def copy(self, w, pos, count):
        T.call_ppc(self.e, FN_CPY, [w["dst"], w["obj"], pos, count])
        return 0


class Host(Side):
    fmt = "<I"
    dataoff = 0x10

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("vtblA"), 2: self.g("vtblB"), 3: self.g("vtblC"), 4: self.g("dtorTV")}, case["queues"])

    def g(self, name):
        return self.lib.tpw_nc_addr(name.encode())

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_nc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def set_bases(self, d, sb):
        self.lib.tpw_nc_setbase(d, sb)

    def log(self):
        return CL.host_end(self.lib)

    def compare(self, w, pos, n1, n2):
        return self.lib.tpw_nc_compare(w["obj"], pos, n1, w["s"], n2)

    def copy(self, w, pos, count):
        self.lib.tpw_nc_copy(w["dst"], w["obj"], pos, count)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_nc_addr.restype, lib.tpw_nc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_nc_alloc.restype, lib.tpw_nc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_nc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_nc_setbase.restype, lib.tpw_nc_setbase.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_nc_compare.restype = C.c_int32
    lib.tpw_nc_compare.argtypes = [C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint64, C.c_uint32]
    lib.tpw_nc_copy.restype, lib.tpw_nc_copy.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_nc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    chars = case["chars"]
    sch = case["s"]
    data = s.alloc(2 * len(chars))
    s.write(data, struct.pack(s.fmt[0] + "%dH" % len(chars), *chars))
    rep = s.alloc(32)
    s.words(rep, [case["len"], case["cap"], case["refs"]])
    s.setptr(rep + s.dataoff, data)
    obj = s.alloc(8)
    s.setptr(obj, rep)
    dst = s.alloc(8)
    sd = s.alloc(2 * len(sch))
    s.write(sd, struct.pack(s.fmt[0] + "%dH" % len(sch), *sch))
    s.set_bases(data, sd)
    s.w = dict(obj=obj, rep=rep, dst=dst, s=sd)


def rep_view(s, rep):
    n, cap, refs = s.rd_words(rep, 3)
    p = s.rd_ptr(rep + s.dataoff)
    text = s.chars(p, min(n + 1, 64)) if p else None
    return (n, cap, refs, text)


def observe(s, case, answer):
    w = s.w
    out = {"answer": answer, "log": s.log(), "src": rep_view(s, w["rep"])}
    if case["op"] == "cpy":
        d = s.rd_ptr(w["dst"])
        out["dst_same"] = d == w["rep"]
        out["dst"] = "null" if d == 0 else (None if d == w["rep"] else rep_view(s, d))
    return out


def run(s, case):
    try:
        build(s, case)
        if case["op"] == "cmp":
            ans = s.compare(s.w, *case["args"])
        else:
            ans = s.copy(s.w, *case["args"])
        return observe(s, case, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


BIG = [0xFFFFFFFF, 0x80000000, 0x7FFFFFFF]
NULLQ = [[0], [0], [0], [1], [0, 1], [1, 0], [0, 0, 1]]
CMPQ = [[0], [0], [5], [-3], [0x7FFFFFFF], [-0x80000000], [1]]


def rchars(r, n):
    return [r.randrange(1, 0x10000) if r.random() < 0.3 else r.randrange(0x41, 0x5B) for _ in range(n)]


def make(r, op, length=None, pos=None, a=None, b=None, refs=None):
    if length is None:
        length = r.choice([0, 1, 2, 3, 4, 5, 7, 8, 12, r.randrange(0, 40)])
    cap = r.choice([length, length + 1, (length + 3) - (length & 3), r.randrange(0, 60)])
    if refs is None:
        refs = r.choice([0, 0, 1, 2, 7, 0xFFFFFFFF])
    pool = [0, 1, length, max(length - 1, 0), length + 1, r.randrange(0, length + 2), r.randrange(0, 100)] + BIG
    if pos is None:
        pos = r.choice(pool)
    if a is None:
        a = r.choice(pool + [(length - pos) & MASK, (length - pos + 1) & MASK, (length - pos - 1) & MASK])
    if b is None:
        b = r.choice(pool)
    c = {"op": op, "len": length, "cap": cap, "refs": refs, "chars": rchars(r, length) + [0] * 6, "s": rchars(r, r.randrange(0, 12)) + [0],
         "queues": {"nullN": r.choice(NULLQ), "nullA": r.choice(NULLQ), "wcsnicmp": r.choice(CMPQ)}}
    if op == "cmp":
        c["args"] = [pos, a, b]          # pos, n1, n2
    else:
        c["args"] = [pos, a]             # pos, count
    return c


def cmp_edges():
    r = random.Random(5229)
    for length in (0, 1, 4, 5):
        for pos in (0, 1, length, length + 1, 0xFFFFFFFF):
            for n1 in (0, 1, length, length + 1, 0xFFFFFFFF):
                for n2 in (0, 1, length, length + 1):
                    yield make(r, "cmp", length, pos, n1, n2)


def cmp_random(r):
    return make(r, "cmp")


def cpy_edges():
    r = random.Random(1229)
    for length in (0, 1, 4, 5, 8):
        for pos in (0, 1, length, length + 1, 0xFFFFFFFF):
            for count in (0, 1, length, length + 1, 0xFFFFFFFF, (length - pos) & MASK):
                for refs in (0, 1, 3):
                    yield make(r, "cpy", length, pos, count, None, refs)


def cpy_random(r):
    return make(r, "cpy")


TESTS = [
    FnTest("std::basic_string<wchar_t,nocase_wchar_traits,std::allocator<wchar_t>>::compare", ppc, host, edge=cmp_edges, rnd=cmp_random),
    FnTest("std::basic_string<wchar_t,nocase_wchar_traits,std::allocator<wchar_t>>::basic_string", ppc, host, edge=cpy_edges, rnd=cpy_random),
]
