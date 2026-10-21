"""Unit v5280, TbIntegerVTag::Format, against libtpw_unit_TbIntegerVTag_Format.dylib (tests/unit_TbIntegerVTag_Format.cpp), program ltms,
src/ltms/TbVTag.cpp:
  TbIntegerVTag::Format 0x1000923c

A case is a tag with a value and a call. swprintf (0x1000f8bc) and the virtual call through __ptr_glue (0x1000f91c, AppendUnicode) are fakes on
both sides recording their arguments (swprintf: size and value; AppendUnicode: count and the characters). Compared: the answer and the log.
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_TbIntegerVTag_Format"
MASK = 0xFFFFFFFF

FN_FORMAT = 0x1000923c
OWN = {FN_FORMAT}
SWPRINTF = 0x1000f8bc
GLUE = 0x1000f91c

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_swprintf(e):
    L = e.cl
    size, value = e.r(4), T.s32(e.r(6))
    L.rec("swprintf", (size, value))
    s = str(value)[:size - 1]
    e.write(e.r(3), b"".join(ord(c).to_bytes(2, "big") for c in s) + b"\0\0")
    e.set_r(3, len(s))


def stub_glue(e):
    L = e.cl
    n = e.r(5)
    raw = e.read(e.r(4), 2 * 32)
    chars = []
    for i in range(0, len(raw), 2):
        w = (raw[i] << 8) | raw[i + 1]
        if w == 0:
            break
        chars.append(w)
    L.rec("AppendUnicode", (n & MASK,))
    for c in chars:
        L.rec("Char", (c,))
    e.set_r(3, 0)


STUBS.internal(SWPRINTF, stub_swprintf)
STUBS.internal(GLUE, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc(e, case):
    e.cl = L = CL.PpcLog({}, {})
    saved = STUBS.activate(e)
    try:
        vt = e.alloc(0x40)
        e.write(vt + 0x18, (0x10001000).to_bytes(4, "big"))
        out = e.alloc(8)
        e.write(out, vt.to_bytes(4, "big"))
        tag = e.alloc(0x14)
        e.write(tag + 0xc, (case["value"] & MASK).to_bytes(4, "big"))
        fmt = e.alloc(12)
        r3, _, _ = T.call_ppc(e, FN_FORMAT, [tag, out, fmt])
        return {"answer": r3 & MASK, "log": L.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    lib.tpw_tivf_format.restype, lib.tpw_tivf_format.argtypes = C.c_uint32, [C.c_int32]
    CL.host_begin(lib, {}, {})
    ans = lib.tpw_tivf_format(T.s32(case["value"]))
    return {"answer": ans & MASK, "log": CL.host_end(lib)}


def edges():
    for v in (0, 1, -1, 9, 10, -9, -10, 99999, 123456789, 0x7FFFFFFF, -0x80000000, -0x7FFFFFFF, 1000000000, -999999999):
        yield {"value": v}


def rnd(r):
    k = r.random()
    if k < 0.3:
        return {"value": r.randrange(-1000, 1000)}
    return {"value": T.s32(r.getrandbits(32) >> r.randrange(0, 32))}


TESTS = [FnTest("TbIntegerVTag::Format", ppc, host, edge=edges, rnd=rnd)]
