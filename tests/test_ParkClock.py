"""Differential tests for unit w125, the park clock's leaves of bullfrog (src/TbTimeStamp_ops.cpp). See difftest.py and tpwtest.py.

TbTimeStamp::operator+, operator-, operator> and GetTime. A stamp or span is 8 bytes: big-endian, high word first on the PowerPC, a native int64_t on
the host. The three operators are pure; operator> is an UNSIGNED 64-bit compare and answers 0 or 1 in the low byte of r3. GetTime hands the stamp to
NS_MacDoze::FileTimeToSystemTime (a recording fake on both sides: the two FILETIME words it is given, the answer and the eight SYSTEMTIME halfwords
come from queues) and writes the fields to the non-NULL outputs only when that answers nonzero. Each case compares the answer, every output cell (and
that NULL outputs are skipped), the stamp and the call log.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_ParkClock"

M64 = (1 << 64) - 1
MIN64, MAX64 = -(1 << 63), (1 << 63) - 1
VP, I32, I64 = C.c_void_p, C.c_int32, C.c_int64
P64 = C.POINTER(I64)
FT_NAME = "FileTimeToSystemTime__10NS_MacDozeFPCQ210NS_MacDoze11tagFILETIMEPQ210NS_MacDoze11_SYSTEMTIME"


def s64(v):
    v &= M64
    return v - (1 << 64) if v & (1 << 63) else v


EDGES64 = [0, 1, -1, 2, -2, 9999, 10000, -10000, 0x7FFFFFFF, 0x80000000, -0x80000000, 0xFFFFFFFF, 0x100000000, -0x100000000, MAX64, MIN64,
           MAX64 - 1, MIN64 + 1, 0x7FFFFFFF00000000, 0x80000000FFFFFFFF, 0x0000000100000001, 0xFFFFFFFF00000000, 864000000000, -864000000000,
           0x01B21DD213814000, 0x00000000FFFFFFFF, 0x7FFFFFFFFFFFFF00, 0x8000000000000100]


def r64(r):
    k = r.random()
    if k < 0.25:
        return s64(r.choice(EDGES64))
    if k < 0.6:
        return s64(r.getrandbits(64))
    if k < 0.8:
        return r.randint(-(1 << 40), 1 << 40)
    return r.randint(-100000, 100000)


def pair(r):
    a = r64(r)
    b = r64(r) if r.random() < 0.75 else s64(r.choice([a, a + 1, a - 1, -a, a ^ (1 << 63), a ^ (1 << 32), a ^ 1]))
    return dict(a=a, b=b)


def pair_edges():
    vals = [0, 1, -1, 0xFFFFFFFF, 0x100000000, -0x100000000, MAX64, MIN64, 0x80000000, 0x7FFFFFFF, 0x80000000FFFFFFFF, 0x7FFFFFFF00000000]
    return [dict(a=s64(a), b=s64(b)) for a in vals for b in vals]


def be_val(e, v):
    return e.alloc(8, struct.pack(">Q", v & M64))


def be_get(e, a):
    return s64(int.from_bytes(e.read(a, 8), "big"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Base(FnTest):
    def run_case(self, runner, case):
        runner.emu.reset_scratch()
        try:
            p = self.ppc(runner.emu, case)
        except ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = self.host(runner.lib, case)
        difference = CL.first_difference(p.pop("log", []), h.pop("log", []))
        errors = T.same_values(p, h, "", False)
        if difference:
            errors.append(difference)
        return errors


# ----------------------------------------------------------------- the three pure operators


def value_op(mangled, sym):
    def ppc(e, c):
        a, b, out = be_val(e, c["a"]), be_val(e, c["b"]), e.alloc(8, b"\xA5" * 8)
        T.call_ppc(e, e.func(mangled), [T.I(out), T.I(a), T.I(b)])
        return dict(out=be_get(e, out), a=be_get(e, a), b=be_get(e, b))

    def host(lib, c):
        a, b, out = I64(c["a"]), I64(c["b"]), I64(0x5A5A)
        f = lib[sym]
        f.restype, f.argtypes = None, [P64, P64, P64]
        f(C.byref(a), C.byref(b), C.byref(out))
        return dict(out=out.value, a=a.value, b=b.value)
    return ppc, host


def gt_ppc(e, c):
    a, b = be_val(e, c["a"]), be_val(e, c["b"])
    r3, _, _ = T.call_ppc(e, e.func("__gt__11TbTimeStampCFRC11TbTimeStamp"), [T.I(a), T.I(b)])
    return dict(ret=r3 & 0xFF, a=be_get(e, a), b=be_get(e, b))


def gt_host(lib, c):
    a, b = I64(c["a"]), I64(c["b"])
    f = lib["tpw_pc_gt"]
    f.restype, f.argtypes = I32, [P64, P64]
    return dict(ret=f(C.byref(a), C.byref(b)), a=a.value, b=b.value)


# ----------------------------------------------------------------- GetTime

# the fake answers the SYSTEMTIME structure's order: year, month, day of week, day, hour, minute, second, millisecond
HALVES = [0, 1, 7, 12, 31, 59, 99, 999, 1904, 2000, 2026, 0x7FFF, 0x8000, 0xFFFF, 0x1234]


def time_case(r, **kw):
    c = dict(stamp=r64(r), mask=r.getrandbits(8) if r.random() < 0.35 else 255,
             garbage=[T.s32(r.getrandbits(32)) for _ in range(8)],
             ftres=r.choice([1, 1, 1, 0, 0, -1, 2, 0x100, 0x7FFFFFFF, -0x80000000, 0x100000000, 0x100000001]),
             ftw=[r.choice(HALVES) if r.random() < 0.5 else r.getrandbits(16) for _ in range(8)])
    c.update(kw)
    return c


def time_edges():
    out = []
    for stamp in EDGES64:
        for res in (1, 0, -1):
            out.append(time_case(random.Random(len(out)), stamp=s64(stamp), mask=255, ftres=res))
    for mask in range(256):
        out.append(time_case(random.Random(1000 + mask), mask=mask, ftres=1))
    for res in (0x100, 0x100000000, 0x100000001, 0x7FFFFFFF, -0x80000000, 2, 0):
        out.append(time_case(random.Random(2000 + res % 977), mask=255, ftres=res))
    return out


def gettime_ppc(e, c):
    L = CL.PpcLog({}, {"ftres": [c["ftres"]], "ftw": list(c["ftw"])})
    e.cl = L
    res = e.alloc(4, b"\xA5" * 4)
    stamp = be_val(e, c["stamp"])
    cells = e.alloc(32, b"".join(struct.pack(">i", g) for g in c["garbage"]))
    outs = [cells + 4 * i if (c["mask"] >> i) & 1 else 0 for i in range(8)]

    def filetime(emu):
        ft, st = emu.r(3), emu.r(4)
        L.rec("FileTimeToSystemTime", [emu.u32(ft), emu.u32(ft + 4)])
        emu.write(st, b"".join(struct.pack(">H", L.ret("ftw", 0) & 0xFFFF) for _ in range(8)))
        emu.set_r(3, T.u32(s64(L.ret("ftres", 0)) & 0xFFFFFFFF))

    S = CL.Stubs()
    S.imp(FT_NAME, filetime)
    saved = S.activate(e)
    try:
        T.call_ppc(e, e.func("GetTime__11TbTimeStampCFPiPiPiPiPiPiPiPi"), [T.I(res), T.I(stamp)] + [T.I(o) for o in outs])
    finally:
        CL.Stubs.deactivate(e, saved)
    return dict(ret=T.s32(e.u32(res)), cells=[T.s32(e.u32(cells + 4 * i)) for i in range(8)], stamp=be_get(e, stamp), log=L.log)


def gettime_host(lib, c):
    cells = (I32 * 8)(*c["garbage"])
    stamp = I64(c["stamp"])
    outs = [C.cast(C.byref(cells, 4 * i), VP) if (c["mask"] >> i) & 1 else None for i in range(8)]
    f = lib["tpw_pc_gettime"]
    f.restype, f.argtypes = I32, [P64] + [VP] * 8
    CL.host_begin(lib, {}, {"ftres": [s64(c["ftres"])], "ftw": list(c["ftw"])})
    try:
        ret = f(C.byref(stamp), *outs)
    finally:
        log = CL.host_end(lib)
    return dict(ret=ret, cells=list(cells), stamp=stamp.value, log=log)


def make(title, sides, edge, rnd):
    ppc, host = sides
    return Base(title, ppc, host, edge=edge, rnd=rnd)


TESTS = [
    make("TbTimeStamp::operator+", value_op("__pl__11TbTimeStampCFRC10TbTimeDiff", "tpw_pc_add"), pair_edges, pair),
    make("TbTimeStamp::operator-", value_op("__mi__11TbTimeStampCFRC11TbTimeStamp", "tpw_pc_sub"), pair_edges, pair),
    make("TbTimeStamp::operator>", (gt_ppc, gt_host), pair_edges, pair),
    make("TbTimeStamp::GetTime", (gettime_ppc, gettime_host), time_edges, time_case),
]
