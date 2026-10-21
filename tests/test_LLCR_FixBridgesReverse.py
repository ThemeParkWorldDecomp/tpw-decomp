"""Unit v51010, LLCR_FixBridgesReverse, against libtpw_unit_LLCR_FixBridgesReverse.dylib (tests/unit_LLCR_FixBridgesReverse.cpp), main app
("SimTheme Park"), src/game/park/LLCellRide.cpp:
  LLCR_FixBridgesReverse 0x1009024c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit (the CTrackCell predicates and setters, CCellBase::GetNeighbours / NumberOfNeighbours, GetNeighbouringCell, OPPOSITE,
LLCR_OffsetEndOfTrack, SetCurRideAnim, Start/StopFlashingObject) is a fake on both sides that records its arguments (cells as their index in
the track grid, ride sets as labels) and answers from the case's queues (the last value of a queue repeats).

The world of a case: gGameWorld pointing at a persistent world block whose track grid rows 24 to 104 hold a random 8-cell pattern (mesh
instance at +4, parent id at +0x10), gRideAnimSets 1 to 3 (flags with and without the flashing bit), the ride object (x, y, rotation) and the
queues. What is compared: the event log of every fake call and the byte gLLCCE0CC. The answer is not read by any caller.

No fused operations. Not tested: a cell index wrapping below 0 (the walk stays about 24 cells from the start).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCR_FixBridgesReverse"
MASK = 0xFFFFFFFF

FN = 0x1009024C
SIZES = {FN: 2216}
OWN = set(SIZES)

A_ISTRACK, A_ISJUST, A_ISCORNER, A_ISBRIDGE, A_ISSTART = 0x100A3574, 0x100A3628, 0x100A37A0, 0x100A34C4, 0x100A36CC
A_OPP, A_GETN, A_NUMN, A_NBR = 0x10102500, 0x100816E8, 0x10081744, 0x1012FDDC
A_SETANIM, A_UNSET, A_SET, A_HFIELD = 0x100C6670, 0x100A29CC, 0x100A2964, 0x100A3BAC
A_START, A_STOP, A_OFFEND = 0x1006D2F8, 0x1006D430, 0x1008C6D8

ADDR = dict(world=0x1037EBFC, sets=0x102C79A4, e0cc=0x102DE0CC)
HOST_LAYOUT = [0x28, 4, 0x10, 0x1102D8]
GRID = 0x1102D8
ROW0, ROWS = 24, 80

PPC_WORLD = [0]
PPC_SETS = {}
HOST_SETS = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cidx(r3):
    r3 &= MASK
    if r3 == 0:
        return -1
    return (r3 - PPC_WORLD[0] - GRID) // 0x28


def pred(name, addr):
    def fn(e):
        e.cl.rec(name, (cidx(e.r(3)),))
        e.set_r(3, e.cl.ret(name) & MASK)
    STUBS.internal(addr, fn)


pred("IsTrack", A_ISTRACK)
pred("IsJustTrack", A_ISJUST)
pred("IsBridge", A_ISBRIDGE)
pred("IsTrackStartBlock", A_ISSTART)
pred("NumberOfNeighbours", A_NUMN)

PHASE = [0]


def st_corner(e):
    PHASE[0] = 0
    e.cl.rec("IsCorner", (cidx(e.r(3)),))
    e.set_r(3, e.cl.ret("IsCorner") & 0xFF)


def st_bridge(e):
    PHASE[0] = 1
    e.cl.rec("IsBridge", (cidx(e.r(3)),))
    e.set_r(3, e.cl.ret("IsBridge") & MASK)


def st_getn(e):
    e.cl.rec("GetNeighbours", (cidx(e.r(3)),))
    e.set_r(3, e.cl.ret("GetNeighbours") & 0xFF)


def st_nbr(e):
    here = cidx(e.r(3))
    v = e.cl.ret("Nbr")
    if PHASE[0] == 1 and v == 0:
        v = 1
    idx = -1
    res = 0
    if v != 0:
        idx = here + v
        if idx < 0 or idx >= 0x4000:
            idx = here
        res = PPC_WORLD[0] + GRID + idx * 0x28
    e.cl.rec("GetNeighbouringCell", (here, e.r(4) & 0xFF, s32(e.r(5)), idx))
    e.set_r(3, res & MASK)


def st_setflag(e):
    e.cl.rec("SetFlag", (cidx(e.r(3)), e.r(4) & 0xFFFF))


def st_unsetflag(e):
    e.cl.rec("UnSetFlag", (cidx(e.r(3)), e.r(4) & 0xFFFF))


def st_hfield(e):
    e.cl.rec("SetHFieldTile", (cidx(e.r(3)),))


def st_opp(e):
    d = e.r(3) & 0xFF
    res = ((d << 4) | (d >> 4)) & 0xFF
    e.cl.rec("OPPOSITE", (d, res))
    e.set_r(3, res)


def st_off(e):
    x = e.s32(e.r(3))
    y = e.s32(e.r(4))
    x, y = s32(x), s32(y)
    e.cl.rec("OffsetEnd", (x, y, s32(e.r(5))))
    x += e.cl.ret("OffX")
    y += e.cl.ret("OffY")
    e.write(e.r(3), struct.pack(">i", x))
    e.write(e.r(4), struct.pack(">i", y))
    e.set_r(3, e.cl.ret("OffDir") & 0xFF)


def st_anim(e):
    L = e.cl
    L.rec("SetCurRideAnim", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK, canon(fb(e.f(1))), e.r(8) & MASK))


def st_start(e):
    e.cl.rec("StartFlashing", (e.cl.lab(e.r(3)),))


def st_stop(e):
    e.cl.rec("StopFlashing", (e.cl.lab(e.r(3)),))


def canon(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


STUBS.internal(A_ISCORNER, st_corner)
STUBS.internal(A_ISBRIDGE, st_bridge)
STUBS.internal(A_GETN, st_getn)
STUBS.internal(A_NBR, st_nbr)
STUBS.internal(A_SET, st_setflag)
STUBS.internal(A_UNSET, st_unsetflag)
STUBS.internal(A_HFIELD, st_hfield)
STUBS.internal(A_OPP, st_opp)
STUBS.internal(A_OFFEND, st_off)
STUBS.internal(A_SETANIM, st_anim)
STUBS.internal(A_START, st_start)
STUBS.internal(A_STOP, st_stop)


def setup(runner):
    T.install_std_stubs(runner.emu)
    PPC_WORLD[0] = T.persist_alloc(runner.emu, GRID + 0xA0000 + 0x100)
    for i in range(1, 4):
        PPC_SETS[i] = T.persist_alloc(runner.emu, 0x40)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_bytes(self, a, n):
        return list(self.read(a, n))


class Ppc(Side):
    fmt = ">I"
    psz = 4
    flagoff = 4

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({PPC_SETS[i]: i for i in range(1, 4)}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def world(self):
        return PPC_WORLD[0]

    def sets(self):
        return PPC_SETS

    def setptr(self, a, v):
        self.words(a, [v])

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, ride, a, b, c):
        T.call_ppc(self.e, FN, [ride & MASK, a, b, c])


class Host(Side):
    fmt = "<I"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.flagoff = lib.flagoff
        if not HOST_SETS:
            for i in range(1, 4):
                HOST_SETS[i] = lib.tpw_fbr_alloc(0x40)
        CL.host_begin(lib, {i: HOST_SETS[i] for i in range(1, 4)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fbr_addr(name.encode())

    def world(self):
        return self.lib.tpw_fbr_world()

    def sets(self):
        return HOST_SETS

    def setptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def alloc(self, n):
        return self.lib.tpw_fbr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, ride, a, b, c):
        self.lib.tpw_fbr_run(ride, a, b, c)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for n, res, args in (("addr", C.c_uint64, [C.c_char_p]), ("alloc", C.c_uint64, [C.c_uint32]), ("world", C.c_uint64, [])):
        f = getattr(lib, "tpw_fbr_" + n)
        f.restype, f.argtypes = res, args
    lib.tpw_fbr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fbr_run.restype, lib.tpw_fbr_run.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_fbr_layout(out)
    assert list(out)[1:n] == HOST_LAYOUT, list(out)[:n]
    lib.flagoff = out[0]
    lib.bound = True


def build(s, case):
    world = s.world()
    s.setptr(s.g("world"), world)
    for i, sa in s.sets().items():
        s.setptr(s.g("sets") + i * s.psz, sa)
        s.write(sa, bytes(0x40))
        s.words(sa + s.flagoff, [case["setflags"][i - 1]])
    e = s.fmt[0]
    pat = bytearray()
    for mesh, parent in case["cells"]:
        cell = bytearray(0x28)
        struct.pack_into(e + "I", cell, 4, mesh)
        struct.pack_into(e + "H", cell, 0x10, parent)
        pat += cell
    s.write(world + GRID + ROW0 * 128 * 0x28, bytes(pat) * (16 * ROWS))
    ride = s.alloc(0x20)
    s.write(ride, bytes(0x20))
    s.write(ride + 4, bytes([case["x"]]))
    s.write(ride + 6, bytes([case["y"]]))
    s.words(ride + 0x10, [case["rot"]])
    s.write(s.g("e0cc"), bytes([0xAA]))
    case["ride"] = ride


def observe(s, case):
    return {"log": s.log(), "e0cc": s.rd_bytes(s.g("e0cc"), 1)}


def run(s, case):
    try:
        build(s, case)
        s.call(case["ride"], case["args"][0], case["args"][1], case["args"][2])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def lst(r, vals, lo, hi, last=None):
    v = [r.choice(vals) for _ in range(r.randint(lo, hi))]
    if last is not None:
        v.append(last)
    return v


DIRS = [1, 4, 0x10, 0x40]
NB = [0, 1, 4, 0x10, 0x40, 5, 0x11, 0x44, 0x55, 0xFF, 3, 0x14, 0x41]


def case_at(r, istrack0=None):
    q = {
        "IsTrack": lst(r, [0, 1, 1], 1, 6),
        "IsJustTrack": [0] * r.choice([0, 0, 1, 3]) + [1],
        "IsCorner": lst(r, [0, 0, 0, 1, 4, 0x10, 5], 0, 4, 0),
        "IsBridge": lst(r, [0, 1, 1], 1, 10),
        "IsTrackStartBlock": lst(r, [0, 1], 1, 4),
        "GetNeighbours": lst(r, NB, 1, 8, 0),
        "NumberOfNeighbours": lst(r, [0, 1, 2, 2, 2, 3], 1, 8),
        "Nbr": lst(r, [0, 1, -1, 2, -2, 127, -127, 128, -128, 129, -129, 256], 1, 8),
        "OffX": [r.randint(-2, 2)],
        "OffY": [r.randint(-2, 2)],
        "OffDir": [r.choice(DIRS + DIRS + DIRS + [0, 2, 0x80])],
    }
    if istrack0 is not None:
        q["IsTrack"] = [istrack0] + q["IsTrack"]
    return {
        "queues": q,
        "x": r.randint(60, 68),
        "y": r.randint(60, 68),
        "rot": r.choice([0, 0x5A, 0xB4, 0x10E, 7]),
        "cells": [(r.choice([0, 1, 2, 3]), r.choice([0, 1, 1, 2])) for _ in range(8)],
        "setflags": [r.choice([0, 0x04000000, 0x04000020, 0x20]) for _ in range(3)],
        "args": [r.randint(0, 1), r.randint(0, 1), r.randint(0, 1)],
    }


def edges():
    r = random.Random(77)
    for t in (0, 1):
        for a in (0, 1):
            for b in (0, 1):
                for c in (0, 1):
                    cs = case_at(r, t)
                    cs["args"] = [a, b, c]
                    yield cs
    for _ in range(300):
        yield case_at(r)


def rnd(r):
    return case_at(r)


TESTS = [
    FnTest("LLCR_FixBridgesReverse", ppc, host, edge=edges, rnd=rnd),
]
