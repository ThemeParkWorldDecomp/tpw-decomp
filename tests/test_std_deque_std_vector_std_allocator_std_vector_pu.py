"""Unit v5938, the Metrowerks std::deque<std::vector<Argument>> (push_back, pop_front, copy constructor, make_iterator), against
libtpw_unit_std_deque_std_vector_std_allocator_std_vector_pu.dylib (tests/unit_std_deque_std_vector_std_allocator_std_vector_pu.cpp), main app
("SimTheme Park"), src/game/MslSupport.cpp:
  push_back 0x10224a3c, pop_front 0x1022488c, deque(const deque&) 0x10224fbc, make_iterator 0x10228c84

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments: the block table (std::__cdeque<const void*>: reserve, resize, push_back,
pop_front, operator[]) is modelled by a few words of arithmetic, grow_buf_back, the vector copy constructor and destructor, the iterator
operators and the runtime imports (strlen, strcpy, operator new, new[], delete, __throw) record and the fakes that build memory
bump-allocate inside the case's pool, so every address is recorded and compared as an offset into the pool.

The world of a case: one 0x6000-byte pool holding the deque (0x1c bytes: block size, the table's capacity/count/start/table words, first,
size), its table of block pointers and the blocks, the vector to copy, the iterator out block, and the two globals 0x102b1f94 (block size
of a new deque) and 0x102b1f98 (the word whose address the constructor hands to the table's push_back). What is compared: the whole pool
(a word that points into the pool is compared as its offset) and the event log.

No fused operations. Not tested: a pop_front that empties a deque whose block table holds fewer than 2 blocks (the original loops 2^32 times),
a block size of 0 (divwu by zero is undefined on the hardware; push_back would also loop forever).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_deque_std_vector_std_allocator_std_vector_pu"
MASK = 0xFFFFFFFF

PUSH_BACK, POP_FRONT, CTOR, MAKE_IT = 0x10224A3C, 0x1022488C, 0x10224FBC, 0x10228C84
SIZES = {PUSH_BACK: 896, POP_FRONT: 256, CTOR: 720, MAKE_IT: 240}
OWN = set(SIZES)

# callees faked on both sides
CD_RESERVE, CD_RESIZE, CD_PUSH_BACK, CD_POP_FRONT, CD_INDEX = 0x101F2730, 0x101F2A10, 0x101F459C, 0x101F25E8, 0x101F2004
GROW, V_COPY, V_DTOR, CURSOR_ADD = 0x102287E4, 0x10227200, 0x102010F8, 0x101F5508
IT_DEREF, IT_INC, IT_NE = 0x10225874, 0x1022571C, 0x102255A0
G_STRLEN, G_NEWARR, G_NEW, G_DELETE, G_STRCPY, G_THROW = 0x10238650, 0x10238668, 0x102384B8, 0x10238218, 0x10238410, 0x10238680

ADDR = dict(bs=0x102B1F94, fill=0x102B1F98)
HOST_LAYOUT = [0x1C, 0x14, 0x18, 0x18, 0x14]

POOL = 0x6000
BUMP = 0x2000
D_AT, S_AT, VEC_AT, OUT_AT = 0x100, 0x140, 0x180, 0x1C0
D_TBL, S_TBL = 0x200, 0x280
D_BLK, S_BLK, STRIDE = 0x400, 0x1000, 0x100
VT = {0x10278B74: 1, 0x10278BB0: 2, 0x10265680: 4}

ENV = {}


def bump(n):
    at = ENV["next"]
    ENV["next"] = at + ((n + 15) & ~15)
    return (ENV["base"] + at) & MASK


def off(w):
    d = (w - ENV["base"]) & MASK
    return d if d < POOL else w


def rd(e, a):
    return e.u32(a)


def wr(e, a, v):
    e.write(a & MASK, struct.pack(">I", v & MASK))


def tsum(b):
    return sum(c * (i + 1) for i, c in enumerate(b)) & MASK


def cbytes(e, a):
    s = e.cstr(a)
    return s.encode("latin-1") if isinstance(s, str) else bytes(s)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cd_reserve(e, c, n):
    cap, count, start, table = (rd(e, c + 4 * i) for i in range(4))
    if n > cap:
        fresh = bump(4 * n)
        for i in range(count):
            wr(e, fresh + 4 * i, rd(e, table + 4 * ((start + i) % cap)))
        wr(e, c + 12, fresh)
        wr(e, c + 8, 0)
        wr(e, c, n)


def s_reserve(e):
    e.cl.rec("cdeque_reserve", (off(e.r(3)), e.r(4)))
    cd_reserve(e, e.r(3), e.r(4))


def s_resize(e):
    c, n, fill = e.r(3), e.r(4), e.r(5)
    e.cl.rec("cdeque_resize", (off(c), n, fill))
    fw = rd(e, fill) if fill else 0
    cd_reserve(e, c, n)
    cap, count, start, table = (rd(e, c + 4 * i) for i in range(4))
    for i in range(count, n):
        wr(e, table + 4 * ((start + i) % cap), fw)
    wr(e, c + 4, n)


def s_cd_push_back(e):
    c, v = e.r(3), rd(e, e.r(4))
    e.cl.rec("cdeque_push_back", (off(c), v))
    cap, count, start, table = (rd(e, c + 4 * i) for i in range(4))
    if count >= cap:
        cd_reserve(e, c, count + 1)
        cap, count, start, table = (rd(e, c + 4 * i) for i in range(4))
    wr(e, table + 4 * ((start + count) % cap), v)
    wr(e, c + 4, count + 1)


def s_cd_pop_front(e):
    c = e.r(3)
    e.cl.rec("cdeque_pop_front", (off(c),))
    cap, count, start, table = (rd(e, c + 4 * i) for i in range(4))
    if count > 0 and cap > 0:
        wr(e, c + 8, (start + 1) % cap)
        wr(e, c + 4, count - 1)


def s_cd_index(e):
    c, i = e.r(3), e.r(4)
    e.cl.rec("cdeque_index", (off(c), i))
    cap, count, start, table = (rd(e, c + 4 * k) for k in range(4))
    at = (start + i) % cap if cap else 0
    e.set_r(3, (table + 4 * at) & MASK)


def s_grow(e):
    me, frm, spare, to = e.r(3), e.r(4), e.r(5), e.r(7)
    s = [rd(e, spare + 4 * i) for i in range(4)]
    e.cl.rec("grow_buf_back", (off(me), frm, to, off(s[0]), s[1], s[2], off(s[3])))   # spare and added live on the stack
    answer = e.cl.ret("grow") & 1
    if e.cl.ret("grow_spare") != 0:
        wr(e, spare + 12, bump(16))
    e.set_r(3, answer)


def s_vcopy(e):
    t, src = e.r(3), e.r(4)
    e.cl.rec("vector_copy", (off(t), off(src)))
    wr(e, t, 0x5A5A0000 + ENV["seq"])
    ENV["seq"] += 1
    wr(e, t + 4, off(src))
    wr(e, t + 8, off(t))


def s_vdtor(e):
    v, flag = e.r(3), T.s32(e.r(4))
    e.cl.rec("vector_dtor", (off(v), flag))
    wr(e, v, 0x6B6B0000 + ENV["seq"])
    ENV["seq"] += 1


def s_cursor_add(e):
    c, n = e.r(3), e.r(4)
    w = [rd(e, c + 4 * i) for i in range(3)]
    e.cl.rec("cursor_add", (off(w[0]), w[1], off(w[2]), n))
    wr(e, c + 4, w[1] + n)
    wr(e, c + 8, w[2] + 4 * n)


def itw(e, a):
    w = [rd(e, a + 4 * i) for i in range(6)]
    return w, (off(w[0]), w[1], off(w[2]), off(w[3]), off(w[4]), w[5])


def s_deref(e):
    w, rec = itw(e, e.r(3))
    e.cl.rec("iter_deref", rec)
    e.set_r(3, w[3])


def s_inc(e):
    a = e.r(3)
    w, rec = itw(e, a)
    e.cl.rec("iter_inc", rec)
    wr(e, a + 12, w[3] + 12)
    wr(e, a + 20, w[5] + 1)


def s_ne(e):
    wa, ra = itw(e, e.r(3))
    wb, rb = itw(e, e.r(4))
    e.cl.rec("iter_ne", ra + rb)
    e.set_r(3, 1 if wa[5] != wb[5] else 0)


def s_strlen(e):
    b = cbytes(e, e.r(3))
    e.cl.rec("strlen", (len(b), tsum(b)))
    e.set_r(3, len(b))


def s_strcpy(e):
    d, b = e.r(3), cbytes(e, e.r(4))
    e.cl.rec("strcpy", (off(d), tsum(b)))
    e.set_r(3, d)      # the bytes are not written (byte order differs between the memories); the checksum is in the event


def s_new(e):
    e.cl.rec("new", (e.r(3),))
    e.set_r(3, bump(e.r(3)))


def s_newarr(e):
    e.cl.rec("new_array", (e.r(3),))
    e.set_r(3, bump(e.r(3)))


def s_delete(e):
    e.cl.rec("delete", (off(e.r(3)),))


def s_throw(e):
    t, o, tv = cbytes(e, e.r(3)), e.r(4), e.r(5)
    e.cl.rec("throw", (tsum(t), VT.get(rd(e, o), 0), off(rd(e, o + 4)), off(rd(e, o + 8)), VT.get(tv, 0)))


def glue(fn):
    """A glue import: the real glue saves the caller's TOC pointer at 0x14(r1) (the caller reloads r2 from there)."""
    def run_glue(e):
        wr(e, e.r(1) + 0x14, e.r(2))
        fn(e)
    return run_glue


for _a, _f in ((CD_RESERVE, s_reserve), (CD_RESIZE, s_resize), (CD_PUSH_BACK, s_cd_push_back), (CD_POP_FRONT, s_cd_pop_front),
               (CD_INDEX, s_cd_index), (GROW, s_grow), (V_COPY, s_vcopy), (V_DTOR, s_vdtor), (CURSOR_ADD, s_cursor_add),
               (IT_DEREF, s_deref), (IT_INC, s_inc), (IT_NE, s_ne)):
    STUBS.internal(_a, _f)
for _a, _f in ((G_STRLEN, s_strlen), (G_NEWARR, s_newarr), (G_NEW, s_new), (G_DELETE, s_delete), (G_STRCPY, s_strcpy), (G_THROW, s_throw)):
    STUBS.internal(_a, glue(_f))


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

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

    def pool(self, base):
        ENV.clear()
        ENV.update(base=base, next=BUMP, seq=0)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        T.call_ppc(self.e, fn, [a & MASK for a in args])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_argdq_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_argdq_alloc(n)

    def pool(self, base):
        ENV.clear()
        ENV.update(base=base & MASK)
        self.lib.tpw_argdq_pool(base)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        f = {PUSH_BACK: self.lib.tpw_argdq_push_back, POP_FRONT: self.lib.tpw_argdq_pop_front, CTOR: self.lib.tpw_argdq_ctor,
             MAKE_IT: self.lib.tpw_argdq_make_iterator}[fn]
        f(*args)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_argdq_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "pool").argtypes = [C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "push_back").argtypes = [C.c_uint64, C.c_uint64]
    getattr(lib, p + "pop_front").argtypes = [C.c_uint64]
    getattr(lib, p + "ctor").argtypes = [C.c_uint64, C.c_uint64]
    getattr(lib, p + "make_iterator").argtypes = [C.c_uint64, C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def put_deque(s, at, tbl, blk, sp):
    """A deque at pool offset `at` with its table at `tbl` (cap slots, each pointing at a block of the stride), blocks from `blk`."""
    P = s.P
    table = [0 if j in sp.get("nulls", ()) else P + blk + j * STRIDE for j in range(sp["cap"])]
    if table:
        s.words(P + tbl, table)
    s.words(P + at, [sp["bs"], sp["cap"], sp["count"], sp["start"], (P + tbl) if sp["cap"] else 0, sp["first"], sp["size"]])


def build(s, case):
    P = s.alloc(POOL)
    s.write(P, bytes(POOL))
    s.P = P
    s.pool(P)
    s.words(s.g("bs"), [case["gbs"]])
    s.words(s.g("fill"), [case["fill"]])
    s.words(P + VEC_AT, case["vec"])
    if case["op"] == "ctor":
        put_deque(s, S_AT, S_TBL, S_BLK, case["src"])
        s.words(P + D_AT, [0xDEAD0000 + i for i in range(7)])
    else:
        put_deque(s, D_AT, D_TBL, D_BLK, case["spec"])
    if case["op"] == "mk":
        s.words(P + OUT_AT, [0xDEAD1000 + i for i in range(6)])


def execute(s, case):
    P = s.P
    op = case["op"]
    if op == "push":
        s.call(PUSH_BACK, [P + D_AT, P + VEC_AT])
    elif op == "pop":
        s.call(POP_FRONT, [P + D_AT])
    elif op == "ctor":
        s.call(CTOR, [P + D_AT, P + S_AT])
    else:
        s.call(MAKE_IT, [P + OUT_AT, P + D_AT, case["index"]])


def observe(s, case):
    words = s.rd_words(s.P, POOL // 4)
    P = s.P & MASK
    return {"log": s.log(), "pool": [(0x80000000 | (w - P)) if P <= w < P + POOL else w for w in words]}


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


def rw(r):
    return 0x3C000000 | r.getrandbits(24)


def base(r, op, spec=None, src=None):
    return {"op": op, "queues": {"grow": [r.choice([0, 1])], "grow_spare": [r.choice([0, 0, 1])]},
            "gbs": r.choice([0, 1, 2, 4, 8, 42, 100]), "fill": rw(r), "vec": [rw(r), rw(r), rw(r)], "spec": spec, "src": src, "index": 0}


def deque_spec(r, bs=None, cap=None, count=None, start=None):
    bs = r.choice([1, 2, 3, 4, 5, 8, 20]) if bs is None else bs
    cap = r.randint(2, 8) if cap is None else cap
    count = r.randint(1, cap) if count is None else count
    start = r.randrange(cap) if start is None else start
    return {"bs": bs, "cap": cap, "count": count, "start": start, "first": 0, "size": 0}


# ---- push_back
def push_case(r, kind=None, **kw):
    kind = kind or r.choice(["n", "n", "n", "g", "g", "z", "t"])
    sp = deque_spec(r, **kw)
    bs, count = sp["bs"], sp["count"]
    if kind == "z":
        sp.update(count=r.choice([0, 0, 1]), first=0, size=0)
        if sp["count"] == 0 and r.random() < 0.5:
            sp.update(cap=0, start=0)
    elif kind == "t":
        sp.update(count=sp["cap"], first=r.randrange(bs * sp["cap"]), size=r.choice([0x15555554, 0x15555555, 0x15555556, 0x20000000, 0xFFFFFFFF, 0xFFFFFFF0]))
    else:
        first = r.randrange(bs * count)
        size = r.randint(0, bs * count - first - 1)
        if kind == "g" and bs * (count - 1) >= first:
            size = bs * (count - 1) - first
        sp.update(first=first, size=size)
    if kind == "z" and r.random() < 0.3 and sp["cap"] > 0:
        sp["nulls"] = (sp["start"],)
    return base(r, "push", sp)


def push_edges():
    r = random.Random(5001)
    for kind in ("n", "g", "z", "t", "n", "g", "t"):
        yield push_case(r, kind)
    yield push_case(r, "g", bs=1, cap=2, count=1, start=0)
    yield push_case(r, "g", bs=4, cap=3, count=3, start=2)
    yield push_case(r, "n", bs=1, cap=2, count=2, start=1)
    yield push_case(r, "n", bs=2, cap=2, count=2, start=1)
    yield push_case(r, "z", bs=4, cap=2, count=1, start=0)
    yield push_case(r, "t", bs=1, cap=2, count=2, start=0)
    # first just below and at 1.5 blocks: the trailing pop loop
    for first in (2, 3, 4, 5, 6, 7):
        c = push_case(r, "n", bs=4, cap=4, count=3, start=3)
        c["spec"].update(first=first, size=r.randint(0, 11 - first) if first < 11 else 0)
        yield c


def push_random(r):
    return push_case(r)


# ---- pop_front
def pop_case(r, kind=None, **kw):
    kind = kind or r.choice(["n", "n", "e", "e"])
    sp = deque_spec(r, **kw)
    if sp["count"] < 2:
        sp["count"] = 2
        sp["cap"] = max(sp["cap"], 2)
        sp["start"] = sp["start"] % sp["cap"]
    sp.update(first=r.randrange(sp["bs"] * sp["count"]), size=1 if kind == "e" else r.randint(2, 6))
    sp["nulls"] = ()
    return base(r, "pop", sp)


def pop_edges():
    r = random.Random(5002)
    for kind in ("n", "e", "n", "e"):
        yield pop_case(r, kind)
    yield pop_case(r, "e", bs=1, cap=2, count=2, start=1)
    yield pop_case(r, "e", bs=4, cap=5, count=5, start=3)
    yield pop_case(r, "e", bs=4, cap=5, count=4, start=4)
    yield pop_case(r, "n", bs=3, cap=2, count=2, start=1)
    yield pop_case(r, "e", bs=5, cap=8, count=8, start=0)


def pop_random(r):
    return pop_case(r)


# ---- make_iterator
def mk_case(r, kind=None, **kw):
    sp = deque_spec(r, bs=kw.pop("bs", r.choice([1, 2, 3, 5, 8, 20])), cap=kw.pop("cap", r.randint(2, 8)),
                    count=kw.pop("count", r.choice([0, 1, 2, 3, 4, 5])))
    sp["count"] = min(sp["count"], sp["cap"])
    sp["start"] = kw.pop("start", r.randrange(sp["cap"]))
    sp["first"] = r.randrange(5)
    sp["size"] = r.randrange(5)
    c = base(r, "mk", sp)
    room = sp["cap"] - sp["start"] + 1
    top = max(sp["bs"] * room, 3)
    c["index"] = kw.pop("index", r.randrange(top))
    return c


def mk_edges():
    r = random.Random(5003)
    for _ in range(6):
        yield mk_case(r)
    yield mk_case(r, bs=4, cap=4, count=0, start=0, index=0)
    yield mk_case(r, bs=4, cap=4, count=0, start=1, index=3)
    yield mk_case(r, bs=4, cap=4, count=2, start=1, index=3)
    yield mk_case(r, bs=4, cap=4, count=2, start=1, index=4)
    yield mk_case(r, bs=4, cap=4, count=3, start=0, index=11)
    yield mk_case(r, bs=4, cap=4, count=3, start=0, index=12)
    yield mk_case(r, bs=1, cap=4, count=2, start=1, index=0)
    yield mk_case(r, bs=1, cap=4, count=2, start=1, index=1)
    yield mk_case(r, bs=1, cap=4, count=2, start=1, index=2)


def mk_random(r):
    return mk_case(r)


# ---- the copy constructor
def ctor_case(r, **kw):
    src = deque_spec(r, bs=kw.get("bs"), cap=kw.get("cap"), count=kw.get("count"), start=kw.get("start"))
    src["first"] = kw.get("first", r.randrange(src["bs"] * src["count"]))
    src["size"] = kw.get("size", r.choice([0, 1, 1, 2, 3, 4, 5, 6, 9]))
    src["nulls"] = ()
    c = base(r, "ctor", None, src)
    if "gbs" in kw:
        c["gbs"] = kw["gbs"]
    return c


def ctor_edges():
    r = random.Random(5004)
    for _ in range(5):
        yield ctor_case(r)
    yield ctor_case(r, size=0)
    yield ctor_case(r, size=1, gbs=0)
    yield ctor_case(r, size=3, gbs=3)
    yield ctor_case(r, size=3, gbs=2)
    yield ctor_case(r, size=3, gbs=4)
    yield ctor_case(r, size=5, gbs=42)
    yield ctor_case(r, bs=2, cap=4, count=3, start=1, first=1, size=4, gbs=8)
    yield ctor_case(r, bs=2, cap=4, count=3, start=3, first=3, size=3, gbs=8)


def ctor_random(r):
    return ctor_case(r)


NAME = "std::deque<std::vector<Argument,std::allocator<Argument>>,std::allocator<std::vector<Argument,std::allocator<Argument>>>>::"

TESTS = [
    FnTest(NAME + "push_back", ppc, host, edge=push_edges, rnd=push_random),
    FnTest(NAME + "pop_front", ppc, host, edge=pop_edges, rnd=pop_random),
    FnTest(NAME + "deque", ppc, host, edge=ctor_edges, rnd=ctor_random),
    FnTest(NAME + "make_iterator", ppc, host, edge=mk_edges, rnd=mk_random),
]
