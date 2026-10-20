"""Unit v5297, CLineList::AddLine and CLineList::~CLineList, against libtpw_unit_CLineList_AddLine.dylib
(tests/unit_CLineList_AddLine.cpp), ltms, src/TbResource_CLineList_AddLine.cpp:
  CLineList::AddLine 0x10005cc0, CLineList::~CLineList 0x10005f28

A case is a world and a call. The world is built twice by one piece of Python code (emulator memory big-endian, host memory
little-endian). The callees outside the unit (vector<CLineInfo>::push_back 0x100084e4, tear_down 0x1000839c, operator delete 0x1000f694)
are fakes on both sides that record their arguments; pointers into the character block are logged as offsets from its base.

The world of a case: a list object (the widest width), a block of 8 characters of 0x10 bytes with random contents. What is compared:
the event log, the widest width, the whole character block (the hidden flag), and for the destructor the answer (0, or this).

No fused operations. Not tested: hideFrom after end (the original's loop would run away).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_CLineList_AddLine"
MASK = 0xFFFFFFFF

FN_ADD, FN_DT = 0x10005CC0, 0x10005F28
SIZES = {FN_ADD: 168, FN_DT: 96}
OWN = set(SIZES)

CALLEE_PUSH, CALLEE_TEAR, CALLEE_DELETE = 0x100084E4, 0x1000839C, 0x1000F694

NCHARS = 8
HOST_LAYOUT = None

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_push(e):
    L = e.cl
    p = e.r(4)
    L.rec("push_back", (L.lab(e.r(3)), T.s32((e.u32(p) - L.base) & MASK), T.s32((e.u32(p + 4) - L.base) & MASK), e.u32(p + 8)))


def stub_tear(e):
    L = e.cl
    L.rec("tear_down", (L.lab(e.r(3)),))


def stub_delete(e):
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)),))


STUBS.internal(CALLEE_PUSH, stub_push)
STUBS.internal(CALLEE_TEAR, stub_tear)
STUBS.internal(CALLEE_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    off_max = 0xC

    def __init__(self, e, case):
        self.e = e
        self.list = e.alloc(0x20)
        self.chars = e.alloc(NCHARS * 0x10)
        self.L = e.cl = CL.PpcLog({self.list: 1}, case["queues"])
        self.L.base = self.chars
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def add(self, a, b, c, w):
        T.call_ppc(self.e, FN_ADD, [self.list, a, b, c, w & MASK])

    def destruct(self, this, flag):
        r3, _, _ = T.call_ppc(self.e, FN_DT, [this, flag & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off_max = lib.off_max
        # a destructor may delete the object: it must come from operator new
        self.list = lib.tpw_cll_new(0x20) if case["op"] == "dt" else lib.tpw_cll_alloc(0x20)
        self.chars = lib.tpw_cll_alloc(NCHARS * 0x10)
        lib.tpw_cll_setbase(self.chars)
        CL.host_begin(lib, {1: self.list}, case["queues"])

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def add(self, a, b, c, w):
        self.lib.tpw_cll_addline(self.list, a, b, c, w & MASK)

    def destruct(self, this, flag):
        r = self.lib.tpw_cll_destruct(this, T.s32(flag))
        return r


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cll_alloc.restype, lib.tpw_cll_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cll_new.restype, lib.tpw_cll_new.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cll_setbase.argtypes = [C.c_uint64]
    lib.tpw_cll_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cll_addline.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64, C.c_uint64, C.c_uint32]
    lib.tpw_cll_destruct.restype, lib.tpw_cll_destruct.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 8)()
    n = lib.tpw_cll_layout(out)
    assert n == 1
    lib.off_max = out[0]
    lib.bound = True


def build(s, case):
    s.words(s.list + s.off_max, [case["max"]])
    s.write(s.chars, case["chars"])


def execute(s, case):
    if case["op"] == "add":
        a, b, c = (s.chars + 0x10 * i for i in case["idx"])
        s.add(a, b, c, case["width"])
        return 0
    this = s.list if case["this"] else 0
    r = s.destruct(this, case["flag"])
    return 0 if r == 0 else (1 if r == this else 2)


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "max": s.rd_word(s.list + s.off_max), "chars": s.read(s.chars, NCHARS * 0x10)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


WIDTHS = [0, 1, 5, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]


def base(r, op):
    return {"op": op, "queues": {}, "max": r.choice(WIDTHS + [r.getrandbits(32)]),
            "chars": bytes(r.getrandbits(8) for _ in range(NCHARS * 0x10))}


def add_case(r, idx=None, width=None, mx=None):
    c = base(r, "add")
    if idx is None:
        a = r.randrange(NCHARS)
        b = r.randrange(a, NCHARS)
        d = r.randrange(0, NCHARS)
        idx = (a, b, r.randrange(b, NCHARS))   # begin, hideFrom, end
        idx = (d, b, idx[2])
    c["idx"] = idx
    c["width"] = r.choice(WIDTHS + [r.getrandbits(32)]) if width is None else width
    if mx is not None:
        c["max"] = mx
    return c


def add_edges():
    r = random.Random(5297)
    for idx in ((0, 0, 0), (0, 0, 3), (1, 2, 2), (0, 3, 7), (2, 1, 1), (0, 7, 7)):
        for mx, w in ((5, 5), (5, 6), (5, 4), (0, 0), (0x7FFFFFFF, 0x80000000), (0x80000000, 0x7FFFFFFF), (0xFFFFFFFF, 0),
                      (0, 0xFFFFFFFF)):
            yield add_case(r, idx, w, mx)


def add_random(r):
    return add_case(r)


def dt_case(r, this=None, flag=None):
    c = base(r, "dt")
    c["this"] = r.choice([0, 1]) if this is None else this
    c["flag"] = r.choice([0, 1, -1, 0x7FFF, 0x8000, 0x10000, 0x18001, 0xFFFF, 2, r.getrandbits(32)]) if flag is None else flag
    return c


def dt_edges():
    r = random.Random(5298)
    for this in (0, 1):
        for flag in (0, 1, 2, -1, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x18001, 0xFFFF8000, 0x80000001):
            yield dt_case(r, this, flag)


def dt_random(r):
    return dt_case(r)


TESTS = [
    FnTest("CLineList::AddLine", ppc, host, edge=add_edges, rnd=add_random),
    FnTest("CLineList::~CLineList", ppc, host, edge=dt_edges, rnd=dt_random),
]
