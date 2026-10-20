"""Unit v5253, std::basic_string<char> of the Metrowerks library, against libtpw_unit_std_basic_string_basic_string.dylib
(tests/unit_std_basic_string_basic_string.cpp), main app, src/TPMain_std_basic_string_basic_string.cpp:
  basic_string(const char*, alloc) 0x10002338, ~basic_string 0x1000221c, init 0x10002b8c, assign(n, c) 0x100730e8, resize 0x10073558,
  basic_string(alloc) 0x1007363c, assign(text, n) 0x101b2440, append(text, n) 0x1020100c

Two pairs of functions share a name (the two constructors, the two assigns): one test of that name drives both of the pair.

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian at the Mac's addresses, host
memory little-endian with native pointers). The callees outside the unit are fakes on both sides that record their arguments: strlen,
operator new / new[] / delete, memcpy, memset, strcpy, the RefCountedPtr constructor, __throw and change_size (which builds a new
representation with a fresh data block, keeping the old characters when asked).

The world: the string object (one word), its representation (len, cap, refs, data block of 80 random bytes), the source text, the two
terminator globals. Compared: the answer (the returned this), the call log, the object's representation after the call (len, cap, refs,
80 data bytes) and the original representation. strlen answers the real length except on the first call where a queue may lie (0xffffffff
reaches the length_error path).
Not tested: n above about 70 (the data blocks are 80 bytes), NULL from operator new (the runtime never returns it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_string_basic_string"
MASK = 0xFFFFFFFF

FN = dict(ctor_c=0x10002338, dtor=0x1000221c, init=0x10002b8c, fill=0x100730e8, resize=0x10073558, ctor0=0x1007363c,
          chars=0x101b2440, append=0x1020100c)
SIZES = {0x10002338: 252, 0x1000221c: 188, 0x10002b8c: 248, 0x100730e8: 104, 0x10073558: 128, 0x1007363c: 240, 0x101b2440: 108,
         0x1020100c: 136}
OWN = set(SIZES)

G_STRLEN, G_NEWA, G_STRCPY, G_THROW, G_MEMCPY, G_MEMSET, G_DELETE, G_NEW = (0x10238650, 0x10238668, 0x10238410, 0x10238680,
                                                                              0x102386b0, 0x10238560, 0x10238218, 0x102384b8)
REFPTR, CHANGE = 0x100026ac, 0x100027b0

ADDR = dict(initnul=0x10278bd2, ctornul=0x1029d294, vtblB=0x10278bb0, vtblLen=0x10278b74, dtorTV=0x10265680)
HOST_LAYOUT = [24, 16, 8, 8]

RETS_SELF = ("ctor_c", "dtor", "fill", "ctor0", "chars", "append")
HOST_TYPES = dict(ctor_c=[C.c_uint64, C.c_uint64], ctor0=[C.c_uint64], dtor=[C.c_uint64, C.c_int16],
                  init=[C.c_uint64, C.c_uint64, C.c_uint64], fill=[C.c_uint64, C.c_uint32, C.c_uint8],
                  resize=[C.c_uint64, C.c_uint32, C.c_uint8], chars=[C.c_uint64, C.c_uint64, C.c_uint32],
                  append=[C.c_uint64, C.c_uint64, C.c_uint32])


def csum(b):
    return sum(x * (i + 1) for i, x in enumerate(b)) & MASK


def cstr(e, a):
    out = bytearray()
    while True:
        b = e.read(a + len(out), 1)[0]
        if b == 0:
            return bytes(out)
        out.append(b)


def blk(e, size):
    n = max(size, 80) + 32
    return e.alloc(n, bytes(n)) + 16


def p32(v):
    return struct.pack(">I", v & MASK)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_strlen(e):
    L = e.cl
    n = len(cstr(e, e.r(3)))
    k = getattr(L, "nstr", 0)
    L.nstr = k + 1
    if k == 0:
        v = L.ret("strlen0") & MASK
        if v:
            n = v
    L.rec("strlen", (n,))
    e.set_r(3, n)


def st_strcpy(e):
    L = e.cl
    d, s = e.r(3), e.r(4)
    b = cstr(e, s)
    L.rec("strcpy", (len(b), csum(b)))
    e.write(d, b + b"\0")
    e.set_r(3, d)


def st_new(e):
    n = e.r(3)
    e.cl.rec("new", (n,))
    e.set_r(3, blk(e, n))


def st_newa(e):
    n = e.r(3)
    e.cl.rec("newarr", (n,))
    e.set_r(3, blk(e, n))


def st_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def st_refptr(e):
    rc, p = e.r(3), e.r(4)
    e.cl.rec("refptr", (int(p != 0), int(e.u32(rc - 4) == ADDR["vtblB"])))
    cnt = blk(e, 4)
    e.write(cnt, p32(1))
    e.write(rc, p32(p) + p32(cnt))


def st_throw(e):
    name, obj, dtor = cstr(e, e.r(3)), e.r(4), e.r(5)
    msg = cstr(e, e.u32(obj + 4))
    e.cl.rec("throw", (len(name), csum(name), int(e.u32(obj) == ADDR["vtblLen"]), len(msg), csum(msg), int(dtor == ADDR["dtorTV"])))


def st_memcpy(e):
    d, s, n = e.r(3), e.r(4), e.r(5)
    e.cl.rec("memcpy", (n, csum(e.read(s, n)) if n <= 4096 else 0))
    if n <= 4096:
        e.write(d, e.read(s, n))
    e.set_r(3, d)


def st_memset(e):
    d, c, n = e.r(3), e.r(4), e.r(5)
    e.cl.rec("memset", (c & 0xFF, n))
    if n <= 4096:
        e.write(d, bytes([c & 0xFF]) * n)
    e.set_r(3, d)


def st_change(e):
    L = e.cl
    me, n, keep = e.r(3), e.r(4), e.r(5) & 0xFF
    L.rec("change_size", (L.lab(me), n, keep))
    old = e.u32(me)
    oldlen, odat = e.u32(old), e.u32(old + 0xC)
    rep = blk(e, 16)
    dat = blk(e, n + 1)
    if keep:
        m = min(oldlen, n)
        if m:
            e.write(dat, e.read(odat, m))
    e.write(rep, p32(n) + p32(n) + p32(1) + p32(dat))
    e.write(me, p32(rep))


for _a, _f in ((G_STRLEN, st_strlen), (G_STRCPY, st_strcpy), (G_NEW, st_new), (G_NEWA, st_newa), (G_DELETE, st_delete),
               (REFPTR, st_refptr), (G_THROW, st_throw), (G_MEMCPY, st_memcpy), (G_MEMSET, st_memset), (CHANGE, st_change)):
    def _glue(e, _f=_f, _a=_a):
        if _a not in (REFPTR, CHANGE):
            e.write(e.r(1) + 0x14, p32(e.r(2)))      # the import glue saves the TOC where the caller reloads it from
        _f(e)
    STUBS.internal(_a, _glue)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def make_rep(self, rep):
        r = self.alloc(self.REP)
        d = self.alloc(96)
        self.write(d, rep["data"])
        self.words(r, [rep["len"], rep["cap"], rep["refs"]])
        self.set_ptr(r + self.DOFF, d)
        return r


class Ppc(Side):
    fmt = ">I"
    REP, DOFF = 16, 0xC

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(16)
        self.L = e.cl = CL.PpcLog({self.obj: 1}, case["queues"])
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

    def set_ptr(self, a, v):
        self.write(a, p32(v))

    def get_ptr(self, a):
        return self.rd_words(a, 1)[0]

    def log(self):
        return self.L.log

    def call(self, op, args):
        return T.call_ppc(self.e, FN[op], [a & MASK for a in args])[0] & MASK


class Host(Side):
    fmt = "<I"
    REP, DOFF = 24, 0x10

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_ssb_reset()
        self.obj = lib.tpw_ssb_alloc(16)
        CL.host_begin(lib, {1: self.obj}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ssb_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ssb_alloc(n)

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

    def call(self, op, args):
        f = getattr(self.lib, "tpw_ssb_" + op)
        return f(*args[:len(HOST_TYPES[op])])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ssb_addr.restype, lib.tpw_ssb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ssb_alloc.restype, lib.tpw_ssb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ssb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ssb_reset.argtypes = []
    for op, types in HOST_TYPES.items():
        f = getattr(lib, "tpw_ssb_" + op)
        f.restype, f.argtypes = (C.c_uint64 if op in RETS_SELF else None), types
    out = (C.c_int64 * 32)()
    n = lib.tpw_ssb_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.write(s.g("initnul"), bytes([case["initnul"]]))
    s.write(s.g("ctornul"), bytes([case["ctornul"]]))
    s.orig = 0
    if case.get("rep") is not None:
        s.orig = s.make_rep(case["rep"])
    s.set_ptr(s.obj, s.orig)
    s.buf = s.alloc(176)
    t = case.get("text", b"")
    s.write(s.buf, t + bytes(176 - len(t)))


def execute(s, case):
    op = case["op"]
    obj = 0 if case.get("null") else s.obj
    n = case.get("n", 0)
    args = {"ctor_c": [obj, s.buf, 0], "ctor0": [obj, 0], "dtor": [obj, case.get("flag", 0)], "init": [obj, s.buf, s.buf + n],
            "fill": [obj, n, case.get("c", 0)], "resize": [obj, n, case.get("c", 0)], "chars": [obj, s.buf, n],
            "append": [obj, s.buf, n]}[op]
    r = s.call(op, args)
    return int(r == obj) if op in RETS_SELF else None


def obs_rep(s, rep):
    if rep == 0:
        return None
    ln, cap, refs = s.rd_words(rep, 3)
    d = s.get_ptr(rep + s.DOFF)
    return (ln, cap, refs, s.read(d, 80) if d else None)


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "cur": obs_rep(s, s.get_ptr(s.obj)), "orig": obs_rep(s, s.orig)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rbytes(r, n):
    return bytes(r.randrange(256) for _ in range(n))


def mkrep(r, ln=None, refs=None):
    if ln is None:
        ln = r.choice([0, 1, 2, 3, 4, 5, r.randrange(0, 41)])
    return {"len": ln, "cap": ln + r.choice([0, 1, 3, 10]), "refs": r.choice([0, 1, 1, 2, 5]) if refs is None else refs,
            "data": rbytes(r, 80)}


def mk(r, op, rep=False, **kw):
    c = {"op": op, "queues": {"strlen0": [0]}, "initnul": r.randrange(256), "ctornul": r.randrange(256),
         "rep": mkrep(r) if rep else None}
    c.update(kw)
    return c


def text(r, n):
    return bytes(r.randrange(1, 256) for _ in range(n))


def ctor_c(r, n=None, strlen0=0):
    n = r.choice([0, 1, 2, 3, 4, 5, 7, 8, r.randrange(9, 61)]) if n is None else n
    return mk(r, "ctor_c", text=text(r, n), queues={"strlen0": [strlen0]})


def ctor0(r):
    return mk(r, "ctor0")


def dtor(r, refs=None, flag=None, null=False):
    if flag is None:
        flag = r.choice([0, 1, 0x7FFF, 0xFFFF, 0x8000, 0x10000, 0x17FFF, 0x80000001, r.getrandbits(32)])
    return mk(r, "dtor", rep=not null, flag=flag, null=null)


def dtor_refs(r, refs, flag):
    c = dtor(r, flag=flag)
    c["rep"]["refs"] = refs
    return c


def init(r, n=None):
    n = r.choice([0, 1, 2, 3, 4, 5, 8, 9, r.randrange(0, 71)]) if n is None else n
    return mk(r, "init", text=rbytes(r, n), n=n)


def craw(r):
    return r.choice([0, 0xFF, 0x80, 0x7F, 0x12345680, 0xABCDEF41, r.getrandbits(32), r.randrange(256)])


def fill(r, n=None):
    n = r.choice([0, 1, 2, 63, 64, r.randrange(0, 65)]) if n is None else n
    return mk(r, "fill", rep=True, n=n, c=craw(r))


def resize(r, n=None, ln=None):
    c = mk(r, "resize", c=craw(r))
    c["rep"] = mkrep(r, ln)
    c["n"] = r.choice([0, 1, c["rep"]["len"], c["rep"]["len"] + 1, c["rep"]["len"] - 1, r.randrange(0, 65)]) if n is None else n
    c["n"] = max(0, min(c["n"], 64))
    return c


def chars(r, n=None):
    n = r.choice([0, 1, 2, 3, 63, 64, r.randrange(0, 65)]) if n is None else n
    return mk(r, "chars", rep=True, n=n, text=rbytes(r, n))


def append(r, n=None):
    c = mk(r, "append", n=r.choice([0, 0, 1, 2, r.randrange(0, 41)]) if n is None else n)
    c["rep"] = mkrep(r, r.randrange(0, 41))
    c["text"] = rbytes(r, c["n"])
    return c


def pair_edges(a_fn, b_fn, extra, seed):
    r = random.Random(seed)
    for _ in range(3):
        yield a_fn(r)
        yield b_fn(r)
    for gen in extra:
        yield gen(r)


def edges_ctor():
    r = random.Random(11)
    for n in (0, 1, 2, 3, 4, 5, 7, 8, 60):
        yield ctor_c(r, n)
    yield ctor_c(r, 5, 0xFFFFFFFF)       # strlen answers npos: the length_error path (and init with n = 0xffffffff)
    yield ctor_c(r, 3, 7)                # strlen lies, init copies 7 bytes
    yield ctor_c(r, 12, 2)
    for _ in range(4):
        yield ctor0(r)


def rnd_ctor(r):
    return ctor0(r) if r.random() < 0.3 else ctor_c(r)


def edges_dtor():
    r = random.Random(12)
    yield dtor(r, null=True)
    for refs in (0, 1, 2, 3):
        for flag in (0, 1, 0xFFFF, 0x8000, 0x10000, 0x7FFF, 0xFFFF8000):
            yield dtor_refs(r, refs, flag)


def edges_init():
    r = random.Random(13)
    for n in (0, 1, 2, 3, 4, 5, 7, 8, 9, 70):
        yield init(r, n)


def edges_assign():
    r = random.Random(14)
    for n in (0, 1, 2, 63, 64):
        yield fill(r, n)
        yield chars(r, n)


def rnd_assign(r):
    return fill(r) if r.random() < 0.5 else chars(r)


def edges_resize():
    r = random.Random(15)
    for ln, n in ((0, 0), (0, 5), (5, 0), (5, 5), (5, 6), (6, 5), (40, 64), (64, 64), (1, 0), (0, 1)):
        yield resize(r, n, ln)


def edges_append():
    r = random.Random(16)
    for n in (0, 1, 2, 40):
        yield append(r, n)


TESTS = [
    FnTest("std::basic_string<char,std::char_traits<char>,std::allocator<char>>::basic_string", ppc, host, edge=edges_ctor, rnd=rnd_ctor),
    FnTest("std::basic_string<char,std::char_traits<char>,std::allocator<char>>::~basic_string", ppc, host, edge=edges_dtor, rnd=dtor),
    FnTest("std::basic_string<char,std::char_traits<char>,std::allocator<char>>::init", ppc, host, edge=edges_init, rnd=init),
    FnTest("std::basic_string<char,std::char_traits<char>,std::allocator<char>>::assign", ppc, host, edge=edges_assign, rnd=rnd_assign),
    FnTest("std::basic_string<char,std::char_traits<char>,std::allocator<char>>::resize", ppc, host, edge=edges_resize, rnd=resize),
    FnTest("std::basic_string<char,std::char_traits<char>,std::allocator<char>>::append", ppc, host, edge=edges_append, rnd=append),
]
