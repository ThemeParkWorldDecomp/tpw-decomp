"""Unit v5569, World.cpp's static initialiser, against libtpw_unit_sinit_World_cpp.dylib (tests/unit_sinit_World_cpp.cpp), main app
("SimTheme Park"), src/World_save_sinit_World_cpp.cpp:
  __sinit_World_cpp 0x10141c14

The world is built twice by one piece of Python code (emulator memory and host memory). The callees (__construct_array, the list
constructor, the wide string constructor, __register_global_object) are fakes on both sides that record their arguments (objects as
labels, the string constructor's text).

The world of a case: random initial words at 0x1037ec00 and 0x1037ec04 and a random halfword at 0x1034cad4. What is compared: those
three globals and the event log. No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_World_cpp"
MASK = 0xFFFFFFFF

FN = 0x10141C14
SIZES = {FN: 348}
OWN = set(SIZES)

ADDR = dict(
    ec00=0x1037EC00, ec04=0x1037EC04, win=0x1034CAD4, array=0x1034CBFC, ctorvec=0x10266DB8, list=0x1034CBD8, listdtor=0x10266DB0,
    wdtor=0x103BCBB8, nlist=0x1034CBE8, nbus=0x1034CB8C, nsea=0x1034CB6C, nferry=0x1034CB4C, ngates=0x1034CB2C, nlights=0x1034CB0C,
    nend=0x1034CAEC, bus=0x1034CB78, sea=0x1034CB58, ferry=0x1034CB38, gates=0x1034CB18, lights=0x1034CAF8, end=0x1034CAD8)
LABELS = {n: i + 1 for i, n in enumerate(sorted(ADDR))}
HOST_LAYOUT = []

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.imp("__construct_array", CL.recorder("construct_array", "LLLii"))
STUBS.internal(0x10141D94, CL.recorder("ListCtor", "L"))
STUBS.imp("__ct__26TbDynamicStringTemplate<w>FPCw", CL.recorder("StringCtor", "LS"))
STUBS.imp("__register_global_object", CL.recorder("register", "LLL"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR[n]: l for n, l in LABELS.items()}, case["queues"])
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
        CL.host_begin(lib, {l: self.g(n) for n, l in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sinit_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_sinit_run()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sinit_addr.restype, lib.tpw_sinit_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sinit_alloc.restype, lib.tpw_sinit_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sinit_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sinit_run.restype, lib.tpw_sinit_run.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_sinit_layout(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.bound = True


def run(s, case):
    try:
        s.words(s.g("ec00"), [case["a"]])
        s.words(s.g("ec04"), [case["b"]])
        s.half(s.g("win"), case["w"])
        s.call()
        return {"log": s.log(), "ec00": s.read(s.g("ec00"), 4)[::1 if s.fmt[0] == ">" else -1],
                "ec04": s.read(s.g("ec04"), 4)[::1 if s.fmt[0] == ">" else -1],
                "win": s.read(s.g("win"), 2)[::1 if s.fmt[0] == ">" else -1]}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r):
    return {"queues": {}, "a": r.getrandbits(32), "b": r.getrandbits(32), "w": r.getrandbits(16)}


def edges():
    r = random.Random(5)
    c = mk(r)
    yield c
    yield {"queues": {}, "a": 0, "b": 0, "w": 0}
    yield {"queues": {}, "a": MASK, "b": MASK, "w": 0xFFFF}


def rnd(r):
    return mk(r)


TESTS = [
    FnTest("__sinit_World_cpp", ppc, host, edge=edges, rnd=rnd),
]
