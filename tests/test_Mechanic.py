"""Unit w187, against libtpw_unit_Mechanic.dylib (tests/unit_Mechanic.cpp), main app ("SimTheme Park"):
  src/Mechanic.cpp   CMechanic::CMechanic (cell) 0x1010409c and (cell, grade, flag, name) 0x10104170, Init 0x10104284, FinalDestroy 0x101043b0,
                     ReceiveMessage 0x10104508, Serialise 0x101045e8, SetState 0x1010478c, ModelState 0x10104928, UpdateTurn 0x101049e0,
                     ChooseNextAction 0x10104a30, ProcessPatrolling 0x10104ca4, ProcessWalkingToBreakdown 0x10104d64, ProcessRepairing 0x10104fd4,
                     ProcessIdle 0x10105114, __sinit_mechanic_cpp 0x101051d8

A case is a mechanic (0x218 bytes of seeded fill between two 0x40 byte guards, the fields the code reads set from the case), the world's tail block (the
turn at world + 0x1da70c; the world pointer is shifted back to it), eight rides (0x60 byte blocks with the type at +0x0e, the entry cell at +0x32, a float
at +0x40 and the upgrade level at +0x4c; ids 20 to 27 in gThingArray), a ride record (the slots the upgrade arm of SetState reads, one per level 0 to 255),
the main balance's slots 180 to 289, the sound banks, the constructor's two statics, the message centre (absent, or a block) and a ride-deleted message,
built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) and in the host library's memory (little-endian). Every
callee outside the 15 functions is a fake that records the same event on both sides (tests/calllog.py), pointers into the mechanic as the label 1, the
file 2, the message 3, the string table 4, the existing message centre 6, the mechanic HQ 7, the advisor 8 and the rides 10 to 17, the script as an offset
from the mechanic; the queues answer from the case. The new message centre is checked through the nothrow operator new, the table pair of
__construct_array and the label of the first header. __sinit_mechanic_cpp: the two words are written with 0xDEADBEEF and 0xCAFEF00D on the Mac, the
function runs, and they are compared with the host's constants.

What is compared: the answer, the event log, the mechanic's fields (decoded) and every other byte of the block, the tail block, the ride blocks, the two
statics and the global message centre. Not tested: a NULL ride row where the code would dereference it, ids of 0x2800 and above (outside gThingArray),
a NULL this, a ride float outside the int range (Unicorn's fctiwz wraps where the processor saturates).
"""
import ctypes as C
import itertools
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_Mechanic"
MASK = 0xFFFFFFFF

(CTOR1, CTOR2, INIT, FINAL, RECV, SER, SETSTATE, MODEL, UPDATE, CHOOSE, PATROL, WALK, REPAIR, IDLE, SINIT) = (
    0x1010409C, 0x10104170, 0x10104284, 0x101043B0, 0x10104508, 0x101045E8, 0x1010478C, 0x10104928, 0x101049E0, 0x10104A30, 0x10104CA4, 0x10104D64,
    0x10104FD4, 0x10105114, 0x101051D8)
ENTRY = dict(ctor1=CTOR1, ctor2=CTOR2, init=INIT, final=FINAL, recv=RECV, ser=SER, setstate=SETSTATE, model=MODEL, update=UPDATE, choose=CHOOSE,
             patrol=PATROL, walk=WALK, repair=REPAIR, idle=IDLE, sinit=SINIT)
SIZES = {CTOR1: 132, CTOR2: 176, INIT: 260, FINAL: 296, RECV: 168, SER: 372, SETSTATE: 348, MODEL: 140, UPDATE: 36, CHOOSE: 576, PATROL: 140, WALK: 564,
         REPAIR: 268, IDLE: 152, SINIT: 28}
OWN = set(SIZES)

ADDR = dict(banks=0x102F7C94, balance_values=0x102B4568 + 8, guard=0x1034C2C0, sprite=0x1034C2C4, sinit_low=0x1034C2CC, sinit_high=0x1034C2C8)
G_WORLD, G_TEXT, G_ARRAY, G_CENTRE = 0x1037EC0C, 0x1037FF18, 0x1034CBFC, 0x1037F6C4
WIDE_CTOR_TV, WIDE_DTOR_TV, CENTRE_SIZE = 0x102657E8, 0x10267288, 0x1D0
HOST_LAYOUT = [0x218, 16, 8, 0x1DA70C, 48, CENTRE_SIZE]   # the host's BalanceValue is 8 bytes (the integer first), its table entry 48

GUARD = 0x40
OBJ = 0x218
OBJ_BLOCK = GUARD + OBJ + GUARD
TAIL = 0x40
TAIL0 = 0x1DA70C
RIDE = 0x60
OIDS = [20, 21, 22, 23, 24, 25, 26, 27]
GHOST = 99                                     # a thing id with no row
SLOT0, SLOT_N = 180, 110
NLEVEL = 256
INFO_MAC = 0x1FC + 0x40 * NLEVEL + 0x40        # the Mac ride record: the slot of level k is at 0x1fc + 0x40 * k
L_OBJ, L_FILE, L_MSG, L_TABLE, L_CENTRE, L_HQ, L_ADVISOR, L_RIDE0 = 1, 2, 3, 4, 6, 7, 8, 10
HQ_ADDR, ADVISOR_ADDR = 0x4000, 0x5000
INFO_HOST_SIZE = 0x20000


def s32(v):
    return T.s32(v)


def put_fmt(order, size):
    return order + {1: "B", 2: "H", 4: "I"}[size]


# ---- the mechanic's fields the code reads or writes (offset, size); everything else must keep its fill
CASE_FIELDS = ((0x20, 4, "sprite"), (0x24, 4, "set"), (0x19C, 4, "state"), (0x1E4, 4, "grade"), (0x1FC, 4, "sturn"), (0x20C, 2, "next"),
               (0x210, 4, "dur"), (0x214, 2, "objrep"))
OBJ_FIELDS = [(off, n) for off, n, k in CASE_FIELDS] + [(0x1A0 + 2 * i, 2) for i in range(0x20)]
RIDE_FIELDS = [(0xE, 2), (0x32, 2), (0x40, 4), (0x4C, 1)]

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
    (0x1010C980, R("CObject::IsFlaggedForMaintenance", "L", ret="i")),
    (0x1010C878, R("CObject::IsBrokenDown", "L", ret="i")),
    (0x1010C9C0, R("CObject::RequiresUpgrade", "L", ret="i")),
    (0x1010CA08, R("CObject::StartFixing", "L")),
    (0x1010CA6C, R("CObject::IsPlayingFixAnim", "L", ret="i")),
    (0x1010B5BC, R("CObject::RepairFully", "Lw")),
    (0x1013F144, R("CWorld::Register", "xL")),
    (0x1013F1E8, R("CWorld::Unregister", "xL")),
    (0x1013B1E4, R("CWorld::GetRand", "x", ret="i")),
    (0x1013EA24, R("CWorld::GetMechanicHQ", "x", ret=lambda e, L: HQ_ADDR)),
    (0x1013EAB0, R("CWorld::GetAdvisor", "x", ret=lambda e, L: ADVISOR_ADDR)),
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


def stub_new_job(e):
    """CMechanicHQ::GetNewJob: the hidden pointer r3, the HQ r4, the mechanic's id in the HIGH half of r5."""
    L = e.cl
    L.rec("CMechanicHQ::GetNewJob", (L.lab(e.r(4)), (e.r(5) >> 16) & 0xFFFF))
    e.write(e.r(3), struct.pack(">H", L.ret("CMechanicHQ::GetNewJob", 0) & 0xFFFF))


def stub_db_info(e):
    L = e.cl
    L.rec("CObject::GetDBInfo", (L.lab(e.r(3)),))
    e.set_r(3, L.info)


def stub_req_ctor(e):
    e.cl.last_request = e.r(3)
    e.cl.rec("CAdvisorRequest", (s32(e.r(4)), (e.r(5) >> 16) & 0xFFFF, e.r(6) & MASK, s32(e.r(7)), e.r(8) & 0xFF, s32(e.r(9))))


def stub_queue(e):
    e.cl.rec("QueueRequest", (e.cl.lab(e.r(3)), 1 if e.r(4) == e.cl.last_request else 0))


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
    e.cl.rec("CTRACE", tuple(ints), (text,))


def stub_subscribe(e):
    L = e.cl
    L.rec("Subscribe", (L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5))))
    e.set_r(3, 1)


STUBS.internal(0x1012E1F8, hidden_thing_id("CThing::GetThingId"))
STUBS.internal(0x1010C5A0, hidden_thing_id("CObject::GetAssignedStaffMember"))
STUBS.internal(0x10105450, stub_new_job)
STUBS.internal(0x10107CD4, stub_db_info)
STUBS.internal(0x1000DEB4, stub_req_ctor)
STUBS.internal(0x1000AE48, stub_queue)
STUBS.internal(0x100FAB9C, stub_spawn)
STUBS.internal(0x100FAA74, stub_set_script)
STUBS.internal(0x10152388, stub_subscribe)
STUBS.internal(0x1000F02C, stub_var("LOADVAR<unsigned long>"))
STUBS.internal(0x1000F5DC, stub_var("SAVEVAR<unsigned long>"))
STUBS.internal(0x1000EB4C, stub_var("LOADVARSTRUCT<ThingId>"))
STUBS.internal(0x1000E9D4, stub_var("SAVEVARSTRUCT<ThingId>"))
STUBS.internal(0x1000DDE8, stub_assert)
STUBS.internal(0x1006FCBC, stub_ctrace)
STUBS.imp("FormatString__8TbITableCFPwUiUle", stub_format)


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
        self.L.last_request = None
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

    def info_slot(self, info, level):
        return info + 0x1FC + 0x40 * level

    def new_info(self):
        info = self.alloc(INFO_MAC)
        self.L.info = info
        return info

    def setup(self, eb, tail, table, centre, info):
        self.L.base = eb
        self.world = (tail - TAIL0) & MASK
        self.put(G_WORLD, 4, self.world)
        self.put(G_TEXT + 0x57C, 4, table)
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
        return self.lib.tpw_mech_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_mech_alloc(n)

    def label(self, address, n):
        self.lib.tpw_cl_label(n, address)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def row(self, row, ptr):
        self.lib.tpw_mech_setrow(row, ptr)

    def info_slot(self, info, level):
        return info + self.lib.infooff + 8 * (124 + 16 * level)

    def new_info(self):
        return self.alloc(INFO_HOST_SIZE)

    def setup(self, eb, tail, table, centre, info):
        self.lib.tpw_mech_setup(eb, tail, self.text.encode(), centre, info)
        self.world = tail

    def message(self, ident):
        return self.lib.tpw_mech_makemsg(ident)

    def centre(self):
        return self.lib.tpw_mech_centre()

    def globals(self):
        out = (C.c_uint32 * 2)()
        self.lib.tpw_mech_globals(out)
        return list(out)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_mech_"
    u64, i32, u32 = C.c_uint64, C.c_int32, C.c_uint32
    for name, res, args in (
        ("addr", u64, [C.c_char_p]), ("alloc", u64, [u32]), ("makemsg", u64, [u32]),
        ("setup", None, [u64, u64, C.c_char_p, u64, u64]), ("setrow", None, [i32, u64]), ("centre", C.c_int64, []), ("setglobals", None, [u32, u32]),
        ("globals", None, [C.POINTER(u32)]), ("sinit", None, [C.POINTER(u32)]),
        ("ctor1", u64, [u64, u32]), ("ctor2", u64, [u64, u32, i32, u32, i32]), ("init", None, [u64, u32]), ("receive", None, [u64, u64]),
        ("serialise", i32, [u64, u64, i32, i32, u32]), ("final", None, [u64]), ("update", None, [u64]), ("choose", None, [u64]), ("model", None, [u64]),
        ("setstate", None, [u64, i32]), ("idle", None, [u64]), ("patrol", None, [u64]), ("walk", None, [u64]), ("repair", None, [u64]),
    ):
        fn = getattr(lib, p + name)
        fn.restype, fn.argtypes = res, args
    lib.tpw_mech_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_mech_layout(out)
    assert list(out)[:6] == HOST_LAYOUT, list(out)[:n]
    assert n == 8 and out[7] == 8 and 0 < out[6] < 0x1000, list(out)[:n]
    lib.infooff = out[6]
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
    info = s.new_info()
    for k in range(NLEVEL):
        s.put(s.info_slot(info, k), 4, c["upg"][k])
    s.rides = []
    for i, oid in enumerate(OIDS):
        t = s.alloc(RIDE)
        s.write(t, fill(c["seed"] + 10 + i, RIDE))
        o = c["objs"][i]
        s.put(t + 0xE, 2, o["type"])
        s.put(t + 0x32, 2, o["f32"])
        s.put(t + 0x40, 4, safe_f40(o["f40"]))
        s.put(t + 0x4C, 1, o["f4c"])
        s.label(t, L_RIDE0 + i)
        s.row(oid, t)
        s.rides.append(t)
    s.row(GHOST, 0)
    s.label(eb, L_OBJ)
    s.label(file, L_FILE)
    s.label(table, L_TABLE)
    s.label(HQ_ADDR, L_HQ)
    s.label(ADVISOR_ADDR, L_ADVISOR)
    if centre:
        s.label(centre, L_CENTRE)
    s.file = file
    s.text = c["text"]
    s.setup(eb, tail, table, centre, info)
    msg = s.message(c["mid"])
    s.label(msg, L_MSG)
    s.msg = msg
    s.put(tail, 4, c["turn"])
    s.tail = tail
    for off, size, key in CASE_FIELDS:
        s.put(eb + off, size, c[key])
    for k in range(8):
        s.put(s.g("banks") + 4 * k, 4, c["banks"][k])
    for k in range(SLOT_N):
        s.put(s.g("balance_values") + s.bal_stride * (SLOT0 + k), 4, c["bal"][k])
    spare = s.alloc(RIDE)
    s.write(spare, fill(c["seed"] + 30, RIDE))
    sr = random.Random(c["seed"] + 31)
    for off, n in RIDE_FIELDS:
        v = sr.getrandbits(8 * n)
        s.put(spare + off, n, safe_f40(v) if off == 0x40 else v)
    s.row(0, spare if c["row0"] else 0)
    s.spare = spare
    if isinstance(s, Ppc):
        s.put(ADDR["guard"], 1, c["ggrd"])
        s.put(ADDR["sprite"], 4, c["gspr"])
    else:
        s.lib.tpw_mech_setglobals(c["ggrd"], c["gspr"])


def safe_f40(bits):
    """Unicorn's fctiwz gives 0x80000000 where the processor saturates (and for NaN), so floats outside the int range are not tested (doubtful list)."""
    x = struct.unpack("<f", struct.pack("<I", bits & MASK))[0]
    if x != x or x >= 2147483648.0 or x <= -2147483648.0:
        return T.f32_bits(float((bits >> 8) % 100000) - 500.0)
    return bits & MASK


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
        return int(lib.tpw_mech_ctor1(eb, cell) == eb)
    if op == "ctor2":
        return int(lib.tpw_mech_ctor2(eb, cell, s32(c["agrade"]), c["flag"] & 0xFF, s32(c["name"])) == eb)
    if op == "sinit":
        out = (C.c_uint32 * 2)()
        lib.tpw_mech_sinit(out)
        return list(out)
    if op == "init":
        lib.tpw_mech_init(eb, c["isprite"] & 0xFF)
    elif op == "recv":
        lib.tpw_mech_receive(eb, s.msg)
    elif op == "ser":
        return lib.tpw_mech_serialise(eb, s.file, c["save"], s32(c["version"]), c["unused"])
    elif op == "setstate":
        lib.tpw_mech_setstate(eb, s32(c["astate"]))
    else:
        getattr(lib, "tpw_mech_" + op)(eb)
    return 0


def observe(s, c, answer):
    out = {"answer": answer, "log": s.log(), "centre": s.centre(), "globals": s.globals()}
    out["obj"] = snapshot(s, s.eb - GUARD, OBJ_BLOCK, OBJ_FIELDS)
    out["tail"] = snapshot(s, s.tail, TAIL, [(0, 4)], 0)
    out["rides"] = [snapshot(s, t, RIDE, RIDE_FIELDS, 0) for t in s.rides]
    out["spare"] = snapshot(s, s.spare, RIDE, RIDE_FIELDS, 0)
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
STATES = [0xFFFFFFFF, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0x7FFFFFFF, 0x80000000]
GRADES = [0, 1, 2, 3, 4, 5, 6, MASK]
TEXTS = ["", "A", "Zed the Great", "x" * 31, "y" * 40, "Mimi"]
RANDS = [0, 1, 2, 3, 9, 10, 11, 0x0F, 0x10, 0x7F, 0x80, 0x8D, 0xFFFFFFFF, 0x80000000, 0xFFFFFFFE]
FLOATS = [0.0, -0.0, 100.0, 99.6, 50.0, 0.4, 1.0, 255.0, 255.9, 256.0, 300.5, -0.5, -100.0, 1e10, -1e10, float("inf"), float("-inf"), float("nan"),
          2147483647.0, 2147483648.0, -2147483648.0, -2147483649.0, 4294967296.0, 12.5, 65.0]
LEVELS = [0, 1, 2, 3, 4, 0x7F, 0x80, 0xFF]


def u32(r):
    return r.choice(U32 + [r.getrandbits(32), r.getrandbits(16), r.getrandbits(8)])


def qlist(r, values):
    return [r.choice(values) for _ in range(r.randrange(1, 4))]


def fbits(r):
    x = r.choice(FLOATS + [None, None])
    return r.getrandbits(32) if x is None else T.f32_bits(x)


def base(r, op):
    cell = r.choice([0, 1, 2, 0x80, 0x81, 0x4000, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16)])
    meid = r.choice([0, 1, 0x2FF, 0xFFFF, r.getrandbits(16), 5])
    other = r.choice([0, 1, 7, 0x2FE, r.getrandbits(16)])
    objrep = r.choice([0, GHOST, OIDS[r.randrange(8)], OIDS[r.randrange(8)], OIDS[r.randrange(8)]])
    c = {"op": op, "seed": r.getrandbits(32), "cell": cell,
         "sprite": u32(r), "set": u32(r), "state": r.choice(STATES), "grade": r.choice(GRADES), "sturn": u32(r),
         "next": r.getrandbits(16), "dur": r.choice([0, 1, 2, 5, MASK, u32(r)]), "objrep": objrep,
         "row0": r.random() < 0.2, "turn": u32(r),
         "bal": [r.choice([0, 1, 5, 100, 249, 0x7FFFFFFF, MASK, 0x80000000, r.getrandbits(32), r.getrandbits(8), r.getrandbits(14)]) for _ in range(SLOT_N)],
         "banks": [u32(r) for _ in range(8)], "text": r.choice(TEXTS), "agrade": r.choice([0, 1, 2, 3, 4, MASK, 5, u32(r)]),
         "name": r.choice([MASK, 0, 1, 0x22, 0x23, 0x24, 0x7FFFFFFF, 0x80000000, r.randrange(0, 0x23), r.randrange(0, 0x40)]), "flag": r.choice([0, 1, 7, 0xFF, 0x1FF]),
         "isprite": r.choice([0, 1, 5, 0xFF, 0x1FF, MASK, r.getrandbits(8)]), "ggrd": r.choice([0, 1, 0x7F, 0x80, 0xFF]), "gspr": r.choice([0, 1, 5, 0xFF, MASK, u32(r)]),
         "save": r.choice([0, 1, 2, MASK, 0x80000000]), "version": r.choice([MASK, 0, 1, 2, 3, 0x7FFFFFFF, 0x80000000, 5]), "unused": u32(r),
         "astate": r.choice(STATES), "centre": r.random() < 0.5, "mtype": r.choice([10, 10, 9, 11, 0, MASK]), "mid": 0, "meid": meid}
    c["mid"] = r.choice([objrep, objrep, r.getrandbits(16), 0])
    c["upg"] = [r.choice([0, 1, 2, 7, 100, 0x7FFFFFFF, MASK, 0x80000000, r.getrandbits(32), r.getrandbits(10)]) for _ in range(NLEVEL)]
    c["objs"] = [{"type": r.choice([0, 1, 0x7F, 0xFFFF, r.getrandbits(16)]), "f32": r.choice([0, 1, 15, 0xFFFF, r.getrandbits(16)]), "f40": fbits(r),
                  "f4c": r.choice(LEVELS + [r.getrandbits(8)])} for _ in OIDS]
    c["queues"] = {
        "CWorld::GetRand": qlist(r, RANDS),
        "CStaff::DoGeneralStaffThing": qlist(r, [0, 0, 1, MASK]), "CStaff::TooTiredToWork": qlist(r, [0, 0, 1, 5]),
        "CPeep::SetRandomDestination": qlist(r, [0, 1, 7]), "CPeep::PeepWalks": qlist(r, [0, 1, 2, 3, MASK, 4]),
        "CPeep::SetDest": qlist(r, [0, 1, 5]),
        "CObject::GetAssignedStaffMember": qlist(r, [0, meid, meid, other]),
        "CObject::IsFlaggedForMaintenance": qlist(r, [0, 0, 1, 3]), "CObject::IsBrokenDown": qlist(r, [0, 1, 1]),
        "CObject::RequiresUpgrade": qlist(r, [0, 0, 1, 5, MASK]), "CObject::IsPlayingFixAnim": qlist(r, [0, 0, 1, 5]),
        "CMechanicHQ::GetNewJob": qlist(r, [0, OIDS[r.randrange(8)], OIDS[r.randrange(8)]]),
        "CStaff::Serialise": qlist(r, [0, 0, 1, MASK]), "vans": qlist(r, [0, 0, 1, MASK, 7]),
        "GetType": [c["mtype"]], "newnull": [r.choice([0, 0, 1])], "CThing::GetThingId": [meid],
    }
    return c


def valid_ride(r, c):
    """The ride row the code dereferences: one of the eight, or row 0 (non-empty) for the id 0."""
    if c["objrep"] not in OIDS:
        if r.random() < 0.5:
            c["objrep"] = OIDS[r.randrange(8)]
        else:
            c["objrep"], c["row0"] = 0, True


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


def idle_tweak(r, c):
    c["state"] = 0
    idx = bal_index(186, 4, c["grade"])
    if 0 <= idx < SLOT_N:
        c["turn"] = (c["sturn"] + c["bal"][idx] + r.choice([-1, 0, 1, 2, -2, 0x7FFFFFFF])) & MASK


def choose_tweak(r, c):
    c["queues"]["CMechanicHQ::GetNewJob"] = qlist(r, [0, 0, OIDS[r.randrange(8)], OIDS[r.randrange(8)]])


def setstate_tweak(r, c):
    valid_ride(r, c)
    if r.random() < 0.5:
        c["astate"] = r.choice([13, 13, 12, 0, 1, r.choice(STATES)])
    if r.random() < 0.3:
        c["queues"]["CObject::RequiresUpgrade"] = [1]


def walk_tweak(r, c):
    valid_ride(r, c)
    c["queues"]["CPeep::PeepWalks"] = qlist(r, [0, 0, 1, 2, 3])
    if r.random() < 0.6:
        c["queues"]["CObject::GetAssignedStaffMember"] = [c["meid"]]


def repair_tweak(r, c):
    c["dur"] = r.choice([0, 0, 1, MASK, 2])
    c["queues"]["CMechanicHQ::GetNewJob"] = qlist(r, [0, OIDS[r.randrange(8)]])


def final_tweak(r, c):
    if r.random() < 0.6:
        c["queues"]["CObject::GetAssignedStaffMember"] = [c["meid"]]


def recv_tweak(r, c):
    if r.random() < 0.7:
        c["mid"] = c["objrep"]


def model_tweak(r, c):
    valid_ride(r, c)
    c["queues"]["CPeep::PeepWalks"] = qlist(r, [0, 0, 1, 2, 3])
    c["queues"]["CMechanicHQ::GetNewJob"] = qlist(r, [0, OIDS[r.randrange(8)]])
    if r.random() < 0.6:
        c["queues"]["CObject::GetAssignedStaffMember"] = [c["meid"]]
    if r.random() < 0.5:
        idle_tweak(r, c)
        c["state"] = r.choice([0, 1, 12, 13])


def model_edges():
    r = random.Random(24)
    for st in STATES + [0, 0, 1, 1, 2, 3, 12, 13]:
        for delta in (-1, 0, 1):
            c = base(r, "model")
            c["state"] = st
            model_tweak(r, c)
            idx = bal_index(186, 4, c["grade"])
            if 0 <= idx < SLOT_N:
                c["turn"] = (c["sturn"] + c["bal"][idx] + delta) & MASK
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


def choose_edges():
    r = random.Random(14)
    for general in (0, 1):
        for tired in (0, 1):
            for job in (0, 20, 27):
                for dest in (0, 1):
                    for maint in (0, 1):
                        for upgrade in (0, 1):
                            for random_dest in (0, 1):
                                c = base(r, "choose")
                                c["queues"]["CStaff::DoGeneralStaffThing"] = [general]
                                c["queues"]["CStaff::TooTiredToWork"] = [tired]
                                c["queues"]["CMechanicHQ::GetNewJob"] = [job]
                                c["queues"]["CPeep::SetDest"] = [dest]
                                c["queues"]["CObject::IsFlaggedForMaintenance"] = [maint]
                                c["queues"]["CObject::RequiresUpgrade"] = [upgrade]
                                c["queues"]["CPeep::SetRandomDestination"] = [random_dest]
                                yield c


def walk_edges():
    r = random.Random(31)
    for walks in (0, 1, 2, 3, MASK):
        for assigned in ("me", "other", "none"):
            for broken in (0, 1):
                for upgrade in (0, 1):
                    for maint in (0, 1):
                        for job in (0, 20, 27):
                            c = base(r, "walk")
                            c["objrep"] = job if job else r.choice([0, GHOST])
                            if job == 0 and c["objrep"] == 0:
                                c["row0"] = False
                            c["queues"]["CPeep::PeepWalks"] = [walks]
                            c["queues"]["CObject::GetAssignedStaffMember"] = [{"me": c["meid"], "other": c["meid"] ^ 1, "none": 0}[assigned]]
                            c["queues"]["CObject::IsBrokenDown"] = [broken]
                            c["queues"]["CObject::RequiresUpgrade"] = [upgrade]
                            c["queues"]["CObject::IsFlaggedForMaintenance"] = [maint]
                            if walks == 0 and assigned == "me":
                                valid_ride(r, c)
                            yield c


def repair_edges():
    r = random.Random(32)
    for dur in (0, 1, 2, MASK):
        for broken in (0, 1):
            for playing in (0, 1):
                for general in (0, 1):
                    for job in (0, 20, GHOST):
                        c = base(r, "repair")
                        c["dur"], c["objrep"] = dur, job
                        if job == 0:
                            c["row0"] = r.random() < 0.5
                        c["queues"]["CObject::IsBrokenDown"] = [broken]
                        c["queues"]["CObject::IsPlayingFixAnim"] = [playing]
                        c["queues"]["CStaff::DoGeneralStaffThing"] = [general]
                        yield c


def setstate_edges():
    r = random.Random(13)
    for state in STATES:
        for upgrade in (0, 1, 5):
            for grade in (0, 3, 6, MASK):
                c = base(r, "setstate")
                valid_ride(r, c)
                c["astate"], c["grade"] = state, grade
                c["queues"]["CObject::RequiresUpgrade"] = [upgrade]
                yield c


def duration_edges():
    """State 13 with every kind of float and level."""
    r = random.Random(12)
    for f in FLOATS:
        for upgrade in (0,):
            for grade in (0, 4, 6):
                c = base(r, "setstate")
                valid_ride(r, c)
                c["astate"], c["grade"] = 13, grade
                c["queues"]["CObject::RequiresUpgrade"] = [upgrade]
                for o in c["objs"]:
                    o["f40"] = T.f32_bits(f)
                yield c
    for level in LEVELS:
        for grade in (0, 4, 5):
            c = base(r, "setstate")
            valid_ride(r, c)
            c["astate"], c["grade"] = 13, grade
            c["queues"]["CObject::RequiresUpgrade"] = [1]
            for o in c["objs"]:
                o["f4c"] = level
            yield c


def final_edges():
    r = random.Random(17)
    for job in (0, 20, GHOST, 27):
        for row0 in (False, True):
            for assigned in ("me", "other", "none"):
                c = base(r, "final")
                c["objrep"], c["row0"] = job, row0
                c["queues"]["CObject::GetAssignedStaffMember"] = [{"me": c["meid"], "other": c["meid"] ^ 1, "none": 0}[assigned]]
                yield c


def sinit_edges():
    yield base(random.Random(16), "sinit")


def setstate_edge():
    yield from setstate_edges()
    yield from duration_edges()


TESTS = [
    FnTest("CMechanic::CMechanic 0x1010409c", ppc, host, edge=grid(11, "ctor1", {"f:centre": [False, True], "newnull": [0, 1], "f:ggrd": [0, 1, 0x80, 0xFF],
                                                                              "f:gspr": [0, 5, 0x1FF, MASK]}), rnd=make_random("ctor1", choose_tweak)),
    FnTest("CMechanic::CMechanic 0x10104170", ppc, host, edge=grid(12, "ctor2", {"f:centre": [False, True], "newnull": [0, 1],
                                                                              "f:name": [MASK, 0, 1, 0x22, 0x23, 0x24, 0x7FFFFFFF, 0x80000000],
                                                                              "f:text": ["", "Mimi", "q" * 40]}), rnd=make_random("ctor2", choose_tweak)),
    FnTest("CMechanic::Init", ppc, host, edge=grid(19, "init", {"f:centre": [False, True], "newnull": [0, 1], "CStaff::DoGeneralStaffThing": [0, 1],
                                                              "f:isprite": [0, 5, 0xFF, 0x1FF]}), rnd=make_random("init", choose_tweak)),
    FnTest("CMechanic::FinalDestroy", ppc, host, edge=final_edges, rnd=make_random("final", final_tweak)),
    FnTest("CMechanic::ReceiveMessage", ppc, host, edge=grid(20, "recv", {"f:mtype": [10, 9, 11, 0, MASK], "f:objrep": [0, 20, GHOST],
                                                                       "f:state": [12, 13, 0, 3], "f:mid": [0, 20, GHOST, 21]},
                                                          lambda r, c: c["queues"].update({"GetType": [c["mtype"]]})), rnd=make_random("recv", recv_tweak)),
    FnTest("CMechanic::Serialise", ppc, host, edge=ser_edges, rnd=make_random("ser")),
    FnTest("CMechanic::SetState", ppc, host, edge=setstate_edge, rnd=make_random("setstate", setstate_tweak)),
    FnTest("CMechanic::ModelState", ppc, host, edge=model_edges, rnd=make_random("model", model_tweak)),
    FnTest("CMechanic::UpdateTurn", ppc, host, edge=grid(18, "update", {"f:state": [0, 1, 12]}), rnd=make_random("update")),
    FnTest("CMechanic::ChooseNextAction", ppc, host, edge=choose_edges, rnd=make_random("choose", choose_tweak)),
    FnTest("CMechanic::ProcessPatrolling", ppc, host, edge=grid(26, "patrol", {"CPeep::PeepWalks": [0, 1, 2, 3, MASK], "CWorld::GetRand": [[0], [1], [0x10], [0x20, 0]],
                                                                            "CStaff::DoGeneralStaffThing": [0, 1], "CMechanicHQ::GetNewJob": [0, 20]}),
           rnd=make_random("patrol", choose_tweak)),
    FnTest("CMechanic::ProcessWalkingToBreakdown", ppc, host, edge=walk_edges, rnd=make_random("walk", walk_tweak)),
    FnTest("CMechanic::ProcessRepairing", ppc, host, edge=repair_edges, rnd=make_random("repair", repair_tweak)),
    FnTest("CMechanic::ProcessIdle", ppc, host, edge=idle_edges, rnd=make_random("idle", idle_tweak)),
    FnTest("__sinit_mechanic_cpp", ppc, host, edge=sinit_edges, rnd=lambda r: base(r, "sinit")),
]
