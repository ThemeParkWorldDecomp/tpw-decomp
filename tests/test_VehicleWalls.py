"""Unit w218, against libtpw_unit_VehicleWalls.dylib (tests/unit_VehicleWalls.cpp), main app ("SimTheme Park"), src/game/rides/vehicle.cpp and
src/game/rides/VehicleCallbacks.cpp:
  vehicle.cpp       Avoid_Walls::calculate 0x10132298
  VehicleCallbacks  implement_nav_rulesA2B 0x10135a70, callback_collision_candidates 0x101356b8, map_local_to_global 0x10135a2c,
                    callback_obstacle_candidates 0x1013598c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for the
original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside the five functions is
either the real code on both sides (unit w203's Vehicle::predict_pos and the Vector2d operators, unit w209's map_global_to_local and map_fn, isqrt,
line_circle_intersect, FixMul and FixDiv with this module's 16.16 model, labs) or a fake that records the same event on both sides: MAP_wall_in_way
(0x10101ca4; the answer is the case's wall table entry for the three bytes, else the case's fallback), CMap::IsXYValid (0x101012f4; the answer is the case's
valid window), GAME_ASSERTMSG (0x1000dde8; the condition and the text) and the two functions the collision walk calls (callback_a and callback_b: the labels of
the two vehicles they get; on the Mac two fake functions reached through __ptr_glue).

The world of a case: gVehZero and gVehOne (mostly 0 and 0x10000, now and then another number so that a function that used the constant instead of the global
shows), gVehMapData (NULL or a block), the two statics of the nav rules (a guard byte and an assert-once word, set per case and compared after), a vehicle
block (the Mac's 0xc8 bytes, 50 words of noise but for the named fields, in a block of 0xd0 between guards; for the collision walk it is the vehicle of a
thing now and then), and for the collision walk a world: gGameWorld pointing at a block with the map cells (cell k at +0x2d8 + k * 0x44, its mWho halfword at
+0x24; 1100 cells, the valid window kept inside x -1..130, y 0..7 so that the squares stay inside), the things (blocks with mKind at +2, the four cell bytes at
+4 to +7, mCellNext at +0xa, room for the vehicle at +0xd4) listed in the cells by chains that end at 0, and the entries of gThingArray (Mac 0x1034cbfc + id *
0x14, host the entry's mThing) that point at them; entry 0 is NULL or not. What is compared: the answer, the event log in order (every wall probe with its
three bytes and the mode, every IsXYValid, every assert with its text, every callback with both labels), the vehicle (all 50 words), the out block, the
statics, the globals, the things (header and the words after it), the cells' mWho, the thing array entries as labels, and the guards. A pointer is compared as
the label of what it points to.

There are no fused operations and no floats: no HardwareRounding.
Not tested: the branch of implement_nav_rulesA2B for y of `from` 0 with y of `to` 0x3ff (a halfword never gives it), a cycle in a thing list (the walk runs
for ever on both sides), a thing id of 0x2800 or more (outside gThingArray), a radius whose cell range is above 8 (the log would run past its 4096 events),
and what the real MAP_wall_in_way (0x10101ca4) does with a byte it is given unmasked (it is a fake here).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_VehicleWalls"
MASK = 0xFFFFFFFF
INT_MAX, INT_MIN = 0x7FFFFFFF, -0x80000000

# ---- the unit's own functions (name, address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
FUNCTIONS = [
    ("avoid_walls", 0x10132298, 2648), ("nav_rules", 0x10135A70, 1324), ("collision", 0x101356B8, 636),
    ("local_to_global", 0x10135A2C, 20), ("obstacle", 0x1013598C, 4),
]
ENTRY = {name: address for name, address, _ in FUNCTIONS}
SIZES = {address: size for _, address, size in FUNCTIONS}
OWN = set(SIZES)

# ---- the callees faked on both sides (the asm's bl targets that are not OWN)
WALL_IN_WAY, IS_XY_VALID, GAME_ASSERT = 0x10101CA4, 0x101012F4, 0x1000DDE8

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_vwl_addr(name)
ADDR = dict(one=0x1034CA8C, zero=0x1034CA90, mapdata=0x1034CA94, guard=0x1034CA98, flag=0x1034CA9C, world=0x1037EBFC, thingarray=0x1034CBFC)
NO_THING = 0x102A459C       # the halfword the walk compares every id with (data, 0)
WORLD_REF = 0x102A460C      # the cell the walk reads the address of gGameWorld from
MAC_ENTRY = 0x14            # the Mac's gThingArray entries are 0x14 bytes apart, mThing first
# ---- the host's offsets and sizes the module relies on, checked once against tpw_vwl_layout: offsetof(Vehicle, mTarget), sizeof(Vehicle),
# sizeof(CThingArrayEntry), offsetof(CThingArrayEntry, mThing), sizeof(CMapCell), offsetof(CMapCell, mWho), offsetof(CThing, mKind),
# offsetof(CThing, mCellX), offsetof(CThing, mCellNext), then the sizes of CThingArrayEntry::mThing, CMapCell::mWho, CThing::mCellX and CThing::mCellNext
HOST_LAYOUT = [0xC8, 0xD0, 48, 0, 0x44, 0x24, 2, 4, 0xA, 8, 2, 1, 2]

GUARD = 0x20            # noise bytes before and after every block
VEH_BLOCK = 0xD0        # the block of the vehicle on both sides (the Mac's Vehicle is 0xc8 bytes of it)
VEH_WORDS = 50          # the Mac's Vehicle in words
THING_VEH = 0xD4        # where a thing keeps its vehicle
THING_SIZE = THING_VEH + VEH_BLOCK
CELLS = 1100
MAP_AT = 0x2D8          # the map's first cell in the world
CELL_SIZE = 0x44
WHO_AT = 0x24           # a cell's mWho
WORLD_SIZE = MAP_AT + CELLS * CELL_SIZE
L_VEHICLE, L_MAPDATA, L_WORLD, L_OUT, L_BEHAVIOUR, L_OTHER, L_THING = 1, 4, 5, 6, 7, 20, 40
# ---- the vehicle's named fields by word index (Mac offset / 4); every other word is noise that must come back unchanged
IDX = dict(radius=1, px=2, py=3, vx=4, vy=5, nav=45)


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the 16.16 model


def fixmul(a, b):
    """FixMul as src/game/park/GoldenTickets.cpp (and the module test_vector2d) define it: rounds half away from zero, saturates."""
    p = a * b
    p += 0x8000 if p >= 0 else -0x8000
    return max(INT_MIN, min(INT_MAX, p >> 16))


def fixdiv(a, b):
    """FixDiv: division by zero saturates by the sign of a; else the quotient of a << 16 rounded half away from zero, saturated."""
    if b == 0:
        return INT_MAX if a >= 0 else INT_MIN
    n = a << 16
    q = abs(n) // abs(b)
    if (n < 0) != (b < 0):
        q = -q
    r = n - q * b
    if abs(r) * 2 >= abs(b):
        q += -1 if (n < 0) != (b < 0) else 1
    return max(INT_MIN, min(INT_MAX, q))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.imp("FixMul", lambda emu: emu.set_r(3, fixmul(s32(emu.r(3)), s32(emu.r(4))) & MASK))
STUBS.imp("FixDiv", lambda emu: emu.set_r(3, fixdiv(s32(emu.r(3)), s32(emu.r(4))) & MASK))
STUBS.imp("labs", lambda emu: emu.set_r(3, T.u32(abs(s32(emu.r(3))))))      # |INT_MIN| stays 0x80000000


def stub_wall(e):
    """MAP_wall_in_way(x, z, dir, mode): the low bytes of the three and the mode; the answer is the case's table entry for them, else its fallback."""
    L = e.cl
    x, z, d = e.r(3) & 0xFF, e.r(4) & 0xFF, e.r(5) & 0xFF
    L.rec("MAP_wall_in_way", (x, z, d, s32(e.r(6))))
    table, fallback = e.vw["walls"]
    e.set_r(3, table.get((x, z, d), fallback) & MASK)


def stub_is_xy_valid(e):
    """CMap::IsXYValid(x, y): the case's answer inside its window, 0 outside."""
    L = e.cl
    x, y = s32(e.r(3)), s32(e.r(4))
    L.rec("CMap::IsXYValid", (x, y))
    x0, x1, y0, y1, yes = e.vw["window"]
    e.set_r(3, (yes if x0 <= x <= x1 and y0 <= y <= y1 else 0) & MASK)


def stub_assert(e):
    """GAME_ASSERTMSG(condition, format, ...): the condition and the text."""
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),), (T.c_format(e, e.cstr(e.r(4)), 5)[:159],))


def stub_callback(name):
    """A function the collision walk calls through its pointer: the labels of its two arguments."""
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), L.lab(e.r(4))))
    return stub


STUBS.internal(WALL_IN_WAY, stub_wall)
STUBS.internal(IS_XY_VALID, stub_is_xy_valid)
STUBS.internal(GAME_ASSERT, stub_assert)

CALLBACKS = []      # the two functions of the collision walk (their TVectors), built once by setup()


def setup(runner):
    T.install_std_stubs(runner.emu)       # the C library's memset, memcpy, sqrt ... (the original isqrt calls sqrt)
    CALLBACKS[:] = [T.fake_function(runner.emu, stub_callback("callback_a")), T.fake_function(runner.emu, stub_callback("callback_b"))]


# ---------------------------------------------------------------------------------------------------------------- the two sides

U64, U32, I32 = C.c_uint64, C.c_uint32, C.c_int32
SIGNATURES = {            # op: (host result type, host argument types); the Mac's argument order is the same
    "avoid_walls": (None, [U64, U64, U64]), "nav_rules": (I32, [I32, I32, I32]), "collision": (None, [U64, I32, I32]),
    "local_to_global": (None, [U64, U32, U32]), "obstacle": (None, [U64, I32, I32]),
}
RETURNS = {"nav_rules"}       # the others answer nothing (a Vector2d through `out`)


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.end + "I", w & MASK) for w in ws))

    def halves(self, a, hs):
        self.write(a, b"".join(struct.pack(self.end + "H", h & 0xFFFF) for h in hs))

    def rd_words(self, a, n):
        return list(struct.unpack(self.end + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_half(self, a):
        return struct.unpack(self.end + "H", self.read(a, 2))[0]

    def put_ptr(self, a, p):
        self.write(a, struct.pack(self.end + ("I" if self.psize == 4 else "Q"), p))

    def rd_ptr(self, a):
        return struct.unpack(self.end + ("I" if self.psize == 4 else "Q"), self.read(a, self.psize))[0]

    def label(self, p):
        return 0 if p == 0 else self.names.get(p, -1)

    def block(self, n, rnd, noise=None):
        """A block of n bytes between two guards, all noise as WORDS in this side's byte order (so that a word read back is the same number on both sides), or
        only its first `noise` bytes when given (the caller fills the rest); its address."""
        base = self.alloc(n + 2 * GUARD)
        k = n if noise is None else noise
        self.words(base, [rnd.getrandbits(32) for _ in range((GUARD + k) // 4)])
        self.words(base + GUARD + n, [rnd.getrandbits(32) for _ in range(GUARD // 4)])
        self.blocks.append((base + GUARD, n))
        return base + GUARD

    def guards(self):
        return [[self.rd_words(a - GUARD, GUARD // 4), self.rd_words(a + n, GUARD // 4)] for a, n in self.blocks]

    def put_entry(self, ident, thing):
        self.put_ptr(self.entry(ident), thing)

    def rd_whos(self):
        """The mWho halfword of every cell, the one before the first included."""
        data = self.read(self.world + MAP_AT - CELL_SIZE, (CELLS + 1) * CELL_SIZE)
        return [struct.unpack_from(self.end + "H", data, k * CELL_SIZE + WHO_AT)[0] for k in range(CELLS + 1)]

    def make(self, case):
        """The world of a case in this side's memory."""
        rnd = random.Random(case["seed"])
        self.names, self.blocks = {}, []
        self.words(self.g("zero"), [case["zero"]])
        self.words(self.g("one"), [case["one"]])
        self.write(self.g("guard"), bytes([case["guard"] & 0xFF]))
        self.words(self.g("flag"), [case["flag"]])
        self.out = self.block(8, rnd)
        self.beh = self.block(0x30, rnd)
        self.mapdata = self.block(0x40, rnd)
        self.put_ptr(self.g("mapdata"), self.mapdata if case["mapdata"] else 0)
        self.names.update({self.out: L_OUT, self.beh: L_BEHAVIOUR, self.mapdata: L_MAPDATA})
        self.put_ptr(self.g("world"), 0)
        self.world = 0
        self.thing_blocks = []
        found = {}
        if case["things"] is not None:
            for k, t in enumerate(case["things"]):
                a = self.block(THING_SIZE, rnd)
                found[t["id"]] = a
                self.thing_blocks.append(a)
                self.halves(a, [t["id"]])
                self.write(a + 2, bytes([t["kind"], t["dead"], t["cx"], t["cxs"], t["cy"], t["cys"]]))
                self.halves(a + 8, [t["prev"], t["next"]])
                self.put_entry(t["id"], a)
                self.names[a] = L_THING + k
                self.names[a + THING_VEH] = L_OTHER + k
            self.put_entry(0, 0x1234 if case["array0"] else 0)
            self.world = self.block(WORLD_SIZE, rnd, MAP_AT - CELL_SIZE)
            tile = b"".join(struct.pack(self.end + "H", h) for h in case["tile"])
            self.write(self.world + MAP_AT - CELL_SIZE, tile * (CELLS + 1))
            for index, ident in case["heads"].items():
                self.halves(self.world + MAP_AT + index * CELL_SIZE + WHO_AT, [ident])
            self.put_ptr(self.g("world"), self.world)
            self.names[self.world] = L_WORLD
        if case["selfid"] is not None:
            self.veh = found[case["selfid"]] + THING_VEH
        else:
            self.veh = self.block(VEH_BLOCK, rnd)
        if case["veh"] is not None:
            self.words(self.veh, case["veh"])
        self.names[self.veh] = L_VEHICLE


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    end = ">"
    psize = 4

    def __init__(self, e, case):
        self.e = e
        e.vw = case
        self.make(case)
        self.words(WORLD_REF, [ADDR["world"]])
        self.halves(NO_THING, [0])
        self.L = e.cl = CL.PpcLog(dict(self.names), case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def entry(self, ident):
        return ADDR["thingarray"] + ident * MAC_ENTRY

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def callback(self, which):
        return CALLBACKS[which]

    def invoke(self, op, args):
        r3, _, _ = T.call_ppc(self.e, ENTRY[op], args)
        return r3 & MASK


class Host(Side):
    """The same on the host library."""
    end = "<"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make(case)
        table, fallback = case["walls"]
        flat = [v for (x, z, d), value in table.items() for v in (x, z, d, s32(value))]
        lib.tpw_vwl_set_walls((C.c_int32 * max(len(flat), 1))(*flat), len(table), s32(fallback))
        x0, x1, y0, y1, yes = case["window"]
        lib.tpw_vwl_set_valid(s32(x0), s32(x1), s32(y0), s32(y1), s32(yes))
        CL.host_begin(lib, {label: address for address, label in self.names.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_vwl_addr(name.encode())

    def entry(self, ident):
        return self.g("thingarray") + ident * HOST_LAYOUT[2] + HOST_LAYOUT[3]

    def alloc(self, n):
        return self.lib.tpw_vwl_alloc(n)       # a zeroed block of the host arena (its address is a 64-bit host pointer: never masked to 32 bits)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def callback(self, which):
        return which

    def invoke(self, op, args):
        restype, argtypes = SIGNATURES[op]
        converted = [s32(a) if t is I32 else a for t, a in zip(argtypes, args)]
        answer = getattr(self.lib, "tpw_vwl_" + op)(*converted) or 0
        return answer & MASK


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_vwl_addr.restype, lib.tpw_vwl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_vwl_alloc.restype, lib.tpw_vwl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_vwl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_vwl_set_walls.restype, lib.tpw_vwl_set_walls.argtypes = None, [C.POINTER(C.c_int32), C.c_int32, C.c_int32]
    lib.tpw_vwl_set_valid.restype, lib.tpw_vwl_set_valid.argtypes = None, [C.c_int32] * 5
    for op, (restype, argtypes) in SIGNATURES.items():
        function = getattr(lib, "tpw_vwl_" + op)
        function.restype, function.argtypes = restype, argtypes
    out = (C.c_int64 * 32)()
    n = lib.tpw_vwl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the call


def arguments(s, case):
    """The call's arguments, the Mac's order (a Vector2d answer goes through `out`, r3 on the Mac)."""
    op, a = case["op"], case["args"]
    if op == "avoid_walls":
        return [s.out, s.beh, s.veh]
    if op == "nav_rules":
        return [a["from"] & MASK, a["to"] & MASK, a["mode"] & MASK]
    if op in ("collision", "obstacle"):
        return [s.veh, a["radius"] & MASK, s.callback(a["which"])]
    return [s.out, a["x"] & MASK, a["y"] & MASK]           # local_to_global


def execute(s, case):
    answer = s.invoke(case["op"], arguments(s, case))
    return answer if case["op"] in RETURNS else 0


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides."""
    o = {
        "answer": answer,
        "log": s.log(),
        "out": s.rd_words(s.out, 2),
        "beh": s.rd_words(s.beh, 0x30 // 4),
        "mapdata": s.rd_words(s.mapdata, 0x40 // 4),
        "veh": s.rd_words(s.veh, VEH_WORDS),
        "statics": [s.read(s.g("guard"), 1)[0], s.rd_word(s.g("flag"))],
        "globals": [s.rd_word(s.g("zero")), s.rd_word(s.g("one")), s.label(s.rd_ptr(s.g("mapdata"))), s.label(s.rd_ptr(s.g("world")))],
        "guards": s.guards(),
    }
    if case["things"] is not None:
        o["things"] = [[s.rd_half(a), s.read(a + 2, 6), s.rd_half(a + 8), s.rd_half(a + 10)] + s.rd_words(a + 12, 50) for a in s.thing_blocks]
        o["entries"] = [s.label(s.rd_ptr(s.entry(t["id"]))) for t in case["things"]] + [s.rd_ptr(s.entry(0)) != 0]
        o["who"] = s.rd_whos()
    return o


def run(s, case):
    try:
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

NAV_MODES = [0, 1, 2, 3, 5, 7, -1, 0x100, INT_MIN]
WALL_VALUES = [1, 1, 1, 2, -1, 0x100, 0x80000000, 0xFF]
GUARDS = [0, 1, 1, 1, 2, 0xFF]
FLAGS = [0, 0, 1, 2, -1, 0x80000000]
U32_EDGES = [0, 1, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x1FFFF, 0x7FFFFFFF, 0x80000000, 0xFFFF0000, 0xFFFFFFFF, 0x12345678]
RADII_W = [0x3333, 0x3333, 0x1000, 0x4000, 0x8000, 0x10000, 0x2000, 0x6666, 1, 0, 0xFFFF, 0x7FFFFFFF, -0x3333, 0x20000]
CELL_WORDS = [0, 1, 2, 5, 10, 63, 64, 100, 124, 125, 126, 127, 128, 129, 0xFE, 0xFF, 0x100, 0x101, 0x7FFF, 0x8000, 0xFFFD, 0xFFFE, 0xFFFF]
VEHICLE_KINDS = (1, 4, 5, 6, 7, 8, 0x12)
RADII = [0, 1, -1, 0xFF, 0x100, 0x3333, 0x3333, 0x1000, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x18000, 0x1FFFF, 0x20000, 0x28000, 0x40000, 0x50000, 0x80000,
         -0x3333, -0x10000, -0x10001, -0x100, -0xFFFF, INT_MAX, INT_MIN, 0x12345, 0x7FFF, 0x6000]


def rv(r):
    return s32(r.getrandbits(32))


def base(r, op):
    """A case with nothing special: no walls, the window of the map's first rows, the globals at their usual numbers, the nav statics set."""
    return {"op": op, "seed": r.getrandbits(48), "zero": 0, "one": 0x10000, "mapdata": r.random() < 0.7, "guard": 1, "flag": 0, "veh": None, "selfid": None,
            "args": {}, "walls": ({}, 0), "window": (0, 0x7F, 0, 7, 1), "things": None, "heads": {}, "tile": [0] * 34, "array0": False, "queues": {}}


def globals_random(r, c):
    c["zero"] = 0 if r.random() < 0.85 else r.choice([0x6666, 0x7000, 5, rv(r)])
    c["one"] = 0x10000 if r.random() < 0.85 else r.choice([0, 0x8000, -0x10000, 0x20000, rv(r)])
    c["guard"] = r.choice(GUARDS)
    c["flag"] = r.choice(FLAGS)


def vehicle_words(r, radius, pos, vel, nav):
    """The Mac's 50 words of a Vehicle: noise but for the named fields."""
    ws = [r.getrandbits(32) for _ in range(VEH_WORDS)]
    ws[IDX["radius"]] = radius & MASK
    ws[IDX["px"]], ws[IDX["py"]] = pos[0] & MASK, pos[1] & MASK
    ws[IDX["vx"]], ws[IDX["vy"]] = vel[0] & MASK, vel[1] & MASK
    ws[IDX["nav"]] = nav & MASK
    return ws


def wall_keys(x, y):
    """The (x, z, dir) bytes of every wall question about the squares around (x, y)."""
    return [((x + dx) & 0xFF, (y + dy) & 0xFF, d) for dx in (-1, 0, 1) for dy in (-1, 0, 1) for d in range(4)]


def table_random(r, x, y, density, on=(), off=(), default=None):
    """The wall table around (x, y): every key a wall with the density; the relative keys of `on` walls and of `off` explicit zeros; the fallback mostly 0."""
    table = {}
    for key in wall_keys(x, y):
        if r.random() < density:
            table[key] = r.choice(WALL_VALUES)
    for dx, dy, d in on:
        table[((x + dx) & 0xFF, (y + dy) & 0xFF, d)] = r.choice(WALL_VALUES)
    for dx, dy, d in off:
        table[((x + dx) & 0xFF, (y + dy) & 0xFF, d)] = 0
    if default is None:
        default = 0 if r.random() < 0.9 else r.choice([1, 2, -1])
    return (table, default)


# ---- map_local_to_global
def local_to_global_edges():
    r = random.Random(204)
    for x in U32_EDGES:
        for y in U32_EDGES:
            yield rnd_local(r, x, y)


def rnd_local(r, x, y):
    c = base(r, "local_to_global")
    c["args"] = {"x": x, "y": y}
    c["veh"] = vehicle_words(r, rv(r), (rv(r), rv(r)), (rv(r), rv(r)), rv(r))
    return c


def local_to_global_random(r):
    pick = lambda: r.choice(U32_EDGES) if r.random() < 0.2 else r.getrandbits(32)
    return rnd_local(r, pick(), pick())


# ---- callback_obstacle_candidates (an empty function: the log stays empty, nothing changes)
def obstacle_case(r, radius, which):
    c = base(r, "obstacle")
    c["args"] = {"radius": radius, "which": which}
    c["veh"] = vehicle_words(r, rv(r), (rv(r), rv(r)), (rv(r), rv(r)), rv(r))
    return c


def obstacle_edges():
    r = random.Random(205)
    for k, radius in enumerate(RADII[:20]):
        yield obstacle_case(r, radius, k & 1)


def obstacle_random(r):
    return obstacle_case(r, r.choice(RADII) if r.random() < 0.5 else rv(r), r.randrange(2))


# ---- implement_nav_rulesA2B
SIDE = {(0, -1): 0, (1, 0): 1, (0, 1): 2, (-1, 0): 3}      # the side of the first square a step crosses (0 = -y, 1 = +x, 2 = +y, 3 = -x)
ROUTES = {                                                 # the four wall keys (relative: dx, dy, dir) of a diagonal step: route 1 twice, route 2 twice
    (1, -1): [(0, 0, 0), (0, -1, 1), (0, 0, 1), (1, 0, 0)],
    (1, 1): [(0, 0, 1), (1, 0, 2), (0, 0, 2), (0, 1, 1)],
    (-1, 1): [(0, 0, 2), (0, 1, 3), (0, 0, 3), (-1, 0, 2)],
    (-1, -1): [(0, 0, 0), (0, -1, 3), (0, 0, 3), (-1, 0, 0)],
}


def square(x, y):
    """The number of the map square (x, y): x + (y * 128 + 1), a halfword."""
    return (x + 128 * y + 1) & 0xFFFF


def nav_case(r, a, b, density=0.35, on=(), off=(), default=None, mode=None, guard=None, flag=None):
    c = base(r, "nav_rules")
    c["args"] = {"from": a & MASK, "to": b & MASK, "mode": r.choice(NAV_MODES) if mode is None else mode}
    xa, ya = ((a & 0xFFFF) - 1) & 0x7F, (((a & 0xFFFF) - 1) & MASK) >> 7
    c["walls"] = table_random(r, xa, ya, density, on, off, default)
    c["guard"] = r.choice(GUARDS) if guard is None else guard
    c["flag"] = r.choice(FLAGS) if flag is None else flag
    return c


def nav_edges():
    r = random.Random(302)
    for xa, ya in ((5, 3), (0, 3), (127, 3), (5, 0), (0, 0), (127, 0x1FF)):
        a = square(xa, ya)
        for (dx, dy), side in SIDE.items():              # the four straight steps with their wall on and off, and the same square
            for wall in (True, False):
                yield nav_case(r, a, square(xa + dx, ya + dy), density=0.0, on=[(0, 0, side)] if wall else [], off=[] if wall else [(0, 0, side)], default=0)
        yield nav_case(r, a, a, density=0.5)
    for dx, dy in ROUTES:                                 # the four diagonal steps with every combination of their four walls
        for mask in range(16):
            keys = ROUTES[(dx, dy)]
            yield nav_case(r, square(5, 3), square(5 + dx, 3 + dy), density=0.0, default=0,
                           on=[k for i, k in enumerate(keys) if mask >> i & 1], off=[k for i, k in enumerate(keys) if not mask >> i & 1])
    for xa, ya in ((0, 3), (127, 3), (0, 0), (127, 0x1FF), (64, 0xFF), (64, 0x100)):      # diagonal steps at the edges of the row and of the byte
        for dx, dy in ROUTES:
            for mask in (0, 3, 5, 10, 12, 15):
                keys = ROUTES[(dx, dy)]
                yield nav_case(r, square(xa, ya), square(xa + dx, ya + dy), density=0.0, default=0,
                               on=[k for i, k in enumerate(keys) if mask >> i & 1], off=[k for i, k in enumerate(keys) if not mask >> i & 1])
    for a, b in ((0x0245, 0x0345), (0x0245, 0x0243), (0x0245, 0x0247), (0x0245, 0x02C7), (0x0245, 0x0145), (1, 0x80), (0x80, 1), (0x7F, 0x80), (0x80, 0x7F),
                 (0, 1), (1, 0), (0, 0), (0xFFFF, 0), (0xFFFF, 0xFFFF), (0xFFFE, 0xFFFF), (0x4000, 0x4001), (0x4000, 0x4080), (0x4000, 0x3FFF)):
        for guard, flag in ((0, 0), (0, 1), (1, 0), (1, 1), (2, 0), (0, -1)):
            yield nav_case(r, a, b, guard=guard, flag=flag)
    for a, b in ((0xFFFF, -1), (0xFFFE, -1), (0xFF7F, -1), (0xFF80, -1), (0x0245, -1), (0, -1), (-1, -1), (-1, 0xFFFF), (0xFFFF, 0xFFFF), (0xFFFF, 0x1FFFF),
                 (0x10001, 0x10002), (0x10001, 2), (1, 0x10002), (0x7FFFFFFF, 0x80000000), (0xFFFF0005, 0x00040006), (5, 0xFFFFFFFE)):
        for flag in (0, 1):
            yield nav_case(r, a, b, flag=flag, guard=1, density=0.5)


def nav_random(r):
    xa = r.choice([0, 1, 5, 63, 64, 100, 126, 127]) if r.random() < 0.5 else r.randrange(0, 128)
    ya = r.choice([0, 1, 2, 3, 100, 255, 256, 0x1FF]) if r.random() < 0.5 else r.randrange(0, 0x200)
    a = square(xa, ya)
    k = r.random()
    if k < 0.5:
        dx, dy = r.choice([(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)])
        b = square(xa + dx, ya + dy)
    elif k < 0.55:
        b = a
    elif k < 0.7:
        b = square(r.randrange(0, 128), r.randrange(0, 0x200))
    elif k < 0.85:
        b = r.choice([0, 1, 0x4000, 0xFFFF, 0x10000 + r.randrange(0, 0x100), 0xFFFFFFFF, a + 0x10000])
    else:
        b = r.getrandbits(32)
    if r.random() < 0.15:
        a |= r.getrandbits(16) << 16
    if r.random() < 0.1:
        b |= r.getrandbits(16) << 16
    c = nav_case(r, a, b, density=r.choice([0.15, 0.35, 0.6]))
    return c


# ---- Avoid_Walls::calculate
EDGE_KEY = {"px": (0, 0, 1), "nx": (0, 0, 3), "py": (0, 0, 2), "ny": (0, 0, 0)}
CORNERS = {                 # the diagonal square's two walls (w), the two shielding walls (s), the two walls of the vehicle's own square toward it (own)
    "A": dict(at=(1, 1), sx=-1, sy=-1, w=[(1, 1, 0), (1, 1, 3)], s=[(0, 1, 0), (1, 0, 3)], own=[(0, 0, 1), (0, 0, 2)]),
    "B": dict(at=(0, 1), sx=1, sy=-1, w=[(-1, 1, 0), (-1, 1, 1)], s=[(0, 1, 0), (-1, 0, 1)], own=[(0, 0, 3), (0, 0, 2)]),
    "C": dict(at=(0, 0), sx=1, sy=1, w=[(-1, -1, 2), (-1, -1, 1)], s=[(-1, 0, 1), (0, -1, 2)], own=[(0, 0, 3), (0, 0, 0)]),
    "D": dict(at=(1, 0), sx=-1, sy=1, w=[(1, -1, 2), (1, -1, 3)], s=[(1, 0, 3), (0, -1, 2)], own=[(0, 0, 1), (0, 0, 0)]),
}


def aw_case(r, cw, ch, radius, pos, vel, on=(), off=(), density=0.25, nav=None, zero=0, one=0x10000, default=None):
    """An Avoid_Walls call: the vehicle in the map square (cw, ch) (the words the position shifts down to) with its radius, position and velocity."""
    c = base(r, "avoid_walls")
    c["zero"], c["one"] = zero, one
    c["walls"] = table_random(r, cw, ch, density, on, off, default)
    c["veh"] = vehicle_words(r, radius, pos, vel, r.choice(NAV_MODES) if nav is None else nav)
    c["window"] = (0, 0xFFFF, 0, 0xFFFF, r.choice([1, 1, 2, -1]))
    return c


def geom_edge(r, cw, ch, radius, e, delta):
    """A position in the square (cw, ch) and a velocity whose prediction (position + velocity at gVehOne 1.0) is `delta` past the limit of the edge e."""
    bx, by = cw << 16, ch << 16
    fx, fy = r.randrange(0, 0x10000), r.randrange(0, 0x10000)
    other = r.choice([0, 1, -1, 0x100, -0x100, 0x1000, -0x1000, 0x4000, -0x4000, 0x10000])
    if e == "px":
        pred = bx + 0x10000 - radius + delta
        fx = r.randrange(0, max(1, min(0x10000, pred - bx + 1)))
        return (bx + fx, by + fy), (pred - bx - fx, other)
    if e == "nx":
        pred = bx + radius + delta
        fx = r.randrange(min(0xFFFF, max(0, pred - bx)), 0x10000)
        return (bx + fx, by + fy), (pred - bx - fx, other)
    if e == "py":
        pred = by + 0x10000 - radius + delta
        fy = r.randrange(0, max(1, min(0x10000, pred - by + 1)))
        return (bx + fx, by + fy), (other, pred - by - fy)
    pred = by + radius + delta
    fy = r.randrange(min(0xFFFF, max(0, pred - by)), 0x10000)
    return (bx + fx, by + fy), (other, pred - by - fy)


def geom_corner(r, cw, ch, radius, k):
    """A position in the square (cw, ch) and a velocity that heads for the corner k (within the radius of it, or about)."""
    c = CORNERS[k]
    x, y = (cw + c["at"][0]) << 16, (ch + c["at"][1]) << 16
    a = r.randrange(1, 0x10001) if c["sx"] < 0 else r.randrange(0, 0x10000)
    b = r.randrange(1, 0x10001) if c["sy"] < 0 else r.randrange(0, 0x10000)
    pos = (x + c["sx"] * a, y + c["sy"] * b)
    reach = max(abs(radius), 0x100)
    target = (x + r.randrange(-reach, reach + 1), y + r.randrange(-reach, reach + 1))
    num, den = r.choice([(1, 1), (3, 2), (2, 1), (1, 2), (3, 4), (5, 4), (3, 1)])
    return pos, ((target[0] - pos[0]) * num // den, (target[1] - pos[1]) * num // den)


def corner_walls(r, k, mask=None):
    """The walls of the corner k as (on, off) lists of relative keys: w1, w2 (the diagonal square's), s1, s2 (the shields) by the bits of mask (or chance)."""
    c = CORNERS[k]
    terms = c["w"] + c["s"]
    bits = [r.random() < 0.6, r.random() < 0.6, r.random() < 0.2, r.random() < 0.2] if mask is None else [bool(mask >> i & 1) for i in range(4)]
    on = [t for t, b in zip(terms, bits) if b]
    off = [t for t, b in zip(terms, bits) if not b]
    if mask is None:
        on += [t for t in c["own"] if r.random() < 0.2]
    else:
        off += c["own"]
    return on, off


def geom_axis(cw, ch, k, along, tail):
    """A vehicle `along` units from the corner k, one unit off the line through it along an axis, heading for the corner with a step of `tail` units (the
    flags the corner needs are met: B and C head toward -x, A toward +x, C and D a little toward -y)."""
    c = CORNERS[k]
    x, y = (cw + c["at"][0]) << 16, (ch + c["at"][1]) << 16
    if k == "A":
        return (x - along, y - 1), (tail, 0)
    if k == "B":
        return (x + along, y - 1), (-tail, 0)
    if k == "C":
        return (x + along, y + 1), (-tail, -1)
    return (x - 1, y + along), (0, -tail)


def aw_edges():
    r = random.Random(301)
    for e in ("px", "nx", "py", "ny"):                   # every edge, its wall on and off, the prediction around the limit
        for wall in (True, False):
            for delta in (-2, -1, 0, 1, 2, 0x4000, -0x4000):
                pos, vel = geom_edge(r, 5, 3, 0x3333, e, delta)
                yield aw_case(r, 5, 3, 0x3333, pos, vel, on=[EDGE_KEY[e]] if wall else [], off=[] if wall else [EDGE_KEY[e]], density=0.2)
    for k in "ABCD":                                     # every corner with every combination of its four wall terms
        for mask in range(16):
            pos, vel = geom_corner(r, 5, 3, 0x3333, k)
            on, off = corner_walls(r, k, mask)
            yield aw_case(r, 5, 3, 0x3333, pos, vel, on=on, off=off, density=0.0, default=0)
    for k in "ABCD":                                     # the push needs t <= the step's length: t is about the distance to the corner (within 0 to 4 units of it, by the
        for along, radius in ((0x900, 0x400), (0x700, 0x200), (0x800, 0x100)):      # radius), the lengths exact below 0x1000: steps one less, equal and one more than t
            for tail in range(along - 6, along + 3):
                pos, vel = geom_axis(5, 3, k, along, tail)
                on, off = corner_walls(r, k, 1)
                yield aw_case(r, 5, 3, radius, pos, vel, on=on, off=off, density=0.0, default=0)
    for k in "ABCD":                                     # and t > 0: at the radius 0x400, t is about the distance less 3 or 4 units, so the first few distances give t <= 0
        for along in range(1, 8):
            pos, vel = geom_axis(5, 3, k, along, 0x800)
            on, off = corner_walls(r, k, 1)
            yield aw_case(r, 5, 3, 0x400, pos, vel, on=on, off=off, density=0.0, default=0)
    for cw, ch in ((0, 0), (0x7F, 0x7F), (0xFF, 0xFF), (0x100, 0x100), (0xFFFE, 0xFFFE), (0, 0xFF), (0xFF, 0), (0x7F, 0), (0, 0x7F), (0xFFFF, 0xFFFF), (0x8000, 0x8000),
                   (0x101, 0xFE)):                      # the byte arguments at the edges of a byte, and the negative positions
        for k in "ABCD":
            pos, vel = geom_corner(r, cw, ch, 0x3333, k)
            on, off = corner_walls(r, k, None)
            yield aw_case(r, cw, ch, 0x3333, pos, vel, on=on, off=off, density=0.4)
        for e in ("px", "nx", "py", "ny"):
            pos, vel = geom_edge(r, cw, ch, 0x3333, e, -1)
            yield aw_case(r, cw, ch, 0x3333, pos, vel, on=[EDGE_KEY[e]], density=0.4)
    for delta in (-1, 0, 1):                             # a vehicle at rest: the length is 0 (no division), d = (0, 0), so the +x and +y sides are asked
        yield aw_case(r, 7, 4, 0x3333, ((7 << 16) + 0x10000 - 0x3333 + delta, (4 << 16) + 0x4000), (0, 0), on=[EDGE_KEY["px"]], density=0.2)
        yield aw_case(r, 7, 4, 0x3333, ((7 << 16) + 0x4000, (4 << 16) + 0x10000 - 0x3333 + delta), (0, 0), on=[EDGE_KEY["py"]], density=0.2)
    for speed in (0x800, 0x10000):                       # a step along an axis: one component of d is 0
        for vel in ((speed, 0), (-speed, 0), (0, speed), (0, -speed)):
            for wall in range(4):
                pos = ((6 << 16) + r.randrange(0x4000, 0xC000), (2 << 16) + r.randrange(0x4000, 0xC000))
                yield aw_case(r, 6, 2, 0x3333, pos, vel, on=[(0, 0, wall)], density=0.2)
    for speed in (0x40, 0x800, 0xFFF):                   # the length equal to gVehZero (no division), and the flags against a gVehZero that is not 0
        for vel in ((speed, 0), (0, -speed), (-speed, 0)):
            pos = ((6 << 16) + r.randrange(0x4000, 0xC000), (2 << 16) + r.randrange(0x4000, 0xC000))
            yield aw_case(r, 6, 2, 0x3333, pos, vel, on=[(0, 0, 1), (0, 0, 3)], density=0.3, zero=speed)
    for radius in (0, 1, -0x3333, 0x7FFFFFFF, INT_MIN, 0x10000, 0x20000, 0xFFFF):         # radii that make the limits wrap, cross the cell or divide by 0
        pos, vel = geom_edge(r, 9, 9, 0x3333, "px", 0x100)
        yield aw_case(r, 9, 9, radius, pos, vel, on=[EDGE_KEY["px"]], density=0.2)
        pos, vel = geom_corner(r, 9, 9, 0x3333, "C")
        on, off = corner_walls(r, "C", 1)
        yield aw_case(r, 9, 9, radius, pos, vel, on=on, off=off, density=0.1)
    for one in (0, 0x8000, -0x10000, 0x20000, 1):        # the prediction by another gVehOne
        pos, vel = geom_edge(r, 9, 9, 0x3333, "nx", -0x100)
        yield aw_case(r, 9, 9, 0x3333, pos, vel, on=[EDGE_KEY["nx"]], density=0.2, one=one)


def pick_cell(r):
    return r.choice(CELL_WORDS) if r.random() < 0.4 else r.randrange(1, 127)


def aw_random(r):
    cw, ch = pick_cell(r), pick_cell(r)
    radius = r.choice(RADII_W)
    kind = r.random()
    if kind < 0.35:
        e = r.choice(["px", "nx", "py", "ny"])
        delta = r.choice([0, 1, -1, 2, -2, r.randrange(-0x3000, 0x3000), r.randrange(-0x3000, 0x3000)])
        pos, vel = geom_edge(r, cw, ch, radius, e, delta)
        c = aw_case(r, cw, ch, radius, pos, vel, on=[EDGE_KEY[e]] if r.random() < 0.85 else [], density=0.3)
    elif kind < 0.7:
        k = r.choice("ABCD")
        pos, vel = geom_corner(r, cw, ch, radius, k)
        on, off = corner_walls(r, k)
        c = aw_case(r, cw, ch, radius, pos, vel, on=on, off=off, density=0.15)
    elif kind < 0.8:
        pos = ((cw << 16) + r.randrange(0, 0x10000), (ch << 16) + r.randrange(0, 0x10000))
        c = aw_case(r, cw, ch, radius, pos, (0, 0), density=0.4)
    else:
        pos = ((cw << 16) + r.randrange(0, 0x10000), (ch << 16) + r.randrange(0, 0x10000))
        vel = (r.choice([0, rv(r) >> 14, r.randrange(-0x20000, 0x20000)]), r.choice([0, rv(r) >> 14, r.randrange(-0x20000, 0x20000)]))
        c = aw_case(r, cw, ch, radius, pos, vel, density=0.35)
    globals_random(r, c)
    return c


# ---- callback_collision_candidates
def cells_of(radius):
    """How many squares around the vehicle's the walk visits on each side: (radius + 0xffff) >> 16 as the Mac's wrapping add and arithmetic shift."""
    return s32(radius + 0xFFFF) >> 16


def square_index(x, y):
    """The index of the map cell of the square (x, y): its number (x + y * 128 + 1, a halfword) less 1."""
    return ((x + (y << 7) + 1) & 0xFFFF) - 1


def lattice(target):
    """The (dx, dy) with dx, dy >= 0 and dx * dx + dy * dy == target."""
    found = []
    dx = 0
    while dx * dx <= target:
        rest = target - dx * dx
        dy = math.isqrt(rest)
        if dy * dy == rest:
            found.append((dx, dy))
        dx += 1
    return found


def thing_at(ident, kind, tx, ty, r=None):
    """A thing of the case: its kind and its position in the 8.8 halfwords (cell byte, then sub byte) of mCellX/mCellXSub and mCellY/mCellYSub."""
    tx, ty = tx & 0xFFFF, ty & 0xFFFF
    return {"id": ident, "kind": kind & 0xFF, "dead": 0 if r is None else r.getrandbits(8), "cx": tx >> 8, "cxs": tx & 0xFF, "cy": ty >> 8, "cys": ty & 0xFF,
            "next": 0, "prev": 0 if r is None else r.getrandbits(16)}


def full_window(cw, ch, radius, yes=1):
    n = max(cells_of(radius), 0)
    return (max(-1, cw - n), min(130, cw + n), max(0, ch - n), min(7, ch + n), yes)


def collision_case(r, cw, ch, radius, things, chains, window=None, fx=None, fy=None, selfid=None, array0=False, which=None):
    """A walk: the vehicle at (cw, ch) plus a fraction, the things, the chains [(cell index, [ids in order])] and the valid window."""
    c = base(r, "collision")
    fx = r.randrange(0, 0x10000) if fx is None else fx
    fy = r.randrange(0, 0x10000) if fy is None else fy
    pos = ((cw << 16) + fx, (ch << 16) + fy)
    c["veh"] = vehicle_words(r, r.choice([0x3333, 0x1000, 0x8000, rv(r)]), pos, (rv(r), rv(r)), r.choice(NAV_MODES))
    by_id = {t["id"]: dict(t) for t in things}
    heads = {}
    for index, ids in chains:
        for a, b in zip(ids, ids[1:] + [0]):
            by_id[a]["next"] = b
        if ids:
            heads[index] = ids[0]
    ids = [t["id"] for t in things]
    c["things"] = [by_id[i] for i in ids]
    c["heads"] = heads
    c["tile"] = [r.choice(ids) for _ in range(34)] if ids else [0] * 34
    c["tile"][WHO_AT // 2] = 0
    c["selfid"] = selfid
    c["array0"] = array0
    c["window"] = full_window(cw, ch, radius) if window is None else window
    c["args"] = {"radius": radius, "which": r.randrange(2) if which is None else which}
    return c


def vehicle_eight(cw, ch, fx, fy):
    """The vehicle's position in 8.8 (its x and y shifted right by 8)."""
    return s32((cw << 16) + fx) >> 8, s32((ch << 16) + fy) >> 8


def offsets(r, sr, r2, vx, vy):
    """Where a thing stands relative to the vehicle in 8.8: inside the range, on the vehicle, on the boundary of the range, around it or anywhere."""
    roll = r.random()
    reach = max(abs(sr), 2)
    if roll < 0.45:
        return r.randrange(-reach, reach + 1), r.randrange(-reach, reach + 1)
    if roll < 0.55:
        return 0, 0
    if roll < 0.75 and 0 < r2 <= 4200000:
        pairs = lattice(r.choice([r2 - 1, r2, r2 + 1]))
        if pairs:
            dx, dy = r.choice(pairs)
            return dx * r.choice([-1, 1]), dy * r.choice([-1, 1])
    if roll < 0.9:
        return r.randrange(-3 * reach, 3 * reach + 1), r.randrange(-3 * reach, 3 * reach + 1)
    return r.randrange(-0x8000, 0x8000) - vx, r.randrange(-0x8000, 0x8000) - vy


def collision_random(r):
    radius = r.choice(RADII)
    n = max(cells_of(radius), 0)
    assert cells_of(radius) <= 8
    cw = r.choice([-1, 0, 1, 2, 64, 125, 126, 127, 128, 129, 130]) if r.random() < 0.35 else r.randrange(0, 128)
    ch = r.choice([0, 1, 6, 7]) if r.random() < 0.35 else r.randrange(0, 8)
    fx, fy = r.randrange(0, 0x10000), r.randrange(0, 0x10000)
    x0, x1, y0, y1, yes = full_window(cw, ch, radius, r.choice([1, 1, 1, 2, -1, 0x100]))
    roll = r.random()
    if roll < 0.12:
        window = (1, 0, 0, 0, yes)                                  # nothing is valid
    elif roll < 0.3:
        a, b = sorted([r.randrange(x0 - 1, x1 + 2), r.randrange(x0 - 1, x1 + 2)])
        c, d = sorted([r.randrange(y0 - 1, y1 + 2), r.randrange(y0 - 1, y1 + 2)])
        window = (max(-1, a), min(130, b), max(0, c), min(7, d), yes)
    else:
        window = (x0, x1, y0, y1, yes)
    visited = [square_index(x, y) for y in range(ch - n, ch + n + 1) for x in range(cw - n, cw + n + 1) if window[0] <= x <= window[1] and window[2] <= y <= window[3]]
    visited = list(dict.fromkeys(visited)) if cells_of(radius) >= 0 else []
    outside = [i for i in {r.randrange(-1, 1027) for _ in range(6)} if i not in visited]
    r.shuffle(visited)
    pool = visited + outside
    vx, vy = vehicle_eight(cw, ch, fx, fy)
    sr = radius >> 8
    r2 = sr * sr
    count = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 10, 12])
    idpool = list(range(1, 41)) if r.random() < 0.8 else [0x27FF, 0x2000, 0x100, 0x7F, 0x80, 0xFF, 0x1000, 0x27FE, 0x1FF, 0x101, 1, 2]
    ids = r.sample(idpool, min(count, len(idpool)))
    things = []
    for ident in ids:
        kind = r.choice(VEHICLE_KINDS) if r.random() < 0.55 else r.randrange(0, 0x14)
        dx, dy = offsets(r, sr, r2, vx, vy)
        things.append(thing_at(ident, kind, vx + dx, vy + dy, r))
    chains, at, order = [], 0, list(ids)
    r.shuffle(order)
    while at < len(order) and pool:
        length = r.choice([1, 1, 2, 3, 4])
        index = pool.pop(0) if (not outside or r.random() < 0.8 or len(pool) <= len(outside)) and pool else pool.pop()
        chains.append((index, order[at:at + length]))
        at += length
    placed = [i for index, chain in chains for i in chain if index in visited]
    selfid = r.choice(placed) if placed and r.random() < 0.25 else None
    if selfid is not None:
        by = {t["id"]: t for t in things}
        t = by[selfid]
        dx, dy = r.randrange(-max(abs(sr), 2) // 2, max(abs(sr), 2) // 2 + 1), r.randrange(-max(abs(sr), 2) // 2, max(abs(sr), 2) // 2 + 1)
        by[selfid] = thing_at(selfid, r.choice(VEHICLE_KINDS) if r.random() < 0.85 else t["kind"], vx + dx, vy + dy, r)
        things = [by[i] for i in ids]
    return collision_case(r, cw, ch, radius, things, chains, window, fx, fy, selfid=selfid, array0=r.random() < 0.25)


def quick(r, specs, radius=0x3333, cw=10, ch=3, fx=0x4000, fy=0x8000, chains=None, **over):
    """A walk with the things (id, kind, dx, dy) around the vehicle in 8.8, all in the vehicle's own square unless `chains` (index -> ids) says otherwise."""
    vx, vy = vehicle_eight(cw, ch, fx, fy)
    things = [thing_at(i, kind, vx + dx, vy + dy, r) for i, kind, dx, dy in specs]
    if chains is None:
        chains = [(square_index(cw, ch), [t["id"] for t in things])] if things else []
    return collision_case(r, cw, ch, radius, things, chains, fx=fx, fy=fy, **over)


def collision_edges():
    r = random.Random(303)
    for k, kind in enumerate(list(range(0x14)) + [0x14, 0x30, 0x80, 0xFF]):      # every kind, near the vehicle
        yield quick(r, [(7, kind, 10, 5)], which=k & 1)
    for radius in (0x3333, 0x1000, 0x8000, 0x10000):                              # the squared distance 0, less 1, equal and more 1 than the squared range
        sr = radius >> 8
        for target in (0, sr * sr - 1, sr * sr, sr * sr + 1):
            pairs = lattice(target)
            dx, dy = pairs[0] if pairs else (sr, 0)
            yield quick(r, [(7, 1, dx, dy), (8, 1, -dy, dx)], radius=radius)
    for total in (4, 5, 6, 7, 8):                                                 # the cap of five calls, in one cell and over two
        specs = [(i + 1, 1, 3 + i, 2 - i) for i in range(total)]
        yield quick(r, specs, radius=0x10000)
        own = square_index(10, 3)
        yield quick(r, specs, radius=0x10000, chains=[(own, [1, 2, 3]), (own + 1, [i + 1 for i in range(3, total)])])
        mixed = [(i + 1, 1 if i % 2 == 0 else 2, 3 + i, 2 - i) for i in range(total + 2)]
        yield quick(r, mixed, radius=0x10000)
    for radius in RADII:                                                          # every range with things around
        specs = [(1, 1, 5, 5), (2, 4, -20, 3), (3, 6, 60, 0), (4, 8, 0, -120), (5, 0x12, 300, 300), (6, 1, 0x100, 0)]
        own = square_index(10, 3)
        yield quick(r, specs, radius=radius, chains=[(own, [1, 2]), (own + 1, [3, 4]), (own - 1, [5]), (own + 128, [6])])
    own = square_index(10, 3)
    specs = [(1, 1, 5, 5), (2, 4, -20, 3), (3, 6, 6, 0)]
    for window in ((10, 10, 3, 3, 1), (11, 12, 3, 3, 1), (9, 11, 4, 4, 1), (0, 0x7F, 0, 7, 0), (10, 10, 3, 3, 0), (-1, 130, 0, 7, 0x100), (1, 0, 0, 7, 1), (10, 9, 3, 3, 1)):
        yield quick(r, specs, radius=0x18000, window=window, chains=[(own, [1]), (own + 1, [2]), (own - 1, [3])])
    for ident_set in ([1, 2], [2, 1], [3]):                                         # the vehicle's own thing, first, second, alone and of another kind
        specs = [(i, 1, 4 * i, 3) for i in ident_set]
        yield quick(r, specs, selfid=ident_set[0])
        yield quick(r, specs, selfid=ident_set[-1])
    yield quick(r, [(1, 2, 4, 3), (2, 1, 5, 5)], selfid=1)
    for cw, window_x in ((0, -1), (0, 0), (1, -1)):                                  # the cell before the first, reached by the square 0
        index = square_index(-1, 0)
        yield quick(r, [(1, 1, 5, 5), (2, 4, -5, 3)], radius=0x10000, cw=cw, ch=0, fx=0x100, fy=0x100, window=(window_x, 3, 0, 1, 1), chains=[(index, [1, 2])])
    for array0 in (False, True):                                                    # entry 0 of the thing table free and used
        yield quick(r, [(1, 1, 4, 4), (2, 1, 5, 5)], array0=array0)
        yield quick(r, [], array0=array0)
    yield quick(r, [(0x27FF, 1, 4, 4), (0x2000, 1, 5, 5), (0x100, 4, 6, 6), (0x7F, 5, 7, 7)])
    yield quick(r, [(0x27FE, 0x12, 4, 4), (1, 1, 5, 5)], chains=[(square_index(10, 3), [0x27FE, 1])])
    for cw in (127, 128, 129, 130):                                                  # squares past the row
        yield quick(r, [(1, 1, 4, 4), (2, 6, 5, 5)], radius=0x20000, cw=cw, ch=2, chains=[(square_index(cw, 2), [1]), (square_index(cw - 1, 2), [2])])


# ---------------------------------------------------------------------------------------------------------------- the tests

TESTS = [
    FnTest("Avoid_Walls::calculate", ppc, host, edge=aw_edges, rnd=aw_random),
    FnTest("implement_nav_rulesA2B", ppc, host, edge=nav_edges, rnd=nav_random),
    FnTest("callback_collision_candidates", ppc, host, edge=collision_edges, rnd=collision_random),
    FnTest("map_local_to_global", ppc, host, edge=local_to_global_edges, rnd=local_to_global_random),
    FnTest("callback_obstacle_candidates", ppc, host, edge=obstacle_edges, rnd=obstacle_random),
]
