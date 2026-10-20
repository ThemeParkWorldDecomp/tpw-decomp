"""w124: the things' turns, original code on one side and src/Weather_turn.cpp, src/Advisor_turn.cpp, src/ChallengeHQ_turn.cpp,
src/ParkAnalyser_turn.cpp, src/msl_tree_insert.cpp and src/ThingMessages.cpp on the other (tests/unit_ThingTurns.cpp).

Functions (main app): CWeather::UpdateTurn 0x10138670, CAdvisor::UpdateTurn 0x1000a8c8, CChallengeHQ::UpdateTurn 0x100f6b98, CParkAnalyser::UpdateTurn
0x100e4188, CParkAnalyserCell::Init 0x100e6054, SubscriptionTree::insert_one 0x101530e4 (the set insertion of the message centre's tree), and the nine
companions: the constructors, GetType and destructors of CMsgWeather (0x101518c4, 0x101399d0, 0x10138b9c), CMsgWeatherWarning (0x10151870, 0x10139994,
0x10138b20) and CMsgChallenge (0x10152008, 0x100f93b0, 0x100f65c8).

Every callee that is not one of these is a fake that records the same event on both sides (the clock, the time stamp operators, the random numbers,
the rain, the sound, the scoring object, the advisor's request queue, the challenge hall's helpers, the message centre's PostMessage and its lazy
construction, the tree's insert_node_at). The worlds are not blocks of their Mac size: the 0x80 bytes at world + 0x1da6f0 are real memory and a world
pointer is that block's address minus 0x1da6f0 on both sides. The object under test lives in a block with 0x40 guard bytes on each side filled with the
same random bytes on both sides; the fields the function reads or writes are compared by value (time stamps are two big endian words on the Mac and a
little endian 64 bit number on the host), every other byte must be unchanged. The fakes can change a world pointer global after a call (queues swapg and
swapw), which shows when each global is read.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ThingTurns"
PROGRAM = "SimTheme Park"
MASK = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF
POISON = b"\xa5"

WEATHER, ADVISOR, HQ, ANALYSER, CELL, INSERT = 0x10138670, 0x1000A8C8, 0x100F6B98, 0x100E4188, 0x100E6054, 0x101530E4
CTORS = [0x101518C4, 0x10151870, 0x10152008]
GETTYPES = [0x101399D0, 0x10139994, 0x100F93B0]
DTORS = [0x10138B9C, 0x10138B20, 0x100F65C8]
VTABLES = [0x102A45EC, 0x102A45C8, 0x102A08D4]
NAMES = ["CMsgWeather", "CMsgWeatherWarning", "CMsgChallenge"]
HELPER, SECONDS, SETSIG = 0x10237E1C, 0x1015F124, 0x1015F02C
OWN = {WEATHER, ADVISOR, HQ, ANALYSER, CELL, INSERT, HELPER, SECONDS, SETSIG} | set(CTORS) | set(GETTYPES) | set(DTORS)

G_WORLD, G_GAME, G_CENTRE, G_RAIN, G_BANK = 0x1037EC0C, 0x1037EBFC, 0x1037F6C4, 0x103BCAA8, 0x102F7C94
RAIN_SLOT = 0x10261058
MAIN, GUARD, SCORING = 0x102B4568, 0x102B4100, 0x102B3AC8
TAIL0 = 0x1DA6F0
TURN_WORD = (0x1DA70C - TAIL0) // 4
TAIL_WORDS = [(4 * i, 4) for i in range(32)]

OBJ_AT = 0x40
CENTRE_SIZE = 0x1D0
CHAL_SIZE = OBJ_AT + 0x30 + 0x40
GT_COUNT = 0x15F

# labels: 1, 2 the worlds, 3 the scoring object, 4 the object, 6 the message centre block, 9 the rain block, 11, 12 the worlds' clocks, 14 to 18 the
# object's members (see the tests), 20 + k the challenges, 22 the tree's end node, 30 + k the tree's nodes, 40 the new node, 41 the key
L_WORLD, L_SCORING, L_OBJ, L_CENTRE, L_RAIN, L_CLOCK, L_CHAL = 1, 3, 4, 6, 9, 11, 20


def s32(v):
    return T.s32(v)


def s64(v):
    v &= M64
    return v - (1 << 64) if v & (1 << 63) else v


def qn(q):
    return {k: [s64(v) for v in vs] for k, vs in q.items()}


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def be64(e, a):
    return int.from_bytes(e.read(a, 8), "big")


def lib_addr(lib, which):
    lib.tpw_tt_addr.restype = C.c_void_p
    lib.tpw_tt_addr.argtypes = [C.c_int32]
    return lib.tpw_tt_addr(which)


def make_fill(seed, size):
    return bytearray(random.Random(seed).randbytes(size))


def sided(fill, fields, init, little):
    """The block of a side: the common fill, with each field (offset, width) holding its value (init, or the PowerPC reading of the fill)."""
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


def absolute(rel):
    return [(OBJ_AT + o, w) for o, w in rel]


def setup(runner):
    T.install_std_stubs(runner.emu)


# ----------------------------------------------------------------- the layouts

WEATHER_SIZE, HQ_SIZE, ADV_SIZE, CELL_SIZE = 0x68, 0x3E4, 0x16F0, 0x30
WEATHER_FIELDS = absolute([(0x0C, 4), (0x10, 8), (0x20, 8), (0x28, 4), (0x2C, 1), (0x30, 4), (0x34, 4), (0x38, 1), (0x3C, 4), (0x40, 4), (0x44, 4),
                           (0x48, 4), (0x4C, 4), (0x50, 4)])
HQ_FIELDS = absolute([(0x3D0, 8), (0x3D9, 1), (0x3DA, 1), (0x3DC, 4)])
CHAL_FIELDS = absolute([(4, 4), (8, 4), (0xC, 2), (0xE, 2), (0x10, 4), (0x14, 4)])
CELL_FIELDS = absolute([(0, 4), (4, 4), (8, 4), (0xC, 4)])


def adv_fields():
    f = [(0xC, 4)]
    for i in range(8):
        o = 0x14 + 0x18 * i
        f += [(o + 8, 4), (o + 0xC, 4), (o + 0x10, 4), (o + 0x14, 1), (o + 0x15, 1), (o + 0x16, 1)]
    f += [(0xD8, 4), (0xDC, 4)]
    for m in range(GT_COUNT):
        o = 0xE0 + 0x10 * m
        f += [(o, 4), (o + 4, 4), (o + 8, 1)]
    f += [(0x16E8, 1), (0x16EC, 4)]
    return absolute(f)


ADV_FIELDS = adv_fields()
BLOCK = {"weather": OBJ_AT + WEATHER_SIZE + 0x40, "hq": OBJ_AT + HQ_SIZE + 0x40, "adv": OBJ_AT + ADV_SIZE + 0x40, "cell": OBJ_AT + CELL_SIZE + 0x40}
FIELDS = {"weather": WEATHER_FIELDS, "hq": HQ_FIELDS, "adv": ADV_FIELDS, "cell": CELL_FIELDS}
GAPS = {k: gaps(FIELDS[k], BLOCK[k]) for k in BLOCK}
CHAL_GAPS = gaps(CHAL_FIELDS, CHAL_SIZE)
MEMBERS = {"weather": {14: 0x10, 15: 0x20}, "hq": {16: 0x3D0, 18: 0x3DC}, "adv": {17: 0xC}, "cell": {}}
SELECT = {"weather": 0, "adv": 1, "hq": 2}


def tail_bytes(c, k, little):
    rr = random.Random(c["seed"] * 3 + k)
    words = [rr.getrandbits(32) for _ in range(32)]
    words[TURN_WORD] = c["turns"][k]
    return struct.pack(("<" if little else ">") + "32I", *words)


def obj_data(c, kind, little):
    fill = make_fill(c["seed"], BLOCK[kind])
    init = {OBJ_AT + o: v for o, v in c["init"].items()}
    return fill, sided(fill, FIELDS[kind], init, little)


def chal_data(c, k, little):
    fill = make_fill(c["seed"] + 77 + k, CHAL_SIZE)
    init = {OBJ_AT + o: v for o, v in (c["chal"][k] if k < len(c.get("chal", [])) else {}).items()}
    return fill, sided(fill, CHAL_FIELDS, init, little)


def base_case(r, **kw):
    c = dict(seed=r.getrandbits(32), world=r.randrange(2), game=r.randrange(2), turns=[r.getrandbits(32), r.getrandbits(32)], centre=1,
             bank=r.getrandbits(32), guard=0, thr=0, mb={}, init={}, chal=[], q={})
    c.update(kw)
    return c


# ----------------------------------------------------------------- the PowerPC side of a case

class Ppc:
    pass


def build_ppc(e, c, kind):
    P = Ppc()
    L = CL.PpcLog({}, qn(c["q"]))
    e.cl = L
    P.L = L
    P.tails = [e.alloc(0x80, tail_bytes(c, k, False)) for k in range(2)]
    P.fill, data = obj_data(c, kind, False)
    P.data = data
    P.block = e.alloc(len(data), data)
    P.obj = P.block + OBJ_AT
    P.centre = e.alloc(CENTRE_SIZE, POISON * CENTRE_SIZE)
    P.rain = e.alloc(0x100, POISON * 0x100)
    P.chals = []
    P.chal_fill = []
    for k in range(4):
        fill, d = chal_data(c, k, False)
        P.chal_fill.append((fill, d))
        P.chals.append(e.alloc(CHAL_SIZE, d))
    for k in range(2):
        L.labels[P.tails[k] - TAIL0] = 1 + k
        L.labels[P.tails[k] - TAIL0 + 0x2A0] = L_CLOCK + k
    L.labels[SCORING] = L_SCORING
    L.labels[P.obj] = L_OBJ
    L.labels[P.centre] = L_CENTRE
    L.labels[P.rain] = L_RAIN
    for k in range(4):
        L.labels[P.chals[k] + OBJ_AT] = L_CHAL + k
    for label, rel in MEMBERS[kind].items():
        L.labels[P.obj + rel] = label
    w32(e, G_WORLD, P.tails[c["world"]] - TAIL0)
    w32(e, G_GAME, P.tails[c["game"]] - TAIL0)
    w32(e, G_CENTRE, P.centre if c["centre"] else 0)
    w32(e, e.u32(RAIN_SLOT), P.rain)
    w32(e, G_BANK, c["bank"])
    e.write(GUARD, bytes([c["guard"]]))
    w32(e, SCORING + 0x20, c["thr"])
    for slot, v in c["mb"].items():
        w32(e, MAIN + 8 + 4 * slot, v)
    P.allocations = []
    return P


def swap_ppc(e, P):
    L = P.L
    g = L.ret("swapg", -1)
    if g >= 0:
        w32(e, G_GAME, P.tails[g] - TAIL0)
    w = L.ret("swapw", -1)
    if w >= 0:
        w32(e, G_WORLD, P.tails[w] - TAIL0)


def world_sel(e, P, address):
    a = e.u32(address) + TAIL0
    return P.tails.index(a) if a in P.tails else -1


POST_TYPES = {0x102A45EC: 1, 0x102A45C8: 2, 0x102A08D4: 0x19}


def common_stubs(e, P):
    """The fakes every function of the unit shares: the clock, the time stamp operators, the random numbers, the message centre."""
    L = P.L
    S = CL.Stubs()
    S.real = set(OWN)

    def funny(emu):
        sp = emu.r(1)
        outs = [emu.r(i) for i in range(5, 11)] + [emu.u32(sp + 0x38)]
        h = emu.u32(sp + 0x3C)
        L.rec("GetFunnyTime", [L.lab(emu.r(4))] + [L.lab(o) for o in outs])
        L.rec("GetFunnyTime.8", [L.lab(h)])
        e.write(emu.r(3), (L.ret("ft") & M64).to_bytes(8, "big"))
        swap_ppc(e, P)

    def rand(emu):
        L.rec("GetRand", [L.lab(emu.r(3))])
        value = L.ret("rand") & MASK
        swap_ppc(e, P)
        emu.set_r(3, value)

    def gt(emu):
        a, b = emu.r(3), emu.r(4)
        L.rec("Stamp>", [L.lab(a), s64(be64(e, a)), L.lab(b), s64(be64(e, b))])
        emu.set_r(3, 1 if L.ret("gt") != 0 else 0)

    def minus(emu):
        a, b = emu.r(4), emu.r(5)
        L.rec("Stamp-", [L.lab(a), s64(be64(e, a)), L.lab(b), s64(be64(e, b))])
        e.write(emu.r(3), (L.ret("minus") & M64).to_bytes(8, "big"))

    def plus(emu):
        a, d = emu.r(4), emu.r(5)
        L.rec("Stamp+", [L.lab(a), s64(be64(e, a)), s64(be64(e, d))])
        e.write(emu.r(3), (L.ret("plus") & M64).to_bytes(8, "big"))

    def diff_set(emu):
        L.rec("DiffSet", [s32(emu.r(i)) for i in range(5, 10)])
        e.write(emu.r(4), ((s32(emu.r(5)) * 864000000000) & M64).to_bytes(8, "big"))
        w32(e, emu.r(3), 0)

    def post(emu):
        this, m = emu.r(3), emu.r(4)
        typ = POST_TYPES[e.u32(m)]
        flags = e.u32(m + 4)
        if typ == 1:
            L.rec("PostMessage", [L.lab(this), 1, flags, s32(e.u32(m + 8))])
        elif typ == 2:
            L.rec("PostMessage", [L.lab(this), 2, flags, s32(e.u32(m + 8)), s32(e.u32(m + 0xC))])
        else:
            hw = lambda a: int.from_bytes(e.read(a, 2), "big")
            L.rec("PostMessage", [L.lab(this), typ, flags, s32(e.u32(m + 8)), s32(e.u32(m + 0xC)), e.read(m + 0x10, 1)[0], s32(e.u32(m + 0x14)),
                                  s32(e.u32(m + 0x18)), s32(e.u32(m + 0x1C)), s32(e.u32(m + 0x20)), s32(e.u32(m + 0x24)), hw(m + 0x28)])
            L.rec("PostMessage.2", [hw(m + 0x2A), s32(e.u32(m + 0x2C))])
        emu.set_r(3, T.u32(L.ret("post")))

    def op_new(emu):
        size = emu.r(3)
        fail = bool(L.ret("newnull", 0))
        L.rec("new", [size, int(fail)])
        p = 0 if fail else emu.alloc(size, POISON * size)
        if p:
            P.allocations.append(p)
        emu.set_r(3, p)

    def construct(emu, call):
        p, ctor, dtor, size, count = (emu.r(i) for i in range(3, 8))
        if ctor == 0x102657E8:
            L.rec("CMessageCentre", [L.newlab(p), int(ctor == 0x102657E8), int(dtor == 0x10267288), size, count])

    P.construct = construct
    S.internal(0x101127D0, funny)
    S.internal(0x1013B1E4, rand)
    S.internal(0x1015256C, post)
    S.imp("__gt__11TbTimeStampCFRC11TbTimeStamp", gt)
    S.imp("__mi__11TbTimeStampCFRC11TbTimeStamp", minus)
    S.imp("__pl__11TbTimeStampCFRC10TbTimeDiff", plus)
    S.imp("Set__10TbTimeDiffFiiiii", diff_set)
    S.imp("__nw__FUl", op_new)
    S.imp("abs", lambda emu: emu.set_r(3, (-s32(emu.r(3)) if s32(emu.r(3)) < 0 else s32(emu.r(3))) & MASK))
    return S


def trace_stub(L):
    def trace(emu):
        L.rec("CTRACE", [emu.r(3)], [emu.cstr(emu.r(4))])
    return trace


def finish_common(e, P, kind):
    buf = e.read(P.block, len(P.data))
    out = {"fields": decode(buf, FIELDS[kind], False), "untouched": moved(buf, P.fill, GAPS[kind]),
           "globals": [P.L.lab(e.u32(G_CENTRE)), len(P.allocations), world_sel(e, P, G_WORLD), world_sel(e, P, G_GAME), e.read(GUARD, 1)[0],
                       s32(e.u32(SCORING + 0x20))]}
    chals = []
    for k in range(3):
        cb = e.read(P.chals[k], CHAL_SIZE)
        chals.append([decode(cb, CHAL_FIELDS, False), moved(cb, P.chal_fill[k][0], CHAL_GAPS)])
    out["chals"] = chals
    out["log"] = P.L.log
    return out


def run_ppc(e, c, kind, entry, extra, args=None, regs=None):
    P = build_ppc(e, c, kind)
    S = common_stubs(e, P)
    extra(e, P, S)
    saved = S.activate(e)
    try:
        T.stub_cb(e, "__construct_array", P.construct)
        for reg, v in (regs or {}).items():
            e.set_r(reg, v)
        r3 = T.call_ppc(e, entry, [P.obj if a == "obj" else a for a in args] if args is not None else [P.obj], max_insns=200_000_000, timeout_us=60_000_000)[0]
        out = finish_common(e, P, kind)
    finally:
        CL.Stubs.deactivate(e, saved)
    out["ret"] = s32(r3) if kind == "cell" else 0
    return out


# ----------------------------------------------------------------- the host side of a case

def api(lib):
    if getattr(lib, "tt_ready", False):
        return lib
    i32, u32, P = C.c_int32, C.c_uint32, C.POINTER
    lib.tpw_tt_world.restype = C.c_void_p
    lib.tpw_tt_world.argtypes = [i32]
    lib.tpw_tt_prepare.argtypes = [i32] * 7
    lib.tpw_tt_balance.argtypes = [i32, i32]
    lib.tpw_tt_globals.argtypes = [P(C.c_int64)]
    lib.tpw_tt_run.argtypes = [i32]
    lib.tpw_tt_cell.argtypes = [i32] * 4
    lib.tpw_tt_cell.restype = i32
    lib.tpw_tt_set_tree.argtypes = [i32, P(i32), P(i32), P(u32), i32, u32]
    lib.tpw_tt_insert.argtypes = [P(C.c_int64)]
    lib.tpw_tt_ctor.argtypes = [i32, P(C.c_int64), P(C.c_int64)]
    lib.tpw_tt_gettype.argtypes = [i32, u32]
    lib.tpw_tt_gettype.restype = i32
    lib.tpw_tt_dtor.argtypes = [i32, i32, i32, P(u32)]
    lib.tt_ready = True
    return lib


def host_run(lib, c, kind, which):
    api(lib)
    fill, data = obj_data(c, kind, True)
    block = lib_addr(lib, 2)
    C.memmove(block, data, len(data))
    obj = block + OBJ_AT
    for k in range(2):
        C.memmove(lib_addr(lib, k), tail_bytes(c, k, True), 0x80)
    C.memset(lib_addr(lib, 3), 0xA5, CENTRE_SIZE)
    C.memset(lib_addr(lib, 4), 0xA5, 0x100)
    chal_fill = []
    for k in range(4):
        f, d = chal_data(c, k, True)
        chal_fill.append(f)
        C.memmove(lib_addr(lib, 5 + k), d, CHAL_SIZE)
    lib.tpw_tt_prepare(c["world"], c["game"], c["centre"], 1, s32(c["bank"]), c["guard"], s32(c["thr"]))
    for slot, v in c["mb"].items():
        lib.tpw_tt_balance(slot, s32(v))
    fixed = {1: lib.tpw_tt_world(0), 2: lib.tpw_tt_world(1), L_CLOCK: lib.tpw_tt_world(0) + 0x2A0, L_CLOCK + 1: lib.tpw_tt_world(1) + 0x2A0,
             L_SCORING: lib_addr(lib, 21), L_OBJ: obj, L_CENTRE: lib_addr(lib, 3), L_RAIN: lib_addr(lib, 4)}
    for k in range(4):
        fixed[L_CHAL + k] = lib_addr(lib, 5 + k) + OBJ_AT
    for label, rel in MEMBERS[kind].items():
        fixed[label] = obj + rel
    CL.host_begin(lib, fixed, qn(c["q"]))
    try:
        if which == 4:
            ret = lib.tpw_tt_cell(*[s32(v) for v in c["args"]])
        else:
            lib.tpw_tt_run(which)
            ret = 0
    finally:
        log = CL.host_end(lib)
    state = (C.c_int64 * 6)()
    lib.tpw_tt_globals(state)
    buf = C.string_at(block, len(data))
    out = {"fields": decode(buf, FIELDS[kind], True), "untouched": moved(buf, fill, GAPS[kind]), "globals": list(state), "ret": ret}
    chals = []
    for k in range(3):
        cb = C.string_at(lib_addr(lib, 5 + k), CHAL_SIZE)
        chals.append([decode(cb, CHAL_FIELDS, True), moved(cb, chal_fill[k], CHAL_GAPS)])
    out["chals"] = chals
    out["log"] = log
    return out


class Base(FnTest):
    """A case builds both sides from the same description; the observed state and the call logs must agree."""

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


# ----------------------------------------------------------------- CWeather::UpdateTurn

FLOATS = [0.0, -0.0, 1.0, -1.0, 0.5, 1.5, 2.5, -2.5, 100.7, -100.7, 12345.678, 2147483520.0, 2147483648.0, -2147483648.0, -2147483904.0, 1e10, -1e10,
          float("nan"), float("inf"), float("-inf"), 3.0e38, 1e-30]
U32_EDGE = [1, 2, 3, 7, 100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFE]


def fbits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def rfloat_bits(r):
    return fbits(r.choice(FLOATS)) if r.random() < 0.7 else r.getrandbits(32)


def rticks(r):
    return r.choice([0, 1, M64, 0x7FFFFFFFFFFFFFFF, 0x8000000000000000, r.getrandbits(64), r.getrandbits(40), r.getrandbits(64)])


def merge(c, over):
    for k, v in over.items():
        if k in ("init", "mb", "q") and isinstance(v, dict):
            c[k].update(v)
        else:
            c[k] = v
    return c


def swap_queues(r):
    q = {}
    if r.random() < 0.3:
        q["swapg"] = [r.choice([-1, -1, 0, 1]) for _ in range(r.randint(1, 4))]
    if r.random() < 0.2:
        q["swapw"] = [r.choice([-1, -1, 0, 1]) for _ in range(r.randint(1, 4))]
    return q


def mk_weather(r, **over):
    mb = {300: r.choice([0, 1, 2, 3, 5, 10, r.randrange(0, 40)]), 301: r.choice([0, 1, 5, 10, 20, 30, r.randrange(0, 60)]),
          302: r.choice([1, 2, 5, 10, 50, 100, r.randrange(1, 400)]), 309: r.choice([1, 2, 5, 10, r.randrange(1, 60)]),
          310: r.choice(U32_EDGE + [r.randrange(1, 1000)]), 311: r.choice([0, 1, 30, r.randrange(0, 500), r.getrandbits(32)]),
          312: r.choice([0, 1, 5, r.randrange(0, 30), r.getrandbits(32)]), 313: r.choice(U32_EDGE + [r.randrange(1, 50)]),
          314: r.choice([1, 10, 100, 500, r.randrange(1, 900), -5, -100, 0x7FFFFFFF, 0x80000000, r.randrange(1, 900)])}
    init = {0x0C: r.getrandbits(32), 0x10: rticks(r), 0x20: rticks(r), 0x28: r.choice([0, 1, 5, r.getrandbits(32)]),
            0x2C: r.choice([0, 0, 1, 1, 2, 0x80, 0xFF]),
            0x30: r.choice([0, r.randrange(0, 3000), r.randrange(0, 200), r.getrandbits(32)]),
            0x34: r.choice([0, 0, r.randrange(0, 3000), r.randrange(0, 200), 0xFFFFFFFF, r.randrange(-5000, 0) & MASK]),
            0x38: r.choice([0, 1, 1, 1, 0xFF, 2, 0x80]),
            0x3C: r.choice([r.randrange(-80, 80) & MASK, r.randrange(-3, 3) & MASK, r.getrandbits(32)]),
            0x40: r.choice([0, 1, 2, r.randrange(0, 12)]),
            0x44: r.choice([-3, -1, 0, 1, 1, 2, 3, 5, r.randrange(-10, 20)]) & MASK,
            0x48: rfloat_bits(r), 0x4C: rfloat_bits(r), 0x50: rfloat_bits(r)}
    q = {"ft": [rticks(r) for _ in range(2)], "gt": [r.randrange(2) for _ in range(3)], "minus": [rticks(r)], "plus": [rticks(r)],
         "fnq": [r.getrandbits(32), r.randrange(0, 5)], "fl": [rfloat_bits(r) for _ in range(12)],
         "rand": [r.choice([r.getrandbits(32), r.randrange(0, 100)]) for _ in range(3)], "newnull": [1 if r.random() < 0.2 else 0],
         "post": [r.randrange(3)], "invclear": [1 if r.random() < 0.85 else 0]}
    q.update(swap_queues(r))
    return merge(base_case(r, centre=r.choice([1, 1, 1, 0]), mb=mb, init=init, q=q), over)


def weather_stubs(e, P, S):
    L = P.L

    def quality(emu):
        L.rec("FindNewQuality", [L.lab(emu.r(3))])
        emu.set_r(3, L.ret("fnq") & MASK)

    def lightning(emu):
        L.rec("FindLightningCoords", [L.lab(emu.r(3)), s32(emu.r(6))])
        for base in (emu.r(4), emu.r(5)):
            for i in range(3):
                w32(e, base + 4 * i, L.ret("fl"))

    def invalidate(emu):
        L.rec("Invalidate", [L.lab(emu.r(3))])
        if L.ret("invclear", 1) != 0:
            e.write(emu.r(3) + 0xC, b"\0")

    def add_lightning(emu):
        sp = emu.r(1)
        L.rec("AddLightning", [L.lab(emu.r(3))] + [emu.r(i) for i in range(4, 10)] + [s32(emu.r(10)), s32(emu.u32(sp + 0x38))])
        emu.set_r(3, 0)

    S.internal(0x10138E70, quality)
    S.internal(0x10139388, lambda emu: L.rec("SetPrecipitationType", [L.lab(emu.r(3)), s32(emu.r(4))]))
    S.internal(0x10138C10, lightning)
    S.internal(0x10139854, invalidate)
    S.imp("SetRaindrops__5CRainFi", lambda emu: L.rec("SetRaindrops", [L.lab(emu.r(3)), s32(emu.r(4))]))
    S.imp("AddLightning__5CRainF10_D3DVECTOR10_D3DVECTORii", add_lightning)
    S.internal(0x100DEE84, lambda emu: L.rec("SND_SetRainLevel", [s32(emu.r(3))]))
    def send_event(emu):
        args = [s32(emu.r(i)) for i in range(3, 9)]
        for i in range(3):
            # Unicorn's fctiwz gives 0x80000000 for a value above INT32_MAX (and +infinity); the processor saturates to 0x7fffffff.
            value = struct.unpack(">f", e.read(P.obj + 0x48 + 4 * i, 4))[0]
            if value >= 2147483648.0 and args[3 + i] == -0x80000000:
                args[3 + i] = 0x7FFFFFFF
        L.rec("SOUND_SendEvent", args)

    S.internal(0x100DD9FC, send_event)
    S.internal(0x1006FCBC, trace_stub(L))


class WeatherTest(Base):
    def __init__(self):
        super().__init__("CWeather::UpdateTurn", lambda e, c: run_ppc(e, c, "weather", WEATHER, weather_stubs), lambda lib, c: host_run(lib, c, "weather", 0),
                         edge=edges_weather, rnd=lambda r: mk_weather(r))


def edges_weather():
    r = random.Random(1240)
    out = []
    for f0c in (0, 1):
        for gt in ([0], [1], [1, 1], [0, 1], [1, 0], [0, 0]):
            for centre, newnull in ((1, 0), (0, 0), (0, 1)):
                out.append(mk_weather(r, centre=centre, init={0x2C: f0c, 0x34: 0xFFFFFFFF, 0x38: 0, 0x44: 5},
                                      q={"gt": gt, "newnull": [newnull], "invclear": [1]}))
    out.append(mk_weather(r, init={0x2C: 1, 0x34: 0xFFFFFFFF, 0x38: 0, 0x44: 5}, q={"gt": [1], "invclear": [0]}))
    for f30, f34, step, maxd in ((0, 0, 5, 100), (3, 0, 5, 100), (100, 0, 5, 100), (100, 100, 5, 100), (0, 100, 5, 100), (100, 103, 5, 10),
                                 (100, 105, 5, 10), (105, 100, 5, 10), (4, 0, 5, 100), (5, 0, 5, 100), (7, 7, 1, 7), (0, 3, 1, 2), (0, 1000, 600, 10),
                                 (1000, 0, 7, 3), (50, 0, 100, 40), (2000000000, 1999999999, 1, 5), (0x7FFFFFFF, 0, 1, 5), (0x80000000, 0, 1, 5),
                                 (3, 0x80000000, 1, 5), (10, 20, 3, 1), (1000000, 1000003, 1, 1000)):
        out.append(mk_weather(r, init={0x2C: 1, 0x30: f30, 0x34: f34, 0x38: 0, 0x44: 5}, mb={302: step, 314: maxd}, q={"gt": [0]}))
    out.append(mk_weather(r, init={0x2C: 1, 0x30: 100, 0x34: 0x80000000, 0x38: 0, 0x44: 5}, q={"gt": [0]}))
    for f3c, speed, f40, f44, f38 in ((0, 1, 0, 3, 1), (1, 1, 0, 3, 1), (5, 1, 0, 3, 1), (-1 & MASK, 1, 0, 3, 1), (0x80000000, 1, 0, 3, 1), (0, 1, 3, 0, 1),
                                      (0, 1, 3, -2 & MASK, 1), (0, 1, 30, 0, 1), (0, 1, 3, 1, 1), (0, 1, 0, 2, 1), (3, 3, 4, 1, 1), (3, 4, 4, 1, 0),
                                      (0, 1, 2, 0, 0xFF), (2, 1, 2, 0, 2)):
        for mb312 in (5, 1, 0x80000000):
            out.append(mk_weather(r, init={0x2C: 1, 0x34: 0xFFFFFFFF, 0x38: f38, 0x3C: f3c, 0x40: f40, 0x44: f44}, mb={309: speed, 312: mb312},
                                  q={"gt": [0]}))
    for f44 in (1, 2, 0, -1 & MASK):
        for bits in ([0.0, 0.0, 0.0], [1.5, -1.5, 2.5], [2147483648.0, -2147483904.0, float("nan")], [float("inf"), float("-inf"), 1e10],
                     [-0.0, 0.5, -0.5], [100.7, -100.7, 12345.678]):
            out.append(mk_weather(r, init={0x2C: 1, 0x34: 0xFFFFFFFF, 0x38: 0, 0x44: f44, 0x48: fbits(bits[0]), 0x4C: fbits(bits[1]), 0x50: fbits(bits[2])},
                                  q={"gt": [0]}))
    for _ in range(12):
        out.append(mk_weather(r, init={0x2C: 1, 0x34: 0xFFFFFFFF, 0x38: 1, 0x3C: 0, 0x44: 0}, mb={313: 1}, q={"gt": [0], "rand": [r.getrandbits(32), 0]}))
    return out


# ----------------------------------------------------------------- CAdvisor::UpdateTurn

MSGS = [0, 1, 2, 7, 0x15E, 0x100]


def mk_advisor(r, **over):
    init = {0xC: r.getrandbits(32), 0xD8: r.getrandbits(32), 0xDC: r.getrandbits(32), 0x16E8: r.choice([0, 0, 0, 1, 1, 2, 0xFF]),
            0x16EC: r.getrandbits(32)}
    for i in range(8):
        o = 0x14 + 0x18 * i
        init[o + 8] = r.choice([-1, -1, -1, 0, 1, 2, 3, r.randrange(-3, 6)]) & MASK
        init[o + 0xC] = r.choice(MSGS)
        init[o + 0x10] = r.randrange(-3, 9) & MASK
        init[o + 0x14] = r.choice([1, 1, 1, 0, 2, 0xFF])
        init[o + 0x15] = r.choice([0, 1, 2])
        init[o + 0x16] = r.choice([0, 0, 1, 2])
    for m in MSGS:
        o = 0xE0 + 0x10 * m
        init[o] = r.getrandbits(32)
        init[o + 4] = r.randrange(-2, 7) & MASK
        init[o + 8] = r.choice([0, 1, 2, 0xFF])
    cyc = r.choice([0, 1, 2, 3, 3, 4, -1, 5])
    n1 = r.randrange(1, 7) if cyc == 3 else r.randrange(0, 7)
    q = {"best": [r.choice(MSGS)], "score": [r.randrange(-3, 9)], "num": [n1, r.randrange(0, 7)], "cycling": [cyc],
         "slot": [r.choice([-1, 0, 1, 2, 3, 4, 5, 6, 7])], "playing": [r.choice([0, 0, 0, 1])], "queued": [r.randrange(3)],
         "len": [r.choice([r.getrandbits(32), r.randrange(0, 5000), 0xFFFFFC17, 0xFFFFFFFF, 0xFFFFFC18, 0])], "play": [r.choice([0, 1, 1, 2])],
         "rand": [r.choice([r.getrandbits(32), r.randrange(0, 100)]) for _ in range(3)]}
    if r.random() < 0.15:
        q["thmut"] = [r.randrange(-2, 9)]
    q.update(swap_queues(r))
    return merge(base_case(r, guard=r.choice([0, 0, 1, 1, 0x80, 0xFF, 2]), thr=r.randrange(-2, 7), init=init, q=q), over)


def advisor_stubs(e, P, S):
    L = P.L
    hw = lambda a: int.from_bytes(e.read(a, 2), "big")

    def scoring_ctor(emu):
        L.rec("Scoring.Ctor", [L.lab(emu.r(3))])

    def answering(name, key, extra=None):
        def stub(emu):
            L.rec(name, [L.lab(emu.r(3)), s32(emu.r(4))])
            emu.set_r(3, T.u32(L.ret(key)))
        return stub

    def request_ctor(emu):
        obj = emu.r(3)
        L.rec("RequestCtor", [s32(emu.r(4)), (emu.r(5) >> 16) & 0xFFFF, emu.r(6), s32(emu.r(7)), emu.r(8) & 0xFF, s32(emu.r(9))])
        e.write(obj, struct.pack(">HHIiiiBBBB", (emu.r(5) >> 16) & 0xFFFF, 0xA5A5, emu.r(6), s32(emu.r(7)), s32(emu.r(4)), s32(emu.r(9)), 1,
                                 emu.r(8) & 0xFF, 0, 0xA5))

    def queue_request(emu):
        m = emu.r(4)
        L.rec("QueueRequest", [L.lab(emu.r(3)), hw(m), e.u32(m + 4), s32(e.u32(m + 8)), s32(e.u32(m + 0xC)), s32(e.u32(m + 0x10)), e.read(m + 0x14, 1)[0],
                               e.read(m + 0x15, 1)[0], e.read(m + 0x16, 1)[0]])
        emu.set_r(3, T.u32(L.ret("queued")))

    def playing(emu):
        L.rec("IsPlaying", [L.lab(emu.r(3))])
        emu.set_r(3, L.ret("playing") & 0xFF)

    def find(emu):
        L.rec("FindHighest", [L.lab(emu.r(3))])
        threshold = L.ret("thmut", -1)
        if threshold != -1:
            w32(e, SCORING + 0x20, threshold)
        emu.set_r(3, T.u32(L.ret("slot")))

    def play(name):
        def stub(emu):
            L.rec(name, [L.lab(emu.r(3)), s32(emu.r(5))])
            w32(e, emu.r(4), L.ret("len"))
            emu.set_r(3, L.ret("play") & MASK)
        return stub

    def get_world(emu):
        L.rec("GetWorld", [L.lab(emu.r(3))])
        emu.set_r(3, e.u32(G_WORLD))

    S.internal(0x10010054, scoring_ctor)
    S.imp("__register_global_object", lambda emu: None)
    S.internal(0x10010F88, answering("Highest", "best"))
    S.internal(0x1001019C, answering("score", "score"))
    S.internal(0x1001055C, answering("num", "num"))
    S.internal(0x10010980, answering("cycling", "cycling"))
    S.internal(0x1000DEB4, request_ctor)
    S.internal(0x1000AE48, queue_request)
    S.internal(0x1000C6E0, playing)
    S.internal(0x1000C7FC, lambda emu: L.rec("SetPlayingFor", [L.lab(emu.r(3)), emu.r(4)]))
    S.internal(0x1000B900, find)
    S.internal(0x1000DFF4, play("Play"))
    S.internal(0x1000E340, play("PlayWithoutScore"))
    S.internal(0x1014205C, get_world)


class AdvisorTest(Base):
    def __init__(self):
        super().__init__("CAdvisor::UpdateTurn", lambda e, c: run_ppc(e, c, "adv", ADVISOR, advisor_stubs, regs={25: 0}),
                         lambda lib, c: host_run(lib, c, "adv", 1), edge=edges_advisor, rnd=lambda r: mk_advisor(r))


def entry_init(i, resp, msg, score, valid=1, only=0, over=0):
    o = 0x14 + 0x18 * i
    return {o + 8: resp & MASK, o + 0xC: msg, o + 0x10: score & MASK, o + 0x14: valid, o + 0x15: only, o + 0x16: over}


def edges_advisor():
    r = random.Random(1241)
    out = []
    # the response choice: cycling, current index and count (the entry says -1)
    for cyc in (0, 1, 2, 3, 4, -1):
        for cur in (-2, -1, 0, 1, 2, 4, 5):
            for n in (0, 1, 2, 3, 5):
                if cyc == 3 and n == 0:
                    continue
                for n2 in sorted({n, 0, 1, 6}):
                    init = {**entry_init(0, -1, 7, 5), 0xE0 + 0x10 * 7 + 4: cur & MASK, 0x16E8: 0}
                    out.append(mk_advisor(r, thr=1, init=init, q={"slot": [0], "playing": [0], "cycling": [cyc], "num": [n, n2], "score": [0], "play": [1],
                                                                  "best": [2]}))
    # the first step: the best of the next record against the threshold
    for score in (-1, 0, 1, 2, 3, 4):
        for thr in (0, 1, 3, 4):
            for playing in (0, 1):
                out.append(mk_advisor(r, thr=thr, q={"score": [score], "playing": [playing], "slot": [-1], "best": [r.choice(MSGS)]}))
    # the silence byte, the slot and the queued score against the minimum
    for flag in (0, 1, 2, 0xFF):
        for slot in (-1, 0, 3, 7):
            for valid in (0, 1, 2):
                for score, thr in ((5, 1), (1, 1), (0, 1), (2, 1), (-1, -2), (-1, -1)):
                    out.append(mk_advisor(r, thr=thr, init={**entry_init(slot if slot >= 0 else 0, 0, 1, score, valid), 0x16E8: flag},
                                          q={"slot": [slot], "playing": [0], "score": [-5], "play": [1]}))
    # the play: both ways, the answer, the length and the states
    for over in (0, 1, 2):
        for play in (0, 1, 2):
            for resp in (-1, 0, 2, 9):
                for length in (0, 5, 0xFFFFFC17, 0xFFFFFFFF, 0xFFFFFC18):
                    out.append(mk_advisor(r, thr=0, init={**entry_init(2, resp, 0x15E, 3, 1, 0, over), 0x16E8: 0},
                                          q={"slot": [2], "playing": [0], "score": [-5], "play": [play], "len": [length], "num": [4, 4],
                                             "cycling": [r.choice([0, 1, 2])]}))
    # the threshold changing under the second read, the worlds changing
    for thr in (0, 2, 4):
        for new in (-1, 0, 3, 6):
            out.append(mk_advisor(r, thr=thr, init={**entry_init(1, 0, 1, 3), 0x16E8: 0}, q={"slot": [1], "playing": [0], "score": [-5], "thmut": [new],
                                                                                            "play": [1]}))
    for g in (0, 1):
        for swaps in ([0], [1], [-1, 1], [-1, -1, 0]):
            out.append(mk_advisor(r, world=g, init={**entry_init(0, -1, 7, 5), 0x16E8: 0},
                                  q={"slot": [0], "playing": [0], "score": [-5], "cycling": [3], "num": [3, 3], "play": [1], "swapw": swaps, "swapg": swaps}))
    return out


# ----------------------------------------------------------------- CChallengeHQ::UpdateTurn

DAY = 864000000000


def rspan(r):
    return r.choice([r.randrange(-400, 400) * DAY + r.choice([0, 0, 1, DAY - 1, r.randrange(0, DAY)]), r.getrandbits(64), 0, M64, 0x7FFFFFFFFFFFFFFF,
                     0x8000000000000000, r.randrange(0, 30) * DAY, r.randrange(-30, 0) * DAY + r.randrange(0, DAY)]) & M64


def mk_chal(r):
    time = r.choice([1, 2, 3, 7, 10, 30, 100, -2, -7, 0x7FFFFFFF, 0x80000000, r.randrange(1, 400), r.getrandbits(32)])
    if time == 0xFFFFFFFF:
        time = 5
    if r.random() < 0.05:
        time = 0
    return {4: time, 8: r.choice([r.getrandbits(32), r.randrange(0, 100), 0xFFFFFFFF]), 0xC: r.getrandbits(16), 0xE: r.getrandbits(16),
            0x10: r.choice([r.getrandbits(32), r.randrange(0, 12)]), 0x14: r.choice([r.getrandbits(32), r.randrange(0, 5000)])}


def mk_hq(r, **over):
    init = {0x3D0: rticks(r), 0x3D9: r.choice([0, 1, 1, 1, 1]), 0x3DA: r.choice([0, 0, 1]), 0x3DC: r.choice([r.getrandbits(32), r.randrange(0, 50)])}
    first = r.choice([0, 1, 1, 1, 2, 3, 4, 1, 1])
    k = r.randrange(1, 5)
    q = {"sys": [r.choice([1, 1, 1, 1, 0, 0x100, 2])], "cur": [first, k, k, k, k], "gt": [r.randrange(2) for _ in range(3)],
         "minus": [rspan(r) for _ in range(2)], "ft": [rticks(r) for _ in range(2)], "aid": [r.choice([r.getrandbits(32), r.randrange(0, 40), 0xFFFFFFFF])],
         "won": [r.choice([0, 0, 1, 0x100, 0x1FF])], "winmut": [r.choice([0, 0, 0, 1])], "winval": [r.getrandbits(32)], "post": [r.randrange(3)],
         "newnull": [1 if r.random() < 0.15 else 0]}
    q.update(swap_queues(r))
    turns = [r.choice([0, 1, 2, 3, 4, 7, 8, 0xFFFFFFFC, r.getrandbits(32)]) for _ in range(2)]
    mb = {482: r.choice([-5, 0, 1, 10, 20, 50, 100, 150, 0x7FFFFFFF, 0x80000000, r.randrange(-20, 160)])}
    return merge(base_case(r, centre=r.choice([1, 1, 1, 0]), turns=turns, mb=mb, init=init, q=q, chal=[mk_chal(r) for _ in range(4)]), over)


def hq_stubs(e, P, S):
    L = P.L
    chals = P.chals

    def system(emu):
        L.rec("IsSystemOn", [L.lab(emu.r(3))])
        emu.set_r(3, L.ret("sys") & 0xFF)

    def current(emu):
        L.rec("GetCurrent", [L.lab(emu.r(3))])
        k = L.ret("cur")
        swap_ppc(e, P)
        emu.set_r(3, 0 if k == 0 else chals[k - 1] + OBJ_AT)

    def did_win(emu):
        value = emu.r(4)
        L.rec("DidWin", [L.lab(emu.r(3)), L.lab(value), s32(e.u32(value)), emu.r(5) & 0xFF])
        if L.ret("winmut") != 0:
            w32(e, value, L.ret("winval"))
        emu.set_r(3, L.ret("won") & 0xFF)

    def active(emu):
        L.rec("GetActiveId", [L.lab(emu.r(3))])
        emu.set_r(3, T.u32(L.ret("aid")))

    S.internal(0x100F7C98, system)
    S.internal(0x100F757C, current)
    S.internal(0x100F7DD8, lambda emu: L.rec("OfferNext", [L.lab(emu.r(3)), emu.r(4) & 0xFF]))
    S.internal(0x100F86D8, lambda emu: L.rec("EndCurrent", [L.lab(emu.r(3))]))
    S.internal(0x100F87D4, lambda emu: L.rec("WonCurrent", [L.lab(emu.r(3))]))
    S.internal(0x100F60D8, did_win)
    S.internal(0x100F8EE4, active)


class HqTest(Base):
    def __init__(self):
        super().__init__("CChallengeHQ::UpdateTurn", lambda e, c: run_ppc(e, c, "hq", HQ, hq_stubs), lambda lib, c: host_run(lib, c, "hq", 2),
                         edge=edges_hq, rnd=lambda r: mk_hq(r))


def tdiv(a, b):
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


def edges_hq():
    r = random.Random(1242)
    out = []
    base = {"cur": [1, 1], "gt": [0], "won": [0], "winmut": [0], "sys": [1]}
    for sys in (0, 1, 0x100):
        for on in (0, 1):
            for wait in (0, 1):
                for gt in (0, 1):
                    for cur in (0, 1, 2):
                        out.append(mk_hq(r, init={0x3D9: on, 0x3DA: wait}, q={**base, "sys": [sys], "gt": [gt], "cur": [cur, cur or 1]}))
    for turn in (0, 1, 2, 3, 4, 5, 7, 8, 0xFFFFFFFC, 0xFFFFFFFF, 0x80000000):
        for gt in (0, 1):
            for won in (0, 1, 0x100):
                for mut in (0, 1):
                    out.append(mk_hq(r, turns=[turn, turn], init={0x3D9: 1}, q={**base, "gt": [gt], "won": [won], "winmut": [mut]}))
    for d in (0, 1, 2, 5, 29, 30, 31, 100, -1, -3, 21474836, 21474837, 42949672, 0x7FFFFFFF, -0x80000000):
        for t in (1, 3, 7, 30, 100, -2, -5, 0x7FFFFFFF, 0x80000000):
            x = (d * 100) & MASK
            x = x - (1 << 32) if x >= 1 << 31 else x
            p = tdiv(x, t if t < 1 << 31 else t - (1 << 32))
            for th in (p - 1, p, p + 1):
                out.append(mk_hq(r, turns=[1, 1], mb={482: th}, init={0x3D9: 1, 0x3D0: 0}, chal=[{4: t}] * 4,
                                 q={**base, "gt": [0], "minus": [(d * DAY) & M64]}))
    for rem in (0, 1, DAY - 1, DAY, DAY + 1, 5 * DAY + 7):
        for sign in (1, -1):
            out.append(mk_hq(r, turns=[0, 0], init={0x3D9: 1}, q={**base, "gt": [0], "minus": [(sign * rem) & M64]}))
    for t in (0, 1):
        out.append(mk_hq(r, turns=[1, 1], init={0x3D9: 1}, chal=[{4: t}] * 4, q={**base, "gt": [0]}))
    for swaps in ([0], [1], [-1, 1], [-1, -1, 0], [0, 1]):
        for turn in (0, 1):
            out.append(mk_hq(r, turns=[turn, 1 - turn], init={0x3D9: 1}, q={**base, "gt": [0], "swapg": swaps, "swapw": swaps}))
    for newnull in (0, 1):
        for centre in (0, 1):
            out.append(mk_hq(r, centre=centre, turns=[1, 1], init={0x3D9: 1}, mb={482: 0x7FFFFFFF}, q={**base, "gt": [0], "newnull": [newnull]}))
            out.append(mk_hq(r, centre=centre, turns=[0, 0], init={0x3D9: 1}, q={**base, "gt": [0], "newnull": [newnull]}))
    return out


# ----------------------------------------------------------------- CParkAnalyser::UpdateTurn and CParkAnalyserCell::Init

def mk_cell(r, **over):
    pool = [0, 1, -1, 7, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32), 100, 0x10000]
    init = {0: r.getrandbits(32), 4: r.getrandbits(32), 8: r.getrandbits(32), 0xC: r.getrandbits(32)}
    return merge(base_case(r, init=init, args=[r.choice(pool) & MASK for _ in range(4)]), over)


def run_analyser(e, c):
    out = run_ppc(e, c, "cell", ANALYSER, lambda e, P, S: None)
    out["ret"] = 0
    return out


def run_cell(e, c):
    return run_ppc(e, c, "cell", CELL, lambda e, P, S: None, args=["obj"] + list(c["args"]))


def cell_cases():
    r = random.Random(1243)
    out = [mk_cell(r, args=[a, b, c, d]) for a in (0, 1, MASK, 0x80000000) for b in (0, 5) for c in (0, 0x7FFFFFFF) for d in (0, MASK, 3)]
    return out


class CellTest(Base):
    def __init__(self):
        super().__init__("CParkAnalyserCell::Init", run_cell, lambda lib, c: host_run(lib, c, "cell", 4), edge=cell_cases, rnd=lambda r: mk_cell(r))


class AnalyserTest(Base):
    def __init__(self):
        super().__init__("CParkAnalyser::UpdateTurn", run_analyser, lambda lib, c: host_run(lib, c, "cell", 3), edge=lambda: [mk_cell(random.Random(i)) for i in range(4)],
                         rnd=lambda r: mk_cell(r))


# ----------------------------------------------------------------- SubscriptionTree::insert_one

TREE_VALUES = [0, 1, 2, 5, 0x7FFF, 0x8000, 0x8001, 0xFFFE, 0xFFFF, 0x1234]
L_END, L_TREE, L_NODE, L_NEW, L_KEY = 22, 23, 30, 40, 41


def mk_tree(r, n=None, key=None, **over):
    n = r.randrange(0, 7) if n is None else n
    left, right = [0] * n, [0] * n
    slots = [(0, "L"), (0, "R")] if n else []
    for i in range(1, n):
        k = r.randrange(len(slots))
        parent, side = slots.pop(k)
        (left if side == "L" else right)[parent] = i + 1
        slots += [(i, "L"), (i, "R")]
    vals = [r.choice(TREE_VALUES + [r.getrandbits(16)]) for _ in range(n)]
    c = dict(n=n, left=left, right=right, vals=vals, count=r.choice([n, 0, r.getrandbits(32)]),
             key=r.choice(TREE_VALUES + [r.getrandbits(16)]) if key is None else key,
             q={"ins": [r.choice([0, 1, 2, 3, 100, 100, r.randrange(0, 7)])]})
    if n and r.random() < 0.5:
        c["key"] = r.choice(vals)
    c.update(over)
    return c


def ppc_insert(e, c):
    L = CL.PpcLog({}, qn(c["q"]))
    e.cl = L
    n = c["n"]
    tree = e.alloc(0x20, POISON * 0x20)
    nodes = [e.alloc(0x20, POISON * 0x20) for _ in range(8)]
    new = e.alloc(0x20, POISON * 0x20)
    key = e.alloc(0x10, POISON * 0x10)
    buf = e.alloc(0x10, POISON * 0x10)
    at = lambda k: 0 if k == 0 else nodes[k - 1]
    for i in range(n):
        e.write(nodes[i], struct.pack(">II", at(c["left"][i]), at(c["right"][i])))
        e.write(nodes[i] + 0xC, struct.pack(">H", c["vals"][i]))
    w32(e, tree, c["count"])
    w32(e, tree + 4, at(1 if n else 0))
    e.write(key, struct.pack(">H", c["key"]))
    L.labels[tree + 4] = L_END
    L.labels[tree] = L_TREE
    for i in range(8):
        L.labels[nodes[i]] = L_NODE + i
    L.labels[new] = L_NEW
    L.labels[key] = L_KEY

    def insert_at(emu):
        L.rec("insert_node_at", [L.lab(emu.r(3)), L.lab(emu.r(4)), emu.r(5) & 0xFF, emu.r(6) & 0xFF, L.lab(emu.r(7)),
                                 int.from_bytes(e.read(emu.r(7), 2), "big")])
        k = L.ret("ins")
        emu.set_r(3, 0 if k == 0 else (new if k == 100 else nodes[k - 1]))

    S = CL.Stubs()
    S.real = set(OWN)
    S.internal(0x101538D8, insert_at)
    saved = S.activate(e)
    try:
        T.call_ppc(e, INSERT, [buf, tree, key])
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"node": L.lab(e.u32(buf)), "inserted": e.read(buf + 4, 1)[0], "log": L.log}


def host_insert(lib, c):
    api(lib)
    n = c["n"]
    arr = lambda xs, t: (t * 8)(*(list(xs) + [0] * (8 - len(xs))))
    lib.tpw_tt_set_tree(1 if n else 0, arr(c["left"], C.c_int32), arr(c["right"], C.c_int32), arr(c["vals"], C.c_uint32), n, c["count"])
    key = lib_addr(lib, 20)
    C.memmove(key, struct.pack("<H", c["key"]), 2)
    fixed = {L_END: lib_addr(lib, 22), L_TREE: lib_addr(lib, 10), L_NEW: lib_addr(lib, 19), L_KEY: key}
    for i in range(8):
        fixed[L_NODE + i] = lib_addr(lib, 11 + i)
    CL.host_begin(lib, fixed, qn(c["q"]))
    try:
        out = (C.c_int64 * 2)()
        lib.tpw_tt_insert(out)
    finally:
        log = CL.host_end(lib)
    return {"node": out[0], "inserted": out[1] & 0xFF, "log": log}


def insert_edges():
    r = random.Random(1244)
    out = []
    for n in range(0, 7):
        for key in TREE_VALUES:
            for ins in (0, 1, 100):
                out.append(mk_tree(r, n=n, key=key, q={"ins": [ins]}))
    out.append(mk_tree(r, n=1, key=5, vals=[5], left=[0], right=[0]))
    out.append(mk_tree(r, n=1, key=4, vals=[5], left=[0], right=[0]))
    out.append(mk_tree(r, n=1, key=6, vals=[5], left=[0], right=[0]))
    out.append(mk_tree(r, n=2, key=5, vals=[5, 5], left=[0, 0], right=[2, 0]))
    out.append(mk_tree(r, n=2, key=0x8000, vals=[0x7FFF, 0x8000], left=[2, 0], right=[0, 0]))
    out.append(mk_tree(r, n=3, key=1, vals=[2, 1, 3], left=[2, 0, 0], right=[3, 0, 0]))
    return out


class InsertTest(FnTest):
    def run_case(self, runner, case):
        runner.emu.reset_scratch()
        try:
            p = ppc_insert(runner.emu, case)
        except ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = host_insert(runner.lib, case)
        errors = T.same_values({k: v for k, v in p.items() if k != "log"}, {k: v for k, v in h.items() if k != "log"}, "", False)
        d = CL.first_difference(p["log"], h["log"])
        if d:
            errors.append(d)
        return errors


# ----------------------------------------------------------------- the companions: constructors, GetType, destructors

# The written ranges of a Mac message object (the rest of the 0x40 poison bytes must stay).
MAC_WRITTEN = [[(0, 4), (4, 4), (8, 4)], [(0, 4), (4, 4), (8, 4), (0xC, 4)],
               [(0, 4), (4, 4), (8, 4), (0xC, 4), (0x10, 1), (0x14, 4), (0x18, 4), (0x1C, 4), (0x20, 4), (0x24, 4), (0x28, 2), (0x2A, 2), (0x2C, 4)]]
MAC_PAYLOAD = [[(8, 4)], [(8, 4), (0xC, 4)], [(8, 4), (0xC, 4), (0x10, 1), (0x14, 4), (0x18, 4), (0x1C, 4), (0x20, 4), (0x24, 4), (0x28, 2), (0x2A, 2), (0x2C, 4)]]
MESSAGE_POOL = [0, 1, 0x15E, 0x15F, 0x10F, 0x17F, MASK, 0x80000000, 7]
WORDS = [0, 1, MASK, 0x7FFFFFFF, 0x80000000, 5, 0x1234, 100]


def new_log(e, queues=None):
    e.cl = CL.PpcLog({}, queues or {})
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
    api(lib)
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
    if kind == 0:
        return [r.choice(WORDS + [r.getrandbits(32)])]
    if kind == 1:
        return [r.choice(WORDS + [r.getrandbits(32)]), r.choice(WORDS + [r.getrandbits(32)])]
    w = lambda: r.choice(WORDS + [r.getrandbits(32)])
    return [w(), r.choice(MESSAGE_POOL + [r.getrandbits(32)]), r.randrange(2), w(), w(), w(), w(), w(), r.choice([0, 1, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16)]),
            r.choice([0, 1, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16)]), r.choice([0, 1, 2, 7, r.getrandbits(32)])]


def ctor_cases(kind):
    def cases():
        r = random.Random(1250 + kind)
        return [dict(args=ctor_args(r, kind), low=r.getrandbits(16)) for _ in range(30)]
    return cases


def ppc_ctor(kind):
    def run(e, c):
        new_log(e)
        obj = e.alloc(0x40, POISON * 0x40)
        a = c["args"]
        if kind < 2:
            args = [obj] + [x & MASK for x in a]
        else:
            args = [obj] + [x & MASK for x in a[:8]] + [((a[8] << 16) | c["low"]) & MASK, ((a[9] << 16) | (c["low"] ^ 0x5A5A)) & MASK, a[10] & MASK]
        ret = T.call_ppc(e, CTORS[kind], args)[0]
        typ = T.call_ppc(e, GETTYPES[kind], [obj])[0]
        payload = []
        for o, w in MAC_PAYLOAD[kind]:
            payload.append(int.from_bytes(e.read(obj + o, w), "big"))
        poison = bytearray(POISON * 0x40)
        mask = bytearray(0x40)
        for o, w in MAC_WRITTEN[kind]:
            mask[o:o + w] = b"\1" * w
        intact = all(e.read(obj + i, 1) == POISON for i in range(0x40) if not mask[i])
        return finish_ppc(e, same=int(ret == obj), flags=e.u32(obj + 4), type=typ, payload=payload, intact=int(intact), vptr=int(e.u32(obj) == VTABLES[kind]))
    return run


def host_ctor(kind):
    def run(lib, c):
        before = host_begin_plain(lib)
        a = list(c["args"])
        vals = (C.c_int64 * 16)(*(a + [0] * (16 - len(a))))
        out = (C.c_int64 * 16)()
        lib.tpw_tt_ctor(kind, vals, out)
        count = [1, 2, 11][kind]
        return host_finish(lib, before, same=1, flags=out[0] & MASK, type=out[1] & MASK, payload=[v & MASK for v in out[2:2 + count]], intact=out[13], vptr=1)
    return run


def mac_payload_fix(kind, c):
    """The payload order of the host (tagId, message, flag, prize, ...) is that of the Mac; the words are masked to their widths."""
    return c


def gettype_cases(kind):
    def cases():
        r = random.Random(1260 + kind)
        return [dict(flags=f) for f in (0, 1, MASK, 0x80000000, 0xA5A5A5A5, 0x1B, 0x19, 2)] + [dict(flags=r.getrandbits(32)) for _ in range(8)]
    return cases


def ppc_gettype(kind):
    def run(e, c):
        new_log(e)
        obj = e.alloc(0x40, POISON * 0x40)
        e.write(obj, struct.pack(">II", VTABLES[kind], c["flags"]))
        first = T.call_ppc(e, GETTYPES[kind], [obj])[0]
        second = T.call_ppc(e, GETTYPES[kind], [obj])[0]
        return finish_ppc(e, first=first, second=second, flags=e.u32(obj + 4), rest=e.read(obj + 8, 0x38), vptr=e.u32(obj))
    return run


def host_gettype(kind):
    def run(lib, c):
        before = host_begin_plain(lib)
        v = lib.tpw_tt_gettype(kind, c["flags"]) & MASK
        return host_finish(lib, before, first=v, second=v, flags=c["flags"], rest=POISON * 0x38, vptr=VTABLES[kind])
    return run


def dtor_cases(kind):
    def cases():
        flags = [0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, MASK, 0x80000000, 0x12340001, 0x1234FFFF, 0x7FFF0000, 0xFFFF0001, 0xFFFF8000, 0x00010000]
        return [dict(flag=f, present=1) for f in flags] + [dict(flag=f, present=0) for f in (0, 1, 0x7FFF, 0x12340001, 0x8000, MASK)]
    return cases


def dtor_random(r):
    return dict(flag=r.choice([r.getrandbits(32), r.getrandbits(16), r.getrandbits(16) | 0x80000000, 1, 0, MASK]), present=r.choice([1, 1, 1, 0]))


def ppc_dtor(kind):
    def run(e, c):
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
        explicit = bool(obj) and (c["flag"] & 0xFFFF if c["flag"] & 0x8000 == 0 else (c["flag"] & 0xFFFF) - 0x10000) <= 0
        return finish_ppc(e, same=int(ret == obj), vptr=int(obj == 0 or e.u32(obj) != 0x11111111), flags=e.u32(obj + 4) if explicit else 0)
    return run


def host_dtor(kind):
    def run(lib, c):
        before = host_begin_plain(lib)
        out = (C.c_uint32 * 2)()
        lib.tpw_tt_dtor(kind, s32(c["flag"]), c["present"], out)
        explicit = c["present"] and s16(c["flag"]) <= 0
        return host_finish(lib, before, same=1, vptr=1, flags=out[0] if explicit else 0)
    return run


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def companion_tests():
    tests = []
    names = ["CMsgWeather", "CMsgWeatherWarning", "CMsgChallenge"]
    for k, n in enumerate(names):
        tests.append(FnTest(f"{n}::{n}", ppc_ctor(k), host_ctor(k), edge=ctor_cases(k), rnd=lambda r, k=k: dict(args=ctor_args(r, k), low=r.getrandbits(16))))
        tests.append(FnTest(f"{n}::GetType", ppc_gettype(k), host_gettype(k), edge=gettype_cases(k), rnd=lambda r: dict(flags=r.getrandbits(32))))
        tests.append(FnTest(f"{n}::~{n}", ppc_dtor(k), host_dtor(k), edge=dtor_cases(k), rnd=dtor_random))
    return tests


TESTS = [WeatherTest(), AdvisorTest(), HqTest(), AnalyserTest(), CellTest(),
         InsertTest("SubscriptionTree::insert_one", None, None, edge=insert_edges, rnd=lambda r: mk_tree(r))] + companion_tests()
