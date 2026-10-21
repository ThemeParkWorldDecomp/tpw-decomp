"""Unit w209, against libtpw_unit_VehiclePath.dylib (tests/unit_VehiclePath.cpp), main app ("SimTheme Park"), src/game/rides/vehicle.cpp and
src/game/rides/VehicleCallbacks.cpp:
  vehicle.cpp       Follow_Path::calculate 0x10132d7c, Vehicle::next_step 0x10133564, Vehicle::recalc_path 0x10133be4, Vehicle::is_stuck 0x101342ac,
                    Pilot::init_behaviours 0x10134404, Pilot::Pilot 0x10134610
  VehicleCallbacks  map_global_to_local 0x101359e8, map_fn 0x10135fd4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for the
original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside the eight functions is
either the real code on both sides (unit w203's steering code: truncate, normalise, perp, Vehicle::is_finished_path, Pilot::set_speed, clear_behaviours, the
Vector2d operators; FixMul and FixDiv with the module's 16.16 model, isqrt, labs) or a fake that records the same event on both sides: wallhug_tricky
(0x10136e7c; it also copies the case's plan, a count and the squares, into the path buffer), CMap::GetTimeStampCell (0x10102724), CMap::MakeNewTimeStamp
(0x101025c4; the map is reported as its offset from gGameWorld, 0x2d8), CMap::IsXYValid (0x101012f4), implement_nav_rulesA2B (0x10135a70), CTRACE
(0x1006fcbc) and the pool's behaviours (the Mac's are objects with a fake vtable whose slot at +8 is Behaviour::calculate; the host's are the library's).
The Mac has no strcmp in the std stubs, so this module's own (C semantics, unsigned bytes) answers the one init_behaviours calls.

The world of a case: gVehOne and gVehZero (mostly 0x10000 and 0, now and then another number so that a function that used the constant instead of the global
shows), gGameWorld (NULL now and then), gVehMapData, the path buffer gVehPath (0x208 bytes of noise, compared whole), the behaviour table (8 entries of a pool
of 12 behaviours with their 32-byte names) and the behaviour list (16 entries, mData anywhere in the first six, mTop and mEnd around it), a vehicle block (the
Mac's 0xc8 bytes between guards; the fields at their Mac offsets, every other word seeded noise, the target at +0x30 on the Mac and in the host's own member
appended after it) and a target block. What is compared: the answer, the event log, the out block, the target block, the vehicle (every word but +0x30 and the
flag word decoded, the four bytes of the flag word as bytes, the target as a label), the path buffer, the globals, the list (header as entry indexes, entries
as weight and behaviour label), the table (labels), the pool's names, and the guards. A pointer is compared as the label of what it points to.

There are no fused operations and no value that reaches fctiwz's positive saturation (set_speed clamps its argument to 2.0): no HardwareRounding.
Not tested: a waypoint index below 0 or above 11 (the original reads the vehicle's other fields or outside it, as does the host; an index of 5 to 11 reads
the padding words after the waypoints, which are fields of the case), a behaviour table entry that is NULL (the Mac's eighth word is NULL and its search runs
off the table, the host's has eight blank behaviours), a list whose top lies below its data (next_step would run off the list), the vtable slot of a
behaviour other than +8, and wallhug_tricky's own planning (the fake hands over the case's count and squares).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_VehiclePath"
MASK = 0xFFFFFFFF
INT_MAX, INT_MIN = 0x7FFFFFFF, -0x80000000

# ---- the unit's own functions (name, address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
FUNCTIONS = [
    ("follow_path", 0x10132D7C, 1184), ("next_step", 0x10133564, 640), ("recalc_path", 0x10133BE4, 1392), ("is_stuck", 0x101342AC, 304),
    ("init_behaviours", 0x10134404, 480), ("pilot_ctor", 0x10134610, 328), ("map_global_to_local", 0x101359E8, 20), ("map_fn", 0x10135FD4, 108),
]
ENTRY = {name: address for name, address, _ in FUNCTIONS}
SIZES = {address: size for _, address, size in FUNCTIONS}
OWN = set(SIZES)

# ---- the callees faked on both sides (the asm's bl targets that are not OWN)
WALLHUG_TRICKY, STAMP_CELL, NEW_STAMP, IS_XY_VALID, NAV_RULES, CTRACE = 0x10136E7C, 0x10102724, 0x101025C4, 0x101012F4, 0x10135A70, 0x1006FCBC

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_vph_addr(name)
ADDR = dict(zero=0x1034CA90, one=0x1034CA8C, list=0x1034CA74, path=0x1034C708, mapdata=0x1034CA94, world=0x1037EBFC, table=0x102A4478, nav=0x102A45A0)
NAV_TVECTOR = 0x10266D88          # what the Mac's pointer at 0x102a45a0 holds (the TVector of implement_nav_rulesA2B)
# ---- the host's offsets and sizes the module relies on, checked once against tpw_vph_layout: offsetof(Vehicle, mTarget), sizeof(Vehicle), sizeof(BehaviourEntry),
# offsetof(BehaviourEntry, mBehaviour), sizeof(BehaviourList), the offset of a behaviour's name, sizeof(wallhug_path), the size of the pool
HOST_LAYOUT = [0xC8, 0xD0, 16, 8, 24, 8, 0x208, 12]

GUARD = 0x20            # noise bytes before and after every block
VEH_SIZE = 0xC8         # the Mac's Vehicle
VEH_BLOCK = 0xD0        # the host's (the last 8 bytes are its mTarget)
FLAG_WORD = 0x60        # the word whose first byte is mPathFlag (the other three are pad)
WORD_OFFSETS = [offset for offset in range(0, VEH_SIZE, 4) if offset not in (0x30, FLAG_WORD)]
WORLD_SIZE = 0x300
LIST_ENTRIES = 16
POOL = 12
MAC_OBJECT = 0x30       # the Mac's behaviour objects are 0x30 bytes apart
PATH_SIZE = 0x208
L_VEHICLE, L_PATH, L_TARGET, L_MAPDATA, L_WORLD, L_OUT, L_POOL = 1, 2, 3, 4, 5, 6, 20

QUEUE_KEYS = {"stamp": "CMap::GetTimeStampCell", "newstamp": "CMap::MakeNewTimeStamp", "valid": "CMap::IsXYValid", "nav": "implement_nav_rulesA2B",
              "wh": "wh_answer", "fx": "force_x", "fy": "force_y"}


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


def oct_dist(dx, dy):
    """The octagonal distance as is_finished_path computes it: signed absolute values (|INT_MIN| stays negative), signed compare, arithmetic shift."""
    ax = dx if dx >= 0 else s32(-dx)
    ay = dy if dy >= 0 else s32(-dy)
    smaller = ay if ax > ay else ax
    return s32(s32(ax + ay) - (smaller >> 1))


def progress(f, one):
    """Vehicle::is_finished_path of the vehicle fields f (a waypoint index of 0 to 4)."""
    if f["flag"] == 1:
        return one
    wx, wy = f["wps"][f["wp"]]
    d = oct_dist(s32(wx - f["pos"][0]), s32(wy - f["pos"][1]))
    remaining = s32(f["leg_b"] + s32(f["leg_a"] + d))
    if f["total"] == 0:
        return one
    return s32(one - fixdiv(remaining, f["total"]))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.imp("FixMul", lambda emu: emu.set_r(3, fixmul(s32(emu.r(3)), s32(emu.r(4))) & MASK))
STUBS.imp("FixDiv", lambda emu: emu.set_r(3, fixdiv(s32(emu.r(3)), s32(emu.r(4))) & MASK))
STUBS.imp("labs", lambda emu: emu.set_r(3, T.u32(abs(s32(emu.r(3))))))      # |INT_MIN| stays 0x80000000


def stub_strcmp(e):
    """strcmp(a, b): the difference of the first bytes that differ (unsigned bytes), 0 at the shared NUL."""
    a, b = e.r(3), e.r(4)
    while True:
        x, y = e.read(a, 1)[0], e.read(b, 1)[0]
        if x != y or x == 0:
            e.set_r(3, (x - y) & MASK)
            return
        a += 1
        b += 1


STUBS.imp("strcmp", stub_strcmp)


def map_offset(e):
    """The map (r3) as its offset from gGameWorld: the Mac's GetMap is `world + 0x2d8`, NULL world or not."""
    return s32(e.r(3) - e.u32(ADDR["world"]))


def stub_wallhug(e):
    """wallhug_tricky(wallhug_path*, NavMode): records the start and end bytes, plans (the count and the squares come from the case), answers the queue."""
    L = e.cl
    path = e.r(3)
    b = e.read(path + 2, 4)
    L.rec("wallhug_tricky", (L.lab(path), s32(e.r(4)), b[0], b[1], b[2], b[3]))
    e.write(path, e.plan[:1])
    e.write(path + 6, e.plan[6:])
    e.set_r(3, L.ret("wh_answer", 1) & MASK)


def stub_stamp_cell(e):
    L = e.cl
    L.rec("CMap::GetTimeStampCell", (map_offset(e), e.r(4), e.r(5)))
    e.set_r(3, L.ret("CMap::GetTimeStampCell", 0) & MASK)


def stub_new_stamp(e):
    L = e.cl
    L.rec("CMap::MakeNewTimeStamp", (map_offset(e),))
    e.set_r(3, L.ret("CMap::MakeNewTimeStamp", 0) & MASK)


def stub_is_xy_valid(e):
    L = e.cl
    L.rec("CMap::IsXYValid", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("CMap::IsXYValid", 1) & MASK)


def stub_nav_rules(e):
    L = e.cl
    L.rec("implement_nav_rulesA2B", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, L.ret("implement_nav_rulesA2B", 1) & MASK)


def stub_ctrace(e):
    """CTRACE(level, format, ...): the level and the formatted text."""
    e.cl.rec("CTRACE", (e.r(3),), (T.c_format(e, e.cstr(e.r(4)), 5)[:159],))


def stub_calculate(e):
    """Behaviour::calculate through the vtable slot at +8: the hidden answer pointer in r3, the behaviour in r4, the vehicle in r5."""
    L = e.cl
    L.rec("Behaviour::calculate", (L.lab(e.r(4)), L.lab(e.r(5))))
    e.write(e.r(3), struct.pack(">II", L.ret("force_x", 0) & MASK, L.ret("force_y", 0) & MASK))


STUBS.internal(WALLHUG_TRICKY, stub_wallhug)
STUBS.internal(STAMP_CELL, stub_stamp_cell)
STUBS.internal(NEW_STAMP, stub_new_stamp)
STUBS.internal(IS_XY_VALID, stub_is_xy_valid)
STUBS.internal(NAV_RULES, stub_nav_rules)
STUBS.internal(CTRACE, stub_ctrace)

VTABLE = 0      # the Mac behaviours' fake vtable, built once by setup()


def pack(words):
    return struct.pack(">%dI" % len(words), *words)


def make_vtable(e, slots, size=0x50):
    table = T.persist_alloc(e, size)
    for slot, callback in slots.items():
        e.write(table + slot, pack([T.fake_function(e, callback)]))
    return table


def setup(runner):
    global VTABLE
    T.install_std_stubs(runner.emu)       # the C library's memset, memcpy, sqrt ... (the original isqrt calls sqrt); the array constructors
    VTABLE = make_vtable(runner.emu, {8: stub_calculate})


# ---------------------------------------------------------------------------------------------------------------- the two sides

U64, U32, I32 = C.c_uint64, C.c_uint32, C.c_int32
SIGNATURES = {            # op: (host result type, host argument types); the Mac's argument order is the same
    "follow_path": (None, [U64, U64, U64]), "next_step": (None, [U64]), "recalc_path": (I32, [U64, U64, I32]), "is_stuck": (I32, [U64]),
    "init_behaviours": (None, []), "pilot_ctor": (U64, [U64]), "map_global_to_local": (None, [U64, U32, U32]), "map_fn": (I32, [I32, I32, U64]),
}
RETURNS = {"recalc_path", "is_stuck", "map_fn"}       # the others answer nothing (a Vector2d through `out`); the constructor answers `this`, compared as a label

# ---- the Vehicle's named fields, by Mac offset; every other word of the 0xc8 bytes (but +0x30) is noise that must come back unchanged
SCALARS = {"f00": 0x00, "radius": 0x04, "force": 0x18, "speed": 0x1C, "f34": 0x34, "stamp": 0x48, "num": 0x54, "loaded": 0x58, "wp": 0x5C, "total": 0xA0,
           "leg_a": 0xA4, "leg_b": 0xA8, "last": 0xAC, "hist": 0xB0, "nav": 0xB4, "unreach": 0xB8}
PAIRS = {"pos": 0x08, "vel": 0x10, "fwd": 0x20, "side": 0x28, "f38": 0x38, "f40": 0x40, "dest": 0x4C}


def vehicle_words(f):
    """The (Mac offset, value) pairs of the named fields of f (the flag byte at +0x60 and the noise kind are written apart)."""
    pairs = []
    for name, value in f.items():
        if name in SCALARS:
            pairs.append((SCALARS[name], value))
        elif name in PAIRS:
            pairs += [(PAIRS[name], value[0]), (PAIRS[name] + 4, value[1])]
        elif name == "wps":
            for k, (x, y) in enumerate(value):
                pairs += [(0x64 + 8 * k, x), (0x68 + 8 * k, y)]
        elif name == "legs":
            pairs += [(0x8C + 4 * k, v) for k, v in enumerate(value)]
        elif name == "pad":
            pairs += [(0xBC + 4 * k, v) for k, v in enumerate(value)]
        elif name not in ("flag", "noise"):
            raise KeyError(name)
    return pairs


def wh_bytes(wh):
    """The 0x208 bytes of the plan the fake wallhug_tricky copies: the count at 0, noise, the first squares as given."""
    rnd = random.Random(wh["seed"])
    data = bytearray(rnd.randbytes(PATH_SIZE))
    data[0] = wh["count"] & 0xFF
    for k, (x, y) in enumerate(wh["points"]):
        data[6 + 2 * k], data[7 + 2 * k] = x & 0xFF, y & 0xFF
    return bytes(data)


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def put_ptr(self, a, p):
        self.write(a, struct.pack(self.pfmt, p))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psize))[0]

    def label(self, p):
        """The label of what a pointer points to: 0 for NULL, -1 for anything the case did not build."""
        return 0 if p == 0 else self.names.get(p, -1)

    def block(self, size, rnd):
        """`size` bytes between two guards, all seeded noise (the same bytes on both sides); returns the address of the first byte."""
        address = self.alloc(GUARD + size + GUARD)
        self.write(address, rnd.randbytes(GUARD + size + GUARD))
        self.sizes[address + GUARD] = size
        return address + GUARD

    def guards(self, address):
        return self.read(address - GUARD, GUARD) + self.read(address + self.sizes[address], GUARD)

    def make(self, case):
        """Allocates and fills every block of the case, writes the globals, the list, the table and the pool (the addresses are the Mac's or the host's own)."""
        rnd = random.Random(case["fill"])
        self.sizes = {}
        self.out = self.block(8, rnd)
        self.target = self.block(8, rnd)
        self.veh = self.block(VEH_BLOCK, rnd)
        self.mapdata = self.block(16, rnd)
        self.world = self.block(WORLD_SIZE, rnd)
        self.entries = self.block(LIST_ENTRIES * 16, rnd)        # the host's entry size on both sides, so the guards are the same bytes
        self.pool = self.pool_objects(rnd)
        self.names = {self.out: L_OUT, self.target: L_TARGET, self.veh: L_VEHICLE, self.mapdata: L_MAPDATA, self.world: L_WORLD, self.g("path"): L_PATH}
        self.names.update({obj: L_POOL + k for k, obj in enumerate(self.pool)})
        self.words(self.out, [0x5A5A5A5A, 0xA5A5A5A5])
        self.words(self.target, [0x1234, 0x5678])
        self.put_vehicle(self.veh, case["veh"], case["target_set"], rnd)
        self.words(self.g("one"), [case["one"]])
        self.words(self.g("zero"), [case["zero"]])
        self.put_ptr(self.g("mapdata"), self.mapdata)
        self.put_ptr(self.g("world"), self.world if case["world"] else 0)
        self.write(self.g("path"), rnd.randbytes(PATH_SIZE))
        self.put_pool(case["names"], rnd)
        self.put_table(case["table"])
        self.put_list(case["list"])
        self.put_extras(case)

    def put_vehicle(self, address, f, target_set, rnd):
        """Noise in every word, then the named fields, the flag byte and the target."""
        kind = f.get("noise", 0)
        words = [rnd.getrandbits(32) if kind == 0 else 0 if kind == 1 else MASK for _ in range(VEH_SIZE // 4)]
        flag_word = rnd.randbytes(4) if kind == 0 else bytes(4) if kind == 1 else b"\xff" * 4
        self.words(address, words)
        self.write(address + FLAG_WORD, flag_word)
        for offset, value in vehicle_words(f):
            self.words(address + offset, [value])
        if "flag" in f:
            self.write(address + FLAG_WORD, bytes([f["flag"] & 0xFF]))
        self.set_target(address, self.target if target_set else 0)

    def get_vehicle(self, address):
        """The words (but +0x30 and the flag word) decoded, the flag word's four bytes, the target as a label."""
        raw = self.read(address, VEH_SIZE)
        words = [struct.unpack_from(self.fmt, raw, offset)[0] for offset in WORD_OFFSETS]
        return [words, bytes(raw[FLAG_WORD:FLAG_WORD + 4]), self.label(self.target_at(address))]

    def put_pool(self, names, rnd):
        for obj, name in zip(self.pool, names):
            raw = name.encode("latin-1") + b"\0"
            self.write(obj + self.name_at, raw + rnd.randbytes(0x20 - len(raw)))
            self.put_vptr(obj)

    def put_table(self, table):
        for k, behaviour in enumerate(table):
            self.put_ptr(self.g("table") + k * self.psize, self.pool[behaviour])

    def get_table(self):
        return [self.label(self.rd_ptr(self.g("table") + k * self.psize)) for k in range(8)]

    def put_list(self, lst):
        for k, (weight, behaviour) in enumerate(lst["entries"]):
            entry = self.entries + k * self.esize
            self.words(entry, [weight])
            self.put_ptr(entry + self.pointer_at, self.pool[behaviour])
        for k, key in enumerate(("data", "top", "end")):
            self.put_ptr(self.g("list") + k * self.psize, self.entries + lst[key] * self.esize)

    def get_list(self):
        """The header as entry indexes, the entries as (weight, behaviour label)."""
        head = [self.rd_ptr(self.g("list") + k * self.psize) for k in range(3)]
        entries = []
        for k in range(LIST_ENTRIES):
            entry = self.entries + k * self.esize
            entries.append([self.rd_word(entry), self.label(self.rd_ptr(entry + self.pointer_at))])
        return [[(p - self.entries) // self.esize for p in head], entries]


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"
    pfmt = ">I"
    psize = 4
    esize = 8
    pointer_at = 4
    name_at = 4

    def __init__(self, e, case):
        self.e = e
        e.plan = wh_bytes(case["wh"])
        self.make(case)
        self.L = e.cl = CL.PpcLog(dict(self.names), case["queues"])
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

    def pool_objects(self, rnd):
        base = self.alloc(POOL * MAC_OBJECT)
        self.write(base, rnd.randbytes(POOL * MAC_OBJECT))
        return [base + k * MAC_OBJECT for k in range(POOL)]

    def put_vptr(self, obj):
        self.words(obj, [VTABLE])

    def put_extras(self, case):
        self.words(ADDR["nav"], [NAV_TVECTOR])

    def set_target(self, address, target):
        self.words(address + 0x30, [target])           # the Mac's pointer to the target

    def target_at(self, address):
        return self.rd_word(address + 0x30)

    def invoke(self, op, args):
        r3, _, _ = T.call_ppc(self.e, ENTRY[op], args)
        return r3 & MASK


class Host(Side):
    """The same on the host library."""
    fmt = "<I"
    pfmt = "<Q"
    psize = 8
    esize = HOST_LAYOUT[2]
    pointer_at = HOST_LAYOUT[3]
    name_at = HOST_LAYOUT[5]

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make(case)
        plan = self.alloc(PATH_SIZE)
        self.write(plan, wh_bytes(case["wh"]))
        lib.tpw_vph_set_source(plan)
        CL.host_begin(lib, {label: address for address, label in self.names.items()}, case["queues"])

    def done(self):
        self.lib.tpw_vph_set_source(0)

    def g(self, name):
        return self.lib.tpw_vph_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_vph_alloc(n)       # a zeroed block of the host arena (its address is a 64-bit host pointer: never masked to 32 bits)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def pool_objects(self, rnd):
        rnd.randbytes(POOL * MAC_OBJECT)            # the Mac's pool takes this much noise; the stream stays the same on both sides
        return [self.lib.tpw_vph_behaviour(k) for k in range(POOL)]

    def put_vptr(self, obj):
        pass

    def put_extras(self, case):
        pass

    def set_target(self, address, target):
        self.put_ptr(address + HOST_LAYOUT[0], target)      # the host's own member

    def target_at(self, address):
        return self.rd_ptr(address + HOST_LAYOUT[0])

    def invoke(self, op, args):
        restype, argtypes = SIGNATURES[op]
        converted = [s32(a) if t is I32 else a for t, a in zip(argtypes, args)]
        answer = getattr(self.lib, "tpw_vph_" + op)(*converted) or 0
        return answer if op == "pilot_ctor" else answer & MASK        # the constructor answers a host pointer (compared as a label), not a 32-bit number


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_vph_addr.restype, lib.tpw_vph_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_vph_alloc.restype, lib.tpw_vph_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_vph_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_vph_set_source.restype, lib.tpw_vph_set_source.argtypes = None, [C.c_uint64]
    lib.tpw_vph_behaviour.restype, lib.tpw_vph_behaviour.argtypes = C.c_uint64, [C.c_int32]
    for op, (restype, argtypes) in SIGNATURES.items():
        function = getattr(lib, "tpw_vph_" + op)
        function.restype, function.argtypes = restype, argtypes
    out = (C.c_int64 * 32)()
    n = lib.tpw_vph_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world and the call


def arguments(s, case):
    """The call's arguments, the Mac's order (a Vector2d answer goes through `out`, r3 on the Mac)."""
    op, a = case["op"], case["args"]
    if op == "follow_path":
        return [s.out, s.pool[0], s.veh]
    if op == "recalc_path":
        return [s.veh, s.veh + 0x4C if a["alias"] else s.target, a["mode"] & MASK]
    if op == "init_behaviours":
        return []
    if op == "map_global_to_local":
        return [s.out, a["x"] & MASK, a["y"] & MASK]
    if op == "map_fn":
        return [a["x"] & MASK, a["y"] & MASK, s.mapdata if a["data"] else 0]
    return [s.veh]                 # next_step, is_stuck, pilot_ctor


def execute(s, case):
    """The call; the constructor's answer is the label of what it returns."""
    if case["op"] == "recalc_path" and not case["args"]["alias"]:
        s.words(s.target, case["args"]["target"])
    answer = s.invoke(case["op"], arguments(s, case))
    if case["op"] == "pilot_ctor":
        return s.label(answer)
    return answer if case["op"] in RETURNS else 0


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides."""
    return {
        "answer": answer,
        "log": s.log(),
        "out": s.rd_words(s.out, 2),
        "target": s.rd_words(s.target, 2),
        "veh": s.get_vehicle(s.veh),
        "path": s.read(s.g("path"), PATH_SIZE),
        "globals": [s.rd_word(s.g("one")), s.rd_word(s.g("zero")), s.label(s.rd_ptr(s.g("mapdata"))), s.label(s.rd_ptr(s.g("world")))],
        "list": s.get_list(),
        "table": s.get_table(),
        "names": [s.read(obj + s.name_at, 0x20) for obj in s.pool],
        "guards": [s.guards(a) for a in (s.out, s.target, s.veh, s.mapdata, s.world, s.entries)],
    }


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

EDGE = [0, 1, -1, 3, 0xFFF, -0xFFF, 0x1000, -0x1000, 0x3FFFFF, -0x3FFFFF, 0x400000, -0x400000, 0x10000, -0x10000, INT_MAX, INT_MIN, 0x12345, -0x54321]
U32_EDGES = [0, 1, 0xFFFF, 0x10000, 0x1FFFF, 0x7FFFFFFF, 0x80000000, 0x80010000, 0xFFFF0000, 0xFFFFFFFF, 0x12345678, 0x00FF0000]
S32_EDGES = [0, 1, 0x7F, 0x80, 0xFF, 0x100, -1, -0x80, 0x7FFF, 0x8000, 0xFFFF, 0x10000, INT_MAX, INT_MIN, 0x1234, -0x1234]
WANTED = ["avoid_walls", "follow_path", "separation"]
DECOYS = ["avoid_wall", "avoid_walls2", "Avoid_walls", "follow_path ", "", "separation_", "follow", "seek", "arrival", "x" * 31, "avoid_walls\x7f", "sepArAtion"]
DEFAULT_NAMES = WANTED + ["other%d" % k for k in range(POOL - 3)]


def rv(r):
    """A random 16.16 number: now an edge value, mostly across the length code's ranges (below 0x1000, below 0x400000, the rest)."""
    k = r.random()
    if k < 0.2:
        return r.choice(EDGE)
    if k < 0.5:
        return r.randrange(-0x2000, 0x2000)
    if k < 0.8:
        return r.randrange(-0x800000, 0x800000)
    return r.randrange(INT_MIN, INT_MAX + 1)


def queues(**more):
    """The queues by their short names (QUEUE_KEYS); a number is a queue of one."""
    return {QUEUE_KEYS[key]: list(value) if isinstance(value, (list, tuple)) else [value] for key, value in more.items()}


def veh_default(**over):
    """A vehicle on its way: three waypoints on a line, the first not reached, the path 3/4 long, no history, not stuck."""
    f = {"radius": 0x3333, "pos": (0x180000, 0x240000), "vel": (0, 0), "force": 0x6666, "speed": 0x3333, "fwd": (0x10000, 0), "side": (0, 0x10000),
         "stamp": 100, "dest": (0x400000, 0x240000), "num": 3, "loaded": 3, "wp": 0, "flag": 0,
         "wps": [(0x200000, 0x240000), (0x300000, 0x240000), (0x400000, 0x240000), (0, 0), (0, 0)], "legs": [0x100000, 0x100000, 0, 0, 0],
         "total": 0x300000, "leg_a": 0x200000, "leg_b": 0, "last": 0, "hist": 0, "nav": 0, "unreach": 0}
    f.update(over)
    return f


def hist_random(r):
    """A stuck history: k of the bits 0 to 14 set (k anywhere from 0 to 15), the upper bits random now and then."""
    k = r.randrange(0, 16)
    value = 0
    for bit in r.sample(range(15), k):
        value |= 1 << bit
    if r.random() < 0.4:
        value |= r.getrandbits(17) << 15
    return value


def veh_random(r):
    """A vehicle of random fields; the waypoint index stays within 0 to 11 (inside the vehicle block; the original does not check it either)."""
    pos = (r.randrange(0, 0x800000), r.randrange(0, 0x800000)) if r.random() < 0.8 else (rv(r), rv(r))
    num = r.randrange(0, 9)
    loaded = min(num, 5) if r.random() < 0.8 else r.randrange(0, 7)
    wp = max(0, min(r.choice([0, 0, 1, 2, num - 1, loaded - 1, loaded, r.randrange(0, 9)]), 11))
    wps = [(s32(pos[0] + r.randrange(-0x80000, 0x80000)), s32(pos[1] + r.randrange(-0x80000, 0x80000))) if r.random() < 0.8 else (rv(r), rv(r))
           for _ in range(5)]
    dest = wps[min(max(num - 1, 0), 4)] if r.random() < 0.6 else (s32(pos[0] + r.randrange(-0x80000, 0x80000)), s32(pos[1] + r.randrange(-0x80000, 0x80000)))
    return veh_default(
        radius=r.choice([0x3333, 0x3333, 0x10000, 0x8000, 0x1000, 1, 0, -0x3333, rv(r)]), pos=pos,
        vel=(r.randrange(-0x4000, 0x4000), r.randrange(-0x4000, 0x4000)) if r.random() < 0.7 else (rv(r), rv(r)),
        force=r.choice([0x6666, 0x6666, 0x10000, 0, rv(r)]), speed=r.choice([0x3333, 0x3333, 0x8000, 0, rv(r)]),
        fwd=(rv(r), rv(r)) if r.random() < 0.3 else (0x10000, 0), side=(rv(r), rv(r)) if r.random() < 0.3 else (0, 0x10000),
        stamp=r.choice([0, 1, 100, 0xFFFFFFFF, 0x80000000, r.getrandbits(32)]), dest=dest, num=num, loaded=loaded, wp=wp,
        flag=r.choice([0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 0xFF]), wps=wps,
        legs=[r.randrange(0, 0x180000) for _ in range(5)], total=r.choice([0, r.randrange(1, 0x800000), rv(r)]),
        leg_a=r.choice([r.randrange(0, 0x400000), rv(r)]), leg_b=r.choice([0, r.randrange(0, 0x400000), rv(r)]),
        last=r.choice([0, r.randrange(-0x10000, 0x10000), rv(r)]), hist=hist_random(r), nav=r.choice([0, 0, 1, 2, rv(r)]), unreach=r.choice([0, 0, 1, 2]),
        noise=r.choice([0, 0, 0, 1, 2]))


def wh_default(r, count=3, points=None):
    return {"count": count, "points": points if points is not None else [(5, 5), (6, 5), (7, 6)], "seed": r.getrandbits(32)}


def wh_random(r):
    count = r.choice([0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 7, 8, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF, r.randrange(0, 256)])
    return {"count": count, "points": [(r.randrange(0, 256), r.randrange(0, 256)) for _ in range(min(count, 8))], "seed": r.getrandbits(32)}


def weight_random(r):
    return r.choice([0x10000, 0xE666, 0x8000, 0x1999, 0, -0x8000, rv(r)])


def list_default(data=3, top=3, end=15):
    return {"data": data, "top": top, "end": end, "entries": [(0x1000 + k, k % POOL) for k in range(LIST_ENTRIES)]}


def list_random(r):
    data = r.randrange(0, 6)
    top = data + r.choice([0, 1, 1, 2, 2, 3, 3, 4, 6, 10])
    end = min(r.choice([top, top + 1, data + r.randrange(0, 11), r.randrange(0, LIST_ENTRIES)]), LIST_ENTRIES - 1)
    return {"data": data, "top": top, "end": end, "entries": [(weight_random(r), r.randrange(POOL)) for _ in range(LIST_ENTRIES)]}


def pool_random(r):
    """The table (8 pool indexes) and the 12 names: the wanted names usually somewhere in the table, decoys and others elsewhere."""
    table = [r.randrange(POOL) for _ in range(8)]
    names = []
    for _ in range(POOL):
        k = r.random()
        names.append(r.choice(WANTED) if k < 0.3 else r.choice(DECOYS) if k < 0.6 else "b%d" % r.randrange(100))
    if r.random() < 0.7:
        for name in WANTED:
            if r.random() < 0.85:
                names[table[r.randrange(8)]] = name
    return table, names


def queues_random(r, veh):
    """The answers of the fakes: the stamp around the vehicle's (a replan now and then), the plan found or not, valid squares, nav rules, the forces."""
    stamp = veh["stamp"] + r.choice([-1, 0, 0, 0, 1, 1, 5]) if r.random() < 0.85 else r.getrandbits(32)
    return queues(stamp=stamp & MASK, newstamp=r.choice([0, 1, 100, -1, rv(r)]),
                  valid=[r.choice([1, 1, 1, 1, 0, 2, -1]) for _ in range(2)], nav=[r.choice([1, 1, 0, 2, -1, 0x100]) for _ in range(2)],
                  wh=r.choice([0x70000000, 0x70000000, 1, 1, 1, 0, 0x6FFFFFFF, 0x70000001, rv(r)]),
                  fx=[r.choice([0, rv(r), r.randrange(-0x20000, 0x20000)]) for _ in range(4)],
                  fy=[r.choice([0, rv(r), r.randrange(-0x20000, 0x20000)]) for _ in range(4)])


def base(r, op):
    """The world every edge case starts from: the globals at their usual values, a default vehicle, the wanted names at table slots 0 to 2, an empty list."""
    return {"op": op, "fill": r.getrandbits(32), "queues": {}, "one": 0x10000, "zero": 0, "world": True, "target_set": True, "veh": veh_default(),
            "list": list_default(), "table": list(range(8)), "names": list(DEFAULT_NAMES), "wh": wh_default(r), "args": {}}


def case(r, op, **over):
    c = base(r, op)
    c.update(over)
    return c


def world(r, op):
    """base() with random globals, vehicle, list, table, plan and answers: the world of the random cases."""
    c = base(r, op)
    c["zero"] = 0 if r.random() < 0.85 else r.choice([0x6666, 0x7000, 5, rv(r)])
    c["one"] = 0x10000 if r.random() < 0.85 else r.choice([0, 0x8000, -0x10000, 0x20000, rv(r)])
    c["world"] = r.random() < 0.97
    c["target_set"] = r.random() < 0.8
    c["veh"] = veh_random(r)
    c["list"] = list_random(r)
    c["table"], c["names"] = pool_random(r)
    c["wh"] = wh_random(r)
    c["queues"] = queues_random(r, c["veh"])
    return c


def rnd_case(r, op, **over):
    c = world(r, op)
    c.update(over)
    return c


def moved(v, dx, dy):
    return (s32(v[0] + dx), s32(v[1] + dy))


# ---- map_global_to_local
def global_to_local_edges():
    r = random.Random(201)
    for x in U32_EDGES:
        for y in U32_EDGES:
            yield case(r, "map_global_to_local", args={"x": x, "y": y})


def global_to_local_random(r):
    pick = lambda: r.choice(U32_EDGES) if r.random() < 0.2 else r.getrandbits(32)
    return rnd_case(r, "map_global_to_local", args={"x": pick(), "y": pick()})


# ---- map_fn
def map_fn_edges():
    r = random.Random(202)
    for x in S32_EDGES:
        for y in S32_EDGES:
            yield case(r, "map_fn", args={"x": x, "y": y, "data": True}, queues=queues(valid=1))
    for answer in (0, 1, 2, -1, 0x100, 0x10000, INT_MIN, INT_MAX, 0xFF):
        for x, y in ((0, 0), (5, 7), (-1, 3), (0x7F, 0x7F), (INT_MAX, INT_MAX), (0x1234, 0x4321)):
            yield case(r, "map_fn", args={"x": x, "y": y, "data": answer != 0}, queues=queues(valid=answer))


def map_fn_random(r):
    pick = lambda: r.choice(S32_EDGES) if r.random() < 0.3 else r.randrange(-0x200, 0x400) if r.random() < 0.5 else r.randrange(INT_MIN, INT_MAX + 1)
    return rnd_case(r, "map_fn", args={"x": pick(), "y": pick(), "data": r.random() < 0.8}, queues=queues(valid=r.choice([1, 1, 0, 2, -1, 0x100])))


# ---- is_stuck
def stuck_veh(hist, **over):
    return veh_default(hist=hist, **over)


def is_stuck_edges():
    r = random.Random(203)
    op = "is_stuck"
    for n in range(16):                                          # n low bits set: the fraction n / 15 (bit 15 is not counted)
        yield case(r, op, veh=stuck_veh((1 << n) - 1))
        yield case(r, op, veh=stuck_veh(((1 << n) - 1) << 1))
    for hist in (0, 0x7FFF, 0xFFFF8000, 0xFFFFFFFF, 0x8000, 0x80007FFF, 0x4000, 0x5555, 0x2AAA, 0x7FFE, 0x80000000, 0x40000000, 0x12345678):
        yield case(r, op, veh=stuck_veh(hist))
    finished = [dict(flag=1), dict(total=0), dict(flag=2), dict(flag=0xFF), dict(leg_a=0, leg_b=0, wp=0, pos=(0x200000, 0x240000)),
                dict(leg_a=0, leg_b=0, wp=1, pos=(0x300000, 0x240000)), dict(total=1), dict(total=-0x300000), dict(leg_a=INT_MAX, leg_b=INT_MAX)]
    for over in finished:                                        # a finished path or progress exactly 1.0 answers gVehZero, whatever the history
        for zero in (0, 0x1234):
            yield case(r, op, veh=stuck_veh(0x7FFF, **over), zero=zero)
    for one in (0x8000, 0, -0x10000, 0x20000):                   # progress against another gVehOne
        for over in ({}, dict(flag=1), dict(total=0)):
            yield case(r, op, veh=stuck_veh(0x3F, **over), one=one)
    for wp in range(5):
        yield case(r, op, veh=stuck_veh(0x1F, wp=wp))
    yield case(r, op, veh=stuck_veh(0x7FFF), world=False)


def is_stuck_random(r):
    c = rnd_case(r, "is_stuck")
    v = c["veh"]
    if r.random() < 0.6:
        v["wp"] = r.randrange(0, 5)
        v["flag"] = r.choice([0, 0, 0, 1, 2])
    return c


# ---- Pilot::Pilot
def pilot_ctor_edges():
    r = random.Random(204)
    for noise in (0, 1, 2):
        for one in (0x10000, 0x8000, 0, -1, INT_MIN):
            for target_set in (True, False):
                yield case(r, "pilot_ctor", veh={"noise": noise}, one=one, target_set=target_set)
    for zero in (0, 0x1234):
        yield case(r, "pilot_ctor", veh={"noise": 0}, zero=zero, world=False)


def pilot_ctor_random(r):
    c = rnd_case(r, "pilot_ctor", veh={"noise": r.choice([0, 0, 0, 1, 2])})
    return c


# ---- Pilot::init_behaviours
def init_case(r, table, names, lst):
    return case(r, "init_behaviours", table=table, names=names, list=lst)


def init_list(data=2, top=5, end=14, r=None):
    lst = list_default(data, top, end)
    return lst


def init_behaviours_edges():
    r = random.Random(205)
    op = "init_behaviours"
    # the wanted names at every combination of slots (a permutation of three of the eight), the others filled with decoys
    arrangements = [(0, 1, 2), (2, 1, 0), (7, 6, 5), (5, 6, 7), (0, 3, 7), (7, 3, 0), (1, 0, 2), (4, 4 + 1, 4 + 2), (3, 2, 1)]
    for slots in arrangements:
        table = list(range(8))
        names = list(DEFAULT_NAMES)
        for slot, name in zip(slots, WANTED):
            names[table[slot]] = name
        for k in range(8):
            if k not in slots:
                names[table[k]] = DECOYS[k % len(DECOYS)]
        yield init_case(r, table, names, list_default(2, 5, 14))
    # each wanted name missing, in every combination
    for mask in range(8):
        names = list(DEFAULT_NAMES)
        for k, name in enumerate(WANTED):
            names[k] = name if mask & (1 << k) else DECOYS[k]
        yield init_case(r, list(range(8)), names, list_default(2, 5, 14))
    # near misses only, then duplicates (the first match wins), then one object in every slot
    for decoy in DECOYS:
        names = list(DEFAULT_NAMES)
        names[0], names[1], names[2] = decoy, decoy, decoy
        yield init_case(r, list(range(8)), names, list_default(2, 5, 14))
    names = list(DEFAULT_NAMES)
    names[3], names[5], names[1] = "follow_path", "follow_path", "decoy"
    yield init_case(r, [0, 1, 2, 3, 4, 5, 6, 7], names, list_default(0, 0, 10))
    yield init_case(r, [4, 3, 1, 3, 5, 2, 3, 0], names, list_default(0, 0, 10))
    for slot in range(8):
        for name in WANTED:
            names = list(DEFAULT_NAMES)
            names[0] = name
            yield init_case(r, [0] * 8, names, list_default(1, 1, 8))
    # the list: data anywhere, top below, at and above it, the end reached after 0, 1, 2 or 3 pushes (or never)
    for data, top, end in ((0, 0, 10), (0, 7, 10), (4, 4, 4), (4, 4, 5), (4, 4, 6), (4, 4, 7), (4, 4, 8), (2, 9, 5), (5, 3, 6), (6, 6, 5), (3, 3, 0), (3, 3, 15),
                           (0, 0, 1), (0, 0, 2), (0, 0, 3), (12, 12, 15), (12, 14, 14), (3, 9, 3)):
        yield init_case(r, list(range(8)), list(DEFAULT_NAMES), list_default(data, top, end))
    # the same object found twice and the table naming one object in every slot
    names = list(DEFAULT_NAMES)
    names[0] = "avoid_walls"
    yield init_case(r, [0] * 8, names, list_default(2, 2, 14))


def init_behaviours_random(r):
    return rnd_case(r, "init_behaviours", list=list_random(r))


# ---- Vehicle::recalc_path
def recalc_args(mode=0, target=(0x300000, 0x340000), alias=False):
    return {"mode": mode, "target": target, "alias": alias}


def recalc_edges():
    r = random.Random(206)
    op = "recalc_path"
    for answer in (0x70000000, 0, 1, 0x6FFFFFFF, 0x70000001, -1, INT_MIN, INT_MAX, 0x7000, 0xF0000000):
        for mode in (0, 1):
            yield case(r, op, queues=queues(wh=answer, newstamp=77), args=recalc_args(mode))
    for mode in (0, 1, 2, -1, 0x100, 0x101, 0x10001, 0x80000001, INT_MIN, INT_MAX, 0xFF):
        yield case(r, op, queues=queues(newstamp=5), args=recalc_args(mode))
    for count in (0, 1, 2, 3, 4, 5, 6, 7, 8, 0x80, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF):
        for mode in (0, 1):
            yield case(r, op, queues=queues(newstamp=count), args=recalc_args(mode), wh=wh_default(r, count, [(k * 3 + 1, k * 5 + 2) for k in range(8)]))
    for pos, target in (((0, 0), (0, 0)), ((INT_MIN, INT_MAX), (INT_MAX, INT_MIN)), ((0x80FF0000, 0x7F010000), (0xFF0000, -0x10000)), ((-1, -1), (-1, -1)),
                        ((0xFF0000, 0xFF0000), (0x1000000, 0x1230000)), ((0x7FFFFFFF, 0x80000000), (0x12345678, 0x9ABCDEF0))):
        for mode in (0, 1, 2):
            v = veh_default(pos=(s32(pos[0]), s32(pos[1])))
            yield case(r, op, veh=v, args=recalc_args(mode, (s32(target[0]), s32(target[1]))), wh=wh_default(r, 4, [(255, 255), (0, 0), (128, 127), (1, 254)]))
    for mode in (0, 1):                                          # the target is the vehicle's own destination (Follow_Path passes it so)
        yield case(r, op, args=recalc_args(mode, alias=True), veh=veh_default(dest=(0x555555, -0x333333)))
    for nav in (0, 1, 2, -1, 0x1234):
        yield case(r, op, veh=veh_default(nav=nav), args=recalc_args(1))
    for hist in (0, 0xFFFFFFFF):                                 # the plan resets the history, the progress, the flag and the unreachable mark
        for unreach in (0, 1, 7):
            yield case(r, op, veh=veh_default(hist=hist, last=0x1234, flag=1, wp=3, unreach=unreach, total=77, leg_a=88, leg_b=99), args=recalc_args(1))
    yield case(r, op, world=False, args=recalc_args(0), queues=queues(newstamp=INT_MIN))
    yield case(r, op, world=False, args=recalc_args(1, alias=True), queues=queues(wh=0x70000000))
    for pos in ((0x2C0000, 0x290000), (0x2D0000, 0x2A0000)):    # a plan that runs through the vehicle's own square (mode 1 puts it in front)
        yield case(r, op, veh=veh_default(pos=pos), args=recalc_args(1), wh=wh_default(r, 6, [(44, 41), (45, 41), (46, 42), (47, 42), (48, 43), (49, 43)]))


def recalc_random(r):
    c = rnd_case(r, "recalc_path")
    c["args"] = recalc_args(r.choice([0, 1, 1, 2, -1, rv(r)]), (rv(r), rv(r)), r.random() < 0.15)
    if r.random() < 0.5:                                         # a short walk near the vehicle, in map squares
        px, py = c["veh"]["pos"][0] >> 16, c["veh"]["pos"][1] >> 16
        c["wh"]["points"] = [((px + k) & 0xFF, (py + (k >> 1)) & 0xFF) for k in range(8)]
    return c


# ---- Vehicle::next_step
def step_case(r, forces=(), weights=(), max_force=0x6666, max_speed=0x3333, pos=(0x180000, 0x240000), vel=(0, 0), data=2, **more):
    """A list of len(weights) entries from entry `data` on (the behaviours' forces come from the queues, one pair per entry)."""
    lst = list_default(data, data + len(weights), 15)
    lst["entries"] = [(weights[k - data] if data <= k < data + len(weights) else 0x1000 + k, k % POOL) for k in range(LIST_ENTRIES)]
    over = dict(force=max_force, speed=max_speed, pos=pos, vel=vel)
    over.update(more.pop("veh", {}))
    q = more.pop("queues", {})
    q = dict(q)
    if forces:
        q.update(queues(fx=[f[0] for f in forces], fy=[f[1] for f in forces]))
    return case(r, "next_step", list=lst, veh=veh_default(**over), queues=q, **more)


def next_step_edges():
    r = random.Random(207)
    forces = [(0, 0), (0x30000, 0x40000), (0x1000, 0x2000), (INT_MAX, INT_MIN), (-0x10000, 0), (0x6666, 0), (0x6667, 0), (3, 4), (INT_MIN, INT_MIN), (0x10000, -0x20000)]
    # no behaviours, one, several (the weights)
    for n in (0, 1, 2, 3, 5, 10):
        for weights in ([0x10000] * n, [0xE666, 0x8000, 0x1999, 0x10000, 0x8000, 0x1999, 0xE666, 0x8000, 0x1999, 0x10000][:n], [0] * n, [-0x10000, 0x7FFFFFFF] * 5):
            yield step_case(r, forces=forces[1:1 + n] or [(0, 0)], weights=weights[:n], vel=(0x100, -0x200))
    for force in forces:
        for max_force in (0, 0x6666, 0x10000, -1, INT_MAX):
            yield step_case(r, forces=[force], weights=[0x10000], max_force=max_force, vel=(0x500, 0x500))
    for weight in (0x10000, 0xE666, 0x8000, 0, -0x10000, INT_MAX, INT_MIN, 1):
        yield step_case(r, forces=[(0x4000, -0x2000)], weights=[weight], vel=(0x100, 0))
    # the summed force is capped again; the velocity is capped; the division by gVehOne
    yield step_case(r, forces=[(0x6666, 0), (0x6666, 0)], weights=[0x10000, 0x10000], max_force=0x6666, max_speed=0x100, vel=(0xFF, 0))
    for one in (0x10000, 0x8000, 0x20000, 0, -0x10000, 1):
        yield step_case(r, forces=[(0x3000, 0x1000)], weights=[0x10000], one=one, vel=(0x100, 0x100))
    for max_speed in (0, 1, 0x100, 0x3333, 0x7FFFFFFF, -5):
        yield step_case(r, forces=[(0x3000, 0x1000)], weights=[0x10000], max_speed=max_speed, vel=(0x2000, 0x1000))
    # the nav rules and the squares: refused and allowed, valid and invalid squares, a position near the wrap, a step across a square boundary
    for nav in (0, 1, 2, -1, 0x100, INT_MIN):
        for valid in (1, 0, 2):
            yield step_case(r, vel=(0x8000, 0x4000), pos=(0x17C000, 0x23F000), queues=queues(nav=nav, valid=[valid, 1]), veh={"hist": 0x5, "nav": 1})
    for valid in ([1, 0], [0, 1], [0, 0], [-1, 2]):
        yield step_case(r, vel=(0x10000, 0x10000), pos=(0x1FFFF0, 0x23FFF0), queues=queues(valid=valid))
    for pos, vel in (((INT_MAX, INT_MAX), (0x1000, 0x1000)), ((0xFFFFF000 - (1 << 32), 0xFFFFFF00 - (1 << 32)), (0x1000, 0x1000)), ((-0x10000, -1), (-1, -0x10000)),
                     ((0x7FFF0000, 0x7FFFFFFF), (0x3333, 0x3333)), ((INT_MIN, INT_MIN), (-0x3333, -0x3333)), ((0xFF0000, 0x1000000), (0x2000, 0))):
        yield step_case(r, pos=pos, vel=vel, queues=queues(valid=1, nav=1))
        yield step_case(r, pos=pos, vel=vel, queues=queues(valid=1, nav=0))
    # the history: the refused step shifts a 1 in, then the progress compare shifts again (bit 31 and 30 fall off)
    for hist in (0, 1, 0x7FFFFFFF, 0x80000000, 0x40000000, 0xC0000000, 0xFFFFFFFF, 0x3FFFFFFF):
        for nav in (0, 1):
            yield step_case(r, vel=(0, 0), queues=queues(nav=nav), veh={"hist": hist})
    # progress against the last one: no movement (no behaviours, velocity zero), the last progress at, one below and one above the progress
    for flag, total, wp in ((0, 0x300000, 0), (0, 0x300000, 2), (1, 0x300000, 0), (0, 0, 1), (2, 0x500000, 1), (0xFF, 0x500000, 3)):
        v = veh_default(flag=flag, total=total, wp=wp)
        p = progress(v, 0x10000)
        for last in (p - 1, p, p + 1, p + 0x10000, -p, INT_MIN, INT_MAX):
            for nav in (1, 0):
                yield step_case(r, queues=queues(nav=nav), veh=dict(flag=flag, total=total, wp=wp, last=s32(last)))
    for one in (0x8000, 0x20000):
        v = veh_default(flag=1)
        p = progress(v, one)
        for last in (p - 1, p, p + 1):
            yield step_case(r, one=one, veh=dict(flag=1, last=last))
    # the forward and side axes follow the velocity (normalise, perp); a velocity of zero
    for vel in ((0, 0), (0x10000, 0), (0, -0x10000), (3, 4), (0x7FFFFFFF, 0x7FFFFFFF), (0x5000, -0x1234), (-1, 0)):
        yield step_case(r, vel=vel, max_speed=0x7FFFFFFF)
    yield step_case(r, world=False, forces=[(0x100, 0x200)], weights=[0x10000], vel=(0x100, 0))
    # entries at the list's far end, mTop = 15
    yield step_case(r, forces=[(1, 2)] * 10, weights=[0x1999] * 10, data=5, vel=(0x10, 0x20))


def next_step_random(r):
    c = rnd_case(r, "next_step")
    v = c["veh"]
    if r.random() < 0.5:                                         # a vehicle that moves within a few squares, so the squares and the step are meaningful
        v["vel"] = (r.randrange(-0x6000, 0x6000), r.randrange(-0x6000, 0x6000))
        v["speed"] = r.choice([0x3333, 0x8000, 0x10000])
        v["force"] = r.choice([0x6666, 0x10000, 0x2000])
    if r.random() < 0.25:                                        # no behaviours: only the velocity and the progress
        c["list"]["top"] = c["list"]["data"]
    if r.random() < 0.3:                                         # the last progress at, one below or one above the progress the step will find
        v["wp"] = r.randrange(0, 5)
        if c["list"]["top"] == c["list"]["data"]:
            v["vel"] = (0, 0)
            v["last"] = s32(progress(v, c["one"]) + r.choice([-1, 0, 0, 1]))
    return c


# ---- Follow_Path::calculate
def fp_case(r, **over):
    return case(r, "follow_path", **over)


def near(pos, dx, dy=0):
    return (s32(pos[0] + dx), s32(pos[1] + dy))


def follow_path_edges():
    r = random.Random(208)
    # the stamp of the ground against the path's: unsigned compare
    for stamp, answer in ((100, 100), (100, 101), (100, 99), (0, 0xFFFFFFFF), (0xFFFFFFFF, 0), (0x7FFFFFFF, 0x80000000), (0x80000000, 0x7FFFFFFF), (0, 0), (5, 6), (6, 5)):
        for wh in (1, 0x70000000, 0):
            yield fp_case(r, veh=veh_default(stamp=stamp), queues=queues(stamp=answer, wh=wh, newstamp=7))
    # the replan after the stamp: the vehicle is on its new plan (the waypoints read afterwards are the new ones)
    for count in (0, 1, 2, 3, 5, 6, 0xFC, 0xFF):
        yield fp_case(r, veh=veh_default(stamp=1, wp=2, flag=1), queues=queues(stamp=2, newstamp=9), wh=wh_default(r, count, [(20, 20), (21, 20), (22, 21), (23, 21), (24, 22), (25, 22)]))
    # stuck: 4, 5, 6, 7 and 15 bits of the history among bits 0 to 14, with and without bits above; a finished path or progress 1.0 is never stuck
    for ones in (0, 4, 5, 6, 7, 15):
        for upper in (0, 0xFFFF8000):
            for wh in (1, 0x70000000):
                yield fp_case(r, veh=veh_default(hist=((1 << ones) - 1) | upper), queues=queues(wh=wh, newstamp=3))
        yield fp_case(r, veh=veh_default(hist=((1 << ones) - 1) << 8), queues=queues(newstamp=4))
        yield fp_case(r, veh=veh_default(hist=(1 << ones) - 1, flag=1))
        yield fp_case(r, veh=veh_default(hist=(1 << ones) - 1, total=0))
    for zero in (0x6666, 0x7000, 0x6665):                         # progress at 1.0 returns gVehZero, which the stuck test then never sees; else the zero is not used
        yield fp_case(r, veh=veh_default(hist=0), zero=zero)
        yield fp_case(r, veh=veh_default(hist=0, flag=1), zero=zero)
    for one in (0x8000, 0, 0x20000):
        yield fp_case(r, veh=veh_default(hist=0x3F), one=one)
    # no waypoints, a finished path (flag 1, 2, 0xff), the brake
    for num in (0, 1, 3):
        for flag in (0, 1, 2, 0xFF):
            yield fp_case(r, veh=veh_default(num=num, flag=flag, vel=(0x1234, -0x4321)))
    yield fp_case(r, veh=veh_default(num=0, flag=1, vel=(INT_MIN, INT_MAX)))
    # the last waypoint: the distance to the destination against 0.1 * radius (0x19999) at, below and above
    for radius in (0x3333, 0x10000, 0x8000, 0, -0x3333, 0x7FFFFFFF):
        edge = fixmul(0x19999, radius)
        for k in (-2, -1, 0, 1, 2):
            pos = (0x180000, 0x240000)
            d = s32(edge + k)
            for dx, dy in ((d, 0), (0, d), (-d, 0), (d // 2, d - d // 2), (d, -d)):
                yield fp_case(r, veh=veh_default(radius=radius, pos=pos, dest=near(pos, dx, dy), num=3, wp=2, vel=(0x100, -0x100), last=1))
    # a waypoint that is not the last: the distance against 2 * radius (0x20000) at, below and above; the step leaves the leg off mPathLegA
    for radius in (0x3333, 0x10000, 0x8000, 0, -0x3333):
        edge = fixmul(0x20000, radius)
        for k in (-2, -1, 0, 1, 2):
            pos = (0x180000, 0x240000)
            d = s32(edge + k)
            for wp in (0, 1):
                wps = [near(pos, d, 0), near(pos, 0, d), (0x400000, 0x240000), (0, 0), (0, 0)]
                yield fp_case(r, veh=veh_default(radius=radius, pos=pos, wps=wps, wp=wp, num=4, loaded=4, leg_a=0x200000))
    # the step that reaches the loaded count replans with mode 0 (the real recalc_path, the case's plan) and the target is read again
    for loaded, num, wp in ((3, 3, 1), (3, 8, 2), (5, 8, 4), (5, 5, 3), (2, 6, 1), (1, 6, 0), (4, 9, 3)):
        for count in (0, 1, 4, 7):
            pos = (0x180000, 0x240000)
            wps = [near(pos, 0x1000 * (k + 1), 0x500) for k in range(5)]
            yield fp_case(r, veh=veh_default(pos=pos, wps=wps, wp=wp, num=num, loaded=loaded), queues=queues(newstamp=12 + count),
                          wh=wh_default(r, count, [(30 + k, 31 - k) for k in range(8)]))
    # the force at a waypoint that is not the last (seek: (target - pos) * gVehOne - velocity) and at the last (arrival: the speed is the smaller of mMaxSpeed and length / 2)
    for one in (0x10000, 0x8000, 0x20000, 0, -0x10000):
        yield fp_case(r, one=one, veh=veh_default(vel=(0x123, -0x456)))
    for speed in (0x7FF, 0x800, 0x801, 0x100, 0, -1):
        for length in (0, 1, 0xFFE, 0xFFF, 0x1000, 0x1001, 0x10FF, 0x1100, 0x6666, 0x40000, 0x400001, 0x7FFFFF):
            pos = (0x180000, 0x240000)
            yield fp_case(r, veh=veh_default(speed=speed, pos=pos, dest=near(pos, length, 0), num=2, wp=1, radius=0x3333, vel=(0x10, 0x20)))
            yield fp_case(r, veh=veh_default(speed=speed, pos=pos, dest=near(pos, -length, length // 3), num=2, wp=1, radius=0, vel=(-5, 7)))
    for zero, length in ((5, 5), (5, 0), (0, 0), (0x1000, 0x1000), (-7, 0)):
        pos = (0x180000, 0x240000)
        yield fp_case(r, zero=zero, veh=veh_default(pos=pos, dest=near(pos, length, 0), num=2, wp=1, radius=0, vel=(3, 4)))
    # the waypoint index and the count: wp == num - 1 or not, wp past the count, wp 5 to 11 (the padding after the waypoints), num 1, num below wp
    for num, wp in ((1, 0), (2, 1), (2, 0), (5, 4), (5, 3), (3, 5), (3, 6), (4, 9), (0, 0), (6, 5), (2, 11), (8, 7), (7, 8)):
        yield fp_case(r, veh=veh_default(num=num, wp=wp, loaded=min(num, 5), pad=[0x1234, -0x5678, 0x2000, 0x3000, 0x4000], legs=[0x100, 0x200, 0x300, 0x400, 0x500]))
    yield fp_case(r, world=False, veh=veh_default(stamp=1), queues=queues(stamp=2, newstamp=3))
    yield fp_case(r, veh=veh_default(wp=1, wps=[(0, 0), (INT_MAX, INT_MIN), (0, 0), (0, 0), (0, 0)], pos=(INT_MIN, INT_MAX)))
    yield fp_case(r, veh=veh_default(wp=2, num=2, dest=(INT_MIN, INT_MIN), pos=(INT_MAX, INT_MAX), vel=(INT_MIN, INT_MIN)))


def follow_path_random(r):
    c = rnd_case("follow_path" and r, "follow_path")
    v = c["veh"]
    k = r.random()
    if k < 0.45:                                                  # a waypoint at the 2 * radius distance (or 0.1 * radius at the last one), at, below, above
        radius = v["radius"]
        wp = v["wp"] if v["wp"] < 5 else 0
        last = v["wp"] == s32(v["num"] - 1)
        edge = fixmul(0x19999 if last else 0x20000, radius)
        d = s32(edge + r.choice([-1, 0, 0, 1]))
        shift = r.choice([(d, 0), (0, d), (-d, 0), (d // 2, d - d // 2)])
        if last:
            v["dest"] = near(v["pos"], *shift)
        else:
            wps = list(v["wps"])
            wps[wp] = near(v["pos"], *shift)
            v["wps"] = wps
    elif k < 0.6:                                                 # the last waypoint far or near, with a speed that straddles length / 2
        v["wp"], v["num"] = r.randrange(0, 5), 0
        v["num"] = v["wp"] + 1
        length = r.choice([0, r.randrange(0, 0x1000), r.randrange(0x1000, 0x400000), r.randrange(0x400000, 0x2000000)])
        v["dest"] = near(v["pos"], r.choice([length, -length]), r.choice([0, length // 2, -length // 3]))
        v["speed"] = r.choice([length // 2 - 1, length // 2, length // 2 + 1, 0x3333, rv(r)])
    if r.random() < 0.3:                                          # a stamp newer than the path's: a replan
        c["queues"].update(queues(stamp=(v["stamp"] + r.choice([1, 2, 1000])) & MASK))
    return c


TESTS = [
    FnTest("Follow_Path::calculate", ppc, host, edge=follow_path_edges, rnd=follow_path_random),
    FnTest("Vehicle::next_step", ppc, host, edge=next_step_edges, rnd=next_step_random),
    FnTest("Vehicle::recalc_path", ppc, host, edge=recalc_edges, rnd=recalc_random),
    FnTest("Vehicle::is_stuck", ppc, host, edge=is_stuck_edges, rnd=is_stuck_random),
    FnTest("Pilot::init_behaviours", ppc, host, edge=init_behaviours_edges, rnd=init_behaviours_random),
    FnTest("Pilot::Pilot", ppc, host, edge=pilot_ctor_edges, rnd=pilot_ctor_random),
    FnTest("map_global_to_local", ppc, host, edge=global_to_local_edges, rnd=global_to_local_random),
    FnTest("map_fn", ppc, host, edge=map_fn_edges, rnd=map_fn_random),
]
