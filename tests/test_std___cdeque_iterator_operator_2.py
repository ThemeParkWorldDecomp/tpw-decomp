"""Unit v5520, std::__cdeque<const void*>::iterator::operator+= 0x101f2bec, against libtpw_unit_std___cdeque_iterator_operator_2.dylib.

A case is a deque (capacity, size, start, block) and an iterator (deque, index, element pointer inside the block), built by one piece
of code on both sides (the host iterator has native 8-byte pointers: layout 0/8/16). Compared: the answer (the iterator address as
an offset from its own base), the index, the pointer as an offset from the block, the deque words. No callees, no fused operations.
Not tested: n = INT32_MIN (the original recurses forever).
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___cdeque_iterator_operator_2"
MASK = 0xFFFFFFFF
FN = 0x101F2BEC


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    fmt = ">I"

    def __init__(self, e):
        self.e = e

    def base(self):
        return 0

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, d):
        self.e.write(a, d)

    def mk_iter(self, deq, idx, ptr):
        a = self.alloc(12)
        self.write(a, struct.pack(">3I", deq, idx, ptr))
        return a

    def rd_iter(self, a):
        return struct.unpack(">3I", self.read(a, 12))

    def call(self, it, n):
        r3, _, _ = T.call_ppc(self.e, FN, [it, n & MASK])
        return r3 & MASK


class Host:
    fmt = "<I"

    def __init__(self, lib):
        self.lib = lib
        if not getattr(lib, "bound", False):
            lib.tpw_cdit_alloc.restype, lib.tpw_cdit_alloc.argtypes = C.c_uint64, [C.c_uint32]
            lib.tpw_cdit_layout.argtypes = [C.POINTER(C.c_int64)]
            lib.tpw_cdit_add.restype, lib.tpw_cdit_add.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
            lib.tpw_cdit_base.restype, lib.tpw_cdit_base.argtypes = C.c_uint64, []
            out = (C.c_int64 * 32)()
            n = lib.tpw_cdit_layout(out)
            assert list(out)[:n] == [0, 8, 16, 24], list(out)[:n]
            lib.bound = True

    def base(self):
        return self.lib.tpw_cdit_base()

    def alloc(self, n):
        return self.lib.tpw_cdit_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def mk_iter(self, deq, idx, ptr):
        a = self.alloc(24)
        self.write(a, struct.pack("<QIxxxxQ", deq, idx, ptr))
        return a

    def rd_iter(self, a):
        return struct.unpack("<QIxxxxQ", self.read(a, 24))

    def call(self, it, n):
        return self.lib.tpw_cdit_add(it, n)


def run(s, case):
    cap, size, start, idx, k, n = case
    blk = s.alloc(4 * max(cap, 1) + 64)
    deq = s.alloc(16)
    s.write(deq, struct.pack(s.fmt[0] + "4I", cap, size, start, blk - s.base()))
    it = s.mk_iter(deq, idx, blk + 4 * k)
    ans = s.call(it, n)
    d, i, p = s.rd_iter(it)
    return {"same": ans == it, "deq": d - deq, "idx": i, "ptr": p - blk,
            "words": list(struct.unpack(s.fmt[0] + "4I", s.read(deq, 16)))[:3]}


def ppc(e, case):
    return run(Ppc(e), case)


def host(lib, case):
    return run(Host(lib), case)


def mk(r, n=None):
    cap = r.randrange(1, 64)
    start = r.randrange(0, cap)
    idx = r.randrange(0, cap)
    k = r.randrange(0, cap)
    if n is None:
        n = r.choice([0, 1, -1, r.randrange(-80, 80), r.randrange(-3, 4), -(start + idx), -(start + idx) - 1, -(start + idx) + 1,
                      cap - start - idx, cap - start - idx - 1, cap - start - idx + 1])
    return (cap, cap, start, idx, k, n)


def edges():
    r = random.Random(77)
    for n in (0, 1, -1, 2, -2, 5, -5, 40, -40, 100, -100, 0x7FFF, -0x7FFF):
        for _ in range(6):
            yield mk(r, n)
    for _ in range(40):
        yield mk(r)


def rnd(r):
    return mk(r)


TESTS = [
    FnTest("std::__cdeque<const_void*,std::allocator<const_void*>>::iterator::operator+=", ppc, host, edge=edges, rnd=rnd),
]
