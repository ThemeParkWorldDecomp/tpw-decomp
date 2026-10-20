"""Unit w145, the map half of MapDelta.cpp (main app, "SimTheme Park"), against libtpw_unit_MapDelta.dylib (tests/unit_MapDelta.cpp),
src/MapDelta_map.cpp (with src/MapDelta.cpp for the compass offsets):
  IsXYValid 0x101012f4, GetRegionalEffectCell 0x10101640, AddRegionalEffector 0x101016cc, RemoveRegionalEffector 0x1010176c,
  ModifyRegionalEffector 0x1010180c, GetTicketBoothPos 0x101019bc, GetBusStopPos 0x10101a50, GetParkEntrancePos 0x10101ae0,
  GetCrossingParkSidePos 0x10101b74, GetCrossingBSSidePos 0x10101c0c, MakeNewTimeStamp 0x101025c4, SetTimeStamp 0x10102604,
  GetTimeStamp 0x101026a0, GetTimeStampCell 0x10102724, OPPOSITE 0x10102500, ADD_DIRECTION 0x1010254c, MAP_count_walking_distance 0x10102394,
  DeleteMapInEngine 0x1010136c, UpdateMapInEngine 0x10101460, CleanUpPark 0x10101554, CMapCell::HasBeenModifiedFrom 0x101029c8,
  CMapCell::RemoveThing 0x10102e54, MarkForLitterCollection 0x101033a0, MarkedForLitterCollection 0x10103428, RemoveLitter 0x10103540,
  GetNeighbouringCell(delta, flag) 0x101036b0, MapDelta::operator* 0x101037d0, GetNeighbouringCell(direction) 0x10103814,
  GetObjectThingId 0x10103a04

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big endian)
for the original, in the host library's memory (little endian) for the translation. The map is one block of 0x1db000 bytes (gWorldMap), the
world pointer is the block less 0x2d8. Callees outside the unit are fakes on both sides recording what they read: GAME_ASSERTMSG, TRACE,
wallhug_trivial (answers from the case's queues), the four mesh and tile members of the cells (the cell's index), PAR_KillEmitter,
SCR_KillScript and CWorldSavvy::GetWorld (the cell's index).

The world of a case: the cells' fields, the thing blocks of the rows of gThingArray (kind, previous and next link; row 0 empty or not), the six
sentinel halfwords, the turn, the time stamp counter, the balance slots of the position getters, the eight region effect records, a random
fill of the time stamps and of the region effect cells. What is compared: the answer (pointers as offsets from the map), the event log, the
cells' fields, the things' links, the time stamp words, the effect cells and records, the counter, and a hash of every other byte of the map.

No fused operations. Not tested: a thing row that holds no thing and is dereferenced (the original reads a NULL), cell ids whose time stamp
word lies outside the map block (id 0 with a sentinel other than 0).
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
SHIM_LIBRARY = "unit_MapDelta"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address: size from the MANIFEST)
SIZES = {
    0x101012F4: 80, 0x10101640: 48, 0x101016CC: 52, 0x1010176C: 52, 0x1010180C: 320, 0x101019BC: 100, 0x10101A50: 100, 0x10101AE0: 100,
    0x10101B74: 100, 0x10101C0C: 100, 0x101025C4: 20, 0x10102604: 72, 0x101026A0: 48, 0x10102724: 24, 0x10102500: 44, 0x1010254C: 80,
    0x10102394: 220, 0x1010136C: 196, 0x10101460: 196, 0x10101554: 196, 0x101029C8: 8, 0x10102E54: 356, 0x101033A0: 36, 0x10103428: 220,
    0x10103540: 324, 0x101036B0: 228, 0x101037D0: 28, 0x10103814: 444, 0x10103A04: 184,
}
OWN = set(SIZES)

# ---- the callees faked on both sides
ASSERT, TRACE, WALLHUG = 0x1000DDE8, 0x1006FCFC, 0x10137D04
CMAP_RIM, CTRACK_RIM, CMAP_RFT, CTRACK_RFT = 0x1009D07C, 0x100A4048, 0x1009CF64, 0x100A3EC0
PAR_KILL, SCR_KILL, GET_WORLD = 0x100BE1E8, 0x100D9478, 0x1014205C

# ---- the Mac's addresses of the globals; the host library's come from tpw_md_addr(name)
G_WORLD, G_WORLDMAP, G_THINGS, G_BALANCE = 0x1037EC0C, 0x1037EC08, 0x1034CBFC, 0x102B4568
ADDR = dict(gMapEmptyCellId=0x102A0A9E, gRemoveThingNoPrev=0x102A0B26, gRemoveThingNoWho=0x102A0B28, gRemoveThingNoNext=0x102A0B2A,
            gLitterNoCollector=0x102A0B50, gGetObjectThingIdEnd=0x102A0B52, gMapTimeStamp=0x1034C21C)
SENT = ["gMapEmptyCellId", "gRemoveThingNoPrev", "gRemoveThingNoWho", "gRemoveThingNoNext", "gLitterNoCollector", "gGetObjectThingIdEnd"]
HOST_LAYOUT = [0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x44, 2, 8, 0xA, 0xC, 0x208]

BLK = 0x1DB000
TURN_OFF = 0x1DA70C - 0x2D8
STAMPS, STAMP_BYTES = 0x1B0000, 0x1104
EFFECTS, EFFECT_BYTES = 0x1B1104, 0x28000
RECORDS = 0x1D9104
ROWS = [0, 1, 2, 3, 4, 5, 6, 0x27FF]
BAL_SLOTS = list(range(341, 360))
# the cell fields the cases write and read: (offset, width, key)
CELLF = [(0, 2, "id"), (2, 1, "kind"), (0x24, 2, "who"), (0x28, 4, "script"), (0x2C, 4, "lit"), (0x30, 2, "coll"), (0x32, 2, "f32"),
         (0x34, 4, "time"), (0x38, 4, "emit")]

# op: (mac address, argument kinds, result kind). Kinds: m map, c cell index, w int32, u uint32, b byte, h tagged id (HIGH halfword on the
# Mac), d a MapDelta (dx, dy); results: i int32, w uint32, b byte, v none, H hidden halfword, P pointer (offset from the map).
SPEC = {
    "isxy": (0x101012F4, "ww", "i"), "effcell": (0x10101640, "mh", "P"), "add": (0x101016CC, "mwh", "v"), "remove": (0x1010176C, "mwh", "v"),
    "modify": (0x1010180C, "mwwh", "v"), "ticket": (0x101019BC, "mw", "H"), "bus": (0x10101A50, "mw", "H"), "entrance": (0x10101AE0, "mw", "H"),
    "crossing_park": (0x10101B74, "mw", "H"), "crossing_bs": (0x10101C0C, "mw", "H"), "newstamp": (0x101025C4, "m", "w"),
    "setstamp": (0x10102604, "mwh", "v"), "getstamp": (0x101026A0, "mh", "w"), "stampcell": (0x10102724, "mww", "w"),
    "opposite": (0x10102500, "b", "b"), "adddir": (0x1010254C, "bb", "b"), "walk": (0x10102394, "hhw", "i"), "delete": (0x1010136C, "m", "v"),
    "update": (0x10101460, "m", "v"), "cleanup": (0x10101554, "m", "v"), "hasmod": (0x101029C8, "cc", "i"), "removething": (0x10102E54, "ch", "v"),
    "markfor": (0x101033A0, "ch", "v"), "marked": (0x10103428, "c", "i"), "removelitter": (0x10103540, "c", "v"),
    "neighbour_delta": (0x101036B0, "cdw", "P"), "neighbour_dir": (0x10103814, "cb", "P"), "objthing": (0x10103A04, "c", "H"),
}
LIGHT = {"isxy", "opposite", "adddir", "walk"}
NAMES = {"isxy": "CMap::IsXYValid", "effcell": "CMap::GetRegionalEffectCell", "add": "CMap::AddRegionalEffector",
         "remove": "CMap::RemoveRegionalEffector", "modify": "CMap::ModifyRegionalEffector", "ticket": "CMap::GetTicketBoothPos",
         "bus": "CMap::GetBusStopPos", "entrance": "CMap::GetParkEntrancePos", "crossing_park": "CMap::GetCrossingParkSidePos",
         "crossing_bs": "CMap::GetCrossingBSSidePos", "newstamp": "CMap::MakeNewTimeStamp", "setstamp": "CMap::SetTimeStamp",
         "getstamp": "CMap::GetTimeStamp", "stampcell": "CMap::GetTimeStampCell", "opposite": "OPPOSITE", "adddir": "ADD_DIRECTION",
         "walk": "MAP_count_walking_distance", "delete": "CMap::DeleteMapInEngine", "update": "CMap::UpdateMapInEngine",
         "cleanup": "CMap::CleanUpPark", "hasmod": "CMapCell::HasBeenModifiedFrom", "removething": "CMapCell::RemoveThing",
         "markfor": "CMapCell::MarkForLitterCollection", "marked": "CMapCell::MarkedForLitterCollection",
         "removelitter": "CMapCell::RemoveLitter", "neighbour_delta": "CMapCell::GetNeighbouringCell(delta)",
         "mul": "MapDelta::operator*", "neighbour_dir": "CMapCell::GetNeighbouringCell(direction)", "objthing": "CMapCell::GetObjectThingId"}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cell_index(e, p):
    return (p - e.md_blk) // 0x44


def track_index(e, p):
    return (p - e.md_blk - 0x110000) // 0x28


def stub_trace(e):
    fmt = e.cstr(e.r(3))
    n = min(fmt.count("%"), 3)
    e.cl.rec("TRACE", [e.r(4 + i) & MASK for i in range(n)], [fmt[:159]])


def stub_wallhug(e):
    L = e.cl
    p = e.r(3)
    L.rec("wallhug_trivial", [s32(e.r(4))] + list(e.read(p + 2, 4)))
    n = L.ret("wh_count", 0) & 0xFF
    pts = [L.ret("wh_pt", 0) & 0xFF for _ in range(2 * n)]
    e.write(p, bytes([n]))
    if pts:
        e.write(p + 6, bytes(pts))
    e.set_r(3, T.u32(L.ret("wh_ret", 0)))


STUBS.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(TRACE, stub_trace)
STUBS.internal(WALLHUG, stub_wallhug)
STUBS.internal(CMAP_RIM, lambda e: e.cl.rec("CMapCell::RemoveInstanceMesh", [cell_index(e, e.r(3))]))
STUBS.internal(CMAP_RFT, lambda e: e.cl.rec("CMapCell::RefreshHFieldTile", [cell_index(e, e.r(3))]))
STUBS.internal(CTRACK_RIM, lambda e: e.cl.rec("CTrackCell::RemoveInstanceMesh", [track_index(e, e.r(3))]))
STUBS.internal(CTRACK_RFT, lambda e: e.cl.rec("CTrackCell::RefreshHFieldTile", [track_index(e, e.r(3))]))
STUBS.internal(PAR_KILL, CL.recorder("PAR_KillEmitter", "i"))
STUBS.internal(SCR_KILL, CL.recorder("SCR_KillScript", "i"))
STUBS.internal(GET_WORLD, lambda e: e.cl.rec("GetWorld", [cell_index(e, e.r(3))]))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    byte = "big"

    def put(self, a, width, v):
        self.write(a, (v & ((1 << (8 * width)) - 1)).to_bytes(width, self.byte))

    def get(self, a, width):
        return int.from_bytes(self.read(a, width), self.byte)


class Ppc(Side):
    order = ">"
    byte = "big"

    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n, bytes(n))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.e.cl.log

    def setworld(self, blk):
        self.blk = self.e.md_blk = blk
        self.put(G_WORLD, 4, blk - 0x2D8)
        self.put(G_WORLDMAP, 4, blk)

    def set_row(self, row, address):
        self.put(G_THINGS + 0x14 * row, 4, address)

    def balance(self, slot, value):
        self.put(G_BALANCE + 8 + 4 * slot, 4, value)

    def block(self):
        return self.e.alloc(BLK, bytes(BLK))

    def invoke(self, op, args):
        addr, kinds, result = SPEC[op]
        words, hidden = [], None
        if result == "H":
            hidden = self.alloc(8)
            words.append(hidden)
        for kind, a in zip(kinds, args):
            if kind == "m":
                words.append(self.blk)
            elif kind == "c":
                words.append(self.blk + a * 0x44)
            elif kind in "wu":
                words.append(a & MASK)
            elif kind == "b":
                words.append(a & 0xFF)
            elif kind == "h":
                words.append((a & 0xFFFF) << 16)
            elif kind == "d":
                p = self.alloc(12)
                self.put(p, 4, a[0])
                self.put(p + 4, 4, a[1])
                words.append(p)
        r3, _, _ = T.call_ppc(self.e, addr, words, max_insns=200_000_000, timeout_us=900_000_000)
        r3 &= MASK
        if result == "i":
            return s32(r3)
        if result == "w":
            return r3
        if result == "b":
            return r3 & 0xFF
        if result == "H":
            return self.get(hidden, 2)
        if result == "P":
            return None if r3 == 0 else s32((r3 - self.blk) & MASK)
        return None

    def mul(self, dx, dy, k):
        out = self.alloc(12)
        p = self.alloc(12)
        self.put(p, 4, dx)
        self.put(p + 4, 4, dy)
        T.call_ppc(self.e, 0x101037D0, [out, p, k & MASK])
        return [s32(self.get(out, 4)), s32(self.get(out + 4, 4))]


class Host(Side):
    order = "<"
    byte = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_md_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_md_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setworld(self, blk):
        self.blk = blk
        self.lib.tpw_md_world(blk)

    def set_row(self, row, address):
        self.lib.tpw_md_row(row, address)

    def balance(self, slot, value):
        self.lib.tpw_md_set_balance(slot, s32(value))

    def block(self):
        return self.lib.tpw_md_block()

    def invoke(self, op, args):
        addr, kinds, result = SPEC[op]
        values = []
        for kind, a in zip(kinds, args):
            if kind == "m":
                values.append(self.blk)
            elif kind == "c":
                values.append(self.blk + a * 0x44)
            elif kind == "d":
                values += [a[0], a[1]]
            elif kind == "w":
                values.append(s32(a))
            else:
                values.append(a & MASK if kind != "b" else a & 0xFF)
        r = getattr(self.lib, "tpw_md_" + op)(*values)
        if result == "P":
            return None if r == 0 else s32((r - self.blk) & MASK)
        if result == "i":
            return s32(r)
        if result == "b":
            return r & 0xFF
        return None if result == "v" else r & MASK

    def mul(self, dx, dy, k):
        out = (C.c_int32 * 2)()
        self.lib.tpw_md_mul(dx, dy, s32(k), out)
        return list(out)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    p = "tpw_md_"
    f = getattr(lib, p + "addr")
    f.restype, f.argtypes = C.c_uint64, [C.c_char_p]
    f = getattr(lib, p + "alloc")
    f.restype, f.argtypes = C.c_uint64, [C.c_uint32]
    f = getattr(lib, p + "block")
    f.restype, f.argtypes = C.c_uint64, []
    getattr(lib, p + "world").argtypes = [C.c_uint64]
    getattr(lib, p + "row").argtypes = [C.c_int32, C.c_uint64]
    getattr(lib, p + "set_balance").argtypes = [C.c_int32, C.c_int32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "mul").argtypes = [C.c_int32, C.c_int32, C.c_int32, C.POINTER(C.c_int32)]
    types = dict(m=C.c_uint64, c=C.c_uint64, w=C.c_int32, u=C.c_uint32, b=C.c_uint32, h=C.c_uint32)
    for op, (_, kinds, result) in SPEC.items():
        f = getattr(lib, p + op)
        args = []
        for kind in kinds:
            args += [C.c_int32, C.c_int32] if kind == "d" else [types[kind]]
        f.argtypes = args
        f.restype = {"i": C.c_int32, "w": C.c_uint32, "b": C.c_uint32, "v": None, "H": C.c_uint32, "P": C.c_uint64}[result]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


DELTAS = {0x1034C284: (1, 0), 0x1034C270: (1, 1), 0x1034C25C: (0, 1), 0x1034C2AC: (0, -1), 0x1034C298: (1, -1),
          0x1034C248: (-1, 1), 0x1034C234: (-1, 0), 0x1034C220: (-1, -1)}


def build(s, c):
    """Writes the case's world through Side: the same code for both memories (offsets are the Mac's)."""
    s.blk = 0
    if s.byte == "big":
        # the compass deltas are built by __sinit_MapDelta_cpp, which the emulator does not run
        for a, (dx, dy) in DELTAS.items():
            s.write(a, struct.pack(">iiI", dx, dy, 0))
    for name in SENT:
        s.put(s.g(name), 2, c["sent"].get(name, 0))
    s.put(s.g("gMapTimeStamp"), 4, c["counter"])
    for slot in BAL_SLOTS:
        s.balance(slot, c["bal"].get(slot, 0))
    s.things = {}
    for row in ROWS:
        make = (row != 0) or c["row0"]
        if not make:
            s.set_row(row, 0)
            continue
        t = s.alloc(0x20)
        spec = c["things"].get(row, {})
        s.put(t + 2, 1, spec.get("kind", 0))
        s.put(t + 8, 2, spec.get("prev", 0))
        s.put(t + 0xA, 2, spec.get("next", 0))
        s.things[row] = t
        s.set_row(row, t)
    if c["op"] in LIGHT or c["op"] == "mul":
        s.setworld(s.alloc(0x400))
        return
    blk = s.block()
    s.setworld(blk)
    s.put(blk + TURN_OFF, 4, c["turn"])
    for i, rec in enumerate(c["recs"] or []):
        for k in range(5):
            s.put(blk + RECORDS + 12 * i + 2 * k, 2, rec[k])
        s.put(blk + RECORDS + 12 * i + 10, 1, rec[5])
    if c["stampfill"] is not None:
        r = random.Random(c["stampfill"])
        s.write(blk + STAMPS, struct.pack(s.order + "%dI" % (STAMP_BYTES // 4), *[r.getrandbits(32) for _ in range(STAMP_BYTES // 4)]))
    if c["efffill"] is not None:
        r = random.Random(c["efffill"])
        s.write(blk + EFFECTS, struct.pack(s.order + "%dH" % (EFFECT_BYTES // 2), *[r.getrandbits(16) for _ in range(EFFECT_BYTES // 2)]))
    for idx, f in c["cells"].items():
        for off, width, key in CELLF:
            s.put(blk + idx * 0x44 + off, width, f.get(key, 0))


def observe(s, c, answer):
    """Everything the call may change, read back the same way on both sides."""
    out = {"answer": answer, "log": s.log(), "counter": s.get(s.g("gMapTimeStamp"), 4),
           "things": {row: (s.get(t + 8, 2), s.get(t + 0xA, 2)) for row, t in s.things.items()}}
    if c["op"] in LIGHT or c["op"] == "mul":
        return out
    blk = s.blk
    data = bytearray(s.read(blk, BLK))
    cells = {}
    for idx in c["cells"]:
        base = idx * 0x44
        cells[idx] = [int.from_bytes(data[base + off:base + off + w], s.byte) for off, w, _ in CELLF]
        for off, w, _ in CELLF:
            data[base + off:base + off + w] = bytes(w)
    out["cells"] = cells
    out["turn"] = int.from_bytes(data[TURN_OFF:TURN_OFF + 4], s.byte)
    data[TURN_OFF:TURN_OFF + 4] = bytes(4)
    out["stamps"] = hashlib.md5(struct.pack("<%dI" % (STAMP_BYTES // 4), *struct.unpack(s.order + "%dI" % (STAMP_BYTES // 4),
                                                                                        data[STAMPS:STAMPS + STAMP_BYTES]))).hexdigest()
    data[STAMPS:STAMPS + STAMP_BYTES] = bytes(STAMP_BYTES)
    out["effects"] = hashlib.md5(struct.pack("<%dH" % (EFFECT_BYTES // 2), *struct.unpack(s.order + "%dH" % (EFFECT_BYTES // 2),
                                                                                          data[EFFECTS:EFFECTS + EFFECT_BYTES]))).hexdigest()
    data[EFFECTS:EFFECTS + EFFECT_BYTES] = bytes(EFFECT_BYTES)
    out["records"] = list(struct.unpack(s.order + "5H2B", data[RECORDS:RECORDS + 12])) if False else [
        list(struct.unpack(s.order + "5H2B", data[RECORDS + 12 * i:RECORDS + 12 * i + 12])) for i in range(8)]
    data[RECORDS:RECORDS + 0x60] = bytes(0x60)
    out["rest"] = hashlib.md5(bytes(data)).hexdigest()
    return out


def run(s, c):
    try:
        build(s, c)
        if c["op"] == "mul":
            answer = s.mul(*c["args"])
        else:
            answer = s.invoke(c["op"], c["args"])
        return observe(s, c, answer)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def world(op, args, **kw):
    c = dict(op=op, args=args, cells={}, things={}, row0=0, sent={}, turn=0, counter=0, bal={}, recs=None, stampfill=None, efffill=None,
             queues={})
    c.update(kw)
    return c


def ri(r, lo, hi):
    return r.randint(lo, hi)


def mixed(r):
    return r.choice([0, 1, 2, 0x7F, 0x80, 0x81, 0xFF, 0x100, 0x3FFF, 0x4000, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF, r.randrange(0x10000),
                     r.randrange(0x4001)])


def tid(r):
    """A map cell id of the 128 by 128 level, edges favoured."""
    return r.choice([1, 2, 0x7F, 0x80, 0x81, 0x82, 0x4000, 0x3FFF, 0x3F80, 0x3F81, 0x2040, r.randrange(1, 0x4001), r.randrange(1, 0x4001)])


def rec_random(r, radius=None):
    recs = []
    for _ in range(8):
        eff = [r.choice([0, 1, -1, 7, -7, 100, -100, 32767, -32768, r.randrange(-32768, 32768)]) & 0xFFFF for _ in range(5)]
        recs.append(eff + [r.choice([0, 1, 2, 3, 5, 9, 20]) if radius is None else radius, r.getrandbits(8)])
    return recs


# ---- IsXYValid
def isxy_edges():
    vals = [-0x80000000, -2, -1, 0, 1, 0x7E, 0x7F, 0x80, 0x81, 0x7FFFFFFF]
    return [world("isxy", [x, y]) for x in vals for y in vals]


def isxy_random(r):
    return world("isxy", [r.choice([-5, -1, 0, 1, 64, 126, 127, 128, 129, 1000, r.randrange(-300, 300)]),
                          r.choice([-5, -1, 0, 1, 64, 126, 127, 128, 129, 1000, r.randrange(-300, 300)])])


# ---- GetRegionalEffectCell
def effcell_edges():
    return [world("effcell", [0, i], efffill=1) for i in (0, 1, 2, 0x7F, 0x80, 0x81, 0x100, 0x3FFF, 0x4000, 0x4001, 0x7FFF, 0x8000, 0xFFFF)]


def effcell_random(r):
    return world("effcell", [0, mixed(r)], efffill=r.randrange(4))


# ---- Add, Remove, Modify
def modify_world(r, op, add, effector, cell, radius):
    c = world(op, [0, add, effector, cell] if op == "modify" else [0, effector, cell], efffill=r.randrange(100), recs=rec_random(r, radius))
    if op != "modify":
        pass
    return c


def modify_case(r, op, cell=None, radius=None, effector=None, add=None):
    effector = r.randrange(8) if effector is None else effector
    cell = tid(r) if cell is None else cell
    add = r.choice([0, 1, 1, 0, 2, -1, 0x100]) if add is None else add
    if radius is None:
        radius = r.choice([0, 0, 1, 2, 3, 4, 7, 10, 20, 40])
    c = modify_world(r, op, add, effector, cell, radius)
    if op != "modify":
        c["args"] = [0, effector, cell]
    return c


def modify_edges(op):
    def make():
        r = random.Random(1450 + len(op))
        out = []
        for cell in (1, 0x80, 0x7F, 0x81, 0x4000, 0x3F80, 0x2040, 0x4001, 0xFFFF, 0):
            for radius in (0, 1, 3):
                out.append(modify_case(r, op, cell, radius, add=1))
        for add in (0, 1, 2, -1, 0x80000000):
            out.append(modify_case(r, op, 0x2040, 2, add=add))
        for radius in (255, 130, 64):
            out.append(modify_case(r, op, 0x2040, radius, add=1))
        out.append(modify_case(r, op, 1, 255, add=0))
        for effector in range(8):
            out.append(modify_case(r, op, 0x2041, 4, effector=effector))
        return out
    return make


# ---- the position getters
def pos_case(op, r, which=None):
    bal = {slot: r.choice([0, 1, 5, 127, 128, -1, 0x7FFF, 0x10000, r.randrange(0, 128), r.randrange(-300, 70000)]) & MASK for slot in BAL_SLOTS}
    return world(op, [0, r.choice([0, 1, 0, 1, 2, -1, 7]) if which is None else which], bal=bal)


def pos_edges(op):
    def make():
        r = random.Random(1470 + len(op))
        return [pos_case(op, r, which) for which in (0, 1, 2, -1) for _ in range(4)]
    return make


# ---- the time stamps
def newstamp_edges():
    return [world("newstamp", [0], counter=v) for v in (0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF, 0x12345678)]


def newstamp_random(r):
    return world("newstamp", [0], counter=r.getrandbits(32))


def stamp_ids(r):
    return r.choice([1, 2, 15, 16, 17, 0x7F, 0x80, 0x81, 0x100, 0x101, 0x7FF, 0x800, 0x4000, 0x7FFF, 0x8000, 0xFFFF, r.randrange(1, 0x10000)])


def setstamp_case(r, cid=None, sentinel=None):
    cid = stamp_ids(r) if cid is None else cid
    sentinel = r.choice([0, 0, cid, 5]) if sentinel is None else sentinel
    if cid == 0 and sentinel != 0:
        sentinel = 0
    return world("setstamp", [0, r.getrandbits(32), cid], sent={"gMapEmptyCellId": sentinel}, stampfill=r.randrange(50))


def setstamp_edges():
    r = random.Random(1481)
    return [setstamp_case(r, cid, s) for cid in (0, 1, 0x80, 0x4000, 0xFFFF, 0x1234) for s in (0, cid, 5)]


def getstamp_edges():
    r = random.Random(1482)
    return [world("getstamp", [0, cid], stampfill=r.randrange(50)) for cid in (1, 0x10, 0x11, 0x80, 0x81, 0x4000, 0x7FFF, 0xFFFF)]


def stampcell_args(r):
    return r.choice([(0, 0), (32, 32), (1, 0), (0, 1), (5, 7), (0x440, 0), (0x21, 0), (31, 31), (r.randrange(33), r.randrange(33)),
                     (r.randrange(0x441), 0)])


def stampcell_edges():
    r = random.Random(1483)
    return [world("stampcell", [0, x, y], stampfill=r.randrange(50)) for x, y in ((0, 0), (1, 0), (0, 1), (32, 32), (0x440, 0), (31, 1))]


# ---- OPPOSITE, ADD_DIRECTION
def opposite_edges():
    return [world("opposite", [d]) for d in range(256)]


def adddir_edges():
    return [world("adddir", [a, b]) for a in range(256) for b in (0, 1, 2, 0x7F, 0x80, 0x81, 0xFF, 0x55)]


# ---- MAP_count_walking_distance
def walk_case(r, count=None, ret=None, from_id=None, to_id=None):
    n = r.choice([0, 1, 2, 3, 5, 12, 40, 255, 200]) if count is None else count
    ret = r.choice([0, 1, 0x70000000, 0x70000001, -1, 0x6FFFFFFF]) if ret is None else ret
    pts = [r.choice([0, 1, 127, 128, 255, r.getrandbits(8)]) for _ in range(2 * n)]
    return world("walk", [r.choice([0, 1, 0x80, 0xFFFF, mixed(r)]) if from_id is None else from_id,
                          r.choice([0, 1, 0x4000, 0xFFFF, mixed(r)]) if to_id is None else to_id, r.choice([0, 1, 2, -1, 99])],
                 queues={"wh_count": [n], "wh_pt": pts or [0], "wh_ret": [ret]})


def walk_edges():
    r = random.Random(1491)
    out = [walk_case(r, count=n, ret=ret) for n in (0, 1, 2, 3, 255) for ret in (0, 0x70000000)]
    out += [walk_case(r, 4, 0, 1, 0x4000), walk_case(r, 4, 0, 0, 0), walk_case(r, 4, 0, 0xFFFF, 0x81)]
    out += [world("walk", [1, 2, 0], queues={"wh_count": [3], "wh_pt": [10, 20, 13, 15, 2, 40], "wh_ret": [0]})]
    return out


# ---- the engine refresh walks
def litter_cell(r, **kw):
    f = dict(id=r.randrange(1, 0x4001), lit=r.choice([0, 7, 8, 1, 3]), script=r.choice([0, 5, 9]), emit=r.choice([0, 3]),
             coll=r.choice([0, 3, 4]), time=r.choice([0, 1, 50, 0x7FFFFFF0]))
    f.update(kw)
    return f


def walks_case(r, op):
    cells = {}
    for _ in range(r.randrange(0, 6)):
        idx = r.choice([0, 1, 127, 128, 0x3FFF, r.randrange(0x4000)])
        cells[idx] = litter_cell(r, id=idx + 1)
    return world(op, [0], cells=cells, turn=r.choice([0, 50, 100, 150, 1000]), sent={"gLitterNoCollector": r.choice([0, 0, 3])},
                 recs=rec_random(r), efffill=r.randrange(5))


def walks_edges(op):
    def make():
        r = random.Random(1500 + len(op))
        out = [walks_case(r, op) for _ in range(3)]
        if op == "cleanup":
            out.append(world(op, [0], cells={5: litter_cell(r, id=6, lit=7, coll=0, time=0), 6: litter_cell(r, id=7, lit=8, coll=3, time=95),
                                             7: litter_cell(r, id=8, lit=7, coll=3, time=0)}, turn=100, recs=rec_random(r), efffill=1))
        return out
    return make


# ---- HasBeenModifiedFrom
def hasmod_case(r):
    return world("hasmod", [r.randrange(3), r.randrange(3)], cells={0: dict(id=1), 1: dict(id=2), 2: dict(id=3)})


# ---- the thing list
def thing_world(r, op, args, extra_rows=()):
    return None


def removething_case(r):
    row0 = r.randrange(2)
    valid = ROWS[1:] + ([0] if row0 else [])
    pick = lambda: r.choice(ROWS[1:] + [0])
    sa, sb, sc = (r.choice([0, 0, 0] + ROWS[1:]) for _ in range(3))
    for _ in range(100):
        removed = r.choice(ROWS[1:])
        prev = r.choice(valid + [sa, sa])
        nxt = r.choice(valid + [sb, sc, 0])
        who = r.choice(valid + [sb])
        head_case = prev == sa
        if head_case and not (nxt == sb or nxt in valid):
            continue
        if not head_case and not (nxt == sc or nxt in valid) and not (prev in valid):
            continue
        if not head_case and prev not in valid:
            continue
        if not head_case and not (nxt in valid or nxt == sc):
            continue
        break
    else:
        removed, prev, nxt, sa, sb, sc, row0 = 1, 0, 0, 0, 0, 0, 0
    things = {row: dict(kind=r.randrange(5), prev=r.choice(valid + [0]), next=r.choice(valid + [0])) for row in ROWS[1:]}
    things[removed] = dict(kind=1, prev=prev, next=nxt)
    return world("removething", [0, removed], cells={0: dict(id=r.randrange(1, 0x4001), who=who)}, things=things, row0=row0,
                 sent={"gRemoveThingNoPrev": sa, "gRemoveThingNoWho": sb, "gRemoveThingNoNext": sc})


def removething_chain(r, position, length, sentinels=(0, 0, 0)):
    rows = r.sample(ROWS[1:], length)
    things = {}
    for i, row in enumerate(rows):
        things[row] = dict(kind=1, prev=rows[i - 1] if i else sentinels[0], next=rows[i + 1] if i + 1 < length else sentinels[2])
    return world("removething", [0, rows[position]], cells={0: dict(id=1, who=rows[0] if rows else 0)}, things=things,
                 sent={"gRemoveThingNoPrev": sentinels[0], "gRemoveThingNoWho": sentinels[1], "gRemoveThingNoNext": sentinels[2]})


def removething_edges():
    r = random.Random(1520)
    out = []
    for length in (1, 2, 3, 4):
        for position in range(length):
            out.append(removething_chain(r, position, length))
    out.append(removething_chain(r, 0, 2, (0, 3, 0)))
    out.append(removething_chain(r, 0, 2, (2, 0, 0)))
    out.append(removething_chain(r, 1, 3, (0, 0, 0)))
    c = removething_chain(r, 1, 3)
    c["row0"] = 1
    out.append(c)
    return out


def markfor_case(r):
    return world("markfor", [0, r.choice([0, 1, 3, 0x27FF, 0xFFFF, mixed(r)])], cells={0: dict(id=tid(r), coll=r.choice([0, 7]), time=r.choice([0, 5])),
                                                                                       1: dict(id=2)}, turn=r.choice([0, 1, 100, 0x7FFFFFFF, MASK, r.getrandbits(32)]))


def marked_case(r, collector=None, turn=None, time=None, sentinel=None):
    collector = r.choice([0, 0, 1, 3, 0x27FF, 0xFFFF]) if collector is None else collector
    turn = r.choice([0, 99, 100, 101, 1000, MASK, r.getrandbits(32)]) if turn is None else turn
    time = r.choice([turn, (turn - 99) & MASK, (turn - 100) & MASK, (turn - 101) & MASK, (turn + 1) & MASK, 0, r.getrandbits(32)]) if time is None else time
    sentinel = r.choice([0, 0, 0, collector]) if sentinel is None else sentinel
    return world("marked", [0], cells={0: dict(id=r.choice([1, 2, 0x81, 0x4000, 0, 0xFFFF, tid(r)]), coll=collector, time=time, f32=r.choice([0, 9])),
                                       1: dict(id=7, coll=5, time=3)}, turn=turn, sent={"gLitterNoCollector": sentinel})


def marked_edges():
    r = random.Random(1530)
    out = [marked_case(r, 3, 1000, 1000 - d) for d in (0, 1, 98, 99, 100, 101, 1000)]
    out += [marked_case(r, 3, 5, 1000), marked_case(r, 0, 50, 0), marked_case(r, 3, 100, 3, sentinel=3), marked_case(r, 0, 100, 3, sentinel=0)]
    out += [marked_case(r, 0xFFFF, MASK, 0), marked_case(r, 7, 0, MASK)]
    return out


def removelitter_case(r, lit=None):
    lit = r.choice([7, 8, 8, 7, 0, 1, 6, 9]) if lit is None else lit
    cid = r.choice([1, 2, 0x80, 0x81, 0x4000, 0x2041, 0, tid(r)])
    return world("removelitter", [0], cells={0: dict(id=cid, lit=lit, script=r.choice([0, 5, 77]), emit=r.choice([0, 0, 9]), coll=r.choice([0, 3]),
                                                     time=r.choice([0, 12]), f32=r.choice([0, 1]))},
                 recs=rec_random(r), efffill=r.randrange(5))


def removelitter_edges():
    r = random.Random(1540)
    out = []
    for lit in (7, 8, 0, 1, 9):
        for script in (0, 5):
            for emit in (0, 9):
                c = removelitter_case(r, lit)
                c["cells"][0].update(script=script, emit=emit, id=0x2041)
                out.append(c)
    return out


# ---- the neighbours
def neighbour_dir_case(r, direction=None, cid=None):
    direction = r.choice([1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0, 3, 0xFF, 0x81, r.getrandbits(8)]) if direction is None else direction
    cid = r.choice([1, 2, 0x7F, 0x80, 0x81, 0x4000, 0x3FFF, 0x3F80, 0x3F81, 0x2041, tid(r)]) if cid is None else cid
    return world("neighbour_dir", [0, direction], cells={0: dict(id=cid)})


def neighbour_dir_edges():
    r = random.Random(1550)
    out = [neighbour_dir_case(r, d, 0x2041) for d in (1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0, 3, 0xFF)]
    out += [neighbour_dir_case(r, d, cid) for cid in (1, 0x7F, 0x80, 0x81, 0x3F80, 0x4000) for d in (1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80)]
    return out


def neighbour_delta_case(r, dx=None, dy=None, flag=None, cid=None):
    dx = r.choice([-1, 0, 1, 2, -2, 5, 127, -128, r.randrange(-200, 200)]) if dx is None else dx
    dy = r.choice([-1, 0, 1, 2, -2, 5, 127, -128, r.randrange(-200, 200)]) if dy is None else dy
    flag = r.choice([0, 1, 1, 2, -1, 3, -3, 100]) if flag is None else flag
    cid = tid(r) if cid is None else cid
    return world("neighbour_delta", [0, (dx, dy), flag], cells={0: dict(id=cid)})


def neighbour_delta_edges():
    r = random.Random(1551)
    out = [neighbour_delta_case(r, 1, 0, 1, 0x7F), neighbour_delta_case(r, 1, 0, 1, 0x7E), neighbour_delta_case(r, 0, 1, 1, 0x3F80),
           neighbour_delta_case(r, 0, -1, 1, 1), neighbour_delta_case(r, -1, 0, 1, 1), neighbour_delta_case(r, 1, 1, 2, 0x3FFF),
           neighbour_delta_case(r, 1, 0, 2, 0x7E), neighbour_delta_case(r, 1, 0, 0, 0x7F), neighbour_delta_case(r, 1, 0, -1, 0x81),
           neighbour_delta_case(r, 2, 3, 1, 0x4000), neighbour_delta_case(r, 1, 1, 1, 0), neighbour_delta_case(r, -1, -1, 1, 0xFFFF)]
    return out


def mul_case(r):
    return world("mul", [r.choice([0, 1, -1, 5, 0x7FFFFFFF, -0x80000000, r.getrandbits(32)]) , r.choice([0, 1, -1, 3, 0x10000, r.getrandbits(32)]),
                         r.choice([0, 1, -1, 2, 0x10000, 0x7FFFFFFF, r.getrandbits(32)])])


def mul_fix(c):
    c["args"] = [s32(v) if v >= 0x80000000 else v for v in c["args"]]
    return c


# ---- GetObjectThingId
def objthing_case(r, length=None, kinds=None, sentinel=None, row0=None):
    length = r.randrange(0, 5) if length is None else length
    sentinel = r.choice([0, 0, 0, 2, 0x27FF]) if sentinel is None else sentinel
    pool = [x for x in ROWS[1:] if x != sentinel]
    rows = r.sample(pool, min(length, len(pool)))
    kinds = [r.choice([0, 1, 3, 4, 3]) for _ in rows] if kinds is None else kinds
    things = {}
    for i, row in enumerate(rows):
        things[row] = dict(kind=kinds[i], next=rows[i + 1] if i + 1 < len(rows) else sentinel)
    return world("objthing", [0], cells={0: dict(id=r.choice([1, 0x81]), who=rows[0] if rows else sentinel)}, things=things,
                 row0=r.randrange(2) if row0 is None else row0, sent={"gGetObjectThingIdEnd": sentinel})


def objthing_edges():
    r = random.Random(1560)
    out = [objthing_case(r, 0, [], 0), objthing_case(r, 0, [], 2), objthing_case(r, 1, [3], 0), objthing_case(r, 1, [1], 0),
           objthing_case(r, 3, [1, 3, 3], 0), objthing_case(r, 3, [1, 4, 0], 0), objthing_case(r, 2, [0, 3], 2, 1),
           objthing_case(r, 2, [3, 3], 0, 1), objthing_case(r, 3, [4, 1, 3], 0x27FF, 0)]
    return out


def edges_of(fn, *a):
    return lambda: fn(*a)


def case_set(maker, edge):
    return dict(edge=edge, rnd=maker)


TESTS = [
    FnTest(NAMES["isxy"], ppc, host, edge=isxy_edges, rnd=isxy_random),
    FnTest(NAMES["effcell"], ppc, host, edge=effcell_edges, rnd=effcell_random),
    FnTest(NAMES["add"], ppc, host, edge=modify_edges("add"), rnd=lambda r: modify_case(r, "add")),
    FnTest(NAMES["remove"], ppc, host, edge=modify_edges("remove"), rnd=lambda r: modify_case(r, "remove")),
    FnTest(NAMES["modify"], ppc, host, edge=modify_edges("modify"), rnd=lambda r: modify_case(r, "modify")),
    FnTest(NAMES["ticket"], ppc, host, edge=pos_edges("ticket"), rnd=lambda r: pos_case("ticket", r)),
    FnTest(NAMES["bus"], ppc, host, edge=pos_edges("bus"), rnd=lambda r: pos_case("bus", r)),
    FnTest(NAMES["entrance"], ppc, host, edge=pos_edges("entrance"), rnd=lambda r: pos_case("entrance", r)),
    FnTest(NAMES["crossing_park"], ppc, host, edge=pos_edges("crossing_park"), rnd=lambda r: pos_case("crossing_park", r)),
    FnTest(NAMES["crossing_bs"], ppc, host, edge=pos_edges("crossing_bs"), rnd=lambda r: pos_case("crossing_bs", r)),
    FnTest(NAMES["newstamp"], ppc, host, edge=newstamp_edges, rnd=newstamp_random),
    FnTest(NAMES["setstamp"], ppc, host, edge=setstamp_edges, rnd=setstamp_case),
    FnTest(NAMES["getstamp"], ppc, host, edge=getstamp_edges, rnd=lambda r: world("getstamp", [0, stamp_ids(r)], stampfill=r.randrange(50))),
    FnTest(NAMES["stampcell"], ppc, host, edge=stampcell_edges, rnd=lambda r: world("stampcell", [0, *stampcell_args(r)], stampfill=r.randrange(50))),
    FnTest(NAMES["opposite"], ppc, host, edge=opposite_edges, rnd=lambda r: world("opposite", [r.getrandbits(8)])),
    FnTest(NAMES["adddir"], ppc, host, edge=adddir_edges, rnd=lambda r: world("adddir", [r.getrandbits(8), r.getrandbits(8)])),
    FnTest(NAMES["walk"], ppc, host, edge=walk_edges, rnd=walk_case),
    FnTest(NAMES["delete"], ppc, host, edge=walks_edges("delete"), rnd=lambda r: walks_case(r, "delete")),
    FnTest(NAMES["update"], ppc, host, edge=walks_edges("update"), rnd=lambda r: walks_case(r, "update")),
    FnTest(NAMES["cleanup"], ppc, host, edge=walks_edges("cleanup"), rnd=lambda r: walks_case(r, "cleanup")),
    FnTest(NAMES["hasmod"], ppc, host, edge=lambda: [hasmod_case(random.Random(1))], rnd=hasmod_case),
    FnTest(NAMES["removething"], ppc, host, edge=removething_edges, rnd=removething_case),
    FnTest(NAMES["markfor"], ppc, host, edge=lambda: [markfor_case(random.Random(i)) for i in range(4)], rnd=markfor_case),
    FnTest(NAMES["marked"], ppc, host, edge=marked_edges, rnd=marked_case),
    FnTest(NAMES["removelitter"], ppc, host, edge=removelitter_edges, rnd=removelitter_case),
    FnTest(NAMES["neighbour_delta"], ppc, host, edge=neighbour_delta_edges, rnd=neighbour_delta_case),
    FnTest(NAMES["mul"], ppc, host, edge=lambda: [mul_fix(world("mul", [a, b, k])) for a in (0, 3, -2) for b in (0, 1, -5) for k in (0, 1, -1, 7)],
           rnd=lambda r: mul_fix(mul_case(r))),
    FnTest(NAMES["neighbour_dir"], ppc, host, edge=neighbour_dir_edges, rnd=neighbour_dir_case),
    FnTest(NAMES["objthing"], ppc, host, edge=objthing_edges, rnd=objthing_case),
]
