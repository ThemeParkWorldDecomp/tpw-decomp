"""Unit v5925: the CodeWarrior cdeque iterator operator-=, queue<errorthingy>::~queue and __cdeque<const void*>::reserve, against
libtpw_unit_std___cdeque_iterator_operator_3.dylib (tests/unit_std___cdeque_iterator_operator_3.cpp), main app, src/MinAnalyserValue_std___cdeque_iterator_operator_3.cpp:
  std::__cdeque<const_void*,std::allocator<const_void*>>::iterator::operator-= 0x101f5bdc
  std::queue<errorthingy,std::deque<errorthingy,std::allocator<errorthingy>>>::~queue 0x101f5200
  std::__cdeque<const_void*,std::allocator<const_void*>>::reserve 0x101f2730

A case is a world and a call, built by one piece of Python code in both memories. The callees outside the unit (iterator::operator+=,
tear_down, ~__cdeque, max_size, strlen, strcpy, operator new, new[], delete, __throw) are fakes on both sides that record their
arguments (addresses relative to the object under test, texts as a hash); max_size answers from the case's queue.

What is compared: the iterator's index and pointer (relative to its block), the queue's answer, the cdeque's four words, the new
block's contents, the event log. No fused operations. Not tested: the unreachable positive branch inside the negative-count path of
operator-= (its count is non-negative there), a block pointer into a block smaller than the deque's capacity, a size above the
capacity in reserve (the original reads past the block).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___cdeque_iterator_operator_3"
MASK = 0xFFFFFFFF

FN_SUB, FN_QUEUE, FN_RESERVE = 0x101F5BDC, 0x101F5200, 0x101F2730
SIZES = {FN_SUB: 316, FN_QUEUE: 104, FN_RESERVE: 660}
OWN = set(SIZES)
HOST_LAYOUT = [16, 4, 28]

A_ITERADD, A_MAXSIZE, A_TEARDOWN, A_CDTOR = 0x101F2BEC, 0x101F492C, 0x101F4A74, 0x101F2520
A_DELETE, A_STRLEN, A_NEWARR, A_NEW, A_STRCPY, A_THROW = 0x10238218, 0x10238650, 0x10238668, 0x102384B8, 0x10238410, 0x10238680
VT_LEN, VT_B, VT_C, DTOR_TV = 0x10278B74, 0x10278BB0, 0x10278BC0, 0x10265680


def s32(v):
    return T.s32(v)


def h(text):
    v = 0
    for b in text:
        v = (v * 31 + b) & MASK
    return v


def cs(e, a):
    return h(e.cstr(a).encode("latin1") if isinstance(e.cstr(a), str) else bytes(e.cstr(a)))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rel(e, p):
    return -1 if p == 0 else (p - e.base) & MASK


def st_iteradd(e):
    e.cl.rec("IterAdd", (s32(e.r(4)),))


def st_maxsize(e):
    e.cl.rec("MaxSize", ())
    e.set_r(3, e.cl.ret("MaxSize") & MASK)


def st_teardown(e):
    e.cl.rec("TearDown", (rel(e, e.r(3)),))


def st_cdtor(e):
    e.cl.rec("CdequeDtor", (rel(e, e.r(3)), T.s16(e.r(4))))


def st_delete(e):
    p = e.r(3)
    if e.mode == "queue":
        e.cl.rec("Delete", (rel(e, p),))
    else:
        e.cl.rec("Delete", (1 if p == e.oldblock else 0,))


def st_strlen(e):
    e.cl.rec("Strlen", (cs(e, e.r(3)),))
    e.set_r(3, len(e.cstr(e.r(3))))


def st_strcpy(e):
    e.cl.rec("Strcpy", (cs(e, e.r(4)),))
    d, s = e.r(3), e.r(4)
    e.write(d, e.cstr(s).encode("latin1") + b"\0" if isinstance(e.cstr(s), str) else bytes(e.cstr(s)) + b"\0")


def st_new(e):
    n = e.r(3)
    e.cl.rec("New", (n,))
    e.set_r(3, e.alloc(max(n, 4), bytes(max(n, 4))))


def st_newarr(e):
    n = e.r(3)
    e.cl.rec("NewArray", (n,))
    e.set_r(3, e.alloc(max(n, 4), bytes(max(n, 4))))


def st_throw(e):
    o = e.r(4)
    vt = e.u32(o)
    vtid = {VT_LEN: 1, VT_B: 2, VT_C: 3}.get(vt, 0)
    cnt = e.u32(o + 8)
    e.cl.rec("Throw", (cs(e, e.r(3)), vtid, 1 if e.r(5) == DTOR_TV else 0, cs(e, e.u32(o + 4)), s32(e.u32(cnt)) if cnt else -1))


STUBS.internal(A_ITERADD, st_iteradd)
STUBS.internal(A_MAXSIZE, st_maxsize)
STUBS.internal(A_TEARDOWN, st_teardown)
STUBS.internal(A_CDTOR, st_cdtor)
def glue(fn):
    """The import glue saves the TOC in 0x14(r1); the caller reloads it after the call."""
    def wrapped(e):
        e.write((e.r(1) + 0x14) & MASK, (e.r(2) & MASK).to_bytes(4, "big"))
        fn(e)
    return wrapped


STUBS.internal(A_DELETE, glue(st_delete))
STUBS.internal(A_STRLEN, glue(st_strlen))
STUBS.internal(A_NEWARR, glue(st_newarr))
STUBS.internal(A_NEW, glue(st_new))
STUBS.internal(A_STRCPY, glue(st_strcpy))
STUBS.internal(A_THROW, glue(st_throw))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(self.pack(w & MASK) for w in ws))

    def rd_words(self, a, n):
        import struct
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        import struct
        self.pack = lambda w: struct.pack(">I", w)
        self.e = e
        e.cl = CL.PpcLog({}, case["queues"])
        e.base = 0
        e.mode = "reserve"
        e.oldblock = 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n, bytes(n))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.e.cl.log

    def hi(self, a):
        return 0

    def sub(self, cd, idx, ptr, n):
        r3, _, _ = T.call_ppc(self.e, FN_SUB_ADDR(self, cd, idx, ptr), [], )
        return r3


def FN_SUB_ADDR(s, cd, idx, ptr):
    it = s.e.alloc(12)
    s.e.write(it, b"".join(s.pack(w & MASK) for w in (cd, idx, ptr)))
    s.it = it
    return FN_SUB


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        import struct
        self.pack = lambda w: struct.pack("<I", w)
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_cdi_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def hi(self, a):
        return (a >> 32) << 32


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cdi_alloc.restype, lib.tpw_cdi_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cdi_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cdi_sub.restype, lib.tpw_cdi_sub.argtypes = None, [C.c_uint64, C.c_uint32, C.c_uint32, C.c_int32, C.POINTER(C.c_uint32)]
    lib.tpw_cdi_queue.restype, lib.tpw_cdi_queue.argtypes = C.c_uint64, [C.c_uint64, C.c_int16]
    lib.tpw_cdi_reserve.restype, lib.tpw_cdi_reserve.argtypes = None, [C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cdi_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def do_sub(s, cd, idx, ptr, n):
    if isinstance(s, Ppc):
        it = s.e.alloc(12)
        s.write(it, b"".join(s.pack(w & MASK) for w in (cd, idx, ptr)))
        T.call_ppc(s.e, FN_SUB, [it, n & MASK])
        return s.rd_words(it, 3)[1:]
    out = (C.c_uint32 * 2)()
    s.lib.tpw_cdi_sub(cd, idx & MASK, ptr & MASK, s32(n), out)
    return list(out)


def sub_world(s, case):
    cap, size, start = case["cap"], case["size"], case["start"]
    cd = s.alloc(16)
    block = s.alloc(cap * 4 + 16)
    s.words(cd, [cap, size, start, block])
    return cd, block


def run_sub(s, case):
    try:
        cd, block = sub_world(s, case)
        idx, ptr = do_sub(s, cd, case["idx"], block + 4 * case["off"], case["n"])
        return {"idx": idx, "ptr": (ptr - block) & MASK, "log": s.log()}
    finally:
        s.done()


def run_queue(s, case):
    try:
        if isinstance(s, Ppc):
            s.e.mode = "queue"
        self_addr = s.alloc(0x1C) if case["alive"] else 0
        if isinstance(s, Ppc):
            s.e.base = self_addr
            r3, _, _ = T.call_ppc(s.e, FN_QUEUE, [self_addr, case["flag"] & MASK])
            ans = r3 & MASK
        else:
            ans = s.lib.tpw_cdi_queue(self_addr, T.s16(case["flag"])) & MASK
        return {"ans": 0 if self_addr == 0 else (ans - self_addr) & MASK, "log": s.log()}
    finally:
        s.done()


def run_reserve(s, case):
    try:
        cap, size, start, n = case["cap"], case["size"], case["start"], case["n"]
        cd = s.alloc(16)
        block = 0
        if case["has"]:
            block = s.alloc(cap * 4 + 16)
            s.words(block, case["data"])
        s.words(cd, [cap, size, start, block])
        if isinstance(s, Ppc):
            s.e.oldblock = block
            T.call_ppc(s.e, FN_RESERVE, [cd, n])
        else:
            s.lib.tpw_cdi_reserve(cd, n)
        w = s.rd_words(cd, 4)
        state = 0 if w[3] == 0 else (1 if w[3] == block & MASK else 2)
        contents = s.rd_words(s.hi(cd) | w[3], w[0]) if w[3] and w[0] <= 4096 else []
        return {"words": w[:3], "state": state, "contents": contents, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return {"sub": run_sub, "queue": run_queue, "reserve": run_reserve}[case["op"]](Ppc(e, case), case)


def host(lib, case):
    return {"sub": run_sub, "queue": run_queue, "reserve": run_reserve}[case["op"]](Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

INT_MIN = -0x80000000


def sub_case(r, n=None, cap=None, start=None, idx=None, off=None):
    cap = r.randrange(1, 40) if cap is None else cap
    start = r.randrange(0, cap) if start is None else start
    idx = r.randrange(0, 40) if idx is None else idx
    if n is None:
        n = r.choice([0, 1, -1, INT_MIN, r.randrange(-60, 60), r.randrange(-(1 << 31), 1 << 31), idx + start, idx + start + 1,
                      idx + start - 1, -(cap - start - idx), -(cap - start - idx) + 1, -(cap - start - idx) - 1, cap, -cap])
    return {"op": "sub", "queues": {}, "cap": cap, "size": r.randrange(0, cap + 1), "start": start, "idx": idx,
            "off": r.randrange(0, cap + 1) if off is None else off, "n": s32(n)}


def sub_edges():
    r = random.Random(11)
    for n in (0, 1, -1, INT_MIN, INT_MIN + 1, 0x7FFFFFFF):
        for _ in range(3):
            yield sub_case(r, n)
    # idx + start boundary on the positive side, sum == cap boundary on the negative side
    for cap, start, idx in ((10, 3, 4), (10, 0, 0), (1, 0, 0), (8, 7, 5)):
        t = idx + start
        for n in (t - 1, t, t + 1, -(cap - t) + 1, -(cap - t), -(cap - t) - 1):
            yield sub_case(r, n, cap, start, idx)
    for _ in range(10):
        yield sub_case(r)


def sub_random(r):
    return sub_case(r)


def queue_case(r, alive=None, flag=None):
    return {"op": "queue", "queues": {}, "alive": r.random() < 0.8 if alive is None else alive,
            "flag": r.choice([0, 1, -1, 2, 0x7FFF, 0x8000, 0x10001, 0x18000, 0xFFFF]) if flag is None else flag}


def queue_edges():
    r = random.Random(12)
    yield queue_case(r, False, 1)
    yield queue_case(r, False, 0)
    for f in (-1, 0, 1, 2, 0x7FFF, 0x8000, 0x10000, 0x10001, 0x18000, 0xFFFF):
        yield queue_case(r, True, f)


def queue_random(r):
    return queue_case(r)


def reserve_case(r, n=None, cap=None, has=None, big=None):
    cap = r.randrange(1, 20) if cap is None else cap
    size = r.randrange(0, cap + 1)
    start = r.randrange(0, cap)
    if n is None:
        n = r.choice([0, 1, cap - 1, cap, cap + 1, cap + 2, r.randrange(0, 60)])
    maxsize = r.choice([0x3FFFFFFF, 0x3FFFFFFF, 0x40000000, n, n + 1, max(n - 1, 0), 5, 0]) if big is None else big
    return {"op": "reserve", "queues": {"MaxSize": [maxsize]}, "cap": cap, "size": size, "start": start, "n": n,
            "has": r.random() < 0.8 if has is None else has, "data": [r.getrandbits(32) for _ in range(cap + 4)]}


def reserve_edges():
    r = random.Random(13)
    for cap in (1, 5):
        for n in (0, cap - 1, cap, cap + 1, cap + 9):
            for has in (True, False):
                for big in (0x3FFFFFFF, n, n - 1 if n else 0):
                    yield reserve_case(r, n, cap, has, big)
    for _ in range(10):
        yield reserve_case(r)


def reserve_random(r):
    return reserve_case(r)


TESTS = [
    FnTest("std::__cdeque<const_void*,std::allocator<const_void*>>::iterator::operator-=", ppc, host, edge=sub_edges, rnd=sub_random),
    FnTest("std::queue<errorthingy,std::deque<errorthingy,std::allocator<errorthingy>>>::~queue", ppc, host, edge=queue_edges, rnd=queue_random),
    FnTest("std::__cdeque<const_void*,std::allocator<const_void*>>::reserve", ppc, host, edge=reserve_edges, rnd=reserve_random),
]
