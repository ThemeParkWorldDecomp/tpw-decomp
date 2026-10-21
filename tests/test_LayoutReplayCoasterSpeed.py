"""Unit v5302, LayoutReplayCoasterSpeed against libtpw_unit_LayoutReplayCoasterSpeed.dylib (tests/unit_LayoutReplayCoasterSpeed.cpp), main app
("SimTheme Park"), src/game/rides/CoasterEdit.cpp:
  LayoutReplayCoasterSpeed 0x10059120

A case is a world and a call. The world is built twice by one piece of Python code. CoasterSetSpeed is a fake on both sides that records its
two arguments.

The world of a case: gObjectTable (256 pointers; 4 bytes on the Mac, 8 on the host) with objects at some slots, each starting with a flag
word. What is compared: the event log and the first word of every object. No fused operations. Not tested: an index of 0x100 or more (the
original reads past the table without a check).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LayoutReplayCoasterSpeed"
MASK = 0xFFFFFFFF

FN = 0x10059120
SIZES = {FN: 80}
OWN = set(SIZES)
CALLEE = 0x10049630
ADDR = dict(table=0x102b6f04)
HOST_LAYOUT = [0x100 * 8]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_set_speed(e):
    e.cl.rec("CoasterSetSpeed", (e.r(3) & MASK, e.r(4) & MASK))


STUBS.internal(CALLEE, stub_set_speed)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    fmt = ">I"
    psize = 4

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

    def call(self, i, sp):
        T.call_ppc(self.e, FN, [i & MASK, sp & MASK])


class Host:
    fmt = "<I"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lcs_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_lcs_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, i, sp):
        self.lib.tpw_lcs_run(i & MASK, sp & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lcs_addr.restype, lib.tpw_lcs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lcs_alloc.restype, lib.tpw_lcs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lcs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lcs_run.restype, lib.tpw_lcs_run.argtypes = None, [C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 8)()
    n = lib.tpw_lcs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        t = s.g("table")
        ps = s.psize
        pf = ">" if s.fmt[0] == ">" else "<"
        pc = "I" if ps == 4 else "Q"
        objs = {}
        s.write(t, b"\0" * (0x100 * ps))
        for idx, flags in case["objs"].items():
            a = s.alloc(8)
            s.write(a, struct.pack(s.fmt, flags & MASK))
            s.write(t + idx * ps, struct.pack(pf + pc, a))
            objs[idx] = a
        s.call(*case["args"])
        flags = {idx: struct.unpack(s.fmt, s.read(a, 4))[0] for idx, a in objs.items()}
        return {"log": s.log(), "flags": sorted(flags.items())}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, idx, speed, present=None):
    objs = {}
    for k in range(r.randrange(0, 6)):
        objs[r.randrange(0x100)] = r.choice([0, 0x8000, MASK, r.getrandbits(32)])
    if present is True:
        objs[idx] = r.choice([0, 0x8000, MASK, 0x7FFF, r.getrandbits(32)])
    elif present is False:
        objs.pop(idx, None)
    return {"queues": {}, "args": [idx, speed], "objs": objs}


def edges():
    r = random.Random(5302)
    for idx in (0, 1, 0x7F, 0xFF):
        for present in (True, False):
            for speed in (0, 1, MASK, 0x80000000):
                yield mk(r, idx, speed, present)


def rnd(r):
    return mk(r, r.randrange(0x100), r.getrandbits(32), r.choice([True, False, None]))


TESTS = [
    FnTest("LayoutReplayCoasterSpeed", ppc, host, edge=edges, rnd=rnd),
]
