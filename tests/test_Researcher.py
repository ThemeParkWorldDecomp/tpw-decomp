"""Unit w188, against libtpw_unit_Researcher.dylib (tests/unit_Researcher.cpp), main app ("SimTheme Park"):
  src/game/people/Researcher.cpp   CRegionEffectCell::operator== 0x10120c7c and Serialise 0x10120cfc, CResearcher::CResearcher (cell) 0x10120e34 and
                       (cell, grade, flag, name) 0x10120eec, Init 0x10121004, Serialise 0x101210e4, FinalDestroy 0x1012123c, ReceiveMessage 0x101212e4,
                       UpdateTurn 0x10121340, ModelState 0x101213fc, ChooseNextAction 0x101214c0, ProcessPatrolling 0x101215d0, ProcessResearching
                       0x10121748, ProcessIdle 0x10121858, DoSomeResearch 0x101219cc, __sinit_Researcher_cpp 0x10121a88

A case is a researcher (0x214 bytes of seeded fill between two 0x40 byte guards, the fields the code reads set from the case; the region effect cell
operations use the first ten bytes as "this" and the ten bytes at +0x20 as the other cell), the world's tail block (the turn at world + 0x1da70c; the
world pointer is shifted back to it), a block standing for the research lab, the main balance's slots 180 to 289, the sound banks and a message block,
built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) and in the host library's memory (little-endian).
Every callee outside the 16 functions is a fake that records the same event on both sides (tests/calllog.py), pointers into the researcher as the label
1, the file 2, the message 3, the string table 4, the lab 7 and the other cell 8, the script as an offset from the researcher; the queues answer from
the case. GetResearchLab answers the lab block (queue nonzero) or NULL. memcmp is a fake on both sides (the host's only while operator== runs).
__sinit_Researcher_cpp: the two words are written with 0xDEADBEEF and 0xCAFEF00D on the Mac, the function runs, and they are compared with the host's
constants.

What is compared: the answer, the event log, the researcher's fields (decoded) and every other byte of the block, the tail block and the two
statics of the sinit. Not tested: a NULL this, balance slots outside 180 to 289 (the grade is one of 0 to 6 or 0xffffffff), a NULL lab where the code
would call a member on it (Init, DoSomeResearch and the turn handler get a lab; FinalDestroy is tested with both).
"""
import ctypes as C
import itertools
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_Researcher"
MASK = 0xFFFFFFFF

(EQ, RSER, CTOR1, CTOR2, INIT, SER, FINAL, RECV, UPDATE, MODEL, CHOOSE, PATROL, RESEARCH, IDLE, DOSOME, SINIT) = (
    0x10120C7C, 0x10120CFC, 0x10120E34, 0x10120EEC, 0x10121004, 0x101210E4, 0x1012123C, 0x101212E4, 0x10121340, 0x101213FC, 0x101214C0, 0x101215D0,
    0x10121748, 0x10121858, 0x101219CC, 0x10121A88)
ENTRY = dict(eq=EQ, rser=RSER, ctor1=CTOR1, ctor2=CTOR2, init=INIT, ser=SER, final=FINAL, recv=RECV, update=UPDATE, model=MODEL, choose=CHOOSE,
             patrol=PATROL, research=RESEARCH, idle=IDLE, dosome=DOSOME, sinit=SINIT)
SIZES = {EQ: 60, RSER: 256, CTOR1: 100, CTOR2: 176, INIT: 180, SER: 292, FINAL: 120, RECV: 36, UPDATE: 140, MODEL: 148, CHOOSE: 220, PATROL: 320,
         RESEARCH: 216, IDLE: 324, DOSOME: 136, SINIT: 28}
OWN = set(SIZES)

ADDR = dict(banks=0x102F7C94, balance_values=0x102B4568 + 8, sinit_low=0x1034C4B4, sinit_high=0x1034C4B0)
G_WORLD, G_TEXT = 0x1037EC0C, 0x1037FF18
HOST_LAYOUT = [0x214, 16, 8, 0x1DA70C, 10]   # the host's BalanceValue is 8 bytes (the integer first)

GUARD = 0x40
OBJ = 0x214
OBJ_BLOCK = GUARD + OBJ + GUARD
TAIL = 0x40
TAIL0 = 0x1DA70C
SLOT0, SLOT_N = 180, 110
L_OBJ, L_FILE, L_MSG, L_TABLE, L_LAB, L_OTHER = 1, 2, 3, 4, 7, 8


def s32(v):
    return T.s32(v)


def put_fmt(order, size):
    return order + {1: "B", 2: "H", 4: "I"}[size]


# ---- the researcher's fields the code reads or writes (offset, size); everything else must keep its fill
CASE_FIELDS = ((0x20, 4, "sprite"), (0x24, 4, "set"), (0x19C, 4, "state"), (0x1E4, 4, "grade"), (0x1FC, 4, "sturn"), (0x20C, 2, "next"), (0x210, 4, "ts"))
OBJ_FIELDS = [(off, n) for off, n, k in CASE_FIELDS] + [(0x1A0 + 2 * i, 2) for i in range(0x20)]

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
    (0x101267CC, R("CStaff::GetGeneralStaffState", "L", ret="i")),
    (0x10126E28, R("CStaff::GetTiredWorking", "L")),
    (0x10126D1C, R("CStaff::GetTiredPatrolling", "L")),
    (0x10126008, R("CStaff::ProcessWalkingToRestArea", "L")),
    (0x101262A8, R("CStaff::ProcessResting", "L")),
    (0x10114FA8, R("CPeep::PeepWalks", "L", ret="i")),
    (0x10115B80, R("CPeep::PlaySound", "Lii")),
    (0x10113870, R("CPeep::SetRandomDestination", "L", ret="i")),
    (0x1013F7D8, R("CWorld::Register", "xL")),
    (0x1013F880, R("CWorld::Unregister", "xL")),
    (0x1013B1E4, R("CWorld::GetRand", "x", ret="i")),
    (0x10123810, R("CResearchLab::UpdateActiveStatus", "L")),
    (0x101222AC, R("CResearchLab::ContributeResearch", "Lu")),
    (0x1007C900, R("ESP_GetRandomSpritesFrom", "i", ret="i")),
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


def stub_lab(e):
    L = e.cl
    L.rec("CWorld::GetResearchLab", ())
    e.set_r(3, L.labaddr if L.ret("CWorld::GetResearchLab", 0) else 0)


def stub_spawn(e):
    L = e.cl
    L.rec("CDelayedScript::SpawnSpriteScript", (s32(e.r(3) - L.base), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))


def stub_set_script(e):
    L = e.cl
    # The turn the researcher stored before the call (the Mac stores it first), as the field's value.
    L.rec("CDelayedScript::SetSpriteScript", (s32(e.r(3) - L.base), s32(e.r(4)), struct.unpack(">I", e.read(L.base + 0x210, 4))[0]))


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


def stub_memcmp(e):
    L = e.cl
    L.rec("memcmp", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK))
    e.set_r(3, L.ret("memcmp", 0) & MASK)


STUBS.internal(0x1012E1F8, hidden_thing_id("CThing::GetThingId"))
STUBS.internal(0x1013ECE8, stub_lab)
STUBS.internal(0x100FAB9C, stub_spawn)
STUBS.internal(0x100FAA74, stub_set_script)
STUBS.internal(0x1000F02C, stub_var("LOADVAR<unsigned long>"))
STUBS.internal(0x1000F5DC, stub_var("SAVEVAR<unsigned long>"))
STUBS.internal(0x1000EB4C, stub_var("LOADVARSTRUCT<ThingId>"))
STUBS.internal(0x1000E9D4, stub_var("SAVEVARSTRUCT<ThingId>"))
STUBS.internal(0x100F518C, stub_var("LOADVAR<short>"))
STUBS.internal(0x100F5658, stub_var("SAVEVAR<short>"))
STUBS.internal(0x1000DDE8, stub_assert)
STUBS.internal(0x1006FCBC, stub_ctrace)
STUBS.imp("FormatString__8TbITableCFPwUiUle", stub_format)
STUBS.imp("memcmp", stub_memcmp)


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
        self.L.text = case["text"]
        self.saved = STUBS.activate(e)

    def done(self):
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

    def setup(self, eb, tail, table, lab):
        self.L.base = eb
        self.L.labaddr = lab
        self.put(G_WORLD, 4, (tail - TAIL0) & MASK)
        self.put(G_TEXT + 0x588, 4, table)

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
        return self.lib.tpw_resr_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_resr_alloc(n)

    def label(self, address, n):
        self.lib.tpw_cl_label(n, address)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setup(self, eb, tail, table, lab):
        self.lib.tpw_resr_setup(eb, tail, self.text.encode(), lab)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_resr_"
    u64, i32, u32 = C.c_uint64, C.c_int32, C.c_uint32
    for name, res, args in (
        ("addr", u64, [C.c_char_p]), ("alloc", u64, [u32]), ("setup", None, [u64, u64, C.c_char_p, u64]), ("sinit", None, [C.POINTER(u32)]),
        ("ctor1", u64, [u64, u32]), ("ctor2", u64, [u64, u32, i32, u32, i32]), ("init", None, [u64, u32]), ("recv", None, [u64, u64]),
        ("ser", i32, [u64, u64, i32, i32, u32]), ("final", None, [u64]), ("update", None, [u64]), ("choose", None, [u64]), ("model", None, [u64]),
        ("idle", None, [u64]), ("patrol", None, [u64]), ("research", None, [u64]), ("dosome", None, [u64]), ("eq", i32, [u64, u64]),
        ("rser", i32, [u64, u64, i32, i32, u32]),
    ):
        fn = getattr(lib, p + name)
        fn.restype, fn.argtypes = res, args
    lib.tpw_resr_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_resr_layout(out)
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
    lab = s.alloc(16)
    s.label(eb, L_OBJ)
    s.label(file, L_FILE)
    s.label(table, L_TABLE)
    s.label(lab, L_LAB)
    s.label(eb + 0x20, L_OTHER)
    s.file = file
    s.text = c["text"]
    s.setup(eb, tail, table, lab)
    msg = s.alloc(16)
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
        if op == "eq":
            return s.call(entry, [eb, eb + 0x20]) & 0xFF
        if op == "init":
            s.call(entry, [eb, c["isprite"]])
        elif op == "recv":
            s.call(entry, [eb, s.msg])
        elif op in ("ser", "rser"):
            return s32(s.call(entry, [eb, s.file, c["save"], c["version"], c["unused"]]))
        else:
            s.call(entry, [eb])
        return 0
    lib = s.lib
    cell = c["cell"]
    if op == "ctor1":
        return int(lib.tpw_resr_ctor1(eb, cell) == eb)
    if op == "ctor2":
        return int(lib.tpw_resr_ctor2(eb, cell, s32(c["agrade"]), c["flag"] & 0xFF, s32(c["name"])) == eb)
    if op == "sinit":
        out = (C.c_uint32 * 2)()
        lib.tpw_resr_sinit(out)
        return list(out)
    if op == "eq":
        return lib.tpw_resr_eq(eb, eb + 0x20) & 0xFF
    if op == "init":
        lib.tpw_resr_init(eb, c["isprite"] & 0xFF)
    elif op == "recv":
        lib.tpw_resr_recv(eb, s.msg)
    elif op in ("ser", "rser"):
        return getattr(lib, "tpw_resr_" + op)(eb, s.file, c["save"], s32(c["version"]), c["unused"])
    else:
        getattr(lib, "tpw_resr_" + op)(eb)
    return 0


def observe(s, c, answer):
    out = {"answer": answer, "log": s.log()}
    out["obj"] = snapshot(s, s.eb - GUARD, OBJ_BLOCK, OBJ_FIELDS)
    out["tail"] = snapshot(s, s.tail, TAIL, [(0, 4)], 0)
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

U32 = [0, 1, 2, 3, 0xF, 0x10, 0x14, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, MASK, 15000]
STATES = [MASK, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 0x7FFFFFFF, 0x80000000]
GRADES = [0, 1, 2, 3, 4, 5, 6, MASK]
TEXTS = ["", "A", "Zed the Great", "x" * 31, "y" * 40, "Mimi"]
RANDS = [0, 1, 2, 3, 4, 5, 0x0F, 0x10, 0x11, 0x7F, 0x80, 0xFFFFFFF0, 0xFFFFFFFF, 0x80000000, 0xFFFFFFFE]
GENERAL = [MASK, 0, 1, 2, 3, 4, 5, 6, 0x7FFFFFFF, 0x80000000]


def u32(r):
    return r.choice(U32 + [r.getrandbits(32), r.getrandbits(16), r.getrandbits(8)])


def qlist(r, values):
    return [r.choice(values) for _ in range(r.randrange(1, 4))]


def base(r, op):
    c = {"op": op, "seed": r.getrandbits(32), "cell": r.choice([0, 1, 2, 0x80, 0x4000, 0x8000, 0xFFFF, r.getrandbits(16)]),
         "sprite": u32(r), "set": u32(r), "state": r.choice(STATES), "grade": r.choice(GRADES), "sturn": u32(r),
         "next": r.getrandbits(16), "ts": r.choice([0, 1, 2, 5, MASK, u32(r)]), "turn": r.choice([0, 19, 20, 40, MASK, 0xFFFFFFEC, u32(r)]),
         "bal": [r.choice([0, 1, 5, 20, 100, 0x7FFFFFFF, MASK, 0x80000000, r.getrandbits(32), r.getrandbits(8)]) for _ in range(SLOT_N)],
         "banks": [u32(r) for _ in range(8)], "text": r.choice(TEXTS), "agrade": r.choice([0, 1, 2, 3, 4, MASK, 5, u32(r)]),
         "name": r.choice([MASK, 0, 1, 0x22, 0x23, 0x24, 0x7FFFFFFF, 0x80000000, r.randrange(0, 0x23), r.randrange(0, 0x40)]),
         "flag": r.choice([0, 1, 7, 0xFF, 0x1FF]), "isprite": r.choice([0, 1, 5, 0xFF, 0x1FF, MASK, r.getrandbits(8)]),
         "save": r.choice([0, 1, 2, MASK, 0x80000000]), "version": r.choice([MASK, 0, 1, 2, 3, 0x7FFFFFFF, 0x80000000, 5]), "unused": u32(r)}
    c["queues"] = {
        "CWorld::GetRand": qlist(r, RANDS), "CStaff::DoGeneralStaffThing": qlist(r, [0, 0, 1, MASK]), "CStaff::TooTiredToWork": qlist(r, [0, 0, 1, 5]),
        "CPeep::SetRandomDestination": qlist(r, [0, 1, 7]), "CPeep::PeepWalks": qlist(r, [0, 1, 2, 3, MASK, 4]),
        "CStaff::Serialise": qlist(r, [0, 0, 1, MASK]), "vans": qlist(r, [0, 0, 1, MASK, 7]), "CStaff::GetGeneralStaffState": qlist(r, GENERAL),
        "ESP_GetRandomSpritesFrom": qlist(r, [0, 1, 5, 0xFF, 0x1FF, MASK, r.getrandbits(32)]), "CWorld::GetResearchLab": [1],
        "CThing::GetThingId": [r.choice([0, 1, 0x2FF, 0x8000, 0xFFFF, r.getrandbits(16)])], "memcmp": qlist(r, [0, 1, MASK, 0x100, 0x80000000, 5])}
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


def idle_tweak(r, c):
    c["state"] = 0
    limit_turn(186, 4, "sturn")(r, c)


def research_tweak(r, c):
    c["state"] = 15
    limit_turn(260, 3, "ts")(r, c)


def update_tweak(r, c):
    c["turn"] = r.choice([0, 19, 20, 40, 100, 21, MASK, 0xFFFFFFEC, r.randrange(0, 1000) * 20])


def model_tweak(r, c):
    if r.random() < 0.5:
        limit_turn(186, 4, "sturn")(r, c)
    else:
        limit_turn(260, 3, "ts")(r, c)


def dosome_tweak(r, c):
    idx = bal_index(261, 3, c["grade"])
    if 0 <= idx < SLOT_N:
        c["bal"][idx] = r.choice([0, 1, 5, 0x7FFFFFFF, MASK, 0x80000000])


def final_tweak(r, c):
    c["queues"]["CWorld::GetResearchLab"] = qlist(r, [0, 1, 1])


def model_edges():
    r = random.Random(24)
    for st in STATES:
        for grade in (0, 4):
            for delta in (-1, 0, 1):
                c = base(r, "model")
                c["state"], c["grade"] = st, grade
                slot, stride, field = (186, 4, "sturn") if st == 0 else (260, 3, "ts")
                c["turn"] = (c[field] + c["bal"][bal_index(slot, stride, grade)] + delta) & MASK
                yield c


def idle_edges():
    r = random.Random(25)
    for grade in GRADES:
        for delta in (-1, 0, 1):
            for rand in ([0], [0x10], [1]):
                for sturn in (0, 7, MASK, 0x80000000):
                    c = base(r, "idle")
                    c["grade"], c["sturn"] = grade, sturn
                    c["bal"][bal_index(186, 4, grade)] = r.choice([0, 1, 5, 100, MASK, 0x7FFFFFFF])
                    c["turn"] = (sturn + c["bal"][bal_index(186, 4, grade)] + delta) & MASK
                    c["queues"]["CWorld::GetRand"] = rand
                    yield c


def research_edges():
    r = random.Random(27)
    for grade in GRADES:
        for delta in (-1, 0, 1, 2):
            for ts in (0, 5, MASK - 2, 0xFFFFFFF0, 0x80000000):
                for dest, rand in itertools.product(([0], [1]), ([0], [1], [0x10], [0xF])):
                    c = base(r, "research")
                    c["grade"], c["ts"], c["state"] = grade, ts, 15
                    c["bal"][bal_index(260, 3, grade)] = r.choice([0, 1, 5, 20, 0x7FFFFFFF, MASK, 0x80000000])
                    c["turn"] = (ts + c["bal"][bal_index(260, 3, grade)] + delta) & MASK
                    c["queues"]["CPeep::SetRandomDestination"] = dest
                    c["queues"]["CWorld::GetRand"] = rand
                    yield c


def ser_edges():
    r = random.Random(28)
    for save, version, staff, vans in itertools.product([0, 1, 2, MASK, 0x80000000], [MASK, 0, 1, 2, 3, 0x7FFFFFFF, 0x80000000], [0, 1, MASK],
                                                        [[0], [1], [0, 1], [0, 0, 1], [MASK]]):
        c = base(r, "ser")
        c["save"], c["version"] = save, version
        c["queues"]["CStaff::Serialise"] = [staff]
        c["queues"]["vans"] = vans
        yield c


def rser_edges():
    r = random.Random(29)
    for save, version, vans in itertools.product([0, 1, 2, MASK, 0x80000000], [MASK, 0, 1, 2, 3, 0x7FFFFFFF, 0x80000000],
                                                  [[0], [1], [0, 1], [0, 0, 1], [0, 0, 0, 1], [0, 0, 0, 0, 1], [MASK]]):
        c = base(r, "rser")
        c["save"], c["version"] = save, version
        c["queues"]["vans"] = vans
        yield c


def sinit_edges():
    yield base(random.Random(16), "sinit")


TESTS = [
    FnTest("CRegionEffectCell::operator== 0x10120c7c", ppc, host, edge=grid(31, "eq", {"memcmp": [0, 1, MASK, 0x100, 0x80000000, 5, 0x7FFFFFFF]}),
           rnd=make_random("eq")),
    FnTest("CRegionEffectCell::Serialise 0x10120cfc", ppc, host, edge=rser_edges, rnd=make_random("rser")),
    FnTest("CResearcher::CResearcher 0x10120e34", ppc, host,
           edge=grid(11, "ctor1", {"ESP_GetRandomSpritesFrom": [0, 5, 0x1FF, MASK, 0x7FFFFFFF], "CStaff::DoGeneralStaffThing": [0, 1],
                                   "CStaff::TooTiredToWork": [0, 1], "CPeep::SetRandomDestination": [0, 1],
                                   "CWorld::GetRand": [[0], [1], [2], [3]]}), rnd=make_random("ctor1")),
    FnTest("CResearcher::CResearcher 0x10120eec", ppc, host,
           edge=grid(12, "ctor2", {"f:name": [MASK, 0, 1, 0x22, 0x23, 0x24, 0x7FFFFFFF, 0x80000000], "f:agrade": [0, 4, MASK], "f:flag": [0, 7, 0x1FF],
                                   "f:text": ["", "Mimi", "q" * 40]}), rnd=make_random("ctor2")),
    FnTest("CResearcher::Init", ppc, host,
           edge=grid(19, "init", {"f:isprite": [0, 5, 0xFF, 0x1FF], "CStaff::DoGeneralStaffThing": [0, 1], "CStaff::TooTiredToWork": [0, 1],
                                  "CPeep::SetRandomDestination": [0, 1], "CWorld::GetRand": [[0], [1], [2], [3]]}), rnd=make_random("init")),
    FnTest("CResearcher::Serialise", ppc, host, edge=ser_edges, rnd=make_random("ser")),
    FnTest("CResearcher::FinalDestroy", ppc, host, edge=grid(17, "final", {"CWorld::GetResearchLab": [[0], [1], [0, 1], [1, 0], [1, 1]], "f:state": [0, 15]}),
           rnd=make_random("final", final_tweak)),
    FnTest("CResearcher::ReceiveMessage", ppc, host, edge=grid(20, "recv", {"f:state": [0, 15, MASK]}), rnd=make_random("recv")),
    FnTest("CResearcher::UpdateTurn", ppc, host,
           edge=grid(21, "update", {"CStaff::GetGeneralStaffState": GENERAL, "f:turn": [0, 19, 20, 21, 40, 100, 0xFFFFFFEC, MASK, 0x80000000, 0x7FFFFFFF],
                                    "f:grade": [0, 4]}), rnd=make_random("update", update_tweak)),
    FnTest("CResearcher::ModelState", ppc, host, edge=model_edges, rnd=make_random("model", model_tweak)),
    FnTest("CResearcher::ChooseNextAction", ppc, host,
           edge=grid(14, "choose", {"CStaff::DoGeneralStaffThing": [0, 1], "CStaff::TooTiredToWork": [0, 1, 5], "CPeep::SetRandomDestination": [0, 1, 7],
                                    "CWorld::GetRand": [[0], [1], [2], [3], [4], [5], [0xFFFFFFFC], [0x103]], "f:turn": [0, MASK]}), rnd=make_random("choose")),
    FnTest("CResearcher::ProcessPatrolling", ppc, host,
           edge=grid(26, "patrol", {"CPeep::PeepWalks": [0, 1, 2, 3, MASK], "CWorld::GetRand": [[0], [1], [0x10], [0x20, 0], [0xF], [0xFFFFFFF0], [0x10, 1]],
                                    "CStaff::DoGeneralStaffThing": [0, 1], "CStaff::TooTiredToWork": [0, 1]}), rnd=make_random("patrol")),
    FnTest("CResearcher::ProcessResearching", ppc, host, edge=research_edges, rnd=make_random("research", research_tweak)),
    FnTest("CResearcher::ProcessIdle", ppc, host, edge=idle_edges, rnd=make_random("idle", idle_tweak)),
    FnTest("CResearcher::DoSomeResearch", ppc, host,
           edge=grid(32, "dosome", {"f:grade": GRADES, "CThing::GetThingId": [0, 1, 0xFFFF, 0x8000, 0x1234]}, dosome_tweak), rnd=make_random("dosome", dosome_tweak)),
    FnTest("__sinit_Researcher_cpp", ppc, host, edge=sinit_edges, rnd=lambda r: base(r, "sinit")),
]
