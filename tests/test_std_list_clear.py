"""Unit v5132, std::list<CToken>::clear, against libtpw_unit_std_list_clear.dylib (tests/unit_std_list_clear.cpp), main app:
  std::list<CToken,std::allocator<CToken>>::clear 0x102109fc

A case is a list object (size word, sentinel, nodes) built twice by ONE piece of Python code: in emulator memory and in the host
arena. Each node holds a pointer to a handle (reference count at 8, a block pointer at 0xc). operator delete is a fake on both sides
recording the label of the pointer. Pointers in memory are compared as labels.

Compared: the event log, the list words, every node's data word and every handle's count after the call. No fused operations.
Not tested: a node with a NULL handle (the original reads it unconditionally); one handle shared by two nodes.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_list_clear"
MASK = 0xFFFFFFFF

FN = 0x102109FC
CALLEE_DELETE = 0x10238218

STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_delete(e):
    L = e.cl
    L.rec("Delete", (L.lab(e.r(3)),))


STUBS.internal(CALLEE_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def build(self, case):
        """Allocates the objects; labels: list 1, node i 2+3i, handle 3+3i, block 4+3i."""
        n = len(case["nodes"])
        self.lst = self.alloc(12)
        self.nodes = [self.alloc(12) for _ in range(n)]
        self.handles = [self.alloc(16) for _ in range(n)]
        self.blocks = [self.alloc(4) for _ in range(n)]
        self.labels = {self.lst: 1}
        for i in range(n):
            self.labels[self.nodes[i]] = 2 + 3 * i
            self.labels[self.handles[i]] = 3 + 3 * i
            self.labels[self.blocks[i]] = 4 + 3 * i

    def fill(self, case):
        n = len(case["nodes"])
        sent = self.lst + 4
        chain = [sent] + self.nodes + [sent]
        for i in range(n):
            self.words(self.nodes[i], [self.w(chain[i]), self.w(chain[i + 2]), self.w(self.handles[i])])
            self.words(self.handles[i] + 8, [case["nodes"][i] & MASK, self.w(self.blocks[i])])
        nxt = self.nodes[0] if n else sent
        prv = self.nodes[-1] if n else sent
        self.words(self.lst, [case["size"], self.w(prv), self.w(nxt)])

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def lab_of_word(self, word):
        if word == 0:
            return 0
        for a, l in self.labels.items():
            if self.w(a) == word:
                return l
        return -1

    def observe(self, case):
        out = {"log": self.log()}
        r = self.rd_words(self.lst, 3)
        out["list"] = [r[0], self.lab_of_word(r[1]), self.lab_of_word(r[2])]
        out["nodes"] = [self.lab_of_word(self.rd_word(self.nodes[i] + 8)) for i in range(len(self.nodes))]
        out["counts"] = [self.rd_word(self.handles[i] + 8) for i in range(len(self.nodes))]
        return out


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.build(case)
        self.L = e.cl = CL.PpcLog(dict(self.labels), case["queues"])
        self.saved = STUBS.activate(e)

    def w(self, a):
        return a

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self):
        T.call_ppc(self.e, FN, [self.lst & MASK])

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.build(case)
        CL.host_begin(lib, {l: a for a, l in self.labels.items()}, case["queues"])

    def w(self, a):
        return self.lib.tpw_slc_off(a) & MASK

    def alloc(self, n):
        return self.lib.tpw_slc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_slc_clear(self.lst)

    def done(self):
        pass


HOST_LAYOUT = [12, 12, 16, 8, 12, 4]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_slc_alloc.restype, lib.tpw_slc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_slc_off.restype, lib.tpw_slc_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_slc_clear.restype, lib.tpw_slc_clear.argtypes = None, [C.c_uint64]
    lib.tpw_slc_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_slc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.fill(case)
        s.call()
        return s.observe(case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, counts, size=None):
    return {"queues": {}, "nodes": list(counts), "size": (r.randrange(1, 10) if size is None else size)}


def edges():
    r = random.Random(5132)
    yield mk(r, [], 0)
    yield mk(r, [], 3)
    yield mk(r, [1], 0)
    yield mk(r, [0, 0, 2, 1, 5, 3], 0)
    for counts in ([0], [1], [2], [0xFFFFFFFF], [0x80000000], [1, 1], [0, 2], [2, 0, 1], [1, 1, 1, 1, 1]):
        yield mk(r, counts, 1)


def rnd(r):
    n = r.randrange(0, 7)
    return mk(r, [r.choice([0, 1, 2, 3, 7, 0xFFFFFFFF, r.randrange(0, 1 << 32)]) for _ in range(n)],
              r.choice([0, 1, n, r.randrange(0, 1 << 32)]))


TESTS = [
    FnTest("std::list<CToken,std::allocator<CToken>>::clear", ppc, host, edge=edges, rnd=rnd),
]
