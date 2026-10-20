"""Unit v5907, online.cpp's static initialiser, CheckShifts and the cdeque assign, against libtpw_unit_sinit_online_cpp.dylib
(tests/unit_sinit_online_cpp.cpp), main app ("SimTheme Park"), src/ProcessGetChatSatellitePlayers_sinit_online_cpp.cpp:
  __sinit_online_cpp 0x1022a06c, CheckShifts 0x1022a3b4,
  std::__cdeque<const_void*,...>::do_assign<__cdeque<vector<Argument>*>::iterator> 0x10229ae4

A case is a world and a call. The world is built twice by one piece of Python code (emulator memory big-endian, host library
memory little-endian). The callees outside the unit are recording fakes on both sides: the two string constructors, the exit
registration, GetKeyState and the runtime (new, delete, strlen, strcpy, throw).

The world of a case: the two globals the initialiser writes preset to random words; for CheckShifts the three key answers; for the
assign the destination deque (capacity, size, start, block), the source deque (capacity, block, start slot) and the two iterators.
What is compared: the answer, the log of fake events, the globals, the destination words and its block, the source block.

No fused operations. The throw path: the PowerPC fake of __throw abandons the call (returns to the caller of do_assign), the host
fake raises a C++ exception the wrapper catches, because the original goes on after a returning __throw into a loop of 2^30 words.
Not tested: a count of exactly 0x3fffffff (it copies a billion words), a NULL block with a capacity that holds the count (the original
writes through NULL).
"""
import ctypes as C
import random
import struct

from unicorn.ppc_const import UC_PPC_REG_LR

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_online_cpp"
MASK = 0xFFFFFFFF

FN_SINIT, FN_SHIFTS, FN_ASSIGN = 0x1022A06C, 0x1022A3B4, 0x10229AE4
SIZES = {FN_SINIT: 720, FN_SHIFTS: 116, FN_ASSIGN: 836}
OWN = set(SIZES)

G_GETKEY = 0x10239280
G_CTOR_CHAR = 0x102382D8
G_CTOR_WIDE = 0x10238800
G_REGISTER = 0x10239940
G_THROW = 0x10238680
G_DELETE = 0x10238218
G_NEW = 0x102384B8
G_NEWARRAY = 0x10238668
G_STRCPY = 0x10238410
G_STRLEN = 0x10238650

ADDR = dict(zero=0x10396FB0, mask=0x10396FAC)
NODES = [0x10396FA0 - 12 * i for i in range(16)] + [0x10396DA0, 0x10396D80, 0x10396D60, 0x10396D40]
LEN_VTBL = 0x10278B74
HOST_LAYOUT = [16, 12]


def s32(v):
    return T.s32(v)


def hsh(data):
    if isinstance(data, str):
        data = data.encode("latin-1")
    return sum((i + 1) * b for i, b in enumerate(data))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_new(e):
    size = e.r(3)
    e.cl.rec("MslNew", (size,))
    e.set_r(3, e.alloc(size or 4))


def stub_newarray(e):
    size = e.r(3)
    e.cl.rec("MslNewArray", (size,))
    e.set_r(3, e.alloc(size or 4))


def stub_delete(e):
    e.cl.rec("MslDelete", (e.cl.lab(e.r(3)),))


def stub_strlen(e):
    s = e.cstr(e.r(3))
    e.cl.rec("MslStrlen", (hsh(s),))
    e.set_r(3, len(s))


def stub_strcpy(e):
    dst, src = e.r(3), e.r(4)
    s = e.cstr(src)
    if isinstance(s, str):
        s = s.encode("latin-1")
    e.cl.rec("MslStrcpy", (e.cl.lab(dst), hsh(s)))
    e.write(dst, s + b"\0")


def stub_throw(e):
    typ, obj, dtor = e.r(3), e.r(4), e.r(5)
    msg = e.u32(obj + 4)
    cnt = e.u32(obj + 8)
    e.cl.rec("MslThrow", (hsh(e.cstr(typ)), 1 if e.u32(obj) == LEN_VTBL else 0, hsh(e.cstr(msg)), e.u32(cnt),
                          1 if dtor == 0x10265680 else 0))
    # abandon do_assign: back to its caller (the saved link register is at frame + 0x108, the caller's frame is the back chain)
    sp = e.r(1)
    e.uc.reg_write(UC_PPC_REG_LR, e.u32(sp + 0x108))
    e.set_r(1, e.u32(sp))


def glue(fn):
    """An import is reached through glue that saves the caller's TOC at 0x14(r1); the caller reloads it after the call."""

    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        return fn(e)

    return wrapped


STUBS.internal(G_GETKEY, glue(CL.recorder("GetKeyState", "i", ret="h")))
STUBS.internal(G_CTOR_CHAR, glue(CL.recorder("TbCtorChar", "Ns", ret=lambda e, log: e.r(3))))
STUBS.internal(G_CTOR_WIDE, glue(CL.recorder("TbCtorWide", "NS", ret=lambda e, log: e.r(3))))
STUBS.internal(G_REGISTER, glue(CL.recorder("RegisterGlobalObject", "LuL")))
STUBS.internal(G_NEW, glue(stub_new))
STUBS.internal(G_NEWARRAY, glue(stub_newarray))
STUBS.internal(G_DELETE, glue(stub_delete))
STUBS.internal(G_STRLEN, glue(stub_strlen))
STUBS.internal(G_STRCPY, glue(stub_strcpy))
STUBS.internal(G_THROW, glue(stub_throw))


def setup(runner):
    pass


class Side:
    def ptr(self, w):
        return w

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
        self.L = e.cl = CL.PpcLog({a: i + 1 for i, a in enumerate(NODES)}, case["queues"])
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

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {i + 1: lib.tpw_sinit_addr(("node%d" % i).encode()) for i in range(20)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sinit_addr(name.encode())

    def alloc(self, n):
        p = self.lib.tpw_sinit_alloc(n)
        self.lib.tpw_sinit_set_high(p >> 32)
        self.high = (p >> 32) << 32
        return p

    def ptr(self, w):
        return self.high | w if w else 0

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        if entry == FN_SINIT:
            self.lib.tpw_sinit_sinit()
            return 0
        if entry == FN_SHIFTS:
            return self.lib.tpw_sinit_shifts() & MASK
        self.lib.tpw_sinit_assign(*[a & MASK if i in (2, 5) else a for i, a in enumerate(args)])
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sinit_addr.restype, lib.tpw_sinit_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sinit_alloc.restype, lib.tpw_sinit_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sinit_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sinit_set_high.argtypes = [C.c_uint64]
    lib.tpw_sinit_shifts.restype = C.c_int32
    lib.tpw_sinit_assign.restype = C.c_int32
    lib.tpw_sinit_assign.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sinit_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    ctx = {}
    if case["op"] == "sinit":
        s.words(s.g("zero"), [case["zero"]])
        s.words(s.g("mask"), [case["mask"]])
    elif case["op"] == "assign":
        scap = case["scap"]
        data = s.alloc(max(scap, 1) * 4)
        s.words(data, case["sdata"][:scap])
        sd = s.alloc(16)
        s.words(sd, [scap, 0, 0, data])
        cap = case["cap"]
        ob = 0
        if case["hasblk"]:
            ob = s.alloc(max(cap, 1) * 4)
            s.words(ob, case["old"][:cap])
        this = s.alloc(16)
        s.words(this, [cap, case["size"], case["w8"], ob])
        ctx.update(data=data, sd=sd, ob=ob, this=this)
    return ctx


def execute(s, case, ctx):
    if case["op"] == "sinit":
        return s.call(FN_SINIT, [])
    if case["op"] == "shifts":
        return s.call(FN_SHIFTS, [])
    a = case["a"]
    n = case["n"]
    return s.call(FN_ASSIGN, [ctx["this"], ctx["sd"], a, ctx["data"] + 4 * case["p0"], ctx["sd"], a + n, 0x1234])


def observe(s, case, ctx, answer):
    r = {"log": s.log()}
    if case["op"] == "sinit":
        r["zero"] = s.rd_word(s.g("zero"))
        r["mask"] = s.rd_word(s.g("mask"))
    elif case["op"] == "shifts":
        r["answer"] = answer & 0xFFFF
    else:
        w = s.rd_words(ctx["this"], 4)
        blk = w[3]
        if blk == 0:
            content = []
        elif blk == ctx["ob"] & MASK:
            content = s.rd_words(s.ptr(blk), case["cap"])
        else:
            content = s.rd_words(s.ptr(blk), case["n"] & MASK)
        w[3] = 0 if blk == 0 else (1 if blk == ctx["ob"] & MASK else 2)
        r["this"] = w
        r["block"] = content
        r["src"] = s.rd_words(ctx["data"], max(case["scap"], 1))
        r["srcdeque"] = s.rd_words(ctx["sd"], 3)
    return r


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


# ---------------------------------------------------------------------------------------------------------------- cases

def sinit_case(r):
    return {"op": "sinit", "queues": {}, "zero": r.getrandbits(32), "mask": r.getrandbits(32)}


def sinit_edges():
    r = random.Random(5907)
    yield sinit_case(r)
    c = sinit_case(r)
    c["zero"] = 0
    c["mask"] = 0x10000
    yield c


KEYS = [0, -1, -32768, 1, 0x7FFF, -2]


def shifts_case(r, keys=None):
    keys = keys or [r.choice(KEYS) for _ in range(3)]
    return {"op": "shifts", "queues": {"GetKeyState": keys}}


def shifts_edges():
    for bits in range(8):
        yield shifts_case(None, [-1 if bits & (1 << i) else 0 for i in range(3)])
    yield shifts_case(None, [-32768, 0x7FFF, -2])
    yield shifts_case(None, [0x7FFF, 1, 0x7FFF])


def shifts_random(r):
    return shifts_case(r)


def assign_case(r, cap=None, n=None, scap=None, p0=None, hasblk=None, throw=False):
    cap = r.choice([0, 1, 2, 3, 5, 8]) if cap is None else cap
    if throw:
        n = r.choice([0x40000000, 0x40000001, 0x80000000, 0xFFFFFFFF, 0xFFFFFFF0])
    else:
        n = r.choice([0, 1, 2, cap, cap + 1, max(cap - 1, 0), r.randrange(0, 14)]) if n is None else n
    scap = r.randrange(1, 9) if scap is None else scap
    p0 = r.randrange(0, scap) if p0 is None else p0
    if hasblk is None:
        hasblk = cap >= n or r.random() < 0.9
        if cap >= n and n != 0:
            hasblk = True
        elif cap < n and r.random() < 0.8:
            hasblk = True
    if not throw and not hasblk and n != 0 and cap >= n:
        hasblk = True
    a = r.choice([0, 1, r.getrandbits(32), 0xFFFFFFFE, r.randrange(0, 50)])
    return {"op": "assign", "queues": {}, "cap": cap, "n": n, "scap": scap, "p0": p0, "hasblk": hasblk, "a": a,
            "size": r.getrandbits(32), "w8": r.getrandbits(32), "old": [r.getrandbits(32) for _ in range(8)],
            "sdata": [r.getrandbits(32) for _ in range(8)]}


def assign_edges():
    r = random.Random(10229)
    yield assign_case(r, cap=0, n=0, hasblk=False)
    yield assign_case(r, cap=0, n=0, hasblk=True)
    yield assign_case(r, cap=3, n=0)
    yield assign_case(r, cap=3, n=3)
    yield assign_case(r, cap=3, n=2)
    yield assign_case(r, cap=3, n=4)
    yield assign_case(r, cap=0, n=1, hasblk=False)
    yield assign_case(r, cap=0, n=1, hasblk=True)
    yield assign_case(r, cap=8, n=8, scap=3, p0=2)
    yield assign_case(r, cap=2, n=9, scap=3, p0=2)
    yield assign_case(r, cap=2, n=7, scap=1, p0=0)
    for k in range(4):
        yield assign_case(r, throw=True)
    yield assign_case(r, cap=3, throw=True, hasblk=False)


def assign_random(r):
    return assign_case(r, throw=r.random() < 0.05)


TESTS = [
    FnTest("__sinit_online_cpp", ppc, host, edge=sinit_edges, rnd=sinit_case),
    FnTest("CheckShifts", ppc, host, edge=shifts_edges, rnd=shifts_random),
    FnTest("std::__cdeque<const_void*,std::allocator<const_void*>>::do_assign<std::__cdeque<std::vector<Argument,std::allocator<Argument>>*,std::allocator<std::vector<Argument,std::allocator<Argument>>*>>::iterator>",
           ppc, host, edge=assign_edges, rnd=assign_random),
]
