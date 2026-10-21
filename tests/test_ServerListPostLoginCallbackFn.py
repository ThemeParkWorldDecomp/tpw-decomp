"""Unit v5721, the login callback of the server list, against libtpw_unit_ServerListPostLoginCallbackFn.dylib
(tests/unit_ServerListPostLoginCallbackFn.cpp), main app ("SimTheme Park"), src/game/online/FS_OnlineList.cpp:
  ServerListPostLoginCallbackFn 0x1017aa6c

A case is a world and a call. The callees outside the unit (the CFSOnlineSystem constructor, CompleteGetList and LoggedInGetList of the
server list) are fakes on both sides that record their arguments. The constructor fake stores a list value (from the case) at +0xfc.

The world of a case: gOnlineSystem null or an object whose +0xfc holds a list value. Compared: the log, whether the global is the
pre-existing object, and the list value stored. Not tested: operator new is a glue fake on both sides and is not logged.
No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ServerListPostLoginCallbackFn"
MASK = 0xFFFFFFFF

FN = 0x1017AA6C
SIZES = {FN: 140}
OWN = set(SIZES)
CTOR = 0x1016F7D0
COMPLETE = 0x1017464C
LOGGED = 0x1017AB30

ADDR = dict(gOnlineSystem=0x10380AB0)
HOST_LAYOUT = []

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ctor(e):
    L = e.cl
    L.rec("Ctor", ())
    e.write(e.r(3) + 0xFC, struct.pack(">I", L.ret("NewList") & MASK))


def stub_complete(e):
    e.cl.rec("CompleteGetList", (e.r(3), T.s32(e.r(4))))


def stub_logged(e):
    e.cl.rec("LoggedInGetList", (e.r(3),))


STUBS.internal(CTOR, stub_ctor)
STUBS.internal(COMPLETE, stub_complete)
STUBS.internal(LOGGED, stub_logged)


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
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_system(self, sys, lst):
        if sys:
            self.words(sys + 0xFC, [lst])
        self.words(ADDR["gOnlineSystem"], [sys])

    def get_system(self):
        return self.rd_word(ADDR["gOnlineSystem"])

    def get_list(self, sys):
        return self.rd_word(sys + 0xFC) if sys else 0

    def call(self, err, ctx):
        T.call_ppc(self.e, FN, [err & MASK, ctx & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_slp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_system(self, sys, lst):
        if sys:
            self.lib.tpw_slp_set_list(sys, lst & MASK)
        self.lib.tpw_slp_set_system(sys)

    def get_system(self):
        return self.lib.tpw_slp_get_system()

    def get_list(self, sys):
        return self.lib.tpw_slp_get_list(sys) if sys else 0

    def call(self, err, ctx):
        self.lib.tpw_slp_a(T.s32(err), ctx & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_slp_addr.restype, lib.tpw_slp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_slp_alloc.restype, lib.tpw_slp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_slp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_slp_set_system.argtypes = [C.c_uint64]
    lib.tpw_slp_get_system.restype = C.c_uint64
    lib.tpw_slp_set_list.argtypes = [C.c_uint64, C.c_uint32]
    lib.tpw_slp_get_list.restype, lib.tpw_slp_get_list.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_slp_a.argtypes = [C.c_int32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_slp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.bound = True


def run(s, case):
    try:
        sys = s.alloc(0x404) if case["have"] else 0
        s.set_system(sys, case["list"])
        s.call(*case["args"])
        now = s.get_system()
        return {"log": s.log(), "created": now != 0 and now != sys, "kept": now == sys,
                "present": now != 0, "list": s.get_list(now)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def case_(r, have, err):
    return {"have": have, "list": r.randrange(1, 1 << 31) & ~3, "args": [err, r.randrange(0, 1 << 32)],
            "queues": {"NewList": [r.randrange(1, 1 << 31) & ~3]}}


def edges():
    r = random.Random(5721)
    for have in (0, 1):
        for err in (0, 1, -1, 2, 0x7FFFFFFF, 0x80000000):
            yield case_(r, have, T.s32(err))


def rnd(r):
    return case_(r, r.choice([0, 1]), r.choice([0, 1, -1, r.randrange(-1000, 1000)]))


TESTS = [
    FnTest("ServerListPostLoginCallbackFn", ppc, host, edge=edges, rnd=rnd),
]
