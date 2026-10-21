"""Unit v5535, CMapCell::ModifyType and CMapCell::GetTile, against libtpw_unit_CMapCell_ModifyType.dylib (tests/unit_CMapCell_ModifyType.cpp), main app
("SimTheme Park"), src/game/park/MapCell.cpp:
  CMapCell::ModifyType 0x10099614, CMapCell::GetTile 0x1009bc54

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for the
original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside the unit
(ValidTypeChange, the path and queue costs, Delete, the bank account, the time stamps, GetObjectThingId, the assert, RecalculateBackOfQueue,
GetNeighbours, GetDirection, IsStraight, IsCorner, GetNeighbouringCell, IsPath, IsTrackX, GetTrackCellEquiv, IsBridge, OPPOSITE, ADD_DIRECTION and
rand) are fakes on both sides that record their arguments and answer from the case's queues; abs is the C library's on both sides.

The world of a case: eight map cells (ids 1 to 8) and four track cells in a world block, the globals gTrackCostFree, gLLCC47E4, the action recorder's
recording word, the no-object thing id, the first eight rows of gThingArray (marker objects), the random flag and the two tile tables with their
counts. What is compared: the answer (and the rotation out parameter), the cells' bytes, the globals the functions write and the event log.

No fused operations. Not tested: a map cell whose parent id is 0 (the original reads before the map).

A third test (unit v5L35), "the tile tables": the host's initial tile tables and counts (copied when the library loads, before a case writes them)
against the words of the program image at 0x1029d870 to 0x1029db47, read from the data section's dump (the other tests write over the emulator's
copy). One case per entry (tile, rotation, mask, pad bytes) and one per count. With only the first word of each table set, every path cell of the
host matched entry 0 of table B and drew the same piece.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CMapCell_ModifyType"
MASK = 0xFFFFFFFF

FN_MODIFY, FN_TILE = 0x10099614, 0x1009bc54
SIZES = {FN_MODIFY: 740, FN_TILE: 1108}
OWN = set(SIZES)

VALID, PATHCOST, QUEUECOST, DELETE = 0x1009b054, 0x10091b60, 0x10091bd8, 0x1009d0f4
GETBANK, GETBAL, WITHDRAW = 0x1013edfc, 0x100f32b4, 0x100f33b0
NEWSTAMP, SETSTAMP, GETTHING = 0x101025c4, 0x10102604, 0x10103a04
ASSERT, RECALC = 0x1000dde8, 0x1010952c
GETNB, GETDIR, ISSTRAIGHT, ISCORNER = 0x100816e8, 0x10081930, 0x1009c8c4, 0x1009c7fc
GETNBCELL, ISPATH, ISTRACKX, GETTRACK, ISBRIDGE = 0x10103814, 0x1009c44c, 0x1009ca44, 0x100f48c4, 0x100a34c4
OPPOSITE, ADDDIR = 0x10102500, 0x1010254c

ADDR = dict(world=0x1037ebfc, free=0x102ddb35, g47e4=0x102e47e4, rec=0x1037ec44 + 0x24, nothing=0x1029e212, things=0x1034cbfc,
            flag=0x102e4830, tabA=0x1029d870, cntA=0x1029d8f4, tabB=0x1029d8f8, cntB=0x1029db44)
CAP_A, CAP_B = 11, 49
BANK = 0xAB1000
HOST_LAYOUT = [0x44, 0, 8, 0xC, 0xD, 0x10, 0x20, 0x28, 0x10, 12, 4, 8, 0x40, 0x2D8, 0x1102D8]
NCELLS, NTRACKS = 8, 4
MAPOFF, TRACKOFF = 0x2D8, 0x1102D8
WORLD_PPC = [0]


def s32(v):
    return T.s32(v)


def rotl8(v, n):
    v &= 0xFF
    return ((v << n) | (v >> (8 - n))) & 0xFF


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def world(e):
    return e.u32(ADDR["world"])


def rel(e, p):
    return s32((p - world(e)) & MASK)


def cell_id(e, p):
    return struct.unpack(">H", e.read(p, 2))[0]


def mk(name, addr, fn):
    STUBS.internal(addr, fn)


def st_valid(e):
    e.cl.rec("ValidTypeChange", (cell_id(e, e.r(3)), s32(e.r(4))))
    e.set_r(3, e.cl.ret("ValidTypeChange") & MASK)


def st_pathcost(e):
    e.cl.rec("PathCost", ())
    e.set_r(3, e.cl.ret("PathCost") & MASK)


def st_queuecost(e):
    e.cl.rec("QueueCost", ())
    e.set_r(3, e.cl.ret("QueueCost") & MASK)


def st_delete(e):
    e.cl.rec("Delete", (cell_id(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), e.u32(ADDR["g47e4"])))


def st_getbank(e):
    e.cl.rec("GetBankAccount", (rel(e, e.r(3)),))
    e.set_r(3, BANK)


def st_getbal(e):
    e.cl.rec("GetBalance", (e.r(3),))
    e.set_r(3, e.cl.ret("GetBalance") & MASK)


def st_withdraw(e):
    e.cl.rec("MakeWithdrawal", (e.r(3), e.r(4)))


def st_newstamp(e):
    e.cl.rec("MakeNewTimeStamp", (rel(e, e.r(3)),))
    e.set_r(3, e.cl.ret("MakeNewTimeStamp") & MASK)


def st_setstamp(e):
    e.cl.rec("SetTimeStamp", (rel(e, e.r(3)), e.r(4), (e.r(5) >> 16) & 0xFFFF))


def st_getthing(e):
    e.cl.rec("GetObjectThingId", (cell_id(e, e.r(4)),))
    e.write(e.r(3), struct.pack(">H", e.cl.ret("GetObjectThingId") & 0xFFFF))


def st_assert(e):
    text = e.cstr(e.r(4))
    data = text.encode() if isinstance(text, str) else bytes(text)
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)), len(data), sum(data)))


def st_recalc(e):
    p = e.r(3)
    e.cl.rec("RecalculateBackOfQueue", (-1 if p == 0 else struct.unpack(">I", e.read(p, 4))[0],))


def simple_cell(name, addr, bits=32):
    def fn(e):
        e.cl.rec(name, (cell_id(e, e.r(3)),))
        v = e.cl.ret(name) & MASK
        e.set_r(3, v & 0xFF if bits == 8 else v)
    mk(name, addr, fn)


def st_getnbcell(e):
    e.cl.rec("GetNeighbouringCell", (cell_id(e, e.r(3)), e.r(4) & 0xFF))
    k = e.cl.ret("GetNeighbouringCell")
    e.set_r(3, 0 if k == 0 else (world(e) + MAPOFF + (k - 1) * 0x44) & MASK)


def st_gettrack(e):
    e.cl.rec("GetTrackCellEquiv", (cell_id(e, e.r(3)),))
    e.set_r(3, (world(e) + TRACKOFF + e.cl.ret("GetTrackCellEquiv") * 0x28) & MASK)


def st_isbridge(e):
    e.cl.rec("IsBridge", (rel(e, e.r(3)),))
    e.set_r(3, e.cl.ret("IsBridge") & MASK)


def st_opposite(e):
    d = e.r(3) & 0xFF
    e.cl.rec("OPPOSITE", (d,))
    e.set_r(3, rotl8(d, 4))


def st_adddir(e):
    a, b = e.r(3) & 0xFF, e.r(4) & 0xFF
    e.cl.rec("ADD_DIRECTION", (a, b))
    e.set_r(3, rotl8(a, 2))


def st_abs(e):
    e.set_r(3, abs(s32(e.r(3))) & MASK)


def st_rand(e):
    e.cl.rec("rand", ())
    e.set_r(3, e.cl.ret("rand") & MASK)


STUBS.internal(VALID, st_valid)
STUBS.internal(PATHCOST, st_pathcost)
STUBS.internal(QUEUECOST, st_queuecost)
STUBS.internal(DELETE, st_delete)
STUBS.internal(GETBANK, st_getbank)
STUBS.internal(GETBAL, st_getbal)
STUBS.internal(WITHDRAW, st_withdraw)
STUBS.internal(NEWSTAMP, st_newstamp)
STUBS.internal(SETSTAMP, st_setstamp)
STUBS.internal(GETTHING, st_getthing)
STUBS.internal(ASSERT, st_assert)
STUBS.internal(RECALC, st_recalc)
simple_cell("GetNeighbours", GETNB, 8)
simple_cell("GetDirection", GETDIR, 8)
simple_cell("IsStraight", ISSTRAIGHT, 8)
simple_cell("IsCorner", ISCORNER, 8)
simple_cell("IsPath", ISPATH)
simple_cell("IsTrackX", ISTRACKX)
STUBS.internal(GETNBCELL, st_getnbcell)
STUBS.internal(GETTRACK, st_gettrack)
STUBS.internal(ISBRIDGE, st_isbridge)
STUBS.internal(OPPOSITE, st_opposite)
STUBS.internal(ADDDIR, st_adddir)
STUBS.imp("abs", st_abs)
STUBS.imp("rand", st_rand)


IMAGE = {}


def read_image(emu):
    """The tables' bytes of the program image (the data section's dump; tables of numbers, nothing relocated)."""
    data = next(b for b in emu.blocks if b["initialized"] and not b["execute"] and b["name"] != "IMPORTS")
    blob = (emu.dir / data["file"]).read_bytes()
    lo = ADDR["tabA"]
    IMAGE["bytes"] = blob[lo - data["start"]:ADDR["cntB"] + 4 - data["start"]]


def setup(runner):
    T.install_std_stubs(runner.emu)
    WORLD_PPC[0] = T.persist_alloc(runner.emu, 0x112000)
    read_image(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def put(self, a, fmt, v):
        self.write(a, struct.pack(self.fmt[0] + fmt, v & {"B": 0xFF, "H": 0xFFFF, "I": MASK}[fmt]))

    def rd_word(self, a):
        return struct.unpack(self.fmt[0] + "I", self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        self.world = WORLD_PPC[0]

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

    def put_world(self):
        self.put(ADDR["world"], "I", self.world)

    def set_thing(self, i, present, marker):
        p = 0
        if present:
            p = self.e.alloc(8)
            self.put(p, "I", marker)
        self.put(ADDR["things"] + i * 0x14, "I", p)

    def call_modify(self, cell, mode):
        return T.call_ppc(self.e, FN_MODIFY, [cell & MASK, mode & MASK])[0] & MASK

    def call_tile(self, cell, out):
        return T.call_ppc(self.e, FN_TILE, [cell & MASK, out & MASK])[0] & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.world = lib.tpw_cm_world()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cm_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def put_world(self):
        pass

    def set_thing(self, i, present, marker):
        self.lib.tpw_cm_setthing(i, 1 if present else 0, marker)

    def call_modify(self, cell, mode):
        return self.lib.tpw_cm_modify(cell, s32(mode)) & MASK

    def call_tile(self, cell, out):
        return self.lib.tpw_cm_gettile(cell, out) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cm_addr.restype, lib.tpw_cm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cm_alloc.restype, lib.tpw_cm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cm_world.restype, lib.tpw_cm_world.argtypes = C.c_uint64, []
    lib.tpw_cm_setthing.restype, lib.tpw_cm_setthing.argtypes = None, [C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_cm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cm_modify.restype, lib.tpw_cm_modify.argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    lib.tpw_cm_gettile.restype, lib.tpw_cm_gettile.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.put_world()
    w = s.world
    s.write(w, bytes(MAPOFF + NCELLS * 0x44 + 0x10))
    s.write(w + TRACKOFF, bytes(NTRACKS * 0x28 + 0x10))
    for k, c in enumerate(case["cells"]):
        a = w + MAPOFF + k * 0x44
        s.put(a, "H", k + 1)
        s.put(a + 8, "I", c["type"])
        s.put(a + 0xC, "B", c["nb"])
        s.put(a + 0xD, "B", c["dir"])
        s.put(a + 0x10, "H", c["parent"])
        s.put(a + 0x20, "H", c["counter"])
    for k, t in enumerate(case["tracks"]):
        s.put(w + TRACKOFF + k * 0x28 + 0x10, "H", t)
    s.put(s.g("free"), "B", case["free"])
    s.put(s.g("g47e4"), "I", case["g47e4"])
    s.put(s.g("rec"), "I", case["rec"])
    s.put(s.g("nothing"), "H", case["nothing"])
    s.put(s.g("flag"), "B", case["flag"])
    for i in range(8):
        s.set_thing(i, case["things"][i], 0x1000 + i)
    for name, cap, cname in (("tabA", CAP_A, "cntA"), ("tabB", CAP_B, "cntB")):
        base = s.g(name)
        for i, (tile, rot, mask) in enumerate(case[name]):
            s.write(base + i * 12, struct.pack(s.fmt[0] + "IIB3x", tile & MASK, rot & MASK, mask))
        s.put(s.g(cname), "I", case[cname])


def execute(s, case):
    cell = s.world + MAPOFF + case["this"] * 0x44
    if case["op"] == "modify":
        return s.call_modify(cell, case["mode"]), None
    out = s.alloc(4)
    s.put(out, "I", 0x5A5A5A5A)
    answer = s.call_tile(cell, out)
    return answer, s.rd_word(out)


def observe(s, case, res):
    answer, out = res
    w = s.world
    return {"answer": answer, "out": out, "log": s.log(), "cells": [struct.unpack(s.fmt[0] + "HBBIIBBHH14sH34s", s.read(w + MAPOFF + k * 0x44, 0x44)) for k in range(NCELLS)],
            "g47e4": s.rd_word(s.g("g47e4")), "flag": s.read(s.g("flag"), 1), "free": s.read(s.g("free"), 1)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

MASKS = [0, 1, 4, 5, 0x10, 0x14, 0x15, 0x40, 0x41, 0x44, 0x50, 0x51, 0x54, 0x55, 0x11, 0x45, 0x04, 0xFF, 0x80, 0xAA]
DIRS = [1, 4, 0x10, 0x40, 2, 0x20, 0x80, 0, 0xFF]
MODES_TILE = [0, 1, 2, 3, -3, 4, 5, -5, 6, 7, 8, 9, 10, 11, 0x15, 0x1E, -7, 0x19, 12, 0x100, -2, -1]


def base(r, op):
    cells = []
    for k in range(NCELLS):
        cells.append(dict(type=r.choice([0, 1, 3, 2, 4, 9, 10, 5, 7, 3, 4, 9, 10]), nb=r.choice(MASKS), dir=r.choice(DIRS),
                          parent=r.randrange(1, 9), counter=r.choice([0, 1, -1, 0x7FFF, r.randrange(-100, 100)]) & 0xFFFF))
    case = dict(op=op, cells=cells, tracks=[r.randrange(0, 6) for _ in range(NTRACKS)], this=r.randrange(0, 4),
                free=r.choice([0, 0, 1, 0xFF]), g47e4=r.choice([0, 0, 1, 7]), rec=r.choice([0, 0, 1, 0x100, MASK]),
                nothing=r.choice([0, 5, 0xFFFF, 3]), flag=r.choice([0, 1, 1, 0xFF]), things=[r.random() < 0.7 for _ in range(8)])
    for name, cap, cname in (("tabA", CAP_A, "cntA"), ("tabB", CAP_B, "cntB")):
        ent = []
        for _ in range(cap):
            kind = r.choice([0, 1, 1, 2, 2, 2, 3, 0xFFFF])
            idx = r.choice([0, 2, 2, 10, 10, 3, 0xFFFF, r.randrange(0, 0x10000)])
            tile = ((kind << 16) | idx) if r.random() > 0.05 else MASK
            ent.append((tile, r.choice([0, 90, 180, 270, -90, r.randrange(-2 ** 31, 2 ** 31)]), r.choice(MASKS)))
        case[name] = ent
        case[cname] = r.choice([cap, cap, 0, 1, -1, r.randrange(0, cap + 1)]) & MASK
    return case


def modify_queues(r, valid=1, cost_p=None, cost_q=None, delta=None, thing=None):
    pc = r.choice([0, 100, 5000, 0x7FFFFFFF, -5, r.randrange(-1000, 100000)]) if cost_p is None else cost_p
    qc = r.choice([0, 100, 5000, 0x7FFFFFFF, -5, r.randrange(-1000, 100000)]) if cost_q is None else cost_q
    bal = (pc if r.random() < 0.5 else qc) + (r.choice([-1, 0, 1]) if delta is None else delta)
    if r.random() < 0.15:
        bal = r.randrange(-2 ** 31, 2 ** 31)
    return {"ValidTypeChange": [valid], "PathCost": [pc], "QueueCost": [qc], "GetBalance": [s32(bal)],
            "MakeNewTimeStamp": [r.randrange(0, 2 ** 32)], "GetObjectThingId": [r.randrange(0, 8) if thing is None else thing]}


def modify_case(r, mode=None, ctype=None, valid=None, free=None, rec=None, delta=None, ptype=None):
    c = base(r, "modify")
    me = c["cells"][c["this"]]
    if mode is None:
        mode = r.choice([0, 1, 3, 2, 5, 7, 0x19, me["type"]])
    if ctype is not None:
        me["type"] = ctype
    elif r.random() < 0.3:
        me["type"] = 3
        mode = 1 if r.random() < 0.7 else mode
    c["mode"] = mode
    if free is not None:
        c["free"] = free
    if rec is not None:
        c["rec"] = rec
    parent = c["cells"][me["parent"] - 1]
    if ptype is not None:
        parent["type"] = ptype
    c["queues"] = modify_queues(r, 1 if (valid is None and r.random() < 0.85) or valid == 1 else 0, delta=delta,
                                thing=r.choice([c["nothing"], r.randrange(0, 8), 0]))
    return c


def modify_edges():
    r = random.Random(5535)
    for mode in (0, 1, 3, 2):
        for ctype in (0, 1, 3, 2, mode):
            for free in (0, 1):
                for rec in (0, 1):
                    for valid in (1, 0):
                        for delta in (-1, 0, 1):
                            yield modify_case(r, mode, ctype, valid, free, rec, delta)
    for ptype in (4, 9, 10, 5):
        for free in (0, 1):
            for delta in (0, -1):
                yield modify_case(r, 1, 3, 1, free, 0, delta, ptype)
    for ptype in (4, 9, 10):
        for _ in range(8):
            yield modify_case(r, 1, 3, 1, 0, 1, 0, ptype)


def modify_random(r):
    return modify_case(r)


def tile_case(r, mode=None):
    c = base(r, "tile")
    me = c["cells"][c["this"]]
    me["type"] = (r.choice(MODES_TILE) if mode is None else mode) & MASK
    nbq = [r.choice(MASKS) for _ in range(r.randrange(1, 13))]
    if r.random() < 0.5:
        nbq[0] = r.choice([e[2] for e in c["tabA"] + c["tabB"]])
    c["queues"] = {
        "GetNeighbours": nbq,
        "GetDirection": [r.choice(DIRS) for _ in range(2)],
        "IsStraight": [r.choice([0, 1, 5, 0x100])],
        "IsCorner": [r.choice([0, 1, 5, 0x100])],
        "GetNeighbouringCell": [r.randrange(0, 9) for _ in range(4)],
        "IsPath": [r.choice([0, 1, 1, 0x100])],
        "IsTrackX": [r.choice([0, 1])],
        "GetTrackCellEquiv": [r.randrange(0, NTRACKS) for _ in range(4)],
        "IsBridge": [r.choice([0, 1, 0x100])],
        "rand": [r.choice([0, 1, 2, 3, -1, -2, -3, 0x7FFF, r.randrange(0, 2 ** 31), -r.randrange(1, 2 ** 31)])],
    }
    if r.random() < 0.6:
        # steer to the path branch: a mode 3 / other tile of kind 2 whose neighbours match
        c["tabB"][0] = (0x20000 | r.choice([0, 1, 5]), 0, c["queues"]["GetNeighbours"][0])
        c["tabA"][0] = (0x20000 | r.choice([0, 1, 5]), 0, c["queues"]["GetNeighbours"][0])
        c["cntA"] = c["cntB"] = r.choice([1, 5, 11])
    return c


def tile_edges():
    r = random.Random(1009)
    for mode in MODES_TILE:
        for _ in range(8):
            yield tile_case(r, mode)
    for d, n in ((0x40, 0x50), (0x10, 0x14), (1, 0x41), (4, 5), (4, 0x50), (1, 5)):
        for _ in range(3):
            c = tile_case(r, 3)
            c["queues"]["IsCorner"] = [1]
            c["queues"]["GetDirection"] = [r.choice(DIRS), d]
            c["queues"]["GetNeighbours"] = [n, n, n]
            c["tabA"][0] = (0x10002, 0, n)
            c["cntA"] = 1
            yield c


def tile_random(r):
    return tile_case(r)


def table_entry(raw, fmt, i):
    tile, rot, mask = struct.unpack_from(fmt + "IiB", raw, i * 12)
    return [tile, rot, mask, list(raw[i * 12 + 9:i * 12 + 12])]


def table_ppc(e, case):
    raw = IMAGE["bytes"]
    if case["count"]:
        return {"count": s32(struct.unpack_from(">I", raw, case["at"] - ADDR["tabA"])[0])}
    return {"entry": table_entry(raw[case["at"] - ADDR["tabA"]:], ">", case["i"])}


def table_host(lib, case):
    bind(lib)
    if case["count"]:
        name = {ADDR["cntA"]: b"initCntA", ADDR["cntB"]: b"initCntB"}[case["at"]]
        return {"count": s32(struct.unpack("<I", C.string_at(lib.tpw_cm_addr(name), 4))[0])}
    name = {ADDR["tabA"]: b"initA", ADDR["tabB"]: b"initB"}[case["at"]]
    return {"entry": table_entry(C.string_at(lib.tpw_cm_addr(name), (case["i"] + 1) * 12), "<", case["i"])}


def table_edges():
    for at, cap in ((ADDR["tabA"], CAP_A), (ADDR["tabB"], CAP_B)):
        for i in range(cap):
            yield dict(at=at, i=i, count=False)
    for at in (ADDR["cntA"], ADDR["cntB"]):
        yield dict(at=at, i=0, count=True)


def table_random(r):
    at, cap = r.choice(((ADDR["tabA"], CAP_A), (ADDR["tabB"], CAP_B)))
    return dict(at=at, i=r.randrange(cap), count=False)


TESTS = [
    FnTest("CMapCell::ModifyType", ppc, host, edge=modify_edges, rnd=modify_random),
    FnTest("CMapCell::GetTile", ppc, host, edge=tile_edges, rnd=tile_random),
    FnTest("the tile tables", table_ppc, table_host, edge=table_edges, rnd=table_random),
]
