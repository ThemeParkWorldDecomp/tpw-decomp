"""Unit v5742, the static initialiser of FS_Online.cpp, against libtpw_unit_sinit_FS_Online_cpp.dylib (tests/unit_sinit_FS_Online_cpp.cpp),
main app ("SimTheme Park"), src/game/online/FS_Online.cpp:
  __sinit_FS_Online_cpp 0x10178308

The function takes no input. The string constructor and __register_global_object are fakes on both sides that record their arguments
(the object as a label, the literal text, the destructor glue address, the registration node as a label). The whole log is compared:
two constructions then two registrations, in order.

No fused operations. Not tested: nothing.
"""
import ctypes as C

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_FS_Online_cpp"
MASK = 0xFFFFFFFF

FN = 0x10178308
CTOR = 0x10238800
REGISTER = 0x10239940
DESTROY_GLUE = 0x103bcbb8

ADDR = dict(str1=0x10380ad4, str2=0x10380ab4, nodeA=0x10380ae8, nodeB=0x10380ac8)
LABELS = {"str1": 1, "str2": 2, "nodeA": 3, "nodeB": 4}
HOST_LAYOUT = []

STUBS = CL.Stubs()
STUBS.real.add(FN)
_ctor = CL.recorder("ctor", "LS")


def save_toc(e):
    """What the glue does first: stw r2, 0x14(r1); the caller reloads r2 from there after the call."""
    e.write(e.r(1) + 0x14, (e.r(2) & MASK).to_bytes(4, "big"))


def stub_ctor(e):
    save_toc(e)
    _ctor(e)


STUBS.internal(CTOR, stub_ctor)


def stub_register(e):
    save_toc(e)
    L = e.cl
    L.rec("register", (L.lab(e.r(3)), 1 if e.r(4) == DESTROY_GLUE else 0, L.lab(e.r(5))))


STUBS.internal(REGISTER, stub_register)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR[k]: LABELS[k] for k in ADDR}, {})
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def call(self):
        T.call_ppc(self.e, FN, [])

    def log(self):
        return self.L.log


class Host:
    def __init__(self, lib):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {LABELS[k]: lib.tpw_sinit_addr(k.encode()) for k in ADDR}, {})

    def done(self):
        pass

    def call(self):
        self.lib.tpw_sinit_run()

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sinit_addr.restype, lib.tpw_sinit_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sinit_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sinit_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s):
    try:
        s.call()
        return {"log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e))


def host(lib, case):
    return run(Host(lib))


def edges():
    yield {}


def rnd(r):
    return {}


TESTS = [
    FnTest("__sinit_FS_Online_cpp", ppc, host, edge=edges, rnd=rnd),
]
