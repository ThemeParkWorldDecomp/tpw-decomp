"""Unit v5099, std::basic_string<wchar_t>::operator[](unsigned long) 0x10212298, against
libtpw_unit_std_basic_string_operator.dylib (tests/unit_std_basic_string_operator.cpp), main app, src/game/MslSupport.cpp.

A case is a string object (one pointer) to a representation (length, capacity, references, data pointer) with a character array.
The callees operator new (sizes recorded, memory zeroed) and wmemcpy (count recorded, really copies) are fakes on both sides.
What is compared: the answer as an offset from the data pointer, the object's representation (new or the same), its fields and
characters, the old representation's fields and characters, and the event log. No fused operations. Not tested: the exception
cleanup of the listing (unreachable: operator new never fails here).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_string_operator"
MASK = 0xFFFFFFFF
FN = 0x10212298
SIZES = {FN: 336}
OWN = set(SIZES)
HOST_LAYOUT = [16, 24]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_new(e):
    n = e.r(3) & MASK
    e.cl.rec("new", (n,))
    e.set_r(3, e.alloc(n) & MASK)


def stub_wmemcpy(e):
    d, s, n = e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK
    e.cl.rec("wmemcpy", (n,))
    e.write(d, e.read(s, n * 2))
    e.set_r(3, d)


def stub_none(e):
    e.cl.rec("unexpected", (0,))


STUBS.internal(0x102384B8, stub_new)
STUBS.internal(0x102389E0, stub_wmemcpy)
STUBS.internal(0x10238218, stub_none)
STUBS.internal(0x10238680, stub_none)
STUBS.internal(0x10238698, stub_none)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def chars(self, a, cs):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(cs), *cs))

    def rd_chars(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def wr_ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    doff = 0xC

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

    def call(self, obj, i):
        r3, _, _ = T.call_ppc(self.e, FN, [obj & MASK, i & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8
    doff = 0x10

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_bsop_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, obj, i):
        return self.lib.tpw_bsop_index(obj, i & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bsop_alloc.restype, lib.tpw_bsop_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bsop_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bsop_index.restype, lib.tpw_bsop_index.argtypes = C.c_uint64, [C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_bsop_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    length, cap, refs = case["length"], case["cap"], case["refs"]
    data = s.alloc(2 * (cap + 2))
    s.chars(data, case["chars"])
    rep = s.alloc(32)
    s.words(rep, [length, cap, refs])
    s.wr_ptr(rep + s.doff, data)
    obj = s.alloc(8)
    s.wr_ptr(obj, rep)
    return obj, rep, data


def rep_state(s, rep, n):
    length, cap, refs = s.rd_words(rep, 3)
    d = s.rd_ptr(rep + s.doff)
    return {"len": length, "cap": cap, "refs": refs, "chars": s.rd_chars(d, n), "data": d}


def run(s, case):
    try:
        obj, rep, data = build(s, case)
        ans = s.call(obj, case["index"])
        new = s.rd_ptr(obj)
        n = case["length"] + 1
        st = rep_state(s, new, n)
        old = rep_state(s, rep, n)
        return {"answer": (ans - st["data"]) & 0xFFFFFFFFFFFFFFFF, "log": s.log(), "copied": new != rep,
                "new": {k: v for k, v in st.items() if k != "data"},
                "old": {k: v for k, v in old.items() if k != "data"},
                "same_data": (st["data"] == data) == (new == rep)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make(r, refs=None, length=None, index=None):
    length = r.choice([0, 1, 2, 3, 4, 5, 7, 8, r.randrange(0, 20)]) if length is None else length
    cap = length + r.choice([0, 0, 1, 2, 5])
    refs = r.choice([0, 1, 2, 3, 7, 0xFFFF]) if refs is None else refs
    chars = [r.randrange(1, 0x10000) for _ in range(length)] + [0] + [r.randrange(0, 0x10000) for _ in range(cap + 1 - length)]
    return {"queues": {}, "length": length, "cap": cap, "refs": refs, "chars": chars[:cap + 2],
            "index": r.choice([0, 1, length, length + 1, r.randrange(0, 30)]) if index is None else index}


def edges():
    r = random.Random(99)
    for refs in (0, 1, 2, 3):
        for length in (0, 1, 3, 4, 5, 8):
            yield make(r, refs, length, 0)
            yield make(r, refs, length, length)


def rnd(r):
    return make(r)


TESTS = [
    FnTest("std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>::operator[]", ppc, host, edge=edges, rnd=rnd),
]
