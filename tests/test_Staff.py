"""Unit w189, against libtpw_unit_Staff.dylib (tests/unit_Staff.cpp), main app ("SimTheme Park"):
  src/game/people/staff.cpp   CStaff::CStaff (cell, model) 0x1012482c and (cell, model, grade, costume) 0x10124908, Init 0x10124a08, FinalDestroy 0x10124b88,
                  ReceiveMessage 0x10124bd8, Serialise 0x10124ce8, UpdateTurn 0x10125208, SetState 0x101252a8, ModelState 0x101254f4, UpdateMonthly
                  0x10125598, LOADVAR / SAVEVAR of CStaff::PayGrade 0x10127e38 / 0x101280ac and of CStaff::StaffState 0x10127f70 / 0x101281fc,
                  CMsgStaffRoomEvent::~CMsgStaffRoomEvent 0x1012622c and GetType 0x10127dfc, __sinit_staff_cpp 0x10128350

A case is a staff member (0x20c bytes of seeded fill between two 0x40 byte guards, the fields the code reads set from the case), the world's tail block
(the turn at world + 0x1da70c; the world pointer is shifted back to it), the park analyser's block (the pointer the world answers is shifted back so that
the history's total, the word the monthly wages are added to, lies at the block's start), the message centre (absent, or a block), the mouse (absent, or a
block) and its two words, the file's bytes and a ride-deleted message, built twice by ONE piece of Python code: in emulator memory at the Mac's addresses
(big-endian) and in the host library's memory (little-endian). Every callee outside the 17 functions is a fake that records the same event on both sides
(tests/calllog.py), pointers into the staff member as the label 1, the file 2, the message 3, the saved-load log 5, the analyser 7, the bank account 8,
the mouse 9 and the existing message centre 6, the script and the thoughts as offsets from the staff member; the queues answer from the case. The four enum
serialise instances run real, over a fake file (LbFile_Read hands out the case's bytes, LbFile_Write records them) and a fake save-load log. The new
objects (message centre, mouse, mouse mode, pick-up mode) are checked through the nothrow operator new (the Mac's sizes), the constructor fakes and the
label of the global. __sinit_staff_cpp: the two words are written with 0xDEADBEEF and 0xCAFEF00D on the Mac, the function runs, and they are compared
with the host's constants.

What is compared: the answer, the event log, the staff member's fields (decoded) and every other byte of the block, the tail block, the analyser block, the
global message centre and mouse and the mouse's two words. The time hired is compared as one 8 byte number. Not tested: a NULL this, a NULL world.
"""
import ctypes as C
import itertools
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_Staff"
MASK = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF

(CTOR1, CTOR2, INIT, FINAL, RECV, SER, UPDATE, SETSTATE, MODEL, MONTHLY, LOADPAY, LOADSTATE, SAVEPAY, SAVESTATE, MSGDTOR, MSGTYPE, SINIT) = (
    0x1012482C, 0x10124908, 0x10124A08, 0x10124B88, 0x10124BD8, 0x10124CE8, 0x10125208, 0x101252A8, 0x101254F4, 0x10125598, 0x10127E38, 0x10127F70,
    0x101280AC, 0x101281FC, 0x1012622C, 0x10127DFC, 0x10128350)
ENTRY = dict(ctor1=CTOR1, ctor2=CTOR2, init=INIT, final=FINAL, recv=RECV, ser=SER, update=UPDATE, setstate=SETSTATE, model=MODEL, monthly=MONTHLY,
             msgdtor=MSGDTOR, msgtype=MSGTYPE, sinit=SINIT)
ENUM_ENTRY = [LOADPAY, LOADSTATE, SAVEPAY, SAVESTATE]
SIZES = {CTOR1: 120, CTOR2: 136, INIT: 348, FINAL: 36, RECV: 220, SER: 1268, UPDATE: 120, SETSTATE: 528, MODEL: 124, MONTHLY: 156, LOADPAY: 236,
         LOADSTATE: 236, SAVEPAY: 260, SAVESTATE: 260, MSGDTOR: 76, MSGTYPE: 8, SINIT: 28}
OWN = set(SIZES)

ADDR = dict(sinit_low=0x1034C4DC, sinit_high=0x1034C4D8)
G_WORLD, G_CENTRE, G_MOUSE, G_W1E0, G_W1E4, GSAVELOAD = 0x1037EC0C, 0x1037F6C4, 0x102DC1F4, 0x102DC1E0, 0x102DC1E4, 0x1037FEFC
BANK = 0x5000
WRITELOG = 0x1015B3DC
WIDE_CTOR_TV, WIDE_DTOR_TV, CENTRE_SIZE = 0x102657E8, 0x10267288, 0x1D0
TOTAL_AT = 0x1F7F0                              # the history's total in the analyser (0x1f100 + 3 * 0x250)
HOST_LAYOUT = [0x20C, 0x1DA70C, CENTRE_SIZE, 0x2A0]

GUARD = 0x40
OBJ = 0x20C
OBJ_BLOCK = GUARD + OBJ + GUARD
TAIL = 0x40
TAIL0 = 0x1DA70C
WIN = 0x40
L_OBJ, L_FILE, L_MSG, L_SAVELOAD, L_CENTRE, L_ANALYSER, L_BANK, L_MOUSE = 1, 2, 3, 5, 6, 7, 8, 9


def s32(v):
    return T.s32(v)


def put_fmt(order, size):
    return order + {1: "B", 2: "H", 4: "I", 8: "Q"}[size]


# ---- the staff member's fields the code reads or writes (offset, size, key); everything else must keep its fill
CASE_FIELDS = ((0x1C, 4, "f1c"), (0x20, 4, "spidx"), (0x24, 4, "spset"), (0xC0, 2, "walk"), (0xC2, 2, "bonus"), (0x19C, 4, "state"), (0x1E4, 4, "grade"),
               (0x1E8, 1, "pct"), (0x1EC, 8, "hired"), (0x1F4, 4, "happy"), (0x1F8, 4, "energy"), (0x1FC, 4, "sturn"), (0x200, 4, "jobs"), (0x204, 2, "rest"),
               (0x206, 2, "bl"), (0x208, 2, "tr"))
OBJ_FIELDS = [(off, n) for off, n, k in CASE_FIELDS] + [(0x1A0 + 2 * i, 2) for i in range(0x21)]

# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
R = CL.recorder

FAKES = [
    (0x10112E18, R("CPeep::CPeep", "Lwi")),
    (0x101130AC, R("CPeep::FinalDestroy", "L")),
    (0x10113138, R("CPeep::Serialise", "LLiiu", ret="i")),
    (0x101158C8, R("CPeep::UpdateTurn", "L")),
    (0x1011524C, R("CPeep::SetStandScript", "L")),
    (0x10126FC8, R("CStaff::GetSalary", "L", ret="i")),
    (0x10127764, R("CStaff::StopResting", "L")),
    (0x10127A24, R("CStaff::CancelPatrolArea", "L")),
    (0x10126444, R("CStaff::ProcessWalkingToStrike", "L")),
    (0x10126504, R("CStaff::ProcessStriking", "L")),
    (0x10126688, R("CStaff::ProcessBeingStunned", "L")),
    (0x1013EBC8, R("CWorld::GetParkAnalyser", "x", ret=lambda e, L: L.analyser)),
    (0x1013EDFC, R("CWorld::GetBankAccount", "x", ret=lambda e, L: BANK)),
    (0x100F33B0, R("CBankAccount::MakeWithdrawal", "Lu")),
    (0x100E4750, R("CParkAnalyser::HiredSomeone", "L")),
    (0x1007F46C, R("CMMPickUpStaff.ctor", "Nib")),
    (0x1007EA78, R("CMMDefault.ctor", "N")),
    (0x1007E2E0, R("AssignMouseMode", "LL")),
    (WRITELOG, R("CSaveLoad::WriteSerialiseLog", "Lisi")),
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


def stub_offset(name, with_arg=False):
    """The thoughts and the script: the object as an offset from the staff member (and r4 for the script)."""
    def stub(e):
        L = e.cl
        ints = [s32(e.r(3) - L.base)]
        if with_arg:
            ints.append(s32(e.r(4)))
        L.rec(name, ints)
    return stub


def stub_funny(e):
    L = e.cl
    world = e.u32(G_WORLD)
    flags = [int(e.r(i) != 0) for i in range(5, 11)]
    flags += [int(e.u32(e.r(1) + o) != 0) for o in (0x38, 0x3C)]
    L.rec("GetFunnyTime", [s32(e.r(4) - world)] + flags)
    e.write(e.r(3), (L.ret("funny", 0) & M64).to_bytes(8, "big"))


def stub_var(name):
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), s32(e.r(4) - L.base), s32(e.r(6))), (e.cstr(e.r(5))[:159],))
        e.set_r(3, L.ret("vans", 0) & MASK)
    return stub


def stub_load_float(e):
    L = e.cl
    L.rec("LOADVAR<float>", (L.lab(e.r(3)), s32(e.r(6))), (e.cstr(e.r(5))[:159],))
    answer = L.ret("vans", 0) & MASK
    if answer == 0:
        e.write(e.r(4), (L.ret("pvload", 0) & MASK).to_bytes(4, "big"))
    e.set_r(3, answer)


def stub_save_float(e):
    L = e.cl
    L.rec("SAVEVAR<float>", (L.lab(e.r(3)), s32(e.r(6)), e.u32(e.r(4))), (e.cstr(e.r(5))[:159],))
    e.set_r(3, L.ret("vans", 0) & MASK)


def stub_load_stamp(e):
    L = e.cl
    L.rec("LOADVARSTRUCT<TbTimeStamp>", (L.lab(e.r(3)), s32(e.r(4) - L.base), s32(e.r(6))), (e.cstr(e.r(5))[:159],))
    answer = L.ret("vans", 0) & MASK
    if answer == 0:
        e.write(e.r(4), (L.ret("stampload", 0) & M64).to_bytes(8, "big"))
    e.set_r(3, answer)


def count_ints(text):
    return text.count("%d")


def stub_assert(e):
    text = e.cstr(e.r(4))[:159]
    ints = [s32(e.r(3))] + [e.r(5 + i) & MASK for i in range(count_ints(text))]
    e.cl.rec("GAME_ASSERTMSG", tuple(ints), (text,))


def stub_trace(e):
    e.cl.rec("TRACE", (), (e.cstr(e.r(3))[:159],))


def stub_subscribe(e):
    L = e.cl
    L.rec("Subscribe", (L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5))))
    e.set_r(3, 1)


def lb_read(e):
    # r3 the result record, r4 the file, r5 the buffer, r6 the length, r7 the count
    L, buf, length = e.cl, e.r(5), e.r(6)
    L.rec("LbFile_Read", (L.lab(e.r(4)), length))
    data = e.stf_file[e.stf_cur[0]:]
    taken = min(len(data), length)
    count = taken
    forced = L.ret("LbFile_Read:xfer", -1)
    if forced >= 0:
        count = forced
    e.write(e.r(7), (count & MASK).to_bytes(4, "big"))
    result = L.ret("LbFile_Read")
    if count == length and result >= 0:
        e.write(buf, (data[:taken] + bytes(length))[:length])
    e.stf_cur[0] += taken
    e.write(e.r(3), (result & MASK).to_bytes(4, "big"))


def lb_write(e):
    L, length = e.cl, e.r(6)
    L.rec("LbFile_Write", (L.lab(e.r(4)), length), (e.read(e.r(5), length).hex()[:150] if length else "",))
    forced = L.ret("LbFile_Write:xfer", -1)
    e.write(e.r(7), ((forced if forced >= 0 else length) & MASK).to_bytes(4, "big"))
    e.write(e.r(3), (L.ret("LbFile_Write") & MASK).to_bytes(4, "big"))


STUBS.internal(0x1012E1F8, hidden_thing_id("CThing::GetThingId"))
STUBS.internal(0x10127658, hidden_thing_id("CStaff::GoToRestArea"))
STUBS.internal(0x1012F058, stub_offset("CThoughts::ProcessThoughtBubbles"))
STUBS.internal(0x1012F35C, stub_offset("CThoughts::RemoveThoughtBubble"))
STUBS.internal(0x100FAA74, stub_offset("CDelayedScript::SetSpriteScript", True))
STUBS.internal(0x100FAC4C, stub_offset("CDelayedScript::KillSpriteScript"))
STUBS.internal(0x101127D0, stub_funny)
STUBS.internal(0x10152388, stub_subscribe)
STUBS.internal(0x100F16B4, stub_load_float)
STUBS.internal(0x100F18D8, stub_save_float)
STUBS.internal(0x100F1468, stub_load_stamp)
for _addr, _name in ((0x100F44A4, "LOADVAR<unsigned int>"), (0x100F45BC, "SAVEVAR<unsigned int>"), (0x100F17C8, "LOADVAR<unsigned char>"),
                     (0x100F1A04, "SAVEVAR<unsigned char>"), (0x10111844, "LOADVAR<wchar_t>"), (0x10111A98, "SAVEVAR<wchar_t>"),
                     (0x1000F02C, "LOADVAR<unsigned long>"), (0x1000F5DC, "SAVEVAR<unsigned long>"), (0x100F53CC, "LOADVARSTRUCT<MapId>"),
                     (0x100F58C0, "SAVEVARSTRUCT<MapId>"), (0x1000EB4C, "LOADVARSTRUCT<ThingId>"), (0x1000E9D4, "SAVEVARSTRUCT<ThingId>"),
                     (0x100F1B1C, "SAVEVARSTRUCT<TbTimeStamp>")):
    STUBS.internal(_addr, stub_var(_name))
STUBS.internal(0x1000DDE8, stub_assert)
STUBS.internal(0x1006FCFC, stub_trace)
STUBS.imp("LbFile_Read__FPvPvUlPUl", lb_read)
STUBS.imp("LbFile_Write__FPvPCvUlPUl", lb_write)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)

    def get_type(emu):
        emu.cl.rec("GetType", (emu.cl.lab(emu.r(3)),))
        emu.set_r(3, T.u32(emu.cl.ret("GetType", 0)))

    # The message's vtable: slot 2 (+8) is GetType, called through the glue with a TVector like every virtual.
    tvec = T.fake_function(e, get_type)
    e.stf_vtable = T.persist_alloc(e, 0x20)
    e.write(e.stf_vtable + 8, struct.pack(">I", tvec))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def put(self, a, size, v):
        self.write(a, struct.pack(put_fmt(self.order, size), v & ((1 << (8 * size)) - 1)))

    def get(self, a, size):
        return struct.unpack(put_fmt(self.order, size), self.read(a, size))[0]


class Ppc(Side):
    order = ">"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({GSAVELOAD: L_SAVELOAD, BANK: L_BANK}, case["queues"])
        e.stf_file, e.stf_cur = case["file"], [0]
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

        def construct(emu, call):
            base, ctor, dtor, size, count = emu.r(3), emu.r(4), emu.r(5), emu.r(6), emu.r(7)
            if ctor == WIDE_CTOR_TV:
                emu.cl.rec("CMessageCentre", [emu.cl.newlab(base), int(ctor == WIDE_CTOR_TV), int(dtor == WIDE_DTOR_TV), size, count])
                return
            for i in range(count):
                call(ctor, base + i * size, 1)

        def op_delete(emu):
            emu.cl.rec("delete", (emu.cl.lab(emu.r(3)),))

        STUBS.imp("__nw__FUl", op_new)
        STUBS.imp("__dl__FPv", op_delete)
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

    def setup(self, eb, tail, window, centre, mouse, w1, w2, file):
        self.L.base = eb
        self.world = (tail - TAIL0) & MASK
        self.L.analyser = (window - TOTAL_AT) & MASK
        self.put(G_WORLD, 4, self.world)
        self.put(G_CENTRE, 4, centre)
        self.put(G_MOUSE, 4, mouse)
        self.put(G_W1E0, 4, w1)
        self.put(G_W1E4, 4, w2)
        self.label(self.L.analyser, L_ANALYSER)

    def message(self, ident):
        m = self.alloc(16)
        self.write(m, struct.pack(">IIHH", self.e.stf_vtable, 0x5A5A5A5A, ident, 0xA5A5))
        return m

    def event(self, thing, arriving):
        m = self.alloc(16)
        self.write(m, struct.pack(">IIHHI", self.e.stf_vtable, 0, thing, 0, arriving))
        return m

    def centre(self):
        word = self.get(G_CENTRE, 4)
        label = self.L.lab(word)
        return label if label >= 0 else (1 << 33) | word

    def mouse(self):
        word = self.get(G_MOUSE, 4)
        label = self.L.lab(word)
        return label if label >= 0 else (1 << 33) | word

    def mousewords(self):
        return [s32(self.get(G_W1E0, 4)), s32(self.get(G_W1E4, 4))]

    def call(self, entry, args):
        return T.call_ppc(self.e, entry, [a & MASK for a in args], max_insns=20_000_000)[0] & MASK


class Host(Side):
    order = "<"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        self.label(lib.tpw_stf_addr(b"gSaveLoad"), L_SAVELOAD)
        self.label(BANK, L_BANK)

    def done(self):
        pass

    def g(self, name):
        return None

    def alloc(self, n):
        return self.lib.tpw_stf_alloc(n)

    def label(self, address, n):
        self.lib.tpw_cl_label(n, address)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setup(self, eb, tail, window, centre, mouse, w1, w2, file):
        buf = (C.c_uint8 * max(len(file), 1))(*file)
        self.lib.tpw_stf_setup(eb, tail, window, centre, mouse, s32(w1), s32(w2), buf, len(file))
        self.world = tail
        self.label(self.lib.tpw_stf_analyser(), L_ANALYSER)

    def message(self, ident):
        return self.lib.tpw_stf_makemsg(ident)

    def event(self, thing, arriving):
        return self.lib.tpw_stf_makeevent(thing, s32(arriving))

    def centre(self):
        return self.lib.tpw_stf_centre()

    def mouse(self):
        return self.lib.tpw_stf_mouse()

    def mousewords(self):
        out = (C.c_int32 * 2)()
        self.lib.tpw_stf_mousewords(out)
        return list(out)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_stf_"
    u64, i32, u32, i64 = C.c_uint64, C.c_int32, C.c_uint32, C.c_int64
    for name, res, args in (
        ("addr", u64, [C.c_char_p]), ("alloc", u64, [u32]), ("makemsg", u64, [u32]), ("makeevent", u64, [u32, i32]), ("eventfields", None, [u64, C.POINTER(i64)]),
        ("setup", None, [u64, u64, u64, u64, u64, i32, i32, C.POINTER(C.c_uint8), u32]), ("analyser", u64, []), ("centre", i64, []), ("mouse", i64, []),
        ("mousewords", None, [C.POINTER(i32)]), ("sinit", None, [C.POINTER(u32)]),
        ("ctor1", u64, [u64, u32, i32]), ("ctor2", u64, [u64, u32, i32, i32, u32]), ("init", None, [u64]), ("final", None, [u64]), ("receive", None, [u64, u64]),
        ("serialise", i32, [u64, u64, i32, i32, u32]), ("update", None, [u64]), ("setstate", None, [u64, i32]), ("model", None, [u64]), ("monthly", None, [u64]),
        ("enumvar", i32, [i32, u64, u64, C.c_char_p, i32]), ("msgdtor", None, [u64, i32]), ("msgtype", i32, [u64]),
    ):
        fn = getattr(lib, p + name)
        fn.restype, fn.argtypes = res, args
    lib.tpw_stf_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_stf_layout(out)
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
    window = s.alloc(WIN)
    s.write(window, fill(c["seed"] + 2, WIN))
    s.put(window, 4, c["mtemp"])
    file = s.alloc(16)
    centre = s.alloc(CENTRE_SIZE) if c["centre"] else 0
    mouse = s.alloc(16) if c["mouse"] else 0
    s.label(eb, L_OBJ)
    s.label(file, L_FILE)
    if centre:
        s.label(centre, L_CENTRE)
    if mouse:
        s.label(mouse, L_MOUSE)
    s.setup(eb, tail, window, centre, mouse, c["w1"], c["w2"], c["file"])
    if c["op"] in ("msgdtor", "msgtype"):
        msg = s.event(c["thing"], c["arriving"])
    else:
        msg = s.message(c["mid"])
    s.label(msg, L_MSG)
    s.msg = msg
    s.file = file
    s.put(tail, 4, c["turn"])
    s.tail = tail
    s.window = window
    for off, size, key in CASE_FIELDS:
        s.put(eb + off, size, c[key])


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
        if op == "enumvar":
            name = s.alloc(64)
            s.write(name, c["fname"].encode() + b"\0")
            value = eb + (0x1E4 if c["kind"] in (0, 2) else 0x19C)
            return s32(s.call(ENUM_ENTRY[c["kind"]], [s.file, value, name, c["nolog"]]))
        entry = ENTRY[op]
        if op == "ctor1":
            return int(s.call(entry, [eb, c["cell"] << 16, c["model"]]) == eb)
        if op == "ctor2":
            return int(s.call(entry, [eb, c["cell"] << 16, c["model"], c["agrade"], c["costume"]]) == eb)
        if op == "sinit":
            s.put(ADDR["sinit_low"], 4, 0xDEADBEEF)
            s.put(ADDR["sinit_high"], 4, 0xCAFEF00D)
            s.call(entry, [])
            return [s.get(ADDR["sinit_low"], 4), s.get(ADDR["sinit_high"], 4)]
        if op == "recv":
            s.call(entry, [eb, s.msg])
        elif op == "ser":
            return s32(s.call(entry, [eb, s.file, c["save"], c["version"], c["unused"]]))
        elif op == "setstate":
            s.call(entry, [eb, c["astate"]])
        elif op == "msgdtor":
            s.call(entry, [s.msg, c["flag"]])
        elif op == "msgtype":
            return s32(s.call(entry, [s.msg]))
        else:
            s.call(entry, [eb])
        return 0
    lib = s.lib
    if op == "enumvar":
        value = eb + (0x1E4 if c["kind"] in (0, 2) else 0x19C)
        return lib.tpw_stf_enumvar(c["kind"], s.file, value, c["fname"].encode(), s32(c["nolog"]))
    if op == "ctor1":
        return int(lib.tpw_stf_ctor1(eb, c["cell"], s32(c["model"])) == eb)
    if op == "ctor2":
        return int(lib.tpw_stf_ctor2(eb, c["cell"], s32(c["model"]), s32(c["agrade"]), c["costume"] & MASK) == eb)
    if op == "sinit":
        out = (C.c_uint32 * 2)()
        lib.tpw_stf_sinit(out)
        return list(out)
    if op == "recv":
        lib.tpw_stf_receive(eb, s.msg)
    elif op == "ser":
        return lib.tpw_stf_serialise(eb, s.file, s32(c["save"]), s32(c["version"]), c["unused"])
    elif op == "setstate":
        lib.tpw_stf_setstate(eb, s32(c["astate"]))
    elif op == "msgdtor":
        lib.tpw_stf_msgdtor(s.msg, s32(c["flag"]))
    elif op == "msgtype":
        return lib.tpw_stf_msgtype(s.msg)
    else:
        getattr(lib, "tpw_stf_" + op)(eb)
    return 0


def observe(s, c, answer):
    out = {"answer": answer, "log": s.log(), "centre": s.centre(), "mouse": s.mouse(), "words": s.mousewords()}
    out["obj"] = snapshot(s, s.eb - GUARD, OBJ_BLOCK, OBJ_FIELDS)
    out["tail"] = snapshot(s, s.tail, TAIL, [(0, 4)], 0)
    out["window"] = snapshot(s, s.window, WIN, [(0, 4)], 0)
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
STATES = [0xFFFFFFFF, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x7FFFFFFF, 0x80000000]
GRADES = [0, 1, 2, 3, 4, 5, MASK, 0x80000000, 0x7FFFFFFF]
FLOATS = [0x00000000, 0x80000000, 0x3F000000, 0xBF000000, 0x42C80000, 0x42C7999A, 0x437F0000, 0x437FE666, 0x43800000, 0x43960000, 0xBF800000, 0x501502F9,
          0x7FC00000, 0x7F800000, 0xFF800000, 0x40490FDB, 0x3F800000, 0x42FE0000, 0x42FF0000]
IDS = [0, 1, 2, 3, 4, 7, 0x2FF, 0xFFFF, 0xFFFE, 0x8000]
SALARIES = [0, 1, 2, 100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, MASK]
# fctiwz of a value outside the int range (3.2e10, inf, NaN) is saturated by the host; the emulator keeps other bits, so the saved happiness
# and energy stay inside the range (the 255 clamp is the clrlwi on the low byte, not a saturation)
SAFE_FLOATS = [f for f in FLOATS if f not in (0x501502F9, 0x7FC00000, 0x7F800000, 0xFF800000)]
STAMPS = [0, 1, 0x100000000, 0x123456789ABCDEF, 0x7FFFFFFF, 0x80000000, 0x7FFFFFFFFFFFFFFF, -1, -0x100000000]
FILES = [b"", bytes(range(1, 9)), bytes(range(1, 4)), bytes([0x78, 0x56, 0x34, 0x12, 0xFF, 0xFF, 0xFF, 0xFF]), bytes(8), bytes([0xFF] * 8), bytes(range(0x80, 0x90))]


def u32(r):
    return r.choice(U32 + [r.getrandbits(32), r.getrandbits(16), r.getrandbits(8)])


def qlist(r, values):
    return [r.choice(values) for _ in range(r.randrange(1, 4))]


def base(r, op):
    rest = r.choice([0, 1, 0x2FF, 0xFFFF, r.getrandbits(16), 5])
    c = {"op": op, "seed": r.getrandbits(32), "cell": r.choice([0, 1, 2, 0x80, 0x81, 0x4000, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16)]),
         "model": r.choice([0, 1, 5, 7, 8, MASK, u32(r)]), "agrade": r.choice(GRADES), "costume": r.choice([0, 1, 7, 0xFF, 0x1FF]),
         "f1c": u32(r), "spidx": r.choice([0, 1, 5, 0xFF, 0x1FF, MASK, 0x80000000, u32(r)]), "spset": r.choice([0, 1, 5, 9, MASK, u32(r)]),
         "walk": r.getrandbits(16), "bonus": r.getrandbits(16), "state": r.choice(STATES), "grade": r.choice(GRADES), "pct": r.choice([0, 1, 50, 0xFF, r.getrandbits(8)]),
         "hired": r.choice(STAMPS) & M64, "happy": r.choice(SAFE_FLOATS), "energy": r.choice(SAFE_FLOATS), "sturn": u32(r),
         "jobs": u32(r), "rest": rest, "bl": r.getrandbits(16), "tr": r.getrandbits(16), "turn": u32(r), "mtemp": u32(r),
         "astate": r.choice(STATES), "centre": r.random() < 0.5, "mouse": r.random() < 0.5, "w1": u32(r), "w2": u32(r),
         "save": r.choice([0, 1, 2, MASK, 0x80000000]), "version": r.choice([MASK, 0, 1, 2, 3, 0x7FFFFFFF, 0x80000000, 5]), "unused": u32(r),
         "mtype": r.choice([10, 10, 12, 12, 9, 11, 13, 15, 0, MASK]), "mid": rest, "file": r.choice(FILES), "kind": r.randrange(4),
         "fname": r.choice(["mState", "mCurrentPayGrade", "x"]), "nolog": r.choice([0, 0, 1, 2, MASK]), "flag": r.choice([0, 1, MASK, 0xFFFF, 0x8000, 0x7FFF, 0x10001, 0x18000, 5]),
         "thing": r.getrandbits(16), "arriving": r.choice([0, 1, MASK, u32(r)])}
    c["mid"] = r.choice([rest, rest, r.getrandbits(16), 0])
    c["queues"] = {
        "CPeep::Serialise": qlist(r, [0, 0, 1, MASK]), "vans": qlist(r, [0, 0, 1, MASK, 7]), "pvload": qlist(r, FLOATS + [r.getrandbits(32)]),
        "stampload": qlist(r, STAMPS), "funny": qlist(r, STAMPS), "CStaff::GetSalary": qlist(r, SALARIES), "CStaff::GoToRestArea": qlist(r, IDS),
        "GetType": [c["mtype"]], "newnull": [r.choice([0, 0, 0, 1])] if r.random() < 0.5 else qlist(r, [0, 0, 1]), "CThing::GetThingId": [r.choice(IDS)],
        "LbFile_Read": [r.choice([0, 0, 0, -1, 1])], "LbFile_Read:xfer": [r.choice([-1, -1, -1, 0, 3, 4, 5])], "LbFile_Write": [r.choice([0, 0, 0, -1, 1])],
        "LbFile_Write:xfer": [r.choice([-1, -1, -1, 0, 3, 4, 5])],
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


def update_tweak(r, c):
    if r.random() < 0.7:
        c["queues"]["CThing::GetThingId"] = [(c["turn"] & 3) + 4 * r.randrange(0, 0x3FFF) if r.random() < 0.5 else r.choice(IDS)]


def setstate_tweak(r, c):
    c["astate"] = r.choice(STATES + [0, 7, 7])
    if r.random() < 0.4:
        c["state"] = 1


def model_tweak(r, c):
    c["state"] = r.choice(STATES + [4, 5, 6, 7])


def recv_tweak(r, c):
    c["state"] = r.choice([1, 2, 3, 2, 3, 0, 4])
    c["mtype"] = r.choice([10, 10, 10, 12, 9, 11])
    c["queues"]["GetType"] = [c["mtype"]]


def ser_tweak(r, c):
    c["save"] = r.choice([0, 1])
    c["version"] = r.choice([0, 1, 0, 1, 2, MASK])
    fail = r.randrange(0, 50)
    c["queues"]["vans"] = [0] * fail + [r.choice([1, MASK, 7])]
    c["queues"]["CPeep::Serialise"] = [0]


def ser_edges():
    r = random.Random(15)
    for save in (0, 1):
        for version in (MASK, 0, 1, 2, 0x7FFFFFFF, 0x80000000):
            for peep in (0, 1, MASK):
                c = base(r, "ser")
                c["save"], c["version"] = save, version
                c["queues"]["CPeep::Serialise"] = [peep]
                c["queues"]["vans"] = [0]
                c["file"] = bytes(range(1, 9))
                c["queues"]["LbFile_Read"], c["queues"]["LbFile_Write"] = [0], [0]
                c["queues"]["LbFile_Read:xfer"], c["queues"]["LbFile_Write:xfer"] = [-1], [-1]
                yield c
    for save in (0, 1):
        for version in (0, 1):
            for fail in range(0, 44):
                for answer in (1, MASK):
                    c = base(r, "ser")
                    c["save"], c["version"] = save, version
                    c["queues"]["CPeep::Serialise"] = [0]
                    c["queues"]["vans"] = [0] * fail + [answer]
                    c["file"] = bytes(range(1, 9))
                    c["queues"]["LbFile_Read"], c["queues"]["LbFile_Write"] = [0], [0]
                    c["queues"]["LbFile_Read:xfer"], c["queues"]["LbFile_Write:xfer"] = [-1], [-1]
                    yield c
    for save in (0, 1):
        for reads in ([-1], [0, -1], [1], [0, 1], [0, 0]):
            for xfer in ([-1], [3], [0, 4], [4, 3], [4, 4, 4]):
                c = base(r, "ser")
                c["save"], c["version"] = save, 1
                c["queues"]["CPeep::Serialise"] = [0]
                c["queues"]["vans"] = [0]
                c["file"] = bytes(range(0x11, 0x21))
                key = "LbFile_Write" if save else "LbFile_Read"
                c["queues"][key] = reads
                c["queues"][key + ":xfer"] = xfer
                yield c
    for save in (0, 1):
        for happy in SAFE_FLOATS:
            for energy in (0x42C80000, 0x437FE666, 0xBF800000):
                c = base(r, "ser")
                c["save"], c["version"] = save, 1
                c["queues"]["CPeep::Serialise"] = [0]
                c["queues"]["vans"] = [0]
                c["happy"], c["energy"] = happy, energy
                c["queues"]["pvload"] = [happy, energy]
                c["file"] = bytes(range(1, 9))
                c["queues"]["LbFile_Read"], c["queues"]["LbFile_Write"] = [0], [0]
                c["queues"]["LbFile_Read:xfer"], c["queues"]["LbFile_Write:xfer"] = [-1], [-1]
                yield c


def enum_edges():
    r = random.Random(41)
    for kind in range(4):
        for fname in ("mState", "mCurrentPayGrade"):
            for nolog in (0, 1, 5, MASK):
                for result, xfer in (([0], [-1]), ([-1], [-1]), ([1], [-1]), ([0], [0]), ([0], [3]), ([0], [5]), ([0], [4]), ([-2], [4])):
                    c = base(r, "enumvar")
                    c["kind"], c["fname"], c["nolog"] = kind, fname, nolog
                    c["queues"]["LbFile_Read"] = c["queues"]["LbFile_Write"] = result
                    c["queues"]["LbFile_Read:xfer"] = c["queues"]["LbFile_Write:xfer"] = xfer
                    c["file"] = r.choice(FILES[1:])
                    yield c


def sinit_edges():
    yield base(random.Random(16), "sinit")


def event_ppc(e, c):
    return {"fields": [15, 0, c["thing"] & 0xFFFF, c["arriving"] & MASK]}


def event_host(lib, c):
    bind(lib)
    m = lib.tpw_stf_makeevent(c["thing"], s32(c["arriving"]))
    out = (C.c_int64 * 4)()
    lib.tpw_stf_eventfields(m, out)
    return {"fields": [out[0], out[1], out[2], out[3] & MASK]}


def event_edges():
    r = random.Random(50)
    for thing in (0, 1, 0xFFFF, 0x2FF):
        for arriving in (0, 1, MASK, 0x80000000):
            c = base(r, "event")
            c["thing"], c["arriving"] = thing, arriving
            yield c


TESTS = [
    FnTest("CStaff::CStaff 0x1012482c", ppc, host, edge=grid(11, "ctor1", {"f:centre": [False, True], "newnull": [0, 1], "f:cell": [0, 1, 0x7FFF, 0xFFFF],
                                                                        "f:model": [0, 5, MASK]}), rnd=make_random("ctor1")),
    FnTest("CStaff::CStaff 0x10124908", ppc, host, edge=grid(12, "ctor2", {"f:centre": [False, True], "newnull": [0, 1], "f:agrade": GRADES,
                                                                        "f:costume": [0, 0x1FF]}), rnd=make_random("ctor2")),
    FnTest("CStaff::Init", ppc, host, edge=grid(19, "init", {"f:centre": [False, True], "newnull": [0, 1], "f:grade": GRADES,
                                                           "funny": [0, 1, 0x100000000, -1, 0x7FFFFFFFFFFFFFFF]}), rnd=make_random("init")),
    FnTest("CStaff::FinalDestroy", ppc, host, edge=grid(17, "final", {"f:state": [0, 3]}), rnd=make_random("final")),
    FnTest("CStaff::ReceiveMessage", ppc, host, edge=grid(20, "recv", {"f:mtype": [10, 9, 11, 12, 13, 15, 0, MASK], "f:state": [0, 1, 2, 3, 4],
                                                                    "f:mid": [0, 5, 0xFFFF], "f:rest": [0, 5, 0xFFFF]},
                                                         lambda r, c: c["queues"].update({"GetType": [c["mtype"]]})), rnd=make_random("recv", recv_tweak)),
    FnTest("CStaff::Serialise", ppc, host, edge=ser_edges, rnd=make_random("ser", ser_tweak)),
    FnTest("CStaff::UpdateTurn", ppc, host, edge=grid(21, "update", {"f:turn": [0, 1, 2, 3, 4, 7, 0xFFFFFFFF, 0x80000000],
                                                                    "CThing::GetThingId": [0, 1, 2, 3, 4, 5, 7, 0xFFFF, 0xFFFC]}), rnd=make_random("update", update_tweak)),
    FnTest("CStaff::SetState", ppc, host, edge=grid(13, "setstate", {"f:astate": STATES, "f:state": [0, 1, 2], "f:mouse": [False, True],
                                                                   "newnull": [[0], [1], [0, 1], [0, 0, 1], [0, 1, 0], [1, 0, 0], [0, 0, 0]]}),
           rnd=make_random("setstate", setstate_tweak)),
    FnTest("CStaff::ModelState", ppc, host, edge=grid(22, "model", {"f:state": STATES + [4, 5, 6, 7, 8]}), rnd=make_random("model", model_tweak)),
    FnTest("CStaff::UpdateMonthly", ppc, host, edge=grid(23, "monthly", {"CStaff::GetSalary": [[0], [1], [0x80000000], [MASK], [5, 7], [MASK, 1], [0x7FFFFFFF, 0x7FFFFFFF]],
                                                                       "f:mtemp": [0, 1, 0x7FFFFFFF, 0x80000000, MASK]}), rnd=make_random("monthly")),
    FnTest("CStaff::PayGrade LOADVAR / SAVEVAR, CStaff::StaffState LOADVAR / SAVEVAR", ppc, host, edge=enum_edges, rnd=make_random("enumvar")),
    FnTest("CMsgStaffRoomEvent::~CMsgStaffRoomEvent", ppc, host, edge=grid(24, "msgdtor", {"f:flag": [0, 1, MASK, 0xFFFF, 0x8000, 0x7FFF, 0x10001, 0x18000, 5]}),
           rnd=make_random("msgdtor")),
    FnTest("CMsgStaffRoomEvent::GetType", ppc, host, edge=grid(25, "msgtype", {"f:thing": [0, 1]}), rnd=make_random("msgtype")),
    FnTest("CMsgStaffRoomEvent inline constructor (host only)", event_ppc, event_host, edge=event_edges, rnd=lambda r: dict(base(r, "event"))),
    FnTest("__sinit_staff_cpp", ppc, host, edge=sinit_edges, rnd=lambda r: base(r, "sinit")),
]
