"""Unit w199, against libtpw_unit_ObjectUpkeep.dylib (tests/unit_ObjectUpkeep.cpp), main app ("SimTheme Park"):
  src/game/Object.cpp  CObject::GetProspectiveDamagePerTurn 0x1010ae5c, GetProspectiveReliability 0x1010b1cc, AddWearAndTear 0x1010b244,
                         RepairFully 0x1010b5bc, CleanFully 0x1010bda8, RequestMaintenance 0x1010beb8, AssignStaffMember 0x1010c4d4, UnassignStaffMember
                         0x1010c554, GetAssignedStaffMember 0x1010c5a0, IsBeingWorkedUpon 0x1010c6d8, IsCondemned 0x1010c8bc, IsDirty 0x1010c900,
                         IsFlaggedForMaintenance 0x1010c980, RequiresUpgrade 0x1010c9c0, StartFixing 0x1010ca08, IsPlayingFixAnim 0x1010ca6c,
                         RequestBreakdown 0x1010d90c, SetWornFlag 0x1010d994
  src/game/Object.cpp CObject::IsBrokenDown 0x1010c878 (another unit; on the Mac it runs as the original, like CThing::GetThingId 0x1012e1f8)

A case is a ride (CObject, 0x448 bytes of seeded fill between two 0x40 byte guards, the fields the code reads or writes set from the case), the world's
tail block (the turn at world + 0x1da70c; the world pointer is shifted back to it), a ride record (the slots the code reads: the base slots 70 to 75 and the
seven upgrade slots of the ride's level and of the next level, every other slot zero), the sixteen ride animation sets, a pool of 32 script blocks
(RSSE_GetScriptPointer hands them out in turn, the label of the block says which call a later one belongs to), the staff rows of gThingArray and row 0,
the game type (AddWearAndTear only: its value and the guard of its function static), built twice by ONE piece of Python code: in emulator memory at the
Mac's addresses (big-endian) and in the host library's memory (little-endian). Every callee outside the 18 functions (and IsBrokenDown, GetThingId) is a
fake that records the same event on both sides (tests/calllog.py): the ride record lookup, IsRunning, IsCoaster, IsCoasterUseable, IsBOQConnected,
GetFrontOfPhysicalQueue, SetState (it stores the state), the script calls, the ride effect, the sprite spawn (with the six words of its data block), the
hoardings, the world's advisor and bank, the map's regional effectors, the two staff queries, the advisor request and its queue, the game type's
constructor, the trace and the assertion. The pointers into the ride are the label 1, the ride database 2, the advisor 3, the bank 4, the map 5, the
animation sets 20 to 35, the script blocks 40 to 71, the staff rows 80 to 84 and row 0 90; the queues answer from the case.

What is compared: the answer (the double of the damage estimate, bit for bit), the event log, the ride's fields (decoded; floats with NaN made one NaN) and
every other byte of the block, the tail block, and for AddWearAndTear the game type and the guard. Not tested: a NULL ride row where the code would
dereference it, staff ids of 0x2800 and above (outside gThingArray), a NULL this, the stale registers (the five opening traces of a non-openable ride,
the repairing and breakdown traces print a %d the original gives no number: neither side logs one; a rotation other than 0, 90, 180 and 270 in an upgrade
that spawns the effect script leaves its position in registers the original never set), floats that Unicorn's fctiwz wraps where the processor saturates
(a condition of 2^31 and more in IsDirty and CleanFully, 2^31 and more in a ride's load and in the reliability's damage: not generated).
"""
import ctypes as C
import itertools
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ObjectUpkeep"
MASK = 0xFFFFFFFF

ENTRY = dict(damage=0x1010AE5C, reliab=0x1010B1CC, wear=0x1010B244, repair=0x1010B5BC, maint=0x1010BEB8, working=0x1010C6D8, assign=0x1010C4D4,
             unassign=0x1010C554, assigned=0x1010C5A0, condemned=0x1010C8BC, dirty=0x1010C900, flagged=0x1010C980, upgrade=0x1010C9C0, fixing=0x1010CA08,
             playfix=0x1010CA6C, clean=0x1010BDA8, breakdown=0x1010D90C, worn=0x1010D994)
SIZES = dict(damage=816, reliab=60, wear=840, repair=1940, maint=808, working=368, assign=36, unassign=24, assigned=256, condemned=24, dirty=88, flagged=8,
             upgrade=24, fixing=56, playfix=76, clean=228, breakdown=88, worn=56)
OWN = set(ENTRY.values()) | {0x1012E1F8, 0x1010C878}          # the 18, CThing::GetThingId and CObject::IsBrokenDown run as the original

G_WORLD, G_MAP, G_ARRAY, G_DB, G_SETS = 0x1037EC0C, 0x1037EC08, 0x1034CBFC, 0x1037F6C8, 0x102C79A4
GAME_TYPE, GAME_GUARD = 0x102B3AA0, 0x102B3AA4
MAP_ADDR, ADVISOR_ADDR, BANK_ADDR = 0x7000, 0x5000, 0x6000
L_OBJ, L_DB, L_ADVISOR, L_BANK, L_MAP, L_SET0, L_POOL0, L_STAFF0, L_ROW0 = 1, 2, 3, 4, 5, 20, 40, 80, 90
NPOOL = 32
STAFF_IDS = [1, 2, 7, 0x100, 0x27FF]

GUARD = 0x40
OBJ = 0x448
OBJ_BLOCK = GUARD + OBJ + GUARD
TAIL = 0x40
TAIL0 = 0x1DA70C
HOST_LAYOUT = [0x448, 8, 0x1DA70C]                  # sizeof(CObject), sizeof(BalanceValue), the offset of mTurn; the fourth is the host offset of a record's slot 0
INFO_MAC = 0x4400
INFO_HOST = 0x20000

# the ride's fields the code reads or writes (offset, size, case key)
CASE_FIELDS = ((0, 2, "tid"), (4, 1, "cx"), (6, 1, "cy"), (0x0E, 2, "type"), (0x10, 4, "rot"), (0x1C, 4, "f1c"), (0x20, 4, "f20"), (0x2E, 2, "flags"),
               (0x40, 4, "f40"), (0x44, 4, "f44"), (0x4C, 1, "f4c"), (0x50, 4, "f50"), (0x54, 4, "f54"), (0x58, 1, "f58"), (0x59, 1, "f59"),
               (0x5A, 2, "f5a"), (0x5C, 4, "f5c"), (0x60, 4, "f60"), (0x64, 4, "f64"), (0x68, 2, "pl"), (0x198, 4, "state"))
OBJ_FIELDS = [(off, n) for off, n, k in CASE_FIELDS]
FLOAT_OFFSETS = (0x40, 0x44)

# the ride record's slots (RideBalanceSlot) the code reads; the upgrade slots are 93 + offset + 16 * level
S_MINCAP, S_MAXCAP, S_MINDUR, S_MAXDUR, S_MAXSPEED = 70, 71, 72, 73, 75
U_CAP, U_REDCAP, U_DUR, U_SPEED, U_REDSPEED, U_WEAR, U_COST = 6, 7, 8, 10, 11, 12, 14
UP_OFFSETS = (U_CAP, U_REDCAP, U_DUR, U_SPEED, U_REDSPEED, U_WEAR, U_COST)
UP_BASE, UP_STRIDE = 93, 16


def s32(v):
    return T.s32(v)


def put_fmt(order, size):
    return order + {1: "B", 2: "H", 4: "I"}[size]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
R = CL.recorder

FAKES = [
    (0x10154868, R("GetRideInfo", "Lw", ret=lambda e, L: L.info)),
    (0x1010CB28, R("CObject::IsRunning", "L", ret="i")),
    (0x10110CD8, R("CObject::IsCoaster", "L", ret="i")),
    (0x10052954, R("IsCoasterUseable", "L", ret="i")),
    (0x10109764, R("CObject::IsBOQConnected", "L", ret="i")),
    (0x100D6734, R("RSSE_Setvar", "Lii")),
    (0x100D6B48, R("RSSE_GetRideSpeed", "L", ret="i")),
    (0x100D6AC0, R("RSSE_SetRideSpeed", "Li")),
    (0x100D67E0, R("RSSE_UpgradeRideEffect", "i")),
    (0x100D9478, R("SCR_KillScript", "i")),
    (0x100614A0, R("ActivateHoardings", "Lu")),
    (0x100617CC, R("DeActivateHoardings", "L")),
    (0x1013EAB0, R("CWorld::GetAdvisor", "x", ret=lambda e, L: ADVISOR_ADDR)),
    (0x1013EDFC, R("CWorld::GetBankAccount", "x", ret=lambda e, L: BANK_ADDR)),
    (0x100F33B0, R("CBankAccount::MakeWithdrawal", "Lu")),
    (0x101016CC, R("CMap::AddRegionalEffector", "Liw")),
    (0x1010176C, R("CMap::RemoveRegionalEffector", "Liw")),
    (0x101269D8, R("CStaff::IsAllocatedToRide", "Lw", ret="i")),
    (0x10126AAC, R("CStaff::IsWorkingOnAllocatedObject", "Lw", ret="i")),
]
for address, fake in FAKES:
    STUBS.internal(address, fake)


def hidden_thing_id(name, this_reg=4):
    """A fake that returns a halfword id through the hidden pointer r3 (this in r4)."""
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(this_reg)),))
        e.write(e.r(3), struct.pack(">H", L.ret(name, 0) & 0xFFFF))
    return stub


def stub_script_pointer(e):
    """RSSE_GetScriptPointer: the next of the 32 blocks of the pool."""
    L = e.cl
    L.rec("RSSE_GetScriptPointer", (s32(e.r(3)),))
    e.set_r(3, L.pool + 16 * (L.pool_n % NPOOL))
    L.pool_n += 1


def stub_getvar(e):
    L = e.cl
    index = s32(e.r(4))
    L.rec("RSSE_Getvar", (L.lab(e.r(3)), index))
    e.set_r(3, L.ret("var%d" % index, 0) & MASK)


def stub_spawn(e):
    """ESP_SpawnScript(block, sprite, a, acts, x, y, z): the six words of the block, then the arguments."""
    L = e.cl
    words = struct.unpack(">6I", e.read(e.r(3), 24))
    L.rec("ESP_SpawnScript", tuple(words) + (s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), CL.fbits(e.f(1)), CL.fbits(e.f(2)), CL.fbits(e.f(3))))
    e.set_r(3, L.ret("ESP_SpawnScript", 0) & MASK)


def stub_set_state(e):
    L = e.cl
    this, state = e.r(3), s32(e.r(4))
    L.rec("CObject::SetState", (L.lab(this), state))
    e.write(this + 0x198, struct.pack(">I", state & MASK))


def stub_req_ctor(e):
    e.cl.last_request = e.r(3)
    e.cl.rec("CAdvisorRequest", (s32(e.r(4)), (e.r(5) >> 16) & 0xFFFF, e.r(6) & MASK, s32(e.r(7)), e.r(8) & 0xFF, s32(e.r(9))))


def stub_queue(e):
    e.cl.rec("QueueRequest", (e.cl.lab(e.r(3)), 1 if e.r(4) == e.cl.last_request else 0))


def game_type_ctor(e):
    e.cl.rec("CGameType", [1])
    e.write(e.r(3), struct.pack(">I", e.cl.ctortype & MASK))


def count_ints(text):
    # the repairing and the breakdown traces have a %d the original gives no number for (a stale register): neither side logs one
    if "repairing fully" in text or "requested breakdown" in text:
        return 0
    return text.count("%d")


def stub_assert(e):
    text = e.cstr(e.r(4))[:159]
    ints = [s32(e.r(3))] + [e.r(5 + i) & MASK for i in range(count_ints(text))]
    e.cl.rec("GAME_ASSERTMSG", tuple(ints), (text,))


def stub_ctrace(e):
    text = e.cstr(e.r(4))[:159]
    ints = [e.r(3) & MASK] + [e.r(5 + i) & MASK for i in range(count_ints(text))]
    e.cl.rec("CTRACE", tuple(ints), (text,))


STUBS.internal(0x10109144, hidden_thing_id("CObject::GetFrontOfPhysicalQueue"))
STUBS.internal(0x100D668C, stub_script_pointer)
STUBS.internal(0x100D6C58, stub_getvar)
STUBS.internal(0x100DA18C, stub_spawn)
STUBS.internal(0x1010DC94, stub_set_state)
STUBS.internal(0x1000DEB4, stub_req_ctor)
STUBS.internal(0x1000AE48, stub_queue)
STUBS.internal(0x1016F5D8, game_type_ctor)
STUBS.internal(0x1000DDE8, stub_assert)
STUBS.internal(0x1006FCBC, stub_ctrace)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def put(self, a, size, v):
        self.write(a, struct.pack(put_fmt(self.order, size), v & ((1 << (8 * size)) - 1)))

    def get(self, a, size):
        return struct.unpack(put_fmt(self.order, size), self.read(a, size))[0]


class Ppc(Side):
    order = ">"
    info_size = INFO_MAC

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.last_request = None
        self.L.ctortype = case["ctortype"]
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def label(self, address, n):
        self.L.labels[address] = n

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def row(self, row, ptr):
        self.put(G_ARRAY + 0x14 * row, 4, ptr)

    def setset(self, index, ptr):
        self.put(G_SETS + 4 * index, 4, ptr)

    def info_slot(self, info, slot):
        return info + 0x0C + 4 * slot

    def db(self):
        return G_DB

    def pool_block(self):
        return self.alloc(NPOOL * 16)

    def setup(self, c, tail, info, pool):
        self.L.info = info
        self.L.pool = pool
        self.L.pool_n = 0
        self.put(G_WORLD, 4, (tail - TAIL0) & MASK)
        self.put(G_MAP, 4, MAP_ADDR)
        if c["op"] == "wear":
            if c["first"]:
                self.write(GAME_GUARD, b"\0")                       # the first call runs the constructor
            else:
                self.put(GAME_TYPE, 4, c["gtype"])
                self.write(GAME_GUARD, bytes([c["gguard"]]))

    def game(self):
        return {"type": self.get(GAME_TYPE, 4), "guard": 1 if self.read(GAME_GUARD, 1)[0] else 0}

    def call(self, entry, args):
        return self.call_full(entry, args)[0] & MASK

    def call_full(self, entry, args):
        return T.call_ppc(self.e, entry, [a & MASK if isinstance(a, int) else a for a in args], max_insns=20_000_000)


class Host(Side):
    order = "<"
    info_size = INFO_HOST

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_oup_alloc(n)

    def label(self, address, n):
        self.lib.tpw_cl_label(n, address)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def row(self, row, ptr):
        self.lib.tpw_oup_setrow(row, ptr)

    def setset(self, index, ptr):
        self.lib.tpw_oup_setset(index, ptr)

    def info_slot(self, info, slot):
        return info + self.lib.infooff + 8 * slot

    def db(self):
        return self.lib.tpw_oup_addr(b"db")

    def pool_block(self):
        return self.lib.tpw_oup_addr(b"pool")

    def setup(self, c, tail, info, pool):
        self.lib.tpw_oup_setup(tail, info, s32(c["ctortype"]))
        if c["op"] == "wear" and not c["first"]:
            self.lib.tpw_oup_mode(s32(c["gtype"]))

    def game(self):
        return {"type": self.lib.tpw_oup_type() & MASK, "guard": 1}


INT_OPS = ("reliab", "maint", "working", "condemned", "dirty", "flagged", "upgrade", "playfix")
VOID_OPS = ("wear", "repair", "assign", "unassign", "fixing", "clean", "breakdown", "worn")


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, i32, u32, f32, f64 = C.c_uint64, C.c_int32, C.c_uint32, C.c_float, C.c_double
    sigs = dict(addr=(u64, [C.c_char_p]), alloc=(u64, [u32]), setup=(None, [u64, u64, i32]), setrow=(None, [i32, u64]), setset=(None, [i32, u64]),
                mode=(None, [i32]), type=(i32, []),
                damage=(f64, [u64, f32, f32, f32, u32]), reliab=(i32, [u64, f32, f32, f32]), wear=(None, [u64]), repair=(None, [u64, u32]),
                maint=(i32, [u64, i32]), working=(i32, [u64]), assign=(None, [u64, u32]), unassign=(None, [u64]), assigned=(u32, [u64]),
                condemned=(i32, [u64]), dirty=(i32, [u64]), flagged=(i32, [u64]), upgrade=(i32, [u64]), fixing=(None, [u64]), playfix=(i32, [u64]),
                clean=(None, [u64]), breakdown=(None, [u64]), worn=(None, [u64]))
    for name, (res, args) in sigs.items():
        fn = getattr(lib, "tpw_oup_" + name)
        fn.restype, fn.argtypes = res, args
    lib.tpw_oup_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 16)()
    n = lib.tpw_oup_layout(out)
    assert list(out)[:3] == HOST_LAYOUT, list(out)[:n]
    assert n == 4 and 0 < out[3] < 0x1000, list(out)[:n]
    lib.infooff = out[3]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def fill(seed, n):
    r = random.Random(seed)
    return bytes(r.getrandbits(8) for _ in range(n))


def fl(bits):
    return T.bits_f32(bits & MASK)


def build(s, c):
    ob = s.alloc(OBJ_BLOCK)
    s.write(ob, fill(c["seed"], OBJ_BLOCK))
    eb = ob + GUARD
    s.eb = eb
    fr = random.Random(c["seed"] + 5)
    for off, n in OBJ_FIELDS:      # the observed fields get the same VALUE on both sides (the fill is bytes)
        s.put(eb + off, n, fr.getrandbits(8 * n))
    tail = s.alloc(TAIL)
    s.write(tail, fill(c["seed"] + 1, TAIL))
    info = s.alloc(s.info_size)
    s.write(info, bytes(s.info_size))
    for slot, v in c["rec"].items():
        s.put(s.info_slot(info, slot), 4, v)
    pool = s.pool_block()
    for i in range(NPOOL):
        s.label(pool + 16 * i, L_POOL0 + i)
    for i in range(16):
        b = s.alloc(0x10)
        s.write(b, fill(c["seed"] + 20 + i, 0x10))
        s.label(b, L_SET0 + i)
        s.setset(i, b)
    for i, sid in enumerate(STAFF_IDS):
        b = s.alloc(0x40)
        s.write(b, fill(c["seed"] + 40 + i, 0x40))
        s.label(b, L_STAFF0 + i)
        s.row(sid, b)
    spare = s.alloc(0x40)
    s.write(spare, fill(c["seed"] + 60, 0x40))
    s.label(spare, L_ROW0)
    s.row(0, spare if c["row0"] else 0)
    s.label(eb, L_OBJ)
    s.label(s.db(), L_DB)
    s.label(ADVISOR_ADDR, L_ADVISOR)
    s.label(BANK_ADDR, L_BANK)
    s.label(MAP_ADDR, L_MAP)
    s.put(tail, 4, c["turn"])
    s.tail = tail
    s.setup(c, tail, info, pool)
    for off, size, key in CASE_FIELDS:
        s.put(eb + off, size, c[key])


def snapshot(s, base, size, fields, skew=GUARD):
    data = bytearray(s.read(base, size))
    values = []
    for off, n in fields:
        i = skew + off
        v = s.get(base + i, n)
        if n == 4 and off in FLOAT_OFFSETS and skew and (v & 0x7F800000) == 0x7F800000 and (v & 0x7FFFFF):
            v = 0x7FC00000          # one NaN
        values.append(v)
        data[i:i + n] = bytes(n)
    return values, bytes(data)


def execute(s, c):
    op, eb = c["op"], s.eb
    floats = (fl(c["speed"]), fl(c["cap"]), fl(c["dur"]))
    if isinstance(s, Ppc):
        entry = ENTRY[op]
        if op == "damage":
            return s.call_full(entry, [eb] + [T.F(x) for x in floats] + [c["use"]])[2]
        if op == "reliab":
            return s32(s.call_full(entry, [eb] + [T.F(x) for x in floats])[0])
        if op in ("repair", "assign"):
            s.call(entry, [eb, c["arg"] << 16])
            return 0
        if op == "maint":
            return s32(s.call(entry, [eb, c["arg"]]))
        if op == "assigned":
            buf = s.alloc(16)
            s.write(buf, b"\xaa" * 16)
            s.call(entry, [buf, eb])
            return s.get(buf, 2)
        if op in INT_OPS:
            return s32(s.call(entry, [eb]))
        s.call(entry, [eb])
        return 0
    f = getattr(s.lib, "tpw_oup_" + op)
    if op == "damage":
        return f(eb, floats[0], floats[1], floats[2], c["use"] & MASK)
    if op == "reliab":
        return f(eb, floats[0], floats[1], floats[2])
    if op in ("repair", "assign"):
        f(eb, c["arg"] & 0xFFFF)
        return 0
    if op == "maint":
        return f(eb, s32(c["arg"]))
    if op == "assigned":
        return f(eb) & 0xFFFF
    if op in INT_OPS:
        return f(eb)
    f(eb)
    return 0


def observe(s, c, answer):
    out = {"answer": answer, "log": s.log()}
    out["obj"] = snapshot(s, s.eb - GUARD, OBJ_BLOCK, OBJ_FIELDS)
    out["tail"] = snapshot(s, s.tail, TAIL, [(0, 4)], 0)
    if c["op"] == "wear":
        out["game"] = s.game()
    return out


def run(s, c):
    try:
        build(s, c)
        return observe(s, c, execute(s, c))
    finally:
        s.done()


def ppc(e, c):
    return run(Ppc(e, c), c)


def host(lib, c):
    return run(Host(lib, c), c)

# ---------------------------------------------------------------------------------------------------------------- case generators

U32 = [0, 1, 2, 3, 0xF, 0x10, 99, 100, 101, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, MASK, 15000]
STATES = [0, 0, 1, 2, 2, 3, 4, 4, 5, 7, 0xFFFFFFFF, 0x80000000]
LEVELS = [0, 1, 2, 3, 0, 1, 2, 4, 0x7F, 0x80, 0xFF]
FLAGS = [0, 1, 8, 9, 0x100, 0x101, 0x108, 0x109, 0xFFFF, 0xFFFE]
ROTS = [0, 0x5A, 0xB4, 0x10E, 1, 90, 0x168, MASK, 0x80000000]
ROT_OK = [0, 0x5A, 0xB4, 0x10E]
USE = [0, 1, 0xFF, 0x100, 0x101, 0x1FF]
ONS = [0, 1, 2, MASK, 0x80000000, 0x100, 0x7FFFFFFF]
FRONTS = [0, 1, 2, 3, 0x7F, 0x80, 0x81, 0x4000, 0x7FFF, 0x8000, 0xFFFF]
LOADS = [0, 1, 2, 3, 5, 9, 10, 11, 15, 20, 25, 30, 50, 99, 100, 101, 255, 1000, 0x7FFFFF80, 0x7FFFFFBF, MASK, 0xFFFFFFFB, 0x80000000]
VARS = [0, 1, 2, 3, 5, 10, 20, 30, 100, 255, 300, 1000, MASK, 0xFFFFFFFB, 0x7FFFFFFF, 0x80000000]
SPEEDS = [0, 1, 2, 5, 10, 20, 30, 50, 100, 255, 300, 1000, MASK, 0xFFFFFFFB, 0x7FFFFFFF, 0x80000000]
FLOATS = [0.0, -0.0, 100.0, 99.6, 50.0, 25.0, 24.9999, 24.5, 24.0, 25.5, 20.0, 19.99, 10.0, 9.99, 10.5, 12.5, 0.4, 1.0, 2.0, 5.0, 0.5, 255.0, 255.9, 256.0,
          280.5, -0.5, -1.0, -100.0, 1e10, -1e10, float("inf"), float("-inf"), float("nan"), 2147483647.0, 1000.0]
SAFE_FLOATS = [0.0, -0.0, 1.0, 2.0, 5.0, 10.0, 20.0, 30.0, 50.0, 99.5, 100.0, 255.0, 300.0, 1000.0, 10000.0, -1.0, -5.0, -100.0, 0.4, 12.5, float("nan")]
WEAR_F40 = [100.0, 99.5, 50.0, 25.0, 21.0, 20.5, 20.0, 19.99, 19.0, 15.0, 12.0, 11.5, 10.5, 10.0, 9.99, 9.0, 5.0, 2.0, 1.0, 0.5, 0.0, -0.0, -1.0, 1e10,
            float("inf"), float("-inf"), float("nan"), 100.4, 255.5, 300.0]
POOL = [0, 1, 2, 3, 5, 10, 20, 30, 100, 255, 256, 257, 300, MASK, 0xFFFFFFFB, 0x7FFFFFFF, 0x80000000]
SMALL = [1, 2, 3, 5, 10, 20, 30, 100, 255, 300, 1000]
WEARS = [0, 1, 2, 3, 5, 10, 20, 30, 50, 100, 1000, MASK, 0xFFFFFFFB]
WEARS_BIG = WEARS + [0x7FFFFFFF, 0x80000000]


def u32(r):
    return r.choice(U32 + [r.getrandbits(32), r.getrandbits(16), r.getrandbits(8)])


def qlist(r, values):
    return [r.choice(values) & MASK for _ in range(r.randrange(1, 4))]


def canon(bits):
    """A float's bits with every NaN made the one quiet NaN (the payload of a computed NaN is not tested)."""
    if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF):
        return 0x7FC00000
    return bits & MASK


def fbits(r, pool=None, rand=True):
    x = r.choice((pool or FLOATS) + ([None, None] if rand else []))
    return canon(r.getrandbits(32)) if x is None else T.f32_bits(x)


def safe_f40(bits):
    """Unicorn's fctiwz gives 0x80000000 where the processor saturates, so a float of 2^31 and more (and +inf) is not tested in IsDirty and CleanFully."""
    x = struct.unpack("<f", struct.pack("<I", bits & MASK))[0]
    if x >= 2147483648.0:
        return T.f32_bits(float((bits >> 8) % 100000) - 500.0)
    return bits & MASK


def base(r, op):
    c = {"op": op, "seed": r.getrandbits(32), "first": False,
         "turn": r.choice([0, 64, 128, 192, 0xFFFFFFC0, 0x80000000, 1, 63, 65, MASK, u32(r), r.getrandbits(32) & ~63, r.getrandbits(32) & ~63]),
         "tid": r.choice([0, 1, 2, 0x100, 0x2FF, 0xFFFF, r.getrandbits(16)]),
         "cx": r.choice([0, 1, 5, 0x7F, 0x80, 0xFF, r.getrandbits(8)]), "cy": r.choice([0, 1, 5, 0x7F, 0x80, 0xFF, r.getrandbits(8)]),
         "type": r.choice([0, 1, 5, 0x7F, 0xFFFF, r.getrandbits(16)]), "rot": r.choice(ROTS), "f1c": r.choice([0, 1, 5, 15]),
         "f20": r.choice([0, 1, 5, 0x100, MASK, 0x80000000, 0x7FFFFFFF, r.getrandbits(32)]), "flags": r.choice(FLAGS + [r.getrandbits(16)]),
         "f40": fbits(r), "f44": fbits(r), "f4c": r.choice(LEVELS), "f50": r.choice([0, 0, 1, 5, MASK, u32(r)]), "f54": u32(r),
         "f58": r.getrandbits(8), "f59": r.getrandbits(8), "f5a": r.choice([0, 0] + STAFF_IDS), "f5c": u32(r),
         "f60": r.choice([0, 0, 1, 2, MASK]), "f64": u32(r), "pl": r.choice([0, 1, 5, 0xFFFF, r.getrandbits(16)]), "state": r.choice(STATES),
         "row0": r.random() < 0.25, "arg": r.choice([0, 1, 2, 0x100, 0xFFFF, r.getrandbits(16)]), "use": r.choice(USE),
         "speed": fbits(r), "cap": fbits(r), "dur": fbits(r), "bound": None,
         "gtype": r.choice([0, 0, 0, 1, 2, MASK, 0x80000000, 0x7FFFFFFF, 3]), "gguard": r.choice([1, 0x7F, 0x80, 0xFF]),
         "ctortype": r.choice([0, 1, 2, MASK])}
    c["queues"] = {
        "CObject::IsRunning": qlist(r, [0, 1, 1]), "CObject::IsCoaster": qlist(r, [0, 0, 1, 5]), "IsCoasterUseable": qlist(r, [0, 1, 1, MASK]),
        "CObject::IsBOQConnected": qlist(r, [0, 1, 1, 3]), "CObject::GetFrontOfPhysicalQueue": qlist(r, FRONTS) + qlist(r, FRONTS),
        "var2": qlist(r, VARS), "var3": qlist(r, VARS), "var4": qlist(r, [0, 1, 5, MASK]), "var5": qlist(r, LOADS), "var7": qlist(r, [0, 1, 2, 5, MASK, 0x80000000]),
        "RSSE_GetRideSpeed": qlist(r, SPEEDS), "ESP_SpawnScript": [r.choice([0, 1, 7, 0x1234, MASK])],
        "CStaff::IsAllocatedToRide": qlist(r, [0, 1, 1, 5]), "CStaff::IsWorkingOnAllocatedObject": qlist(r, [0, 1, 2, MASK]),
    }
    return c


def make_rec(r, c):
    """The ride record: the five base slots and the seven upgrade slots of the ride's level and of the next one."""
    safe = c["op"] == "reliab"
    rv = lambda: r.choice(POOL + [r.randrange(0, 40), r.getrandbits(12), r.getrandbits(32)]) & MASK
    rec = {S_MINCAP: rv(), S_MAXCAP: rv(), S_MINDUR: rv(), S_MAXDUR: rv(), S_MAXSPEED: rv()}
    if safe:
        rec[S_MAXCAP] = r.choice(SMALL + [MASK, 0xFFFFFFFB, 0x7FFFFFFF, 0x80000000])
        rec[S_MAXSPEED] = r.choice(SMALL + [MASK, 0xFFFFFFFB, 0x7FFFFFFF, 0x80000000])
    for lvl in (c["f4c"] & 0xFF, (c["f4c"] + 1) & 0xFF):
        for off in UP_OFFSETS:
            rec[UP_BASE + UP_STRIDE * lvl + off] = (r.choice(WEARS if safe else WEARS_BIG) if off == U_WEAR else rv()) & MASK
    return rec


def finalize(r, c):
    op = c["op"]
    if op in ("dirty", "clean"):
        c["f40"] = safe_f40(c["f40"])
    if op == "repair" and c["state"] == 2 and c["f4c"] in (0, 1) and c["rot"] not in ROT_OK:
        c["rot"] = r.choice(ROT_OK)        # the position of the effect script is stale registers in the original for the other rotations
    rec = make_rec(r, c)
    lvl0, lvl1 = c["f4c"] & 0xFF, (c["f4c"] + 1) & 0xFF
    for off, v in c.get("rl0", {}).items():
        rec[UP_BASE + UP_STRIDE * lvl0 + off] = v & MASK
    for off, v in c.get("rl", {}).items():
        rec[UP_BASE + UP_STRIDE * lvl1 + off] = v & MASK
    for slot, v in c.get("rb", {}).items():
        rec[slot] = v & MASK
    kind = c["bound"]
    d = c["bd"] if "bd" in c else r.choice([-1, 0, 1])
    if kind == "speed":
        red = s32(rec[UP_BASE + UP_STRIDE * lvl0 + U_REDSPEED])
        if 0 < red < (1 << 24):
            if op == "wear":
                c["queues"]["RSSE_GetRideSpeed"] = [(red + d) & MASK]
            else:
                c["speed"] = T.f32_bits(float(red)) + d      # the float next below or above
    elif kind == "load":
        red = s32(rec[UP_BASE + UP_STRIDE * lvl0 + U_REDCAP])
        if 0 < red < 100000:
            c["queues"]["var5"] = [(10 * red + d) & MASK]
            rec[S_MAXCAP] = 10000000
    c["rec"] = rec
    return c


def set_axis(c, k, v):
    if k.startswith("f:"):
        c[k[2:]] = v
    elif k.startswith("rl0:"):
        c.setdefault("rl0", {})[int(k[4:])] = v
    elif k.startswith("rl:"):
        c.setdefault("rl", {})[int(k[3:])] = v
    elif k.startswith("rb:"):
        c.setdefault("rb", {})[int(k[3:])] = v
    else:
        c["queues"][k] = list(v) if isinstance(v, (list, tuple)) else [v]


def grid(seed, op, axes, extra=None):
    """Edge cases: the product of the axes ("f:field" a case field, "rl0:n" / "rl:n" the upgrade slot offset n of the ride's level / the next level, "rb:n" a base slot,
    else a queue name) over a random base case."""
    def edges():
        r = random.Random(seed)
        keys = list(axes)
        for combo in itertools.product(*[axes[k] for k in keys]):
            c = base(r, op)
            for k, v in zip(keys, combo):
                set_axis(c, k, v)
            if extra:
                extra(r, c)
            yield finalize(r, c)
    return edges


def chain(*gens):
    def edges():
        for g in gens:
            yield from g()
    return edges


def make_random(op, tweak=None):
    def rnd(r):
        c = base(r, op)
        if tweak:
            tweak(r, c)
        return finalize(r, c)
    return rnd


def damage_tweak(r, c):
    c["bound"] = r.choice([None, None, "speed", "load"])
    if r.random() < 0.5:
        c["speed"] = fbits(r, [0.0, 1.0, 5.0, 10.0, 20.0, 50.0, 100.0, 255.0, 300.0, -1.0, float("nan")])
    if r.random() < 0.3:
        c["cap"] = fbits(r, [0.0, 1.0, 5.0, 10.0, 20.0, 50.0, 100.0, 300.0, -1.0])


def reliab_tweak(r, c):
    c["speed"], c["cap"], c["dur"] = fbits(r, SAFE_FLOATS, False), fbits(r, SAFE_FLOATS, False), fbits(r, SAFE_FLOATS, False)
    c["bound"] = r.choice([None, None, "speed", "load"])


def reliab_floats(r, c):
    c["speed"], c["cap"], c["dur"] = fbits(r, SAFE_FLOATS, False), fbits(r, SAFE_FLOATS, False), fbits(r, SAFE_FLOATS, False)


def wear_tweak(r, c):
    c["turn"] = r.choice([0, 64, 128, 192, 0xFFFFFFC0, 64 * r.randrange(0, 1000), 63, 65, 1, u32(r)])
    c["flags"] = r.choice([0, 0x100, 0x100, 1, 0x101, 8, r.getrandbits(16)])
    c["f40"] = fbits(r, WEAR_F40, False)
    c["f44"] = fbits(r, [100.0, 50.0, 0.5, 0.0, 0.01, -1.0, 99.99, 100.5, float("inf"), float("nan")])
    c["queues"]["CObject::IsRunning"] = qlist(r, [0, 1, 1, 1])
    c["queues"]["var5"] = qlist(r, [0, 1, 3, 10, 40, 100, 0xFFFFFFFB])
    c["bound"] = r.choice([None, None, "speed", "load"])
    c["gtype"] = r.choice([0, 0, 0, 0, 1, 2, MASK, 3, 0x80000000])


def repair_tweak(r, c):
    c["state"] = r.choice([2, 2, 2, 0, 1, 3, 4, 5])
    c["f4c"] = r.choice([0, 1, 2, 3, 0, 1, 2, 0xFF, 4, 0x7F])
    c["f50"] = r.choice([0, 0, 1, 5])
    c["flags"] = r.choice([0, 8, 0x100, 9, 1, 0x108])


def maint_tweak(r, c):
    c["arg"] = r.choice(ONS)
    c["f60"] = r.choice([0, 0, 1, 2, MASK])
    c["state"] = r.choice([0, 0, 1, 2, 3, 4])
    c["f64"] = r.choice([0, 1, 5, MASK])


def staff_tweak(r, c):
    c["f5a"] = r.choice([0, 0] + STAFF_IDS)
    c["f5c"] = r.choice([0, 1, 99, 100, 1000, MASK, 0xFFFFFFC0, 0xFFFFFF9C, 0xFFFFFF9B, 0x7FFFFFFF, 0x80000000, r.getrandbits(32)])
    if r.random() < 0.6:
        c["turn"] = (c["f5c"] + 100 + r.choice([-2, -1, 0, 1, 2, 50, -50, 0x7FFFFFFF, 0x80000000])) & MASK


def flags_tweak(r, c):
    c["flags"] = r.choice(FLAGS)
    c["f40"] = fbits(r, [100.0, 50.0, 25.0, 24.9999, 24.5, 24.0, 0.0, -0.0, 1.0, 255.0, 256.0, 280.5, -1.0, -0.5, float("nan"), float("-inf"), -1e10, 0.4])


def staff_grid(seed, op):
    return grid(seed, op, {"f:f5a": [0, 1, 7], "f:f5c": [1000], "f:turn": [1099, 1100, 1101, 1102, 0, 0x80000000], "f:row0": [False, True],
                           "CStaff::IsAllocatedToRide": [0, 1], "CStaff::IsWorkingOnAllocatedObject": [0, 1, 5]})


def wear_first():
    r = random.Random(7)
    c = base(r, "wear")
    c.update(first=True, gguard=0, ctortype=0, gtype=0, turn=64, flags=0x100, f4c=1, f40=T.f32_bits(50.0), f44=T.f32_bits(80.0), bound=None)
    c["queues"].update({"CObject::IsRunning": [1], "var5": [10], "RSSE_GetRideSpeed": [50], "var2": [10], "var3": [20]})
    c["rl0"] = {U_WEAR: 5, U_REDSPEED: 20, U_REDCAP: 2}
    c["rb"] = {S_MAXCAP: 100, S_MAXSPEED: 100}
    yield finalize(r, c)


def fix_ok_tests(op, seed, **extra_axes):
    axes = {"CObject::IsBOQConnected": [0, 1], "CObject::IsCoaster": [0, 1], "IsCoasterUseable": [0, 1]}
    axes.update(extra_axes)
    return grid(seed, op, axes)


damage_edges = chain(
    grid(101, "damage", {"f:f4c": [0, 1, 2, 3], "f:use": [0, 1, 0xFF, 0x100], "var5": [[0], [3], [25], [1000], [0xFFFFFFFB]]}),
    grid(102, "damage", {"f:bound": ["speed", "load"], "f:f4c": [0, 1, 2, 3], "f:use": [0, 1], "f:bd": [-1, 0, 1]}),
    grid(103, "damage", {"var5": [[0xFFFFFFFB], [0], [1], [7], [31], [0x7FFFFF80]], "rb:71": [0, 1, 5, 30, MASK], "f:use": [0, 1]}))
reliab_edges = chain(
    grid(111, "reliab", {"f:f4c": [0, 1, 2, 3], "var5": [[0], [7], [40], [0xFFFFFFFB]], "f:speed": [T.f32_bits(0.0), T.f32_bits(30.0), T.f32_bits(300.0)],
                         "f:cap": [T.f32_bits(0.0), T.f32_bits(10.0), T.f32_bits(1000.0)]}),
    grid(112, "reliab", {"f:bound": ["speed", "load"], "f:f4c": [0, 1, 2, 3], "f:bd": [-1, 0, 1]}, extra=reliab_floats))
wear_edges = chain(
    wear_first,
    grid(121, "wear", {"CObject::IsRunning": [0, 1], "f:flags": [0, 0x100], "f:turn": [0, 64, 1, 63, 128], "f:gtype": [0, 1, 2, MASK], "f:gguard": [1, 0xFF]},
         extra=lambda r, c: c["queues"].update({"var5": [10], "RSSE_GetRideSpeed": [30]})),
    grid(122, "wear", {"f:f40": [T.f32_bits(x) for x in (100.0, 50.0, 21.0, 20.0, 19.99, 12.0, 10.0, 9.99, 5.0, 0.0, -1.0, float("nan"), float("inf"))],
                       "var5": [[0], [1], [40]], "f:f44": [T.f32_bits(x) for x in (100.0, 50.0, 0.5, float("nan"))], "f:turn": [64], "f:flags": [0x100],
                       "f:gtype": [0]}),
    grid(123, "wear", {"f:bound": ["speed", "load"], "f:f4c": [0, 1, 2, 3], "f:bd": [-1, 0, 1], "f:turn": [128], "f:flags": [0x100], "f:gtype": [0]}))
repair_edges = chain(
    grid(131, "repair", {"f:state": [0, 1, 2, 3, 4], "f:f4c": [0, 1, 2, 3, 0xFF], "f:flags": [0, 8], "f:rot": [0], "f:f50": [0, 5]}),
    grid(132, "repair", {"f:state": [2], "f:f4c": [0, 1], "f:rot": [0, 0x5A, 0xB4, 0x10E],
                         "CObject::GetFrontOfPhysicalQueue": [[1, 1], [0, 0], [0x80, 0x81], [0xFFFF, 2], [0x4000, 0x7FFF]], "f:flags": [0, 8]}),
    grid(133, "repair", {"f:state": [0, 1, 3, 4, 2], "CObject::IsBOQConnected": [0, 1], "CObject::IsCoaster": [0, 1], "IsCoasterUseable": [0, 1]}),
    grid(134, "repair", {"f:state": [2], "f:f4c": [0, 1, 2], "rl:6": [0, 5, 300, 257, MASK], "rl:8": [0, 7, 300, MASK], "rl:10": [0, 9, MASK]}),
    grid(135, "repair", {"f:state": [2], "f:f4c": [0], "rb:70": [0, 3, 10], "rb:71": [0, 5, 50, 0xFFFFFFFD], "rl:6": [1, 2, 4, 7, 60, 300]}),
    grid(136, "repair", {"f:state": [2], "f:f4c": [1], "rb:72": [0, 3, 10], "rb:73": [0, 5, 50, 0xFFFFFFFD], "rl:8": [1, 2, 4, 7, 60, 300]}))
maint_edges = chain(
    grid(141, "maint", {"f:arg": [0, 1, 2, MASK], "f:f60": [0, 1, 2], "f:state": [0, 1, 2, 4], "CObject::IsBOQConnected": [0, 1]}),
    grid(142, "maint", {"f:arg": [0], "f:f60": [1], "f:state": [0], "CObject::IsBOQConnected": [1], "CObject::IsCoaster": [0, 1], "IsCoasterUseable": [0, 1],
                        "f:f64": [0, 1]}),
    grid(143, "maint", {"f:arg": [1, 0], "f:f60": [0, 1], "f:f20": [0, 5], "f:f1c": [0, 15], "f:pl": [0, 0x1234]}))
working_edges = chain(staff_grid(151, "working"))
assigned_edges = chain(staff_grid(152, "assigned"))
assign_edges = grid(153, "assign", {"f:arg": [0, 1, 0x100, 0xFFFF], "f:turn": [0, 1, MASK, 0x80000000], "f:f5a": [0, 7], "f:f5c": [0, 5]})
unassign_edges = grid(154, "unassign", {"f:f5a": [0, 7], "f:f5c": [0, 5, MASK]})
condemned_edges = grid(155, "condemned", {"f:state": [0, 1, 2, 3, 4, 5, MASK, 0x80000000]})
upgrade_edges = grid(156, "upgrade", {"f:state": [0, 1, 2, 3, 4, 5, MASK, 0x80000000]})
flagged_edges = grid(157, "flagged", {"f:f60": [0, 1, 2, MASK, 0x80000000, 0x100]})
dirty_edges = grid(158, "dirty", {"f:flags": [0, 1, 8, 9, 0x101, 0xFFFF], "f:f40": [T.f32_bits(x) for x in (100.0, 25.0, 24.9999, 24.5, 24.0, 0.0, -0.0, 1.0, 255.0, 256.0, 280.5, -1.0, -0.5,
                                                                                                          float("nan"), float("-inf"), 0.4, 281.0)]})
clean_edges = grid(159, "clean", {"f:flags": [0, 1, 9], "f:f40": [T.f32_bits(x) for x in (100.0, 24.0, 25.0, 0.0, 255.0, 280.0)], "f:cx": [0, 5, 0xFF], "f:cy": [0, 3, 0xFF]})
playfix_edges = grid(160, "playfix", {"var7": [0, 1, 2, MASK, 0x7FFFFFFF, 0x80000000], "f:f20": [0, 5]})
fixing_edges = grid(161, "fixing", {"f:f20": [0, 5, MASK], "f:f50": [0, 3]})
breakdown_edges = grid(162, "breakdown", {"f:f20": [0, 5, MASK], "f:tid": [0, 1, 0xFFFF]})
worn_edges = grid(163, "worn", {"f:f20": [0, 5, MASK], "f:f40": [0, T.f32_bits(100.0)]})

TESTS = [
    FnTest("CObject::AddWearAndTear", ppc, host, edge=wear_edges, rnd=make_random("wear", wear_tweak)),
    FnTest("CObject::GetProspectiveDamagePerTurn", ppc, host, edge=damage_edges, rnd=make_random("damage", damage_tweak)),
    FnTest("CObject::GetProspectiveReliability", ppc, host, edge=reliab_edges, rnd=make_random("reliab", reliab_tweak)),
    FnTest("CObject::RepairFully", ppc, host, edge=repair_edges, rnd=make_random("repair", repair_tweak)),
    FnTest("CObject::CleanFully", ppc, host, edge=clean_edges, rnd=make_random("clean", flags_tweak)),
    FnTest("CObject::RequestMaintenance", ppc, host, edge=maint_edges, rnd=make_random("maint", maint_tweak)),
    FnTest("CObject::IsBeingWorkedUpon", ppc, host, edge=working_edges, rnd=make_random("working", staff_tweak)),
    FnTest("CObject::AssignStaffMember", ppc, host, edge=assign_edges, rnd=make_random("assign")),
    FnTest("CObject::UnassignStaffMember", ppc, host, edge=unassign_edges, rnd=make_random("unassign")),
    FnTest("CObject::GetAssignedStaffMember", ppc, host, edge=assigned_edges, rnd=make_random("assigned", staff_tweak)),
    FnTest("CObject::IsCondemned", ppc, host, edge=condemned_edges, rnd=make_random("condemned")),
    FnTest("CObject::IsDirty", ppc, host, edge=dirty_edges, rnd=make_random("dirty", flags_tweak)),
    FnTest("CObject::IsFlaggedForMaintenance", ppc, host, edge=flagged_edges, rnd=make_random("flagged")),
    FnTest("CObject::RequiresUpgrade", ppc, host, edge=upgrade_edges, rnd=make_random("upgrade")),
    FnTest("CObject::StartFixing", ppc, host, edge=fixing_edges, rnd=make_random("fixing")),
    FnTest("CObject::IsPlayingFixAnim", ppc, host, edge=playfix_edges, rnd=make_random("playfix")),
    FnTest("CObject::RequestBreakdown", ppc, host, edge=breakdown_edges, rnd=make_random("breakdown")),
    FnTest("CObject::SetWornFlag", ppc, host, edge=worn_edges, rnd=make_random("worn")),
]
