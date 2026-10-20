"""Unit v5304, the socket wrapper's constructor and SendData, against libtpw_unit_CSock_SendData.dylib (tests/unit_CSock_SendData.cpp),
main app ("SimTheme Park"), src/Email_Send_CSock_SendData.cpp:
  CSock::CSock 0x10201f50, CSock::SendData 0x10202058

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory (big-endian) for the original,
and in host memory (little-endian) for the translation. Callees outside the unit are fakes on both sides: Sock::Sock (0x1020e2a0)
records one event and writes two marker words at +4 and +8; send (glue 0x1023a798) records socket, length, flags, the first byte and
the byte sum of the data, and answers from the case's queue (negative as is, else min(value, length)). memset is the standard stub.

The world of a case: a 0x18 byte object of garbage words (its word at +4 is the socket) and a data buffer. What is compared: the answer
(for the constructor: that it returned the object), the six object words, the buffer and the event log. No fused operations.
Not tested: a send that answers 0 forever (the original loops forever too).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CSock_SendData"
MASK = 0xFFFFFFFF

FN_CTOR, FN_SEND = 0x10201f50, 0x10202058
SIZES = {FN_CTOR: 88, FN_SEND: 148}
OWN = set(SIZES)
CALLEE_SOCK = 0x1020e2a0

HOST_LAYOUT = [4, 8, 0x18]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_sock(e):
    e.cl.rec("SockCtor", (1,))
    e.write(e.r(3) + 4, struct.pack(">II", 0x5ca1ab1e, 0x11111111))


def stub_send(e):
    L = e.cl
    ln = s32(e.r(5))
    data = e.read(e.r(4), ln) if ln > 0 else b""
    L.rec("send", (s32(e.r(3)), ln, s32(e.r(6)), data[0] if data else 0, sum(data)))
    q = s32(L.ret("send"))
    e.set_r(3, (q if q < 0 else min(q, ln)) & MASK)


STUBS.internal(CALLEE_SOCK, stub_sock)
STUBS.imp("send", stub_send)


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

    def ctor(self, obj):
        r3, _, _ = T.call_ppc(self.e, FN_CTOR, [obj])
        return int((r3 & MASK) == obj)

    def send(self, obj, buf, ln):
        r3, _, _ = T.call_ppc(self.e, FN_SEND, [obj, buf, ln & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_cs_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ctor(self, obj):
        return self.lib.tpw_cs_ctor(obj)

    def send(self, obj, buf, ln):
        return self.lib.tpw_cs_send(obj, buf, s32(ln)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cs_alloc.restype, lib.tpw_cs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cs_ctor.restype, lib.tpw_cs_ctor.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_cs_send.restype, lib.tpw_cs_send.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        obj = s.alloc(0x18)
        s.words(obj, case["obj"])
        size = max(len(case["buf"]), 4)
        buf = s.alloc(size)
        s.write(buf, case["buf"])
        if case["op"] == "ctor":
            ans = s.ctor(obj)
        else:
            ans = s.send(obj, 0 if case["null"] else buf, case["len"])
        return {"answer": ans, "log": s.log(), "obj": s.rd_words(obj, 6), "buf": s.read(buf, size)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


FULL = 0x7FFFFFFF


def make(r, op, ln=0, null=False, queue=None):
    n = ln if ln > 0 else 4
    buf = r.getrandbits(8 * n).to_bytes(n, "big")
    return {"op": op, "len": ln & MASK, "null": null, "buf": buf, "obj": [r.getrandbits(32) for _ in range(6)],
            "queues": {"send": queue if queue else [FULL]}}


def rqueue(r):
    k = r.choice([0, 0, 1, 2, 3])
    q = [r.randrange(1, 6000) for _ in range(k)]
    q.append(r.choice([FULL, FULL, -1, -5]))
    return q


def ctor_edges():
    r = random.Random(11)
    yield make(r, "ctor")
    yield make(r, "ctor")


def ctor_random(r):
    return make(r, "ctor")


def send_edges():
    r = random.Random(12)
    queues = [[FULL], [-1], [FULL, -1], [1, FULL], [0x2000, FULL], [100, 5000, FULL], [0x1fff, 0x2001, FULL], [5, -3]]
    for ln in (0, 1, 2, 0x1fff, 0x2000, 0x2001, 0x4000, 0x4001, 20000, -1, -0x2000, 0x80000000):
        for q in queues:
            yield make(r, "send", ln - (1 << 32) if ln >= 0x80000000 else ln, queue=q)
    for ln in (0, 1, 0x2001, -1):
        yield make(r, "send", ln, null=True)


def send_random(r):
    ln = r.choice([0, 1, 0x2000, 0x2001, r.randrange(0, 20000), r.randrange(0, 300), -r.randrange(1, 100)])
    return make(r, "send", ln, null=(r.random() < 0.08), queue=rqueue(r))


TESTS = [
    FnTest("CSock::CSock", ppc, host, edge=ctor_edges, rnd=ctor_random),
    FnTest("CSock::SendData", ppc, host, edge=send_edges, rnd=send_random),
]
