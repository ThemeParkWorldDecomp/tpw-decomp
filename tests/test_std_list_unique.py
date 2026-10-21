"""Unit v5683, std::list<unsigned int>::unique and ::sort, against libtpw_unit_std_list_unique.dylib (tests/unit_std_list_unique.cpp), main app,
src/game/MslSupport.cpp:
  unique 0x10208dcc, sort 0x10208c04

A case is a list of up to 8 values (small range, so runs of equals occur) built twice by one piece of code (Mac memory / host arena). Callees outside
the unit are fakes on both sides: merge 0x102095a0 (records both lists' sizes and up to 5 values each; drains the second list when the case says so)
and operator delete (records the node's value). Compared: the event log, the list size, the values forward, the prev links and the sentinel's prev.

No fused operations. Not tested: a list whose size word is negative (sort would walk off the list).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_list_unique"
MASK = 0xFFFFFFFF

FN_UNIQUE, FN_SORT = 0x10208DCC, 0x10208C04
SIZES = {FN_UNIQUE: 308, FN_SORT: 388}
OWN = set(SIZES)
MERGE = 0x102095A0
DELETE_GLUE = 0x10238218

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def _vals(e, lst):
    s = lst + 4
    out = []
    c = e.u32(lst + 8)
    for _ in range(5):
        if c == s:
            out.append(0x1FFFFFFFF)
            continue
        out.append(e.u32(c + 8))
        c = e.u32(c + 4)
    return out


def stub_merge(e):
    L = e.cl
    a, b = e.r(3), e.r(4)
    L.rec("merge", tuple([e.u32(a), e.u32(b)] + _vals(e, a)[:] + _vals(e, b)[:])[:12])
    if L.ret("merge"):
        s = b + 4
        e.write(b, (0).to_bytes(4, "big") + s.to_bytes(4, "big") + s.to_bytes(4, "big"))


def stub_delete(e):
    e.cl.rec("delete", (e.u32(e.r(3) + 8),))


STUBS.internal(MERGE, stub_merge)
STUBS.imp("__dl__FPv", stub_delete)


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        import struct
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        import struct
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

    def call(self, fn, lst):
        T.call_ppc(self.e, fn, [lst])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        r = self.lib.tpw_sul_alloc(n)
        self.base = self.lib.tpw_sul_base()
        return r

    def read(self, a, n):
        return C.string_at(self.base + a, n)

    def write(self, a, data):
        C.memmove(self.base + a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, lst):
        (self.lib.tpw_sul_sort if fn == FN_SORT else self.lib.tpw_sul_unique)(lst)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sul_alloc.restype, lib.tpw_sul_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sul_base.restype, lib.tpw_sul_base.argtypes = C.c_uint64, []
    lib.tpw_sul_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sul_sort.restype, lib.tpw_sul_sort.argtypes = None, [C.c_uint64]
    lib.tpw_sul_unique.restype, lib.tpw_sul_unique.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sul_layout(out)
    assert list(out)[:n] == [12, 12], list(out)[:n]
    lib.bound = True


def build(s, case):
    lst = s.alloc(12)
    sent = lst + 4
    nodes = [s.alloc(12) for _ in case["vals"]]
    chain = [sent] + nodes + [sent]
    for i, n in enumerate(nodes):
        s.words(n, [chain[i], chain[i + 2], case["vals"][i]])
    s.words(lst, [len(nodes), chain[-2], chain[1]])
    return lst


def observe(s, lst):
    sent = lst + 4
    size, last, cur = s.rd_words(lst, 3)
    vals, links = [], []
    prev = sent
    for _ in range(40):
        if cur == sent:
            break
        n = s.rd_words(cur, 3)
        links.append(n[0] == prev)
        vals.append(n[2])
        prev = cur
        cur = n[1]
    return {"size": size, "vals": vals, "links": links, "tail": last == prev, "log": s.log()}


def run(s, case):
    try:
        lst = build(s, case)
        s.call(case["fn"], lst)
        return observe(s, lst)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, fn, vals=None):
    if vals is None:
        n = r.choice([0, 1, 2, 2, 3, 4, 5, 6, 7, 8])
        hi = r.choice([1, 2, 3, 100])
        vals = [r.randrange(hi + 1) for _ in range(n)]
    return {"fn": fn, "vals": vals, "queues": {"merge": [r.choice([0, 1])]}}


def edges(fn):
    r = random.Random(fn)
    for vals in ([], [5], [1, 2], [2, 1], [3, 3], [1, 1, 1], [1, 1, 2], [2, 1, 1], [1, 2, 2], [1, 2, 3], [3, 2, 1],
                 [0xFFFFFFFF, 0], [0, 0xFFFFFFFF], [4, 4, 5, 5, 5, 6, 6], [1, 2, 1, 2, 1, 2], [7] * 8, [1, 1, 2, 3, 3, 3, 4, 4]):
        for drain in (0, 1):
            c = mk(r, fn, vals)
            c["queues"]["merge"] = [drain]
            yield c


TESTS = [
    FnTest("std::list<unsigned_int,std::allocator<unsigned_int>>::unique", ppc, host, edge=lambda: edges(FN_UNIQUE),
           rnd=lambda r: mk(r, FN_UNIQUE)),
    FnTest("std::list<unsigned_int,std::allocator<unsigned_int>>::sort", ppc, host, edge=lambda: edges(FN_SORT),
           rnd=lambda r: mk(r, FN_SORT)),
]
