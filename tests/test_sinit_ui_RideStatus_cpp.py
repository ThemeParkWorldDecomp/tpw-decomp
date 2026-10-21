"""Unit v5300, the ui_RideStatus.cpp static initialiser, against libtpw_unit_sinit_ui_RideStatus_cpp.dylib, main app:
  __sinit_ui_RideStatus_cpp 0x101bc3ec

The global panel is pre-filled with garbage on both sides, the function runs, and the constructor call (a fake on both sides that
records its object), the first word, the table pointer (+0x20, compared as "is the ops table") and the word at +0x28 are compared.

No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_ui_RideStatus_cpp"
MASK = 0xFFFFFFFF

FN = 0x101bc3ec
CTOR = 0x101929e8
ADDR = dict(panel=0x103959a4, ops=0x102ad5fc)

STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_ctor(e):
    e.cl.rec("UIStatusPanel", (e.cl.lab(e.r(3)),))


STUBS.internal(CTOR, stub_ctor)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({ADDR["panel"]: 1}, {})
        self.saved = STUBS.activate(e)

    def fill(self, c):
        p = ADDR["panel"]
        self.e.write(p, struct.pack(">I", c[0]))
        self.e.write(p + 0x20, struct.pack(">I", c[1]))
        self.e.write(p + 0x28, struct.pack(">I", c[2]))

    def call(self):
        T.call_ppc(self.e, FN, [])

    def observe(self):
        p = ADDR["panel"]
        return {"f00": self.e.u32(p), "ops": self.e.u32(p + 0x20) == ADDR["ops"], "rawops": self.e.u32(p + 0x20) if self.e.u32(p + 0x20) != ADDR["ops"] else 0,
                "f28": self.e.u32(p + 0x28), "log": self.e.cl.log}

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.a = lambda n: lib.tpw_sinit_addr(n.encode())
        CL.host_begin(lib, {1: self.a("panel")}, {})

    def fill(self, c):
        C.memmove(self.a("f00"), struct.pack("<I", c[0]), 4)
        # the table pointer is a Ptr32 (the arena offset of RideStatusPanelTable(), unit v5L31)
        C.memmove(self.a("mops"), struct.pack("<I", self.lib.tpw_sinit_off(self.a("ops")) + 8 if c[1] & 1 else (0x1000 + c[1]) & 0xFFFFFFFF), 4)
        C.memmove(self.a("f28"), struct.pack("<I", c[2]), 4)

    def call(self):
        self.lib.tpw_sinit_run()

    def observe(self):
        ops = struct.unpack("<I", C.string_at(self.a("mops"), 4))[0]
        return {"f00": struct.unpack("<I", C.string_at(self.a("f00"), 4))[0], "ops": ops == self.lib.tpw_sinit_off(self.a("ops")), "rawops": 0,
                "f28": struct.unpack("<I", C.string_at(self.a("f28"), 4))[0], "log": CL.host_end(self.lib)}

    def done(self):
        pass


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sinit_addr.restype, lib.tpw_sinit_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sinit_alloc.restype, lib.tpw_sinit_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sinit_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sinit_off.restype, lib.tpw_sinit_off.argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sinit_layout(out)
    assert list(out)[:n] == [4, 4], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.fill(case["fill"])
        s.call()
        o = s.observe()
        o.pop("rawops")
        return o
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def edges():
    yield {"fill": (0, 0, 0)}
    yield {"fill": (MASK, MASK, MASK)}
    yield {"fill": (1, ADDR["ops"], 1)}


def rnd(r):
    return {"fill": (r.getrandbits(32), r.getrandbits(32), r.getrandbits(32))}


TESTS = [
    FnTest("__sinit_ui_RideStatus_cpp", ppc, host, edge=edges, rnd=rnd),
]
