"""UI_BankruptAfterLoad 0x1018710c against libtpw_unit_UI_BankruptAfterLoad.dylib (tests/unit_UI_BankruptAfterLoad.cpp),
src/ui_main_panel_UI_Congratulations_UI_BankruptAfterLoad.cpp.

Every callee is a fake on both sides recording its arguments. The world: gBumholeWindow and a built window (labels 1 and 2), the
queue answers of GameWindowToggle_Hidden and the rectangle GetPosition writes (right, bottom). Compared: the log and gBankruptWindow
(starts non-zero, must be cleared). No fused operations. Not tested: nothing omitted.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UI_BankruptAfterLoad"
MASK = 0xFFFFFFFF

FN = 0x1018710C
OWN = {FN}

ADDR = dict(bankrupt=0x1038CC58, bumhole=0x103949DC, script=0x102A7756, anim=0x102A7914, msg=0x10267690)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def st_hidden(e):
    e.cl.rec("Hidden", ())
    e.set_r(3, e.cl.ret("Hidden") & MASK)


def st_toggle(e):
    e.cl.rec("ToggleGameWindow", ())


def st_build(e):
    L = e.cl
    L.rec("Build", (L.lab(e.r(3)), 1 if e.r(4) == ADDR["script"] else 0, 1 if e.r(5) == ADDR["msg"] else 0))
    e.set_r(3, e.built)


def st_ctor(e):
    e.cl.rec("CoolRect", ())


def st_dtor(e):
    e.cl.rec("~CoolRect", ())


def st_getpos(e):
    L = e.cl
    L.rec("GetPosition", (L.lab(e.r(3)), s32(e.r(5))))
    rect = e.r(4)
    e.write(rect + 8, struct.pack(">hh", s32(L.ret("right")) if False else _s16(L.ret("right")), _s16(L.ret("bottom"))))


def _s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def st_pivot(e):
    L = e.cl
    L.rec("setPivot", (L.lab(e.r(3)), _s16(e.r(4)), _s16(e.r(5))))


def st_anim(e):
    L = e.cl
    L.rec("startAnim", (L.lab(e.r(3)), 1 if e.r(4) == ADDR["anim"] else 0, s32(e.r(5))))


def st_kb(e):
    L = e.cl
    L.rec("CaptureKb", (L.lab(e.r(3)), s32(e.r(4))))


STUBS.internal(0x101A8C1C, st_hidden)
STUBS.internal(0x101A8AC0, st_toggle)
STUBS.internal(0x101DEB78, st_build)
STUBS.internal(0x101C7448, st_ctor)
STUBS.internal(0x101C74E0, st_dtor)
STUBS.internal(0x101DB884, st_getpos)
STUBS.internal(0x101DDD34, st_pivot)
STUBS.internal(0x101DD08C, st_anim)
STUBS.internal(0x101DCCEC, st_kb)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.bum = e.alloc(64)
        self.built = e.alloc(64)
        e.built = self.built
        self.L = e.cl = CL.PpcLog({self.bum: 1, self.built: 2}, case["queues"])
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
        self.bum = lib.tpw_ba_alloc(64)
        self.built = lib.tpw_ba_alloc(64)
        CL.host_begin(lib, {1: self.bum, 2: self.built}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ba_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_ba_run()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ba_addr.restype, lib.tpw_ba_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ba_alloc.restype, lib.tpw_ba_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ba_run.restype, lib.tpw_ba_run.argtypes = None, []
    lib.bound = True


def run(s, case):
    try:
        s.words(s.g("bankrupt"), [case["stale"]])
        if isinstance(s, Host):      # host pointers are 64 bits (the arena is not 32-bit addressable)
            s.write(s.g("bumhole"), struct.pack("<Q", s.bum))
            s.write(s.g("built"), struct.pack("<Q", s.built))
        else:
            s.words(s.g("bumhole"), [s.bum])
        s.call()
        return {"log": s.log(), "bankrupt": s.rd_word(s.g("bankrupt"))}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, hidden=None, right=None, bottom=None):
    return {
        "stale": r.choice([0, 0x1234, 0xFFFFFFF0]),
        "queues": {
            "Hidden": [r.choice([0, 1, 5, -1]) if hidden is None else hidden],
            "right": [r.choice([0, 1, -1, 2, 3, 0x7FFF, -0x8000, r.randrange(-3000, 3000)]) if right is None else right],
            "bottom": [r.choice([0, 1, -1, 2, 3, 0x7FFF, -0x8000, r.randrange(-3000, 3000)]) if bottom is None else bottom],
        },
    }


def edges():
    r = random.Random(77)
    for h in (0, 1, -1):
        for rb in ((0, 0), (1, -1), (-1, 1), (0x7FFF, -0x8000), (-0x8000, 0x7FFF), (5, 7)):
            yield mk(r, h, *rb)


def rnd(r):
    return mk(r)


TESTS = [FnTest("UI_BankruptAfterLoad", ppc, host, edge=edges, rnd=rnd)]
