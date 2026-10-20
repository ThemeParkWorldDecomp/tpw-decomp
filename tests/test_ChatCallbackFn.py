"""Unit v5212, the chat callback, against libtpw_unit_ChatCallbackFn.dylib (tests/unit_ChatCallbackFn.cpp), main app
("SimTheme Park"), src/COnlineControl_..._ChatCallbackFn.cpp:
  ChatCallbackFn 0x10214a0c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory (big-endian) and in the host library.
The callees outside the unit (operator new, the COnlineControl constructor, mutex lock and unlock, AddUIError, COnlineControl::CallbackFn)
are fakes on both sides that record their arguments; a pointer to the control is logged as "is gOnlineControl", the argument vector by the
marker word it points to.

The world of a case: gOnlineControl either NULL (created on first use) or a block of random bytes; an id; a vector block with a marker.
What is compared: the log, the first 0x40 bytes of the control, whether the control is the preset one, the marker.

No fused operations. Not tested: nothing is left out.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ChatCallbackFn"
MASK = 0xFFFFFFFF

FN = 0x10214A0C
SIZES = {FN: 260}
OWN = set(SIZES)

NEW, CTOR, LOCK, UNLOCK, ADDERR, CBFN = 0x102384B8, 0x10213870, 0x10200D8C, 0x10200DE0, 0x101F1DF0, 0x10214B70

ADDR = dict(gOnline=0x10396D28)
HOST_LAYOUT = [0x10, 0xE8C]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cur(e):
    return e.u32(ADDR["gOnline"])


def stub_new(e):
    n = e.r(3) & MASK
    e.cl.rec("new", (n,))
    e.set_r(3, e.alloc(n))


def stub_ctor(e):
    e.cl.rec("ctor", ())


def stub_lock(e):
    e.cl.rec("lock", (1 if ((e.r(3) - 0x1C) & MASK) == cur(e) else 0,))


def stub_unlock(e):
    e.cl.rec("unlock", (1 if ((e.r(3) - 0x1C) & MASK) == cur(e) else 0,))


def stub_err(e):
    e.cl.rec("AddUIError", (s32(e.r(3)), 1 if e.r(4) else 0))


def stub_cb(e):
    e.cl.rec("CallbackFn", (1 if (e.r(3) & MASK) == cur(e) else 0, s32(e.r(4)), e.u32(e.r(5) & MASK)))


STUBS.internal(NEW, stub_new)
STUBS.internal(CTOR, stub_ctor)
STUBS.internal(LOCK, stub_lock)
STUBS.internal(UNLOCK, stub_unlock)
STUBS.internal(ADDERR, stub_err)
STUBS.internal(CBFN, stub_cb)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def wr_ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

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

    def call(self, vec, i):
        r3, _, _ = T.call_ppc(self.e, FN, [i & MASK, 0, vec & MASK])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cco_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cco_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, vec, i):
        self.lib.tpw_cco_a(s32(i), vec)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cco_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "a").restype, getattr(lib, p + "a").argtypes = None, [C.c_int32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        g = s.g("gOnline")
        vec = s.alloc(16)
        s.words(vec, [case["marker"]])
        preset = 0
        if case["have"]:
            preset = s.alloc(0xE8C)
            s.write(preset, bytes(case["bytes"]))
        s.wr_ptr(g, preset)
        s.call(vec, case["id"])
        p = s.rd_ptr(g)
        return {"log": s.log(), "same": p == preset, "isnull": p == 0,
                "obj": s.read(p, 0x40) if p else b"", "marker": struct.unpack(s.fmt, s.read(vec, 4))[0]}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


IDS = [-6, -5, -4, -3, -2, -1, 0, 1, 5, 100, -100, 0x7FFFFFFF, -0x80000000]


def mk(r, i, have):
    return {"queues": {}, "id": i, "have": have, "marker": r.randrange(1, 1 << 32),
            "bytes": [r.randrange(256) for _ in range(0x40)] + [0] * (0xE8C - 0x40)}


def edges():
    r = random.Random(5212)
    for i in IDS:
        for have in (False, True):
            yield mk(r, i, have)


def rnd(r):
    return mk(r, r.choice(IDS + [r.randrange(-10, 10)]), r.choice([False, True]))


TESTS = [
    FnTest("ChatCallbackFn", ppc, host, edge=edges, rnd=rnd),
]
