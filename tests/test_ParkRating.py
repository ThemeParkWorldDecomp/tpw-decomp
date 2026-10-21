"""Differential tests for unit w159, the park rating, the park value and the attraction points (src/game/park/analyser.cpp). See difftest.py and tpwtest.py.

CParkAnalyser::GetOverallParkRating (0x100ed8cc), GetParkValue (0x100e5110) and GetTotalParkAttractionPoints (0x100e5b1c). The real functions that run besides
them: NumberOfVisitorsInPark, NumberOfObjectTypeInPark, NumberOfObjectInPark, NumberOfStaffType, SecondsSinceLastUpdate and the 64 bit divide helper. Every
other callee is a fake that records the same event on both sides (the world accessor and its used-thing walk, the assert, the analyser value's Get and Set,
GetRideInfo, both GetObjectControl overloads, IsInsidePark, IsCondemned, IsAvailableToChoose, GetObjectAge, GetFunnyTimeSinceGameTurn and the nine map cell
predicates); the answers come from queues. The cell predicates are not logged one by one: they answer from a hash of (seed, kind, place) and fold every call into
one hash that is logged once (before the next event), so a cell walk that asks a predicate in another order, another place or another number of times differs.

Not tested (see the report): a decay time of 0 (a division by zero), an object count of -1 against INT_MIN, a GetRideInfo that answers NULL, the high word of
the 64 bit tick quotient (the ticks stay within a few days of game time), a thing mF4C above 3 in the park value (the ride record has three upgrades), a
cyclic object list.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ParkRating"

MASK = 0xFFFFFFFF
M62 = 0x3FFFFFFFFFFFFFFF
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF
TABLE = 0x1034CBFC
GWORLD = 0x1037EC0C
ASSERT, GETWORLD, HEAD, NEXT, VALGET, VALSET = 0x1000DDE8, 0x1014205C, 0x1013ADB0, 0x1013AE08, 0x100EF3C8, 0x100EF400
RIDEINFO, CTRL, CTRLC, RDB = 0x10154868, 0x100FA0B8, 0x100FA1B8, 0x1037F6C8
ISINSIDE, ISCOND, ISAVAIL, GETAGE, FUNNY = 0x10115A80, 0x1010C8BC, 0x101083E0, 0x10107F50, 0x10112C80
CELLS = [(0x1009BB70, 0), (0x1009C408, 1), (0x1009C44C, 2), (0x1009C490, 3), (0x1009C4DC, 4), (0x1009C524, 5), (0x1009C568, 6), (0x1009C660, 7), (0x1009C96C, 8)]
RATING, VALUE, ATTRACT = 0x100ED8CC, 0x100E5110, 0x100E5B1C
OWN = {RATING, VALUE, ATTRACT, 0x100E7278, 0x100EA5D8, 0x100EA52C, 0x100E8144, 0x1015F124, 0x10237E1C}
TAIL0 = 0x1DA6F0
GUARD = 0x40
ANALYSER_SIZE = 0x21C48 + 2 * GUARD
VAL_AT = 0x1F0E8
SLOTS, RIDES, CONTROLS, MAXID = 8, 5, 3, 15
RIDE_BYTES = 0x4000   # the PowerPC ride block (the host RideInfo has its own layout)
L_ANALYSER, L_VAL, L_WORLD, L_RDB, L_THING, L_RIDE, L_CTL = 1, 2, 3, 5, 10, 20, 40
ASSERT_TEXT = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"
TICK = 864000000000
WALKS = 24


def s32(v):
    return T.s32(v)


def setup(runner):
    T.install_std_stubs(runner.emu)


def lib_addr(lib, which):
    lib.tpw_pq_addr.restype = C.c_void_p
    lib.tpw_pq_addr.argtypes = [C.c_int32]
    return lib.tpw_pq_addr(which)


class Base(FnTest):
    def run_case(self, runner, case):
        runner.emu.reset_scratch()
        try:
            p = self.ppc(runner.emu, case)
        except ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = self.host(runner.lib, case)
        pl, hl = p.pop("log"), h.pop("log")
        difference = CL.first_difference(pl, hl)
        errors = T.same_values(p, h, "", False)
        if difference:
            errors.append(difference)
        return errors


def f32bits(v):
    return struct.unpack(">I", struct.pack(">f", v))[0]


# ------------------------------------------------------------------ the cell predicates (the same hash on both sides)


def cell_answer(state, kind, place, flag, seed, flag_density, density):
    state[0] = ((state[0] ^ (kind | (place << 8) | (flag << 24))) * 0x100000001B3) & M62
    state[1] += 1
    x = (place * 0x9E3779B1 + kind * 0x85EBCA6B + seed * 0xC2B2AE35) & MASK
    x ^= x >> 15
    x = (x * 0x2C1B3C6D) & MASK
    x ^= x >> 12
    d = flag_density if kind == 0 else density
    if (x & 15) >= d:
        return 0
    return (1, 0x80000000, 0xFFFFFFFF, 2)[(x >> 8) & 3]


def stubs_for(e, L, world, ctx):
    rides, controls, analyser = ctx["rides"], ctx["controls"], ctx["analyser"]
    cells = ctx["cells"]
    st = [0xCBF29CE484222325 & M62, 0]

    def rec(name, ints=(), texts=()):
        if st[1] > 0:
            L.rec("CellHash", [st[0], st[1]])
            st[1] = 0
        L.rec(name, ints, texts)
    ctx["flush"] = lambda: rec_flush()

    def rec_flush():
        if st[1] > 0:
            L.rec("CellHash", [st[0], st[1]])
            st[1] = 0

    def ride_of(key):
        v = L.ret(key, 0)
        return rides[v - 1] if 1 <= v <= RIDES else 0

    def assertion(emu):
        rec("GAME_ASSERTMSG", [s32(emu.r(3))], [emu.cstr(emu.r(4))])

    def getworld(emu):
        rec("GetWorld", [L.lab(emu.r(3))])
        emu.set_r(3, world)

    def head(emu):
        rec("GetUsedThingHead", [L.lab(emu.r(4))])
        e.write(emu.r(3), struct.pack(">H", L.ret("head", 0) & 0xFFFF))

    def nxt(emu):
        rec("GetNextUsedThing", [L.lab(emu.r(4)), L.lab(emu.r(5))])
        e.write(emu.r(3), struct.pack(">H", L.ret("next", 0) & 0xFFFF))

    def inside(emu):
        rec("IsInsidePark", [L.lab(emu.r(3))])
        emu.set_r(3, T.u32(L.ret("inside", 0)))

    def valget(emu):
        rec("ValGet", [L.lab(emu.r(3))])
        emu.set_r(3, T.u32(L.ret("get", 0)))

    def valset(emu):
        rec("ValSet", [L.lab(emu.r(3)), s32(emu.r(4))])

    def rideinfo(emu):
        rec("GetRideInfo", [L.lab(emu.r(3)), emu.r(4) >> 16])
        emu.set_r(3, ride_of("rideinfo"))

    def control(emu):
        which = L.ret("ctl", 0) % CONTROLS
        rec("GetObjectControl", [emu.r(3) - world, emu.r(4) >> 16, s32(emu.r(5)), which])
        emu.set_r(3, controls[which])

    def controlc(emu):
        which = L.ret("ctlc", 0) % CONTROLS
        rec("GetObjectControlConst", [emu.r(3) - world, emu.r(4) >> 16, s32(emu.r(5)), which])
        emu.set_r(3, controls[which])

    def cond(emu):
        rec("IsCondemned", [L.lab(emu.r(3))])
        emu.set_r(3, T.u32(L.ret("cond", 0)))

    def avail(emu):
        rec("IsAvailableToChoose", [L.lab(emu.r(3))])
        emu.set_r(3, T.u32(L.ret("avail", 0)))

    def age(emu):
        rec("GetObjectAge", [L.lab(emu.r(3))])
        emu.set_r(3, T.u32(L.ret("age", 0)))

    def funny(emu):
        rec("GetFunnyTimeSinceGameTurn", [emu.r(4) - world, emu.r(5)])
        ticks = L.ret("span", 0) & 0xFFFFFFFFFFFFFFFF
        e.write(emu.r(3), struct.pack(">Q", ticks))

    def cellstub(kind):
        def f(emu):
            place = (emu.r(3) - world - 0x2D8) // 0x44 + 1
            flag = (emu.r(4) & 0xFFFF) if kind == 0 else 0
            emu.set_r(3, cell_answer(st, kind, place, flag, *cells))
        return f

    S = CL.Stubs()
    S.real = OWN
    for addr, fn in ((ASSERT, assertion), (GETWORLD, getworld), (HEAD, head), (NEXT, nxt), (VALGET, valget), (VALSET, valset), (RIDEINFO, rideinfo),
                     (CTRL, control), (CTRLC, controlc), (ISINSIDE, inside), (ISCOND, cond), (ISAVAIL, avail), (GETAGE, age), (FUNNY, funny)):
        S.internal(addr, fn)
    for addr, kind in CELLS:
        S.internal(addr, cellstub(kind))
    return S


def build_ppc(e, c):
    L = CL.PpcLog({}, c["queues"])
    e.cl = L
    block = e.alloc(ANALYSER_SIZE, b"\xa5" * ANALYSER_SIZE)
    analyser = block + GUARD
    tail = e.alloc(0x80, b"\xa5" * 0x80)
    world = tail - TAIL0
    things = [e.alloc(0x448, b"\xa5" * 0x448) for _ in range(SLOTS)]
    rides = [e.alloc(RIDE_BYTES, b"\xa5" * RIDE_BYTES) for _ in range(RIDES)]
    controls = [e.alloc(0x20, b"\xa5" * 0x20) for _ in range(CONTROLS)]
    L.labels.update({analyser: L_ANALYSER, analyser + VAL_AT: L_VAL, world: L_WORLD, RDB: L_RDB})
    L.labels.update({a: L_THING + i for i, a in enumerate(things)})
    L.labels.update({a: L_RIDE + i for i, a in enumerate(rides)})
    L.labels.update({a: L_CTL + i for i, a in enumerate(controls)})
    e.write(GWORLD, struct.pack(">I", world))
    e.write(tail + 0x56, struct.pack(">H", c["objhead"] & 0xFFFF))
    e.write(tail + 0x1C, struct.pack(">I", c["turn"] & MASK))
    e.write(analyser + VAL_AT, struct.pack(">IiB", c["val"][0] & MASK, c["val"][1], c["val"][2]))
    for i, th in enumerate(c["things"]):
        a = things[i]
        e.write(a + 2, bytes([th["kind"] & 0xFF, th["dead"] & 0xFF]))
        e.write(a + 0xC, struct.pack(">HH", th["f0c"] & 0xFFFF, th["otype"] & 0xFFFF))
        e.write(a + 0x40, struct.pack(">I", f32bits(th["f40"])))
        e.write(a + 0x4C, bytes([th["f4c"] & 0xFF]))
    for i, rd in enumerate(c["rides"]):
        for off, v in ((0x7A8, rd["uitype"]), (0x9C, rd["track"]), (0x5C, rd["decay"]), (0x58, rd["value"])):
            e.write(rides[i] + off, struct.pack(">I", v))
        for k in range(3):
            e.write(rides[i] + 0x268 + 4 * k, struct.pack(">I", rd["bonus"][k]))
        for k in range(4):
            e.write(rides[i] + 0x1B8 + 0x40 * k, struct.pack(">I", rd["costs"][k]))
    for i, ct in enumerate(c["controls"]):
        e.write(controls[i] + 0x18, struct.pack(">iI", ct[0], ct[1] & MASK))
    for row in range(MAXID + 1):
        slot = c["rows"].get(row, 0)
        e.write(TABLE + 0x14 * row, struct.pack(">I", things[slot - 1] if slot else 0))
    ctx = dict(rides=rides, controls=controls, analyser=analyser, world=world, things=things, block=block, cells=c["cells"])
    return L, stubs_for(e, L, world, ctx), ctx


def masked(data):
    b = bytearray(data)
    b[GUARD + VAL_AT:GUARD + VAL_AT + 12] = bytes(12)
    return bytes(b)


def run_ppc(e, c):
    L, S, ctx = build_ppc(e, c)
    saved = S.activate(e)
    try:
        r3, _, _ = T.call_ppc(e, (RATING, VALUE, ATTRACT)[c["fn"]], [T.I(ctx["analyser"])] + ([T.I(c["seconds"] & MASK)] if c["fn"] == 1 else []))
    finally:
        CL.Stubs.deactivate(e, saved)
    ctx["flush"]()
    return dict(ret=s32(r3), analyser=masked(e.read(ctx["block"], ANALYSER_SIZE)), log=L.log)


def run_host(lib, c):
    lib.tpw_pq_reset()
    lib.tpw_pq_objhead.argtypes = [C.c_uint32]
    lib.tpw_pq_turn.argtypes = [C.c_uint32]
    lib.tpw_pq_val.argtypes = [C.c_uint32, C.c_int32, C.c_uint32]
    lib.tpw_pq_cells.argtypes = [C.c_uint32] * 3
    lib.tpw_pq_row.argtypes = [C.c_int32] * 2
    lib.tpw_pq_thing.argtypes = [C.c_int32] * 3 + [C.c_uint32] * 4
    lib.tpw_pq_ride.argtypes = [C.c_int32] + [C.c_uint32] * 7 + [C.c_void_p]
    lib.tpw_pq_control.argtypes = [C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_pq_objhead(c["objhead"] & 0xFFFF)
    lib.tpw_pq_turn(c["turn"] & MASK)
    lib.tpw_pq_val(c["val"][0] & MASK, c["val"][1], c["val"][2])
    lib.tpw_pq_cells(*c["cells"])
    for i, th in enumerate(c["things"]):
        lib.tpw_pq_thing(i, th["kind"] & 0xFF, th["dead"] & 0xFF, th["otype"] & 0xFFFF, th["f0c"] & 0xFFFF, f32bits(th["f40"]), th["f4c"] & 0xFF)
    for i, rd in enumerate(c["rides"]):
        costs = (C.c_uint32 * 4)(*rd["costs"])
        lib.tpw_pq_ride(i, rd["uitype"], rd["track"], rd["decay"], rd["value"], rd["bonus"][0], rd["bonus"][1], rd["bonus"][2], costs)
    for i, ct in enumerate(c["controls"]):
        lib.tpw_pq_control(i, ct[0], ct[1] & MASK)
    for row in range(MAXID + 1):
        lib.tpw_pq_row(row, c["rows"].get(row, 0))
    fixed = {L_ANALYSER: lib_addr(lib, 1), L_VAL: lib_addr(lib, 6), L_WORLD: lib_addr(lib, 2), L_RDB: lib_addr(lib, 4)}
    fixed.update({L_THING + i: lib_addr(lib, 10 + i) for i in range(SLOTS)})
    fixed.update({L_RIDE + i: lib_addr(lib, 20 + i) for i in range(RIDES)})
    fixed.update({L_CTL + i: lib_addr(lib, 40 + i) for i in range(CONTROLS)})
    CL.host_begin(lib, fixed, {k: list(v) for k, v in c["queues"].items()})
    lib.tpw_pq_run.restype = C.c_uint32
    lib.tpw_pq_run.argtypes = [C.c_int32, C.c_uint32]
    try:
        ret = lib.tpw_pq_run(c["fn"], c["seconds"] & MASK)
    finally:
        log = CL.host_end(lib)
    dbg = [struct.unpack("<I", C.string_at(lib_addr(lib, 20 + i) + 0x7A8, 4))[0] for i in range(RIDES)]
    return dict(ret=s32(ret), analyser=masked(C.string_at(lib_addr(lib, 0), ANALYSER_SIZE)), log=log)


# ------------------------------------------------------------------ the cases

F40S = [0.0, 0.5, 1.0, 50.0, 99.0, 99.9, 100.0, 100.5, 254.9, 255.0, 255.9, 256.0, 257.0, 300.7, -1.0, -2.5, 1e9, -1e9, 2147483520.0, -2147483648.0, 12345.678]
UITYPES = [0, 0, 1, 2, 3, 4, 7, -1]
NUMS = [-7, 1, 1, 2, 3, 5, 100, INT_MAX]


def ride_words(r, uitype=None, track=None):
    """A ride record as a dict of the fields the unit reads (the host's RideInfo has its own layout, so it is set by field)."""
    return dict(
        uitype=(r.choice(UITYPES) if uitype is None else uitype) & MASK,
        track=(r.choice([0, 0, 1, 2, 3, -1]) if track is None else track) & MASK,
        decay=r.choice([1, 1, 2, 3, 5, 10, 100, -2, -3, 0x7FFFFFFF]) & MASK,
        value=r.choice([0, 1, 5, 100, -5, 1000, INT_MAX, INT_MIN, r.getrandbits(32)]) & MASK,
        bonus=[r.choice([0, 1, 7, 100, -5, 1000, INT_MAX, INT_MIN, r.getrandbits(32)]) & MASK for _ in range(3)],
        costs=[r.choice([0, 1, 2, 10, 100, 5000, 100000, INT_MAX, 0xFFFFFFFF, r.getrandbits(32)]) for _ in range(4)])


def rand_thing(r, kind=None):
    return dict(kind=r.choice([1, 1, 1, 3, 3, 4, 5, 6, 7, 8, 0, 2, 9, 0xFF]) if kind is None else kind, dead=r.choice([0, 0, 0, 1, 2, 0xFF]),
                f0c=0, otype=r.choice([0, 1, 2, 3, 4, 5, 7, 0xFFFF, 0x1234]), f40=r.choice(F40S) if r.random() < 0.7 else r.uniform(-50, 400),
                f4c=r.choice([0, 1, 2, 3, 4, 5, 0x7F, 0x80, 0xC8, 0xFF]) if r.random() < 0.8 else r.randrange(256))


def finish_queues(c, fn, r):
    used = c["used"]
    q = c["queues"]
    q["head"] = [used[0]] * WALKS if used else [0]
    q["next"] = (used[1:] + [0]) * WALKS if used else [0]


def make_world(r, fn):
    n = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 8])
    ids = r.sample(range(1, MAXID + 1), n)
    rows = {i: s for i, s in zip(ids, r.sample(range(1, SLOTS + 1), n))}
    things = [rand_thing(r) for _ in range(SLOTS)]
    if fn == 1:
        for t in things:
            t["f4c"] = r.choice([0, 1, 2, 3])
    used = [r.choice(ids) for _ in range(r.randint(0, 30))] if ids else []
    objl = r.sample(ids, r.randint(0, len(ids)))
    for k, oid in enumerate(objl):
        things[rows[oid] - 1]["f0c"] = objl[k + 1] if k + 1 < len(objl) else 0
    if r.random() < 0.15:
        rows[0] = r.randint(1, SLOTS)
    rides = [ride_words(r) for _ in range(RIDES)]
    controls = [(r.choice(NUMS), r.getrandbits(32)) for _ in range(CONTROLS)]
    q = {"rideinfo": [r.randint(1, RIDES) for _ in range(r.randint(1, 6))], "ctl": [r.randrange(CONTROLS) for _ in range(r.randint(1, 8))],
         "ctlc": [r.randrange(CONTROLS) for _ in range(r.randint(1, 8))],
         "inside": [r.choice([0, 1, 1, 2, -1, 0x100]) for _ in range(r.randint(1, 6))], "get": [r.choice([INT_MIN, -5, 0, 1, 7, 100000, INT_MAX])],
         "cond": [r.choice([0, 0, 1, 2, -1, 0x100]) for _ in range(r.randint(1, 5))], "avail": [r.choice([0, 1, 1, 1, 2, -1, 0x100]) for _ in range(r.randint(1, 5))],
         "age": [r.choice([-5, -1, 0, 1, 2, 5, 9, 10, 29, 30, 100, 1000]) for _ in range(r.randint(1, 5))],
         "span": [r.choice([-3, -1, 0, 1, 2, 5, 40]) * TICK + r.choice([0, 1, TICK - 1, 12345]) for _ in range(r.randint(1, 5))]}
    turn = r.getrandbits(32)
    c = dict(fn=fn, things=things, rows=rows, used=used, objhead=objl[0] if objl else 0, rides=rides, controls=controls, queues=q, turn=turn, seconds=0,
             val=(r.getrandbits(32), r.choice([INT_MIN, -1, 0, 5, 1000, INT_MAX]), 1), cells=(r.getrandbits(32), 10, 5))
    if fn == 1:
        recompute = r.random() < 0.1
        c["val"] = (r.getrandbits(32), c["val"][1], 0 if recompute else r.choice([1, 2, 0x80, 0xFF]))
        c["seconds"] = r.choice([0, 1, MASK, 100, r.getrandbits(32)])
        c["cells"] = (r.getrandbits(32), r.choice([8, 12, 15, 16]), r.choice([2, 4, 8, 16]))
        if not recompute:
            # the cached answer: the seconds against the age of the stamp, (turn - stamp) >> 2 unsigned
            age = ((turn - c["val"][0]) & MASK) >> 2
            c["seconds"] = r.choice([age, (age + 1) & MASK, (age - 1) & MASK, 0, MASK])
    finish_queues(c, fn, r)
    return c


def edges(fn):
    out = []
    r = random.Random(900 + fn)
    for _ in range(40):
        out.append(make_world(r, fn))
    if fn == 0:
        # one thing per kind, lists of chosen length: visitors around the cap of 1000, rides around the 20 and 10 caps, staff around 4
        def base(types_uitype, f4c, listing):
            c = make_world(r, 0)
            things = [rand_thing(r, kind=1), rand_thing(r, kind=3), rand_thing(r, kind=4), rand_thing(r, kind=5), rand_thing(r, kind=6), rand_thing(r, kind=7),
                      rand_thing(r, kind=8), rand_thing(r, kind=3)]
            for t in things:
                t["dead"] = 0
                t["f0c"] = 0
            things[1]["f4c"] = f4c
            things[7]["f4c"] = f4c
            c["things"] = things
            c["rows"] = {i + 1: i + 1 for i in range(SLOTS)}
            c["rides"] = [ride_words(r, uitype=types_uitype) for _ in range(RIDES)]
            c["used"] = listing
            c["queues"]["inside"] = [1]
            c["queues"]["rideinfo"] = [r.randint(1, RIDES)]
            c["objhead"] = 0
            finish_queues(c, 0, r)
            return c
        for nv in (0, 1, 2, 50, 999, 1000, 1001, 1100):
            out.append(base(3, 0, [1] * nv + [2, 3, 5]))
            out.append(base(0, 2, [1] * nv + [2] * (nv % 7)))
        for uit in (0, 1, 2, 3, 4):
            for n in (0, 1, 3, 4, 5, 6, 9, 10, 11, 13, 14, 19, 20, 21):
                for f4c in (0, 1, 2, 3, 0x80, 0xFF):
                    if r.random() < 0.55:
                        out.append(base(uit, f4c, [2] * n + [8] * (n // 2) + [1, 1]))
        for kind_slot in (3, 4, 5, 6, 7):
            for n in (0, 1, 3, 4, 5, 6, 8):
                out.append(base(r.choice([0, 1, 2, 3]), 1, [kind_slot] * n + [2, 1]))
    if fn == 1:
        for seconds in (0, 1, 5, MASK, 0x80000000, 0x7FFFFFFF):
            for worked in (0, 1, 2, 0x80, 0xFF):
                for stamp_delta in (0, 3, 4, 7, 400, MASK, 0x80000000):
                    c = make_world(r, 1)
                    c["val"] = ((c["turn"] - stamp_delta) & MASK, c["val"][1], worked)
                    c["seconds"] = seconds
                    if worked == 0 and r.random() < 0.8:
                        c["val"] = (c["val"][0], c["val"][1], 1)
                    out.append(c)
        for k in range(14):
            c = make_world(r, 1)
            c["val"] = (c["val"][0], c["val"][1], 0)
            c["cells"] = (r.getrandbits(32), (16, 8, 0, 14, 12, 16, 15)[k % 7], (16, 1, 8, 4, 16, 0, 2)[k % 7])
            out.append(c)
        # the object walk: every ride shape with an object list of several upgrades levels
        for k in range(30):
            c = make_world(r, 1)
            c["val"] = (c["val"][0], c["val"][1], 1)
            c["seconds"] = 0
            c["val"] = ((c["turn"] - 400) & MASK, c["val"][1], 0)
            c["cells"] = (r.getrandbits(32), 16, 0)
            out.append(c)
    if fn == 2:
        for k in range(100):
            c = make_world(r, 2)
            ids = list(c["rows"].keys())
            ids = [i for i in ids if i]
            if ids:
                objl = r.sample(ids, len(ids))
                for t in c["things"]:
                    t["f0c"] = 0
                for j, oid in enumerate(objl):
                    c["things"][c["rows"][oid] - 1]["f0c"] = objl[j + 1] if j + 1 < len(objl) else 0
                    c["things"][c["rows"][oid] - 1]["otype"] = r.choice([0, 1, 2])
                c["objhead"] = objl[0]
            c["rides"] = [ride_words(r, uitype=r.choice([0, 2, 0, 2, 1, 3]), track=r.choice([0, 0, 1])) for _ in range(RIDES)]
            c["controls"] = [(r.choice(NUMS), r.getrandbits(32)) for _ in range(CONTROLS)]
            c["queues"]["avail"] = [r.choice([1, 1, 1, 0, 2])]
            out.append(c)
    return out


def mk(name, fn):
    return Base(name, run_ppc, run_host, edge=lambda: edges(fn), rnd=lambda r: make_world(r, fn))


TESTS = [mk("CParkAnalyser::GetOverallParkRating", 0), mk("CParkAnalyser::GetParkValue", 1), mk("CParkAnalyser::GetTotalParkAttractionPoints", 2)]
