"""Differential tests for unit w202, the ride's admittance and queue length (src/game/Object.cpp). See difftest.py and tpwtest.py.

The eleven CObject members HasFreeQueueSpace, GetIdealQueueLength, QueueTooLong, AdmitPerson, FinishedAdmitting, ReadyToAdmit,
PersonLeftDuringAdmittance, LetPeopleOn, LetPeopleOff, ProcessStateFunctioning and GoFirstPerson. Both sides work in ONE 2 MB block (the PowerPC side maps
it at RB, the host library's gRegion has the same layout): the call trace at 0x100 (tag, value pairs, pointers as offsets from the block), the ride object
(0x448 bytes between guard bytes), 64 guest blocks of 0x240 bytes (the thing id at +0, the state word at +0x220), the ride record, a script block, 64 anim
set blocks and a world-sized block (only mTurn is read). The thing table's rows point at the guest blocks. The ride record's value slots are 4 bytes on the
Mac and 8 on the host (BalanceValue holds a pointer), so each side gets the record in its own layout from the same 160 words.
Every callee outside the unit is a fake that appends the same (tag, value) pairs to the trace on both sides: the assert and CTRACE (every argument), the
script's variables and tour, GetNumPeopleInQueue, RemovePersonFromQueue, IsCoaster, SetState (the real one is another unit's), the five CPerson members the
ride calls (ReadyForAdmitting and ExitRide answer a word per guest), the two camera mesh finders, ToggleRideRideIn1stPerson, ActivateHoardings and the ride
record lookup (CRideDatabase::GetRideInfo). CThing::GetThingId, cw_cvt_fp2unsigned and the float conversions run as the original.
A case compares the answer, the whole trace, the ride object (the named fields as numbers, the rest as bytes, the guard bytes), the guest blocks and the
bits of the speed ratio global.
Not tested: a thing row without a guest where the ride fetches one (the original dereferences the null), an upgrade level above 3 (the record holds four),
an object type that is not in the ride database, and ProcessStateFunctioning with a remaining life (mF44) of 2^31 or more (the emulator's fctiwz answers
0x80000000 there, the Mac and ppc_fctiwz 0x7FFFFFFF, so the two sides would choose another state).
"""
import ctypes as C
import itertools
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ObjectAdmit"

MASK = 0xFFFFFFFF
RB = 0x70000000
REGION = 0x200000
TRACE, CAP = 0x100, 0x1000
OBJ, PERS, REC, SCRIPT, SETS, WORLD = 0x10000, 0x11000, 0x1A000, 0x1B000, 0x1C000, 0x20000
OBJ_SIZE, GUARD, PSIZE, NSLOT, NROWS, NSET, SET_SIZE = 0x448, 0x40, 0x240, 64, 256, 64, 0x40
TURN_AT = 0x1DA70C
NREC, MAC_SLOT0 = 160, 0x0C
TABLE, G_WORLD, G_SETS, G_RATIO, G_DATABASE = 0x1034CBFC, 0x1037EC0C, 0x102C79A4, 0x1034C35C, 0x1037F6C8

ASSERT, CTRACE, GETSCRIPT, GETVAR, SETVAR, TOUR = 0x1000DDE8, 0x1006FCBC, 0x100D668C, 0x100D6C58, 0x100D6734, 0x100D6BD0
GETINFO, COASTER, NUM, REMOVE, SETSTATE = 0x10154868, 0x10110CD8, 0x10108EF4, 0x10108C20, 0x1010DC94
FORCE, DISMISS, READYFOR, ONADMIT, EXITRIDE = 0x1011DF70, 0x1011F1B8, 0x1011F0B8, 0x1011F114, 0x1011F5A4
BMP, FLY, TOGGLE, HOARDINGS = 0x1002CF30, 0x10075278, 0x10032FF4, 0x100614A0
HAS_FREE, IDEAL, TOO_LONG, ADMIT, FINISHED, READY, LEFT = 0x101085A0, 0x10108624, 0x10108818, 0x1010D0E0, 0x1010D35C, 0x1010D3EC, 0x1010D464
ON, OFF, FUNCTIONING, FIRST = 0x1010E61C, 0x1010EA00, 0x1010EB44, 0x1010ED8C
OWN = {HAS_FREE, IDEAL, TOO_LONG, ADMIT, FINISHED, READY, LEFT, ON, OFF, FUNCTIONING, FIRST}

# sizeof CObject, offsets of mBase, mObjectType, mF1C, mF20, mF24, mFlags, mQueuedPerson, mF3C, mF44, mF4C, mF54, mF58, mF59, mF64, mPersonLoaded, mState,
# CWorld::mTurn, then the offset of the ride record's value slot 0, sizeof BalanceValue and the number of value slots
HOST_LAYOUT = [0x448, 0, 0x0E, 0x1C, 0x20, 0x24, 0x2E, 0x38, 0x3C, 0x44, 0x4C, 0x54, 0x58, 0x59, 0x64, 0x68, 0x198, 0x1DA70C, 0x18, 8, 160]
HOST_SLOT0 = HOST_LAYOUT[18]

# The ride record's value slots the unit reads (the first level's for the upgrade array; a level is 16 slots on).
SPRITES, TRACK_TYPE, CONSTRAIN, INIT_SPEED, WAIT_CONSTANT = 29, 36, 68, 103, 106

OBJ_FIELDS = {"tid": (0x00, "H"), "type": (0x0E, "H"), "f1c": (0x1C, "I"), "f20": (0x20, "I"), "f24": (0x24, "i"), "flags": (0x2E, "H"),
              "queued": (0x38, "H"), "f3c": (0x3C, "I"), "f44": (0x44, "I"), "f4c": (0x4C, "B"), "f54": (0x54, "I"), "f58": (0x58, "B"),
              "f59": (0x59, "B"), "f64": (0x64, "I"), "loaded": (0x68, "H"), "state": (0x198, "i")}
SIZES = {"B": 1, "H": 2, "I": 4, "i": 4}


def fb(x):
    return T.f32_bits(x)


WORDS = (0, 1, 2, 3, 0x100, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF)
ANSWERS = (0, 0, 1, 1, 2, 0x100, 0x10000, 0xFFFFFFFF, 0x80000000)   # what a fake answers: zero is false, every other word is true
OBJ_STATES = (0, 1, 2, 3, 4, 5, 0xFFFFFFFF, 0x100, 0x10001, 0x80000000)
OPEN_WORDS = (1, 1, 2, 0x100, 0x10000, 0x80000000)   # mF64 of a running ride
GUEST_STATES = (0xD, 0xE, 0x10, 0, 1, 0xC, 0xF, 0x11, 0x10D, 0x1000E, 0x1000D, 0x8000000D, 0x80000010, 0x110, 0xFFFFFFFF)
F3C = (0, 1, 2, 3, 4, 5, 25, 100, 0x1FFFFFFF, 0x20000000, 0x3FFFFFFF, 0x40000000, 0x40000001, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF)
SPEEDS = (0, 0, 1, 2, 5, 50, 100, 150, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 16777217)   # mF54, an unsigned word
INITS = (0, 1, -1, 2, 3, 5, 50, 100, 1000, 0x7FFFFFFF, -0x80000000, 16777217)   # the record's initial speed, a signed word
BYTES = (0, 1, 2, 3, 5, 10, 60, 100, 127, 128, 255)
CONSTANTS = (fb(0.0), fb(-0.0), fb(1.0), fb(0.5), fb(2.0), fb(3.0), fb(40.0), fb(0.1), fb(1.2), fb(-1.0), 0x7FC00000, 0x7F800000, 0xFF800000, 1,
             0x7F7FFFFF, fb(1e9), fb(3e9), fb(1e10), fb(7.5), fb(0.75))
# mF44 (the remaining life) floats. None is 2^31 or more: the emulator's fctiwz answers 0x80000000 for them where the Mac saturates to 0x7FFFFFFF.
LIFE = tuple(fb(x) for x in (0.0, -0.0, 0.5, 0.99, 1.0, 100.0, 254.9, 255.0, 255.5, 256.0, 256.5, 257.0, 511.0, 2147483520.0, -1.0, -255.0, -256.0,
                             -257.0, -2147483648.0, -3e9, -1e10)) + (0x7FC00000, 0xFF800000)
# Queue length tuples (mF54, the record's initial speed, the queue constant's bits, mF59, mF58) found by a search over the single precision chain: another
# order of the steps (left to right, divide first, a reciprocal, the whole chain in double, the ratio in double) gives another length for each.
SENSITIVE = ((200, 100, 1122369536, 43, 115), (2, 2, 1119617024, 243, 94), (30, 10, 1125777408, 117, 231), (50, 50, 1120731136, 48, 41),
             (10, 10, 1120154419, 210, 109), (20, 10, 1128084275, 15, 22), (15, 5, 1117650944, 161, 79), (6, 3, 1106142822, 20, 149),
             (15, 5, 1123182182, 220, 101), (30, 10, 1115055718, 85, 154), (10, 17, 1125181030, 34, 138), (48, 43, 1112041062, 215, 167),
             (7, 23, 1100899942, 115, 99), (40, 101, 1115829043, 101, 186), (56, 27, 1121714176, 243, 220), (33, 27, 1124335616, 108, 48),
             (5, 3, 1109236122, 219, 197), (29, 13, 1118437376, 117, 3), (27, 15, 1118502912, 240, 76), (29, 47, 1122304000, 188, 229))


def put(buf, e, fmt, off, v):
    if fmt == "i":
        v = T.s32(v)
    else:
        v &= (1 << (8 * SIZES[fmt])) - 1
    struct.pack_into(e + fmt, buf, off, v)


def get(buf, e, fmt, off):
    return struct.unpack_from(e + fmt, buf, off)[0]


# ------------------------------------------------------------------ the two sides


class PpcSide:
    big = True

    def __init__(self, e):
        self.e = e

    def write(self, off, data):
        self.e.write(RB + off, bytes(data))

    def read(self, off, n):
        return self.e.read(RB + off, n)


class HostSide:
    big = False

    def __init__(self, lib):
        self.lib, self.base = lib, lib.tpw_oad_base()

    def write(self, off, data):
        data = bytes(data)
        C.memmove(C.c_void_p(self.base + off), data, len(data))

    def read(self, off, n):
        return C.string_at(self.base + off, n)


def obj_bytes(c, big):
    e = ">" if big else "<"
    buf = bytearray(c["objfill"])
    for name, (off, fmt) in OBJ_FIELDS.items():
        put(buf, e, fmt, off, c["obj"][name])
    return bytes(buf)


def guest_bytes(p, big):
    e = ">" if big else "<"
    buf = bytearray(p["fill"])
    put(buf, e, "H", 0x00, p["tid"])
    put(buf, e, "I", 0x220, p["state"])
    return bytes(buf)


def record_bytes(c, big):
    """The ride record's 160 value slots in one side's layout: 4 bytes each on the Mac, 8 on the host (junk in the upper half)."""
    if big:
        return struct.pack(">%dI" % NREC, *c["rec"])
    return b"".join(struct.pack("<II", lo, hi) for lo, hi in zip(c["rec"], c["rechi"]))


def build(side, c):
    e = ">" if side.big else "<"
    side.write(OBJ - GUARD, b"\xa5" * GUARD)
    side.write(OBJ, obj_bytes(c, side.big))
    side.write(OBJ + OBJ_SIZE, b"\xa5" * GUARD)
    for slot, p in c["slots"].items():
        side.write(PERS + PSIZE * slot, guest_bytes(p, side.big))
    side.write(REC + (MAC_SLOT0 if side.big else HOST_SLOT0), record_bytes(c, side.big))
    side.write(WORLD + TURN_AT, struct.pack(e + "I", c["turn"] & MASK))


def observe(side, c, ret, ratio, n, tags, vals):
    e = ">" if side.big else "<"
    raw = bytearray(side.read(OBJ, OBJ_SIZE))
    obj = {}
    for name, (off, fmt) in OBJ_FIELDS.items():
        obj[name] = get(raw, e, fmt, off)
        raw[off:off + SIZES[fmt]] = bytes(SIZES[fmt])
    guards = side.read(OBJ - GUARD, GUARD) + side.read(OBJ + OBJ_SIZE, GUARD)
    people = []
    for slot in sorted(c["slots"]):
        blk = bytearray(side.read(PERS + PSIZE * slot, PSIZE))
        people.append([get(blk, e, "H", 0), get(blk, e, "I", 0x220)])
        blk[0:2] = bytes(2)
        blk[0x220:0x224] = bytes(4)
        people.append(bytes(blk))
    return dict(ret=ret, ratio=ratio, n=n, tags=tags, vals=vals, obj=obj, rest=bytes(raw), guards=guards, people=people)


# ------------------------------------------------------------------ the PowerPC side


def tr(e, tag, val):
    n = e.u32(RB)
    if n < CAP:
        e.write(RB + TRACE + 8 * n, struct.pack(">II", tag, val & MASK))
    e.write(RB, struct.pack(">I", n + 1))


def rel(p):
    return (p - RB) & MASK


def text_hash(s):
    h = 0
    for ch in s.encode("latin-1"):
        h = (h * 31 + ch) & MASK
    return h


def walk(emu, fmt, reg, tag):
    """One trace entry for each conversion of the format, from register reg on (a %s as the hash of its text)."""
    i = 0
    while i < len(fmt):
        if fmt[i] != "%":
            i += 1
            continue
        i += 1
        if i >= len(fmt):
            break
        if fmt[i] == "%":
            i += 1
            continue
        v = emu.r(reg)
        reg += 1
        tr(emu, tag, text_hash(emu.cstr(v)) if fmt[i] == "s" else v)
        i += 1


def stubs_for(e, c):
    """The fakes of every callee outside the unit; each appends to the trace what the host's fake appends."""
    def guest_slot(p):
        slot = (rel(p) - PERS) // PSIZE
        return slot if 0 <= slot < NSLOT else 0

    def assertion(emu):
        tr(emu, 20, emu.r(3))
        fmt = emu.cstr(emu.r(4))
        tr(emu, 21, text_hash(fmt))
        walk(emu, fmt, 5, 22)

    def ctrace(emu):
        tr(emu, 11, emu.r(3))
        fmt = emu.cstr(emu.r(4))
        tr(emu, 12, text_hash(fmt))
        walk(emu, fmt, 5, 13)

    def getscript(emu):
        tr(emu, 30, emu.r(3))
        emu.set_r(3, RB + SCRIPT)

    def getvar(emu):
        index = T.s32(emu.r(4))
        tr(emu, 31, rel(emu.r(3)))
        tr(emu, 32, emu.r(4))
        emu.set_r(3, (c["vars"][index] & MASK) if 0 <= index < 16 else 0)

    def setvar(emu):
        tr(emu, 33, rel(emu.r(3)))
        tr(emu, 34, emu.r(4))
        tr(emu, 35, emu.r(5))

    def tour(emu):
        tr(emu, 36, rel(emu.r(3)))
        emu.set_r(3, c["tour"] & MASK)

    def count(emu):
        tr(emu, 40, rel(emu.r(3)))
        emu.set_r(3, c["count"] & MASK)

    def remove(emu):
        tr(emu, 41, rel(emu.r(3)))
        tr(emu, 42, emu.r(4) >> 16)

    def coaster(emu):
        tr(emu, 43, rel(emu.r(3)))
        emu.set_r(3, c["coaster"] & MASK)

    def setstate(emu):
        tr(emu, 44, rel(emu.r(3)))
        tr(emu, 45, emu.r(4))

    def person_call(tag):
        def fn(emu):
            tr(emu, tag, rel(emu.r(3)))
        return fn

    def ready(emu):
        tr(emu, 48, rel(emu.r(3)))
        emu.set_r(3, c["ready"][guest_slot(emu.r(3))] & MASK)

    def exit_ride(emu):
        tr(emu, 50, rel(emu.r(3)))
        emu.set_r(3, c["exit"][guest_slot(emu.r(3))] & MASK)

    def bmp(emu):
        tr(emu, 51, emu.r(3))
        emu.set_r(3, c["bmp"] & MASK)

    def fly(emu):
        tr(emu, 52, emu.r(3))
        emu.set_r(3, c["fly"] & MASK)

    def toggle(emu):
        for k in range(6):
            tr(emu, 53 + k, emu.r(3 + k))

    def getinfo(emu):
        tr(emu, 60, 1 if emu.r(3) == G_DATABASE else 0)
        tr(emu, 61, emu.r(4) >> 16)
        emu.set_r(3, RB + REC)

    def hoardings(emu):
        tr(emu, 62, rel(emu.r(3)))
        tr(emu, 63, emu.r(4))

    S = CL.Stubs()
    S.real = OWN
    for addr, fn in ((ASSERT, assertion), (CTRACE, ctrace), (GETSCRIPT, getscript), (GETVAR, getvar), (SETVAR, setvar), (TOUR, tour), (NUM, count),
                     (REMOVE, remove), (COASTER, coaster), (SETSTATE, setstate), (FORCE, person_call(46)), (DISMISS, person_call(47)),
                     (READYFOR, ready), (ONADMIT, person_call(49)), (EXITRIDE, exit_ride), (BMP, bmp), (FLY, fly), (TOGGLE, toggle),
                     (GETINFO, getinfo), (HOARDINGS, hoardings)):
        S.internal(addr, fn)
    return S


def setup(runner):
    emu = runner.emu
    T.install_std_stubs(emu)
    emu.uc.mem_map(RB, REGION)
    emu.write(G_WORLD, struct.pack(">I", RB + WORLD))
    emu.write(G_SETS, struct.pack(">%dI" % NSET, *[RB + SETS + SET_SIZE * k for k in range(NSET)]))
    bind(runner.lib)
    runner.lib.tpw_oad_init()


def bind(lib):
    lib.tpw_oad_base.restype = C.c_uint64
    lib.tpw_oad_begin.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.c_uint32]
    lib.tpw_oad_rows.argtypes = [C.c_void_p, C.c_int32]
    lib.tpw_oad_ratio.restype = C.c_uint32
    lib.tpw_oad_trace_n.restype = C.c_uint32
    lib.tpw_oad_trace_tags.restype = C.c_void_p
    lib.tpw_oad_trace_vals.restype = C.c_void_p
    lib.tpw_oad_layout.argtypes = [C.c_void_p]
    lib.tpw_oad_layout.restype = C.c_int32
    for name in ("admit", "ready", "left"):
        getattr(lib, "tpw_oad_" + name).argtypes = [C.c_uint32]
    lib.tpw_oad_first.argtypes = [C.c_int32]
    for name in ("has_free", "too_long", "admit", "finished", "ready"):
        getattr(lib, "tpw_oad_" + name).restype = C.c_int32
    lib.tpw_oad_ideal.restype = C.c_uint32
    out = (C.c_int64 * 32)()
    n = lib.tpw_oad_layout(out)
    got = [out[i] for i in range(n)]
    assert got == HOST_LAYOUT, [hex(v) for v in got]


def ppc_trace(e):
    import array
    n = e.u32(RB)
    raw = e.read(RB + TRACE, 8 * min(n, CAP))
    a = array.array("I")
    a.frombytes(raw)
    a.byteswap()
    return n, a[0::2].tobytes(), a[1::2].tobytes()


def host_trace(lib):
    n = lib.tpw_oad_trace_n()
    k = min(n, CAP)
    return n, C.string_at(lib.tpw_oad_trace_tags(), 4 * k), C.string_at(lib.tpw_oad_trace_vals(), 4 * k)


def hword(v):
    return (v & 0xFFFF) << 16


def make_test(title, addr, ppc_args, host_call, rnd, edges, kind="s32"):
    """kind: s32 (the answer is r3 as a signed word), u32 (as an unsigned word), none (a function without an answer)."""
    def ppc(e, c):
        side = PpcSide(e)
        e.write(RB, struct.pack(">I", 0))
        e.write(G_RATIO, struct.pack(">I", c["ratio"] & MASK))
        build(side, c)
        rows = bytearray(NROWS * 0x14)
        for row, slot in enumerate(c["rows"]):
            struct.pack_into(">I", rows, 0x14 * row, RB + PERS + PSIZE * slot if slot >= 0 else 0)
        e.write(TABLE, bytes(rows))
        S = stubs_for(e, c)
        saved = S.activate(e)
        try:
            r3, _, _ = T.call_ppc(e, addr, ppc_args(c))
        finally:
            CL.Stubs.deactivate(e, saved)
        ret = T.s32(r3) if kind == "s32" else (r3 & MASK if kind == "u32" else 0)
        n, tags, vals = ppc_trace(e)
        return observe(side, c, ret, e.u32(G_RATIO), n, tags, vals)

    def host(lib, c):
        side = HostSide(lib)
        knobs = (C.c_int32 * 32)(*[T.s32(v) for v in c["vars"] + [c["count"], c["coaster"], c["bmp"], c["fly"], c["tour"]]])
        ready = (C.c_int32 * NSLOT)(*[T.s32(v) for v in c["ready"]])
        exits = (C.c_int32 * NSLOT)(*[T.s32(v) for v in c["exit"]])
        lib.tpw_oad_begin(knobs, ready, exits, c["ratio"] & MASK)
        build(side, c)
        rows = (C.c_int32 * NROWS)(*c["rows"])
        lib.tpw_oad_rows(rows, NROWS)
        ret = host_call(lib, c)
        ret = T.s32(ret) if kind == "s32" else (ret & MASK if kind == "u32" else 0)
        n, tags, vals = host_trace(lib)
        return observe(side, c, ret, lib.tpw_oad_ratio(), n, tags, vals)

    return FnTest(title, ppc, host, edge=edges, rnd=rnd)


# ------------------------------------------------------------------ case generators


def junk(r):
    return r.choice(WORDS) if r.random() < 0.5 else r.getrandbits(32)


def new_case(r):
    """A ride with random fields, answers and record; a function's generator sets the ones it reads."""
    return dict(
        vars=[junk(r) for _ in range(16)], count=junk(r), coaster=r.choice(ANSWERS), bmp=r.choice(ANSWERS), fly=junk(r), tour=r.choice(ANSWERS),
        ready=[r.choice(ANSWERS) for _ in range(NSLOT)], exit=[r.choice(ANSWERS) for _ in range(NSLOT)],
        ratio=r.getrandbits(32), turn=r.getrandbits(32), arg=r.getrandbits(16), g=r.getrandbits(16), on=junk(r), objfill=r.randbytes(OBJ_SIZE),
        obj=dict(tid=r.getrandbits(16), type=r.getrandbits(16), f1c=r.randrange(NSET), f20=r.getrandbits(32), f24=junk(r), flags=r.getrandbits(16),
                 queued=r.getrandbits(16), f3c=junk(r), f44=fb(r.uniform(-300.0, 300.0)), f4c=r.randrange(4), f54=junk(r), f58=r.getrandbits(8),
                 f59=r.getrandbits(8), f64=r.choice(OPEN_WORDS), loaded=r.getrandbits(16), state=r.choice(OBJ_STATES)),
        rec=[r.getrandbits(32) for _ in range(NREC)], rechi=[r.getrandbits(32) for _ in range(NREC)], slots={}, rows=[-1] * NROWS)


def add_guest(r, c, pid, state=None):
    """The guest of thing id pid: a block in a free slot (his state word as given) that the thing table's row pid points at."""
    if c["rows"][pid] < 0:
        slot = r.choice([s for s in range(NSLOT) if s not in c["slots"]])
        c["slots"][slot] = dict(fill=r.randbytes(PSIZE), tid=pid if r.random() < 0.85 else r.getrandbits(16), state=0)
        c["rows"][pid] = slot
    guest = c["slots"][c["rows"][pid]]
    guest["state"] = r.choice(GUEST_STATES) if state is None else state
    return guest


def f32(x):
    try:
        return struct.unpack(">f", struct.pack(">f", x))[0]
    except OverflowError:
        return math.copysign(math.inf, x)


def fdiv(a, b):
    if b == 0.0:
        if a == 0.0 or a != a:
            return math.nan
        return math.copysign(math.inf, a) * math.copysign(1.0, b)
    return a / b


def ideal_of(c):
    """The ideal queue length worked out in Python, only to put a count next to it (a case compares the original's answer, not this one)."""
    o = c["obj"]
    if not o["flags"] & 8:
        return 100
    level = o["f4c"] & 3
    ratio = 1.0
    if o["f54"]:
        ratio = f32(fdiv(f32(float(o["f54"] & MASK)), f32(float(T.s32(c["rec"][INIT_SPEED + 16 * level])))))
    length = f32(fdiv(f32(T.bits_f32(c["rec"][WAIT_CONSTANT + 16 * level]) * f32(ratio * float(o["f59"] & 0xFF))), float(o["f58"] & 0xFF)))
    value = length if length > 4.0 else 4.0
    return MASK if value >= 4294967296.0 else int(value)


def set_level(c, level, speed, constant):
    c["obj"]["f4c"] = level
    c["rec"][INIT_SPEED + 16 * level] = speed & MASK
    c["rec"][WAIT_CONSTANT + 16 * level] = constant & MASK


# ---- HasFreeQueueSpace


def free_case(r, f3c=None, delta=None):
    c = new_case(r)
    f3c = r.choice(F3C) if f3c is None else f3c
    c["obj"]["f3c"] = f3c
    c["count"] = (f3c * 4 + (r.choice((-1, 0, 1)) if delta is None else delta)) & MASK
    if delta is None and r.random() < 0.15:
        c["count"] = junk(r)
    return c


def free_edges():
    out = [free_case(random.Random(100 + i), f3c, delta) for i, (f3c, delta) in enumerate(itertools.product(F3C, (-1, 0, 1)))]
    for i, (f3c, count) in enumerate(itertools.product(F3C, (0, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, 0x40000000))):
        c = free_case(random.Random(150 + i), f3c, 0)
        c["count"] = count
        out.append(c)
    return out


# ---- GetIdealQueueLength and QueueTooLong


def queue_case(r, queue=None):
    c = new_case(r)
    o = c["obj"]
    has_queue = r.random() < 0.85 if queue is None else queue
    o["flags"] = (o["flags"] & ~8) | (8 if has_queue else 0)
    o["f54"] = r.choice(SPEEDS) if r.random() < 0.9 else r.getrandbits(32)
    o["f58"] = r.choice(BYTES) if r.random() < 0.9 else r.getrandbits(8)
    o["f59"] = r.choice(BYTES) if r.random() < 0.9 else r.getrandbits(8)
    set_level(c, r.randrange(4), r.choice(INITS), r.choice(CONSTANTS))
    return c


def queue_edges(count_near):
    out = []
    i = 0
    for queue in (False, True):
        for level in range(4):
            for speed in (0, 100):
                c = queue_case(random.Random(200 + i), queue)
                set_level(c, level, 50, fb(2.0))
                c["obj"]["f54"] = speed
                c["obj"]["f58"] = 10
                c["obj"]["f59"] = 20
                out.append(c)
                i += 1
    for constant in CONSTANTS:
        c = queue_case(random.Random(200 + i), True)
        c["obj"]["f54"] = 0
        c["obj"]["f58"], c["obj"]["f59"] = 4, 6
        set_level(c, i % 4, 100, constant)
        out.append(c)
        i += 1
    for duration, capacity in ((0, 5), (5, 0), (0, 0), (1, 1), (255, 255), (1, 255), (255, 1)):
        c = queue_case(random.Random(200 + i), True)
        c["obj"]["f54"] = 0
        c["obj"]["f58"], c["obj"]["f59"] = duration, capacity
        set_level(c, i % 4, 100, fb(3.0))
        out.append(c)
        i += 1
    for speed, initial in itertools.product((0x80000000, 0xC0000000, 0xFFFFFFFF, 0x7FFFFFFF, 3000000000, 16777217, 33554435),
                                            (1, -1, 1 << 30, 0x7FFFFFFF, -0x80000000, 16777217, 0)):
        c = queue_case(random.Random(200 + i), True)
        c["obj"]["f54"] = speed
        c["obj"]["f58"], c["obj"]["f59"] = 7, 9
        set_level(c, i % 4, initial, fb(1.5))
        out.append(c)
        i += 1
    for k, (speed, initial, constant, capacity, duration) in enumerate(SENSITIVE):
        for delta in (-1, 0, 1) if count_near else (0,):
            c = queue_case(random.Random(250 + k), True)
            c["obj"]["f54"], c["obj"]["f58"], c["obj"]["f59"] = speed, duration, capacity
            set_level(c, k % 4, initial, constant)
            c["delta"] = delta
            out.append(c)
    if count_near:
        for k, c in enumerate(out):
            c["count"] = (ideal_of(c) + c.get("delta", (-1, 0, 1)[k % 3])) & MASK
        for count, queue in itertools.product((0, 1, 99, 100, 101, -1, 0x7FFFFFFF, 0x80000000), (False, True)):
            c = queue_case(random.Random(280 + len(out)), queue)
            c["count"] = count & MASK
            out.append(c)
    return out


def long_case(r, queue=None):
    c = queue_case(r, queue)
    c["count"] = (ideal_of(c) + r.choice((-1, 0, 1, 0, -1, 1, 2, -2))) & MASK
    if r.random() < 0.1:
        c["count"] = junk(r)
    return c


# ---- AdmitPerson, FinishedAdmitting, ReadyToAdmit, PersonLeftDuringAdmittance


def admit_case(r, state=None, running=None, same=None, held=None):
    c = new_case(r)
    o, person = c["obj"], c["arg"]
    o["state"] = r.choice(OBJ_STATES) if state is None else state
    o["f64"] = r.choice((0,) + OPEN_WORDS) if running is None else (r.choice(OPEN_WORDS) if running else 0)
    same = r.random() < 0.6 if same is None else same
    o["loaded"] = person if same else (person ^ r.choice((1, 0x8000, r.getrandbits(16) or 1))) & 0xFFFF
    held = r.choice(("zero", "zero", "person", "wide", "other")) if held is None else held
    c["vars"][0] = {"zero": 0, "person": person, "wide": person | 0x10000 * r.randrange(1, 0x10000), "other": junk(r) | 1}[held]
    return c


def admit_edges():
    out = []
    for i, (state, running, same, held) in enumerate(itertools.product((0, 1, 2, 3, 4, 5), (True, False), (True, False), ("zero", "person"))):
        out.append(admit_case(random.Random(300 + i), state, running, same, held))
    for i, state in enumerate(OBJ_STATES):
        out.append(admit_case(random.Random(400 + i), state, True, True, "zero"))
    for i, running in enumerate(OPEN_WORDS):
        c = admit_case(random.Random(500 + i), 0, True, True, "zero")
        c["obj"]["f64"] = running
        out.append(c)
    return out


def finished_case(r, held=None):
    c = new_case(r)
    queued = c["obj"]["queued"]
    held = r.choice(("zero", "same", "same", "wide", "next", "junk")) if held is None else held
    c["vars"][0] = {"zero": 0, "same": queued, "wide": queued | 0x10000 * r.randrange(1, 0x10000), "next": (queued + 1) & MASK, "junk": junk(r)}[held]
    if r.random() < 0.1:
        c["obj"]["queued"] = 0
    return c


def finished_edges():
    out = []
    for i, (held, queued) in enumerate(itertools.product(("zero", "same", "wide", "next", "junk"), (0, 1, 0x8000, 0xFFFF, 1234))):
        c = finished_case(random.Random(600 + i), held)
        c["obj"]["queued"] = queued
        c["vars"][0] = {"zero": 0, "same": queued, "wide": queued | 0x10000, "next": (queued + 1) & MASK, "junk": 0xFFFFFFFF}[held]
        out.append(c)
    return out


def ready_case(r, same=None):
    c = new_case(r)
    same = r.random() < 0.5 if same is None else same
    c["obj"]["loaded"] = c["arg"] if same else (c["arg"] ^ r.choice((1, 0x8000, 0x100, r.getrandbits(16) or 1))) & 0xFFFF
    return c


def ready_edges():
    out = []
    for i, (person, loaded) in enumerate(((0, 0), (0, 1), (1, 0), (1, 1), (0xFFFF, 0xFFFF), (0x8000, 0x8000), (0x8000, 0), (0xFFFF, 0x7FFF), (0x1234, 0x1234),
                                          (0x1234, 0x1235), (0x1234, 0x0234))):
        c = ready_case(random.Random(700 + i))
        c["arg"] = person
        c["obj"]["loaded"] = loaded
        out.append(c)
    return out


def left_case(r, same=None, held=None):
    c = admit_case(r, 0, True, same, held)
    return c


def left_edges():
    out = []
    for i, (same, held) in enumerate(itertools.product((True, False), ("zero", "person", "wide", "other"))):
        out.append(left_case(random.Random(800 + i), same, held))
    for i, person in enumerate((0, 1, 0x8000, 0xFFFF)):
        c = left_case(random.Random(850 + i), True, "person")
        c["arg"] = person
        c["obj"]["loaded"] = person
        c["vars"][0] = person
        out.append(c)
    return out


# ---- LetPeopleOn, LetPeopleOff, ProcessStateFunctioning

HELD = ("zero", "same", "other", "wide")
QUEUED = ("none", "some", "some")
LOADED = ("none", "loading", "other")
SPACES = {
    "room": dict(v2=5, v5=2, v9=0, coaster=0, track=0, f100=0),
    "full coaster": dict(v2=5, v5=5, v9=0, coaster=1, track=0, f100=0),
    "full track": dict(v2=5, v5=7, v9=0, coaster=0, track=2, f100=0),
    "full other": dict(v2=5, v5=5, v9=0, coaster=0, track=3, f100=0),
    "continuous": dict(v2=5, v5=2, v9=1, coaster=0, track=0, f100=1),
    "interval": dict(v2=5, v5=2, v9=1, coaster=0, track=0, f100=0),
    "zero capacity": dict(v2=0, v5=-1, v9=0, coaster=0, track=0, f100=0),
    "negative capacity": dict(v2=-3, v5=-2, v9=0, coaster=0, track=2, f100=0),
    "signed full": dict(v2=-0x80000000, v5=0x7FFFFFFF, v9=0, coaster=0, track=3, f100=0),
    "signed room": dict(v2=0x7FFFFFFF, v5=-0x80000000, v9=0, coaster=0, track=3, f100=0),
    "continuous wide": dict(v2=5, v5=2, v9=0x10000, coaster=0, track=0, f100=1),
    "interval wide": dict(v2=5, v5=2, v9=0x10000, coaster=0, track=0, f100=0),
    "full coaster wide": dict(v2=5, v5=5, v9=0, coaster=0x100, track=3, f100=0),
    "full track wide": dict(v2=5, v5=5, v9=0, coaster=0, track=0x10002, f100=0),
}


def random_space(r):
    capacity = r.choice((5, 5, 5, 0, -3, 1, junk(r)))
    return dict(v2=capacity, v5=r.choice((capacity - 1, capacity, capacity + 1, 0, -1, junk(r))), v9=r.choice((0, 0, 1, junk(r))), coaster=r.choice(ANSWERS),
                track=r.choice((2, 2, 0, 1, 3, 0x20000, junk(r))), f100=r.getrandbits(1))


def on_case(r, closed=None, held=None, queued=None, loaded=None, space=None, ready=None):
    """LetPeopleOn. closed: mF64 is 0. held: the script's variable 0 (zero, the queue's head, someone else, the head with high bits). queued: the queue's head
    (none or a guest). loaded: the person being loaded (none, a guest in the state of being loaded, a guest in another state). space: variables 2, 5 and 9, the
    coaster answer, the record's track type and the continuous flag."""
    c = new_case(r)
    o, v = c["obj"], c["vars"]
    head, other, third = r.sample(range(1, NROWS), 3)
    closed = r.random() < 0.3 if closed is None else closed
    held = r.choice(HELD) if held is None else held
    queued = r.choice(QUEUED) if queued is None else queued
    loaded = r.choice(LOADED) if loaded is None else loaded
    space = random_space(r) if space is None else (SPACES[space] if isinstance(space, str) else space)
    o["f64"] = 0 if closed else r.choice(OPEN_WORDS)
    o["queued"] = head if queued == "some" else 0
    v[0] = {"zero": 0, "same": o["queued"], "other": other, "wide": o["queued"] | 0x10000 * r.randrange(1, 0x10000)}[held]
    v[2], v[5], v[9] = space["v2"] & MASK, space["v5"] & MASK, space["v9"] & MASK
    c["coaster"] = space["coaster"]
    c["rec"][TRACK_TYPE] = space["track"] & MASK
    o["flags"] = (o["flags"] & ~0x100) | (0x100 if space["f100"] else 0)
    if o["queued"]:
        add_guest(r, c, o["queued"], r.choice((0xE, 0xE, 0x10, 0xD, 0, 0x1000E)) if closed else None)
        c["ready"][c["rows"][o["queued"]]] = r.choice(ANSWERS) if ready is None else ready
    if loaded == "none":
        o["loaded"] = 0
    else:
        o["loaded"] = head if (queued == "some" and r.random() < 0.3) else third
        add_guest(r, c, o["loaded"], 0xD if loaded == "loading" else r.choice([s for s in GUEST_STATES if s != 0xD]))
    if closed and o["queued"] == 0 and v[0] != 0:
        add_guest(r, c, 0)   # the thing table's row 0 holds a guest, so the fetch of the head (none) finds one
    if r.random() < 0.06:
        add_guest(r, c, 0)   # the row-0 assert fails
    return c


def on_edges():
    out = []
    i = 0
    for held, queued, loaded in itertools.product(HELD, ("none", "some"), LOADED):
        out.append(on_case(random.Random(1000 + i), True, held, queued, loaded, "room"))
        i += 1
    for held, queued, loaded, space in itertools.product(HELD, ("none", "some"), LOADED, SPACES):
        out.append(on_case(random.Random(1000 + i), False, held, queued, loaded, space, (0, 1, 0x100)[i % 3]))
        i += 1
    for space, ready in itertools.product(SPACES, (0, 1, 2, 0x100, 0x10000, 0x80000000, 0xFFFFFFFF)):
        out.append(on_case(random.Random(1000 + i), False, "zero", "some", "none", space, ready))
        i += 1
    return out


def off_case(r, var1=None, state=None, answer=None):
    c = new_case(r)
    pid = r.randrange(1, NROWS)
    var1 = r.choice(("id", "id", "zero", "low zero", "wide")) if var1 is None else var1
    c["vars"][1] = {"id": pid, "zero": 0, "low zero": 0x10000 * r.randrange(1, 0x10000), "wide": pid | 0x10000 * r.randrange(1, 0x10000)}[var1]
    guest = add_guest(r, c, pid, 0x10 if r.random() < 0.7 else None)
    if state is not None:
        guest["state"] = state
    c["exit"][c["rows"][pid]] = r.choice(ANSWERS) if answer is None else answer
    if r.random() < 0.06:
        add_guest(r, c, 0)
    return c


def off_edges():
    out = []
    for i, (var1, state, answer) in enumerate(itertools.product(("id", "zero", "low zero", "wide"), (0x10, 0xD, 0x10010, 0), (0, 1, 0x100, 0x10000, 0x80000000, 0xFFFFFFFF))):
        out.append(off_case(random.Random(1500 + i), var1, state, answer))
    return out


def functioning_case(r, v7=None, life=None):
    c = on_case(r)
    v = c["vars"]
    v[7] = r.choice((0, 0, 1, 2, 0x100, 0x80000000, junk(r))) if v7 is None else v7
    c["obj"]["f44"] = r.choice(LIFE) if life is None and r.random() < 0.8 else (fb(r.uniform(-300.0, 300.0)) if life is None else life)
    pid = r.randrange(1, NROWS)
    v[1] = r.choice((pid, pid, 0, 0x10000 * r.randrange(1, 0x10000), pid | 0x10000 * r.randrange(1, 0x10000)))
    if v[1] & 0xFFFF:
        add_guest(r, c, v[1] & 0xFFFF, 0x10 if r.random() < 0.7 else None)
        c["exit"][c["rows"][v[1] & 0xFFFF]] = r.choice(ANSWERS)
    return c


def functioning_edges():
    return [functioning_case(random.Random(1700 + i), v7, life) for i, (v7, life) in enumerate(itertools.product((0, 1), LIFE))]


# ---- GoFirstPerson


def first_case(r, f24=None, bmp=None, tour=None, on=None):
    c = new_case(r)
    c["obj"]["f1c"] = r.getrandbits(32)
    c["obj"]["f24"] = r.choice((0, 0, 1, -1, junk(r))) if f24 is None else f24
    c["bmp"] = r.choice(ANSWERS) if bmp is None else bmp
    c["tour"] = r.choice(ANSWERS) if tour is None else tour
    c["on"] = r.choice((0, 1, -1, 2, 0x7FFFFFFF, junk(r))) if on is None else on
    return c


def first_edges():
    return [first_case(random.Random(1900 + i), f24, bmp, tour, on)
            for i, (f24, bmp, tour, on) in enumerate(itertools.product((0, 1, -1), (0, 1, 0x100, 0x80000000), (0, 1, 0x10000), (0, 1)))]


# ------------------------------------------------------------------ the tests


def arg_hw(c):
    return hword(c["arg"]) | c["g"]


TESTS = [
    make_test("CObject::HasFreeQueueSpace", HAS_FREE, lambda c: [RB + OBJ], lambda lib, c: lib.tpw_oad_has_free(), free_case, free_edges),
    make_test("CObject::GetIdealQueueLength", IDEAL, lambda c: [RB + OBJ], lambda lib, c: lib.tpw_oad_ideal(), queue_case, lambda: queue_edges(False),
              kind="u32"),
    make_test("CObject::QueueTooLong", TOO_LONG, lambda c: [RB + OBJ], lambda lib, c: lib.tpw_oad_too_long(), long_case, lambda: queue_edges(True)),
    make_test("CObject::AdmitPerson", ADMIT, lambda c: [RB + OBJ, arg_hw(c)], lambda lib, c: lib.tpw_oad_admit(c["arg"]), admit_case, admit_edges),
    make_test("CObject::FinishedAdmitting", FINISHED, lambda c: [RB + OBJ], lambda lib, c: lib.tpw_oad_finished(), finished_case, finished_edges),
    make_test("CObject::ReadyToAdmit", READY, lambda c: [RB + OBJ, arg_hw(c)], lambda lib, c: lib.tpw_oad_ready(c["arg"]), ready_case, ready_edges),
    make_test("CObject::PersonLeftDuringAdmittance", LEFT, lambda c: [RB + OBJ, arg_hw(c)], lambda lib, c: lib.tpw_oad_left(c["arg"]), left_case, left_edges,
              kind="none"),
    make_test("CObject::LetPeopleOn", ON, lambda c: [RB + OBJ], lambda lib, c: lib.tpw_oad_on(), on_case, on_edges, kind="none"),
    make_test("CObject::LetPeopleOff", OFF, lambda c: [RB + OBJ], lambda lib, c: lib.tpw_oad_off(), off_case, off_edges, kind="none"),
    make_test("CObject::ProcessStateFunctioning", FUNCTIONING, lambda c: [RB + OBJ], lambda lib, c: lib.tpw_oad_functioning(), functioning_case,
              functioning_edges, kind="none"),
    make_test("CObject::GoFirstPerson", FIRST, lambda c: [RB + OBJ, c["on"] & MASK], lambda lib, c: lib.tpw_oad_first(T.s32(c["on"])), first_case,
              first_edges, kind="none"),
]
