"""Unit v5199, show_person_history_info 0x1014f65c, against libtpw_unit_show_person_history_info.dylib, main app, src/game/keyboard.cpp.

A case is a world and a call: gIntrface.mPointedThing is NULL or a block whose byte at +2 is the kind. CPeep::ReportAllEvents is a
recording fake on both sides. Compared: the answer, the event log, the pointer and the kind byte. No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_show_person_history_info"
MASK = 0xFFFFFFFF
FN = 0x1014F65C
REPORT = 0x10115C3C
ADDR = dict(gIntrface=0x102DC1AC)
HOST_LAYOUT = [0]

STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_report(e):
    e.cl.rec("ReportAllEvents", (e.cl.lab(e.r(3)),))


STUBS.internal(REPORT, stub_report)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, {})
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

    def call(self):
        r3, _, _ = T.call_ppc(self.e, FN, [])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_shpi_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_shpi_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        return self.lib.tpw_shpi_run() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_shpi_"
    lib.tpw_shpi_addr.restype, lib.tpw_shpi_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_shpi_alloc.restype, lib.tpw_shpi_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_shpi_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_shpi_run.restype, lib.tpw_shpi_run.argtypes = C.c_uint32, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_shpi_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        g = s.g("gIntrface")
        if case["kind"] is None:
            s.write(g, struct.pack(s.fmt, 0))
        else:
            a = s.alloc(16)
            s.write(a + 2, bytes([case["kind"]]))
            s.write(g, struct.pack(s.fmt.replace("I", "Q") if s.fmt == "<I" else s.fmt, a))
        ans = s.call()
        return {"answer": ans, "log": s.log(), "null": case["kind"] is None}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def edges():
    yield {"kind": None}
    for k in range(256):
        yield {"kind": k}


def rnd(r):
    return {"kind": r.choice([None, r.randrange(256), r.choice([1, 4, 5, 6, 7, 8, 0x12])])}


TESTS = [
    FnTest("show_person_history_info", ppc, host, edge=edges, rnd=rnd),
]
