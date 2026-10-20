"""Unit v5259, std::list<std::string>::unique, against libtpw_unit_std_list_std_basic_string_std_allocator_std_basi_2.dylib
(tests/unit_std_list_std_basic_string_std_allocator_std_basi_2.cpp), main app, src/std_list_std_basic_string_std_allocator_std_basi_2.cpp:
  std::list<std::string>::unique 0x1020833c

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian, host memory little-endian).
The callees outside the unit are fakes on both sides: the string compare (0x10209cac) records (id of the first string, pos, length of the
first, id of the second, length of the second) and answers from the queue "compare"; the list erase (0x102084d8) records the ids of its
first and last iterator (-1 for the sentinel) and stores the last iterator in the result slot, as the real one answers.

The world of a case: a circular list of 0 to 8 nodes (size word, sentinel, nodes with a string rep of length, id and text pointer).
What is compared: the event log and the chain of ids of the list after the call. No fused operations. Not tested: none.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_list_std_basic_string_std_allocator_std_basi_2"
MASK = 0xFFFFFFFF

FN_UNIQUE = 0x1020833C
SIZES = {FN_UNIQUE: 212}
OWN = set(SIZES)
COMPARE = 0x10209CAC
ERASE = 0x102084D8

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_compare(e):
    L = e.cl
    selfid = e.u32(e.u32(e.r(3)) + 0x10)
    sid = e.u32(e.r(6))
    L.rec("compare", (selfid, e.r(4) & MASK, e.r(5) & MASK, sid, e.r(7) & MASK))
    e.set_r(3, L.ret("compare") & MASK)


def stub_erase(e):
    L = e.cl
    slot, lst, first, last = e.r(3), e.r(4), e.r(5), e.r(6)
    idf = -1 if first == lst + 4 else e.u32(e.u32(first + 8) + 0x10)
    idl = -1 if last == lst + 4 else e.u32(e.u32(last + 8) + 0x10)
    L.rec("erase", (idf, idl))
    e.write(slot, struct.pack(">I", last & MASK))


STUBS.internal(COMPARE, stub_compare)
STUBS.internal(ERASE, stub_erase)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    base = 0

    def ptr(self, a):
        return a - self.base

    def adr(self, w):
        return w + self.base

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

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, lst):
        T.call_ppc(self.e, FN_UNIQUE, [lst & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_sluq_alloc(4)   # the arena base is set by the first allocation
        self.base = lib.tpw_sluq_base()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_sluq_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, lst):
        self.lib.tpw_sluq_unique(lst)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sluq_alloc.restype, lib.tpw_sluq_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sluq_base.restype, lib.tpw_sluq_base.argtypes = C.c_uint64, []
    lib.tpw_sluq_unique.restype, lib.tpw_sluq_unique.argtypes = None, [C.c_uint64]
    lib.tpw_sluq_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sluq_layout(out)
    assert list(out)[:n] == [], list(out)[:n]
    lib.bound = True


def build(s, case):
    ids = case["ids"]
    n = len(ids)
    lst = s.alloc(12)
    nodes = []
    for i, ident in enumerate(ids):
        node = s.alloc(12)
        rep = s.alloc(0x14)
        s.words(rep, [case["lens"][i], 0, 0, s.ptr(rep + 0x10), ident])
        s.words(node + 8, [s.ptr(rep)])
        nodes.append(s.ptr(node))
    end = s.ptr(lst) + 4
    chain = [end] + nodes + [end]
    for i, node in enumerate(nodes):
        s.words(s.adr(node), [chain[i], chain[i + 2]])
    s.words(lst, [n, nodes[-1] if nodes else end, nodes[0] if nodes else end])
    return lst


def run(s, case):
    try:
        lst = build(s, case)
        s.call(lst)
        out = []
        end = s.ptr(lst) + 4
        a = s.rd_word(lst + 8)
        for _ in range(len(case["ids"]) + 2):
            if a == end:
                break
            out.append(s.rd_word(s.adr(s.rd_word(s.adr(a) + 8)) + 0x10))
            a = s.rd_word(s.adr(a) + 4)
        return {"log": s.log(), "chain": out, "size": s.rd_word(lst)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make(ids, answers, lens=None):
    r = random.Random(len(ids) * 7 + len(answers))
    return {"ids": ids, "lens": lens or [r.randrange(0, 50) for _ in ids], "queues": {"compare": answers or [0]}}


def edges():
    yield make([], [0])
    yield make([5], [0])
    yield make([5, 5], [0])
    yield make([5, 6], [1])
    yield make([5, 5, 5], [0, 0])
    yield make([5, 5, 6], [0, 1])
    yield make([5, 6, 6], [1, 0])
    yield make([1, 2, 3], [1])
    yield make([1, 1, 2, 2, 2, 3, 3], [0, 1, 0, 0, 1, 0, 1])
    yield make([1, 1, 1, 1], [0])
    yield make([1, 1, 1, 1], [0, 1, 0, 0, 1, 1])
    yield make([1, 2, 2, 3], [-1, 0, 5, 1])


def rnd(r):
    n = r.randrange(0, 9)
    ids = [r.randrange(1, 100000) for _ in range(n)]
    answers = [r.choice([0, 0, 1, -1, 7]) for _ in range(r.randrange(1, 14))]
    return make(ids, answers, [r.randrange(0, 200) for _ in range(n)])


TESTS = [
    FnTest("std::list<std::basic_string<char,std::char_traits<char>,std::allocator<char>>,std::allocator<std::basic_string<char,std::char_traits<char>,std::allocator<char>>>>::unique",
           ppc, host, edge=edges, rnd=rnd),
]
