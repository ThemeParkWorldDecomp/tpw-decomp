"""Unit v5701, std::__tree<wstring,...>::erase_one<wstring> 0x1021e664, against libtpw_unit_std___tree_std_basic_string_std_less_std_basic_s_2.dylib.

A case is a tree (a binary search tree built from a list of keys, nodes in memory on each side with that side's pointer width) and a query
key. A string's first word is its key. The callees less<wstring>::operator() (0x1021eb7c, answers key a < key b unsigned) and __tree::erase
(0x10228104) are fakes on both sides that log their arguments (the keys). Compared: the answer, the log, the count and root key.
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___tree_std_basic_string_std_less_std_basic_s_2"
MASK = 0xFFFFFFFF

FN = 0x1021E664
SIZES = {FN: 220}
LESS = 0x1021EB7C
ERASE = 0x10228104

STUBS = CL.Stubs()
STUBS.real.update(SIZES)


def stub_less(e):
    L = e.cl
    a, b = e.u32(e.r(4)), e.u32(e.r(5))
    L.rec("Less", (L.lab(e.r(3)), a, b))
    e.set_r(3, 1 if a < b else 0)


def stub_erase(e):
    L = e.cl
    L.rec("Erase", (L.lab(e.r(3)), e.u32(e.r(4) + 0xC)))


STUBS.internal(LESS, stub_less)
STUBS.internal(ERASE, stub_erase)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    ROOT, FUNC, RIGHT, PARENT, VAL, NODE = 4, 8, 4, 8, 0xC, 0x10

    def __init__(self, e, case):
        self.e = e
        self.tree = e.alloc(0x20)
        self.L = e.cl = CL.PpcLog({self.tree: 1, self.tree + self.FUNC: 2}, case["queues"])
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

    def call(self, key):
        r3, _, _ = T.call_ppc(self.e, FN, [self.tree, key])
        return r3 & 0xFF


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    ROOT, FUNC, RIGHT, PARENT, VAL, NODE = 8, 16, 8, 16, 0x18, 0x20

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.tree = lib.tpw_wt_alloc(0x20)
        CL.host_begin(lib, {1: self.tree, 2: self.tree + self.FUNC}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_wt_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, key):
        return self.lib.tpw_wt_erase_one(self.tree, key) & 0xFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_wt_alloc.restype, lib.tpw_wt_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_wt_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_wt_erase_one.restype, lib.tpw_wt_erase_one.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_wt_layout(out)
    assert list(out)[:n] == [8, 16, 24], list(out)[:n]
    lib.bound = True


def build(s, case):
    # BST by insertion order (duplicates go right)
    nodes = []   # [key, addr, left, right]
    for k in case["keys"]:
        addr = s.alloc(s.NODE)
        s.words(addr + s.VAL, [k])
        n = [k, addr, None, None]
        if nodes:
            cur = nodes[0]
            while True:
                side = 2 if k < cur[0] else 3
                if cur[side] is None:
                    cur[side] = n
                    break
                cur = cur[side]
        nodes.append(n)
    for n in nodes:
        s.ptr(n[1], n[2][1] if n[2] else 0)
        s.ptr(n[1] + s.RIGHT, n[3][1] if n[3] else 0)
    s.words(s.tree, [len(nodes)])
    s.ptr(s.tree + s.ROOT, nodes[0][1] if nodes else 0)
    s.root = nodes[0][1] if nodes else 0
    q = s.alloc(8)
    s.words(q, [case["q"]])
    return q


def run(s, case):
    try:
        q = build(s, case)
        ans = s.call(q)
        root = s.rd_word(s.root + s.VAL) if s.root else None
        return {"answer": ans, "log": s.log(), "count": s.rd_word(s.tree), "root": root}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(keys, q):
    return {"keys": keys, "q": q, "queues": {}}


def edges():
    yield mk([], 5)
    yield mk([5], 5)
    yield mk([5], 4)
    yield mk([5], 6)
    yield mk([5, 3, 8], 3)
    yield mk([5, 3, 8], 8)
    yield mk([5, 3, 8], 5)
    yield mk([5, 3, 8], 4)
    yield mk([5, 3, 8], 9)
    yield mk([5, 3, 8], 1)
    yield mk([5, 5, 5], 5)
    yield mk([1, 2, 3, 4], 3)
    yield mk([4, 3, 2, 1], 2)
    yield mk([0x80000000, 1], 1)
    yield mk([0x80000000, 1], 0x80000000)


def rnd(r):
    n = r.randrange(0, 9)
    keys = [r.randrange(0, 12) for _ in range(n)]
    return mk(keys, r.randrange(0, 12))


TESTS = [
    FnTest("std::__tree<std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>,std::less<std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>>,std::allocator<std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>>>::erase_one<std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>>", ppc, host, edge=edges, rnd=rnd),
]
