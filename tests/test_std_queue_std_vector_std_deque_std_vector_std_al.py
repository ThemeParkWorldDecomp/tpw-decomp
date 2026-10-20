"""Unit v5725, the destructor of std::queue<std::vector<Argument>, std::deque<...>>, against
libtpw_unit_std_queue_std_vector_std_deque_std_vector_std_al.dylib, src/COnlinePerson_SetDest_GotoPark_OK_std_queue_...cpp:
  ~queue 0x10213a08

A case is a queue object of five words (the block table pointer at 0x10) built twice by one piece of code. The callees, the deque's
tear_down (0x1022462c) and operator delete (0x10238218), are fakes on both sides that log which object they got (the queue, the block
table, or another address with its raw word). NULL this and the delete flag (a short; the upper half of r4 is junk) are covered.
Compared: the answer (this returned), the log and the five words. No fused operations. Not tested: nothing.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_queue_std_vector_std_deque_std_vector_std_al"
MASK = 0xFFFFFFFF

FN = 0x10213A08
SIZES = {FN: 132}
OWN = set(SIZES)
TEARDOWN = 0x1022462C
DELETE = 0x10238218
HOST_LAYOUT = [0x10]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cls(e, p):
    ctx = getattr(e, "qctx", (0, 0))
    return 1 if p == ctx[0] else (2 if p == ctx[1] else 0)


def stub_teardown(e):
    e.cl.rec("tear_down", (cls(e, e.r(3)),))


def stub_delete(e):
    c = cls(e, e.r(3))
    e.cl.rec("delete", (c, (e.r(3) & MASK) if c == 0 else 0))


STUBS.internal(TEARDOWN, stub_teardown)
STUBS.internal(DELETE, stub_delete)


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

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, obj, block, flag, junk):
        self.e.qctx = (obj, block)
        r3, _, _ = T.call_ppc(self.e, FN, [obj, ((junk << 16) | (flag & 0xFFFF)) & MASK])
        return 1 if (r3 & MASK) == obj else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_qd_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, obj, block, flag, junk):
        return self.lib.tpw_qd_dtor(obj, T.s16(flag))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_qd_alloc.restype, lib.tpw_qd_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_qd_dtor.restype, lib.tpw_qd_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_qd_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_qd_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        if case["null"]:
            obj, block = 0, 0
            words = None
        else:
            obj = s.alloc(32)
            block = s.alloc(16)
            words = list(case["words"])
            words[4] = {0: 0, 1: block, 2: case["raw"]}[case["mode"]]
            if case["mode"] == 1:
                pass
            s.words(obj, words)
            block = words[4]
        ans = s.call(obj, block, case["flag"], case["junk"])
        back = None if words is None else s.rd_words(obj, 5)
        if back is not None and case["mode"] == 1:
            assert back[4] == block & MASK, (back[4], block)      # addresses differ per side: compare the others
            back[4] = 0xB10C
        return {"answer": ans, "log": s.log(), "words": back}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, null=False, mode=None, flag=None):
    return {"queues": {}, "null": null, "mode": r.choice([0, 1, 2]) if mode is None else mode,
            "flag": r.choice([-1, 0, 1, 2, 0x7FFF, -0x8000, r.randrange(-40000, 40000)]) if flag is None else flag,
            "junk": r.choice([0, 0xFFFF, 1, r.randrange(0, 0x10000)]), "raw": r.randrange(1, 1 << 32) | 1,
            "words": [r.randrange(0, 1 << 32) for _ in range(5)]}


def edges():
    r = random.Random(5725)
    for flag in (-1, 0, 1, 2, 0x7FFF, -0x8000, 0x8000, 0xFFFF):
        for mode in (0, 1, 2):
            yield mk(r, False, mode, flag)
        yield mk(r, True, 0, flag)


def rnd(r):
    return mk(r, r.random() < 0.1)


TESTS = [
    FnTest("std::queue<std::vector<Argument,std::allocator<Argument>>,std::deque<std::vector<Argument,std::allocator<Argument>>,std::allocator<std::vector<Argument,std::allocator<Argument>>>>>::~queue",
           ppc, host, edge=edges, rnd=rnd),
]
