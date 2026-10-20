"""Unit v5140, ccServerInfo::~ccServerInfo, against libtpw_unit_ccServerInfo_Dtor_ccServerInfo.dylib
(tests/unit_ccServerInfo_Dtor_ccServerInfo.cpp), main app, src/FS_Online_init_ccServerInfo_Dtor_ccServerInfo.cpp:
  ccServerInfo::~ccServerInfo 0x101731dc

A case is an object of 0x26 bytes with the five string pointers (+0x08, +0x0c, +0x16, +0x1a, +0x22) each NULL or a small block whose
first word is a tag, and random other fields. free is a fake on both sides recording the tag of each block it is given (so the order
and the set of freed blocks are compared). Compared: the event log and every field of the object after the call.

The destructor's hidden flag (r4) is 0, negative or 0x8000 (negative as a short): no operator delete. Not tested: a positive flag (the
host's `delete` expression does the deleting, the host destructor has no flag) and a NULL this (the host never calls it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ccServerInfo_Dtor_ccServerInfo"
MASK = 0xFFFFFFFF

FN = 0x101731dc
SIZES = {FN: 220}
OWN = set(SIZES)
PTR_OFFS = [0x08, 0x0C, 0x16, 0x1A, 0x22]
HOST_LAYOUT = [0x08, 0x0C, 0x16, 0x1A, 0x22, 0x26]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_free(e):
    p = e.r(3)
    e.cl.rec("free", (e.u32(p),))


STUBS.imp("free", stub_free)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


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

    def ptr(self, a):
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, obj, flag):
        T.call_ppc(self.e, FN, [obj & MASK, flag & 0xFFFF])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_ccs_alloc(n)

    def ptr(self, a):
        return self.lib.tpw_ccs_off(a)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, obj, flag):
        self.lib.tpw_ccs_dtor(obj)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ccs_alloc.restype, lib.tpw_ccs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ccs_off.restype, lib.tpw_ccs_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_ccs_dtor.restype, lib.tpw_ccs_dtor.argtypes = None, [C.c_uint64]
    lib.tpw_ccs_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ccs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        obj = s.alloc(0x28)
        s.words(obj + 0x00, [case["f00"]])
        s.words(obj + 0x04, [case["f04"]])
        s.words(obj + 0x10, [case["f10"]])
        s.write(obj + 0x14, struct.pack(s.fmt[0] + "H", case["f14"]))
        s.words(obj + 0x1E, [case["f1e"]])
        for off, tag in zip(PTR_OFFS, case["tags"]):
            if tag:
                blk = s.alloc(8)
                s.words(blk, [tag])
                s.words(obj + off, [s.ptr(blk)])
        s.call(obj, case["flag"])
        res = {"log": s.log()}
        for off in (0x00, 0x04, 0x08, 0x0C, 0x10, 0x16, 0x1A, 0x1E, 0x22):
            res["w%02x" % off] = s.rd_word(obj + off)
        res["h14"] = s.rd_half(obj + 0x14)
        return res
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, mask, flag=None):
    tags = [(r.randrange(1, MASK) if mask >> i & 1 else 0) for i in range(5)]
    return {"queues": {}, "tags": tags, "flag": r.choice([0, 0xFFFF, 0x8000, 0xFFFE]) if flag is None else flag,
            "f00": r.randrange(1 << 32), "f04": r.randrange(1 << 32), "f10": r.randrange(1 << 32),
            "f14": r.randrange(1 << 16), "f1e": r.randrange(1 << 32)}


def edges():
    r = random.Random(5140)
    for mask in range(32):
        yield mk(r, mask)
    for flag in (0, 0xFFFF, 0x8000, 0xFFFE):
        yield mk(r, 31, flag)


def rnd(r):
    return mk(r, r.randrange(32))


TESTS = [
    FnTest("ccServerInfo::~ccServerInfo", ppc, host, edge=edges, rnd=rnd),
]
