"""Unit v5023, COnlineControl's chat commands, against libtpw_unit_COnlineControl_Locate.dylib (tests/unit_COnlineControl_Locate.cpp), main app
("SimTheme Park"), src/COnlineControl_COnlineControl_COnlineControl_Locate.cpp:
  Locate 0x102187f4, Page 0x1021864c, Goto 0x1021899c, BlackMark 0x102184a0, BuddyOn 0x10218c44

A case is a call on an object (a zeroed 0xe8c block) with a name string block. Every callee outside the unit (the string constructors and
destructors, Argument, the argument vector, the buddy map's find, DoCommand) is a fake on both sides that records its arguments and writes
marker words into the objects it builds, so the order, the objects and the command numbers all show in the log. The find answers end() or
not from the case's queue. Compared: the event log. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineControl_Locate"
MASK = 0xFFFFFFFF

FNS = {"Locate": 0x102187f4, "Page": 0x1021864c, "Goto": 0x1021899c, "BlackMark": 0x102184a0, "BuddyOn": 0x10218c44}
SIZES = {0x102187f4: 312, 0x1021864c: 312, 0x1021899c: 312, 0x102184a0: 312, 0x10218c44: 388}
OWN = set(SIZES)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def words(e, a, n):
    return tuple(e.u32(a + 4 * i) for i in range(n))


def put(e, a, v):
    e.write(a, (v & MASK).to_bytes(4, "big"))


def s16(v):
    return T.s16(v & 0xFFFF)


def st_str_ctor(e):
    e.cl.rec("StrCtor", ())
    put(e, e.r(3), 0xB6)


def st_wstr_ctor(e):
    e.cl.rec("WStrCtor", ())
    put(e, e.r(3), 0xA5)


def st_str_dtor(e):
    e.cl.rec("StrDtor", (s16(e.r(4)), e.u32(e.r(3))))


def st_wstr_dtor(e):
    e.cl.rec("WStrDtor", (s16(e.r(4)), e.u32(e.r(3))))


def st_wcopy(e):
    e.cl.rec("WCopy", (e.cl.lab(e.r(4)), e.r(5) & MASK, e.r(6) & MASK))
    put(e, e.r(3), 0xE9)


def st_ncopy(e):
    e.cl.rec("NCopy", (e.cl.lab(e.r(4)), e.r(5) & MASK, e.r(6) & MASK))
    put(e, e.r(3), 0xF1)


def st_ndtor(e):
    e.cl.rec("NDtor", (s16(e.r(4)), e.u32(e.r(3))))


def st_find(e):
    L = e.cl
    tree, key = e.r(4), e.r(5)
    L.rec("Find", (L.lab(tree), e.u32(key)))
    answer = L.ret("Find")
    put(e, e.r(3), tree + (4 if answer else 8))


def st_argctor(e):
    e.cl.rec("ArgCtor", (e.u32(e.r(4)),))
    s = e.r(3)
    put(e, s, 0xC7)
    put(e, s + 4, 0xD8)
    put(e, s + 8, 0x77)


def st_push(e):
    a = e.r(4)
    e.cl.rec("PushBack", (e.u32(e.r(3)),) + words(e, a, 3))


def st_teardown(e):
    e.cl.rec("TearDown", words(e, e.r(3), 3))


def st_docommand(e):
    e.cl.rec("DoCommand", (e.cl.lab(e.r(3)),) + words(e, e.r(4), 3))


STUBS.internal(0x1007363c, st_str_ctor)
STUBS.internal(0x101e0b84, st_wstr_ctor)
STUBS.internal(0x1000221c, st_str_dtor)
STUBS.internal(0x101468f8, st_wstr_dtor)
STUBS.internal(0x101f7424, st_wcopy)
STUBS.internal(0x1022213c, st_ncopy)
STUBS.internal(0x10222020, st_ndtor)
STUBS.internal(0x1021f8d8, st_find)
STUBS.internal(0x102011a8, st_argctor)
STUBS.internal(0x10201478, st_push)
STUBS.internal(0x10201310, st_teardown)
STUBS.internal(0x10219bec, st_docommand)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_oc_alloc.restype, lib.tpw_oc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    for n in FNS:
        f = getattr(lib, "tpw_oc_" + n)
        f.restype, f.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.bound = True


def ppc_fn(name):
    def ppc(e, case):
        this = e.alloc(0xE8C)
        nm = e.alloc(16)
        L = e.cl = CL.PpcLog({this: 1, nm: 2, this + 0xE34: 3}, case["queues"])
        saved = STUBS.activate(e)
        try:
            T.call_ppc(e, FNS[name], [this, nm])
        finally:
            CL.Stubs.deactivate(e, saved)
        return {"log": L.log}
    return ppc


def host_fn(name):
    def host(lib, case):
        bind(lib)
        this = lib.tpw_oc_alloc(0xE8C)
        nm = lib.tpw_oc_alloc(16)
        CL.host_begin(lib, {1: this, 2: nm, 3: this + 0xE34}, case["queues"])
        getattr(lib, "tpw_oc_" + name)(this, nm)
        return {"log": CL.host_end(lib)}
    return host


def case(find):
    return {"queues": {"Find": [find]}}


def edges():
    yield case(0)
    yield case(1)


def rnd(r):
    return case(r.choice([0, 1]))


TESTS = [FnTest("COnlineControl::" + n, ppc_fn(n), host_fn(n), edge=edges, rnd=rnd) for n in FNS]
