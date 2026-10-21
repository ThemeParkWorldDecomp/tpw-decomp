"""Unit v5377, the finance panel static initialiser, against libtpw_unit_sinit_ui_FinancePanel_cpp.dylib
(tests/unit_sinit_ui_FinancePanel_cpp.cpp), main app, src/game/ui/ui_FinancePanel.cpp:
  __sinit_ui_FinancePanel_cpp 0x101a0dbc

A case is a world and a call. The world is built by ONE piece of Python code in both memories. LbPalette_FindColour and the CUIGrapher
constructor are fakes on both sides that record their arguments; FindColour answers from a queue.

The world of a case: the 40 colour bytes pre-filled randomly. Compared: the 40 bytes, the grapher (an opaque object, not touched), the log.
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_ui_FinancePanel_cpp"
MASK = 0xFFFFFFFF

FN = 0x101A0DBC
SIZES = {FN: 476}
OWN = set(SIZES)

CTOR = 0x101FD6E8
ADDR = dict(pal=0x103BCD28, colours=0x103948D8, grapher=0x10394894)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_find(e):
    L = e.cl
    L.rec("FindColour", (L.lab(e.r(3)), e.r(4) & 0xFF, e.r(5) & 0xFF, e.r(6) & 0xFF))
    e.set_r(3, L.ret("FindColour") & 0xFF)


def stub_ctor(e):
    L = e.cl
    L.rec("CUIGrapher", (L.lab(e.r(3)),))


STUBS.imp("LbPalette_FindColour__FPC9TbPaletteUcUcUc", stub_find)
STUBS.internal(CTOR, stub_ctor)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({e.u32(0x10260D44): 1, ADDR["pal"]: 1,ADDR["grapher"]: 2}, case["queues"])
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

    def call(self):
        T.call_ppc(self.e, FN, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("pal"), 2: self.g("grapher")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fp_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_fp_run()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_fp_addr.restype, lib.tpw_fp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fp_run.restype = None
    out = (C.c_int64 * 32)()
    n = lib.tpw_fp_layout(out)
    assert list(out)[:n] == [40], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.write(s.g("colours"), bytes(case["fill"]))
        s.call()
        return {"colours": s.read(s.g("colours"), 40), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, answers):
    return {"queues": {"FindColour": list(answers)}, "fill": [r.randrange(256) for _ in range(40)]}


def edges():
    r = random.Random(77)
    yield mk(r, [0] * 8)
    yield mk(r, [0xFF] * 8)
    yield mk(r, [1, 2, 3, 4, 5, 6, 7, 8])
    yield mk(r, [0x80, 0x7F, 0xFE, 0x01, 0x10, 0x20, 0x40, 0x99])


def rnd(r):
    return mk(r, [r.randrange(256) for _ in range(8)])


TESTS = [
    FnTest("__sinit_ui_FinancePanel_cpp", ppc, host, edge=edges, rnd=rnd),
]
