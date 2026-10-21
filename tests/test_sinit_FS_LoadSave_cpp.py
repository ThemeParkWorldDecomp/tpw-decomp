"""Unit v5720, the static initialiser of FS_LoadSave.cpp, against libtpw_unit_sinit_FS_LoadSave_cpp.dylib
(tests/unit_sinit_FS_LoadSave_cpp.cpp), main app, src/game/save/FS_LoadSave.cpp:
  __sinit_FS_LoadSave_cpp 0x101fd668

A case is a world and a call: gLoadSave's four words start random (both memories), the call runs, and the four words and the event log
(__register_global_object with object, destructor and node as labels, a fake on both sides) are compared. No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_FS_LoadSave_cpp"
MASK = 0xFFFFFFFF

FN = 0x101fd668
SIZES = {FN: 84}
OWN = set(SIZES)

ADDR = dict(words=0x10396a7c, node=0x10396a8c, dtor=0x10268660)
HOST_LAYOUT = [16]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_register(e):
    L = e.cl
    L.rec("register", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))


STUBS.imp("__register_global_object", stub_register)


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
        self.L = e.cl = CL.PpcLog({0x10396a7c: 1, 0x10268660: 2, 0x10396a8c: 3}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self):
        T.call_ppc(self.e, FN, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("words"), 2: self.g("dtor"), 3: self.g("node")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sl_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_sl_sinit()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sl_addr.restype, lib.tpw_sl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sl_alloc.restype, lib.tpw_sl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sl_sinit.restype, lib.tpw_sl_sinit.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_sl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.words(s.g("words"), case["words"])
        s.words(s.g("node"), [0, 0, 0])
        s.call()
        return {"log": s.log(), "words": s.rd_words(s.g("words"), 4), "node": s.rd_words(s.g("node"), 3)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def case_of(words):
    return {"queues": {}, "words": words}


def edges():
    yield case_of([0, 0, 0, 0])
    yield case_of([MASK, MASK, MASK, MASK])
    yield case_of([0x102b154c, 0x102b1540, 1, 2])


def rnd(r):
    return case_of([r.getrandbits(32) for _ in range(4)])


TESTS = [
    FnTest("__sinit_FS_LoadSave_cpp", ppc, host, edge=edges, rnd=rnd),
]
