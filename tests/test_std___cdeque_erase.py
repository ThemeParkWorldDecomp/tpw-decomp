"""Unit v5149, the const void* deque (std::__cdeque): erase, resize and push_back, against libtpw_unit_std___cdeque_erase.dylib
(tests/unit_std___cdeque_erase.cpp), main app ("SimTheme Park"), src/std___cdeque_erase.cpp:
  erase 0x101f3704, resize 0x101f2a10, push_back 0x101f459c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit are fakes on both
sides: insert, the iterator copy and +=, strlen, strcpy, operator new / new[] / delete and __throw, each recording what the real one reads.

The world of a case: a deque (capacity, size, start, block of 64 random words), the min-capacity global, the value. What is compared:
the answer (erase: the iterator it gives back), the four words of the deque (the block as "same as before" or its words), the block's
words, the event log.

No fused operations. Not tested: a size above capacity in push_back (the copy would read past the block), operator new answering
NULL (the runtime never does).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___cdeque_erase"
MASK = 0xFFFFFFFF

FN_ERASE, FN_RESIZE, FN_PUSH = 0x101F3704, 0x101F2A10, 0x101F459C
SIZES = {FN_ERASE: 856, FN_RESIZE: 396, FN_PUSH: 828}
OWN = set(SIZES)

INSERT, ICOPY, IADD = 0x101F3B10, 0x101F2DD8, 0x101F2BEC
G_STRLEN, G_NEWARR, G_NEW, G_DELETE, G_DELARR, G_STRCPY, G_THROW, G_ENDCATCH = (
    0x10238650, 0x10238668, 0x102384B8, 0x10238218, 0x10238638, 0x10238410, 0x10238680, 0x10238698)

ADDR = dict(gmin=0x102B1420, vtblB=0x10278BB0, vtblC=0x10278BC0, vtblLen=0x10278B74, dtorTV=0x10265680)
HOST_LAYOUT = [0, 4, 8, 12, 16]

NOTE = [0]


def fnv(b):
    h = 0x811C9DC5
    for c in b:
        h = ((h ^ c) * 0x01000193) & MASK
    return h


def readc(e, a):
    out = b""
    while len(out) < 300:
        c = e.read(a + len(out), 1)
        if c == b"\0":
            break
        out += c
    return out


def poff(e, deq, ptr):
    return ((ptr - e.u32(deq + 12)) // 4) & MASK


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def save_toc(e):
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))      # what the glue does before it jumps


def stub_insert(e):
    deq, idx, ptr, count, valp = e.r(4), e.r(5), e.r(6), e.r(7), e.r(8)
    this = e.r(3)
    e.cl.rec("Insert", (e.u32(this), e.u32(this + 4), e.u32(this + 8), 1 if deq == this else 0, idx, poff(e, deq, ptr), count,
                        e.u32(valp)))


def stub_icopy(e):
    dest, src = e.r(3), e.r(4)
    w = [e.u32(src + 4 * i) for i in range(3)]
    e.cl.rec("IterCopy", (w[1], poff(e, w[0], w[2])))
    e.write(dest, struct.pack(">3I", *w))
    e.set_r(3, dest)


def stub_iadd(e):
    it, n = e.r(3), e.r(4)
    deq, idx, ptr = [e.u32(it + 4 * i) for i in range(3)]
    e.cl.rec("IterAdd", (idx, poff(e, deq, ptr), n & MASK))
    cap, buf = e.u32(deq), e.u32(deq + 12)
    off = (ptr - buf) // 4 + n
    if off >= cap:
        off -= cap
    e.write(it + 4, struct.pack(">II", (idx + n) & MASK, (buf + 4 * off) & MASK))
    e.set_r(3, it)


def stub_strlen(e):
    save_toc(e)
    b = readc(e, e.r(3))
    e.cl.rec("strlen", (len(b), fnv(b)))
    e.set_r(3, len(b))


def stub_strcpy(e):
    save_toc(e)
    dest, b = e.r(3), readc(e, e.r(4))
    e.cl.rec("strcpy", (len(b), fnv(b)))
    e.write(dest, b + b"\0")
    e.set_r(3, dest)


def stub_new(e):
    save_toc(e)
    size = e.r(3)
    e.cl.rec("new", (size,))
    e.set_r(3, e.alloc(size or 4))


def stub_newarr(e):
    save_toc(e)
    size = e.r(3)
    e.cl.rec("newarr", (size,))
    e.set_r(3, e.alloc(size or 4))


def stub_delete(e):
    save_toc(e)
    e.cl.rec("delete", (1 if e.r(3) == NOTE[0] else 0,))


def stub_delarr(e):
    save_toc(e)
    e.cl.rec("deletearr", (1 if e.r(3) == NOTE[0] else 0,))


def stub_throw(e):
    save_toc(e)
    typ, obj, dtor = e.r(3), e.r(4), e.r(5)
    vt = e.u32(obj)
    vtbl = {ADDR["vtblLen"]: 1, ADDR["vtblB"]: 2, ADDR["vtblC"]: 3}.get(vt, 0)
    p, c = e.u32(obj + 4), e.u32(obj + 8)
    count = e.u32(c) if c else MASK
    e.cl.rec("throw", (fnv(readc(e, typ)), vtbl, fnv(readc(e, p)), count, 1 if dtor == ADDR["dtorTV"] else 0))


def stub_endcatch(e):
    save_toc(e)
    e.cl.rec("endcatch", ())


STUBS.internal(INSERT, stub_insert)
STUBS.internal(ICOPY, stub_icopy)
STUBS.internal(IADD, stub_iadd)
STUBS.internal(G_STRLEN, stub_strlen)
STUBS.internal(G_STRCPY, stub_strcpy)
STUBS.internal(G_NEW, stub_new)
STUBS.internal(G_NEWARR, stub_newarr)
STUBS.internal(G_DELETE, stub_delete)
STUBS.internal(G_DELARR, stub_delarr)
STUBS.internal(G_THROW, stub_throw)
STUBS.internal(G_ENDCATCH, stub_endcatch)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def off(self, a):
        """The pointer word of an address: the host's words are offsets from the arena base."""
        return (a - self.base) & MASK if a else 0


class Ppc(Side):
    fmt = ">I"
    base = 0

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    def note(self, p):
        NOTE[0] = p

    def call_erase(self, ret, d, f, l):
        T.call_ppc(self.e, FN_ERASE, [ret, d, f[0], f[1], f[2], l[0], l[1], l[2]])

    def call_resize(self, d, n, v):
        T.call_ppc(self.e, FN_RESIZE, [d, n, v])

    def call_push(self, d, vp, v):
        T.call_ppc(self.e, FN_PUSH, [d, vp])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    @property
    def base(self):
        return self.lib.tpw_cdq_base()      # read after the first allocation: the arena may start lazily

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cdq_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cdq_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def note(self, p):
        self.lib.tpw_cdq_note(p)

    def call_erase(self, ret, d, f, l):
        self.lib.tpw_cdq_erase(d, f[0], f[1], f[2], l[0], l[1], l[2], ret)

    def call_resize(self, d, n, v):
        self.lib.tpw_cdq_resize(d, n, v)

    def call_push(self, d, vp, v):
        self.lib.tpw_cdq_push_back(d, v)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cdq_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "note").argtypes = [C.c_uint64]
    lib.tpw_cdq_base.restype = C.c_uint64
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cdq_erase.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint64, C.c_uint64]
    lib.tpw_cdq_resize.argtypes = [C.c_uint64, C.c_uint32, C.c_uint32]
    lib.tpw_cdq_push_back.argtypes = [C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.words(s.g("gmin"), [case["g"]])
    buf = s.alloc(256)
    s.words(buf, case["buf"])
    d = s.alloc(16)
    s.words(d, [case["cap"], case["size"], case["start"], s.off(buf)])
    s.d, s.buf = d, buf
    s.note(buf)


def iter_at(s, case, idx):
    cap, start = case["cap"], case["start"]
    pos = start + idx
    if cap and pos >= cap:
        pos -= cap
    return (s.d, idx, s.buf + 4 * pos)


def execute(s, case):
    op = case["op"]
    if op == "erase":
        ret = s.alloc(12)
        s.call_erase(ret, s.d, iter_at(s, case, case["a"]), iter_at(s, case, case["b"]))
        w = s.rd_words(ret, 3)
        return (w[0] == s.off(s.d), w[1], (w[2] - s.off(s.buf)) & MASK)
    if op == "resize":
        s.call_resize(s.d, case["n"], case["value"])
        return None
    vp = s.alloc(4)
    s.words(vp, [case["value"]])
    s.call_push(s.d, vp, case["value"])
    return None


def observe(s, case, answer):
    d = s.rd_words(s.d, 4)
    same = d[3] == s.off(s.buf)
    n = 64 if same else min(d[0], 64)
    return {"answer": answer, "log": s.log(), "hdr": d[:3], "bufsame": same, "buf": s.rd_words(s.base + d[3], n)}


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


def world(r, op, cap, size, start, g=None):
    return {"op": op, "queues": {}, "cap": cap, "size": size, "start": start, "g": r.choice([0, 1, 1, 2, 3, 7]) if g is None else g,
            "buf": [r.getrandbits(32) for _ in range(64)], "value": r.choice([0, r.randrange(1, 0x10000), r.randrange(1, 0x10000)])}   # a pointer word: an offset inside the arena


def rand_world(r, op, caps=(1, 2, 3, 4, 5, 8, 13, 16)):
    cap = r.choice(caps)
    size = r.randint(0, cap)
    return world(r, op, cap, size, r.randrange(cap) if cap else 0)


def erase_case(r, cap, size, start, a, b):
    c = world(r, "erase", cap, size, start)
    c["a"], c["b"] = a, b
    return c


def erase_edges():
    r = random.Random(5149)
    for cap in (1, 2, 3, 4):
        for size in range(cap + 1):
            for start in range(cap):
                for a in range(size + 1):
                    for b in range(a, size + 1):
                        yield erase_case(r, cap, size, start, a, b)


def erase_random(r):
    c = rand_world(r, "erase")
    a = r.randint(0, c["size"])
    c["a"], c["b"] = a, r.randint(a, c["size"])
    return c


def resize_edges():
    r = random.Random(5150)
    for cap in (1, 2, 3, 4):
        for size in range(cap + 1):
            for start in range(cap):
                for n in range(size + 3):
                    c = world(r, "resize", cap, size, start)
                    c["n"] = n
                    yield c


def resize_random(r):
    c = rand_world(r, "resize")
    c["n"] = r.randint(0, c["size"] + 4)
    return c


def push_edges():
    r = random.Random(5151)
    for cap in (0, 1, 2, 3, 4):
        for size in range(cap + 1):
            for start in range(max(cap, 1)):
                yield world(r, "push", cap, size, start, 1)
    for g in (0, 1, 2, 3, 6, 20):
        for cap in (0, 1, 2, 5, 16):
            yield world(r, "push", cap, cap, r.randrange(cap) if cap else 0, g)
    for start in range(1, 9):                       # size 0x3fffffff: the length error, then the store goes on
        yield world(r, "push", 0x40000000, 0x3FFFFFFF, start, 1)


def push_random(r):
    cap = r.choice([0, 1, 2, 3, 4, 5, 8, 13, 16])
    size = cap if r.random() < 0.6 else r.randint(0, cap)
    return world(r, "push", cap, size, r.randrange(cap) if cap else 0, r.choice([0, 1, 2, 3, 4, 8, 20]))


TESTS = [
    FnTest("std::__cdeque<const_void*,std::allocator<const_void*>>::erase", ppc, host, edge=erase_edges, rnd=erase_random),
    FnTest("std::__cdeque<const_void*,std::allocator<const_void*>>::resize", ppc, host, edge=resize_edges, rnd=resize_random),
    FnTest("std::__cdeque<const_void*,std::allocator<const_void*>>::push_back", ppc, host, edge=push_edges, rnd=push_random),
]
