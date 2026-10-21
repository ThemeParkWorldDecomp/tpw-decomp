"""Unit v5067, SAVEVARSTRUCT<CThoughts::EventHistType>, against libtpw_unit_SAVEVARSTRUCT.dylib (tests/unit_SAVEVARSTRUCT.cpp), main app,
src/game/buildings/TrainingCentre.cpp:
  SAVEVARSTRUCT<CThoughts::EventHistType> 0x1012f9a0

The world is built twice by one piece of code. Callees WriteSerialiseLog, GAME_ASSERTMSG and LbFile_Write are fakes on both sides recording
their arguments (the write fake also records the four bytes it is given); LbFile_Write answers a result and a written count from the case.
Compared: the answer, the call log, the value afterwards. No fused operations. Not tested: nothing excluded (the TbError constant word at
0x102a43f8 is never written, so it stays zero).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SAVEVARSTRUCT"
MASK = 0xFFFFFFFF

FN = 0x1012f9a0
SIZES = {FN: 260}
OWN = set(SIZES)

WSL = 0x1015b3dc
ASSERT = 0x1000dde8
LBWRITE = 0x10238878

ADDR = dict(gSaveLoad=0x1037fefc)
HOST_LAYOUT = [2, 4]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_wsl(e):
    L = e.cl
    L.rec("WriteSerialiseLog", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(6))), [e.cstr(e.r(5))])


def stub_assert(e):
    L = e.cl
    L.rec("GAME_ASSERTMSG", (T.s32(e.r(3)),), [e.cstr(e.r(4))])


def stub_write(e):
    L = e.cl
    b = e.read(e.r(5), 4)
    L.rec("LbFile_Write", (L.lab(e.r(4)), b[0], b[1], b[2], b[3], e.r(6)), [])
    written = L.ret("written") & MASK
    e.write(e.r(7), struct.pack(">I", written))
    e.write(e.r(3), struct.pack(">I", L.ret("LbFile_Write") & MASK))


STUBS.internal(WSL, stub_wsl)
STUBS.internal(ASSERT, stub_assert)
STUBS.internal(LBWRITE, stub_write)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({0x1037fefc: 1}, case["queues"])
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
        with_ = T.call_ppc(self.e, FN, [file & MASK, value & MASK, name & MASK, nolog & MASK])
        return with_[0] & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("gSaveLoad")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sv_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sv_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, file, value, name, nolog):
        return self.lib.tpw_sv_save(file, value, name, T.s32(nolog)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sv_addr.restype, lib.tpw_sv_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sv_alloc.restype, lib.tpw_sv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sv_save.restype, lib.tpw_sv_save.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sv_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        file = s.alloc(16)
        value = s.alloc(8)
        name = s.alloc(16)
        s.write(value, struct.pack(s.fmt[0] + "HH", case["a"], case["b"]))
        s.write(name, b"mActionHistIndex\0"[:16])
        answer = s.call(file, value, name, case["nolog"])
        after = struct.unpack(s.fmt[0] + "HH", s.read(value, 4))
        return {"answer": answer, "log": s.log(), "value": after}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, nolog, result, written, a=None, b=None):
    return {"nolog": nolog, "a": r.randrange(0x10000) if a is None else a, "b": r.randrange(0x10000) if b is None else b,
            "queues": {"LbFile_Write": [result & MASK], "written": [written]}}


def edges():
    r = random.Random(5067)
    for nolog in (0, 1, 7, -1):
        for result in (0, 1, 0x7FFFFFFF, -1, 0x80000000):
            for written in (4, 0, 3, 5):
                yield mk(r, nolog, result, written)
    yield mk(r, 0, 0, 4, 0x1234, 0x89AB)
    yield mk(r, 1, 0, 4, 0xFFFF, 0)
    yield mk(r, 1, 0, 4, 0, 0xFFFF)


def rnd(r):
    return mk(r, r.choice([0, 1, r.randrange(-5, 5)]), r.choice([0, 0, 1, -1, r.randrange(-1 << 31, 1 << 31)]),
              r.choice([4, 4, 0, 3, 5, r.randrange(0, 10)]))


TESTS = [
    FnTest("SAVEVARSTRUCT<CThoughts::EventHistType>", ppc, host, edge=edges, rnd=rnd),
]
