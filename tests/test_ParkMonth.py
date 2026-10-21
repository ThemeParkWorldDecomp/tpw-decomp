"""Unit w161, the park analyser's messages, month end, admissions, thoughts and cell save, against libtpw_unit_ParkMonth.dylib
(tests/unit_ParkMonth.cpp), main app ("SimTheme Park"):
  src/game/park/analyser.cpp: CParkAnalyser::ReceiveMessage 0x100e41bc, PersonPaidAdmission 0x100e4470, UpdateEndOfMonth 0x100e47fc,
    GetThreeMostDominantThoughts 0x100e5dc8, FinalDestroy 0x100e6fd0, CParkAnalyserCell::Serialise 0x100e60a0, __sinit_analyser_cpp 0x100f1fb0
  src/game/buildings/ResearchLab.cpp, src/game/buildings/StaffHQ.cpp, src/game/park/BankAccount.cpp, src/game/park/analyser.cpp, src/game/ui/MessageCentre.cpp: the constructors 0x1015210c 0x10151cfc 0x10151f14, GetType 0x100ef1f4 0x101244d0 0x1012c244 and destructors 0x100e4584
    0x1012412c 0x1012b4f4 of CMsgAddToChallenge, CMsgResearchCompleted and CMsgStaffInfo, and CBankAccount::GetBalance 0x100f32b4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for the
original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside the unit is a fake
on both sides that records its arguments and answers from the case: FinalDestroy, the asserts, GetWorld and the used-thing walk, IsInsidePark,
GetFunnyTime, GetRideInfo, SetLastOfObjectTypeResearched, the analyser's queries, the histories (SetAtEnd of int and float, SetFromTemp,
GetAverageOfLast), GetBankAccount, PostMessage and the message centre's new and construction, LOADVAR / SAVEVAR of int and the Serialise of the two
Val kinds.

The world of a case: the analyser block (0x21c48 bytes with 0xA5 guards, the counters, the six time stamps, the month totals and the largest price
typed), the world's address (never dereferenced), the bank, the ride record, the thing table and the things (kind at +2, thought at +0x30), the
message centre pointer (NULL: made on the first post), the message of ReceiveMessage (its vtable makes GetType a fake), the cell block and the file.
What is compared: the answer, the typed fields, the rest of the block byte for byte, the thoughts' output bytes, the message centre pointer's label,
the number of allocations and the event log. The companions compare the constructed object (flags, type, payload, the untouched bytes), GetType
(twice) and the destructor's delete and flags.

No fused operations (the only float is passed through). Not tested: a thought outside 0 .. 22 (the original indexes its counts without a check), a
NULL from GetRideInfo (the original reads through it), a thing id whose table row is NULL (the original reads through it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ParkMonth"
MASK = 0xFFFFFFFF
M64 = (1 << 64) - 1
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST)
RECEIVE, PAID, MONTH, THOUGHTS, FINAL, CELL, SINIT = 0x100E41BC, 0x100E4470, 0x100E47FC, 0x100E5DC8, 0x100E6FD0, 0x100E60A0, 0x100F1FB0
CTORS = [0x1015210C, 0x10151CFC, 0x10151F14]
GETTYPES = [0x100EF1F4, 0x101244D0, 0x1012C244]
DTORS = [0x100E4584, 0x1012412C, 0x1012B4F4]
VTABLES = [0x102A0554, 0x102A1118, 0x102A11FC]
BALANCE = 0x100F32B4
SIZES = {RECEIVE: 360, PAID: 216, MONTH: 528, THOUGHTS: 576, FINAL: 36, CELL: 1220, SINIT: 28, BALANCE: 8}
for _k in range(3):
    SIZES[CTORS[_k]] = (60, 44, 36)[_k]
    SIZES[GETTYPES[_k]] = 8
    SIZES[DTORS[_k]] = 76
OWN = set(SIZES)

# ---- the callees faked on both sides
ASSERT, GETWORLD, HEAD, NEXT, INSIDE, FUNNY, POST = 0x1000DDE8, 0x1014205C, 0x1013ADB0, 0x1013AE08, 0x10115A80, 0x101127D0, 0x1015256C
GETRIDE, SETLAST, GVIP, GRIP, HAPPY, SINCEB, PARKVALUE, RATING = 0x10154868, 0x100EA968, 0x100E5414, 0x100E558C, 0x100E4CD8, 0x100E7994, 0x100E5110, 0x100ED8CC
H_SET_I, H_SET_F, H_FROM_TEMP, H_AVG = 0x100F09DC, 0x100F069C, 0x100F0A48, 0x100F0180
VAL_I, VAL_F, LOADVAR_I, SAVEVAR_I, FINALDESTROY, GETBANK = 0x100EF278, 0x100EF4A8, 0x1000EF18, 0x1000F4B0, 0x1012E9F8, 0x1013EDFC
CENTRE_CTOR, CENTRE_DTOR = 0x102657E8, 0x10267288

# ---- the Mac's addresses of the globals; the host library's come from tpw_pn_addr(name)
G_WORLD, G_GAME_WORLD, G_CENTRE, TABLE, G_RDB, G_W19C, G_W1A0 = 0x1037EC0C, 0x1037EBFC, 0x1037F6C4, 0x1034CBFC, 0x1037F6C8, 0x1034C19C, 0x1034C1A0
# ---- the host's offsets and sizes the module relies on, checked once against tpw_pn_layout
HOST_LAYOUT = [0x21C48, 0x1F100, 0x250, 0x21BFC, 0x21C0C, 0x21C24, 0x21C3C, 0x7C, None, 0x2A0, 8, 0x1D0, 0x128, 0xC]
MAC_UI_TYPE = 0x7A8

ANALYSER, GUARD, CELL_BYTES, CELL_GUARD = 0x21C48, 0x40, 0x7C, 0x40
RIDE_BYTES, THING_BYTES, SLOTS, ROWS = 0x1000, 0x80, 8, 16
INT_OFFS = [0x21BFC, 0x21C00, 0x21C04, 0x21C08, 0x21C3C] + [0x1F100 + k * 0x250 for k in range(13)]
STAMP_OFFS = [0x21C0C, 0x21C14, 0x21C1C, 0x21C24, 0x21C2C, 0x21C34]
FILE_BYTES = 16
(L_AN, L_WORLD, L_PT, L_MSG, L_BANK, L_CENTRE, L_RDB, L_FILE, L_CELL) = range(1, 10)
L_THING = 10
POISON = b"\xa5"

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def s64(v):
    v &= M64
    return v - (1 << 64) if v & (1 << 63) else v


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes


def rec_off(e, a):
    return a - e.cl.base


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", [s32(e.r(3))], [e.cstr(e.r(4))])


def stub_getworld(e):
    e.cl.rec("GetWorld", [e.cl.lab(e.r(3))])
    e.set_r(3, e.cl.world)


def stub_head(e):
    e.cl.rec("GetUsedThingHead", [e.cl.lab(e.r(4))])
    e.write(e.r(3), struct.pack(">H", e.cl.ret("head", 0) & 0xFFFF))


def stub_next(e):
    e.cl.rec("GetNextUsedThing", [e.cl.lab(e.r(4)), e.cl.lab(e.r(5))])
    e.write(e.r(3), struct.pack(">H", e.cl.ret("next", 0) & 0xFFFF))


def stub_inside(e):
    e.cl.rec("IsInsidePark", [e.cl.lab(e.r(3))])
    e.set_r(3, e.cl.ret("inside", 0) & MASK)


def stub_funny(e):
    sp = e.r(1)
    e.cl.rec("GetFunnyTime", [e.cl.lab(e.r(4))] + [e.r(i) for i in range(5, 11)] + [e.u32(sp + 0x38), e.u32(sp + 0x3C)])
    e.write(e.r(3), (e.cl.ret("ft", 0) & M64).to_bytes(8, "big"))


def stub_post(e):
    this, m = e.r(3), e.r(4)
    vt = e.u32(m)
    hw = lambda a: int.from_bytes(e.read(a, 2), "big")
    e.cl.rec("PostMessage", [e.cl.lab(this), 0x1A if vt == VTABLES[0] else vt, e.u32(m + 4), s32(e.u32(m + 8)), s32(e.u32(m + 0xC)), hw(m + 0x10), hw(m + 0x12)])
    e.set_r(3, e.cl.ret("post", 0) & MASK)


def stub_getride(e):
    e.cl.rec("GetRideInfo", [e.cl.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF])
    e.set_r(3, e.cl.ride)


def stub_setlast(e):
    e.cl.rec("SetLast", [rec_off(e, e.r(3)), s32(e.r(4)), (e.r(5) >> 16) & 0xFFFF])


def stub_query(name, key):
    def stub(e):
        e.cl.rec(name, [rec_off(e, e.r(3)), e.r(4)])
        e.set_r(3, e.cl.ret(key, 0) & MASK)
    return stub


def stub_sinceb(e):
    e.cl.rec("NumberOfVisitorsSinceBeginning", [rec_off(e, e.r(3))])
    e.set_r(3, e.cl.ret("sinceb", 0) & MASK)


def stub_rating(e):
    e.cl.rec("GetOverallParkRating", [rec_off(e, e.r(3))])
    e.set_r(3, e.cl.ret("rating", 0) & MASK)


def stub_happy(e):
    e.cl.rec("GetHappinessLevel", [rec_off(e, e.r(3)), e.r(4)])
    e.set_f1(bitsf(e.cl.ret("hap", 0) & MASK))


def stub_set_i(e):
    e.cl.rec("HistSetAtEnd", [rec_off(e, e.r(3)), s32(e.r(4))])


def stub_set_f(e):
    e.cl.rec("HistSetAtEndF", [rec_off(e, e.r(3)), canon_f(fb(e.f(1)))])


def stub_from_temp(e):
    e.cl.rec("HistSetFromTemp", [rec_off(e, e.r(3)), s32(e.u32(e.r(3)))])


def stub_avg(e):
    e.cl.rec("HistAverage", [rec_off(e, e.r(3)), s32(e.r(5))])
    e.write(e.r(4), struct.pack(">i", s32(e.cl.ret("avg", 0))))


def stub_val(name):
    def stub(e):
        e.cl.rec(name, [rec_off(e, e.r(3)), e.cl.lab(e.r(4)), s32(e.r(5)), s32(e.r(6)), e.r(7)])
        e.set_r(3, e.cl.ret("ans", 0) & MASK)
    return stub


def make_var(name, loads):
    def stub(e):
        L = e.cl
        value = e.r(4)
        L.rec(name, [L.lab(e.r(3)), value - L.base, s32(e.r(6))], [e.cstr(e.r(5))[:159]])
        answer = L.ret("ans", 0)
        stored = L.ret("val", -1)
        if loads and stored != -1:
            e.write(value, (stored & MASK).to_bytes(4, "big"))
        e.set_r(3, answer & MASK)
    return stub


def stub_finaldestroy(e):
    e.cl.rec("FinalDestroy", [e.cl.lab(e.r(3))])


def stub_getbank(e):
    e.cl.rec("GetBankAccount", [e.cl.lab(e.r(3))])
    e.set_r(3, e.cl.bank)


def op_new(e):
    size = e.r(3)
    fail = bool(e.cl.ret("newnull", 0))
    e.cl.rec("new", [size, int(fail)])
    p = 0 if fail else e.alloc(size, POISON * size)
    if p:
        e.cl.news += 1
    e.set_r(3, p)


for _a, _f in ((ASSERT, stub_assert), (GETWORLD, stub_getworld), (HEAD, stub_head), (NEXT, stub_next), (INSIDE, stub_inside), (FUNNY, stub_funny),
               (POST, stub_post), (GETRIDE, stub_getride), (SETLAST, stub_setlast), (GVIP, stub_query("GetVisitorsInPark", "gvip")),
               (GRIP, stub_query("GetRidesInPark", "grip")), (PARKVALUE, stub_query("GetParkValue", "pv")), (HAPPY, stub_happy), (SINCEB, stub_sinceb),
               (RATING, stub_rating), (H_SET_I, stub_set_i), (H_SET_F, stub_set_f), (H_FROM_TEMP, stub_from_temp), (H_AVG, stub_avg),
               (VAL_I, stub_val("ValSerialise<int>")), (VAL_F, stub_val("ValSerialise<float>")), (LOADVAR_I, make_var("LOADVAR<int>", True)),
               (SAVEVAR_I, make_var("SAVEVAR<int>", False)), (FINALDESTROY, stub_finaldestroy), (GETBANK, stub_getbank)):
    STUBS.internal(_a, _f)
STUBS.imp("__nw__FUl", op_new)


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
        self.L.msg_type = case.get("mtype", 0)
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def label(self, a, n):
        self.L.labels[a] = n

    def set_base(self, a):
        self.L.base = a

    def log(self):
        return self.L.log

    def world(self, c, an):
        """Globals of the case: the world (an address only), the bank, the ride record, the centre, the thing table."""
        e = self.e
        blk = e.alloc(0x40)
        world = blk - 0x2A0
        self.L.world = world
        self.label(world, L_WORLD)
        self.label(world + 0x2A0, L_PT)
        self.label(G_RDB, L_RDB)
        bank = e.alloc(0x128, POISON * 0x128)
        self.label(bank, L_BANK)
        self.put(bank + 0xC, c["balance"], 4)
        self.L.bank = bank
        ride = e.alloc(RIDE_BYTES, POISON * RIDE_BYTES)
        self.put(ride + MAC_UI_TYPE, c["ui"], 4)
        self.L.ride = ride
        centre = 0
        if c["centre"]:
            centre = e.alloc(0x1D0, POISON * 0x1D0)
            self.label(centre, L_CENTRE)
        self.put(G_WORLD, world, 4)
        self.put(G_GAME_WORLD, world, 4)
        self.put(G_CENTRE, centre, 4)
        return dict(world=world, bank=bank, ride=ride, centre=centre)

    def things(self, c):
        addr = []
        for i, th in enumerate(c["things"]):
            a = self.e.alloc(THING_BYTES, POISON * THING_BYTES)
            self.label(a, L_THING + i)
            self.write(a + 2, bytes([th["kind"] & 0xFF]))
            self.put(a + 0x30, th["thought"], 4)
            addr.append(a)
        for row in range(ROWS):
            slot = c["rows"].get(row, 0)
            self.put(TABLE + 0x14 * row, addr[slot - 1] if slot else 0, 4)

    def file(self):
        f = self.e.alloc(FILE_BYTES, POISON * FILE_BYTES)
        self.label(f, L_FILE)
        return f

    def message(self, c):
        e = self.e
        m = e.alloc(0x40, POISON * 0x40)
        self.label(m, L_MSG)
        self.put(m, e.mc_vtable, 4)
        self.put(m + 4, c["mflags"], 4)
        t = c["mtype"]
        if t == 0x11:
            self.put(m + 8, c["marg"], 2)
        elif t == 0x17:
            self.put(m + 0xC, c["marg2"], 4)
        else:
            self.put(m + 8, c["marg"], 4)
            self.put(m + 0xC, c["marg2"], 4)
        return m

    def call(self, fn, args):
        return T.call_ppc(self.e, fn, args)[0] & MASK

    def centre_after(self):
        return self.L.lab(self.e.u32(G_CENTRE))

    def news(self):
        return self.L.news


class Host(Side):
    order = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.fixed = {}
        self.queues = qn(case["queues"])

    def start(self):
        self.fixed[L_RDB] = self.lib.tpw_pn_addr(b"rdb")
        CL.host_begin(self.lib, self.fixed, self.queues)

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_pn_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def label(self, a, n):
        self.fixed[n] = a

    def set_base(self, a):
        self.lib.tpw_pn_base(a)

    def log(self):
        return CL.host_end(self.lib)

    def world(self, c, an):
        blk = self.alloc(0x40)
        world = blk - 0x2A0
        self.label(world, L_WORLD)
        self.label(world + 0x2A0, L_PT)
        bank = self.alloc(0x128)
        C.memset(bank, 0xA5, 0x128)
        self.label(bank, L_BANK)
        self.put(bank + 0xC, c["balance"], 4)
        ride = self.alloc(RIDE_BYTES)
        C.memset(ride, 0xA5, RIDE_BYTES)
        self.put(ride + self.ui_off, c["ui"], 4)
        centre = 0
        if c["centre"]:
            centre = self.alloc(0x1D0)
            C.memset(centre, 0xA5, 0x1D0)
            self.label(centre, L_CENTRE)
        self.lib.tpw_pn_set(world, bank, ride, centre)
        return dict(world=world, bank=bank, ride=ride, centre=centre)

    def things(self, c):
        addr = []
        for i, th in enumerate(c["things"]):
            a = self.alloc(THING_BYTES)
            C.memset(a, 0xA5, THING_BYTES)
            self.label(a, L_THING + i)
            self.write(a + 2, bytes([th["kind"] & 0xFF]))
            self.put(a + 0x30, th["thought"], 4)
            addr.append(a)
        for row in range(ROWS):
            slot = c["rows"].get(row, 0)
            self.lib.tpw_pn_row(row, addr[slot - 1] if slot else 0)

    def file(self):
        f = self.alloc(FILE_BYTES)
        C.memset(f, 0xA5, FILE_BYTES)
        self.label(f, L_FILE)
        return f

    def message(self, c):
        self.label(self.lib.tpw_pn_msg(), L_MSG)
        return 0

    def call(self, fn, args):
        return fn(*args) & MASK

    def centre_after(self):
        return self.lib.tpw_pn_centre_label()

    def news(self):
        return self.lib.tpw_pn_news()


def qn(q):
    return {k: [s64(v) for v in vs] for k, vs in q.items()}


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_pn_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pn_base.argtypes = [C.c_uint64]
    lib.tpw_pn_set.argtypes = [C.c_uint64] * 4
    lib.tpw_pn_row.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_pn_msg.restype = C.c_uint64
    lib.tpw_pn_centre_label.restype = C.c_int64
    lib.tpw_pn_news.restype = C.c_int32
    lib.tpw_pn_receive.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_pn_paid.argtypes = [C.c_uint64, C.c_int32]
    lib.tpw_pn_month.argtypes = [C.c_uint64]
    lib.tpw_pn_thoughts.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_pn_final.argtypes = [C.c_uint64]
    lib.tpw_pn_cell.restype, lib.tpw_pn_cell.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_pn_balance.restype, lib.tpw_pn_balance.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_pn_ctor.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_pn_gettype.restype, lib.tpw_pn_gettype.argtypes = C.c_int32, [C.c_int32, C.c_uint32]
    lib.tpw_pn_dtor.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.POINTER(C.c_uint32)]
    lib.tpw_pn_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_pn_layout(out)
    got = list(out)[:n]
    assert len(got) == len(HOST_LAYOUT) and all(h is None or h == g for g, h in zip(got, HOST_LAYOUT)), got
    Host.ui_off = got[8]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_an(s, c):
    """The analyser block with its guards: 0xA5 everywhere, then the typed fields in the side's byte order; the log's offsets are from it."""
    block = s.alloc(ANALYSER + 2 * GUARD)
    s.write(block, POISON * (ANALYSER + 2 * GUARD))
    an = block + GUARD
    for off in INT_OFFS:
        s.put(an + off, c["ints"][off], 4)
    for off in STAMP_OFFS:
        s.put(an + off, c["stamps"][off], 8)
    s.set_base(an)
    s.label(an, L_AN)
    return block, an


def build(s, c):
    op = c["op"]
    st = {}
    if op == "cell":
        block = s.alloc(CELL_BYTES + 2 * CELL_GUARD)
        s.write(block, c["raw"])
        for k in range(4):
            s.put(block + CELL_GUARD + 4 * k, c["cellints"][k], 4)
        st["block"], st["obj"] = block, block + CELL_GUARD
        s.set_base(st["obj"])
        s.label(st["obj"], L_CELL)
        st["file"] = s.file()
        st.update(s.world(c, 0))
        if hasattr(s, "start"):
            s.start()
        return st
    if op == "balance":
        bank = s.alloc(0x128 + 2 * GUARD)
        s.write(bank, c["raw"])
        s.put(bank + GUARD + 0xC, c["bal"], 4)
        st["block"], st["obj"] = bank, bank + GUARD
        if hasattr(s, "start"):
            s.start()
        return st
    if op == "sinit":
        if hasattr(s, "start"):
            s.start()
        return st
    st["block"], st["obj"] = build_an(s, c)
    st.update(s.world(c, st["obj"]))
    s.things(c)
    if op == "receive":
        st["msg"] = s.message(c)
    if op == "thoughts":
        st["buf"] = s.alloc(16)
        s.write(st["buf"], POISON * 16)
    if hasattr(s, "start"):
        s.start()
    return st


class PpcRun(Ppc):
    def execute(self, c, st):
        op, an = c["op"], st.get("obj")
        if op == "receive":
            self.call(RECEIVE, [an, st["msg"]])
            return 0
        if op == "paid":
            self.call(PAID, [an, c["price"] & MASK])
            return 0
        if op == "month":
            self.call(MONTH, [an])
            return 0
        if op == "final":
            self.call(FINAL, [an])
            return 0
        if op == "thoughts":
            self.call(THOUGHTS, [an, st["buf"], st["buf"] + 4, st["buf"] + 8])
            return 0
        if op == "cell":
            return self.call(CELL, [an, st["file"], c["save"] & MASK, c["version"] & MASK, c["unused"] & MASK])
        if op == "balance":
            return self.call(BALANCE, [an])
        raise AssertionError(op)

    def sinit(self):
        for a in (G_W19C - 4, G_W19C, G_W1A0, G_W1A0 + 4):
            self.write(a, b"\x5a\xa5\x5a\xa5")
        self.call(SINIT, [])
        return [self.e.u32(a) for a in (G_W19C - 4, G_W19C, G_W1A0, G_W1A0 + 4)]


class HostRun(Host):
    def execute(self, c, st):
        op, an, L = c["op"], st.get("obj"), self.lib
        if op == "receive":
            L.tpw_pn_receive(an, c["mtype"], s32(c["marg"]) if c["mtype"] != 0x11 else c["marg"] & 0xFFFF, s32(c["marg2"]))
            return 0
        if op == "paid":
            L.tpw_pn_paid(an, s32(c["price"]))
            return 0
        if op == "month":
            L.tpw_pn_month(an)
            return 0
        if op == "final":
            L.tpw_pn_final(an)
            return 0
        if op == "thoughts":
            L.tpw_pn_thoughts(an, st["buf"])
            return 0
        if op == "cell":
            return L.tpw_pn_cell(an, st["file"], s32(c["save"]), s32(c["version"]), c["unused"] & MASK) & MASK
        if op == "balance":
            return L.tpw_pn_balance(an) & MASK
        raise AssertionError(op)

    def sinit(self):
        w = [self.lib.tpw_pn_addr(b"w19c"), self.lib.tpw_pn_addr(b"w1a0")]
        junk = 0x5AA55AA5
        return [junk, C.c_uint32.from_address(w[0]).value, C.c_uint32.from_address(w[1]).value, junk]


def observe(s, c, st, answer):
    op = c["op"]
    out = {"answer": answer, "log": s.log()}
    if op == "balance":
        image = bytearray(s.read(st["block"], 0x128 + 2 * GUARD))
        out["balance"] = s.get(image, GUARD + 0xC, 4)
        image[GUARD + 0xC:GUARD + 0x10] = b"\0\0\0\0"
        out["raw"] = bytes(image)
        return out
    if op == "cell":
        image = bytearray(s.read(st["block"], CELL_BYTES + 2 * CELL_GUARD))
        dec = []
        for off in (0, 4, 8, 12):
            dec.append(s.get(image, CELL_GUARD + off, 4))
            image[CELL_GUARD + off:CELL_GUARD + off + 4] = b"\0\0\0\0"
        out["fields"], out["raw"] = dec, bytes(image)
        return out
    image = bytearray(s.read(st["block"], ANALYSER + 2 * GUARD))
    ints, stamps = [], []
    for off in INT_OFFS:
        ints.append(s.get(image, GUARD + off, 4))
        image[GUARD + off:GUARD + off + 4] = b"\0\0\0\0"
    for off in STAMP_OFFS:
        stamps.append(s.get(image, GUARD + off, 8))
        image[GUARD + off:GUARD + off + 8] = b"\0" * 8
    out.update(ints=ints, stamps=stamps, raw=bytes(image), centre=s.centre_after(), news=s.news())
    if op == "thoughts":
        out["out"] = s.read(st["buf"], 16)
    return out


def run(s, c):
    try:
        if c["op"] == "sinit":
            if hasattr(s, "start"):
                s.start()
            return {"words": s.sinit(), "log": s.log()}
        st = build(s, c)
        return observe(s, c, st, s.execute(c, st))
    finally:
        s.done()


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


def ppc(e, case):
    return run(PpcRun(e, case), case)


def host(lib, case):
    return run(HostRun(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

INTS = [0, 1, -1, 2, 3, 5, 100, 1000, 0x7FFF, 0x8000, 0xFFFF, 0x10000, INT_MAX, INT_MIN, INT_MIN + 1, 0x12345678, -0x12345678]
STAMPS = [0, 1, -1, 0x100000000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x123456789ABCDEF0, -0x123456789ABCDEF0, (1 << 63) - 1, -(1 << 63)]
FLOATS = [0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x3FC00000, 0x42C80000, 0x7F800000, 0xFF800000, 0x7FC00000, 0x7F7FFFFF, 0x1E3CE508, 0xC2F6E979]
MSG_TYPES = [0xC, 0x11, 0x13, 0x17, 0, 1, 0xB, 0xD, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, -1, INT_MAX, INT_MIN]
KINDS = [1, 1, 1, 1, 0, 2, 3, 4, 0x13, 0x81, 0xFF]
UI_TYPES = [0, 1, 2, 3, 5, 7, 0x10, 0x20, 0x7F, -1]
WORDS = [0, 1, 2, 3, 0x100, 0x10000, 0x80000000, 0xFFFFFFFF, 0x7FFFFFFF, 0x20000]
QINTS = [0, 1, -1, 5, 100, 12345, INT_MAX, INT_MIN, 0x10000, 0xFFFF]


def ri(r):
    return r.choice(INTS) if r.random() < 0.7 else r.randrange(INT_MIN, INT_MAX + 1)


def mk_thing(r):
    return dict(kind=r.choice(KINDS), thought=r.choice([0, 1, 2, 3, 4, 5, 10, 21, 22, r.randrange(23)]))


def base(r, op):
    c = dict(op=op)
    c["ints"] = {off: ri(r) for off in INT_OFFS}
    c["stamps"] = {off: r.choice(STAMPS) if r.random() < 0.5 else r.randrange(-(1 << 63), 1 << 63) for off in STAMP_OFFS}
    c["balance"] = ri(r)
    c["ui"] = r.choice(UI_TYPES)
    c["centre"] = r.randrange(2)
    c["things"] = [mk_thing(r) for _ in range(r.randrange(1, SLOTS + 1))]
    ids = r.sample(range(1, ROWS), r.randrange(0, min(len(c["things"]) + 3, ROWS - 1) + 1))
    c["ids"] = ids
    c["rows"] = {i: r.randrange(1, len(c["things"]) + 1) for i in range(1, ROWS)}
    c["rows"][0] = r.choice([0, 0, 0, 1])
    c["price"] = ri(r)
    c["mtype"], c["mflags"] = r.choice(MSG_TYPES), r.choice(WORDS)
    c["marg"] = r.choice([0, 1, 2, 3, 4, -1, 0xFFFF, 0x10000, 0x8000, r.randrange(0, 0x10000)])
    c["marg2"] = r.choice([0, 1, 2, 3, 4, -1, INT_MAX, INT_MIN, r.randrange(INT_MIN, INT_MAX + 1)])
    c["queues"] = queues(r, c)
    return c


def queues(r, c):
    ids = c["ids"]
    return {"head": [ids[0] if ids else 0], "next": ids[1:] + [0], "inside": [r.choice(WORDS) for _ in range(len(ids) + 1)],
            "ft": [r.choice(STAMPS) if r.random() < 0.5 else r.randrange(-(1 << 63), 1 << 63)], "avg": [ri(r)], "gvip": [ri(r)], "grip": [ri(r)],
            "hap": [r.choice(FLOATS)], "sinceb": [ri(r)], "pv": [ri(r)], "rating": [ri(r)], "post": [ri(r)], "newnull": [r.choice([0, 0, 0, 1])],
            "ans": [0], "val": [-1]}


def with_(c, **kw):
    c = dict(c)
    for k, v in kw.items():
        c[k] = v
    return c


def thoughts_case(r, counts=None, inside=None):
    """Things whose thoughts follow `counts` (thought -> number of visitors), all inside the park."""
    c = base(r, "thoughts")
    things = []
    for t, n in (counts or {}).items():
        things += [dict(kind=1, thought=t)] * n
    if counts is not None:
        things = things[:ROWS - 1] or [dict(kind=0, thought=0)]
        c["things"] = things
        c["ids"] = list(range(1, len(things) + 1))
        c["rows"] = {i: i for i in range(1, len(things) + 1)}
        c["rows"][0] = 0
        c["queues"] = queues(r, c)
        c["queues"]["inside"] = [1]
    return c


def thoughts_edges():
    r = random.Random(1611)
    yield with_(base(r, "thoughts"), ids=[], queues=dict(base(r, "thoughts")["queues"], head=[0], next=[0]))
    for counts in ({}, {5: 1}, {3: 1}, {1: 2}, {2: 2, 1: 1}, {3: 1, 4: 1, 5: 1}, {3: 1, 4: 1, 5: 1, 6: 1}, {3: 2, 4: 2, 5: 2, 6: 2}, {22: 3, 21: 2, 20: 1},
                   {3: 3, 4: 2, 5: 2, 6: 1, 7: 1}, {1: 1, 2: 1, 3: 1}, {0: 4, 1: 1}, {0: 4}, {22: 1}, {5: 3, 6: 3, 7: 3}, {3: 1, 7: 2, 12: 2, 22: 4}):
        yield thoughts_case(r, counts)
    c = thoughts_case(r, {5: 2, 6: 1})
    c["rows"][0] = 1
    yield c
    c = thoughts_case(r, {5: 2, 6: 1})
    c["queues"]["inside"] = [0, 1, 0, 1]
    yield c
    c = thoughts_case(r, {5: 2, 6: 1})
    c["queues"]["inside"] = [0]
    yield c
    c = thoughts_case(r, {5: 2, 6: 1})
    c["things"] = [dict(kind=k, thought=5) for k in (0, 2, 3, 0x81, 0xFF, 1)]
    c["ids"] = list(range(1, 7))
    c["rows"] = {i: i for i in range(1, 7)}
    c["rows"][0] = 0
    c["queues"] = queues(r, c)
    yield c


def thoughts_random(r):
    c = base(r, "thoughts")
    for t in c["things"]:
        if r.random() < 0.7:
            t["kind"] = 1
    c["queues"]["inside"] = [r.choice([0, 1, 1, 1, 2, 0x100]) for _ in range(len(c["ids"]) + 1)]
    if r.random() < 0.7:
        c["rows"][0] = 0
    return c


def receive_edges():
    r = random.Random(1612)
    for t in MSG_TYPES:
        for a in (3, 2, 0, -1):
            yield with_(base(r, "receive"), mtype=t, marg=a if t != 0x11 else 5, marg2=2 if a == 3 else a)
    for t in (0x13, 0x17):
        for a in (0, 1, 2, 3, 4, 0x103, 0x10002, -1, INT_MIN, 0x80000003):
            yield with_(base(r, "receive"), mtype=t, marg=a, marg2=a)
    for ui in UI_TYPES:
        yield with_(base(r, "receive"), mtype=0x11, ui=ui, marg=r.randrange(0x10000))
    for idv in (0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x10001, -1):
        yield with_(base(r, "receive"), mtype=0x11, marg=idv)
    for fl in WORDS:
        yield with_(base(r, "receive"), mtype=0x13, marg=3, mflags=fl)


def receive_random(r):
    c = base(r, "receive")
    if r.random() < 0.6:
        c["mtype"] = r.choice([0xC, 0x11, 0x13, 0x17])
    if c["mtype"] == 0x13 and r.random() < 0.5:
        c["marg"] = 3
    if c["mtype"] == 0x17 and r.random() < 0.5:
        c["marg2"] = 2
    return c


def paid_edges():
    r = random.Random(1613)
    for price in INTS + [r.randrange(INT_MIN, INT_MAX + 1) for _ in range(6)]:
        for centre in (0, 1):
            yield with_(base(r, "paid"), price=price, centre=centre)
    for centre in (0, 1):
        for nn in (0, 1):
            for p in (INT_MIN, 7):
                c = base(r, "paid")
                c["centre"], c["price"] = centre, p
                c["queues"]["newnull"] = [nn]
                yield c
    for k in INT_OFFS[:1] + [0x21C3C]:
        for v in (INT_MIN, -1, 0, 5, INT_MAX):
            c = base(r, "paid")
            c["ints"][k] = v
            c["price"] = v
            yield c
    for v in (INT_MAX, INT_MIN):
        c = base(r, "paid")
        c["ints"][0x21C08] = v
        yield c


def paid_random(r):
    c = base(r, "paid")
    if r.random() < 0.5:
        c["price"] = c["ints"][0x21C3C] + r.choice([-1, 0, 1])
    return c


def month_edges():
    r = random.Random(1614)
    for bal in INTS:
        yield with_(base(r, "month"), balance=bal)
    for hap in FLOATS:
        c = base(r, "month")
        c["queues"]["hap"] = [hap]
        yield c
    for k in range(13):
        c = base(r, "month")
        c["ints"][0x1F100 + k * 0x250] = r.choice([0, 1, -1, INT_MAX, INT_MIN, 77])
        yield c
    for key in ("gvip", "grip", "sinceb", "pv", "rating", "avg"):
        for v in (0, 1, -1, INT_MAX, INT_MIN):
            c = base(r, "month")
            c["queues"][key] = [v]
            yield c
    for off in (0x21BFC, 0x21C00):
        c = base(r, "month")
        c["ints"][off] = 12345
        yield c


def month_random(r):
    return base(r, "month")


def final_edges():
    r = random.Random(1615)
    for _ in range(3):
        yield base(r, "final")


def final_random(r):
    return base(r, "final")


def cell_case(r, version=None, save=None, fail=None):
    c = base(r, "cell")
    c["raw"] = r.getrandbits(8 * (CELL_BYTES + 2 * CELL_GUARD)).to_bytes(CELL_BYTES + 2 * CELL_GUARD, "little")
    c["save"] = r.choice([0, 1, 2, -1, INT_MIN, 0x10000, 0x100]) if save is None else save
    c["version"] = r.choice([-1, 0, 1, 2, INT_MIN, INT_MAX, 0, 1]) if version is None else version
    c["unused"] = r.choice(WORDS + [r.getrandbits(32)])
    c["cellints"] = [ri(r) for _ in range(4)]
    if fail is None:
        fail = r.choice([None] + list(range(13)))
    ans = [0] * fail + [r.choice([1, -1, 5, INT_MIN, INT_MAX, 0x100])] if fail is not None else [0]
    c["queues"]["ans"] = ans
    c["queues"]["val"] = [r.choice([-1, 0, 1, 5, INT_MAX, 0xFFFFFFFF, 0x80000000, r.getrandbits(32)]) for _ in range(5)]
    return c


def cell_edges():
    r = random.Random(1616)
    for version in (-1, 0, 1, 2, INT_MIN, INT_MAX, 3, -2):
        for save in (0, 1):
            yield cell_case(r, version, save, None)
    for fail in [None] + list(range(13)):
        for save in (0, 1, 2, -1):
            for version in (0, 1):
                yield cell_case(r, version, save, fail)


def cell_random(r):
    return cell_case(r)


def balance_case(r, raw=None):
    c = dict(op="balance", queues={})
    c["raw"] = r.getrandbits(8 * (0x128 + 2 * GUARD)).to_bytes(0x128 + 2 * GUARD, "little") if raw is None else raw
    c["bal"] = ri(r)
    return c


def balance_edges():
    r = random.Random(1617)
    for v in INTS:
        c = balance_case(r)
        c["bal"] = v
        yield c


def sinit_cases():
    return [dict(op="sinit", queues={}) for _ in range(2)]


# ---------------------------------------------------------------------------------------------------------------- the companions: constructors, GetType, destructors

MAC_WRITTEN = [[(0, 4), (4, 4), (8, 4), (0xC, 4), (0x10, 2), (0x12, 2)], [(0, 4), (4, 4), (8, 2), (0xC, 4)], [(0, 4), (4, 4), (8, 4), (0xC, 4)]]
MAC_PAYLOAD = [[(8, 4), (0xC, 4), (0x10, 2), (0x12, 2)], [(8, 2), (0xC, 4)], [(8, 4), (0xC, 4)]]
WORDS32 = [0, 1, MASK, 0x7FFFFFFF, 0x80000000, 5, 0x1234, 100, 0x1A, 0x17, 0x11]
ID16 = [0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x1234]


def new_log(e):
    e.cl = CL.PpcLog({}, {})
    e.cl.deletes = 0
    e.news.clear()
    return e.cl


def op_delete(e):
    p = e.r(3)
    if p:
        e.cl.deletes += 1
        if e.cl.lab(p) > 0:
            e.cl.rec("delete", (e.cl.lab(p),))
            e.cl.forget(p)


def heap(lib):
    return list((C.c_int32 * 4).in_dll(lib, "tpw_cl_heap"))


def finish_ppc(e, **fields):
    out = dict(fields)
    out["log"] = e.cl.log
    out["deletes"] = e.cl.deletes
    out["news"] = len(e.news)
    return out


def host_begin_plain(lib):
    bind(lib)
    CL.host_begin(lib, {}, {})
    return heap(lib)


def host_finish(lib, before, **fields):
    out = dict(fields)
    now = heap(lib)
    out["log"] = CL.host_end(lib)
    out["deletes"] = now[2] - before[2]
    out["news"] = now[0] - before[0]
    return out


def ctor_args(r, kind):
    w = lambda: r.choice(WORDS32 + [r.getrandbits(32)])
    h = lambda: r.choice(ID16 + [r.getrandbits(16)])
    return [[w(), w(), h(), h()], [h(), w()], [w(), w()]][kind]


def ctor_cases(kind):
    def cases():
        r = random.Random(1620 + kind)
        return [dict(args=ctor_args(r, kind), low=r.getrandbits(16)) for _ in range(30)]
    return cases


def ppc_ctor(kind):
    def run_(e, c):
        new_log(e)
        obj = e.alloc(0x40, POISON * 0x40)
        a = c["args"]
        low = c["low"]
        if kind == 0:
            args = [obj, a[0] & MASK, a[1] & MASK, ((a[2] << 16) | low) & MASK, ((a[3] << 16) | (low ^ 0x5A5A)) & MASK]
        elif kind == 1:
            args = [obj, ((a[0] << 16) | low) & MASK, a[1] & MASK]
        else:
            args = [obj, a[0] & MASK, a[1] & MASK]
        ret = T.call_ppc(e, CTORS[kind], args)[0]
        typ = T.call_ppc(e, GETTYPES[kind], [obj])[0]
        payload = [int.from_bytes(e.read(obj + o, w), "big") for o, w in MAC_PAYLOAD[kind]]
        mask = bytearray(0x40)
        for o, w in MAC_WRITTEN[kind]:
            mask[o:o + w] = b"\1" * w
        intact = all(e.read(obj + i, 1) == POISON for i in range(0x40) if not mask[i])
        return finish_ppc(e, same=int(ret == obj), flags=e.u32(obj + 4), type=typ, payload=payload, intact=int(intact), vptr=int(e.u32(obj) == VTABLES[kind]))
    return run_


def host_ctor(kind):
    def run_(lib, c):
        before = host_begin_plain(lib)
        a = list(c["args"])
        vals = (C.c_int64 * 16)(*(a + [0] * (16 - len(a))))
        out = (C.c_int64 * 16)()
        lib.tpw_pn_ctor(kind, vals, out)
        count = [4, 2, 2][kind]
        return host_finish(lib, before, same=1, flags=out[0] & MASK, type=out[1] & MASK, payload=[v & MASK for v in out[2:2 + count]], intact=out[13], vptr=1)
    return run_


def gettype_cases(kind):
    def cases():
        r = random.Random(1630 + kind)
        return [dict(flags=f) for f in (0, 1, MASK, 0x80000000, 0xA5A5A5A5, 0x1B, 0x19, 2)] + [dict(flags=r.getrandbits(32)) for _ in range(8)]
    return cases


def ppc_gettype(kind):
    def run_(e, c):
        new_log(e)
        obj = e.alloc(0x40, POISON * 0x40)
        e.write(obj, struct.pack(">II", VTABLES[kind], c["flags"]))
        first = T.call_ppc(e, GETTYPES[kind], [obj])[0]
        second = T.call_ppc(e, GETTYPES[kind], [obj])[0]
        return finish_ppc(e, first=first, second=second, flags=e.u32(obj + 4), rest=e.read(obj + 8, 0x38), vptr=e.u32(obj))
    return run_


def host_gettype(kind):
    def run_(lib, c):
        before = host_begin_plain(lib)
        v = lib.tpw_pn_gettype(kind, c["flags"]) & MASK
        return host_finish(lib, before, first=v, second=v, flags=c["flags"], rest=POISON * 0x38, vptr=VTABLES[kind])
    return run_


def dtor_cases(kind):
    def cases():
        flags = [0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, MASK, 0x80000000, 0x12340001, 0x1234FFFF, 0x7FFF0000, 0xFFFF0001, 0xFFFF8000, 0x00010000]
        return [dict(flag=f, present=1) for f in flags] + [dict(flag=f, present=0) for f in (0, 1, 0x7FFF, 0x12340001, 0x8000, MASK)]
    return cases


def dtor_random(r):
    return dict(flag=r.choice([r.getrandbits(32), r.getrandbits(16), r.getrandbits(16) | 0x80000000, 1, 0, MASK]), present=r.choice([1, 1, 1, 0]))


def ppc_dtor(kind):
    def run_(e, c):
        new_log(e)
        obj = e.alloc(0x40, POISON * 0x40) if c["present"] else 0
        e.cl.labels[obj] = 3
        if obj:
            e.write(obj, struct.pack(">II", 0x11111111, 0x11223344))
        s = CL.Stubs()
        s.real.update(OWN)
        s.imp("__dl__FPv", op_delete)
        saved = s.activate(e)
        try:
            ret = T.call_ppc(e, DTORS[kind], [obj, c["flag"]])[0]
        finally:
            CL.Stubs.deactivate(e, saved)
        explicit = bool(obj) and s16(c["flag"]) <= 0
        return finish_ppc(e, same=int(ret == obj), vptr=int(obj == 0 or e.u32(obj) != 0x11111111), flags=e.u32(obj + 4) if explicit else 0)
    return run_


def host_dtor(kind):
    def run_(lib, c):
        before = host_begin_plain(lib)
        out = (C.c_uint32 * 2)()
        lib.tpw_pn_dtor(kind, s32(c["flag"]), c["present"], out)
        explicit = c["present"] and s16(c["flag"]) <= 0
        return host_finish(lib, before, same=1, vptr=1, flags=out[0] if explicit else 0)
    return run_


def companion_tests():
    tests = []
    for k, n in enumerate(["CMsgAddToChallenge", "CMsgResearchCompleted", "CMsgStaffInfo"]):
        tests.append(FnTest(f"{n}::{n}", ppc_ctor(k), host_ctor(k), edge=ctor_cases(k), rnd=lambda r, k=k: dict(args=ctor_args(r, k), low=r.getrandbits(16))))
        tests.append(FnTest(f"{n}::GetType", ppc_gettype(k), host_gettype(k), edge=gettype_cases(k), rnd=lambda r: dict(flags=r.getrandbits(32))))
        tests.append(FnTest(f"{n}::~{n}", ppc_dtor(k), host_dtor(k), edge=dtor_cases(k), rnd=dtor_random))
    return tests


def gen(f):
    return lambda: list(f())


TESTS = [
    Base("CParkAnalyser::ReceiveMessage", ppc, host, edge=gen(receive_edges), rnd=receive_random),
    Base("CParkAnalyser::PersonPaidAdmission", ppc, host, edge=gen(paid_edges), rnd=paid_random),
    Base("CParkAnalyser::UpdateEndOfMonth", ppc, host, edge=gen(month_edges), rnd=month_random),
    Base("CParkAnalyser::GetThreeMostDominantThoughts", ppc, host, edge=gen(thoughts_edges), rnd=thoughts_random),
    Base("CParkAnalyser::FinalDestroy", ppc, host, edge=gen(final_edges), rnd=final_random),
    Base("CParkAnalyserCell::Serialise", ppc, host, edge=gen(cell_edges), rnd=cell_random),
    Base("__sinit_analyser_cpp", ppc, host, edge=sinit_cases, rnd=lambda r: dict(op="sinit", queues={})),
    Base("CBankAccount::GetBalance", ppc, host, edge=gen(balance_edges), rnd=lambda r: balance_case(r)),
] + companion_tests()
