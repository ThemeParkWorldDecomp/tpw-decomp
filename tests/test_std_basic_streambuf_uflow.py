"""Unit v5104, std::basic_streambuf<char> base functions, against libtpw_unit_std_basic_streambuf_uflow.dylib
(tests/unit_std_basic_streambuf_uflow.cpp), main app, src/game/MslSupport.cpp:
  uflow 0x10070690, imbue 0x1007048c, ~basic_streambuf 0x10070304, getloc 0x10070e1c

A case is a streambuf object (0x24 bytes), a vtable block, a get buffer and a locale. The callees (the virtual at vtable 0x28 through
__ptr_glue, the locale copy constructor, assignment and destructor, operator delete) are fakes on both sides recording their arguments.
Compared: the answer, the object words (gptr relative to the buffer, vtable as the global or relative to its block), the event log.
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_streambuf_uflow"
MASK = 0xFFFFFFFF

FN_UFLOW, FN_IMBUE, FN_DT, FN_GETLOC = 0x10070690, 0x1007048c, 0x10070304, 0x10070e1c
SIZES = {FN_UFLOW: 116, FN_IMBUE: 72, FN_DT: 144, FN_GETLOC: 72}
OWN = set(SIZES)

PTR_GLUE, DELETE, LOC_COPY, LOC_ASSIGN, LOC_DTOR = 0x1023baa8, 0x10238218, 0x10072030, 0x10072128, 0x10071f34
ADDR = dict(vtbl=0x1029d0f8)
# host object: native pointers (arena memory is above 4 GB): vtable, gptr, zero10, locale offsets, size
HOST_LAYOUT = [0, 16, 32, 44, 56]
PPC_O = dict(vt=0, gptr=8, z=0x10, loc=0x1c, size=0x24, pw=4)
HOST_O = dict(vt=0, gptr=16, z=32, loc=44, size=56, pw=8)


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_underflow(e):
    L = e.cl
    L.rec("Underflow", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("Underflow") & MASK)


def stub_copy(e):
    L = e.cl
    L.rec("LocaleCopyCtor", (L.lab(e.r(3)), L.lab(e.r(4))))


def stub_assign(e):
    L = e.cl
    L.rec("LocaleAssign", (L.lab(e.r(3)), L.lab(e.r(4))))


def stub_ldtor(e):
    L = e.cl
    L.rec("LocaleDtor", (L.lab(e.r(3)), s32(e.r(4))))


def stub_delete(e):
    L = e.cl
    L.rec("Delete", (L.lab(e.r(3)),))


STUBS.internal(PTR_GLUE, stub_underflow)
STUBS.internal(LOC_COPY, stub_copy)
STUBS.internal(LOC_ASSIGN, stub_assign)
STUBS.internal(LOC_DTOR, stub_ldtor)
STUBS.internal(DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def make(self):
        self.obj = self.alloc(self.o["size"])
        self.vblk = self.alloc(0x30)
        self.buf = self.alloc(16)
        self.loc = self.alloc(8)
        self.out = self.alloc(8)

    def ptr(self, a):
        return struct.unpack(self.fmt[0] + ("Q" if self.o["pw"] == 8 else "I"), self.read(a, self.o["pw"]))[0]

    def wptr(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + ("Q" if self.o["pw"] == 8 else "I"), v))

    def labels(self):
        return (self.obj, self.obj + self.o["loc"], self.loc, self.out)


class Ppc(Side):
    fmt = ">I"
    o = PPC_O

    def __init__(self, e, case):
        self.e = e
        self.make()
        o = self.labels()
        self.L = e.cl = CL.PpcLog({o[0]: 1, o[1]: 2, o[2]: 3, o[3]: 4}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        return T.call_ppc(self.e, fn, args)[0] & MASK


class Host(Side):
    fmt = "<I"
    o = HOST_O

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make()
        o = self.labels()
        CL.host_begin(lib, {1: o[0], 2: o[1], 3: o[2], 4: o[3]}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sb_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        l = self.lib
        if fn == FN_UFLOW:
            return l.tpw_sb_uflow(args[0]) & MASK
        if fn == FN_IMBUE:
            l.tpw_sb_imbue(args[0], args[1])
            return 0
        if fn == FN_GETLOC:
            l.tpw_sb_getloc(args[1], args[0])
            return 0
        return l.tpw_sb_dt(args[0], s32(args[1]))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_sb_"
    lib.tpw_sb_addr.restype, lib.tpw_sb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sb_alloc.restype, lib.tpw_sb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sb_uflow.restype, lib.tpw_sb_uflow.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_sb_imbue.restype, lib.tpw_sb_imbue.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_sb_getloc.restype, lib.tpw_sb_getloc.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_sb_dt.restype, lib.tpw_sb_dt.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sb_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.write(s.buf, bytes(case["buf"]))
    s.words(s.vblk, case["vtab"])
    o = s.o
    s.wptr(s.obj + o["vt"], s.vblk)
    s.wptr(s.obj + o["gptr"], s.buf + case["off"])
    s.words(s.obj + o["z"], case["words"][:3])
    s.words(s.obj + o["loc"], case["locw"])
    s.words(s.loc, case["locw"][::-1])


def execute(s, case):
    op = case["op"]
    this = 0 if case.get("null") else s.obj
    if op == "uflow":
        return s.call(FN_UFLOW, [this])
    if op == "imbue":
        s.call(FN_IMBUE, [this, s.loc])      # void: r3 is not an answer
        return 0
    if op == "getloc":
        s.call(FN_GETLOC, [s.out, this])
        return 0
    r = s.call(FN_DT, [this, case["flag"]])
    return "null" if r == 0 else r - s.obj


def observe(s, case, answer):
    o = s.o
    v = s.ptr(s.obj + o["vt"])
    vt = "G" if v == s.g("vtbl") else v - s.vblk
    return {"answer": answer, "log": s.log(), "vt": vt, "gptr": s.ptr(s.obj + o["gptr"]) - s.buf,
            "z": s.rd_words(s.obj + o["z"], 3), "ol": s.rd_words(s.obj + o["loc"], 2), "out": s.rd_words(s.out, 2),
            "loc": s.rd_words(s.loc, 2)}


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


def base(r, op, under=None):
    words = [0] + [r.getrandbits(32) for _ in range(8)]
    words[1] = 0
    words[3] = 0
    return {"op": op, "queues": {"Underflow": [r.choice([-1, 0, 0x41, 255, 256, -2, 0x7fffffff]) if under is None else under]},
            "buf": [r.randrange(256) for _ in range(16)], "off": r.randrange(8),
            "vtab": [r.getrandbits(32) for _ in range(12)], "words": words, "locw": [r.getrandbits(32), r.getrandbits(32)],
            "flag": r.choice([0, 1, -1, 5, 0x7fff, -0x8000, 0x10000, 0x18000, 0x8001, 0xffff0001, 0x12340000]) & MASK,
            "null": False}


def mk(op, edges):
    def edge():
        r = random.Random(hash(op) & 0xffff or 7)
        for kw in edges:
            c = base(r, op, kw.get("under"))
            c.update({k: v for k, v in kw.items() if k != "under"})
            yield c

    def rnd(r):
        c = base(r, op)
        c["null"] = (op == "dt" and r.random() < 0.15)
        return c
    return edge, rnd


u_e, u_r = mk("uflow", [{"under": -1}, {"under": 0}, {"under": 5, "off": 0}, {"under": 5, "off": 7}, {"under": 0xffffffff}])
i_e, i_r = mk("imbue", [{}, {"locw": [0, 0]}])
g_e, g_r = mk("getloc", [{}, {"locw": [0, 0]}])
d_e, d_r = mk("dt", [{"null": True}, {"flag": 0}, {"flag": 1}, {"flag": 0xffff}, {"flag": 0x8000}, {"flag": 0x10001},
                     {"flag": 0x17fff}, {"flag": 0x18000}])

TESTS = [
    FnTest("std::basic_streambuf<char,std::char_traits<char>>::uflow", ppc, host, edge=u_e, rnd=u_r),
    FnTest("std::basic_streambuf<char,std::char_traits<char>>::imbue", ppc, host, edge=i_e, rnd=i_r),
    FnTest("std::basic_streambuf<char,std::char_traits<char>>::~basic_streambuf", ppc, host, edge=d_e, rnd=d_r),
    FnTest("std::basic_streambuf<char,std::char_traits<char>>::getloc", ppc, host, edge=g_e, rnd=g_r),
]
