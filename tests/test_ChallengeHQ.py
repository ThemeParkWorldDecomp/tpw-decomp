"""Unit w175, the challenge hall's progress, against libtpw_unit_ChallengeHQ.dylib (tests/unit_ChallengeHQ.cpp), main app ("SimTheme Park"),
src/game/park/ChallengeHQ.cpp:
  CChallengeHQ::FinalDestroy 0x100f6b60, Serialise 0x100f6fe0, AddToCounter 0x100f7324, GetCurrentChallenge 0x100f757c, ReceiveMessage 0x100f75e4,
  OfferFollowup 0x100f7fac, TellPlayerChallengeOffered 0x100f8194, ChallengeAccepted 0x100f83cc, ChallengeDeclined 0x100f853c,
  EndCurrentChallenge 0x100f86d8, WonCurrentChallenge 0x100f87d4, LostCurrentChallenge 0x100f8b20 (and the CMsgChallenge constructor 0x10152008, which
  the hall builds for real on both sides)

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for the
original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside the unit is a fake
on both sides that records its arguments and answers from the case: CThing::Serialise, CChallenge::Serialise, DidThisWinIt, GetOfferId, GetWinId,
GetLoseId, LOADVAR / SAVEVAR of int and bool, the VARSTRUCT of the time stamp and the thing id, GetFunnyTime, the time stamp's operator+, GetParkAnalyser,
GetBankAccount, the analyser's three queries, MakeDeposit, PostMessage with the message centre's new and construction, the trace and the assert.

The world of a case: the hall (0x3e4 bytes between two random guards of 0x40 bytes, every byte of it a typed field of the case: the twenty challenge
rows, the current index, the next event time, the three flags, the value and the thing id), the file, gWorld and gGameWorld (addresses only, with the
park time at +0x2a0), the analyser and the bank blocks, the message centre pointer (NULL: made on the first post), the message of ReceiveMessage (its
vtable makes GetType a fake) and the three balance slots 478, 479 and 481. What is compared: the answer (GetCurrentChallenge as the offset from the hall),
every field of the hall, both guards, the message centre pointer's label, the number of allocations and the event log.

No fused operations. Not tested: ChallengeDeclined with a current index outside 0 .. 19 while a reply is awaited (the original reads the byte at address
0x1d, the translation reads through NULL), and a force flag other than 0 and 1.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ChallengeHQ"
MASK = 0xFFFFFFFF
M64 = (1 << 64) - 1
DAY = 864000000000

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
FINAL, SERIALISE, ADD, CURRENT, RECEIVE, OFFER, TELL = 0x100F6B60, 0x100F6FE0, 0x100F7324, 0x100F757C, 0x100F75E4, 0x100F7FAC, 0x100F8194
ACCEPT, DECLINE, END, WON, LOST, MSGCTOR = 0x100F83CC, 0x100F853C, 0x100F86D8, 0x100F87D4, 0x100F8B20, 0x10152008
SIZES = {FINAL: 4, SERIALISE: 784, ADD: 216, CURRENT: 48, RECEIVE: 1656, OFFER: 436, TELL: 492, ACCEPT: 312, DECLINE: 356, END: 196, WON: 788,
         LOST: 544, MSGCTOR: 88}
OWN = set(SIZES)

# ---- the callees faked on both sides
THING_SERIALISE, CHAL_SERIALISE, DID_WIN, OFFER_ID, WIN_ID, LOSE_ID = 0x1012E010, 0x100F5B80, 0x100F60D8, 0x100F8D7C, 0x100F904C, 0x100F91B0
LOADVAR_I, SAVEVAR_I, LOADVAR_B, SAVEVAR_B = 0x1000EF18, 0x1000F4B0, 0x1000F144, 0x1000F70C
LOADSTRUCT_TS, SAVESTRUCT_TS, LOADSTRUCT_ID, SAVESTRUCT_ID = 0x100F1468, 0x100F1B1C, 0x1000EB4C, 0x1000E9D4
FUNNY, POST, TRACE, ASSERT = 0x101127D0, 0x1015256C, 0x1006FCBC, 0x1000DDE8
GET_ANALYSER, GET_BANK, FILTH, HAPPY, HAPPY_TYPE, DEPOSIT = 0x1013EBC8, 0x1013EDFC, 0x100E9420, 0x100E8B64, 0x100E89FC, 0x100F32EC
CENTRE_CTOR, CENTRE_DTOR, CHALLENGE_VTABLE = 0x102657E8, 0x10267288, 0x102A08D4

# ---- the Mac's addresses of the globals
G_WORLD, G_GAME, G_CENTRE, G_BALANCE = 0x1037EC0C, 0x1037EBFC, 0x1037F6C4, 0x102B4568
SLOT_DONE, SLOT_DECLINED, SLOT_FORFEIT = 478, 479, 481
HALL_SIZE, GUARD = 0x3E4, 0x40
POISON = b"\xa5"

# ---- the hall's typed fields (offset, size): every byte of the 0x3e4
ROW_FIELDS = [(0, 4), (4, 4), (8, 4), (0xC, 2), (0xE, 2), (0x10, 4), (0x14, 4), (0x18, 4), (0x1C, 1), (0x1D, 1), (0x1E, 1), (0x1F, 1), (0x20, 4), (0x24, 4),
              (0x28, 4), (0x2C, 1), (0x2D, 1), (0x2E, 2)]
HEAD_FIELDS = [(0, 4), (4, 4), (8, 4)]
TAIL_FIELDS = [(0x3CC, 4), (0x3D0, 8), (0x3D8, 1), (0x3D9, 1), (0x3DA, 1), (0x3DB, 1), (0x3DC, 4), (0x3E0, 2), (0x3E2, 1), (0x3E3, 1)]
FIELDS = HEAD_FIELDS + [(0xC + 0x30 * i + o, n) for i in range(20) for o, n in ROW_FIELDS] + TAIL_FIELDS
assert sum(n for _, n in FIELDS) == HALL_SIZE

HOST_LAYOUT = [HALL_SIZE, 0xC, 0x30, 0x3CC, 0x3D0, 0x3D8, 0x3D9, 0x3DA, 0x3DC, 0x3E0, 8, 2]

# the hall's offsets by name
CUR, NEXT, SYS, ON, WAIT, VALUE, THING = 0x3CC, 0x3D0, 0x3D8, 0x3D9, 0x3DA, 0x3DC, 0x3E0


def row(i, off):
    return 0xC + 0x30 * i + off


def s32(v):
    return T.s32(v)


def s64(v):
    v &= M64
    return v - (1 << 64) if v >> 63 else v


def be64(e, a):
    return int.from_bytes(e.read(a, 8), "big")


def be(e, a, v, n):
    e.write(a, (v & ((1 << (8 * n)) - 1)).to_bytes(n, "big"))


# ---------------------------------------------------------------------------------------------------------------- labels
L_HALL, L_FILE, L_WORLD, L_PT, L_GAME, L_ANALYSER, L_BANK, L_CENTRE, L_MSG = 1, 2, 3, 4, 5, 6, 7, 8, 9


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def chal_row(L, p):
    return (p - L.hall - 0xC) // 0x30


def stub_thing_serialise(e):
    L = e.cl
    L.rec("ThingSerialise", [L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5)), s32(e.r(6)), e.r(7) & MASK])
    e.set_r(3, L.ret("thing", 0) & MASK)


def stub_chal_serialise(e):
    L = e.cl
    L.rec("ChalSerialise", [chal_row(L, e.r(3)), L.lab(e.r(4)), s32(e.r(5)), s32(e.r(6)), e.r(7) & MASK])
    e.set_r(3, L.ret("chal", 0) & MASK)


def stub_did_win(e):
    L = e.cl
    value = e.r(4)
    L.rec("DidWin", [chal_row(L, e.r(3)), value - L.hall, s32(e.u32(value)), e.r(5) & 0xFF])
    if L.ret("winmut", 0) != 0:
        be(e, value, L.ret("winval"), 4)
    e.set_r(3, L.ret("won") & 0xFF)


def make_id(name, key):
    def stub(e):
        L = e.cl
        L.rec(name, [chal_row(L, e.r(3))])
        e.set_r(3, L.ret(key) & MASK)
    return stub


def make_var(name, load, size):
    def stub(e):
        L = e.cl
        value = e.r(4)
        L.rec(name, [L.lab(e.r(3)), value - L.hall, s32(e.r(6))], [e.cstr(e.r(5))[:159]])
        answer = L.ret("ans", 0)
        write = L.ret("wr", 1)
        stored = L.ret("val", 0)
        if load and write != 0:
            be(e, value, stored, size)
        e.set_r(3, answer & MASK)
    return stub


def stub_funny(e):
    L = e.cl
    sp = e.r(1)
    outs = [e.r(i) for i in range(5, 11)] + [e.u32(sp + 0x38)]
    h = e.u32(sp + 0x3C)
    L.rec("GetFunnyTime", [L.lab(e.r(4))] + [L.lab(o) for o in outs])
    L.rec("GetFunnyTime.8", [L.lab(h)])
    e.write(e.r(3), (L.ret("ft") & M64).to_bytes(8, "big"))


def stub_plus(e):
    L = e.cl
    a, d = e.r(4), e.r(5)
    L.rec("Stamp+", [L.lab(a), s64(be64(e, a)), s64(be64(e, d))])
    e.write(e.r(3), (L.ret("plus") & M64).to_bytes(8, "big"))


def stub_post(e):
    L = e.cl
    this, m = e.r(3), e.r(4)
    assert e.u32(m) == CHALLENGE_VTABLE, hex(e.u32(m))
    hw = lambda a: int.from_bytes(e.read(a, 2), "big")
    L.rec("PostMessage", [L.lab(this), 0x19, e.u32(m + 4), s32(e.u32(m + 8)), s32(e.u32(m + 0xC)), e.read(m + 0x10, 1)[0], s32(e.u32(m + 0x14)),
                          s32(e.u32(m + 0x18)), s32(e.u32(m + 0x1C)), s32(e.u32(m + 0x20)), s32(e.u32(m + 0x24)), hw(m + 0x28)])
    L.rec("PostMessage.2", [hw(m + 0x2A), s32(e.u32(m + 0x2C))])
    e.set_r(3, T.u32(L.ret("post")))


def stub_new(e):
    L = e.cl
    size = e.r(3)
    fail = bool(L.ret("newnull", 0))
    L.rec("new", [size, int(fail)])
    p = 0 if fail else e.alloc(size, POISON * size)
    if p:
        L.news += 1
    e.set_r(3, p)


def stub_trace(e):
    L = e.cl
    text = e.cstr(e.r(4))
    L.rec("CTRACE", [e.r(3), s32(e.r(5)) if "%d" in text else 0], [text])


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", [s32(e.r(3))], [e.cstr(e.r(4))])


def stub_object(name, key, args=False):
    def stub(e):
        L = e.cl
        L.rec(name, [L.lab(e.r(3))] + ([s32(e.r(4))] if args else []))
        e.set_r(3, L.ret(key) & MASK)
    return stub


def stub_get_analyser(e):
    L = e.cl
    L.rec("GetParkAnalyser", [L.lab(e.r(3))])
    e.set_r(3, L.analyser)


def stub_get_bank(e):
    L = e.cl
    L.rec("GetBank", [L.lab(e.r(3))])
    e.set_r(3, L.bank)


def stub_deposit(e):
    L = e.cl
    L.rec("MakeDeposit", [L.lab(e.r(3)), e.r(4) & MASK])


for _a, _f in ((THING_SERIALISE, stub_thing_serialise), (CHAL_SERIALISE, stub_chal_serialise), (DID_WIN, stub_did_win),
               (OFFER_ID, make_id("GetOfferId", "oid")), (WIN_ID, make_id("GetWinId", "wid")), (LOSE_ID, make_id("GetLoseId", "lid")),
               (LOADVAR_I, make_var("LOADVAR<int>", True, 4)), (SAVEVAR_I, make_var("SAVEVAR<int>", False, 4)),
               (LOADVAR_B, make_var("LOADVAR<bool>", True, 1)), (SAVEVAR_B, make_var("SAVEVAR<bool>", False, 1)),
               (LOADSTRUCT_TS, make_var("LOADVARSTRUCT<TbTimeStamp>", True, 8)), (SAVESTRUCT_TS, make_var("SAVEVARSTRUCT<TbTimeStamp>", False, 8)),
               (LOADSTRUCT_ID, make_var("LOADVARSTRUCT<ThingId>", True, 2)), (SAVESTRUCT_ID, make_var("SAVEVARSTRUCT<ThingId>", False, 2)),
               (FUNNY, stub_funny), (POST, stub_post), (TRACE, stub_trace), (ASSERT, stub_assert), (GET_ANALYSER, stub_get_analyser),
               (GET_BANK, stub_get_bank), (FILTH, stub_object("Filth", "filth")), (HAPPY, stub_object("Happy", "happy")),
               (HAPPY_TYPE, stub_object("HappyType", "happyt", True)), (DEPOSIT, stub_deposit)):
    STUBS.internal(_a, _f)
STUBS.imp("__pl__11TbTimeStampCFRC10TbTimeDiff", stub_plus)
STUBS.imp("__nw__FUl", stub_new)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)

    def get_type(emu):
        emu.cl.rec("GetType", [emu.cl.lab(emu.r(3))])
        emu.set_r(3, emu.cl.msg_type & MASK)

    tvec = T.fake_function(e, get_type)
    e.mc_vtable = T.persist_alloc(e, 0x20)
    e.write(e.mc_vtable + 8, struct.pack(">I", tvec))

    def construct(emu, call):
        p, ctor, dtor, size, count = (emu.r(i) for i in range(3, 8))
        if ctor == CENTRE_CTOR:
            emu.cl.rec("CMessageCentre", [emu.cl.newlab(p), int(ctor == CENTRE_CTOR), int(dtor == CENTRE_DTOR), size, count])

    T.stub_cb(e, "__construct_array", construct)


# ---------------------------------------------------------------------------------------------------------------- the two sides


def qn(q):
    return {k: [s64(v) for v in vs] for k, vs in q.items()}


class Side:
    """What the shared code needs from either memory (order: the byte order of the typed fields)."""

    def put(self, a, value, size):
        self.write(a, (value & ((1 << (8 * size)) - 1)).to_bytes(size, self.order))

    def get(self, image, off, size):
        return int.from_bytes(bytes(image[off:off + size]), self.order)


class Ppc(Side):
    order = "big"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, qn(case["queues"]))
        self.L.news = 0
        self.L.msg_type = case["mtype"]
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n, POISON * n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def label(self, a, n):
        self.L.labels[a] = n

    def log(self):
        return self.L.log

    def globals(self, hall, world, game, analyser, bank, centre):
        e = self.e
        self.L.hall, self.L.analyser, self.L.bank = hall, analyser, bank
        self.put(G_WORLD, world, 4)
        self.put(G_GAME, game, 4)
        self.put(G_CENTRE, centre, 4)

    def balance(self, slot, value):
        self.put(G_BALANCE + 8 + 4 * slot, value, 4)

    def centre_label(self):
        return self.L.lab(self.e.u32(G_CENTRE))

    def news(self):
        return self.L.news

    def call(self, fn, args, r29=None):
        if r29 is not None:
            self.e.set_r(29, r29)
        return T.call_ppc(self.e, fn, args)[0] & MASK

    def message(self, case, m):
        """The message block: the vtable, the flags and the words of the case's type."""
        self.put(m, self.e.mc_vtable, 4)
        self.put(m + 4, case["mflags"], 4)
        t, w = case["mtype"], case["mw"]
        if t == 0x11:
            self.put(m + 8, w[0], 2)
            self.put(m + 0xC, w[1], 4)
        elif t == 0x1A:
            self.put(m + 8, w[0], 4)
            self.put(m + 0xC, w[1], 4)
            self.put(m + 0x10, w[2], 2)
            self.put(m + 0x12, w[3], 2)

    def receive(self, case, hall):
        m = self.alloc(0x40)
        self.label(m, L_MSG)
        self.message(case, m)
        return self.call(RECEIVE, [hall, m], r29=0)


class Host(Side):
    order = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.fixed = {}
        self.queues = qn(case["queues"])

    def start(self):
        self.fixed[L_MSG] = self.lib.tpw_chq_msg_addr()
        CL.host_begin(self.lib, self.fixed, self.queues)

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_chq_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def label(self, a, n):
        self.fixed[n] = a

    def log(self):
        return CL.host_end(self.lib)

    def globals(self, hall, world, game, analyser, bank, centre):
        self.lib.tpw_chq_prepare(hall, world, game, analyser, bank, centre)

    def balance(self, slot, value):
        self.lib.tpw_chq_balance(slot, s32(value))

    def centre_label(self):
        out = (C.c_int64 * 2)()
        self.lib.tpw_chq_globals(out)
        return out[0]

    def news(self):
        out = (C.c_int64 * 2)()
        self.lib.tpw_chq_globals(out)
        return out[1]

    def call(self, fn, args, r29=None):
        return getattr(self.lib, "tpw_chq_" + HOST_NAMES[fn])(*[a & M64 for a in args]) & MASK

    def receive(self, case, hall):
        w = case["mw"] + [0, 0, 0, 0]
        return self.lib.tpw_chq_receive(hall, case["mtype"], case["mflags"], w[0] & MASK, w[1] & MASK, w[2] & 0xFFFF, w[3] & 0xFFFF) & MASK


HOST_NAMES = {FINAL: "final", SERIALISE: "serialise", ADD: "add", CURRENT: "current", OFFER: "offer", TELL: "tell", ACCEPT: "accept",
              DECLINE: "decline", END: "end", WON: "won", LOST: "lost"}


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    p = "tpw_chq_"
    for name in ("final", "serialise", "add", "current", "receive", "offer", "tell", "accept", "decline", "end", "won", "lost"):
        fn = getattr(lib, p + name)
        fn.restype = C.c_uint64
        fn.argtypes = [C.c_uint64] * {"serialise": 5, "add": 5, "receive": 7, "tell": 3}.get(name, 1)
    lib.tpw_chq_msg_addr.restype = C.c_uint64
    lib.tpw_chq_alloc.restype, lib.tpw_chq_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_chq_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_chq_prepare.argtypes = [C.c_uint64] * 6
    lib.tpw_chq_balance.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_chq_globals.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_chq_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's globals and blocks through Side: the same code for both memories (offsets are the Mac's). Returns the hall's address."""
    block = s.alloc(GUARD + HALL_SIZE + GUARD)
    hall = block + GUARD
    s.write(block, case["guard"][:GUARD])
    s.write(hall + HALL_SIZE, case["guard"][GUARD:])
    for (off, size) in FIELDS:
        s.put(hall + off, case["hall"][off], size)
    file = s.alloc(0x40)
    world = s.alloc(0x300)
    game = s.alloc(0x300)
    analyser = s.alloc(0x40)
    bank = s.alloc(0x40)
    centre = s.alloc(0x1D0) if case["centre"] else 0
    for a, n in ((hall, L_HALL), (file, L_FILE), (world, L_WORLD), (world + 0x2A0, L_PT), (game, L_GAME), (analyser, L_ANALYSER), (bank, L_BANK)):
        s.label(a, n)
    if centre:
        s.label(centre, L_CENTRE)
    s.globals(hall, world, game, analyser, bank, centre)
    for slot, value in case["bal"].items():
        s.balance(slot, value)
    s.file = file
    s.start() if hasattr(s, "start") else None
    return hall


def execute(s, case, hall):
    """The call; the answer of a void function is 0."""
    op, a = case["op"], case["args"]
    if op == "final":
        s.call(FINAL, [hall])
        return 0
    if op == "ser":
        return s32(s.call(SERIALISE, [hall, s.file, a[0], a[1], a[2]]))
    if op == "add":
        if isinstance(s, Ppc):
            s.call(ADD, [hall, a[0], a[1], (a[2] << 16) | case["lo"][0], (a[3] << 16) | case["lo"][1]])
        else:
            s.call(ADD, [hall, a[0], a[1], a[2], a[3]])
        return 0
    if op == "cur":
        r = s.call(CURRENT, [hall])
        return (r - hall) & MASK if r else 0
    if op == "recv":
        s.receive(case, hall)
        return 0
    if op == "offer":
        return s.call(OFFER, [hall]) & 0xFF
    if op == "tell":
        k = a[0]
        return s.call(TELL, [hall, hall + row(k, 0) if k >= 0 else 0, a[1]]) & 0xFF
    fn = {"accept": ACCEPT, "decline": DECLINE, "end": END, "won": WON, "lost": LOST}[op]
    s.call(fn, [hall])
    return 0


def observe(s, case, hall, answer):
    """Everything the call may change, read back the same way on both sides."""
    img = s.read(hall - GUARD, GUARD + HALL_SIZE + GUARD)
    return {"answer": answer, "log": s.log(), "fields": [s.get(img, GUARD + off, size) for off, size in FIELDS],
            "guard": bytes(img[:GUARD]) + bytes(img[GUARD + HALL_SIZE:]), "centre": s.centre_label(), "news": s.news()}


def run(s, case):
    try:
        hall = build(s, case)
        return observe(s, case, hall, execute(s, case, hall))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

INTS = [0, 1, -1, 2, -2, 3, 7, 30, 100, 0x7FFFFFFF, -0x80000000, -0x7FFFFFFF]
PRIZES = [0, 1, -1, 2, 3, -3, 5, 1000, 4999, 0x7FFFFFFF, -0x80000000, -0x7FFFFFFF]
FLAGS = [0, 0, 1, 1, 2, 0x80, 0xFF]


def rint(r):
    return r.choice(INTS + [r.getrandbits(32), r.randrange(-300, 300)]) & MASK


def rticks(r):
    return r.choice([r.getrandbits(64), 0, M64, 0x7FFFFFFFFFFFFFFF, 0x8000000000000000, r.randrange(0, 400) * DAY, r.randrange(0, DAY)]) & M64


def mk_rows(r):
    types = [r.choice([0, 1, 2, 3, 0x10, 0x18, 0x19, 0x1A, 0x1B, r.randrange(1, 0x24), r.randrange(1, 6)]) for _ in range(20)]
    rows = []
    for i in range(20):
        rows.append({0: types[i], 4: rint(r), 8: rint(r), 0xC: r.choice([0, 1, 2, 3, r.getrandbits(16)]), 0xE: r.getrandbits(16),
                     0x10: r.choice([r.getrandbits(32), 0xFFFFFFFF, 0, 1, 2, 3, 4, 5]), 0x14: r.choice(PRIZES + [r.getrandbits(32)]) & MASK,
                     0x18: r.choice([0, 0, r.choice(types), r.choice(types), r.choice(types), r.randrange(0, 0x24)]),
                     0x1C: r.choice(FLAGS), 0x1D: r.choice(FLAGS), 0x1E: r.getrandbits(8), 0x1F: r.getrandbits(8), 0x20: r.getrandbits(32),
                     0x24: r.choice([r.getrandbits(32), 0, 1, 0x7FFFFFFF, 0xFFFFFFFF]), 0x28: r.choice([r.getrandbits(32), 0, 1, 2, 3, 0xFFFFFFFF, 5]),
                     0x2C: r.choice(FLAGS), 0x2D: r.getrandbits(8), 0x2E: r.choice([r.getrandbits(16), 0, 1])})
    return rows


def mk_hall(r, over=None, rows=None):
    """Every field of the hall: random, then `over` (hall offset -> value) and `rows` (row index -> {row offset: value}) applied."""
    h = {off: r.getrandbits(8 * size) for off, size in HEAD_FIELDS}
    rowvals = mk_rows(r)
    for i, extra in (rows or {}).items():
        rowvals[i].update(extra)
    for i in range(20):
        for off, size in ROW_FIELDS:
            h[row(i, off)] = rowvals[i][off] & ((1 << (8 * size)) - 1)
    cur = r.choice(list(range(20)) * 2 + [-1, 20, 0x7FFFFFFF, 0x80000000, r.getrandbits(32), 19, 0]) & MASK
    h.update({CUR: cur, NEXT: rticks(r), SYS: r.choice(FLAGS), ON: r.choice(FLAGS), WAIT: r.choice(FLAGS), 0x3DB: r.getrandbits(8), VALUE: rint(r),
              THING: r.choice([0, 1, 2, r.getrandbits(16)]), 0x3E2: r.getrandbits(8), 0x3E3: r.getrandbits(8)})
    for off, v in (over or {}).items():
        h[off] = v & ((1 << (8 * dict(FIELDS)[off])) - 1)
    return h


def mk_queues(r):
    return {"ft": [rticks(r) for _ in range(2)], "plus": [rticks(r) for _ in range(2)], "post": [r.choice([0, 1, 5, -1])],
            "newnull": [1 if r.random() < 0.12 else 0], "oid": [rint(r)], "wid": [rint(r)], "lid": [rint(r)], "won": [r.choice([0, 1, 0x100, 0x1FF])],
            "winmut": [r.choice([0, 0, 0, 1])], "winval": [rint(r)], "filth": [r.choice([0, 1, 50, 99, 100, 101, -1, rint(r)])],
            "happy": [r.choice([0, 1, 50, 100, -1, rint(r)])], "happyt": [r.choice([0, 1, 50, 100, -1, rint(r)])], "thing": [r.choice([0, 0, 5])],
            "chal": [r.choice([0, 0, 5])], "ans": [0], "wr": [1], "val": [rint(r)]}


def mk_case(r, op, args=None, over=None, rows=None, q=None, bal=None, centre=None, mtype=0, mw=None, lo=None):
    balance = {SLOT_DONE: r.choice(INTS + [r.randrange(0, 100)]), SLOT_DECLINED: r.choice(INTS + [r.randrange(0, 100)]),
               SLOT_FORFEIT: r.choice([0, 1, 2, 3, 4, 5, 10, -1, 0x7FFFFFFF, -0x80000000])}
    balance.update(bal or {})
    queues = mk_queues(r)
    queues.update(q or {})
    return {"op": op, "hall": mk_hall(r, over, rows), "guard": r.randbytes(2 * GUARD), "args": args or [], "queues": queues,
            "centre": r.choice([1, 1, 1, 0]) if centre is None else centre, "bal": balance, "mtype": mtype, "mflags": r.getrandbits(32),
            "mw": mw or [r.getrandbits(32) for _ in range(4)], "lo": lo or [r.getrandbits(16), r.getrandbits(16)]}


def cur_ok(r):
    """A current index that names a row."""
    return r.randrange(20)


def edge_cases(factory, items):
    r = random.Random(7)
    return [factory(r, *item) for item in items]


# ---- FinalDestroy and GetCurrentChallenge
def final_case(r):
    return mk_case(r, "final")


def final_edges():
    r = random.Random(1751)
    return [final_case(r) for _ in range(3)]


def cur_case(r, index=None):
    return mk_case(r, "cur", over=None if index is None else {CUR: index})


def cur_edges():
    r = random.Random(1752)
    return [cur_case(r, i) for i in (-1, 0, 1, 18, 19, 20, 21, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x80000001, 0x100000000 - 20)]


# ---- AddToCounter
def add_case(r, **kw):
    """Each test of the original failing alone is an edge; the random cases pick matching and broken parts."""
    cur = kw.get("cur", r.randrange(20))
    ty, obj = r.choice([1, 0x10, 0x18, 0x1A, r.randrange(1, 0x24)]), r.choice([0, 1, 2, r.getrandbits(16)])
    rowset = {"cur": cur, "rows": {}}
    type_match = kw.get("type_match", r.random() < 0.7)
    obj_match = kw.get("obj_match", r.random() < 0.7)
    thing_mode = kw.get("thing_mode", r.choice(["none", "same", "same", "other"]))
    on = kw.get("on", r.choice([0, 1, 1, 1, 2, 0x80]))
    amount = kw.get("amount", r.choice(INTS + [r.getrandbits(32)]))
    thing = r.choice([1, 2, r.randrange(1, 0xFFFF)])
    row_over = {0: ty if type_match else (ty + 1 + r.randrange(3)) & 0xFF, 0xC: obj if obj_match else (obj + 1 + r.randrange(3)) & 0xFFFF}
    actual = {"none": 0, "same": thing, "other": (thing + 1) & 0xFFFF}[thing_mode]
    return mk_case(r, "add", args=[ty, amount & MASK, obj, thing], over={CUR: cur, ON: on, THING: actual}, rows={cur: row_over}, centre=1)


def add_edges():
    r = random.Random(1753)
    out = []
    for on in (0, 1, 2, 0x80):
        for tm in (True, False):
            for om in (True, False):
                for mode in ("none", "same", "other"):
                    out.append(add_case(r, on=on, type_match=tm, obj_match=om, thing_mode=mode))
    for amount in (0, 1, -1, 0x7FFFFFFF, -0x80000000, 5):
        for v in (0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
            c = add_case(r, on=1, type_match=True, obj_match=True, thing_mode="none", amount=amount)
            c["hall"][VALUE] = v
            out.append(c)
    for cur in (-1, 20, 0x7FFFFFFF, 0x80000000):
        c = add_case(r, on=1, type_match=True, obj_match=True)
        c["hall"][CUR] = cur & MASK
        out.append(c)
    return out


# ---- ReceiveMessage
def recv_case(r, mtype=None, ctype=None, staff=None, **kw):
    mtype = r.choice([0xB, 0xB, 0xB, 0x11, 0x1A, 0x1A, 0x17, 0, 0x19, 0x7FFFFFFF]) if mtype is None else mtype
    ctype = r.choice([0x18, 0x19, 0x1B, 0x1A, 0x10, 0, 3]) if ctype is None else ctype
    staff = r.choice([-1, 0, 1, 2, 3, 4, 5, 6, 0x7FFFFFFF, r.getrandbits(32)]) if staff is None else staff
    cur = kw.get("cur", r.randrange(20))
    on = kw.get("on", r.choice([0, 1, 1, 1, 2]))
    typ_match = r.random() < 0.7
    # an add message that matches the current challenge's type and object, now and then
    row_over = {0: ctype, 0x10: staff & MASK}
    mw = [r.getrandbits(32) for _ in range(4)]
    if mtype == 0x1A:
        mw[0] = ctype if kw.get("add_match", r.random() < 0.6) else r.randrange(0x24)
        mw[1] = rint(r)
        row_over[0xC] = mw[2] = kw.get("obj", r.choice([0, 1, 2, r.getrandbits(16)]))
        mw[2] &= 0xFFFF
        mw[3] &= 0xFFFF
    if mtype == 0x11:
        mw[0] &= 0xFFFF
        mw[1] = r.choice([0, 0, 1, 2, 0x100, 0xFFFFFFFF, r.getrandbits(32)])
        row_over[0] = kw.get("rtype", r.choice([0x10, 0x10, ctype]))
        row_over[0xC] = 0 if r.random() < 0.8 else r.getrandbits(16)
    if mtype == 0xB:
        row_over[0xC] = 0 if r.random() < 0.8 else r.getrandbits(16)
    mw.append(0)
    c = mk_case(r, "recv", over={CUR: cur, ON: on, THING: r.choice([0, 0, 1, 2])}, rows={cur: row_over}, mtype=mtype, mw=mw[:4], centre=1)
    return c


def recv_edges():
    r = random.Random(1754)
    out = []
    for mtype in (0xB, 0x11, 0x1A, 0x17, 0, 0x19, 1, 2, 0x7FFFFFFF):
        out.append(recv_case(r, mtype=mtype))
    for ctype in (0x18, 0x19, 0x1A, 0x1B, 0x10, 0, 3, 0x17, 0x1C):
        for on in (0, 1):
            for q in (0, 1, 50, 99, 100, 101, -1, 0x7FFFFFFF, -0x80000000):
                c = recv_case(r, mtype=0xB, ctype=ctype, on=on)
                c["queues"].update({"filth": [q], "happy": [q], "happyt": [q]})
                out.append(c)
    for staff in (-1, 0, 1, 2, 3, 4, 5, 6, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
        out.append(recv_case(r, mtype=0xB, ctype=0x1B, staff=staff, on=1))
    for cur in (-1, 20, 0x7FFFFFFF, 0x80000000):
        for mtype in (0xB, 0x11, 0x1A):
            c = recv_case(r, mtype=mtype)
            c["hall"][CUR] = cur & MASK
            out.append(c)
    for cat in (0, 1, 2, 0x100, 0xFFFFFFFF):
        for on in (0, 1):
            c = recv_case(r, mtype=0x11, rtype=0x10, on=on)
            c["mw"][1] = cat
            out.append(c)
    for match in (True, False):
        for on in (0, 1):
            out.append(recv_case(r, mtype=0x1A, add_match=match, on=on))
    return out


# ---- Serialise
def ser_case(r, save=None, version=None, nonzero=None):
    version = r.choice([-1, 0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 1, 1, 0]) if version is None else version
    save = r.choice([0, 1, 2, 0x100]) if save is None else save
    ans = [0] * 20 if nonzero is None else [0] * nonzero + [r.choice([1, -1, 5])]
    vals = [r.getrandbits(32) for _ in range(3)]
    q = {"ans": ans, "wr": [r.choice([1, 1, 0])], "val": [rticks(r) if r.random() < 0.5 else rint(r)], "thing": [r.choice([0, 3])], "chal": [r.choice([0, 7])]}
    c = mk_case(r, "ser", args=[save, version & MASK, r.getrandbits(32)], q=q, centre=1)
    c["queues"]["val"] = [rint(r), rticks(r), rint(r), r.getrandbits(8), r.choice([0, 1, 2]), rint(r), r.getrandbits(16)][:r.randrange(1, 8)]
    return c


def ser_edges():
    r = random.Random(1755)
    out = []
    for version in (-1, 0, 1, 2, 0x7FFFFFFF, 0x80000000):
        for save in (0, 1, 2):
            out.append(ser_case(r, save=save, version=version))
    for save in (0, 1):
        for nonzero in range(7):
            out.append(ser_case(r, save=save, version=1, nonzero=nonzero))
            out.append(ser_case(r, save=save, version=0, nonzero=nonzero))
    for save in (0, 1):
        for k in range(7):
            c = ser_case(r, save=save, version=1)
            # loaded values that differ from every field's current value, one call at a time
            c["queues"]["val"] = [0xAB, 0xCD0000000000EF01, 0x12, 0x34, 0x56, 0x789ABCDE, 0xBEEF][k:] or [0]
            c["queues"]["wr"] = [1]
            out.append(c)
    return out


# ---- the follow-up chain: OfferFollowup
def offer_case(r, **kw):
    cur = kw.get("cur", r.randrange(20))
    types = r.sample(range(1, 0x24), 20)
    follow = r.choice([0, 0, types[r.randrange(20)], types[r.randrange(20)], 99])
    rows = {i: {0: types[i], 0x2C: r.choice([0, 0, 0, 1, 2]), 0x28: r.choice([0, 1, 2, 3, 4, 0xFFFFFFFF, 0x80000000])} for i in range(20)}
    if r.random() < 0.3:
        rows[r.randrange(20)][0] = 0
    rows[cur][0x18] = follow
    on = kw.get("on", r.choice([0, 0, 0, 1, 2]))
    c = mk_case(r, "offer", over={CUR: cur, ON: on}, rows=rows, bal={SLOT_FORFEIT: r.choice([0, 1, 2, 3, 4, 5, -1])})
    return c


def offer_edges():
    r = random.Random(1756)
    out = []
    for on in (0, 1, 2):
        for cur in (-1, 0, 5, 19, 20, 0x80000000):
            c = offer_case(r, cur=cur if 0 <= cur < 20 else 0, on=on)
            c["hall"][CUR] = cur & MASK
            out.append(c)
    for forfeit in (-1, 0, 1, 2, 3, 4):
        for declined in (0, 1, 2, 3, 4, 0xFFFFFFFF, 0x80000000):
            for won in (0, 1, 2):
                c = offer_case(r, on=0)
                cur = c["hall"][CUR] if 0 <= c["hall"][CUR] < 20 else 0
                follow = c["hall"][row(cur, 0x18)]
                for i in range(20):
                    if c["hall"][row(i, 0)] == follow and follow:
                        c["hall"][row(i, 0x28)] = declined
                        c["hall"][row(i, 0x2C)] = won
                c["bal"][SLOT_FORFEIT] = forfeit
                out.append(c)
    for newnull in (0, 1):
        for centre in (0, 1):
            c = offer_case(r, on=0)
            c["queues"]["newnull"] = [newnull]
            c["centre"] = centre
            out.append(c)
    # the followup names several rows, or the current one itself, or a row of type 0
    c = offer_case(r, on=0, cur=3)
    for i in (3, 7, 12):
        c["hall"][row(i, 0)] = 5
        c["hall"][row(i, 0x2C)] = 0
        c["hall"][row(i, 0x28)] = 0
    c["hall"][row(3, 0x18)] = 5
    c["bal"][SLOT_FORFEIT] = 3
    out.append(c)
    return out


# ---- TellPlayerChallengeOffered
def tell_case(r, k=None, force=None):
    k = r.choice([-1, 0, 1, 5, 19, r.randrange(20)]) if k is None else k
    return mk_case(r, "tell", args=[k, r.choice([0, 1]) if force is None else force])


def tell_edges():
    r = random.Random(1757)
    out = []
    for k in (-1, 0, 7, 19):
        for force in (0, 1):
            for newnull in (0, 1):
                for centre in (0, 1):
                    c = tell_case(r, k, force)
                    c["queues"]["newnull"] = [newnull]
                    c["centre"] = centre
                    out.append(c)
    return out


# ---- ChallengeAccepted, ChallengeDeclined
def accept_case(r, wait=None, cur=None, tt=None):
    cur = r.choice([r.randrange(20), r.randrange(20), -1, 20, 0x80000000, 0x7FFFFFFF]) if cur is None else cur
    wait = r.choice(FLAGS) if wait is None else wait
    rows = {} if not 0 <= (cur if cur < 1 << 31 else cur - (1 << 32)) < 20 else {cur: {4: rint(r) if tt is None else tt}}
    return mk_case(r, "accept", over={CUR: cur & MASK, WAIT: wait}, rows=rows)


def accept_edges():
    r = random.Random(1758)
    out = []
    for wait in (0, 1, 2, 0x80):
        for cur in (-1, 0, 19, 20, 0x80000000):
            out.append(accept_case(r, wait, cur))
    for tt in (0, 1, 2, 7, 30, -1, -2, 0x7FFFFFFF, 0x80000000, 0x80000001, 21474836, 21474837, 0x10000000, 0x20000000):
        out.append(accept_case(r, 1, 4, tt))
    return out


def decline_case(r, wait=None, indep=None, prize=None, cur=None):
    wait = r.choice(FLAGS) if wait is None else wait
    cur = r.randrange(20) if cur is None else cur
    row_over = {0x1D: r.choice(FLAGS) if indep is None else indep, 0x14: r.choice(PRIZES) & MASK if prize is None else prize & MASK}
    return mk_case(r, "decline", over={CUR: cur, WAIT: wait, THING: r.choice([0, 1, 7, r.getrandbits(16)])}, rows={cur: row_over})


def decline_edges():
    r = random.Random(1759)
    out = []
    for wait in (0, 1, 2, 0x80):
        for indep in (0, 1, 2, 0x80):
            out.append(decline_case(r, wait, indep))
    for prize in (0, 1, -1, 2, 3, -2, -3, 5, -5, 0x7FFFFFFF, -0x80000000, -0x7FFFFFFF, 1000):
        out.append(decline_case(r, 1, 0, prize))
    for days in (0, 1, -1, 5, 0x7FFFFFFF, -0x80000000, 21474836, 21474837):
        c = decline_case(r, 1, 0)
        c["bal"][SLOT_DECLINED] = days
        out.append(c)
    c = decline_case(r, 0, 0, cur=0)
    c["hall"][CUR] = 0x80000000
    out.append(c)
    return out


# ---- EndCurrentChallenge, WonCurrentChallenge, LostCurrentChallenge
def end_case(r, cur=None, won=None):
    cur = r.choice([r.randrange(20), r.randrange(20), -1, 20]) if cur is None else cur
    c = mk_case(r, "end", over={CUR: cur & MASK})
    if won is not None:
        c["queues"]["won"] = [won]
    return c


def end_edges():
    r = random.Random(1760)
    out = []
    for cur in (-1, 0, 19, 20, 0x7FFFFFFF, 0x80000000):
        for won in (0, 1, 0x100, 0x1FF):
            for mut in (0, 1):
                c = end_case(r, cur, won)
                c["queues"]["winmut"] = [mut]
                out.append(c)
    return out


def won_case(r, cur=None, follow=None, **kw):
    cur = r.choice([r.randrange(20), r.randrange(20), r.randrange(20), -1, 20, 0x80000000]) if cur is None else cur
    follow = r.choice([0, 0, 1, 2]) if follow is None else follow
    types = r.sample(range(1, 0x24), 20)
    rows = {i: {0: types[i], 0x2C: r.choice([0, 0, 0, 1, 2]), 0x28: r.choice([0, 1, 2, 3, 4, 0xFFFFFFFF])} for i in range(20)}
    if 0 <= cur < 20:
        rows[cur][0x18] = 0 if follow == 0 else (types[r.randrange(20)] if follow == 1 else 99)
        rows[cur][0x2E] = r.getrandbits(16)
    for i in rows:
        rows[i].update(kw.get("row", {}))
    c = mk_case(r, "won", over={CUR: cur & MASK, ON: r.choice([0, 1, 1, 2])}, rows=rows, bal={SLOT_FORFEIT: r.choice([0, 1, 2, 3, 4, 5, -1])})
    return c


def won_edges():
    r = random.Random(1761)
    out = []
    for cur in (-1, 20, 0x80000000, 0, 9, 19):
        for follow in (0, 1, 2):
            for forfeit in (0, 3, 5):
                c = won_case(r, cur, follow)
                c["bal"][SLOT_FORFEIT] = forfeit
                out.append(c)
    for on in (0, 1, 2):
        out.append(won_case(r, 4, 1))
        out[-1]["hall"][ON] = on
    for days in (0, 1, -1, 0x7FFFFFFF, -0x80000000, 21474837):
        c = won_case(r, 2, 0)
        c["bal"][SLOT_DONE] = days
        out.append(c)
    for prize in (0, 1, -1, 0x7FFFFFFF, -0x80000000):
        c = won_case(r, 6, 0)
        c["hall"][row(6, 0x14)] = prize & MASK
        out.append(c)
    # the follow-up: more than one row of the wanted type; one the player may be offered; one declined too often
    for centre in (0, 1):
        for newnull in (0, 1):
            c = won_case(r, 5, 1)
            c["centre"] = centre
            c["queues"]["newnull"] = [newnull]
            out.append(c)
    return out


def lost_case(r, cur=None):
    cur = r.choice([r.randrange(20), r.randrange(20), -1, 20, 0x80000000]) if cur is None else cur
    return mk_case(r, "lost", over={CUR: cur & MASK, THING: r.choice([0, 1, 9])})


def lost_edges():
    r = random.Random(1762)
    out = []
    for cur in (-1, 0, 19, 20, 0x80000000):
        for newnull in (0, 1):
            for centre in (0, 1):
                c = lost_case(r, cur)
                c["queues"]["newnull"] = [newnull]
                c["centre"] = centre
                out.append(c)
    for lost in (0, 1, 0x7FFFFFFF, 0xFFFFFFFF):
        c = lost_case(r, 3)
        c["hall"][row(3, 0x24)] = lost
        out.append(c)
    for days in (0, 1, -1, 0x7FFFFFFF, -0x80000000, 21474837):
        c = lost_case(r, 3)
        c["bal"][SLOT_DONE] = days
        out.append(c)
    return out


TESTS = [
    FnTest("CChallengeHQ::FinalDestroy", ppc, host, edge=final_edges, rnd=final_case),
    FnTest("CChallengeHQ::Serialise", ppc, host, edge=ser_edges, rnd=ser_case),
    FnTest("CChallengeHQ::AddToCounter", ppc, host, edge=add_edges, rnd=add_case),
    FnTest("CChallengeHQ::GetCurrentChallenge", ppc, host, edge=cur_edges, rnd=cur_case),
    FnTest("CChallengeHQ::ReceiveMessage", ppc, host, edge=recv_edges, rnd=recv_case),
    FnTest("CChallengeHQ::OfferFollowup", ppc, host, edge=offer_edges, rnd=offer_case),
    FnTest("CChallengeHQ::TellPlayerChallengeOffered", ppc, host, edge=tell_edges, rnd=tell_case),
    FnTest("CChallengeHQ::ChallengeAccepted", ppc, host, edge=accept_edges, rnd=accept_case),
    FnTest("CChallengeHQ::ChallengeDeclined", ppc, host, edge=decline_edges, rnd=decline_case),
    FnTest("CChallengeHQ::EndCurrentChallenge", ppc, host, edge=end_edges, rnd=end_case),
    FnTest("CChallengeHQ::WonCurrentChallenge", ppc, host, edge=won_edges, rnd=won_case),
    FnTest("CChallengeHQ::LostCurrentChallenge", ppc, host, edge=lost_edges, rnd=lost_case),
]
