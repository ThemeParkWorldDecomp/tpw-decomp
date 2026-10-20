"""Unit v5648: SamsUtilities::USwapBlockStruct<FLYBOY> 0x10079740, against libtpw_unit_SamsUtilities_USwapBlockStruct.dylib, main app,
src/FLY_Load_SamsUtilities_USwapBlockStruct.cpp.

A case is a count and that many 0xe4-byte records of random bytes. The PowerPC side runs the original with its real swap callees
(no fakes needed: nothing outside the unit has effects). What is compared: every byte of the records after the call, plus a guard
word after the last record (must stay untouched). No fused operations. Not tested: negative counts (the original would loop ~2^32 times).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SamsUtilities_USwapBlockStruct"
FN = 0x10079740
REC = 0xE4
GUARD = b"\xde\xad\xbe\xef"


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_swapfly_run.restype = None
    lib.tpw_swapfly_run.argtypes = [C.c_uint64, C.c_int32]
    lib.bound = True


STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_swap_block(e):
    """_USwapBlock32(unsigned long*, long): reverses the bytes of `count` words (the platform import the Mac swap calls)."""
    a, n = e.r(3), T.s32(e.r(4))
    for i in range(max(n, 0)):
        e.write(a + 4 * i, e.read(a + 4 * i, 4)[::-1])


STUBS.imp("_USwapBlock32__13SamsUtilitiesFPUll", stub_swap_block)


def ppc(e, case):
    data = case["data"] + GUARD
    a = e.alloc(len(data))
    e.write(a, data)
    e.cl = CL.PpcLog({}, {})
    saved = STUBS.activate(e)
    try:
        T.call_ppc(e, FN, [a, case["count"]])
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"bytes": e.read(a, len(data))}


def host(lib, case):
    bind(lib)
    data = case["data"] + GUARD
    buf = C.create_string_buffer(data, len(data))
    lib.tpw_swapfly_run(C.addressof(buf), case["count"])
    return {"bytes": bytes(buf.raw)}


def mk(r, count):
    return {"count": count, "data": bytes(r.randrange(256) for _ in range(count * REC))}


def edges():
    r = random.Random(648)
    for n in (0, 1, 2, 3):
        yield mk(r, n)


def rnd(r):
    return mk(r, r.randrange(0, 5))


TESTS = [FnTest("SamsUtilities::USwapBlockStruct<FLYBOY>", ppc, host, edge=edges, rnd=rnd)]
