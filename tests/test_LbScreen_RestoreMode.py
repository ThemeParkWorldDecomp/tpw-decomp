"""Unit v5761, LbScreen_RestoreMode, against libtpw_unit_LbScreen_RestoreMode.dylib (tests/unit_LbScreen_RestoreMode.cpp), bullfrog,
src/bullfrog/WinScrn.cpp:
  LbScreen_RestoreMode 0x10069cac

The callees (ReleaseMode, IDirectDraw::RestoreDisplayMode, IUnknown::Release, LbSurface_RegisterDirectDraw) are fakes on both sides that
record their arguments (the DirectDraw object as a label). The world: gWinScrnDirectDraw null or set, and a hidden result word
pre-filled with junk. Compared: the result word, gWinScrnDirectDraw and the event log. No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbScreen_RestoreMode"
MASK = 0xFFFFFFFF

FN = 0x10069CAC
SIZES = {FN: 116}
OWN = set(SIZES)

RELEASE_MODE = 0x100689AC
REGISTER_DD = 0x10066FC0

ADDR = dict(dd=0x10075234, ddobj=0x10080000)
HOST_LAYOUT = [4]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_release_mode(e):
    e.cl.rec("ReleaseMode", ())


def stub_register(e):
    L = e.cl
    L.rec("RegisterDirectDraw", (L.lab(e.r(3)),))


def stub_restore(e):
    L = e.cl
    L.rec("RestoreDisplayMode", (L.lab(e.r(3)),))
    e.set_r(3, 0)


def stub_release(e):
    L = e.cl
    L.rec("Release", (L.lab(e.r(3)),))
    e.set_r(3, 0)


STUBS.internal(RELEASE_MODE, stub_release_mode)
STUBS.internal(REGISTER_DD, stub_register)
STUBS.imp("RestoreDisplayMode__11IDirectDrawFv", stub_restore)
STUBS.imp("Release__Q210NS_MacDoze8IUnknownFv", stub_release)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    pass


class Ppc(Side):
    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["ddobj"]: 1}, case["queues"])
        self.saved = STUBS.activate(e)
        self.buf = None

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def put_dd(self, on):
        self.e.write(ADDR["dd"], struct.pack(">I", ADDR["ddobj"] if on else 0))

    def get_dd(self):
        return struct.unpack(">I", self.e.read(ADDR["dd"], 4))[0] != 0

    def call(self):
        buf = self.e.alloc(16)
        self.e.write(buf, struct.pack(">I", 0xDEADBEEF))
        T.call_ppc(self.e, FN, [buf])
        return struct.unpack(">I", self.e.read(buf, 4))[0]

    def log(self):
        return self.L.log


class Host(Side):
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.dd = lib.tpw_rm_addr(b"dd")
        self.obj = lib.tpw_rm_addr(b"ddobj")
        CL.host_begin(lib, {1: self.obj}, case["queues"])

    def done(self):
        pass

    def put_dd(self, on):
        C.memmove(self.dd, struct.pack("<Q", self.obj if on else 0), 8)

    def get_dd(self):
        return struct.unpack("<Q", C.string_at(self.dd, 8))[0] != 0

    def call(self):
        return self.lib.tpw_rm_run() & MASK

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rm_addr.restype, lib.tpw_rm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rm_run.restype, lib.tpw_rm_run.argtypes = C.c_uint32, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_rm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.put_dd(case["dd"])
        ans = s.call()
        return {"answer": ans, "log": s.log(), "dd": s.get_dd()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def edges():
    for dd in (0, 1):
        yield {"dd": dd, "queues": {}}


def rnd(r):
    return {"dd": r.choice([0, 1]), "queues": {}}


TESTS = [
    FnTest("LbScreen_RestoreMode", ppc, host, edge=edges, rnd=rnd),
]
