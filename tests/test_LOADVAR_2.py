"""Unit v5736, LOADVAR<CThoughts::ThoughtType>, against libtpw_unit_LOADVAR_2.dylib (tests/unit_LOADVAR_2.cpp), main app
("SimTheme Park"), src/game/buildings/TrainingCentre.cpp:
  LOADVAR<CThoughts::ThoughtType> 0x1012f700

A case is a world and a call. The world is built twice by ONE piece of Python code. The callees outside the unit
(CSaveLoad::WriteSerialiseLog, LbFile_Read, GAME_ASSERTMSG) are fakes on both sides that record their arguments; LbFile_Read
answers a TbError, a transferred count and the four file bytes from the case.

The world of a case: the value cell's old word, the file bytes, the error, the count, noLog. What is compared: the answer, the
value cell and the event log. No fused operations. Not tested: nothing left out.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LOADVAR_2"
MASK = 0xFFFFFFFF

FN = 0x1012F700
SIZES = {FN: 236}
OWN = set(SIZES)

WSL = 0x1015B3DC
READ = 0x10238848
ASSERT = 0x1000DDE8
GSAVELOAD = 0x1037FEFC
FILE = 0x00123450      # never dereferenced: unknown object, label -1 on both sides
NAME = 0x00200000

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_read(e):
    """TbError LbFile_Read(void* file, void* buf, ulong len, ulong* got): r3 is the hidden result pointer."""
    L = e.cl
    L.rec("LbFile_Read", (L.lab(e.r(4)), e.r(6) & MASK))
    err = L.ret("err") & MASK
    n = L.ret("n") & MASK
    data = L.ret("data") & MASK
    e.write(e.r(5), struct.pack("<I", data))   # the file's bytes: little-endian
    e.write(e.r(7), struct.pack(">I", n))
    e.write(e.r(3), struct.pack(">I", err))


STUBS.internal(WSL, CL.recorder("WriteSerialiseLog", "Lisi"))
STUBS.imp("LbFile_Read__FPvPvUlPUl",stub_read)
STUBS.internal(ASSERT, CL.recorder("assert", "is"))


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
        self.L = e.cl = CL.PpcLog({GSAVELOAD: 1}, case["queues"])
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

    def name(self):
        return self.e.alloc(16)

    def call(self, val, name, nolog):
        r3, _, _ = T.call_ppc(self.e, FN, [FILE, val, name, nolog & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
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

    def name(self):
        return self.lib.tpw_lv_alloc(16)

    def call(self, val, name, nolog):
        return self.lib.tpw_lv_load(FILE, val, name, nolog) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lv_addr.restype, lib.tpw_lv_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lv_alloc.restype, lib.tpw_lv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lv_load.restype, lib.tpw_lv_load.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lv_layout(out)
    assert list(out)[:n] == [4], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        val = s.alloc(4)
        s.words(val, [case["old"]])
        name = s.name()
        s.write(name, case["text"].encode() + b"\0")
        ans = s.call(val, name, case["nolog"])
        # success: the value as a number (the file is little-endian, the Mac swaps); failure: the bytes, which must stay as the fake left them
        value = s.rd_word(val) if ans == 0 else s.read(val, 4)
        return {"answer": ans, "value": value, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, err=None, n=None, nolog=None):
    err = r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, 5, -5]) if err is None else err
    n = r.choice([4, 4, 4, 0, 3, 5, 8]) if n is None else n
    data = r.choice([0, 0xFFFFFFFF, 0x01020304, 0x80000000, r.getrandbits(32), r.getrandbits(32)])
    return {"queues": {"err": [err & MASK], "n": [n], "data": [data]}, "old": r.getrandbits(32),
            "text": r.choice(["a", "mThought", "x" * 10]), "nolog": r.choice([0, 1, -1, 7]) if nolog is None else nolog}


def edges():
    r = random.Random(77)
    for err in (0, 1, -1, 0x7FFFFFFF, -0x80000000):
        for n in (4, 0, 3, 5):
            for nolog in (0, 1):
                yield mk(r, err, n, nolog)


def rnd(r):
    return mk(r)


TESTS = [FnTest("LOADVAR<CThoughts::ThoughtType>", ppc, host, edge=edges, rnd=rnd)]
