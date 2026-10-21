"""Unit v5125, std::exception::~exception (a destructor with the hidden short delete flag), against
libtpw_unit_std_exception_Dtor_exception.dylib (tests/unit_std_exception_Dtor_exception.cpp), ltms library,
src/ltms/MslSupport.cpp:
  std::exception::~exception 0x10008304

A case is an object (or NULL) and a flag. The world is built by one piece of Python code on both memories. operator delete (glue
0x1000f694) is a fake on both sides recording the object. The object's first word starts as a marker; what is compared: the answer
(NULL, the object or other), whether the first word now holds the vtable address, the rest of the object, the event log.

No fused operations. Not tested: __unexpected (unreachable cleanup).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_std_exception_Dtor_exception"
MASK = 0xFFFFFFFF

FN = 0x10008304
OWN = {FN}
OPERATOR_DELETE = 0x1000F694
ADDR = dict(vtbl=0x100114DC)       # the ltms TOC base + 0x104c

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_delete(e):
    e.cl.rec("OperatorDelete", (e.cl.lab(e.r(3)),))


STUBS.internal(OPERATOR_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


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

    def rd_ptr(self, a):
        return self.rd_words(a, 1)[0]

    def call(self, obj, flag):
        r3, _, _ = T.call_ppc(self.e, FN, [obj & MASK, flag & MASK])
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
        return self.lib.tpw_stdexc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_stdexc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def call(self, obj, flag):
        return self.lib.tpw_stdexc_dtor(obj, T.s32(flag))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_stdexc_addr.restype, lib.tpw_stdexc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_stdexc_alloc.restype, lib.tpw_stdexc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_stdexc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_stdexc_dtor.restype, lib.tpw_stdexc_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 4)()
    n = lib.tpw_stdexc_layout(out)
    assert list(out)[:n] == [], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        obj = 0
        if case["has_obj"]:
            obj = s.alloc(16)
            s.words(obj, [case["marker"], case["marker"], case["tail"], case["tail"] ^ 0xFFFFFFFF])
        ans = s.call(obj, case["flag"])
        res = {"answer": 0 if ans == 0 else (1 if ans == obj else 2), "log": s.log()}
        if obj:
            res["vt"] = s.rd_ptr(obj) == s.g("vtbl")
            res["rest"] = s.rd_words(obj + 8, 2)
        return res
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, has_obj, flag):
    return {"queues": {}, "has_obj": has_obj, "flag": flag & MASK, "marker": r.choice([0, 0x11111111, 0xDEADBEEF]),
            "tail": r.getrandbits(32)}


def edges():
    r = random.Random(5125)
    yield mk(r, False, 0)
    yield mk(r, False, 1)
    yield mk(r, False, 0xFFFFFFFF)
    for f in (0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0xFFFF0001, 0x00018000, 0xFFFFFFFF, 0x80000000):
        yield mk(r, True, f)


def rnd(r):
    return mk(r, r.random() < 0.9, r.choice([r.getrandbits(32), r.randrange(-3, 4), r.randrange(0, 3)]))


TESTS = [
    FnTest("std::exception::~exception", ppc, host, edge=edges, rnd=rnd),
]
