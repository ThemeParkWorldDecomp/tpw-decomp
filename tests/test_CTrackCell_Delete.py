"""Unit v5990, CTrackCell::Delete, against libtpw_unit_CTrackCell_Delete.dylib (tests/unit_CTrackCell_Delete.cpp), main app
("SimTheme Park"), src/game/park/TrackCell.cpp:
  CTrackCell::Delete 0x100a40c4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee
outside the unit (the cell base's accessors, SetHFieldTile, ModifyType, GetNeighbouringCell, the cost, bank and map calls, OPPOSITE,
ADD_DIRECTION, LLCR_DeleteRide, GAME_ASSERTMSG) is a fake on both sides that records its arguments (a cell as its index in the world's
track cells) and answers from the case's queues. abs runs for real on both sides. Delete itself runs as the original (it recurses).

The world of a case: nine track cells (ids 0 to 8; id 0 is the cell before the grid), the globals the function reads (first block, track
end, two flags, the recorder's flag word), the thing array's first four entries. What is compared: the event log and every field of the
nine cells.

No fused operations. Not tested: a parent id outside 0 to 8 (it would read outside the grid), a track-X cell whose parent is a track-X cell
(the original recurses without end).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CTrackCell_Delete"
MASK = 0xFFFFFFFF

FN = 0x100a40c4
SIZES = {FN: 3480}
OWN = set(SIZES)

ADDR = dict(gLLCCFirstBlockX=0x1029e02c, gLLCCFirstBlockY=0x1029e030, gLLCCE034=0x1029e034, gLLCCE038=0x1029e038,
            gLLCC47E0=0x102e47e0, gLLCC47E4=0x102e47e4, gRecording=0x1037ec44 + 0x24, gGameWorld=0x1037ebfc,
            gThingArray=0x1034cbfc, gMapDeltaPlusY=0x1034c25c, gMapDeltaPlusX=0x1034c284, gMapDeltaPlusXPlusY=0x1034c270)
HOST_LAYOUT = [0x28, 0x24]

CELLFMT = "HBBIIBBHHHIIIhBBhH"   # the 0x28 bytes of a cell, no padding
CELLSIZE = 0x28
NCELLS = 9                       # ids 0 to 8; id k is at B + k * 0x28, so the world's track grid starts at B + 0x28
WORLD_DELTA = 0x28 - 0x1102d8    # world = B + WORLD_DELTA

CUR = {"B": 0}


def s32(v):
    return T.s32(v)


def ref(addr):
    """A cell's index in the world's track cells."""
    return (addr - CUR["B"]) // CELLSIZE - 1


def rot2(d):
    return ((d << 2) | (d >> 6)) & 0xFF


def rot4(d):
    return ((d << 4) | (d >> 4)) & 0xFF


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def reg(addr, name, kinds, ret=None):
    """A fake at addr: logs name with one value per kind (c cell index, i s32, u u32, b u8) from r3 on, then ret(e, L, vals)."""
    def fn(e):
        vals = []
        for k, kind in enumerate(kinds):
            v = e.r(3 + k)
            if kind == "c":
                vals.append(ref(v))
            elif kind == "i":
                vals.append(s32(v))
            elif kind == "u":
                vals.append(v & MASK)
            else:
                vals.append(v & 0xFF)
        e.cl.rec(name, tuple(vals))
        if ret:
            ret(e, e.cl, vals)
    STUBS.internal(addr, fn)


def r_word(key):
    return lambda e, L, v: e.set_r(3, L.ret(key) & MASK)


def r_byte(key):
    return lambda e, L, v: e.set_r(3, L.ret(key) & 0xFF)


def r_cell(key):
    def f(e, L, v):
        k = L.ret(key)
        e.set_r(3, 0 if k == 0 else (CUR["B"] + k * CELLSIZE) & MASK)
    return f


reg(0x10091c54, "KartCost", "", r_word("KartCost"))
reg(0x10091cd4, "WaterCost", "", r_word("WaterCost"))
reg(0x101012f4, "IsXYValid", "ii", r_word("IsXYValid"))
reg(0x10081930, "GetDirection", "c", r_byte("GetDirection"))
reg(0x10081678, "RemoveNeighbour", "cb")
reg(0x10081810, "ModifyDirection", "cb")
reg(0x10081608, "ModifyNeighbours", "cb")
reg(0x1008184c, "RemoveDirection", "cb")
reg(0x10081a1c, "SetOverlapCounter", "ci")
reg(0x10081968, "NumberOfDirections", "c", r_word("NumberOfDirections"))
reg(0x100816e8, "GetNeighbours", "c", r_byte("GetNeighbours"))
reg(0x10081890, "NumberOfHVNeighbours", "c", r_word("NumberOfHVNeighbours"))
reg(0x10081744, "NumberOfNeighbours", "c", r_word("NumberOfNeighbours"))
reg(0x100f49a0, "GetMapCellEquiv", "c", lambda e, L, v: e.set_r(3, 0x6B000000 + ((v[0] + 1) & 0xFF)))
reg(0x10102500, "OPPOSITE", "b", lambda e, L, v: e.set_r(3, rot4(v[0])))
reg(0x1010254c, "ADD_DIRECTION", "bb", lambda e, L, v: e.set_r(3, rot2(v[0])))
reg(0x1010fd98, "GetResidualValuePercentage", "u", r_word("Pct"))
reg(0x100f32ec, "MakeDeposit", "uu")
reg(0x1012fddc, "GetNeighbouringCellDir", "cbi", r_cell("NbrDir"))
reg(0x100a0828, "ModifyType", "ci")
reg(0x100a3bac, "SetHFieldTile", "c")
reg(0x100892c4, "LLCR_DeleteRide", "iii")


def stub_bank(e):
    e.cl.rec("GetBankAccount", (s32(e.r(3) - (CUR["B"] + WORLD_DELTA)),))
    e.set_r(3, 0x5A5A5A00)


STUBS.internal(0x1013edfc, stub_bank)


def stub_object_thing_id(e):
    """CMapCell::GetObjectThingId(out, cell): one halfword through the hidden pointer."""
    L = e.cl
    out, cell = e.r(3), e.r(4)
    L.rec("GetObjectThingId", (cell & MASK,))
    e.write(out, struct.pack(">H", L.ret("ThingId") & 0xFFFF))


def stub_delta(e):
    """CTrackCell::GetNeighbouringCell(MapDelta delta, int offset): the delta is a pointer to its words."""
    L = e.cl
    this, d, off = e.r(3), e.r(4), e.r(5)
    L.rec("GetNeighbouringCellDelta", (ref(this), s32(e.u32(d)), s32(e.u32(d + 4)), s32(off)))
    k = L.ret("NbrDelta")
    e.set_r(3, 0 if k == 0 else (CUR["B"] + k * CELLSIZE) & MASK)


def stub_abs(e):
    e.set_r(3, abs(s32(e.r(3))) & MASK)


STUBS.internal(0x10103a04, stub_object_thing_id)
STUBS.internal(0x1012fcb8, stub_delta)
STUBS.internal(0x1000dde8, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.imp("abs", stub_abs)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def put(self, a, code, v):
        self.write(a, struct.pack(self.fmt[0] + code, v))

    def cell(self, a, c):
        self.write(a, struct.pack(self.fmt[0] + CELLFMT, *c))

    def read_cell(self, a):
        return struct.unpack(self.fmt[0] + CELLFMT, self.read(a, CELLSIZE))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    def wptr(self, a, v):
        self.put(a, "I", v)

    def set_thing(self, i, v):
        self.put(ADDR["gThingArray"] + i * 0x14, "I", v)

    def consts(self):
        """The three compass offsets (initial data in the image; written so the test does not rely on it)."""
        for name, (x, y) in (("gMapDeltaPlusY", (0, 1)), ("gMapDeltaPlusX", (1, 0)), ("gMapDeltaPlusXPlusY", (1, 1))):
            self.write(ADDR[name], struct.pack(">iiI", x, y, 0))

    def call(self, cell, a, b, c):
        T.call_ppc(self.e, FN, [cell, a & MASK, b & MASK, c])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ctd_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ctd_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def wptr(self, a, v):
        self.put(a, "Q", v)

    def set_thing(self, i, v):
        self.lib.tpw_ctd_set_thing(i, v)

    def consts(self):
        pass

    def call(self, cell, a, b, c):
        self.lib.tpw_ctd_delete(cell, a, b, c)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ctd_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "set_thing").argtypes = [C.c_int32, C.c_uint64]
    getattr(lib, p + "delete").argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    B = s.alloc(NCELLS * CELLSIZE + 0x40)
    CUR["B"] = B
    s.wptr(s.g("gGameWorld"), B + WORLD_DELTA)
    s.consts()
    for i, c in enumerate(case["cells"]):
        s.cell(B + i * CELLSIZE, c)
    for name in ("gLLCCFirstBlockX", "gLLCCFirstBlockY", "gLLCCE034", "gLLCCE038", "gLLCC47E0", "gLLCC47E4", "gRecording"):
        s.put(s.g(name), "I", case[name] & MASK)
    for i, v in enumerate(case["things"]):
        s.set_thing(i, v)


def observe(s, case):
    B = CUR["B"]
    return {"log": s.log(), "cells": [s.read_cell(B + i * CELLSIZE) for i in range(NCELLS)]}


def run(s, case):
    try:
        build(s, case)
        s.call(CUR["B"] + case["this"] * CELLSIZE, case["a"], case["b"], case["c"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

TYPES = [0, 5, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14, 0x18, 0x19, 0x1a, 0x20, -1]
XTYPES = (0xc, 0x11)
COSTS = [0, 1, 100, 1000, 0x7FFFFFFF, 0xFFFFFFFF, 0x12345678]
PCTS = [0, 1, 50, 100, 101, 0x7FFFFFFF, -1, 0x10000]


def rq(r, choices):
    return [r.choice(choices) for _ in range(r.randrange(1, 6))]


def make_case(r, mode=None, ab=None):
    X = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 3, 2, -1, 100])
    Y = r.choice([0, 0, 0, 0, 1, -1, 3])
    EX = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 3, 2, -1])
    EY = r.choice([0, 0, 0, 0, 1, -1, 3])
    first = (X + (Y << 7) + 1) & 0xFFFF
    last = (EX + (EY << 7) + 1) & 0xFFFF
    this = r.randrange(1, 9)
    types = [r.choice(TYPES) for _ in range(NCELLS)]
    types[0] = 0
    if mode is not None:
        types[this] = mode
    cells = []
    for i in range(NCELLS):
        cid = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, first, last])
        parent = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8])
        counter = r.choice([-1, 0, 1, 5, -2, 0x7FFF, -0x8000])
        flags = r.choice([0, 0x10, 0x10, 0x20, 0xFFFF, 0x1234 & ~0x10, r.randrange(0x10000)])
        mesh = r.choice([0, 0, 0x12345678])
        c = [cid, r.randrange(256), r.randrange(256), mesh, types[i] & MASK, r.randrange(256), r.randrange(256), flags, parent,
             r.randrange(0x10000), r.randrange(1 << 32), r.randrange(1 << 32), r.randrange(1 << 32), counter, r.randrange(256),
             r.randrange(256), r.choice([-1, 0, 7, -0x8000]), r.randrange(0x10000)]
        cells.append(c)
    cells[0] = [0] * 3 + [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    cells[0] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    # the cells' parents that matter point only into the grid; a track-X cell that is `this` needs a parent that is not a track-X cell
    if types[this] in XTYPES:
        cells[this][8] = r.choice([p for p in range(NCELLS) if types[p] not in XTYPES])
    cells[this][0] = r.choice([first, last, 0, this, this, r.randrange(9)])
    if r.random() < 0.5:
        # neighbours that belong to this cell's ride
        for p in range(1, NCELLS):
            if p != this and types[p] not in XTYPES and r.random() < 0.5:
                cells[p][8] = cells[this][0] if cells[this][0] < NCELLS else 0
    a, b = ab if ab is not None else (r.choice([0, -1, 2, -2, 1, 5, -5, 0]), r.choice([0, -1, 2, -2, 1, 5, 0, -5]))
    if ab is None and r.random() < 0.5:
        a, b = r.choice([(0, 0), (-1, -1), (2, 0), (-2, 0), (0, 2), (0, -2), (2, 2), (-2, -2)])
    queues = {
        "KartCost": rq(r, COSTS), "WaterCost": rq(r, COSTS),
        "IsXYValid": rq(r, [0, 1, 1]),
        "GetDirection": rq(r, [0, 1, 4, 0x10, 0x40, 2, 0x80, 5]),
        "NumberOfDirections": rq(r, [0, 1, 2, 3, 4]),
        "GetNeighbours": rq(r, [0, 1, 4, 0x10, 0x40, 5, 0x41, 0x14, 0x11, 0x44, 0x50, 0x45, 0xFF, 2]),
        "NumberOfHVNeighbours": rq(r, [0, 1, 2, 2, 3]),
        "NumberOfNeighbours": rq(r, [0, 1, 1, 2, 3]),
        "ThingId": rq(r, [0, 1, 2, 3]),
        "Pct": rq(r, PCTS),
        "NbrDir": rq(r, [0, 0] + list(range(1, 9))),
        "NbrDelta": rq(r, [0, 0] + list(range(1, 9))),
    }
    return {"queues": queues, "cells": cells, "this": this, "a": a, "b": b, "c": r.choice([0, 1]),
            "gLLCCFirstBlockX": X, "gLLCCFirstBlockY": Y, "gLLCCE034": EX, "gLLCCE038": EY,
            "gLLCC47E0": r.choice([0, 1]), "gLLCC47E4": r.choice([0, 1]), "gRecording": r.choice([0, 0, 1]),
            "things": [r.choice([0, 0x50000000])] + [0x50000100 * k for k in (1, 2, 3)]}


def edges():
    r = random.Random(5990)
    pairs = [(0, 0), (-1, -1), (2, 0), (-2, 0), (0, 2), (0, -2), (1, 1), (2, 2), (-2, -2), (5, 0)]
    for mode in (0, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14, 0x19, 0x1a):
        for ab in pairs:
            yield make_case(r, mode, ab)
    for _ in range(300):
        yield make_case(r, r.choice([0xb, 0xd, 0x10, 0x12, 0x19]), r.choice(pairs))


def rnd(r):
    return make_case(r)


TESTS = [
    FnTest("CTrackCell::Delete", ppc, host, edge=edges, rnd=rnd),
]
