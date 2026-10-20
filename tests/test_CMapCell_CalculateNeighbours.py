"""Unit v5632, CMapCell::CalculateNeighbours: links a map cell with the neighbours of its mode (mode 1, paths: the eight compass neighbours, then
the diagonals beside queues are dropped; modes 0 and 3: the current object's entry cell and the straight neighbour the a / b pair points to),
against libtpw_unit_CMapCell_CalculateNeighbours.dylib (tests/unit_CMapCell_CalculateNeighbours.cpp), main app ("SimTheme Park"),
src/MapDelta_map_CMapCell_CalculateNeighbours.cpp:
  CMapCell::CalculateNeighbours 0x10099998

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments: the two GetNeighbouringCell answer the cell the case's tables give for the
caller's label and the offset (x, y) or the direction bit; GetNeighbours, GetDirection, NumberOfNeighbours, IsQueue and MakeNewTimeStamp
answer from the case's queues; ModifyNeighbours, RemoveNeighbour, SetHFieldTile, SetTimeStamp and SOUND_SendEvent only record;
ADD_DIRECTION records and answers the real rotation (so the direction loop ends as on the Mac).

The world of a case: a world block holding the map (label 90) and eight cells (labels 1 to 8; the call's cell is 1) at world + 0x2d8 +
label * 0x44 with their id, mode and parent id; gGameWorld, the current object (its entry cell id at +0x32), gLLCCDB68, the kept a / b,
the four parent halfwords, the join flag, the ui sound bank and the action recorder's recording word; the eight compass offsets (on the
Mac side; the host's are constants with the same values). What is compared: the event log, every field of the eight cells and the globals.

No fused operations (the sound's position is 10.0f times a value below 128, exact). Not tested: a NULL straight neighbour beside a diagonal
one of the mode (the original reads it without a check; on the map a diagonal cell always has both straight cells) and a NULL current object
in modes 0 and 3 (read without a check).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CMapCell_CalculateNeighbours"
MASK = 0xFFFFFFFF

FN = 0x10099998
SIZES = {FN: 5540}
OWN = set(SIZES)

MAKE_STAMP, SET_STAMP = 0x101025C4, 0x10102604
GNC_DELTA, GNC_BIT = 0x101036B0, 0x10103814
MODIFY_NB, REMOVE_NB, NUMBER_NB = 0x10081608, 0x10081678, 0x10081744
IS_QUEUE, SET_TILE = 0x1009C490, 0x1009CDB4
GET_DIR, GET_NB = 0x10081930, 0x100816E8
SOUND, ADD_DIR = 0x100DD9FC, 0x1010254C

ADDR = dict(prevA=0x1029E214, prevB=0x1029E218, idMinusY=0x1029E21C, idPlusX=0x1029E21E, idPlusY=0x1029E220, idMinusX=0x1029E222,
            join=0x102E4838, db68=0x102DDB68, db6c=0x102DDB6C, bank3=0x102F7C94 + 0xC, world=0x1037EBFC, recording=0x1037EC44 + 0x24)
# The compass offsets (Mac addresses; the host's are constants of the unit library with these values).
DELTAS = {0x1034C2AC: (0, -1), 0x1034C298: (1, -1), 0x1034C284: (1, 0), 0x1034C270: (1, 1), 0x1034C25C: (0, 1), 0x1034C248: (-1, 1),
          0x1034C234: (-1, 0), 0x1034C220: (-1, -1)}
MAX_CELLS = 16
K = 8                 # cells per case, labels 1..K (1 is the call's cell)
MAP_LABEL = 90
HOST_LAYOUT = [0, 8, 0xC, 0x10, 0x44, 0x2D8, 0x32, MAX_CELLS]

# Every field of a cell (offset, size), so both byte orders read the same numbers.
FIELDS = [(0, 2), (2, 1), (3, 1), (4, 4), (8, 4), (0xC, 1), (0xD, 1), (0xE, 2), (0x10, 2), (0x12, 2), (0x14, 4), (0x18, 4), (0x1C, 4),
          (0x20, 2), (0x22, 1), (0x23, 1), (0x24, 2), (0x26, 1), (0x27, 1), (0x28, 4), (0x2C, 4), (0x30, 2), (0x32, 2), (0x34, 4),
          (0x38, 4), (0x3C, 4), (0x40, 2), (0x42, 2)]


def s32(v):
    return T.s32(v)


def key_of(x, y):
    return (x + 1) * 3 + (y + 1)


def cell_addr(world, label):
    return 0 if label <= 0 else world + 0x2D8 + label * 0x44


def add_direction(a, b):
    n, first = 0, a
    while (first & 0xFF) > 1:
        first >>= 1
        n += 1
    v = (b << n) & MASK
    return ((v >> 8) & 0xFF) if v > 0x80 else (v & 0xFF)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = {}


def stub_make_stamp(e):
    L = e.cl
    L.rec("MakeNewTimeStamp", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("MakeNewTimeStamp") & MASK)


def stub_set_stamp(e):
    L = e.cl
    L.rec("SetTimeStamp", (L.lab(e.r(3)), e.r(4) & MASK, (e.r(5) >> 16) & 0xFFFF))


def stub_gnc_delta(e):
    L = e.cl
    caller = L.lab(e.r(3))
    x, y = struct.unpack(">ii", e.read(e.r(4), 8))
    L.rec("GetNeighbouringCellD", (caller, x, y, s32(e.r(5))))
    label = 0
    if 1 <= caller <= MAX_CELLS and -1 <= x <= 1 and -1 <= y <= 1:
        label = CUR["nbr"][caller * 9 + key_of(x, y)]
    e.set_r(3, cell_addr(CUR["world"], label))


def stub_gnc_bit(e):
    L = e.cl
    caller = L.lab(e.r(3))
    d = e.r(4) & 0xFF
    L.rec("GetNeighbouringCellB", (caller, d))
    label = 0
    if 1 <= caller <= MAX_CELLS:
        for i in range(8):
            if d == 1 << i:
                label = CUR["dir"][caller * 8 + i]
    e.set_r(3, cell_addr(CUR["world"], label))


def recorder_mask(name):
    def fn(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), e.r(4) & 0xFF))
    return fn


def recorder_this(name, ret=None):
    def fn(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)),))
        if ret == "b":
            e.set_r(3, L.ret(name) & 0xFF)
        elif ret == "i":
            e.set_r(3, L.ret(name) & MASK)
    return fn


def stub_sound(e):
    L = e.cl
    L.rec("SOUND_SendEvent", tuple(s32(e.r(i)) for i in range(3, 9)))
    e.set_r(3, 0)


def stub_add_dir(e):
    L = e.cl
    a, b = e.r(3) & 0xFF, e.r(4) & 0xFF
    L.rec("ADD_DIRECTION", (a, b))
    e.set_r(3, add_direction(a, b))


STUBS.internal(MAKE_STAMP, stub_make_stamp)
STUBS.internal(SET_STAMP, stub_set_stamp)
STUBS.internal(GNC_DELTA, stub_gnc_delta)
STUBS.internal(GNC_BIT, stub_gnc_bit)
STUBS.internal(MODIFY_NB, recorder_mask("ModifyNeighbours"))
STUBS.internal(REMOVE_NB, recorder_mask("RemoveNeighbour"))
STUBS.internal(NUMBER_NB, recorder_this("NumberOfNeighbours", "i"))
STUBS.internal(IS_QUEUE, recorder_this("IsQueue", "i"))
STUBS.internal(SET_TILE, recorder_this("SetHFieldTile"))
STUBS.internal(GET_DIR, recorder_this("GetDirection", "b"))
STUBS.internal(GET_NB, recorder_this("GetNeighbours", "b"))
STUBS.internal(SOUND, stub_sound)
STUBS.internal(ADD_DIR, stub_add_dir)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides

WORLD_SIZE = 0x2D8 + (K + 2) * 0x44


def fixed_labels(world):
    labels = {world + 0x2D8: MAP_LABEL}
    for i in range(1, K + 1):
        labels[cell_addr(world, i)] = i
    return labels


class Side:
    """What the shared code needs from either memory."""

    def put(self, a, size, v):
        code = {1: "B", 2: "H", 4: "I"}[size]
        self.write(a, struct.pack(self.end + code, v & ((1 << (8 * size)) - 1)))

    def get(self, a, size):
        code = {1: "B", 2: "H", 4: "I"}[size]
        return struct.unpack(self.end + code, self.read(a, size))[0]


class Ppc(Side):
    end = ">"

    def __init__(self, e, case):
        self.e = e
        self.world = e.alloc(WORLD_SIZE)
        self.obj = e.alloc(0x40)
        CUR.clear()
        CUR.update(nbr=case["nbr"], dir=case["dir"], world=self.world)
        self.L = e.cl = CL.PpcLog(fixed_labels(self.world), case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def pointers(self):
        self.put(ADDR["world"], 4, self.world)
        self.put(ADDR["db6c"], 4, self.obj)
        for a, (x, y) in DELTAS.items():
            self.put(a, 4, x)
            self.put(a + 4, 4, y)

    def call(self, this, mode, a, b, flag):
        T.call_ppc(self.e, FN, [this & MASK, mode & MASK, a & MASK, b & MASK, flag])


class Host(Side):
    end = "<"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.world = lib.tpw_cn_alloc(WORLD_SIZE)
        self.obj = lib.tpw_cn_alloc(0x40)
        nbr = (C.c_int32 * len(case["nbr"]))(*case["nbr"])
        dirs = (C.c_int32 * len(case["dir"]))(*case["dir"])
        lib.tpw_cn_tables(nbr, dirs, self.world)
        CL.host_begin(lib, {v: k for k, v in fixed_labels(self.world).items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cn_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def pointers(self):
        self.write(self.g("world"), struct.pack("<Q", self.world))
        self.write(self.g("curobj"), struct.pack("<Q", self.obj))
        self.put(self.g("db6c"), 4, self.obj & MASK)

    def call(self, this, mode, a, b, flag):
        self.lib.tpw_cn_calc(this, s32(mode), s32(a), s32(b), flag)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cn_addr.restype, lib.tpw_cn_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cn_alloc.restype, lib.tpw_cn_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cn_tables.restype, lib.tpw_cn_tables.argtypes = None, [C.POINTER(C.c_int32), C.POINTER(C.c_int32), C.c_uint64]
    lib.tpw_cn_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cn_calc.restype = None
    lib.tpw_cn_calc.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cn_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

GLOBALS = [("prevA", 4), ("prevB", 4), ("idMinusY", 2), ("idPlusX", 2), ("idPlusY", 2), ("idMinusX", 2), ("join", 4), ("db68", 2),
           ("bank3", 4), ("recording", 4)]


def build(s, case):
    s.pointers()
    for name, size in GLOBALS:
        s.put(s.g(name), size, case["g"][name])
    s.put(s.obj + 0x32, 2, case["entry"])
    for i, (cid, mtype, parent) in enumerate(case["cells"], 1):
        a = cell_addr(s.world, i)
        s.put(a + 0, 2, cid)
        s.put(a + 8, 4, mtype)
        s.put(a + 0x10, 2, parent)


def observe(s, case):
    cells = [[s.get(cell_addr(s.world, i) + off, size) for off, size in FIELDS] for i in range(1, K + 1)]
    globs = {name: s.get(s.g(name), size) for name, size in GLOBALS}
    return {"log": s.log(), "cells": cells, "globals": globs, "entry": s.get(s.obj + 0x32, 2)}


def run(s, case):
    try:
        build(s, case)
        s.call(cell_addr(s.world, 1), case["mode"], case["a"], case["b"], case["flag"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

STRAIGHT = [(0, -1), (1, 0), (0, 1), (-1, 0)]
# Each diagonal and the two straight cells beside it (read without a NULL check when the diagonal has the mode).
DIAG = {(1, -1): [(0, -1), (1, 0)], (1, 1): [(0, 1), (1, 0)], (-1, 1): [(0, 1), (-1, 0)], (-1, -1): [(0, -1), (-1, 0)]}
BYTES = [0x10, 0x40, 1, 4, 0xFF, 0, 0x55, 0xAA, 0x11, 0x44]


def pick_byte(r):
    return r.choice(BYTES) if r.random() < 0.8 else r.randrange(256)


def make(r, mode=None, p_null=None, join=None):
    if mode is None:
        mode = r.choice([1, 1, 1, 1, 0, 0, 3, 3, 2, 4, -1, 5])
    if p_null is None:
        p_null = r.choice([0.0, 0.15, 0.3, 0.6])
    ids = [r.choice([0, 1, 2, 0x81, 0x7F, 0xFFFF, r.randrange(0x10000), r.randrange(1, 300)]) for _ in range(5)]
    g = dict(prevA=r.choice([0x80, 1, -1, 0, 0x80, r.randrange(-3, 3)]), prevB=r.choice([0x80, 1, -1, 0, r.randrange(-3, 3)]),
             idMinusY=ids[0], idPlusX=ids[1], idPlusY=ids[2], idMinusX=ids[3], db68=ids[4],
             join=r.choice([0, 1]) if join is None else join, bank3=r.choice([0, 3, r.randrange(1 << 31)]),
             recording=r.choice([0, 0, 1]))
    types = [mode, mode, mode, 1, 1, 9, 10, 3, 0, 2]
    cells = []
    for i in range(1, K + 1):
        cid = r.choice([1, 0, 0x80, 0x81, 0xFFFF, r.randrange(0x10000), r.randrange(1, 0x4001)])
        mtype = r.choice(types) if i > 1 else r.choice([3, 3, 1, 0, mode, 9])
        parent = r.choice([g["db68"], g["idMinusY"], g["idPlusX"], g["idPlusY"], g["idMinusX"], r.randrange(0x10000)])
        cells.append((cid, mtype & MASK, parent))
    nbr = [0] * ((MAX_CELLS + 1) * 9)
    dirs = [0] * ((MAX_CELLS + 1) * 8)
    for caller in range(1, K + 1):
        for x in (-1, 0, 1):
            for y in (-1, 0, 1):
                if (x, y) != (0, 0):
                    nbr[caller * 9 + key_of(x, y)] = 0 if r.random() < p_null else r.randrange(1, K + 1)
        for i in range(8):
            dirs[caller * 8 + i] = 0 if r.random() < p_null else r.randrange(1, K + 1)
    for d, beside in DIAG.items():
        if any(nbr[9 + key_of(*b)] == 0 for b in beside):
            nbr[9 + key_of(*d)] = 0
    if r.random() < 0.6:
        x, y = r.choice(STRAIGHT)
        entry = nbr[9 + key_of(x, y)] + 1
    else:
        entry = r.choice([0, 1, 2, r.randrange(0x10000), r.randrange(1, K + 3)])
    queues = {"GetNeighbours": [pick_byte(r) for _ in range(40)], "GetDirection": [pick_byte(r) for _ in range(40)],
              "NumberOfNeighbours": [r.choice([0, 1, 1, 2, 3, -1]) for _ in range(20)],
              "IsQueue": [r.choice([0, 0, 1, 1, 2]) for _ in range(60)], "MakeNewTimeStamp": [r.randrange(1 << 32)]}
    a = r.choice([1, -1, 0, 0x80, r.randrange(-3, 3), r.randrange(1 << 32)])
    b = r.choice([1, -1, 0, 0x80, r.randrange(-3, 3), r.randrange(1 << 32)])
    return {"mode": mode & MASK, "a": a & MASK, "b": b & MASK, "flag": r.choice([0, 1, 1]), "g": {k: v & MASK for k, v in g.items()},
            "entry": entry & 0xFFFF, "cells": cells, "nbr": nbr, "dir": dirs, "queues": queues}


def edges():
    """Every mode (the jump between -1 and 5), every neighbour density, both join settings, the kept pair at 0x80 and the a / b corners."""
    r = random.Random(5632)
    for mode in (-1, 0, 1, 2, 3, 4, 5):
        for p_null in (0.0, 0.3, 1.0):
            for join in (0, 1):
                yield make(r, mode, p_null, join)
    for mode in (0, 3):
        for pa, pb in ((0x80, 1), (1, 0x80), (1, -1), (-1, 1), (0x80, 0x80)):
            c = make(r, mode, 0.0, 1)
            c["g"]["prevA"], c["g"]["prevB"] = pa & MASK, pb & MASK
            yield c
        for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1)):
            c = make(r, mode, 0.0, 0)
            c["a"], c["b"] = a & MASK, b & MASK
            yield c
    for _ in range(20):
        yield make(r, 1, 0.0)


def rnd(r):
    return make(r)


TESTS = [
    FnTest("CMapCell::CalculateNeighbours", ppc, host, edge=edges, rnd=rnd),
]
