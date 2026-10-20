"""Differential tests for unit w152, the park analyser's shop and sideshow superlatives (src/ParkAnalyser_shops.cpp). See difftest.py and tpwtest.py.

The twelve CParkAnalyser::GetShopWith{Best,Worst}{Value,Satisfaction} (with a ShopType, and without one), GetSideshowWith{Best,Worst}Value and
GetSideshowWith{Best,Worst}Satisfaction. Every callee is a fake that records the same event on both sides (GAME_ASSERTMSG, the world accessor and its
used-thing walk, CObject::GetDBInfo, GetObjectAge and GetCustomerSatisfaction, CHistory<unsigned int,30>::GetSumOfLast, CRideDatabase::GetRideInfo). The
list of the walk comes from queues; the age, satisfaction and sums of a thing are tables (so that the several reads of one sum agree), and the ride
records are one block per object type. __cvt_fp2unsigned (0x10237cd8) runs as the original on the PowerPC side. A case compares the answer, the result
halfword and its guard bytes, the analyser block (guards on both sides of `this`) and the whole call log. Not tested: the thing table beyond 16 entries,
object types above 3, a null ride record.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ParkShops"

MASK = 0xFFFFFFFF
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF
TABLE = 0x1034CBFC
ASSERT, GETWORLD, HEAD, NEXT = 0x1000DDE8, 0x1014205C, 0x1013ADB0, 0x1013AE08
GETDB, AGE, SAT, SUMLAST, GETRIDE = 0x10107CD4, 0x10107F50, 0x1010F9D0, 0x100F1000, 0x10154868
# The functions in the manifest's order; the first four take a ShopType.
ADDRS = [0x100EAA24, 0x100EAC7C, 0x100EAED4, 0x100EB0B4, 0x100EB294, 0x100EB504, 0x100EB778, 0x100EB970,
         0x100EBB68, 0x100EBDBC, 0x100EC014, 0x100EC228]
NAMES = ["GetShopWithBestValue", "GetShopWithWorstValue", "GetShopWithBestSatisfaction", "GetShopWithWorstSatisfaction",
         "GetShopWithBestValue", "GetShopWithWorstValue", "GetShopWithBestSatisfaction", "GetShopWithWorstSatisfaction",
         "GetSideshowWithBestValue", "GetSideshowWithWorstValue", "GetSideshowWithBestSatisfaction", "GetSideshowWithWorstSatisfaction"]
OWN = set(ADDRS)
L_ANALYSER, L_WORLD, L_THING, L_RIDE = 1, 3, 10, 30
SLOTS, RIDES, MAXID = 6, 4, 15
OBJ_SIZE, RIDE_SIZE = 0x448, 0x800
ANALYSER_AT, ANALYSER_BLOCK, RESULT_AT, RESULT_BLOCK = 0x40, 0xC0, 0x10, 0x30
ASSERT_TEXT = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"
NAN = float("nan")


def s32(v):
    return T.s32(v)


def setup(runner):
    T.install_std_stubs(runner.emu)


def lib_addr(lib, which):
    lib.tpw_sh_addr.restype = C.c_void_p
    lib.tpw_sh_addr.argtypes = [C.c_int32]
    return lib.tpw_sh_addr(which)


def kind_of(which):
    """0 a shop of one type, 1 any food shop, 2 a sideshow."""
    return 0 if which < 4 else (1 if which < 8 else 2)


# ------------------------------------------------------------------ the PowerPC side


def stubs_for(e, L, a, c):
    """The PowerPC fakes."""
    def assertion(emu):
        L.rec("GAME_ASSERTMSG", [s32(emu.r(3))], [emu.cstr(emu.r(4))])

    def getworld(emu):
        L.rec("GetWorld", [L.lab(emu.r(3))])
        emu.set_r(3, a["world"])

    def head(emu):
        L.rec("GetUsedThingHead", [L.lab(emu.r(4))])
        e.write(emu.r(3), struct.pack(">H", L.ret("head", 0) & 0xFFFF))

    def nxt(emu):
        L.rec("GetNextUsedThing", [L.lab(emu.r(4)), L.lab(emu.r(5))])
        e.write(emu.r(3), struct.pack(">H", L.ret("next", 0) & 0xFFFF))

    def slot_of(addr):
        return a["things"].index(addr)

    def getdb(emu):
        L.rec("GetDBInfo", [L.lab(emu.r(3))])
        emu.set_r(3, a["rides"][struct.unpack(">H", e.read(emu.r(3) + 0xE, 2))[0]])

    def age(emu):
        L.rec("GetObjectAge", [L.lab(emu.r(3))])
        emu.set_r(3, T.u32(c["things"][slot_of(emu.r(3))][2]))

    def sat(emu):
        L.rec("GetCustomerSatisfaction", [L.lab(emu.r(3))])
        emu.set_r(3, T.u32(c["things"][slot_of(emu.r(3))][3]))

    def sumlast(emu):
        this, out, count = emu.r(3), emu.r(4), emu.r(5)
        offset = 0x1A4 if this - 0x1A4 in a["things"] else 0x22C
        base = this - offset
        L.rec("GetSumOfLast", [L.lab(base), offset, s32(count)])
        e.write(out, struct.pack(">d", c["things"][slot_of(base)][4 if offset == 0x1A4 else 5]))

    def getride(emu):
        ident = (emu.r(4) >> 16) & 0xFFFF
        L.rec("GetRideInfo", [ident])
        emu.set_r(3, a["rides"][ident])

    S = CL.Stubs()
    S.real = OWN
    for addr, fn in ((ASSERT, assertion), (GETWORLD, getworld), (HEAD, head), (NEXT, nxt), (GETDB, getdb), (AGE, age), (SAT, sat),
                     (SUMLAST, sumlast), (GETRIDE, getride)):
        S.internal(addr, fn)
    return S


def queues_of(c):
    return {"head": [c["order"][0] if c["order"] else 0], "next": c["order"][1:] + [0]}


def build_ppc(e, c):
    """The PowerPC memory of a case; returns (log, stubs, addresses)."""
    L = CL.PpcLog({}, queues_of(c))
    e.cl = L
    abase = e.alloc(ANALYSER_BLOCK, b"\xa5" * ANALYSER_BLOCK)
    rbase = e.alloc(RESULT_BLOCK, b"\xa5" * RESULT_BLOCK)
    world = e.alloc(0x100, b"\xa5" * 0x100)
    things = [e.alloc(OBJ_SIZE, b"\xa5" * OBJ_SIZE) for _ in range(SLOTS)]
    rides = [e.alloc(RIDE_SIZE, b"\xa5" * RIDE_SIZE) for _ in range(RIDES)]
    L.labels.update({abase + ANALYSER_AT: L_ANALYSER, world: L_WORLD})
    L.labels.update({t: L_THING + i for i, t in enumerate(things)})
    L.labels.update({r: L_RIDE + i for i, r in enumerate(rides)})
    e.write(rbase + RESULT_AT, struct.pack(">H", c["result0"]))
    for i, (kind, otype, age, sat, s1, s2) in enumerate(c["things"]):
        e.write(things[i] + 2, bytes([kind & 0xFF]))
        e.write(things[i] + 0xE, struct.pack(">H", otype))
    for i, (shop, ui) in enumerate(c["rides"]):
        e.write(rides[i] + 0x164, struct.pack(">i", shop))
        e.write(rides[i] + 0x7A8, struct.pack(">i", ui))
    for row in range(MAXID + 1):
        slot = c["rows"].get(row, 0)
        e.write(TABLE + 0x14 * row, struct.pack(">I", things[slot - 1] if slot else 0))
    a = dict(abase=abase, rbase=rbase, world=world, things=things, rides=rides)
    return L, stubs_for(e, L, a, c), a


def observe(e, a):
    """The result halfword, the result block without it and the analyser block."""
    block = e.read(a["rbase"], RESULT_BLOCK)
    return dict(id=struct.unpack(">H", block[RESULT_AT:RESULT_AT + 2])[0], rguard=bytes(block[:RESULT_AT] + block[RESULT_AT + 2:]),
                analyser=bytes(e.read(a["abase"], ANALYSER_BLOCK)))


def query_ppc(which):
    def fn(e, c):
        L, S, a = build_ppc(e, c)
        args = [T.I(a["abase"] + ANALYSER_AT)] + ([T.I(c["type"])] if which < 4 else []) + [T.I(a["rbase"] + RESULT_AT)]
        saved = S.activate(e)
        try:
            r3, _, _ = T.call_ppc(e, ADDRS[which], args)
        finally:
            CL.Stubs.deactivate(e, saved)
        return dict(ret=s32(r3), log=L.log, **observe(e, a))
    return fn


# ------------------------------------------------------------------ the host side


def query_host(which):
    def fn(lib, c):
        lib.tpw_sh_reset.argtypes = [C.c_uint32]
        lib.tpw_sh_row.argtypes = [C.c_int32, C.c_int32]
        lib.tpw_sh_thing.argtypes = [C.c_int32] * 5
        lib.tpw_sh_sum.argtypes = [C.c_int32, C.c_int32, C.c_double]
        lib.tpw_sh_ride.argtypes = [C.c_int32] * 3
        lib.tpw_sh_call.argtypes = [C.c_int32, C.c_int32]
        lib.tpw_sh_call.restype = C.c_int32
        lib.tpw_sh_reset(c["result0"])
        for i, (kind, otype, age, sat, s1, s2) in enumerate(c["things"]):
            lib.tpw_sh_thing(i, kind, otype, age, sat)
            lib.tpw_sh_sum(i, 0, s1)
            lib.tpw_sh_sum(i, 1, s2)
        for i, (shop, ui) in enumerate(c["rides"]):
            lib.tpw_sh_ride(i, shop, ui)
        for row in range(MAXID + 1):
            lib.tpw_sh_row(row, c["rows"].get(row, 0))
        fixed = {L_ANALYSER: lib_addr(lib, 0), L_WORLD: lib_addr(lib, 1)}
        fixed.update({L_THING + i: lib_addr(lib, 10 + i) for i in range(SLOTS)})
        fixed.update({L_RIDE + i: lib_addr(lib, 20 + i) for i in range(RIDES)})
        CL.host_begin(lib, fixed, queues_of(c))
        try:
            ret = lib.tpw_sh_call(which, c["type"])
        finally:
            log = CL.host_end(lib)
        block = C.string_at(lib_addr(lib, 4), RESULT_BLOCK)
        return dict(ret=ret, log=log, id=struct.unpack("<H", block[RESULT_AT:RESULT_AT + 2])[0],
                    rguard=bytes(block[:RESULT_AT] + block[RESULT_AT + 2:]), analyser=C.string_at(lib_addr(lib, 3), ANALYSER_BLOCK))
    return fn


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

SUMS = [0.0, 0.4, 0.99, 1.0, 2.0, 3.0, 7.0, 10.0, 100.0, 12345.0, 42949672.0, 42949673.0, 2147483647.0, 2147483648.0, 4294967295.0, 4294967296.0,
        1e12, -1.0, NAN]
AGES = [29, 30, 31, -1, INT_MIN, INT_MAX, 100, 5, 1000]
SATS = [0, 1, -1, 50, 50, 51, 99, 100, 120, -30, INT_MIN, INT_MAX, INT_MIN + 1]
KINDS = [0, 1, 2, 3, 3, 3, 3, 4, 8, 0x13, 0xFF]


def passing_ride(which, r):
    """A ride record the filter of `which` accepts, as (shop type, UI type), and the ShopType argument."""
    k = kind_of(which)
    if k == 0:
        shop = r.randrange(-1, 8)
        return (shop, r.randrange(4)), shop
    if k == 1:
        return (r.choice([1, 5, 6]), r.randrange(4)), r.randrange(-1, 8)
    return (r.randrange(-1, 8), 2), r.randrange(-1, 8)


def thing(r, **kw):
    t = dict(kind=3, otype=0, age=31, sat=r.choice(SATS), s1=r.choice(SUMS[1:]), s2=r.choice(SUMS))
    t.update(kw)
    return (t["kind"], t["otype"], t["age"], t["sat"], t["s1"], t["s2"])


def make_case(which, r, things, rides=None, order=None, **kw):
    """things: the tuples of the used things (at most 6); thing i has id i + 1 unless `order` names the ids; rides: (shop, ui) per type."""
    ride, shop = passing_ride(which, r)
    rides = rides or [ride] * RIDES
    rows = {i + 1: i + 1 for i in range(len(things))}
    pad = [thing(r)] * (SLOTS - len(things))
    c = dict(which=which, type=shop, result0=r.choice([0, 0x5A5A, 0xFFFF, 0x1234]), order=list(order) if order is not None else list(range(1, len(things) + 1)),
             rows=rows, things=list(things) + pad, rides=rides)
    c.update(kw)
    return c


def random_case(which):
    def fn(r):
        n = r.choice([0, 1, 1, 2, 3, 4, 5, 6])
        ride, shop = passing_ride(which, r)
        rides = []
        for _ in range(RIDES):
            if r.random() < 0.65:
                rides.append(passing_ride(which, r)[0] if kind_of(which) != 0 else (shop, r.randrange(4)))
            else:
                rides.append((r.randrange(-1, 8), r.randrange(4)))
        ids = r.sample(range(1, MAXID + 1), n)
        slots = r.sample(range(1, SLOTS + 1), n)
        things = []
        for _ in range(SLOTS):
            things.append(thing(r, kind=r.choice(KINDS) if r.random() < 0.8 else 3, otype=r.randrange(RIDES),
                                age=r.choice(AGES) if r.random() < 0.5 else r.randint(31, 90), sat=r.choice(SATS) if r.random() < 0.7 else r.randint(-200, 200),
                                s1=r.choice(SUMS) if r.random() < 0.6 else r.choice([1.0, 5.0, 20.0, 400.0]),
                                s2=r.choice(SUMS) if r.random() < 0.6 else float(r.randint(0, 5000))))
        rows = {i: s for i, s in zip(ids, slots)}
        if r.random() < 0.2:
            rows[0] = r.randint(1, SLOTS)
        return dict(which=which, type=shop, result0=r.getrandbits(16), order=ids, rows=rows, things=things, rides=rides)
    return fn


def edges(which):
    def fn():
        r = random.Random(1000 + which)
        out = []
        out.append(make_case(which, r, []))
        for kind in (0, 1, 2, 3, 4, 8, 0x13):
            out.append(make_case(which, r, [thing(r, kind=kind)]))
        # the filter: every ride record against every ShopType argument or UI type
        for shop in range(-1, 8):
            for ui in range(4):
                for arg in ([-1, 0, 1, 2, 3, 4, 5, 6, 7] if kind_of(which) == 0 else [0]):
                    out.append(make_case(which, r, [thing(r)], rides=[(shop, ui)] * RIDES, type=arg))
        # the ages
        for age in AGES + [0, 32, 2]:
            out.append(make_case(which, r, [thing(r, age=age)]))
        # the sums of the two histories, converted to unsigned
        for s1 in SUMS:
            for s2 in SUMS:
                out.append(make_case(which, r, [thing(r, s1=s1, s2=s2, sat=r.choice(SATS))]))
        # the satisfactions and the keep rule: ties, negatives, the extremes, in every order
        for _ in range(60):
            n = r.randint(2, 6)
            sats = [r.choice(SATS) for _ in range(n)]
            out.append(make_case(which, r, [thing(r, sat=s, s1=r.choice([1.0, 3.0, 7.0]), s2=r.choice([0.0, 5.0, 100.0, 12345.0, 42949673.0, 4294967295.0])) for s in sats]))
        # the ratio above 0x7fffffff compares as a negative int; the first match is kept on ties
        for s2 in (4294967295.0, 42949673.0, 21474836.0, 21474837.0):
            for s1 in (1.0, 2.0, 100.0):
                out.append(make_case(which, r, [thing(r, s1=s1, s2=s2), thing(r, s1=1.0, s2=1.0), thing(r, s1=s1, s2=s2)]))
        # a mix of matching and rejected things in one walk; a different ride per object type
        for _ in range(20):
            rides = [passing_ride(which, r)[0], (r.randrange(-1, 8), r.randrange(4)), passing_ride(which, r)[0], (9, 9)]
            out.append(make_case(which, r, [thing(r, otype=r.randrange(4), kind=r.choice([3, 3, 1])) for _ in range(5)], rides=rides, type=rides[0][0]))
        out.append(make_case(which, r, [thing(r), thing(r)], rows={0: 1, 1: 1, 2: 2}))        # entry 0 of the thing table taken: the assertions read false
        return out
    return fn


def make(which):
    return Base("CParkAnalyser::" + NAMES[which] + " (0x%08X)" % ADDRS[which], query_ppc(which), query_host(which), edge=edges(which), rnd=random_case(which))


TESTS = [make(i) for i in range(12)]
