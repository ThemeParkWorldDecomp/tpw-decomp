"""Unit w203, against libtpw_unit_VehicleSteer.dylib (tests/unit_VehicleSteer.cpp), main app ("SimTheme Park"), src/game/rides/vehicle.cpp:
  vector helpers    perp 0x1013380c, normalise 0x1013384c, truncate 0x101339f4
  Vehicle           point_to_local 0x101333c4, vec_to_global 0x101334a4, predict_pos 0x10133b58, is_finished_path 0x101341a4,
                    clear_behaviours 0x10133358, ~Vehicle 0x101332cc
  behaviours        Seek::calculate 0x10131788, Arrival::calculate 0x1013181c, Separation::calculate 0x10131c64,
                    Repel_Obstacle::calculate 0x10131f08, Avoid_Obstacle::calculate 0x101320e0 and their callbacks do_separation 0x10131a7c,
                    do_repel_obstacle 0x10131dcc, do_avoid_obstacle 0x10131fc8
  Pilot             reset 0x101347b0, new_target 0x101347ec, move 0x10134844, get_pos 0x10134888, set_speed 0x101349cc, is_finished_path 0x101348c4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for the
original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside the 23 functions
is either the real code on both sides (FixMul and FixDiv with the module's 16.16 model, isqrt, xaxis_circle_intersect and the Vector2d operators,
labs) or a fake that records the same event on both sides: the two candidate walks (0x101356b8, 0x1013598c; they add the case's queued numbers to
gVehSteerSum instead of calling the callback, and the obstacle walk sets gVehAvoidNearest for do_avoid_obstacle), Vehicle::recalc_path (0x10133be4),
Vehicle::next_step (0x10133564) and operator delete. The callbacks are tested by direct calls.

The world of a case: the six globals of vehicle.cpp (gVehZero and gVehOne mostly at 0 and 0x10000, now and then another number so that a function
that used the constant instead of the global shows), the behaviour list (16 entries, mData at entry 3, mTop anywhere around it), a vehicle block
(the Mac's 0xc8 bytes between guards; the fields at their Mac offsets, the unknown spans seeded noise, the target pointer at +0x30 on the Mac and in
the host's own member), a second vehicle, an obstacle, a target, an input vector and an answer block. What is compared: the answer, the event log, the
answer block, every block (fields decoded, noise and guards as bytes), the globals and the list (header as entry indexes, entries as weight and
pointer). The Mac's vehicle is 0xc8 bytes and the host's 0xd0 (its mTarget replaces the Mac's word at +0x30 and is appended), so the word at +0x30
and the host's last 8 bytes are not compared. Vehicle::~Vehicle is the Mac's deleting destructor (this, flag): the host forms are the destructor in
place (flag not above zero as a halfword), delete of a heap Vehicle (above zero) and delete of NULL (this NULL); the logs (the delete events) are compared.

There are no fused operations and no value that reaches fctiwz's positive saturation (set_speed clamps its argument to 2.0), so no HardwareRounding.
Not tested: a waypoint index below 0 or above 8 (the original reads the vehicle's other fields or outside it, as does the host; an index of 5 to 8 reads
the five padding words after the waypoints, which are fields of the case, and the totals), a NULL target pointer (both read it), list pointers above
0x80000000 (the Mac compares them unsigned, the host's arena has none), the behaviour objects' vtables and names (no function here reads them).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_VehicleSteer"
MASK = 0xFFFFFFFF
INT_MAX, INT_MIN = 0x7FFFFFFF, -0x80000000

# ---- the unit's own functions (name, address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
FUNCTIONS = [
    ("perp", 0x1013380C, 24), ("normalise", 0x1013384C, 380), ("truncate", 0x101339F4, 312),
    ("point_to_local", 0x101333C4, 168), ("vec_to_global", 0x101334A4, 136), ("predict_pos", 0x10133B58, 96),
    ("clear_behaviours", 0x10133358, 60), ("dtor", 0x101332CC, 104),
    ("seek", 0x10131788, 100), ("arrival", 0x1013181C, 404), ("do_separation", 0x10131A7C, 276), ("separation", 0x10131C64, 308),
    ("do_repel_obstacle", 0x10131DCC, 256), ("repel_obstacle", 0x10131F08, 136), ("do_avoid_obstacle", 0x10131FC8, 220),
    ("avoid_obstacle", 0x101320E0, 384),
    ("pilot_reset", 0x101347B0, 20), ("pilot_new_target", 0x101347EC, 48), ("pilot_move", 0x10134844, 32), ("pilot_get_pos", 0x10134888, 20),
    ("pilot_set_speed", 0x101349CC, 128), ("vehicle_finished", 0x101341A4, 216), ("pilot_finished", 0x101348C4, 216),
]
ENTRY = {name: address for name, address, _ in FUNCTIONS}
SIZES = {address: size for _, address, size in FUNCTIONS}
OWN = set(SIZES)

# ---- the callees faked on both sides (the asm's bl targets that are not OWN) and the TVectors the callbacks are passed by
WALK_VEHICLES, WALK_OBSTACLES = 0x101356B8, 0x1013598C
RECALC_PATH, NEXT_STEP = 0x10133BE4, 0x10133564
TV_SEPARATION, TV_REPEL, TV_AVOID = 0x10266D70, 0x10266D60, 0x10266D50

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_vst_addr(name)
ADDR = dict(zero=0x1034CA90, one=0x1034CA8C, sum=0x1034CA60, range=0x1034CA58, nearest=0x1034CA5C, list=0x1034CA74)
# ---- the host's offsets and sizes the module relies on, checked once against tpw_vst_layout: offsetof(Vehicle, mTarget), sizeof(Vehicle),
# sizeof(BehaviourEntry), offsetof(BehaviourEntry, mBehaviour), sizeof(BehaviourList), sizeof(Obstacle), offsetof(Obstacle, mRadius)
HOST_LAYOUT = [0xC8, 0xD0, 16, 8, 24, 12, 8]

GUARD = 0x20            # noise bytes before and after every block
VEH_SIZE = 0xC8         # the Mac's Vehicle
VEH_BLOCK = 0xD0        # the host's (the last 8 bytes are its mTarget)
LIST_ENTRIES = 16
L_VEHICLE, L_OTHER, L_OBSTACLE, L_TARGET = 1, 2, 3, 4
L_DO_SEPARATION, L_DO_REPEL, L_DO_AVOID = 11, 12, 13

# ---- the Vehicle's fields that are words, by Mac offset; the rest of the 0xc8 bytes (but +0x30) is noise that must come back unchanged
VEHICLE_WORDS = ([0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x54, 0x5C, 0xA0, 0xA4, 0xA8] +
                 [0x64 + 4 * k for k in range(10)] + [0x8C + 4 * k for k in range(5)])      # the last five: the padding after the waypoints
COVERED = {offset + k for offset in VEHICLE_WORDS for k in range(4)} | {0x30, 0x31, 0x32, 0x33}
VEHICLE_WORDS.sort()


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def s32(v):
    return T.s32(v)


s16 = T.s16


class Bits(int):
    """A float argument, given as its single-precision bits."""


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


def ab(v):
    """cmpwi / neg: |v| as a 32-bit word (|INT_MIN| stays 0x80000000)."""
    return (-v) & MASK if v < 0 else v


def length(x, y):
    """The length the source computes in truncate, Arrival and Avoid_Obstacle (logical shifts, a result that may be negative)."""
    ax, ay = ab(x), ab(y)
    bits = ax | ay
    if bits & 0xFFFFF000 == 0:
        return s32(math.isqrt((ax * ax + ay * ay) & MASK))
    if bits & 0xFFC00000 == 0:
        return s32(math.isqrt(((ax >> 8) ** 2 + (ay >> 8) ** 2) & MASK) << 8)
    return s32(math.isqrt(((ax >> 18) ** 2 + (ay >> 18) ** 2) & MASK) << 18)


def xaxis(x, y, r):
    """xaxis_circle_intersect (src/game/ui/vector2d.cpp) as a 32-bit word."""
    a = s32((x >> 18) * (x >> 18) + (y >> 18) * (y >> 18))
    c = s32(a - fixmul(r, r))
    m2x = s32(x * -2)
    disc = s32(fixmul(m2x, m2x) - s32(c * 4))
    if disc <= 0:
        return 0x80000000
    return (s32(x * 2 - math.isqrt(disc)) & MASK) >> 1


def local(f, p):
    """Vehicle::point_to_local of the vehicle fields f."""
    dx, dy = s32(p[0] - f["pos"][0]), s32(p[1] - f["pos"][1])
    return (s32(fixmul(dx, f["fwd"][0]) + fixmul(dy, f["fwd"][1])), s32(fixmul(dx, f["side"][0]) + fixmul(dy, f["side"][1])))


def labs(v):
    return s32(ab(v))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.imp("FixMul", lambda emu: emu.set_r(3, fixmul(s32(emu.r(3)), s32(emu.r(4))) & MASK))
STUBS.imp("FixDiv", lambda emu: emu.set_r(3, fixdiv(s32(emu.r(3)), s32(emu.r(4))) & MASK))
STUBS.imp("labs", lambda emu: emu.set_r(3, T.u32(abs(s32(emu.r(3))))))      # |INT_MIN| stays 0x80000000


def add_to_sum(e):
    """What the callbacks would have summed: the queued cand_x and cand_y added to gVehSteerSum in the Mac's memory (wrapping)."""
    for offset, key in ((0, "cand_x"), (4, "cand_y")):
        address = ADDR["sum"] + offset
        e.write(address, struct.pack(">I", (e.u32(address) + e.cl.ret(key, 0)) & MASK))


def stub_walk_vehicles(e):
    """callback_collision_candidates(const Vehicle& vehicle, int32_t radius, callback): the callback is the TVector the original passes."""
    L = e.cl
    L.rec("callback_collision_candidates", (L.lab(e.r(3)), s32(e.r(4)), L.lab(e.r(5))))
    add_to_sum(e)


def stub_walk_obstacles(e):
    """callback_obstacle_candidates(const Vehicle& vehicle, int32_t radius, callback): for do_avoid_obstacle it also sets gVehAvoidNearest."""
    L = e.cl
    L.rec("callback_obstacle_candidates", (L.lab(e.r(3)), s32(e.r(4)), L.lab(e.r(5))))
    add_to_sum(e)
    if e.r(5) == TV_AVOID:
        e.write(ADDR["nearest"], struct.pack(">I", L.ret("cand_near", e.u32(ADDR["nearest"])) & MASK))


def stub_recalc_path(e):
    """Vehicle::recalc_path(const Vector2d& target, int mode): this in r3, the target's two words behind r4."""
    L = e.cl
    L.rec("Vehicle::recalc_path", (L.lab(e.r(3)), e.s32(e.r(4)), e.s32(e.r(4) + 4), s32(e.r(5))))


def stub_next_step(e):
    e.cl.rec("Vehicle::next_step", (e.cl.lab(e.r(3)),))


def stub_delete(e):
    """operator delete(void*): the label of the pointer."""
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)),))


STUBS.internal(WALK_VEHICLES, stub_walk_vehicles)
STUBS.internal(WALK_OBSTACLES, stub_walk_obstacles)
STUBS.internal(RECALC_PATH, stub_recalc_path)
STUBS.internal(NEXT_STEP, stub_next_step)
STUBS.imp("__dl__FPv", stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)       # the C library's memset, memcpy, sqrt ... (the original isqrt calls sqrt); __destroy_arr does nothing


# ---------------------------------------------------------------------------------------------------------------- the two sides

U64, I32, F32 = C.c_uint64, C.c_int32, C.c_float
SIGNATURES = {            # op: (host result type, host argument types); the Mac's argument order is the same
    "perp": (None, [U64, U64]), "normalise": (None, [U64, U64]), "truncate": (None, [U64, U64, I32]),
    "point_to_local": (None, [U64, U64, U64]), "vec_to_global": (None, [U64, U64, U64]), "predict_pos": (None, [U64, U64, I32]),
    "clear_behaviours": (None, []), "dtor": (None, [U64, I32]),
    "seek": (None, [U64, U64, U64]), "arrival": (None, [U64, U64, U64]), "separation": (None, [U64, U64, U64]),
    "repel_obstacle": (None, [U64, U64, U64]), "avoid_obstacle": (None, [U64, U64, U64]),
    "do_separation": (None, [U64, U64]), "do_repel_obstacle": (None, [U64, U64]), "do_avoid_obstacle": (None, [U64, U64]),
    "pilot_reset": (None, [U64, I32, I32, I32, I32]), "pilot_new_target": (None, [U64, I32, I32]), "pilot_move": (None, [U64]),
    "pilot_get_pos": (None, [U64, U64, U64]), "pilot_set_speed": (None, [U64, F32]),
    "vehicle_finished": (I32, [U64]), "pilot_finished": (I32, [U64]),
}
RETURNS = {"vehicle_finished", "pilot_finished"}       # the others answer nothing (a Vector2d through `out`)


def vehicle_words(f):
    """The (Mac offset, value) pairs of a vehicle's fields (the flag byte at +0x60 is written apart)."""
    pairs = [(0x04, f["radius"]), (0x08, f["pos"][0]), (0x0C, f["pos"][1]), (0x10, f["vel"][0]), (0x14, f["vel"][1]), (0x18, f["force"]),
             (0x1C, f["speed"]), (0x20, f["fwd"][0]), (0x24, f["fwd"][1]), (0x28, f["side"][0]), (0x2C, f["side"][1]), (0x54, f["num"]),
             (0x5C, f["wp"]), (0xA0, f["total"]), (0xA4, f["leg_a"]), (0xA8, f["leg_b"])]
    for k, (x, y) in enumerate(f["wps"]):
        pairs += [(0x64 + 8 * k, x), (0x68 + 8 * k, y)]
    for k, value in enumerate(f["pad"]):
        pairs.append((0x8C + 4 * k, value))       # an index of 5 to 8 reads the padding (and the totals); words, so both byte orders read one number
    return pairs


def dtor_form(case):
    """The host form of a Mac flag: 2 delete of NULL, 1 delete (flag above zero as a halfword), 0 the destructor in place."""
    if case["null"]:
        return 2
    return 1 if s16(case["flag"]) > 0 else 0


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def block(self, size, rnd):
        """`size` bytes between two guards, all seeded noise (the same bytes on both sides); returns the address of the first byte."""
        address = self.alloc(GUARD + size + GUARD)
        self.write(address, bytes(rnd.getrandbits(8) for _ in range(GUARD + size + GUARD)))
        self.sizes[address + GUARD] = size
        return address + GUARD

    def guards(self, address):
        return self.read(address - GUARD, GUARD) + self.read(address + self.sizes[address], GUARD)

    def make(self, case):
        """Allocates and fills every block of the case, writes the globals and the list (the addresses are the Mac's or the host's own)."""
        rnd = random.Random(case["fill"])
        self.sizes = {}
        self.out = self.block(8, rnd)
        self.vec = self.block(8, rnd)
        self.target = self.block(8, rnd)
        self.veh = self.block(VEH_BLOCK, rnd)
        self.other = self.block(VEH_BLOCK, rnd)
        self.obst = self.block(16, rnd)
        self.beh = self.block(0x24, rnd)
        self.entries = self.block(LIST_ENTRIES * 16, rnd)       # the host's entry size on both sides, so the guards are the same bytes
        self.words(self.out, case["out"])
        self.words(self.vec, case["vec"])
        self.words(self.target, case["target"])
        self.words(self.obst, case["obst"])
        self.put_vehicle(self.veh, case["veh"])
        self.put_vehicle(self.other, case["other"])
        self.put_list(case["list"])
        for name in ("zero", "one", "range", "nearest"):
            self.words(self.g(name), [case[name]])
        self.words(self.g("sum"), case["sum"])

    def put_vehicle(self, address, f):
        for offset, value in vehicle_words(f):
            self.words(address + offset, [value])
        self.write(address + 0x60, bytes([f["flag"] & 0xFF]))
        self.set_target(address)

    def get_vehicle(self, address):
        """The fields as words, then the noise bytes (the span between the fields, but +0x30); both come back equal on the two sides."""
        raw = self.read(address, VEH_SIZE)
        words = [struct.unpack_from(self.fmt, raw, offset)[0] for offset in VEHICLE_WORDS]
        return [words, bytes(raw[i] for i in range(VEH_SIZE) if i not in COVERED)]

    def put_list(self, lst):
        for k, (weight, pointer) in enumerate(lst["entries"]):
            entry = self.entries + k * self.esize
            self.words(entry, [weight])
            self.write(entry + self.pointer_at, struct.pack(self.pfmt, pointer))
        for k, key in enumerate(("data", "top", "end")):
            self.write(self.g("list") + k * self.psize, struct.pack(self.pfmt, self.entries + lst[key] * self.esize))

    def get_list(self):
        """The header as entry indexes, the entries as (weight, behaviour pointer)."""
        head = self.read(self.g("list"), 3 * self.psize)
        indexes = [(struct.unpack_from(self.pfmt, head, k * self.psize)[0] - self.entries) // self.esize for k in range(3)]
        entries = []
        for k in range(LIST_ENTRIES):
            entry = self.entries + k * self.esize
            entries.append([self.rd_word(entry), struct.unpack(self.pfmt, self.read(entry + self.pointer_at, self.psize))[0]])
        return [indexes, entries]


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"
    pfmt = ">I"
    psize = 4
    esize = 8
    pointer_at = 4

    def __init__(self, e, case):
        self.e = e
        self.make(case)
        fixed = {self.veh: L_VEHICLE, self.other: L_OTHER, self.obst: L_OBSTACLE, self.target: L_TARGET,
                 TV_SEPARATION: L_DO_SEPARATION, TV_REPEL: L_DO_REPEL, TV_AVOID: L_DO_AVOID}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def set_target(self, address):
        self.words(address + 0x30, [self.target])           # the Mac's pointer to the target

    def dtor_args(self, case):
        return [0 if case["null"] else self.veh, case["flag"] & MASK]

    def invoke(self, op, args):
        """The call: integer and pointer arguments as words, floats as F(...); answers r3."""
        r3, _, _ = T.call_ppc(self.e, ENTRY[op], [T.F(bitsf(a)) if isinstance(a, Bits) else a for a in args])
        return r3 & MASK


class Host(Side):
    """The same on the host library."""
    fmt = "<I"
    pfmt = "<Q"
    psize = 8
    esize = HOST_LAYOUT[2]
    pointer_at = HOST_LAYOUT[3]

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.heap = lib.tpw_vst_new() if case["op"] == "dtor" and dtor_form(case) == 1 else 0     # delete needs a block from new
        self.make(case)
        fixed = {L_VEHICLE: self.heap or self.veh, L_OTHER: self.other, L_OBSTACLE: self.obst, L_TARGET: self.target,
                 L_DO_SEPARATION: self.g("do_separation"), L_DO_REPEL: self.g("do_repel_obstacle"), L_DO_AVOID: self.g("do_avoid_obstacle")}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_vst_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_vst_alloc(n)       # zeroed, 32-bit addressable (platform/arena.cpp)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_target(self, address):
        self.write(address + HOST_LAYOUT[0], struct.pack("<Q", self.target))      # the host's own member

    def dtor_args(self, case):
        return [self.heap or self.veh, dtor_form(case)]

    def invoke(self, op, args):
        restype, argtypes = SIGNATURES[op]
        converted = [s32(a) if t is I32 else bitsf(a) if t is F32 else a for t, a in zip(argtypes, args)]
        return (getattr(self.lib, "tpw_vst_" + op)(*converted) or 0) & MASK


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_vst_addr.restype, lib.tpw_vst_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_vst_alloc.restype, lib.tpw_vst_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_vst_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_vst_new.restype, lib.tpw_vst_new.argtypes = C.c_uint64, []
    for op, (restype, argtypes) in SIGNATURES.items():
        function = getattr(lib, "tpw_vst_" + op)
        function.restype, function.argtypes = restype, argtypes
    out = (C.c_int64 * 32)()
    n = lib.tpw_vst_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world and the call


def arguments(s, case):
    """The call's arguments, the Mac's order (a Vector2d answer goes through `out`, r3 on the Mac)."""
    op, a = case["op"], case["args"]
    if op in ("perp", "normalise"):
        return [s.out, s.vec]
    if op == "truncate":
        return [s.out, s.vec, a[0] & MASK]
    if op in ("point_to_local", "vec_to_global"):
        return [s.out, s.veh, s.vec]
    if op == "predict_pos":
        return [s.out, s.veh, a[0] & MASK]
    if op == "clear_behaviours":
        return []
    if op == "dtor":
        return s.dtor_args(case)
    if op in ("seek", "arrival", "separation", "repel_obstacle", "avoid_obstacle"):
        return [s.out, s.beh, s.veh]
    if op == "do_separation":
        return [s.veh, s.veh if a[0] == "self" else s.other]
    if op in ("do_repel_obstacle", "do_avoid_obstacle"):
        return [s.veh, s.obst]
    if op == "pilot_reset":
        return [s.veh] + [v & MASK for v in a]
    if op == "pilot_new_target":
        return [s.veh] + [v & MASK for v in a]
    if op == "pilot_get_pos":
        return [s.veh, s.out, s.out + 4]
    if op == "pilot_set_speed":
        return [s.veh, Bits(a[0])]
    return [s.veh]                 # pilot_move, vehicle_finished, pilot_finished


def execute(s, case):
    """The call."""
    answer = s.invoke(case["op"], arguments(s, case))
    return answer if case["op"] in RETURNS else 0


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides."""
    return {
        "answer": answer,
        "log": s.log(),
        "out": s.rd_words(s.out, 2),
        "vec": s.rd_words(s.vec, 2),
        "target": s.rd_words(s.target, 2),
        "obstacle": s.rd_words(s.obst, 3),
        "veh": s.get_vehicle(s.veh),
        "other": s.get_vehicle(s.other),
        "globals": [s.rd_word(s.g(name)) for name in ("zero", "one", "range", "nearest")] + s.rd_words(s.g("sum"), 2),
        "list": s.get_list(),
        "guards": [s.guards(a) for a in (s.out, s.vec, s.target, s.veh, s.other, s.obst, s.beh, s.entries)],
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
SHORT = [0, 1, -1, 0x10000, -0x10000, INT_MAX, INT_MIN]
POSITIONS = [(0, 0), (0x10000, -0x20000), (INT_MIN, INT_MAX)]
VELOCITIES = [(0, 0), (0x10000, -0x10000), (INT_MIN, INT_MAX)]
ROTATIONS = [((0x10000, 0), (0, 0x10000)), ((0xB505, 0xB505), (-0xB505, 0xB505)), ((0, 0x10000), (-0x10000, 0)),
             ((INT_MAX, INT_MIN), (INT_MIN, INT_MAX)), ((0, 0), (0, 0))]       # forward and side: identity, 45 degrees, 90 degrees, extremes, zero


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


def rotation(r):
    """A forward and a side vector of a random turn (length 1.0)."""
    angle = r.uniform(0, 2 * math.pi)
    c, s = round(math.cos(angle) * 0x10000), round(math.sin(angle) * 0x10000)
    return (c, s), (-s, c)


def veh_default(**over):
    """A vehicle with the identity frame and everything else zero."""
    f = {"radius": 0, "pos": (0, 0), "vel": (0, 0), "force": 0, "speed": 0, "fwd": (0x10000, 0), "side": (0, 0x10000), "num": 0, "wp": 0,
         "flag": 0, "wps": [(0, 0)] * 5, "pad": [0] * 5, "total": 0, "leg_a": 0, "leg_b": 0}
    f.update(over)
    return f


def veh_random(r):
    """Every field random. The waypoint index stays within 0 to 8 (inside the vehicle block; the original does not check it either)."""
    return veh_default(radius=rv(r), pos=(rv(r), rv(r)), vel=(rv(r), rv(r)), force=rv(r), speed=rv(r), fwd=(rv(r), rv(r)), side=(rv(r), rv(r)),
                       num=r.randrange(-1, 9), wp=r.randrange(0, 9), flag=r.choice([0, 1, 2, 0xFF]), wps=[(rv(r), rv(r)) for _ in range(5)],
                       pad=[rv(r) for _ in range(5)], total=rv(r), leg_a=rv(r), leg_b=rv(r))


def list_default(data=3, top=3, end=15):
    return {"data": data, "top": top, "end": end, "entries": [(0x1000 + k, 0x20000 + k) for k in range(LIST_ENTRIES)]}


def list_random(r):
    data = r.randrange(0, 12)
    top = r.randrange(max(data - 3, 0), LIST_ENTRIES)
    return {"data": data, "top": top, "end": r.randrange(0, LIST_ENTRIES),
            "entries": [(r.randrange(1, MASK), r.randrange(1, MASK)) for _ in range(LIST_ENTRIES)]}


def base(r, op):
    """The world every edge case starts from: the globals at their usual values, an identity vehicle, a list of 16 entries with mData at entry 3."""
    return {"op": op, "fill": r.getrandbits(32), "queues": {}, "zero": 0, "one": 0x10000, "range": 0, "nearest": 0, "sum": (0, 0),
            "out": (0x5A5A5A5A, 0xA5A5A5A5), "vec": (0, 0), "target": (0, 0), "obst": (0, 0, 0), "veh": veh_default(), "other": veh_default(),
            "list": list_default(), "args": []}


def case(r, op, **over):
    c = base(r, op)
    c.update(over)
    return c


def world(r, op):
    """base() with random globals, vehicles, vectors, obstacle and list: the world of the random cases."""
    c = base(r, op)
    c["zero"] = 0 if r.random() < 0.85 else rv(r)
    c["one"] = 0x10000 if r.random() < 0.85 else rv(r)
    c["range"], c["nearest"] = rv(r), r.choice([rv(r), INT_MAX])
    c["sum"] = (rv(r), rv(r))
    c["vec"], c["target"], c["obst"] = (rv(r), rv(r)), (rv(r), rv(r)), (rv(r), rv(r), rv(r))
    c["veh"], c["other"], c["list"] = veh_random(r), veh_random(r), list_random(r)
    return c


def rnd_case(r, op, **over):
    c = world(r, op)
    c.update(over)
    return c


def sum_queues(r, span=0x40000):
    """What the walks add to gVehSteerSum."""
    return {"cand_x": [r.randrange(-span, span)], "cand_y": [r.randrange(-span, span)]}


def fixed_queues(x=0x111, y=-0x222, **more):
    queues = {"cand_x": [x], "cand_y": [y]}
    queues.update({key: [value] for key, value in more.items()})
    return queues


# ---- perp, normalise
def vec_edges(op, seed, values):
    r = random.Random(seed)
    for x in values:
        for y in values:
            yield case(r, op, vec=(x, y))


def vec_random(op):
    def generate(r):
        return rnd_case(r, op, vec=(rv(r), rv(r)))
    return generate


def perp_edges():
    return vec_edges("perp", 101, SHORT)


def normalise_edges():
    """Every pair of the edge values: the three ranges of the bits test, 0xfff and 0x1000, 0x3fffff and 0x400000, INT_MIN."""
    return vec_edges("normalise", 102, EDGE)


# ---- truncate
TRUNCATE_VECTORS = [(0, 0), (0x10000, 0), (0, -0x10000), (3, 4), (0xFFF, 0xFFF), (0x1000, 0), (0x3FFFFF, 0x3FFFFF), (0x400000, 0), (INT_MAX, INT_MAX),
                    (INT_MIN, 0), (INT_MIN, INT_MIN), (-0x12345, 0x6789)]


def truncate_edges():
    """The max one below, at and one above the vector's length (the compare is signed), and the extremes."""
    r = random.Random(103)
    for v in TRUNCATE_VECTORS:
        n = length(*v)
        for m in (n - 1, n, n + 1, 0, 1, -1, INT_MAX, INT_MIN, 0x10000):
            yield case(r, "truncate", vec=v, args=[s32(m)])


def truncate_random(r):
    v = (rv(r), rv(r))
    n = length(*v)
    return rnd_case(r, "truncate", vec=v, args=[s32(r.choice([n - 1, n, n + 1, rv(r), r.randrange(0, 0x100000)]))])


# ---- point_to_local, vec_to_global
def frame_edges(op, seed):
    """Every rotation with every pair of the short edge values, the vehicle's position cycling through three."""
    r = random.Random(seed)
    k = 0
    for fwd, side in ROTATIONS:
        for x in SHORT:
            for y in SHORT:
                k += 1
                yield case(r, op, veh=veh_default(pos=POSITIONS[k % 3], fwd=fwd, side=side), vec=(x, y))


def point_to_local_edges():
    return frame_edges("point_to_local", 104)


def vec_to_global_edges():
    return frame_edges("vec_to_global", 105)


# ---- predict_pos
def predict_pos_edges():
    r = random.Random(106)
    k = 0
    for vx in SHORT:
        for vy in (0, -1, INT_MAX, INT_MIN, 0x10000):
            for t in (0, 1, -1, 0xA0000, INT_MAX, INT_MIN):
                k += 1
                yield case(r, "predict_pos", veh=veh_default(pos=POSITIONS[k % 3], vel=(vx, vy)), args=[t])


def predict_pos_random(r):
    return rnd_case(r, "predict_pos", args=[r.choice([0xA0000, 0, 1, -1, rv(r), r.randrange(-0x100000, 0x100000)])])


# ---- clear_behaviours
def clear_edges():
    """mTop at every entry from two below mData up (mData's own entry is never cleared; an mTop below mData changes nothing)."""
    r = random.Random(107)
    for data, tops in ((3, range(1, 14)), (0, range(0, 16)), (15, range(13, 16))):
        for top in tops:
            yield case(r, "clear_behaviours", list=list_default(data=data, top=top))


def clear_random(r):
    return rnd_case(r, "clear_behaviours")


# ---- ~Vehicle: the Mac's flags (r4)
FLAGS = [0, 1, -1, 2, 0x7FFF, 0x8000, 0x10000, 0x10001, 0xFFFF, INT_MAX, INT_MIN]


def dtor_edges():
    r = random.Random(108)
    for flag in FLAGS:
        yield case(r, "dtor", flag=flag & MASK, null=False)
    for flag in (0, 1):
        yield case(r, "dtor", flag=flag, null=True)


def dtor_random(r):
    return rnd_case(r, "dtor", flag=r.choice(FLAGS + [r.getrandbits(32)]) & MASK, null=r.random() < 0.1)


# ---- Seek::calculate
def seek_edges():
    r = random.Random(109)
    k = 0
    for tx in SHORT:
        for ty in (0, INT_MIN, 0x10000):
            k += 1
            yield case(r, "seek", target=(tx, ty), veh=veh_default(pos=POSITIONS[k % 3], vel=VELOCITIES[k % 3]))


# ---- Arrival::calculate
ARRIVAL_OFFSETS = [(1, 0), (0, 0xFFF), (0x1000, 0), (3, 4), (0x3FFFFF, 0), (0, 0x400000), (0x10000, 0), (INT_MAX, 0), (INT_MIN, 0), (0, 0)]


def arrival_edges():
    """Offsets of every length range; mMaxSpeed one below, at and one above len / 2.0 (the compare is signed); gVehZero equal to the length."""
    r = random.Random(110)
    for k, (ox, oy) in enumerate(ARRIVAL_OFFSETS):
        pos, vel = POSITIONS[k % 3], VELOCITIES[k % 3]
        target = (s32(pos[0] + ox), s32(pos[1] + oy))
        n = length(ox, oy)
        t = fixdiv(n, 0x20000)
        for speed in (t - 1, t, t + 1, 0, INT_MAX, INT_MIN, -1):
            yield case(r, "arrival", target=target, veh=veh_default(pos=pos, vel=vel, speed=s32(speed)))
        yield case(r, "arrival", target=target, zero=n, veh=veh_default(pos=pos, vel=vel, speed=t))


def arrival_random(r):
    if r.random() < 0.6:
        v = veh_random(r)
        pos = v["pos"]
        v["speed"] = r.choice([r.randrange(0, 0x30000), rv(r)])
        target = (s32(pos[0] + r.randrange(-0x60000, 0x60000)), s32(pos[1] + r.randrange(-0x60000, 0x60000)))
        return rnd_case(r, "arrival", target=target, veh=v)
    return rnd_case(r, "arrival", target=(rv(r), rv(r)))


# ---- Separation::calculate
def separation_veh(wp, num, flag, radius, dx, dy):
    """A vehicle (dx, dy) away from its current waypoint."""
    wps = [(0x1000 * (k + 1), -0x800 * (k + 1)) for k in range(5)]
    wx, wy = wps[wp]
    return veh_default(radius=radius, pos=(s32(wx + dx), s32(wy + dy)), wps=wps, wp=wp, num=num, flag=flag)


def separation_edges():
    """On the last leg (mWaypoint == count - 1, or the flag byte not zero) a vehicle within gVehOne + mRadius of the waypoint answers (0, 0):
    one below, at and one above that distance, along an axis and diagonally."""
    r = random.Random(111)
    for flag in (0, 1, 0xFF):
        for wp, num in ((4, 5), (0, 1), (2, 5), (3, 5)):
            for radius in (0, 0x8000):
                threshold = 0x10000 + radius
                for delta in (-1, 0, 1):
                    for dx, dy in ((threshold + delta, 0), (-(threshold + delta), 0), (0, threshold + delta), (threshold // 2 + delta, threshold // 2)):
                        yield case(r, "separation", veh=separation_veh(wp, num, flag, radius, dx, dy), queues=fixed_queues(), sum=(5, -7))
    for pos in ((INT_MIN, 0), (INT_MAX, INT_MIN)):
        yield case(r, "separation", veh=veh_default(pos=pos, wp=0, num=1, radius=0x8000), queues=fixed_queues(), sum=(5, -7))


def separation_random(r):
    v = veh_random(r)
    v["wp"] = r.randrange(0, 5)
    if r.random() < 0.5:
        v["num"] = v["wp"] + 1
    if r.random() < 0.6:
        wx, wy = v["wps"][v["wp"]]
        v["radius"] = r.randrange(0, 0x30000)
        v["pos"] = (s32(wx + r.randrange(-0x30000, 0x30000)), s32(wy + r.randrange(-0x30000, 0x30000)))
    return rnd_case(r, "separation", veh=v, queues=sum_queues(r))


# ---- do_separation and do_repel_obstacle: the same distance test against another vehicle or an obstacle
def near_pairs(radius_a, radius_b, one=0x10000):
    """(dx, dy) pairs whose octagonal distance is one below, at and one above radius_a + radius_b + one (dx and dy of either sign)."""
    threshold = s32(radius_a + radius_b + one)
    pairs = []
    for delta in (-1, 0, 1):
        dist = s32(threshold + delta)
        pairs += [(dist, 0), (-dist, 0), (0, dist), (0, -dist)] if dist >= 0 else [(dist, 0)]
    return pairs


RADII = [(0, 0), (0x8000, 0x4000), (0x7FFF0000, 0x10), (-5, 3)]
BASE_POS = (0x5000, -0x3000)


def callback_edges(op, seed, other_kind):
    r = random.Random(seed)
    for ra, rb in RADII:
        for dx, dy in near_pairs(ra, rb) + [(0, 0), (0x8000, 0x8000), (-0x8000, 0x4000)]:
            me = veh_default(radius=ra, pos=(s32(BASE_POS[0] + dx), s32(BASE_POS[1] + dy)))
            if other_kind == "vehicle":
                other = dict(other=veh_default(radius=rb, pos=BASE_POS), args=["other"])
            else:
                other = dict(obst=(BASE_POS[0], BASE_POS[1], rb))
            yield case(r, op, veh=me, sum=(0x100, -0x200), **other)
            yield case(r, op, veh=me, sum=(INT_MAX, INT_MIN), zero=s32(max(abs(dx), abs(dy))), **other)      # gVehZero equal to the distance
            yield case(r, op, veh=me, sum=(0x100, -0x200), one=0x8000, **other)                               # gVehOne read, not 0x10000
    for pos in ((INT_MIN, 0), (INT_MAX, INT_MIN), (0, INT_MIN)):
        me = veh_default(radius=0x8000, pos=pos)
        if other_kind == "vehicle":
            other = dict(other=veh_default(radius=0x4000, pos=(0, 0)), args=["other"])
        else:
            other = dict(obst=(0, 0, 0x4000))
        yield case(r, op, veh=me, sum=(7, 9), **other)
    if other_kind == "vehicle":
        yield case(r, op, veh=veh_default(radius=0x8000, pos=BASE_POS), sum=(7, 9), args=["self"])       # the vehicle against itself


def callback_random(op, other_kind):
    def generate(r):
        c = rnd_case(r, op, args=["other"])
        if r.random() < 0.7:
            span = 0x30000
            me, rb = veh_random(r), r.randrange(-0x4000, 0x20000)
            me["radius"] = r.randrange(-0x4000, 0x20000)
            me["pos"] = (s32(BASE_POS[0] + r.randrange(-span, span)), s32(BASE_POS[1] + r.randrange(-span, span)))
            c["veh"] = me
            if other_kind == "vehicle":
                c["other"] = veh_default(radius=rb, pos=BASE_POS)
                if r.random() < 0.1:
                    c["args"] = ["self"]
            else:
                c["obst"] = (BASE_POS[0], BASE_POS[1], rb)
        return c
    return generate


def separation_cb_edges():
    return callback_edges("do_separation", 112, "vehicle")


def repel_cb_edges():
    return callback_edges("do_repel_obstacle", 113, "obstacle")


# ---- Repel_Obstacle::calculate
def repel_edges():
    r = random.Random(114)
    for radius in (0, 1, 0x8000, 0x3FFFFFFF, 0x40000000, INT_MAX, INT_MIN, -1):
        yield case(r, "repel_obstacle", veh=veh_default(radius=radius), queues=fixed_queues(), sum=(0x123, 0x456))
    yield case(r, "repel_obstacle", veh=veh_default(radius=0x8000), queues=fixed_queues(INT_MAX, INT_MIN), sum=(0x123, 0x456))


def repel_random(r):
    return rnd_case(r, "repel_obstacle", queues=sum_queues(r))


# ---- do_avoid_obstacle
AVOID_OBSTACLES = [(0x50000, 0), (0x50000, 0x8000), (0x50000, -0x8000), (0x30000, 0x20000), (-0x10000, 0), (0x50000, INT_MIN)]


def avoid_edges():
    """Per obstacle, frame and obstacle radius: the hit; then each compare one below, at and one above (q.x > gVehZero, t >= gVehZero,
    q.x - radius <= gVehAvoidRange, labs(q.y) < the radii, t < gVehAvoidNearest), with q.y INT_MIN among the obstacles."""
    r = random.Random(115)
    for fwd, side in ROTATIONS[:2]:
        for p in AVOID_OBSTACLES:
            for orad in (0x4000, 0x20000):
                veh = veh_default(pos=(0x1000, 0), fwd=fwd, side=side, radius=0x8000)
                qx, qy = local(veh, p)
                t = s32(xaxis(qx, qy, orad))
                hit = dict(veh=veh, obst=(p[0], p[1], orad), range=0x100000, nearest=INT_MAX, sum=(0x111, 0x222))
                yield case(r, "do_avoid_obstacle", **hit)
                for zero in (qx - 1, qx, qx + 1, t - 1, t, t + 1):
                    yield case(r, "do_avoid_obstacle", **dict(hit, zero=s32(zero)))
                for rng in (qx - orad - 1, qx - orad, qx - orad + 1, INT_MIN, INT_MAX):
                    yield case(r, "do_avoid_obstacle", **dict(hit, range=s32(rng)))
                for delta in (-1, 0, 1):
                    yield case(r, "do_avoid_obstacle", **dict(hit, veh=dict(veh, radius=s32(labs(qy) - orad + delta))))
                for nearest in (t - 1, t, t + 1, 0, INT_MIN, -1):
                    yield case(r, "do_avoid_obstacle", **dict(hit, nearest=s32(nearest)))


def avoid_random(r):
    c = rnd_case(r, "do_avoid_obstacle")
    if r.random() < 0.7:
        fwd, side = rotation(r)
        c["veh"] = veh_default(pos=(r.randrange(-0x20000, 0x20000), r.randrange(-0x20000, 0x20000)), fwd=fwd, side=side,
                               radius=r.randrange(0, 0x30000))
        c["obst"] = (r.randrange(-0x80000, 0x80000), r.randrange(-0x80000, 0x80000), r.randrange(0, 0x30000))
        c["zero"], c["range"] = 0, r.randrange(0, 0x100000)
        c["nearest"] = r.choice([INT_MAX, r.randrange(0, 0x100000)])
    return c


# ---- Avoid_Obstacle::calculate
def avoid_calc_edges():
    """Velocities of every length range, radii, and a nearest hit that is absent, 0x7fffffff (none) or any other (the walk sets it)."""
    r = random.Random(116)
    for vel in ((0, 0), (0x10000, 0), (0x1000, -0x2000), (INT_MAX, 0), (INT_MIN, INT_MIN), (0x100000, 0x100000)):
        for radius in (0, 0x8000, INT_MAX):
            for near in (None, INT_MAX, 0, 0x1234):
                more = {} if near is None else {"cand_near": near}
                veh = veh_default(pos=(0x100, -0x200), vel=vel, radius=radius, fwd=(0xB505, 0xB505), side=(-0xB505, 0xB505))
                yield case(r, "avoid_obstacle", veh=veh, queues=fixed_queues(**more), sum=(5, 6), range=7, nearest=8)


def avoid_calc_random(r):
    c = rnd_case(r, "avoid_obstacle", queues=sum_queues(r))
    if r.random() < 0.7:
        fwd, side = rotation(r)
        c["veh"] = veh_default(pos=(rv(r), rv(r)), vel=(r.randrange(-0x40000, 0x40000), r.randrange(-0x40000, 0x40000)), fwd=fwd, side=side,
                               radius=r.randrange(0, 0x30000))
    if r.random() < 0.6:
        c["queues"]["cand_near"] = [r.choice([INT_MAX, r.randrange(0, 0x100000), rv(r)])]
    return c


# ---- the Pilot
def pilot_reset_edges():
    r = random.Random(117)
    for k in range(40):
        yield case(r, "pilot_reset", veh=veh_default(pos=(7, 8), vel=(9, 10)), args=[r.choice(SHORT) for _ in range(4)])


def pilot_reset_random(r):
    return rnd_case(r, "pilot_reset", args=[rv(r) for _ in range(4)])


def pilot_new_target_edges():
    r = random.Random(118)
    for x in SHORT:
        for y in SHORT:
            yield case(r, "pilot_new_target", args=[x, y])


def pilot_new_target_random(r):
    return rnd_case(r, "pilot_new_target", args=[rv(r), rv(r)])


def pilot_move_edges():
    r = random.Random(119)
    yield case(r, "pilot_move")
    yield case(r, "pilot_move", veh=veh_default(pos=(1, 2), vel=(3, 4)))


def pilot_move_random(r):
    return rnd_case(r, "pilot_move")


def pilot_get_pos_edges():
    r = random.Random(120)
    for x in SHORT:
        for y in SHORT:
            yield case(r, "pilot_get_pos", veh=veh_default(pos=(x, y), vel=(5, 6)))


def pilot_get_pos_random(r):
    return rnd_case(r, "pilot_get_pos")


def speed_bits_around(speed, count=3):
    """The single-precision bits of `speed` and its neighbours (count each way)."""
    centre = fb(speed)
    return [centre + k for k in range(-count, count + 1)]


def speed_edges():
    """0, -0.0, ordinary speeds, 2.0 and its neighbours (the clamp), the speeds at which 0.4 * speed and 0.2 * speed reach 0x28f (the lower
    clamp) and the integers, infinities, NaNs, denormals and the extremes."""
    r = random.Random(121)
    bits = [0, 0x80000000, fb(1.0), fb(0.5), fb(-1.0), fb(-0.01), fb(3.0), fb(1e30), fb(-1e30), fb(1e-30), fb(-1e-30), 0x00000001, 0x80000001,
            0x007FFFFF, 0x00800000, 0x7F7FFFFF, 0xFF7FFFFF, 0x7F800000, 0xFF800000, 0x7FC00000, 0xFFC00000, 0x7F800001, 0xFF800001, 0x7FFFFFFF]
    bits += speed_bits_around(2.0, 3)
    bits += speed_bits_around(0x28F / (0.4 * 65536.0), 4)
    bits += speed_bits_around(0x28F / (0.2 * 65536.0), 4)
    bits += [fb(k / 16.0) for k in range(-4, 40, 3)]
    for b in bits:
        yield case(r, "pilot_set_speed", veh=veh_default(force=0x1111, speed=0x2222), args=[b & MASK])


def speed_random(r):
    k = r.random()
    if k < 0.1:
        b = r.choice([0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0x40000000, 0x7F7FFFFF])
    elif k < 0.6:
        b = fb(r.uniform(-1, 3))
    elif k < 0.9:
        b = fb(r.uniform(-0.1, 0.1))
    else:
        b = r.getrandbits(32)
    return rnd_case(r, "pilot_set_speed", args=[b])


# ---- is_finished_path (the Vehicle's and the Pilot's)
def finished_veh(flag, wp, total, pos, leg_a=0x2000, leg_b=0x3000):
    wps = [(0x20000 * (k + 1), -0x10000 * k) for k in range(5)]
    pad = [0x1234 * (k + 1) for k in range(5)]
    return veh_default(flag=flag, wp=wp, total=total, pos=pos, wps=wps, pad=pad, leg_a=leg_a, leg_b=leg_b)


def finished_edges(op, seed):
    """The flag byte 0, 1 (finished: answers gVehOne), 2 and 0xff; mPathTotal 0 (answers gVehOne), 1 and others; every waypoint index of the
    table and four past it; legs and positions of either sign and extremes."""
    r = random.Random(seed)
    for flag in (0, 1, 2, 0xFF):
        for total in (0, 1, 0x100000, -0x100000):
            for wp in range(0, 5):
                yield case(r, op, veh=finished_veh(flag, wp, total, POSITIONS[wp % 3]), one=0x10000 if wp != 4 else 0x8000)
    for wp in (5, 6, 7, 8):
        yield case(r, op, veh=finished_veh(0, wp, 0x40000, (0x1000, 0x2000)))
    for leg_a, leg_b in ((INT_MAX, INT_MAX), (INT_MIN, INT_MIN), (-0x8000, 0x4000)):
        yield case(r, op, veh=finished_veh(0, 2, 0x50000, (0, 0), leg_a, leg_b))
    yield case(r, op, veh=finished_veh(0, 1, INT_MIN, (INT_MIN, INT_MAX)))


def finished_random(op):
    def generate(r):
        c = rnd_case(r, op)
        if r.random() < 0.6:
            v = c["veh"]
            v["wp"], v["flag"] = r.randrange(0, 5), r.choice([0, 0, 0, 1, 2])
            v["pos"] = (r.randrange(-0x80000, 0x80000), r.randrange(-0x80000, 0x80000))
            v["wps"] = [(r.randrange(-0x80000, 0x80000), r.randrange(-0x80000, 0x80000)) for _ in range(5)]
            v["total"] = r.choice([0, r.randrange(1, 0x200000), rv(r)])
            v["leg_a"], v["leg_b"] = r.randrange(0, 0x100000), r.randrange(0, 0x100000)
        return c
    return generate


def vehicle_finished_edges():
    return finished_edges("vehicle_finished", 122)


def pilot_finished_edges():
    return finished_edges("pilot_finished", 123)


TESTS = [
    FnTest("perp", ppc, host, edge=perp_edges, rnd=vec_random("perp")),
    FnTest("normalise", ppc, host, edge=normalise_edges, rnd=vec_random("normalise")),
    FnTest("truncate", ppc, host, edge=truncate_edges, rnd=truncate_random),
    FnTest("Vehicle::point_to_local", ppc, host, edge=point_to_local_edges, rnd=vec_random("point_to_local")),
    FnTest("Vehicle::vec_to_global", ppc, host, edge=vec_to_global_edges, rnd=vec_random("vec_to_global")),
    FnTest("Vehicle::predict_pos", ppc, host, edge=predict_pos_edges, rnd=predict_pos_random),
    FnTest("Vehicle::clear_behaviours", ppc, host, edge=clear_edges, rnd=clear_random),
    FnTest("Vehicle::~Vehicle", ppc, host, edge=dtor_edges, rnd=dtor_random),
    FnTest("Seek::calculate", ppc, host, edge=seek_edges, rnd=vec_random("seek")),
    FnTest("Arrival::calculate", ppc, host, edge=arrival_edges, rnd=arrival_random),
    FnTest("do_separation", ppc, host, edge=separation_cb_edges, rnd=callback_random("do_separation", "vehicle")),
    FnTest("Separation::calculate", ppc, host, edge=separation_edges, rnd=separation_random),
    FnTest("do_repel_obstacle", ppc, host, edge=repel_cb_edges, rnd=callback_random("do_repel_obstacle", "obstacle")),
    FnTest("Repel_Obstacle::calculate", ppc, host, edge=repel_edges, rnd=repel_random),
    FnTest("do_avoid_obstacle", ppc, host, edge=avoid_edges, rnd=avoid_random),
    FnTest("Avoid_Obstacle::calculate", ppc, host, edge=avoid_calc_edges, rnd=avoid_calc_random),
    FnTest("Pilot::reset", ppc, host, edge=pilot_reset_edges, rnd=pilot_reset_random),
    FnTest("Pilot::new_target", ppc, host, edge=pilot_new_target_edges, rnd=pilot_new_target_random),
    FnTest("Pilot::move", ppc, host, edge=pilot_move_edges, rnd=pilot_move_random),
    FnTest("Pilot::get_pos", ppc, host, edge=pilot_get_pos_edges, rnd=pilot_get_pos_random),
    FnTest("Pilot::set_speed", ppc, host, edge=speed_edges, rnd=speed_random),
    FnTest("Vehicle::is_finished_path", ppc, host, edge=vehicle_finished_edges, rnd=finished_random("vehicle_finished")),
    FnTest("Pilot::is_finished_path", ppc, host, edge=pilot_finished_edges, rnd=finished_random("pilot_finished")),
]
