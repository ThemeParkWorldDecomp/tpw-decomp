"""Unit v5668, CTextureCache::TestUploadSpeed 0x100339c8 against libtpw_unit_CTextureCache_TestUploadSpeed.dylib.

The cache holds one page of the size flag the faked ConvertSurfaceSizeToFlag answers; its texture is a fake whose LockSurface and
UnlockSurface (called through __ptr_glue on the PowerPC side) record an event and answer from the case. CProfiler::GetClockCycleCount10h
is faked too. Compared: the answer (float bits), the settings flags, the event log. No fused operations. Not tested: a flag outside 0..5
and a format outside 0..5 (table reads past the end).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_CTextureCache_TestUploadSpeed"
MASK = 0xFFFFFFFF

FN = 0x100339C8
CONVERT, CLOCK, GLUE = 0x10029220, 0x1001FC8C, 0x1004C858
SETTINGS, PROFILER = 0x10165FC8, 0x100B050C
TV = {}


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_convert(e):
    L = e.cl
    L.rec("Convert", (T.s32(e.r(3)),))
    e.set_r(3, L.ret("Convert") & MASK)


def stub_clock(e):
    L = e.cl
    L.rec("Clock", (1 if e.r(3) == PROFILER else 0,))
    e.set_r(3, L.ret("Clock") & MASK)


def stub_glue(e):
    L = e.cl
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))   # the glue saves the caller's TOC pointer
    name = "Lock" if e.r(12) == TV["lock"] else "Unlock"
    L.rec(name, (L.lab(e.r(3)),))
    e.set_r(3, L.ret(name) & MASK)


STUBS.internal(CONVERT, stub_convert)
STUBS.internal(CLOCK, stub_clock)
STUBS.internal(GLUE, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_tus_addr.restype, lib.tpw_tus_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_tus_setup.restype, lib.tpw_tus_setup.argtypes = C.c_uint64, [C.c_int32, C.c_int32]
    lib.tpw_tus_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_tus_call.restype, lib.tpw_tus_call.argtypes = C.c_double, [C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_tus_layout(out)
    assert list(out)[:n] == [0, 4], list(out)[:n]
    lib.bound = True


def fbits(x):
    return canon_f(struct.unpack(">I", struct.pack(">f", x))[0])


def ppc(e, case):
    cache = e.alloc(0x1900)
    link = e.alloc(0x20)
    tex = e.alloc(0x20)
    vt = e.alloc(0x40)
    TV["lock"] = e.alloc(8)
    TV["unlock"] = e.alloc(8)
    flag = case["flag"]
    e.write(cache + flag * 0x400, struct.pack(">I", link))
    e.write(cache + 0x1800 + flag * 4, struct.pack(">I", case["pages"] & MASK))
    e.write(link, struct.pack(">I", tex))
    e.write(tex + 0x10, struct.pack(">I", vt))
    e.write(vt + 0x18, struct.pack(">I", TV["lock"]))
    e.write(vt + 0x1C, struct.pack(">I", TV["unlock"]))
    e.write(SETTINGS, struct.pack(">II", case["flags"], case["fmt"]))
    if case.get("dump"):
        print(e.read(0x10051EB0, 64).hex())
    L = e.cl = CL.PpcLog({tex: 1}, case["queues"])
    saved = STUBS.activate(e)
    try:
        _, _, f1 = T.call_ppc(e, FN, [cache, case["size"] & MASK, case["count"] & MASK])
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"ans": fbits(f1), "flags": struct.unpack(">I", e.read(SETTINGS, 4))[0], "log": L.log}


def host(lib, case):
    bind(lib)
    tex = lib.tpw_tus_setup(case["flag"], case["pages"])
    s = lib.tpw_tus_addr(b"settings")
    C.memmove(s, struct.pack("<II", case["flags"], case["fmt"]), 8)
    CL.host_begin(lib, {1: tex}, case["queues"])
    d = lib.tpw_tus_call(case["size"], case["count"])
    flags = struct.unpack("<I", C.string_at(s, 4))[0]
    return {"ans": fbits(d), "flags": flags, "log": CL.host_end(lib)}


def mk(r, size=None, count=None, pages=None, fail=None, fmt=None):
    flag = r.randrange(6)
    count = r.choice([0, 1, 2, 5, -2]) if count is None else count
    pages = r.choice([-1, 0, 1, 3]) if pages is None else pages
    n = abs(count) + 2
    lock = [1] * n
    unlock = [1] * n
    if fail is None:
        fail = r.choice([None, "lock", "unlock"])
    k = r.randrange(0, 3)
    if count < 0 and fail is None:
        fail = r.choice(["lock", "unlock"])
    if fail == "lock":
        lock = [1] * k + [0]
    elif fail == "unlock":
        unlock = [1] * k + [0]
    return {"flag": flag, "pages": pages, "count": count, "fmt": r.randrange(6) if fmt is None else fmt,
            "size": r.choice([0, 1, 16, 64, 128, 256, 1000]) if size is None else size,
            "flags": r.choice([0, 0x17A, 0xFFFFFFFF, 0x20000000]),
            "queues": {"Convert": [flag], "Lock": lock, "Unlock": unlock,
                       "Clock": [r.randrange(-2**31, 2**31) if r.random() < .3 else r.randrange(0, 1000), r.randrange(0, 100000)]}}


def edges():
    r = random.Random(5)
    c = mk(r, pages=1, count=0, fail="none")
    c["dump"] = True
    yield c
    for count in (0, 1, 3, -1):
        for fail in (None, "lock", "unlock"):
            for pages in (0, 1, -1):
                yield mk(r, count=count, pages=pages, fail=fail)
    c = mk(r, count=2, pages=1, fail="none")
    c["queues"]["Clock"] = [7, 7]
    yield c


def rnd(r):
    return mk(r)


TESTS = [FnTest("CTextureCache::TestUploadSpeed", ppc, host, edge=edges, rnd=rnd)]
