"""Unit w184, against libtpw_unit_Handyman.dylib (tests/unit_Handyman.cpp), main app ("SimTheme Park"):
  src/Handyman.cpp   CHandyman::CHandyman (cell) 0x100fefd0 and (cell, grade, flag, name) 0x100ff0a4, Init 0x100ff1b8, FinalDestroy 0x100ff2e0,
                     ReceiveMessage 0x100ff3c4, Serialise 0x100ff4c0, UpdateTurn 0x100ff6b4, ChooseNextAction 0x100ff7e4, SetState 0x100ffb70,
                     ModelState 0x100ffca8, ProcessPatrolling 0x100ffd78, ProcessWalkingToLitter 0x100ffe38, ProcessCleaningLitter 0x10100050,
                     ProcessWalkingToCleanToilet 0x10100230, ProcessCleaningToilet 0x101003ac, ProcessIdle 0x101004bc, FindNearestDirtyToilet
                     0x10100580, __sinit_Handyman_cpp 0x1010084c

A case is a handyman (0x218 bytes of seeded fill between two 0x40 byte guards, the fields the code reads set from the case), the world's tail block
(the turn at world + 0x1da70c and the object list head at + 0x1da746; the world pointer is shifted back to it), a map of 16 cells (0x44 bytes each,
seeded fill, the litter kind at +0x2c and the collector at +0x30 from the case; cell id k is the entry at block + 0x44 * k, the map pointer is
block + 0x44), eight toilets (0x40 byte blocks with the cell bytes at +4 and +6, the list link at +0xc, the flags at +0x2e and the entry cell at +0x32;
ids 20 to 27 in gThingArray, a subset of them chained from the head), the main balance's slots 180 to 289, the sound banks, the constructor's two
statics, the message centre (absent, or a block) and a ride-deleted message, built twice by ONE piece of Python code: in emulator memory at the Mac's
addresses (big-endian) and in the host library's memory (little-endian). Every callee outside the 18 functions is a fake that records the same event
on both sides (tests/calllog.py), pointers into the handyman as the label 1, the file 2, the message 3, the string table 4, the existing message
centre 6 and the toilets 10 to 17, the script and the thoughts as offsets from the handyman, the map cells as offsets from the map; the queues answer
from the case. The new message centre is checked through the nothrow operator new, the table pair of __construct_array and the label of the first
header. __sinit_Handyman_cpp: the two words are written with 0xDEADBEEF and 0xCAFEF00D on the Mac, the function runs, and they are compared with the
host's constants.

What is compared: the answer, the event log, the handyman's fields (decoded) and every other byte of the block, the tail block, the map block, the
toilet blocks, the two statics and the global message centre. Not tested: a NULL toilet row where the code would dereference it, ids of 0x2800 and
above (outside gThingArray), map cell ids above 15, a NULL this.
"""
import ctypes as C
import itertools
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_Handyman"
MASK = 0xFFFFFFFF

(CTOR1, CTOR2, INIT, FINAL, RECV, SER, UPDATE, CHOOSE, SETSTATE, MODEL, PATROL, TOLITTER, CLEANLITTER, TOTOILET, CLEANTOILET, IDLE, FIND, SINIT) = (
    0x100FEFD0, 0x100FF0A4, 0x100FF1B8, 0x100FF2E0, 0x100FF3C4, 0x100FF4C0, 0x100FF6B4, 0x100FF7E4, 0x100FFB70, 0x100FFCA8, 0x100FFD78, 0x100FFE38,
    0x10100050, 0x10100230, 0x101003AC, 0x101004BC, 0x10100580, 0x1010084C)
ENTRY = dict(ctor1=CTOR1, ctor2=CTOR2, init=INIT, final=FINAL, recv=RECV, ser=SER, update=UPDATE, choose=CHOOSE, setstate=SETSTATE, model=MODEL,
             patrol=PATROL, tolitter=TOLITTER, cleanlitter=CLEANLITTER, totoilet=TOTOILET, cleantoilet=CLEANTOILET, idle=IDLE, find=FIND, sinit=SINIT)
SIZES = {CTOR1: 132, CTOR2: 176, INIT: 256, FINAL: 180, RECV: 196, SER: 452, UPDATE: 260, CHOOSE: 856, SETSTATE: 248, MODEL: 164, PATROL: 140,
         TOLITTER: 480, CLEANLITTER: 424, TOTOILET: 320, CLEANTOILET: 216, IDLE: 152, FIND: 660, SINIT: 28}
OWN = set(SIZES)

ADDR = dict(banks=0x102F7C94, balance_values=0x102B4568 + 8, guard=0x1034C20C, sprite=0x1034C210, sinit_low=0x1034C218, sinit_high=0x1034C214)
G_WORLD, G_MAP, G_TEXT, G_ARRAY, G_CENTRE = 0x1037EC0C, 0x1037EC08, 0x1037FF18, 0x1034CBFC, 0x1037F6C4
WIDE_CTOR_TV, WIDE_DTOR_TV, CENTRE_SIZE = 0x102657E8, 0x10267288, 0x1D0
HOST_LAYOUT = [0x218, 16, 8, 0x1DA70C, 48, CENTRE_SIZE, 0x44, 0x1DA746]   # the host's BalanceValue is 8 bytes (the integer first), its table entry 48

GUARD = 0x40
OBJ = 0x218
OBJ_BLOCK = GUARD + OBJ + GUARD
TAIL = 0x40
TAIL0 = 0x1DA70C
HEAD_AT = 0x1DA746 - TAIL0
CELL = 0x44
NCELL = 16
MAP_BLOCK = CELL * NCELL
TOILET = 0x40
OIDS = [20, 21, 22, 23, 24, 25, 26, 27]
GHOST = 99                                     # a thing id with no row
SLOT0, SLOT_N = 180, 110
L_OBJ, L_FILE, L_MSG, L_TABLE, L_CENTRE, L_TOILET0 = 1, 2, 3, 4, 6, 10


def s32(v):
    return T.s32(v)


def put_fmt(order, size):
    return order + {1: "B", 2: "H", 4: "I"}[size]


# ---- the handyman's fields the code reads or writes (offset, size); everything else must keep its fill
CASE_FIELDS = ((4, 1, "cellx"), (6, 1, "celly"), (0x18, 2, "posx"), (0x1A, 2, "posy"), (0x20, 4, "sprite"), (0x24, 4, "set"), (0xC2, 2, "speed"),
               (0x19C, 4, "state"), (0x1E4, 4, "grade"), (0x1FC, 4, "sturn"), (0x20C, 2, "next"), (0x210, 4, "ts"), (0x214, 2, "tcell"), (0x216, 2, "toilet"))
OBJ_FIELDS = [(off, n) for off, n, k in CASE_FIELDS if k not in ("posx", "posy")] + [(0x1A0 + 2 * i, 2) for i in range(0x20)]

# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
R = CL.recorder

FAKES = [
    (0x1012482C, R("CStaff::CStaff", "Lwi")),
    (0x10124908, R("CStaff::CStaff4", "Lwiib")),
    (0x10124B88, R("CStaff::FinalDestroy", "L")),
    (0x10124BD8, R("CStaff::ReceiveMessage", "LL")),
    (0x10124CE8, R("CStaff::Serialise", "LLiiu", ret="i")),
    (0x10125208, R("CStaff::UpdateTurn", "L")),
    (0x101252A8, R("CStaff::SetState", "Li")),
    (0x101254F4, R("CStaff::ModelState", "L")),
    (0x101272B0, R("CStaff::DoGeneralStaffThing", "L", ret="i")),
    (0x10126CB4, R("CStaff::TooTiredToWork", "L", ret="i")),
    (0x10126E28, R("CStaff::GetTiredWorking", "L")),
    (0x10126D1C, R("CStaff::GetTiredPatrolling", "L")),
    (0x10126008, R("CStaff::ProcessWalkingToRestArea", "L")),
    (0x101262A8, R("CStaff::ProcessResting", "L")),
    (0x10114FA8, R("CPeep::PeepWalks", "L", ret="i")),
    (0x10115B80, R("CPeep::PlaySound", "Lii")),
    (0x10113870, R("CPeep::SetRandomDestination", "L", ret="i")),
    (0x10115400, R("CPeep::SetDest", "Lw", ret="i")),
    (0x1010C4D4, R("CObject::AssignStaffMember", "Lw")),
    (0x1010C554, R("CObject::UnassignStaffMember", "L")),
    (0x1010C900, R("CObject::IsDirty", "L", ret="i")),
    (0x1010C980, R("CObject::IsFlaggedForMaintenance", "L", ret="i")),
    (0x1010BDA8, R("CObject::CleanFully", "L")),
    (0x1010ACDC, R("CObject::Reopen", "L")),
    (0x1013EF14, R("CWorld::Register", "xL")),
    (0x1013EFB8, R("CWorld::Unregister", "xL")),
    (0x1013B1E4, R("CWorld::GetRand", "x", ret="i")),
    (0x1013EBC8, R("CWorld::GetParkAnalyser", "x", ret=lambda e, L: 0x4000)),
]
for address, fake in FAKES:
    STUBS.internal(address, fake)


def hidden_thing_id(name, this_reg=4):
    """A fake that returns a ThingId through the hidden pointer r3 (this in r4)."""
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(this_reg)),))
        e.write(e.r(3), struct.pack(">H", L.ret(name, 0) & 0xFFFF))
    return stub


def stub_nearby(e):
    """CParkAnalyser::GetNearbyLitteredCell: the hidden pointer r3, the analyser r4, the ids in the HIGH halves of r5 and r6, the radius r7."""
    L = e.cl
    L.rec("CParkAnalyser::GetNearbyLitteredCell", ((e.r(5) >> 16) & 0xFFFF, (e.r(6) >> 16) & 0xFFFF, s32(e.r(7))))
    e.write(e.r(3), struct.pack(">H", L.ret("CParkAnalyser::GetNearbyLitteredCell", 0) & 0xFFFF))


def stub_pair(name, answers):
    """SetDest(MapPos, MapPos) and Teleport(MapPos, MapPos): r3 the peep, r4 and r5 the two bytes in the HIGH halves."""
    def stub(e):
        L = e.cl
        a, b = e.r(4), e.r(5)
        L.rec(name, (L.lab(e.r(3)), (a >> 24) & 0xFF, (a >> 16) & 0xFF, (b >> 24) & 0xFF, (b >> 16) & 0xFF))
        if answers:
            e.set_r(3, L.ret(name, 0) & MASK)
    return stub


def stub_mark(e):
    L = e.cl
    L.rec("CMapCell::MarkForLitterCollection", (s32(e.r(3) - L.mapbase), (e.r(4) >> 16) & 0xFFFF))


def stub_remove(e):
    L = e.cl
    L.rec("CMapCell::RemoveLitter", (s32(e.r(3) - L.mapbase),))


def stub_thoughts(e):
    L = e.cl
    L.rec("CThoughts::ProcessThoughtBubbles", (s32(e.r(3) - L.base),))


def stub_seed(e):
    e.cl.rec("CWorld::SeedRand", (e.r(4) & MASK,))


def stub_spawn(e):
    L = e.cl
    L.rec("CDelayedScript::SpawnSpriteScript", (s32(e.r(3) - L.base), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))


def stub_set_script(e):
    L = e.cl
    L.rec("CDelayedScript::SetSpriteScript", (s32(e.r(3) - L.base), s32(e.r(4))))


def stub_format(e):
    L = e.cl
    this, out, size, sid = e.r(3), e.r(4), e.r(5), e.r(6)
    L.rec("TbITable::FormatString", (L.lab(this), size & MASK, sid & MASK))
    if size == 0:
        e.set_r(3, 0)
        return
    kept = L.text[:size - 1]
    e.write(out, b"".join(struct.pack(">H", ord(ch)) for ch in kept) + b"\0\0")
    e.set_r(3, len(kept))


def stub_var(name):
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), s32(e.r(4) - L.base), s32(e.r(6))), (e.cstr(e.r(5))[:159],))
        e.set_r(3, L.ret("vans", 0) & MASK)
    return stub


def count_ints(text):
    return text.count("%d")


def stub_assert(e):
    text = e.cstr(e.r(4))[:159]
    ints = [s32(e.r(3))] + [e.r(5 + i) & MASK for i in range(count_ints(text))]
    e.cl.rec("GAME_ASSERTMSG", tuple(ints), (text,))


def stub_ctrace(e):
    text = e.cstr(e.r(4))[:159]
    ints = [e.r(3) & MASK] + [e.r(5 + i) & MASK for i in range(count_ints(text))]
    if text.startswith("Handyman %d at") and len(ints) > 1:
        ints[1] = (ints[1] >> 16) & 0xFFFF         # here the ThingId argument sits in the high half of its word (a struct passed by value)
    elif text.startswith("Handyman %d:") and len(ints) > 1:
        ints[1] &= 0xFFFF                           # here it was widened from a halfword (the high half is whatever the register held)
    e.cl.rec("CTRACE", tuple(ints), (text,))


def stub_trace(e):
    e.cl.rec("TRACE", (), (e.cstr(e.r(3))[:159],))


def stub_subscribe(e):
    L = e.cl
    L.rec("Subscribe", (L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5))))
    e.set_r(3, 1)


STUBS.internal(0x1012E1F8, hidden_thing_id("CThing::GetThingId"))
STUBS.internal(0x1010C5A0, hidden_thing_id("CObject::GetAssignedStaffMember"))
STUBS.internal(0x100E6D38, stub_nearby)
STUBS.internal(0x101154F0, stub_pair("CPeep::SetDestPos", True))
STUBS.internal(0x101159D0, stub_pair("CPeep::Teleport", False))
STUBS.internal(0x101033A0, stub_mark)
STUBS.internal(0x10103540, stub_remove)
STUBS.internal(0x1012F058, stub_thoughts)
STUBS.internal(0x1013B244, stub_seed)
STUBS.internal(0x100FAB9C, stub_spawn)
STUBS.internal(0x100FAA74, stub_set_script)
STUBS.internal(0x10152388, stub_subscribe)
STUBS.internal(0x1000F02C, stub_var("LOADVAR<unsigned long>"))
STUBS.internal(0x1000F5DC, stub_var("SAVEVAR<unsigned long>"))
STUBS.internal(0x1000EB4C, stub_var("LOADVARSTRUCT<ThingId>"))
STUBS.internal(0x1000E9D4, stub_var("SAVEVARSTRUCT<ThingId>"))
STUBS.internal(0x100F53CC, stub_var("LOADVARSTRUCT<MapId>"))
STUBS.internal(0x100F58C0, stub_var("SAVEVARSTRUCT<MapId>"))
STUBS.internal(0x1000DDE8, stub_assert)
STUBS.internal(0x1006FCBC, stub_ctrace)
STUBS.internal(0x1006FCFC, stub_trace)
STUBS.imp("FormatString__8TbITableCFPwUiUle", stub_format)
STUBS.imp("labs", lambda emu: emu.set_r(3, T.u32(abs(s32(emu.r(3))))))


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)

    def get_type(emu):
        emu.cl.rec("GetType", (emu.cl.lab(emu.r(3)),))
        emu.set_r(3, T.u32(emu.cl.ret("GetType", 0)))

    # The message's vtable: slot 2 (+8) is GetType, called through the glue with a TVector like every virtual.
    tvec = T.fake_function(e, get_type)
    e.hm_vtable = T.persist_alloc(e, 0x20)
    e.write(e.hm_vtable + 8, struct.pack(">I", tvec))


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
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.text = case["text"]
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

    def setup(self, eb, tail, table, centre, mapblock):
        self.L.base = eb
        self.L.mapbase = mapblock + CELL
        self.world = (tail - TAIL0) & MASK
        self.put(G_WORLD, 4, self.world)
        self.put(G_MAP, 4, mapblock + CELL)
        self.put(G_TEXT + 0x578, 4, table)
        self.put(G_CENTRE, 4, centre)

    def message(self, ident):
        m = self.alloc(16)
        self.write(m, struct.pack(">IIHH", self.e.hm_vtable, 0x5A5A5A5A, ident, 0xA5A5))
        return m

    def centre(self):
        word = self.get(G_CENTRE, 4)
        label = self.L.lab(word)
        return label if label >= 0 else (1 << 33) | word

    def globals(self):
        return [self.get(ADDR["guard"], 1), self.get(ADDR["sprite"], 4)]

    def call(self, entry, args):
        return T.call_ppc(self.e, entry, [a & MASK for a in args], max_insns=20_000_000)[0] & MASK


class Host(Side):
    order = "<"
    bal_stride = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        if name in ("guard", "sprite"):
            return None
        return self.lib.tpw_hm_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_hm_alloc(n)

    def label(self, address, n):
        self.lib.tpw_cl_label(n, address)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def row(self, row, ptr):
        self.lib.tpw_hm_setrow(row, ptr)

    def setup(self, eb, tail, table, centre, mapblock):
        self.lib.tpw_hm_setup(eb, tail, self.text.encode(), centre, mapblock + CELL)
        self.world = tail

    def message(self, ident):
        return self.lib.tpw_hm_makemsg(ident)

    def centre(self):
        return self.lib.tpw_hm_centre()

    def globals(self):
        out = (C.c_uint32 * 2)()
        self.lib.tpw_hm_globals(out)
        return list(out)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_hm_"
    u64, i32, u32 = C.c_uint64, C.c_int32, C.c_uint32
    for name, res, args in (
        ("addr", u64, [C.c_char_p]), ("alloc", u64, [u32]), ("makemsg", u64, [u32]),
        ("setup", None, [u64, u64, C.c_char_p, u64, u64]), ("setrow", None, [i32, u64]), ("centre", C.c_int64, []), ("setglobals", None, [u32, u32]),
        ("globals", None, [C.POINTER(u32)]), ("sinit", None, [C.POINTER(u32)]),
        ("ctor1", u64, [u64, u32]), ("ctor2", u64, [u64, u32, i32, u32, i32]), ("init", None, [u64, u32]), ("receive", None, [u64, u64]),
        ("serialise", i32, [u64, u64, i32, i32, u32]), ("final", None, [u64]), ("update", None, [u64]), ("choose", None, [u64]), ("model", None, [u64]),
        ("setstate", None, [u64, i32]), ("idle", None, [u64]), ("patrol", None, [u64]), ("tolitter", None, [u64]), ("cleanlitter", None, [u64]),
        ("totoilet", None, [u64]), ("cleantoilet", None, [u64]), ("find", u32, [u64]),
    ):
        fn = getattr(lib, p + name)
        fn.restype, fn.argtypes = res, args
    lib.tpw_hm_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_hm_layout(out)
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
    file = s.alloc(16)
    table = s.alloc(64) if isinstance(s, Ppc) else s.g("table")
    centre = s.alloc(CENTRE_SIZE) if c["centre"] else 0
    mapblock = s.alloc(MAP_BLOCK)
    s.write(mapblock, fill(c["seed"] + 3, MAP_BLOCK))
    s.mapblock = mapblock
    for k in range(NCELL):
        s.put(mapblock + CELL * k + 0x2C, 4, c["lit"][k])
        s.put(mapblock + CELL * k + 0x30, 2, c["coll"][k])
    s.toilets = []
    for i, oid in enumerate(OIDS):
        t = s.alloc(TOILET)
        s.write(t, fill(c["seed"] + 10 + i, TOILET))
        o = c["objs"][i]
        nxt = 0
        if i in c["chain"]:
            at = c["chain"].index(i)
            nxt = OIDS[c["chain"][at + 1]] if at + 1 < len(c["chain"]) else 0
        else:
            nxt = o["link"]
        s.put(t + 4, 1, o["x"])
        s.put(t + 6, 1, o["y"])
        s.put(t + 0xC, 2, nxt)
        s.put(t + 0x2E, 2, o["flags"])
        s.put(t + 0x32, 2, o["f32"] if o["f32"] is not None else (c["cellx"] + c["celly"] * 0x80 + 1) & 0xFFFF)
        s.label(t, L_TOILET0 + i)
        s.row(oid, t)
        s.toilets.append(t)
    s.row(GHOST, 0)
    s.label(eb, L_OBJ)
    s.label(file, L_FILE)
    s.label(table, L_TABLE)
    if centre:
        s.label(centre, L_CENTRE)
    s.file = file
    s.text = c["text"]
    s.setup(eb, tail, table, centre, mapblock)
    msg = s.message(c["mid"])
    s.label(msg, L_MSG)
    s.msg = msg
    s.put(tail, 4, c["turn"])
    s.put(tail + HEAD_AT, 2, OIDS[c["chain"][0]] if c["chain"] else 0)
    s.tail = tail
    for off, size, key in CASE_FIELDS:
        if key in ("posx", "posy"):         # a MapPos (two bytes), not a number: the same BYTES on both sides
            s.write(eb + off, struct.pack(">H", c[key]))
        else:
            s.put(eb + off, size, c[key])
    for k in range(8):
        s.put(s.g("banks") + 4 * k, 4, c["banks"][k])
    for k in range(SLOT_N):
        s.put(s.g("balance_values") + s.bal_stride * (SLOT0 + k), 4, c["bal"][k])
    spare = s.alloc(0x40)
    s.row(0, spare if c["row0"] else 0)
    if isinstance(s, Ppc):
        s.put(ADDR["guard"], 1, c["ggrd"])
        s.put(ADDR["sprite"], 4, c["gspr"])
    else:
        s.lib.tpw_hm_setglobals(c["ggrd"], c["gspr"])


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
        cell = c["cell"] << 16
        if op == "ctor1":
            return int(s.call(entry, [eb, cell]) == eb)
        if op == "ctor2":
            return int(s.call(entry, [eb, cell, c["agrade"], c["flag"], c["name"]]) == eb)
        if op == "sinit":
            s.put(ADDR["sinit_low"], 4, 0xDEADBEEF)
            s.put(ADDR["sinit_high"], 4, 0xCAFEF00D)
            s.call(entry, [])
            return [s.get(ADDR["sinit_low"], 4), s.get(ADDR["sinit_high"], 4)]
        if op == "find":
            out = s.alloc(8)
            s.call(entry, [out, eb])
            return struct.unpack(">H", s.read(out, 2))[0]
        if op == "init":
            s.call(entry, [eb, c["isprite"]])
        elif op == "recv":
            s.call(entry, [eb, s.msg])
        elif op == "ser":
            return s32(s.call(entry, [eb, s.file, c["save"], c["version"], c["unused"]]))
        elif op == "setstate":
            s.call(entry, [eb, c["astate"]])
        else:
            s.call(entry, [eb])
        return 0
    lib = s.lib
    cell = c["cell"]
    if op == "ctor1":
        return int(lib.tpw_hm_ctor1(eb, cell) == eb)
    if op == "ctor2":
        return int(lib.tpw_hm_ctor2(eb, cell, s32(c["agrade"]), c["flag"] & 0xFF, s32(c["name"])) == eb)
    if op == "sinit":
        out = (C.c_uint32 * 2)()
        lib.tpw_hm_sinit(out)
        return list(out)
    if op == "find":
        return lib.tpw_hm_find(eb)
    if op == "init":
        lib.tpw_hm_init(eb, c["isprite"] & 0xFF)
    elif op == "recv":
        lib.tpw_hm_receive(eb, s.msg)
    elif op == "ser":
        return lib.tpw_hm_serialise(eb, s.file, c["save"], s32(c["version"]), c["unused"])
    elif op == "setstate":
        lib.tpw_hm_setstate(eb, s32(c["astate"]))
    else:
        getattr(lib, "tpw_hm_" + op)(eb)
    return 0


def observe(s, c, answer):
    out = {"answer": answer, "log": s.log(), "centre": s.centre(), "globals": s.globals()}
    out["obj"] = snapshot(s, s.eb - GUARD, OBJ_BLOCK, OBJ_FIELDS)
    out["tail"] = snapshot(s, s.tail, TAIL, [(0, 4), (HEAD_AT, 2)], 0)
    out["map"] = snapshot(s, s.mapblock, MAP_BLOCK, [(CELL * k + o, n) for k in range(NCELL) for o, n in ((0x2C, 4), (0x30, 2))], 0)
    out["toilets"] = [snapshot(s, t, TOILET, [(4, 1), (6, 1), (0xC, 2), (0x2E, 2), (0x32, 2)], 0) for t in s.toilets]
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

U32 = [0, 1, 2, 3, 0xF, 0x10, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, MASK, 15000]
STATES = [0xFFFFFFFF, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0x7FFFFFFF, 0x80000000]
GRADES = [0, 1, 2, 3, 4, 5, 6, MASK]
TEXTS = ["", "A", "Zed the Great", "x" * 31, "y" * 40, "Mimi"]
RANDS = [0, 1, 2, 3, 9, 10, 11, 0x0F, 0x10, 0x7F, 0x80, 0x8D, 0xFFFFFFFF, 0x80000000, 0xFFFFFFFE]


def u32(r):
    return r.choice(U32 + [r.getrandbits(32), r.getrandbits(16), r.getrandbits(8)])


def qlist(r, values):
    return [r.choice(values) for _ in range(r.randrange(1, 4))]


def base(r, op):
    cell = r.choice([0, 1, 2, 0x80, 0x81, 0x4000, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16)])
    cellx = r.choice([0, 1, 5, 14, 0x7F, 0x80, 0xFF, r.getrandbits(8)])
    celly = r.choice([0, 0, 1, 0x7F, 0x80, 0xFF, r.getrandbits(8)])
    meid = r.choice([0, 1, 0x2FF, 0xFFFF, r.getrandbits(16), 5])
    other = r.choice([0, 1, 7, 0x2FE, r.getrandbits(16)])
    toilet = r.choice([0, GHOST, OIDS[r.randrange(8)], OIDS[r.randrange(8)], OIDS[r.randrange(8)]])
    c = {"op": op, "seed": r.getrandbits(32), "cell": cell, "cellx": cellx, "celly": celly, "posx": r.getrandbits(16), "posy": r.getrandbits(16),
         "sprite": u32(r), "set": u32(r), "speed": r.getrandbits(16), "state": r.choice(STATES), "grade": r.choice(GRADES), "sturn": u32(r),
         "next": r.getrandbits(16), "ts": r.choice([0, 1, 2, 5, MASK, u32(r)]), "tcell": r.choice([0, 1, 7, 15, r.randrange(0, 16)]),
         "toilet": toilet, "row0": r.random() < 0.2, "turn": u32(r),
         "bal": [r.choice([0, 1, 5, 100, 0x7FFFFFFF, MASK, 0x80000000, r.getrandbits(32), r.getrandbits(8)]) for _ in range(SLOT_N)],
         "banks": [u32(r) for _ in range(8)], "text": r.choice(TEXTS), "agrade": r.choice([0, 1, 2, 3, 4, MASK, 5, u32(r)]),
         "name": r.choice([MASK, 0, 1, 0x22, 0x23, 0x24, 0x7FFFFFFF, 0x80000000, r.randrange(0, 0x23), r.randrange(0, 0x40)]), "flag": r.choice([0, 1, 7, 0xFF, 0x1FF]),
         "isprite": r.choice([0, 1, 5, 0xFF, 0x1FF, MASK, r.getrandbits(8)]), "ggrd": r.choice([0, 1, 0x7F, 0x80, 0xFF]), "gspr": r.choice([0, 1, 5, 0xFF, MASK, u32(r)]),
         "save": r.choice([0, 1, 2, MASK, 0x80000000]), "version": r.choice([MASK, 0, 1, 2, 3, 0x7FFFFFFF, 0x80000000, 5]), "unused": u32(r),
         "astate": r.choice(STATES), "centre": r.random() < 0.5, "mtype": r.choice([10, 10, 9, 11, 0, MASK]), "mid": 0, "meid": meid}
    c["mid"] = r.choice([toilet, toilet, r.getrandbits(16), 0])
    c["lit"] = [r.choice([0, 0, 1, 2, 7, 8, r.getrandbits(32)]) for _ in range(NCELL)]
    c["coll"] = [r.choice([meid, meid, other, 0, 0xFFFF]) for _ in range(NCELL)]
    c["objs"] = [{"x": r.choice([0, 1, 5, 10, 40, 0x7F, 0x80, r.getrandbits(8)]), "y": r.choice([0, 0, 1, 3, 40, r.getrandbits(8)]),
                  "flags": r.choice([1, 1, 3, 0, 0x100, 0x1FF, r.getrandbits(16)]), "link": r.getrandbits(16),
                  "f32": r.choice([None, None, r.getrandbits(16), 1, 15])} for _ in OIDS]
    c["chain"] = r.sample(range(8), r.randrange(0, 7))
    c["queues"] = {
        "CWorld::GetRand": qlist(r, RANDS),
        "CStaff::DoGeneralStaffThing": qlist(r, [0, 0, 1, MASK]), "CStaff::TooTiredToWork": qlist(r, [0, 0, 1, 5]),
        "CPeep::SetRandomDestination": qlist(r, [0, 1, 7]), "CPeep::PeepWalks": qlist(r, [0, 1, 2, 3, MASK, 4]),
        "CPeep::SetDest": qlist(r, [0, 1, 5]), "CPeep::SetDestPos": qlist(r, [0, 1, 1, 2, 5, MASK]),
        "CParkAnalyser::GetNearbyLitteredCell": qlist(r, [0, 0, 1, 5, 15, 0xFFFF, r.randrange(0, 16)]),
        "CObject::GetAssignedStaffMember": qlist(r, [0, meid, meid, other]), "CObject::IsDirty": qlist(r, [0, 1, 1, 5]),
        "CObject::IsFlaggedForMaintenance": qlist(r, [0, 0, 1, 3]),
        "CStaff::Serialise": qlist(r, [0, 0, 1, MASK]), "vans": qlist(r, [0, 0, 1, MASK, 7]),
        "GetType": [c["mtype"]], "newnull": [r.choice([0, 0, 1])], "CThing::GetThingId": [meid],
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
    """Edge cases: the product of the axes (a queue name, or "f:field" for a case field) over a random base case."""
    def edges():
        r = random.Random(seed)
        keys = list(axes)
        for combo in itertools.product(*[axes[k] for k in keys]):
            c = base(r, op)
            for k, v in zip(keys, combo):
                if k.startswith("f:"):
                    c[k[2:]] = v
                else:
                    c["queues"][k] = list(v) if isinstance(v, (list, tuple)) else [v]
            if extra:
                extra(r, c)
            yield c
    return edges


def bal_index(slot, stride, grade):
    return slot + stride * s32(grade) - SLOT0


def limit_turn(slot, stride, field):
    """A case whose turn is at, below or above `field + balance[slot + stride * grade]` (wrapping)."""
    def tweak(r, c):
        idx = bal_index(slot, stride, c["grade"])
        if 0 <= idx < SLOT_N:
            c["turn"] = (c[field] + c["bal"][idx] + r.choice([-1, 0, 1, 2, -2, 0x7FFFFFFF])) & MASK
    return tweak


def near_cells(r, c):
    c["cellx"], c["celly"] = r.randrange(0, 15), 0


def idle_tweak(r, c):
    c["state"] = 0
    limit_turn(186, 4, "sturn")(r, c)


def update_tweak(r, c):
    c["state"] = r.choice([0, 0, 0, 1, 8])
    idx = bal_index(186, 4, c["grade"])
    if 0 <= idx < SLOT_N and r.random() < 0.7:
        c["bal"][idx] = r.choice([0, 1, 2, 3, 7, 100, MASK])
        interval = c["bal"][idx]
        c["turn"] = (interval * r.randrange(0, 4) + r.choice([0, 0, 1])) & MASK if interval else r.choice([0, 1, 5])


def choose_tweak(r, c):
    c["queues"]["CParkAnalyser::GetNearbyLitteredCell"] = qlist(r, [0, 0, 1, 5, 15, 0xFFFF, r.randrange(0, 16)])


def tolitter_tweak(r, c):
    near_cells(r, c)
    c["queues"]["CPeep::PeepWalks"] = qlist(r, [0, 0, 1, 2, 3])
    if r.random() < 0.6:
        c["coll"][c["tcell"]] = c["meid"]
    if r.random() < 0.7:
        c["lit"][c["tcell"]] = r.choice([1, 2, 8])


def cleanlitter_tweak(r, c):
    near_cells(r, c)
    limit_turn(233, 3, "ts")(r, c)
    if r.random() < 0.5:
        c["lit"][c["cellx"] + 1] = r.choice([1, 2, 7])


def totoilet_tweak(r, c):
    c["toilet"] = OIDS[r.randrange(8)]
    c["queues"]["CPeep::PeepWalks"] = qlist(r, [0, 0, 1, 2, 3])
    c["objs"][OIDS.index(c["toilet"])]["f32"] = r.choice([None, None, 3])
    if r.random() < 0.5:
        c["cellx"], c["celly"] = 2, 0


def cleantoilet_tweak(r, c):
    c["toilet"] = r.choice([0, GHOST, OIDS[r.randrange(8)]])
    limit_turn(233, 3, "ts")(r, c)


def recv_tweak(r, c):
    if r.random() < 0.7:
        c["mid"] = c["toilet"]


def find_tweak(r, c):
    c["queues"]["CObject::GetAssignedStaffMember"] = qlist(r, [0, c["meid"], c["meid"], 3])
    c["queues"]["CPeep::SetDest"] = qlist(r, [0, 1, 1])


def model_tweak(r, c):
    c["toilet"] = OIDS[r.randrange(8)]
    c["queues"]["CPeep::PeepWalks"] = qlist(r, [0, 0, 1, 2, 3])
    near_cells(r, c)
    if r.random() < 0.5:
        limit_turn(186, 4, "sturn")(r, c)


def model_edges():
    r = random.Random(24)
    for st in STATES + [0, 0, 1, 1, 2, 3, 8, 9, 10, 11]:
        for grade in (0, 4):
            for delta in (-1, 0, 1):
                c = base(r, "model")
                c["state"], c["grade"] = st, grade
                model_tweak(r, c)
                c["turn"] = (c["sturn"] + c["bal"][bal_index(186, 4, grade)] + delta) & MASK
                yield c


def idle_edges():
    r = random.Random(25)
    for grade in (0, 1, 4, 5, MASK):
        for delta in (-1, 0, 1):
            for rand in (0, 0x10, 1):
                for general in (0, 1):
                    c = base(r, "idle")
                    c["state"], c["grade"] = 0, grade
                    c["turn"] = (c["sturn"] + c["bal"][bal_index(186, 4, grade)] + delta) & MASK
                    c["queues"]["CWorld::GetRand"] = [rand]
                    c["queues"]["CStaff::DoGeneralStaffThing"] = [general]
                    yield c


def update_edges():
    r = random.Random(18)
    for state in (0, 1):
        for interval in (0, 1, 2, 7, MASK):
            for multiple in (True, False):
                for gnlc in (0, 5):
                    for grade in (0, 3):
                        c = base(r, "update")
                        c["state"], c["grade"] = state, grade
                        c["bal"][bal_index(186, 4, grade)] = interval
                        c["turn"] = (interval * 2 if multiple else interval * 2 + 1) & MASK if interval else (0 if multiple else 5)
                        c["queues"]["CParkAnalyser::GetNearbyLitteredCell"] = [gnlc]
                        yield c


def cleaning_edges(op, seed, slot_field):
    def edges():
        r = random.Random(seed)
        for delta in (-1, 0, 1, 2):
            for lit in (0, 1):
                for grade in (0, 3, 6):
                    for tcell in (0, 3):
                        c = base(r, op)
                        c["grade"], c["tcell"] = grade, tcell
                        near_cells(r, c)
                        c["lit"][c["cellx"] + 1] = lit
                        c["turn"] = (c["ts"] + c["bal"][bal_index(233, 3, grade)] + delta) & MASK
                        if op == "cleantoilet":
                            c["toilet"] = r.choice([0, GHOST, 21])
                        yield c
    return edges


def ser_edges():
    r = random.Random(15)
    for save in (0, 1):
        for version in (MASK, 0, 1, 2, 0x7FFFFFFF, 0x80000000):
            for base_ans in (0, 1):
                for vans in ([0, 0, 0, 0], [1], [0, 1], [0, 0, 1], [0, 0, 0, 1]):
                    c = base(r, "ser")
                    c["save"], c["version"] = save, version
                    c["queues"]["CStaff::Serialise"] = [base_ans]
                    c["queues"]["vans"] = vans
                    yield c


def tolitter_edges():
    r = random.Random(30)
    for coll in (True, False):
        for lit in (0, 1, 8):
            for walks in (0, 1, 2, 3):
                for tcell in (0, 1, 7, 15):
                    c = base(r, "tolitter")
                    near_cells(r, c)
                    c["tcell"] = tcell
                    c["coll"][tcell] = c["meid"] if coll else (c["meid"] ^ 1)
                    c["lit"][tcell] = lit
                    c["queues"]["CPeep::PeepWalks"] = [walks]
                    yield c


def totoilet_edges():
    r = random.Random(31)
    for walks in (0, 1, 2, 3):
        for same in (True, False):
            for assigned in ("me", "other", "none"):
                for general in (0, 1):
                    c = base(r, "totoilet")
                    totoilet_tweak(r, c)
                    c["cellx"], c["celly"] = 2, 0
                    c["objs"][OIDS.index(c["toilet"])]["f32"] = 3 if same else 9
                    c["queues"]["CPeep::PeepWalks"] = [walks]
                    c["queues"]["CObject::GetAssignedStaffMember"] = [{"me": c["meid"], "other": c["meid"] ^ 1, "none": 0}[assigned]]
                    yield c


def find_edges():
    r = random.Random(32)
    for n in (0, 1, 2, 3, 6):
        for dest in ([0], [1], [1, 0], [0, 1]):
            for maint in ([0], [1], [1, 0]):
                for dirty in ([0], [1]):
                    for assigned in ("me", "other", "none", "mix"):
                        c = base(r, "find")
                        c["chain"] = r.sample(range(8), n)
                        for o in c["objs"]:
                            o["flags"] = r.choice([1, 1, 1, 0])
                        c["queues"]["CPeep::SetDest"] = dest
                        c["queues"]["CObject::IsFlaggedForMaintenance"] = maint
                        c["queues"]["CObject::IsDirty"] = dirty
                        c["queues"]["CObject::GetAssignedStaffMember"] = {
                            "me": [c["meid"]], "other": [c["meid"] ^ 1], "none": [0], "mix": [0, c["meid"], c["meid"] ^ 1]}[assigned]
                        yield c


def equal_distance_edges():
    r = random.Random(33)
    for n in (2, 3):
        c = base(r, "find")
        c["chain"] = list(range(n))
        for o in c["objs"]:
            o["flags"], o["x"], o["y"] = 1, 5, 2
        c["cellx"], c["celly"] = 3, 1
        c["queues"]["CObject::GetAssignedStaffMember"] = [0]
        c["queues"]["CObject::IsFlaggedForMaintenance"] = [1]
        c["queues"]["CPeep::SetDest"] = [1]
        yield c


def find_all_edges():
    yield from find_edges()
    yield from equal_distance_edges()


def sinit_edges():
    yield base(random.Random(16), "sinit")


TESTS = [
    FnTest("CHandyman::CHandyman 0x100fefd0", ppc, host, edge=grid(11, "ctor1", {"f:centre": [False, True], "newnull": [0, 1], "f:ggrd": [0, 1, 0x80, 0xFF],
                                                                              "f:gspr": [0, 5, 0x1FF, MASK]}), rnd=make_random("ctor1", choose_tweak)),
    FnTest("CHandyman::CHandyman 0x100ff0a4", ppc, host, edge=grid(12, "ctor2", {"f:centre": [False, True], "newnull": [0, 1],
                                                                              "f:name": [MASK, 0, 1, 0x22, 0x23, 0x24, 0x7FFFFFFF, 0x80000000],
                                                                              "f:text": ["", "Mimi", "q" * 40]}), rnd=make_random("ctor2", choose_tweak)),
    FnTest("CHandyman::Init", ppc, host, edge=grid(19, "init", {"f:centre": [False, True], "newnull": [0, 1], "CStaff::DoGeneralStaffThing": [0, 1],
                                                              "f:isprite": [0, 5, 0xFF, 0x1FF]}), rnd=make_random("init", choose_tweak)),
    FnTest("CHandyman::FinalDestroy", ppc, host, edge=grid(17, "final", {"f:toilet": [0, 20, GHOST, 27], "f:row0": [False, True], "f:state": [0, 11]}),
           rnd=make_random("final")),
    FnTest("CHandyman::ReceiveMessage", ppc, host, edge=grid(20, "recv", {"f:mtype": [10, 9, 11, 0, MASK], "f:toilet": [0, 20, GHOST], "f:state": [11, 0, 3, 10],
                                                                       "f:mid": [0, 20, GHOST, 21]}, lambda r, c: c["queues"].update({"GetType": [c["mtype"]]})),
           rnd=make_random("recv", recv_tweak)),
    FnTest("CHandyman::Serialise", ppc, host, edge=ser_edges, rnd=make_random("ser")),
    FnTest("CHandyman::UpdateTurn", ppc, host, edge=update_edges, rnd=make_random("update", update_tweak)),
    FnTest("CHandyman::ChooseNextAction", ppc, host,
           edge=grid(14, "choose", {"CStaff::DoGeneralStaffThing": [0, 1], "CStaff::TooTiredToWork": [0, 1], "CParkAnalyser::GetNearbyLitteredCell": [0, 1, 5, 15, 0x80],
                                    "CPeep::SetDestPos": [0, 1, 2], "CPeep::SetRandomDestination": [0, 1],
                                    "CWorld::GetRand": [[0], [1, 9], [10, 0x7F], [0x80, 0x8D], [9, 1]]}), rnd=make_random("choose", choose_tweak)),
    FnTest("CHandyman::SetState", ppc, host, edge=grid(13, "setstate", {"f:astate": STATES, "f:grade": [0, 3], "f:turn": [0, 5, MASK], "f:toilet": [0, 21]}),
           rnd=make_random("setstate")),
    FnTest("CHandyman::ModelState", ppc, host, edge=model_edges, rnd=make_random("model", model_tweak)),
    FnTest("CHandyman::ProcessPatrolling", ppc, host, edge=grid(26, "patrol", {"CPeep::PeepWalks": [0, 1, 2, 3, MASK], "CWorld::GetRand": [[0], [1], [0x10], [0x20, 0]],
                                                                            "CStaff::DoGeneralStaffThing": [0, 1], "CParkAnalyser::GetNearbyLitteredCell": [0, 5]}),
           rnd=make_random("patrol", choose_tweak)),
    FnTest("CHandyman::ProcessWalkingToLitter", ppc, host, edge=tolitter_edges, rnd=make_random("tolitter", tolitter_tweak)),
    FnTest("CHandyman::ProcessCleaningLitter", ppc, host, edge=cleaning_edges("cleanlitter", 34, "ts"), rnd=make_random("cleanlitter", cleanlitter_tweak)),
    FnTest("CHandyman::ProcessWalkingToCleanToilet", ppc, host, edge=totoilet_edges, rnd=make_random("totoilet", totoilet_tweak)),
    FnTest("CHandyman::ProcessCleaningToilet", ppc, host, edge=cleaning_edges("cleantoilet", 35, "ts"), rnd=make_random("cleantoilet", cleantoilet_tweak)),
    FnTest("CHandyman::ProcessIdle", ppc, host, edge=idle_edges, rnd=make_random("idle", idle_tweak)),
    FnTest("CHandyman::FindNearestDirtyToilet", ppc, host, edge=find_all_edges, rnd=make_random("find", find_tweak)),
    FnTest("__sinit_Handyman_cpp", ppc, host, edge=sinit_edges, rnd=lambda r: base(r, "sinit")),
]
