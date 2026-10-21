"""Unit v5045, SAVEVAR<ResearchCat>, against libtpw_unit_SAVEVAR.dylib (tests/unit_SAVEVAR.cpp), main app, src/game/buildings/ResearchLab.cpp:
  SAVEVAR<ResearchCat> 0x10124680

A case is a file label, a value word, a name and noLog. WriteSerialiseLog, GAME_ASSERTMSG and LbFile_Write are fakes on both sides that
record their arguments (LbFile_Write also the bytes it was given); LbFile_Write answers a result and a transferred count from the case.
Compared: the answer, the event log, the value (unchanged). No fused operations. Not tested: the global 0x102a114c (a zero word the
original compares the result with; nothing writes it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SAVEVAR"
MASK = 0xFFFFFFFF

FN = 0x10124680
SIZES = {FN: 260}
OWN = set(SIZES)

WSL = 0x1015b3dc
ASSERT = 0x1000dde8
GSAVE = 0x1037fefc
FILE = 0x5000

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(WSL, CL.recorder("WriteSerialiseLog", "Lisi"))
STUBS.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))


def lbwrite(e):
    L = e.cl
    data = struct.unpack("<I", e.read(e.r(5), 4))[0]
    trans = e.u32(e.r(7))
    L.rec("LbFile_Write", (L.lab(e.r(4)), data, e.r(6) & MASK, trans))
    e.write(e.r(3), struct.pack(">I", L.ret("wres") & MASK))
    e.write(e.r(7), struct.pack(">I", L.ret("wcnt") & MASK))


STUBS.imp("LbFile_Write__FPvPCvUlPUl", lbwrite)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({GSAVE: 1, FILE: 2}, case["queues"])
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

    def call(self, value, name, nolog):
        r3, _, _ = T.call_ppc(self.e, FN, [FILE, value, name, nolog & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: lib.tpw_sv_gsave(), 2: FILE}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_sv_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, value, name, nolog):
        return self.lib.tpw_sv_f(FILE, value, name, T.s32(nolog)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sv_alloc.restype, lib.tpw_sv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sv_gsave.restype, lib.tpw_sv_gsave.argtypes = C.c_uint64, []
    lib.tpw_sv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sv_f.restype, lib.tpw_sv_f.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sv_layout(out)
    assert list(out)[:n] == [4], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        v = s.alloc(4)
        s.words(v, [case["value"]])
        nm = s.alloc(32)
        s.write(nm, case["name"].encode() + b"\0")
        ans = s.call(v, nm, case["nolog"])
        return {"answer": ans, "log": s.log(), "value": s.rd_word(v)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, value=None, res=None, cnt=None, nolog=None):
    return {"value": r.getrandbits(32) if value is None else value,
            "name": r.choice(["mNumProjects", "x", "mCat"]),
            "nolog": r.choice([0, 1, 5, -1]) if nolog is None else nolog,
            "queues": {"wres": [r.choice([0, 1, -1, 7, -0x80000000]) if res is None else res],
                       "wcnt": [r.choice([4, 4, 0, 3, 5]) if cnt is None else cnt]}}


def edges():
    r = random.Random(77)
    for nolog in (0, 1):
        for res in (0, 1, -1, 0x7FFFFFFF, -0x80000000):
            for cnt in (4, 0, 3, 5):
                yield mk(r, res=res, cnt=cnt, nolog=nolog)
    for v in (0, 1, 0xFFFFFFFF, 0x80000000, 0x12345678):
        yield mk(r, value=v, res=0, cnt=4, nolog=0)


def rnd(r):
    return mk(r)


TESTS = [
    FnTest("SAVEVAR<ResearchCat>", ppc, host, edge=edges, rnd=rnd),
]
