"""Unit v5374, the outbox panel's column click, against libtpw_unit_ColumnClick.dylib (tests/unit_ColumnClick.cpp), main app
("SimTheme Park"), src/game/ui/ui_onlineOutbox.cpp:
  ColumnClick 0x101e3e98

A case is a world and a call. The world is built twice by one piece of Python code. InterfaceWindow::GetWindow, InterfaceListControl2::
GetData, ChangeItem and SetData are fakes on both sides that record their arguments (objects as labels, the item's columns 2 and 3).

The world of a case: gOutboxWindow, a list object whose current entry word is set, and the data word GetData answers.
What is compared: the event log (which holds the stored data word and the columns), the global, the current entry.

No fused operations. Not tested: nothing.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ColumnClick"
MASK = 0xFFFFFFFF

FN = 0x101E3E98
SIZES = {FN: 328}
OWN = set(SIZES)

GETWINDOW, GETDATA, CHANGEITEM, SETDATA = 0x101DBE40, 0x101D41F0, 0x101D3660, 0x101D3FCC
ADDR = dict(win=0x10395E3C)
CUR = {}
HOSTOFF = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_getwindow(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, CUR["list"] & MASK)


def stub_getdata(e):
    L = e.cl
    L.rec("GetData", (L.lab(e.r(3)), s32(e.r(4))))
    e.write(e.r(5), struct.pack(">I", L.ret("GetData") & MASK))
    e.set_r(3, 1)


def stub_changeitem(e):
    L = e.cl
    item = e.r(5)
    L.rec("ChangeItem", (L.lab(e.r(3)), s32(e.r(4)), s32(e.u32(item + 8)), s32(e.u32(item + 12)), s32(e.r(6)), s32(e.r(7)), s32(e.r(8))))


def stub_setdata(e):
    L = e.cl
    L.rec("SetData", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


STUBS.internal(GETWINDOW, stub_getwindow)
STUBS.internal(GETDATA, stub_getdata)
STUBS.internal(CHANGEITEM, stub_changeitem)
STUBS.internal(SETDATA, stub_setdata)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.win = e.alloc(16)
        self.lst = e.alloc(0x200)
        CUR["list"] = self.lst
        self.L = e.cl = CL.PpcLog({self.win: 1, self.lst: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def build(self, case):
        self.e.write(ADDR["win"], struct.pack(">I", self.win))
        self.e.write(self.lst + 0x14C, struct.pack(">I", case["entry"] & MASK))

    def call(self, column):
        T.call_ppc(self.e, FN, [column & MASK])

    def observe(self):
        return {"log": self.L.log, "entry": struct.unpack(">I", self.e.read(self.lst + 0x14C, 4))[0],
                "win": struct.unpack(">I", self.e.read(ADDR["win"], 4))[0] == self.win}


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.win = lib.tpw_cc_alloc(16)
        self.lst = lib.tpw_cc_alloc(HOSTOFF["size"] + 16)
        lib.tpw_cc_setlist(self.lst)
        CL.host_begin(lib, {1: self.win, 2: self.lst}, case["queues"])

    def done(self):
        pass

    def build(self, case):
        C.memmove(self.lib.tpw_cc_addr(b"win"), struct.pack("<Q", self.win), 8)
        C.memmove(self.lst + HOSTOFF["entry"], struct.pack("<I", case["entry"] & MASK), 4)

    def call(self, column):
        self.lib.tpw_cc_click(s32(column))

    def observe(self):
        a = self.lib.tpw_cc_addr(b"win")
        return {"log": CL.host_end(self.lib), "entry": struct.unpack("<I", C.string_at(self.lst + HOSTOFF["entry"], 4))[0],
                "win": struct.unpack("<Q", C.string_at(a, 8))[0] == self.win}


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cc_addr.restype, lib.tpw_cc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cc_alloc.restype, lib.tpw_cc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cc_setlist.argtypes = [C.c_uint64]
    lib.tpw_cc_click.argtypes = [C.c_int32]
    lib.tpw_cc_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cc_layout(out)
    assert n == 2
    HOSTOFF["entry"], HOSTOFF["size"] = out[0], out[1]
    lib.bound = True


def run(s, case):
    try:
        s.build(case)
        s.call(case["column"])
        return s.observe()
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(column, data, entry):
    return {"column": column, "entry": entry, "queues": {"GetData": [data & MASK]}}


def edges():
    for column in (-5, 0, 1, 2, 3, 4, 100):
        for hi in (0, 1, 2, 3):
            for lo in (0, 0x123456):
                for entry in (0, 5, -1):
                    yield mk(column, (hi << 24) | lo, entry)


def rnd(r):
    return mk(r.choice([-1, 0, 1, 2, 2, 3, 7]), r.getrandbits(32), r.choice([0, 1, 9, -1, r.getrandbits(32)]))


TESTS = [FnTest("ColumnClick", ppc, host, edge=edges, rnd=rnd)]
