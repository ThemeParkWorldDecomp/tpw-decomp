"""Unit v5643, SAVEVAR<CThoughts::ThoughtType> 0x1012f844, against libtpw_unit_SAVEVAR_4.dylib (tests/unit_SAVEVAR_4.cpp), main app,
src/TrackCell_SAVEVARSTRUCT_SAVEVAR_4.cpp.

The callees CSaveLoad::WriteSerialiseLog, GAME_ASSERTMSG and LbFile_Write are fakes on both sides recording their arguments (the write
records the swapped word). The world: the value word, the noLog flag, the write's error and transferred count from the case's queues.
Compared: the answer, the value word, the event log. The data word at 0x102a43fc (the TbError constant) is zero and never written.
No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SAVEVAR_4"
MASK = 0xFFFFFFFF

FN = 0x1012F844
WRITELOG, ASSERT, LBWRITE = 0x1015B3DC, 0x1000DDE8, 0x10238878

ADDR = dict(gSaveLoad=0x1037FEFC, file=0x102A4400)
HOST_LAYOUT = [4]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.add(FN)
STUBS.internal(WRITELOG, CL.recorder("WriteSerialiseLog", "Lisi"))
STUBS.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))


def stub_write(e):
    L = e.cl
    word = struct.unpack(">I", e.read(e.r(5), 4))[0]
    L.rec("LbFile_Write", (L.lab(e.r(4)), e.r(6) & MASK, word))
    err = L.ret("LbFile_Write") & MASK
    e.write(e.r(3), struct.pack(">I", err))
    e.write(e.r(7), struct.pack(">I", L.ret("written") & MASK))


STUBS.internal(LBWRITE, stub_write)


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
        self.L = e.cl = CL.PpcLog({ADDR["gSaveLoad"]: 1, ADDR["file"]: 2}, case["queues"])
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

    def call(self, file, value, name, nolog):
        r3, _, _ = T.call_ppc(self.e, FN, [file, value, name, nolog & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("gSaveLoad"), 2: self.g("file")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sv4_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sv4_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, file, value, name, nolog):
        return self.lib.tpw_sv4_f(file, value, name, s32(nolog)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_sv4_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "f").restype = C.c_uint32
    getattr(lib, p + "f").argtypes = [C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        v = s.alloc(8)
        s.words(v, [case["value"], 0x5A5A5A5A])
        nm = s.alloc(16)
        s.write(nm, b"thoughtName\0")
        ans = s.call(s.g("file"), v, nm, case["nolog"])
        return {"answer": ans, "log": s.log(), "value": s.rd_words(v, 2)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make(r, nolog=None, value=None, err=None, written=None):
    return {
        "nolog": r.choice([0, 1, -1, 5]) if nolog is None else nolog,
        "value": r.getrandbits(32) if value is None else value,
        "queues": {
            "LbFile_Write": [r.choice([0, 0, 1, -1, -5, 0x80000000, 0x7FFFFFFF]) if err is None else err],
            "written": [r.choice([4, 4, 0, 3, 5, 0xFFFFFFFF]) if written is None else written],
        },
    }


def edges():
    r = random.Random(643)
    for nolog in (0, 1, -1):
        for err in (0, 1, -1, 0x80000000, 0x7FFFFFFF):
            for written in (4, 0, 3, 5):
                yield make(r, nolog, r.getrandbits(32), err, written)
    for v in (0, 1, 0x80000000, 0xFFFFFFFF, 0x12345678):
        yield make(r, 0, v, 0, 4)


def rnd(r):
    return make(r)


TESTS = [
    FnTest("SAVEVAR<CThoughts::ThoughtType>", ppc, host, edge=edges, rnd=rnd),
]
