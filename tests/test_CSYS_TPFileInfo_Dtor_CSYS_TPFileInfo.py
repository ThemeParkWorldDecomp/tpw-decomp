"""Unit v5714, CSYS_TPFileInfo::~CSYS_TPFileInfo, against libtpw_unit_CSYS_TPFileInfo_Dtor_CSYS_TPFileInfo.dylib, main app,
src/FS_System_CSYS_TPFileInfo_Dtor_CSYS_TPFileInfo.cpp:
  CSYS_TPFileInfo::~CSYS_TPFileInfo 0x10005288

A case is an object (or NULL) and a delete flag. The string destructor (0x102382f0) and operator delete (0x10238218) are fakes on both
sides that record the string or block as a label (object, +0x14, ... +0x50 on the Mac; the members on the host). Compared: the event
log (five string destructions, last member first, then the delete when the signed 16 bit flag is positive) and that r3 is this.
No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CSYS_TPFileInfo_Dtor_CSYS_TPFileInfo"
MASK = 0xFFFFFFFF

FN = 0x10005288
SIZES = {FN: 160}
OWN = set(SIZES)

STR_DTOR = 0x102382F0
OP_DELETE = 0x10238218
OFFS = [0x00, 0x14, 0x28, 0x3C, 0x50]
HOST_OFFS = []   # filled by bind: offsets of the five members on the host

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_str_dtor(e):
    e.cl.rec("StrDtor", (e.cl.lab(e.r(3)),))


def stub_delete(e):
    e.cl.rec("Delete", (e.cl.lab(e.r(3)),))


STUBS.internal(STR_DTOR, stub_str_dtor)
STUBS.internal(OP_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(0x64) if not case["null"] else 0
        fixed = {}
        if self.obj:
            for i, o in enumerate(OFFS):
                fixed[self.obj + o] = i + 1
        self.L = e.cl = CL.PpcLog(fixed, {})
        self.saved = STUBS.activate(e)

    def call(self, flag):
        r3, _, _ = T.call_ppc(self.e, FN, [self.obj, flag & MASK])
        return (r3 & MASK) == self.obj

    def log(self):
        return self.L.log

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_fi_alloc(HOST_OFFS[5]) if not case["null"] else 0
        fixed = {}
        if self.obj:
            for i in range(5):
                fixed[i + 1] = self.obj + HOST_OFFS[i]
        CL.host_begin(lib, fixed, {})

    def call(self, flag):
        self.lib.tpw_fi_dtor(self.obj, T.s16(flag))
        return True

    def log(self):
        return CL.host_end(self.lib)

    def done(self):
        pass


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_fi_alloc.restype, lib.tpw_fi_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fi_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fi_dtor.restype, lib.tpw_fi_dtor.argtypes = None, [C.c_uint64, C.c_int16]
    out = (C.c_int64 * 32)()
    n = lib.tpw_fi_layout(out)
    HOST_OFFS[:] = [0] + list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        answer = s.call(case["flag"])
        return {"answer": answer, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


FLAGS = [0, 1, -1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0xFFFF8000, 0x12340000, 0x00018000]


def mk(null, flag):
    return {"null": null, "flag": flag & MASK}


def edges():
    for null in (False, True):
        for f in FLAGS:
            yield mk(null, f)


def rnd(r):
    return mk(r.random() < 0.1, r.choice(FLAGS) if r.random() < 0.5 else r.getrandbits(32))


TESTS = [
    FnTest("CSYS_TPFileInfo::~CSYS_TPFileInfo", ppc, host, edge=edges, rnd=rnd),
]
