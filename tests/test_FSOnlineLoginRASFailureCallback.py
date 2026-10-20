"""Unit v5113, the online login callbacks, against libtpw_unit_FSOnlineLoginRASFailureCallback.dylib, main app, src/FS_Online_FSOnlineLoginRASFailureCallback.cpp:
  FSOnlineLoginRASFailureCallback 0x1017100c, FSOnlineLoginConnectionCallback 0x10171178

The world: the global gOnlineSystem, either NULL (the system is made: operator new, the constructor fake) or an existing object. The
callees (the constructor, LoginRASCancelled, LoginConnectionCallback) are fakes on both sides recording their arguments. Compared: the log
and the global (unchanged, or made non-NULL). No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FSOnlineLoginRASFailureCallback"
MASK = 0xFFFFFFFF

FN_RAS, FN_CONN = 0x1017100C, 0x10171178
SIZES = {FN_RAS: 104, FN_CONN: 128}
OWN = set(SIZES)

CTOR, RAS_CANCELLED, CONN_CB = 0x1016F7D0, 0x10171114, 0x101715A0
ADDR = dict(gOnlineSystem=0x10380AB0)
HOST_LAYOUT = []

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(CTOR, CL.recorder("ctor", "N"))
STUBS.internal(RAS_CANCELLED, CL.recorder("LoginRASCancelled", "L"))
STUBS.internal(CONN_CB, CL.recorder("LoginConnectionCallback", "LiiLL"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(16)
        self.L = e.cl = CL.PpcLog({self.obj: 1}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def put_ptr(self, a, v):
        self.write(a, struct.pack(">I", v))

    def get_ptr(self, a):
        return struct.unpack(">I", self.read(a, 4))[0]

    def log(self):
        return self.L.log

    def call_ras(self):
        T.call_ppc(self.e, FN_RAS, [])

    def call_conn(self, a, b, c, d):
        T.call_ppc(self.e, FN_CONN, [a & MASK, b & MASK, c, d])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_frc_alloc(16)
        CL.host_begin(lib, {1: self.obj}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_frc_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def put_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def get_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call_ras(self):
        self.lib.tpw_frc_ras()

    def call_conn(self, a, b, c, d):
        self.lib.tpw_frc_conn(T.s32(a), T.s32(b), c, d)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_frc_addr.restype, lib.tpw_frc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_frc_alloc.restype, lib.tpw_frc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_frc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_frc_ras.restype, lib.tpw_frc_ras.argtypes = None, []
    lib.tpw_frc_conn.restype, lib.tpw_frc_conn.argtypes = None, [C.c_int32, C.c_int32, C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_frc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.bound = True


def run(s, case, fn):
    try:
        g = s.g("gOnlineSystem")
        s.put_ptr(g, s.obj if case["pre"] else 0)
        if fn == "ras":
            s.call_ras()
        else:
            s.call_conn(*case["args"])
        v = s.get_ptr(g)
        return {"log": s.log(), "kept": v == s.obj, "set": v != 0}
    finally:
        s.done()


def mk(fn):
    return (lambda e, case: run(Ppc(e, case), case, fn)), (lambda lib, case: run(Host(lib, case), case, fn))


def base(pre, args=None):
    return {"pre": pre, "queues": {}, "args": args or [0, 0, 0, 0]}


def ras_edges():
    yield base(False)
    yield base(True)


def ras_random(r):
    return base(r.random() < 0.5)


def conn_case(r, pre):
    def p():
        return r.choice([0, r.randrange(1, 1 << 32)])
    return base(pre, [r.choice([0, 1, -1, r.randrange(-1000, 1000), r.randrange(-(1 << 31), 1 << 31)]) & MASK,
                      r.choice([0, 1, -1, r.randrange(-1000, 1000), r.randrange(-(1 << 31), 1 << 31)]) & MASK, p(), p()])


def conn_edges():
    r = random.Random(5113)
    for pre in (False, True):
        yield base(pre)
        yield base(pre, [MASK, 0x80000000, 0x1000, 0x2000])
        for _ in range(3):
            yield conn_case(r, pre)


def conn_random(r):
    return conn_case(r, r.random() < 0.5)


ras_ppc, ras_host = mk("ras")
conn_ppc, conn_host = mk("conn")

TESTS = [
    FnTest("FSOnlineLoginRASFailureCallback", ras_ppc, ras_host, edge=ras_edges, rnd=ras_random),
    FnTest("FSOnlineLoginConnectionCallback", conn_ppc, conn_host, edge=conn_edges, rnd=conn_random),
]
