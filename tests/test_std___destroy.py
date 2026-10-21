"""Unit v5310, std::__destroy<Argument*> (range, 0x10201744 and the tagged form 0x102013c0) and std::__destroy<Argument> (0x102017d4),
against libtpw_unit_std___destroy.dylib (tests/unit_std___destroy.cpp), main app ("SimTheme Park"), src/game/MslSupport.cpp:
  0x10201744 (first, last): __destroy<Argument> for every 12-byte element
  0x102017d4 (argument): NULL check, then the wide string (offset 4) and the string (offset 0) destructors
  0x102013c0 (first, last, tag): Argument::~Argument(element, -1) for every element

A case is an array of 6 Arguments (12 bytes each) and a call with pointers into it (or NULL, or past the end, or first >= last, or an
unaligned last). The array is allocated on each side first, then the call log gets a label for every element and string field. The callees
(Argument::~Argument 0x1020127c, the string destructors 0x1000221c and 0x101468f8) are fakes on both sides that record the object and the
flag. What is compared: the event log. No fused operations. Not tested: nothing is skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___destroy"
MASK = 0xFFFFFFFF

FN_RANGE, FN_ONE, FN_TAG = 0x10201744, 0x102017d4, 0x102013c0
SIZES = {FN_RANGE: 76, FN_ONE: 68, FN_TAG: 80}
OWN = set(SIZES)

ARG_DTOR, STR_DTOR, WSTR_DTOR = 0x1020127c, 0x1000221c, 0x101468f8
N = 6
HOST_LAYOUT = [0, 4, 8, 12]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_arg_dtor(e):
    L = e.cl
    L.rec("ArgDtor", (L.lab(e.r(3)), T.s32(e.r(4))))
    # r3 comes back unchanged


def stub_str_dtor(e):
    L = e.cl
    L.rec("StrDtor", (L.lab(e.r(3)), T.s32(e.r(4))))


def stub_wstr_dtor(e):
    L = e.cl
    L.rec("WStrDtor", (L.lab(e.r(3)), T.s32(e.r(4))))


STUBS.internal(ARG_DTOR, stub_arg_dtor)
STUBS.internal(STR_DTOR, stub_str_dtor)
STUBS.internal(WSTR_DTOR, stub_wstr_dtor)


def setup(runner):
    T.install_std_stubs(runner.emu)


def labels(base):
    """label -> address: element i is 1 + 3 i (its string field has the same address), 2 + 3 i is its wide string field."""
    out = {}
    for i in range(N):
        out[base + 12 * i] = 1 + 3 * i
        out[base + 12 * i + 4] = 2 + 3 * i
        out[base + 12 * i + 8] = 3 + 3 * i
    return out


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.base = e.alloc(12 * N + 16)
        self.L = e.cl = CL.PpcLog(labels(self.base), case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def addr(self, off):
        return 0 if off is None else (self.base + off) & MASK

    def log(self):
        return self.L.log

    def call(self, op, a, b):
        fn = {"range": FN_RANGE, "one": FN_ONE, "tag": FN_TAG}[op]
        args = [self.addr(a)] if op == "one" else [self.addr(a), self.addr(b)] + ([0] if op == "tag" else [])
        T.call_ppc(self.e, fn, args)


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_sd_alloc(12 * N + 16)
        fixed = {}
        for addr, lab in labels(self.base).items():
            fixed.setdefault(lab, addr)
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def addr(self, off):
        return 0 if off is None else self.base + off

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, a, b):
        if op == "one":
            self.lib.tpw_sd_one(self.addr(a))
        elif op == "range":
            self.lib.tpw_sd_range(self.addr(a), self.addr(b))
        else:
            self.lib.tpw_sd_tag(self.addr(a), self.addr(b))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sd_alloc.restype, lib.tpw_sd_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sd_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sd_one.argtypes = [C.c_uint64]
    lib.tpw_sd_range.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_sd_tag.argtypes = [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sd_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.call(case["op"], case["a"], case["b"])
        return {"log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(op, a, b=None):
    return {"op": op, "a": a, "b": b, "queues": {}}


def one_edges():
    for a in [None] + [12 * i for i in range(N)]:
        yield mk("one", a)


def one_random(r):
    return mk("one", r.choice([None, 12 * r.randrange(N), 12 * r.randrange(N) + r.randrange(12)]))


def range_edges_for(op):
    def edges():
        for i in range(N + 1):
            for j in range(N + 1):
                yield mk(op, 12 * i, 12 * j)
        yield mk(op, 0, 5)
        yield mk(op, 0, 12)
        yield mk(op, 0, 13)
        yield mk(op, 0, 25)
        yield mk(op, 12, 12 * 3 + 1)
        yield mk(op, 24, 0)
        yield mk(op, 0, None)       # last NULL: no iteration
    return edges


def range_random(op):
    def rnd(r):
        return mk(op, r.randrange(0, 12 * N + 1), r.randrange(0, 12 * N + 1))
    return rnd


TESTS = [
    FnTest("std::__destroy<Argument*>", ppc, host, edge=range_edges_for("range"), rnd=range_random("range")),
    FnTest("std::__destroy<Argument>", ppc, host, edge=one_edges, rnd=one_random),
    FnTest("std::__destroy<Argument*>", ppc, host, edge=range_edges_for("tag"), rnd=range_random("tag")),
]
