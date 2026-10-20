"""Unit v5645, the MSL std::basic_string<wchar_t> init, three constructors and change_size, against
libtpw_unit_std_basic_string_init.dylib (tests/unit_std_basic_string_init.cpp), main app ("SimTheme Park"),
src/Cheat_std_basic_string_init.cpp:
  init 0x10146cf8, basic_string(const wchar_t*, unsigned long, alloc) 0x10146b8c, basic_string(const wchar_t*, alloc) 0x10146a14,
  change_size 0x101b2cd8, basic_string(alloc) 0x101e0b84

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (operator new,
delete, new[], strlen, strcpy, wmemcpy, the counted-pointer constructor 0x100026ac, __throw) are fakes on both sides that record the
same events; texts are logged as a hash, blocks as sizes and first characters (addresses differ between the sides). wcslen is the
platform's real function on the host and a plain loop on the PowerPC side. Every block a fake hands out has zeroed padding before and
after it, because a huge length wraps the 32-bit address to a little before the block (the original does the same).

The world of a case: the three terminator globals (random characters), a text buffer, the string object (a pointer to the
representation), and for change_size a representation (length, capacity, reference count, data) with random characters. What is
compared: the answer (the constructors answer this), the representation and its data (and the padding before it), the original
representation after change_size, the event log.

No fused operations. Not tested: a text longer than 0x7ffffffe characters through the C-string constructor (wcslen cannot answer
it); a capacity of 0 for change_size (the original loops forever doubling it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_string_init"
MASK = 0xFFFFFFFF

FN_INIT, FN_CTOR, FN_CTORC, FN_CHANGE, FN_CTORE = 0x10146CF8, 0x10146B8C, 0x10146A14, 0x101B2CD8, 0x101E0B84
SIZES = {FN_INIT: 264, FN_CTOR: 244, FN_CTORC: 256, FN_CHANGE: 904, FN_CTORE: 244}
OWN = set(SIZES)

ADDR = dict(nulInit=0x102A4D3C, nulResize=0x102AC356, nulEmpty=0x102B006C)
HOST_LAYOUT = [0, 4, 8, 16]
VT_IDS = {0x10278B74: 1, 0x10278BB0: 2, 0x10278BC0: 3}
NAME = "std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>::"


def fnv(data):
    if isinstance(data, str):
        data = data.encode("latin-1")
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & MASK
    return h


def cstr_bytes(e, a):
    out = bytearray()
    while True:
        b = e.read(a + len(out), 1)[0]
        if b == 0:
            return bytes(out)
        out.append(b)


def u16s(e, a, n):
    return list(struct.unpack(">%dH" % n, e.read(a, 2 * n))) if n else []


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def block(e, size):
    n = min(max(size, 256), 1024)
    return e.alloc(n + 32) + 16


def s_new(e):
    e.cl.rec("new", (e.r(3) & MASK,))
    e.set_r(3, block(e, e.r(3) & MASK))


def s_newarr(e):
    e.cl.rec("newarr", (e.r(3) & MASK,))
    e.set_r(3, block(e, e.r(3) & MASK))


def s_delete(e):
    p = e.r(3) & MASK
    c = u16s(e, p, 2) if p else [0, 0]
    e.cl.rec("delete", (c[0], c[1]))


def s_strlen(e):
    t = cstr_bytes(e, e.r(3) & MASK)
    e.cl.rec("strlen", (fnv(t), len(t)))
    e.set_r(3, len(t))


def s_strcpy(e):
    d, s = e.r(3) & MASK, e.r(4) & MASK
    t = cstr_bytes(e, s)
    e.cl.rec("strcpy", (fnv(t),))
    e.write(d, t + b"\0")


def s_wcslen(e):
    a = e.r(3) & MASK
    n = 0
    while u16s(e, a + 2 * n, 1)[0]:
        n += 1
    e.set_r(3, n)


def s_wmemcpy(e):
    d, s, n = e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK
    c = u16s(e, s, min(n, 4)) + [0] * 4
    e.cl.rec("wmemcpy", (n, c[0], c[1], c[2], c[3]))
    if n <= 1024:
        e.write(d, e.read(s, 2 * n))
    e.set_r(3, d)


def s_rcctor(e):
    this, p = e.r(3) & MASK, e.r(4) & MASK
    e.cl.rec("rcctor", (1 if p else 0,))
    e.write(this, struct.pack(">II", p, 0))


def s_throw(e):
    t, ex, tv = e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK
    vt, p, cnt = struct.unpack(">III", e.read(ex, 12))
    count = struct.unpack(">I", e.read(cnt, 4))[0] if cnt else 0
    e.cl.rec("throw", (fnv(cstr_bytes(e, t)), VT_IDS.get(vt, 0), fnv(cstr_bytes(e, p)), count, 1 if tv == 0x10265680 else 0))


def glue(fn):
    """The glue saves the caller's TOC pointer in the frame (the code reloads r2 from there after the call)."""
    def wrapped(e):
        e.write((e.r(1) & MASK) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapped


STUBS.internal(0x102384B8, glue(s_new))
STUBS.internal(0x10238668, glue(s_newarr))
STUBS.internal(0x10238218, glue(s_delete))
STUBS.internal(0x10238650, glue(s_strlen))
STUBS.internal(0x10238410, glue(s_strcpy))
STUBS.internal(0x102389C8, glue(s_wcslen))
STUBS.internal(0x102389E0, glue(s_wmemcpy))
STUBS.internal(0x100026AC, s_rcctor)
STUBS.internal(0x10238680, glue(s_throw))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def buf(self, nchars):
        """A block of nchars characters with 16 zero bytes before and after (the address of the first character)."""
        return self.alloc(2 * nchars + 32) + 16

    def put16(self, a, chars):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", c & 0xFFFF) for c in chars))

    def rd16(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n))) if n else []

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def wptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def w32(self, a, v):
        self.write(a, struct.pack(self.fmt, v & MASK))

    def r32(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def make_rep(self, ln, cap, refs, data):
        rep = self.alloc(48)
        self.w32(rep, ln)
        self.w32(rep + 4, cap)
        self.w32(rep + 8, refs)
        self.wptr(rep + self.doff, data)
        return rep


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    doff = 0xC

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

    def call(self, fn, args):
        return T.call_ppc(self.e, fn, [a & MASK for a in args])[0] & MASK

    def same(self, ret, ptr):
        return ret == ptr


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    doff = 0x10

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sbsi_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sbsi_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        f = {FN_INIT: self.lib.tpw_sbsi_init, FN_CTOR: self.lib.tpw_sbsi_ctor, FN_CTORC: self.lib.tpw_sbsi_ctorc,
             FN_CHANGE: self.lib.tpw_sbsi_change, FN_CTORE: self.lib.tpw_sbsi_ctore}[fn]
        count = {FN_INIT: 3, FN_CTOR: 3, FN_CTORC: 2, FN_CHANGE: 3, FN_CTORE: 1}[fn]
        return f(*[a & 0xFFFFFFFFFFFFFFFF for a in args[:count]]) or 0

    def same(self, ret, ptr):
        return ret == ptr


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_sbsi_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sbsi_init.restype, lib.tpw_sbsi_init.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_sbsi_ctor.restype, lib.tpw_sbsi_ctor.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64, C.c_uint32]
    lib.tpw_sbsi_ctorc.restype, lib.tpw_sbsi_ctorc.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64]
    lib.tpw_sbsi_ctore.restype, lib.tpw_sbsi_ctore.argtypes = C.c_uint64, [C.c_uint64]
    lib.tpw_sbsi_change.restype, lib.tpw_sbsi_change.argtypes = None, [C.c_uint64, C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sbsi_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    ctx = {}
    for name, v in zip(("nulInit", "nulResize", "nulEmpty"), case["nul"]):
        s.put16(s.g(name), [v])
    chars = case["chars"]
    ctx["text"] = s.buf(len(chars) + 40)
    s.put16(ctx["text"], chars)
    ctx["self"] = s.alloc(16)
    if case["op"] == "change":
        ln, cap, refs = case["rep"]
        data = s.buf(cap + 1)
        s.put16(data, case["data"])
        ctx["rep"] = s.make_rep(ln, cap, refs, data)
        ctx["data"] = data
        s.wptr(ctx["self"], ctx["rep"])
    return ctx


def execute(s, case, ctx):
    op, me, tx = case["op"], ctx["self"], ctx["text"]
    if op == "init":
        s.call(FN_INIT, [me, tx + case["first"], tx + case["last"], 0, 0])
        return None
    if op == "ctor":
        return s.same(s.call(FN_CTOR, [me, tx, case["n"], 0]), me)
    if op == "ctorc":
        return s.same(s.call(FN_CTORC, [me, tx, 0]), me)
    if op == "ctore":
        return s.same(s.call(FN_CTORE, [me]), me)
    s.call(FN_CHANGE, [me, case["n"], case["keep"]])
    return None


def rep_view(s, rep):
    ln, cap, refs = s.r32(rep), s.r32(rep + 4), s.r32(rep + 8)
    data = s.rptr(rep + s.doff)
    nch = min(cap + 1, 100) if cap < 0x10000 else 8
    chars = s.rd16(data - 8, 4 + nch)
    return {"len": ln, "cap": cap, "refs": refs, "chars": chars}


def observe(s, case, ctx, answer):
    out = {"answer": answer, "log": s.log(), "text": s.rd16(ctx["text"] - 8, 4 + len(case["chars"]) + 8)}
    rep = s.rptr(ctx["self"])
    out["rep"] = rep_view(s, rep) if rep else None
    if case["op"] == "change":
        out["same_rep"] = rep == ctx["rep"]
        out["orig"] = rep_view(s, ctx["rep"])
        out["same_data"] = bool(rep) and s.rptr(rep + s.doff) == ctx["data"]
    return out


def run(s, case):
    try:
        ctx = build(s, case)
        return observe(s, case, ctx, execute(s, case, ctx))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rchars(r, n, zero_end=False):
    cs = [r.randrange(1, 0x10000) for _ in range(n)]
    return cs + ([0] if zero_end else [])


def nul(r):
    return [r.choice([0, 0x41, r.randrange(0x10000)]) for _ in range(3)]


def base(r, op):
    return {"op": op, "queues": {}, "nul": nul(r), "chars": [], "n": 0}


def init_case(r, k=None, mode=None):
    c = base(r, "init")
    k = r.choice([0, 1, 2, 3, 4, 5, 7, 12]) if k is None else k
    c["chars"] = rchars(r, 24)
    mode = r.choice(["even", "even", "odd", "back"]) if mode is None else mode
    c["first"] = 16
    if mode == "even":
        c["last"] = 16 + 2 * k
    elif mode == "odd":
        c["last"] = 16 + 2 * k + 1
    else:   # last before first: a negative count wraps to a huge one
        c["first"] = 16
        c["last"] = 16 - [1, 2, 3, 4, 5, 6, 7, 8][k % 8]
    return c


def ctor_case(r, n=None):
    c = base(r, "ctor")
    c["n"] = r.choice([0, 1, 2, 3, 4, 5, 8, 13]) if n is None else n
    c["chars"] = rchars(r, 24)
    return c


def ctorc_case(r, k=None):
    c = base(r, "ctorc")
    k = r.choice([0, 1, 2, 3, 4, 7, 12]) if k is None else k
    c["chars"] = rchars(r, k, True)
    return c


def ctore_case(r):
    return base(r, "ctore")


def change_case(r, n=None, keep=None, rep=None):
    c = base(r, "change")
    cap = r.randrange(1, 21) if rep is None else rep[1]
    ln = r.randrange(0, cap + 1) if rep is None else rep[0]
    refs = r.choice([0, 1, 2, 3]) if rep is None else rep[2]
    c["rep"] = (ln, cap, refs)
    c["data"] = rchars(r, cap + 1)
    c["n"] = r.choice([0, 1, cap - 1, cap, cap + 1, cap + 2, 2 * cap + 1, r.randrange(0, 60), 90]) if n is None else n
    c["n"] = max(c["n"], 0)
    c["keep"] = r.choice([0, 1, 0x100, 0x101, 0xFF]) if keep is None else keep
    return c


INIT_EDGES = [(k, m) for m in ("even", "odd", "back") for k in range(8)]


def init_edges():
    r = random.Random(11)
    for k, m in INIT_EDGES:
        yield init_case(r, k, m)


def ctor_edges():
    r = random.Random(12)
    for n in (0, 1, 2, 3, 4, 7, 0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
        yield ctor_case(r, n)


def ctorc_edges():
    r = random.Random(13)
    for k in (0, 1, 2, 3, 4, 5, 8):
        yield ctorc_case(r, k)


def ctore_edges():
    r = random.Random(14)
    for _ in range(3):
        yield ctore_case(r)


def change_edges():
    r = random.Random(15)
    for refs in (0, 1, 2, 3):
        for keep in (0, 1, 0x100):
            for n in (0, 2, 5, 6, 7, 8, 30, 0x7FFFFFFE, 0x7FFFFFFF, 0x80000000):
                yield change_case(r, n, keep, (r.randrange(0, 6), 5, refs))
    for ln, cap in ((3, 3), (0, 1), (7, 7)):
        for refs in (0, 2):
            for n in (cap, cap + 1, 2 * cap, 2 * cap + 1):
                yield change_case(r, n, 1, (ln, cap, refs))


TESTS = [
    FnTest(NAME + "init", ppc, host, edge=init_edges, rnd=init_case),
    FnTest(NAME + "basic_string@0x10146b8c", ppc, host, edge=ctor_edges, rnd=ctor_case),
    FnTest(NAME + "basic_string@0x10146a14", ppc, host, edge=ctorc_edges, rnd=ctorc_case),
    FnTest(NAME + "change_size", ppc, host, edge=change_edges, rnd=change_case),
    FnTest(NAME + "basic_string@0x101e0b84", ppc, host, edge=ctore_edges, rnd=ctore_case),
]
