"""Unit w129 (main app), against libtpw_unit_MacroAI.dylib (tests/unit_MacroAI.cpp):

  src/MacroAI.cpp            CMacroAI::Init (0x1010096c) and Update (0x101009dc)
  src/Arrival_update.cpp     CArrival::Init (0x100f22c4), Update (0x100f22ec), UpdateArrivalRate (0x100f2600)
  src/ParkTime_update.cpp    CParkTime::Init (0x10112694), Update (0x10112214), GetFunnyTimeSinceSessionStart (0x10112b14)
  src/StaffDB_update.cpp     CStaffDB::Init (0x10129298), Update (0x1012a10c)
  src/GoldenTicket_check.cpp CGoldenTicketControl::CheckForTickets (0x100fbd2c)
  src/Game_iterate.cpp       CGame::Iterate (0x10149838)
  src/ControlManager_setup.cpp CControlManager::SetupInitialControls (0x100fa2ac)
  src/WorldSavvy.cpp         CWorldSavvy::GetWorld (0x1014205c)
  src/ParkAnalyser_gates.cpp CParkAnalyser::GatesOpen (0x100e7028)
  src/DateMessages.cpp       the constructor, GetType and destructor of CMsgEndOfDay, CMsgEndOfMonth and CMsgEndOfYear

They run as the original on the PowerPC side (the signed 64 bit divide helper 0x10237e1c and CTimeSig's two inline members too) and as src/ on the host.
Every other callee is a fake that records the same event on both sides (see unit_MacroAI.cpp).

The world is the head block (staff database, park time, golden ticket control, arrivals) plus two tail blocks (the turn); a world pointer is a tail's
address minus 0x1da6f0 on both sides, and gWorld and gGameWorld can name different tails. Memory the code owns is built from VALUES (a random fill with the
fields the code reads or writes packed in each side's byte order), compared by value, and every other byte must still hold its fill.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_MacroAI"

MASK = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF
POISON = b"\xa5"

A = dict(macro_init=0x1010096C, macro_update=0x101009DC, arr_init=0x100F22C4, arr_update=0x100F22EC, arr_rate=0x100F2600, pt_init=0x10112694,
         pt_update=0x10112214, pt_since=0x10112B14, st_init=0x10129298, st_update=0x1012A10C, tickets=0x100FBD2C, iterate=0x10149838,
         setup=0x100FA2AC, getworld=0x1014205C, gates=0x100E7028)
CTORS, GETTYPES, DTORS = [0x10151C24, 0x10151C6C, 0x10151CB4], [0x10112DE0, 0x10112DA8, 0x10112D70], [0x1011261C, 0x101125A4, 0x1011252C]
VTABLES = [0x102A0D44, 0x102A0D20, 0x102A0CFC]
TYPES = [0xB, 0xC, 0xD]
HELPER, TIMESIG_SET, TIMESIG_TURNS = 0x10237E1C, 0x1015F02C, 0x1015F070
OWN = set(A.values()) | set(CTORS) | set(GETTYPES) | set(DTORS) | {HELPER, TIMESIG_SET, TIMESIG_TURNS}

WORLD, GAMEWORLD, G_CENTRE, TPWFLAGS = 0x1037EC0C, 0x1037EBFC, 0x1037F6C4, 0x102B2B4C
IT_FLAG, IT_GUARD, CLOCK, DATE, UI = 0x1037F130, 0x1037F12C, 0x1037F048, 0x1034C388, 0x10396440
GAME_TYPE, GAME_GUARD, MAIN = 0x102B3AA0, 0x102B3AA4, 0x102B4568
TAIL0 = 0x1DA6F0
TURN_WORD = (0x1DA70C - TAIL0) // 4
GUARD = 0x40
HEAD_SIZE = 0x300
HEAD_TOTAL = GUARD + HEAD_SIZE + GUARD
CONTROL_SIZE = 0x12CC
CONTROL_TOTAL = GUARD + CONTROL_SIZE + GUARD
GAME_TOTAL = 0x80
L_WORLD, L_CENTRE, L_STAFF, L_PARK, L_GOLDEN, L_ARRIVAL, L_SESSION, L_UI, L_CLOCK, L_GAME, L_DATE, L_START = 1, 5, 20, 21, 22, 23, 24, 8, 9, 10, 30, 25

I32S = [0, 1, 2, 3, 5, 12, 13, 31, 100, 1900, 1901, 2000, 2100, 0x7FFFFFFF, 0x80000000, MASK, MASK - 1, 0x1234]
U32S = [0, 1, 2, 3, 4, 5, 15000, 0x7FFFFFFF, 0x80000000, MASK]
TICKS = [0, 1, MASK, 0x100000000, 0x7FFFFFFFFFFFFFFF, 0x8000000000000000, M64, 0x01B21DD213814000]


def s32(v):
    return T.s32(v)


def s64(v):
    v &= M64
    return v - (1 << 64) if v & (1 << 63) else v


def qn(q):
    return {k: [s64(v) for v in vs] for k, vs in q.items()}


def setup(runner):
    T.install_std_stubs(runner.emu)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def be64(e, a):
    return int.from_bytes(e.read(a, 8), "big")


def fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & MASK
    return h


def lib_addr(lib, which):
    lib.tpw_ma_addr.restype = C.c_void_p
    lib.tpw_ma_addr.argtypes = [C.c_int32]
    return lib.tpw_ma_addr(which)


# ------------------------------------------------------------------ blocks built from values


def make_fill(seed, size):
    return bytearray(random.Random(seed).randbytes(size))


def sided(fill, fields, init, little):
    buf = bytearray(fill)
    for o, w in fields:
        v = init.get(o)
        if v is None:
            v = int.from_bytes(fill[o:o + w], "big")
        buf[o:o + w] = (v & ((1 << (8 * w)) - 1)).to_bytes(w, "little" if little else "big")
    return bytes(buf)


def decode(buf, fields, little):
    return [int.from_bytes(buf[o:o + w], "little" if little else "big") for o, w in fields]


def gaps(fields, size):
    out, at = [], 0
    for o, w in sorted(fields):
        if o > at:
            out.append((at, o))
        at = max(at, o + w)
    if at < size:
        out.append((at, size))
    return out


def moved(buf, fill, ranges):
    return [a for a, b in ranges if bytes(buf[a:b]) != bytes(fill[a:b])]


H = GUARD   # the world head's offset inside the head block
REC = lambda slot: H + 0x14 * slot
VALID, ONPTR, STAMP, TIMEOUT = 0xA, 0xB, 0xC, 0x10
F294 = H + 0x294
STOP = H + 0x299
PT = H + 0x2A0
PT_START, PT_SESSION, PT_MONTH, PT_DAY, PT_YEAR, PT_RATE = PT, PT + 8, PT + 0x10, PT + 0x14, PT + 0x18, PT + 0x1C
HEAD_FIELDS = ([(REC(s) + o, w) for s in range(32) for o, w in ((VALID, 1), (ONPTR, 1), (STAMP, 4), (TIMEOUT, 4))] + [(F294, 4)] +
               [(STOP + k, 1) for k in range(5)] + [(PT_START, 8), (PT_SESSION, 8), (PT_MONTH, 4), (PT_DAY, 4), (PT_YEAR, 4), (PT_RATE, 4)])
TAIL_WORDS = [(4 * i, 4) for i in range(32)]
GAME_FIELDS = [(0x18, 4)]


def head_bytes(c, little):
    return sided(make_fill(c["seed"], HEAD_TOTAL), HEAD_FIELDS, c["h"], little)


def head_fill(c):
    return make_fill(c["seed"], HEAD_TOTAL)


def tail_words(c, k, little):
    r = random.Random(c["seed"] * 3 + k)
    words = [r.getrandbits(32) for _ in range(32)]
    words[TURN_WORD] = c["turns"][k]
    return struct.pack(("<" if little else ">") + "32I", *words)


def game_bytes(c, little):
    return sided(make_fill(c["seed"] + 7, GAME_TOTAL), GAME_FIELDS, {0x18: c["restart"]}, little)


def control_bytes(c):
    return bytes(make_fill(c["seed"] + 11, CONTROL_TOTAL))


# ------------------------------------------------------------------ the shared case harness

BALANCE_SLOTS = [158, 159, 160, 161, 162, 175, 176]


def base_queues(r):
    def pick(pool, n):
        return [r.choice(pool) if r.random() < 0.5 else r.getrandbits(32) for _ in range(n)]
    return {"ftout": [r.choice([1, 2, 12, 13, 31, 1999, 2000, 2001, 0, MASK]) if r.random() < 0.6 else r.getrandbits(32) for _ in range(10)],
            "ftret": [r.getrandbits(64)], "plus": [r.choice(TICKS)], "minus": [r.choice(TICKS) if r.random() < 0.5 else r.getrandbits(64)],
            "gtmut": [0], "gtnew": [r.getrandbits(64)], "gterr": [r.choice([0, 1, M64, 0x80000000])],
            "pmut": [0], "pday": pick([0, 1, 31, MASK], 2), "pmon": pick([0, 1, 11, MASK], 2), "newnull": [0],
            "numstaff": [r.randrange(0, 40)], "nst": [r.randrange(0, 12) for _ in range(14)], "ffs": [r.choice([0x20, 0x20, r.randrange(32)]) for _ in range(6)],
            "rand": pick([0, 1, MASK, 0x80000000, 7], 8), "closed": [r.choice([0, 1, M64, 0x80000000, r.getrandbits(32)])],
            "nfmut": [0], "nfflags": [r.getrandbits(32)], "nfrestart": [r.getrandbits(32)], "rstop": [0], "rstopk": [r.randrange(5)], "rstopv": [r.randrange(2)]}


def default_case(r):
    return dict(seed=r.getrandbits(32), h={}, turns=[r.choice(U32S) if r.random() < 0.4 else r.getrandbits(32) for _ in range(2)], wsel=r.randrange(2),
                gsel=r.randrange(2), centre=r.randrange(2), date=[r.getrandbits(32) for _ in range(9)], bal=[r.randrange(0, 12) for _ in range(7)],
                flags=r.getrandbits(32), iterflag=r.getrandbits(32), restart=r.choice([0, 1, 2, MASK, 0x80000000, 0x100]), iguard=r.choice([0, 1, 0x7F, 0x80, 0xFF]),
                type=r.choice([0, 1, 2, MASK, 0x80000000, 0x7FFFFFFF]), tguard=r.choice([1, 0x7F, 0x80, 0xFF]), ctortype=r.choice([0, 1, 2, MASK]),
                q=base_queues(r))


def mk(r, **kw):
    c = default_case(r)
    over = kw.pop("override", {})
    c.update(kw)
    c["q"].update(over)
    return c


class Env:
    pass


def ppc_env(e, c):
    v = Env()
    v.L = L = CL.PpcLog({}, qn(c["q"]))
    e.cl = L
    v.head = e.alloc(HEAD_TOTAL, head_bytes(c, False))
    v.tails = [e.alloc(0x80, tail_words(c, k, False)) for k in range(2)]
    v.centre = e.alloc(0x1D0, POISON * 0x1D0)
    v.game = e.alloc(GAME_TOTAL, game_bytes(c, False))
    v.control = e.alloc(CONTROL_TOTAL, control_bytes(c))
    v.news = 0
    h = v.head + H
    L.labels.update({v.tails[0] - TAIL0: L_WORLD, v.tails[1] - TAIL0: L_WORLD + 1, v.centre: L_CENTRE, h: L_STAFF, h + 0x2A0: L_PARK, h + 0x2A8: L_SESSION,
                     h + 0x2C0: L_GOLDEN, h + 0x2C4: L_ARRIVAL, UI: L_UI, CLOCK: L_CLOCK, v.game: L_GAME, h + 0x2A0 + 0: L_PARK})
    L.labels.update({DATE + 4 * i: L_DATE + i for i in range(9)})
    w32(e, WORLD, v.tails[c["wsel"]] - TAIL0)
    w32(e, GAMEWORLD, v.tails[c["gsel"]] - TAIL0)
    w32(e, G_CENTRE, v.centre if c["centre"] else 0)
    w32(e, TPWFLAGS, c["flags"])
    w32(e, IT_FLAG, c["iterflag"])
    e.write(IT_GUARD, bytes([c["iguard"]]))
    w32(e, GAME_TYPE, c["type"])
    e.write(GAME_GUARD, bytes([c["tguard"]]))
    for i, word in enumerate(c["date"]):
        w32(e, DATE + 4 * i, word)
    for slot, value in zip(BALANCE_SLOTS, c["bal"]):
        w32(e, MAIN + 8 + 4 * slot, value)

    def stack_words(emu, first, count):
        return [emu.u32(emu.r(1) + 0x38 + 4 * i) for i in range(count)]

    def ftime(emu):
        outs = [emu.r(i) for i in range(5, 11)] + stack_words(emu, 0, 2)
        L.rec("GetFunnyTime", [L.lab(emu.r(4))] + [L.lab(o) for o in outs])
        for o in outs:
            if o:
                w32(e, o, L.ret("ftout", 0))
        e.write(emu.r(3), (L.ret("ftret", 0) & M64).to_bytes(8, "big"))

    def plus(emu):
        L.rec("TimeStamp+", [L.lab(emu.r(4)), s64(be64(emu, emu.r(4))), s64(be64(emu, emu.r(5)))])
        emu.write(emu.r(3), (L.ret("plus", 0) & M64).to_bytes(8, "big"))

    def minus(emu):
        L.rec("TimeStamp-", [L.lab(emu.r(4)), s64(be64(emu, emu.r(4))), L.lab(emu.r(5)), s64(be64(emu, emu.r(5)))])
        emu.write(emu.r(3), (L.ret("minus", 0) & M64).to_bytes(8, "big"))

    def gettime(emu):
        stamp = emu.r(4)
        outs = [emu.r(i) for i in range(5, 11)] + stack_words(emu, 0, 2)
        L.rec("GetTime", [s64(be64(emu, stamp))] + [L.lab(o) for o in outs])
        if L.ret("gtmut", 0):
            emu.write(stamp, (L.ret("gtnew", 0) & M64).to_bytes(8, "big"))
        w32(e, emu.r(3), L.ret("gterr", 0))

    def post(emu):
        msg = emu.r(4)
        L.rec("PostMessage", [L.lab(emu.r(3)), {VTABLES[k]: TYPES[k] for k in range(3)}.get(emu.u32(msg), -1), emu.u32(msg + 4)])
        if L.ret("pmut", 0):
            w32(e, DATE + 0xC, L.ret("pday", 0))
            w32(e, DATE + 0x10, L.ret("pmon", 0))
        emu.set_r(3, 1)

    def op_new(emu):
        size = emu.r(3)
        fail = bool(L.ret("newnull", 0))
        L.rec("new", [size, int(fail)])
        p = 0 if fail else emu.alloc(size, POISON * size)
        if p:
            v.news += 1
        emu.set_r(3, p)

    def construct(emu, call):
        p, ctor, dtor, size, count = (emu.r(i) for i in range(3, 8))
        if ctor == 0x102657E8:
            L.rec("CMessageCentre", [L.newlab(p), int(ctor == 0x102657E8), int(dtor == 0x10267288), size, count])

    def datechange(emu):
        words = [emu.r(i) for i in range(4, 11)] + stack_words(emu, 0, 2)
        L.rec("DateChange", [L.lab(emu.r(3))] + [s32(w) for w in words])

    def rec_this(name):
        return lambda emu: L.rec(name, [L.lab(emu.r(3))])

    def answer(name, key, signed=False):
        def fake(emu):
            L.rec(name, [L.lab(emu.r(3))])
            emu.set_r(3, L.ret(key, 0) & MASK)
        return fake

    def nst(emu):
        L.rec("GetNumStaffOfType", [L.lab(emu.r(3)), s32(emu.r(4))])
        emu.set_r(3, L.ret("nst", 0) & MASK)

    def generate(emu):
        L.rec("Generate", [emu.r(3) - h, s32(emu.r(4)), s32(emu.r(5))])

    def rand(emu):
        L.rec("GetRand", [L.lab(emu.r(3))])
        if L.ret("rstop", 0):
            e.write(h + 0x299 + L.ret("rstopk", 0), bytes([L.ret("rstopv", 0) & 0xFF]))
        emu.set_r(3, L.ret("rand", 0) & MASK)

    def game_type(emu):
        L.rec("CGameType", [1])
        w32(e, emu.r(3), c["ctortype"])

    def nextframe(emu):
        L.rec("NextFrame", [L.lab(emu.r(3))])
        if L.ret("nfmut", 0):
            w32(e, TPWFLAGS, L.ret("nfflags", 0))
            w32(e, v.game + 0x18, L.ret("nfrestart", 0))

    def controls(emu):
        t = emu.r(3)
        L.rec("CheckAllControlsInitialised", [L.lab(t), fnv(e.read(t, CONTROL_SIZE)), e.read(t + CONTROL_SIZE, 1)[0], e.read(t + CONTROL_SIZE + 1, 1)[0]])

    def op_delete(emu):
        p = emu.r(3)
        if p:
            L.rec("delete", [L.lab(p)])

    S = CL.Stubs()
    S.real = OWN
    for addr, fn in ((0x101127D0, ftime), (0x1015256C, post), (0x101EFE60, datechange), (0x100FC1D0, rec_this("CheckForLocal")),
                     (0x100FC56C, rec_this("CheckForGlobal")), (0x100FC6C4, rec_this("CheckForSecret")), (0x100F2350, rec_this("CreatePeople")),
                     (0x1012A4F8, rec_this("GenerateOpeningStaffPool")), (0x10129830, rec_this("CountStaffTypes")),
                     (0x10129A14, rec_this("EnsureMinimumAmountInPool")), (0x10129CEC, answer("GetNumStaff", "numstaff")), (0x10129C6C, nst),
                     (0x1012A48C, answer("GetFirstFreeSlot", "ffs")), (0x10128B0C, generate), (0x1013B1E4, rand),
                     (0x1013FF7C, answer("IsParkClosed", "closed")), (0x1016F5D8, game_type), (0x10147AE0, nextframe), (0x100FA32C, controls),
                     (0x101EFE04, lambda emu: L.rec("StaffDBChange", [L.lab(emu.r(3)), s32(emu.r(4))])),
                     (0x101ADA64, lambda emu: L.rec("ParkStatusPanelDateChange", [s32(emu.r(3))])),
                     (0x101A0CF4, lambda emu: L.rec("FinancePanelDateChange", [s32(emu.r(3))])),
                     (0x101A577C, lambda emu: L.rec("LoansPanelDateChange", [s32(emu.r(3))]))):
        S.internal(addr, fn)
    S.imp("__pl__11TbTimeStampCFRC10TbTimeDiff", plus)
    S.imp("__mi__11TbTimeStampCFRC11TbTimeStamp", minus)
    S.imp("GetTime__11TbTimeStampCFPiPiPiPiPiPiPiPi", gettime)
    S.imp("__nw__FUl", op_new)
    S.imp("__dl__FPv", op_delete)
    v.S = S
    v.construct = construct
    return v


def ppc_report(e, v, c, **extra):
    L = v.L
    raw = e.read(v.head, HEAD_TOTAL)
    out = {"head": decode(raw, HEAD_FIELDS, False), "untouched": moved(raw, head_fill(c), gaps(HEAD_FIELDS, HEAD_TOTAL)),
           "date": [e.u32(DATE + 4 * i) for i in range(9)], "centre": L.lab(e.u32(G_CENTRE)), "allocs": v.news, "flags": e.u32(TPWFLAGS),
           "iflag": e.u32(IT_FLAG), "restart": e.u32(v.game + 0x18), "tails": [decode(e.read(a, 0x80), TAIL_WORDS, False) for a in v.tails],
           "gworld": L.lab(e.u32(WORLD)), "ggame": L.lab(e.u32(GAMEWORLD)), "log": L.log}
    out.update(extra)
    return out


def run_ppc(e, c, entry, args_of, kind=None, post=None):
    v = ppc_env(e, c)
    saved = v.S.activate(e)
    try:
        T.stub_cb(e, "__construct_array", v.construct)
        spare = e.alloc(0x40, POISON * 0x40)
        args = args_of(v, spare)
        r3 = T.call_ppc(e, entry, args)[0]
        out = ppc_report(e, v, c)
        if kind == "int":
            out["r3"] = r3 & MASK
        elif kind == "label":
            out["r3"] = v.L.lab(r3)
        if post:
            out.update(post(e, v))
    finally:
        CL.Stubs.deactivate(e, saved)
    return out


def host_prepare(lib, c, first=False):
    lib.tpw_ma_config.argtypes = [C.c_int32] * 3
    lib.tpw_ma_config(c["wsel"], c["gsel"], c["ctortype"])
    lib.tpw_ma_prepare.argtypes = [C.c_int32]
    lib.tpw_ma_prepare(c["centre"])
    C.memmove(lib_addr(lib, 2), head_bytes(c, True), HEAD_TOTAL)
    for k in range(2):
        C.memmove(lib_addr(lib, k), tail_words(c, k, True), 0x80)
    C.memmove(lib_addr(lib, 4), game_bytes(c, True), GAME_TOTAL)
    lib.tpw_ma_restart.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_ma_restart(1, s32(c["restart"]))
    C.memmove(lib_addr(lib, 3), control_bytes(c), CONTROL_TOTAL)
    lib.tpw_ma_date.argtypes = [C.POINTER(C.c_uint32), C.c_int32]
    lib.tpw_ma_date((C.c_uint32 * 9)(*c["date"]), 1)
    lib.tpw_ma_flags.argtypes = [C.c_uint32, C.c_int32]
    lib.tpw_ma_flags(c["flags"], s32(c["iterflag"]))
    lib.tpw_ma_balance.argtypes = [C.POINTER(C.c_int32)]
    lib.tpw_ma_balance((C.c_int32 * 7)(*[s32(x) for x in c["bal"]]))
    if not first:
        lib.tpw_ma_mode.argtypes = [C.c_int32]
        lib.tpw_ma_mode(s32(c["type"]))


def run_host(lib, c, fn, first=False, extra=None):
    host_prepare(lib, c, first)
    head = lib_addr(lib, 2)
    lib.tpw_ma_world.restype = C.c_void_p
    fixed = {L_WORLD + k: lib.tpw_ma_world(k) for k in range(2)}
    fixed.update({L_CENTRE: lib_addr(lib, 5), L_STAFF: head + H, L_PARK: head + H + 0x2A0, L_SESSION: head + H + 0x2A8, L_GOLDEN: head + H + 0x2C0,
                  L_ARRIVAL: head + H + 0x2C4, L_UI: lib_addr(lib, 7), L_CLOCK: lib_addr(lib, 8), L_GAME: lib_addr(lib, 4)})
    date = lib_addr(lib, 6)
    fixed.update({L_DATE + i: date + 4 * i for i in range(9)})
    CL.host_begin(lib, fixed, qn(c["q"]))
    try:
        result = fn(lib)
    finally:
        log = CL.host_end(lib)
    raw = C.string_at(head, HEAD_TOTAL)
    words = (C.c_uint32 * 9)()
    lib.tpw_ma_date(words, 0)
    glob = (C.c_int64 * 4)()
    lib.tpw_ma_globals.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ma_globals(glob)
    out = {"head": decode(raw, HEAD_FIELDS, True), "untouched": moved(raw, head_fill(c), gaps(HEAD_FIELDS, HEAD_TOTAL)), "date": list(words),
           "centre": glob[0], "allocs": glob[1], "flags": glob[2] & MASK, "iflag": glob[3] & MASK,
           "restart": lib.tpw_ma_restart(0, 0) & MASK,
           "tails": [decode(C.string_at(lib_addr(lib, k), 0x80), TAIL_WORDS, True) for k in range(2)],
           "gworld": 0, "ggame": 0, "log": log}
    # The world globals as labels: the host's gWorld and gGameWorld name a tail's world.
    out["gworld"] = L_WORLD + c["wsel"]
    out["ggame"] = L_WORLD + c["gsel"]
    if result is not None:
        out["r3"] = result
    if extra:
        out.update(extra(lib))
    return out


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
        for side, report in (("ppc", p), ("host", h)):
            if report.get("untouched"):
                errors.append(f"{side}: a byte outside the stores of the function changed: {report['untouched'][:4]}")
        if difference:
            errors.append(difference)
        return errors


# ------------------------------------------------------------------ the functions

THIS_OFFSET = {"macro": 0, "staff": 0, "park": 0x2A0, "golden": 0x2C0, "arrival": 0x2C4}


class Fn(Base):
    """One translated function on a world: `where` names the receiver (a head offset, the game block, the control table or a spare block); the answer
    kind is None (void), "int" (r3 as a word), "label" (r3 as a call-log label) or "i64" (the hidden result of GetFunnyTimeSinceSessionStart)."""

    def __init__(self, name, entry, where, hostcall, edge, rnd, kind=None, post_ppc=None, post_host=None):
        super().__init__(name, self.ppc_case, self.host_case, edge=edge, rnd=rnd)
        self.entry, self.where, self.hostcall, self.kind = entry, where, hostcall, kind
        self.post_ppc, self.post_host = post_ppc, post_host

    def this_of(self, v, spare):
        w = self.where
        if w in THIS_OFFSET:
            return v.head + H + THIS_OFFSET[w]
        if w == "game":
            return v.game
        if w == "control":
            return v.control + GUARD
        return spare

    def ppc_case(self, e, c):
        if self.kind == "i64":
            res = e.alloc(0x20, b"\x5a" * 0x20)

            def post(emu, v):
                return {"ticks": s64(be64(emu, res + 8)), "guard": bytes(emu.read(res, 8)) + bytes(emu.read(res + 16, 0x10))}
            return run_ppc(e, c, self.entry, lambda v, spare: [res + 8, self.this_of(v, spare)], None, post)
        post = self.post_ppc
        return run_ppc(e, c, self.entry, lambda v, spare: [self.this_of(v, spare)], self.kind, (lambda emu, v: post(emu, v, c)) if post else None)

    def host_case(self, lib, c):
        if self.kind == "i64":
            return self.host_since(lib, c)
        post = self.post_host
        return run_host(lib, c, self.hostcall, first=bool(c.get("first")), extra=(lambda lb: post(lb, c)) if post else None)

    def host_since(self, lib, c):
        box = []

        def call(lb):
            lb.tpw_ma_since.restype = C.c_int64
            box.append(lb.tpw_ma_since())
        out = run_host(lib, c, call)
        out["ticks"] = box[0]
        out["guard"] = b"\x5a" * 0x18
        return out


def call0(name, *args, restype=None):
    def run(lib):
        fn = getattr(lib, name)
        fn.restype = restype
        fn.argtypes = [C.c_int32] * len(args)
        r = fn(*args)
        return r
    return run


def void(name, *args):
    def run(lib):
        fn = getattr(lib, name)
        fn.argtypes = [C.c_int32] * len(args)
        fn(*args)
    return run


# ---- case makers


def turn_pick(r):
    return r.choice(U32S + [100, 200, 0xFFFFFF9C, 99, 101, 1000, 0x80000064, 0x7FFFFFFF - 0x7FFFFFFF % 100]) if r.random() < 0.6 else r.getrandbits(32)


def mk_world(r, **kw):
    c = mk(r, **kw)
    c["turns"] = [turn_pick(r), turn_pick(r)]
    return c


def mk_arrival(r):
    return mk_world(r)


def rec_values(r, turn, force=None):
    t = r.choice([0, 1, 2, 3, 10, 100, 0x3FFFFFFF, 0x40000000, 0x7FFFFFFF, 0x80000000, MASK, r.getrandbits(32)])
    limit = (t << 2) & MASK
    d = r.choice([-1, 0, 1, 2, 0x7FFFFFFF, 0x80000000, -0x7FFFFFFF])
    stamp = (turn - (limit + d)) & MASK if r.random() < 0.8 else r.getrandbits(32)
    return t, stamp


def mk_staff(r, flavour=None, **kw):
    c = mk_world(r, **kw)
    turn = c["turns"][c["wsel"]]
    h = {}
    for s in range(32):
        valid = r.choice([0, 1, 1, 1, 0x80, 0xFF, 2])
        on = r.choice([0, 0, 0, 1, 0xFF, 0x80])
        if flavour == "noretire":
            on = r.choice([1, 0xFF])
        t, stamp = rec_values(r, turn)
        h[REC(s) + VALID], h[REC(s) + ONPTR], h[REC(s) + TIMEOUT], h[REC(s) + STAMP] = valid, on, t, stamp
    iv = r.choice([0, 1, 2, 3, 100, 0x3FFFFFFF, 0x40000000, 0x7FFFFFFF, MASK, 0x80000000, 0x80000001, 15000, r.randrange(0, 5000)])
    limit = (iv << 2) & MASK
    d = r.choice([-1, 0, 1, 2, 0x7FFFFFFF, 0x80000000, -0x7FFFFFFF])
    h[F294] = (turn - (limit + d)) & MASK if r.random() < 0.85 else r.getrandbits(32)
    for k in range(5):
        h[STOP + k] = r.choice([0, 0, 0, 1, 0x80, 0xFF])
    pre = sum(1 for s in range(32) if h[REC(s) + VALID])
    room = 0x20 - pre
    mx = r.choice([0, 1, 2, 5, 0x1F, 0x20, 0x21, MASK, 0x7FFFFFFF, 0x80000000, room - 1, room, room + 1, room + 2, r.randrange(0, 40)]) & MASK
    c["bal"] = [r.choice([0, 1, 2, 3, 5, 12, 20, r.randrange(0, 15)]) for _ in range(5)] + [iv, mx]
    c["h"] = h
    q = c["q"]
    q["numstaff"] = [r.choice([0, 1, 5, 0x1F, 0x20, 0x21, 0x3F, r.randrange(0, 40)])]
    q["nst"] = [r.choice([0, 0, 1, 2, 3, 5, 8, 12, r.randrange(0, 15)]) for _ in range(12)]
    q["ffs"] = [r.choice([0x20, 0, 5, 31, r.randrange(32)]) for _ in range(10)]
    q["rand"] = [r.choice([0, 1, 2, MASK, 0x80000000, 7, 99, r.getrandbits(32)]) for _ in range(12)]
    q["rstop"] = [r.choice([0, 0, 0, 1])]
    return c


def edges_staff_update(extra=0):
    r = random.Random(9100 + extra)
    out = []
    for flavour in (None, "noretire"):
        for _ in range(40):
            out.append(mk_staff(r, flavour))
    # Boundaries of pass 1: one record at a time, the stamp at limit - 1, limit, limit + 1 (and the on-pointer byte).
    for delta in (-1, 0, 1):
        for t in (0, 1, 0x3FFFFFFF, 0x40000000, 0x80000000, MASK):
            for on in (0, 1):
                for valid in (0, 1, 0xFF):
                    c = mk_staff(r)
                    turn = c["turns"][c["wsel"]]
                    s = r.randrange(32)
                    limit = (t << 2) & MASK
                    c["h"][REC(s) + TIMEOUT], c["h"][REC(s) + STAMP] = t, (turn - (limit + delta)) & MASK
                    c["h"][REC(s) + ONPTR], c["h"][REC(s) + VALID] = on, valid
                    c["h"][F294] = turn
                    out.append(c)
    # Pass 2 boundary: the top-up clock at interval * 4 - 1, + 0, + 1; a clock that never fires.
    for iv in (0, 1, 2, 100, 0x3FFFFFFF, 0x40000000, 0x7FFFFFFF, 0x80000000, MASK):
        for delta in (-1, 0, 1):
            c = mk_staff(r)
            turn = c["turns"][c["wsel"]]
            c["bal"][5] = iv
            c["h"][F294] = (turn - (((iv << 2) & MASK) + delta)) & MASK
            out.append(c)
    # The most to make against the room: below, at and above, with the balance's maximum negative and zero.
    for pre in (0, 1, 15, 31, 32):
        for dm in (-1, 0, 1):
            for mxv in (None, 0, MASK, 0x80000000):
                c = mk_staff(r, "noretire")
                for s in range(32):
                    c["h"][REC(s) + VALID] = 1 if s < pre else 0
                room = 0x20 - pre
                c["bal"][6] = (room + dm) & MASK if mxv is None else mxv
                c["bal"][5] = 0
                c["h"][F294] = (c["turns"][c["wsel"]] - 5) & MASK
                out.append(c)
    # Stop flags: every subset of one category, with the dice always picking the stopped category's place.
    for k in range(5):
        for flag in (0, 1, 0xFF):
            c = mk_staff(r, "noretire")
            c["bal"][5] = 0
            c["h"][F294] = (c["turns"][c["wsel"]] - 5) & MASK
            c["h"][STOP + k] = flag
            c["bal"][6] = 20
            out.append(c)
    # The dice stop the production of a category and the free record is 0x20 at once.
    for first in (0x20, 0, 31):
        c = mk_staff(r, "noretire")
        c["bal"][5] = 0
        c["bal"][6] = 20
        c["h"][F294] = (c["turns"][c["wsel"]] - 5) & MASK
        c["q"]["ffs"] = [first, 0x20]
        c["q"]["rstop"] = [1]
        out.append(c)
    return out


def rnd_staff_update(r):
    return mk_staff(r, r.choice([None, "noretire"]))


def mk_parktime(r, **kw):
    c = mk_world(r, **kw)
    ft = [r.choice([2000, 1999, 1900, 2001, 2100, 0, r.getrandbits(32), 0x7FFFFFFF, 0x80000000, MASK, 1901]),
          r.choice([1, 2, 6, 11, 12, 13, 0, r.getrandbits(32), 0x80000000, MASK]),
          r.choice([1, 2, 15, 30, 31, 32, 0, r.getrandbits(32), 0x80000000, MASK]),
          r.randrange(7) if r.random() < 0.8 else r.getrandbits(32), r.randrange(24), r.randrange(60), r.randrange(60),
          r.choice([2000, 1999, 1900, 2001, 0, r.getrandbits(32), MASK])]
    c["q"]["ftout"] = ft
    keep_day = (ft[2] - 1 + 1) & MASK
    keep_month = (ft[1] - 1) & MASK
    c["h"] = {PT_DAY: keep_day if r.random() < 0.4 else r.choice([0, 1, 31, MASK, 0x80000000, (keep_day + 1) & MASK, (keep_day - 1) & MASK, r.getrandbits(32)]),
              PT_MONTH: keep_month if r.random() < 0.4 else r.choice([0, 11, MASK, 0x80000000, (keep_month + 1) & MASK, (keep_month - 1) & MASK, r.getrandbits(32)]),
              PT_YEAR: ft[7] if r.random() < 0.4 else r.choice([0, 1900, 2000, MASK, 0x80000000, (ft[7] + 1) & MASK, (ft[7] - 1) & MASK, r.getrandbits(32)])}
    q = c["q"]
    q["pmut"] = [r.choice([0, 0, 1])]
    q["newnull"] = [r.choice([0, 0, 1]) for _ in range(3)]
    c["centre"] = r.choice([0, 0, 1])
    return c


def edges_parktime_update():
    r = random.Random(9200)
    out = []
    # Each of the three comparisons equal or different, with and without the lazy centre, with each of the allocations failing.
    for dd in (0, 1):
        for mm in (0, 1):
            for yy in (0, 1):
                for centre in (0, 1):
                    for nn in ([0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1], [1, 1, 1]):
                        c = mk_parktime(r)
                        ft = c["q"]["ftout"]
                        c["h"][PT_DAY] = (ft[2] + (1 if dd else 0)) & MASK
                        c["h"][PT_MONTH] = (ft[1] - 1 + (1 if mm else 0)) & MASK
                        c["h"][PT_YEAR] = (ft[7] + (1 if yy else 0)) & MASK
                        c["centre"] = centre
                        c["q"]["newnull"] = nn
                        c["q"]["pmut"] = [0]
                        out.append(c)
    # The day and month compared as signed 32 bit values around their edges; the year with its 1900 and the raw one.
    for day in (0, 1, 2, 30, 31, 32, 0x7FFFFFFF, 0x80000000, MASK):
        for month in (0, 1, 2, 12, 13, 0x7FFFFFFF, 0x80000000, MASK):
            c = mk_parktime(r)
            c["q"]["ftout"][2], c["q"]["ftout"][1] = day, month
            out.append(c)
    # A post that changes the date's day and month (after a post the UI gets the changed date, and the month comparison reads the changed month).
    for _ in range(20):
        c = mk_parktime(r)
        c["q"]["pmut"] = [1]
        c["q"]["pday"] = [r.choice([0, 1, 31, MASK, r.getrandbits(32)]) for _ in range(2)]
        c["q"]["pmon"] = [r.choice([0, 11, MASK, r.getrandbits(32)]) for _ in range(2)]
        c["h"][PT_MONTH] = r.choice([c["q"]["pmon"][0], c["q"]["pmon"][1], (c["q"]["ftout"][1] - 1) & MASK])
        out.append(c)
    return out


def edges_parktime_init():
    r = random.Random(9300)
    out = []
    for month in (0, 1, 2, 12, 13, 0x7FFFFFFF, 0x80000000, MASK):
        for year in (0, 1900, 2000, MASK, 0x80000000):
            c = mk_parktime(r)
            c["q"]["ftout"] = [month, year]
            out.append(c)
    for _ in range(10):
        out.append(mk_parktime(r))
    return out


def mk_since(r, **kw):
    c = mk_world(r, **kw)
    rate = r.choice(U32S) if r.random() < 0.6 else r.getrandbits(32)
    start = r.choice(TICKS) if r.random() < 0.5 else r.getrandbits(64)
    c["h"] = {PT_RATE: rate, PT_START: start, PT_SESSION: r.getrandbits(64)}
    c["q"]["plus"] = [r.choice(TICKS) if r.random() < 0.5 else r.getrandbits(64)]
    c["q"]["minus"] = [r.choice(TICKS) if r.random() < 0.5 else r.getrandbits(64)]
    c["q"]["gtmut"] = [r.randrange(2) if r.random() < 0.3 else 0]
    return c


def edges_since():
    r = random.Random(9400)
    out = []
    for rate in U32S:
        for turn in U32S:
            c = mk_since(r)
            c["h"][PT_RATE] = rate
            c["turns"][c["wsel"]] = turn
            out.append(c)
    for rate in (0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 15000, 3, 5, 1):
        for turn in (0xFFFFFFFE, 0xFFFFFFFD, 0x80000001, 0x7FFFFFFF, 0x55555555, 0xAAAAAAAA, 0x40000000):
            c = mk_since(r)
            c["h"][PT_RATE] = rate
            c["turns"][c["wsel"]] = turn
            out.append(c)
    for start in TICKS:
        for plus in TICKS:
            c = mk_since(r)
            c["h"][PT_START] = start
            c["q"]["plus"] = [plus]
            out.append(c)
    for err in (0, 1, M64, 0x80000000):
        for mut in (0, 1):
            c = mk_since(r)
            c["q"]["gterr"], c["q"]["gtmut"], c["q"]["gtnew"] = [err], [mut], [r.getrandbits(64)]
            out.append(c)
    for wsel in (0, 1):
        c = mk_since(r)
        c["wsel"] = wsel
        out.append(c)
    return out


def mk_ticket(r, **kw):
    c = mk_world(r, **kw)
    c["type"] = r.choice([0, 1, 2, MASK, 0x80000000, 0x7FFFFFFF])
    return c


def edges_tickets():
    r = random.Random(9500)
    first = mk_ticket(r)
    first["first"] = True
    first["tguard"] = 0            # the static is not built yet on the PowerPC side: its constructor runs first
    first["ctortype"] = 0
    out = [first]
    for t in (0, 1, 2, -1 & MASK, 0x80000000, 0x7FFFFFFF, 3):
        for guard in (1, 0x7F, 0x80, 0xFF):
            c = mk_ticket(r)
            c["type"], c["tguard"] = t, guard
            out.append(c)
    return out


def mk_macro(r, op, **kw):
    c = mk_staff(r) if op == 1 else mk_world(r)
    if op == 1:
        pt = mk_parktime(r)
        c["q"].update({k: v for k, v in pt["q"].items() if k in ("ftout", "pmut", "newnull")})
        c["h"].update(pt["h"])
        c["centre"] = pt["centre"]
        c["q"]["ftout"] = pt["q"]["ftout"]
    else:
        c["q"]["ftout"] = [r.choice([1, 2, 12, 2000, MASK]), r.choice([1999, 2000, 0, MASK])]
    c["type"] = r.choice([0, 1, 2, MASK, 0x80000000, 0x7FFFFFFF])
    c.update(kw)
    return c


def edges_macro_update():
    r = random.Random(9600)
    out = []
    for turn in (0, 100, 200, 99, 101, 0xFFFFFFFC, 0xFFFFFF9C, 0x80000000, 0x80000064, 0x7FFFFFFF, 1000, MASK, 300, 50, 150, 0x7FFFFFFF - 0x7FFFFFFF % 100,
                 0xFFFFFFFF - 0xFFFFFFFF % 100):
        for gsel, wsel in ((0, 1), (1, 0), (0, 0)):
            c = mk_macro(r, 1)
            c["gsel"], c["wsel"] = gsel, wsel
            c["turns"][gsel] = turn
            out.append(c)
    for _ in range(30):
        c = mk_macro(r, 1)
        c["gsel"] = r.randrange(2)
        c["turns"][c["gsel"]] = 100 * r.randrange(0, 40)
        out.append(c)
    return out


def mk_iterate(r, **kw):
    c = mk_world(r, **kw)
    c["flags"] = r.choice([0, 8, 2, 0xA, MASK, 0x80000000, 7, 0xF7, r.getrandbits(32)])
    c["restart"] = r.choice([0, 1, 2, MASK, 0x80000000, 0x100, 0x10000])
    c["iguard"] = r.choice([0, 0, 1, 0x7F, 0x80, 0xFF])
    c["iterflag"] = r.choice([0, 1, 8, MASK, r.getrandbits(32)])
    q = c["q"]
    q["nfmut"] = [r.choice([0, 0, 1])]
    q["nfflags"] = [r.choice([0, 8, 2, 0xA, MASK, 0x80000000, r.getrandbits(32)])]
    q["nfrestart"] = [r.choice([0, 1, MASK, 0x80000000, r.getrandbits(32)])]
    return c


def edges_iterate():
    r = random.Random(9700)
    out = []
    for flags in (0, 8, 2, 0xA, 7, 0xF, 0xFFFFFFF7, MASK, 0x80000008, 0x80000000):
        for restart in (0, 1, 0x80000000):
            for guard in (0, 1):
                for mut in (0, 1):
                    c = mk_iterate(r)
                    c["flags"], c["restart"], c["iguard"] = flags, restart, guard
                    c["q"]["nfmut"] = [mut]
                    c["q"]["nfflags"] = [r.choice([0, 8, 2, 0xA, MASK, 0x80000008])]
                    c["q"]["nfrestart"] = [r.choice([0, 1, 0x80000000])]
                    out.append(c)
    return out


def mk_spare(r, **kw):
    c = mk_world(r, **kw)
    c["q"]["closed"] = [r.choice([0, 1, M64, 0x80000000, 2, 0xFF, r.getrandbits(32)])]
    return c


def edges_gates():
    r = random.Random(9800)
    out = []
    for closed in (0, 1, M64, 0x80000000, 0x7FFFFFFF, 2, 0x100, 0xFF, 0x1FF, 0xFFFFFF00, 0x80000001):
        for wsel in (0, 1):
            c = mk_spare(r)
            c["q"]["closed"], c["wsel"] = [closed], wsel
            out.append(c)
    return out


def edges_getworld():
    r = random.Random(9900)
    return [mk_spare(r, wsel=w, gsel=g) for w in (0, 1) for g in (0, 1) for _ in range(3)]


def edges_setup():
    r = random.Random(9950)
    return [mk_world(r) for _ in range(8)]


def edges_arrival():
    r = random.Random(9960)
    return [mk_world(r) for _ in range(6)]


def edges_macro_init():
    r = random.Random(9970)
    return [mk_macro(r, 0) for _ in range(12)]


# ---- extra comparisons


def post_tickets_ppc(e, v, c):
    return {"type": e.u32(GAME_TYPE), "guard": 1 if e.read(GAME_GUARD, 1)[0] else 0}


def post_tickets_host(lib, c):
    lib.tpw_ma_type.restype = C.c_int32
    return {"type": lib.tpw_ma_type() & MASK, "guard": 1}


def post_iterate_ppc(e, v, c):
    return {"guard": e.read(IT_GUARD, 1)[0]}


def post_iterate_host(lib, c):
    return {"guard": 1 if c["iguard"] == 0 else c["iguard"]}


def post_setup_ppc(e, v, c):
    return {"controls": fnv(e.read(v.control, CONTROL_TOTAL))}


def post_setup_host(lib, c):
    return {"controls": fnv(C.string_at(lib_addr(lib, 3), CONTROL_TOTAL))}


def gates_host(lib):
    lib.tpw_ma_gates.restype = C.c_int32
    return lib.tpw_ma_gates()


def getworld_host(lib):
    lib.tpw_ma_getworld.restype = C.c_int32
    return lib.tpw_ma_getworld()


# ------------------------------------------------------------------ the companions: constructors, GetType, destructors

MAC_WRITTEN = [(0, 4), (4, 4)]


def new_log(e, queues=None):
    e.cl = CL.PpcLog({}, queues or {})
    e.cl.deletes = 0
    e.news.clear()
    return e.cl


def op_delete_stub(e):
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
    CL.host_begin(lib, {}, {})
    return heap(lib)


def host_finish(lib, before, **fields):
    out = dict(fields)
    now = heap(lib)
    out["log"] = CL.host_end(lib)
    out["deletes"] = now[2] - before[2]
    out["news"] = now[0] - before[0]
    return out


def ppc_ctor(kind):
    def run(e, c):
        new_log(e)
        obj = e.alloc(0x40, POISON * 0x40)
        ret = T.call_ppc(e, CTORS[kind], [obj])[0]
        typ = T.call_ppc(e, GETTYPES[kind], [obj])[0]
        mask = bytearray(0x40)
        for o, w in MAC_WRITTEN:
            mask[o:o + w] = b"\1" * w
        intact = all(e.read(obj + i, 1) == POISON for i in range(8, 0x40))
        return finish_ppc(e, same=int(ret == obj), flags=e.u32(obj + 4), type=typ, intact=int(intact), vptr=int(e.u32(obj) == VTABLES[kind]))
    return run


def host_ctor(kind):
    def run(lib, c):
        before = host_begin_plain(lib)
        out = (C.c_int64 * 3)()
        lib.tpw_ma_ctor.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
        lib.tpw_ma_ctor(kind, out)
        return host_finish(lib, before, same=1, flags=out[0] & MASK, type=out[1] & MASK, intact=out[2], vptr=1)
    return run


def gettype_cases():
    r = random.Random(9990)
    return [dict(flags=f) for f in (0, 1, MASK, 0x80000000, 0xA5A5A5A5, 0x1B, 0x19, 2)] + [dict(flags=r.getrandbits(32)) for _ in range(8)]


def ppc_gettype(kind):
    def run(e, c):
        new_log(e)
        obj = e.alloc(0x40, POISON * 0x40)
        e.write(obj, struct.pack(">II", VTABLES[kind], c["flags"]))
        first = T.call_ppc(e, GETTYPES[kind], [obj])[0]
        second = T.call_ppc(e, GETTYPES[kind], [obj])[0]
        return finish_ppc(e, first=first, second=second, flags=e.u32(obj + 4), rest=bytes(e.read(obj + 8, 0x38)), vptr=e.u32(obj))
    return run


def host_gettype(kind):
    def run(lib, c):
        before = host_begin_plain(lib)
        lib.tpw_ma_gettype.argtypes = [C.c_int32, C.c_uint32]
        v = lib.tpw_ma_gettype(kind, c["flags"]) & MASK
        return host_finish(lib, before, first=v, second=v, flags=c["flags"], rest=POISON * 0x38, vptr=VTABLES[kind])
    return run


def dtor_cases():
    flags = [0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, MASK, 0x80000000, 0x12340001, 0x1234FFFF, 0x7FFF0000, 0xFFFF0001, 0xFFFF8000, 0x00010000]
    return [dict(flag=f, present=1) for f in flags] + [dict(flag=f, present=0) for f in (0, 1, 0x7FFF, 0x12340001, 0x8000, MASK)]


def dtor_random(r):
    return dict(flag=r.choice([r.getrandbits(32), r.getrandbits(16), r.getrandbits(16) | 0x80000000, 1, 0, MASK]), present=r.choice([1, 1, 1, 0]))


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def ppc_dtor(kind):
    def run(e, c):
        new_log(e)
        obj = e.alloc(0x40, POISON * 0x40) if c["present"] else 0
        e.cl.labels[obj] = 3
        if obj:
            e.write(obj, struct.pack(">II", 0x11111111, 0x11223344))
        s = CL.Stubs()
        s.real.update(OWN)
        s.imp("__dl__FPv", op_delete_stub)
        saved = s.activate(e)
        try:
            ret = T.call_ppc(e, DTORS[kind], [obj, c["flag"]])[0]
        finally:
            CL.Stubs.deactivate(e, saved)
        explicit = bool(obj) and s16(c["flag"]) <= 0
        return finish_ppc(e, same=int(ret == obj), vptr=int(obj == 0 or e.u32(obj) != 0x11111111), flags=e.u32(obj + 4) if explicit else 0)
    return run


def host_dtor(kind):
    def run(lib, c):
        before = host_begin_plain(lib)
        out = (C.c_uint32 * 2)()
        lib.tpw_ma_dtor.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.POINTER(C.c_uint32)]
        lib.tpw_ma_dtor(kind, s32(c["flag"]), c["present"], out)
        explicit = c["present"] and s16(c["flag"]) <= 0
        return host_finish(lib, before, same=1, vptr=1, flags=out[0] if explicit else 0)
    return run


def companion_tests():
    tests = []
    for k, n in enumerate(["CMsgEndOfDay", "CMsgEndOfMonth", "CMsgEndOfYear"]):
        tests.append(FnTest(f"{n}::{n}", ppc_ctor(k), host_ctor(k), edge=lambda: [dict(i=i) for i in range(3)], rnd=lambda r: dict(i=r.getrandbits(8))))
        tests.append(FnTest(f"{n}::GetType", ppc_gettype(k), host_gettype(k), edge=gettype_cases, rnd=lambda r: dict(flags=r.getrandbits(32))))
        tests.append(FnTest(f"{n}::~{n}", ppc_dtor(k), host_dtor(k), edge=dtor_cases, rnd=dtor_random))
    return tests


def rnd_of(maker, *a):
    return lambda r: maker(r, *a)


TESTS = [
    Fn("CGoldenTicketControl::CheckForTickets", A["tickets"], "golden", void("tpw_ma_tickets"), edges_tickets, rnd_of(mk_ticket),
       post_ppc=post_tickets_ppc, post_host=post_tickets_host),
    Fn("CMacroAI::Init", A["macro_init"], "macro", void("tpw_ma_macro", 0), edges_macro_init, rnd_of(mk_macro, 0)),
    Fn("CMacroAI::Update", A["macro_update"], "macro", void("tpw_ma_macro", 1), edges_macro_update, rnd_of(mk_macro, 1)),
    Fn("CArrival::Init", A["arr_init"], "arrival", void("tpw_ma_arrival", 0), edges_arrival, rnd_of(mk_arrival)),
    Fn("CArrival::Update", A["arr_update"], "arrival", void("tpw_ma_arrival", 1), edges_arrival, rnd_of(mk_arrival)),
    Fn("CArrival::UpdateArrivalRate", A["arr_rate"], "arrival", void("tpw_ma_arrival", 2), edges_arrival, rnd_of(mk_arrival)),
    Fn("CParkTime::Init", A["pt_init"], "park", void("tpw_ma_parktime", 0), edges_parktime_init, rnd_of(mk_parktime)),
    Fn("CParkTime::Update", A["pt_update"], "park", void("tpw_ma_parktime", 1), edges_parktime_update, rnd_of(mk_parktime)),
    Fn("CParkTime::GetFunnyTimeSinceSessionStart", A["pt_since"], "park", None, edges_since, rnd_of(mk_since), kind="i64"),
    Fn("CStaffDB::Init", A["st_init"], "staff", void("tpw_ma_staff", 0), lambda: edges_staff_update(1)[:20], rnd_staff_update),
    Fn("CStaffDB::Update", A["st_update"], "staff", void("tpw_ma_staff", 1), edges_staff_update, rnd_staff_update),
    Fn("CGame::Iterate", A["iterate"], "game", void("tpw_ma_iterate"), edges_iterate, rnd_of(mk_iterate), post_ppc=post_iterate_ppc, post_host=post_iterate_host),
    Fn("CControlManager::SetupInitialControls", A["setup"], "control", void("tpw_ma_setup"), edges_setup, rnd_of(mk_world), post_ppc=post_setup_ppc,
       post_host=post_setup_host),
    Fn("CWorldSavvy::GetWorld", A["getworld"], "spare", getworld_host, edges_getworld, rnd_of(mk_spare), kind="label"),
    Fn("CParkAnalyser::GatesOpen", A["gates"], "spare", gates_host, edges_gates, rnd_of(mk_spare), kind="int"),
] + companion_tests()
