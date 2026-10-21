"""Unit v5115, the CodeWarrior std::deque<errorthingy>, against libtpw_unit_std_deque_pop_front.dylib (tests/unit_std_deque_pop_front.cpp),
main app ("SimTheme Park"), src/game/MslSupport.cpp:
  pop_front 0x101f2074, ~deque 0x101f52e0, make_iterator 0x101f53b4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit are fakes on both sides: the block table's operator[] (records its table label and index), tear_down (records the
deque), the cursor's += (records the table label, the block number and the count, then moves the block number and the slot address on
by the count) and operator delete (records the label of the pointer, or its value when it is no known block).

The world of a case: the deque object (7 words), a 64-word block table (random block pointers) and, for make_iterator, a 6-word result.
What is compared: the answer, the object, the table, the result and the event log; addresses of the object and the table are shown
relative to their block so both memories compare. No fused operations. Not tested: a block count under 2 for pop_front on an emptied
deque (the original loops about 2^32 times), a block size of 0 (divwu by 0 is undefined on the hardware), ~deque of the address
0xfffffffc (the host's arena cannot hold it as a 32-bit offset).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_deque_pop_front"
MASK = 0xFFFFFFFF

FN_POP, FN_DTOR, FN_MK = 0x101F2074, 0x101F52E0, 0x101F53B4
SIZES = {FN_POP: 216, FN_DTOR: 124, FN_MK: 240}
OWN = set(SIZES)

CALLEE_INDEX, CALLEE_TEARDOWN, CALLEE_ADD, CALLEE_DELETE = 0x101F2004, 0x101F4A74, 0x101F5508, 0x10238218
TABWORDS = 64

HOST_LAYOUT = [0x1C, 4, 0x14, 0x18, 0x18, 0xC]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_index(e):
    """void* __cdeque::operator[](unsigned long): records the table and the index."""
    e.cl.rec("Index", (e.cl.lab(e.r(3)), e.r(4) & MASK))
    e.set_r(3, 0)


def stub_teardown(e):
    e.cl.rec("TearDown", (e.cl.lab(e.r(3)),))


def stub_add(e):
    """const_iterator::operator+=(long): records, then moves the block number and the slot on."""
    it, n = e.r(3), s32(e.r(4))
    e.cl.rec("CursorAdd", (e.cl.lab(e.u32(it)), e.u32(it + 4), n))
    e.write(it + 4, struct.pack(">I", (e.u32(it + 4) + n) & MASK))
    e.write(it + 8, struct.pack(">I", (e.u32(it + 8) + n * 4) & MASK))


def stub_delete(e):
    p = e.r(3) & MASK
    e.cl.rec("delete", ({e.cl_obj: 1, e.cl_tab: 3}.get(p, p),))


STUBS.internal(CALLEE_INDEX, stub_index)
STUBS.internal(CALLEE_TEARDOWN, stub_teardown)
STUBS.internal(CALLEE_ADD, stub_add)
STUBS.internal(CALLEE_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def make_world(self):
        self.obj = self.alloc(0x20)
        self.tab = self.alloc(4 * TABWORDS)
        self.out = self.alloc(0x18)

    def rel(self, w):
        """A word shown relative to the object or the table when it points into them."""
        w &= MASK
        tab, obj = self.w(self.tab), self.w(self.obj)
        if tab <= w < tab + 4 * TABWORDS + 4:
            return "tab+%d" % (w - tab)
        if obj <= w < obj + 0x20:
            return "obj+%d" % (w - obj)
        return w


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make_world()
        e.cl_obj, e.cl_tab = self.obj, self.tab
        self.L = e.cl =CL.PpcLog({self.obj: 1, self.obj + 4: 2, self.tab: 3}, {})
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def w(self, a):
        """An address as the Mac's word."""
        return a

    def ptr(self, word):
        return word

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def pop(self):
        T.call_ppc(self.e, FN_POP, [self.obj])

    def dtor(self, this, flag):
        return T.call_ppc(self.e, FN_DTOR, [this, flag])[0] & MASK

    def mk(self, n):
        T.call_ppc(self.e, FN_MK, [self.out, self.obj, n])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_edq_base()
        self.make_world()
        lib.tpw_edq_known(self.w(self.obj), self.w(self.tab))
        CL.host_begin(lib, {1: self.obj, 2: self.obj + 4, 3: self.tab}, {})

    def done(self):
        pass

    def w(self, a):
        """An address as the Mac's word: the host's words are offsets from the arena base."""
        return (a - self.base) & MASK

    def ptr(self, word):
        return self.base + word if word else 0

    def alloc(self, n):
        return self.lib.tpw_edq_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def pop(self):
        self.lib.tpw_edq_pop(self.obj)

    def dtor(self, this, flag):
        r = self.lib.tpw_edq_dtor(this, s32(flag))
        return self.w(r) if r else 0

    def mk(self, n):
        self.lib.tpw_edq_mk(self.out, self.obj, n)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_edq_alloc.restype, lib.tpw_edq_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_edq_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_edq_base.restype, lib.tpw_edq_base.argtypes = C.c_uint64, []
    lib.tpw_edq_known.restype, lib.tpw_edq_known.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_edq_pop.restype, lib.tpw_edq_pop.argtypes = None, [C.c_uint64]
    lib.tpw_edq_dtor.restype, lib.tpw_edq_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_edq_mk.restype, lib.tpw_edq_mk.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_edq_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    data = {"tab": s.w(s.tab), "null": 0}.get(case["data"], case["data"])
    s.words(s.obj, [case["bs"], case["cap"], case["count"], case["start"], data, case["first"], case["size"]])
    s.words(s.tab, case["table"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(),
            "obj": [s.rel(w) for w in s.rd_words(s.obj, 7)],
            "tab": [s.rel(w) for w in s.rd_words(s.tab, TABWORDS)],
            "out": [s.rel(w) for w in s.rd_words(s.out, 6)]}


def run(s, case):
    try:
        build(s, case)
        op = case["op"]
        answer = 0
        if op == "pop":
            s.pop()
        elif op == "mk":
            s.mk(case["n"])
        else:
            this = s.ptr({"obj": s.w(s.obj), "null": 0, "m4": 0xFFFFFFFC}[case["this"]])
            answer = s.rel(s.dtor(this, case["flag"]))
        return observe(s, case, answer)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rblock(r):
    return r.choice([0, r.randrange(0x20000000, 0x7FFFFFFF), r.randrange(0x20000000, 0x7FFFFFFF), 0xFFFFFFF0])


def base(r, op, **kw):
    cap = r.randrange(2, 17)
    c = {"op": op, "bs": r.randrange(1, 21), "cap": cap, "count": r.randrange(2, cap + 1), "start": r.randrange(0, cap),
         "data": "tab", "first": r.choice([0, r.randrange(0, 1000), r.randrange(0, 1 << 32)]),
         "size": r.choice([1, 1, 0, 2, 5, r.randrange(0, 1 << 32)]), "table": [rblock(r) for _ in range(TABWORDS)],
         "n": 0, "this": "obj", "flag": 0}
    c.update(kw)
    return c


def pop_edges():
    r = random.Random(11)
    for size in (1, 0, 2, 3, 0xFFFFFFFF):
        for count in (2, 3, 4):
            for first in (0, 7, 0xFFFFFFFF):
                yield base(r, "pop", size=size, count=count, cap=4, start=r.randrange(0, 4), first=first)
    for start in (0, 1, 3):          # the wrap of the slot index past the capacity
        yield base(r, "pop", size=1, count=4, cap=4, start=start)
        yield base(r, "pop", size=1, count=9, cap=9, start=start)
    for bs in (1, 2, 3, 16):
        yield base(r, "pop", bs=bs, size=1)


def pop_random(r):
    return base(r, "pop")


def dtor_edges():
    r = random.Random(12)
    for this in ("obj", "null"):
        for flag in (0, 1, -1, 2, 0x7FFF, -0x8000, 0x10000, 0x18000):
            for data in ("tab", "null", 0x30000000):
                yield base(r, "dtor", this=this, flag=flag, data=data)


def dtor_random(r):
    return base(r, "dtor", this=r.choice(["obj", "obj", "obj", "null"]),
                flag=r.choice([0, 1, -1, 2, r.randrange(-0x8000, 0x8000), r.randrange(0, 1 << 32)]),
                data=r.choice(["tab", "null", r.randrange(0x20000000, 0x7FFFFFFF)]))


def mk_case(r, bs, n, **kw):
    start = kw.pop("start", r.randrange(0, 16))
    return base(r, "mk", bs=bs, n=n, start=start, cap=16, count=kw.pop("count", r.randrange(0, 17)), **kw)


def mk_edges():
    r = random.Random(13)
    for bs in (1, 2, 5, 20):
        for n in (0, 1, bs - 1, bs, bs + 1, 2 * bs - 1, 2 * bs, 3 * bs + 2, 40 * bs):
            if n < 0:
                continue
            for count in (0, 1, 5):
                yield mk_case(r, bs, n, count=count, start=r.choice([0, 1, 15]))


def mk_random(r):
    bs = r.randrange(1, 21)
    return mk_case(r, bs, r.choice([r.randrange(0, bs), r.randrange(bs, 40 * bs), bs, 0]))


TESTS = [
    FnTest("std::deque<errorthingy,std::allocator<errorthingy>>::pop_front", ppc, host, edge=pop_edges, rnd=pop_random),
    FnTest("std::deque<errorthingy,std::allocator<errorthingy>>::~deque", ppc, host, edge=dtor_edges, rnd=dtor_random),
    FnTest("std::deque<errorthingy,std::allocator<errorthingy>>::make_iterator", ppc, host, edge=mk_edges, rnd=mk_random),
]
