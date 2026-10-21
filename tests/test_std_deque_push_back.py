"""Unit v5243, std::deque<errorthingy>::push_back and grow_buf_back, against libtpw_unit_std_deque_push_back.dylib
(tests/unit_std_deque_push_back.cpp), main app ("SimTheme Park"), src/game/MslSupport.cpp:
  push_back 0x101f21ac, grow_buf_back 0x101f2e7c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit are fakes on both
sides that record their arguments: the runtime (strlen, new[], new, delete, strcpy, __throw, the counted string constructor) and the
block table (__cdeque<const void*>: operator[], reserve, resize, pop_front, push_back, swap, do_assign). The table fakes have small
deterministic effects (reserve allocates a table and copies, do_assign copies a range, push_back counts, pop_front moves the first
slot, swap swaps the four words) so the unit's follow-up reads see consistent state. Pointers in events and memory are labels: the
index of the block in the case's registry and the offset inside it.

The world of a case: the deque (block size, block table words, first, size), its block table with blocks (or raw words for grow_buf_back),
the pad word, the record, and for grow_buf_back the spare table and the counter. What is compared: the answer, the event log, the pad,
and the content of every block of the registry (the deque, the tables, the blocks, the ones the fakes allocate).

No fused operations. Not tested: a block size of 0 (division by zero), a deque whose element block is a null pointer with a non-zero
offset (a write to a wild address), a table whose index runs past the 48 slots of the world.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_deque_push_back"
MASK = 0xFFFFFFFF

FN_PUSH, FN_GROW = 0x101F21AC, 0x101F2E7C
SIZES = {FN_PUSH: 776, FN_GROW: 848}
OWN = set(SIZES)

G_STRLEN, G_NEWA, G_STRCPY, G_THROW, G_NEW, G_DELETE = 0x10238650, 0x10238668, 0x10238410, 0x10238680, 0x102384B8, 0x10238218
RC_CTOR = 0x100026AC
CD_INDEX, CD_POP, CD_SWAP, CD_RESERVE, CD_RESIZE, CD_ASSIGN, CD_PUSH = (0x101F2004, 0x101F25E8, 0x101F2668, 0x101F2730, 0x101F2A10,
                                                                      0x101F3278, 0x101F459C)
ADDR = dict(pad=0x102B141C)
LEN_VTBL, LEN_DTOR = 0x10278B74, 0x10265680
HOST_LAYOUT = [0x1C, 4, 0x14, 0x18, 16]
TBL = 48


def s32(v):
    return T.s32(v)


def rd(e, a):
    return e.u32(a & MASK)


def wr(e, a, v):
    e.write(a & MASK, struct.pack(">I", v & MASK))


def cs(e, a):
    if a == 0:
        return b""
    b = e.cstr(a)
    return b.encode("latin1") if isinstance(b, str) else bytes(b)


def hsh(b):
    h = 0
    for c in b:
        h = (h * 31 + c) & MASK
    return s32(h)


def lab_of(reg, p):
    p &= MASK
    if p == 0:
        return 0
    for i, (a, sz, k) in enumerate(reg):
        a &= MASK
        if a <= p < a + sz:
            return ((i + 1) << 16) | (p - a)
    return -1


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lab(e, p):
    return lab_of(e.sdp["reg"], p)


def new_block(e, size, kind, alloc):
    a = e.alloc(alloc if alloc else 4)
    e.sdp["reg"].append((a, size, kind))
    return a


def st_strlen(e):
    b = cs(e, e.r(3))
    e.cl.rec("strlen", (hsh(b),))
    e.set_r(3, len(b))


def st_new(e):
    size = e.r(3) & MASK
    e.cl.rec("new", (s32(size),))
    e.set_r(3, new_block(e, size, 0, size))


def st_newa(e):
    size = e.r(3) & MASK
    e.cl.rec("new_a", (s32(size),))
    a = new_block(e, size, 1, size)
    e.sdp["last"] = a
    e.set_r(3, a)


def st_rcctor(e):
    self, p = e.r(3) & MASK, e.r(4) & MASK
    e.cl.rec("rcctor", (1 if p == e.sdp["last"] else 0,))
    wr(e, self, p)
    wr(e, self + 4, 0)


def st_strcpy(e):
    d, s = e.r(3) & MASK, e.r(4) & MASK
    b = cs(e, s)
    e.cl.rec("strcpy", (1 if d == e.sdp["last"] else 0, hsh(b)))
    e.write(d, b + b"\0")
    e.set_r(3, d)


def st_throw(e):
    obj = e.r(3 + 1) & MASK
    e.cl.rec("throw", (hsh(cs(e, e.r(3))), 1 if rd(e, obj) == LEN_VTBL else 0, 1 if (e.r(5) & MASK) == LEN_DTOR else 0,
                       hsh(cs(e, rd(e, obj + 4)))))


def st_delete(e):
    e.cl.rec("delete", (lab(e, e.r(3)),))


def st_index(e):
    cd, idx = e.r(3) & MASK, e.r(4) & MASK
    e.cl.rec("cd_index", (lab(e, cd), s32(idx)))
    cap, first, tab = rd(e, cd), rd(e, cd + 8), rd(e, cd + 12)
    e.set_r(3, tab if cap == 0 else tab + 4 * ((first + idx) % cap))


def st_reserve(e):
    cd, n = e.r(3) & MASK, e.r(4) & MASK
    e.cl.rec("cd_reserve", (lab(e, cd), s32(n)))
    cap, count, first, tab = [rd(e, cd + 4 * i) for i in range(4)]
    if n > cap:
        t = new_block(e, 192, 0, 192)
        if cap:
            for i in range(count):
                wr(e, t + 4 * i, rd(e, tab + 4 * ((first + i) % cap)))
        wr(e, cd, n)
        wr(e, cd + 8, 0)
        wr(e, cd + 12, t)


def st_resize(e):
    cd, n, fill = e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK
    e.cl.rec("cd_resize", (lab(e, cd), s32(n), s32(fill)))
    wr(e, cd + 4, n)


def st_pop(e):
    cd = e.r(3) & MASK
    e.cl.rec("cd_pop", (lab(e, cd),))
    cap, count, first = rd(e, cd), rd(e, cd + 4), rd(e, cd + 8)
    if count:
        wr(e, cd + 4, count - 1)
        if cap:
            first += 1
            if first >= cap:
                first -= cap
            wr(e, cd + 8, first)


def st_push(e):
    cd, v = e.r(3) & MASK, e.r(4) & MASK
    e.cl.rec("cd_push", (lab(e, cd), 1 if v == ADDR["pad"] else 0, s32(rd(e, v))))
    wr(e, cd + 4, rd(e, cd + 4) + 1)


def st_swap(e):
    a, b = e.r(3) & MASK, e.r(4) & MASK
    e.cl.rec("cd_swap", (lab(e, a), lab(e, b)))
    for i in range(4):
        x, y = rd(e, a + 4 * i), rd(e, b + 4 * i)
        wr(e, a + 4 * i, y)
        wr(e, b + 4 * i, x)


def st_assign(e):
    d = e.r(3) & MASK
    bc, bi, bs = e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK
    ec, ei, es = e.r(7) & MASK, e.r(8) & MASK, e.r(9) & MASK
    boff = (bs - rd(e, bc + 12)) & MASK
    eoff = (es - rd(e, ec + 12)) & MASK
    e.cl.rec("cd_assign", (lab(e, d), lab(e, bc), s32(bi), s32(boff), lab(e, ec), s32(ei), s32(eoff)))
    ln = (ei - bi) & MASK
    start = boff // 4
    bcap, st = rd(e, bc), rd(e, bc + 12)
    dcap, dfirst, dt = rd(e, d), rd(e, d + 8), rd(e, d + 12)
    if bcap and dcap and ln <= 64:
        for i in range(ln):
            wr(e, dt + 4 * ((dfirst + i) % dcap), rd(e, st + 4 * ((start + i) % bcap)))
    wr(e, d + 4, ln)


def glue(f):
    """The import glue saves the TOC register in the caller's frame; the caller reloads it after the call."""

    def g(e):
        wr(e, e.r(1) + 0x14, e.r(2))
        f(e)

    return g


for _a, _f in ((G_STRLEN, glue(st_strlen)), (G_NEW, glue(st_new)), (G_NEWA, glue(st_newa)), (RC_CTOR, st_rcctor),
               (G_STRCPY, glue(st_strcpy)), (G_THROW, glue(st_throw)), (G_DELETE, glue(st_delete)), (CD_INDEX, st_index),
               (CD_RESERVE, st_reserve), (CD_RESIZE, st_resize), (CD_POP, st_pop), (CD_PUSH, st_push), (CD_SWAP, st_swap),
               (CD_ASSIGN, st_assign)):
    STUBS.internal(_a, _f)


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

    def mk(self, n, kind=0):
        a = self.alloc(n)
        self.reg(a, n, kind)
        return a


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        e.sdp = {"reg": [], "last": 0}
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def reg(self, a, n, kind):
        self.e.sdp["reg"].append((a, n, kind))

    def registry(self):
        return list(self.e.sdp["reg"])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_push(self, d, rec):
        T.call_ppc(self.e, FN_PUSH, [d, rec])
        return 0

    def call_grow(self, d, p2, cd, out, p5):
        r3, _, _ = T.call_ppc(self.e, FN_GROW, [d, p2, cd, out, p5])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.high = None
        lib.tpw_sdp_begin()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sdp_addr(name.encode())

    def alloc(self, n):
        a = self.lib.tpw_sdp_alloc(n)
        if self.high is None:
            self.high = a >> 32
            self.lib.tpw_sdp_set_high(self.high)
        assert a >> 32 == self.high, hex(a)
        return a

    def reg(self, a, n, kind):
        self.lib.tpw_sdp_reg(a, n, kind)

    def registry(self):
        out = []
        arr = (C.c_uint64 * 3)()
        for i in range(self.lib.tpw_sdp_count()):
            self.lib.tpw_sdp_entry(i, arr)
            out.append((arr[0], arr[1], arr[2]))
        return out

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_push(self, d, rec):
        self.lib.tpw_sdp_push_back(d, rec)
        return 0

    def call_grow(self, d, p2, cd, out, p5):
        return self.lib.tpw_sdp_grow(d, p2, cd, out, p5) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_sdp_"
    lib.tpw_sdp_addr.restype, lib.tpw_sdp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sdp_alloc.restype, lib.tpw_sdp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sdp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sdp_begin.argtypes = []
    lib.tpw_sdp_set_high.argtypes = [C.c_uint64]
    lib.tpw_sdp_reg.argtypes = [C.c_uint64, C.c_uint32, C.c_int32]
    lib.tpw_sdp_count.restype, lib.tpw_sdp_count.argtypes = C.c_uint32, []
    lib.tpw_sdp_entry.argtypes = [C.c_uint32, C.POINTER(C.c_uint64)]
    lib.tpw_sdp_push_back.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_sdp_grow.restype, lib.tpw_sdp_grow.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32, C.c_uint64, C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sdp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    D = s.D = s.mk(0x1C)
    T_ = s.mk(4 * TBL) if c["tab"] else 0
    s.words(D, [c["bs"], c["cap"], c["count"], c["first"], T_, c["tfirst"], c["tsize"]])
    for i, sl in enumerate(c["slots"]):
        if isinstance(sl, list):
            B = s.mk(max(len(sl) * 4, 4))
            s.words(B, sl)
            v = B
        else:
            v = sl
        if T_:
            s.words(T_ + 4 * i, [v])
    s.words(s.g("pad"), [c["pad"]])
    s.rec = s.alloc(8)
    s.words(s.rec, c["rec"])
    if c["op"] == "grow":
        s.out = s.mk(4)
        s.words(s.out, [c["out0"]])
        cd = c["cd"]
        if cd["mode"] == "same":
            s.cd = D + 4
        else:
            s.cd = s.mk(16)
            t2 = s.mk(4 * TBL) if cd["tab"] else 0
            s.words(s.cd, [cd["cap"], cd["count"], cd["first"], t2])
            if t2:
                s.words(t2, cd["slots"])


def execute(s, c):
    if c["op"] == "push":
        return s.call_push(s.D, s.rec)
    return s.call_grow(s.D, c["p2"], s.cd, s.out, c["p5"])


def observe(s, c, answer):
    reg = s.registry()
    mem = []
    for a, size, kind in reg:
        if kind == 1:
            mem.append(s.read(a, size).hex())
        else:
            ws = s.rd_words(a, (size + 3) // 4)
            mem.append([w if lab_of(reg, w) == -1 else lab_of(reg, w) for w in ws])
    return {"answer": answer, "log": s.log(), "pad": s.rd_word(s.g("pad")), "mem": mem, "sizes": [r[1] for r in reg]}


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


def blk(r, n):
    return [r.getrandbits(32) for _ in range(n)]


def slots_blocks(r, cap, nulls, blksz):
    return [blk(r, (blksz + 3) // 4) if (i < cap and i not in nulls) else 0 for i in range(TBL)]


def push_case(r, mode=None, bs=None, C=None, big=None):
    mode = mode or r.choice(["grow", "grow", "plain", "plain", "null", "pop", "err", "empty"])
    bs = bs or r.choice([1, 2, 3, 4, 5, 8])
    c = {"op": "push", "queues": {}, "pad": r.getrandbits(32), "rec": [r.getrandbits(32), r.getrandbits(32)], "bs": bs}
    c["blksz"] = min(bs * 8, 64)
    nulls = set()
    if big:
        tsize, idx, C = big
        c["bs"] = bs = 0x40000000
        c["blksz"] = 64
        cap = C + r.choice([0, 1])
        first = r.randrange(cap)
        tfirst = (idx - tsize) & MASK
    elif mode == "empty":
        C = 0
        cap = r.choice([0, 1, 2, 3, 5])
        first = r.randrange(cap) if cap else 0
        tfirst = tsize = 0
    else:
        C = C or (r.randint(3, 8) if mode == "pop" else r.randint(1, 7))
        cap = C + r.choice([0, 0, 1, 3])
        first = r.randrange(cap)
        trig = bs * (C - 1)
        cands = [i for i in range(bs * C) if i != trig]
        if mode == "plain" or mode == "pop" or mode == "null":
            if mode == "null":
                cands = [bs * k for k in range(C - 1)]
            if mode == "pop":
                cands = [i for i in cands if i >= (bs * 3) // 2]
            if not cands:
                mode = "grow"
        if mode == "err":
            cands = list(range(bs * C))
        if mode == "grow":
            tfirst = r.randint(0, trig)
            tsize = trig - tfirst
            if r.random() < 0.5:
                nulls.add((first + C - 1) % cap)
        else:
            idx = r.choice(cands)
            if mode == "pop":
                tfirst = r.randint((bs * 3) // 2, idx)
                tsize = idx - tfirst
            elif mode == "err":
                k = r.randint(1, bs * C + 2)
                tsize = (1 << 32) - k
                tfirst = idx + k
            else:
                tfirst = r.randint(0, idx)
                tsize = idx - tfirst
            if mode == "null":
                nulls.add(first if idx == 0 else (first + idx // bs) % cap)
    c.update(cap=cap, count=C, first=first, tab=cap > 0, tfirst=tfirst & MASK, tsize=tsize & MASK,
             slots=slots_blocks(r, cap, nulls, c["blksz"]))
    return c


def push_edges():
    r = random.Random(5243)
    for mode in ("grow", "plain", "null", "pop", "err", "empty"):
        for bs in (1, 2, 4, 5):
            for C in (1, 2, 3, 5):
                yield push_case(r, mode, bs, C)
    for tsize in (0x1FFFFFFE, 0x1FFFFFFF, 0x20000000):
        for C, idx in ((1, 1), (1, 3), (3, 1), (3, 3)):
            yield push_case(r, "plain", big=(tsize, idx, C))


def push_random(r):
    return push_case(r)


def grow_case(r, variant=None, n=None):
    variant = variant or r.choice(["swap_zero", "swap_valid", "swap_same", "noswap_zero", "noswap_valid", "noswap_same"])
    bs = r.choice([1, 2, 3, 5, 8])
    swap = variant.startswith("swap")
    cap = r.randint(1, 10) if swap else r.randint(2, 10)
    count = cap if swap else r.randint(1, cap - 1)
    first = r.randrange(cap)
    if n is None:
        n = r.choice([0, 1, bs - 1, bs, bs + 1, 2 * bs, 2 * bs + 1, 3 * bs])
    p2 = r.randint(0, 4)
    mode = variant.split("_")[1]
    cd = {"mode": mode, "cap": 0, "count": 0, "first": 0, "tab": False, "slots": [0] * TBL}
    if mode == "valid":
        cc = r.randint(1, 20)
        cd.update(cap=cc, count=r.randint(0, cc), first=r.randrange(cc), tab=True,
                  slots=[r.getrandbits(32) | 1 if i < cc else 0 for i in range(TBL)])
    return {"op": "grow", "queues": {}, "pad": r.getrandbits(32), "rec": [0, 0], "bs": bs, "cap": cap, "count": count, "first": first,
            "tab": True, "tfirst": r.getrandbits(8), "tsize": r.getrandbits(8),
            "slots": [r.getrandbits(32) | 1 if i < cap else 0 for i in range(TBL)], "p2": p2, "p5": p2 + n, "out0": r.randint(0, 5),
            "cd": cd}


def grow_edges():
    r = random.Random(77)
    for variant in ("swap_zero", "swap_valid", "swap_same", "noswap_zero", "noswap_valid", "noswap_same"):
        for _ in range(10):
            yield grow_case(r, variant)
    for _ in range(30):
        yield grow_case(r)


def grow_random(r):
    return grow_case(r)


TESTS = [
    FnTest("std::deque<errorthingy,std::allocator<errorthingy>>::push_back", ppc, host, edge=push_edges, rnd=push_random),
    FnTest("std::deque<errorthingy,std::allocator<errorthingy>>::grow_buf_back", ppc, host, edge=grow_edges, rnd=grow_random),
]
