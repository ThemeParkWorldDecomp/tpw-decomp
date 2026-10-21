"""Unit w139 (main app), against libtpw_unit_MapCells.dylib (tests/unit_MapCells.cpp):

  src/game/World.cpp       CWorld::Register for CHandyman (0x1013ef14), CMechanic (0x1013f144), CEntertainer (0x1013f374), CGuard (0x1013f5ac),
                               CResearcher (0x1013f7d8) and CObject (0x1013fa10)
  src/game/park/Map.cpp             CMap::Init (0x10100a8c)
  src/game/park/MapCell.cpp         CMapCell::SetHFieldTile (0x1009cdb4)
  src/game/park/MapDelta.cpp             MapDelta::Rotate (0x10103d88)
  src/game/TPPointer.cpp GetMapCellFromPointerWorldPosn (0x10067084)
  src/game/park/TrackCell.cpp     CTrackCell::ModifyType (0x100a0828)

The eleven run as the original on the PowerPC side and as src/ on the host. Every callee outside them is a fake that records the same event on both sides
(tests/calllog.py): GetThingId, GetTile, SingleHFieldTextureChange, LLCCM_CreateQMesh and DestroyQMesh, IsXYValid, the two track cost getters, ValidTypeChange,
IsJustTrack, GetMapCellEquiv, GetNeighbouringCell, CMapCell::Delete, the map's time stamps, the bank account and the assertion.

Memory the functions own is built from VALUES (fields packed in each side's byte order over a common fill) and compared by value; every byte outside the fields
must still hold its fill. The world is never of its Mac size except for CMap::Init (about 1.9 MB): Register works on a 16 byte tail with the world pointer
shifted back to it, ModifyType needs only a world address.
"""
import ctypes as C
import hashlib
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_MapCells"

MASK = 0xFFFFFFFF
REG_ADDR = [0x1013EF14, 0x1013F144, 0x1013F374, 0x1013F5AC, 0x1013F7D8, 0x1013FA10]
REG_NAMES = ["CWorld::Register(CHandyman)", "CWorld::Register(CMechanic)", "CWorld::Register(CEntertainer)", "CWorld::Register(CGuard)",
             "CWorld::Register(CResearcher)", "CWorld::Register(CObject)"]
REG_HEAD = [4, 6, 8, 12, 10, 14]                    # the head halfword in the 16 byte tail (world 0x1da738 on)
REG_LINK = [0x20C] * 5 + [0xC]
INIT, TILE, ROTATE, POINTER, MODIFY = 0x10100A8C, 0x1009CDB4, 0x10103D88, 0x10067084, 0x100A0828
OWN = set(REG_ADDR) | {INIT, TILE, ROTATE, POINTER, MODIFY}

GET_THING_ID, GET_TILE, SINGLE_CHANGE, CREATE_Q, DESTROY_Q = 0x1012E1F8, 0x1009BC54, 0x1005B840, 0x10081C58, 0x10081D80
ASSERT, IS_XY_VALID = 0x1000DDE8, 0x101012F4
KART_COST, WATER_COST, VALID_CHANGE, IS_JUST_TRACK, EQUIV, DELETE = 0x10091C54, 0x10091CD4, 0x100A149C, 0x100A3628, 0x100F49A0, 0x1009D0F4
GET_BANK, GET_BALANCE, WITHDRAW, NEIGHBOUR, STAMP_NEW, STAMP_SET = 0x1013EDFC, 0x100F32B4, 0x100F33B0, 0x101036B0, 0x101025C4, 0x10102604

G_WORLD, G_HEADER, G_WIDTH, G_HEIGHT, G_INTRFACE = 0x1037EBFC, 0x102C6C7C, 0x102C6C68, 0x102C6C64, 0x102DC1AC
G_BALANCE, G_RECORDER, G_4824, G_4858, G_FREE = 0x102B4568, 0x1037EC44, 0x102E4824, 0x102E4858, 0x102DDB35

INIT_SIZE = 0x1D9170
SLOTS = list(range(370, 418))
MAP_BLOCK = 0x2D8            # the map sits at world + 0x2d8


def s32(v):
    return T.s32(v)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def setup(runner):
    T.install_std_stubs(runner.emu)


def lib_addr(lib, which):
    lib.tpw_mc_addr.restype = C.c_void_p
    lib.tpw_mc_addr.argtypes = [C.c_int32]
    return lib.tpw_mc_addr(which)


# ------------------------------------------------------------------ blocks built from values

def build(fill, fields, little):
    """The block: the common fill with each field (offset -> (width, value)) packed in the side's byte order."""
    buf = bytearray(fill)
    for off, (w, v) in fields.items():
        buf[off:off + w] = (v & ((1 << (8 * w)) - 1)).to_bytes(w, "little" if little else "big")
    return bytes(buf)


def decode(buf, fields, little):
    return {off: int.from_bytes(buf[off:off + w], "little" if little else "big") for off, (w, _) in fields.items()}


def rest(buf, fields):
    """The bytes outside the fields (the fields zeroed), as a hash."""
    b = bytearray(buf)
    for off, (w, _) in fields.items():
        b[off:off + w] = bytes(w)
    return hashlib.md5(bytes(b)).hexdigest()


def fill_bytes(seed, n):
    return random.Random(seed).randbytes(n)


class Base(FnTest):
    """The answers, the observed state and the call logs must agree."""

    def run_case(self, runner, case):
        runner.emu.reset_scratch()
        try:
            p = self.ppc(runner.emu, case)
        except ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = self.host(runner.lib, case)
        difference = CL.first_difference(p.pop("log"), h.pop("log"))
        errors = T.same_values(p, h, "", False)
        if difference:
            errors.append(difference)
        return errors


STUBS = CL.Stubs()
STUBS.real = set(OWN)


def run_ppc(e, labels, queues, body):
    """Runs body(L) with the unit's stubs and a fresh log; returns (the body's dict, the log)."""
    L = CL.PpcLog(labels, queues)
    e.cl = L
    saved = STUBS.activate(e)
    try:
        out = body(L)
    finally:
        CL.Stubs.deactivate(e, saved)
    return out, L.log


def run_host(lib, fixed, queues, body):
    CL.host_begin(lib, fixed, queues)
    try:
        out = body()
    finally:
        log = CL.host_end(lib)
    return out, log


# ------------------------------------------------------------------ the stubs (shared by the six functions)

def _get_thing_id(e):
    L = e.cl
    L.rec("GetThingId", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetThingId", 0) & 0xFFFF))


def _get_tile(e):
    L = e.cl
    this = e.r(3)
    L.rec("GetTile", (L.lab(this), s32(e.u32(this + 8))))
    e.write(e.r(4), struct.pack(">I", L.ret("rotation", 0) & MASK))
    e.set_r(3, L.ret("tile", 0) & MASK)


def _single_change(e):
    L = e.cl
    p = e.r(5)
    L.rec("SingleHFieldTextureChange", (s32(e.r(3)), s32(e.r(4)), L.lab(p)) + struct.unpack(">3I", e.read(p, 12)))


def _create_q(e):
    L = e.cl
    L.rec("LLCCM_CreateQMesh", (s32(e.r(3)), s32(e.r(4)), e.r(5), e.r(6), e.r(7), s32(e.r(8))))
    e.set_r(3, L.ret("mesh", 0) & MASK)


def _neighbour(e):
    L = e.cl
    dx, dy = struct.unpack(">ii", e.read(e.r(4), 8))
    L.rec("GetNeighbouringCell", (L.lab(e.r(3)), dx, dy, s32(e.r(5))))
    e.set_r(3, nbr_addr(e, L.ret("neighbour", 0)))


def nbr_addr(e, k):
    return e.nbr[k - 1] if 1 <= k <= 4 else 0


def _equiv(e):
    L = e.cl
    L.rec("GetMapCellEquiv", (L.lab(e.r(3)),))
    e.set_r(3, nbr_addr(e, L.ret("equiv", 0)))


def _bank(e):
    L = e.cl
    L.rec("GetBankAccount", (L.lab(e.r(3)),))
    e.set_r(3, e.account)


STUBS.internal(GET_THING_ID, _get_thing_id)
STUBS.internal(GET_TILE, _get_tile)
STUBS.internal(SINGLE_CHANGE, _single_change)
STUBS.internal(CREATE_Q, _create_q)
STUBS.internal(DESTROY_Q, CL.recorder("LLCCM_DestroyQMesh", "u"))
STUBS.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(IS_XY_VALID, CL.recorder("IsXYValid", "ii", ret="i"))
STUBS.internal(KART_COST, CL.recorder("GetKartTrackCost", "", ret=lambda e, L: L.ret("kart", 0)))
STUBS.internal(WATER_COST, CL.recorder("GetWaterTrackCost", "", ret=lambda e, L: L.ret("water", 0)))
STUBS.internal(VALID_CHANGE, CL.recorder("ValidTypeChange", "Li", ret=lambda e, L: L.ret("valid", 1)))
STUBS.internal(IS_JUST_TRACK, CL.recorder("IsJustTrack", "L", ret=lambda e, L: L.ret("just", 0)))
STUBS.internal(EQUIV, _equiv)
STUBS.internal(NEIGHBOUR, _neighbour)
STUBS.internal(DELETE, CL.recorder("Delete", "Lii"))
STUBS.internal(GET_BANK, _bank)
STUBS.internal(GET_BALANCE, CL.recorder("GetBalance", "L", ret=lambda e, L: L.ret("balance", 0)))
STUBS.internal(WITHDRAW, CL.recorder("MakeWithdrawal", "Lu"))
STUBS.internal(STAMP_NEW, CL.recorder("MakeNewTimeStamp", "L", ret=lambda e, L: L.ret("stamp", 0)))
STUBS.internal(STAMP_SET, CL.recorder("SetTimeStamp", "Luw"))


# ------------------------------------------------------------------ CWorld::Register

HALVES = 0x10000


class RegisterTest(Base):
    def __init__(self, kind):
        super().__init__(REG_NAMES[kind], self.ppc_case, self.host_case, edge=lambda: edges_register(kind), rnd=lambda r: rnd_register(kind, r))
        self.kind = kind

    def fields(self, c):
        return {REG_LINK[self.kind]: (2, c["link"])}

    def tail(self, c, little):
        halves = [random.Random(c["seed"]).getrandbits(16) for _ in range(8)]
        halves[REG_HEAD[self.kind] // 2] = c["old"]
        return struct.pack(("<" if little else ">") + "8H", *halves)

    def thing(self, c):
        return fill_bytes(c["seed"] + 1, 0x420)

    def ppc_case(self, e, c):
        tail = e.alloc(16, self.tail(c, False))
        thing = e.alloc(0x420, build(self.thing(c), self.fields(c), False))

        def body(L):
            T.call_ppc(e, REG_ADDR[self.kind], [(tail - 0x1DA738) & MASK, thing])
            return dict(tail=list(struct.unpack(">8H", e.read(tail, 16))), thing=decode(e.read(thing, 0x420), self.fields(c), False),
                        rest=rest(e.read(thing, 0x420), self.fields(c)))
        out, log = run_ppc(e, {thing: 1}, {"GetThingId": [c["id"]]}, body)
        out["log"] = log
        return out

    def host_case(self, lib, c):
        tail, thing = lib_addr(lib, 0), lib_addr(lib, 1)
        C.memmove(tail, self.tail(c, True), 16)
        C.memmove(thing, build(self.thing(c), self.fields(c), True), 0x420)

        def body():
            lib.tpw_mc_register.argtypes = [C.c_int32]
            lib.tpw_mc_register(self.kind)
            return dict(tail=list(struct.unpack("<8H", C.string_at(tail, 16))), thing=decode(C.string_at(thing, 0x420), self.fields(c), True),
                        rest=rest(C.string_at(thing, 0x420), self.fields(c)))
        out, log = run_host(lib, {1: thing}, {"GetThingId": [c["id"]]}, body)
        out["log"] = log
        return out


def mk_register(r, old=None, id_=None):
    pick = [0, 1, 2, 0x7FFF, 0x8000, 0xFFFF]
    return dict(seed=r.getrandbits(32), old=r.choice(pick + [r.getrandbits(16)]) if old is None else old,
                id=r.choice(pick + [r.getrandbits(16)]) if id_ is None else id_, link=r.getrandbits(16))


def edges_register(kind):
    r = random.Random(100 + kind)
    out = [mk_register(r, old=0, id_=5), mk_register(r, old=7, id_=5), mk_register(r, old=0xFFFF, id_=0), mk_register(r, old=5, id_=5),
           mk_register(r, old=0, id_=0), mk_register(r, old=1, id_=0xFFFF), mk_register(r, old=0x8000, id_=0x8001)]
    return out + [mk_register(r) for _ in range(8)]


def rnd_register(kind, r):
    return mk_register(r, old=0 if r.random() < 0.25 else None)


# ------------------------------------------------------------------ CMap::Init

RECORDS = 0x1D9104
CELL_ID_FIELD = {}


def init_fill(c):
    return bytearray(b"\xa5" * (INIT_SIZE + 0x20))


def canon(buf, big):
    """The block with the cell ids in little endian and the eight records cleared (they are decoded apart)."""
    b = bytearray(buf)
    if big:
        for i in range(0x4000):
            o = i * 0x44
            b[o:o + 2] = b[o:o + 2][::-1]
            o = 0x110000 + i * 0x28
            b[o:o + 2] = b[o:o + 2][::-1]
    b[RECORDS:RECORDS + 0x60] = bytes(0x60)
    return hashlib.md5(bytes(b)).hexdigest()


def records(buf, little):
    order = "<" if little else ">"
    return [list(struct.unpack(order + "5H2B", buf[RECORDS + 12 * i:RECORDS + 12 * i + 12])) for i in range(8)]


def init_state(buf, little):
    cells = []
    for i in (0, 1, 2, 127, 128, 129, 0x3FFF):
        cells.append(struct.unpack(("<" if little else ">") + "HB", buf[i * 0x44:i * 0x44 + 3]))
        cells.append(struct.unpack(("<" if little else ">") + "HB", buf[0x110000 + i * 0x28:0x110000 + i * 0x28 + 3]))
    return dict(hash=canon(buf[:INIT_SIZE], not little), records=records(buf, little), cells=cells, guard=bytes(buf[INIT_SIZE:]) == b"\xa5" * 0x20)


class InitTest(Base):
    def __init__(self):
        super().__init__("CMap::Init", self.ppc_case, self.host_case, edge=edges_init, rnd=rnd_init)

    def ppc_case(self, e, c):
        block = e.alloc(INIT_SIZE + 0x20, bytes(init_fill(c)))
        for slot, v in zip(SLOTS, c["words"]):
            w32(e, G_BALANCE + 8 + 4 * slot, v)

        def body(L):
            T.call_ppc(e, INIT, [block], max_insns=60_000_000, timeout_us=600_000_000)
            return init_state(e.read(block, INIT_SIZE + 0x20), False)
        out, log = run_ppc(e, {block: 2}, {}, body)
        out["log"] = log
        return out

    def host_case(self, lib, c):
        block = lib_addr(lib, 2)
        C.memmove(block, bytes(init_fill(c)), INIT_SIZE + 0x20)
        lib.tpw_mc_set_balance.argtypes = [C.c_int32, C.c_int32]
        for slot, v in zip(SLOTS, c["words"]):
            lib.tpw_mc_set_balance(slot, T.s32(v))

        def body():
            lib.tpw_mc_init()
            return init_state(C.string_at(block, INIT_SIZE + 0x20), True)
        out, log = run_host(lib, {2: block}, {}, body)
        out["log"] = log
        return out


def mk_init(r, kind):
    if kind == 0:
        words = [r.getrandbits(32) for _ in SLOTS]
    elif kind == 1:
        words = [r.choice([0, 1, 0xFF, 0x100, 0xFFFF, 0x10000, 0x12345678, 0x80000000, MASK]) for _ in SLOTS]
    else:
        words = [r.randrange(0, 300) for _ in SLOTS]
    return dict(words=words)


def edges_init():
    r = random.Random(500)
    return [mk_init(r, k) for k in (0, 1, 2, 0, 1, 2)] + [dict(words=[0] * 48), dict(words=[MASK] * 48), dict(words=[0x00010002] * 48),
                                                          dict(words=[(i + 1) * 0x01010101 & MASK for i in range(48)])]


def rnd_init(r):
    return mk_init(r, r.randrange(3))


# ------------------------------------------------------------------ CMapCell::SetHFieldTile

TILE_FIELDS = {0: (2, 0), 2: (1, 0), 4: (4, 0), 8: (4, 0), 0x14: (4, 0), 0x18: (4, 0), 0x1C: (4, 0)}
GUARD = 0x20


class TileTest(Base):
    def __init__(self):
        super().__init__("CMapCell::SetHFieldTile", self.ppc_case, self.host_case, edge=edges_tile, rnd=rnd_tile)

    def fields(self, c):
        return {0: (2, c["id"]), 2: (1, c["kind"]), 4: (4, c["mesh"]), 8: (4, c["type"]), 0x14: (4, c["t0"]), 0x18: (4, c["t1"]), 0x1C: (4, c["t2"])}

    def block(self, c, little):
        cell = build(fill_bytes(c["seed"], 0x44), self.fields(c), little)
        return b"\x5a" * GUARD + cell + b"\x5a" * GUARD

    def state(self, raw, g, little):
        cell = raw[GUARD:GUARD + 0x44]
        return dict(cell=decode(cell, self.fields(c_dummy), little), rest=rest(cell, self.fields(c_dummy)), guard=raw[:GUARD] + raw[GUARD + 0x44:] == b"\x5a" * (2 * GUARD),
                    g=g)

    def ppc_case(self, e, c):
        global c_dummy
        c_dummy = c
        block = e.alloc(0x44 + 2 * GUARD, self.block(c, False))
        cell = block + GUARD
        for i, v in enumerate(c["g"]):
            w32(e, G_4824 + 4 * i, v)

        def body(L):
            T.call_ppc(e, TILE, [cell])
            return self.state(e.read(block, 0x44 + 2 * GUARD), list(struct.unpack(">3I", e.read(G_4824, 12))), False)
        queues = {"tile": [c["tile"]], "rotation": [c["rot"] & MASK], "mesh": [c["newmesh"]]}
        out, log = run_ppc(e, {cell: 1, cell + 0x14: 3, G_4824: 4}, queues, body)
        out["log"] = log
        return out

    def host_case(self, lib, c):
        global c_dummy
        c_dummy = c
        block = lib_addr(lib, 8)
        cell = lib_addr(lib, 3)
        C.memmove(block, self.block(c, True), 0x44 + 2 * GUARD)
        g = lib_addr(lib, 7)
        C.memmove(g, struct.pack("<3I", *c["g"]), 12)

        def body():
            lib.tpw_mc_tile()
            return self.state(C.string_at(block, 0x44 + 2 * GUARD), list(struct.unpack("<3I", C.string_at(g, 12))), True)
        queues = {"tile": [c["tile"]], "rotation": [c["rot"]], "mesh": [c["newmesh"]]}
        out, log = run_host(lib, {1: cell, 3: cell + 0x14, 4: g}, queues, body)
        out["log"] = log
        return out


def mk_tile(r, **kw):
    tile = r.choice([0, 0x37, 0x20000, 0x2FFFF, 0x20001, 0x30000, 0x80010005, 0xFFFF0000, 0x00010000, r.getrandbits(32), 0x20000 | r.getrandbits(16)])
    c = dict(seed=r.getrandbits(32), id=r.choice([0, 1, 2, 3, 128, 129, 0x3FFF, 0x4000, 0xFFFF, r.getrandbits(16)]), kind=r.choice([1, 2]),
             mesh=r.choice([0, 0, 7, r.getrandbits(32)]), type=r.choice([-7, -1, 0, 1, 2, 3, 5, 7, 0x1E, 0x7FFFFFFF, 0x80000000, r.getrandbits(32)]) & MASK,
             t0=r.getrandbits(32), t1=r.getrandbits(32), t2=r.getrandbits(32), tile=tile, rot=r.choice([0, 90, 180, 270, -90, r.getrandbits(32)]),
             newmesh=r.choice([0, 5, r.getrandbits(32)]), g=[r.getrandbits(32) for _ in range(3)])
    c.update(kw)
    return c


def edges_tile():
    r = random.Random(900)
    out = []
    for t in (-1, 0, 1, 2, 5, 7, 0x80000000):
        for mesh in (0, 9):
            for tile in (0x20003, 0x40003, 0x37):
                out.append(mk_tile(r, type=t & MASK, mesh=mesh, tile=tile))
    out += [mk_tile(r, id=0), mk_tile(r, id=0, tile=0x20003, type=2), mk_tile(r, id=0xFFFF, tile=0x20003, type=2), mk_tile(r, tile=0x80010005, type=3),
            mk_tile(r, tile=0xFFFFFFFF, type=3, rot=-45 & MASK)]
    return out


def rnd_tile(r):
    return mk_tile(r)


# ------------------------------------------------------------------ MapDelta::Rotate

ROTS = [0, 90, 180, 270, -90, -180, -270, 360, 450, -450, 540, 720, -360, 45, 1, -1, 359, 1000, 2147483647, -2147483648, -2147483647, 180 + 360 * 1000,
        90 + 360 * 100000, -90 - 360 * 100000]


class RotateTest(Base):
    def __init__(self):
        super().__init__("MapDelta::Rotate", self.ppc_case, self.host_case, edge=edges_rotate, rnd=rnd_rotate)

    def ppc_case(self, e, c):
        res = e.alloc(16, b"\xa5" * 16)
        this = e.alloc(16, struct.pack(">iiI", c["x"], c["y"], 0xA5A5A5A5))

        def body(L):
            T.call_ppc(e, ROTATE, [res, this, c["rot"] & MASK])
            return dict(xy=list(struct.unpack(">ii", e.read(res, 8))), this=struct.unpack(">iiI", e.read(this, 12)))
        out, log = run_ppc(e, {}, {}, body)
        out["log"] = log
        return out

    def host_case(self, lib, c):
        o = (C.c_int32 * 2)()
        lib.tpw_mc_rotate.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_void_p]

        def body():
            lib.tpw_mc_rotate(c["x"], c["y"], c["rot"], C.addressof(o))
            return dict(xy=[o[0], o[1]], this=(c["x"], c["y"], 0xA5A5A5A5))
        out, log = run_host(lib, {}, {}, body)
        out["log"] = log
        return out


def mk_rotate(r, rot=None):
    return dict(x=r.choice([0, 1, -1, 5, -7, 2147483647, -2147483648, r.randrange(-1000, 1000), r.randrange(-2**31, 2**31)]),
                y=r.choice([0, 1, -1, 3, -9, 2147483647, -2147483648, r.randrange(-1000, 1000), r.randrange(-2**31, 2**31)]),
                rot=r.choice(ROTS) if rot is None else rot)


def edges_rotate():
    r = random.Random(1300)
    return [mk_rotate(r, rot=rot) for rot in ROTS for _ in range(2)]


def rnd_rotate(r):
    return mk_rotate(r, rot=r.choice(ROTS + [r.randrange(-2**31, 2**31), r.randrange(-3000, 3000), 90 * r.randrange(-30, 30)]))


# ------------------------------------------------------------------ GetMapCellFromPointerWorldPosn

F32 = [0.0, 1.0, -1.0, 0.5, 2.0, 8.0, 10.5, -3.25, 100.0, 1.0e-30, 1.0e30, -1.0e30, 0.1, 1.0 / 3.0, float("inf"), float("-inf"), float("nan"), 3.0e9, -3.0e9]


def f32(v):
    try:
        return struct.unpack("<f", struct.pack("<f", v))[0]
    except OverflowError:
        return float("inf") if v > 0 else float("-inf")


class PointerTest(Base):
    def __init__(self):
        super().__init__("GetMapCellFromPointerWorldPosn", self.ppc_case, self.host_case, edge=edges_pointer, rnd=rnd_pointer)

    def ppc_case(self, e, c):
        header = field = 0
        if c["mode"] >= 1:
            header = e.alloc(0x100, b"\x00" * 0x100)
        if c["mode"] == 2:
            field = e.alloc(0x30, struct.pack(">IHH4f2I2fII", 0, 0, 0, *c["f"], *c["c"], 0.0, 0.0, 0, 0))
            w32(e, header + 0x6C, field)
        pos = e.alloc(16, struct.pack(">3f", *c["pos"]))
        w32(e, G_HEADER, header)
        w32(e, G_WIDTH, c["w"] & MASK)
        w32(e, G_HEIGHT, c["h"] & MASK)
        e.write(G_INTRFACE + 0x24, struct.pack(">H", c["f24"]))

        def body(L):
            T.call_ppc(e, POINTER, [pos])
            return dict(f24=struct.unpack(">H", e.read(G_INTRFACE + 0x24, 2))[0], pos=list(struct.unpack(">3I", e.read(pos, 12))))
        try:
            out, log = run_ppc(e, {}, {"IsXYValid": [c["valid"]]}, body)
        finally:
            w32(e, G_HEADER, 0)
        out["log"] = log
        return out

    def host_case(self, lib, c):
        lib.tpw_mc_field.argtypes = [C.c_int32, C.c_float, C.c_float, C.c_float, C.c_float, C.c_uint32, C.c_uint32, C.c_int32, C.c_int32]
        lib.tpw_mc_field(c["mode"], *c["f"], *c["c"], c["w"], c["h"])
        lib.tpw_mc_set_f24.argtypes = [C.c_uint32]
        lib.tpw_mc_set_f24(c["f24"])
        lib.tpw_mc_get_f24.restype = C.c_uint32
        lib.tpw_mc_pointer.argtypes = [C.c_float] * 3

        def body():
            lib.tpw_mc_pointer(*c["pos"])
            return dict(f24=lib.tpw_mc_get_f24(), pos=list(struct.unpack("<3I", struct.pack("<3f", *c["pos"]))))
        out, log = run_host(lib, {}, {"IsXYValid": [c["valid"]]}, body)
        lib.tpw_mc_field(0, 0, 0, 0, 0, 0, 0, 0, 0)
        out["log"] = log
        return out


def quotient_unsafe(a, origin, size):
    """True when (a - origin) / size in single precision is +infinity or at least 2^31: the emulator's fctiwz answers 0x80000000 there, the processor
    (and ppc_fctiwz) 0x7fffffff, so such a case cannot run here."""
    d = f32(a - origin)
    if size == 0.0:
        return d > 0.0 and d == d
    if d != d or size != size:
        return False
    if abs(d) == float("inf"):
        return (d > 0) == (size > 0)
    q = f32(d / size)
    return q >= 2147483648.0


def safe(c):
    if c["mode"] == 2:
        if quotient_unsafe(c["pos"][0], c["f"][0], c["f"][2]):
            c["pos"][0] = c["f"][0]
        if quotient_unsafe(c["pos"][2], c["f"][1], c["f"][3]):
            c["pos"][2] = c["f"][1]
    return c


def mk_pointer(r, **kw):
    o = [f32(r.choice(F32 + [r.uniform(-50, 50)])) for _ in range(2)]
    size = [f32(r.choice([1.0, 1.0, 0.5, 2.0, 4.0, 0.25, 3.0, 1.5, 0.0, -1.0, 1.0e-30, float("nan"), r.uniform(0.1, 5)])) for _ in range(2)]
    counts = [r.choice([0, 1, 2, 5, 64, 128, 129, 200, 0x7FFFFFFF, 0x80000000, MASK]) for _ in range(2)]
    w = r.choice([0, 1, 3, 64, 128, 200, -1, 0x7FFFFFFF, r.randrange(1, 140)])
    h = r.choice([0, 1, 3, 64, 128, 200, -1, 0x7FFFFFFF, r.randrange(1, 140)])
    cell_x, cell_z = r.uniform(-4, 140), r.uniform(-4, 140)
    pos = [f32(o[0] + size[0] * cell_x) if r.random() < 0.8 else f32(r.choice(F32)), f32(r.uniform(-5, 5)),
           f32(o[1] + size[1] * cell_z) if r.random() < 0.8 else f32(r.choice(F32))]
    c = dict(mode=r.choice([2, 2, 2, 2, 2, 2, 2, 1, 0]), f=o + size, c=counts, w=w, h=h, pos=pos, valid=r.choice([0, 1, 1, 1]), f24=r.getrandbits(16))
    c.update(kw)
    return safe(c)


def edges_pointer():
    r = random.Random(1700)
    out = [mk_pointer(r, mode=0), mk_pointer(r, mode=1), mk_pointer(r, mode=2, valid=0), mk_pointer(r, mode=2, valid=1)]
    base = dict(mode=2, f=[0.0, 0.0, 1.0, 1.0], c=[128, 128], w=128, h=128, valid=1, f24=0xABCD)
    for x, z in [(0.0, 0.0), (0.5, 0.5), (127.9, 127.9), (128.0, 128.0), (-0.5, -0.5), (-1.0, 3.0), (1000.0, 1000.0), (5.0, -2.0), (3.0, 4.0), (2.9999, 3.9999),
                 (float("nan"), 1.0), (1.0, float("nan")), (float("inf"), float("-inf")), (3.0e9, -3.0e9), (-0.0, -0.0), (127.0, 0.0), (0.0, 127.0)]:
        out.append(dict(base, pos=[f32(x), 0.0, f32(z)]))
    # counts and sizes of the height field smaller or larger than the map's
    for counts, wh in [([10, 20], (128, 128)), ([200, 300], (50, 60)), ([0, 0], (128, 128)), ([MASK, MASK], (128, 128)), ([0x80000000, 0x80000000], (128, 128)),
                       ([128, 128], (0, 0)), ([128, 128], (-1, -1)), ([128, 128], (1, 1)), ([5, 128], (128, 3))]:
        for pos in ([3.0, 0.0, 4.0], [100.0, 0.0, 150.0], [-5.0, 0.0, -5.0], [20.5, 0.0, 30.5]):
            out.append(dict(base, c=counts, w=wh[0], h=wh[1], pos=[f32(v) for v in pos]))
    out.append(dict(base, f=[5.0, -5.0, 0.0, 0.0], pos=[5.0, 0.0, -5.0]))
    out.append(dict(base, f=[5.0, -5.0, 0.5, 2.0], pos=[7.0, 0.0, 3.0]))
    return [safe(dict(c, pos=list(c['pos']))) for c in out]


def rnd_pointer(r):
    return mk_pointer(r)


# ------------------------------------------------------------------ CTrackCell::ModifyType

MODES = [0, 1, 7, 0xB, 0x10, 0x19, 3, 2, 5, 0x1E, -1, 0x7FFFFFFF]
TRACK_FIELDS = {0: (2, 0), 2: (1, 0), 8: (4, 0), 0x20: (2, 0)}


class ModifyTest(Base):
    def __init__(self):
        super().__init__("CTrackCell::ModifyType", self.ppc_case, self.host_case, edge=edges_modify, rnd=rnd_modify)

    def fields(self, c):
        return {0: (2, c["id"]), 2: (1, 2), 8: (4, c["type"]), 0x20: (2, c["overlap"])}

    def block(self, c, little):
        return b"\x5a" * GUARD + build(fill_bytes(c["seed"], 0x28), self.fields(c), little) + b"\x5a" * GUARD

    def nbr_block(self, c, k, little):
        return build(fill_bytes(c["seed"] + 10 + k, 0x44), {8: (4, c["ntypes"][k])}, little)

    def state(self, raw, c, little):
        cell = raw[GUARD:GUARD + 0x28]
        return dict(cell=decode(cell, self.fields(c), little), rest=rest(cell, self.fields(c)), guard=raw[:GUARD] + raw[GUARD + 0x28:] == b"\x5a" * (2 * GUARD))

    def queues(self, c):
        q = {"kart": [c["kart"]], "water": [c["water"]], "valid": [c["valid"]], "balance": [c["balance"]], "stamp": [c["stamp"]], "just": [c["just"]],
             "equiv": [c["equiv"]], "neighbour": list(c["nbr"])}
        return q

    def ppc_case(self, e, c):
        block = e.alloc(0x28 + 2 * GUARD, self.block(c, False))
        cell = block + GUARD
        world = e.alloc(0x300, b"\xa5" * 0x300)
        e.account = e.alloc(0x20, b"\xa5" * 0x20)
        e.nbr = [e.alloc(0x44, self.nbr_block(c, k, False)) for k in range(4)]
        w32(e, G_WORLD, world)
        # The three compass offsets are made by a static constructor (the dump holds zeros): (1, 0), (1, 1) and (0, 1) as the listing copies them.
        for addr, xy in ((0x1034C284, (1, 0)), (0x1034C270, (1, 1)), (0x1034C25C, (0, 1))):
            e.write(addr, struct.pack(">ii", *xy))
        e.write(G_FREE, bytes([c["free"]]))
        w32(e, G_4858, c["flag"])
        w32(e, G_RECORDER + 0x24, c["rec"])
        labels = {cell: 1, world: 5, world + MAP_BLOCK: 4, e.account: 6}
        labels.update({a: 10 + k for k, a in enumerate(e.nbr)})

        def body(L):
            T.call_ppc(e, MODIFY, [cell, c["mode"] & MASK])
            return self.state(e.read(block, 0x28 + 2 * GUARD), c, False)
        out, log = run_ppc(e, labels, self.queues(c), body)
        out["log"] = log
        return out

    def host_case(self, lib, c):
        block, cell = lib_addr(lib, 9), lib_addr(lib, 4)
        C.memmove(block, self.block(c, True), 0x28 + 2 * GUARD)
        nbr = [lib_addr(lib, 10 + k) for k in range(4)]
        for k in range(4):
            C.memmove(nbr[k], self.nbr_block(c, k, True), 0x44)
        lib.tpw_mc_set_globals.argtypes = [C.c_int32, C.c_int32, C.c_int32]
        lib.tpw_mc_set_globals(c["free"], T.s32(c["flag"]), T.s32(c["rec"]))
        world, account = lib_addr(lib, 5), lib_addr(lib, 6)
        fixed = {1: cell, 5: world, 4: world + MAP_BLOCK, 6: account}
        fixed.update({10 + k: nbr[k] for k in range(4)})

        def body():
            lib.tpw_mc_modify.argtypes = [C.c_int32]
            lib.tpw_mc_modify(T.s32(c["mode"] & MASK))
            return self.state(C.string_at(block, 0x28 + 2 * GUARD), c, True)
        out, log = run_host(lib, fixed, self.queues(c), body)
        out["log"] = log
        return out


def mk_modify(r, **kw):
    cost = r.choice([0, 1, 100, 5000, 0x7FFFFFFF, 0x80000000, MASK, r.getrandbits(32)])
    mode = r.choice(MODES)
    c = dict(seed=r.getrandbits(32), id=r.choice([1, 2, 3, 0x4000, r.getrandbits(16)]), type=r.choice([0, 1, 2, 5, 7, 0xB, 0x10, 0x19, 3, r.getrandbits(32)]) & MASK,
             overlap=r.choice([0, 1, 5, 0x7FFF, 0xFFFF, r.getrandbits(16)]), mode=mode & MASK, kart=r.choice([0, 250, 1000, MASK]) if r.random() < 0.8 else cost,
             water=r.choice([0, 400, 2000, 0x80000000]) if r.random() < 0.8 else cost, valid=r.choice([1, 1, 1, 0, 5]),
             balance=r.choice([0, 1, -1, 100, 250, 249, 251, 1000, 999, 399, 400, 401, 2000, 0x7FFFFFFF, -0x80000000, r.randrange(-5000, 5000), -2000]),
             stamp=r.getrandbits(32), just=r.choice([0, 1, 1, 1, 7]), equiv=r.choice([0, 1, 1, 1]), nbr=[r.choice([0, 2, 3, 4, 2, 3, 4]) for _ in range(3)],
             ntypes=[r.choice([0, 1, 0, 1, 2, 5, MASK]) for _ in range(4)], free=r.choice([0, 0, 1, 2, 0xFF]), flag=r.choice([0, 1, 1, 5, MASK]), rec=r.choice([0, 0, 1, 7]))
    if r.random() < 0.5:
        c["type"] = c["mode"] if r.random() < 0.2 else c["type"]
    c.update(kw)
    return c


def edges_modify():
    r = random.Random(2100)
    out = []
    for mode in (0, 1, 7, 0xB, 0x10, 0x19, 3):
        for valid in (0, 1):
            for typ in (mode & MASK, 9):
                out.append(mk_modify(r, mode=mode, valid=valid, type=typ, free=0, rec=0, balance=1000000, kart=250, water=400))
    # the affordability edge, for the cost modes
    for mode, cost in ((0xB, 250), (0x10, 400), (3, 0)):
        for bal in (cost - 1, cost, cost + 1, 0, -1):
            for rec in (0, 1):
                out.append(mk_modify(r, mode=mode, valid=1, type=9, free=0, rec=rec, balance=bal, kart=250, water=400))
    # the whole delete chain: every neighbour present or missing, every type
    for k in range(4):
        out.append(mk_modify(r, mode=3, valid=1, type=9, just=1, flag=1, equiv=1, nbr=[2, 3, 4], ntypes=[1, 1, 1, 1], free=0, balance=100))
    for miss in range(4):
        nbr = [2, 3, 4]
        equiv = 1
        if miss == 0:
            equiv = 0
        else:
            nbr[miss - 1] = 0
        out.append(mk_modify(r, mode=3, valid=1, type=9, just=1, flag=1, equiv=equiv, nbr=nbr, ntypes=[0, 1, 0, 1], free=0, balance=100))
    for bad in range(4):
        types = [0, 1, 0, 1]
        types[bad] = 2
        out.append(mk_modify(r, mode=3, valid=1, type=9, just=1, flag=1, equiv=1, nbr=[2, 3, 4], ntypes=types, free=0, balance=100))
    out.append(mk_modify(r, mode=3, valid=1, type=9, just=1, flag=0, equiv=1, nbr=[2, 3, 4], ntypes=[0, 0, 0, 0]))
    out.append(mk_modify(r, mode=3, valid=1, type=9, just=0, flag=1, equiv=1, nbr=[2, 3, 4], ntypes=[0, 0, 0, 0]))
    out.append(mk_modify(r, mode=3, valid=1, type=9, just=1, flag=1, equiv=1, nbr=[1, 1, 1], ntypes=[0, 0, 0, 0]))
    # the free flag and the withdrawal
    for free in (0, 1):
        for mode in (0, 7, 0xB, 3):
            out.append(mk_modify(r, mode=mode, valid=1, type=9, free=free, rec=0, balance=1000, kart=250))
    return out


def rnd_modify(r):
    return mk_modify(r)


TESTS = [RegisterTest(k) for k in range(6)] + [InitTest(), TileTest(), RotateTest(), PointerTest(), ModifyTest()]
