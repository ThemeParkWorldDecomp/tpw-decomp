"""Unit v5680, the CodeWarrior std::deque<errorthingy>, against libtpw_unit_std_deque_tear_down.dylib (tests/unit_std_deque_tear_down.cpp),
main app, src/game/MslSupport.cpp:
  std::deque<errorthingy>::tear_down 0x101f4a74, the copy constructor 0x101f4d14, destroy 0x101f584c

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory, big-endian, for the original; host
memory, little-endian, for the translation). The callees outside the unit are fakes on both sides that record their arguments: both
make_iterators (0x101f56f8, 0x101f53b4), the cursor copy (0x101f5d6c), operator+= (0x101f5adc), operator-- (0x101f4c84), reserve, resize,
push_back of the block table, operator new and delete. The table fakes hand out memory the case pre-allocated.

The world: one deque (block size, table capacity, count, first slot, records first/size) with random block contents; for the copy
constructor also a garbage destination, the block-size global 0x102b142c and the pad word 0x102b1430. What is compared: the event log, both
deques, the tables, the new block. destroy and tear_down write nothing (errorthingy has no destructor): the log is what they do.
No fused operations. Not tested: a block size of 0 (the original divides by it and then never ends).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_deque_tear_down"
MASK = 0xFFFFFFFF

TEAR, COPY, DESTROY = 0x101F4A74, 0x101F4D14, 0x101F584C
SIZES = {TEAR: 432, COPY: 664, DESTROY: 424}
OWN = set(SIZES)

MKIT_NC, MKIT_C = 0x101F56F8, 0x101F53B4
CUR_COPY, CUR_ADD, CUR_DEC = 0x101F5D6C, 0x101F5ADC, 0x101F4C84
RESERVE, RESIZE, PUSH_BACK = 0x101F50D4, 0x101F5044, 0x101F459C
OP_NEW, OP_DELETE = 0x102384B8, 0x10238218

ADDR = dict(BS=0x102B142C, FILL=0x102B1430)
HOST_LAYOUT = [4, 0x14, 0x18, 0x1C, 0x18, 0xC]

CUR = {}   # the plan of the case on the PowerPC side (the stubs are module level)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cur(e, L, c):
    cd = e.u32(c)
    tab = e.u32(cd + 0xC)
    return (L.lab(cd), e.u32(c + 4), ((e.u32(c + 8) - tab) & MASK) // 4)


def mkit(name):
    def f(e):
        L = e.cl
        out, sf, idx = e.r(3), e.r(4), e.r(5)
        L.rec(name, (L.lab(sf), idx & MASK))
        bs = e.u32(sf)
        cd = sf + 4
        cap, first, tab = e.u32(cd), e.u32(cd + 8), e.u32(cd + 0xC)
        b = idx // bs
        slot = tab + ((first + b) % cap) * 4
        blk = e.u32(slot)
        e.write(out, struct.pack(">6I", cd, b, slot, (blk + (idx % bs) * 8) & MASK, (blk + bs * 8) & MASK, idx))
    return f


def stub_cur_copy(e):
    L = e.cl
    dst, src = e.r(3), e.r(4)
    L.rec("CursorCopy", cur(e, L, src))
    e.write(dst, e.read(src, 12))


def stub_cur_add(e):
    L = e.cl
    c, count = e.r(3), e.r(4) & MASK
    L.rec("CursorAdd", cur(e, L, c) + (count,))
    cd = e.u32(c)
    w4 = (e.u32(c + 4) + count) & MASK
    cap, first, tab = e.u32(cd), e.u32(cd + 8), e.u32(cd + 0xC)
    e.write(c + 4, struct.pack(">I", w4))
    e.write(c + 8, struct.pack(">I", (tab + ((first + w4) % cap) * 4) & MASK))


def stub_cur_dec(e):
    L = e.cl
    c = e.r(3)
    L.rec("CursorDec", cur(e, L, c))
    cd = e.u32(c)
    cap, tab = e.u32(cd), e.u32(cd + 0xC)
    slot = e.u32(c + 8)
    if slot == tab:
        slot = tab + cap * 4
    e.write(c + 4, struct.pack(">I", (e.u32(c + 4) - 1) & MASK))
    e.write(c + 8, struct.pack(">I", (slot - 4) & MASK))


def stub_reserve(e):
    L = e.cl
    c, n = e.r(3), e.r(4)
    L.rec("reserve", (L.lab(c), n))
    e.write(c, struct.pack(">I", n))
    e.write(c + 8, struct.pack(">I", (L.ret("first") & MASK) % n))
    e.write(c + 12, struct.pack(">I", CUR["nt"]))


def stub_resize(e):
    L = e.cl
    c, n, fill = e.r(3), e.r(4), e.r(5)
    L.rec("resize", (L.lab(c), n, fill))
    e.write(c + 4, struct.pack(">I", n))
    cap, first, tab = e.u32(c), e.u32(c + 8), e.u32(c + 12)
    for i in range(n):
        e.write(tab + ((first + i) % cap) * 4, struct.pack(">I", fill))


def stub_push_back(e):
    L = e.cl
    c, vp = e.r(3), e.r(4)
    L.rec("push_back", (L.lab(c), L.lab(vp), e.u32(vp)))
    cap, count, first, tab = e.u32(c), e.u32(c + 4), e.u32(c + 8), e.u32(c + 12)
    if count < cap:
        e.write(tab + ((first + count) % cap) * 4, struct.pack(">I", e.u32(vp)))
        e.write(c + 4, struct.pack(">I", count + 1))


def stub_new(e):
    e.cl.rec("new", (e.r(3),))
    e.set_r(3, CUR["nb"])


def stub_delete(e):
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)),))


for _a, _f in ((MKIT_NC, mkit("make_iterator_nc")), (MKIT_C, mkit("make_iterator")), (CUR_COPY, stub_cur_copy), (CUR_ADD, stub_cur_add),
               (CUR_DEC, stub_cur_dec), (RESERVE, stub_reserve), (RESIZE, stub_resize), (PUSH_BACK, stub_push_back), (OP_NEW, stub_new),
               (OP_DELETE, stub_delete)):
    STUBS.internal(_a, _f)


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

    def make_plan(self, case):
        P = {"A": self.alloc(0x20), "B": self.alloc(0x20), "tab": self.alloc(4 * case["cap"] + 16)}
        P["blk"] = [self.alloc(case["bs"] * 8 + 8) for _ in range(case["count"])]
        P["nt"] = self.alloc(64)
        P["nb"] = self.alloc(0x400)
        self.P = P
        fixed = {P["A"]: 1, P["A"] + 4: 2, P["B"]: 3, P["B"] + 4: 4, self.g("FILL"): 5}
        for k, b in enumerate(P["blk"]):
            fixed[b] = 10 + k
        return fixed


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = self.make_plan(case)
        CUR.clear()
        CUR.update(self.P)
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def call_tear(self, a):
        T.call_ppc(self.e, TEAR, [a])

    def call_copy(self, a, b):
        T.call_ppc(self.e, COPY, [a, b])

    def call_destroy(self, a, first, last):
        T.call_ppc(self.e, DESTROY, [a] + first + last)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = self.make_plan(case)
        lib.tpw_edq_set(self.P["nt"], self.P["nb"])
        CL.host_begin(lib, {v: k for k, v in fixed.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_edq_addr(name.encode())

    high = 0

    def alloc(self, n):
        a = self.lib.tpw_edq_alloc(n)
        self.high = (a >> 32) << 32
        return a

    def full(self, a):
        return a if a >> 32 else a | self.high   # a global's address is whole; a world word gets the arena's upper half

    def read(self, a, n):
        return C.string_at(self.full(a), n)

    def write(self, a, data):
        C.memmove(self.full(a), data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_tear(self, a):
        self.lib.tpw_edq_tear(a)

    def call_copy(self, a, b):
        self.lib.tpw_edq_copy(a, b)

    def call_destroy(self, a, first, last):
        self.lib.tpw_edq_destroy(a, (C.c_uint32 * 6)(*first), (C.c_uint32 * 6)(*last))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_edq_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_edq_set.argtypes = [C.c_uint32, C.c_uint32]
    lib.tpw_edq_tear.argtypes = [C.c_uint32]
    lib.tpw_edq_copy.argtypes = [C.c_uint32, C.c_uint32]
    lib.tpw_edq_destroy.argtypes = [C.c_uint32, C.c_void_p, C.c_void_p]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    P = s.P
    D = P["B"] if case["op"] == "copy" else P["A"]
    bs, cap, count, first = case["bs"], case["cap"], case["count"], case["first"]
    s.words(s.g("BS"), [case["g"]])
    s.words(s.g("FILL"), [case["fillw"]])
    s.words(D, [bs, cap, count, first, P["tab"], case["mfirst"], case["msize"]])
    if case["op"] == "copy":
        s.words(P["A"], [0xDEADBEE0 + i for i in range(7)])
    for k in range(count):
        s.words(P["tab"] + 4 * ((first + k) % cap), [P["blk"][k]])
        s.words(P["blk"][k], case["data"][k])


def iter_words(s, a, index):
    bs = s.rd_word(a)
    cd = a + 4
    cap, first, tab = s.rd_words(cd, 4)[0], s.rd_words(cd, 4)[2], s.rd_words(cd, 4)[3]
    b = index // bs
    slot = tab + ((first + b) % cap) * 4
    blk = s.rd_word(slot)
    return [cd & MASK, b, slot & MASK, (blk + (index % bs) * 8) & MASK, (blk + bs * 8) & MASK, index]


def execute(s, case):
    P = s.P
    op = case["op"]
    if op == "tear":
        s.call_tear(P["A"])
    elif op == "copy":
        s.call_copy(P["A"], P["B"])
    else:
        b = iter_words(s, P["A"], case["bi"])
        e = iter_words(s, P["A"], case["ei"])
        if case["eqelem"]:
            e[3] = b[3]
        s.call_destroy(P["A"], b, e)


def observe(s, case):
    P = s.P
    tags = {P["A"] & MASK: "A", P["B"] & MASK: "B", P["tab"] & MASK: "tab", P["nt"] & MASK: "nt", P["nb"] & MASK: "nb"}
    for k, b in enumerate(P["blk"]):
        tags[b & MASK] = "b%d" % k

    def norm(ws):
        return [tags.get(w, w) for w in ws]

    return {"log": s.log(), "A": norm(s.rd_words(P["A"], 7)), "B": norm(s.rd_words(P["B"], 7)),
            "tab": norm(s.rd_words(P["tab"], case["cap"] + 2)), "nt": norm(s.rd_words(P["nt"], 8)),
            "nb": norm(s.rd_words(P["nb"], 64))}


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def mk(r, op, bs=None, cap=None, count=None, first=None, mfirst=None, msize=None, bi=None, ei=None, g=None, eqelem=None, qfirst=None):
    bs = r.randint(1, 6) if bs is None else bs
    cap = r.randint(2, 5) if cap is None else cap
    if count is None:
        count = r.randint(0 if op == "tear" else 1, cap)
    first = r.randrange(cap) if first is None else first
    mfirst = r.randrange(bs) if mfirst is None else mfirst
    total = count * bs
    if msize is None:
        msize = r.randint(0, max(0, total - mfirst))
    if bi is None:
        bi = r.randint(0, total)
    if ei is None:
        ei = r.randint(bi, total)
    return {"op": op, "bs": bs, "cap": cap, "count": count, "first": first, "mfirst": mfirst, "msize": msize, "bi": bi, "ei": ei,
            "g": r.randint(1, 12) if g is None else g, "fillw": r.choice([0, r.getrandbits(32)]),
            "eqelem": (r.random() < 0.1) if eqelem is None else eqelem,
            "data": [[r.getrandbits(32) for _ in range(bs * 2)] for _ in range(count)],
            "queues": {"first": [r.randrange(2) if qfirst is None else qfirst]}}


def tear_edges():
    r = random.Random(11)
    yield mk(r, "tear", count=0)
    yield mk(r, "tear", count=1, cap=2, first=0)
    yield mk(r, "tear", count=2, cap=2, first=1)
    yield mk(r, "tear", count=3, cap=3, first=2)
    yield mk(r, "tear", count=4, cap=5, first=3)
    yield mk(r, "tear", count=3, cap=4, first=0, msize=0)
    yield mk(r, "tear", count=2, cap=4, first=2, bs=1, mfirst=0, msize=2)


def copy_edges():
    r = random.Random(12)
    yield mk(r, "copy", msize=0)
    yield mk(r, "copy", count=1, cap=2, first=0, bs=4, mfirst=1, msize=2, g=8, qfirst=0)
    yield mk(r, "copy", count=1, cap=2, first=0, bs=4, mfirst=1, msize=2, g=8, qfirst=1)
    yield mk(r, "copy", count=3, cap=3, first=1, bs=2, mfirst=0, msize=6, g=3, qfirst=1)
    yield mk(r, "copy", count=3, cap=3, first=2, bs=2, mfirst=1, msize=5, g=64, qfirst=0)
    yield mk(r, "copy", count=4, cap=5, first=4, bs=3, mfirst=2, msize=10, g=4, qfirst=1)
    yield mk(r, "copy", count=2, cap=2, first=1, bs=1, mfirst=0, msize=2, g=1, qfirst=1)
    yield mk(r, "copy", count=1, cap=2, first=1, bs=5, mfirst=0, msize=5, g=2, qfirst=0)


def destroy_edges():
    r = random.Random(13)
    yield mk(r, "destroy", count=2, bs=3, bi=2, ei=2, eqelem=False)
    yield mk(r, "destroy", count=2, bs=3, bi=1, ei=3, eqelem=True)
    yield mk(r, "destroy", count=2, bs=3, cap=3, first=2, bi=0, ei=2, eqelem=False)
    yield mk(r, "destroy", count=2, bs=3, cap=3, first=2, bi=1, ei=3, eqelem=False)
    yield mk(r, "destroy", count=3, bs=3, cap=3, first=2, bi=1, ei=7, eqelem=False)
    yield mk(r, "destroy", count=3, bs=3, cap=3, first=1, bi=3, ei=9, eqelem=False)
    yield mk(r, "destroy", count=4, bs=2, cap=4, first=3, bi=1, ei=8, eqelem=False)
    yield mk(r, "destroy", count=4, bs=2, cap=5, first=4, bi=0, ei=7, eqelem=False)
    yield mk(r, "destroy", count=1, bs=1, cap=2, first=1, bi=0, ei=1, eqelem=False)


TESTS = [
    FnTest("std::deque<errorthingy,std::allocator<errorthingy>>::tear_down", ppc, host, edge=tear_edges, rnd=lambda r: mk(r, "tear")),
    FnTest("std::deque<errorthingy,std::allocator<errorthingy>>::deque", ppc, host, edge=copy_edges, rnd=lambda r: mk(r, "copy")),
    FnTest("std::deque<errorthingy,std::allocator<errorthingy>>::destroy", ppc, host, edge=destroy_edges, rnd=lambda r: mk(r, "destroy")),
]
