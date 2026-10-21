"""Unit w178, against libtpw_unit_Entertainer.dylib (tests/unit_Entertainer.cpp), main app ("SimTheme Park"):
  src/game/people/Entertainer.cpp      CEntertainer::CEntertainer (cell) 0x100fb094 and (cell, grade, costume, name) 0x100fb224, CStaff::~CStaff 0x100fb194,
                           Init 0x100fb33c, ReceiveMessage 0x100fb448, Serialise 0x100fb4a8, FinalDestroy 0x100fb600, UpdateTurn 0x100fb6b0,
                           ChooseNextAction 0x100fb7a4, SetState 0x100fb934, ModelState 0x100fba44, __sinit_Entertainer_cpp 0x100fbc84
  src/game/World.cpp CWorld::Unregister for CHandyman 0x1013efb8, CMechanic 0x1013f1e8, CEntertainer 0x1013f41c, CGuard 0x1013f650,
                           CResearcher 0x1013f880 and CObject 0x1013fab4

A case is an entertainer (0x214 bytes of seeded fill between two 0x40 byte guards, the fields the code reads set from the case), the world's tail
block (the turn at world + 0x1da70c and the six list heads 0x1da73c to 0x1da746; the world pointer is shifted back to it), the main balance's slots
170 to 250, the sound banks, the two statics and a list of things in gThingArray, built twice by ONE piece of Python code: in emulator memory at the
Mac's addresses (big-endian) and in the host library's memory (little-endian). Every callee outside the 18 functions is a fake that records the same
event on both sides (tests/calllog.py), pointers into the entertainer as the label 1, the file 2, the message 3 and the string table 4, the
script and the Vehicle as offsets from the entertainer; the queues answer from the case. CWorld::Unregister runs REAL on both sides (also under
FinalDestroy), against a list whose members are the case's ids with their links and a thing table of the case; CWorld::Register is a fake.

What is compared: the answer, the event log, the entertainer's fields (decoded) and every other byte of the block, the tail block (turn, heads,
other bytes), the list members' links, the two statics and (destructor) the number of operator delete calls. __sinit_Entertainer_cpp: the two words
read back (the host needs no function: the words are constant initialised).

The only fused or rounded operations are the three fctiwz of ModelState (ROUNDING covers the saturation above INT32_MAX). Not tested: Unregister of a
thing that is not in its list (the walk never ends), ids of 0x2800 and above (outside gThingArray), a NULL this for any function but the destructor.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_Entertainer"
MASK = 0xFFFFFFFF

CTOR1, DTOR, CTOR2, INIT, RECV, SER, FINAL, UPDATE, CHOOSE, SETSTATE, MODEL, SINIT = (
    0x100FB094, 0x100FB194, 0x100FB224, 0x100FB33C, 0x100FB448, 0x100FB4A8, 0x100FB600, 0x100FB6B0, 0x100FB7A4, 0x100FB934, 0x100FBA44, 0x100FBC84)
UNREG = [0x1013EFB8, 0x1013F1E8, 0x1013F41C, 0x1013F650, 0x1013F880, 0x1013FAB4]
UNREG_NAMES = ["CHandyman", "CMechanic", "CEntertainer", "CGuard", "CResearcher", "CObject"]
HEADS = [0x30, 0x32, 0x34, 0x38, 0x36, 0x3A]     # the head halfwords in the tail block (world 0x1da73c, 3e, 40, 44, 42, 46)
LINKS = [0x20C] * 5 + [0xC]
SIZES = {CTOR1: 172, DTOR: 108, CTOR2: 176, INIT: 224, RECV: 36, SER: 292, FINAL: 124, UPDATE: 196, CHOOSE: 344, SETSTATE: 204, MODEL: 528, SINIT: 28}
SIZES.update({a: 344 for a in UNREG})
OWN = set(SIZES)

ADDR = dict(guard=0x1034C1D4, counter=0x1034C1D8, sinit_low=0x1034C1E0, sinit_high=0x1034C1DC, banks=0x102F7C94, balance_values=0x102B4568 + 8)
G_WORLD, G_MAP, G_TEXT, G_ARRAY, G_SPEED = 0x1037EC0C, 0x1037EC08, 0x1037FF18, 0x1034CBFC, 0x102A0D5A
HOST_LAYOUT = [0x214, 16, 8, 0x1DA70C, 0x1DA73C, 48]   # the host's BalanceValue is 8 bytes (the integer first), its table entry 48

GUARD = 0x40
OBJ = 0x214
OBJ_BLOCK = GUARD + OBJ + GUARD
TAIL = 0x40
TAIL0 = 0x1DA70C
MEMBER = 0x260
SLOT0, SLOT_N = 170, 81
L_OBJ, L_FILE, L_MSG, L_TABLE = 1, 2, 3, 4


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


def put_fmt(order, size):
    return order + {1: "B", 2: "H", 4: "I"}[size]


# ---- the entertainer's fields the code reads or writes (offset, size); everything else must keep its fill
OBJ_FIELDS = [(4, 1), (6, 1), (0x1E4, 4), (0x1FC, 4), (0x20, 4), (0x24, 4), (0xC0, 2), (0x19C, 4)] + [(0x1A0 + 2 * i, 2) for i in range(0x20)] + [(0x20C, 2), (0x210, 4)]
MEMBER_FIELDS = [(0xC, 2), (0x20C, 2)]


def obj_fields(op):
    """The entertainer's fields: the unregisters touch the halfword link at 0xc (the object list), the others the script handle word there."""
    return [(0xC, 2), (0xE, 2)] + OBJ_FIELDS if op in ("unreg", "final") else [(0xC, 4)] + OBJ_FIELDS


class ROUNDING(HardwareRounding):
    """HardwareRounding aimed at the unit's functions only."""

    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


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
    (0x1013F374, R("CWorld::Register", "xL")),
    (0x1013B1E4, R("CWorld::GetRand", "x", ret="i")),
    (0x1013EBC8, R("CWorld::GetParkAnalyser", "x", ret=lambda e, L: 0x4000)),
    (0x100E6C90, R("CParkAnalyser::IsNearbyThingOfType", "xiwi", ret="i")),
    (0x101016CC, R("CMap::AddRegionalEffector", "xiw")),
    (0x1010176C, R("CMap::RemoveRegionalEffector", "xiw")),
    (0x1007C990, R("ESP_GetNumberOfCustomAnims", "ii", ret="i")),
    (0x100DD9FC, R("SOUND_SendEvent", "iiiiii", ret="i")),
]
for address, fake in FAKES:
    STUBS.internal(address, fake)


def stub_last_cell(e):
    """CPeep::LastCellVisited: the two byte answer through the hidden pointer r3; this in r4."""
    L = e.cl
    L.rec("CPeep::LastCellVisited", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("CPeep::LastCellVisited", 0) & 0xFFFF))


def stub_thing_id(e):
    L = e.cl
    L.rec("CThing::GetThingId", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("CThing::GetThingId", 0) & 0xFFFF))


def stub_spawn(e):
    L = e.cl
    L.rec("CDelayedScript::SpawnSpriteScript", (s32(e.r(3) - L.base), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))


def stub_set_script(e):
    L = e.cl
    L.rec("CDelayedScript::SetSpriteScript", (s32(e.r(3) - L.base), s32(e.r(4))))


def stub_position(e):
    L = e.cl
    L.rec("SCR_GetScriptPosition", (s32(e.r(3)),))
    for k, name in enumerate(("SCR_x", "SCR_y", "SCR_z")):
        e.write(e.r(4 + k), struct.pack(">I", L.ret(name, 0) & MASK))
    e.write(e.r(7), struct.pack(">I", L.ret("SCR_mode", 0) & MASK))


def stub_vehicle(e):
    L = e.cl
    ints = [s32(e.r(3) - L.base)]
    if e.r(4) != 0:
        ints.append(s32(e.r(4)))       # the Mac passes the flag 0; the host has no flag to log
    L.rec("Vehicle::~Vehicle", ints)


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


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),), (e.cstr(e.r(4))[:159],))


def stub_trace(e):
    e.cl.rec("CTRACE", (e.r(3) & MASK,), (e.cstr(e.r(4))[:159],))


def stub_delete(e):
    e.cl.deleted.append(e.r(3))
    e.cl.rec("delete", (1,))


STUBS.internal(0x10113814, stub_last_cell)
STUBS.internal(0x1012E1F8, stub_thing_id)
STUBS.internal(0x100FAB9C, stub_spawn)
STUBS.internal(0x100FAA74, stub_set_script)
STUBS.internal(0x100D93B8, stub_position)
STUBS.internal(0x101332CC, stub_vehicle)
STUBS.internal(0x1000F02C, stub_var("LOADVAR<unsigned long>"))
STUBS.internal(0x1000F5DC, stub_var("SAVEVAR<unsigned long>"))
STUBS.internal(0x1000EB4C, stub_var("LOADVARSTRUCT<ThingId>"))
STUBS.internal(0x1000E9D4, stub_var("SAVEVARSTRUCT<ThingId>"))
STUBS.internal(0x1000DDE8, stub_assert)
STUBS.internal(0x1006FCBC, stub_trace)
STUBS.imp("FormatString__8TbITableCFPwUiUle", stub_format)
STUBS.imp("__dl__FPv", stub_delete)


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
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.deleted = []
        self.L.text = case["text"]
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n, heap=False):
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

    def setup(self, eb, tail, table):
        self.L.base = eb
        self.world = (tail - TAIL0) & MASK
        self.put(G_WORLD, 4, self.world)
        self.put(G_MAP, 4, 0x5000)
        self.put(G_TEXT + 0x580, 4, table)

    def call(self, entry, args, rounding=False):
        if rounding:
            with ROUNDING(self.e):
                return T.call_ppc(self.e, entry, [a & MASK for a in args])[0] & MASK
        return T.call_ppc(self.e, entry, [a & MASK for a in args])[0] & MASK

    def deletes(self):
        return len(self.L.deleted)


class Host(Side):
    order = "<"
    bal_stride = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        self.before = (C.c_int64 * 4)()
        lib.tpw_ent_heap(self.before)

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ent_addr(name.encode())

    def alloc(self, n, heap=False):
        return self.lib.tpw_ent_new(n) if heap else self.lib.tpw_ent_alloc(n)

    def label(self, address, n):
        self.lib.tpw_cl_label(n, address)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def row(self, row, ptr):
        self.lib.tpw_ent_setrow(row, ptr)

    def setup(self, eb, tail, table):
        self.eb, self.tail = eb, tail
        self.lib.tpw_ent_setup(eb, tail, self.text.encode())
        self.world = tail

    def call(self, entry, args, rounding=False):
        return self.lib.tpw_ent_call(entry, *args)

    def deletes(self):
        after = (C.c_int64 * 4)()
        self.lib.tpw_ent_heap(after)
        return after[2] - self.before[2]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ent_"
    for name, res, args in (
        ("addr", C.c_uint64, [C.c_char_p]), ("alloc", C.c_uint64, [C.c_uint32]), ("new", C.c_uint64, [C.c_uint32]),
        ("setup", None, [C.c_uint64, C.c_uint64, C.c_char_p]), ("setrow", None, [C.c_int32, C.c_uint64]),
        ("heap", None, [C.POINTER(C.c_int64)]), ("ctor1", C.c_uint64, [C.c_uint64, C.c_uint32]),
        ("ctor2", C.c_uint64, [C.c_uint64, C.c_uint32, C.c_int32, C.c_uint32, C.c_int32]), ("dtor", None, [C.c_uint64, C.c_int32]),
        ("init", None, [C.c_uint64, C.c_uint32]), ("receive", None, [C.c_uint64, C.c_uint64]),
        ("serialise", C.c_int32, [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]), ("final", None, [C.c_uint64]),
        ("update", None, [C.c_uint64]), ("choose", None, [C.c_uint64]), ("setstate", None, [C.c_uint64, C.c_int32]), ("model", None, [C.c_uint64]),
        ("sinit", None, [C.POINTER(C.c_uint32)]), ("unregister", None, [C.c_int32, C.c_uint64]),
    ):
        fn = getattr(lib, p + name)
        fn.restype, fn.argtypes = res, args
    lib.tpw_ent_layout.argtypes = [C.POINTER(C.c_int64)]

    def call(entry, *a):
        raise AssertionError("use the op functions")
    out = (C.c_int64 * 32)()
    n = lib.tpw_ent_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def fill(seed, n):
    r = random.Random(seed)
    return bytes(r.getrandbits(8) for _ in range(n))


def build(s, c):
    r = random.Random(c["seed"])
    heap = c["op"] == "dtor"
    if heap:       # delete needs the allocation's own address
        eb = s.alloc(OBJ, True)
        ob = eb - GUARD
        s.write(eb, fill(c["seed"], OBJ))
    else:
        ob = s.alloc(OBJ_BLOCK, False)
        s.write(ob, fill(c["seed"], OBJ_BLOCK))
        eb = ob + GUARD
    s.eb = eb
    fr = random.Random(c["seed"] + 5)
    for off, n in obj_fields(c["op"]):      # the observed fields get the same VALUE on both sides (the fill is bytes)
        s.put(eb + off, n, fr.getrandbits(8 * n))
    tail = s.alloc(TAIL)
    s.write(tail, fill(c["seed"] + 1, TAIL))
    file = s.alloc(16)
    msg = s.alloc(16)
    table = s.alloc(64) if isinstance(s, Ppc) else s.g("table")
    s.label(eb, L_OBJ)
    s.label(file, L_FILE)
    s.label(msg, L_MSG)
    s.label(table, L_TABLE)
    s.file, s.msg = file, msg
    if hasattr(s, "text") or isinstance(s, Host):
        s.text = c["text"]
    s.setup(eb, tail, table)
    s.put(tail, 4, c["turn"])
    for k, off in enumerate(HEADS):
        s.put(tail + off - 0, 2, c["heads"][k])
    s.tail = tail
    for off, size, key in (((0xC, 4, "script"),) if c["op"] not in ("unreg", "final") else ()) + ((4, 1, "cellx"), (6, 1, "celly"), (0x20, 4, "sprite"), (0x24, 4, "set"), (0xC0, 2, "speed"),
                           (0x19C, 4, "state"), (0x1E4, 4, "grade"), (0x1FC, 4, "sturn"), (0x20C, 2, "next"), (0x210, 4, "started")):
        s.put(eb + off, size, c[key])
    s.put(s.g("guard"), 1, c["guard"])
    s.put(s.g("counter"), 4, c["counter"])
    for k in range(8):
        s.put(s.g("banks") + 4 * k, 4, c["banks"][k])
    for k in range(SLOT_N):
        s.put(s.g("balance_values") + s.bal_stride * (SLOT0 + k), 4, c["bal"][k])
    # the list: the target is the entertainer's own block, the other members are small blocks
    s.members = []
    if c["op"] in ("unreg", "final"):
        kind = c["kind"]
        link = LINKS[kind]
        ids = c["ids"]
        for i, ident in enumerate(ids):
            if i == c["t"]:
                blk = eb
            else:
                blk = s.alloc(MEMBER)
                s.write(blk, fill(c["seed"] + 100 + i, MEMBER))
                for off, n in MEMBER_FIELDS:
                    s.put(blk + off, n, (c["seed"] * 31 + i * 7 + off) & 0xFFFF)
            s.members.append(blk)
            s.row(ident, blk)
        for i, ident in enumerate(ids):
            s.put(s.members[i] + link, 2, ids[i + 1] if i + 1 < len(ids) else c["lastlink"])
        s.put(tail + HEADS[kind], 2, ids[0])
        spare = s.alloc(0x40)
        s.row(0, spare if c["row0"] else 0)
    s.put(G_ARRAY, 0, 0) if False else None


def snapshot(s, base, size, fields):
    data = bytearray(s.read(base, size))
    values = []
    for off, n in fields:
        i = GUARD + off if base + GUARD == s.eb else off
        values.append(s.get(base + i, n))
        data[i:i + n] = bytes(n)
    return values, bytes(data)


def execute(s, c):
    op = c["op"]
    eb = s.eb
    if isinstance(s, Ppc):
        cell = c["cell"] << 16
        if op == "ctor1":
            return int(s.call(CTOR1, [eb, cell]) == eb)
        if op == "ctor2":
            return int(s.call(CTOR2, [eb, cell, c["agrade"], c["costume"], c["name"]]) == eb)
        if op == "dtor":
            s.call(DTOR, [0 if c["null"] else eb, c["flag"]])
            return 0
        if op == "init":
            s.call(INIT, [eb, c["costume"]])
        elif op == "recv":
            s.call(RECV, [eb, s.msg])
        elif op == "ser":
            return s32(s.call(SER, [eb, s.file, c["save"], c["version"], c["unused"]]))
        elif op == "final":
            s.call(FINAL, [eb])
        elif op == "update":
            s.call(UPDATE, [eb])
        elif op == "choose":
            s.call(CHOOSE, [eb])
        elif op == "setstate":
            s.call(SETSTATE, [eb, c["astate"]])
        elif op == "model":
            s.call(MODEL, [eb], rounding=True)
        elif op == "sinit":
            s.put(ADDR["sinit_low"], 4, 0xDEADBEEF)
            s.put(ADDR["sinit_high"], 4, 0xCAFEF00D)
            s.call(SINIT, [])
            return [s.get(ADDR["sinit_low"], 4), s.get(ADDR["sinit_high"], 4)]
        elif op == "unreg":
            s.call(UNREG[c["kind"]], [s.world, eb])
        return 0
    lib = s.lib
    cell = c["cell"]
    if op == "ctor1":
        return int(lib.tpw_ent_ctor1(eb, cell) == eb)
    if op == "ctor2":
        return int(lib.tpw_ent_ctor2(eb, cell, s32(c["agrade"]), c["costume"], s32(c["name"])) == eb)
    if op == "dtor":
        form = 2 if c["null"] else (1 if s16(c["flag"]) > 0 else 0)
        lib.tpw_ent_dtor(eb, form)
        return 0
    if op == "init":
        lib.tpw_ent_init(eb, c["costume"])
    elif op == "recv":
        lib.tpw_ent_receive(eb, s.msg)
    elif op == "ser":
        return lib.tpw_ent_serialise(eb, s.file, c["save"], s32(c["version"]), c["unused"])
    elif op == "final":
        lib.tpw_ent_final(eb)
    elif op == "update":
        lib.tpw_ent_update(eb)
    elif op == "choose":
        lib.tpw_ent_choose(eb)
    elif op == "setstate":
        lib.tpw_ent_setstate(eb, s32(c["astate"]))
    elif op == "model":
        lib.tpw_ent_model(eb)
    elif op == "sinit":
        out = (C.c_uint32 * 2)()
        lib.tpw_ent_sinit(out)
        return [out[0], out[1]]
    elif op == "unreg":
        lib.tpw_ent_unregister(c["kind"], eb)
    return 0


def observe(s, c, answer):
    out = {"answer": answer, "log": s.log()}
    if c["op"] == "sinit":
        return out
    out["globals"] = [s.get(s.g("guard"), 1), s.get(s.g("counter"), 4)]
    if c["op"] == "dtor":
        out["deleted"] = s.deletes()
        return out
    out["obj"] = snapshot(s, s.eb - GUARD, OBJ_BLOCK, obj_fields(c["op"]))
    tail_vals = [s.get(s.tail, 4)] + [s.get(s.tail + off, 2) for off in HEADS]
    traw = bytearray(s.read(s.tail, TAIL))
    traw[0:4] = bytes(4)
    for off in HEADS:
        traw[off:off + 2] = bytes(2)
    out["tail"] = (tail_vals, bytes(traw))
    out["members"] = [snapshot(s, m, MEMBER, MEMBER_FIELDS) if m != s.eb else None for m in s.members]
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
STATES = [0xFFFFFFFF, 0, 1, 2, 3, 4, 0xD, 0xE, 0xF, 0x10, 0x80000000]
TEXTS = ["", "A", "Zed the Great", "x" * 31, "y" * 40, "Mimi"]


def u32(r):
    return r.choice(U32 + [r.getrandbits(32), r.getrandbits(16), r.getrandbits(8)])


def qlist(r, values):
    return [r.choice(values) for _ in range(r.randrange(1, 4))]


def base(r, op):
    cell = r.choice([0, 1, 2, 0x80, 0x81, 0x4000, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16)])
    c = {"op": op, "seed": r.getrandbits(32), "cell": cell, "cellx": r.choice([0, 1, 0x7F, 0x80, 0xFF, r.getrandbits(8)]),
         "celly": r.choice([0, 1, 0x7F, 0x80, 0xFF, r.getrandbits(8)]), "script": u32(r), "sprite": u32(r), "set": u32(r), "speed": r.getrandbits(16),
         "state": r.choice(STATES + [u32(r)]), "grade": r.choice([0, 1, 2, 3, 4, 0, 1, 2, 3, 4, MASK, 5, 6]), "sturn": u32(r), "next": r.getrandbits(16),
         "started": u32(r), "turn": u32(r), "heads": [r.choice([0, r.getrandbits(16)]) for _ in range(6)],
         "bal": [r.choice([0, 1, 5, 100, 0x7FFFFFFF, MASK, 0x80000000, r.getrandbits(32), r.getrandbits(8)]) for _ in range(SLOT_N)],
         "banks": [u32(r) for _ in range(8)], "guard": r.choice([0, 1, 0x80, 0xFF, r.getrandbits(8)]), "counter": r.choice([0, 1, 2, 3, MASK, 0x7FFFFFFF, 0x80000000, r.getrandbits(32)]),
         "text": r.choice(TEXTS), "costume": r.choice([0, 1, 2, 3, 0x7F, 0x80, 0xFF, r.getrandbits(8)]), "agrade": r.choice([0, 1, 2, 3, 4, MASK, 5, u32(r)]),
         "name": r.choice([MASK, 0, 1, 0x22, 0x23, 0x24, 0x7FFFFFFF, 0x80000000, r.randrange(0, 0x23), r.randrange(0, 0x40)]),
         "flag": r.choice([1, 0, 2, 0x7FFF, 0x8000, 0x10000, 0x10001, MASK, 0xFFFF, 0x7FFFFFFF, 0x80000000]), "null": False,
         "save": r.choice([0, 1, 2, MASK, 0x80000000]), "version": r.choice([MASK, 0, 1, 2, 3, 0x7FFFFFFF, 0x80000000, 5]), "unused": u32(r),
         "astate": r.choice(STATES), "kind": r.randrange(6), "row0": False, "lastlink": r.choice([0, 0, r.getrandbits(16)])}
    ids = r.sample(range(1, 0x2800), r.randrange(1, 7))
    if r.random() < 0.3:
        ids[0] = r.choice([1, 0x27FF, 0x2000, 5])
        ids = list(dict.fromkeys(ids))
    c["ids"] = ids
    c["t"] = r.choice([0, len(ids) - 1, r.randrange(len(ids))])
    c["queues"] = {
        "CWorld::GetRand": qlist(r, [0, 1, 2, 3, 6, 7, 0xF, 0x10, 0x20, 0x1F, 0xFFFFFFFF, 0x80000000, 0xFFFFFFFE, r.getrandbits(32)]),
        "CStaff::DoGeneralStaffThing": qlist(r, [0, 0, 1, MASK]), "CStaff::TooTiredToWork": qlist(r, [0, 0, 1, 5]),
        "CParkAnalyser::IsNearbyThingOfType": qlist(r, [0, 1, MASK]), "CPeep::SetRandomDestination": qlist(r, [0, 1, 7]),
        "CPeep::PeepWalks": qlist(r, [0, 1, 2, 3, MASK, 4]), "CPeep::LastCellVisited": qlist(r, [cell & 0xFFFF, (cell + 1) & 0xFFFF, r.getrandbits(16), 0]),
        "ESP_GetNumberOfCustomAnims": qlist(r, [0, 1, 2, 3, 7, MASK, 0x7FFFFFFF, 0x80000000, 100]), "SOUND_SendEvent": qlist(r, [0, 5]),
        "SCR_x": qlist(r, FLOAT_BITS(r)), "SCR_y": qlist(r, FLOAT_BITS(r)), "SCR_z": qlist(r, FLOAT_BITS(r)), "SCR_mode": qlist(r, [0, 1, 2]),
        "CStaff::Serialise": qlist(r, [0, 0, 1, MASK]), "vans": qlist(r, [0, 0, 1, MASK, 7]),
        "CThing::GetThingId": [ids[c["t"]]],
    }
    return c


def FLOAT_BITS(r):
    out = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x3F800000, 0xBF800000, 0x4F000000, 0xCF000000, 0x4EFFFFFF, 0xCF000001, 0x7F7FFFFF]
    out += [struct.unpack(">I", struct.pack(">f", r.uniform(-1000, 1000)))[0] for _ in range(3)]
    return out


def around_limit(r, turn_key, limit_slot_base, c):
    """Puts the turn at, one below and one above the limit the state compares with (wrapping)."""
    pass


def make_edges(op, variants):
    def edges():
        r = random.Random(sum(map(ord, op)) * 7 + 1)
        for v in variants:
            c = base(r, op)
            c.update(v(c) if callable(v) else v)
            c["queues"]["CThing::GetThingId"] = [c["ids"][c["t"]]]
            yield c
    return edges


def make_random(op, tweak=None):
    def rnd(r):
        c = base(r, op)
        if tweak:
            tweak(r, c)
        c["queues"]["CThing::GetThingId"] = [c["ids"][c["t"]]]
        return c
    return rnd


def bal(c, slot):
    return c["bal"][slot - SLOT0 + 4 * (0 if c["grade"] in range(0, 5) else 0)]


def limit_case(slot, field):
    """A case whose turn is at, below or above `field + balance[slot + 4 * grade]`."""
    def tweak(r, c):
        g = s32(c["grade"])
        idx = slot + 4 * g - SLOT0
        if 0 <= idx < SLOT_N:
            c["turn"] = (c[field] + c["bal"][idx] + r.choice([-1, 0, 1, 2, -2, 0x7FFFFFFF])) & MASK
    return tweak


def model_tweak(r, c):
    if c["state"] == 0:
        limit_case(186, "sturn")(r, c)
    elif c["state"] == 0xE:
        limit_case(212, "started")(r, c)


def choose_tweak(r, c):
    c["grade"] = r.choice([0, 1, 2, 3, 4, 0, 1, 2, 3, 4, MASK, 5])
    c["turn"] = r.choice([c["turn"], c["turn"] & ~3, (c["turn"] & ~3) | r.randrange(1, 4)])


def unreg_tweak(r, c):
    c["row0"] = r.random() < 0.3
    c["op"] = "unreg"


def final_tweak(r, c):
    c["kind"] = 2
    c["row0"] = r.random() < 0.3


def ser_tweak(r, c):
    pass


EDGE_MODEL = [{"state": s, "queues_over": {}} for s in STATES]


def model_edges():
    r = random.Random(4242)
    for st in STATES + [0xE, 0xE, 0, 0, 1, 1, 1]:
        for grade in (0, 4):
            for delta in (-1, 0, 1):
                c = base(r, "model")
                c["state"], c["grade"] = st, grade
                if st == 0:
                    c["turn"] = (c["sturn"] + c["bal"][186 + 4 * grade - SLOT0] + delta) & MASK
                if st == 0xE:
                    c["turn"] = (c["started"] + c["bal"][212 + 4 * grade - SLOT0] + delta) & MASK
                c["queues"]["CWorld::GetRand"] = [r.choice([0, 0x10, 1, 0xF0])]
                c["queues"]["CThing::GetThingId"] = [c["ids"][c["t"]]]
                yield c


def unreg_edges(kind):
    def edges():
        r = random.Random(900 + kind)
        for n, t, row0 in ((1, 0, False), (2, 0, False), (2, 1, False), (3, 1, False), (3, 2, True), (6, 5, False), (6, 0, True), (4, 2, False), (5, 3, True)):
            c = base(r, "unreg")
            c["kind"] = kind
            c["ids"] = r.sample(range(1, 0x2800), n)
            c["t"], c["row0"] = t, row0
            c["queues"]["CThing::GetThingId"] = [c["ids"][t]]
            yield c
        c = base(r, "unreg")
        c["kind"], c["ids"], c["t"] = kind, [1, 0x27FF, 2], 1
        c["queues"]["CThing::GetThingId"] = [0x27FF]
        yield c
    return edges


def unreg_random(kind):
    def rnd(r):
        c = base(r, "unreg")
        c["kind"] = kind
        c["row0"] = r.random() < 0.3
        c["queues"]["CThing::GetThingId"] = [c["ids"][c["t"]]]
        return c
    return rnd


def dtor_edges():
    r = random.Random(77)
    for flag in (1, 0, 2, 0x7FFF, 0x8000, 0x10000, 0x10001, MASK, 0xFFFF, 0x7FFFFFFF, 0x80000000):
        c = base(r, "dtor")
        c["flag"] = flag
        yield c
    for flag in (1, 0, MASK):
        c = base(r, "dtor")
        c["flag"], c["null"] = flag, True
        yield c


def dtor_random(r):
    c = base(r, "dtor")
    c["null"] = r.random() < 0.1
    return c


def ctor1_edges():
    r = random.Random(11)
    for guard in (0, 1, 0x80, 0xFF):
        for counter in (0, 1, 2, 3, MASK, 0x7FFFFFFF, 0x80000000, 0xFF, 0x100):
            c = base(r, "ctor1")
            c["guard"], c["counter"] = guard, counter
            yield c


def ctor2_edges():
    r = random.Random(12)
    for name in (MASK, 0, 1, 0x22, 0x23, 0x24, 0x7FFFFFFF, 0x80000000):
        for text in ("", "Mimi", "q" * 40):
            c = base(r, "ctor2")
            c["name"], c["text"] = name, text
            yield c


def setstate_edges():
    r = random.Random(13)
    for st in STATES:
        for n in (0, 1, 2, 3, 7, MASK, 0x7FFFFFFF, 0x80000000):
            c = base(r, "setstate")
            c["astate"] = st
            c["queues"]["ESP_GetNumberOfCustomAnims"] = [n]
            c["queues"]["CWorld::GetRand"] = [r.choice([0, 1, 5, 0xFFFFFFFF, 0xFFFFFFFE])]
            yield c


def choose_edges():
    r = random.Random(14)
    for tired in (0, 1):
        for general in (0, 1):
            for rand in (0, 3, 1, 2, 0xFFFFFFFF, 0xFFFFFFFE):
                for near in (0, 1):
                    for turn in (0, 1, 3, 4):
                        c = base(r, "choose")
                        c["queues"]["CStaff::TooTiredToWork"] = [tired]
                        c["queues"]["CStaff::DoGeneralStaffThing"] = [general]
                        c["queues"]["CWorld::GetRand"] = [rand]
                        c["queues"]["CParkAnalyser::IsNearbyThingOfType"] = [near]
                        c["turn"] = turn
                        c["grade"] = r.choice([0, 4, 2])
                        yield c


def ser_edges():
    r = random.Random(15)
    for save in (0, 1):
        for version in (MASK, 0, 1, 2, 0x7FFFFFFF, 0x80000000):
            for base_ans in (0, 1):
                for vans in ([0, 0], [1], [0, 1]):
                    c = base(r, "ser")
                    c["save"], c["version"] = save, version
                    c["queues"]["CStaff::Serialise"] = [base_ans]
                    c["queues"]["vans"] = vans
                    yield c


def sinit_edges():
    yield base(random.Random(16), "sinit")


def simple_edges(op, seed, n=12):
    def edges():
        r = random.Random(seed)
        for _ in range(n):
            yield base(r, op)
    return edges


def final_edges():
    r = random.Random(17)
    for n, t in ((1, 0), (2, 0), (2, 1), (3, 1), (5, 4)):
        for row0 in (False, True):
            c = base(r, "final")
            c["kind"], c["row0"] = 2, row0
            c["ids"] = r.sample(range(1, 0x2800), n)
            c["t"] = t
            c["queues"]["CThing::GetThingId"] = [c["ids"][t]]
            yield c


def update_edges():
    r = random.Random(18)
    for same in (True, False):
        for _ in range(6):
            c = base(r, "update")
            here = (c["cellx"] + (c["celly"] << 7) + 1) & 0xFFFF
            c["queues"]["CPeep::LastCellVisited"] = [here if same else (here + r.choice([1, 0x8000, 0xFFFF])) & 0xFFFF]
            yield c


def init_edges():
    r = random.Random(19)
    for costume in (0, 1, 2, 0x7F, 0xFF):
        for turn in (0, 4, 5):
            c = base(r, "init")
            c["costume"], c["turn"] = costume, turn
            yield c


TESTS = [
    FnTest("CEntertainer::CEntertainer 0x100fb094", ppc, host, edge=ctor1_edges, rnd=make_random("ctor1")),
    FnTest("CStaff::~CStaff", ppc, host, edge=dtor_edges, rnd=dtor_random),
    FnTest("CEntertainer::CEntertainer 0x100fb224", ppc, host, edge=ctor2_edges, rnd=make_random("ctor2")),
    FnTest("CEntertainer::Init", ppc, host, edge=init_edges, rnd=make_random("init", choose_tweak)),
    FnTest("CEntertainer::ReceiveMessage", ppc, host, edge=simple_edges("recv", 20), rnd=make_random("recv")),
    FnTest("CEntertainer::Serialise", ppc, host, edge=ser_edges, rnd=make_random("ser")),
    FnTest("CEntertainer::FinalDestroy", ppc, host, edge=final_edges, rnd=make_random("final", final_tweak)),
    FnTest("CEntertainer::UpdateTurn", ppc, host, edge=update_edges, rnd=make_random("update")),
    FnTest("CEntertainer::ChooseNextAction", ppc, host, edge=choose_edges, rnd=make_random("choose", choose_tweak)),
    FnTest("CEntertainer::SetState", ppc, host, edge=setstate_edges, rnd=make_random("setstate")),
    FnTest("CEntertainer::ModelState", ppc, host, edge=model_edges, rnd=make_random("model", model_tweak)),
    FnTest("__sinit_Entertainer_cpp", ppc, host, edge=sinit_edges, rnd=lambda r: base(r, "sinit")),
] + [FnTest("CWorld::Unregister(%s)" % UNREG_NAMES[k], ppc, host, edge=unreg_edges(k), rnd=unreg_random(k)) for k in range(6)]
