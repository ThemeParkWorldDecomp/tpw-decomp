"""Unit v5015, CGame::GetLocalisedThemeName 0x1014a570, against libtpw_unit_CGame_GetLocalisedThemeName.dylib
(tests/unit_CGame_GetLocalisedThemeName.cpp), main app, src/game/Game.cpp.

A case is a theme string object (one of the four globals' addresses is picked by the TPUniStrEq fake from the case's "theme" queue: 0..3 or
4 for none), a table pointer in gText+0x5a4 and an output string whose vtable slot 0x20 is a marker. The callees outside the unit are fakes on
both sides that record their arguments: TPUniStrEq, the char string's constructor and destructor, TbITable::GetConstString,
UIMain_ConvertMulti and the virtual call (assign: object label, text pointer, 1 when the slot read was the marker). Compared: the event log.
No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CGame_GetLocalisedThemeName"
MASK = 0xFFFFFFFF

FN = 0x1014A570
OWN = {FN: 392}
TPUNISTREQ = 0x1000548C
CONVERT = 0x10180334
CTOR = 0x10238458
DTOR = 0x102382F0
GETSTR = 0x10238B78
PTRGLUE = 0x1023BAA8
MARKER = 0x00ABCDE0

ADDR = dict(fantasy=0x1037F10C, hallow=0x1037F0EC, jungle=0x1037F0CC, space=0x1037F0AC, gtext=0x1037FF18)
ORDER = ["fantasy", "hallow", "jungle", "space"]
HOST_LAYOUT = []

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_eq(e):
    L = e.cl
    L.rec("TPUniStrEq", (L.lab(e.r(3)), L.lab(e.r(4))))
    pick = L.ret("theme")
    e.set_r(3, 1 if 0 <= pick < 4 and (e.r(4) & MASK) == ADDR[ORDER[pick]] else 0)


def stub_convert(e):
    e.cl.rec("Convert", (e.r(3) & MASK,))
    e.set_r(3, (e.r(3) + 0x100) & MASK)


def stub_ctor(e):
    e.cl.rec("ctor", ())


def stub_dtor(e):
    e.cl.rec("dtor", ())


def stub_getstr(e):
    L = e.cl
    L.rec("GetConstString", (L.lab(e.r(3)), e.r(4) & MASK))
    e.set_r(3, (0x1000 + (e.r(4) & MASK) * 16) & MASK)


def stub_ptrglue(e):
    L = e.cl
    L.rec("assign", (L.lab(e.r(3)), e.r(4) & MASK, 1 if (e.r(12) & MASK) == MARKER else 0))


STUBS.internal(TPUNISTREQ, stub_eq)
STUBS.internal(CONVERT, stub_convert)
def glue(fn):
    """The import glue saves the caller's TOC at 0x14(r1) (the caller reloads r2 from there)."""

    def wrapped(e):
        e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
        fn(e)

    return wrapped


STUBS.internal(CTOR, glue(stub_ctor))
STUBS.internal(DTOR, glue(stub_dtor))
STUBS.internal(GETSTR, glue(stub_getstr))
STUBS.internal(PTRGLUE, glue(stub_ptrglue))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.inp = e.alloc(16)
        self.tbl = e.alloc(16)
        self.self_ = e.alloc(0x40)
        self.out = e.alloc(16)
        vt = e.alloc(0x40)
        e.write(vt + 0x20, struct.pack(">I", MARKER))
        e.write(self.out, struct.pack(">I", vt))
        fixed = {self.inp: 1, self.out: 2, self.tbl: 3, ADDR["fantasy"]: 4, ADDR["hallow"]: 5, ADDR["jungle"]: 6, ADDR["space"]: 7}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_table(self):
        self.words(ADDR["gtext"] + 0x5A4, [self.tbl])

    def call(self):
        T.call_ppc(self.e, FN, [self.self_, self.inp, self.out])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.inp = lib.tpw_gltn_alloc(16)
        self.tbl = lib.tpw_gltn_alloc(16)
        self.self_ = lib.tpw_gltn_alloc(0x40)
        self.out = lib.tpw_gltn_makeout()
        fixed = {1: self.inp, 2: self.out, 3: self.tbl}
        for i, n in enumerate(ORDER):
            fixed[4 + i] = lib.tpw_gltn_addr(n.encode())
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_table(self):
        self.lib.tpw_gltn_settable(self.tbl)

    def call(self):
        self.lib.tpw_gltn_call(self.self_, self.inp, self.out)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gltn_addr.restype, lib.tpw_gltn_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gltn_alloc.restype, lib.tpw_gltn_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gltn_makeout.restype, lib.tpw_gltn_makeout.argtypes = C.c_uint64, []
    lib.tpw_gltn_settable.argtypes = [C.c_uint64]
    lib.tpw_gltn_call.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_gltn_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gltn_layout(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.bound = True


def run(s, case):
    try:
        s.set_table()
        s.call()
        return {"log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(theme):
    return {"queues": {"theme": [theme]}}


def edges():
    for t in (0, 1, 2, 3, 4, -1):
        yield mk(t)


def rnd(r):
    return mk(r.choice([0, 1, 2, 3, 4, r.randrange(-3, 8)]))


TESTS = [FnTest("CGame::GetLocalisedThemeName", ppc, host, edge=edges, rnd=rnd)]
