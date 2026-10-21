"""Unit v5196, std::_RefCountedPtr<std::__locale_imp<__int128>,std::_Single<...>>::operator=, against
libtpw_unit_std__RefCountedPtr_std___locale_imp_std__Single_.dylib, src/game/MslSupport.cpp:
  operator= 0x10072128

A case is a world and a call, built by one piece of Python in both memories. Callees outside the unit are fakes on both sides that
record the same events: vector size / operator[] (slot impl + 0x20 + 4i), the facet virtual delete (ptr_glue), CharArray removeRef /
destroy / deallocate, the vector destructor and operator delete. Objects are labelled so the log compares across the two address spaces.

The world: this (impl, count), other (impl, count), the count words, the impl's CharArray word at +0xc and up to 6 facet slots with
reference counts. Compared: the answer (label of this), the log, both objects, the counts, the impl word, the facets' counts.
No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std__RefCountedPtr_std___locale_imp_std__Single_"
MASK = 0xFFFFFFFF

FN = 0x10072128
SIZES = {FN: 368}
OWN = set(SIZES)

C_SIZE, C_INDEX, C_GLUE, C_REMOVE, C_DESTROY, C_DEALLOC, C_VDTOR, C_DELETE = (
    0x10072618, 0x100725a0, 0x1023baa8, 0x10072688, 0x1007272c, 0x10072364, 0x100724d0, 0x10238218)
HOST_LAYOUT = [8, 16, 8, 0x10]
NF = 6

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def st_size(e):
    L = e.cl
    L.rec("size", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("size") & MASK)


def st_index(e):
    L = e.cl
    L.rec("index", (L.lab(e.r(3)), e.r(4) & MASK))
    e.set_r(3, (e.r(3) + 0x20 + 4 * e.r(4)) & MASK)


def st_glue(e):
    L = e.cl
    L.rec("facetdel", (L.lab(e.r(3)), s32(e.r(4))))


def st_remove(e):
    L = e.cl
    L.rec("removeRef", (e.r(3) & MASK,))
    e.set_r(3, L.ret("removeRef") & MASK)


def st_destroy(e):
    L = e.cl
    L.rec("destroy", (L.lab((e.r(3) - 0xc) & MASK), e.r(4) & MASK))


def st_dealloc(e):
    L = e.cl
    L.rec("dealloc", (L.lab((e.r(3) - 0xc) & MASK), e.r(4) & MASK, e.r(5) & MASK))


def st_vdtor(e):
    L = e.cl
    L.rec("vecdtor", (L.lab(e.r(3)), s32(e.r(4))))


def st_delete(e):
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)),))


for _a, _f in ((C_SIZE, st_size), (C_INDEX, st_index), (C_GLUE, st_glue), (C_REMOVE, st_remove), (C_DESTROY, st_destroy),
               (C_DEALLOC, st_dealloc), (C_VDTOR, st_vdtor), (C_DELETE, st_delete)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def ptrs(self, a, ws):
        self.write(a, b"".join(struct.pack(self.pfmt, w) for w in ws))

    def rd_ptrs(self, a, n):
        return list(struct.unpack(self.pfmt[0] + "%d" % n + self.pfmt[1], self.read(a, self.pw * n)))

    def make_world(self):
        """Allocates the blocks (before the log is created) and returns {address: label}."""
        b = self.alloc(0x400)
        self.this = b
        self.other = b + 0x10
        self.impl = b + 0x20
        self.cnt = b + 0xa0
        self.oimpl = b + 0xb0
        self.ocnt = b + 0x130
        self.facets = [b + 0x140 + 0x10 * i for i in range(NF)]
        labs = {self.this: 3, self.other: 4, self.impl: 1, self.cnt: 2, self.oimpl: 6, self.ocnt: 5}
        for i, f in enumerate(self.facets):
            labs[f] = 11 + i
        self.labs = labs
        return labs

    def lb(self, w):
        return self.labs.get(w, w)


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    pw, O_CNT, O_CA, O_SLOT, O_REFS = 4, 4, 0xc, 0x20, 4

    def __init__(self, e, case):
        self.e = e
        labs = self.make_world()
        self.L = e.cl = CL.PpcLog(dict(labs), case["queues"])
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

    def call(self):
        r3, _, _ = T.call_ppc(self.e, FN, [self.this, self.other])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    pw, O_CNT, O_CA, O_SLOT, O_REFS = 8, 8, 0x10, 0x40, 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        labs = self.make_world()
        CL.host_begin(lib, {v: k for k, v in labs.items()}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_lrp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        return self.lib.tpw_lrp_assign(self.this, self.other)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lrp_alloc.restype, lib.tpw_lrp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lrp_assign.restype, lib.tpw_lrp_assign.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64]
    lib.tpw_lrp_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lrp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.ptrs(s.this, [s.impl if case["imp"] else 0])
    s.ptrs(s.this + s.pw, [s.cnt if case["cnt"] is not None else 0])
    if case["cnt"] is not None:
        s.words(s.cnt, [case["cnt"]])
    if case["same"]:
        oi = s.impl if case["imp"] else 0
    else:
        oi = s.oimpl if case["oimp"] else 0
    s.ptrs(s.other, [oi])
    s.ptrs(s.other + s.pw, [s.ocnt if case["ocnt"] is not None else 0])
    if case["ocnt"] is not None:
        s.words(s.ocnt, [case["ocnt"]])
    s.ptrs(s.impl + s.O_CA, [case["ca"]])
    for i in range(NF):
        fa, refs = case["facets"][i]
        s.ptrs(s.impl + s.O_SLOT + s.pw * i, [s.facets[i] if fa else 0])
        s.ptrs(s.facets[i], [s.facets[i]])   # a readable "vtable" for the original's lwz 8(vt)
        s.words(s.facets[i] + s.O_REFS, [refs])


def observe(s, case, answer):
    return {
        "answer": s.lb(answer),
        "log": s.log(),
        "this": [s.lb(w) for w in s.rd_ptrs(s.this, 2)],
        "other": [s.lb(w) for w in s.rd_ptrs(s.other, 2)],
        "cnt": s.rd_words(s.cnt, 1),
        "ocnt": s.rd_words(s.ocnt, 1),
        "impl": [s.lb(w) for w in s.rd_ptrs(s.impl + s.O_CA, 1)],
        "slots": [s.lb(w) for w in s.rd_ptrs(s.impl + s.O_SLOT, NF)],
        "refs": [s.rd_words(f + s.O_REFS, 1)[0] for f in s.facets],
    }


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call())
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r):
    n = r.randrange(0, NF + 1)
    return {
        "same": r.random() < 0.15,
        "imp": r.random() < 0.85,
        "oimp": r.random() < 0.8,
        "cnt": r.choice([None, 1, 1, 1, 2, 3, 0, 0xFFFFFFFF, 0x7FFFFFFF]),
        "ocnt": r.choice([None, 0, 1, 2, 0xFFFFFFFF]),
        "ca": r.choice([0, 0x1234, 0x77]),
        "facets": [(r.random() < 0.8, r.choice([1, 1, 2, 3, 0, 0xFFFFFFFF])) for _ in range(NF)],
        "queues": {"size": [n], "removeRef": [r.choice([0, 0x55, 0x1234])]},
    }


def edges():
    r = random.Random(196)
    for same in (True, False):
        for imp in (True, False):
            for cnt in (None, 1, 2, 0):
                for n in (0, 1, 3, 6):
                    for ca in (0, 0x1234):
                        for rr in (0, 0x55):
                            c = base(r)
                            c.update(same=same, imp=imp, cnt=cnt, ca=ca)
                            c["queues"] = {"size": [n], "removeRef": [rr]}
                            yield c


def rnd(r):
    return base(r)


TESTS = [
    FnTest("std::_RefCountedPtr<std::__locale_imp<__int128>,std::_Single<std::__locale_imp<__int128>>>::operator=", ppc, host,
           edge=edges, rnd=rnd),
]
