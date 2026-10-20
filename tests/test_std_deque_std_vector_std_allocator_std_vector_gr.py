"""Unit v5976, std::deque<std::vector<Argument>>::grow_buf_back, ::destroy and ::~deque, against
libtpw_unit_std_deque_std_vector_std_allocator_std_vector_gr.dylib (tests/unit_std_deque_std_vector_std_allocator_std_vector_gr.cpp),
main app ("SimTheme Park"), src/std_deque_std_vector_std_allocator_std_vector_gr.cpp:
  grow_buf_back 0x102287e4, destroy 0x10228fd4, ~deque 0x10213b80

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit are fakes on both
sides that record the same events: operator new (it allocates and registers a block) and delete, the block table's reserve, push_back
(adds one to its count while below the capacity) and do_assign, tear_down, the iterator copy and += (a move by n slots that wraps once at
the end of the table) and __destroy (first block address and byte length). Pointers are logged as (region number, offset) over the blocks
the case registered or operator new made, so the two sides' different addresses compare equal.

The world of a case: the deque (block size, the block table words, first, size), for grow_buf_back the spare table and the added counter
and the fill global, for destroy the block table with its blocks and the two iterators. What is compared: the answer, the event log, the
words of every registered block (the deque, the tables, the new blocks) with pointers made region numbers, and the fill global.

No fused operations. Not tested: the exception handler of grow_buf_back (it only runs if a callee throws, and operator new never fails);
an empty table with start 0 (grow_buf_back then writes far outside the table); a block size of 0 (division by zero, undefined);
a last index below the first; this + 4 equal to 0 in ~deque.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_deque_std_vector_std_allocator_std_vector_gr"
MASK = 0xFFFFFFFF

GROW, DESTROY, DTOR = 0x102287e4, 0x10228fd4, 0x10213b80
SIZES = {GROW: 844, DESTROY: 424, DTOR: 128}
OWN = set(SIZES)

OP_NEW, OP_DELETE = 0x102384b8, 0x10238218
RESERVE, PUSH_BACK, DO_ASSIGN, TEAR_DOWN = 0x101f2730, 0x101f459c, 0x10229ae4, 0x1022462c
ITER_COPY, ITER_ADD, DESTROY_RANGE = 0x102295ec, 0x10229498, 0x1022935c

ADDR = dict(fill=0x102b2088)
HOST_LAYOUT = [0x1c, 0x10, 0x18, 0xc, 0x14]

PREGS = []          # the PowerPC side's registered blocks (base, size), in registration order


def s32(v):
    return T.s32(v)


def desc(regs, a):
    a &= MASK
    for i, (b, sz) in enumerate(regs):
        if b <= a < b + sz:
            return (i + 1, a - b)
    return (0, a)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def D(e, a):
    return desc(PREGS, a)


def stub_new(e):
    size = e.r(3)
    a = e.alloc(size)
    PREGS.append((a & MASK, size))
    e.cl.rec("new", (size,))
    e.set_r(3, a & MASK)


def stub_delete(e):
    e.cl.rec("delete", D(e, e.r(3)))


def stub_reserve(e):
    c = e.r(3)
    w = [e.u32(c + 4 * i) for i in range(4)]
    e.cl.rec("reserve", D(e, c) + (e.r(4), w[0], w[1], w[2]) + D(e, w[3]))


def stub_push_back(e):
    c = e.r(3)
    w = [e.u32(c + 4 * i) for i in range(4)]
    e.cl.rec("push_back", D(e, c) + (w[0], w[1], w[2]) + D(e, w[3]) + (e.u32(e.r(4)),))
    if w[1] < w[0]:
        e.write(c + 4, struct.pack(">I", w[1] + 1))


def stub_do_assign(e):
    r = [e.r(i) for i in range(3, 10)]
    e.cl.rec("do_assign", D(e, r[0]) + D(e, r[1]) + (r[2],) + D(e, r[3]) + D(e, r[4]) + (r[5],) + D(e, r[6]))


def stub_tear_down(e):
    e.cl.rec("tear_down", D(e, e.r(3)))


def stub_iter_copy(e):
    dest, src = e.r(3), e.r(4)
    w = [e.u32(src + 4 * i) for i in range(3)]
    e.cl.rec("iter_copy", D(e, w[0]) + (w[1],) + D(e, w[2]))
    e.write(dest, struct.pack(">3I", *w))


def stub_iter_add(e):
    it, n = e.r(3), s32(e.r(4))
    q, idx, ptr = [e.u32(it + 4 * i) for i in range(3)]
    e.cl.rec("iter_add", D(e, q) + (idx,) + D(e, ptr) + (n,))
    idx = (idx + n) & MASK
    ptr = (ptr + 4 * n) & MASK
    cap, table = e.u32(q), e.u32(q + 12)
    if ptr >= (table + cap * 4) & MASK:
        ptr = (ptr - cap * 4) & MASK
    e.write(it, struct.pack(">3I", q, idx, ptr))


def stub_destroy_range(e):
    first, last = e.r(3), e.r(4)
    e.cl.rec("destroy", D(e, first) + ((last - first) & MASK,))


for addr, fn in ((OP_NEW, stub_new), (OP_DELETE, stub_delete), (RESERVE, stub_reserve), (PUSH_BACK, stub_push_back),
                 (DO_ASSIGN, stub_do_assign), (TEAR_DOWN, stub_tear_down), (ITER_COPY, stub_iter_copy), (ITER_ADD, stub_iter_add),
                 (DESTROY_RANGE, stub_destroy_range)):
    STUBS.internal(addr, fn)


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

    def norm(self, w):
        i, off = desc(self.regions(), w)
        return ("p", i, off) if i else w


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        PREGS.clear()
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        a = self.e.alloc(n)
        PREGS.append((a & MASK, n))
        return a

    def regions(self):
        return list(PREGS)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def grow(self, deque, frm, spare, added, to):
        r3, _, _ = T.call_ppc(self.e, GROW, [deque, frm & MASK, spare, added, to & MASK])
        return r3 & 0xFF

    def destroy(self, deque, a, b):
        T.call_ppc(self.e, DESTROY, [deque] + list(a) + list(b))
        return 0

    def dtor(self, deque, flag):
        r3, _, _ = T.call_ppc(self.e, DTOR, [deque, flag & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        self.high = 0
        bind(lib)
        lib.tpw_argdq_reset()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_argdq_addr(name.encode())

    def alloc(self, n):
        a = self.lib.tpw_argdq_alloc(n)
        self.high = a & ~MASK
        return a

    def regions(self):
        out = []
        for i in range(self.lib.tpw_argdq_regcount()):
            v = self.lib.tpw_argdq_region(i)
            out.append((v & MASK, v >> 32))
        return out

    def read(self, a, n):
        if a <= MASK:
            a |= self.high
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def grow(self, deque, frm, spare, added, to):
        return self.lib.tpw_argdq_grow(deque, frm & MASK, spare, added, to & MASK) & 0xFF

    def destroy(self, deque, a, b):
        self.lib.tpw_argdq_destroy(deque, (C.c_uint32 * 6)(*[w & MASK for w in a]), (C.c_uint32 * 6)(*[w & MASK for w in b]))
        return 0

    def dtor(self, deque, flag):
        return self.lib.tpw_argdq_dtor(deque, s16(flag)) & MASK


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_argdq_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "region").restype, getattr(lib, p + "region").argtypes = C.c_uint64, [C.c_int32]
    getattr(lib, p + "regcount").restype = C.c_int32
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "grow").restype = C.c_uint32
    getattr(lib, p + "grow").argtypes = [C.c_uint64, C.c_uint32, C.c_uint64, C.c_uint64, C.c_uint32]
    getattr(lib, p + "destroy").restype = None
    getattr(lib, p + "destroy").argtypes = [C.c_uint64, C.c_void_p, C.c_void_p]
    getattr(lib, p + "dtor").restype = C.c_uint32
    getattr(lib, p + "dtor").argtypes = [C.c_uint64, C.c_int16]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def table(s, cap, extra=8):
    """A block table of cap + extra words with plain filler words."""
    return s.alloc((cap + extra) * 4)


def build(s, case):
    c = case
    a = {}
    s.words(s.g("fill"), [c["fill"]])
    if c["op"] == "grow":
        deq = s.alloc(0x20)
        tab = table(s, c["cap"])
        s.words(tab, c["tab_words"])
        spare = s.alloc(16)
        stab = table(s, c["scap"])
        s.words(stab, c["stab_words"])
        added = s.alloc(4)
        s.words(added, [c["added"]])
        s.words(deq, [c["bs"], c["cap"], c["count"], c["start"], tab, 0, 0])
        s.words(spare, [c["scap"], c["scount"], c["sstart"], stab])
        a.update(deq=deq, spare=deq + 4 if c["same"] else spare, added=added)
    elif c["op"] == "destroy":
        deq = s.alloc(0x20)
        cap = c["cap"]
        tab = table(s, cap, 24)
        blocks = [s.alloc(c["bs"] * 12) for _ in range(cap)]
        s.words(tab, blocks + [blocks[0]] * 24)
        s.words(deq, [c["bs"], cap, c["count"], c["start"], tab, 0, 0])
        a.update(deq=deq)
    else:
        deq = s.alloc(0x20) if c["this"] else 0
        if c["this"]:
            tab = s.alloc(32) if c["table"] else 0
            s.words(deq, [c["bs"], 3, 2, 1, tab, 5, 6])
        a.update(deq=deq)
    return a


def execute(s, case, a):
    c = case
    if c["op"] == "grow":
        return s.grow(a["deq"], c["from"], a["spare"], a["added"], c["to"])
    if c["op"] == "destroy":
        return s.destroy(a["deq"], c["A"], c["B"])
    return s.dtor(a["deq"], c["flag"])


def observe(s, case, a, answer):
    regs = s.regions()
    dump = [[s.norm(w) for w in s.rd_words(b, sz // 4)] for b, sz in regs]
    out = {"answer": answer, "log": s.log(), "regs": [r[1] for r in regs], "dump": dump, "fill": s.rd_word(s.g("fill"))}
    if case["op"] == "dtor":
        out["answer"] = s.norm(answer) if answer else 0
    return out


def run(s, case):
    try:
        a = build(s, case)
        return observe(s, case, a, execute(s, case, a))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rwords(r, n):
    return [r.choice([0, 1, 0x1234, r.randrange(0, 0x10000)]) for _ in range(n)]


def grow_case(r, bs=None, cap=None, count=None, start=None, same=None, diff=None):
    bs = bs or r.choice([1, 2, 3, 5, 6])
    cap = cap or r.randrange(1, 7)
    count = r.choice([cap, cap, r.randrange(0, cap + 1)]) if count is None else count
    start = r.randrange(0, cap) if start is None else start
    if count == 0 and start == 0:
        count = 1
    scap = r.randrange(1, 7)
    scount = r.randrange(0, scap + 1)
    sstart = r.randrange(0, scap)
    if scount == 0 and sstart == 0:
        scount = 1
    same = r.random() < 0.15 if same is None else same
    diff = r.choice([0, 1, bs - 1, bs, bs + 1, 2 * bs, 2 * bs + 1, 3 * bs, r.randrange(0, 4 * bs)]) if diff is None else diff
    frm = r.randrange(0, 20)
    return {"op": "grow", "queues": {}, "bs": bs, "cap": cap, "count": count, "start": start, "scap": scap, "scount": scount,
            "sstart": sstart, "same": same, "from": frm, "to": frm + max(diff, 0), "fill": r.choice([0, 0x1234, r.randrange(0, 1 << 32)]),
            "added": r.choice([0, 7]), "tab_words": rwords(r, cap + 8), "stab_words": rwords(r, scap + 8)}


def grow_edges():
    r = random.Random(5976)
    for bs in (1, 3):
        for cap, count, start in ((1, 1, 0), (3, 3, 0), (3, 3, 2), (4, 2, 1), (4, 0, 2), (2, 2, 1)):
            for diff in (0, bs, bs + 1, 3 * bs):
                for same in (False, True):
                    yield grow_case(r, bs, cap, count, start, same, diff)
    for _ in range(6):
        yield grow_case(r)


def grow_random(r):
    return grow_case(r)


def destroy_case(r, equal=None, n=None, ai=None):
    bs = r.choice([1, 2, 3, 4, 6])
    cap = r.randrange(2, 7)
    ai = r.randrange(0, 3 * bs + 1) if ai is None else ai
    n = r.choice([0, 1, bs - 1, bs, bs + 1, 2 * bs, 3 * bs, r.randrange(0, 4 * bs + 1)]) if n is None else n
    equal = r.random() < 0.15 if equal is None else equal
    ea = r.randrange(1, 1 << 32)
    eb = ea if equal else (ea ^ r.randrange(1, 1 << 16))
    cur = lambda: [r.randrange(0, 1 << 32) for _ in range(3)]
    A = cur() + [ea, r.randrange(0, 1 << 32), ai]
    B = cur() + [eb, r.randrange(0, 1 << 32), ai + n]
    return {"op": "destroy", "queues": {}, "bs": bs, "cap": cap, "count": r.randrange(0, cap + 1), "start": r.randrange(0, cap),
            "A": A, "B": B, "fill": 0}


def destroy_edges():
    r = random.Random(77)
    yield destroy_case(r, True)
    for bs_n in (0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 13):
        for ai in (0, 1, 2, 5):
            yield destroy_case(r, False, bs_n, ai)


def destroy_random(r):
    return destroy_case(r)


def dtor_case(r, this=None, table=None, flag=None):
    return {"op": "dtor", "queues": {}, "bs": r.choice([1, 2, 42]), "this": r.random() < 0.9 if this is None else this,
            "table": r.random() < 0.8 if table is None else table,
            "flag": r.choice([-1, 0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001]) if flag is None else flag, "fill": 0}


def dtor_edges():
    r = random.Random(13)
    for this in (False, True):
        for table in (False, True):
            for flag in (-1, 0, 1, 2, 0x7FFF, 0x8000, 0x10000):
                yield dtor_case(r, this, table, flag)


def dtor_random(r):
    return dtor_case(r)


TESTS = [
    FnTest("std::deque<std::vector<Argument,std::allocator<Argument>>,std::allocator<std::vector<Argument,std::allocator<Argument>>>>::grow_buf_back",
           ppc, host, edge=grow_edges, rnd=grow_random),
    FnTest("std::deque<std::vector<Argument,std::allocator<Argument>>,std::allocator<std::vector<Argument,std::allocator<Argument>>>>::destroy",
           ppc, host, edge=destroy_edges, rnd=destroy_random),
    FnTest("std::deque<std::vector<Argument,std::allocator<Argument>>,std::allocator<std::vector<Argument,std::allocator<Argument>>>>::~deque",
           ppc, host, edge=dtor_edges, rnd=dtor_random),
]
