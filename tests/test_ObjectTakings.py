"""Differential tests for unit w198, a ride's takings, costs, prizes and satisfaction (src/game/Object.cpp), against libtpw_unit_ObjectTakings.dylib
(tests/unit_ObjectTakings.cpp), main app ("SimTheme Park"): the 29 CObject members
  GetObjectAge 0x10107f50, GetRemainingLife 0x10108024, WalkedAway 0x1010eec4, HadCustomer 0x1010ef0c, RecordNewTakings 0x1010ef54,
  RecordACost 0x1010f34c, HadAWinner 0x1010f4b8, GetPricePerUse 0x1010f4f4, GetPrizeValue 0x1010f52c, SetPrizeValue 0x1010f564,
  GetProspectiveCostOfGoods 0x1010f598, GetCostOfGoods 0x1010f6ec, GetTotalRevenue 0x1010f894, GetProfitability 0x1010f8d4,
  ExpressSatisfaction 0x1010f974, GetCustomerSatisfaction 0x1010f9d0, GetPercentageOfWinners 0x1010fa40, GetProductQuality 0x1010fb38,
  SetProductQuality 0x1010fb78, GetChanceOfWinning 0x1010fbb8, SetChanceOfWinning 0x1010fbf8, SetAmountOfSpecialIngredient 0x1010fc38,
  GetAmountOfSpecialIngredient 0x1010fc7c, GetSpecialIngredientName 0x1010fcc4, GetResidualValuePercentage 0x1010fd98,
  GetCurrentResidualValue 0x1010ffc8, PersonUsedToilet 0x10110070, WillIWin 0x101103e0, SetPricePerUse 0x101105f4

A case is an object and a call. The object (0x448 bytes between two 0x40 guards, filled with 0xa5) is built twice by ONE piece of Python code: in emulator memory
at the Mac's addresses (big-endian) for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back.
The fields set are the ones the unit reads (the cell, the type, the build stamp, the script handle, the flags, the cleanliness and remaining life floats, the
upgrade level, the counters at 0x17c to 0x1a0 and the six 30 entry histories). The ride record GetRideInfo answers is built from a hashed slot table with the
case's overrides (the special ingredient, the shop type, the four scrap values of each of the four upgrade levels, the cost of the upgrade) and the case's UI type.
The callees outside the unit are fakes on both sides that record their arguments and answer from the case's queues (see the unit file). The history sums and
averages, GetWorld and the helpers 0x10237e1c and 0x10237cd8 run as the original. What is compared: the answer, the call log, every object field and history
word (the floats with every NaN made one), a checksum of the object's other bytes and both guards, the analyser's two history words and the rest of its block
(RecordNewTakings), and the game type object (PersonUsedToilet: the type and the guard byte).

The game type object is a function-local static of an inline accessor (guard byte 0x102b3aa4, object 0x102b3aa0); the host constructs it once per process, so the
FIRST edge case of PersonUsedToilet is the "first use" case (the constructor runs on both sides); every later case sets the type first.

Fused operations: the float expressions and fctiwz of the unit's functions (HardwareRounding on their ranges). Not tested: an upgrade level above 3 (the host
ride record has 160 slots and the Mac reads past them), a history with an entry index outside -1..29, a NULL gText table (a NULL pointer is as invalid on the host
as on the Mac), the person pointer of WillIWin (never read), a Mac word that is not a valid TbTimeStamp, the callees (faked).
"""
import ctypes as C
import random
import struct
import zlib

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ObjectTakings"
MASK = 0xFFFFFFFF
M64 = (1 << 64) - 1
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF
I64_MIN, I64_MAX = -(1 << 63), (1 << 63) - 1

# ---- the unit's own functions (address, size from the MANIFEST)
FN = {"age": 0x10107F50, "life": 0x10108024, "walked": 0x1010EEC4, "customer": 0x1010EF0C, "takings": 0x1010EF54, "cost": 0x1010F34C,
      "winner": 0x1010F4B8, "price": 0x1010F4F4, "prize": 0x1010F52C, "setprize": 0x1010F564, "prospective": 0x1010F598, "goods": 0x1010F6EC,
      "revenue": 0x1010F894, "profit": 0x1010F8D4, "express": 0x1010F974, "satisfaction": 0x1010F9D0, "pctwin": 0x1010FA40, "quality": 0x1010FB38,
      "setquality": 0x1010FB78, "chance": 0x1010FBB8, "setchance": 0x1010FBF8, "setingredient": 0x1010FC38, "ingredient": 0x1010FC7C,
      "ingname": 0x1010FCC4, "resipct": 0x1010FD98, "resival": 0x1010FFC8, "toilet": 0x10110070, "win": 0x101103E0, "setprice": 0x101105F4}
SIZES = {0x10107F50: 168, 0x10108024: 20, 0x1010EEC4: 28, 0x1010EF0C: 28, 0x1010EF54: 968, 0x1010F34C: 320, 0x1010F4B8: 16, 0x1010F4F4: 8,
         0x1010F52C: 8, 0x1010F564: 8, 0x1010F598: 280, 0x1010F6EC: 376, 0x1010F894: 16, 0x1010F8D4: 112, 0x1010F974: 40, 0x1010F9D0: 56,
         0x1010FA40: 192, 0x1010FB38: 12, 0x1010FB78: 12, 0x1010FBB8: 12, 0x1010FBF8: 12, 0x1010FC38: 8, 0x1010FC7C: 12, 0x1010FCC4: 156,
         0x1010FD98: 500, 0x1010FFC8: 112, 0x10110070: 832, 0x101103E0: 252, 0x101105F4: 8}
OWN = set(SIZES)
# the originals that run natively under the unit: GetWorld, the signed 64 bit divide, __cvt_fp2unsigned and the history members
HELPERS = {0x1014205C, 0x10237E1C, 0x10237CD8, 0x100F1000, 0x100F1134, 0x100F1F0C, 0x10111274, 0x1011132C, 0x101114C0, 0x10111D80}
VOID = {"walked", "customer", "takings", "cost", "winner", "setprize", "express", "setquality", "setchance", "setingredient", "toilet", "setprice"}
SIGNED = {"age", "life", "prospective", "goods", "revenue", "profit", "satisfaction", "resipct"}
HARGS = {"takings": [C.c_uint32], "cost": [C.c_uint32], "setprize": [C.c_int32], "prospective": [C.c_float, C.c_float], "express": [C.c_int32],
         "setquality": [C.c_uint32], "setchance": [C.c_uint32], "setingredient": [C.c_int32], "toilet": [C.c_uint32], "win": [C.c_uint64],
         "setprice": [C.c_int32]}
HRES = {"age": C.c_int32, "life": C.c_int32, "price": C.c_uint32, "prize": C.c_uint32, "prospective": C.c_int32, "goods": C.c_int32,
        "revenue": C.c_int32, "profit": C.c_int32, "satisfaction": C.c_int32, "pctwin": C.c_uint32, "quality": C.c_uint32, "chance": C.c_uint32,
        "ingredient": C.c_uint32, "ingname": C.c_uint64, "resipct": C.c_int32, "resival": C.c_uint32, "win": C.c_int32}

# ---- the callees faked on both sides
ASSERT, CTRACE, GETRIDE, GETBANK, GETAN, GETHQ = 0x1000DDE8, 0x1006FCBC, 0x10154868, 0x1013EDFC, 0x1013EBC8, 0x1013EE88
DEPOSIT, WITHDRAW, COUNTER, FUNNY, CTOR, ADDEFF, REMEFF = 0x100F32EC, 0x100F33B0, 0x100F7324, 0x101127D0, 0x1016F5D8, 0x101016CC, 0x1010176C
GETSCRIPT, SETVAR, GETRAND = 0x100D668C, 0x100D6734, 0x1013B1E4

# ---- the Mac's addresses of the globals
G_WORLD, G_GWORLD, G_MAP, G_TABLE, G_RIDEDB = 0x1037EC0C, 0x1037EBFC, 0x1037EC08, 0x1037FF18 + 0x55C, 0x1037F6C8
GT_OBJ, GT_GUARD = 0x102B3AA0, 0x102B3AA4
HOST_LAYOUT = [0x2A0, 0x1F100, 0x250, 0x88, 0x448, 4, 6, 0x0E, 0x14, 0x20, 0x2E, 0x40, 0x44, 0x4C, 0x5A, 0x60, 0x6C, 0xF4, 0x1A4, 0x22C, 0x2B4, 0x33C,
               0x17C, 0x180, 0x184, 0x188, 0x18C, 0x190, 0x194, 0x198, 0x19C, 0x1A0]

OBJ_SIZE, GUARD = 0x448, 0x40
AN_SIZE = 0x20800
AN7, AN8 = 0x20130, 0x20380
HIST_BASES = [0x6C, 0xF4, 0x1A4, 0x22C, 0x2B4, 0x33C]
WORD_OFFS = [0x17C, 0x180, 0x184, 0x188, 0x18C, 0x190, 0x194, 0x198, 0x19C, 0x1A0]
FIELDS = [(0x04, "B"), (0x06, "B"), (0x0E, "H"), (0x14, "Q"), (0x20, "I"), (0x2E, "H"), (0x40, "I"), (0x44, "I"), (0x4C, "B"), (0x5A, "H"),
          (0x60, "I")] + [(o, "I") for o in WORD_OFFS]
SIZE_OF = {"B": 1, "H": 2, "I": 4, "Q": 8}
COVERED = [(o, SIZE_OF[f]) for o, f in FIELDS] + [(b, 0x88) for b in HIST_BASES]
L_OBJ, L_WORLD, L_CLK, L_GWORLD, L_BANK, L_AN, L_HQ, L_MAP, L_TABLE, L_SCRIPT, L_RIDEDB, L_BUILT = range(1, 13)
RIDE_SLOTS = 160
S_ING, S_SHOP, S_COST = 83, 86, 107
S_SCRAP1 = 95   # slots 95..98 are the four years; the next upgrade level is 16 slots up


def s32(v):
    return T.s32(v)


def s64(v):
    v &= M64
    return v - (1 << 64) if v & (1 << 63) else v


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    """A float's bits with every NaN made one."""
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def be64(e, a):
    return int.from_bytes(e.read(a, 8), "big")


# ---------------------------------------------------------------------------------------------------------------- rounding


class ROUNDING(HardwareRounding):
    """HardwareRounding aimed at the unit's functions only (Unicorn rounds fused operations twice and saturates fctiwz wrongly)."""

    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes


def make_stubs(S):
    """The Stubs registrations; S is the Ppc side (it holds the log and the case's blocks)."""
    def log():
        return S.e.cl

    def assertion(emu):
        log().rec("GAME_ASSERTMSG", [s32(emu.r(3))], [emu.cstr(emu.r(4))])

    def trace(emu):
        log().rec("CTRACE", [emu.r(3) & MASK], [emu.cstr(emu.r(4))])

    def getride(emu):
        L = log()
        L.rec("GetRideInfo", [L.lab(emu.r(3)), emu.r(4) >> 16])
        emu.set_r(3, S.ride)

    def accessor(name, key):
        def fn(emu):
            L = log()
            L.rec(name, [L.lab(emu.r(3))])
            emu.set_r(3, getattr(S, key))
        return fn

    def getrand(emu):
        L = log()
        L.rec("GetRand", [L.lab(emu.r(3))])
        emu.set_r(3, L.ret("rand") & MASK)

    def deposit(emu):
        L = log()
        L.rec("MakeDeposit", [L.lab(emu.r(3)), emu.r(4) & MASK])

    def withdraw(emu):
        L = log()
        L.rec("MakeWithdrawal", [L.lab(emu.r(3)), emu.r(4) & MASK])

    def counter(emu):
        L = log()
        L.rec("AddToCounter", [L.lab(emu.r(3)), s32(emu.r(4)), s32(emu.r(5)), emu.r(6) >> 16, emu.r(7) >> 16])

    def addeff(emu):
        L = log()
        L.rec("AddRegionalEffector", [L.lab(emu.r(3)), s32(emu.r(4)), emu.r(5) >> 16])

    def remeff(emu):
        L = log()
        L.rec("RemoveRegionalEffector", [L.lab(emu.r(3)), s32(emu.r(4)), emu.r(5) >> 16])

    def getscript(emu):
        log().rec("RSSE_GetScriptPointer", [s32(emu.r(3))])
        emu.set_r(3, S.script)

    def setvar(emu):
        L = log()
        L.rec("RSSE_Setvar", [L.lab(emu.r(3)), s32(emu.r(4)), s32(emu.r(5))])

    def ctor(emu):
        log().rec("CGameType", [1])
        S.e.write(emu.r(3), struct.pack(">I", S.ctor & MASK))

    def funny(emu):
        L = log()
        sp = emu.r(1)
        outs = [emu.r(i) for i in range(5, 11)] + [emu.u32(sp + 0x38)]
        h = emu.u32(sp + 0x3C)
        L.rec("GetFunnyTime", [L.lab(emu.r(4))] + [L.lab(o) for o in outs])
        L.rec("GetFunnyTime.8", [L.lab(h)])
        S.e.write(emu.r(3), (L.ret("ft") & M64).to_bytes(8, "big"))

    def minus(emu):
        L = log()
        a, b = emu.r(4), emu.r(5)
        L.rec("Stamp-", [L.lab(a), s64(be64(S.e, a)), L.lab(b), s64(be64(S.e, b))])
        S.e.write(emu.r(3), (L.ret("minus") & M64).to_bytes(8, "big"))

    def conststr(emu):
        L = log()
        L.rec("GetConstString", [L.lab(emu.r(3)), emu.r(4) & MASK])
        emu.set_r(3, L.ret("name") & MASK)

    return ((ASSERT, assertion), (CTRACE, trace), (GETRIDE, getride), (GETBANK, accessor("GetBankAccount", "bank")),
            (GETAN, accessor("GetParkAnalyser", "analyser")), (GETHQ, accessor("GetChallengeHQ", "hq")), (GETRAND, getrand), (DEPOSIT, deposit),
            (WITHDRAW, withdraw), (COUNTER, counter), (ADDEFF, addeff), (REMEFF, remeff), (GETSCRIPT, getscript), (SETVAR, setvar), (CTOR, ctor),
            (FUNNY, funny)), (("__mi__11TbTimeStampCFRC11TbTimeStamp", minus), ("GetConstString__8TbITableCFUl", conststr))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory (self.fmt is the byte order)."""

    def put(self, a, fmt, value):
        self.write(a, struct.pack(self.fmt[0] + fmt, value))

    def get(self, a, fmt):
        return struct.unpack(self.fmt[0] + fmt, self.read(a, struct.calcsize(fmt)))[0]

    def block(self, size, fill=0xA5):
        a = self.alloc(size)
        self.write(a, bytes([fill]) * size)
        return a


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.ride = self.bank = self.analyser = self.hq = self.script = self.ctor = 0
        self.L = e.cl = CL.PpcLog({}, queues_of(case))
        internals, imports = make_stubs(self)
        stubs = CL.Stubs()
        stubs.real.update(OWN)
        stubs.real.update(HELPERS)
        for addr, fn in internals:
            stubs.internal(addr, fn)
        for name, fn in imports:
            stubs.imp(name, fn)
        self.saved = stubs.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def label(self, label, address):
        self.L.labels[address] = label

    def start(self):
        self.label(L_RIDEDB, G_RIDEDB)

    def log(self):
        return self.L.log

    def set_env(self, world, gworld, map_, table, bank, analyser, hq, script, ctor):
        self.put(G_WORLD, "I", world)
        self.put(G_GWORLD, "I", gworld)
        self.put(G_MAP, "I", map_)
        self.put(G_TABLE, "I", table)
        self.bank, self.analyser, self.hq, self.script, self.ctor = bank, analyser, hq, script, ctor

    def set_ride(self, values, uitype):
        self.ride = self.alloc(0x7B0)
        self.write(self.ride + 0x0C, b"".join(struct.pack(">I", v & MASK) for v in values))
        self.put(self.ride + 0x7A8, "I", uitype & MASK)

    def set_game(self, game):
        self.put(GT_OBJ, "I", 0xA5A5A5A5 if game["first"] else game["type"] & MASK)
        self.write(GT_GUARD, bytes([0 if game["first"] else 1]))

    def game_state(self):
        return {"type": s32(self.get(GT_OBJ, "I")), "guard": self.get(GT_GUARD, "B")}

    def call(self, op, o, args):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN[op], [o] + args)
        if op in VOID:
            return 0
        return s32(r3) if op in SIGNED else r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.labels = {}
        self.queues = queues_of(case)
        self.blocks = []

    def done(self):
        for block in self.blocks:
            self.lib.tpw_otk_free(block)

    def alloc(self, n):
        a = self.lib.tpw_otk_alloc(n)
        self.blocks.append(a)
        return a

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def label(self, label, address):
        self.labels[label] = address

    def start(self):
        self.label(L_RIDEDB, self.lib.tpw_otk_ridedb())
        CL.host_begin(self.lib, self.labels, self.queues)

    def log(self):
        return CL.host_end(self.lib)

    def set_env(self, world, gworld, map_, table, bank, analyser, hq, script, ctor):
        self.lib.tpw_otk_env(world, gworld, map_, table, bank, analyser, hq, script, ctor)

    def set_ride(self, values, uitype):
        self.lib.tpw_otk_ride((C.c_int32 * RIDE_SLOTS)(*[s32(v) for v in values]), s32(uitype))

    def set_game(self, game):
        if not game["first"]:
            self.lib.tpw_otk_mode(s32(game["type"]))

    def game_state(self):
        return {"type": self.lib.tpw_otk_type(), "guard": 1}

    def call(self, op, o, args):
        f = getattr(self.lib, "tpw_otk_" + op)
        if op == "prospective":
            r = f(o, args[0].value, args[1].value)
        else:
            r = f(o, *args)
        if op in VOID:
            return 0
        return r & MASK if op not in SIGNED else r


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_otk_alloc.restype, lib.tpw_otk_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_otk_free.argtypes = [C.c_uint64]
    lib.tpw_otk_ridedb.restype = C.c_uint64
    lib.tpw_otk_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_otk_env.argtypes = [C.c_uint64] * 8 + [C.c_int32]
    lib.tpw_otk_ride.argtypes = [C.POINTER(C.c_int32), C.c_int32]
    lib.tpw_otk_mode.argtypes = [C.c_int32]
    lib.tpw_otk_type.restype = C.c_int32
    for op in FN:
        f = getattr(lib, "tpw_otk_" + op)
        f.argtypes = [C.c_uint64] + HARGS.get(op, [])
        f.restype = HRES.get(op)
    out = (C.c_int64 * 64)()
    n = lib.tpw_otk_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def queues_of(case):
    return {k: [s64(v) for v in vs] for k, vs in case["q"].items()}


# ---------------------------------------------------------------------------------------------------------------- the world of a case


def ride_values(case):
    """The ride record's slots 0 to 159: a hash, then the case's overrides."""
    values = [s32((slot * 7919 + 13) * 31) for slot in range(RIDE_SLOTS)]
    for slot, v in case["slots"].items():
        values[slot] = s32(v)
    return values


def put_hist(s, base, h):
    s.put(base, "I", h["temp"] & MASK)
    for i, v in enumerate(h["data"]):
        s.put(base + 4 + 4 * i, "I", v & MASK)
    s.put(base + 0x7C, "I", h["cur"] & MASK)
    s.put(base + 0x80, "I", h["num"] & MASK)
    s.put(base + 0x84, "B", h["wrap"] & 1)


def build(s, case):
    """Writes the case's blocks through Side.put: the same code for both memories (offsets are the Mac's)."""
    s.obj_base = s.block(OBJ_SIZE + 2 * GUARD)
    o = s.obj = s.obj_base + GUARD
    for off, fmt in FIELDS:
        s.put(o + off, fmt, case["f"][off] & ((1 << (8 * SIZE_OF[fmt])) - 1))
    for base, h in zip(HIST_BASES, case["h"]):
        put_hist(s, o + base, h)
    big = case["op"] == "takings"
    s.an_base = s.block((AN_SIZE if big else 0x40) + 2 * GUARD)
    an = s.an = s.an_base + GUARD
    if big:
        s.put(an + AN7, "I", case["an"][0] & MASK)
        s.put(an + AN8, "I", case["an"][1] & MASK)
    world, gworld = s.block(0x400), s.block(0x400)
    map_, table, bank, hq, script = [s.block(0x40) for _ in range(5)]
    s.set_env(world, gworld, map_, table, bank, an, hq, script, case.get("game", {}).get("ctor", 1))
    s.set_ride(ride_values(case), case["uitype"])
    if "game" in case:
        s.set_game(case["game"])
    for label, address in ((L_OBJ, o), (L_WORLD, world), (L_CLK, world + 0x2A0), (L_GWORLD, gworld), (L_BANK, bank), (L_AN, an), (L_HQ, hq),
                           (L_MAP, map_), (L_TABLE, table), (L_SCRIPT, script), (L_BUILT, o + 0x14)):
        s.label(label, address)
    s.start()


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides."""
    o = s.obj
    whole = bytearray(s.read(s.obj_base, OBJ_SIZE + 2 * GUARD))
    for off, size in COVERED:
        whole[GUARD + off:GUARD + off + size] = bytes(size)
    fields = [s.get(o + off, fmt) for off, fmt in FIELDS]
    for i, (off, fmt) in enumerate(FIELDS):
        if off in (0x40, 0x44):
            fields[i] = canon_f(fields[i])
    hists = [[s.get(o + b + 4 * i, "I") for i in range(33)] + [s.get(o + b + 0x84, "B")] for b in HIST_BASES]
    out = {"answer": answer, "log": s.log(), "fields": fields, "hist": hists, "rest": [len(whole), zlib.crc32(bytes(whole))]}
    if case["op"] == "takings":
        raw = bytearray(s.read(s.an_base, AN_SIZE + 2 * GUARD))
        for off in (AN7, AN8):
            raw[GUARD + off:GUARD + off + 4] = bytes(4)
        out["an"] = [s.get(s.an + AN7, "I"), s.get(s.an + AN8, "I"), len(raw), zlib.crc32(bytes(raw))]
    if "game" in case:
        out["game"] = s.game_state()
    return out


def args_of(side_is_ppc, case):
    op, a = case["op"], case["args"]
    if op in ("takings", "cost", "toilet", "setprize", "setprice", "setingredient", "express"):
        v = a[0] & MASK
        return [v] if side_is_ppc else [v if op in ("takings", "cost", "toilet") else s32(v)]
    if op in ("setquality", "setchance"):
        return [a[0] & 0xFF]
    if op == "prospective":
        return [T.F(a[0]), T.F(a[1])] if side_is_ppc else [C.c_float(a[0]), C.c_float(a[1])]
    if op == "win":
        return [0]
    return []


def run(s, case, is_ppc):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], s.obj, args_of(is_ppc, case)))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case, True)


def host(lib, case):
    return run(Host(lib, case), case, False)


class Base(FnTest):
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


# ---------------------------------------------------------------------------------------------------------------- case generators

DAY = 864000000000
MONTH, YEAR = 30 * DAY, 365 * DAY
WORDS = [0, 1, 2, 0x7F, 0x80, 0xFF, 0x100, 0x101, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFF, 0xFFFFFFFE, 0x12345678, 0xDEADBEEF]
AMOUNTS = [0, 1, 2, 19, 20, 21, 100, 1000, 2000, 0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFF]
STAMPS = [0, 1, M64, I64_MAX, 1 << 63, 0x123456789ABCDEF0, 0xFEDCBA9876543210, DAY, 1 << 32]
SPANS = [0, 1, -1, DAY - 1, DAY, DAY + 1, -DAY + 1, -DAY, -DAY - 1, I64_MIN, I64_MAX, 1 << 32, (1 << 32) - 1, -(1 << 32), DAY * 5, -DAY * 5,
         DAY * 0x7FFFFFFF, DAY * 0x80000000, DAY * 0x100000001, DAY * 0xFFFFFFFF + 5, 29 * DAY, 30 * DAY, 31 * DAY, 364 * DAY, 365 * DAY, 366 * DAY,
         2 * YEAR, 3 * YEAR, 10 * YEAR]
RSPANS = [0, 1, -1, MONTH - 1, MONTH, MONTH + 1, YEAR - 1, YEAR, YEAR + 1, 2 * YEAR - 1, 2 * YEAR, 2 * YEAR + 1, 3 * YEAR - 1, 3 * YEAR, 4 * YEAR,
          10 * YEAR, 1000 * YEAR, -DAY, -MONTH, -YEAR + 1, -YEAR, -YEAR - 1, -2 * YEAR, I64_MIN, I64_MAX, 11 * MONTH, 12 * MONTH, 13 * MONTH]
FLOATS = [0.0, -0.0, 0.5, 1.0, 24.0, 24.5, 24.99, 25.0, 25.01, 25.5, 50.0, 99.9, 100.0, 100.5, 127.5, 255.0, 255.5, 256.0, 300.0, 1000.0, -1.0, -0.5,
          -25.0, 1e10, -1e10, 2147483647.0, 2147483648.0, -2147483648.0, -2147483649.0, 1e30, float("nan"), float("inf"), float("-inf"), 3.9, 0.001]
RV = [0, 1, 2, 5, 50, 99, 100, 101, 255, 1000, -1, -100, INT_MAX, INT_MIN, 12345]
QUALITIES = [0, 49, 50, 51, 150, 250, 255]
RANDS = [0, 1, 49, 50, 99, 100, 101, 149, 150, 255, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
GAMETYPES = [0, 1, 2, 3, -1, 0x100]
F_OFFS = [off for off, _ in FIELDS]


def rnd_word(r):
    return r.choice(WORDS) if r.random() < 0.6 else r.getrandbits(32)


def rnd_field(r, off):
    if off in (0x04, 0x06):
        return r.choice([0, 1, 127, 128, 255, r.randrange(256)])
    if off == 0x0E:
        return r.choice([0, 1, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16)])
    if off == 0x14:
        return r.choice(STAMPS)
    if off == 0x2E:
        return r.choice([0, 1, 2, 3, 0x8000, 0xFFFF, 0xFFFE, r.getrandbits(16)])
    if off in (0x40, 0x44):
        return fb(r.choice(FLOATS)) if r.random() < 0.8 else fb(r.uniform(-50, 300))
    if off == 0x4C:
        return r.randrange(4)
    if off == 0x5A:
        return r.getrandbits(16)
    return rnd_word(r)


def rnd_hist(r):
    data = [r.randrange(0, 200) if r.random() < 0.6 else rnd_word(r) for _ in range(30)]
    return {"temp": rnd_word(r), "data": data, "cur": r.choice([-1, 0, 29, r.randrange(30), r.randrange(30)]),
            "num": 30 if r.random() < 0.75 else r.choice([1, 5, 29, 30]), "wrap": r.choice([0, 1])}


def rnd_slots(r):
    slots = {S_ING: r.choice([0, 1, 2, 3, 4, 5, -1, 7]), S_SHOP: r.choice(list(range(-1, 9))), S_COST: r.choice(RV)}
    for level in range(4):
        for year in range(4):
            slots[S_SCRAP1 + year + 16 * level] = r.choice(RV) if r.random() < 0.7 else s32(r.getrandbits(32))
    return slots


def blank(r, op, args=()):
    c = {"op": op, "args": list(args), "f": {off: rnd_field(r, off) for off in F_OFFS}, "h": [rnd_hist(r) for _ in range(6)],
         "uitype": r.choice([0, 1, 1, 2, 2, 3, 4, -1, 0x100]), "slots": rnd_slots(r),
         "q": {"ft": [r.choice([0, 1, M64, I64_MAX, 1 << 63, r.getrandbits(64), r.getrandbits(40)])],
               "minus": [r.choice(SPANS) if r.random() < 0.7 else s64(r.getrandbits(r.choice([20, 40, 50, 64])))],
               "rand": [r.choice(RANDS) if r.random() < 0.6 else r.getrandbits(32)], "name": [rnd_word(r)]},
         "an": [rnd_word(r), rnd_word(r)]}
    if op == "toilet":
        c["game"] = {"type": r.choice(GAMETYPES), "first": False, "ctor": 1}
    return c


def mk(op, seed, args=(), f=None, h=None, slots=None, uitype=None, q=None, game=None, an=None):
    """A case from a seed with the given parts replaced (h: {history index: dict of the parts to replace})."""
    r = random.Random(seed * 131 + len(op) * 7 + 3)
    c = blank(r, op, args)
    if f:
        c["f"].update(f)
    for k, v in (h or {}).items():
        c["h"][k].update(v)
    if slots:
        c["slots"].update(slots)
    if uitype is not None:
        c["uitype"] = uitype
    if q:
        c["q"].update(q)
    if game is not None:
        c["game"] = dict(c.get("game", {"ctor": 1}), **game)
    if an is not None:
        c["an"] = an
    return c


def rcase(op, args_fn=lambda r: (), **fixed):
    """A random case maker: the blank case with the op's arguments."""
    def make(r):
        c = blank(r, op, args_fn(r))
        for k, v in fixed.items():
            c[k] = v
        return c
    return make


def pick_amount(r):
    return r.choice(AMOUNTS) if r.random() < 0.7 else r.getrandbits(32)


def hist_of(values, cur=29, num=30, wrap=1, temp=0):
    """A history holding `values` (the rest random-free zeros)."""
    data = list(values) + [0] * (30 - len(values))
    return {"temp": temp, "data": data[:30], "cur": cur, "num": num, "wrap": wrap}


class Seed:
    """A deterministic counter for the edge cases."""
    n = 0

    @classmethod
    def next(cls):
        cls.n += 1
        return cls.n


# ---- one-field getters and setters
def getter_edges(op, off):
    def edges():
        for i, v in enumerate(WORDS):
            yield mk(op, 1000 + i, f={off: v})
    return edges


def setter_edges(op, args):
    def edges():
        for i, v in enumerate(args):
            yield mk(op, 2000 + i, [v])
    return edges


def counter_edges(op, index, off):
    """walked / customer / winner: the total word and the running count of one history."""
    def edges():
        for i, t in enumerate(WORDS):
            yield mk(op, 3000 + i, h={index: {"temp": t}}, f={off: WORDS[(i * 5 + 3) % len(WORDS)]} if off else None)
    return edges


# ---- age and the residual value
def age_edges():
    for i, span in enumerate(SPANS):
        for j, st in enumerate(STAMPS[:4]):
            yield mk("age", 4000 + i * 7 + j, f={0x14: st}, q={"ft": [[0, 1, I64_MAX, 1 << 40][j]], "minus": [span]})


def resi_edges(op):
    def edges():
        for i, span in enumerate(RSPANS):
            for cust in (0, 5):
                level = (i + cust) % 4
                yield mk(op, 5000 + i * 3 + cust, f={0x19C: cust, 0x4C: level}, q={"minus": [span]})
        for level in range(4):
            for v in (1, 0, -1, 100, INT_MIN, INT_MAX, 2):
                slots = {S_SCRAP1 + 16 * level: v, S_COST: 77}
                yield mk(op, 5500 + level * 11 + (v & 0xFF), f={0x19C: 0, 0x4C: level}, q={"minus": [MONTH - 1 if v != 2 else 0]}, slots=slots)
        for i, cost in enumerate(RV):
            yield mk(op, 5800 + i, f={0x19C: 3}, q={"minus": [RSPANS[i * 2 % len(RSPANS)]]}, slots={S_COST: cost})
    return edges


# ---- takings and costs
def takings_edges():
    n = 0
    for shop in range(-2, 9):
        for ing in (-1, 0, 1, 2, 3, 4, 5):
            n += 1
            yield mk("takings", 6000 + n, [AMOUNTS[n % len(AMOUNTS)]], uitype=1, slots={S_SHOP: shop, S_ING: ing})
    for ui in (0, 2, 3, 4, -1, 5, 0x100):
        for amount in AMOUNTS:
            n += 1
            yield mk("takings", 6200 + n, [amount], uitype=ui)
    for amount in AMOUNTS:
        n += 1
        yield mk("takings", 6400 + n, [amount], uitype=1, slots={S_SHOP: 4, S_ING: 2}, an=[amount, 0xFFFFFFFF - amount])


def cost_edges():
    n = 0
    for ui in (0, 1, 2, 3, 4, -1, 0x100):
        for amount in AMOUNTS:
            n += 1
            yield mk("cost", 7000 + n, [amount], uitype=ui)


# ---- the cost of goods
def goods_edges():
    n = 0
    for q in QUALITIES + [0x100, 0x1FF, 0x7F00]:
        for ing in QUALITIES + [0x180]:
            n += 1
            yield mk("goods", 8000 + n, f={0x188: q, 0x194: ing, 0x184: WORDS[n % len(WORDS)]}, slots={S_ING: [1, 3, 2, 4, 0, 5, -1][n % 7]})
    for ing in (1, 3):
        for cost in (0, 1, 1000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
            n += 1
            yield mk("goods", 8500 + n, f={0x188: 250, 0x194: 0, 0x184: cost}, slots={S_ING: ing})


QP = [0.0, 49.0, 50.0, 51.0, 150.0, 250.0, 255.0, -100.0, 1e9, 49.99, 149.99, 150.01, 249.99, 250.01, -0.0, 1e30, float("nan"), float("inf"), float("-inf")]


def prospective_edges():
    n = 0
    for i, q in enumerate(QP):
        for j, ing in enumerate(QP[::3] + [75.0, 123.5]):
            n += 1
            yield mk("prospective", 9000 + n, [T.f32(q), T.f32(ing)], f={0x184: WORDS[(i + j) % len(WORDS)]}, slots={S_ING: [1, 3, 2, 4, 0, 5, -1][(i + j) % 7]})


def prospective_rand(r):
    def val():
        return T.f32(r.choice(QP) if r.random() < 0.5 else r.uniform(-300, 600))
    return blank(r, "prospective", [val(), val()])


# ---- histories
def sums_edges(op, index_a, index_b=None):
    def edges():
        n = 0
        for a_vals in ([0] * 30, [1] * 30, [0xFFFFFFFF] * 30, list(range(30)), [0x80000000] * 30, [100] * 30):
            for b_vals in ([0] * 30, [1] * 30, [0xFFFFFFFF] * 30, list(range(30, 0, -1)), [7] * 30):
                n += 1
                h = {index_a: hist_of(a_vals, cur=n % 30, num=30, wrap=n & 1)}
                if index_b is not None:
                    h[index_b] = hist_of(b_vals, cur=(n * 7) % 30)
                yield mk(op, 10000 + n, h=h)
        for num in (0, 1, 5, 29):
            for cur in (-1, 0, 29):
                n += 1
                h = {index_a: hist_of(list(range(1, 31)), cur=cur, num=num, wrap=1)}
                if index_b is not None:
                    h[index_b] = hist_of(list(range(5, 35)), cur=cur, num=30)
                yield mk(op, 10500 + n, h=h)
    return edges


def profit_edges():
    n = 0
    for costs in ([0] * 30, [1] * 30, [0xFFFFFFFF] * 30, list(range(30)), [0x7FFFFFFF] * 30):
        for takings in ([0] * 30, [1] * 30, [0xFFFFFFFF] * 30, list(range(30, 0, -1)), [0x40000000] * 30):
            n += 1
            yield mk("profit", 11000 + n, h={0: hist_of(takings, cur=n % 30), 1: hist_of(costs, cur=(n * 3) % 30)})
    for num in (0, 1, 29):
        n += 1
        yield mk("profit", 11500 + n, h={0: hist_of(list(range(30)), num=num), 1: hist_of(list(range(30)), num=30)})
        n += 1
        yield mk("profit", 11500 + n, h={0: hist_of(list(range(30)), num=30), 1: hist_of(list(range(30)), num=num)})


def satisfaction_edges():
    n = 0
    for vals in ([0] * 30, [50] * 30, [-50 & MASK] * 30, [INT_MAX] * 30, [INT_MIN & MASK] * 30, list(range(30)), [(-i) & MASK for i in range(30)]):
        for cur in (0, 13, 29):
            n += 1
            yield mk("satisfaction", 12000 + n, h={5: hist_of(vals, cur=cur, num=30, wrap=1)})
    for num in (1, 5, 29):
        for cur in (-1, 0, 29):
            n += 1
            yield mk("satisfaction", 12500 + n, h={5: hist_of(list(range(1, 31)), cur=cur, num=num, wrap=0)})


def express_edges():
    n = 0
    for temp in (0, 1, 0xFFFFFFFF, 2, 3, INT_MAX, INT_MIN & MASK, 100, 0xFFFFFFFD):
        for v in (0, 1, -1, 2, 3, -3, INT_MAX, INT_MIN, 100, -100):
            n += 1
            yield mk("express", 13000 + n, [v], h={5: {"temp": temp}})


# ---- the special ingredient's name
def ingname_edges():
    n = 0
    for ing in (-3, -1, 0, 1, 2, 3, 4, 5, 6, 7, INT_MIN, INT_MAX):
        for name in (0, 1, 0x12345678, 0xFFFFFFFF):
            n += 1
            yield mk("ingname", 14000 + n, slots={S_ING: ing}, q={"name": [name]})


# ---- the toilet
def toilet_edges():
    # the very first use of the game type object: the constructor runs on both sides (do not call tpw_otk_mode before it)
    yield mk("toilet", 15000, [100], f={0x2E: 1, 0x40: fb(10.0), 0x20: 7}, game={"first": True, "ctor": 1, "type": 1})
    n = 0
    for gtype in (0, 1, 2, -1):
        for flags in (0, 1, 3):
            for i, clean in enumerate(FLOATS):
                n += 1
                yield mk("toilet", 15100 + n, [AMOUNTS[(i + n) % len(AMOUNTS)]], f={0x2E: flags, 0x40: fb(clean), 0x44: fb(1.0)},
                         game={"type": gtype, "first": False})
    for clean in (25.0, 25.5, 30.0, 100.0, 24.5):
        for amount in (0, 1, 20, 100, 101, 500, 2000, 2001):
            n += 1
            yield mk("toilet", 16000 + n, [amount], f={0x2E: 1, 0x40: fb(clean)}, game={"type": 0, "first": False})
    for cx, cy in ((0, 0), (255, 255), (127, 3), (3, 200)):
        n += 1
        yield mk("toilet", 16500 + n, [2000], f={0x2E: 1, 0x40: fb(60.0), 0x04: cx, 0x06: cy}, game={"type": 0, "first": False})
        n += 1
        yield mk("toilet", 16500 + n, [0], f={0x2E: 1, 0x40: fb(5.0), 0x04: cx, 0x06: cy}, game={"type": 1, "first": False})


def toilet_rand(r):
    c = blank(r, "toilet", [pick_amount(r)])
    c["f"][0x2E] = r.choice([0, 1, 1, 3, r.getrandbits(16)])
    return c


# ---- the sideshow's draw
def win_edges():
    n = 0
    for ui in (2, 0, 1, 3, -1):
        for chance in (0, 1, 50, 99, 100, 101, 255):
            for rand in RANDS[:10]:
                n += 1
                yield mk("win", 17000 + n, [0], uitype=ui, f={0x18C: (0xABCDEF00 | chance) if n % 3 else chance}, q={"rand": [rand]})


def win_rand(r):
    c = blank(r, "win", [0])
    c["f"][0x18C] = r.choice([0, 1, 50, 99, 100, 101, 255]) | (r.getrandbits(24) << 8 if r.random() < 0.4 else 0)
    c["uitype"] = r.choice([2, 2, 0, 1, 3])
    return c


# ---- the arguments of the random cases
def life_edges():
    for i, x in enumerate(FLOATS):
        yield mk("life", 18000 + i, f={0x44: fb(x)})


def make(name, ppc_fn, host_fn, edges, rnd):
    return Base(name, ppc_fn, host_fn, edge=edges, rnd=rnd)


def small_arg(r):
    return [r.choice(WORDS) if r.random() < 0.6 else r.getrandbits(32)]


def byte_arg(r):
    return [r.choice([0, 1, 0x7F, 0x80, 0xFE, 0xFF, r.randrange(256)])]


TESTS = [
    make("CObject::GetObjectAge", ppc, host, age_edges, rcase("age")),
    make("CObject::GetRemainingLife", ppc, host, life_edges, rcase("life")),
    make("CObject::WalkedAway", ppc, host, counter_edges("walked", 3, 0x1A0), rcase("walked")),
    make("CObject::HadCustomer", ppc, host, counter_edges("customer", 2, 0x19C), rcase("customer")),
    make("CObject::RecordNewTakings", ppc, host, takings_edges, rcase("takings", lambda r: [pick_amount(r)])),
    make("CObject::RecordACost", ppc, host, cost_edges, rcase("cost", lambda r: [pick_amount(r)])),
    make("CObject::HadAWinner", ppc, host, counter_edges("winner", 4, None), rcase("winner")),
    make("CObject::GetPricePerUse", ppc, host, getter_edges("price", 0x190), rcase("price")),
    make("CObject::GetPrizeValue", ppc, host, getter_edges("prize", 0x184), rcase("prize")),
    make("CObject::SetPrizeValue", ppc, host, setter_edges("setprize", WORDS), rcase("setprize", small_arg)),
    make("CObject::GetProspectiveCostOfGoods", ppc, host, prospective_edges, prospective_rand),
    make("CObject::GetCostOfGoods", ppc, host, goods_edges, rcase("goods")),
    make("CObject::GetTotalRevenue", ppc, host, lambda: [mk("revenue", 19000 + i, f={0x17C: a, 0x180: b}) for i, (a, b) in
                                                       enumerate((a, b) for a in WORDS[:9] for b in WORDS[8:])], rcase("revenue")),
    make("CObject::GetProfitability", ppc, host, profit_edges, rcase("profit")),
    make("CObject::ExpressSatisfaction", ppc, host, express_edges, rcase("express", lambda r: [r.choice([0, 1, -1, 2, 3, INT_MAX, INT_MIN]) if r.random() < 0.5 else s32(r.getrandbits(32))])),
    make("CObject::GetCustomerSatisfaction", ppc, host, satisfaction_edges, rcase("satisfaction")),
    make("CObject::GetPercentageOfWinners", ppc, host, sums_edges("pctwin", 2, 4), rcase("pctwin")),
    make("CObject::GetProductQuality", ppc, host, getter_edges("quality", 0x188), rcase("quality")),
    make("CObject::SetProductQuality", ppc, host, setter_edges("setquality", [0, 1, 0x7F, 0x80, 0xFE, 0xFF]), rcase("setquality", byte_arg)),
    make("CObject::GetChanceOfWinning", ppc, host, getter_edges("chance", 0x18C), rcase("chance")),
    make("CObject::SetChanceOfWinning", ppc, host, setter_edges("setchance", [0, 1, 50, 99, 100, 0xFF]), rcase("setchance", byte_arg)),
    make("CObject::SetAmountOfSpecialIngredient", ppc, host, setter_edges("setingredient", WORDS), rcase("setingredient", small_arg)),
    make("CObject::GetAmountOfSpecialIngredient", ppc, host, getter_edges("ingredient", 0x194), rcase("ingredient")),
    make("CObject::GetSpecialIngredientName", ppc, host, ingname_edges, rcase("ingname")),
    make("CObject::GetResidualValuePercentage", ppc, host, resi_edges("resipct"), rcase("resipct")),
    make("CObject::GetCurrentResidualValue", ppc, host, resi_edges("resival"), rcase("resival")),
    make("CObject::PersonUsedToilet", ppc, host, toilet_edges, toilet_rand),
    make("CObject::WillIWin", ppc, host, win_edges, win_rand),
    make("CObject::SetPricePerUse", ppc, host, setter_edges("setprice", WORDS), rcase("setprice", small_arg)),
]
