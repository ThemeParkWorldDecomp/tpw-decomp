"""Unit v5733, FixedSizeArray<BehaviourEntry>::~FixedSizeArray, against libtpw_unit_FixedSizeArray_Dtor_FixedSizeArray.dylib
(tests/unit_FixedSizeArray_Dtor_FixedSizeArray.cpp), main app ("SimTheme Park"), src/game/rides/VehicleCallbacks.cpp:
  FixedSizeArray<BehaviourEntry>::~FixedSizeArray 0x101355dc

A case is an object (or NULL) whose first word points at a block (or is NULL), and a hidden delete flag (r4, only the low 16 bits are read, signed).
Built twice by one piece of Python code. __destroy_new_array (glue 0x10238ad0) and operator delete (glue 0x10238218) are fakes on both sides that
record their arguments (pointers as labels). Compared: the answer (this, as a label) and the call log.

No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FixedSizeArray_Dtor_FixedSizeArray"
MASK = 0xFFFFFFFF

FN = 0x101355DC
SIZES = {FN: 96}
OWN = set(SIZES)

DESTROY = 0x10238AD0
DELETE = 0x10238218

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_destroy(e):
    L = e.cl
    L.rec("DestroyArray", (L.lab(e.r(3)), T.s32(e.r(4))))


def stub_delete(e):
    L = e.cl
    L.rec("Delete", (L.lab(e.r(3)),))


STUBS.internal(DESTROY, stub_destroy)
STUBS.internal(DELETE, stub_delete)


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"      # the Mac's pointer

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(16)
        self.data = e.alloc(16)
        self.L = e.cl = CL.PpcLog({self.obj: 1, self.data: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, this, flag):
        r3, _, _ = T.call_ppc(self.e, FN, [this & MASK, flag & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"      # the host's native pointer

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        self.obj = lib.tpw_fsa_alloc(16)
        self.data = lib.tpw_fsa_alloc(16)
        CL.host_begin(lib, {1: self.obj, 2: self.data}, case["queues"])

    def done(self):
        pass

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, this, flag):
        return self.lib.tpw_fsa_dtor(this, T.s32(flag))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_fsa_addr.restype, lib.tpw_fsa_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fsa_alloc.restype, lib.tpw_fsa_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fsa_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fsa_dtor.restype, lib.tpw_fsa_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_fsa_layout(out)
    assert list(out)[:n] == [0], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.write(s.obj, struct.pack(s.pfmt, s.data if case["hasdata"] else 0))
        this = s.obj if case["hasthis"] else 0
        r = s.call(this, case["flag"])
        label = 0 if r == 0 else (1 if r == s.obj else -1)
        return {"answer": label, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(hasthis, hasdata, flag):
    return {"queues": {}, "hasthis": hasthis, "hasdata": hasdata, "flag": flag & MASK}


def edges():
    for flag in (0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x18000, 0xFFFF8001, 0x80000000, 0xFFFFFFFF, 0x7FFF0000):
        for hasthis in (True, False):
            for hasdata in (True, False):
                yield mk(hasthis, hasdata, flag)


def rnd(r):
    return mk(r.random() < 0.8, r.random() < 0.8, r.choice([r.getrandbits(32), r.getrandbits(16), r.randrange(-3, 4)]))


TESTS = [
    FnTest("FixedSizeArray<BehaviourEntry>::~FixedSizeArray", ppc, host, edge=edges, rnd=rnd),
]
