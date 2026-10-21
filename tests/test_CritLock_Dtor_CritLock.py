"""Unit v5732, CritLock::~CritLock, against libtpw_unit_CritLock_Dtor_CritLock.dylib (tests/unit_CritLock_Dtor_CritLock.cpp), bullfrog,
src/bullfrog/FileSystem.cpp:
  CritLock::~CritLock 0x10014738

A case is a CritLock object (or NULL) holding a section pointer (or NULL) and a delete flag. LeaveCriticalSection and operator delete are
fakes on both sides recording the pointer as a label. Compared: the answer (as a label), the word at offset 0 afterwards (as a label),
the event log. No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_CritLock_Dtor_CritLock"
MASK = 0xFFFFFFFF

FN = 0x10014738
SIZES = {FN: 108}
OWN = set(SIZES)
LEAVE = 0x1006BD84
DELETE = 0x1006B2D4
HOST_LAYOUT = [0]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_leave(e):
    L = e.cl
    L.rec("Leave", (L.lab(e.r(3)),))


def stub_delete(e):
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)),))


STUBS.internal(LEAVE, stub_leave)
STUBS.internal(DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def put_ptr(self, a, v):
        self.write(a, struct.pack(">I", v))

    def get_ptr(self, a):
        return self.rd_word(a)

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(16)
        self.sect = e.alloc(16)
        self.L = e.cl = CL.PpcLog({self.obj: 1, self.sect: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, this, flag):
        r3, _, _ = T.call_ppc(self.e, FN, [this & MASK, flag & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    # the host's pointer is 8 bytes wide (host addresses exceed 32 bits)
    def put_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def get_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_crit_alloc(16)
        self.sect = lib.tpw_crit_alloc(16)
        CL.host_begin(lib, {1: self.obj, 2: self.sect}, case["queues"])

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, this, flag):
        return self.lib.tpw_crit_dtor(this, C.c_int32(T.s32(flag)).value)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_crit_alloc.restype, lib.tpw_crit_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_crit_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_crit_dtor.restype, lib.tpw_crit_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_crit_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def label(s, v):
    return 0 if v == 0 else 1 if v == s.obj else 2 if v == s.sect else -1


def run(s, case):
    try:
        s.put_ptr(s.obj, s.sect if case["has_sect"] else 0)
        this = s.obj if case["has_obj"] else 0
        ans = s.call(this, case["flag"])
        return {"answer": label(s, ans), "word": label(s, s.get_ptr(s.obj)), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


FLAGS = [0, 1, -1, 0x7FFF, 0x8000, 0x10000, 0x10001, 0xFFFF8000, 2, -2]


def mk(has_obj, has_sect, flag):
    return {"queues": {}, "has_obj": has_obj, "has_sect": has_sect, "flag": flag & MASK}


def edges():
    for o in (0, 1):
        for s in (0, 1):
            for f in FLAGS:
                yield mk(o, s, f)


def rnd(r):
    return mk(r.choice([0, 1, 1]), r.choice([0, 1]), r.choice([r.choice(FLAGS), r.getrandbits(32)]))


TESTS = [
    FnTest("CritLock::~CritLock", ppc, host, edge=edges, rnd=rnd),
]
