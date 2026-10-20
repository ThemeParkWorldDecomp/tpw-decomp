"""Unit v5765, Sock::Close, against libtpw_unit_Sock_Close.dylib (tests/unit_Sock_Close.cpp), main app
("SimTheme Park"), src/COnlineParser_ParseGoto_COnlineParser_ParseIgnore_Sock_Close.cpp:
  Sock::Close 0x1020e358

A case is a Sock object (vtable word, socket handle at +4) and a call. shutdown, recv and closesocket are fakes on both sides that record
their arguments; recv answers from the queue "Recv" (the last value repeats).

What is compared: the event log and the two words of the object (the handle is -1 afterwards unless it was -1 already).

No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Sock_Close"
MASK = 0xFFFFFFFF

FN_CLOSE = 0x1020E358
SIZES = {FN_CLOSE: 132}
OWN = set(SIZES)

CALLEE_SHUTDOWN = 0x1023A7B0
CALLEE_RECV = 0x1023A780
CALLEE_CLOSESOCKET = 0x1023A7C8

HOST_LAYOUT = [4, 8]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_shutdown(e):
    e.cl.rec("Shutdown", (s32(e.r(3)), s32(e.r(4))))


def stub_recv(e):
    L = e.cl
    L.rec("Recv", (s32(e.r(3)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("Recv") & MASK)


def stub_closesocket(e):
    e.cl.rec("CloseSocket", (s32(e.r(3)),))


STUBS.internal(CALLEE_SHUTDOWN, stub_shutdown)
STUBS.internal(CALLEE_RECV, stub_recv)
STUBS.internal(CALLEE_CLOSESOCKET, stub_closesocket)


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

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_close(self, obj):
        T.call_ppc(self.e, FN_CLOSE, [obj & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_sock_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_close(self, obj):
        self.lib.tpw_sock_close(obj)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sock_alloc.restype, lib.tpw_sock_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sock_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sock_close.restype, lib.tpw_sock_close.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sock_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        obj = s.alloc(8)
        s.words(obj, [case["vt"], case["sock"]])
        s.call_close(obj)
        return {"log": s.log(), "obj": s.rd_words(obj, 2)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(sock, recvs, vt=0x10270000):
    return {"vt": vt, "sock": sock & MASK, "queues": {"Recv": [v & MASK for v in recvs]}}


def edges():
    yield mk(-1, [5])
    yield mk(0, [0])
    yield mk(5, [0])
    yield mk(5, [-1])
    yield mk(5, [1, 0])
    yield mk(5, [256, 256, 3, 0])
    yield mk(7, [3, -1])
    yield mk(0x7FFFFFFF, [1, 1, 1, 0])
    yield mk(-2, [0])


def rnd(r):
    sock = r.choice([-1, 0, 1, r.randrange(2, 1000), -r.randrange(2, 50)])
    vals = [r.choice([1, 2, 255, 256, 0x7FFFFFFF]) for _ in range(r.randrange(0, 6))]
    vals.append(r.choice([0, -1, -100, 0x80000000]))
    return mk(sock, vals, r.randrange(0, 1 << 32))


TESTS = [
    FnTest("Sock::Close", __import__("sys").modules[__name__].ppc, host, edge=edges, rnd=rnd),
]
