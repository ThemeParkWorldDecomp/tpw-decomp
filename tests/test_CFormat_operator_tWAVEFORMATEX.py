"""Unit v51124, small sound functions, against libtpw_unit_CFormat_operator_tWAVEFORMATEX.dylib, sound program:
  CFormat::operator_tWAVEFORMATEX* 0x10002280, bfl::FileSystem::CFile::Read 0x10001f38, TbError::TbError 0x100015b8,
  TbError::operator_long 0x10000b88, CAutoArrayPtr<void*> dtor 0x10007340, operator void** 0x100073d4, ctor 0x100074a8.

Callees are fakes on both sides: FillWaveFormatEx 0x100021b0, the indirect Read glue 0x1002f154 (impl set, dest set, size, answer from
the queue), operator new[] 0x1002ebfc, delete[] 0x1002ebcc, delete 0x1002ea94. Host free functions take the object address (the
CFile case takes the access object, the value at this+4). Compared: answers, the object words (pointers as set/not set), the log.
Not tested: pointer values (they differ between memories).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_CFormat_operator_tWAVEFORMATEX"
MASK = 0xFFFFFFFF

FMT, READ, ECT, EOP, DTOR, POP, CTOR = 0x10002280, 0x10001f38, 0x100015b8, 0x10000b88, 0x10007340, 0x100073d4, 0x100074a8
SIZES = {FMT: 48, READ: 64, ECT: 8, EOP: 8, DTOR: 100, POP: 8, CTOR: 60}
FILL, RGLUE, NEWA, DELA, DEL = 0x100021b0, 0x1002f154, 0x1002ebfc, 0x1002ebcc, 0x1002ea94

STUBS = CL.Stubs()
STUBS.real.update(SIZES)


def s32(v):
    return T.s32(v)


STUBS.internal(FILL, lambda e: e.cl.rec("FillWaveFormatEx", ()))


def st_read(e):
    L = e.cl
    L.rec("Read", (int(e.r(3) != 0), int(e.r(4) != 0), e.r(5) & MASK))
    e.set_r(3, L.ret("Read") & MASK)


def st_new(e):
    n = e.r(3) & MASK
    e.cl.rec("new[]", (n,))
    e.set_r(3, e.alloc(max(n, 4)))


STUBS.internal(RGLUE, st_read)
STUBS.internal(NEWA, st_new)
STUBS.internal(DELA, lambda e: e.cl.rec("delete[]", (int(e.r(3) != 0),)))
STUBS.internal(DEL, lambda e: e.cl.rec("delete", (int(e.r(3) != 0),)))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def word(self, a, v):
        self.e.write(a, (v & MASK).to_bytes(4, "big"))

    def rd(self, a):
        return int.from_bytes(self.e.read(a, 4), "big")

    def log(self):
        return self.e.cl.log

    def call(self, fn, args):
        return T.call_ppc(self.e, fn, [a & MASK for a in args])[0] & MASK

    def fmt_(self, o):
        return (self.call(FMT, [o]) - o) & MASK

    def read_(self, o, impl, dest, size):
        return self.call(READ, [o, dest, size])

    def errctor(self, o, v):
        return int(self.call(ECT, [o, v]) == o)

    def erropl(self, o):
        return self.call(EOP, [o])

    def dtor(self, o, flag):
        return int(self.call(DTOR, [o, flag]) == o)

    def ptrop(self, o):
        return self.call(POP, [o])

    def ctor(self, o, n):
        return int(self.call(CTOR, [o, n]) == o)


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_v51124_alloc(n)

    def word(self, a, v):
        C.memmove(a, (v & MASK).to_bytes(4, "little"), 4)

    def rd(self, a):
        return int.from_bytes(C.string_at(a, 4), "little")

    def log(self):
        return CL.host_end(self.lib)

    def fmt_(self, o):
        return (self.lib.tpw_v51124_fmt(o) - o) & MASK

    def read_(self, o, impl, dest, size):
        return self.lib.tpw_v51124_read(1 if impl else 0, dest, size) & MASK

    def errctor(self, o, v):
        return int(self.lib.tpw_v51124_errctor(o, s32(v)) == o)

    def erropl(self, o):
        return self.lib.tpw_v51124_erropl(o) & MASK

    def dtor(self, o, flag):
        return int(self.lib.tpw_v51124_dtor(o, s32(flag)) == o)

    def ptrop(self, o):
        return self.lib.tpw_v51124_ptrop(o) & MASK

    def ctor(self, o, n):
        return int(self.lib.tpw_v51124_ctor(o, n) == o)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_v51124_"
    f = lambda n: getattr(lib, p + n)
    f("alloc").restype, f("alloc").argtypes = C.c_uint64, [C.c_uint32]
    f("layout").argtypes = [C.POINTER(C.c_int64)]
    f("fmt").restype, f("fmt").argtypes = C.c_uint64, [C.c_uint64]
    f("read").restype, f("read").argtypes = C.c_int32, [C.c_uint32, C.c_uint64, C.c_uint32]
    f("errctor").restype, f("errctor").argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    f("erropl").restype, f("erropl").argtypes = C.c_int32, [C.c_uint64]
    f("dtor").restype, f("dtor").argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    f("ptrop").restype, f("ptrop").argtypes = C.c_uint64, [C.c_uint64]
    f("ctor").restype, f("ctor").argtypes = C.c_uint64, [C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = f("layout")(out)
    assert list(out)[:n] == [4], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        op, a = case["op"], case["args"]
        o = s.alloc(32)
        res = {}
        if op == "fmt":
            res["answer"] = s.fmt_(o)
        elif op == "read":
            impl = a[0]
            if impl:
                vt, io = s.alloc(32), s.alloc(16)
                s.word(io, vt)
                s.word(vt + 12, 0x10009999)
                s.word(o + 4, io)
            res["answer"] = s32(s.read_(o, impl, a[1], a[2]))
        elif op == "ect":
            s.word(o, 0xDEADBEEF)
            res["answer"] = s.errctor(o, a[0])
            res["word"] = s.rd(o)
        elif op == "eop":
            s.word(o, a[0])
            res["answer"] = s.erropl(o)
        elif op == "pop":
            s.word(o, a[0])
            res["answer"] = s.ptrop(o)
        elif op == "dtor":
            if a[0]:
                s.word(o, a[0])
            res["answer"] = s.dtor(o if a[2] else 0, a[1])
        elif op == "ctor":
            res["answer"] = s.ctor(o, a[0])
            res["set"] = int(s.rd(o) != 0)
        res["log"] = s.log()
        return res
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


W = [0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 5, 0x1000, 0x12345678]


def gen(op, argf, edges):
    def e():
        r = random.Random(7)
        for a in edges:
            yield {"op": op, "args": a, "queues": {"Read": [r.choice([0, 5, -1, 100])]}}

    def rn(r):
        return {"op": op, "args": argf(r), "queues": {"Read": [r.choice([0, 5, -1, 100, r.getrandbits(32)])]}}

    return dict(edge=e, rnd=rn)


def flag(r):
    return r.choice([0, 1, -1, 5, 0x7FFF, 0x8000, 0x10000, 0x18000, 0x10001, r.getrandbits(32)])


TESTS = [
    FnTest("CFormat::operator_tWAVEFORMATEX*", ppc, host, **gen("fmt", lambda r: [], [[]])),
    FnTest("bfl::FileSystem::CFile::Read", ppc, host,
           **gen("read", lambda r: [r.choice([0, 1]), r.choice([0, 0x7000]), r.getrandbits(r.choice([4, 32]))],
                 [[0, 0, 0], [1, 0, 0], [1, 0x7000, 16], [0, 0x7000, 16]])),
    FnTest("TbError::TbError", ppc, host, **gen("ect", lambda r: [r.choice(W)], [[w] for w in W])),
    FnTest("TbError::operator_long", ppc, host, **gen("eop", lambda r: [r.choice(W)], [[w] for w in W])),
    FnTest("CAutoArrayPtr<void*>::~CAutoArrayPtr", ppc, host,
           **gen("dtor", lambda r: [r.choice([0, 0x5000]), flag(r), r.choice([0, 1, 1, 1])],
                 [[p, f, s] for p in (0, 0x5000) for f in (0, 1, -1, 0x8000, 0x10000, 5) for s in (0, 1)])),
    FnTest("CAutoArrayPtr<void*>::operator_void**", ppc, host, **gen("pop", lambda r: [r.choice([0, 0x5000, 0x1000])], [[0], [0x5000]])),
    FnTest("CAutoArrayPtr<void*>::CAutoArrayPtr", ppc, host,
           **gen("ctor", lambda r: [r.choice([0, 1, 2, 7, 100, 0x4000, r.randrange(0, 1000)])], [[0], [1], [3], [0x40000000]])),
]
