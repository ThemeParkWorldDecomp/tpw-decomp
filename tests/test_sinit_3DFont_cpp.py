"""Unit v5109, the 3DFont.cpp static initialiser, against libtpw_unit_sinit_3DFont_cpp.dylib (tests/unit_sinit_3DFont_cpp.cpp), main app
("SimTheme Park"), src/Light_sinit_3DFont_cpp.cpp:
  __sinit_3DFont_cpp 0x1000757c

A case is a world and a call. The world is built twice by ONE piece of Python code. The callees CLight::CLight, SetNormal, SetColor and
__register_global_object are fakes on both sides that record their arguments (objects as labels: global 1, dtor 2, node 3, light 4);
CMapLinkBase::CMapLinkBase is inline on the host and records nothing on either side.

The world of a case: a random word in the light's mFlags. What is compared: mFlags after the call (2), and the event log. The Mac's
inline vtable words cannot be compared with the host's native layout. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_3DFont_cpp"
MASK = 0xFFFFFFFF

FN = 0x1000757c
SIZES = {FN: 228}
OWN = set(SIZES)

CALLEE_CMAPLINK = 0x10007b7c
CALLEE_CLIGHT = 0x10007a98
CALLEE_NORMAL = 0x10007964
CALLEE_COLOR = 0x1000784c

ADDR = dict(global_=0x102b34c8, node=0x102b353c, dtor=0x102657b8)
LIGHT_OFF_PPC, FLAGS_OFF_PPC = 0x10, 0x50


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_cmaplink(e):
    pass


def stub_clight(e):
    e.cl.rec("CLight", (e.cl.lab(e.r(3)), T.s32(e.r(4))))


def stub_normal(e):
    e.cl.rec("SetNormal", (e.cl.lab(e.r(3)), fb(e.f(1)), fb(e.f(2)), fb(e.f(3))))


def stub_color(e):
    e.cl.rec("SetColor", (e.cl.lab(e.r(3)), fb(e.f(1)), fb(e.f(2)), fb(e.f(3)), fb(e.f(4))))


def stub_register(e):
    e.cl.rec("register", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), e.cl.lab(e.r(5))))


STUBS.internal(CALLEE_CMAPLINK, stub_cmaplink)
STUBS.internal(CALLEE_CLIGHT, stub_clight)
STUBS.internal(CALLEE_NORMAL, stub_normal)
STUBS.internal(CALLEE_COLOR, stub_color)
STUBS.imp("__register_global_object", stub_register)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["global_"]: 1, ADDR["dtor"]: 2, ADDR["node"]: 3,
                                   ADDR["global_"] + LIGHT_OFF_PPC: 4}, case["queues"])
        self.saved = STUBS.activate(e)

    def flags_addr(self):
        return ADDR["global_"] + LIGHT_OFF_PPC + FLAGS_OFF_PPC

    def write_flags(self, v):
        self.e.write(self.flags_addr(), struct.pack(">I", v & MASK))

    def read_flags(self):
        return struct.unpack(">I", self.e.read(self.flags_addr(), 4))[0]

    def log(self):
        return self.L.log

    def call(self):
        T.call_ppc(self.e, FN, [])

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        g = lib.tpw_sinit_addr
        self.flags = g(b"global") + lib.offs[0] + lib.offs[1]
        CL.host_begin(lib, {1: g(b"global"), 2: g(b"dtor"), 3: g(b"node"), 4: g(b"global") + lib.offs[0]}, case["queues"])

    def write_flags(self, v):
        C.memmove(self.flags, struct.pack("<I", v & MASK), 4)

    def read_flags(self):
        return struct.unpack("<I", C.string_at(self.flags, 4))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_sinit_run()

    def done(self):
        pass


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sinit_addr.restype, lib.tpw_sinit_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sinit_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sinit_layout(out)
    lib.offs = list(out)[:n]
    assert 0 <= lib.offs[0] < 0x100 and 0 <= lib.offs[1] < 0x100, lib.offs
    lib.bound = True


def run(s, case):
    try:
        s.write_flags(case["flags"])
        s.call()
        return {"flags": s.read_flags(), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def edges():
    for f in (0, 2, MASK, 1, 0x80000000):
        yield {"queues": {}, "flags": f}


def rnd(r):
    return {"queues": {}, "flags": r.getrandbits(32)}


TESTS = [
    FnTest("__sinit_3DFont_cpp", ppc, host, edge=edges, rnd=rnd),
]
