"""Differential tests for unit w151, the park analyser's object and ride type queries (src/game/park/analyser.cpp, src/game/Object.cpp). See difftest.py and tpwtest.py.

The 26 CParkAnalyser queries over the things (toilets, staff rooms, rides, shops), the object list, the ride database and the object controls, and
CObject::IsClosed and IsBrokenDown. Every callee is a fake that records the same event on both sides (the world accessor and its used-thing walk, the asserts,
the analyser value's Get and Set, the ride database's GetFirstRide, GetNextRide and GetRideInfo, the const GetObjectControl, GetResearchLab, the lab's
GetNumberLeftToResearch, the history's GetValue and SetAtEnd); the answers come from queues. The things, the ride records, the controls and the world's
object list head are built from one description on both sides; a case compares the answer (and the hidden result and the filth), the whole call log and the
analyser block with guard bytes around it.

Not tested (see the report): a thing mF40 that is NaN or beyond the int range (Unicorn's fctiwz does not saturate as the processor does, so the values stay
within +-2e9), a UIObjType index beyond -2 .. 6 (the history array index is logged as a byte offset), a GetRideInfo that answers NULL (the Mac dereferences it),
a cyclic object list.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ParkObjects"

MASK = 0xFFFFFFFF
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF
TABLE = 0x1034CBFC
ASSERT, GETWORLD, HEAD, NEXT, VALGET, VALSET = 0x1000DDE8, 0x1014205C, 0x1013ADB0, 0x1013AE08, 0x100EF3C8, 0x100EF400
FIRSTRIDE, NEXTRIDE, RIDEINFO, CTRL, GETLAB, LEFT = 0x101547B4, 0x10154800, 0x10154868, 0x100FA1B8, 0x1013EC58, 0x10122D3C
GETVALUE, SETATEND, RDB = 0x100F0F04, 0x100F0E38, 0x1037F6C8
ISCLOSED, ISBROKEN = 0x1010CAE8, 0x1010C878
TAIL0 = 0x1DA6F0
GUARD = 0x40
ANALYSER_SIZE = 0x21C48 + 2 * GUARD
VAL_AT = 0x1F0DC
SLOTS, RIDES, CONTROLS, MAXID = 6, 5, 3, 15
L_ANALYSER, L_VAL, L_WORLD, L_LAB, L_RDB, L_THING, L_RIDE, L_CTL = 1, 2, 3, 4, 5, 10, 20, 40
ASSERT_TEXT = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"

# name, address, host index, argument kinds ("type", "i32", "id", "bool"), result ("int", "id" (hidden result), "void")
FUNCS = [
    ("CParkAnalyser::NumberOfToiletsInPark", 0x100E8F28, 0, [], "int"),
    ("CParkAnalyser::NumberOfClosedToiletsInPark", 0x100E9070, 1, [], "int"),
    ("CParkAnalyser::NumberOfStaffRoomsInPark", 0x100E91B0, 2, [], "int"),
    ("CParkAnalyser::NumberOfClosedStaffRoomsInPark", 0x100E92DC, 3, [], "int"),
    ("CParkAnalyser::AvgFilthLevelOfToilets", 0x100E9420, 4, [], "int"),
    ("CParkAnalyser::FilthiestToilet", 0x100E9580, 5, ["filth"], "id"),
    ("CParkAnalyser::NumberOfBrokenDownRidesInPark", 0x100E9714, 6, [], "int"),
    ("CParkAnalyser::NumberOfWornRidesInPark", 0x100E9844, 7, ["level"], "int"),
    ("CParkAnalyser::NumberOfObjectTypeInPark", 0x100EA5D8, 8, ["type", "level"], "int"),
    ("CParkAnalyser::NumberOfClosedObjectTypeInPark", 0x100EA748, 9, ["type"], "int"),
    ("CParkAnalyser::GetHighestUpgradeLevelForObject", 0x100EA3B0, 10, ["id"], "int"),
    ("CParkAnalyser::TypesOfRidesAvailable", 0x100E9998, 11, ["bool"], "int"),
    ("CParkAnalyser::TypesOfUpgradesAvailable", 0x100E9A94, 12, ["bool"], "int"),
    ("CParkAnalyser::TypesOfShopsAvailable", 0x100E9B8C, 13, ["bool"], "int"),
    ("CParkAnalyser::TypesOfSideshowsAvailable", 0x100E9C80, 14, ["bool"], "int"),
    ("CParkAnalyser::TypesOfFeaturesAvailable", 0x100E9D78, 15, ["bool"], "int"),
    ("CParkAnalyser::TypesOfRidesLeftToResearch", 0x100E9E70, 16, [], "int"),
    ("CParkAnalyser::TypesOfShopsLeftToResearch", 0x100E9EEC, 17, [], "int"),
    ("CParkAnalyser::TypesOfSideshowsLeftToResearch", 0x100E9F68, 18, [], "int"),
    ("CParkAnalyser::TypesOfFeaturesLeftToResearch", 0x100E9FE8, 19, [], "int"),
    ("CParkAnalyser::AllObjectsResearchedAndPresent", 0x100EA064, 20, [], "int"),
    ("CParkAnalyser::PercentageOfTotalObjectsInPark", 0x100EA194, 21, [], "int"),
    ("CParkAnalyser::TypesOfParticularObjectTypeInPark", 0x100EA298, 22, ["type", "bool"], "int"),
    ("CParkAnalyser::NumberOfObjectInPark", 0x100EA52C, 23, ["id"], "int"),
    ("CParkAnalyser::LastOfObjectTypeResearched", 0x100EA8B8, 24, ["type", "back"], "id"),
    ("CParkAnalyser::SetLastOfObjectTypeResearched", 0x100EA968, 25, ["type", "id"], "void"),
]
OWN = {f[1] for f in FUNCS} | {ISCLOSED, ISBROKEN}


def s32(v):
    return T.s32(v)


def setup(runner):
    T.install_std_stubs(runner.emu)


def lib_addr(lib, which):
    lib.tpw_po_addr.restype = C.c_void_p
    lib.tpw_po_addr.argtypes = [C.c_int32]
    return lib.tpw_po_addr(which)


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


def f32bits(v):
    return struct.unpack(">I", struct.pack(">f", v))[0]


def stubs_for(e, L, world, ctx):
    rides, controls, lab, analyser = ctx["rides"], ctx["controls"], ctx["lab"], ctx["analyser"]

    def ride_of(key):
        v = L.ret(key, 0)
        return rides[v - 1] if 1 <= v <= RIDES else 0

    def assertion(emu):
        L.rec("GAME_ASSERTMSG", [s32(emu.r(3))], [emu.cstr(emu.r(4))])

    def getworld(emu):
        L.rec("GetWorld", [L.lab(emu.r(3))])
        emu.set_r(3, world)

    def head(emu):
        L.rec("GetUsedThingHead", [L.lab(emu.r(4))])
        e.write(emu.r(3), struct.pack(">H", L.ret("head", 0) & 0xFFFF))

    def nxt(emu):
        L.rec("GetNextUsedThing", [L.lab(emu.r(4)), L.lab(emu.r(5))])
        e.write(emu.r(3), struct.pack(">H", L.ret("next", 0) & 0xFFFF))

    def valget(emu):
        L.rec("ValGet", [L.lab(emu.r(3))])
        emu.set_r(3, T.u32(L.ret("get", 0)))

    def valset(emu):
        L.rec("ValSet", [L.lab(emu.r(3)), s32(emu.r(4))])

    def first(emu):
        L.rec("GetFirstRide", [L.lab(emu.r(3))])
        emu.set_r(3, ride_of("first"))

    def nextride(emu):
        L.rec("GetNextRide", [L.lab(emu.r(3))])
        emu.set_r(3, ride_of("nextride"))

    def rideinfo(emu):
        L.rec("GetRideInfo", [L.lab(emu.r(3)), emu.r(4) >> 16])
        emu.set_r(3, ride_of("rideinfo"))

    def control(emu):
        which = L.ret("ctl", 0) % CONTROLS
        L.rec("GetObjectControl", [emu.r(3) - world, emu.r(4) >> 16, s32(emu.r(5)), which])
        emu.set_r(3, controls[which])

    def getlab(emu):
        L.rec("GetResearchLab", [L.lab(emu.r(3))])
        emu.set_r(3, lab)

    def left(emu):
        L.rec("GetNumberLeftToResearch", [L.lab(emu.r(3)), s32(emu.r(4))])
        emu.set_r(3, T.u32(L.ret("left", 0)))

    def getvalue(emu):
        found = 1 if L.ret("gv", 0) else 0
        L.rec("HistoryGetValue", [emu.r(3) - analyser, s32(emu.r(5)), found])
        e.write(emu.r(4), struct.pack(">H", L.ret("gvid", 0) & 0xFFFF))
        emu.set_r(3, found)

    def setatend(emu):
        L.rec("HistorySetAtEnd", [emu.r(3) - analyser, emu.r(4) >> 16])

    S = CL.Stubs()
    S.real = OWN
    for addr, fn in ((ASSERT, assertion), (GETWORLD, getworld), (HEAD, head), (NEXT, nxt), (VALGET, valget), (VALSET, valset), (FIRSTRIDE, first),
                     (NEXTRIDE, nextride), (RIDEINFO, rideinfo), (CTRL, control), (GETLAB, getlab), (LEFT, left), (GETVALUE, getvalue),
                     (SETATEND, setatend)):
        S.internal(addr, fn)
    return S


def build_ppc(e, c):
    """The PowerPC memory of a case; returns (log, stubs, addresses)."""
    L = CL.PpcLog({}, {k: [T.s64(v) if hasattr(T, "s64") else v for v in vs] for k, vs in c["queues"].items()})
    e.cl = L
    block = e.alloc(ANALYSER_SIZE, b"\xa5" * ANALYSER_SIZE)
    analyser = block + GUARD
    tail = e.alloc(0x80, b"\xa5" * 0x80)
    world = tail - TAIL0
    things = [e.alloc(0x448, b"\xa5" * 0x448) for _ in range(SLOTS)]
    rides = [e.alloc(0x800, b"\xa5" * 0x800) for _ in range(RIDES)]
    controls = [e.alloc(0x20, b"\xa5" * 0x20) for _ in range(CONTROLS)]
    lab = e.alloc(0x40, b"\xa5" * 0x40)
    L.labels.update({analyser: L_ANALYSER, analyser + VAL_AT: L_VAL, world: L_WORLD, lab: L_LAB, RDB: L_RDB})
    L.labels.update({a: L_THING + i for i, a in enumerate(things)})
    L.labels.update({a: L_RIDE + i for i, a in enumerate(rides)})
    L.labels.update({a: L_CTL + i for i, a in enumerate(controls)})
    e.write(tail + 0x56, struct.pack(">H", c["objhead"] & 0xFFFF))
    for i, th in enumerate(c["things"]):
        if th is not None:
            a = things[i]
            e.write(a + 2, bytes([th["kind"] & 0xFF]))
            e.write(a + 0xC, struct.pack(">HH", th["f0c"] & 0xFFFF, th["otype"] & 0xFFFF))
            e.write(a + 0x2E, struct.pack(">H", th["flags"] & 0xFFFF))
            e.write(a + 0x40, struct.pack(">I", f32bits(th["f40"])))
            e.write(a + 0x4C, bytes([th["f4c"] & 0xFF]))
            e.write(a + 0x64, struct.pack(">I", th["f64"] & MASK))
            e.write(a + 0x198, struct.pack(">I", th["state"] & MASK))
    for i, rd in enumerate(c["rides"]):
        e.write(rides[i], struct.pack(">H", rd[0] & 0xFFFF))
        e.write(rides[i] + 0xC4, struct.pack(">I", rd[2] & MASK))
        e.write(rides[i] + 0x7A8, struct.pack(">I", rd[1] & MASK))
    for i, ct in enumerate(c["controls"]):
        e.write(controls[i] + 0x10, bytes([ct[0] & 0xFF]))
        e.write(controls[i] + 0x14, struct.pack(">iiI", ct[1], ct[2], ct[3] & MASK))
    for row in range(MAXID + 1):
        slot = c["rows"].get(row, 0)
        e.write(TABLE + 0x14 * row, struct.pack(">I", things[slot - 1] if slot else 0))
    ctx = dict(rides=rides, controls=controls, lab=lab, analyser=analyser, world=world, things=things, block=block)
    return L, stubs_for(e, L, world, ctx), ctx


def host_setup(lib, c):
    lib.tpw_po_reset()
    lib.tpw_po_objhead.argtypes = [C.c_uint32]
    lib.tpw_po_row.argtypes = [C.c_int32] * 2
    lib.tpw_po_thing.argtypes = [C.c_int32, C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_int32]
    lib.tpw_po_ride.argtypes = [C.c_int32, C.c_uint32, C.c_int32, C.c_int32]
    lib.tpw_po_control.argtypes = [C.c_int32, C.c_uint32, C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_po_objhead(c["objhead"] & 0xFFFF)
    for i, th in enumerate(c["things"]):
        if th is not None:
            lib.tpw_po_thing(i, th["kind"] & 0xFF, th["flags"] & 0xFFFF, f32bits(th["f40"]), th["f4c"] & 0xFF, th["otype"] & 0xFFFF,
                             th["f0c"] & 0xFFFF, th["f64"] & MASK, s32(th["state"]))
    for i, rd in enumerate(c["rides"]):
        lib.tpw_po_ride(i, rd[0] & 0xFFFF, s32(rd[1]), s32(rd[2]))
    for i, ct in enumerate(c["controls"]):
        lib.tpw_po_control(i, ct[0] & 0xFF, s32(ct[1]), s32(ct[2]), ct[3] & MASK)
    for row in range(MAXID + 1):
        lib.tpw_po_row(row, c["rows"].get(row, 0))
    fixed = {L_ANALYSER: lib_addr(lib, 1), L_VAL: lib_addr(lib, 6), L_WORLD: lib_addr(lib, 2), L_LAB: lib_addr(lib, 3), L_RDB: lib_addr(lib, 4)}
    fixed.update({L_THING + i: lib_addr(lib, 10 + i) for i in range(SLOTS)})
    fixed.update({L_RIDE + i: lib_addr(lib, 20 + i) for i in range(RIDES)})
    fixed.update({L_CTL + i: lib_addr(lib, 40 + i) for i in range(CONTROLS)})
    CL.host_begin(lib, fixed, {k: [v for v in vs] for k, vs in c["queues"].items()})


# ------------------------------------------------------------------ the cases

F40S = [0.0, 0.5, -0.5, 1.0, 99.0, 99.9, 100.0, 100.5, 254.9, 255.0, 255.9, 256.0, 257.0, 300.7, -1.0, -2.5, -100.0, 1e9, -1e9, 2147483520.0,
        -2147483648.0, 65536.0, 511.0, 12345.678]
LEVELS = [INT_MIN, -256, -2, -1, 0, 1, 2, 50, 99, 100, 101, 255, 256, 257, INT_MAX]
FLAGS = [0, 1, 2, 3, 4, 0x81, 0x82, 0x100, 0xFFFF, 0xFFFD, 0xFFFE]
KINDS = [3, 3, 3, 3, 3, 0, 1, 2, 4, 0x83, 0xFF]
COUNTS = [INT_MIN, -3, -1, 0, 1, 2, 5, 100, INT_MAX]


def rand_thing(r, **kw):
    t = dict(kind=r.choice(KINDS), flags=r.choice(FLAGS) if r.random() < 0.9 else r.getrandbits(16),
             f40=r.choice(F40S) if r.random() < 0.6 else r.uniform(-50, 400), f4c=r.choice([0, 1, 2, 3, 4, 5, 0x7F, 0x80, 0xFF]) if r.random() < 0.8 else r.randrange(256),
             otype=r.choice([0, 1, 2, 3, 4, 5, 7, 0xFFFF]) if r.random() < 0.9 else r.getrandbits(16), f0c=0, f64=r.choice([0, 0, 1, 2, 0x100, 0x10000, MASK, 0x80000000]),
             state=r.choice([0, 1, 2, 3, 4, -1, INT_MIN, 0x100, 0x10001]))
    t.update(kw)
    return t


def rand_arg(r, kind, c):
    if kind == "type":
        return r.choice([0, 0, 1, 1, 2, 2, 3, 3, 4, 5, -1, -2, 6])
    if kind == "level" or kind == "back":
        return r.choice(LEVELS) if r.random() < 0.8 else s32(r.getrandbits(32))
    if kind == "id":
        types = [t["otype"] for t in c["things"] if t is not None] + [r.randrange(0, 8)]
        return r.choice(types) if r.random() < 0.85 else r.getrandbits(16)
    if kind == "bool":
        return r.choice([0, 1, 1, 2, 0x100, 0x101, 0x1FF, 0xFF00, 0xFFFFFF00, MASK, 0x80, r.getrandbits(32)])
    if kind == "filth":
        return r.choice([0, 1, -1, 0x5A5A5A5A, 100, INT_MIN, INT_MAX])
    return 0


def make_world(r, **kw):
    n = r.choice([0, 1, 2, 3, 4, 5, 6, 6])
    ids = r.sample(range(1, MAXID + 1), n)
    rows = {i: s for i, s in zip(ids, r.sample(range(1, SLOTS + 1), n))}
    things = [rand_thing(r) for _ in range(SLOTS)]
    # the used-thing list and the object list are two orders over the rows' ids
    used = r.sample(ids, len(ids))
    objl = r.sample(ids, r.randint(0, len(ids)))
    for k, oid in enumerate(objl):
        things[rows[oid] - 1]["f0c"] = objl[k + 1] if k + 1 < len(objl) else 0
    for i, t in enumerate(things):
        if t["f0c"] == 0 and i + 1 not in [rows.get(x) for x in objl[:-1]]:
            t["f0c"] = r.choice([0, 0, 0xFFFF]) if (i + 1) not in [rows.get(x) for x in objl] else t["f0c"]
    if r.random() < 0.2:
        rows[0] = r.randint(1, SLOTS)
    rides = [(r.choice([0, 1, 2, 3, 4, 5, 7, 0xFFFF]) if r.random() < 0.8 else r.getrandbits(16), r.choice([0, 0, 1, 2, 3, 4, 5, 0x100, -1]),
              r.choice([-5, -1, 0, 0, 1, 2, 100, INT_MIN, INT_MAX])) for _ in range(RIDES)]
    controls = [(r.choice([0, 1, 1, 2, 0xFF, 0x80, 0x100]), r.choice([-2, -1, 0, 1, 2, 5, INT_MIN, INT_MAX]), r.choice(COUNTS), r.getrandbits(32))
                for _ in range(CONTROLS)]
    nride = r.randint(0, RIDES)
    walk = r.sample(range(1, RIDES + 1), nride)
    q = {"head": [used[0] if used else 0], "next": used[1:] + [0], "first": [walk[0] if walk else 0], "nextride": walk[1:] + [0],
         "rideinfo": [r.randint(1, RIDES) for _ in range(r.randint(1, 6))], "ctl": [r.randrange(CONTROLS) for _ in range(r.randint(1, 8))],
         "left": [r.choice([-2, -1, 0, 0, 1, 2, 7, INT_MAX, INT_MIN]) for _ in range(r.randint(1, 3))], "get": [r.choice(COUNTS)],
         "gv": [r.choice([0, 1]) for _ in range(r.randint(1, 2))], "gvid": [r.choice([0, 1, 5, 0xFFFF, r.getrandbits(16)])]}
    c = dict(things=things, rows=rows, objhead=objl[0] if objl else 0, rides=rides, controls=controls, queues=q)
    c.update(kw)
    return c


def call_ppc(e, spec, c, ctx):
    name, addr, idx, kinds, result = spec
    args = c["args"]
    regs = []
    for kind, v in zip(kinds, args):
        if kind == "id":
            regs.append(T.I((v & 0xFFFF) << 16))
        elif kind == "filth":
            regs.append(T.I(ctx["filth"]))
        elif kind == "bool":
            regs.append(T.I(v & MASK))
        else:
            regs.append(T.I(v & MASK))
    if result == "id":
        regs = [T.I(ctx["result"]), T.I(ctx["analyser"])] + regs
    else:
        regs = [T.I(ctx["analyser"])] + regs
    return T.call_ppc(e, addr, regs)


def make_ppc(spec):
    name, addr, idx, kinds, result = spec

    def run(e, c):
        L, S, ctx = build_ppc(e, c)
        if result == "id":
            ctx["result"] = e.alloc(8, b"\xa5" * 8)
        if "filth" in kinds:
            ctx["filth"] = e.alloc(8, struct.pack(">i", s32(c["args"][0])) + b"\xa5" * 4)
        saved = S.activate(e)
        try:
            r3, _, _ = call_ppc(e, spec, c, ctx)
        finally:
            CL.Stubs.deactivate(e, saved)
        out = dict(analyser=bytes(e.read(ctx["block"], ANALYSER_SIZE)), log=L.log)
        if result == "int":
            out["ret"] = s32(r3)
        if result == "id":
            out["id"] = struct.unpack(">H", bytes(e.read(ctx["result"], 2)))[0]
        if "filth" in kinds:
            out["filth"] = struct.unpack(">i", bytes(e.read(ctx["filth"], 4)))[0]
        return out
    return run


def make_host(spec):
    name, addr, idx, kinds, result = spec

    def run(lib, c):
        host_setup(lib, c)
        lib.tpw_po_run.restype = C.c_uint32
        lib.tpw_po_run.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.POINTER(C.c_int32)]
        a = list(c["args"]) + [0, 0]
        filth = C.c_int32(s32(a[0])) if "filth" in kinds else C.c_int32(0)
        try:
            if "filth" in kinds:
                ret = lib.tpw_po_run(idx, 0, 0, C.byref(filth))
            else:
                ret = lib.tpw_po_run(idx, s32(a[0] & MASK) if kinds and kinds[0] != "bool" else s32(a[0] & MASK), s32(a[1] & MASK), None)
        finally:
            log = CL.host_end(lib)
        out = dict(analyser=C.string_at(lib_addr(lib, 0), ANALYSER_SIZE), log=log)
        if result == "int":
            out["ret"] = s32(ret)
        if result == "id":
            out["id"] = ret & 0xFFFF
        if "filth" in kinds:
            out["filth"] = filth.value
        return out
    return run


def cases_for(spec):
    kinds = spec[3]

    def rnd(r):
        c = make_world(r)
        c["args"] = [rand_arg(r, k, c) for k in kinds]
        return c

    def edge():
        out = []
        r = random.Random(2000 + spec[2])
        for _ in range(60):
            c = rnd(r)
            out.append(c)
        # every filth value of a toilet, every flag, every level
        for n in range(0, 4):
            for f40s in ([0.0] * 3, [99.9, 100.0, 255.0], [256.0, -1.0, 0.5], [50.0, 50.0, 49.0], [10.0, 20.0, 20.0], [20.0, 10.0, 30.0]):
                c = make_world(r)
                rows = {i + 1: i + 1 for i in range(n)}
                c["rows"] = rows
                c["things"] = [rand_thing(r, kind=3, flags=r.choice([1, 2, 3]), f40=f40s[i % 3], f4c=r.choice([0, 3, 5]), otype=r.choice([1, 2]), f0c=(i + 2) if i + 1 < n else 0)
                               for i in range(SLOTS)]
                c["queues"]["head"] = [1 if n else 0]
                c["queues"]["next"] = [i + 2 for i in range(n - 1)] + [0]
                c["objhead"] = 1 if n else 0
                c["args"] = [rand_arg(r, k, c) for k in kinds]
                out.append(c)
        # ride database walks of each length with golden tickets and controls
        for n in range(0, RIDES + 1):
            for golden in ([0] * 5, [1] * 5, [-1] * 5, [0, 1, -1, 0, 5], [-1, 0, 0, 1, 0]):
                for num in ([0] * 3, [1] * 3, [-1, 0, 2], [0, 1, 0], [5, 0, 0]):
                    c = make_world(r)
                    c["rides"] = [(r.randrange(8), t, g) for t, g in zip([0, 0, 1, 2, 3], golden)]
                    c["controls"] = [(r.choice([0, 1]), r.choice([-1, 0, 1]), v, 0) for v in num]
                    walk = list(range(1, n + 1))
                    c["queues"]["first"] = [walk[0] if walk else 0]
                    c["queues"]["nextride"] = walk[1:] + [0]
                    c["queues"]["ctl"] = [r.randrange(3) for _ in range(6)]
                    c["args"] = [rand_arg(r, k, c) for k in kinds]
                    out.append(c)
        # the arguments, one by one
        for k in range(len(kinds)):
            pool = {"type": [-2, -1, 0, 1, 2, 3, 4, 5, 6], "level": LEVELS, "back": LEVELS, "id": [0, 1, 2, 3, 4, 5, 0xFFFF], "bool": [0, 1, 0x100, 0xFF, MASK],
                    "filth": [0, 7]}[kinds[k]]
            for v in pool:
                c = make_world(r)
                c["args"] = [rand_arg(r, kk, c) for kk in kinds]
                c["args"][k] = v
                out.append(c)
        return out
    return edge, rnd


def mk(spec):
    edge, rnd = cases_for(spec)
    return Base(spec[0], make_ppc(spec), make_host(spec), edge=edge, rnd=rnd)


# ------------------------------------------------------------------ CObject::IsClosed and IsBrokenDown (run as themselves on both sides)


def pred_case(r, **kw):
    c = make_world(r)
    c["slot"] = r.randrange(SLOTS)
    if r.random() < 0.7:
        c["things"][c["slot"]] = rand_thing(r)
    c.update(kw)
    return c


def pred_edges():
    out = []
    r = random.Random(77)
    for f64 in (0, 1, 2, 0x100, 0x10000, 0x1000000, 0x80000000, MASK):
        for state in (0, 1, 2, 3, 4, -1, INT_MIN, INT_MAX, 0x100, 0x10001, 0x1000000):
            c = pred_case(r)
            c["things"][c["slot"]] = rand_thing(r, f64=f64, state=state)
            out.append(c)
    return out


def pred_test(name, addr, hostfn):
    def ppc(e, c):
        L, S, ctx = build_ppc(e, c)
        r3, _, _ = T.call_ppc(e, addr, [T.I(ctx["things"][c["slot"]])])
        return dict(ret=s32(r3), log=L.log, analyser=bytes(e.read(ctx["block"], ANALYSER_SIZE)))

    def host(lib, c):
        host_setup(lib, c)
        fn = getattr(lib, hostfn)
        fn.restype = C.c_uint32
        fn.argtypes = [C.c_int32]
        try:
            ret = fn(c["slot"])
        finally:
            log = CL.host_end(lib)
        return dict(ret=s32(ret), log=log, analyser=C.string_at(lib_addr(lib, 0), ANALYSER_SIZE))
    return Base(name, ppc, host, edge=pred_edges, rnd=pred_case)


TESTS = [mk(spec) for spec in FUNCS] + [pred_test("CObject::IsClosed", ISCLOSED, "tpw_po_isclosed"),
                                         pred_test("CObject::IsBrokenDown", ISBROKEN, "tpw_po_isbroken")]
