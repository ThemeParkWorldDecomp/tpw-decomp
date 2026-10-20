"""Static initialiser of ui_cityLobbyController.cpp (0x101e0cec), against libtpw_unit_sinit_ui_cityLobbyController_cpp.dylib.
The string constructor (0x10238800) and __register_global_object (0x10239940) are fakes on both sides recording their arguments
(objects, destructor and nodes as labels, the text pointer as its offset into the string pool). Compared: the event log.
No fused operations. Not tested: the text contents (the pool is not modelled)."""
import ctypes as C

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_ui_cityLobbyController_cpp"
MASK = 0xFFFFFFFF
FN = 0x101E0CEC
CTOR = 0x10238800
REG = 0x10239940
ADDR = dict(pool=0x1024EDC2, strA=0x10395C94, strB=0x10395C74, nodeA=0x10395CA8, nodeB=0x10395C88, dtor=0x103BCBB8)
LABELS = {"strA": 1, "strB": 2, "nodeA": 3, "nodeB": 4, "dtor": 5}

STUBS = CL.Stubs()
STUBS.real.add(FN)


def save_toc(e):
    """The real glue saves the TOC pointer at 0x14(r1) for the caller to reload."""
    e.write(e.r(1) + 0x14, (e.r(2) & MASK).to_bytes(4, "big"))


def stub_ctor(e):
    save_toc(e)
    e.cl.rec("Ctor", (e.cl.lab(e.r(3)), e.r(4) - ADDR["pool"]))


def stub_reg(e):
    save_toc(e)
    L = e.cl
    L.rec("Register", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))


STUBS.internal(CTOR, stub_ctor)
STUBS.internal(REG, stub_reg)


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc(e, case):
    e.cl = CL.PpcLog({ADDR[k]: v for k, v in LABELS.items()}, {})
    saved = STUBS.activate(e)
    try:
        T.call_ppc(e, FN, [])
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"log": e.cl.log}


def host(lib, case):
    lib.tpw_sinit_addr.restype, lib.tpw_sinit_addr.argtypes = C.c_uint64, [C.c_char_p]
    CL.host_begin(lib, {v: lib.tpw_sinit_addr(k.encode()) for k, v in LABELS.items()}, {})
    lib.tpw_sinit_run()
    return {"log": CL.host_end(lib)}


def edges():
    yield {}


def rnd(r):
    return {}


TESTS = [FnTest("__sinit_ui_cityLobbyController_cpp", ppc, host, edge=edges, rnd=rnd)]
