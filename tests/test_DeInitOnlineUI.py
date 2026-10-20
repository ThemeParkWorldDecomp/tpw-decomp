"""Unit v5769, DeInitOnlineUI, against libtpw_unit_DeInitOnlineUI.dylib (tests/unit_DeInitOnlineUI.cpp), main app
("SimTheme Park"), src/UI_pump_DeInitOnlineUI.cpp:
  DeInitOnlineUI 0x10186620

A case is a world and a call. The world is built in emulator memory (Mac addresses, big-endian) for the original and in the host
library for the translation. The callees RemoveOnlinePanel, UI_SendMessage, UI_DisableHelpText and CMapWho::Unlink are fakes on
both sides that record their arguments.

The world of a case: gFirstPersonWindow (null or an object), gei_MapWho (null or an object). On the PowerPC side the light object
at 0x1038cc78 holds a word that points at a word holding the link pointer (the translation's static_cast to the virtual base
gives the same object on the host; both are labelled). What is compared: the event log and the two globals.

No fused operations. Not tested: the null branch of the light (its address is never null).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DeInitOnlineUI"
MASK = 0xFFFFFFFF

FN = 0x10186620
SIZES = {FN: 124}
OWN = set(SIZES)

REMOVE_PANEL = 0x101E92C4
SEND_MESSAGE = 0x101C9518
DISABLE_HELP = 0x101CAA4C

ADDR = dict(gFirstPersonWindow=0x103949E0, gei_MapWho=0x103BC908, light=0x1038CC78)
HOST_LAYOUT = []

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_remove(e):
    e.cl.rec("RemoveOnlinePanel", ())


def stub_send(e):
    L = e.cl
    L.rec("UI_SendMessage", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))))
    e.set_r(3, 0)


def stub_disable(e):
    e.cl.rec("UI_DisableHelpText", ())


def stub_unlink(e):
    L = e.cl
    L.rec("Unlink", (L.lab(e.r(3)), L.lab(e.r(4)), T.s32(e.r(5))))
    e.set_r(3, 0)


STUBS.internal(REMOVE_PANEL, stub_remove)
STUBS.internal(SEND_MESSAGE, stub_send)
STUBS.internal(DISABLE_HELP, stub_disable)
STUBS.imp("Unlink__7CMapWhoFP12CMapLinkBasei", stub_unlink)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def put_ptr(self, name, value):
        self.words(self.g(name), [value])

    def get_ptr(self, name):
        return self.rd_word(self.g(name))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.win = e.alloc(16) if case["win"] else 0
        self.mw = e.alloc(16) if case["mw"] else 0
        self.vb = e.alloc(16)
        self.ptr = e.alloc(16)
        e.write(self.ptr, struct.pack(">I", self.vb))
        e.write(ADDR["light"], struct.pack(">I", self.ptr))
        fixed = {self.vb: 1}
        if self.win:
            fixed[self.win] = 2
        if self.mw:
            fixed[self.mw] = 3
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "gei_MapWho":      # a data import: the loader patched the TOC slot to point at the real variable
            return self.e.u32(0x102607E4)
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def run(self):
        T.call_ppc(self.e, FN, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.win = lib.tpw_dio_alloc(16) if case["win"] else 0
        self.mw = lib.tpw_dio_alloc(16) if case["mw"] else 0
        base = C.c_uint64.from_address(self.g("lightbase")).value
        fixed = {1: base}
        if self.win:
            fixed[2] = self.win
        if self.mw:
            fixed[3] = self.mw
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_dio_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def put_ptr(self, name, value):          # host pointers are 64 bits
        C.c_uint64.from_address(self.g(name)).value = value

    def get_ptr(self, name):
        return C.c_uint64.from_address(self.g(name)).value

    def run(self):
        self.lib.tpw_dio_run()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_dio_addr.restype, lib.tpw_dio_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_dio_alloc.restype, lib.tpw_dio_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_dio_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_dio_run.restype, lib.tpw_dio_run.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_dio_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.put_ptr("gFirstPersonWindow", s.win)
        s.put_ptr("gei_MapWho", s.mw)
        s.run()
        return {"log": s.log(), "win": s.get_ptr("gFirstPersonWindow") == s.win, "mw": s.get_ptr("gei_MapWho") == s.mw}
    finally:
        s.done()


def ppc(e, case):
    r = run(Ppc(e, case), case)
    return r


def host(lib, case):
    return run(Host(lib, case), case)


def mk(win, mw):
    return {"win": win, "mw": mw, "queues": {}}


def edges():
    for w in (0, 1):
        for m in (0, 1):
            yield mk(w, m)


def rnd(r):
    return mk(r.choice([0, 1]), r.choice([0, 1]))


TESTS = [
    FnTest("DeInitOnlineUI", ppc, host, edge=edges, rnd=rnd),
]
