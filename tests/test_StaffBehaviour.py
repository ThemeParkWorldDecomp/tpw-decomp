"""Unit w191, against libtpw_unit_StaffBehaviour.dylib (tests/unit_StaffBehaviour.cpp), main app ("SimTheme Park"):
  src/game/people/staff.cpp   CStaff::ProcessWalkingToRestArea 0x10126008, ProcessResting 0x101262a8, ProcessWalkingToStrike 0x10126444, ProcessStriking
                            0x10126504, ProcessBeingStunned 0x10126688, GetTiredPatrolling 0x10126d1c, GetTiredWorking 0x10126e28, FindNearestRestingPlace
                            0x10127108, DoGeneralStaffThing 0x101272b0, GoToRestArea 0x10127658, StopResting 0x10127764, SetPatrolArea 0x101278f0,
                            GetPatrolArea 0x1012798c, CancelPatrolArea 0x10127a24, HasPatrolArea 0x10127a78, InsidePatrolArea 0x10127af8, FindDestInPatrolArea
                            0x10127c10

A case is a staff member (0x20c bytes of seeded fill between two 0x40 byte guards, the fields the code reads set from the case), the world's tail block (the
turn at world + 0x1da70c and the head of the object list at world + 0x1da746; the world pointer is shifted back to it), eight objects with the thing ids 20 to
27 in gThingArray (a subset chained from the head, with the flags, the cell, the entry cell and the script handle the code reads), a map block, the message
centre (absent, or a block) and the main balance's slots 180 to 289 and 345 to 368, built twice by ONE piece of Python code: in emulator memory at the Mac's
addresses (big-endian) and in the host library's memory (little-endian). Every callee outside the 17 functions is a fake that records the same event on both
sides (tests/calllog.py), pointers into the staff member as the label 1, the message centre 6, the analyser 7, the staff headquarters 8 and the script 9, the
thoughts and the delayed script as offsets from the staff member, map cells as offsets from the map; the queues answer from the case. FindNearestRestingPlace
and GoToRestArea run real inside the others (a hidden pointer answer on the Mac). The staff-room event the message centre is sent is read field by field in
the PostMessage fake (flags, thing id, arriving); its destructor is silent on both sides.

What is compared: the answer, the event log, the staff member's fields (decoded) and every other byte of the block, the tail block and the global message
centre. Not tested: a NULL this, a NULL world, an object row that is NULL, energy or happiness values that are NaN, infinite or outside the int range (the
emulator's fctiwz does not saturate), balance slots outside the filled ranges.
"""
import ctypes as C
import itertools
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_StaffBehaviour"
MASK = 0xFFFFFFFF

ENTRY = dict(walkrest=0x10126008, resting=0x101262A8, walkstrike=0x10126444, striking=0x10126504, stunned=0x10126688, tiredpatrol=0x10126D1C,
             tiredwork=0x10126E28, findrest=0x10127108, general=0x101272B0, gorest=0x10127658, stop=0x10127764, setpatrol=0x101278F0, getpatrol=0x1012798C,
             cancel=0x10127A24, has=0x10127A78, inside=0x10127AF8, finddest=0x10127C10)
SIZES = {0x10126008: 492, 0x101262A8: 368, 0x10126444: 140, 0x10126504: 340, 0x10126688: 92, 0x10126D1C: 220, 0x10126E28: 216, 0x10127108: 368,
         0x101272B0: 884, 0x10127658: 224, 0x10127764: 352, 0x101278F0: 28, 0x1012798C: 20, 0x10127A24: 36, 0x10127A78: 84, 0x10127AF8: 188, 0x10127C10: 440}
OWN = set(SIZES)

ADDR = dict(balance_values=0x102B4568 + 8)
G_WORLD, G_MAP, G_ARRAY, G_CENTRE = 0x1037EC0C, 0x1037EC08, 0x1034CBFC, 0x1037F6C4
WIDE_CTOR_TV, WIDE_DTOR_TV, CENTRE_SIZE = 0x102657E8, 0x10267288, 0x1D0
HOST_LAYOUT = [0x20C, 16, 8, 0x1DA70C, CENTRE_SIZE, 0x44, 0x1DA746]   # the host's BalanceValue is 8 bytes (the integer first)
HQ, ANALYSER, SCRIPT = 0x6000, 0x4000, 0x7000

GUARD = 0x40
OBJ = 0x20C
OBJ_BLOCK = GUARD + OBJ + GUARD
TAIL = 0x40
TAIL0 = 0x1DA70C
HEAD_AT = 0x1DA746 - TAIL0
CELL = 0x44
NCELL = 16
MAP_BLOCK = CELL * NCELL
ROOM = 0x40
OIDS = [20, 21, 22, 23, 24, 25, 26, 27]
GHOST = 99
SLOTS = list(range(180, 290)) + list(range(345, 369))
L_OBJ, L_CENTRE, L_ANALYSER, L_HQ, L_SCRIPT = 1, 6, 7, 8, 9


def s32(v):
    return T.s32(v)


def put_fmt(order, size):
    return order + {1: "B", 2: "H", 4: "I", 8: "Q"}[size]


# ---- the staff member's fields the code reads or writes (offset, size, key); everything else must keep its fill
CASE_FIELDS = ((2, 1, "kind"), (4, 1, "cellx"), (6, 1, "celly"), (0x1C, 4, "f1c"), (0x20, 4, "spidx"), (0x24, 4, "spset"), (0xC0, 2, "walk"), (0x188, 4, "f188"),
               (0x19C, 4, "state"), (0x1E4, 4, "grade"), (0x1F4, 4, "happy"), (0x1F8, 4, "energy"), (0x1FC, 4, "sturn"), (0x204, 2, "rest"), (0x206, 2, "bl"),
               (0x208, 2, "tr"))
OBJ_FIELDS = [(off, n) for off, n, k in CASE_FIELDS]

# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
R = CL.recorder


def stub_pair(name):
    """SetDest(MapPos, MapPos): r3 the peep, r4 and r5 the two bytes in the HIGH halves."""
    def stub(e):
        L = e.cl
        a, b = e.r(4), e.r(5)
        L.rec(name, (L.lab(e.r(3)), (a >> 24) & 0xFF, (a >> 16) & 0xFF, (b >> 24) & 0xFF, (b >> 16) & 0xFF))
        e.set_r(3, L.ret(name, 0) & MASK)
    return stub


def stub_thought(e):
    L = e.cl
    L.rec("CThoughts::ShowThoughtBubble", (s32(e.r(3) - L.base), s32(e.r(4)), e.r(5) & 0xFF))


def stub_spawn(e):
    L = e.cl
    L.rec("CDelayedScript::SpawnSpriteScript", (s32(e.r(3) - L.base), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))


def stub_ispath(e):
    L = e.cl
    L.rec("CMapCell::IsPath", (s32(e.r(3) - L.mapbase),))
    e.set_r(3, L.ret("CMapCell::IsPath", 0) & MASK)


def stub_post(e):
    L = e.cl
    m = e.r(4)
    L.rec("PostMessage", (L.lab(e.r(3)), e.u32(m + 4), (e.u32(m + 8) >> 16) & 0xFFFF, e.u32(m + 0xC)))
    e.set_r(3, L.ret("PostMessage", 0) & MASK)


def hidden_thing_id(name, this_reg=4):
    """A fake that returns a ThingId through the hidden pointer r3 (this in r4)."""
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(this_reg)),))
        e.write(e.r(3), struct.pack(">H", L.ret(name, 0) & 0xFFFF))
    return stub


def count_ints(text):
    return text.count("%d")


def stub_assert(e):
    text = e.cstr(e.r(4))[:159]
    ints = [s32(e.r(3))] + [e.r(5 + i) & MASK for i in range(min(3, count_ints(text)))]
    e.cl.rec("GAME_ASSERTMSG", tuple(ints), (text,))


def stub_ctrace(e):
    text = e.cstr(e.r(4))[:159]
    ints = [e.r(3) & MASK] + [e.r(5 + i) & MASK for i in range(min(3, count_ints(text)))]
    e.cl.rec("CTRACE", tuple(ints), (text,))


def stub_trace(e):
    e.cl.rec("TRACE", (), (e.cstr(e.r(3))[:159],))


FAKES = [
    (0x10114FA8, R("CPeep::PeepWalks", "L", ret="i")),
    (0x10115400, R("CPeep::SetDest", "Lw", ret="i")),
    (0x101252A8, R("CStaff::SetState", "Li")),
    (0x10126718, R("CStaff::GetStaffType", "L", ret="i")),
    (0x10126904, R("CStaff::IsStriking", "L", ret="i")),
    (0x1012BDE0, R("CStaffHQ::IsStriking", "Li", ret="i")),
    (0x1012BE24, R("CStaffHQ::StopStrikeInProgress", "Li")),
    (0x1013B1E4, R("CWorld::GetRand", "x", ret="i")),
    (0x1013FF7C, R("CWorld::IsParkClosed", "x", ret="i")),
    (0x1013FFB4, R("CWorld::ParkGates_GetStatus", "x", ret="i")),
    (0x1013E99C, R("CWorld::GetStaffHQ", "x", ret=lambda e, L: HQ)),
    (0x1013EBC8, R("CWorld::GetParkAnalyser", "x", ret=lambda e, L: ANALYSER)),
    (0x100E7278, R("CParkAnalyser::NumberOfVisitorsInPark", "L", ret="i")),
    (0x101012F4, R("CMap::IsXYValid", "ii", ret="i")),
    (0x100D668C, R("RSSE_GetScriptPointer", "i", ret=lambda e, L: SCRIPT)),
    (0x100D6C58, R("RSSE_Getvar", "Li", ret="i")),
    (0x100D6734, R("RSSE_Setvar", "Lii")),
    (0x100FB7A4, R("CEntertainer::ChooseNextAction", "L")),
    (0x100FE430, R("CGuard::ChooseNextAction", "L")),
    (0x100FF7E4, R("CHandyman::ChooseNextAction", "L")),
    (0x10104A30, R("CMechanic::ChooseNextAction", "L")),
    (0x101214C0, R("CResearcher::ChooseNextAction", "L")),
]
for address, fake in FAKES:
    STUBS.internal(address, fake)
STUBS.internal(0x1012E1F8, hidden_thing_id("CThing::GetThingId"))
STUBS.internal(0x101154F0, stub_pair("CPeep::SetDestPos"))
STUBS.internal(0x1012F0D8, stub_thought)
STUBS.internal(0x100FAB9C, stub_spawn)
STUBS.internal(0x1009C44C, stub_ispath)
STUBS.internal(0x1015256C, stub_post)
STUBS.internal(0x1012622C, lambda e: None)                  # ~CMsgStaffRoomEvent: silent on both sides
STUBS.internal(0x1000DDE8, stub_assert)
STUBS.internal(0x1006FCBC, stub_ctrace)
STUBS.internal(0x1006FCFC, stub_trace)
STUBS.imp("labs", lambda emu: emu.set_r(3, T.u32(abs(s32(emu.r(3))))))


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
    bal_stride = 4

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({HQ: L_HQ, ANALYSER: L_ANALYSER, SCRIPT: L_SCRIPT}, case["queues"])
        self.saved = STUBS.activate(e)
        self.fresh = e.alloc(CENTRE_SIZE)

        def op_new(emu):
            fail = emu.cl.ret("newnull", 0) != 0
            emu.cl.rec("new", [emu.r(3), int(fail)])
            emu.set_r(3, 0 if fail else self.fresh)

        def construct(emu, call):
            base, ctor, dtor, size, count = emu.r(3), emu.r(4), emu.r(5), emu.r(6), emu.r(7)
            if ctor == WIDE_CTOR_TV:
                emu.cl.rec("CMessageCentre", [emu.cl.newlab(base), int(ctor == WIDE_CTOR_TV), int(dtor == WIDE_DTOR_TV), size, count])
                return
            for i in range(count):
                call(ctor, base + i * size, 1)

        STUBS.imp("__nw__FUl", op_new)
        self.saved2 = STUBS.activate(e)
        T.stub_cb(e, "__construct_array", construct)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved2)
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

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

    def setup(self, eb, tail, centre, mapblock):
        self.L.base = eb
        self.L.mapbase = mapblock + CELL
        self.world = (tail - TAIL0) & MASK
        self.put(G_WORLD, 4, self.world)
        self.put(G_MAP, 4, mapblock + CELL)
        self.put(G_CENTRE, 4, centre)

    def centre(self):
        word = self.get(G_CENTRE, 4)
        label = self.L.lab(word)
        return label if label >= 0 else (1 << 33) | word

    def call(self, entry, args):
        return T.call_ppc(self.e, entry, [a & MASK for a in args], max_insns=20_000_000)[0] & MASK


class Host(Side):
    order = "<"
    bal_stride = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {L_HQ: HQ, L_ANALYSER: ANALYSER, L_SCRIPT: SCRIPT}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_stb_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_stb_alloc(n)

    def label(self, address, n):
        self.lib.tpw_cl_label(n, address)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def row(self, row, ptr):
        self.lib.tpw_stb_setrow(row, ptr)

    def setup(self, eb, tail, centre, mapblock):
        self.lib.tpw_stb_setup(eb, tail, centre, mapblock + CELL)
        self.world = tail

    def centre(self):
        return self.lib.tpw_stb_centre()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_stb_"
    u64, i32, u32 = C.c_uint64, C.c_int32, C.c_uint32
    plain = [("walkrest", None), ("resting", None), ("walkstrike", None), ("striking", None), ("stunned", None), ("tiredpatrol", None), ("tiredwork", None),
             ("findrest", u32), ("general", i32), ("gorest", u32), ("stop", None), ("cancel", None), ("has", i32), ("finddest", i32)]
    for name, res, args in (
        ("addr", u64, [C.c_char_p]), ("alloc", u64, [u32]), ("setup", None, [u64, u64, u64, u64]), ("setrow", None, [i32, u64]), ("centre", C.c_int64, []),
        ("setpatrol", None, [u64, u32, u32]), ("getpatrol", u32, [u64, u32, u32]), ("inside", i32, [u64, u32]),
    ) + tuple((n, r, [u64]) for n, r in plain):
        fn = getattr(lib, p + name)
        fn.restype, fn.argtypes = res, args
    lib.tpw_stb_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_stb_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def fill(seed, n):
    r = random.Random(seed)
    return bytes(r.getrandbits(8) for _ in range(n))


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
    centre = s.alloc(CENTRE_SIZE) if c["centre"] else 0
    mapblock = s.alloc(MAP_BLOCK)
    s.write(mapblock, fill(c["seed"] + 3, MAP_BLOCK))
    for i, oid in enumerate(OIDS):
        t = s.alloc(ROOM)
        s.write(t, fill(c["seed"] + 10 + i, ROOM))
        o = c["objs"][i]
        if i in c["chain"]:
            at = c["chain"].index(i)
            nxt = OIDS[c["chain"][at + 1]] if at + 1 < len(c["chain"]) else 0
        else:
            nxt = o["link"]
        s.put(t + 4, 1, o["x"])
        s.put(t + 6, 1, o["y"])
        s.put(t + 0xC, 2, nxt)
        s.put(t + 0x20, 4, o["f20"])
        s.put(t + 0x2E, 2, o["flags"])
        s.put(t + 0x32, 2, o["f32"])
        s.row(oid, t)
    s.row(GHOST, 0)
    s.label(eb, L_OBJ)
    if centre:
        s.label(centre, L_CENTRE)
    s.setup(eb, tail, centre, mapblock)
    s.put(tail, 4, c["turn"])
    s.put(tail + HEAD_AT, 2, OIDS[c["chain"][0]] if c["chain"] else 0)
    s.tail = tail
    for off, size, key in CASE_FIELDS:
        s.put(eb + off, size, c[key])
    for k in SLOTS:
        s.put(s.g("balance_values") + s.bal_stride * k, 4, c["bal"][k])
    spare = s.alloc(0x40)
    s.row(0, spare if c["row0"] else 0)


def snapshot(s, base, size, fields, skew=GUARD):
    data = bytearray(s.read(base, size))
    values = []
    for off, n in fields:
        i = skew + off
        values.append(s.get(base + i, n))
        data[i:i + n] = bytes(n)
    return values, bytes(data)


def execute(s, c):
    op = c["op"]
    eb = s.eb
    if isinstance(s, Ppc):
        entry = ENTRY[op]
        if op in ("findrest", "gorest"):
            out = s.alloc(8)
            s.call(entry, [out, eb])
            return struct.unpack(">H", s.read(out, 2))[0]
        if op == "getpatrol":
            out = s.alloc(8)
            s.write(out, struct.pack(">HH", c["gp0"], c["gp1"]))
            s.call(entry, [eb, out, out + 2])
            a, b = struct.unpack(">HH", s.read(out, 4))
            return a | (b << 16)
        if op == "setpatrol":
            s.call(entry, [eb, c["abl"] << 16, c["atr"] << 16])
            return 0
        if op == "inside":
            return s32(s.call(entry, [eb, c["acell"] << 16]))
        if op in ("general", "has", "finddest"):
            return s32(s.call(entry, [eb]))
        s.call(entry, [eb])
        return 0
    lib = s.lib
    if op == "setpatrol":
        lib.tpw_stb_setpatrol(eb, c["abl"], c["atr"])
        return 0
    if op == "getpatrol":
        return lib.tpw_stb_getpatrol(eb, c["gp0"], c["gp1"])
    if op == "inside":
        return lib.tpw_stb_inside(eb, c["acell"])
    fn = getattr(lib, "tpw_stb_" + op)
    result = fn(eb)
    return 0 if result is None else result


def observe(s, c, answer):
    out = {"answer": answer, "log": s.log(), "centre": s.centre()}
    out["obj"] = snapshot(s, s.eb - GUARD, OBJ_BLOCK, OBJ_FIELDS)
    out["tail"] = snapshot(s, s.tail, TAIL, [(0, 4), (HEAD_AT, 2)], 0)
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


def fbits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


# energy and happiness: finite and inside the int range (the emulator's fctiwz does not saturate); edges of 0, 10/11, 19/20, 97/98, 99, 100, 255/256
STATV = [fbits(x) for x in (0.0, -0.0, 0.5, 1.0, -0.5, -1.0, 9.99, 10.0, 10.5, 11.0, 19.99, 20.0, 20.5, 39.99, 40.0, 59.99, 60.0, 79.99, 80.0, 96.99, 97.0, 97.5,
                            98.0, 98.5, 99.0, 99.5, 99.99, 100.0, 100.5, 101.0, 255.0, 256.0, 300.0, 50.0, 200.0, 1e5, -300.0, 8388608.0, 0.01, 0.025)]
RATEV = [fbits(x) for x in (0.0, 0.01, 0.5, 1.0, 2.5, 10.0, 50.0, 99.0, 100.0, 150.0, -1.0, -0.25, -100.0, -150.0, 1e5, 3.4e9, 1e-30)]
INTV = [0, 1, 2, 3, 5, 10, 50, 100, 101, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, MASK, MASK - 1]
RESTV = [MASK, 0, 10, 19, 20, 49, 50, 99, 100, 101, 255, 0x7FFFFFFF, 0x80000000]
GRADES = [0, 1, 2, 3, 4, MASK, 5]
BIGGRADES = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, MASK, MASK - 1, MASK - 4, 0x7FFFFFFF, 0x80000000, 0x80000001, 0x7FFFFFF0]
CELLS = [0, 1, 2, 0x7F, 0x80, 0x81, 0x82, 0xFF, 0x100, 0x101, 0x4000, 0x7FFF, 0x8000, 0xFFFF, 0x2001, 0x0F01]
IDS = [0, 1, 2, 5, 0x2FF, 0xFFFF, 20, 21, 22, 27]
RANDS = [0, 1, 7, 8, 0xF, 0x10, 0x17, 0x18, 0x7F, 0x80, 0xFF, 0x100, 0x1FF, 0x7FF, 0x800, 0xFFF, MASK, 0x7FFFFFFF, 0x80000000, 0x12345678, 0x9ABCDEF0, 100, 60]


def u32(r):
    return r.choice(INTV + [r.getrandbits(32), r.getrandbits(16), r.getrandbits(8)])


def qlist(r, values):
    return [r.choice(values) for _ in range(r.randrange(1, 4))]


def bal_default(r, slot):
    if 187 <= slot <= 208 and (slot - 187) % 4 in (0, 1):
        return r.choice(RATEV)
    return r.choice(INTV + [r.getrandbits(32)])


def base(r, op):
    c = {"op": op, "seed": r.getrandbits(32), "kind": r.choice([4, 5, 6, 7, 8, 4, 5, 6, 7, 8, 0, 1, 2, 3, 9, 0xFF]), "cellx": r.choice([0, 1, 5, 0x40, 0x7F, 0x80, 0xFF, r.getrandbits(8)]),
         "celly": r.choice([0, 1, 5, 0x40, 0x7F, 0x80, 0xFF, r.getrandbits(8)]), "f1c": r.choice(INTV), "spidx": u32(r), "spset": u32(r), "walk": r.getrandbits(16),
         "f188": u32(r), "state": u32(r), "grade": r.choice(GRADES), "happy": r.choice(STATV), "energy": r.choice(STATV), "sturn": u32(r),
         "rest": r.choice(IDS), "bl": r.choice(CELLS + [r.getrandbits(16)]), "tr": r.choice(CELLS + [r.getrandbits(16)]), "turn": u32(r),
         "centre": r.random() < 0.5, "row0": r.random() < 0.2, "abl": r.choice(CELLS), "atr": r.choice(CELLS), "acell": r.choice(CELLS + [r.getrandbits(16)]),
         "gp0": r.getrandbits(16), "gp1": r.getrandbits(16)}
    if op in ("walkrest", "resting", "stop"):      # these read the rest area's row (a null row would be a null read on both sides)
        c["rest"] = r.choice(OIDS)
    c["objs"] = []
    for _ in OIDS:
        c["objs"].append({"x": r.choice([0, 1, 5, 0x40, 0x7F, 0x80, 0xFF, r.getrandbits(8)]), "y": r.choice([0, 1, 5, 0x40, 0x7F, 0x80, 0xFF, r.getrandbits(8)]),
                          "link": r.choice(OIDS + [0]), "f20": u32(r), "flags": r.choice([0, 2, 2, 2, 3, 0x12, 1, 4, 0xFFFF, 0x102]), "f32": r.choice(CELLS + [r.getrandbits(16)])})
    chain = list(range(len(OIDS)))
    r.shuffle(chain)
    c["chain"] = chain[:r.choice([0, 1, 2, 3, 5, 8])]
    c["bal"] = {k: bal_default(r, k) for k in SLOTS}
    c["bal"][183] = r.choice(RESTV)
    c["bal"][184] = r.choice(INTV)
    for slot in (349, 350):
        c["bal"][slot] = r.choice([0, 1, 5, 0x7F, 0x80, 0xFFFF, 0x10000, MASK, 100])
    for slot in (361, 362):
        c["bal"][slot] = r.choice([0, 1, 5, 0x7F, 0xFF, 0x100, MASK, 0x7FFFFFFF])
    for slot in (363, 364):
        c["bal"][slot] = r.choice([0, 1, 2, 10, 0x7F, 0x100, 0x7FFFFFFF, 0x80000000, MASK])
    c["queues"] = {
        "CPeep::PeepWalks": qlist(r, [0, 1, 2, 3, MASK, 5]), "CPeep::SetDest": qlist(r, [0, 1, MASK, 5]), "CPeep::SetDestPos": qlist(r, [0, 1, MASK, 5]),
        "CStaff::GetStaffType": qlist(r, [0, 1, 2, 3, 4, 5]), "CStaff::IsStriking": qlist(r, [0, 1, 2, MASK]), "CStaffHQ::IsStriking": qlist(r, [0, 1, 2, MASK]),
        "CWorld::GetRand": [r.choice(RANDS + [r.getrandbits(32)]) for _ in range(r.randrange(1, 8))], "CWorld::IsParkClosed": qlist(r, [0, 1, 2, MASK]),
        "CWorld::ParkGates_GetStatus": qlist(r, [0, 1, 1, 2, MASK, 5]), "CParkAnalyser::NumberOfVisitorsInPark": qlist(r, [0, 0, 1, 5, MASK]),
        "CMap::IsXYValid": qlist(r, [0, 1, 1, MASK, 5]), "CMapCell::IsPath": qlist(r, [0, 1, 1, 2, MASK]), "RSSE_Getvar": qlist(r, [0, 1, MASK, 5, 0x7FFFFFFF, 0x80000000]),
        "PostMessage": qlist(r, [1, 0]), "CThing::GetThingId": [r.choice(IDS)], "newnull": [r.choice([0, 0, 0, 1])] if r.random() < 0.5 else qlist(r, [0, 0, 1]),
    }
    return c


def make_random(op, tweak=None):
    def rnd(r):
        c = base(r, op)
        if tweak:
            tweak(r, c)
        return c
    return rnd


def grid(seed, op, axes, extra=None):
    """Edge cases: the product of the axes ("f:field" a case field, "b:slot" a balance slot, otherwise a queue) over a random base case."""
    def edges():
        r = random.Random(seed)
        keys = list(axes)
        for combo in itertools.product(*[axes[k] for k in keys]):
            c = base(r, op)
            for k, v in zip(keys, combo):
                if k.startswith("f:"):
                    c[k[2:]] = v
                elif k.startswith("b:"):
                    c["bal"][int(k[2:])] = v
                else:
                    c["queues"][k] = list(v) if isinstance(v, (list, tuple)) else [v]
            if extra:
                extra(r, c)
            yield c
    return edges


def chain_all(r, c):
    c["chain"] = list(range(len(OIDS)))
    r.shuffle(c["chain"])


def chain_some(r, c):
    c["chain"] = c["chain"] or [r.randrange(len(OIDS))]


def rest_tweak(r, c):
    c["grade"] = r.choice(GRADES)
    if r.random() < 0.7:
        c["energy"] = r.choice([fbits(x) for x in (0.0, 50.0, 90.0, 95.0, 98.0, 99.0, 99.5, 100.0, 200.0, -5.0, 1e5)])
    if r.random() < 0.5:
        c["bal"][187 + 4 * (c["grade"] if c["grade"] < 5 else 0)] = r.choice(RATEV)


def kind_walk(r, c):
    c["kind"] = r.choice([4, 5, 6, 7, 8])


def tired_tweak(r, c):
    c["grade"] = r.choice(BIGGRADES)


def stunned_tweak(r, c):
    c["grade"] = r.choice(GRADES)
    c["sturn"] = r.choice([0, 1, c["turn"], (c["turn"] - r.choice([0, 1, 2, 3, 100])) & MASK, r.getrandbits(32)])
    c["bal"][186 + 4 * (c["grade"] if c["grade"] < 5 else 0)] = r.choice([0, 1, 2, 5, 10, 0x7FFFFFFF, MASK, 0x55555555, 0x55555556, 0x2AAAAAAA, 0x2AAAAAAB])


def striking_tweak(r, c):
    c["f1c"] = r.choice([0, 1, 0x7F, 0x80, 0x7FF, 0x800, 0x7F00, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, r.getrandbits(32)])


def patrol_tweak(r, c):
    c["bl"] = r.choice(CELLS + [r.getrandbits(16)])
    c["tr"] = r.choice(CELLS + [r.getrandbits(16)])
    if r.random() < 0.5:
        c["tr"] = (c["bl"] + r.choice([0, 1, 2, 0x80, 0x81, 0x102, 5, 0x7F, 0xFF])) & 0xFFFF


def finddest_tweak(r, c):
    patrol_tweak(r, c)
    if r.random() < 0.5:
        c["bl"] = r.choice([1, 2, 0x81, 0x82, 0x102, 0x203])
        c["tr"] = c["bl"] + r.choice([0, 1, 2, 3, 0x80, 0x81, 0x100, 0x7F, 7])
    c["queues"]["CMap::IsXYValid"] = [r.choice([0, 1, 1, 1]) for _ in range(r.randrange(1, 34))]
    c["queues"]["CMapCell::IsPath"] = [r.choice([0, 1, 1]) for _ in range(r.randrange(1, 34))]
    c["queues"]["CPeep::SetDest"] = [r.choice([0, 0, 0, 1]) for _ in range(r.randrange(1, 34))]


def general_tweak(r, c):
    c["energy"] = r.choice(STATV)
    c["happy"] = r.choice(STATV)
    if r.random() < 0.4:
        c["queues"]["CStaffHQ::IsStriking"] = [1]
        c["queues"]["CWorld::ParkGates_GetStatus"] = [1]
    if r.random() < 0.5:
        c["chain"] = c["chain"] or [r.randrange(len(OIDS))]


def findrest_tweak(r, c):
    for o in c["objs"]:
        if r.random() < 0.4:
            o["x"], o["y"] = r.choice([0, 1, 2, 0x7F, 0x80]), r.choice([0, 1, 2, 3, 0x7F])
        if r.random() < 0.3:
            o["flags"] = r.choice([2, 3, 0x12, 0x102])
    if r.random() < 0.5:
        chain_all(r, c)


def walkrest_tweak(r, c):
    c["queues"]["CPeep::PeepWalks"] = [r.choice([0, 0, 1, 2, 2, 3, MASK])]
    c["grade"] = r.choice(GRADES)
    c["rest"] = r.choice(OIDS)


def stop_tweak(r, c):
    c["rest"] = r.choice(OIDS)
    c["queues"]["CThing::GetThingId"] = [r.choice(IDS + [0x7FFF, 0x8000, 0xFFFE])]


TESTS = [
    FnTest("CStaff::ProcessWalkingToRestArea", ppc, host,
           edge=grid(31, "walkrest", {"CPeep::PeepWalks": [0, 1, 2, 3, MASK, 5], "f:centre": [False, True], "f:rest": [20, 27], "newnull": [0, 1],
                                      "f:row0": [False, True], "CThing::GetThingId": [0, 1, 0x2FF, 0xFFFF, 0x8000], "b:184": [0, 1, 10, 50, 100, 101, MASK, 0x7FFFFFFF],
                                      "f:happy": [fbits(0.0), fbits(10.0), fbits(99.5), fbits(100.0), fbits(105.0)]}),
           rnd=make_random("walkrest", walkrest_tweak)),
    FnTest("CStaff::ProcessResting", ppc, host,
           edge=grid(32, "resting", {"f:kind": [4, 5, 6, 7, 8, 0, 1, 2, 3, 9, 0xFF], "f:energy": [fbits(x) for x in (0.0, 90.0, 99.0, 99.9, 100.0, 150.0, -50.0, 1e5)],
                                     "f:grade": GRADES, "CWorld::GetRand": [[0]], "f:happy": [fbits(0.0), fbits(50.0), fbits(100.0), fbits(-100.0), fbits(1e5)],
                                     "f:rest": [20, 22]}),
           rnd=make_random("resting", rest_tweak)),
    FnTest("CStaff::ProcessWalkingToStrike", ppc, host, edge=grid(33, "walkstrike", {"CPeep::PeepWalks": [0, 1, 2, 3, MASK, 5, 0x7FFFFFFF], "f:f188": [0, 1, MASK]}),
           rnd=make_random("walkstrike")),
    FnTest("CStaff::ProcessStriking", ppc, host,
           edge=grid(34, "striking", {"CWorld::GetRand": [[0], [8], [0x7F], [0x80], [0xFF], [0x100], [0x800], [0xFF8], [0x1], [MASK], [0x78]],
                                      "f:f1c": [0, 1, 0x7F, 0x80, 0x7FE, 0x7FF, 0x800, 0x7F00, 0xFFFFFFFF, 0x80000000],
                                      "CWorld::IsParkClosed": [0, 1, 2], "CParkAnalyser::NumberOfVisitorsInPark": [0, 1], "CStaff::IsStriking": [0, 1],
                                      "CStaff::GetStaffType": [0, 3], "CPeep::SetDest": [0, 1], "b:349": [0, 5, 0x7F, MASK], "b:350": [0, 1, 0x7F, 0x10000]}),
           rnd=make_random("striking", striking_tweak)),
    FnTest("CStaff::ProcessBeingStunned", ppc, host,
           edge=grid(35, "stunned", {"f:grade": GRADES, "f:turn": [0, 1, 5, 100, 0x7FFFFFFF, 0x80000000, MASK], "f:sturn": [0, 1, 4, 5, 6, 12, 0x7FFFFFFF, MASK],
                                     "b:186": [0, 1, 2, 5, 0x7FFFFFFF, MASK], "b:190": [0, 1, 2], "b:194": [3]}),
           rnd=make_random("stunned", stunned_tweak)),
    FnTest("CStaff::GetTiredPatrolling", ppc, host,
           edge=grid(36, "tiredpatrol", {"f:grade": BIGGRADES, "f:energy": [fbits(x) for x in (0.0, 0.01, 0.06, 0.5, 1.0, 50.0, 99.9, 100.0, 100.5, 200.0, -1.0)],
                                         "f:happy": [fbits(x) for x in (0.0, 0.005, 0.03, 0.2, 1.0, 50.0, 99.9, 100.0, 100.5, 200.0, -1.0)]}),
           rnd=make_random("tiredpatrol", tired_tweak)),
    FnTest("CStaff::GetTiredWorking", ppc, host,
           edge=grid(37, "tiredwork", {"f:grade": BIGGRADES, "f:energy": [fbits(x) for x in (0.0, 0.025, 0.1, 0.5, 1.0, 50.0, 99.9, 100.0, 100.5, 200.0, -1.0)],
                                       "f:happy": [fbits(x) for x in (0.0, 0.01, 0.06, 0.2, 1.0, 50.0, 99.9, 100.0, 100.5, 200.0, -1.0)]}),
           rnd=make_random("tiredwork", tired_tweak)),
    FnTest("CStaff::FindNearestRestingPlace", ppc, host,
           edge=grid(38, "findrest", {"f:chain": [[], [0], [0, 1], [3, 1, 2], [0, 1, 2, 3, 4, 5, 6, 7], [7, 6, 5, 4, 3, 2, 1, 0]], "f:row0": [False, True],
                                      "f:cellx": [0, 1, 0x7F, 0x80, 0xFF], "f:celly": [0, 1, 0x7F, 0x80]}),
           rnd=make_random("findrest", findrest_tweak)),
    FnTest("CStaff::DoGeneralStaffThing (strike)", ppc, host,
           edge=grid(39, "general", {"f:kind": [4, 6, 7, 3, 8], "CStaffHQ::IsStriking": [0, 1], "CWorld::ParkGates_GetStatus": [1, 0, 2],
                                     "CPeep::SetDestPos": [0, 1], "b:363": [0, 1, 10, MASK], "b:364": [0, 1, 7, 0x80000000],
                                     "CWorld::GetRand": [[0x1234, 0x56, 0x789, 0x9A], [MASK, MASK, MASK, MASK, 1, 1, 1]], "b:361": [5, 0x100], "b:362": [0x7F, 3]},
                      lambda r, c: c["queues"].update({"CPeep::SetDest": [0]})),
           rnd=make_random("general", general_tweak)),
    FnTest("CStaff::DoGeneralStaffThing (energy)", ppc, host,
           edge=grid(40, "general", {"f:kind": [4], "f:energy": [fbits(x) for x in (0.0, 9.5, 19.99, 20.0, 49.0, 50.0, 59.9, 99.0, 99.99, 100.0, 255.0, 256.0, 300.0, -1.0, 1e5)],
                                     "b:183": RESTV, "f:happy": [fbits(x) for x in (0.0, 9.99, 10.0, 10.5, 11.0, 50.0, 96.9, 97.0, 97.5, 98.0, 99.0, 100.0, 255.0)],
                                     "CWorld::GetRand": [[0], [1], [0x10], [0xF]], "CStaffHQ::IsStriking": [[0]]},
                      lambda r, c: chain_all(r, c) if c["happy"] & 1 else None),
           rnd=make_random("general", general_tweak)),
    FnTest("CStaff::DoGeneralStaffThing (tired)", ppc, host,
           edge=grid(41, "general", {"f:energy": [fbits(0.0), fbits(5.0), fbits(49.0)], "b:183": [50, 100], "f:chain": [[], [0], [0, 1, 2, 3]], "CPeep::SetDest": [[0], [1]],
                                     "CWorld::GetRand": [[0], [1], [0x10], [0x20], [0xF]], "f:happy": [fbits(0.0), fbits(1.0), fbits(50.0), fbits(100.0)],
                                     "b:184": [0, 7, MASK], "CStaffHQ::IsStriking": [[0]], "f:centre": [False, True]}),
           rnd=make_random("general", general_tweak)),
    FnTest("CStaff::GoToRestArea", ppc, host,
           edge=grid(42, "gorest", {"f:chain": [[], [0], [2, 1], [0, 1, 2, 3, 4, 5, 6, 7]], "CPeep::SetDest": [[0], [1], [MASK]], "f:row0": [False, True],
                                    "f:rest": [0, 5, 0xFFFF]}),
           rnd=make_random("gorest", findrest_tweak)),
    FnTest("CStaff::StopResting", ppc, host,
           edge=grid(43, "stop", {"f:rest": OIDS[:3] + [27], "CThing::GetThingId": [0, 1, 0x2FF, 0xFFFF, 0x8000], "f:centre": [False, True], "newnull": [0, 1],
                                  "RSSE_Getvar": [0, 1, 0x80000000, MASK, 0x7FFFFFFF], "f:row0": [False, True], "f:spidx": [0, 5, MASK], "f:spset": [0, 4, MASK]}),
           rnd=make_random("stop", stop_tweak)),
    FnTest("CStaff::SetPatrolArea", ppc, host, edge=grid(44, "setpatrol", {"f:abl": CELLS, "f:atr": CELLS}), rnd=make_random("setpatrol")),
    FnTest("CStaff::GetPatrolArea", ppc, host, edge=grid(45, "getpatrol", {"f:bl": CELLS, "f:tr": CELLS}), rnd=make_random("getpatrol")),
    FnTest("CStaff::CancelPatrolArea", ppc, host, edge=grid(46, "cancel", {"f:bl": CELLS[:6], "f:tr": CELLS[:6]}), rnd=make_random("cancel")),
    FnTest("CStaff::HasPatrolArea", ppc, host, edge=grid(47, "has", {"f:bl": CELLS, "f:tr": [0, 1, 0x3FFF, 0x4000, 0x4001, 0xFFFF]}), rnd=make_random("has", patrol_tweak)),
    FnTest("CStaff::InsidePatrolArea", ppc, host,
           edge=grid(48, "inside", {"f:bl": [0, 1, 2, 0x81, 0x82, 0x4000], "f:tr": [0, 1, 2, 0x82, 0x103, 0x4000, 0xFFFF], "f:acell": CELLS + [0x83, 0x102, 0x103, 0x104, 0x1FF]}),
           rnd=make_random("inside", patrol_tweak)),
    FnTest("CStaff::FindDestInPatrolArea", ppc, host,
           edge=grid(49, "finddest", {"f:bl": [1, 2, 0x81, 0x102, 0x7F, 0x4000, 0], "f:tr": [1, 2, 0x82, 0x103, 0x8000, 0, 0xFFFF],
                                      "CMap::IsXYValid": [[1], [0], [1, 1, 0]], "CMapCell::IsPath": [[1], [0], [0, 1]], "CPeep::SetDest": [[1], [0], [0, 1]],
                                      "CWorld::GetRand": [[0, 0], [5, 7, 9, 11], [MASK, MASK]]}),
           rnd=make_random("finddest", finddest_tweak)),
]
