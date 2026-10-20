"""Unit v5710, the MSL std::exception destructor of the main app, against libtpw_unit_std_exception_Dtor_exception_2.dylib
(tests/unit_std_exception_Dtor_exception_2.cpp), main app ("SimTheme Park"), src/TPMain_std_exception_Dtor_exception_2.cpp:
  std::exception::~exception 0x10002184

A case is an object (NULL or one word of garbage) and a delete flag (r4, only its low 16 bits are read, signed). The callee, operator
delete (glue 0x10238218), is a fake on both sides recording a "delete" event. What is compared: the answer (this, as NULL or the object),
the object's word (the vtable global 0x10278bc0 or the untouched garbage) and the event log. No fused operations. Not tested: nothing
left out (the __unexpected landing pad is not reachable).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_exception_Dtor_exception_2"
MASK = 0xFFFFFFFF

FN_DTOR = 0x10002184
SIZES = {FN_DTOR: 108}
OWN = set(SIZES)
OPERATOR_DELETE = 0x10238218
ADDR = dict(vtbl=0x10278bc0)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_delete(e):
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)),))


STUBS.internal(OPERATOR_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def dtor(self, obj, flag):
        r3, _, _ = T.call_ppc(self.e, FN_DTOR, [obj & MASK, flag & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_exc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_exc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def dtor(self, obj, flag):
        return self.lib.tpw_exc_dtor(obj, T.s32(flag)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_exc_addr.restype, lib.tpw_exc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_exc_alloc.restype, lib.tpw_exc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_exc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_exc_dtor.restype, lib.tpw_exc_dtor.argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_exc_layout(out)
    assert list(out)[:n] == [4 if False else 8], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        obj = 0
        if case["has_obj"]:
            obj = s.alloc(16)
            s.words(obj, [case["garbage"]])
        ans = s.dtor(obj, case["flag"])
        word = None
        if obj:
            w = s.rd_word(obj)
            word = "VT" if w == s.g("vtbl") & MASK else w   # the host's pointer is 8 bytes: its low half sits first (little endian)
        return {"this": "NULL" if ans == 0 else ("OBJ" if ans == obj & MASK else ans), "word": word, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(has_obj, flag, garbage=0x12345678):
    return {"queues": {}, "has_obj": has_obj, "flag": flag & MASK, "garbage": garbage}


def edges():
    for has in (False, True):
        for flag in (0, 1, 2, 0xFFFF, 0x7FFF, 0x8000, 0x8001, 0x10000, 0x10001, 0x18000, 0xFFFF0001, 0xFFFFFFFF, 0x80000000):
            yield mk(has, flag)
    yield mk(True, 1, 0)
    yield mk(True, 1, 0x10278BC0)


def rnd(r):
    return mk(r.random() < 0.85, r.choice([r.getrandbits(32), r.randrange(-3, 4), r.randrange(0, 0x20000)]), r.getrandbits(32))


TESTS = [
    FnTest("std::exception::~exception", ppc, host, edge=edges, rnd=rnd),
]
