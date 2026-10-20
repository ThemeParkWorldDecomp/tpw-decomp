"""Unit v5982, LLCC_BuildTrackDB 0x1008ce04, against libtpw_unit_LLCC_BuildTrackDB_2.dylib (tests/unit_LLCC_BuildTrackDB_2.cpp), main app
("SimTheme Park"), src/LLCellRide_offsets_LLCC_SetTrackAnimReverse_LLCC_BuildTrackDB_2.cpp:
  LLCC_BuildTrackDB 0x1008ce04 (4,048 bytes)

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit is a fake on both sides that records its arguments (a cell by its index in the track grid) and answers from the case's queue:
CTrackCell IsTrackX IsTrack IsCorner IsCrossRoad IsStraight IsFlag IsJustTrack SetHFieldTile SetSegmentNumber GetNeighbouringCell,
CCellBase GetMapCellEquiv GetDirection GetNeighbours ModifyDirection NumberOfNeighbours, CMapCell::GetObjectThingId, CObject::GetDBInfo,
GAME_ASSERTMSG, CTRACE, BMP_FreeTrack, BMP_TrackSectionPlaceSection, FindDataNodeIndexByDisableCode, LLCR_OverRide (the pointer overload),
OPPOSITE (the nibble swap). Every queue ends in the value that stops the walk (IsTrack answers 0), so the loop always ends.

The world of a case: the first block's cell (gLLCCFirstBlockX / Y), the rotation, the direction byte, the two halfwords gLLCCE0B0 / B2, the
balance words +0x57c / +0x580 / +0x584, gGameWorld (a block of 0x1e0000 bytes with 41 rows of 128 track cells of random content around the
start and the halfword at +0x1da732), four things in gThingArray (objects with the words at +0x1c, +0x24 and the ride record), four ride anim
sets (gRideAnimSets) sharing one mesh header with random node and mini node arrays. What is compared: the answer, the event log, the node
and mini node arrays after the call.

No fused operations (the single precision products are plain fmuls; the values are small, so fctiwz does not saturate). Not tested: a
thing id whose gThingArray row is NULL on the first thing (the original reads 0x24 of it), a NULL neighbour in the three-neighbour branch
(the original reads it), ride anim indices beyond the four sets (the original reads past the table).
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCC_BuildTrackDB_2"
MASK = 0xFFFFFFFF

FN = 0x1008CE04
SIZES = {FN: 4048}
OWN = set(SIZES)

ADDR = dict(rot=0x1029DF20, fx=0x1029E02C, fy=0x1029E030, dir=0x102DE0CC, b0=0x1029E0B0, b2=0x1029E0B2,
            gw=0x1037EBFC, things=0x1034CBFC, anims=0x102C79A4, bal=0x102B4568)
BAL_OFF = (0x57C, 0x580, 0x584)
HOST_LAYOUT = [0x44, 0x46, 0x70, 0x74, 0xA0, 0x58, 0, 1, 1, 0, 4, 8, 0x10, 0x28]

CELL0 = 0x1102D8
ROWS = 41
REGION = ROWS * 128


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cidx(e, a):
    if a == 0:
        return -1000
    return (a - e.u32(ADDR["gw"]) - CELL0) // 0x28


def fake(name, addr, args, ret=None, mask=MASK):
    def fn(e):
        L = e.cl
        L.rec(name, tuple(args(e)))
        if ret is not None:
            e.set_r(3, ret(e, L, name) & mask)
    STUBS.internal(addr, fn)


def q(e, L, name):
    return L.ret(name)


fake("IsTrackX", 0x100A367C, lambda e: [cidx(e, e.r(3))], q)
fake("IsTrack", 0x100A3574, lambda e: [cidx(e, e.r(3))], q)
fake("IsCorner", 0x100A37A0, lambda e: [cidx(e, e.r(3))], q, 0xFF)
fake("IsCrossRoad", 0x100A3914, lambda e: [cidx(e, e.r(3))], q, 0xFF)
fake("IsStraight", 0x100A3868, lambda e: [cidx(e, e.r(3))], q, 0xFF)
fake("IsFlag", 0x100A2918, lambda e: [cidx(e, e.r(3)), e.r(4) & 0xFFFF], q)
fake("IsJustTrack", 0x100A3628, lambda e: [cidx(e, e.r(3))], q)
fake("SetHFieldTile", 0x100A3BAC, lambda e: [cidx(e, e.r(3))])
fake("SetSegmentNumber", 0x100A3A38, lambda e: [cidx(e, e.r(3)), T.s16(e.r(4))])
fake("GetMapCellEquiv", 0x100F49A0, lambda e: [cidx(e, e.r(3))])
fake("GetDirection", 0x10081930, lambda e: [cidx(e, e.r(3))], q, 0xFF)
fake("GetNeighbours", 0x100816E8, lambda e: [cidx(e, e.r(3))], q, 0xFF)
fake("ModifyDirection", 0x10081810, lambda e: [cidx(e, e.r(3)), e.r(4) & 0xFF])
fake("NumberOfNeighbours", 0x10081744, lambda e: [cidx(e, e.r(3))], q)
fake("GAME_ASSERTMSG", 0x1000DDE8, lambda e: [e.r(3) & 0xFF])
fake("CTRACE", 0x1006FCBC, lambda e: [e.r(3)])
fake("BMP_FreeTrack", 0x100270E0, lambda e: [s32(e.r(3))])
fake("BMP_TrackSectionPlaceSection", 0x1002EF98, lambda e: [s32(e.r(i)) for i in range(3, 10)])
fake("FindDataNodeIndexByDisableCode", 0x1020017C, lambda e: [s32(e.u32(e.r(3) + 0xF8)), s32(e.r(4)), s32(e.r(5))], q)
fake("OPPOSITE", 0x10102500, lambda e: [e.r(3) & 0xFF],
     lambda e, L, name: ((e.r(3) & 0xFF) << 4 | (e.r(3) & 0xFF) >> 4) & 0xFF)


def stub_neighbouring(e):
    """CTrackCell::GetNeighbouringCell(unsigned char, int): the queue gives a packed cell id, 0 for NULL."""
    L = e.cl
    L.rec("GetNeighbouringCell", (cidx(e, e.r(3)), e.r(4) & 0xFF, s32(e.r(5))))
    v = L.ret("GetNeighbouringCell")
    e.set_r(3, 0 if v == 0 else (e.u32(ADDR["gw"]) + CELL0 + (v - 1) * 0x28) & MASK)


def stub_thing_id(e):
    L = e.cl
    L.rec("GetObjectThingId", (cidx(e, e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetObjectThingId") & 0xFFFF))


def stub_dbinfo(e):
    L = e.cl
    L.rec("GetDBInfo", (e.u32(e.r(3) + 0x30),))
    e.set_r(3, (e.r(3) + 0x34 - 0x10) & MASK)


def stub_overRide(e):
    L = e.cl
    L.rec("LLCR_OverRide", (e.s32(e.r(4)), e.s32(e.r(5))))
    e.write(e.r(3), struct.pack(">H", L.ret("LLCR_OverRide") & 0xFFFF))


STUBS.internal(0x1012FDDC, stub_neighbouring)
STUBS.internal(0x10103A04, stub_thing_id)
STUBS.internal(0x10107CD4, stub_dbinfo)
STUBS.internal(0x1008905C, stub_overRide)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def u16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def u8(self, a, v):
        self.write(a, struct.pack("B", v & 0xFF))


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

    def world(self):
        a = self.e.alloc(0x1E0000)
        self.words(ADDR["gw"], [a])
        return a

    def off(self, a):
        return a

    def setbal(self, k, v):
        self.words(ADDR["bal"] + BAL_OFF[k], [v])

    def setthing(self, i, a):
        self.words(ADDR["things"] + i * 0x14, [a])

    def setanim(self, i, a):
        self.words(ADDR["anims"] + i * 4, [a])

    def link(self, st, mesh, header, ident):
        self.words(st + 8, [mesh])
        self.words(mesh + 4, [header])
        self.words(st + 0xF8, [ident])

    def run(self, flag):
        r3, _, _ = T.call_ppc(self.e, FN, [flag])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bt_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_bt_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def world(self):
        return self.lib.tpw_bt_world()

    def off(self, a):
        return self.lib.tpw_bt_off(a)

    def setbal(self, k, v):
        self.lib.tpw_bt_setbalance(k, s32(v))

    def setthing(self, i, a):
        self.lib.tpw_bt_setthing(i, a)

    def setanim(self, i, a):
        self.lib.tpw_bt_setanim(i, a)

    def link(self, st, mesh, header, ident):
        self.lib.tpw_bt_link(st, mesh, header, ident)

    def run(self, flag):
        return self.lib.tpw_bt_run(flag) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bt_addr.restype, lib.tpw_bt_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bt_alloc.restype, lib.tpw_bt_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bt_world.restype, lib.tpw_bt_world.argtypes = C.c_uint64, []
    lib.tpw_bt_off.restype, lib.tpw_bt_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_bt_setbalance.restype, lib.tpw_bt_setbalance.argtypes = None, [C.c_int32, C.c_int32]
    lib.tpw_bt_setthing.restype, lib.tpw_bt_setthing.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_bt_setanim.restype, lib.tpw_bt_setanim.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_bt_link.restype, lib.tpw_bt_link.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint64, C.c_uint32]
    lib.tpw_bt_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bt_run.restype, lib.tpw_bt_run.argtypes = C.c_int32, [C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_bt_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

CELL_FMT = "HBBIIBBHH22x"


def build(s, c):
    gw = s.world()
    s.words(s.g("rot"), [c["rot"]])
    s.words(s.g("fx"), [c["fx"]])
    s.words(s.g("fy"), [c["fy"]])
    s.u8(s.g("dir"), c["dir"])
    s.u16(s.g("b0"), c["b0"])
    s.u16(s.g("b2"), c["b2"])
    for k in range(3):
        s.setbal(k, c["bal"][k])
    # the cells: ROWS rows of 128, the random templates chosen per cell, the special ones laid over
    first = c["row0"] * 128 + 1
    packed = [struct.pack(s.fmt[0] + CELL_FMT, *t) for t in c["tmpl"]]
    cells = [packed[i] for i in c["cells"]]
    for ident, t in c["over"].items():
        cells[ident - first] = struct.pack(s.fmt[0] + CELL_FMT, *t)
    s.write(gw + CELL0 + (first - 1) * 0x28, b"".join(cells))
    s.u16(gw + 0x1DA732, c["w16"])
    # the things
    objs = {}
    for k in range(1, 5):
        anim, w24, infow = c["things"][k - 1]
        obj = s.alloc(0x40)
        info = s.alloc(0x40)
        s.words(obj + 0x1C, [anim])
        s.words(obj + 0x24, [w24])
        s.words(obj + 0x30, [k])
        s.words(obj + 0x34, [infow])
        objs[k] = obj
    for i in range(8):
        s.setthing(i, objs.get(i, 0))
    if c["t0"]:
        s.setthing(0, objs[1])
    # the ride anim sets
    nodes = s.alloc(4 * 0xA0)
    minis = s.alloc(8 * 0x58)
    s.words(nodes, c["nodes"])
    s.words(minis, c["minis"])
    header = s.alloc(0x100)
    s.u16(header + 0x44, c["ncount"])
    s.u16(header + 0x46, c["f46"])
    s.words(header + 0x70, [s.off(nodes)])
    s.words(header + 0x74, [s.off(minis)])
    mesh = s.alloc(0x200)
    for k in range(4):
        st = s.alloc(0x200)
        s.link(st, mesh, header, k + 1)
        s.setanim(k, st)
    s.mem = (nodes, minis)


def observe(s, c, answer):
    nodes, minis = s.mem
    return {"answer": answer, "log": s.log(), "nodes": s.rd_words(nodes, 4 * 40), "minis": s.rd_words(minis, 8 * 22)}


def run(s, c):
    try:
        build(s, c)
        return observe(s, c, s.run(c["flag"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

DIRS = [0, 1, 4, 0x10, 0x40]
BYTES = [0, 1, 4, 0x10, 0x40, 0x41, 0x14, 5, 0x50, 0x44, 0x11, 3, 0x80, 0xFF]


def rq(r, vals, n=10):
    return [r.choice(vals) for _ in range(n)]


def base(r):
    fx = r.randrange(10, 100)
    fy = r.randrange(26, 40)
    row0 = fy - 20
    first = row0 * 128 + 1
    last = first + REGION - 1
    lo = fx + r.randrange(-8, 3)
    hi = lo + r.randrange(0, 12)
    row = fy + r.choice([0, 0, 2, -2, 4, 1])
    nullable = False
    tmpl = []
    for _ in range(12):
        ident = r.choice([r.randrange(1, 65536), row * 128 + r.randrange(lo, hi + 1) + 1])
        tmpl.append((ident, 2, r.randrange(256), r.choice([0, 0, 1, 0x1234]), r.choice([0, 0xD, 0x12, 2, 5, 0xC, 0x11]),
                     r.randrange(256), r.choice(BYTES), r.randrange(65536), r.randrange(first, last + 1)))
    closed_id = (fy + 3) * 128 + fx + 1
    open_id = (fy + 4) * 128 + fx + 1
    over = {closed_id: (closed_id, 2, 0, 1, 0xD, 0, 4, 0, first + 5), open_id: (open_id, 2, 0, 0, 2, 0, 4, 0, first + 9)}
    cells = [r.randrange(len(tmpl)) for _ in range(REGION)]
    pf = r.choice([0.1, 0.5, 0.9])
    ids = []
    for _ in range(12):
        k = r.random()
        if k < 0.3:
            ids.append(r.choice([closed_id, open_id]))
        elif nullable and k < 0.5:
            ids.append(0)
        else:
            ids.append(fx + r.randrange(-6, 7) + (fy + r.randrange(-6, 7)) * 128 + 1)
    istrack = [1 if r.random() < 0.9 else 0] + [1 if r.random() < 0.85 else 0 for _ in range(r.randrange(0, 6))] + [0]
    queues = {
        "IsTrackX": [r.choice([0, 0, 1])],
        "IsTrack": istrack,
        "IsCorner": rq(r, r.choice([[0], [0, 0x41, 0x14], [0x41, 0x14, 5, 0x50, 0x11, 0], [5, 0x50]])),
        "IsCrossRoad": rq(r, r.choice([[0], [0, 1], [1]])),
        "IsStraight": rq(r, r.choice([[0x44], [0x11], [0x44, 0x11, 5, 0], [0x44, 0x11]])),
        "IsFlag": rq(r, r.choice([[0], [1], [0, 1], [1, 1, 1, 0]])),
        "IsJustTrack": rq(r, [0, 1]),
        "GetDirection": rq(r, BYTES),
        "GetNeighbours": rq(r, BYTES),
        "NumberOfNeighbours": rq(r, [0, 1, 2, 4] if nullable else [0, 1, 2, 3, 3, 3, 4]),
        "GetNeighbouringCell": ids,
        "GetObjectThingId": rq(r, [1, 2, 3, 4], 3),
        "LLCR_OverRide": rq(r, [0, 1, 2, 3, 4, 5, 6]),
        "FindDataNodeIndexByDisableCode": rq(r, [-1, 0, 1, 2, 3]),
    }
    return {
        "flag": r.choice([0, 1]), "rot": r.choice([0, 0x5A, 0xB4, 0x10E, 0x2D, 0x100]), "fx": fx, "fy": fy,
        "dir": r.choice(DIRS + [0x41, 2]), "b0": r.choice([0, 1, 2, 3]), "b2": r.choice([0xFFFF, 0, 1, 2]),
        "bal": (lo, row, hi), "row0": row0, "tmpl": tmpl, "cells": cells, "over": over, "w16": r.randrange(0, 7),
        "things": [(r.randrange(4), r.getrandbits(32), r.getrandbits(32)) for _ in range(4)], "t0": r.choice([0, 0, 1]),
        "ncount": r.choice([2, 3, 4]), "f46": r.choice([0, 1, 2]),
        "nodes": [r.getrandbits(32) for _ in range(4 * 40)], "minis": [r.getrandbits(32) for _ in range(8 * 22)],
        "queues": queues, "closed": closed_id, "open": open_id,
    }


def edge_case(r, queues=None, **over):
    c = base(r)
    c["flag"] = 0
    c["rot"] = 0
    if queues:
        c["queues"].update(queues)
    c.update(over)
    return c


def edges():
    r = __import__("random").Random(982)
    # the start cell is not a track / a track part: early answer 0, with and without IsTrackX
    for tx in (0, 1):
        for flag in (0, 1):
            yield edge_case(r, {"IsTrackX": [tx], "IsTrack": [0]}, flag=flag)
            yield edge_case(r, {"IsTrackX": [tx], "IsTrack": [1, 0]}, flag=flag)
    # the four rotations that move the start and two that do not
    for rot in (0, 0x5A, 0xB4, 0x10E, 0x2D, 0x100, 0xB5, 0x59):
        yield edge_case(r, {"IsTrack": [1, 1, 0]}, rot=rot)
    # corners
    for code in (0x41, 0x14, 5, 0x50, 0x11, 0x44, 0):
        for flag in (0, 1):
            yield edge_case(r, {"IsTrack": [1, 1, 1, 0], "IsCorner": [code], "NumberOfNeighbours": [2]}, flag=flag)
    # cross roads
    for flag in (0, 1):
        yield edge_case(r, {"IsTrack": [1, 1, 1, 0], "IsCorner": [0], "IsCrossRoad": [1], "NumberOfNeighbours": [2]}, flag=flag)
    # straight sections: both codes, flag and no flag, flagged cells in and out of the balance range
    for code in (0x44, 0x11, 5, 0):
        for flag in (0, 1):
            for fl in (0, 1):
                for inrange in (0, 1):
                    c = edge_case(r, {"IsTrack": [1, 1, 1, 1, 0], "IsCorner": [0], "IsCrossRoad": [0], "IsStraight": [code],
                                      "IsFlag": [fl], "NumberOfNeighbours": [2], "LLCR_OverRide": [1, 2, 0, 3]}, flag=flag)
                    if inrange:
                        c["bal"] = (c["fx"] - 20, c["fy"], c["fx"] + 20)
                    yield c
    # the flagged cells' ends: every combination of the four neighbour answers, and four node patterns
    for combo in range(16):
        ids = [(c_ if (combo >> k) & 1 else o_) for k, (c_, o_) in enumerate([(0, 0)] * 4)]
        for fd in ([0, 1], [-1, 2], [3, -1], [-1, -1]):
            c = edge_case(r, flag=1, dir=4, b0=0, b2=0xFFFF)
            ids = [c["closed"] if (combo >> k) & 1 else c["open"] for k in range(4)]
            c["queues"].update({"IsTrack": [1, 1, 1, 1, 0], "IsCorner": [0], "IsCrossRoad": [0], "IsStraight": [0x44], "IsFlag": [1],
                                "NumberOfNeighbours": [2], "GetNeighbouringCell": ids, "LLCR_OverRide": [1],
                                "FindDataNodeIndexByDisableCode": fd, "IsJustTrack": [combo & 1]})
            yield c
    # the same with a non-zero B2 so that the answer 0 is compared, and the world word as the answer
    for b2 in (1, 0):
        c = edge_case(r, flag=1, dir=4, b0=0, b2=b2)
        c["queues"].update({"IsTrack": [1, 1, 1, 1, 0], "IsCorner": [0], "IsCrossRoad": [0], "IsStraight": [0x44], "IsFlag": [1],
                            "NumberOfNeighbours": [2], "LLCR_OverRide": [1]})
        yield c
    # a walk that comes back to its start: east, north-south steps by the corner rule, whole track
    for flag in (0, 1):
        for g in (0x41, 0x14):
            c = edge_case(r, flag=flag, dir=1)
            c["queues"].update({"IsTrack": [1], "IsCorner": [g], "NumberOfNeighbours": [2],
                                "GetNeighbours": [0x10, 0x14, 0x40, 0x50, 1, 0x41, 4, 5],
                                "GetDirection": [0x55]})
            yield c
    # directions without a step, a walk stopped by few neighbours, and the three-neighbour branch
    for d in (0, 1, 4, 0x10, 0x40, 2, 0x41):
        for nn in ([1], [3], [2]):
            for nb_track in (0, 1):
                c = edge_case(r, dir=d, flag=0)
                c["queues"].update({"IsTrack": [1, 1, nb_track, 1, 0], "IsCorner": [0], "IsCrossRoad": [0], "IsStraight": [0],
                                    "IsFlag": [0], "NumberOfNeighbours": nn, "IsJustTrack": [1],
                                    "GetNeighbouringCell": [c["closed"], c["open"]], "GetNeighbours": [0, 1, 0x10]})
                yield c
    # unflagged cells: SetHFieldTile only when the cell has no mesh and is just track
    for jt in (0, 1):
        for mesh_cell in ("closed", "open"):
            c = edge_case(r, dir=4, flag=1)
            c["queues"].update({"IsTrack": [1, 1, 1, 0], "IsCorner": [0], "IsCrossRoad": [0], "IsStraight": [0], "IsFlag": [0],
                                "NumberOfNeighbours": [2], "IsJustTrack": [jt]})
            yield c


def rnd(r):
    return base(r)


TESTS = [
    FnTest("LLCC_BuildTrackDB", ppc, host, edge=edges, rnd=rnd),
]
