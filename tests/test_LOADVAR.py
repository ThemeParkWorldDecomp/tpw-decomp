"""Unit v5315, LOADVAR<CPerson::PersonState>, against libtpw_unit_LOADVAR.dylib (tests/unit_LOADVAR.cpp), main app, src/..._GetBagged_LOADVAR.cpp:
  LOADVAR<CPerson::PersonState> 0x1011b7cc

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian, host memory little-endian).
The callees outside the unit (CSaveLoad::WriteSerialiseLog, LbFile_Read with its hidden TbError result, GAME_ASSERTMSG) are fakes on both
sides that record their arguments; LbFile_Read hands over the case's four file bytes, the transferred count and the TbError from queues.

Compared: the answer, the value word, the event log. No fused operations. Not tested: a NULL value pointer (the original writes through it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LOADVAR"
MASK = 0xFFFFFFFF

FN = 0x1011b7cc
SIZES = {FN: 236}
OWN = set(SIZES)

WRITELOG = 0x1015b3dc
READ = 0x10238848
ASSERT = 0x1000dde8

ADDR = dict(gSaveLoad=0x1037fefc)


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_read(e):
    L = e.cl
    L.rec("LbFile_Read", (L.lab(e.r(4)), e.r(6) & MASK))
    e.write(e.r(5), struct.pack("<I", L.case["data"]))
    e.write(e.r(7), struct.pack(">I", L.ret("xfer") & MASK))
    e.write(e.r(3), struct.pack(">I", L.ret("read") & MASK))


STUBS.internal(WRITELOG, CL.recorder("WriteSerialiseLog", "Lis" + "i"))
STUBS.internal(ASSERT, CL.recorder("assert", "is"))
STUBS.internal(READ, stub_read)


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
        self.L = e.cl = CL.PpcLog({0x1037fefc: 1}, case["queues"])
        self.L.case = case
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

    def call(self, file, value, name, nolog):
        r3, _, _ = T.call_ppc(self.e, FN, [file, value, name, nolog & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.case = case
        CL.host_begin(lib, {1: lib.tpw_lv_addr(b"gSaveLoad")}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_lv_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, file, value, name, nolog):
        return self.lib.tpw_lv_load(file, value, name, s32(nolog), self.case["data"]) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lv_addr.restype, lib.tpw_lv_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lv_alloc.restype, lib.tpw_lv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lv_load.restype = C.c_int32
    lib.tpw_lv_load.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lv_layout(out)
    assert list(out)[:n] == [4], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        file = s.alloc(16)
        value = s.alloc(4)
        name = s.alloc(32)
        s.words(value, [case["init"]])
        s.write(name, case["name"].encode() + b"\0")
        ans = s.call(file, value, name, case["nolog"])
        return {"answer": ans, "value": s.read(value, 4)[::-1] if (ans == 0 and s.fmt == "<I") else s.read(value, 4), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, nolog=None, err=None, xfer=None, data=None):
    return {"nolog": r.choice([0, 1, -1, 7]) if nolog is None else nolog,
            "queues": {"read": [r.choice([0, 0, 1, -1, 0x80000000, 5, 0x7fffffff]) if err is None else err],
                       "xfer": [r.choice([4, 4, 0, 3, 5, 8]) if xfer is None else xfer]},
            "data": r.getrandbits(32) if data is None else data,
            "init": r.getrandbits(32),
            "name": r.choice(["state", "PersonState", ""])}


def edges():
    r = random.Random(5315)
    for nolog in (0, 1, -1):
        for err in (0, 1, -1, 0x80000000, 0x7fffffff):
            for xfer in (4, 0, 3, 5):
                yield mk(r, nolog, err, xfer)
    yield mk(r, 0, 0, 4, 0x12345678)
    yield mk(r, 0, 0, 4, 0)
    yield mk(r, 0, 0, 4, 0xFFFFFFFF)


def rnd(r):
    return mk(r)


TESTS = [FnTest("LOADVAR<CPerson::PersonState>", ppc, host, edge=edges, rnd=rnd)]
