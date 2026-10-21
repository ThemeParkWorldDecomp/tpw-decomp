"""Unit v5309, LbTime_GetTime and LbTime_GetDate, against libtpw_unit_LbTime_GetTime.dylib (tests/unit_LbTime_GetTime.cpp), bullfrog,
src/bullfrog/LbTime.cpp:
  LbTime_GetTime 0x100468e4, LbTime_GetDate 0x10046968

A case is a SYSTEMTIME (eight 16-bit words) answered by the fake NS_MacDoze::GetLocalTime (0x1006ba24, same event on both sides) and a
call with a 16-byte output block. Compared: the four output words and the call log. No fused operations. Not tested: nothing skipped
(a NULL output pointer is written unconditionally by the original).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbTime_GetTime"
MASK = 0xFFFFFFFF

FN_TIME, FN_DATE = 0x100468E4, 0x10046968
SIZES = {FN_TIME: 88, FN_DATE: 88}
OWN = set(SIZES)
CALLEE = 0x1006BA24

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_get_local_time(e):
    L = e.cl
    L.rec("GetLocalTime", ())
    a = e.r(3)
    ws = [L.ret("st%d" % i) & 0xFFFF for i in range(8)]
    e.write(a, struct.pack(">8H", *ws))


STUBS.internal(CALLEE, stub_get_local_time)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def call(self, fn):
        out = self.e.alloc(16)
        T.call_ppc(self.e, fn, [out])
        return list(struct.unpack(">4I", self.e.read(out, 16))), self.L.log


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def call(self, fn):
        out = self.lib.tpw_lbt_alloc(16)
        C.memset(out, 0xAA, 16) if False else None
        (self.lib.tpw_lbt_time if fn == FN_TIME else self.lib.tpw_lbt_date)(out)
        return list(struct.unpack("<4I", C.string_at(out, 16))), CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lbt_alloc.restype, lib.tpw_lbt_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lbt_time.argtypes = [C.c_uint64]
    lib.tpw_lbt_date.argtypes = [C.c_uint64]
    lib.tpw_lbt_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lbt_layout(out)
    assert list(out)[:n] == [16, 12], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        words, log = s.call(case["fn"])
        return {"out": words, "log": log}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(fn, ws):
    return {"fn": fn, "queues": {"st%d" % i: [ws[i]] for i in range(8)}}


def edges(fn):
    def gen():
        yield mk(fn, [0] * 8)
        yield mk(fn, [0xFFFF] * 8)
        yield mk(fn, [0x8000] * 8)
        yield mk(fn, [2024, 12, 6, 31, 23, 59, 59, 999])
        for i in range(8):
            ws = [0] * 8
            ws[i] = 0xFFFF
            yield mk(fn, ws)
            ws[i] = 0x8001
            yield mk(fn, ws)
            ws = [i + 1] * 8
            ws[i] = 0x1234
            yield mk(fn, ws)
    return gen


def rnd(fn):
    def gen(r):
        return mk(fn, [r.choice([r.randrange(0, 0x10000), r.randrange(0, 2100), 0xFFFF]) for _ in range(8)])
    return gen


TESTS = [
    FnTest("LbTime_GetTime", ppc, host, edge=edges(FN_TIME), rnd=rnd(FN_TIME)),
    FnTest("LbTime_GetDate", ppc, host, edge=edges(FN_DATE), rnd=rnd(FN_DATE)),
]
