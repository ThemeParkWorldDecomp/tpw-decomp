"""Unit v5729, CSubscriptionHeader::Save and ::Load, against libtpw_unit_CSubscriptionHeader_Load.dylib
(tests/unit_CSubscriptionHeader_Load.cpp), main app, src/MessageCentre_post_CSubscriptionHeader_Load.cpp:
  CSubscriptionHeader::Save 0x10152928, CSubscriptionHeader::Load 0x10152a10

A case is a header and a call. The world is built twice by one piece of Python code (Mac layout in the emulator, host layout in the
library). Save walks a real tree (built balanced from sorted ids, so left and right children both occur) with the program's own
increment; LbFile_Write, LbFile_Read, CSubscription::Save / Load and insert_one are fakes on both sides that record their arguments.

The file is little-endian on both sides: the read fake delivers the word's bytes low first and the write fake records the bytes read
low first (the Mac swaps its count on the way in and out, the host does not).

Compared: the answer and the call log (the file handle, the count word as the file holds it, every id). Not tested: a count that never
ends (the insert fake always accepting a huge count); the original's begin word at +0xc is written on the Mac side only (the host
computes the leftmost node).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CSubscriptionHeader_Load"
MASK = 0xFFFFFFFF

SAVE, LOAD = 0x10152928, 0x10152A10
SIZES = {SAVE: 180, LOAD: 196}
OWN = set(SIZES)
CSUB_LOAD, CSUB_SAVE, INSERT_ONE = 0x10152BB4, 0x10152B08, 0x101530E4
HOST_LAYOUT = [0, 8, 24, 16]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_read(e):
    L = e.cl
    L.rec("LbFile_Read", (e.r(4), e.r(6)))
    e.write(e.r(5), struct.pack("<I", L.ret("LbFile_Read") & MASK))


def stub_write(e):
    L = e.cl
    L.rec("LbFile_Write", (e.r(4), e.r(6), struct.unpack("<I", e.read(e.r(5), 4))[0]))


def stub_sub_load(e):
    L = e.cl
    L.rec("CSub_Load", (e.u32(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("CSub_Load") & 0xFFFF))


def stub_sub_save(e):
    L = e.cl
    L.rec("CSub_Save", (e.u32(e.r(4)), struct.unpack(">H", e.read(e.r(3), 2))[0]))


def stub_insert(e):
    L = e.cl
    key = struct.unpack(">H", e.read(e.r(5), 2))[0]
    L.rec("insert_one", (L.lab(e.r(4)), key))
    e.write(e.r(3), struct.pack(">I", 0))
    e.write(e.r(3) + 4, bytes([1 if L.ret("insert_one") else 0]))


STUBS.imp("LbFile_Read__FPvPvUlPUl", stub_read)
STUBS.imp("LbFile_Write__FPvPCvUlPUl", stub_write)
STUBS.internal(CSUB_LOAD, stub_sub_load)
STUBS.internal(CSUB_SAVE, stub_sub_save)
STUBS.internal(INSERT_ONE, stub_insert)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def half(self, a, v):
        self.write(a, struct.pack(self.hfmt, v & 0xFFFF))


class Ppc(Side):
    fmt, pfmt, hfmt, P, COUNT = ">I", ">I", ">H", 4, 4

    def __init__(self, e, case):
        self.e = e
        self.hdr = e.alloc(0x10)
        self.L = e.cl = CL.PpcLog({self.hdr + 4: 1}, case["queues"])
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

    def call(self, op, slot):
        r3, _, _ = T.call_ppc(self.e, SAVE if op == "save" else LOAD, [self.hdr, slot])
        return r3 & MASK


class Host(Side):
    fmt, pfmt, hfmt, P, COUNT = "<I", "<Q", "<H", 8, 0

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.hdr = lib.tpw_csh_alloc(0x10)
        CL.host_begin(lib, {1: self.hdr}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_csh_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, slot):
        f = self.lib.tpw_csh_save if op == "save" else self.lib.tpw_csh_load
        return f(self.hdr, slot) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_csh_addr.restype, lib.tpw_csh_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_csh_alloc.restype, lib.tpw_csh_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_csh_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("save", "load"):
        f = getattr(lib, "tpw_csh_" + n)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_csh_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def mk(s, ids, parent, nodes):
    """A balanced tree of the (sorted) ids; node = left, right, parent (colour 0), value."""
    if not ids:
        return 0
    m = len(ids) // 2
    n = s.alloc(4 * s.P)
    nodes[ids[m]] = n
    s.ptr(n + 2 * s.P, parent)
    s.half(n + 3 * s.P, ids[m])
    left = mk(s, ids[:m], n, nodes)
    right = mk(s, ids[m + 1:], n, nodes)
    s.ptr(n, left)
    s.ptr(n + s.P, right)
    return n


def build(s, case):
    s.slot = s.alloc(8)
    s.ptr(s.slot, case["file"])
    if case["op"] == "save":
        nodes = {}
        root = mk(s, case["ids"], s.hdr + 8, nodes)
        s.words(s.hdr + s.COUNT, [case["count"]])
        s.ptr(s.hdr + 8, root)
        if s.P == 4:
            s.ptr(s.hdr + 0xC, nodes[case["ids"][0]] if case["ids"] else s.hdr + 8)


def run(s, case):
    try:
        build(s, case)
        return {"answer": s.call(case["op"], s.slot), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def bswap(v):
    return struct.unpack("<I", struct.pack(">I", v & MASK))[0]


def base(r, op):
    return {"op": op, "file": r.choice([0x1234, 0x77, 0x40000000]), "queues": {}}


def save_case(r, k=None, count=None):
    c = base(r, "save")
    k = r.randrange(0, 12) if k is None else k
    c["ids"] = sorted(r.sample(range(0, 0x10000), k))
    c["count"] = k if count is None else count
    return c


def save_edges():
    r = random.Random(11)
    for k in range(0, 10):
        yield save_case(r, k)
    yield save_case(r, 3, 0)
    yield save_case(r, 2, 0xFFFFFFFF)
    yield save_case(r, 1, 0x12345678)
    yield save_case(r, 0, 5)


def save_random(r):
    return save_case(r, count=r.choice([None, 0x01020304, r.randrange(0, 1 << 32)]))


def load_case(r, n=None, fail=None):
    c = base(r, "load")
    n = r.choice([0, 1, 2, 3, 5, 8, -1, -7, -(1 << 31)]) if n is None else n
    if fail is None:
        fail = r.randrange(0, 12)
    c["queues"] = {
        "LbFile_Read": [bswap(n)],
        "CSub_Load": [r.randrange(0, 0x10000) for _ in range(r.randrange(1, 9))],
        "insert_one": [1] * fail + [0],
    }
    return c


def load_edges():
    r = random.Random(12)
    for n in (0, 1, 2, 3, 8, -1, -(1 << 31)):
        for fail in (0, 1, 2, 9):
            yield load_case(r, n, fail)


def load_random(r):
    return load_case(r)


TESTS = [
    FnTest("CSubscriptionHeader::Save", ppc, host, edge=save_edges, rnd=save_random),
    FnTest("CSubscriptionHeader::Load", ppc, host, edge=load_edges, rnd=load_random),
]
