"""Unit v5578, SetCoasterButtonState, against libtpw_unit_SetCoasterButtonState.dylib (tests/unit_SetCoasterButtonState.cpp), main app,
src/LayoutSetRotatePylon_DeleteLastTrackBrace_SetCoasterButtonState.cpp:
  SetCoasterButtonState 0x10057064

The callees DisableCurrentPylonEditState and HandleCoasterButtonDown are fakes on both sides recording their events (the latter with its
argument). The world: gCoasterEditDown. Compared: the global and the event log. No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SetCoasterButtonState"
MASK = 0xFFFFFFFF

FN = 0x10057064
SIZES = {FN: 140}
OWN = set(SIZES)
DISABLE = 0x10053b58
BUTTON = 0x10056d7c
ADDR = dict(down=0x102c16d8)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_disable(e):
    e.cl.rec("Disable", ())


def stub_button(e):
    e.cl.rec("ButtonDown", (e.r(3) & MASK,))


STUBS.internal(DISABLE, stub_disable)
STUBS.internal(BUTTON, stub_button)


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

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, state, arg):
        T.call_ppc(self.e, FN, [state & MASK, arg & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_scb_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, state, arg):
        self.lib.tpw_scb_set(state & MASK, arg & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_scb_addr.restype, lib.tpw_scb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_scb_alloc.restype, lib.tpw_scb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_scb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_scb_set.restype, lib.tpw_scb_set.argtypes = None, [C.c_uint32, C.c_uint32]
    lib.bound = True


def run(s, case):
    try:
        s.words(s.g("down"), [case["down"]])
        s.call(*case["args"])
        return {"log": s.log(), "down": s.rd_words(s.g("down"), 1)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(down, state, arg):
    return {"queues": {}, "down": down, "args": [state, arg]}


def rmask(r):
    k = r.random()
    if k < 0.2:
        return 0
    if k < 0.5:
        return 1 << r.randrange(32)
    if k < 0.7:
        return r.randrange(1, 0x400)
    return r.getrandbits(32)


def edges():
    for v in (0, 1, 2, 0x100, 0x1ff, 0x200, 0x80000000, 0xFFFFFFFF, 0x1ff00, 0x300):
        yield mk(0xFFFFFFFF, v, 0)
        yield mk(0xFFFFFFFF, 0, v)
        yield mk(0x12345678, v, v)
    for b in range(32):
        yield mk(0xFFFFFFFF, 1 << b, 1 << b)
    yield mk(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF)


def rnd(r):
    return mk(r.getrandbits(32), rmask(r), rmask(r))


TESTS = [
    FnTest("SetCoasterButtonState", ppc, host, edge=edges, rnd=rnd),
]
