"""Unit v5305, std::list<unsigned int>::erase and clear, against libtpw_unit_std_list_erase.dylib (tests/unit_std_list_erase.cpp), main app,
src/std_list_erase.cpp:
  erase 0x1020919c (188), clear 0x102090dc (124)

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian, host memory little-endian).
operator delete (0x10238218) is a fake on both sides that records the data word of the node it is given.

The world: a list block (size, sentinel prev, sentinel next), a ring of N nodes (prev, next, data) around the sentinel at list + 4, a size
word that need not equal N. Compared: the log of deletes, the size, the sentinel, every node's links (as indexes, deleted nodes keep theirs),
the stored iterator. No fused operations. Not tested: first after last (the original loops past the sentinel).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_list_erase"
MASK = 0xFFFFFFFF

FN_ERASE, FN_CLEAR = 0x1020919C, 0x102090DC
SIZES = {FN_ERASE: 188, FN_CLEAR: 124}
OWN = set(SIZES)
CALLEE_DELETE = 0x10238218

HOST_LAYOUT = [12, 8, 12]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_delete(e):
    e.cl.rec("delete", (e.u32(e.r(3) + 8),))


STUBS.internal(CALLEE_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def w(self, a):
        return a

    def un(self, v):
        return v

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def erase(self, ret, lst, first, last):
        T.call_ppc(self.e, FN_ERASE, [ret, lst, first, last])

    def clear(self, lst):
        T.call_ppc(self.e, FN_CLEAR, [lst])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_sl_alloc(n)

    def w(self, a):
        """The word that stands for the native address a: its arena offset."""
        return self.lib.tpw_sl_off(a)

    def un(self, v):
        return self.lib.tpw_sl_ptr(v)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def erase(self, ret, lst, first, last):
        self.lib.tpw_sl_erase(ret, lst, first, last)

    def clear(self, lst):
        self.lib.tpw_sl_clear(lst)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sl_addr.restype, lib.tpw_sl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sl_alloc.restype, lib.tpw_sl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sl_erase.restype, lib.tpw_sl_erase.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32]
    lib.tpw_sl_clear.restype, lib.tpw_sl_clear.argtypes = None, [C.c_uint64]
    lib.tpw_sl_off.restype, lib.tpw_sl_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_sl_ptr.restype, lib.tpw_sl_ptr.argtypes = C.c_uint64, [C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    n = case["n"]
    s.lst = s.alloc(12)
    s.nodes = [s.alloc(12) for _ in range(n)]
    s.ret = s.alloc(4)
    s.ring = [s.lst + 4] + s.nodes          # ring[0] is the sentinel; list order is nodes then sentinel
    order = s.nodes + [s.lst + 4]
    m = len(order)
    for i, a in enumerate(order):
        prev, nxt = s.w(order[(i - 1) % m]), s.w(order[(i + 1) % m])
        if i == m - 1:
            s.words(a, [prev, nxt])
        else:
            s.words(a, [prev, nxt, 1000 + i])
    s.words(s.lst, [case["size"]])
    s.words(s.ret, [0xDEADBEEF])
    s.index = {s.w(a): i for i, a in enumerate(order)}


def idx(s, v):
    return s.index.get(v, ("raw", 1 if v else 0))


def execute(s, case):
    n = case["n"]
    pos = lambda p: s.w(s.nodes[p] if p < n else s.lst + 4)
    if case["op"] == "erase":
        s.erase(s.ret, s.lst, pos(case["a"]), pos(case["b"]))
    else:
        s.clear(s.lst)


def observe(s, case):
    order = s.nodes + [s.lst + 4]
    st = []
    for a in order:
        w = s.rd_words(a, 2)
        st.append([idx(s, w[0]), idx(s, w[1])])
    data = [s.rd_words(a, 3)[2] for a in s.nodes]
    return {"size": s.rd_words(s.lst, 1)[0], "links": st, "data": data, "ret": idx(s, s.rd_words(s.ret, 1)[0]), "log": s.log()}


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


def mk(op, n, size, a=0, b=0):
    return {"op": op, "queues": {}, "n": n, "size": size, "a": a, "b": b}


def erase_case(r, n=None, a=None, b=None):
    if n is None:
        n = r.randrange(0, 8)
    if a is None:
        a = r.randrange(0, n + 1)
    if b is None:
        b = r.randrange(a, n + 1)
    size = n if r.random() < 0.8 else r.choice([0, 1, n + 3, MASK])
    return mk("erase", n, size, a, b)


def erase_edges():
    r = random.Random(11)
    for n in range(0, 5):
        for a in range(0, n + 1):
            for b in range(a, n + 1):
                yield erase_case(r, n, a, b)


def erase_random(r):
    return erase_case(r)


def clear_case(r, n=None, size=None):
    if n is None:
        n = r.randrange(0, 8)
    if size is None:
        size = r.choice([n, n, 0, 1, 5])
    return mk("clear", n, size)


def clear_edges():
    r = random.Random(12)
    for n in range(0, 4):
        for size in (0, n, 7):
            yield clear_case(r, n, size)


def clear_random(r):
    return clear_case(r)


TESTS = [
    FnTest("std::list<unsigned_int,std::allocator<unsigned_int>>::erase", ppc, host, edge=erase_edges, rnd=erase_random),
    FnTest("std::list<unsigned_int,std::allocator<unsigned_int>>::clear", ppc, host, edge=clear_edges, rnd=clear_random),
]
