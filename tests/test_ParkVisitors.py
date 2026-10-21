"""Differential tests for unit w154, the park analyser's visitor counts and opinions (src/game/park/analyser.cpp). See difftest.py and tpwtest.py.

CParkAnalyser::OfferOpinionOn, IveMisbehaved, PersonLeftPark, GetHappinessLevel, GetVisitorsInPark, NumberOfVisitorsOutsidePark, NumberOfVisitorsNeedingToilet,
NumberOfHungryVisitors, NumberOfThirstyVisitors, MisbehavingVisitorsLastMonth, NumberOfVisitorsSinceBeginning, NumberOfVisitorsInLastMonths,
NumberOfVisitorsWithBalloons, NumberOfVisitorsWithCostumes, PercentageOfVisitorsWithHappinessOver and Under, GetAverageOpinionOn, HaveCompleteSetOfOpinions and
GetAverageHappinessAroundPoint. Every callee is a fake that records the same event on both sides (the asserts, the world accessor, GetMap, the used-thing walk,
IsInsidePark, GatesOpen, GetTicketBoothPos, GetFunnyTimeForGameTurns, the analyser value and the histories); the answers come from queues. The things, the map
cells, the analyser and the world's turn word are built from one description on both sides, and a case compares the answer, the analyser's other bytes (a guard
of 0x40 bytes each side) and the whole call log. CTimeSig::SecondsSinceLastUpdate (0x1015f124) and the 64-bit divide helper (0x10237e1c) run as the original.
The free shim CParkAnalyser_GetHappinessLevel is a forwarder (untested on its own).
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ParkVisitors"

MASK = 0xFFFFFFFF
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF
TABLE, G_GAME_WORLD = 0x1034CBFC, 0x1037EBFC
ASSERT, GETWORLD, HEAD, NEXT, INSIDE = 0x1000DDE8, 0x1014205C, 0x1013ADB0, 0x1013AE08, 0x10115A80
VALGET, VALSET, GATES, BOOTH, GETMAP, FUNNY = 0x100EF3C8, 0x100EF400, 0x100E7028, 0x101019BC, 0x10142098, 0x10112928
H_SET_U8, H_AVG, H_FAR, H_SET_I, H_GETV = 0x100EF8C4, 0x100EF930, 0x100EFB1C, 0x100F0114, 0x1001C2A8
SECONDS, DIVIDE = 0x1015F124, 0x10237E1C
OFFER, MISBEHAVED, LEFT, HAPPY, GVIP, OUTSIDE = 0x100E4360, 0x100E43E4, 0x100E4600, 0x100E4CD8, 0x100E5414, 0x100E73A8
TOILET, HUNGRY, THIRSTY, LASTMONTH, SINCE, INMONTHS = 0x100E7520, 0x100E7680, 0x100E77D8, 0x100E7930, 0x100E7994, 0x100E79E8
BALLOONS, COSTUMES, OVER, UNDER, AVGOP, COMPLETE, AROUND = 0x100E7A94, 0x100E7BD8, 0x100E7D1C, 0x100E7E98, 0x100E8018, 0x100E80A8, 0x100EF00C
OWN = {OFFER, MISBEHAVED, LEFT, HAPPY, GVIP, OUTSIDE, TOILET, HUNGRY, THIRSTY, LASTMONTH, SINCE, INMONTHS, BALLOONS, COSTUMES, OVER, UNDER, AVGOP, COMPLETE,
       AROUND, SECONDS, DIVIDE}
TAIL0 = 0x1DA6F0
VAL_AT = 0x1F0B8
HIST_AT = 0x20820
ANALYSER_SIZE = 0x21C10
GUARD = 0x40
CELL_PAD = 0x100
CELL_BYTES = 0x4000 * 0x44
SLOTS = 8
THING_SIZE = 0x448
L_AN, L_VAL, L_WORLD, L_MAP, L_PT, L_THING = 1, 2, 3, 7, 8, 10
ASSERT_TEXT = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"


def s32(v):
    return T.s32(v)


def s64(v):
    v &= (1 << 64) - 1
    return v - (1 << 64) if v & (1 << 63) else v


def setup(runner):
    T.install_std_stubs(runner.emu)


def lib_addr(lib, which):
    lib.tpw_pv_addr.restype = C.c_void_p
    lib.tpw_pv_addr.argtypes = [C.c_int32]
    return lib.tpw_pv_addr(which)


def fbits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


NAN, INF = 0x7FC00000, 0x7F800000
# NOTE: no float at or above 2^31 (Unicorn gives 0x80000000 for fctiwz there, the processor 0x7fffffff; see test_SkyRender.py, FINDINGS 2026-10-08)
FLOATS = [fbits(x) for x in (0.0, -0.0, 0.5, 1.0, 1.5, 100.25, 126.5, 127.0, 127.9, 128.0, 128.5, 254.9, 255.0, 255.9, 256.0, 257.0, 300.0, 511.0, 512.0, -1.0,
                             -0.5, -255.0, -256.0, -257.0, -1e10, -2147483648.0, -2147483000.0, 2147483000.0, 16777216.0)] + [NAN, INF | 0x80000000, 0xFFC00000]
KINDS = [0, 1, 1, 1, 1, 2, 3, 4, 0x13, 0x81, 0xFF]
WORDS = [0, 1, 2, 3, 4, 0x100, 0x10000, 0x80000000, 0xFFFFFFFF, 0x7FFFFFFF, 0x20000]
INSIDE_ANSWERS = [0, 1, 1, 2, 0x100, 0x10000, 0x80000000, 0xFFFFFFFF, 0xFFFF0000, 0x7FFFFFFF]
LEVELS = [INT_MIN, -256, -2, -1, 0, 1, 2, 50, 100, 126, 127, 128, 129, 200, 254, 255, 256, 257, 511, 1000, INT_MAX]


def rand_float(r):
    k = r.random()
    if k < 0.45:
        return r.choice(FLOATS)
    if k < 0.9:
        return fbits(r.uniform(-2, 270))
    bits = r.getrandbits(32)
    value = struct.unpack(">f", struct.pack(">I", bits))[0]
    return bits if (value != value or value < 2147483648.0) else fbits(255.0)


def mk_thing(r, kind=None, outside=False):
    return dict(kind=kind if kind is not None else r.choice(KINDS), next=r.getrandbits(16), costume=r.choice(WORDS), happy=rand_float(r), thirst=rand_float(r),
                hunger=rand_float(r), toilet=rand_float(r), balloon=r.choice(WORDS), state=3 if (outside and r.random() < 0.6) else r.choice(WORDS + [3, 3]))


# ------------------------------------------------------------------ the memory of a case (one description, both byte orders)


def writes(c):
    """(target, offset, struct code, value): target 'an', 'tail', 'cell' or a thing slot number."""
    w = []
    an = c["an"]
    w += [("an", 0x21BFC, "i", an["kids"]), ("an", 0x21C00, "i", an["away"]), ("an", 0x21C04, "i", an["ever"]), ("an", 0x21C08, "i", an["life"])]
    w += [("an", VAL_AT, "I", an["stamp"] & MASK), ("an", VAL_AT + 4, "i", an["val"]), ("an", VAL_AT + 8, "B", an["worked"] & 0xFF)]
    w += [("tail", 0x1C, "I", c["turn"] & MASK)]
    for slot, th in enumerate(c["things"]):
        w += [(slot, 2, "B", th["kind"] & 0xFF), (slot, 0xA, "H", th["next"] & 0xFFFF), (slot, 0x24, "I", th["costume"]), (slot, 0x19C, "I", th["happy"]),
              (slot, 0x1A4, "I", th["thirst"]), (slot, 0x1A8, "I", th["hunger"]), (slot, 0x1AC, "I", th["toilet"]), (slot, 0x210, "I", th["balloon"]),
              (slot, 0x220, "I", th["state"])]
    for index, who in c["who"].items():
        w += [("cell", index * 0x44 + 0x24, "H", who)]
    return w


def mask_memory(raw, big):
    """The analyser block (guards included) without the six values and the four counters; the counters as numbers."""
    fmt = ">i" if big else "<i"
    counters = tuple(struct.unpack_from(fmt, raw, GUARD + 0x21BFC + 4 * k)[0] for k in range(4))
    rest = bytearray(raw)
    for lo, hi in ((VAL_AT, VAL_AT + 0x48), (0x21BFC, 0x21C0C)):
        rest[GUARD + lo:GUARD + hi] = b"\0" * (hi - lo)
    return dict(counters=counters, rest=bytes(rest))


def qn(q):
    return {k: [s64(v) for v in vs] for k, vs in q.items()}


def queues(c):
    return {"head": [c["head"]], "next": c["nxt"], "inside": c["inside"], "gates": [c["gates"]], "booth": c["booth"], "avg": [c["avg"]], "far": [c["far"]],
            "hv": [c["hv"]], "hvok": [c["hvok"]], "ticks": [c["ticks"]], "get": [c["get"]]}


def stubs_for(e, L, world, cells, an):
    def off(a):
        return a - an

    def assertion(emu):
        L.rec("GAME_ASSERTMSG", [s32(emu.r(3))], [emu.cstr(emu.r(4))])

    def getworld(emu):
        L.rec("GetWorld", [L.lab(emu.r(3))])
        emu.set_r(3, world)

    def getmap(emu):
        L.rec("GetMap", [L.lab(emu.r(3))])
        emu.set_r(3, cells)

    def head(emu):
        L.rec("GetUsedThingHead", [L.lab(emu.r(4))])
        e.write(emu.r(3), struct.pack(">H", L.ret("head", 0) & 0xFFFF))

    def nxt(emu):
        L.rec("GetNextUsedThing", [L.lab(emu.r(4)), L.lab(emu.r(5))])
        e.write(emu.r(3), struct.pack(">H", L.ret("next", 0) & 0xFFFF))

    def inside(emu):
        L.rec("IsInsidePark", [L.lab(emu.r(3))])
        emu.set_r(3, T.u32(L.ret("inside", 0)))

    def gates(emu):
        L.rec("GatesOpen", [L.lab(emu.r(3))])
        emu.set_r(3, 1 if L.ret("gates", 0) else 0)

    def booth(emu):
        L.rec("GetTicketBoothPos", [L.lab(emu.r(4)), s32(emu.r(5))])
        e.write(emu.r(3), struct.pack(">H", L.ret("booth", 0) & 0xFFFF))

    def funny(emu):
        L.rec("GetFunnyTimeForGameTurns", [L.lab(emu.r(4)), emu.r(5)])
        e.write(emu.r(3), struct.pack(">q", L.ret("ticks", 0)))

    def valget(emu):
        L.rec("ValGet", [off(emu.r(3))])
        emu.set_r(3, T.u32(L.ret("get", 0)))

    def valset(emu):
        L.rec("ValSet", [off(emu.r(3)), s32(emu.r(4))])

    def set_u8(emu):
        L.rec("HistSetAtEnd", [off(emu.r(3)), emu.r(4) & 0xFF])

    def set_int(emu):
        L.rec("HistSetAtEnd", [off(emu.r(3)), s32(emu.r(4))])

    def average(emu):
        L.rec("HistAverage", [off(emu.r(3)), s32(emu.r(5))])
        e.write(emu.r(4), bytes([L.ret("avg", 0) & 0xFF]))

    def farback(emu):
        L.rec("HistHowFarBack", [off(emu.r(3))])
        emu.set_r(3, T.u32(L.ret("far", 0)))

    def getvalue(emu):
        L.rec("HistGetValue", [off(emu.r(3)), s32(emu.r(5))])
        e.write(emu.r(4), struct.pack(">i", s32(L.ret("hv", 0))))
        emu.set_r(3, ((0x5A5A5A << 8) | (1 if L.ret("hvok", 0) else 0)) & MASK)   # the bool is the low byte, the rest is garbage

    S = CL.Stubs()
    S.real = OWN
    for addr, fn in ((ASSERT, assertion), (GETWORLD, getworld), (GETMAP, getmap), (HEAD, head), (NEXT, nxt), (INSIDE, inside), (GATES, gates), (BOOTH, booth),
                     (FUNNY, funny), (VALGET, valget), (VALSET, valset), (H_SET_U8, set_u8), (H_SET_I, set_int), (H_AVG, average), (H_FAR, farback),
                     (H_GETV, getvalue)):
        S.internal(addr, fn)
    return S


def build_ppc(e, c):
    """The PowerPC memory of a case; returns (log, stubs, addresses)."""
    L = CL.PpcLog({}, qn(queues(c)))
    e.cl = L
    block = e.alloc(GUARD + ANALYSER_SIZE + GUARD, b"\xa5" * (GUARD + ANALYSER_SIZE + GUARD))
    an = block + GUARD
    tail = e.alloc(0x80, b"\0" * 0x80)
    cellblock = e.alloc(CELL_PAD + CELL_BYTES + CELL_PAD, b"\0" * (CELL_PAD + CELL_BYTES + CELL_PAD))
    cells = cellblock + CELL_PAD
    things = [e.alloc(THING_SIZE, b"\xa5" * THING_SIZE) for _ in range(SLOTS)]
    world = tail - TAIL0 if c["mode"] == "walk" else cells - 0x2D8
    L.labels.update({an: L_AN, an + VAL_AT: L_VAL, world: L_WORLD, world + 0x2A0: L_PT})
    if c["mode"] == "cells":
        L.labels[cells] = L_MAP
    L.labels.update({a: L_THING + i for i, a in enumerate(things)})
    addr = {"an": an, "tail": tail, "cell": cells}
    addr.update({i: a for i, a in enumerate(things)})
    for target, offset, code, value in writes(c):
        e.write(addr[target] + offset, struct.pack(">" + code, value))
    for row in range(0, SLOTS + 1):
        slot = c["rows"].get(row, 0)
        e.write(TABLE + 0x14 * row, struct.pack(">I", things[slot - 1] if slot else 0))
    e.write(G_GAME_WORLD, struct.pack(">I", world))
    return L, stubs_for(e, L, world, cells, an), dict(an=an, block=block)


def host_setup(lib, c):
    lib.tpw_pv_mode.restype = C.c_void_p
    lib.tpw_pv_mode.argtypes = [C.c_int32]
    world = lib.tpw_pv_mode(0 if c["mode"] == "walk" else 1)
    an = lib_addr(lib, 0)
    C.memset(an - GUARD, 0xA5, GUARD + ANALYSER_SIZE + GUARD)
    C.memset(lib_addr(lib, 1), 0, 0x80)
    if c["mode"] == "cells":
        C.memset(lib_addr(lib, 3), 0, CELL_PAD + CELL_BYTES + CELL_PAD)
    for i in range(SLOTS):
        C.memset(lib_addr(lib, 10 + i), 0xA5, THING_SIZE)
    addr = {"an": an, "tail": lib_addr(lib, 1), "cell": lib_addr(lib, 2)}
    addr.update({i: lib_addr(lib, 10 + i) for i in range(SLOTS)})
    for target, offset, code, value in writes(c):
        data = struct.pack("<" + code, value)
        C.memmove(addr[target] + offset, data, len(data))
    lib.tpw_pv_row.argtypes = [C.c_int32, C.c_int32]
    for row in range(0, SLOTS + 1):
        lib.tpw_pv_row(row, c["rows"].get(row, 0))
    fixed = {L_AN: an, L_VAL: an + VAL_AT, L_WORLD: world, L_PT: world + 0x2A0}
    if c["mode"] == "cells":
        fixed[L_MAP] = addr["cell"]
    fixed.update({L_THING + i: addr[i] for i in range(SLOTS)})
    CL.host_begin(lib, fixed, qn(queues(c)))
    return an


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


# ------------------------------------------------------------------ the cases


def place(r, c, cells):
    """Chains the things (ids 1..8) into the given map cell indices; who: cell index -> first thing id."""
    pool = r.sample(range(1, SLOTS + 1), r.choice([0, 1, 2, 3, 4, 6, 8, 8]))
    who = {}
    for index in cells:
        if not pool:
            break
        n = r.randint(1, min(3, len(pool)))
        chain, pool = pool[:n], pool[n:]
        who[index] = chain[0]
        for i, tid in enumerate(chain):
            c["things"][tid - 1]["next"] = chain[i + 1] if i + 1 < len(chain) else 0
    for tid in pool:
        c["things"][tid - 1]["next"] = 0
    c["who"] = who


def case(r, mode="walk", n=None, cells=None, outside=False, **kw):
    things = [mk_thing(r, outside=outside) for _ in range(SLOTS)]
    stamp = r.getrandbits(32)
    c = dict(mode=mode, things=things, rows={i: i for i in range(1, SLOTS + 1)}, who={})
    if r.random() < 0.12:
        c["rows"][0] = r.randint(1, SLOTS)
    c["an"] = dict(kids=r.choice([0, 1, 5, 100, INT_MAX, -1]), away=r.choice([0, 3, 77, INT_MAX, INT_MIN]), ever=r.choice([0, 9, 1000, INT_MAX]),
                   life=r.choice([0, 1, 12345, 2000000, INT_MAX, INT_MIN, -5]), stamp=stamp, val=r.choice([0, 1, 7, -3, INT_MIN]),
                   worked=r.choice([0, 1, 1, 2, 0x80, 0xFF]))
    c["turn"] = (stamp + r.choice([0, 0, 1, 2, 3, 4, 5, 7, 8, 100, 1 << 31, MASK])) & MASK if r.random() < 0.7 else r.getrandbits(32)
    ids = r.sample(range(1, SLOTS + 1), n if n is not None else r.choice([0, 1, 1, 2, 3, 4, 5, 6, 7, 8]))
    c["head"], c["nxt"] = (ids[0] if ids else 0), ids[1:] + [0]
    c["inside"] = [r.choice(INSIDE_ANSWERS) for _ in range(SLOTS)]
    c["gates"] = r.choice([0, 1, 1, 1, 2, 0x100, MASK])
    c["booth"] = [r.choice([1, 2, 129, 0x2000, 0x4000, 130, 0x2001]) for _ in range(2)]
    c["avg"], c["far"] = r.getrandbits(8), r.choice([-5, -1, 0, 1, 49, 50, 51, 100, INT_MAX, INT_MIN, 48])
    c["hv"], c["hvok"] = r.choice([0, 1, 100, -7, INT_MAX, INT_MIN, 2000000]), r.choice([0, 1, 1])
    c["ticks"] = r.choice(TICKS) if r.random() < 0.7 else s64(r.getrandbits(64)) >> r.randint(0, 60)
    c["get"] = r.choice([0, 1, 7, -3, INT_MAX, INT_MIN])
    c["garbage"] = r.getrandbits(32)
    if mode == "cells":
        place(r, c, cells if cells is not None else [])
    c.update(kw)
    return c


DAY = 864000000000
TICKS = [0, 1, DAY - 1, DAY, DAY + 1, 2 * DAY, 5 * DAY + 77, -1, -DAY, -DAY + 1, -DAY - 1, (1 << 63) - 1, -(1 << 63), 3 * DAY - 1, 1 << 40, 100 * DAY, 2**32 * DAY // 2]


def result(kind, r3, f1):
    if kind == "i":
        return s32(r3)
    if kind == "b":
        return r3 & 0xFF
    return 0   # "v": a void function


def call_ppc(entry, args_fn, kind="i"):
    def fn(e, c):
        L, S, a = build_ppc(e, c)
        saved = S.activate(e)
        try:
            r3, _, f1 = T.call_ppc(e, entry, args_fn(a["an"], c))
        finally:
            CL.Stubs.deactivate(e, saved)
        out = dict(log=L.log, mem=mask_memory(bytes(e.read(a["block"], GUARD + ANALYSER_SIZE + GUARD)), True))
        if kind == "f":
            single = struct.unpack(">f", struct.pack(">f", f1))[0]
            out["bits"] = fbits(single)
            out["exact"] = (single == f1) or (single != single and f1 != f1)
        else:
            out["ret"] = result(kind, r3, f1)
        return out
    return fn


def call_host(which, a_fn, kind="i"):
    def fn(lib, c):
        an = host_setup(lib, c)
        try:
            if kind == "f":
                lib.tpw_pv_happy.restype = C.c_float
                lib.tpw_pv_happy.argtypes = [C.c_uint32]
                value = lib.tpw_pv_happy(a_fn(c)[0] & MASK)
            else:
                lib.tpw_pv_call.restype = C.c_int32
                lib.tpw_pv_call.argtypes = [C.c_int32, C.c_int32, C.c_int32]
                a, b = a_fn(c)
                value = lib.tpw_pv_call(which, s32(a), s32(b))
        finally:
            log = CL.host_end(lib)
        out = dict(log=log, mem=mask_memory(C.string_at(an - GUARD, GUARD + ANALYSER_SIZE + GUARD), False))
        if kind == "f":
            out["bits"] = fbits(value)
            out["exact"] = True
        else:
            out["ret"] = (value & 0xFF) if kind == "b" else (0 if kind == "v" else value)
        return out
    return fn


def noarg(an, c):
    return [T.I(an)]


def noarg_host(c):
    return (0, 0)


def junk(c, k=0):
    return (c["garbage"] * (k + 3) + 0x9E3779B9 * (k + 1)) & MASK


def u8_args(an, c):
    return [T.I(an), T.I((junk(c) & 0xFFFFFF00) | (c["arg"] & 0xFF))]


def u8_host(c):
    return (c["arg"] & 0xFF, 0)


def i_args(an, c):
    return [T.I(an), T.I(c["arg"] & MASK)]


def i_host(c):
    return (c["arg"], 0)


def bool_args(an, c):
    return [T.I(an), T.I((junk(c) & 0xFFFFFF00) | (c["arg"] & 0xFF))]


def bool_host(c):
    return (1 if (c["arg"] & 0xFF) else 0, 0)


def offer_args(an, c):
    return [T.I(an), T.I(c["arg"] & MASK), T.I((junk(c) & 0xFFFFFF00) | (c["arg2"] & 0xFF))]


def offer_host(c):
    return (c["arg"], c["arg2"] & 0xFF)


def happy_args(an, c):
    return [T.I(an), T.I(c["arg"] & MASK)]


def happy_host(c):
    return (c["arg"] & MASK, 0)


def around_args(an, c):
    return [T.I(an), T.I(((c["arg"] & 0xFFFF) << 16) | (junk(c) & 0xFFFF)), T.I(c["radius"] & MASK)]


def around_host(c):
    return (c["arg"] & 0xFFFF, s32(c["radius"]))


def with_arg(gen, pool, wide=True):
    def fn(r, **kw):
        k = r.random()
        arg = r.choice(pool) if k < 0.7 else (s32(r.getrandbits(32)) if (k < 0.85 and wide) else r.randint(-5, 300))
        return gen(r, arg=arg, arg2=r.choice([0, 1, 7, 0x80, 0xFF, r.getrandbits(8)]), **kw)
    return fn


# ---- walks (the used things): the counts and the percentages
def walk_rnd(pool):
    return lambda r: with_arg(case, pool)(r)


def walk_edges(pool):
    def fn():
        r = random.Random(31)
        out = []
        for n in (0, 1, 2, 3, 5, 8):
            for inside in ([1] * 8, [0] * 8, [1, 0] * 4, [0x100] * 8, [0x80000000] * 8):
                for level in pool:
                    c = case(r, n=n, arg=level, arg2=1, inside=inside)
                    out.append(c)
        # every kind of thing, one visitor at a time
        for kind in (0, 1, 2, 3, 4, 0x81):
            for level in (0, 1, 127, 128, 255):
                th = [mk_thing(r, kind=kind) for _ in range(SLOTS)]
                out.append(case(r, n=3, arg=level, arg2=1, things=th, inside=[1] * 8))
        out.append(case(r, n=3, arg=5, arg2=1, rows={0: 2, **{i: i for i in range(1, SLOTS + 1)}}))      # row 0 taken: the assertions read false
        return out
    return fn


BYTE_LEVELS = [0, 1, 2, 126, 127, 128, 129, 254, 255, 256, 0x1FF, 0x1234, -1, INT_MIN]


def gen_levelled(floatpool):
    """Walk cases whose thing fields come from floats near the level, so that every compare is on the edge."""
    def fn(r, **kw):
        arg = r.choice(LEVELS)
        th = [mk_thing(r, kind=r.choice([1, 1, 1, 1, 0, 2])) for _ in range(SLOTS)]
        for t in th:
            for f in ("happy", "thirst", "hunger", "toilet"):
                t[f] = r.choice(floatpool)
        return case(r, arg=arg, arg2=1, things=th, inside=[r.choice([1, 1, 1, 0]) for _ in range(SLOTS)], **kw)
    return fn


def mixed(*gens):
    return lambda r: r.choice(gens)(r)


def walk_test(title, entry, which, args_fn, host_fn, pool, kind="i"):
    return Base(title, call_ppc(entry, args_fn, kind), call_host(which, host_fn, kind), edge=walk_edges(pool),
                rnd=mixed(walk_rnd(pool), gen_levelled(FLOATS), gen_levelled([fbits(float(x)) for x in (0, 1, 127, 128, 129, 254, 255, 256)])))


# ---- GetVisitorsInPark
def gvip_edges():
    r = random.Random(41)
    out = []
    for worked in (0, 1, 0x80):
        for secs in (0, 1, 3, 4, 8, 9, 100):
            for arg in (0, 1, 3, 4, 5, 8, 99, MASK, 0x80000000):
                stamp = r.getrandbits(32) & ~3
                out.append(case(r, n=r.choice([0, 2, 4]), arg=arg, arg2=0, an=dict(kids=0, away=0, ever=0, life=0, stamp=stamp, val=5, worked=worked),
                                turn=(stamp + secs * 4) & MASK))
    return out


def gvip_rnd(r):
    return with_arg(case, [0, 1, 2, 3, 4, 5, 8, 10, 100, MASK, 0x80000000])(r)


# ---- NumberOfVisitorsOutsidePark
def outside_rnd(r):
    cells = r.sample([0, 1, 127, 128, 0x1FFF, 0x3FFF, 5, 300], r.randint(0, 6))
    booth = [r.choice([1, 2, 129, 130, 0x2000, 0x4000, 6]) for _ in range(2)]
    cells = list({*cells, *[b - 1 for b in booth if r.random() < 0.8]})
    return case(r, mode="cells", cells=cells, outside=True, booth=booth, arg=0, arg2=0)


def outside_edges():
    r = random.Random(51)
    out = []
    for b0 in (1, 129, 0x4000):
        for b1 in (1, 2, 0x4000):
            for cells in ([b0 - 1], [b1 - 1], [b0 - 1, b1 - 1], []):
                out.append(case(r, mode="cells", cells=cells, outside=True, booth=[b0, b1], arg=0, arg2=0))
    for _ in range(12):
        out.append(outside_rnd(r))
    return out


# ---- GetAverageHappinessAroundPoint
def visited(ident, radius):
    index = (ident - 1) & MASK
    x, y = index & 0x7F, index >> 7
    end_x = (x + radius) & MASK
    end_x = 0x7F if end_x > 0x7F else end_x
    end_y = (y + radius) & MASK
    end_y = 0x7F if end_y > 0x7F else end_y
    start_x, start_y = s32(x - radius), s32(y - radius)
    ids = []
    cx = start_x
    while cx <= s32(end_x):
        cy = start_y
        while cy <= s32(end_y):
            ids.append(((cy << 7) + 1 + cx) & 0xFFFF)
            cy += 1
            if len(ids) > 300:
                return None
        cx += 1
    return ids


def around_case(r, ident=None, radius=None):
    for _ in range(200):
        i = ident if ident is not None else r.choice([r.randint(1, 0x4000), r.randint(1, 0x4000), 1, 0x4000, 129, 128, 0x7F, 0x80, 0x2000, 0x3F80, 0x3FFF])
        rad = radius if radius is not None else r.choice([0, 0, 1, 1, 2, 3, 4, 5, 0xFFFFFFFF, 0xFFFFFF80, 0x80000000, 0x7FFFFFFF, 130, r.randint(0, 6)])
        ids = visited(i, rad)
        if ids is not None and all(0 <= v <= 0x4000 for v in ids):
            break
    else:
        i, rad, ids = 0x2000, 1, visited(0x2000, 1)
    pool = list(set(ids))
    cells = r.sample(pool, min(len(pool), r.randint(0, 8))) if pool else []
    return case(r, mode="cells", cells=[v - 1 for v in cells if v >= 1], arg=i, radius=rad, arg2=0)


def around_edges():
    r = random.Random(61)
    out = []
    for ident in (0x2000, 129 + 1, 1, 0x4000, 0x7F, 0x80, 0x3F81, 0x2040):
        for radius in (0, 1, 2, 3, 0xFFFFFFFF, 0x80000000, 0xFFFFFF80):
            c = around_case(r, ident, radius)
            if visited(ident, radius) is not None and all(0 <= v <= 0x4000 for v in visited(ident, radius)):
                out.append(c)
    for _ in range(25):
        out.append(around_case(r))
    return out


# ---- the simple ones
def simple_rnd(pool, wide):
    return lambda r: with_arg(case, pool, wide)(r)


def simple_edges(pool):
    def fn():
        r = random.Random(71)
        return [case(r, arg=a, arg2=b) for a in pool for b in (0, 1, 0x7F, 0x80, 0xFF)]
    return fn


def simple_test(title, entry, which, args_fn, host_fn, pool, kind="i", wide=True):
    return Base(title, call_ppc(entry, args_fn, kind), call_host(which, host_fn, kind), edge=simple_edges(pool), rnd=simple_rnd(pool, wide))


TESTS = [
    simple_test("CParkAnalyser::OfferOpinionOn", OFFER, 0, offer_args, offer_host, [0, 1, 7, 0x13, 0x14, -1, 3], "v", wide=False),
    simple_test("CParkAnalyser::IveMisbehaved", MISBEHAVED, 1, i_args, i_host, [0, 1, -1, 2, 0x100, INT_MIN, INT_MAX], "v"),
    simple_test("CParkAnalyser::PersonLeftPark", LEFT, 2, i_args, i_host, [0, 1, 2, 24, 100, 1000, -1, INT_MAX, INT_MIN, 0x10000], "v"),
    Base("CParkAnalyser::GetHappinessLevel", call_ppc(HAPPY, happy_args, "f"), call_host(0, happy_host, "f"), edge=walk_edges([0, 4, MASK, 0x80000000]),
         rnd=mixed(walk_rnd([0, 4, 1, MASK]), gen_levelled(FLOATS), gen_levelled([fbits(float(x)) for x in (0, 1, 127, 128, 129, 254, 255, 256)]))),
    Base("CParkAnalyser::GetVisitorsInPark", call_ppc(GVIP, i_args), call_host(3, i_host), edge=gvip_edges, rnd=gvip_rnd),
    Base("CParkAnalyser::NumberOfVisitorsOutsidePark", call_ppc(OUTSIDE, noarg), call_host(4, noarg_host), edge=outside_edges, rnd=outside_rnd),
    walk_test("CParkAnalyser::NumberOfVisitorsNeedingToilet", TOILET, 5, u8_args, u8_host, BYTE_LEVELS),
    walk_test("CParkAnalyser::NumberOfHungryVisitors", HUNGRY, 6, u8_args, u8_host, BYTE_LEVELS),
    walk_test("CParkAnalyser::NumberOfThirstyVisitors", THIRSTY, 7, u8_args, u8_host, BYTE_LEVELS),
    simple_test("CParkAnalyser::MisbehavingVisitorsLastMonth", LASTMONTH, 8, bool_args, bool_host, [0, 1, 2, 0x100, 0x80, 0xFF, 0x1FF00]),
    simple_test("CParkAnalyser::NumberOfVisitorsSinceBeginning", SINCE, 9, noarg, noarg_host, [0]),
    simple_test("CParkAnalyser::NumberOfVisitorsInLastMonths", INMONTHS, 10, i_args, i_host, [0, 1, 2, 11, 143, 144, -1, INT_MAX, INT_MIN]),
    walk_test("CParkAnalyser::NumberOfVisitorsWithBalloons", BALLOONS, 11, noarg, noarg_host, [0]),
    walk_test("CParkAnalyser::NumberOfVisitorsWithCostumes", COSTUMES, 12, noarg, noarg_host, [0]),
    walk_test("CParkAnalyser::PercentageOfVisitorsWithHappinessOver", OVER, 13, i_args, i_host, LEVELS),
    walk_test("CParkAnalyser::PercentageOfVisitorsWithHappinessUnder", UNDER, 14, i_args, i_host, LEVELS),
    simple_test("CParkAnalyser::GetAverageOpinionOn", AVGOP, 15, i_args, i_host, [0, 1, 7, 0x13, 0x14, -1], "b", wide=False),
    simple_test("CParkAnalyser::HaveCompleteSetOfOpinions", COMPLETE, 16, i_args, i_host, [0, 1, 7, 0x13, 0x14, -1], "b", wide=False),
    Base("CParkAnalyser::GetAverageHappinessAroundPoint", call_ppc(AROUND, around_args), call_host(17, around_host), edge=around_edges, rnd=around_case),
]
