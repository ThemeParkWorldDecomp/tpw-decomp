"""Unit w182, against libtpw_unit_Guard.dylib (tests/unit_Guard.cpp), main app ("SimTheme Park"):
  src/Guard.cpp   CGuard::CGuard (cell) 0x100fda68 and (cell, grade, flag, name) 0x100fdb08, Init 0x100fdc10, FinalDestroy 0x100fdd60, ReceiveMessage
                  0x100fdec0, Serialise 0x100fdf94, ProsecutePerp 0x100fe134, IsAvailable 0x100fe2b0, IsCarrying 0x100fe300, UpdateTurn 0x100fe344,
                  ChooseNextAction 0x100fe430, ModelState 0x100fe4e8, SetState 0x100fe5a8, ProcessIdle 0x100fe774, ProcessPatrolling 0x100fe888,
                  ProcessProsecuting 0x100fe99c, ProcessWalkingToEntrance 0x100fed5c, ProcessReturningToPatrol 0x100feebc

A case is a guard (0x214 bytes of seeded fill between two 0x40 byte guards, the fields the code reads set from the case), the world's tail block (the
turn at world + 0x1da70c; the world pointer is shifted back to it), the main balance's slots 180 to 289, the sound banks, a kid in gThingArray (a
0x40 byte block with the cell bytes at +4 and +6; row 0 sometimes non-empty so the assertion answers 0), the message centre (absent, or a block) and a
death message, built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) and in the host library's memory
(little-endian). Every callee outside the 18 functions is a fake that records the same event on both sides (tests/calllog.py), pointers into the
guard as the label 1, the file 2, the message 3, the string table 4, the kid 5 and the existing message centre 6, the script as an offset from the
guard; the queues answer from the case. The new message centre is checked through the nothrow operator new, the table pair of __construct_array and
the label of the first header.

What is compared: the answer, the event log, the guard's fields (decoded) and every other byte of the block, the tail block (turn, other bytes) and
the global message centre. Not tested: a NULL kid row where the code would dereference it (ProsecutePerp, ProcessProsecuting, ProcessWalkingToEntrance),
ids of 0x2800 and above (outside gThingArray), a NULL this.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_Guard"
MASK = 0xFFFFFFFF

CTOR1, CTOR2, INIT, FINAL, RECV, SER, PROSECUTE, ISAVAIL, ISCARRY, UPDATE, CHOOSE, MODEL, SETSTATE, IDLE, PATROL, PROSECUTING, TOENTRANCE, TORETURN = (
    0x100FDA68, 0x100FDB08, 0x100FDC10, 0x100FDD60, 0x100FDEC0, 0x100FDF94, 0x100FE134, 0x100FE2B0, 0x100FE300, 0x100FE344, 0x100FE430, 0x100FE4E8,
    0x100FE5A8, 0x100FE774, 0x100FE888, 0x100FE99C, 0x100FED5C, 0x100FEEBC)
ENTRY = dict(ctor1=CTOR1, ctor2=CTOR2, init=INIT, final=FINAL, recv=RECV, ser=SER, prosecute=PROSECUTE, isavail=ISAVAIL, iscarry=ISCARRY, update=UPDATE,
             choose=CHOOSE, model=MODEL, setstate=SETSTATE, idle=IDLE, patrol=PATROL, prosecuting=PROSECUTING, toentrance=TOENTRANCE, toreturn=TORETURN)
SIZES = {CTOR1: 84, CTOR2: 164, INIT: 300, FINAL: 308, RECV: 160, SER: 372, PROSECUTE: 292, ISAVAIL: 36, ISCARRY: 24, UPDATE: 196, CHOOSE: 136, MODEL: 152,
         SETSTATE: 400, IDLE: 232, PATROL: 228, PROSECUTING: 912, TOENTRANCE: 296, TORETURN: 156}
OWN = set(SIZES)

ADDR = dict(banks=0x102F7C94, balance_values=0x102B4568 + 8)
G_WORLD, G_MAP, G_TEXT, G_ARRAY, G_CENTRE = 0x1037EC0C, 0x1037EC08, 0x1037FF18, 0x1034CBFC, 0x1037F6C4
WIDE_CTOR_TV, WIDE_DTOR_TV, CENTRE_SIZE = 0x102657E8, 0x10267288, 0x1D0
HOST_LAYOUT = [0x214, 16, 8, 0x1DA70C, 48, CENTRE_SIZE]   # the host's BalanceValue is 8 bytes (the integer first), its table entry 48

GUARD = 0x40
OBJ = 0x214
OBJ_BLOCK = GUARD + OBJ + GUARD
TAIL = 0x40
TAIL0 = 0x1DA70C
KID = 0x40
SLOT0, SLOT_N = 180, 110
L_OBJ, L_FILE, L_MSG, L_TABLE, L_KID, L_CENTRE = 1, 2, 3, 4, 5, 6


def s32(v):
    return T.s32(v)


def put_fmt(order, size):
    return order + {1: "B", 2: "H", 4: "I"}[size]


# ---- the guard's fields the code reads or writes (offset, size); everything else must keep its fill
OBJ_FIELDS = ([(4, 1), (6, 1), (0x20, 4), (0x24, 4), (0xC2, 2), (0x188, 4), (0x19C, 4), (0x1E4, 4), (0x1FC, 4), (0x20C, 2), (0x20E, 2), (0x210, 4)]
              + [(0x1A0 + 2 * i, 2) for i in range(0x20)])
CASE_FIELDS = ((4, 1, "cellx"), (6, 1, "celly"), (0x20, 4, "sprite"), (0x24, 4, "set"), (0xC2, 2, "speed"), (0x188, 4, "f188"), (0x19C, 4, "state"),
               (0x1E4, 4, "grade"), (0x1FC, 4, "sturn"), (0x20C, 2, "next"), (0x20E, 2, "perp"), (0x210, 4, "ts"))

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
    (0x1011B508, R("CPerson::GetBagged", "L")),
    (0x1011B594, R("CPerson::GetDebagged", "Lw")),
    (0x1011B700, R("CPerson::IsCatchable", "L", ret="i")),
    (0x1013F5AC, R("CWorld::Register", "xL")),
    (0x1013F650, R("CWorld::Unregister", "xL")),
    (0x1013B1E4, R("CWorld::GetRand", "x", ret="i")),
    (0x1013EBC8, R("CWorld::GetParkAnalyser", "x", ret=lambda e, L: 0x4000)),
    (0x100E43E4, R("CParkAnalyser::IveMisbehaved", "xi")),
    (0x101016CC, R("CMap::AddRegionalEffector", "xiw")),
    (0x1010176C, R("CMap::RemoveRegionalEffector", "xiw")),
    (0x1007C900, R("ESP_GetRandomSpritesFrom", "i", ret="i")),
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


def stub_map_cell(name):
    def stub(e):
        L = e.cl
        L.rec(name, (s32(e.r(5)),))
        e.write(e.r(3), struct.pack(">H", L.ret(name, 0) & 0xFFFF))
    return stub


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


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),), (e.cstr(e.r(4))[:159],))


def stub_trace(e):
    text = e.cstr(e.r(4))[:159]
    ints = (e.r(3) & MASK, e.r(5) & MASK) if "%d" in text else (e.r(3) & MASK,)
    e.cl.rec("CTRACE", ints, (text,))


def stub_subscribe(e):
    L = e.cl
    L.rec("Subscribe", (L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5))))
    e.set_r(3, 1)


STUBS.internal(0x10113814, stub_last_cell)
STUBS.internal(0x1012E1F8, stub_thing_id)
STUBS.internal(0x100FAB9C, stub_spawn)
STUBS.internal(0x100FAA74, stub_set_script)
STUBS.internal(0x101019BC, stub_map_cell("CMap::GetTicketBoothPos"))
STUBS.internal(0x10101AE0, stub_map_cell("CMap::GetParkEntrancePos"))
STUBS.internal(0x10152388, stub_subscribe)
STUBS.internal(0x100F44A4, stub_var("LOADVAR<unsigned int>"))
STUBS.internal(0x100F45BC, stub_var("SAVEVAR<unsigned int>"))
STUBS.internal(0x1000EB4C, stub_var("LOADVARSTRUCT<ThingId>"))
STUBS.internal(0x1000E9D4, stub_var("SAVEVARSTRUCT<ThingId>"))
STUBS.internal(0x1000DDE8, stub_assert)
STUBS.internal(0x1006FCBC, stub_trace)
STUBS.imp("FormatString__8TbITableCFPwUiUle", stub_format)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)

    def get_type(emu):
        emu.cl.rec("GetType", (emu.cl.lab(emu.r(3)),))
        emu.set_r(3, T.u32(emu.cl.ret("GetType", 0)))

    # The message's vtable: slot 2 (+8) is GetType, called through the glue with a TVector like every virtual.
    tvec = T.fake_function(e, get_type)
    e.gd_vtable = T.persist_alloc(e, 0x20)
    e.write(e.gd_vtable + 8, struct.pack(">I", tvec))


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

    def setup(self, eb, tail, table, centre):
        self.L.base = eb
        self.world = (tail - TAIL0) & MASK
        self.put(G_WORLD, 4, self.world)
        self.put(G_MAP, 4, 0x5000)
        self.put(G_TEXT + 0x584, 4, table)
        self.put(G_CENTRE, 4, centre)

    def message(self, vtable_id):
        m = self.alloc(16)
        self.write(m, struct.pack(">IIHH", self.e.gd_vtable, 0x5A5A5A5A, vtable_id, 0xA5A5))
        return m

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
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gd_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_gd_alloc(n)

    def label(self, address, n):
        self.lib.tpw_cl_label(n, address)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def row(self, row, ptr):
        self.lib.tpw_gd_setrow(row, ptr)

    def setup(self, eb, tail, table, centre):
        self.lib.tpw_gd_setup(eb, tail, self.text.encode(), centre)
        self.world = tail

    def message(self, ident):
        return self.lib.tpw_gd_makemsg(ident)

    def centre(self):
        return self.lib.tpw_gd_centre()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_gd_"
    u64, i32, u32 = C.c_uint64, C.c_int32, C.c_uint32
    for name, res, args in (
        ("addr", u64, [C.c_char_p]), ("alloc", u64, [u32]), ("makemsg", u64, [u32]),
        ("setup", None, [u64, u64, C.c_char_p, u64]), ("setrow", None, [i32, u64]), ("centre", C.c_int64, []),
        ("ctor1", u64, [u64, u32]), ("ctor2", u64, [u64, u32, i32, u32, i32]), ("init", None, [u64]), ("receive", None, [u64, u64]),
        ("serialise", i32, [u64, u64, i32, i32, u32]), ("final", None, [u64]), ("prosecute", None, [u64, u32]), ("isavailable", i32, [u64]),
        ("iscarrying", i32, [u64]), ("update", None, [u64]), ("choose", None, [u64]), ("model", None, [u64]), ("setstate", None, [u64, i32]),
        ("idle", None, [u64]), ("patrol", None, [u64]), ("prosecuting", None, [u64]), ("toentrance", None, [u64]), ("toreturn", None, [u64]),
    ):
        fn = getattr(lib, p + name)
        fn.restype, fn.argtypes = res, args
    lib.tpw_gd_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gd_layout(out)
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
    kid = s.alloc(KID)
    s.write(kid, fill(c["seed"] + 2, KID))
    s.put(kid + 4, 1, c["kx"])
    s.put(kid + 6, 1, c["ky"])
    s.label(eb, L_OBJ)
    s.label(file, L_FILE)
    s.label(table, L_TABLE)
    s.label(kid, L_KID)
    if centre:
        s.label(centre, L_CENTRE)
    s.file = file
    s.text = c["text"]
    s.setup(eb, tail, table, centre)
    msg = s.message(c["mid"])
    s.label(msg, L_MSG)
    s.msg = msg
    s.put(tail, 4, c["turn"])
    s.tail = tail
    for off, size, key in CASE_FIELDS:
        s.put(eb + off, size, c[key])
    s.put(s.g("banks") + 0, 4, 0)
    for k in range(8):
        s.put(s.g("banks") + 4 * k, 4, c["banks"][k])
    for k in range(SLOT_N):
        s.put(s.g("balance_values") + s.bal_stride * (SLOT0 + k), 4, c["bal"][k])
    s.row(c["pid"], kid if c["kidrow"] else 0)
    spare = s.alloc(0x40)
    s.row(0, spare if c["row0"] else 0)


def snapshot(s, base, size, fields):
    data = bytearray(s.read(base, size))
    values = []
    for off, n in fields:
        i = GUARD + off
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
        if op == "recv":
            s.call(entry, [eb, s.msg])
        elif op == "ser":
            return s32(s.call(entry, [eb, s.file, c["save"], c["version"], c["unused"]]))
        elif op == "prosecute":
            s.call(entry, [eb, c["pid"] << 16])
        elif op in ("isavail", "iscarry"):
            return s.call(entry, [eb])
        elif op == "setstate":
            s.call(entry, [eb, c["astate"]])
        else:
            s.call(entry, [eb])
        return 0
    lib = s.lib
    cell = c["cell"]
    if op == "ctor1":
        return int(lib.tpw_gd_ctor1(eb, cell) == eb)
    if op == "ctor2":
        return int(lib.tpw_gd_ctor2(eb, cell, s32(c["agrade"]), c["flag"] & 0xFF, s32(c["name"])) == eb)
    if op == "recv":
        lib.tpw_gd_receive(eb, s.msg)
    elif op == "ser":
        return lib.tpw_gd_serialise(eb, s.file, c["save"], s32(c["version"]), c["unused"])
    elif op == "prosecute":
        lib.tpw_gd_prosecute(eb, c["pid"])
    elif op == "isavail":
        return lib.tpw_gd_isavailable(eb)
    elif op == "iscarry":
        return lib.tpw_gd_iscarrying(eb)
    elif op == "setstate":
        lib.tpw_gd_setstate(eb, s32(c["astate"]))
    else:
        getattr(lib, "tpw_gd_" + op)(eb)
    return 0


def observe(s, c, answer):
    out = {"answer": answer, "log": s.log(), "centre": s.centre()}
    out["obj"] = snapshot(s, s.eb - GUARD, OBJ_BLOCK, OBJ_FIELDS)
    traw = bytearray(s.read(s.tail, TAIL))
    traw[0:4] = bytes(4)
    out["tail"] = (s.get(s.tail, 4), bytes(traw))
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
STATES = [0xFFFFFFFF, 0, 1, 2, 3, 4, 7, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0x7FFFFFFF, 0x80000000, 5, 6, 8]
GRADES = [0, 1, 2, 3, 4, 5, 6, MASK]
TEXTS = ["", "A", "Zed the Great", "x" * 31, "y" * 40, "Mimi"]


def u32(r):
    return r.choice(U32 + [r.getrandbits(32), r.getrandbits(16), r.getrandbits(8)])


def qlist(r, values):
    return [r.choice(values) for _ in range(r.randrange(1, 4))]


def base(r, op):
    cell = r.choice([0, 1, 2, 0x80, 0x81, 0x4000, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16)])
    cellx = r.choice([0, 1, 0x7F, 0x80, 0xFF, r.getrandbits(8)])
    celly = r.choice([0, 1, 0x7F, 0x80, 0xFF, r.getrandbits(8)])
    pid = r.choice([1, 2, 0x27FF, 0x1000, r.randrange(1, 0x2800)])
    c = {"op": op, "seed": r.getrandbits(32), "cell": cell, "cellx": cellx, "celly": celly, "sprite": u32(r), "set": u32(r), "speed": r.getrandbits(16),
         "f188": u32(r), "state": r.choice(STATES), "grade": r.choice(GRADES), "sturn": u32(r), "next": r.getrandbits(16),
         "perp": r.choice([0, pid, pid, pid]), "ts": r.choice([0, 1, 2, 5, MASK, u32(r)]), "pid": pid, "kidrow": True, "row0": r.random() < 0.2,
         "kx": r.choice([cellx, cellx, r.getrandbits(8)]), "ky": r.choice([celly, celly, r.getrandbits(8)]), "turn": u32(r),
         "bal": [r.choice([0, 1, 5, 100, 0x7FFFFFFF, MASK, 0x80000000, r.getrandbits(32), r.getrandbits(8)]) for _ in range(SLOT_N)],
         "banks": [u32(r) for _ in range(8)], "text": r.choice(TEXTS), "agrade": r.choice([0, 1, 2, 3, 4, MASK, 5, u32(r)]),
         "name": r.choice([MASK, 0, 1, 0x22, 0x23, 0x24, 0x7FFFFFFF, 0x80000000, r.randrange(0, 0x23), r.randrange(0, 0x40)]), "flag": r.choice([0, 1, 7, 0xFF, 0x1FF]),
         "save": r.choice([0, 1, 2, MASK, 0x80000000]), "version": r.choice([MASK, 0, 1, 2, 3, 0x7FFFFFFF, 0x80000000, 5]), "unused": u32(r),
         "astate": r.choice(STATES), "centre": r.random() < 0.5, "mtype": r.choice([0x1b, 0x1b, 0x1a, 0x1c, 0, MASK]), "mid": 0}
    c["mid"] = r.choice([c["perp"], c["perp"], r.getrandbits(16), 0])
    c["queues"] = {
        "CWorld::GetRand": qlist(r, [0, 1, 2, 3, 6, 7, 0xF, 0x10, 0x20, 0x1F, 0xFFFFFFFF, 0x80000000, 0xFFFFFFFE, r.getrandbits(32)]),
        "CStaff::DoGeneralStaffThing": qlist(r, [0, 0, 1, MASK]), "CStaff::TooTiredToWork": qlist(r, [0, 0, 1, 5]),
        "CPeep::SetRandomDestination": qlist(r, [0, 1, 7]), "CPeep::PeepWalks": qlist(r, [0, 1, 2, 3, MASK, 4]),
        "CPeep::LastCellVisited": qlist(r, [cell & 0xFFFF, (cell + 1) & 0xFFFF, r.getrandbits(16), 0]),
        "CPeep::SetDest": qlist(r, [0, 1, 5]), "CPerson::IsCatchable": qlist(r, [0, 1, 9]), "ESP_GetRandomSpritesFrom": qlist(r, [0, 1, 2, 5, MASK]),
        "CMap::GetTicketBoothPos": qlist(r, [0, 1, 0x8001, r.getrandbits(16)]), "CMap::GetParkEntrancePos": qlist(r, [0, 1, 0x8001, r.getrandbits(16)]),
        "CStaff::Serialise": qlist(r, [0, 0, 1, MASK]), "vans": qlist(r, [0, 0, 1, MASK, 7]),
        "GetType": [c["mtype"]], "newnull": [r.choice([0, 0, 1])], "CThing::GetThingId": [r.choice([0, 1, 0x2FF, 0xFFFF, r.getrandbits(16)])],
    }
    return c


def make_random(op, tweak=None):
    def rnd(r):
        c = base(r, op)
        if tweak:
            tweak(r, c)
        return c
    return rnd


def make_edges(op, seed, variants):
    def edges():
        r = random.Random(seed)
        for v in variants(r, op):
            yield v
    return edges


def limit_turn(slot, stride, field):
    """A case whose turn is at, below or above `field + balance[slot + stride * grade]` (wrapping)."""
    def tweak(r, c):
        g = s32(c["grade"])
        idx = slot + stride * g - SLOT0
        if 0 <= idx < SLOT_N:
            c["turn"] = (c[field] + c["bal"][idx] + r.choice([-1, 0, 1, 2, -2, 0x7FFFFFFF])) & MASK
    return tweak


def idle_tweak(r, c):
    c["state"] = 0
    limit_turn(186, 4, "sturn")(r, c)
    c["turn"] = r.choice([c["turn"], c["turn"], (c["turn"] & ~3) | r.randrange(0, 4)])


def choose_tweak(r, c):
    c["turn"] = r.choice([c["turn"], c["turn"] & ~3, (c["turn"] & ~3) | r.randrange(1, 4)])


def setstate_tweak(r, c):
    c["kidrow"] = r.random() < 0.7


def prosecuting_tweak(r, c):
    c["kidrow"] = True
    c["ts"] = r.choice([0, 1, 2, 5])
    c["queues"]["CPeep::PeepWalks"] = qlist(r, [0, 0, 1, 2, 3])
    c["queues"]["CPeep::SetDest"] = qlist(r, [0, 1])
    c["queues"]["CPerson::IsCatchable"] = qlist(r, [0, 1, 1])
    c["queues"]["CMap::GetTicketBoothPos"] = qlist(r, [0, 1, 0x8001, r.getrandbits(16)])


def entrance_tweak(r, c):
    c["perp"] = c["pid"]
    c["kidrow"] = True
    c["queues"]["CPeep::PeepWalks"] = qlist(r, [0, 0, 1, 2, 3])


def prosecute_tweak(r, c):
    c["kidrow"] = True


def recv_tweak(r, c):
    if r.random() < 0.7:
        c["mid"] = c["perp"]


def model_edges(r, op):
    for st in STATES + [0, 0, 1, 1, 2, 3, 0x10, 0x12, 0x13]:
        for grade in (0, 4):
            for delta in (-1, 0, 1):
                c = base(r, op)
                c["state"], c["grade"] = st, grade
                if st == 0:
                    c["turn"] = (c["sturn"] + c["bal"][186 + 4 * grade - SLOT0] + delta) & MASK
                c["kidrow"] = True
                c["perp"] = c["pid"]
                c["ts"] = r.choice([0, 3])
                yield c


def idle_edges(r, op):
    for grade in (0, 1, 4, 5, MASK):
        for delta in (-1, 0, 1):
            for rand in (0, 0x10, 1):
                for general in (0, 1):
                    for turn_low in (0, 1):
                        c = base(r, op)
                        c["state"], c["grade"] = 0, grade
                        c["turn"] = (c["sturn"] + c["bal"][186 + 4 * s32(grade) - SLOT0] + delta) & MASK
                        c["turn"] = (c["turn"] & ~3) | turn_low
                        c["queues"]["CWorld::GetRand"] = [rand]
                        c["queues"]["CStaff::DoGeneralStaffThing"] = [general]
                        c["queues"]["CPeep::SetRandomDestination"] = [r.choice([0, 1])]
                        yield c


def patrol_edges(r, op):
    for walks in (0, 1, 2, 3, MASK):
        for general in (0, 1):
            for rand in (0, 1):
                for turn in (0, 1, 3, 4):
                    for dest in (0, 1):
                        c = base(r, op)
                        c["queues"]["CPeep::PeepWalks"] = [walks]
                        c["queues"]["CStaff::DoGeneralStaffThing"] = [general]
                        c["queues"]["CWorld::GetRand"] = [rand * 0x10]
                        c["queues"]["CPeep::SetRandomDestination"] = [dest]
                        c["turn"] = turn
                        yield c


def choose_edges(r, op):
    for general in (0, 1):
        for dest in (0, 1, 7):
            for turn in (0, 1, 2, 3, 4, 0x80000003):
                c = base(r, op)
                c["queues"]["CStaff::DoGeneralStaffThing"] = [general]
                c["queues"]["CPeep::SetRandomDestination"] = [dest]
                c["turn"] = turn
                yield c


def setstate_edges(r, op):
    for st in STATES:
        for mstate in (0x12, 3):
            for perp in (0, 1):
                for kidrow in (True, False):
                    for grade in (0, 3):
                        c = base(r, op)
                        c["astate"], c["state"], c["kidrow"], c["grade"] = st, mstate, kidrow, grade
                        c["perp"] = c["pid"] if perp else 0
                        yield c


def prosecuting_edges(r, op):
    for ts in (0, 1, 5):
        for perp in (0, 1):
            for walks in (0, 1, 2, 3):
                for same in (True, False):
                    for catch in (0, 1):
                        for sd in ((0, 0), (1, 0), (0, 1), (1, 1)):
                            c = base(r, op)
                            c["ts"], c["kidrow"] = ts, True
                            c["perp"] = c["pid"] if perp else 0
                            c["kx"], c["ky"] = (c["cellx"], c["celly"]) if same else ((c["cellx"] + 1) & 0xFF, c["celly"])
                            c["queues"]["CPeep::PeepWalks"] = [walks]
                            c["queues"]["CPerson::IsCatchable"] = [catch]
                            c["queues"]["CPeep::SetDest"] = list(sd)
                            c["queues"]["CMap::GetTicketBoothPos"] = [r.choice([0, 1, 0x8001]), r.choice([1, 0x8001])]
                            yield c
    c = base(r, op)           # the cell alias: x = 0x80, y = 0 and x = 0, y = 1 are the same cell
    c["ts"], c["perp"], c["kidrow"] = 5, c["pid"], True
    c["cellx"], c["celly"], c["kx"], c["ky"] = 0x80, 0, 0, 1
    c["queues"]["CPeep::PeepWalks"], c["queues"]["CPerson::IsCatchable"] = [0], [1]
    yield c


def entrance_edges(r, op):
    for walks in (0, 1, 2, 3, MASK):
        for turn in (0, 1):
            for dest in (0, 1):
                c = base(r, op)
                c["perp"], c["kidrow"] = c["pid"], True
                c["turn"] = turn
                c["queues"]["CPeep::PeepWalks"] = [walks]
                c["queues"]["CPeep::SetDest"] = [dest]
                yield c


def return_edges(r, op):
    for walks in (0, 1, 2, 3, MASK):
        for turn in (0, 1, 2):
            for dest in (0, 1):
                c = base(r, op)
                c["turn"] = turn
                c["queues"]["CPeep::PeepWalks"] = [walks]
                c["queues"]["CPeep::SetDest"] = [dest]
                yield c


def prosecute_edges(r, op):
    for tired in (0, 1, 5):
        for dest in (0, 1):
            for row0 in (False, True):
                c = base(r, op)
                c["kidrow"], c["row0"] = True, row0
                c["queues"]["CStaff::TooTiredToWork"] = [tired]
                c["queues"]["CPeep::SetDest"] = [dest]
                yield c


def final_edges(r, op):
    for mstate in (0x12, 0x11, 3, 0):
        for perp in (0, 1):
            for kidrow in (True, False):
                for row0 in (False, True):
                    c = base(r, op)
                    c["state"], c["kidrow"], c["row0"] = mstate, kidrow, row0
                    c["perp"] = c["pid"] if perp else 0
                    yield c


def update_edges(r, op):
    for same in (True, False):
        for _ in range(6):
            c = base(r, op)
            here = (c["cellx"] + (c["celly"] << 7) + 1) & 0xFFFF
            c["queues"]["CPeep::LastCellVisited"] = [here if same else (here + r.choice([1, 0x8000, 0xFFFF])) & 0xFFFF]
            yield c


def init_edges(r, op):
    for centre in (False, True):
        for fail in (0, 1):
            for general in (0, 1):
                c = base(r, op)
                c["centre"] = centre
                c["queues"]["newnull"] = [fail]
                c["queues"]["CStaff::DoGeneralStaffThing"] = [general]
                yield c


def ctor_edges(r, op):
    for centre in (False, True):
        for fail in (0, 1):
            for name in (MASK, 0, 1, 0x22, 0x23, 0x24, 0x7FFFFFFF, 0x80000000):
                for text in ("", "Mimi", "q" * 40):
                    c = base(r, op)
                    c["centre"], c["name"], c["text"] = centre, name, text
                    c["queues"]["newnull"] = [fail]
                    yield c


def ser_edges(r, op):
    for save in (0, 1):
        for version in (MASK, 0, 1, 2, 0x7FFFFFFF, 0x80000000):
            for base_ans in (0, 1):
                for vans in ([0, 0, 0], [1], [0, 1], [0, 0, 1]):
                    c = base(r, op)
                    c["save"], c["version"] = save, version
                    c["queues"]["CStaff::Serialise"] = [base_ans]
                    c["queues"]["vans"] = vans
                    yield c


def recv_edges(r, op):
    for mtype in (0x1b, 0x1a, 0x1c, 0):
        for same in (True, False):
            c = base(r, op)
            c["mtype"], c["perp"] = mtype, c["pid"]
            c["mid"] = c["pid"] if same else (c["pid"] ^ 1)
            c["queues"]["GetType"] = [mtype]
            yield c


def simple_edges(r, op):
    for _ in range(12):
        yield base(r, op)


def state_edges(r, op):
    for st in STATES:
        c = base(r, op)
        c["state"] = st
        yield c


TESTS = [
    FnTest("CGuard::CGuard 0x100fda68", ppc, host, edge=make_edges("ctor1", 11, ctor_edges), rnd=make_random("ctor1")),
    FnTest("CGuard::CGuard 0x100fdb08", ppc, host, edge=make_edges("ctor2", 12, ctor_edges), rnd=make_random("ctor2")),
    FnTest("CGuard::Init", ppc, host, edge=make_edges("init", 19, init_edges), rnd=make_random("init", choose_tweak)),
    FnTest("CGuard::FinalDestroy", ppc, host, edge=make_edges("final", 17, final_edges), rnd=make_random("final", setstate_tweak)),
    FnTest("CGuard::ReceiveMessage", ppc, host, edge=make_edges("recv", 20, recv_edges), rnd=make_random("recv", recv_tweak)),
    FnTest("CGuard::Serialise", ppc, host, edge=make_edges("ser", 15, ser_edges), rnd=make_random("ser")),
    FnTest("CGuard::ProsecutePerp", ppc, host, edge=make_edges("prosecute", 21, prosecute_edges), rnd=make_random("prosecute", prosecute_tweak)),
    FnTest("CGuard::IsAvailable", ppc, host, edge=make_edges("isavail", 22, state_edges), rnd=make_random("isavail")),
    FnTest("CGuard::IsCarrying", ppc, host, edge=make_edges("iscarry", 23, state_edges), rnd=make_random("iscarry")),
    FnTest("CGuard::UpdateTurn", ppc, host, edge=make_edges("update", 18, update_edges), rnd=make_random("update")),
    FnTest("CGuard::ChooseNextAction", ppc, host, edge=make_edges("choose", 14, choose_edges), rnd=make_random("choose", choose_tweak)),
    FnTest("CGuard::ModelState", ppc, host, edge=make_edges("model", 24, model_edges), rnd=make_random("model", prosecuting_tweak)),
    FnTest("CGuard::SetState", ppc, host, edge=make_edges("setstate", 13, setstate_edges), rnd=make_random("setstate", setstate_tweak)),
    FnTest("CGuard::ProcessIdle", ppc, host, edge=make_edges("idle", 25, idle_edges), rnd=make_random("idle", idle_tweak)),
    FnTest("CGuard::ProcessPatrolling", ppc, host, edge=make_edges("patrol", 26, patrol_edges), rnd=make_random("patrol", choose_tweak)),
    FnTest("CGuard::ProcessProsecuting", ppc, host, edge=make_edges("prosecuting", 27, prosecuting_edges), rnd=make_random("prosecuting", prosecuting_tweak)),
    FnTest("CGuard::ProcessWalkingToEntrance", ppc, host, edge=make_edges("toentrance", 28, entrance_edges), rnd=make_random("toentrance", entrance_tweak)),
    FnTest("CGuard::ProcessReturningToPatrol", ppc, host, edge=make_edges("toreturn", 29, return_edges), rnd=make_random("toreturn")),
]
