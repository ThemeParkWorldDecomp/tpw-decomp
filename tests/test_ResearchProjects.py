"""w211: the research lab's projects, efforts and outputs, original code on one side and src/game/buildings/ResearchLab.cpp on the other
(tests/unit_ResearchProjects.cpp).

Functions (main app): CResearchLab::FinalDestroy 0x10121b7c, ContributeResearch 0x101222ac, NextProject 0x101223e8, PreviousProject 0x101225f0, GetEffort
0x101227fc, GetTotalEffortFromActiveCategories 0x1012283c, SetEffort 0x10122918, SetWorkLoad 0x10122998, GetResearchSpeed 0x10122a04, GetCurrentProject
0x10122af4, GetEstimatedCostForCurrentProject 0x10122b7c, GetRemainingPoints 0x10122c94, GetNumberLeftToResearch 0x10122d3c, UpdateHighestGroup
0x10122e28, IsAllResearchDone 0x101234d0, CheatCompleteAll 0x10123578, CheatCompleteOnlyPaidFor 0x10123644, IsCategoryActive 0x10123768,
UpdateActiveStatus 0x10123810, CalculateOutputs 0x10123998, GetResearchOutputFromCategory 0x10123a5c; CResearchProject::SetupProject 0x10123d7c,
SetupStatUpgrade 0x10123e6c, ContributeResearch 0x10123fc8, IsProjectCompleted 0x101241ac and GetRemainingPoints 0x1012420c.

A case is a lab and a call. The lab (its 0x100 projects, five categories and four words), the eight object control records, the eight ride records
and the two world tails are built twice from ONE description: in emulator memory at the Mac's layout (big-endian) for the original, and in the host
library's memory (little-endian, the ride records with the host's own value offsets) for the translation. Every callee outside the unit is a fake that
records the same event on both sides and answers from the case: the ride database, both object control getters (they answer the record of the id),
HasBeenBought, GetWorld (a fake on both sides, not run as the original), the park analyser's const getter and its two researcher abilities, the
advisor, its request and its queue, the two messages' constructors, PostMessage, the nothrow operator new with the message centre's header array, and
CThing::FinalDestroy. The helper 0x10237cd8 (__cvt_fp2unsigned) runs as the original.

The world of a case: the lab block (0x40 guard bytes each side, the fill uniform per aligned word with every byte at least 0x80, so that a read of an
undescribed word gives the same number on both sides and a read of an index word from it is negative), the global world and game world (different
tails half the time; the researcher list's head halfword at world + 0x1da742 is set per side, the other tail bytes are random), the balance words
318 to 326 (the level thresholds), the message centre (a block or absent, operator new answering a block or NULL) and the answers of the queues
(bought, maxab, curab, newnull). What is compared: the answer (the halfword written through the hidden pointer for GetCurrentProject, with the rest
of its block untouched), every field of the 0x100 projects, five categories and four lab words, the object control records, the ride records, every
guard and gap, the message centre global with the allocation count, and the event log.

No fused operations (the asm has only lfs, lfd, fsubs, fmuls, fdivs, fadds, fcmpo; the one fctiwz is inside the helper 0x10237cd8 and only sees
values in range), so no HardwareRounding. Not tested: a category argument outside -1 to 6 (the pure readers; the writers take 0 to 4, NextProject and
PreviousProject also 5), GetEffort's group beyond 6, a project index or mBestProject beyond 255, a category level above 8 (the balance words end at
326), a statistics upgrade's level above 2 (the ride record has no more slots), the low half of a tag register (it is undefined on the Mac), and the
messages' destructors (silent on both sides).
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ResearchProjects"
PROGRAM = "SimTheme Park"
MASK = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF
POISON = b"\xa5"

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
FINAL, CONTRIB, NEXT, PREV, GETEFF, TOTAL, SETEFF, SETLOAD = 0x10121B7C, 0x101222AC, 0x101223E8, 0x101225F0, 0x101227FC, 0x1012283C, 0x10122918, 0x10122998
SPEED, CURRENT, COST, REMAIN, LEFT, HIGHEST, ALLDONE = 0x10122A04, 0x10122AF4, 0x10122B7C, 0x10122C94, 0x10122D3C, 0x10122E28, 0x101234D0
CHEATALL, CHEATPAID, ACTIVE, UPDATE, CALC, OUTPUT = 0x10123578, 0x10123644, 0x10123768, 0x10123810, 0x10123998, 0x10123A5C
SETUP, SETUPSTAT, PCONTRIB, PDONE, PREMAIN = 0x10123D7C, 0x10123E6C, 0x10123FC8, 0x101241AC, 0x1012420C
SIZES = {FINAL: 36, CONTRIB: 260, NEXT: 460, PREV: 460, GETEFF: 16, TOTAL: 148, SETEFF: 76, SETLOAD: 56, SPEED: 172, CURRENT: 68, COST: 196,
         REMAIN: 100, LEFT: 160, HIGHEST: 728, ALLDONE: 112, CHEATALL: 148, CHEATPAID: 228, ACTIVE: 100, UPDATE: 336, CALC: 140, OUTPUT: 16,
         SETUP: 176, SETUPSTAT: 280, PCONTRIB: 296, PDONE: 36, PREMAIN: 88}
OWN = set(SIZES)
CVT = 0x10237CD8     # __cvt_fp2unsigned, the original runs under the unit

# ---- the callees faked on both sides
GETRIDE, GETCTL, GETCTLC, BOUGHT, GETWORLD, GETAN, MAXAB, CURAB = 0x10154868, 0x100FA0B8, 0x100FA1B8, 0x100FBFEC, 0x1014205C, 0x1013EB38, 0x100E8ED8, 0x100E8DD8
GETADV, REQCTOR, QUEUE, MSGEVENT, MSGDONE, POST, FINALD = 0x1013EAB0, 0x1000DEB4, 0x1000AE48, 0x10151DF8, 0x10151CFC, 0x1015256C, 0x1012E9F8

# ---- the Mac's addresses of the globals
G_WORLD, G_GAME, G_CENTRE = 0x1037EC0C, 0x1037EBFC, 0x1037F6C4
RIDEDB, MAIN, NOTHING = 0x1037F6C8, 0x102B4568, 0x102A10F8
TAIL0, HEAD_AT = 0x1DA6F0, 0x52
MANAGER_AT, TICKETS_AT = 0x1D943C, 0x2C0
FIRST_NEW, CENTRE_SIZE = 400, 0x1D0

# labels: 1, 2 the worlds, 21, 22 their control managers, 23, 24 their golden tickets, 4 the lab, 6 the message centre, 7 the advisor, 30 + k the
# analysers, 60 the ride database
L_WORLD, L_MANAGER, L_TICKETS, L_LAB, L_CENTRE, L_ADVISOR, L_AN, L_RIDEDB = 1, 21, 23, 4, 6, 7, 30, 60

# ---- the layouts
LAB_AT, LAB_SIZE = 0x40, 0x146C
LAB_BLOCK = LAB_AT + LAB_SIZE + 0x40
NPROJ, PROJ0, PSIZE = 0x100, 0xC, 0x14
CAT0, CSIZE = 0x140C, 0x10
W_COUNT, W_LOAD, W_BEST, W_FLAG = 0x145C, 0x1460, 0x1464, 0x1468
NREC, CTL_AT, CTL_SIZE = 8, 0x40, 0x20
CTL_BLOCK = CTL_AT + NREC * CTL_SIZE + 0x40
RIDE_BLOCK = 0x1000
RES_AT, RES_BLOCK = 0x40, 0x40 + 0x10 + 0x40
# the ride record's value slots the unit reads: golden ticket cost, group, category, the research cost of the three levels, the upgrade cost of the
# three levels, the id of the ride type an upgrade belongs to
RIDE_SLOTS = (46, 91, 92, 93, 109, 125, 107, 123, 139, 158)
S_GOLD, S_GROUP, S_CATEGORY, S_COST0, S_UPCOST0, S_UPGRADES = 46, 91, 92, 93, 107, 158

# what tpw_rpj_layout must answer (a header that moved shows up here): the lab, a project, a category, an object control, the world's head, the centre
HOST_LAYOUT = [0x146C, 0xC, 0x140C, 0x145C, 0x1460, 0x1464, 0x1468,
               0x14, 0, 4, 8, 9, 0xC, 0x10,
               0x10, 0, 4, 8, 0xC,
               0x20, 0, 4, 0x10, 0x14,
               0x1DA742, CENTRE_SIZE]

# the pool of object ids (record i of a case has id ids[i], a permutation of it)
POOL = [0x0001, 0x0002, 0x1234, 0x7FFF, 0x8001, 0xFFFF, 0x0100, 0x00FF]

# field layouts: (offset, width). A project: id, category, group, level, score, progress; a category: effort, output, current project, level
PROJ_FIELDS = ((0, 2), (4, 4), (8, 1), (9, 1), (0xC, 4), (0x10, 4))
CAT_FIELDS = ((0, 4), (4, 4), (8, 4), (0xC, 1))
CTL_REL = ((0, 2), (4, 4), (0x10, 1), (0x14, 4))      # type, upgrade cost, available, upgrades available


def s32(v):
    return T.s32(v)


def s64(v):
    v &= M64
    return v - (1 << 64) if v & (1 << 63) else v


def qn(q):
    return {k: [s64(v) for v in vs] for k, vs in q.items()}


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    """A float's bits with every NaN made one (the processors make different NaNs)."""
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def make_fill(seed, size):
    return bytearray(random.Random(seed).randbytes(size))


def uniform_fill(seed, size):
    """Every aligned 4-byte word has one byte value (0x80 to 0xFF) four times: any read of it is the same number on both sides."""
    rr = random.Random(seed)
    out = bytearray()
    for _ in range(size // 4):
        out += bytes([rr.randrange(0x80, 0x100)]) * 4
    return out


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


def setup(runner):
    T.install_std_stubs(runner.emu)
    assert runner.emu.read(NOTHING, 2) == b"\0\0", "the halfword at 0x102a10f8 is not the original's 0"


# ---------------------------------------------------------------------------------------------------------------- the layouts of the blocks

LAB_REL = []
for _k in range(NPROJ):
    LAB_REL += [(PROJ0 + PSIZE * _k + o, w) for o, w in PROJ_FIELDS]
for _g in range(5):
    LAB_REL += [(CAT0 + CSIZE * _g + o, w) for o, w in CAT_FIELDS]
LAB_REL += [(W_COUNT, 4), (W_LOAD, 4), (W_BEST, 4), (W_FLAG, 1)]
LAB_FIELDS = [(LAB_AT + o, w) for o, w in LAB_REL]
LAB_GAPS = gaps(LAB_FIELDS, LAB_BLOCK)
FLOAT_IDX = [6 * k + j for k in range(NPROJ) for j in (4, 5)]
CTL_FIELDS = [(CTL_AT + CTL_SIZE * i + o, w) for i in range(NREC) for o, w in CTL_REL]
CTL_GAPS = gaps(CTL_FIELDS, CTL_BLOCK)
HOST_RIDE = {}
RIDE_GAPS = {}


def ride_fields(little):
    if little:
        return [(HOST_RIDE["id"], 2)] + [(HOST_RIDE[s], 4) for s in RIDE_SLOTS]
    return [(0, 2)] + [(0xC + 4 * s, 4) for s in RIDE_SLOTS]


def ride_gaps(little):
    if little not in RIDE_GAPS:
        RIDE_GAPS[little] = gaps(ride_fields(little), RIDE_BLOCK)
    return RIDE_GAPS[little]


def lab_init(c):
    init = {}
    for k, (ident, cat, group, level, score, progress) in enumerate(c["proj"]):
        b = LAB_AT + PROJ0 + PSIZE * k
        init[b], init[b + 4], init[b + 8], init[b + 9], init[b + 0xC], init[b + 0x10] = ident, cat, group, level, score, progress
    for g, (effort, output, cur, level) in enumerate(c["cats"]):
        b = LAB_AT + CAT0 + CSIZE * g
        init[b], init[b + 4], init[b + 8], init[b + 0xC] = effort, output, cur, level
    init[LAB_AT + W_COUNT], init[LAB_AT + W_LOAD], init[LAB_AT + W_BEST], init[LAB_AT + W_FLAG] = c["count"], c["load"], c["best"], c["flag"]
    return init


def lab_data(c, little):
    fill = uniform_fill(c["seed"], LAB_BLOCK)
    return fill, sided(fill, LAB_FIELDS, lab_init(c), little)


def lab_view(buf, little):
    v = decode(buf, LAB_FIELDS, little)
    for i in FLOAT_IDX:
        v[i] = canon_f(v[i])
    base = 6 * NPROJ
    return {"proj": [v[6 * k:6 * k + 6] for k in range(NPROJ)], "cats": [v[base + 4 * g:base + 4 * g + 4] for g in range(5)], "words": v[base + 20:base + 24]}


def ctl_data(c, little):
    fill = make_fill(c["seed"] + 7, CTL_BLOCK)
    init = {}
    for i, (kind, cost, avail, upg) in enumerate(c["recs"]):
        b = CTL_AT + CTL_SIZE * i
        init[b], init[b + 4], init[b + 0x10], init[b + 0x14] = kind, cost, avail, upg
    return fill, sided(fill, CTL_FIELDS, init, little)


def ride_data(c, k, little):
    fill = make_fill(c["seed"] + 100 + k, RIDE_BLOCK)
    fields = ride_fields(little)
    ride = c["rides"][k]
    init = {fields[0][0]: ride["id"]}
    for (o, w), s in zip(fields[1:], RIDE_SLOTS):
        init[o] = ride[s]
    return fill, sided(fill, fields, init, little)


def tail_bytes(c, k, little):
    rr = random.Random(c["seed"] * 3 + k)
    words = [rr.getrandbits(32) for _ in range(32)]
    buf = bytearray(struct.pack(("<" if little else ">") + "32I", *words))
    buf[HEAD_AT:HEAD_AT + 2] = struct.pack("<H" if little else ">H", c["heads"][k] & 0xFFFF)
    return bytes(buf)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC side of a case

class Ppc:
    pass


def build_ppc(e, c):
    P = Ppc()
    L = CL.PpcLog({}, qn(c["q"]), FIRST_NEW)
    e.cl = L
    L.last_request = None
    L.last_message = None
    P.L = L
    P.tails = [e.alloc(0x80, tail_bytes(c, k, False)) for k in range(2)]
    P.lab_fill, data = lab_data(c, False)
    P.lab_block = e.alloc(len(data), data)
    P.lab = P.lab_block + LAB_AT
    P.ctl_fill, data = ctl_data(c, False)
    P.ctl_block = e.alloc(len(data), data)
    P.ride_fill, P.rides = [], []
    for k in range(NREC):
        fill, data = ride_data(c, k, False)
        P.ride_fill.append(fill)
        P.rides.append(e.alloc(RIDE_BLOCK, data))
    P.centre = e.alloc(CENTRE_SIZE, POISON * CENTRE_SIZE)
    P.an = [e.alloc(0x40, POISON * 0x40) for _ in range(2)]
    P.advisor = e.alloc(0x40, POISON * 0x40)
    P.result = e.alloc(RES_BLOCK, POISON * RES_BLOCK)
    for k in range(2):
        world = P.tails[k] - TAIL0
        L.labels[world] = L_WORLD + k
        L.labels[world + MANAGER_AT] = L_MANAGER + k
        L.labels[world + TICKETS_AT] = L_TICKETS + k
        L.labels[P.an[k]] = L_AN + k
    L.labels[P.lab] = L_LAB
    L.labels[P.centre] = L_CENTRE
    L.labels[P.advisor] = L_ADVISOR
    L.labels[RIDEDB] = L_RIDEDB
    w32(e, G_WORLD, P.tails[c["world"]] - TAIL0)
    w32(e, G_GAME, P.tails[c["game"]] - TAIL0)
    w32(e, G_CENTRE, P.centre if c["centre"] else 0)
    for slot, v in c["mb"].items():
        w32(e, MAIN + 8 + 4 * slot, v)
    P.allocations = []
    return P


def ppc_stubs(e, P, c):
    L = P.L
    S = CL.Stubs()
    S.real = set(OWN) | {CVT}
    I = S.internal

    def record(ident):
        """The record of an id; the first one (with an event) for an id that no record has."""
        for i, v in enumerate(c["ids"]):
            if v == ident:
                return i
        L.rec("NoRecord", [ident])
        return 0

    def ride(emu):
        ident = (emu.r(4) >> 16) & 0xFFFF
        L.rec("GetRideInfo", [L.lab(emu.r(3)), ident])
        emu.set_r(3, P.rides[record(ident)])

    def control(name):
        def stub(emu):
            ident = (emu.r(4) >> 16) & 0xFFFF
            L.rec(name, [L.lab(emu.r(3)), ident, s32(emu.r(5))])
            emu.set_r(3, P.ctl_block + CTL_AT + CTL_SIZE * record(ident))
        return stub

    def bought(emu):
        L.rec("HasBeenBought", [L.lab(emu.r(3)), (emu.r(4) >> 16) & 0xFFFF])
        emu.set_r(3, L.ret("bought") & 0xFF)

    def world(emu):
        L.rec("GetWorld", [L.lab(emu.r(3))])
        emu.set_r(3, e.u32(G_WORLD))

    def analyser(emu):
        L.rec("GetParkAnalyser.const", [L.lab(emu.r(3))])
        emu.set_r(3, P.an[0 if emu.r(3) == P.tails[0] - TAIL0 else 1])

    def ability(name, key):
        def stub(emu):
            L.rec(name, [L.lab(emu.r(3))])
            emu.set_r(3, T.u32(L.ret(key)))
        return stub

    def advisor(emu):
        L.rec("CWorld::GetAdvisor", [L.lab(emu.r(3))])
        emu.set_r(3, P.advisor)

    def req_ctor(emu):
        L.last_request = emu.r(3)
        L.rec("CAdvisorRequest", [s32(emu.r(4)), (emu.r(5) >> 16) & 0xFFFF, emu.r(6) & MASK, s32(emu.r(7)), emu.r(8) & 0xFF, s32(emu.r(9))])

    def queue(emu):
        L.rec("QueueRequest", [L.lab(emu.r(3)), 1 if emu.r(4) == L.last_request else 0])
        emu.set_r(3, 0)

    def msg_event(emu):
        L.last_message = emu.r(3)
        L.rec("CMsgEvent", [s32(emu.r(4))])

    def msg_done(emu):
        L.last_message = emu.r(3)
        L.rec("CMsgResearchCompleted", [(emu.r(4) >> 16) & 0xFFFF, s32(emu.r(5))])

    def post(emu):
        L.rec("PostMessage", [L.lab(emu.r(3)), 1 if emu.r(4) == L.last_message else 0])
        emu.set_r(3, 1)

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
    I(GETRIDE, ride)
    I(GETCTL, control("GetObjectControl"))
    I(GETCTLC, control("GetObjectControl.const"))
    I(BOUGHT, bought)
    I(GETWORLD, world)
    I(GETAN, analyser)
    I(MAXAB, ability("MaxResearcherAbility", "maxab"))
    I(CURAB, ability("CurrentResearcherAbility", "curab"))
    I(GETADV, advisor)
    I(REQCTOR, req_ctor)
    I(QUEUE, queue)
    I(MSGEVENT, msg_event)
    I(MSGDONE, msg_done)
    I(POST, post)
    I(FINALD, lambda emu: L.rec("CThing::FinalDestroy", [L.lab(emu.r(3))]))
    S.imp("__nw__FUl", op_new)
    return S


RET = {"none": lambda v: 0, "u32": lambda v: v & MASK, "s32": lambda v: s32(v), "bool": lambda v: int(v & 0xFF != 0), "byte": lambda v: v & 0xFF,
       "half": lambda v: v & 0xFFFF}


def run_ppc(e, c, spec):
    P = build_ppc(e, c)
    S = ppc_stubs(e, P, c)
    saved = S.activate(e)
    try:
        T.stub_cb(e, "__construct_array", P.construct)
        r3 = T.call_ppc(e, spec.entry, spec.ppc_args(P, c), max_insns=5_000_000, timeout_us=60_000_000)[0]
        out = collect_ppc(e, P, c, spec)
    finally:
        CL.Stubs.deactivate(e, saved)
    if spec.ret == "half":
        r3 = struct.unpack(">H", e.read(P.result + RES_AT, 2))[0]
    out["ret"] = RET[spec.ret](r3)
    return out


def collect_ppc(e, P, c, spec):
    buf = e.read(P.lab_block, LAB_BLOCK)
    out = lab_view(buf, False)
    bad = moved(buf, P.lab_fill, LAB_GAPS)
    ctl = e.read(P.ctl_block, CTL_BLOCK)
    out["ctl"] = decode(ctl, CTL_FIELDS, False)
    bad += moved(ctl, P.ctl_fill, CTL_GAPS)
    out["rides"] = []
    for k in range(NREC):
        b = e.read(P.rides[k], RIDE_BLOCK)
        out["rides"].append(decode(b, ride_fields(False), False))
        bad += moved(b, P.ride_fill[k], ride_gaps(False))
    for k in range(2):
        bad += moved(e.read(P.tails[k], 0x80), tail_bytes(c, k, False), [(0, 0x80)])
    bad += moved(e.read(P.result, RES_BLOCK), POISON * RES_BLOCK, [(0, RES_AT), (RES_AT + 2, RES_BLOCK)])
    out["untouched"] = bad
    out["globals"] = [P.L.lab(e.u32(G_CENTRE)), len(P.allocations)]
    out["log"] = P.L.log
    return out


# ---------------------------------------------------------------------------------------------------------------- the host side of a case

def api(lib):
    if getattr(lib, "rpj_ready", False):
        return lib
    i32, i64 = C.c_int32, C.c_int64
    lib.tpw_rpj_addr.restype = C.c_void_p
    lib.tpw_rpj_addr.argtypes = [i32]
    lib.tpw_rpj_world.restype = C.c_void_p
    lib.tpw_rpj_world.argtypes = [i32]
    lib.tpw_rpj_prepare.argtypes = [i32] * 3
    lib.tpw_rpj_ids.argtypes = [C.POINTER(i32)]
    lib.tpw_rpj_balance.argtypes = [i32, i32]
    lib.tpw_rpj_ride_offset.argtypes = [i32]
    lib.tpw_rpj_ride_offset.restype = i32
    lib.tpw_rpj_layout.argtypes = [C.POINTER(i64)]
    lib.tpw_rpj_layout.restype = i32
    lib.tpw_rpj_globals.argtypes = [C.POINTER(i64)]
    lib.tpw_rpj_run.argtypes = [i32, i64, i64, i32]
    lib.tpw_rpj_run.restype = i64
    out = (i64 * 40)()
    n = lib.tpw_rpj_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    HOST_RIDE["id"] = lib.tpw_rpj_ride_offset(-1)
    for s in RIDE_SLOTS:
        HOST_RIDE[s] = lib.tpw_rpj_ride_offset(s)
    lib.rpj_ready = True
    return lib


def host_run(lib, c, spec):
    api(lib)
    at = lib.tpw_rpj_addr
    for k in range(2):
        C.memmove(at(k), tail_bytes(c, k, True), 0x80)
    lab_fill, data = lab_data(c, True)
    C.memmove(at(2), data, len(data))
    ctl_fill, data = ctl_data(c, True)
    C.memmove(at(3), data, len(data))
    ride_fill = []
    for k in range(NREC):
        fill, data = ride_data(c, k, True)
        ride_fill.append(fill)
        C.memmove(at(4 + k), data, RIDE_BLOCK)
    C.memset(at(12), 0xA5, CENTRE_SIZE)
    C.memset(at(13), 0xA5, 0x40)
    C.memset(at(14), 0xA5, 0x40)
    C.memset(at(15), 0xA5, 0x40)
    lib.tpw_rpj_prepare(c["world"], c["game"], c["centre"])
    lib.tpw_rpj_ids((C.c_int32 * NREC)(*c["ids"]))
    for slot, v in c["mb"].items():
        lib.tpw_rpj_balance(slot, s32(v))
    fixed = {L_LAB: at(2) + LAB_AT, L_CENTRE: at(12), L_ADVISOR: at(15), L_RIDEDB: at(16)}
    for k in range(2):
        world = lib.tpw_rpj_world(k)
        fixed[L_WORLD + k] = world
        fixed[L_MANAGER + k] = world + MANAGER_AT
        fixed[L_TICKETS + k] = world + TICKETS_AT
        fixed[L_AN + k] = at(13 + k)
    CL.host_begin(lib, fixed, qn(c["q"]), FIRST_NEW)
    try:
        ret = lib.tpw_rpj_run(spec.which, c["a"], c["b"], c["k"])
    finally:
        log = CL.host_end(lib)
    state = (C.c_int64 * 2)()
    lib.tpw_rpj_globals(state)
    buf = C.string_at(at(2), LAB_BLOCK)
    out = lab_view(buf, True)
    bad = moved(buf, lab_fill, LAB_GAPS)
    ctl = C.string_at(at(3), CTL_BLOCK)
    out["ctl"] = decode(ctl, CTL_FIELDS, True)
    bad += moved(ctl, ctl_fill, CTL_GAPS)
    out["rides"] = []
    for k in range(NREC):
        b = C.string_at(at(4 + k), RIDE_BLOCK)
        out["rides"].append(decode(b, ride_fields(True), True))
        bad += moved(b, ride_fill[k], ride_gaps(True))
    for k in range(2):
        bad += moved(C.string_at(at(k), 0x80), tail_bytes(c, k, True), [(0, 0x80)])
    out["untouched"] = bad
    out["globals"] = list(state)
    out["ret"] = RET[spec.ret](ret & MASK)
    out["log"] = log
    return out


class Spec(FnTest):
    """A case builds both sides from the same description; the observed state and the call logs must agree."""

    def __init__(self, name, which, entry, ret, ppc_args, make, edges):
        FnTest.__init__(self, name, None, None, edge=edges, rnd=make)
        self.which, self.entry, self.ret, self.ppc_args = which, entry, ret, ppc_args

    def run_case(self, runner, case):
        runner.emu.reset_scratch()
        api(runner.lib)
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


# ---------------------------------------------------------------------------------------------------------------- value pools

SPECIAL_F = [fb(x) for x in (0.0, -0.0, 1.0, -1.0, 0.1, 100.0, 1e10, -1e10, float("inf"), float("-inf"), 1e-30)] + [0x7FC00000]
SCORE_F = [fb(x) for x in (0.1, 0.5, 1.0, 1.5, 99.0, 100.0, 12345.678, 16777216.0, 3.0e38)]
TAME_SCORE_F = [fb(x) for x in (0.1, 0.5, 1.0, 1.5, 99.0, 100.0, 12345.678, 16777216.0)]
TAME_F = [fb(x) for x in (0.0, -0.0, 1.0, -1.0, 0.1, 100.0, 1e-30)]
BEST_F = [fb(x) for x in (1.0, 0.5, 0.0, -1.0, 2.0, 50.0, 100.0, 1e-30, 1e30, float("inf"), float("-inf"))] + [0x3F800001, 0x7FC00000, 0x3F7FFFFF]
THRESH = [0, 1, 10, 25, 33, 34, 50, 51, 66, 67, 75, 99, 100, 101, 0x80000000, 0xFFFFFFFF]
EFFORTS = [0, 0, 1, 5, 10, 20, 25, 33, 50, 99, 100, 101, 255, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
LOADS = [0, 1, 50, 99, 100, 101, 0xFFFFFFFF, 0x80000000]
ABILITIES = [0, 1, 10, 50, 99, 100, 101, 1000, -1, -100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 12345678]
POINTS = [0, 1, 7, 100, 12345, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
NPS = [0, 1, 1, 2, 2, 3, 3, 4, 5, 6, 8, 12, 16]
INTS = [0, 1, -1, 2, 3, 4, 5, 10, 100, 1000, 16777216, 16777217, 0x7FFFFFFF, -0x80000000, -2, 99999]


def nudge(bits, d):
    """The next float up or down, for a finite positive one."""
    return bits + d if 0 < bits < 0x7F800000 else bits


def rpair(r, tame=False):
    """A project's (score, progress) bits: fresh, exactly done, a step either side of done, partial, over, special. `tame` keeps both finite and
    small: GetRemainingPoints converts 100 * (score - progress) / best with the helper 0x10237cd8, and Unicorn's fctiwz saturates a number out
    of range (or a NaN) differently from the hardware, so the cases that reach it stay in range."""
    special = TAME_F if tame else SPECIAL_F
    score = fb(float(r.randrange(1, 2001))) if r.random() < 0.75 else r.choice((TAME_SCORE_F if tame else SCORE_F + SPECIAL_F))
    k = r.random()
    if k < 0.20:
        progress = 0
    elif k < 0.40:
        progress = score
    elif k < 0.48:
        progress = nudge(score, 1)
    elif k < 0.56:
        progress = nudge(score, -1)
    elif k < 0.70:
        progress = fb(bitsf(score) * r.uniform(0, 1)) if bitsf(score) == bitsf(score) else score
    elif k < 0.80:
        progress = fb(bitsf(score) + r.uniform(0, 500)) if bitsf(score) == bitsf(score) else score
    else:
        progress = r.choice(special)
    return score, progress


def pick_id(r, ids):
    if r.random() < 0.9:
        return r.choice(ids)
    return r.choice([0, 0x4242, r.getrandbits(16)])


def partial(r, score):
    """A progress short of the score (0 when the score is not an ordinary number)."""
    s = bitsf(score)
    return fb(s * r.uniform(0, 0.99)) if s == s and abs(s) < 1e30 else 0


def rproject(r, ids, focus, level_max, tame=False, keen=False):
    """One project. `keen` makes the project one the next/previous search can pick: the focus category, a group the category's level reaches, not done."""
    score, progress = rpair(r, tame)
    if keen:
        cat = focus if r.random() < 0.9 else r.choice([0, 1, 2, 3, 4, 5, -1])
        group = r.choice([0, 0, 1, 1, 2, 3, 4, 5, 8, 0x80])
        if r.random() < 0.6:
            progress = partial(r, score)
    else:
        cat = focus if r.random() < 0.6 else r.choice([0, 1, 2, 3, 4, 5, -1, 6, 0x100000, -0x7FFFFFFF])
        group = r.choice([0, 0, 1, 2, 3, 4, 0x7F, 0x80, 0xFF])
    level = r.choice([0, 1, 2]) if level_max == 2 else r.choice([0, 1, 2, 3, 0xFF])
    return (pick_id(r, ids), cat & MASK, group, level, score, progress)


def rcats(r, np, active):
    cats = []
    for _ in range(5):
        if np and r.random() < active:
            cur = r.randrange(np)
        else:
            cur = r.choice([-1, -1, -2, -0x80000000, r.randrange(0, 256)])
        effort = r.choice(EFFORTS) if r.random() < 0.8 else r.randrange(0, 101)
        output = r.choice([0, 25, 100, r.randrange(0, 101), r.getrandbits(32)])
        cats.append((effort, output, cur & MASK, r.choice([0, 1, 2, 3, 4, 7, 8])))
    return cats


def rride(r, ids):
    cost = lambda: r.choice([0, 0, 1, 10, 100, 1000, 16777217, 0x7FFFFFFF, -1, -0x80000000, r.randrange(1, 5000)])
    ride = {"id": r.choice(ids) if r.random() < 0.9 else r.choice(POOL)}
    ride[S_GOLD] = r.choice([0, 0, 1, 5, -1, 0x7FFFFFFF, -0x80000000])
    ride[S_GROUP] = r.choice([0, 1, 2, 3, 4, 0xFF, 0x100, 0x1FF, -1])
    ride[S_CATEGORY] = r.choice([0, 1, 2, 3, 4, 4, 4, 5, -1, -0x7FFFFFFC])
    for level in range(3):
        ride[S_COST0 + 16 * level] = cost()
        ride[S_UPCOST0 + 16 * level] = r.choice([0, 1, 100, -1, 0x7FFFFFFF, -0x80000000, r.getrandbits(32)])
    ride[S_UPGRADES] = r.choice([0, 0, r.choice(ids), r.choice(ids) | 0x10000, 0x10000, r.choice(ids) | (r.getrandbits(16) << 16), 0xFFFF0000])
    return ride


def rrec(r):
    return (r.getrandbits(16), r.choice([0, -1, 100, 0x7FFFFFFF, -0x80000000, r.getrandbits(32)]), r.choice([0, 1, 0, 1, 2, 0xFF]),
            r.choice([-1, -1, 0, 1, 2, 3, 5]))


def rbest(r, np):
    k = r.random()
    if k < 0.1:
        return -1
    if k < 0.7 and np:
        return r.randrange(np)
    return r.randrange(256)


def rqueues(r):
    return {"bought": [r.choice([0, 1, 1, 2, 0xFF, 0x100]) for _ in range(300)], "maxab": [r.choice(ABILITIES) for _ in range(4)],
            "curab": [r.choice(ABILITIES) for _ in range(4)], "newnull": [1 if r.random() < 0.25 else 0 for _ in range(8)]}


def gen(r, np=None, focus=None, level_max=255, active=0.7, count=None, tame=False, keen=False):
    """A whole lab: np described projects (the rest of the 0x100 are the uniform fill), the categories, the words, the records, the rides."""
    ids = r.sample(POOL, NREC)
    if np is None:
        np = r.choice(NPS)
    if focus is None:
        focus = r.randrange(5)
    proj = [rproject(r, ids, focus, level_max, tame, keen) for _ in range(np)]
    if count is None:
        count = np if r.random() < 0.85 else r.randrange(0, NPROJ + 1)
    return dict(seed=r.getrandbits(32), np=np, proj=proj, cats=rcats(r, np, active), count=count, load=r.choice(LOADS + [r.randrange(0, 101)]),
                best=rbest(r, np), flag=r.choice([0, 1, 2, 0xFF]), ids=ids, recs=[rrec(r) for _ in range(NREC)], rides=[rride(r, ids) for _ in range(NREC)],
                heads=[r.choice([0, 0, 1, 0x8000, 0xFFFF, r.getrandbits(16)]) for _ in range(2)], world=r.randrange(2), game=r.randrange(2),
                centre=r.choice([0, 1, 1]), mb={slot: r.choice(THRESH) for slot in range(318, 327)}, q=rqueues(r), a=0, b=0, k=0)


# ---------------------------------------------------------------------------------------------------------------- the calls

def args_lab(P, c):
    return [P.lab]


def args_lab_a(P, c):
    return [P.lab, c["a"] & MASK]


def args_lab_ab(P, c):
    return [P.lab, c["a"] & MASK, c["b"] & MASK]


def args_hidden(P, c):
    return [P.result + RES_AT, P.lab, c["a"] & MASK]


def proj_of(P, c):
    return P.lab + PROJ0 + PSIZE * c["k"]


def args_proj(P, c):
    return [proj_of(P, c)]


def args_proj_ride(P, c):
    return [proj_of(P, c), P.rides[c["a"]]]


def args_proj_float(P, c):
    return [proj_of(P, c), T.F(bitsf(c["a"]))]


CAT_PURE = [-1, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5, 5, 6]     # the signed category of a reader: 5 and 6 read the words after the categories


def build(r, which, np=None, a=None, b=None, k=None, count=None, fix=None):
    """A case of function `which` (the numbers of tpw_rpj_run); a, b, k override the random arguments; fix(case) edits the finished case."""
    level_max = 2 if which == COSTW else 255
    if which in (NEXTW, PREVW):
        a = r.choice([0, 1, 2, 3, 4, 4, 4, 4, 5]) if a is None else a
        c = gen(r, np, a if a < 5 else r.randrange(5), count=count, keen=True)
    elif which in (SPEEDW, CURRENTW, COSTW, REMAINW, LEFTW, ACTIVEW, OUTPUTW):
        a = r.choice(CAT_PURE) if a is None else a
        c = gen(r, np, a if 0 <= a < 5 else r.randrange(5), level_max, 0.9 if which in (CURRENTW, COSTW, REMAINW) else 0.7, count,
                tame=(which == REMAINW))
        if which == REMAINW and c["np"] and r.random() < 0.85:
            c["best"] = r.randrange(c["np"])
        if which == COSTW and a == 4:
            # the current project of category 4 must be one described (its level is read from it)
            cur = c["cats"][4]
            if c["np"] == 0 or (cur[2] & MASK) >= c["np"]:
                c["cats"][4] = (cur[0], cur[1], r.randrange(c["np"]) if c["np"] else MASK, cur[3])
    elif which == HIGHESTW:
        a = r.randrange(5) if a is None else a
        c = gen(r, np if np is not None else r.choice([1, 2, 3, 4, 6, 8, 12, 16]), a, count=count)
        c["cats"][a] = (c["cats"][a][0], c["cats"][a][1], c["cats"][a][2], r.choice([0, 0, 1, 2, 3, 6, 7, 8]))
        for i in range(c["np"]):
            ident, cat, group, level, score, progress = c["proj"][i]
            c["proj"][i] = (ident, a if r.random() < 0.8 else cat, r.choice([0, 1, 2, 3, 4, 8]), level, score, progress)
        c["count"] = c["np"] if r.random() < 0.9 else c["count"]
        c["mb"] = {slot: r.choice([0, 0, 0, 1, 25, 50, 51, 100, 101, 0xFFFFFFFF]) for slot in range(318, 327)}
    elif which == GETEFFW:
        a = r.randrange(7) if a is None else a
        c = gen(r, np, count=count)
    elif which == SETEFFW:
        a = r.randrange(5) if a is None else a
        b = r.choice(EFFORTS + [r.randrange(0, 151)]) if b is None else b
        c = gen(r, np, count=count)
    elif which == SETLOADW:
        a = r.choice(LOADS + [r.randrange(0, 151), 0x7FFFFFFF]) if a is None else a
        c = gen(r, np, count=count)
    elif which == CONTRIBW:
        a = r.choice(POINTS + [r.randrange(0, 1001)]) if a is None else a
        c = gen(r, np if np is not None else r.choice([1, 2, 3, 4, 6]), active=0.9, count=count)
    elif which in (SETUPW, SETUPSTATW):
        a = r.randrange(NREC) if a is None else a
        c = gen(r, np, count=count)
        c["k"] = r.randrange(c["np"]) if k is None and c["np"] and r.random() < 0.8 else (r.randrange(NPROJ) if k is None else k)
        if which == SETUPSTATW:
            ride = c["rides"][a]
            if r.random() < 0.7:
                ride[S_CATEGORY] = r.choice([0, 1, 2, 3, 5, -1])
    elif which in (PCONTRIBW, PDONEW, PREMAINW):
        c = gen(r, np if np is not None else r.choice([1, 2, 3, 4]), count=count, tame=(which == PREMAINW))
        c["k"] = r.randrange(c["np"]) if k is None else k
        score, progress = bitsf(c["proj"][c["k"]][4]), bitsf(c["proj"][c["k"]][5])
        if which == PCONTRIBW:
            if a is None and r.random() < 0.45 and abs(score) < 1e30 and abs(progress) < 1e30:
                # the points that finish the project, give or take a half
                a = fb(score - progress + r.choice([0.0, 0.0, 0.5, -0.5, 1.0, -1.0, 100.0]))
            a = r.choice(SPECIAL_F + [fb(float(r.randrange(0, 600))), fb(r.uniform(-50, 600)), fb(0.5)]) if a is None else a
        elif which == PREMAINW:
            if a is None and r.random() < 0.55:
                a = fb(max(1.0000001, score * r.uniform(0.3, 2.0)))
            a = r.choice(BEST_F + [fb(r.uniform(1, 3000))]) if a is None else a
        else:
            a = 0
    else:
        c = gen(r, np, count=count)
        a = 0 if a is None else a
    c["a"], c["b"] = a, 0 if b is None else b
    if k is not None:
        c["k"] = k
    if fix:
        fix(c)
    c["which"] = which
    return c


def with_pair(k, score, progress):
    """A fix: project k gets this score and progress."""
    def fix(c):
        e = c["proj"][k]
        c["proj"][k] = (e[0], e[1], e[2], e[3], score, progress)
    return fix


def with_current(g, k, score, progress, j, best):
    """A fix: category g works on project k (this score and progress) and the best project is j with score `best` (j may be k: its score is then best)."""
    def fix(c):
        e = c["proj"][k]
        c["proj"][k] = (e[0], e[1], e[2], e[3], score, progress)
        cat = c["cats"][g]
        c["cats"][g] = (cat[0], cat[1], k, cat[3])
        c["best"] = j
        e = c["proj"][j]
        c["proj"][j] = (e[0], e[1], e[2], e[3], best, e[5])
    return fix


(FINALW, CONTRIBW, NEXTW, PREVW, GETEFFW, TOTALW, SETEFFW, SETLOADW, SPEEDW, CURRENTW, COSTW, REMAINW, LEFTW, HIGHESTW, ALLDONEW, CHEATALLW, CHEATPAIDW,
 ACTIVEW, UPDATEW, CALCW, OUTPUTW, SETUPW, SETUPSTATW, PCONTRIBW, PDONEW, PREMAINW) = range(26)


def make(which):
    return lambda r: build(r, which)


def edges(which, params):
    def produce():
        r = random.Random(7000 + which)
        return [build(r, which, **p) for p in params]
    return produce


CATS = [-1, 0, 1, 2, 3, 4, 5, 6]
SIZES_NP = (0, 1, 3)
BIG = dict(np=NPROJ)
COUNTS = (0, 1, 2, 3, 4, 0x100)       # mProjectCount against the three described projects: none, some, all, and the fill beyond them
PAIRS = [fb(x) for x in (0.0, -0.0, 1.0, -1.0, 100.0, float("inf"), float("-inf"))] + [0x7FC00000, 0x3F800001, 0x3F7FFFFF]
# (score, progress) pairs and best scores around GetRemainingPoints' two compares (0.1f, 1.0f) and its conversion
EDGE_PAIRS = [(fb(0.1), 0), (nudge(fb(0.1), 1), 0), (fb(1.0), 0), (fb(100.0), fb(50.0)), (fb(100.0), fb(100.0)), (fb(100.0), fb(150.0)),
              (fb(100.0), fb(-1.0)), (fb(16777216.0), 0), (fb(2000.0), 0), (fb(1.5), fb(0.5)), (fb(3.0), fb(1.0))]
EDGE_BEST = [fb(1.0), 0x3F800001, fb(2.0), fb(7.0), fb(1000.0)]
TESTS = [
    Spec("CResearchLab::FinalDestroy", FINALW, FINAL, "none", args_lab, make(FINALW), edges(FINALW, [dict(np=n) for n in SIZES_NP])),
    Spec("CResearchLab::ContributeResearch", CONTRIBW, CONTRIB, "none", args_lab_a, make(CONTRIBW),
         edges(CONTRIBW, [dict(np=n, a=p) for n in (0, 1, 3) for p in POINTS] + [dict(np=NPROJ, a=100)])),
    Spec("CResearchLab::NextProject", NEXTW, NEXT, "none", args_lab_a, make(NEXTW),
         edges(NEXTW, [dict(np=n, a=cat) for n in SIZES_NP for cat in range(6)] + [dict(a=cat, **BIG) for cat in (0, 4)]
               + [dict(np=3, a=cat, count=cnt) for cat in (0, 4) for cnt in COUNTS])),
    Spec("CResearchLab::PreviousProject", PREVW, PREV, "none", args_lab_a, make(PREVW),
         edges(PREVW, [dict(np=n, a=cat) for n in SIZES_NP for cat in range(6)] + [dict(a=cat, **BIG) for cat in (0, 4)]
               + [dict(np=3, a=cat, count=cnt) for cat in (0, 4) for cnt in COUNTS])),
    Spec("CResearchLab::GetEffort", GETEFFW, GETEFF, "u32", args_lab_a, make(GETEFFW), edges(GETEFFW, [dict(np=1, a=g) for g in range(7)])),
    Spec("CResearchLab::GetTotalEffortFromActiveCategories", TOTALW, TOTAL, "u32", args_lab, make(TOTALW),
         edges(TOTALW, [dict(np=n) for n in SIZES_NP] + [dict(**BIG)])),
    Spec("CResearchLab::SetEffort", SETEFFW, SETEFF, "u32", args_lab_ab, make(SETEFFW),
         edges(SETEFFW, [dict(np=1, a=g, b=e) for g in range(5) for e in (0, 1, 99, 100, 101, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF)])),
    Spec("CResearchLab::SetWorkLoad", SETLOADW, SETLOAD, "none", args_lab_a, make(SETLOADW),
         edges(SETLOADW, [dict(np=1, a=v) for v in (0, 1, 99, 100, 101, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF)])),
    Spec("CResearchLab::GetResearchSpeed", SPEEDW, SPEED, "u32", args_lab_a, make(SPEEDW), edges(SPEEDW, [dict(np=1, a=cat) for cat in CATS])),
    Spec("CResearchLab::GetCurrentProject", CURRENTW, CURRENT, "half", args_hidden, make(CURRENTW),
         edges(CURRENTW, [dict(np=n, a=cat) for n in (0, 1, 3) for cat in CATS])),
    Spec("CResearchLab::GetEstimatedCostForCurrentProject", COSTW, COST, "s32", args_lab_a, make(COSTW),
         edges(COSTW, [dict(np=n, a=cat) for n in (0, 1, 3) for cat in CATS])),
    Spec("CResearchLab::GetRemainingPoints", REMAINW, REMAIN, "u32", args_lab_a, make(REMAINW),
         edges(REMAINW, [dict(np=n, a=cat) for n in (0, 1, 3) for cat in CATS]
               + [dict(np=3, a=g, fix=with_current(g, 1, s, p, 0, bst)) for g in (0, 4) for s, p in EDGE_PAIRS for bst in EDGE_BEST]
               + [dict(np=3, a=g, fix=with_current(g, 1, s, p, 1, s)) for g in (0, 4) for s, p in EDGE_PAIRS])),
    Spec("CResearchLab::GetNumberLeftToResearch", LEFTW, LEFT, "s32", args_lab_a, make(LEFTW),
         edges(LEFTW, [dict(np=n, a=cat) for n in SIZES_NP for cat in CATS] + [dict(a=cat, **BIG) for cat in (0, 5)]
               + [dict(np=3, a=cat, count=cnt) for cat in (0, 5) for cnt in COUNTS])),
    Spec("CResearchLab::UpdateHighestGroup", HIGHESTW, HIGHEST, "none", args_lab_a, make(HIGHESTW),
         edges(HIGHESTW, [dict(np=n, a=g) for n in (1, 4, 8) for g in range(5)] + [dict(a=g, **BIG) for g in (0, 4)])),
    Spec("CResearchLab::IsAllResearchDone", ALLDONEW, ALLDONE, "bool", args_lab, make(ALLDONEW),
         edges(ALLDONEW, [dict(np=n) for n in SIZES_NP] + [dict(**BIG)] + [dict(np=3, count=cnt) for cnt in COUNTS])),
    Spec("CResearchLab::CheatCompleteAll", CHEATALLW, CHEATALL, "none", args_lab, make(CHEATALLW),
         edges(CHEATALLW, [dict(np=n) for n in SIZES_NP] + [dict(**BIG)] + [dict(np=3, count=cnt) for cnt in COUNTS])),
    Spec("CResearchLab::CheatCompleteOnlyPaidFor", CHEATPAIDW, CHEATPAID, "none", args_lab, make(CHEATPAIDW),
         edges(CHEATPAIDW, [dict(np=n) for n in SIZES_NP] + [dict(**BIG)] + [dict(np=3, count=cnt) for cnt in COUNTS])),
    Spec("CResearchLab::IsCategoryActive", ACTIVEW, ACTIVE, "bool", args_lab_a, make(ACTIVEW), edges(ACTIVEW, [dict(np=n, a=cat) for n in (0, 1, 3) for cat in CATS])),
    Spec("CResearchLab::UpdateActiveStatus", UPDATEW, UPDATE, "none", args_lab, make(UPDATEW), edges(UPDATEW, [dict(np=1) for _ in range(12)])),
    Spec("CResearchLab::CalculateOutputs", CALCW, CALC, "none", args_lab, make(CALCW), edges(CALCW, [dict(np=1) for _ in range(6)])),
    Spec("CResearchLab::GetResearchOutputFromCategory", OUTPUTW, OUTPUT, "u32", args_lab_a, make(OUTPUTW), edges(OUTPUTW, [dict(np=1, a=cat) for cat in CATS])),
    Spec("CResearchProject::SetupProject", SETUPW, SETUP, "byte", args_proj_ride, make(SETUPW),
         edges(SETUPW, [dict(np=2, a=a, k=k) for a in range(4) for k in (0, 1, 255)])),
    Spec("CResearchProject::SetupStatUpgrade", SETUPSTATW, SETUPSTAT, "byte", args_proj_ride, make(SETUPSTATW),
         edges(SETUPSTATW, [dict(np=2, a=a, k=k) for a in range(4) for k in (0, 1, 255)])),
    Spec("CResearchProject::ContributeResearch", PCONTRIBW, PCONTRIB, "none", args_proj_float, make(PCONTRIBW),
         edges(PCONTRIBW, [dict(np=2, a=fb(p), k=k) for p in (0.0, 1.0, 50.0, 1000.0) for k in (0, 1)]
               + [dict(np=1, k=0, a=fb(pt), fix=with_pair(0, fb(100.0), fb(pg))) for pg in (0.0, 99.0, 99.5, 100.0, 101.0) for pt in (0.0, 0.5, 1.0, 100.0, -1.0)]
               + [dict(np=1, k=0, a=x, fix=with_pair(0, fb(100.0), fb(50.0))) for x in (0x7F800000, 0xFF800000, 0x7FC00000)])),
    Spec("CResearchProject::IsProjectCompleted", PDONEW, PDONE, "bool", args_proj, make(PDONEW),
         edges(PDONEW, [dict(np=3, k=k) for k in (0, 1, 2)] + [dict(np=1, k=0, fix=with_pair(0, s, p)) for s in PAIRS for p in PAIRS])),
    Spec("CResearchProject::GetRemainingPoints", PREMAINW, PREMAIN, "u32", args_proj_float, make(PREMAINW),
         edges(PREMAINW, [dict(np=3, a=b, k=k) for b in BEST_F for k in (0, 1)]
               + [dict(np=1, a=bst, k=0, fix=with_pair(0, s, p)) for s, p in EDGE_PAIRS for bst in EDGE_BEST])),
]
