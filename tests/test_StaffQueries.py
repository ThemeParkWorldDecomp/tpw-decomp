"""Unit w190, against libtpw_unit_StaffQueries.dylib (tests/unit_StaffQueries.cpp), main app ("SimTheme Park"):
  src/game/people/staff.cpp  CStaff::SackMe 0x10125660, TrainMe 0x10125854, GetEmploymentDuration 0x10125a10, CanBePickedUp 0x10125aec, PickMeUp 0x10125b98,
                         DropMe 0x10125de4, BeingPointedAt 0x10125f30, GetStaffType 0x10126718, GetGeneralStaffState 0x101267cc, GetSkill 0x1012686c,
                         IsStriking 0x10126904, IsAllocatedToRide 0x101269d8, IsWorkingOnAllocatedObject 0x10126aac, IsRestingInStaffRoom 0x10126c1c,
                         TooTiredToWork 0x10126cb4, SetName 0x10126f30, GetName 0x10126f98, GetSalary() 0x10126fc8 and the static GetSalary(type, grade) 0x101270b0

A case is a staff member (0x218 bytes of seeded fill between two 0x40 byte guards; the fields the code reads set from the case, the 0x214 and 0x216 words the
mechanic and the handyman keep included), a world block (GetEmploymentDuration only takes its address), the park analyser's block (the pointer the world answers
is shifted back so that the totals of the histories 3 and 4 lie at its start and 0x250 on), the mouse (absent or a block), its two words, the current mouse mode
(absent or a fake object whose GetMode answers the queue), the balance table (all 504 slots from the case) and a name buffer, built twice by ONE piece of Python
code: in emulator memory at the Mac's addresses (big-endian) and in the host library's memory (little-endian). Every callee outside the 19 functions is a fake
that records the same event on both sides (tests/calllog.py): pointers into the staff member as the label 1, the analyser 7, the bank account 8, the mouse 9, the
mouse mode 10, the staff headquarters 11 and the mechanic headquarters 12; the script as an offset from the staff member. The new objects (mouse, default mode) are
checked through the nothrow operator new (the Mac's sizes), the constructor fake and the label of the global. TbTimeStamp::operator- is a plain subtraction on both
sides; wcsncpy (a glue import on the Mac) is a 16 bit copy in the PowerPC stub.

What is compared: the answer, the event log, the staff member's fields (decoded) and every other byte of the block, the analyser's two words and the rest of its
block, the global mouse and its two words. Not tested: a NULL this, a NULL world. TrainMe with a kind outside 4 to 8 divides by a register the Mac leaves from its
caller (the test sets r26 to 1 before the call, the translation uses 1).
"""
import ctypes as C
import itertools
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_StaffQueries"
MASK = 0xFFFFFFFF

(SACK, TRAIN, DURATION, CANPICK, PICKME, DROPME, POINTED, STAFFTYPE, GENSTATE, SKILL, STRIKING, ALLOCRIDE, WORKING, RESTING, TIRED, SETNAME, GETNAME,
 SALARY, SSALARY) = (0x10125660, 0x10125854, 0x10125A10, 0x10125AEC, 0x10125B98, 0x10125DE4, 0x10125F30, 0x10126718, 0x101267CC, 0x1012686C, 0x10126904,
                     0x101269D8, 0x10126AAC, 0x10126C1C, 0x10126CB4, 0x10126F30, 0x10126F98, 0x10126FC8, 0x101270B0)
ENTRY = dict(sack=SACK, train=TRAIN, duration=DURATION, canpick=CANPICK, pickme=PICKME, dropme=DROPME, pointed=POINTED, stafftype=STAFFTYPE,
             genstate=GENSTATE, skill=SKILL, striking=STRIKING, allocride=ALLOCRIDE, working=WORKING, resting=RESTING, tired=TIRED, setname=SETNAME,
             getname=GETNAME, salary=SALARY, ssalary=SSALARY)
SIZES = {SACK: 464, TRAIN: 404, DURATION: 168, CANPICK: 128, PICKME: 548, DROPME: 252, POINTED: 172, STAFFTYPE: 136, GENSTATE: 108, SKILL: 112, STRIKING: 172,
         ALLOCRIDE: 120, WORKING: 268, RESTING: 56, TIRED: 56, SETNAME: 64, GETNAME: 8, SALARY: 192, SSALARY: 36}
GETWORLD = 0x1014205C
OWN = set(SIZES) | {GETWORLD}

G_WORLD, G_MOUSE, G_W1E0, G_W1E4, G_MODE = 0x1037EC0C, 0x102DC1F4, 0x102DC1E0, 0x102DC1E4, 0x102DC1F0
BALANCE = 0x102B4568
BANK, STAFFHQ, MECHHQ = 0x5000, 0x6000, 0x7000
TOTAL3, TOTAL4 = 0x1F7F0, 0x1FA40
HOST_LAYOUT = [0x20C, 0x2A0, 16, TOTAL3, TOTAL4, 0x214, 0x216, 4, 16]

GUARD = 0x40
OBJ = 0x218
OBJ_BLOCK = GUARD + OBJ + GUARD
WIN = 0x290
WORLD = 0x300
NAMEBUF = 0xB0                                   # 0x58 sixteen bit characters
L_OBJ, L_ANALYSER, L_BANK, L_MOUSE, L_MODE, L_STAFFHQ, L_MECHHQ = 1, 7, 8, 9, 10, 11, 12


def s32(v):
    return T.s32(v)


def put_fmt(order, size):
    return order + {1: "B", 2: "H", 4: "I", 8: "Q"}[size]


# ---- the staff member's fields the code reads or writes (offset, size, key); everything else must keep its fill
CASE_FIELDS = ((0x2, 1, "kind"), (0x20, 4, "spidx"), (0x24, 4, "spset"), (0x19C, 4, "state"), (0x1E4, 4, "grade"), (0x1E8, 1, "pct"), (0x1EC, 8, "hired"),
               (0x1F4, 4, "happy"), (0x1F8, 4, "energy"), (0x1FC, 4, "sturn"), (0x204, 2, "rest"), (0x214, 2, "mechobj"), (0x216, 2, "toilet"))
OBJ_FIELDS = [(off, n) for off, n, k in CASE_FIELDS] + [(0x1A0 + 2 * i, 2) for i in range(0x21)]
WIN_FIELDS = [(0, 4), (0x250, 4)]

# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
R = CL.recorder

FAKES = [
    (0x10115A80, R("CPeep::IsInsidePark", "L", ret="i")),
    (0x101152A4, R("CPeep::SpawnParticleEffect", "Li")),
    (0x101159D0, R("CPeep::Teleport", "Lww")),
    (0x101252A8, R("CStaff::SetState", "Li")),
    (0x1013E99C, R("CWorld::GetStaffHQ", "x", ret=lambda e, L: STAFFHQ)),
    (0x1013EA24, R("CWorld::GetMechanicHQ", "x", ret=lambda e, L: MECHHQ)),
    (0x1013EBC8, R("CWorld::GetParkAnalyser", "x", ret=lambda e, L: L.analyser)),
    (0x1013EDFC, R("CWorld::GetBankAccount", "x", ret=lambda e, L: BANK)),
    (0x100F33B0, R("CBankAccount::MakeWithdrawal", "Lu")),
    (0x1012BDE0, R("CStaffHQ::IsStriking", "Li", ret="i")),
    (0x1012C100, R("CStaffHQ::PickUpStaff", "Lw")),
    (0x10105818, R("CMechanicHQ::ReassignJobsAfterDrop", "LLw")),
    (0x10104A30, R("CMechanic::ChooseNextAction", "L")),
    (0x1007E2E0, R("AssignMouseMode", "LL")),
    (0x1007EA78, R("CMMDefault.ctor", "N")),
    (0x1012E728, R("CThing::KillMe", "L")),
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


def stub_spawn(e):
    L = e.cl
    L.rec("CDelayedScript::SpawnSpriteScript", [s32(e.r(3) - L.base), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))])


def stub_funny(e):
    L = e.cl
    world = e.u32(G_WORLD)
    flags = [int(e.r(i) != 0) for i in range(5, 11)]
    flags += [int(e.u32(e.r(1) + o) != 0) for o in (0x38, 0x3C)]
    L.rec("GetFunnyTime", [s32(e.r(4) - world)] + flags)
    e.write(e.r(3), (L.ret("funny", 0) & 0xFFFFFFFFFFFFFFFF).to_bytes(8, "big"))


def stub_minus(e):
    # TbTimeStamp::operator-: r3 the hidden result, r4 this, r5 the other stamp (two big-endian words)
    this, other = int.from_bytes(e.read(e.r(4), 8), "big"), int.from_bytes(e.read(e.r(5), 8), "big")
    e.write(e.r(3), ((this - other) & 0xFFFFFFFFFFFFFFFF).to_bytes(8, "big"))


def stub_wcsncpy(e):
    dest, src, count = e.r(3), e.r(4), e.r(5)
    done = False
    for i in range(count):
        unit = 0 if done else int.from_bytes(e.read(src + 2 * i, 2), "big")
        if unit == 0:
            done = True
        e.write(dest + 2 * i, unit.to_bytes(2, "big"))
    e.set_r(3, dest)


def count_ints(text):
    return text.count("%d")


def stub_assert(e):
    text = e.cstr(e.r(4))[:159]
    ints = [s32(e.r(3))] + [e.r(5 + i) & MASK for i in range(min(count_ints(text), 2))]
    e.cl.rec("GAME_ASSERTMSG", tuple(ints), (text,))


def stub_ctrace(e):
    text = e.cstr(e.r(4))[:159]
    ints = [e.r(3) & MASK] + [e.r(5 + i) & MASK for i in range(min(count_ints(text), 1))]
    e.cl.rec("CTRACE", tuple(ints), (text,))


STUBS.internal(0x1012E1F8, hidden_thing_id("CThing::GetThingId"))
STUBS.internal(0x100FAB9C, stub_spawn)
STUBS.internal(0x101127D0, stub_funny)
STUBS.internal(0x10239988, stub_minus)
STUBS.internal(0x10238CB0, stub_wcsncpy)
STUBS.internal(0x1000DDE8, stub_assert)
STUBS.internal(0x1006FCBC, stub_ctrace)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)

    def get_mode(emu):
        emu.cl.rec("GetMode", (emu.cl.lab(emu.r(3)),))
        emu.set_r(3, T.u32(emu.cl.ret("GetMode", 0)))

    # The mouse mode's vtable: slot +0x2c is GetMode, called through the glue with a TVector like every virtual.
    tvec = T.fake_function(e, get_mode)
    e.stq_vtable = T.persist_alloc(e, 0x40)
    e.write(e.stq_vtable + 0x2C, struct.pack(">I", tvec))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def put(self, a, size, v):
        self.write(a, struct.pack(put_fmt(self.order, size), v & ((1 << (8 * size)) - 1)))

    def get(self, a, size):
        return struct.unpack(put_fmt(self.order, size), self.read(a, size))[0]

    def put_balance(self, values):
        self.write(self.balance, struct.pack(self.order + "504I", *values))


class Ppc(Side):
    order = ">"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({BANK: L_BANK, STAFFHQ: L_STAFFHQ, MECHHQ: L_MECHHQ}, case["queues"])
        self.saved = STUBS.activate(e)

        def op_new(emu):
            fail = emu.cl.ret("newnull", 0) != 0
            size = emu.r(3)
            emu.cl.rec("new", [size, int(fail)])
            if fail:
                emu.set_r(3, 0)
                return
            block = emu.alloc(max(size, 16))
            if size == 1:
                emu.cl.newlab(block)        # the mouse has no constructor call: it is labelled here (and on the host)
            emu.set_r(3, block)

        STUBS.imp("__nw__FUl", op_new)
        self.saved2 = STUBS.activate(e)
        self.balance = BALANCE + 8

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved2)
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

    def setup(self, eb, world, window, mouse, w1, w2, with_mode):
        self.L.base = eb
        self.L.analyser = (window - TOTAL3) & MASK
        self.put(G_WORLD, 4, world)
        self.put(G_MOUSE, 4, mouse)
        self.put(G_W1E0, 4, w1)
        self.put(G_W1E4, 4, w2)
        mode = 0
        if with_mode:
            mode = self.alloc(16)
            self.write(mode, struct.pack(">I", self.e.stq_vtable))
        self.put(G_MODE, 4, mode)
        self.label(self.L.analyser, L_ANALYSER)
        return mode

    def mouse(self):
        word = self.get(G_MOUSE, 4)
        label = self.L.lab(word)
        return label if label >= 0 else (1 << 33) | word

    def mousewords(self):
        return [s32(self.get(G_W1E0, 4)), s32(self.get(G_W1E4, 4))]

    def call(self, entry, args):
        self.e.set_r(26, 1)
        return T.call_ppc(self.e, entry, [a & MASK for a in args], max_insns=20_000_000)[0] & MASK


class Host(Side):
    order = "<"

    def put_balance(self, values):
        # the host's BalanceValue is a union with a pointer: eight bytes a slot, the integer in the low four
        self.write(self.balance, struct.pack("<504Q", *values))

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        for address, n in ((BANK, L_BANK), (STAFFHQ, L_STAFFHQ), (MECHHQ, L_MECHHQ)):
            self.label(address, n)
        self.balance = lib.tpw_stq_balance()

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_stq_alloc(n)

    def label(self, address, n):
        self.lib.tpw_cl_label(n, address)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setup(self, eb, world, window, mouse, w1, w2, with_mode):
        mode_block = self.alloc(64) if with_mode else 0
        mode = self.lib.tpw_stq_setup(eb, world, window, mouse, s32(w1), s32(w2), int(with_mode), mode_block)
        self.label(self.lib.tpw_stq_analyser(), L_ANALYSER)
        return mode

    def mouse(self):
        return self.lib.tpw_stq_mouse()

    def mousewords(self):
        out = (C.c_int32 * 2)()
        self.lib.tpw_stq_mousewords(out)
        return list(out)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_stq_"
    u64, i32, u32, i64 = C.c_uint64, C.c_int32, C.c_uint32, C.c_int64
    for name, res, args in (
        ("alloc", u64, [u32]), ("balance", u64, []), ("setup", u64, [u64, u64, u64, u64, i32, i32, i32, u64]), ("analyser", u64, []), ("mouse", i64, []),
        ("mousewords", None, [C.POINTER(i32)]),
        ("sack", None, [u64]), ("train", None, [u64, i32]), ("duration", i32, [u64]), ("canpick", i32, [u64]), ("pickme", None, [u64]),
        ("dropme", None, [u64, u32]), ("pointed", None, [u64]), ("stafftype", i32, [u64]), ("genstate", i32, [u64]), ("skill", i32, [u64]),
        ("striking", i32, [u64]), ("allocride", i32, [u64, u32]), ("working", i32, [u64, u32]), ("resting", i32, [u64, u32]), ("tired", i32, [u64]),
        ("setname", None, [u64, u64]), ("getname", i64, [u64]), ("salary", u32, [u64]), ("ssalary", u32, [i32, u32]),
    ):
        fn = getattr(lib, p + name)
        fn.restype, fn.argtypes = res, args
    lib.tpw_stq_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_stq_layout(out)
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
    world = s.alloc(WORLD)
    s.write(world, fill(c["seed"] + 1, WORLD))
    window = s.alloc(WIN)
    s.write(window, fill(c["seed"] + 2, WIN))
    s.put(window, 4, c["mtemp3"])
    s.put(window + 0x250, 4, c["mtemp4"])
    mouse = s.alloc(16) if c["mouse"] else 0
    namebuf = s.alloc(NAMEBUF)
    nr = random.Random(c["seed"] + 3)
    for i in range(NAMEBUF // 2):
        s.put(namebuf + 2 * i, 2, nr.getrandbits(16))
    s.label(eb, L_OBJ)
    if mouse:
        s.label(mouse, L_MOUSE)
    mode = s.setup(eb, world, window, mouse, c["w1"], c["w2"], c["mode"])
    if mode:
        s.label(mode, L_MODE)
    s.world, s.window, s.namebuf = world, window, namebuf
    for off, size, key in CASE_FIELDS:
        s.put(eb + off, size, c[key])
    for i, unit in enumerate(c["name"]):
        s.put(namebuf + 2 * i, 2, unit)
    s.put_balance(c["table"])


def snapshot(s, base, size, fields, skew=GUARD):
    data = bytearray(s.read(base, size))
    values = []
    for off, n in fields:
        i = skew + off
        values.append(s.get(base + i, n))
        data[i:i + n] = bytes(n)
    return values, bytes(data)


def execute(s, c):
    op, eb = c["op"], s.eb
    arg = {"train": c["amount"], "dropme": c["cell"] << 16, "allocride": c["id"] << 16, "working": c["id"] << 16, "resting": c["id"] << 16,
           "setname": s.namebuf}
    ret_signed = op in ("duration", "canpick", "stafftype", "genstate", "skill", "striking", "allocride", "working", "resting", "tired")
    if isinstance(s, Ppc):
        if op == "ssalary":
            return s.call(ENTRY[op], [c["type"], c["agrade"]])
        args = [eb] + ([arg[op]] if op in arg else [])
        r3 = s.call(ENTRY[op], args)
        if op == "getname":
            return s32(r3 - eb)
        if op == "salary":
            return r3
        return s32(r3) if ret_signed else 0
    lib = s.lib
    if op == "ssalary":
        return lib.tpw_stq_ssalary(s32(c["type"]), c["agrade"] & MASK)
    fn = getattr(lib, "tpw_stq_" + op)
    value = arg.get(op)
    if op in ("dropme", "allocride", "working", "resting"):
        value >>= 16
    if op == "train":
        value = s32(value)
    result = fn(eb) if op not in arg else fn(eb, value)
    if op in ("getname", "salary"):
        return result
    return result if ret_signed else 0


def observe(s, c, answer):
    out = {"answer": answer, "log": s.log(), "mouse": s.mouse(), "words": s.mousewords()}
    out["obj"] = snapshot(s, s.eb - GUARD, OBJ_BLOCK, OBJ_FIELDS)
    out["window"] = snapshot(s, s.window, WIN, WIN_FIELDS, 0)
    out["name"] = [s.get(s.namebuf + 2 * i, 2) for i in range(NAMEBUF // 2)]
    out["world"] = s.read(s.world, WORLD)
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

STATES = [0xFFFFFFFF, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xB, 0xC, 0xD, 0xE, 0x12, 0x13, 0x7FFFFFFF, 0x80000000]
KINDS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xFF]
BIT_KINDS = [4, 5, 6, 7, 8]
SMALL_GRADES = [0, 1, 2, 3, 4]
FLOATS = [0x00000000, 0x80000000, 0x3F000000, 0xBF000000, 0x42C80000, 0x437F0000, 0x437FE666, 0x43800000, 0x43960000, 0xBF800000, 0x501502F9, 0x7FC00000,
          0x7F800000, 0xFF800000, 0x40490FDB, 0x3F800000, 0x42FE0000, 0x42FF0000, 0x437FFFFF, 0x4F000000, 0xCF000000, 0x3FFFFFFF]
IDS = [0, 1, 2, 3, 4, 7, 0x2FF, 0xFFFF, 0xFFFE, 0x8000]
STAMPS = [0, 1, 0x100000000, 0x123456789ABCDEF, 0x7FFFFFFF, 0x80000000, 0x7FFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFF00000000, 863999999999, 864000000000,
          864000000001, 1 << 40]
AMOUNTS = [0, 1, 2, 7, 99, 100, 101, 199, 200, 1000, 12345, 0x7FFFFFFF, MASK, 0xFFFFFFFE, 0x80000000, 0x80000001, MASK - 99]
TABLE_VALUES = [0, 1, 2, 3, 7, 10, 25, 50, 99, 100, 101, 1000, MASK, MASK - 1, 0x7FFFFFFF, 0x80000000, 0x80000001, 0x10000, 0x10001]


def table(r, small=False):
    values = [r.getrandbits(32) for _ in range(504)]
    for i in range(0, 504, 1):
        if r.random() < 0.6:
            values[i] = r.choice(TABLE_VALUES)
    if small:
        for i in range(504):
            values[i] = r.choice([1, 2, 3, 5, 7, 10, 50, 100, 1000, MASK, 0, 0x7FFFFFFF])
    return values


def rest_level(r):
    return r.choice([-1, 0, 1, 5, 50, 100, 254, 255, 256, 0x7FFFFFFF, -0x80000000, r.randrange(0, 300)]) & MASK


def base(r, op):
    rest = r.choice([0, 1, 0x2FF, 0xFFFF, r.getrandbits(16), 5])
    length = r.choice([0, 1, 2, 5, 31, 32, 33, 40, 80])
    name = [r.choice([1, 0x41, 0x263A, 0xFFFF, r.getrandbits(16) | 1]) for _ in range(length)] + [0] + [r.getrandbits(16) for _ in range(2)]
    c = {"op": op, "seed": r.getrandbits(32), "kind": r.choice(KINDS + BIT_KINDS * 3), "spidx": r.choice([0, 1, 5, MASK, 0x80000000, r.getrandbits(32)]),
         "spset": r.choice([0, 1, 5, 9, MASK, r.getrandbits(32)]), "state": r.choice(STATES), "grade": r.choice(SMALL_GRADES + [r.randrange(-30, 60)]),
         "pct": r.choice([0, 1, 50, 98, 99, 100, 0xFF, r.getrandbits(8)]), "hired": r.choice(STAMPS), "happy": r.choice(FLOATS), "energy": r.choice(FLOATS),
         "sturn": r.getrandbits(32), "rest": rest, "mechobj": r.choice([rest, rest, r.getrandbits(16), 0]), "toilet": r.choice([rest, rest, r.getrandbits(16), 0]),
         "mtemp3": r.getrandbits(32), "mtemp4": r.getrandbits(32), "mouse": r.random() < 0.5, "w1": r.getrandbits(32), "w2": r.getrandbits(32),
         "mode": r.random() < 0.7, "amount": r.choice(AMOUNTS), "cell": r.choice([0, 1, 2, 0x7F, 0x80, 0x81, 0x4000, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16)]),
         "id": r.choice([rest, rest, 0, r.getrandbits(16), 0xFFFF]), "type": r.randrange(-15, 12), "agrade": r.choice([0, 1, 4, 0xFF, 0x100, 0x101, r.getrandbits(32), r.randrange(0, 80)]),
         "name": name, "table": table(r, r.random() < 0.3)}
    c["table"][183] = rest_level(r)
    c["queues"] = {
        "CPeep::IsInsidePark": [r.choice([0, 1, 1, MASK, 5])], "CStaffHQ::IsStriking": [r.choice([0, 1, 2, MASK])], "CThing::GetThingId": [r.choice(IDS)],
        "GetMode": [r.choice([6, 6, 1, 0, 7, MASK])], "funny": [r.choice(STAMPS)], "newnull": [r.choice([0, 0, 0, 1])] if r.random() < 0.5 else [r.choice([0, 0, 1]) for _ in range(3)],
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


def small_table_tweak(r, c):
    # the training and pay slots small enough to give interesting quotients and the grade inside the table
    c["grade"] = r.choice(SMALL_GRADES + [-1, 5, 6, 30])
    if r.random() < 0.6:
        for i in range(504):
            c["table"][i] = r.choice([1, 2, 3, 5, 7, 10, 25, 50, 100, 1000, 0, MASK, 0x7FFFFFFF, 0x80000000])


def train_tweak(r, c):
    small_table_tweak(r, c)
    if r.random() < 0.8:
        c["kind"] = r.choice(BIT_KINDS)
    c["amount"] = r.choice(AMOUNTS + [r.getrandbits(32), r.randrange(0, 20000)])
    c["pct"] = r.choice([0, 1, 50, 98, 99, 100, 0xFF, r.getrandbits(8)])


def salary_tweak(r, c):
    small_table_tweak(r, c)
    c["agrade"] = r.randrange(0, 70) + r.choice([0, 0, 0x100, 0xFFFFFF00])
    c["type"] = r.randrange(-15, 12)


def pick_tweak(r, c):
    c["state"] = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 0xB, 0xD, 0x12, 0x13, r.getrandbits(32)])
    c["kind"] = r.choice([7, 7, 4, 5, 6, 8, 0])


def drop_tweak(r, c):
    c["kind"] = r.choice([4, 4, 5, 6, 7, 8, 0])


def alloc_tweak(r, c):
    c["kind"] = r.choice([4, 5, 4, 5, 6, 7, 8, 0])
    c["state"] = r.choice([0xB, 0xD, 0xB, 0xD, 0, 3, r.getrandbits(32)])


def no_positive_overflow(bits):
    # the emulator's fctiwz wraps a positive value of 2^31 or more to 0x80000000; the hardware (and the host) saturate to 0x7FFFFFFF
    return 0x437F0000 if (bits >> 31) == 0 and ((bits >> 23) & 0xFF) >= 0x9E and bits <= 0x7F800000 else bits


def tired_tweak(r, c):
    c["energy"] = no_positive_overflow(r.choice(FLOATS + [r.getrandbits(32)]))


FLOAT_TEXT = [0x00000000, 0x3F800000, 0x437F0000, 0x437FFFFF, 0x43800000, 0x43FF0000, 0x4EFFFFFF, 0xCF000000, 0xBF800000, 0xBF000000, 0x42FE0000, 0x42FF0000,
              0x7FC00000, 0xFF800000, 0x3F000000, 0x437F8000, 0x437FE666, 0x80000000]


TESTS = [
    FnTest("CStaff::SackMe", ppc, host, edge=grid(31, "sack", {"f:kind": [4, 5, 6, 7, 8, 0, 9], "f:mouse": [False, True], "f:mode": [False, True],
                                                             "GetMode": [6, 1, 0], "newnull": [[0], [1], [0, 1], [0, 0, 1], [0, 1, 0], [1, 0, 0]],
                                                             "f:mtemp3": [0, 0x7FFFFFFF, MASK]}, small_table_tweak), rnd=make_random("sack", small_table_tweak)),
    FnTest("CStaff::TrainMe", ppc, host, edge=grid(32, "train", {"f:grade": [0, 1, 2, 3, 4, 5, -1], "f:kind": [4, 5, 6, 7, 8],
                                                               "f:amount": [0, 1, 99, 100, 101, 1000, 0x7FFFFFFF, MASK, 0x80000000],
                                                               "f:pct": [0, 50, 99, 100, 255]}, small_table_tweak), rnd=make_random("train", train_tweak)),
    FnTest("CStaff::GetEmploymentDuration", ppc, host, edge=grid(33, "duration", {"f:hired": STAMPS, "funny": STAMPS}), rnd=make_random("duration")),
    FnTest("CStaff::CanBePickedUp", ppc, host, edge=grid(34, "canpick", {"f:state": STATES, "CPeep::IsInsidePark": [0, 1, MASK]}), rnd=make_random("canpick", pick_tweak)),
    FnTest("CStaff::PickMeUp", ppc, host, edge=grid(35, "pickme", {"f:state": [0, 1, 2, 3, 4, 5, 7, 0xD, 0x12, 0x13], "CPeep::IsInsidePark": [0, 1],
                                                                 "f:mouse": [False, True], "f:mode": [False, True], "GetMode": [6, 1],
                                                                 "newnull": [[0], [1], [0, 1], [0, 0, 1], [0, 1, 0], [1, 0, 0]]}), rnd=make_random("pickme", pick_tweak)),
    FnTest("CStaff::DropMe", ppc, host, edge=grid(36, "dropme", {"f:cell": [1, 2, 0x7F, 0x80, 0x81, 0x4000, 0, 0xFFFF], "f:kind": [4, 5, 6, 7, 8, 0]}),
           rnd=make_random("dropme", drop_tweak)),
    FnTest("CStaff::BeingPointedAt", ppc, host, edge=grid(37, "pointed", {"f:state": [0, 1, 2, 3, 4, 5, 7, 0xD, 0x12, 0x13], "f:kind": [7, 4, 5, 6],
                                                                       "CPeep::IsInsidePark": [0, 1]}), rnd=make_random("pointed", pick_tweak)),
    FnTest("CStaff::GetStaffType", ppc, host, edge=grid(38, "stafftype", {"f:kind": KINDS}), rnd=make_random("stafftype")),
    FnTest("CStaff::GetGeneralStaffState", ppc, host, edge=grid(39, "genstate", {"f:state": STATES}), rnd=make_random("genstate")),
    FnTest("CStaff::GetSkill", ppc, host, edge=grid(40, "skill", {"f:grade": [0, 1, 2, 3, 4, 5, -1, -2, 0x7FFFFFFF, -0x80000000, 16777217, -16777217],
                                                                "f:pct": [0, 1, 50, 99, 100, 255]}), rnd=make_random("skill")),
    FnTest("CStaff::IsStriking", ppc, host, edge=grid(41, "striking", {"f:kind": KINDS, "CStaffHQ::IsStriking": [0, 1, 2, MASK]}), rnd=make_random("striking")),
    FnTest("CStaff::IsAllocatedToRide", ppc, host, edge=grid(42, "allocride", {"f:kind": [4, 5, 6, 0], "f:mechobj": [0, 5, 0xFFFF], "f:toilet": [0, 5, 0xFFFF],
                                                                              "f:id": [0, 5, 6, 0xFFFF]}), rnd=make_random("allocride", alloc_tweak)),
    FnTest("CStaff::IsWorkingOnAllocatedObject", ppc, host, edge=grid(43, "working", {"f:kind": [4, 5, 6, 0], "f:state": [0xB, 0xD, 0xC, 0, 3], "f:mechobj": [5, 0],
                                                                                        "f:toilet": [5, 0], "f:id": [0, 5, 6]}), rnd=make_random("working", alloc_tweak)),
    FnTest("CStaff::IsRestingInStaffRoom", ppc, host, edge=grid(44, "resting", {"f:state": [0, 2, 3, 4, 0x80000003], "f:rest": [0, 5, 0xFFFF], "f:id": [0, 5, 0xFFFF]}),
           rnd=make_random("resting")),
    FnTest("CStaff::TooTiredToWork", ppc, host, edge=grid(45, "tired", {"f:energy": FLOAT_TEXT}, lambda r, c: c["table"].__setitem__(183, r.choice([-1, 0, 1, 100, 255, 256, 0x7FFFFFFF, 0, 50]) & MASK)),
           rnd=make_random("tired", tired_tweak)),
    FnTest("CStaff::SetName", ppc, host, edge=grid(46, "setname", {"f:name": [[0, 7], [1, 0, 9, 9], [0x41] * 31 + [0, 5], [0x41] * 32 + [0x42, 0], [0x41] * 33 + [0],
                                                                              [0x41] * 40 + [0], [0x41] * 5 + [0] + [0x43] * 40]}), rnd=make_random("setname")),
    FnTest("CStaff::GetName", ppc, host, edge=grid(47, "getname", {"f:kind": [4, 0]}), rnd=make_random("getname")),
    FnTest("CStaff::GetSalary", ppc, host, edge=grid(48, "salary", {"f:kind": KINDS, "f:grade": [0, 1, 4, -1, 5]}, small_table_tweak), rnd=make_random("salary", salary_tweak)),
    FnTest("CStaff::GetSalary (static)", ppc, host, edge=grid(49, "ssalary", {"f:type": [-3, 0, 1, 2, 3, 4, 5, 6], "f:agrade": [0, 1, 4, 69, 70, 0x100, 0x101, 0xFFFFFF00]},
                                                                   small_table_tweak), rnd=make_random("ssalary", salary_tweak)),
]
