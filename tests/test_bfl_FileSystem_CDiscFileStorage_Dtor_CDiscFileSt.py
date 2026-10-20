"""Unit v5110, the ltms CDiscFileStorage destructor 0x100034c0, against libtpw_unit_bfl_FileSystem_CDiscFileStorage_Dtor_CDiscFileSt.dylib,
src/TbIResourceIndex_bfl_FileSystem_CDiscFileStorage_Dtor_CDiscFileSt.cpp:
  bfl::FileSystem::CDiscFileStorage::~CDiscFileStorage 0x100034c0

A case is an object (or NULL) and the hidden delete flag word. The callees (the base destructor 0x1000f79c and operator delete
0x1000f694) are fakes on both sides recording the object (all blocks are unknown objects, label -1; NULL is 0).
Compared: the answer (NULL, this or other) and the event log. The vtable store is not compared (the host has no Mac vtable).
No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_bfl_FileSystem_CDiscFileStorage_Dtor_CDiscFileSt"
MASK = 0xFFFFFFFF

FN = 0x100034C0
SIZES = {FN: 104}
OWN = set(SIZES)
CALLEE_BASE_DTOR = 0x1000F79C
CALLEE_DELETE = 0x1000F694

HOST_LAYOUT = []

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_base_dtor(e):
    e.cl.rec("CFileStorage_dtor", (e.cl.lab(e.r(3)),))


def stub_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


STUBS.internal(CALLEE_BASE_DTOR, stub_base_dtor)
STUBS.internal(CALLEE_DELETE, stub_delete)


def setup(runner):
    pass


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def log(self):
        return self.L.log

    def call(self, obj, flag):
        r3, _, _ = T.call_ppc(self.e, FN, [obj & MASK, flag & MASK])
        return r3 & MASK


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_cdfs_alloc(n)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, obj, flag):
        freed = C.c_int32(0)
        r = self.lib.tpw_cdfs_dtor(obj, T.s32(flag), C.byref(freed))
        self.freed = freed.value
        return r


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cdfs_addr.restype, lib.tpw_cdfs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cdfs_alloc.restype, lib.tpw_cdfs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cdfs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cdfs_dtor.restype, lib.tpw_cdfs_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32, C.POINTER(C.c_int32)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cdfs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        obj = s.alloc(16) if case["obj"] else 0
        ans = s.call(obj, case["flag"])
        log = s.log()
        if isinstance(s, Ppc):
            # the host sees operator delete through the heap (see the unit's cpp): the PowerPC delete events become a flag
            freed = int(any(ev_[0] == "delete" for ev_ in log))
            log = [ev_ for ev_ in log if ev_[0] != "delete"]
        else:
            freed = s.freed
        return {"answer": "null" if ans == 0 else ("this" if ans == obj else "other"), "log": log, "freed": freed}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(obj, flag):
    return {"queues": {}, "obj": obj, "flag": flag & MASK}


def edges():
    for obj in (0, 1):
        for flag in (0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0xFFFFFFFF, 0x10000, 0x10001, 0x18000, 0x7FFF0001, 0x80000001):
            yield mk(obj, flag)


def rnd(r):
    return mk(r.choice([0, 1, 1]), r.choice([r.getrandbits(32), r.getrandbits(16), r.randrange(-3, 4)]))


TESTS = [
    FnTest("bfl::FileSystem::CDiscFileStorage::~CDiscFileStorage", ppc, host, edge=edges, rnd=rnd),
]
