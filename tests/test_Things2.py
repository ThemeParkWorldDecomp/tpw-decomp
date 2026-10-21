"""w130: the things' second batch, original code on one side and src/game/buildings/ResearchLab.cpp, src/game/park/ChallengeHQ.cpp and src/game/advisor/Advisor.cpp, src/game/park/BankAccount.cpp, src/game/park/Weather.cpp, src/game/park/analyser.cpp on the
other (tests/unit_Things2.cpp).

Functions (main app): CResearchLab::Init 0x10121bd4, SetupProjects 0x10122004, SetupCategories 0x101221a8, CParkAnalyser::BuiltSomething 0x100e46a4,
NumberOfStaffType 0x100e8144, CBankAccount::MakeWithdrawal 0x100f33b0, CChallengeHQ::AddChallenge 0x100f7498, ClearInvalidFollowups 0x100f680c,
IsSystemOn 0x100f7c98, OfferNextChallenge 0x100f7dd8, CAdvisor::IsPlaying 0x1000c6e0, FindHighestScoringSlotInQueue 0x1000b900 and
CWeather::SetPrecipitationType 0x10139388.

Every other callee is a fake that records the same event on both sides. The worlds are not blocks of their Mac size: the 0x80 bytes at world + 0x1da6f0
are real memory and a world pointer is that block's address minus 0x1da6f0 on both sides. The object under test lives in a block with 0x40 guard bytes on
each side filled with the same random bytes on both sides; the fields a function reads or writes are compared by value, every other byte must be
unchanged. The two park analysers, the challenge argument block of AddChallenge are compared the same way.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Things2"
PROGRAM = "SimTheme Park"
MASK = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF
POISON = b"\xa5"

INIT, SETUPP, SETUPC, BUILT, STAFF = 0x10121BD4, 0x10122004, 0x101221A8, 0x100E46A4, 0x100E8144
WITHDRAW, ADDCH, CLEARF, ISON, OFFER = 0x100F33B0, 0x100F7498, 0x100F680C, 0x100F7C98, 0x100F7DD8
ISPLAY, FINDHI, PRECIP, HELPER = 0x1000C6E0, 0x1000B900, 0x10139388, 0x10237E1C
OWN = {INIT, SETUPP, SETUPC, BUILT, STAFF, WITHDRAW, ADDCH, CLEARF, ISON, OFFER, ISPLAY, FINDHI, PRECIP, HELPER}

G_WORLD, G_GAME, G_CENTRE, G_BANK = 0x1037EC0C, 0x1037EBFC, 0x1037F6C4, 0x102F7C94
RAIN_SLOT = 0x10261058
MAIN = 0x102B4568
TABLE = 0x1034CBFC
G_TYPE, G_TYPE_GUARD = 0x102B3AA0, 0x102B3AA4
RIDEDB, CLOCK = 0x1037F6C8, 0x1037F048
TAIL0 = 0x1DA6F0
TURN_WORD = (0x1DA70C - TAIL0) // 4

OBJ_AT = 0x40
OBJ_BLOCK = 0x40 + 0x1700 + 0x40
AN_BLOCK = 0x40 + 0x21C48 + 0x40
CHAL_BLOCK = 0x40 + 0x30 + 0x40
RIDE_BLOCK, THING_BLOCK, NRIDES, NTHINGS = 0x1000, 0x40, 6, 8
CENTRE_SIZE = 0x1D0
FIRST_NEW = 400

# labels: 1, 2 the worlds, 11, 12 their clocks, 4 the object, 6 the message centre, 9 the rain, 30 + k the analysers, 40 + k the rides, 50 + k the things,
# 60 the ride database, 61 the clock, 70 + k the research projects, 90 the challenge argument, 200 + k the challenges
L_WORLD, L_CLOCK, L_OBJ, L_CENTRE, L_RAIN, L_AN, L_RIDE, L_THING, L_RIDEDB, L_CLOCKOBJ, L_PROJ, L_ARG, L_CHAL = 1, 11, 4, 6, 9, 30, 40, 50, 60, 61, 70, 90, 200


def s32(v):
    return T.s32(v)


def s64(v):
    v &= M64
    return v - (1 << 64) if v & (1 << 63) else v


def qn(q):
    return {k: [s64(v) for v in vs] for k, vs in q.items()}


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def lib_addr(lib, which):
    lib.tpw_t2_addr.restype = C.c_void_p
    lib.tpw_t2_addr.argtypes = [C.c_int32]
    return lib.tpw_t2_addr(which)


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


def absolute(rel):
    return [(OBJ_AT + o, w) for o, w in rel]


def setup(runner):
    T.install_std_stubs(runner.emu)


# ----------------------------------------------------------------- the layouts

CHAL_SLOT = [(0, 4), (4, 4), (8, 4), (0xC, 2), (0xE, 2), (0x10, 4), (0x14, 4), (0x18, 4), (0x1C, 1), (0x1D, 1), (0x1E, 2), (0x20, 4), (0x24, 4),
             (0x28, 4), (0x2C, 1), (0x2D, 1), (0x2E, 2)]
LAB_FIELDS = []
for _k in range(16):
    LAB_FIELDS += [(0xC + 0x14 * _k + o, w) for o, w in ((0, 4), (4, 4), (8, 1), (0xC, 4), (0x10, 4))]
for _g in range(5):
    LAB_FIELDS += [(0x140C + 0x10 * _g + 8, 4), (0x140C + 0x10 * _g + 0xC, 1)]
LAB_FIELDS += [(0x145C, 4), (0x1460, 4), (0x1464, 4), (0x1468, 1)]
HQ_FIELDS = []
for _k in range(20):
    HQ_FIELDS += [(0xC + 0x30 * _k + o, w) for o, w in CHAL_SLOT]
HQ_FIELDS += [(0x3CC, 4), (0x3D8, 1), (0x3D9, 1), (0x3DA, 1), (0x3DC, 4)]
ADV_FIELDS = []
for _i in range(8):
    ADV_FIELDS += [(0x14 + 0x18 * _i + 0x10, 4), (0x14 + 0x18 * _i + 0x14, 1)]
ADV_FIELDS += [(0xD8, 4), (0xDC, 4)]
WEATHER_FIELDS = [(0x30, 4), (0x54, 4)]
BANK_FIELDS = [(0xC, 4), (0x114, 4), (0x11C, 4), (0x120, 4), (0x124, 4)]
AN_REL = [(0x21C2C, 8), (0x1F5A0, 4)]
KIND_FIELDS = {"lab": LAB_FIELDS, "hq": HQ_FIELDS, "adv": ADV_FIELDS, "weather": WEATHER_FIELDS, "bank": BANK_FIELDS, "an": []}
OBJ_FIELDS = {k: absolute(v) for k, v in KIND_FIELDS.items()}
OBJ_GAPS = {k: gaps(OBJ_FIELDS[k], OBJ_BLOCK) for k in OBJ_FIELDS}
AN_FIELDS = absolute(AN_REL)
AN_GAPS = gaps(AN_FIELDS, AN_BLOCK)
ARG_FIELDS = absolute(CHAL_SLOT)
ARG_GAPS = gaps(ARG_FIELDS, CHAL_BLOCK)


def tail_bytes(c, k, little):
    rr = random.Random(c["seed"] * 3 + k)
    words = [rr.getrandbits(32) for _ in range(32)]
    words[TURN_WORD] = c["turns"][k]
    return struct.pack(("<" if little else ">") + "32I", *words)


def obj_data(c, little):
    fill = make_fill(c["seed"], OBJ_BLOCK)
    init = {OBJ_AT + o: v for o, v in c["init"].items()}
    return fill, sided(fill, OBJ_FIELDS[c["kind"]], init, little)


def an_data(c, k, little):
    fill = make_fill(c["seed"] + 5 + k, AN_BLOCK)
    init = {OBJ_AT + o: v for o, v in c["an"][k].items()} if k < len(c["an"]) else {}
    return fill, sided(fill, AN_FIELDS, init, little)


def arg_data(c, little):
    fill = make_fill(c["seed"] + 99, CHAL_BLOCK)
    init = {OBJ_AT + o: v for o, v in c["arg"].items()}
    return fill, sided(fill, ARG_FIELDS, init, little)


HOST_RIDE = {}


def ride_bytes(c, k, little):
    buf = bytearray(make_fill(c["seed"] + 200 + k, RIDE_BLOCK))
    ride = c["rides"][k] if k < len(c["rides"]) else {}
    order = "little" if little else "big"
    ui, fail = (HOST_RIDE["ui"], HOST_RIDE["fail"]) if little else (0x7A8, 0x804)
    buf[ui:ui + 4] = (ride.get("ui", 0) & MASK).to_bytes(4, order)
    buf[fail:fail + 4] = (ride.get("fail", 0) & MASK).to_bytes(4, order)
    return bytes(buf)


def base_case(r, kind, **kw):
    c = dict(kind=kind, seed=r.getrandbits(32), world=r.randrange(2), game=r.randrange(2), turns=[r.getrandbits(32), r.getrandbits(32)],
             centre=1, bank=r.getrandbits(32), mb={}, init={}, an=[{}, {}], arg={}, rides=[], nrides=0, rows={}, things=[], q={}, guard=1, gtype=0,
             g0=False, args=[], which2=0)
    c.update(kw)
    return c


def merge(c, over):
    for k, v in over.items():
        if k in ("init", "mb", "q", "arg") and isinstance(v, dict):
            c[k].update(v)
        else:
            c[k] = v
    return c


# ----------------------------------------------------------------- the PowerPC side of a case

class Ppc:
    pass


def build_ppc(e, c):
    P = Ppc()
    kind = c["kind"]
    L = CL.PpcLog({}, qn(c["q"]), FIRST_NEW)
    e.cl = L
    P.L = L
    P.tails = [e.alloc(0x80, tail_bytes(c, k, False)) for k in range(2)]
    P.fill, data = obj_data(c, False)
    P.data = data
    P.block = e.alloc(len(data), data)
    P.obj = P.block + OBJ_AT
    P.an_fill, P.an_block = [], []
    for k in range(2):
        f, d = an_data(c, k, False)
        P.an_fill.append(f)
        P.an_block.append(e.alloc(len(d), d))
    P.centre = e.alloc(CENTRE_SIZE, POISON * CENTRE_SIZE)
    P.rain = e.alloc(0x100, POISON * 0x100)
    P.arg_fill, d = arg_data(c, False)
    P.arg_block = e.alloc(len(d), d)
    P.rides = [e.alloc(RIDE_BLOCK, ride_bytes(c, k, False)) for k in range(NRIDES)]
    P.things = []
    for k in range(NTHINGS):
        kd = c["things"][k] if k < len(c["things"]) else (0, 0)
        P.things.append(e.alloc(THING_BLOCK, POISON * 2 + bytes([kd[0] & 0xFF, kd[1] & 0xFF]) + POISON * (THING_BLOCK - 4)))
    for k in range(2):
        L.labels[P.tails[k] - TAIL0] = L_WORLD + k
        L.labels[P.tails[k] - TAIL0 + 0x2A0] = L_CLOCK + k
        L.labels[P.an_block[k] + OBJ_AT] = L_AN + k
    L.labels[P.obj] = L_OBJ
    L.labels[P.centre] = L_CENTRE
    L.labels[P.rain] = L_RAIN
    L.labels[P.arg_block + OBJ_AT] = L_ARG
    L.labels[RIDEDB] = L_RIDEDB
    L.labels[CLOCK] = L_CLOCKOBJ
    for k in range(NRIDES):
        L.labels[P.rides[k]] = L_RIDE + k
    for k in range(NTHINGS):
        L.labels[P.things[k]] = L_THING + k
    if kind == "lab":
        for k in range(16):
            L.labels[P.obj + 0xC + 0x14 * k] = L_PROJ + k
    if kind == "hq":
        for k in range(20):
            L.labels[P.obj + 0xC + 0x30 * k] = L_CHAL + k
    w32(e, G_WORLD, P.tails[c["world"]] - TAIL0)
    w32(e, G_GAME, P.tails[c["game"]] - TAIL0)
    w32(e, G_CENTRE, P.centre if c["centre"] else 0)
    w32(e, e.u32(RAIN_SLOT), P.rain)
    w32(e, G_BANK, c["bank"])
    e.write(G_TYPE_GUARD, bytes([c["guard"]]))
    w32(e, G_TYPE, c["gtype"])
    for slot, v in c["mb"].items():
        w32(e, MAIN + 8 + 4 * slot, v)
    for i in range(NTHINGS + 1):
        s = c["rows"].get(i, 0)
        w32(e, TABLE + 0x14 * i, P.things[s - 1] if s else 0)
    P.allocations = []
    P.ride_pos = 0
    return P


def ppc_stubs(e, P, c):
    L = P.L
    S = CL.Stubs()
    S.real = set(OWN)
    I = S.internal

    def world(emu):
        L.rec("GetWorld", [L.lab(emu.r(3))])
        emu.set_r(3, e.u32(G_WORLD))

    def head(emu):
        L.rec("GetUsedThingHead", [L.lab(emu.r(4))])
        e.write(emu.r(3), struct.pack(">H", L.ret("head", 0) & 0xFFFF))

    def nxt(emu):
        L.rec("GetNextUsedThing", [L.lab(emu.r(4)), L.lab(emu.r(5))])
        e.write(emu.r(3), struct.pack(">H", L.ret("next", 0) & 0xFFFF))

    def analyser(emu):
        L.rec("GetParkAnalyser", [L.lab(emu.r(3))])
        emu.set_r(3, P.an_block[0 if emu.r(3) == P.tails[0] - TAIL0 else 1] + OBJ_AT)

    def gates(emu):
        L.rec("GatesOpen", [L.lab(emu.r(3))])
        emu.set_r(3, L.ret("gates") & 0xFF)

    def funny(emu):
        sp = emu.r(1)
        outs = [emu.r(i) for i in range(5, 11)] + [emu.u32(sp + 0x38)]
        h = emu.u32(sp + 0x3C)
        L.rec("GetFunnyTime", [L.lab(emu.r(4))] + [L.lab(o) for o in outs])
        L.rec("GetFunnyTime.8", [L.lab(h)])
        e.write(emu.r(3), (L.ret("ft") & M64).to_bytes(8, "big"))

    def since(emu):
        L.rec("SinceStart", [L.lab(emu.r(4))])
        e.write(emu.r(3), (L.ret("span") & M64).to_bytes(8, "big"))

    def gametype(emu):
        L.rec("CGameType", [])
        w32(e, emu.r(3), L.ret("gtctor"))

    def project(name, qs):
        def stub(emu):
            p = emu.r(3)
            L.rec(name, [L.lab(p), L.lab(emu.r(4))])
            w32(e, p + 0xC, L.ret(qs[0]))
            w32(e, p + 4, L.ret(qs[1]))
            e.write(p + 8, bytes([L.ret(qs[2]) & 0xFF]))
            emu.set_r(3, T.u32(L.ret(qs[3])))
        return stub

    def first(emu):
        L.rec("GetFirstRide", [L.lab(emu.r(3))])
        P.ride_pos = 0
        emu.set_r(3, P.rides[0] if c["nrides"] > 0 else 0)

    def nextride(emu):
        L.rec("GetNextRide", [L.lab(emu.r(3))])
        P.ride_pos += 1
        emu.set_r(3, P.rides[P.ride_pos] if P.ride_pos < c["nrides"] else 0)

    def tell(emu):
        L.rec("Tell", [L.lab(emu.r(3)), L.lab(emu.r(4)), emu.r(5) & 0xFF])
        emu.set_r(3, T.u32(L.ret("tell")))

    def trace(emu):
        L.rec("CTRACE", [emu.r(3), s32(emu.r(5))], [emu.cstr(emu.r(4))])

    def send_event(emu):
        L.rec("SOUND_SendEvent", [s32(emu.r(i)) for i in range(3, 9)])
        emu.set_r(3, T.u32(L.ret("handle")))

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

    def subscribe(emu):
        L.rec("Subscribe", [L.lab(emu.r(3)), L.lab(emu.r(4)), s32(emu.r(5))])
        emu.set_r(3, L.ret("sub") & 0xFF)

    P.construct = construct
    I(0x1014205C, world)
    I(0x1000DDE8, lambda emu: L.rec("GAME_ASSERTMSG", [s32(emu.r(3))], [emu.cstr(emu.r(4))[:159]]))
    I(0x1013ADB0, head)
    I(0x1013AE08, nxt)
    I(0x1013EBC8, analyser)
    I(0x100E7028, gates)
    I(0x101127D0, funny)
    I(0x10112B14, since)
    I(0x1016F5D8, gametype)
    I(0x1006FCBC, trace)
    I(0x10122918, lambda emu: L.rec("SetEffort", [L.lab(emu.r(3)), s32(emu.r(4)), emu.r(5)]))
    I(0x10122E28, lambda emu: L.rec("UpdateHighestGroup", [L.lab(emu.r(3)), s32(emu.r(4))]))
    I(0x10123998, lambda emu: L.rec("CalculateOutputs", [L.lab(emu.r(3))]))
    I(0x10123D7C, project("SetupProject", ("pscore", "pcat", "pf08", "sp")))
    I(0x10123E6C, project("SetupStatUpgrade", ("uscore", "ucat", "uf08", "su")))
    I(0x101241AC, lambda emu: (L.rec("IsProjectCompleted", [L.lab(emu.r(3))]), emu.set_r(3, L.ret("done") & 0xFF)))
    I(0x101547B4, first)
    I(0x10154800, nextride)
    I(0x100F8194, tell)
    I(0x1014733C, lambda emu: (L.rec("ReadFixed", [L.lab(emu.r(3))]), emu.set_r(3, T.u32(L.ret("clock")))))
    I(0x100DD9FC, send_event)
    I(0x100DE0FC, lambda emu: L.rec("SOUND_FadeSoundEffect", [s32(emu.r(3))]))
    I(0x10152388, subscribe)
    S.imp("SetRainType__5CRainFi", lambda emu: L.rec("SetRainType", [L.lab(emu.r(3)), s32(emu.r(4))]))
    S.imp("SetRaindrops__5CRainFi", lambda emu: L.rec("SetRaindrops", [L.lab(emu.r(3)), s32(emu.r(4))]))
    S.imp("__nw__FUl", op_new)
    return S


RET = {"none": lambda v: 0, "bool": lambda v: int(v & 0xFF != 0), "int": lambda v: s32(v)}


def run_ppc(e, c, spec):
    P = build_ppc(e, c)
    S = ppc_stubs(e, P, c)
    saved = S.activate(e)
    try:
        T.stub_cb(e, "__construct_array", P.construct)
        target = P.an_block[c["which2"]] + OBJ_AT if c["kind"] == "an" else P.obj
        arg = P.arg_block + OBJ_AT if spec.which == 6 else None
        callargs = [target] + ([arg] if arg is not None else [a & MASK for a in c["args"]])
        r3 = T.call_ppc(e, spec.entry, callargs, max_insns=200_000_000, timeout_us=60_000_000)[0]
        out = collect_ppc(e, P, c)
    finally:
        CL.Stubs.deactivate(e, saved)
    out["ret"] = RET[spec.ret](r3)
    return out


def collect_ppc(e, P, c):
    buf = e.read(P.block, len(P.data))
    out = {"fields": decode(buf, OBJ_FIELDS[c["kind"]], False), "untouched": moved(buf, P.fill, OBJ_GAPS[c["kind"]])}
    ans, bad = [], []
    for k in range(2):
        b = e.read(P.an_block[k], AN_BLOCK)
        ans.append(decode(b, AN_FIELDS, False))
        bad += moved(b, P.an_fill[k], AN_GAPS)
    out["an"] = ans
    out["untouched"] += bad
    a = e.read(P.arg_block, CHAL_BLOCK)
    out["arg"] = decode(a, ARG_FIELDS, False)
    out["untouched"] += moved(a, P.arg_fill, ARG_GAPS)
    out["globals"] = [P.L.lab(e.u32(G_CENTRE)), len(P.allocations)]
    out["log"] = P.L.log
    return out


# ----------------------------------------------------------------- the host side of a case

def api(lib):
    if getattr(lib, "t2_ready", False):
        return lib
    i32, i64 = C.c_int32, C.c_int64
    lib.tpw_t2_world.restype = C.c_void_p
    lib.tpw_t2_world.argtypes = [i32]
    lib.tpw_t2_prepare.argtypes = [i32] * 6
    lib.tpw_t2_balance.argtypes = [i32, i32]
    lib.tpw_t2_row.argtypes = [i32, i32]
    lib.tpw_t2_thing.argtypes = [i32, i32, i32]
    lib.tpw_t2_gametype.argtypes = [i32]
    lib.tpw_t2_gametype_built.restype = i32
    lib.tpw_t2_globals.argtypes = [C.POINTER(i64)]
    lib.tpw_t2_run.argtypes = [i32, i64, i32]
    lib.tpw_t2_run.restype = i64
    lib.tpw_t2_ride_offset.argtypes = [i32]
    lib.tpw_t2_ride_offset.restype = i32
    HOST_RIDE["ui"], HOST_RIDE["fail"] = lib.tpw_t2_ride_offset(0), lib.tpw_t2_ride_offset(1)
    lib.t2_ready = True
    return lib


def host_run(lib, c, spec):
    api(lib)
    kind = c["kind"]
    fill, data = obj_data(c, True)
    block = lib_addr(lib, 2)
    C.memmove(block, data, len(data))
    obj = block + OBJ_AT
    for k in range(2):
        C.memmove(lib_addr(lib, k), tail_bytes(c, k, True), 0x80)
    an_fill = []
    for k in range(2):
        f, d = an_data(c, k, True)
        an_fill.append(f)
        C.memmove(lib_addr(lib, 3 + k), d, len(d))
    C.memset(lib_addr(lib, 5), 0xA5, CENTRE_SIZE)
    C.memset(lib_addr(lib, 6), 0xA5, 0x100)
    arg_fill, d = arg_data(c, True)
    C.memmove(lib_addr(lib, 7), d, len(d))
    for k in range(NRIDES):
        C.memmove(lib_addr(lib, 8 + k), ride_bytes(c, k, True), RIDE_BLOCK)
    for k in range(NTHINGS):
        kd = c["things"][k] if k < len(c["things"]) else (0, 0)
        C.memset(lib_addr(lib, 14 + k), 0xA5, THING_BLOCK)
        lib.tpw_t2_thing(k, kd[0] & 0xFF, kd[1] & 0xFF)
    lib.tpw_t2_prepare(c["world"], c["game"], c["centre"], 1, s32(c["bank"]), c["nrides"])
    for slot, v in c["mb"].items():
        lib.tpw_t2_balance(slot, s32(v))
    for i in range(NTHINGS + 1):
        lib.tpw_t2_row(i, c["rows"].get(i, 0))
    if not c["g0"]:
        lib.tpw_t2_gametype(s32(c["gtype"]))
    fixed = {L_WORLD: lib.tpw_t2_world(0), L_WORLD + 1: lib.tpw_t2_world(1), L_CLOCK: lib.tpw_t2_world(0) + 0x2A0,
             L_CLOCK + 1: lib.tpw_t2_world(1) + 0x2A0, L_OBJ: obj, L_AN: lib_addr(lib, 3) + OBJ_AT, L_AN + 1: lib_addr(lib, 4) + OBJ_AT,
             L_CENTRE: lib_addr(lib, 5), L_RAIN: lib_addr(lib, 6), L_ARG: lib_addr(lib, 7) + OBJ_AT, L_RIDEDB: lib_addr(lib, 23),
             L_CLOCKOBJ: lib_addr(lib, 22)}
    for k in range(NRIDES):
        fixed[L_RIDE + k] = lib_addr(lib, 8 + k)
    for k in range(NTHINGS):
        fixed[L_THING + k] = lib_addr(lib, 14 + k)
    if kind == "lab":
        for k in range(16):
            fixed[L_PROJ + k] = obj + 0xC + 0x14 * k
    if kind == "hq":
        for k in range(20):
            fixed[L_CHAL + k] = obj + 0xC + 0x30 * k
    CL.host_begin(lib, fixed, qn(c["q"]), FIRST_NEW)
    try:
        arg = c["args"][0] if c["args"] else 0
        ret = lib.tpw_t2_run(spec.which, s32(arg) if spec.which in (4, 12) else arg, c["which2"])
    finally:
        log = CL.host_end(lib)
    state = (C.c_int64 * 2)()
    lib.tpw_t2_globals(state)
    buf = C.string_at(block, len(data))
    out = {"fields": decode(buf, OBJ_FIELDS[kind], True), "untouched": moved(buf, fill, OBJ_GAPS[kind])}
    ans = []
    for k in range(2):
        b = C.string_at(lib_addr(lib, 3 + k), AN_BLOCK)
        ans.append(decode(b, AN_FIELDS, True))
        out["untouched"] += moved(b, an_fill[k], AN_GAPS)
    out["an"] = ans
    a = C.string_at(lib_addr(lib, 7), CHAL_BLOCK)
    out["arg"] = decode(a, ARG_FIELDS, True)
    out["untouched"] += moved(a, arg_fill, ARG_GAPS)
    out["globals"] = list(state)
    out["ret"] = RET[spec.ret](ret & MASK)
    out["log"] = log
    return out


class Spec(FnTest):
    """A case builds both sides from the same description; the observed state and the call logs must agree."""

    def __init__(self, name, which, entry, ret, edges, mk):
        FnTest.__init__(self, name, None, None, edge=edges, rnd=mk)
        self.which, self.entry, self.ret = which, entry, ret

    def run_case(self, runner, case):
        runner.emu.reset_scratch()
        api(runner.lib)
        if case.get("g0") and runner.lib.tpw_t2_gametype_built():
            case = dict(case, g0=False, guard=1)   # the host's function-local static is built already: only its first use shows the constructor
        try:
            p = run_ppc(runner.emu, case, self)
        except ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = host_run(runner.lib, case, self)
        difference = CL.first_difference(p.pop("log"), h.pop("log"))
        errors = T.same_values(p, h, "", False)
        for side, report in (("ppc", p), ("host", h)):
            if report.get("untouched"):
                errors.append(f"{side}: a byte outside the stores of the function changed: {report['untouched'][:4]}")
        if difference:
            errors.append(difference)
        return errors


# ----------------------------------------------------------------- value pools

FLOATS = [0.0, -0.0, 1.0, -1.0, 0.5, 1.5, 100.7, -100.7, 12345.678, 1e10, -1e10, float("nan"), float("inf"), float("-inf"), 3.0e38, 1e-30]
U32_EDGE = [0, 1, 2, 3, 7, 100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFE]


def fbits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def rfloat(r):
    return fbits(r.choice(FLOATS)) if r.random() < 0.7 else r.getrandbits(32)


def rword(r):
    return r.choice(U32_EDGE + [r.getrandbits(32), r.getrandbits(32), r.randrange(0, 50)])


def mk(kind, **kw):
    return lambda r: base_case(r, kind, **kw)


# ----------------------------------------------------------------- the research lab

def lab_projects(r, n):
    return {0xC + 0x14 * k + o: v for k in range(n) for o, v in ((4, r.choice([0, 1, 2, 3, 4, 5, r.getrandbits(32)])), (8, r.choice([0, 1, 2, 3, 0xFF, r.getrandbits(8)])),
                                                            (0xC, rfloat(r)))}


def lab_state(r, count):
    init = lab_projects(r, 16)
    init.update({0x145C: count, 0x1460: rword(r), 0x1464: r.choice([-1, -2, 0x80000000, 0, 1, 5, 9, 15, r.randrange(0, 16)]) & MASK,
                 0x1468: r.choice([0, 1, 2, 0xFF])})
    for g in range(5):
        init[0x140C + 0x10 * g + 8] = rword(r)
        init[0x140C + 0x10 * g + 0xC] = r.choice([0, 1, 2, 3, 0xFF, r.getrandbits(8)])
    return init


def lab_queues(r):
    def score():
        return rfloat(r)
    return {"pscore": [score() for _ in range(8)], "pcat": [r.choice([0, 1, 2, 3, 4, 5, -1, r.getrandbits(32)]) for _ in range(8)],
            "pf08": [r.choice([0, 1, 2, 5, 0xFF, r.getrandbits(32)]) for _ in range(8)],
            "sp": [r.choice([0, 1, 0x100, 0xFF, 0x7F00, r.getrandbits(32)]) for _ in range(8)],
            "uscore": [score() for _ in range(8)], "ucat": [r.choice([0, 1, 2, 3, 4, 5, r.getrandbits(32)]) for _ in range(8)],
            "uf08": [r.choice([0, 1, 2, 5, 0xFF, r.getrandbits(32)]) for _ in range(8)],
            "su": [r.choice([0, 1, 0x100, 0xFF, r.getrandbits(32)]) for _ in range(8)],
            "done": [r.choice([0, 0, 1, 0x100, 0xFF, 2]) for _ in range(12)]}


def rides_of(r, n):
    return [dict(ui=r.choice([0, 1, 2, 3, 4, 5, -1, 0x80000000, r.randrange(-3, 8)]), fail=r.choice([0, 0, 0, 1, 2, 0x100, 0xFFFFFFFF])) for _ in range(n)]


def mb_research(r):
    mb = {334: rword(r)}
    for g in range(5):
        mb[328 + g] = rword(r)
    return mb


def mk_lab(r, count=None, **over):
    n = r.randint(0, NRIDES)
    c = base_case(r, "lab", init=lab_state(r, r.randrange(0, 11) if count is None else count), q=lab_queues(r), rides=rides_of(r, NRIDES), nrides=n,
                  mb=mb_research(r))
    return merge(c, over)


def mk_init(r, **over):
    c = mk_lab(r)
    ids = r.sample(range(1, NTHINGS + 1), r.randint(0, NTHINGS))
    c["rows"] = {i: r.randint(1, NTHINGS) for i in range(1, NTHINGS + 1)}
    c["rows"][0] = 0 if r.random() < 0.9 else 1
    c["things"] = [(r.choice([4, 5, 6, 7, 8, 8, 8, 0, 9, 3]), r.choice([0, 0, 0, 1, 2])) for _ in range(NTHINGS)]
    c["q"].update(list_queues(ids))
    c["q"]["newnull"] = [1 if r.random() < 0.2 else 0]
    c["q"]["sub"] = [r.choice([0, 1, 0x100])]
    c["centre"] = r.choice([0, 1, 1])
    return merge(c, over)


def init_edges():
    out = []
    for centre in (0, 1):
        for nn in (0, 1):
            for ids in ([], [1], [1, 2], [3, 1, 2]):
                for model_kind in (8, 7, 0):
                    r = random.Random(len(out))
                    c = mk_init(r, centre=centre)
                    c["q"]["newnull"] = [nn]
                    c["q"].update(list_queues(ids))
                    c["rows"] = {1: 1, 2: 2, 3: 3}
                    c["things"] = [(model_kind, 0), (8, 0), (8, 1)]
                    out.append(c)
    return out


def setupp_edges():
    out = []
    for best in (-1, -2, 0x80000000, 0, 3, 9):
        for n in (0, 1, 2, 3, 6):
            r = random.Random(best + n)
            c = mk_lab(r, count=r.randrange(0, 10), nrides=n)
            c["init"][0x1464] = best & MASK
            out.append(c)
    # score ordering: the second project is greater, equal, smaller, nan, negative
    for pair in ([1.0, 2.0], [2.0, 1.0], [1.0, 1.0], [float("nan"), 1.0], [1.0, float("nan")], [-1.0, -2.0], [0.0, -0.0], [float("inf"), float("inf")]):
        for best in (-1, 0):
            r = random.Random(7)
            c = mk_lab(r, count=1, nrides=1)
            c["init"][0x1464] = best & MASK
            c["init"][0xC + 0x0C] = fbits(pair[0])
            c["q"]["pscore"] = [fbits(pair[1])]
            c["q"]["sp"] = [1]
            c["rides"] = [dict(ui=0, fail=0)]
            out.append(c)
    return out


def setupc_edges():
    out = []
    for count in (0, 1, 2, 5, 16):
        for done in (0, 1):
            r = random.Random(count * 2 + done)
            c = mk_lab(r, count=count)
            c["q"]["done"] = [done]
            for i in range(count):
                c["init"][0xC + 0x14 * i + 4] = i % 5
                c["init"][0xC + 0x14 * i + 8] = i % 3
            for g in range(5):
                c["init"][0x140C + 0x10 * g + 0xC] = 1 + g % 2
            out.append(c)
    return out


# ----------------------------------------------------------------- the park analyser

def list_queues(ids):
    return {"head": [ids[0] if ids else 0], "next": list(ids[1:]) + [0]}


def mk_staff(r, **over):
    n = r.randint(0, 6)
    ids = [r.randint(1, NTHINGS) for _ in range(n)]
    c = base_case(r, "an", which2=r.randrange(2), args=[r.choice([0, 3, 4, 5, 6, 7, 8, 8, 9, 0x13, r.randrange(0, 20)])])
    c["rows"] = {i: r.randint(1, NTHINGS) for i in range(1, NTHINGS + 1)}
    c["rows"][0] = 0 if r.random() < 0.85 else r.randint(1, NTHINGS)
    c["things"] = [(r.choice([0, 3, 4, 5, 6, 7, 8, 8, 9, 0x13]), r.choice([0, 0, 0, 1, 2, 0xFF])) for _ in range(NTHINGS)]
    c["q"] = list_queues(ids)
    return merge(c, over)


def staff_edges():
    out = []
    for model in range(0, 12):
        for dead in (0, 1):
            c = base_case(random.Random(model), "an", args=[model], which2=model & 1)
            c["rows"] = {0: 0, 1: 1, 2: 2}
            c["things"] = [(model, dead), (4 + model % 5, 0)]
            c["q"] = list_queues([1, 2, 1])
            out.append(c)
    out.append(merge(base_case(random.Random(1), "an", args=[8]), dict(rows={0: 1, 1: 1}, things=[(8, 0)], q=list_queues([1]))))
    out.append(merge(base_case(random.Random(2), "an", args=[8]), dict(rows={}, things=[], q=list_queues([]))))
    return out


def mk_built(r):
    c = base_case(r, "an", which2=r.randrange(2))
    c["q"] = {"ft": [r.choice([0, 1, M64, 0x7FFFFFFFFFFFFFFF, 0x8000000000000000, r.getrandbits(64), r.getrandbits(40)])]}
    return c


def built_edges():
    out = []
    for w in (0, 1):
        for k in (0, 1):
            for t in (0, 1, M64, 0x7FFFFFFFFFFFFFFF, 0x8000000000000000, 0x123456789ABCDEF0):
                out.append(merge(base_case(random.Random(w * 2 + k), "an", world=w, which2=k), dict(q={"ft": [t]})))
    return out


# ----------------------------------------------------------------- the bank

def mk_withdraw(r):
    bal = r.choice([0, 1, -1, 100, -100, 0x7FFFFFFF, 0x80000000, r.getrandbits(32), r.randrange(-1000, 1000)]) & MASK
    last = r.choice([-1, 0, 1, r.getrandbits(32), r.randrange(-10, 10)]) & MASK
    amount = r.choice([0, 1, 5, 100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32), r.randrange(0, 2000)])
    init = {0xC: bal, 0x114: r.choice([0, 1, 1, 2, 0x100, r.getrandbits(32)]), 0x11C: last, 0x120: r.getrandbits(32), 0x124: rword(r)}
    c = base_case(r, "bank", init=init, args=[amount])
    c["an"] = [{0x1F5A0 - 0x0: rword(r)}, {0x1F5A0: rword(r)}]
    return c


def withdraw_edges():
    out = []
    for enabled in (0, 1, 2):
        for bal, last, amount in ((100, 100, 50), (100, 100, 100), (100, 100, 101), (50, -1, 100), (-5, -5, 1), (0, 0, 1), (0, -1, 0), (5, 0, 5),
                                  (0x80000000, 0, 1), (0x7FFFFFFF, 1, 0xFFFFFFFF), (1, 1, 0x80000000)):
            for world, game in ((0, 0), (0, 1), (1, 0), (1, 1)):
                r = random.Random(len(out))
                c = mk_withdraw(r)
                c["init"].update({0xC: bal & MASK, 0x114: enabled, 0x11C: last & MASK})
                c["args"] = [amount]
                c["world"], c["game"] = world, game
                out.append(c)
    return out


# ----------------------------------------------------------------- the challenge hall

def rslot(r, small=True):
    t = r.choice([0, 0, 1, 2, 3, 4, 5] if small else [0, 1, 2, r.getrandbits(32)])
    return {0: t, 4: rword(r), 8: rword(r), 0xC: r.getrandbits(16), 0xE: r.getrandbits(16), 0x10: rword(r), 0x14: rword(r),
            0x18: r.choice([0, 0, 1, 2, 3, 4, 5, 6]), 0x1C: r.choice([0, 1]), 0x1D: r.choice([0, 1, 1]), 0x1E: r.getrandbits(16),
            0x20: rword(r), 0x24: rword(r), 0x28: r.choice([0, 1, 2, 3, 4, 5, r.getrandbits(32)]), 0x2C: r.choice([0, 0, 1]), 0x2D: r.getrandbits(8),
            0x2E: r.getrandbits(16)}


def hq_slots(r, small=True):
    init = {}
    for k in range(20):
        for o, v in rslot(r, small).items():
            init[0xC + 0x30 * k + o] = v
    return init


def mk_hq(r, **over):
    init = hq_slots(r)
    init.update({0x3CC: r.choice([-3, -2, -1, 0, 1, 5, 18, 19, 20, 21, 25, 40, 0x7FFFFFF0, 0x80000000, r.randrange(-3, 40)]) & MASK,
                 0x3D8: r.choice([0, 0, 1]), 0x3D9: r.choice([0, 0, 0, 1]), 0x3DA: r.choice([0, 0, 0, 1]), 0x3DC: rword(r)})
    c = base_case(r, "hq", init=init, mb={480: r.choice([0, 1, 2, 5, 10, -1, 0x80000000, r.randrange(0, 30)]) & MASK,
                                         481: r.choice([0, 1, 2, 3, 4, 5, -1, 0x80000000, r.randrange(-2, 7)]) & MASK})
    c["q"] = {"tell": [r.choice([0, 1, 0x100, 0xFF, 0x101, r.getrandbits(32)])]}
    return merge(c, over)


def mk_clear(r):
    init = {}
    for k in range(20):
        init[0xC + 0x30 * k] = r.choice([0, 1, 2, 3, 4, 5])
        init[0xC + 0x30 * k + 0x18] = r.choice([0, 0, 1, 2, 3, 4, 5, 6])
        for o, v in rslot(r).items():
            if o not in (0, 0x18):
                init[0xC + 0x30 * k + o] = v
    c = mk_hq(r)
    c["init"].update(init)
    return c


def clear_edges():
    out = []
    for types, follows in (([1, 2, 3], [2, 3, 1]), ([1, 2, 3], [4, 4, 4]), ([1, 1, 2], [1, 0, 0]), ([0, 2, 0], [1, 1, 1]), ([1, 2, 3], [0, 0, 0]),
                           ([1, 2], [2, 1]), ([3, 3], [3, 3])):
        r = random.Random(len(out))
        c = mk_clear(r)
        for k in range(20):
            c["init"][0xC + 0x30 * k] = types[k] if k < len(types) else 0
            c["init"][0xC + 0x30 * k + 0x18] = follows[k] if k < len(follows) else 0
        out.append(c)
    return out


def mk_add(r):
    c = mk_hq(r)
    c["arg"] = rslot(r, False)
    full = r.random() < 0.15
    for k in range(20):
        c["init"][0xC + 0x30 * k] = r.choice([1, 2, 3]) if (full or r.random() < 0.6) else 0
    return c


def add_edges():
    out = []
    for free in (None, 0, 1, 7, 19, (3, 9)):
        r = random.Random(len(out))
        c = mk_add(r)
        for k in range(20):
            c["init"][0xC + 0x30 * k] = 1
        for f in ((free,) if isinstance(free, int) else (free or ())):
            c["init"][0xC + 0x30 * f] = 0
        out.append(c)
    return out


def mk_ison(r):
    c = mk_hq(r)
    c["gtype"] = r.choice([0, 0, 0, 0, 1, 2, 3, r.getrandbits(32)])
    day = 864000000000
    nd = r.choice([0, 1, 2, 5, 10, 100, r.randrange(0, 30)])
    base = c["mb"][480]
    span = r.choice([s32(base) * day + r.choice([-1, 0, 1, day - 1, day]), nd * day + r.randrange(0, day), r.getrandbits(64) - (1 << 63),
                     (r.getrandbits(32) << 32) + r.getrandbits(32), -day, 0, 1 << 62, r.getrandbits(48)])
    c["q"].update({"gates": [r.choice([0, 1, 1, 0x100, 0xFF, 2])], "span": [span & M64], "gtctor": [0]})
    return c


def ison_edges():
    out = []
    day = 864000000000
    first = mk_ison(random.Random(1))
    first.update(g0=True, guard=0, gtype=0)
    first["init"][0x3D8] = 0
    first["q"].update({"gtctor": [0], "gates": [1], "span": [100 * day]})
    first["mb"][480] = 5
    out.append(first)
    for gtype in (0, 1, 0x80000000):
        for sw in (0, 1):
            for gates in (0, 1):
                for dd in (-1, 0, 1):
                    r = random.Random(len(out))
                    c = mk_ison(r)
                    c["gtype"] = gtype
                    c["init"][0x3D8] = sw
                    c["init"][0x3D9] = 0
                    c["init"][0x3DA] = 0
                    c["mb"][480] = 7
                    c["q"].update({"gates": [gates], "span": [(7 + dd) * day + (day - 1 if dd == -1 else 0)]})
                    out.append(c)
    for bal in (0, -1, 0x7FFFFFFF, 0x80000000):
        for span in (0, day - 1, day, -day, 1 << 32 << 0, (1 << 32) * day, (1 << 63) - 1, -(1 << 63)):
            r = random.Random(len(out))
            c = mk_ison(r)
            c["gtype"] = 0
            c["init"][0x3D8] = 0
            c["mb"][480] = bal & MASK
            c["q"].update({"gates": [1], "span": [span & M64]})
            out.append(c)
    return out


def mk_offer(r):
    c = mk_hq(r)
    c["args"] = [r.choice([0, 1, 1])]
    return c


def offer_edges():
    out = []
    for cur in (-3, -1, 0, 1, 18, 19, 20, 21, 39, 40, 0x7FFFFFF0, 0x80000000):
        for flags in ((0, 0), (1, 0), (0, 1)):
            for tell in (0, 1, 0x100):
                r = random.Random(len(out))
                c = mk_offer(r)
                c["init"][0x3CC] = cur & MASK
                c["init"][0x3D9], c["init"][0x3DA] = flags
                c["q"]["tell"] = [tell]
                c["args"] = [tell & 1]
                c["mb"][481] = 3
                for k in range(20):
                    c["init"][0xC + 0x30 * k] = 0
                    c["init"][0xC + 0x30 * k + 0x28] = 1
                    c["init"][0xC + 0x30 * k + 0x2C] = 0
                    c["init"][0xC + 0x30 * k + 0x1D] = 1
                if cur >= -1:
                    c["init"][0xC + 0x30 * ((cur + 1) % 20)] = 1
                    c["init"][0xC + 0x30 * ((cur + 1) % 20) + 0x2C] = 1 if tell == 1 else 0
                    c["init"][0xC + 0x30 * ((cur + 5) % 20)] = 1
                out.append(c)
    for dec, bal in ((0, 1), (1, 1), (2, 1), (2, 3), (3, 3), (-1 & MASK, 0), (5, 0x80000000), (0x80000000, 0x80000000), (0, -1 & MASK)):
        for won in (0, 1):
            for indep in (0, 1):
                r = random.Random(len(out))
                c = mk_offer(r)
                c["init"][0x3CC] = 4
                c["init"][0x3D9], c["init"][0x3DA] = 0, 0
                c["mb"][481] = bal
                for k in range(20):
                    c["init"][0xC + 0x30 * k] = 1 if k in (5, 9) else 0
                    c["init"][0xC + 0x30 * k + 0x28] = dec
                    c["init"][0xC + 0x30 * k + 0x2C] = won if k == 5 else 0
                    c["init"][0xC + 0x30 * k + 0x1D] = indep if k == 5 else 1
                out.append(c)
    return out


# ----------------------------------------------------------------- the advisor and the weather

def mk_play(r):
    start = r.choice([0, 1, 100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFF0, 0xFFFFFFFF, r.getrandbits(32), r.randrange(0, 1000)])
    dur = r.choice([0, 1, 10, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32), r.randrange(0, 1000)])
    clock = r.choice([start, (start + dur) & MASK, (start + dur - 1) & MASK, (start + dur + 1) & MASK, 0, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF,
                      r.getrandbits(32)])
    c = base_case(r, "adv", init={0xD8: start, 0xDC: dur})
    c["q"] = {"clock": [clock]}
    return c


def play_edges():
    out = []
    for start, dur in ((0, 0), (5, 5), (0xFFFFFFFF, 1), (0xFFFFFFF0, 0x20), (0x80000000, 0x80000000), (0x7FFFFFFF, 1), (100, 0xFFFFFFFF)):
        for delta in (-1, 0, 1):
            for clock in ((start + dur + delta) & MASK, start, 0, 0xFFFFFFFF, 0x80000000):
                r = random.Random(len(out))
                c = mk_play(r)
                c["init"].update({0xD8: start, 0xDC: dur})
                c["q"]["clock"] = [clock]
                out.append(c)
    return out


def mk_find(r):
    init = {}
    for i in range(8):
        init[0x14 + 0x18 * i + 0x10] = r.choice([0, 1, 5, 5, 10, -1, -5, 0x7FFFFFFF, 0x80000000, r.getrandbits(32), r.randrange(-20, 20)]) & MASK
        init[0x14 + 0x18 * i + 0x14] = r.choice([0, 0, 1, 1, 2, 0xFF, 0x80])
    return base_case(r, "adv", init=init)


def find_edges():
    out = []
    for valids in ([0] * 8, [1] * 8, [0, 0, 0, 0, 0, 0, 0, 1], [1, 0, 0, 0, 0, 0, 0, 0], [0, 1, 0, 1, 0, 1, 0, 1], [0x80, 2, 0xFF, 0, 0, 0, 0, 0]):
        for scores in ([5] * 8, [1, 2, 3, 4, 5, 6, 7, 8], [8, 7, 6, 5, 4, 3, 2, 1], [-1, -2, -3, -1, 0, -9, 0, -1], [0x80000000, 0x7FFFFFFF, 0, 1, 2, 3, 4, 5],
                       [3, 9, 9, 2, 9, 1, 0, 9]):
            c = mk_find(random.Random(len(out)))
            for i in range(8):
                c["init"][0x14 + 0x18 * i + 0x10] = scores[i] & MASK
                c["init"][0x14 + 0x18 * i + 0x14] = valids[i]
            out.append(c)
    return out


def mk_precip(r):
    c = base_case(r, "weather", init={0x30: rword(r), 0x54: r.choice([0, 0, 1, 7, 0x80000000, r.getrandbits(32)])},
                  args=[r.choice([-1, 0, 1, 2, 3, 4, 100, 0x7FFFFFFF, 0x80000000, -2, r.randrange(-5, 8)])])
    c["q"] = {"handle": [r.choice([0, 1, 5, 0x80000000, r.getrandbits(32)])]}
    return c


def precip_edges():
    out = []
    for typ in (-1, 0, 1, 2, 3, 4, 0x7FFFFFFF, 0x80000000):
        for f54 in (0, 1, 0x80000000):
            for handle in (0, 9):
                c = mk_precip(random.Random(len(out)))
                c["init"][0x54] = f54
                c["args"] = [typ]
                c["q"]["handle"] = [handle]
                out.append(c)
    return out


def rnd(fn):
    return fn


TESTS = [
    Spec("CResearchLab::Init", 0, INIT, "none", init_edges, mk_init),
    Spec("CResearchLab::SetupProjects", 1, SETUPP, "none", setupp_edges, mk_lab),
    Spec("CResearchLab::SetupCategories", 2, SETUPC, "none", setupc_edges, mk_lab),
    Spec("CParkAnalyser::BuiltSomething", 3, BUILT, "none", built_edges, mk_built),
    Spec("CParkAnalyser::NumberOfStaffType", 4, STAFF, "int", staff_edges, mk_staff),
    Spec("CBankAccount::MakeWithdrawal", 5, WITHDRAW, "none", withdraw_edges, mk_withdraw),
    Spec("CChallengeHQ::AddChallenge", 6, ADDCH, "none", add_edges, mk_add),
    Spec("CChallengeHQ::ClearInvalidFollowups", 7, CLEARF, "none", clear_edges, mk_clear),
    Spec("CChallengeHQ::IsSystemOn", 8, ISON, "bool", ison_edges, mk_ison),
    Spec("CChallengeHQ::OfferNextChallenge", 9, OFFER, "none", offer_edges, mk_offer),
    Spec("CAdvisor::IsPlaying", 10, ISPLAY, "bool", play_edges, mk_play),
    Spec("CAdvisor::FindHighestScoringSlotInQueue", 11, FINDHI, "int", find_edges, mk_find),
    Spec("CWeather::SetPrecipitationType", 12, PRECIP, "none", precip_edges, mk_precip),
]
