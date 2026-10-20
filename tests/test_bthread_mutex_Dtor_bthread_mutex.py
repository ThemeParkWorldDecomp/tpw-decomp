"""Unit v5120, bthread_mutex::~bthread_mutex, against libtpw_unit_bthread_mutex_Dtor_bthread_mutex.dylib
(tests/unit_bthread_mutex_Dtor_bthread_mutex.cpp), main app, src/email_bthread_mutex_Dtor_bthread_mutex.cpp:
  bthread_mutex::~bthread_mutex 0x10200d04 (host: bthread_mutex::Destroy, the hidden delete flag as a parameter)

A case is a 16-byte mutex object (or NULL) and a delete flag (r4; only its low halfword counts, extsh). NS_MacDoze::DeleteCriticalSection
and operator delete are fakes on both sides recording the object (as a label). Compared: the answer (this, as a label), the event log and
the object's first word. No fused operations. Not tested: none.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_bthread_mutex_Dtor_bthread_mutex"
MASK = 0xFFFFFFFF

FN = 0x10200D04
DELETE_CS = 0x1023B338
OP_DELETE = 0x10238218
HOST_LAYOUT = [0]

STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_delete_cs(e):
    e.cl.rec("DeleteCriticalSection", (e.cl.lab(e.r(3)),))


def stub_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


STUBS.internal(DELETE_CS, stub_delete_cs)
STUBS.internal(OP_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def label(self, a):
        return 1 if a and a == self.obj else (0 if a == 0 else -1)


class Ppc(Side):
    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(16)
        self.L = e.cl = CL.PpcLog({self.obj: 1}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def write(self, a, data):
        self.e.write(a, data)

    def word(self):
        return struct.unpack(">I", self.e.read(self.obj, 4))[0]

    def log(self):
        return self.L.log

    def call(self, ptr, flag):
        r3, _, _ = T.call_ppc(self.e, FN, [ptr & MASK, flag & MASK])
        return r3 & MASK


class Host(Side):
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_mtxd_alloc(16)
        CL.host_begin(lib, {1: self.obj}, case["queues"])

    def done(self):
        pass

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def word(self):
        return struct.unpack("<I", C.string_at(self.obj, 4))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, ptr, flag):
        return self.lib.tpw_mtxd_destroy(ptr, T.s16(flag))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_mtxd_alloc.restype, lib.tpw_mtxd_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_mtxd_destroy.restype, lib.tpw_mtxd_destroy.argtypes = C.c_uint64, [C.c_uint64, C.c_int16]
    out = (C.c_int64 * 32)()
    n = lib.tpw_mtxd_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.write(s.obj, (case["handle"] & MASK).to_bytes(4, ">" if isinstance(s, Ppc) else "<") if False else
                struct.pack(">I" if isinstance(s, Ppc) else "<I", case["handle"] & MASK))
        ptr = 0 if case["null"] else s.obj
        ans = s.call(ptr, case["flag"])
        return {"answer": s.label(ans), "log": s.log(), "word": s.word()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, null, flag):
    return {"queues": {}, "null": null, "flag": flag & MASK, "handle": r.getrandbits(32)}


FLAGS = [0, 1, -1, 2, 0x7FFF, -0x8000, 0x8000, 0x10000, 0x12340000, 0x12340001, 0x1234FFFF, 0xFFFF0000, 0xFFFF0001, 0x80000001]


def edges():
    r = random.Random(5120)
    for null in (True, False):
        for f in FLAGS:
            yield mk(r, null, f)


def rnd(r):
    return mk(r, r.random() < 0.2, r.choice(FLAGS + [r.getrandbits(32)]))


TESTS = [
    FnTest("bthread_mutex::~bthread_mutex", ppc, host, edge=edges, rnd=rnd),
]
