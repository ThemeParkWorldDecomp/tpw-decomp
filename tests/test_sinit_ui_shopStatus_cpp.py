"""Unit v5716, the static initialiser of ui_shopStatus.cpp, against libtpw_unit_sinit_ui_shopStatus_cpp.dylib
(tests/unit_sinit_ui_shopStatus_cpp.cpp), main app ("SimTheme Park"), src/CreateStaffCostsPanel_UpdateStaffCost_sinit_ui_shopStatus_cpp.cpp:
  __sinit_ui_shopStatus_cpp 0x101bdfb8

The world: gShopStatusPanel's first word and its table pointer hold garbage before the call. The callee UIStatusPanel::UIStatusPanel
(0x101929e8) is a fake on both sides recording its object. Compared: the event log, the word at +0, and whether the pointer at +0x20
is the address of the shop panel's table (0x102adbd8 on the Mac, gShopStatusOps on the host).

No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_ui_shopStatus_cpp"
MASK = 0xFFFFFFFF

FN = 0x101BDFB8
SIZES = {FN: 76}
OWN = set(SIZES)
CTOR = 0x101929E8
ADDR = dict(panel=0x103959E0, ops=0x102ADBD8)
HOST_LAYOUT = [48, 8]      # the host's offset of mOps (Mac 0x20) and its size

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ctor(e):
    e.cl.rec("UIStatusPanel", (e.cl.lab(e.r(3)),))


STUBS.internal(CTOR, stub_ctor)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["panel"]: 1}, {})
        self.saved = STUBS.activate(e)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def ops_is_set(self):
        return struct.unpack(">I", self.read(ADDR["panel"] + 0x20, 4))[0] == ADDR["ops"]

    def word0(self):
        return struct.unpack(">I", self.read(ADDR["panel"], 4))[0]

    def call(self):
        T.call_ppc(self.e, FN, [])

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("panel")}, {})

    def g(self, name):
        return self.lib.tpw_sinit_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ops_is_set(self):
        return struct.unpack("<Q", self.read(self.g("panel") + HOST_LAYOUT[0], 8))[0] == self.g("ops")

    def word0(self):
        return struct.unpack("<I", self.read(self.g("panel"), 4))[0]

    def call(self):
        self.lib.tpw_sinit_run()

    def done(self):
        pass


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sinit_addr.restype, lib.tpw_sinit_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sinit_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sinit_run.restype, lib.tpw_sinit_run.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_sinit_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        p = s.g("panel")
        s.words(p, [case["w0"]])
        if s.fmt == "<I":
            s.write(p + HOST_LAYOUT[0], struct.pack("<Q", case["w20"] | (case["w0"] << 32)))
        else:
            s.write(p + 0x20, struct.pack(">I", case["w20"] & MASK))
        s.call()
        return {"log": s.log(), "word0": s.word0(), "ops": s.ops_is_set()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r):
    return {"w0": r.getrandbits(32), "w20": r.getrandbits(32)}


def edges():
    import random
    r = random.Random(5716)
    yield {"w0": 0, "w20": 0}
    yield {"w0": 1, "w20": ADDR["ops"]}
    yield base(r)


def rnd(r):
    return base(r)


TESTS = [
    FnTest("__sinit_ui_shopStatus_cpp", ppc, host, edge=edges, rnd=rnd),
]
