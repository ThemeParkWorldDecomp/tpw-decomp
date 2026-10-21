"""Differential tests for unit w153, the analyser's ride superlatives (src/game/park/analyser.cpp). See difftest.py and tpwtest.py.

CParkAnalyser::GetMostExcitingGokart, GetGokartWithMostCrossRoads, GetLongestTrackRide (both overloads), GetCoasterWithMostLoops, GetHighestCoaster,
GetAverageToiletQueueLenght, FindRideWithUnconnectedQueue, GetAverageExcitementLevelOfAllRides, GetScariestRide and GetRidesInPark. Every callee is a fake
that records the same event on both sides (GAME_ASSERTMSG, the world accessor and its used-thing walk, GetRideInfo, GetDBInfo, the CObject queries, the track
and coaster statistics, the cell system's current object, the analyser value's Get and Set); the answers come from queues, in the order of the calls (a
case fills every queue; a function uses the ones it asks). The thing table, the things, the world's turn word and the analyser values are built from one
description on both sides. CTimeSig::SecondsSinceLastUpdate (0x1015f124) and CodeWarrior's double to unsigned helper (0x10237cd8) run as the original on the
PowerPC side. A case compares the answer, the result halfword with its guard bytes, the whole analyser block with its guards and the call log.
Not tested: a NULL ride record or thing (the Mac would crash), a walk longer than the queues.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ParkRides"

MASK = 0xFFFFFFFF
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF
TABLE, G_WORLD, G_RIDEDB = 0x1034CBFC, 0x1037EC0C, 0x1037F6C8
ASSERT, GETWORLD, HEAD, NEXT = 0x1000DDE8, 0x1014205C, 0x1013ADB0, 0x1013AE08
GETRIDEINFO, GETDBINFO, EXCITEMENT, ISCOASTER, ISTRACK, BOQ, QUEUE = 0x10154868, 0x10107CD4, 0x1010CFB0, 0x10110CD8, 0x10110D50, 0x10109764, 0x10108EF4
SECTIONS, STATS, CURRENT, VALGET, VALSET, SECONDS = 0x10026F20, 0x1004D970, 0x100888FC, 0x100EF3C8, 0x100EF400, 0x1015F124
EXCITING, CROSSROADS, LONGEST_T, LONGEST_K, LOOPS, HIGHEST = 0x100EC484, 0x100EC668, 0x100EC858, 0x100ECA98, 0x100ECC80, 0x100ECE94
TOILET, UNCONNECTED, AVERAGE, SCARIEST, RIDES = 0x100ED0C0, 0x100ED240, 0x100ED3A0, 0x100ED57C, 0x100E558C
OWN = {EXCITING, CROSSROADS, LONGEST_T, LONGEST_K, LOOPS, HIGHEST, TOILET, UNCONNECTED, AVERAGE, SCARIEST, RIDES, SECONDS}
TAIL0 = 0x1DA6F0
ANALYSER_SIZE = 0x21C48
GUARD = 0x40
VAL_A, VAL_B = 0x1F0C4, 0x1F0F4
L_ANALYSER, L_VALA, L_WORLD, L_VALB, L_THING = 1, 2, 3, 4, 10
SLOTS = 8
MAXID = 15
ASSERT_TEXT = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"
RESULT_SIZE = 0x30
OBJECT_SIZE = 0x448


def s32(v):
    return T.s32(v)


def u16(v):
    return v & 0xFFFF


def s64(v):
    v &= (1 << 64) - 1
    return v - (1 << 64) if v & (1 << 63) else v


def setup(runner):
    T.install_std_stubs(runner.emu)


def lib_addr(lib, which):
    lib.tpw_pg_addr.restype = C.c_void_p
    lib.tpw_pg_addr.argtypes = [C.c_int32]
    return lib.tpw_pg_addr(which)


def fbits(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


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


# ------------------------------------------------------------------ the world of a case (both sides built from the description)


def queues_of(c):
    q = {k: list(v) for k, v in c["q"].items()}
    q["head"] = [c["head"]]
    q["next"] = list(c["nxt"])
    return {k: [s64(v) for v in vs] for k, vs in q.items()}


def stubs_for(e, L, world, recs):
    """The PowerPC fakes."""
    recA, recB, _things = recs

    def assertion(emu):
        L.rec("GAME_ASSERTMSG", [s32(emu.r(3))], [emu.cstr(emu.r(4))])

    def getworld(emu):
        L.rec("GetWorld", [L.lab(emu.r(3))])
        emu.set_r(3, world)

    def head(emu):
        L.rec("GetUsedThingHead", [L.lab(emu.r(4))])
        e.write(emu.r(3), struct.pack(">H", u16(L.ret("head", 0))))

    def nxt(emu):
        L.rec("GetNextUsedThing", [L.lab(emu.r(4)), L.lab(emu.r(5))])
        e.write(emu.r(3), struct.pack(">H", u16(L.ret("next", 0))))

    def rideinfo(emu):
        L.rec("GetRideInfo", [(emu.r(4) >> 16) & 0xFFFF])
        e.write(recA + 0x7A8, struct.pack(">I", T.u32(L.ret("ui", 0))))
        emu.set_r(3, recA)

    def dbinfo(emu):
        L.rec("GetDBInfo", [L.lab(emu.r(3))])
        e.write(recB + 0x9C, struct.pack(">I", T.u32(L.ret("track", 0))))
        emu.set_r(3, recB)

    def excitement(emu):
        L.rec("GetExcitementLevel", [L.lab(emu.r(3)), emu.r(4) & 0xFF])
        emu.set_r(3, T.u32(L.ret("exc", 0)))

    def simple(name, key):
        def fn(emu):
            L.rec(name, [L.lab(emu.r(3))])
            emu.set_r(3, T.u32(L.ret(key, 0)))
        return fn

    def sections(emu):
        mask = sum(1 << i for i in range(4) if emu.r(4 + i) != 0)
        L.rec("BMP_CountTrackSections", [s32(emu.r(3)), mask])
        for i in range(4):
            if emu.r(4 + i) != 0:
                e.write(emu.r(4 + i), struct.pack(">I", T.u32(L.ret("cs", 0))))
        emu.set_r(3, T.u32(L.ret("count", 0)))

    def stats(emu):
        L.rec("GetCoasterTrackStats", [L.lab(emu.r(3))])
        out = [emu.r(4), emu.r(5), emu.r(6), emu.r(7)]
        for ptr, key in zip(out, ("fa", "fh", "fc", "loops")):
            e.write(ptr, struct.pack(">I", T.u32(L.ret(key, 0))))

    def current(emu):
        L.rec("LLCM_GetCurrentObject")
        slot = L.ret("cur", 0)
        emu.set_r(3, 0 if slot == 0 else things_of[slot - 1])

    def valget(emu):
        L.rec("ValGet", [L.lab(emu.r(3))])
        emu.set_r(3, T.u32(L.ret("get", 0)))

    def valset(emu):
        L.rec("ValSet", [L.lab(emu.r(3)), s32(emu.r(4))])

    things_of = recs[2]
    S = CL.Stubs()
    S.real = OWN
    for addr, fn in ((ASSERT, assertion), (GETWORLD, getworld), (HEAD, head), (NEXT, nxt), (GETRIDEINFO, rideinfo), (GETDBINFO, dbinfo),
                     (EXCITEMENT, excitement), (ISCOASTER, simple("IsCoaster", "coaster")), (ISTRACK, simple("IsTrackRide", "trackride")),
                     (BOQ, simple("IsBOQConnected", "boq")), (QUEUE, simple("GetNumPeopleInQueue", "queue")), (SECTIONS, sections),
                     (STATS, stats), (CURRENT, current), (VALGET, valget), (VALSET, valset)):
        S.internal(addr, fn)
    return S


def build_ppc(e, c):
    """The PowerPC memory of a case; returns (log, stubs, addresses)."""
    L = CL.PpcLog({}, queues_of(c))
    e.cl = L
    block = e.alloc(ANALYSER_SIZE + 2 * GUARD, b"\xa5" * (ANALYSER_SIZE + 2 * GUARD))
    analyser = block + GUARD
    tail = e.alloc(0x80, b"\xa5" * 0x80)
    world = tail - TAIL0
    result = e.alloc(RESULT_SIZE, b"\xa5" * RESULT_SIZE)
    things = [e.alloc(OBJECT_SIZE, b"\xa5" * OBJECT_SIZE) for _ in range(SLOTS)]
    recA = e.alloc(0x800, b"\0" * 0x800)
    recB = e.alloc(0x800, b"\0" * 0x800)
    L.labels.update({analyser: L_ANALYSER, analyser + VAL_A: L_VALA, analyser + VAL_B: L_VALB, world: L_WORLD})
    L.labels.update({a: L_THING + i for i, a in enumerate(things)})
    e.write(tail + 0x1C, struct.pack(">I", c["turn"] & MASK))
    for at in (VAL_A, VAL_B):
        e.write(analyser + at, struct.pack(">II", c["stamp"] & MASK, c["val"] & MASK) + bytes([c["worked"] & 0xFF]))
    for i, th in enumerate(c["things"]):
        kind, otype, flags, f24 = th
        e.write(things[i] + 2, bytes([kind & 0xFF]))
        e.write(things[i] + 0xE, struct.pack(">H", otype & 0xFFFF))
        e.write(things[i] + 0x24, struct.pack(">I", f24 & MASK))
        e.write(things[i] + 0x2E, struct.pack(">H", flags & 0xFFFF))
    for row in range(MAXID + 1):
        slot = c["rows"].get(row, 0)
        e.write(TABLE + 0x14 * row, struct.pack(">I", things[slot - 1] if slot else 0))
    e.write(G_WORLD, struct.pack(">I", world))
    return L, stubs_for(e, L, world, (recA, recB, things)), dict(analyser=analyser, block=block, result=result, things=things)


def host_setup(lib, c):
    lib.tpw_pg_reset()
    lib.tpw_pg_turn.argtypes = [C.c_uint32]
    lib.tpw_pg_val.argtypes = [C.c_int32, C.c_uint32, C.c_int32]
    lib.tpw_pg_row.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_pg_thing.argtypes = [C.c_int32] * 5
    lib.tpw_pg_turn(c["turn"] & MASK)
    lib.tpw_pg_val(c["worked"] & 0xFF, c["stamp"] & MASK, c["val"])
    for i, th in enumerate(c["things"]):
        lib.tpw_pg_thing(i, th[0] & 0xFF, th[1] & 0xFFFF, th[2] & 0xFFFF, s32(th[3]))
    for row in range(MAXID + 1):
        lib.tpw_pg_row(row, c["rows"].get(row, 0))
    fixed = {L_ANALYSER: lib_addr(lib, 0), L_VALA: lib_addr(lib, 0) + VAL_A, L_VALB: lib_addr(lib, 0) + VAL_B, L_WORLD: lib_addr(lib, 1)}
    fixed.update({L_THING + i: lib_addr(lib, 10 + i) for i in range(SLOTS)})
    CL.host_begin(lib, fixed, queues_of(c))


def host_blocks(lib):
    return dict(block=C.string_at(lib_addr(lib, 4), ANALYSER_SIZE + 2 * GUARD), result=C.string_at(lib_addr(lib, 3), RESULT_SIZE))


def ppc_blocks(e, a):
    """The PowerPC blocks; the two analyser values' words are byte-swapped to the host's order (the walk never touches other words)."""
    block = bytearray(e.read(a["block"], ANALYSER_SIZE + 2 * GUARD))
    for at in (VAL_A, VAL_B):
        for w in (0, 4):
            o = GUARD + at + w
            block[o:o + 4] = bytes(reversed(block[o:o + 4]))
    result = bytearray(e.read(a["result"], RESULT_SIZE))
    result[0x10:0x12] = bytes(reversed(result[0x10:0x12]))                        # the ThingId halfword
    return dict(block=bytes(block), result=bytes(result))


def make_ppc(entry, args_of, conv):
    """A PowerPC side: calls `entry` with the result block (r4 unless `hidden`) and the case's arguments."""
    def fn(e, c):
        L, S, a = build_ppc(e, c)
        saved = S.activate(e)
        try:
            r3, _, f1 = T.call_ppc(e, entry, args_of(c, a))
        finally:
            CL.Stubs.deactivate(e, saved)
        out = dict(ret=conv(r3, f1) if conv else 0, log=L.log)
        out.update(ppc_blocks(e, a))
        if conv is None:                                                          # a ThingId returned through the hidden pointer
            out["ret"] = struct.unpack(">H", bytes(e.read(a["result"] + 0x10, 2)))[0]
        return out
    return fn


def make_host(name, restype, argtypes, args_of):
    def fn(lib, c):
        host_setup(lib, c)
        f = getattr(lib, name)
        f.restype = restype
        f.argtypes = argtypes
        try:
            ret = f(*args_of(c))
        finally:
            log = CL.host_end(lib)
        out = dict(ret=ret, log=log)
        out.update(host_blocks(lib))
        return out
    return fn


R4 = lambda c, a: [T.I(a["analyser"]), T.I(a["result"] + 0x10)]                    # this, the ThingId reference
TYPE5 = lambda c, a: R4(c, a) + [T.I(u16(c["type"]) << 16)]                          # the ObjectId arrives in the high halfword
TRACK5 = lambda c, a: R4(c, a) + [T.I(c["track"])]
SECS5 = lambda c, a: [T.I(a["analyser"]), T.I(c["seconds"])]
U8 = lambda r3, f1: r3 & 0xFF
S32 = lambda r3, f1: s32(r3)
U32 = lambda r3, f1: T.u32(r3)


def toilet_ret(r3, f1):
    return fbits(f1)


# ------------------------------------------------------------------ the cases

KINDS = [0, 1, 2, 3, 3, 3, 3, 4, 0x13, 0xFF]
EXC = [0, 1, 2, 127, 128, 129, 254, 255]
EXC_GARBAGE = [0, 0, 0, 0x100, 0x5500, 0xFFFFFF00, 0x80000000]
COUNTS = [INT_MIN, -2, -1, 0, 1, 2, 5, 100, INT_MAX - 1, INT_MAX]
LOOPS_V = [0, 1, 2, 0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFE, 0xFFFFFFFF]
HEIGHTS = [0.0, -0.0, 0.5, 1.0, 1.5, 2.0, 100.0, 16777216.0, 16777217.0, 2147483647.0, 2147483648.0, 3e9, 4294967040.0, 4294967295.0, 4294967296.0, 5e9,
           -1.0, -0.5, float("nan"), float("inf"), float("-inf")]
HEIGHT_BITS = [fbits(h) for h in HEIGHTS] + [0x7FC00001, 0xFFC00000, 0x00000001, 0x80000001]
QUEUES = [0, 1, 2, 5, 0x100, 0x10000, 0x80000000, 0xFFFFFFFF, -1, 7]


def pick(r, values):
    return r.choice(values)


def rand_word(r):
    return s32(r.getrandbits(32))


def rand_exc(r):
    v = r.choice(EXC) if r.random() < 0.7 else r.randint(0, 255)
    return v | (r.choice(EXC_GARBAGE) if r.random() < 0.3 else 0)


def rand_queues(r, n=12):
    return {
        "ui": [r.choice([0, 0, 0, 1, 2, 3, 4, -1]) for _ in range(n)],
        "track": [r.choice([-1, 0, 1, 1, 1, 2]) for _ in range(n)],
        "exc": [rand_exc(r) for _ in range(n)],
        "coaster": [r.choice([0, 1, 1, 5, -1, 0x100]) for _ in range(n)],
        "trackride": [r.choice([0, 1, 1, 5, -1, 0x100]) for _ in range(n)],
        "boq": [r.choice([0, 0, 1, 5, -1, 0x100]) for _ in range(n)],
        "queue": [r.choice(QUEUES) if r.random() < 0.6 else rand_word(r) for _ in range(n)],
        "cs": [r.choice(COUNTS) if r.random() < 0.7 else rand_word(r) for _ in range(n)],
        "count": [r.choice(COUNTS) if r.random() < 0.7 else rand_word(r) for _ in range(n)],
        "fa": [r.choice(HEIGHT_BITS) if r.random() < 0.5 else r.getrandbits(32) for _ in range(n)],
        "fh": [r.choice(HEIGHT_BITS) if r.random() < 0.7 else r.getrandbits(32) for _ in range(n)],
        "fc": [r.choice(HEIGHT_BITS) for _ in range(n)],
        "loops": [r.choice(LOOPS_V) if r.random() < 0.7 else r.getrandbits(32) for _ in range(n)],
        "cur": [r.choice([0, 0, 1, 2, 3, 4, 5, 6, 7, 8]) for _ in range(n)],
        "get": [rand_word(r) for _ in range(n)],
    }


def ride_case(r, **kw):
    """A random case: a used list of up to 6 things, per-call answers from queues, the Val and the parameters."""
    n = r.choice([0, 1, 1, 2, 3, 3, 4, 5, 6])
    ids = r.sample(range(1, MAXID + 1), n)
    slots = r.sample(range(1, SLOTS + 1), n)
    rows = {i: s for i, s in zip(ids, slots)}
    if r.random() < 0.1:
        rows[0] = r.randint(1, SLOTS)
    types = [r.choice([1, 2, 3, 0x1234, 0xFFFF]) for _ in range(SLOTS)]
    things = [(r.choice(KINDS), types[i], r.getrandbits(16) if r.random() < 0.4 else r.choice([0xF, 0xC, 0x4, 0x8, 0x1, 0x5, 0xD, 0xE]), rand_word(r))
              for i in range(SLOTS)]
    stamp = r.getrandbits(32)
    turn = (stamp + r.choice([0, 0, 1, 2, 3, 4, 5, 7, 8, 12, 16, 100, 1 << 31, MASK])) & MASK if r.random() < 0.7 else r.getrandbits(32)
    secs = ((turn >> 2) - (stamp >> 2)) & MASK
    c = dict(rows=rows, things=things, head=ids[0] if ids else 0, nxt=ids[1:] + [0], q=rand_queues(r),
             stamp=stamp, turn=turn, worked=r.choice([0, 1, 1, 2, 0x80, 0xFF]) if r.random() < 0.9 else r.randrange(256), val=rand_word(r),
             type=r.choice([0, 0, types[slots[0] - 1] if slots else 0, r.choice(types), 0x7777, r.getrandbits(16)]),
             track=r.choice([-1, 0, 1, 1, 2, INT_MIN, INT_MAX]), any=r.choice([0, 1]),
             seconds=r.choice([0, 1, 2, 3, MASK, (secs - 1) & MASK, secs, (secs + 1) & MASK, r.getrandbits(32)]))
    c.update(kw)
    return c


def chain_case(r, n, **over):
    """n rides in a row (kind 3, ui 0, track 1, a coaster, a track ride, unconnected, all flags), answers overridden by `over`."""
    base = ride_case(r, rows={i: i for i in range(1, n + 1)}, head=1 if n else 0, nxt=list(range(2, n + 1)) + [0],
                     things=[(3, 1, 0xF, 100 * i) for i in range(SLOTS)], type=1, track=1, worked=0, any=0)
    base["q"].update(ui=[0] * 12, track=[1] * 12, coaster=[1] * 12, trackride=[1] * 12, boq=[0] * 12)
    for key, value in over.items():
        if key in base["q"]:
            base["q"][key] = value
        else:
            base[key] = value
    return base


def pad(values):
    return list(values) + [values[-1]] * (12 - len(values))


def edges():
    out = []
    r = random.Random(153)
    # no things at all, row 0 taken, one thing of every kind
    out.append(ride_case(r, rows={}, head=0, nxt=[0]))
    out.append(ride_case(r, rows={0: 3, 1: 1, 2: 2}, head=1, nxt=[2, 0]))
    for kind in (0, 1, 2, 3, 4, 0x13):
        out.append(ride_case(r, rows={1: 1}, head=1, nxt=[0], things=[(kind, 1, 0xF, 5)] * SLOTS))
    # excitement sequences with ties and the byte edges
    for seq in ([0], [0, 0], [1, 0], [0, 1], [127, 128], [128, 127], [255, 0, 255], [5, 5, 5], [0, 255, 0x1FF, 0x100], [0x1FF, 0x100], [255, 254, 255, 1, 0]):
        for n in (1, 2, 3, 4):
            out.append(chain_case(r, n, exc=pad(seq)))
    # counts and sections: signed compares at the edges
    for seq in ([0], [INT_MIN], [INT_MIN, -1], [-1, INT_MIN], [-1, 0], [0, -1], [INT_MAX, INT_MIN], [INT_MIN, INT_MAX], [1, 1], [-5, -5, -4], [INT_MAX, INT_MAX]):
        for n in (1, 2, 3):
            out.append(chain_case(r, n, count=pad(seq), cs=pad(seq)))
    # loops: unsigned compares
    for seq in ([0], [0, 0], [1, 0], [0, 1], [0x7FFFFFFF, 0x80000000], [0x80000000, 0x7FFFFFFF], [0xFFFFFFFF, 0], [0, 0xFFFFFFFF], [0x80000000, 0x80000000], [0xFFFFFFFF, 0xFFFFFFFF]):
        for n in (1, 2, 3):
            out.append(chain_case(r, n, loops=pad(seq)))
    # heights: the float compare and the conversion, in pairs and triples
    for a in HEIGHT_BITS:
        for b in HEIGHT_BITS[::3]:
            out.append(chain_case(r, 2, fh=pad([a, b])))
    for a in HEIGHT_BITS[::2]:
        out.append(chain_case(r, 3, fh=pad([a, a, a])))
        out.append(chain_case(r, 1, fh=pad([a])))
    # the toilet queue: sums that wrap, counts of 0, 1 and several
    for seq in ([0], [1], [INT_MAX, 1], [INT_MIN, -1], [INT_MAX, INT_MAX, INT_MAX], [-1, -1], [16777217, 1], [3, 4, 5], [INT_MIN, INT_MIN], [7, 0, 0]):
        for n in (1, 2, 3, 4):
            out.append(chain_case(r, n, queue=pad(seq)))
    out.append(chain_case(r, 3, things=[(3, 1, 0xE, 0)] * SLOTS))
    # the unconnected queue: every combination of flags, boq answers, any and the current object
    for flags in (0x0, 0x4, 0x8, 0xC, 0xD, 0x1C, 0xFFFF):
        for boq in (0, 1, -1):
            for cur in (0, 1, 2):
                for any in (0, 1):
                    out.append(chain_case(r, 2, things=[(3, 1, flags, 0)] * SLOTS, boq=[boq] * 12, cur=[cur] * 12, any=any))
    # the cached values: worked-out byte and the seconds against the clock
    for worked in (0, 1, 0x80, 0xFF):
        for secs in (0, 1, 2, 3, 100, 0x3FFFFFFF):
            for param in (0, 1, 2, 3, 99, 100, 101, MASK, 0x80000000, 0x7FFFFFFF):
                stamp = r.getrandbits(32) & ~3
                out.append(chain_case(r, 2, worked=worked, stamp=stamp, turn=(stamp + 4 * secs) & MASK, seconds=param))
    out.append(chain_case(r, 2, worked=1, stamp=8, turn=0, seconds=0))       # the clock went backwards: the difference is negative, so unsigned huge
    out.append(chain_case(r, 2, worked=1, stamp=0x80000000, turn=0, seconds=MASK))
    out.append(chain_case(r, 3, worked=0, ui=pad([0, 1, 2, 0, 3, 4]), exc=pad([10, 20, 30, 40, 50, 60])))
    # ride records: every mUIType, track types
    for ui in (0, 1, 2, 3, 4, -1, INT_MIN, INT_MAX):
        out.append(chain_case(r, 3, ui=[ui] * 12))
        out.append(chain_case(r, 3, ui=pad([ui, 0])))
    for track in (-1, 0, 1, 2, INT_MIN, INT_MAX):
        for tr in (-1, 0, 1, 2):
            out.append(chain_case(r, 3, track=pad([track, 1, tr]), **{"track": pad([track, 1, tr])}, ) if False else chain_case(r, 3, track=pad([track, 1, tr])))
    # the type filters: any, a match, a miss
    for ftype in (0, 1, 2, 0x7777, 0xFFFF):
        out.append(chain_case(r, 3, type=ftype, things=[(3, 1, 0xF, 1), (3, 2, 0xF, 2), (3, 1, 0xF, 3)] + [(3, 1, 0xF, 4)] * 5))
    return out


def random_case(r):
    return ride_case(r)


def make(title, ppc, host):
    return Base(title, ppc, host, edge=edges, rnd=random_case)


I32, U32T, U8T = C.c_int32, C.c_uint32, C.c_uint32
TESTS = [
    make("CParkAnalyser::GetMostExcitingGokart", make_ppc(EXCITING, R4, U8), make_host("tpw_pg_exciting", C.c_uint32, [], lambda c: [])),
    make("CParkAnalyser::GetGokartWithMostCrossRoads", make_ppc(CROSSROADS, R4, S32), make_host("tpw_pg_crossroads", I32, [], lambda c: [])),
    make("CParkAnalyser::GetLongestTrackRide (type)", make_ppc(LONGEST_T, TYPE5, S32),
         make_host("tpw_pg_longest_type", I32, [U32T], lambda c: [u16(c["type"])])),
    make("CParkAnalyser::GetLongestTrackRide (track)", make_ppc(LONGEST_K, TRACK5, S32),
         make_host("tpw_pg_longest_track", I32, [I32], lambda c: [c["track"]])),
    make("CParkAnalyser::GetCoasterWithMostLoops", make_ppc(LOOPS, TYPE5, U32),
         make_host("tpw_pg_loops", U32T, [U32T], lambda c: [u16(c["type"])])),
    make("CParkAnalyser::GetHighestCoaster", make_ppc(HIGHEST, TYPE5, U32),
         make_host("tpw_pg_highest", U32T, [U32T], lambda c: [u16(c["type"])])),
    make("CParkAnalyser::GetAverageToiletQueueLenght", make_ppc(TOILET, lambda c, a: [T.I(a["analyser"])], toilet_ret),
         make_host("tpw_pg_toilet", U32T, [], lambda c: [])),
    make("CParkAnalyser::FindRideWithUnconnectedQueue", make_ppc(UNCONNECTED, lambda c, a: [T.I(a["result"] + 0x10), T.I(a["analyser"]), T.I(c["any"])],
                                                                None),
         make_host("tpw_pg_unconnected", U32T, [I32], lambda c: [c["any"]])),
    make("CParkAnalyser::GetAverageExcitementLevelOfAllRides", make_ppc(AVERAGE, SECS5, S32),
         make_host("tpw_pg_average", I32, [U32T], lambda c: [c["seconds"]])),
    make("CParkAnalyser::GetScariestRide", make_ppc(SCARIEST, R4, U8), make_host("tpw_pg_scariest", C.c_uint32, [], lambda c: [])),
    make("CParkAnalyser::GetRidesInPark", make_ppc(RIDES, SECS5, S32), make_host("tpw_pg_rides", I32, [U32T], lambda c: [c["seconds"]])),
]
