"""Unit v5887, LLCR_FixBridges (walks a ride's track from its start, fixing the bridge cells' flags), against
libtpw_unit_LLCR_FixBridges_2.dylib (tests/unit_LLCR_FixBridges_2.cpp), main app, src/LLC..._LLCR_FixBridges_2.cpp:
  LLCR_FixBridges 0x1008fa34

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (LLCR_OffsetStartOfTrack, OPPOSITE, the cell predicates, GetNeighbours, NumberOfNeighbours, GetNeighbouringCell, SetFlag, UnSetFlag,
SetHFieldTile, SetCurRideAnim, StartFlashingObject, StopFlashingObject) are fakes on both sides that record their arguments (cells as grid
index). The predicates answer from scratch bytes of the cell (+0x14 track, +0x15 not just track, +0x16 corner, +0x17 bridge, +0x18 start
block), the neighbour byte is the cell's +0xc, GetNeighbouringCell walks the grid by 2 and answers NULL outside it. After 150 IsJustTrack
calls the fakes make the walk end, so no case loops forever (both sides count alike).

The world of a case: a track grid of 48 rows (gGameWorld points so that the grid lands on the block), three ride anim sets (indices 1 to 3)
with flag words, a ride object (cell x +4, y +6, rotation +0x10), the direction answer of OffsetStartOfTrack. What is compared: the answer,
gLLCCE0CC, the anim set flag words and the event log.

No fused operations. Not tested: a start cell outside the 48 rows (the fakes would read outside the block).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCR_FixBridges_2"
MASK = 0xFFFFFFFF

FN = 0x1008FA34
OWN = {FN}

A_OFFSET, A_IS_TRACK, A_JUST, A_CORNER, A_OPP = 0x1008C620, 0x100A3574, 0x100A3628, 0x100A37A0, 0x10102500
A_GETNB, A_BRIDGE, A_NBCELL, A_NUMNB = 0x100816E8, 0x100A34C4, 0x1012FDDC, 0x10081744
A_UNSET, A_SET, A_HFIELD, A_ANIM, A_START, A_STOP, A_STARTBLK = 0x100A29CC, 0x100A2964, 0x100A3BAC, 0x100C6670, 0x1006D2F8, 0x1006D430, 0x100A36CC

ADDR = dict(world=0x1037EBFC, sets=0x102C79A4, E0CC=0x102DE0CC)
GRID_OFF = 0x1102D8
ROWS = 48
GRID_SIZE = 0x28 * 128 * ROWS
# cell x +4 (CThing), y +6, rotation +0x10; anim set mFlags +4; cell mesh +4, neighbours +0xc, parent +0x10; cell size; pointer size (host 8)
HOST_LAYOUT = [4, 6, 0, 0x10, 8, 4, 0xC, 0x10, 0x28, 8]   # the host's anim set has a native pointer first: mFlags at 8

ST = {"just": 0}


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def rd(e, a, n=1):
    return e.read(a, n)[0] if n == 1 else e.read(a, n)


def world_base(e):
    return e.u32(ADDR["world"]) + GRID_OFF


def cidx(e, p):
    if p == 0:
        return 0x7FFFFFFF
    return (p - world_base(e)) // 0x28


def panic():
    return ST["just"] > 150


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk_pred(name, off, fn=None):
    def stub(e):
        p = e.r(3) & MASK
        e.cl.rec(name, (cidx(e, p),))
        v = 0 if p == 0 else rd(e, p + off)
        e.set_r(3, fn(v) if fn else v)
    return stub


def stub_offset(e):
    x, y = s32(e.s32(e.r(3))), s32(e.s32(e.r(4)))
    e.cl.rec("OffsetStart", (x, y, s32(e.r(5))))
    e.set_r(3, e.cl.ret("off") & 0xFF)


def stub_opp(e):
    d = e.r(3) & 0xFF
    e.cl.rec("OPPOSITE", (d,))
    e.set_r(3, ((d << 4) | (d >> 4)) & 0xFF)


def stub_getnb(e):
    p = e.r(3) & MASK
    e.cl.rec("GetNeighbours", (cidx(e, p),))
    e.set_r(3, 0 if (panic() or p == 0) else rd(e, p + 0xC))


def stub_numnb(e):
    p = e.r(3) & MASK
    e.cl.rec("NumberOfNeighbours", (cidx(e, p),))
    e.set_r(3, 0 if p == 0 else bin(rd(e, p + 0xC)).count("1"))


def stub_just(e):
    p = e.r(3) & MASK
    e.cl.rec("IsJustTrack", (cidx(e, p),))
    ST["just"] += 1
    e.set_r(3, 1 if panic() else int(rd(e, p + 0x15) != 1))


def stub_corner(e):
    p = e.r(3) & MASK
    e.cl.rec("IsCorner", (cidx(e, p),))
    e.set_r(3, 0 if (panic() or p == 0) else rd(e, p + 0x16))


def stub_flag(name):
    def stub(e):
        e.cl.rec(name, (cidx(e, e.r(3) & MASK), e.r(4) & 0xFFFF))
    return stub


def stub_hfield(e):
    e.cl.rec("SetHFieldTile", (cidx(e, e.r(3) & MASK),))


def stub_nbcell(e):
    p = e.r(3) & MASK
    d, off = e.r(4) & 0xFF, s32(e.r(5))
    e.cl.rec("GetNeighbouringCell", (cidx(e, p), d, off))
    if p == 0:
        e.set_r(3, 0)
        return
    i = cidx(e, p)
    if i < 0:
        e.set_r(3, 0)
        return
    x, y = i & 127, i >> 7
    if d == 1:
        y -= off
    elif d == 4:
        x += off
    elif d == 0x10:
        y += off
    elif d == 0x40:
        x -= off
    else:
        e.set_r(3, 0)
        return
    if x < 0 or x > 127 or y < 0 or y > 47:
        e.set_r(3, 0)
        return
    e.set_r(3, (world_base(e) + 0x28 * (y * 128 + x)) & MASK)


def stub_anim(e):
    e.cl.rec("SetCurRideAnim", (e.cl.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK, fb(e.f(1)), e.r(8) & MASK))


def stub_flash(name):
    def stub(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)),))
    return stub


STUBS.internal(A_OFFSET, stub_offset)
STUBS.internal(A_IS_TRACK, mk_pred("IsTrack", 0x14, lambda v: int(v != 0)))
STUBS.internal(A_JUST, stub_just)
STUBS.internal(A_CORNER, stub_corner)
STUBS.internal(A_OPP, stub_opp)
STUBS.internal(A_GETNB, stub_getnb)
STUBS.internal(A_BRIDGE, mk_pred("IsBridge", 0x17, lambda v: int(v != 0)))
STUBS.internal(A_NBCELL, stub_nbcell)
STUBS.internal(A_NUMNB, stub_numnb)
STUBS.internal(A_UNSET, stub_flag("UnSetFlag"))
STUBS.internal(A_SET, stub_flag("SetFlag"))
STUBS.internal(A_HFIELD, stub_hfield)
STUBS.internal(A_ANIM, stub_anim)
STUBS.internal(A_START, stub_flash("StartFlashingObject"))
STUBS.internal(A_STOP, stub_flash("StopFlashingObject"))
STUBS.internal(A_STARTBLK, mk_pred("IsTrackStartBlock", 0x18, lambda v: int(v != 0)))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def put(self, a, fmt, *vals):
        self.write(a, struct.pack(self.fmt[0] + fmt, *vals))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    psz = 4

    def __init__(self, e, case):
        self.e = e
        self.case = case
        ST["just"] = 0
        self.sets = []
        self.set_labels = {}
        self.L = e.cl = CL.PpcLog(self.set_labels, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def label(self, addr, n):
        self.set_labels[addr] = n

    def call(self, ride, a, b, c):
        self.L = self.e.cl = CL.PpcLog(dict(self.set_labels), self.case["queues"])   # the labels are known only after the build
        r3, _, _ = T.call_ppc(self.e, FN, [ride & MASK, a, b, c])
        return r3 & 0xFF


class Host(Side):
    fmt = "<I"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_fb_reset()
        self.set_labels = {}
        self.case = case

    def begin(self):
        CL.host_begin(self.lib, {n: a for a, n in self.set_labels.items()}, self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fb_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_fb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def label(self, addr, n):
        self.set_labels[addr] = n

    def call(self, ride, a, b, c):
        self.begin()
        return self.lib.tpw_fb_fix(ride, a, b, c) & 0xFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_fb_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "fix").restype, getattr(lib, p + "fix").argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    world = s.alloc(GRID_OFF + GRID_SIZE)
    block = world + GRID_OFF
    assert world > 0 and (s.psz == 8 or block + GRID_SIZE < 0x100000000)
    s.put(s.g("world"), "I" if s.psz == 4 else "Q", world)
    sets = []
    for i, fl in enumerate(case["sets"]):
        a = s.alloc(0x40)
        s.put(a + s.psz, "I", fl)
        sets.append(a)
        s.label(a, i + 1)
        s.put(s.g("sets") + s.psz * (i + 1), "I" if s.psz == 4 else "Q", a)
    for (x, y, mesh, nb, parent, track, notjust, corner, bridge, startblk) in case["cells"]:
        a = block + 0x28 * (y * 128 + x)
        s.put(a + 4, "I", mesh)
        s.write(a + 0xC, bytes([nb]))
        s.put(a + 0x10, "H", parent)
        s.write(a + 0x14, bytes([track, notjust, corner, bridge, startblk]))
    ride = s.alloc(0x40)
    s.write(ride + 4, bytes([case["rx"]]))
    s.write(ride + 6, bytes([case["ry"]]))
    s.put(ride + 0x10, "i", case["rot"])
    s.sets_addr = sets
    return ride


def execute(s, ride, case):
    return s.call(ride, *case["flags"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "E0CC": s.read(s.g("E0CC"), 1)[0],
            "setflags": [s.rd_words(a + s.psz, 1)[0] for a in s.sets_addr]}


def run(s, case):
    try:
        ride = build(s, case)
        return observe(s, case, execute(s, ride, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


DIRS = [1, 4, 0x10, 0x40]
STEP = {1: (0, -2), 4: (2, 0), 0x10: (0, 2), 0x40: (-2, 0)}


def rand_cell(r, x, y, nb):
    return (x, y, r.choice([0, 0, 1, 2, 3]), nb, r.choice([1, 1, 2]), int(r.random() < 0.85), int(r.random() < 0.1),
            int(r.random() < 0.15), int(r.random() < 0.45), int(r.random() < 0.25))


def make(r, d0=None, length=None, bridges=None):
    rx, ry = r.randrange(10, 50), r.randrange(10, 40)
    d = r.choice(DIRS) if d0 is None else d0
    n = r.randrange(1, 6) if length is None else length
    cells, x, y = [], rx, ry
    for i in range(n):
        nb = d | (d >> 4 | d << 4) & 0xFF if r.random() < 0.7 else d
        if i == n - 1 and r.random() < 0.6:
            nb &= ~d & 0xFF
        if r.random() < 0.15:
            nb = r.randrange(256)
        c = rand_cell(r, x, y, nb)
        if bridges is not None:
            c = c[:8] + (int(bridges),) + c[9:]
            c = c[:5] + (1, 0, 0) + c[8:] if bridges else c
        cells.append(c)
        dx, dy = STEP[d]
        x, y = x + dx, y + dy
    for _ in range(r.randrange(0, 6)):
        cells.append(rand_cell(r, x + r.randrange(-3, 4), y + r.randrange(-3, 4), r.randrange(256)))
    seen = {}
    for c in cells:
        seen[(c[0], c[1])] = c
    return {"rx": rx, "ry": ry, "rot": r.randrange(-1, 4) * 90, "cells": list(seen.values()),
            "sets": [r.choice([0, 0x4000000, 0x20, 0x4000020]) for _ in range(3)],
            "flags": [r.randrange(2), r.randrange(2), r.randrange(2)],
            "queues": {"off": [d if r.random() < 0.93 else r.choice([0, 2, 3, 0x80, 0x11])]}}


def edges():
    r = random.Random(5887)
    for d in DIRS:
        for bridges in (True, False):
            for n in (1, 2, 4):
                yield make(r, d, n, bridges)
    for _ in range(30):
        yield make(r)


def rnd(r):
    return make(r)


TESTS = [
    FnTest("LLCR_FixBridges", ppc, host, edge=edges, rnd=rnd),
]
