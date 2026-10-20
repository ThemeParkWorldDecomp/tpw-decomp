"""CResearchCategory::Serialise 0x101242e4, against libtpw_unit_CResearchCategory_Serialise.dylib.

SAVEVAR/LOADVAR instances (unsigned long, int, unsigned char) are fakes on both sides recording file label, field offset, the value
(save) and the answer from the queue "rc" (a list consumed in call order); load fakes also store the queue "val" into the field.
Compared: the answer, the log, the four words of the object. No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CResearchCategory_Serialise"
MASK = 0xFFFFFFFF
FN = 0x101242e4
FILE_ADDR = 0x5000
STUBS = CL.Stubs()
STUBS.real.add(FN)

NAMES = {"effort": 0x102456c4, "output": 0x102456cb, "currentProject": 0x102456d2, "highestGroupAvailable": 0x102456e1}


def make(kind, addr, size, cond):
    def stub(e):
        L = e.cl
        name = e.cstr(e.r(5))
        off = (e.r(4) - e.obj) & MASK
        if kind == "SAVEVAR":
            v = int.from_bytes(e.read(e.r(4), size), "big")
            L.rec("SAVEVAR:" + name, (L.lab(e.r(3)), off, v, e.r(6)))
        else:
            L.rec("LOADVAR:" + name, (L.lab(e.r(3)), off, e.r(6)))
            val = L.ret("val") & ((1 << (8 * size)) - 1)
            e.write(e.r(4), val.to_bytes(size, "big"))
        e.set_r(3, L.ret("rc") & MASK)
    STUBS.internal(addr, stub)


for k, (a, b, c) in {"SAVEVAR": (0x1000f5dc, 0x1000f4b0, 0x100f1a04), "LOADVAR": (0x1000f02c, 0x1000ef18, 0x100f17c8)}.items():
    make(k, a, 4, 0)
    make(k, b, 4, 0)
    make(k, c, 1, 0)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({FILE_ADDR: 1}, case["queues"])
        self.saved = STUBS.activate(e)

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

    def file(self):
        return FILE_ADDR

    def call(self, obj, args):
        self.e.obj = obj
        r3, _, _ = T.call_ppc(self.e, FN, [obj, self.file()] + [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.f = lib.tpw_rc_alloc(16)
        CL.host_begin(lib, {1: self.f}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_rc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def log(self):
        return CL.host_end(self.lib)

    def file(self):
        return self.f

    def call(self, obj, args):
        return self.lib.tpw_rc_serialise(obj, self.f, *[T.s32(a) if i < 2 else a & MASK for i, a in enumerate(args)]) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rc_alloc.restype, lib.tpw_rc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rc_serialise.restype = C.c_int32
    lib.tpw_rc_serialise.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rc_layout(out)
    assert list(out)[:n] == [0, 4, 8, 12], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        obj = s.alloc(16)
        s.words(obj, case["init"])
        ans = s.call(obj, case["args"])
        return {"answer": ans, "log": s.log(), "obj": s.rd_words(obj, 3), "b": s.read(obj + 12, 1)[0]}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, save, version, rcs=None):
    rc = rcs if rcs is not None else [r.choice([0, 0, 0, 1, 2, -1]) for _ in range(4)]
    return {"queues": {"rc": [x & MASK for x in rc], "val": [r.getrandbits(32), r.getrandbits(32), r.getrandbits(32), r.getrandbits(32)]},
            "init": [r.getrandbits(32) for _ in range(3)] + [r.getrandbits(8) * 0x01010101], "args": [save, version, r.getrandbits(32)]}


def edges():
    r = random.Random(77)
    for save in (0, 1, 5):
        for ver in (-1, 0, 1, 2, 3, 0x80000000):
            yield mk(r, save, ver, [0, 0, 0, 0])
            for k in range(4):
                rcs = [0] * 4
                rcs[k] = 1
                yield mk(r, save, ver, rcs)


def rnd(r):
    return mk(r, r.choice([0, 1]), r.choice([0, 1, 2, -1, r.randrange(-3, 5)]))


TESTS = [FnTest("CResearchCategory::Serialise", ppc, host, edge=edges, rnd=rnd)]
