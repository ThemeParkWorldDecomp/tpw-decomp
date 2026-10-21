"""Unit v5312, CStaticMBBuffer::AppendMB x2, AppendUnicode x2, Close, against libtpw_unit_CStaticMBBuffer_AppendMB.dylib
(tests/unit_CStaticMBBuffer_AppendMB.cpp), program ltms, src/ltms/TbITable.cpp:
  AppendMB(ptr, size) 0x100000dc, AppendMB(u16) 0x100001e8, AppendUnicode(ptr, size) 0x10000284, AppendUnicode(wchar) 0x10000394, Close 0x10000420

A case is a buffer object (cursor, end, converter, overflow flag) over a data block with padding, built by one piece of Python code in both
memories (field offsets differ: the host has 8 byte pointers). The converter's GetMBValue, the converter's Release (vtable +8) and the buffer's
AppendMB(u16) (vtable +0x14) are fakes recording events on both sides. Compared: the log, the whole data block, cursor (relative to the block),
end, converter, overflow flag. No fused operations. Not tested: a cursor below the end that is NULL (the original would write at address 0).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_CStaticMBBuffer_AppendMB"
MASK = 0xFFFFFFFF

F_MBP, F_MB16, F_UP, F_UC, F_CLOSE = 0x100000DC, 0x100001E8, 0x10000284, 0x10000394, 0x10000420
OWN = {F_MBP, F_MB16, F_UP, F_UC, F_CLOSE}
GETMB = 0x100056F8
GLUE = 0x1000F91C
CODE_MB16, CODE_REL = 0x7001, 0x7002
PPC_OFF = {"cur": 4, "end": 8, "conv": 0xC, "ovf": 0x10}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_getmb(e):
    L = e.cl
    L.rec("GetMBValue", (L.lab(e.r(3)), e.r(4) & 0xFFFF))
    e.set_r(3, L.ret("GetMBValue") & 0xFFFF)


def stub_glue(e):
    L = e.cl
    code = e.u32(e.r(12))
    if code == CODE_MB16:
        L.rec("AppendMB16", (L.lab(e.r(3)), e.r(4) & 0xFFFF))
    elif code == CODE_REL:
        L.rec("Release", (L.lab(e.r(3)),))
        e.set_r(3, 0)
    else:
        L.rec("UNEXPECTED", (L.lab(e.r(3)),))


STUBS.internal(GETMB, stub_getmb)
STUBS.internal(GLUE, stub_glue)

NBLK = 8      # pad before the data
NMAX = 8


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    fmt = ">"
    ptr = 4

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(0x30)
        self.conv = e.alloc(0x10)
        self.L = e.cl = CL.PpcLog({self.obj: 1, self.conv: 2}, case["queues"])
        self.saved = STUBS.activate(e)
        self.off = PPC_OFF

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, d):
        self.e.write(a, d)

    def log(self):
        return self.L.log

    def vtables(self):
        vt = self.e.alloc(0x20)
        tv1 = self.e.alloc(8)
        self.write(tv1, struct.pack(">II", CODE_MB16, 0))
        self.write(vt + 0x14, struct.pack(">I", tv1))
        cvt = self.e.alloc(0x10)
        tv2 = self.e.alloc(8)
        self.write(tv2, struct.pack(">II", CODE_REL, 0))
        self.write(cvt + 8, struct.pack(">I", tv2))
        self.write(self.obj, struct.pack(">I", vt))
        self.write(self.conv, struct.pack(">I", cvt))

    def call(self, fn, args):
        T.call_ppc(self.e, fn, [self.obj] + args)


class Host:
    fmt = "<"
    ptr = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_cmb_alloc(0x40)
        self.conv = lib.tpw_cmb_alloc(0x10)
        CL.host_begin(lib, {1: self.obj, 2: self.conv}, case["queues"])
        self.off = dict(zip(("cur", "end", "conv", "ovf"), HOST_OFF))

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_cmb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def log(self):
        return CL.host_end(self.lib)

    def vtables(self):
        self.write(self.obj, struct.pack("<Q", self.lib.tpw_cmb_addr(b"bufvt")))
        self.write(self.conv, struct.pack("<Q", self.lib.tpw_cmb_addr(b"convvt")))

    def call(self, fn, args):
        f = {F_MBP: self.lib.tpw_cmb_appendmb_p, F_MB16: self.lib.tpw_cmb_appendmb_c, F_UP: self.lib.tpw_cmb_appendu_p,
             F_UC: self.lib.tpw_cmb_appendu_c, F_CLOSE: self.lib.tpw_cmb_close}[fn]
        f(self.obj, *args)


HOST_OFF = None


def bind(lib):
    global HOST_OFF
    if getattr(lib, "bound", False):
        return
    lib.tpw_cmb_addr.restype, lib.tpw_cmb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cmb_alloc.restype, lib.tpw_cmb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cmb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cmb_appendmb_p.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32]
    lib.tpw_cmb_appendmb_c.argtypes = [C.c_uint64, C.c_uint32]
    lib.tpw_cmb_appendu_p.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32]
    lib.tpw_cmb_appendu_c.argtypes = [C.c_uint64, C.c_uint32]
    lib.tpw_cmb_close.argtypes = [C.c_uint64]
    out = (C.c_int64 * 8)()
    n = lib.tpw_cmb_layout(out)
    HOST_OFF = list(out)[:n]
    assert HOST_OFF[0] == 8 and HOST_OFF[3] == 32, HOST_OFF
    lib.bound = True


def setp(s, name, v):
    s.write(s.obj + s.off[name], struct.pack(s.fmt + ("I" if s.ptr == 4 else "Q"), v))


def getp(s, name):
    return struct.unpack(s.fmt + ("I" if s.ptr == 4 else "Q"), s.read(s.obj + s.off[name], s.ptr))[0]


def run(s, case):
    try:
        n = case["n"]
        blk = s.alloc(NBLK + NMAX + 16)
        s.write(blk, bytes((0x30 + i) & 0xFF for i in range(NBLK + NMAX + 16)))
        buf = blk + NBLK
        s.vtables()
        setp(s, "cur", 0 if case["cur"] is None else buf + case["cur"])
        setp(s, "end", 0 if case["end"] is None else buf + case["end"])
        setp(s, "conv", s.conv if case["conv"] else 0)
        s.write(s.obj + s.off["ovf"], bytes([case["ovf"]]))
        op = case["op"]
        if op == "mbp":
            src = s.alloc(32)
            s.write(src, bytes(case["src"]) + bytes(32 - len(case["src"])))
            s.call(F_MBP, [src, case["size"]])
        elif op == "mbc":
            s.call(F_MB16, [case["val"]])
        elif op == "up":
            src = s.alloc(48)
            s.write(src, b"".join(struct.pack(s.fmt + "H", c) for c in case["wide"]) + bytes(48 - 2 * len(case["wide"])))
            s.call(F_UP, [src, case["size"]])
        elif op == "uc":
            s.call(F_UC, [case["val"]])
        else:
            s.call(F_CLOSE, [])
        cur, end, conv = getp(s, "cur"), getp(s, "end"), getp(s, "conv")
        return {"log": s.log(), "block": s.read(blk, NBLK + NMAX + 16),
                "cur": None if cur == 0 else cur - buf, "end": None if end == 0 else end - buf,
                "conv": 0 if conv == 0 else (1 if conv == s.conv else -1), "ovf": s.read(s.obj + s.off["ovf"], 1)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    n = r.randrange(0, 7)
    k = r.random()
    if k < 0.1:
        cur, end = None, None
    elif k < 0.3:
        cur, end = n, n
    elif k < 0.4:
        cur, end = max(n - 1, 0), n
    else:
        cur, end = r.randrange(0, n + 1), n
    return {"op": op, "n": n, "cur": cur, "end": end, "conv": r.random() < 0.8, "ovf": 1 if r.random() < 0.2 else 0,
            "queues": {"GetMBValue": [r.choice([0, 1, 0xFF, 0x100, 0x1234, 0xFFFF, r.randrange(0, 65536)]) for _ in range(12)]}}


def sizes(r):
    return r.choice([0xFFFFFFFF, 0xFFFFFFFF, 0, 1, 2, r.randrange(0, 12), 0xFFFFFFFE])


def gen(r, op):
    c = base(r, op)
    if op == "mbp":
        c["src"] = [r.choice([0, 1, 0x7F, 0x80, 0xFF, r.randrange(1, 256)]) for _ in range(r.randrange(0, 10))]
        c["size"] = sizes(r)
        if c["size"] == 0xFFFFFFFE:
            c["size"] = r.randrange(0, 12)
    elif op == "mbc":
        c["val"] = r.choice([0, 1, 0xFF, 0x100, 0x1234, 0xFF00, 0xFFFF, r.randrange(0, 65536)])
    elif op == "up":
        c["wide"] = [r.choice([0, 1, 0x41, 0x100, 0xFFFF, r.randrange(1, 65536)]) for _ in range(r.randrange(0, 12))]
        c["size"] = sizes(r)
        if c["size"] == 0xFFFFFFFE:
            c["size"] = r.randrange(0, 12)
    elif op == "uc":
        c["val"] = r.choice([0, 1, 0x41, 0x100, 0xFFFF, r.randrange(0, 65536)])
    return c


def edges(op):
    def f():
        r = random.Random(sum(map(ord, op)))
        for _ in range(60):
            yield gen(r, op)
        r = random.Random(7)
        for conv in (False, True):
            for ovf in (0, 1):
                for cur, end in ((None, None), (0, 0), (1, 1), (0, 1), (2, 3), (3, 3)):
                    c = gen(r, op)
                    c.update(conv=conv, ovf=ovf, cur=cur, end=end, n=max(end or 0, 1))
                    yield c
    return f


def mk(name, op):
    return FnTest(name, ppc, host, edge=edges(op), rnd=lambda r: gen(r, op))


TESTS = [
    mk("CStaticMBBuffer::AppendMB(const unsigned char*, unsigned long)", "mbp"),
    mk("CStaticMBBuffer::AppendMB(unsigned short)", "mbc"),
    mk("CStaticMBBuffer::AppendUnicode(const wchar_t*, unsigned long)", "up"),
    mk("CStaticMBBuffer::AppendUnicode(wchar_t)", "uc"),
    mk("CStaticMBBuffer::Close", "cl"),
]
