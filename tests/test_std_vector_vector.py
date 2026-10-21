"""Unit v5181, std::vector<Argument> copy constructor and push_back, against libtpw_unit_std_vector_vector.dylib
(tests/unit_std_vector_vector.cpp), main app ("SimTheme Park"), src/game/MslSupport.cpp:
  std::vector<Argument,std::allocator<Argument>>::vector 0x10227200, std::vector<Argument,std::allocator<Argument>>::push_back 0x10201478

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory, big-endian, and host memory,
little-endian). The callees outside the unit are fakes on both sides that record their arguments: operator new, new[], delete, strlen,
strcpy, __throw (it returns), the counted-pointer constructor, Argument's copy constructor (copies its three words) and
std::__destroy<Argument*> (first pointer label, element count). Strings are logged as (length, byte sum). The placement new runs as the
original.

The world: a vector (capacity, size, data pointer), a data block of 8 Arguments with 16 bytes of padding either side (random words), a
source vector for the copy constructor, and the Argument to push. Compared: the answer, the vector, the class of the data pointer (null,
old block, new block), the bytes of every block including a newly allocated one, and the event log.

No fused operations. Not tested: sizes between 0x15555558 and the wrap-around point that would write outside any block (the throw path
is tested with sizes 0x15555555..0x15555557, whose size * 12 wraps to a slot inside the padded block).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_vector_vector"
MASK = 0xFFFFFFFF

FN_CTOR, FN_PUSH = 0x10227200, 0x10201478
SIZES = {FN_CTOR: 304, FN_PUSH: 616}
OWN = set(SIZES)

NEW, NEWA, DELETE, STRLEN, STRCPY, THROW = 0x102384b8, 0x10238668, 0x10238218, 0x10238650, 0x10238410, 0x10238680
RCCTOR, COPY, DESTROY_A, DESTROY_B, PLACEMENT_NEW = 0x100026ac, 0x10201878, 0x102013c0, 0x10201744, 0x10201850
HOST_LAYOUT = [0, 4, 8, 16]

BLOCK = 128          # a data block: 16 bytes of padding, 8 Arguments (96), 16 bytes of padding
PAD = 16

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.add(PLACEMENT_NEW)


def cstr(e, a):
    s = e.cstr(a)
    return s.encode("latin1") if isinstance(s, str) else bytes(s)


def stub_new(e):
    n = e.r(3) & MASK
    e.cl.rec("new", (n,))
    e.set_r(3, e.alloc(max(n, 4)))


def stub_newa(e):
    n = e.r(3) & MASK
    e.cl.rec("new[]", (n,))
    e.set_r(3, e.alloc(max(n, 4)))


def stub_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def stub_strlen(e):
    s = cstr(e, e.r(3))
    e.cl.rec("strlen", (len(s), sum(s)))
    e.set_r(3, len(s))


def stub_strcpy(e):
    d, a = e.r(3), e.r(4)
    s = cstr(e, a)
    e.cl.rec("strcpy", (len(s), sum(s)))
    e.write(d, s + b"\0")
    e.set_r(3, d)


def stub_throw(e):
    t = cstr(e, e.r(3))
    obj = e.r(4)
    msg = cstr(e, e.u32(obj + 4))
    e.cl.rec("throw", (len(t), sum(t), e.cl.lab(e.r(5)), e.cl.lab(e.u32(obj)), len(msg), sum(msg)))


def stub_rcctor(e):
    self_, p = e.r(3), e.r(4)
    e.cl.rec("rcctor", (e.cl.lab(p),))
    e.write(self_, struct.pack(">I", p & MASK))


def stub_copy(e):
    d, s = e.r(3), e.r(4)
    data = e.read(s, 12)
    e.cl.rec("copy", struct.unpack(">3I", data))
    e.write(d, data)


def stub_destroy(e):
    a, b = e.r(3) & MASK, e.r(4) & MASK
    e.cl.rec("destroy", (e.cl.lab(a), ((b - a) & MASK) // 12))


def glue(fn):
    """The import glue saves the TOC register in the caller's frame (0x14(r1)); the caller reloads r2 from there."""
    def wrapped(e):
        e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapped


for addr, fn in ((NEW, glue(stub_new)), (NEWA, glue(stub_newa)), (DELETE, glue(stub_delete)), (STRLEN, glue(stub_strlen)),
                 (STRCPY, glue(stub_strcpy)), (THROW, glue(stub_throw)), (RCCTOR, stub_rcctor), (COPY, stub_copy),
                 (DESTROY_A, stub_destroy), (DESTROY_B, stub_destroy)):
    STUBS.internal(addr, fn)


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def make_blocks(self):
        # labels: 1 vector, 2 source vector, 3 argument, 4 data block, 5 source data block
        self.V, self.S, self.A = self.alloc(16), self.alloc(16), self.alloc(16)
        self.D, self.SD = self.alloc(BLOCK), self.alloc(BLOCK)


class Ppc(Side):
    fmt = ">I"
    ptr = 4

    def __init__(self, e, case):
        self.e = e
        self.make_blocks()
        self.L = e.cl = CL.PpcLog({self.V: 1, self.S: 2, self.A: 3, self.D + PAD: 4, self.SD + PAD: 5,
                                   0x10265680: 6, 0x10278b74: 7}, case["queues"])
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

    def ctor(self):
        r3, _, _ = T.call_ppc(self.e, FN_CTOR, [self.V, self.S])
        return (r3 & MASK) == self.V

    def push(self):
        T.call_ppc(self.e, FN_PUSH, [self.V, self.A])
        return None


class Host(Side):
    fmt = "<I"
    ptr = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_blocks()
        CL.host_begin(lib, {1: self.V, 2: self.S, 3: self.A, 4: self.D + PAD, 5: self.SD + PAD,
                            6: lib.tpw_svv_addr(b"dtorTV"), 7: lib.tpw_svv_addr(b"lenVtbl")}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_svv_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ctor(self):
        return self.lib.tpw_svv_ctor(self.V, self.S) == self.V

    def push(self):
        self.lib.tpw_svv_push_back(self.V, self.A)
        return None


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_svv_addr.restype, lib.tpw_svv_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_svv_alloc.restype, lib.tpw_svv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_svv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_svv_ctor.restype, lib.tpw_svv_ctor.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64]
    lib.tpw_svv_push_back.restype, lib.tpw_svv_push_back.argtypes = None, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_svv_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def put_vector(s, v, cap, size, data):
    s.words(v, [cap, size])
    s.write(v + 8, (data & ((1 << (8 * s.ptr)) - 1)).to_bytes(s.ptr, "big" if s.fmt[0] == ">" else "little"))


def get_ptr(s, v):
    return int.from_bytes(s.read(v + 8, s.ptr), "big" if s.fmt[0] == ">" else "little")


def build(s, case):
    s.words(s.D, case["dwords"])
    s.words(s.SD, case["swords"])
    s.words(s.A, case["arg"])
    if case["op"] == "push":
        put_vector(s, s.V, case["cap"], case["size"], s.D + PAD if case["data"] else 0)
        put_vector(s, s.S, 0x1234, 0x77, 0)
    else:
        put_vector(s, s.V, 0xdead, 0xbeef, 0x4444)
        put_vector(s, s.S, case["cap"], case["size"], s.SD + PAD if case["data"] else 0)


def execute(s, case):
    return s.ctor() if case["op"] == "ctor" else s.push()


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log(), "V": s.rd_words(s.V, 2), "S": s.rd_words(s.S, 2), "A": s.rd_words(s.A, 3),
           "D": s.rd_words(s.D, BLOCK // 4), "SD": s.rd_words(s.SD, BLOCK // 4)}
    p = get_ptr(s, s.V)
    cap = out["V"][0]
    if p == 0:
        out["cls"] = "null"
    elif p == s.D + PAD:
        out["cls"] = "old"
    elif p == s.SD + PAD:
        out["cls"] = "src"
    else:
        out["cls"] = "new"
        out["new"] = s.rd_words(p, 3 * min(cap, 16))
    out["sptr"] = "src" if get_ptr(s, s.S) == s.SD + PAD else ("null" if get_ptr(s, s.S) == 0 else "other")
    return out


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


class Case(dict):
    def __repr__(self):
        return "%s cap=%#x size=%#x data=%s" % (self["op"], self["cap"], self["size"], self["data"])


def base(r, op, cap, size, data=True):
    return Case({"op": op, "queues": {}, "cap": cap, "size": size, "data": data,
            "dwords": [r.getrandbits(32) for _ in range(BLOCK // 4)],
            "swords": [r.getrandbits(32) for _ in range(BLOCK // 4)],
            "arg": [r.getrandbits(32) for _ in range(3)]})


def ctor_case(r, size=None):
    size = r.randrange(0, 9) if size is None else size
    return base(r, "ctor", r.randrange(0, 12), size, data=size > 0 or r.random() < 0.5)


def ctor_edges():
    r = random.Random(11)
    for size in (0, 1, 2, 3, 7, 8):
        yield ctor_case(r, size)


def push_case(r, cap=None, size=None):
    if cap is None:
        size = r.randrange(0, 8)
        cap = size + r.choice([0, 0, 1, 2, 5]) if size else r.choice([0, 0, 1, 4])
        if size > cap or cap > 7 and size == cap:
            cap = max(size, 1) if size == cap else cap
        cap = min(cap, 7 if size < cap else cap)
    return base(r, "push", cap, size, data=cap > 0 or size > 0 or r.random() < 0.3)


def push_edges():
    r = random.Random(12)
    # grow from empty, grow at several sizes, fast path, size above capacity, the throw boundary
    for cap, size, data in ((0, 0, False), (1, 1, True), (2, 2, True), (3, 3, True), (4, 4, True), (4, 0, True), (4, 3, True),
                            (8, 7, True), (4, 5, True), (2, 3, True), (3, 5, True), (1, 0, True), (0, 0, True),
                            (0x15555555, 0x15555554, True), (0x15555556, 0x15555555, True), (0x15555557, 0x15555556, True),
                            (0x15555558, 0x15555557, True), (0x20000000, 0x15555555, True)):
        yield base(r, "push", cap, size, data)


def push_random(r):
    size = r.randrange(0, 8)
    kind = r.random()
    if kind < 0.5:
        cap = size
    elif kind < 0.8:
        cap = size + r.randrange(1, 4)
    else:
        cap = max(1, (size + 1) // 2 + r.randrange(0, 2))
        if 2 * cap < size + 1:
            cap = size
    cap = min(cap, 8)
    if size > 2 * cap - 1 and cap > 0 and size >= cap:
        size = min(size, 2 * cap - 1) if cap > size else size
    if size >= cap and 2 * cap < size + 1:
        cap = size
    return base(r, "push", cap, size, data=cap > 0 or size > 0)


TESTS = [
    FnTest("std::vector<Argument,std::allocator<Argument>>::vector", ppc, host, edge=ctor_edges, rnd=ctor_case),
    FnTest("std::vector<Argument,std::allocator<Argument>>::push_back", ppc, host, edge=push_edges, rnd=push_random),
]
