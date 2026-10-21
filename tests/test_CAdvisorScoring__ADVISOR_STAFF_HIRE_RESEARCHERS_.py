"""Unit v5971, the advisor scorers for opening and closing the park, hiring and training researchers, mechanic and handyman coverage,
and GetHighestScoringFromNext, against libtpw_unit_CAdvisorScoring__ADVISOR_STAFF_HIRE_RESEARCHERS_.dylib
(tests/unit_CAdvisorScoring__ADVISOR_STAFF_HIRE_RESEARCHERS_.cpp), main app ("SimTheme Park"),
src/game/advisor/AdvisorScoring.cpp:
  _ADVISOR_OPEN_PARK 0x100113dc, _ADVISOR_CLOSE_PARK 0x1001151c, _ADVISOR_STAFF_HIRE_RESEARCHERS_1 0x100121b0,
  _ADVISOR_STAFF_HIRE_RESEARCHERS_3 0x10012468, _ADVISOR_STAFF_MECHANIC_COVERAGE 0x1001274c,
  _ADVISOR_STAFF_HANDYMAN_COVERAGE 0x100129e4, _ADVISOR_STAFF_TRAIN_RESEARCHERS 0x10013444, GetHighestScoringFromNext 0x10010f88

A case is a world and a call. The world is built twice by shared Python code: in emulator memory at the Mac's addresses for the
original, and in the host library for the translation. Every callee outside the unit (the world's getters, the analyser's queries, the
park time, CanIQueueThis, the game type's constructor, CTRACE, and the scorer every table record points at, reached through the glue
0x1023ba70) is a fake on both sides that records its arguments and answers from the case's queues. The 64 bit divide helper
0x10237e1c runs as the original.

The world of a case: the balance values the function reads (set by Mac offset), the park time ticks, the analyser's answers, the
staff headquarters' training word, and for GetHighestScoringFromNext the whole message table (skipped flag, message number, mode), the
cursor, the count and the game type object with its guard byte. What is compared: the answer, the event log, the cursor and the game
type object with its guard. No fused operations. Not tested: the CTRACE of a message that is not found (the scan always finds the
cursor's own record, so it is unreachable), negative cursors and message numbers outside the table (they read before it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_STAFF_HIRE_RESEARCHERS_"
MASK = 0xFFFFFFFF

FNS = [  # (name, address, size), the order of the host wrapper's `which`
    ("_ADVISOR_STAFF_HIRE_RESEARCHERS_3", 0x10012468, 344),
    ("_ADVISOR_STAFF_HIRE_RESEARCHERS_1", 0x100121B0, 300),
    ("_ADVISOR_STAFF_MECHANIC_COVERAGE", 0x1001274C, 272),
    ("_ADVISOR_STAFF_HANDYMAN_COVERAGE", 0x100129E4, 224),
    ("_ADVISOR_CLOSE_PARK", 0x1001151C, 232),
    ("_ADVISOR_STAFF_TRAIN_RESEARCHERS", 0x10013444, 184),
    ("_ADVISOR_OPEN_PARK", 0x100113DC, 260),
    ("GetHighestScoringFromNext", 0x10010F88, 548),
]
OWN = {a for _, a, _ in FNS}

GREF, GWORLD = 0x102A460C, 0x1037EBFC          # the word at GREF holds GWORLD's address; GWORLD holds the world
GT_VAL, GT_GUARD = 0x102B3AA0, 0x102B3AA4
TABLE = 0x1027EFA4
NREC = 0x15F
D = 0x1792F8648000                             # ticks per unit of the divide
HOST_LAYOUT = [0, 4]

SLOT_OFFS = [0xA4, 0xA8, 0x114, 0x118, 0x120, 0x124, 0x128, 0x12C, 0x138, 0x13C, 0x140, 0x150, 0x154, 0x158, 0x1B4, 0x1B8]

PP = {}                                        # object addresses of the running PowerPC case

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk(name, regs=(), obj=None):
    """A PowerPC fake: records (label of r3, then the listed registers), answers the queue `name` or an object's address."""
    def fn(e):
        L = e.cl
        vals = [L.lab(e.r(3))]
        for reg, kind in regs:
            v = e.r(reg)
            vals.append(v & 0xFF if kind == "b" else T.s32(v))
        L.rec(name, tuple(vals))
        e.set_r(3, PP[obj] if obj else L.ret(name) & MASK)
    return fn


def stub_time(e):
    L = e.cl
    L.rec("FunnyTime", (T.s32(e.r(4) - PP["world"]),))
    hi, lo = L.ret("TickHi") & MASK, L.ret("TickLo") & MASK
    e.write(e.r(3), struct.pack(">II", hi, lo))


def stub_gametype(e):
    L = e.cl
    L.rec("CGameType", (0,))
    e.write(e.r(3), struct.pack(">I", L.ret("CGameType") & MASK))


def stub_scorer(e):
    L = e.cl
    L.rec("Scorer", ((e.r(12) - 0x10 - TABLE) // 0x30,))
    e.set_r(3, L.ret("Scorer") & MASK)


for addr, fn in [
    (0x1013FF7C, mk("IsParkClosed")),
    (0x1013EBC8, mk("GetParkAnalyser", obj="analyser")),
    (0x1013E99C, mk("GetStaffHQ", obj="hq")),
    (0x1013EAB0, mk("GetAdvisor", obj="advisor")),
    (0x100E8ED8, mk("MaxAbility")),
    (0x100E8D4C, mk("ForHire", [(4, "i")])),
    (0x100E8DD8, mk("ResearcherAbility")),
    (0x100E8144, mk("NumStaff", [(4, "i")])),
    (0x100E86A4, mk("PatrolArea", [(4, "i")])),
    (0x100EE924, mk("Percent", [(4, "i"), (5, "i")])),
    (0x100E7028, mk("GatesOpen")),
    (0x100EA5D8, mk("ObjInPark", [(4, "i"), (5, "i")])),
    (0x100E73A8, mk("Outside")),
    (0x100E9E70, mk("LeftToResearch")),
    (0x100E9998, mk("Available", [(4, "b")])),
    (0x100EA298, mk("ParticularObj", [(4, "i"), (5, "b")])),
    (0x100E88A0, mk("AvgQuality", [(4, "i")])),
    (0x1000B3D4, mk("CanQueue", [(4, "i"), (5, "b")])),
    (0x10112B14, stub_time),
    (0x1016F5D8, stub_gametype),
    (0x1023BA70, stub_scorer),
]:
    STUBS.internal(addr, fn)
STUBS.internal(0x1006FCBC, CL.recorder("CTRACE", "usi"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def table_words(self, recs):
        return [w for (sk, msg, mode) in recs for w in (sk, msg, mode)]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.world = e.alloc(0x1000)
        self.analyser, self.advisor = e.alloc(0x100), e.alloc(0x100)
        self.hq = e.alloc(0x80)
        self.scoring = e.alloc(0x400)
        PP.update(world=self.world, analyser=self.analyser, hq=self.hq, advisor=self.advisor)
        self.L = e.cl = CL.PpcLog({self.world: 1, self.analyser: 2, self.hq: 3, self.advisor: 4, self.scoring: 5}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def setup_world(self, case):
        self.words(GREF, [GWORLD])
        self.words(GWORLD, [self.world])
        self.words(self.hq + 0x1C, [case["hq"]])

    def slot(self, off, v):
        self.words(self.scoring + off, [v])

    def set_cursor(self, v):
        self.words(self.scoring, [v])

    def cursor(self):
        return struct.unpack(">I", self.read(self.scoring, 4))[0]

    def table(self, recs):
        self.write(TABLE, b"".join(struct.pack(">12I", sk & MASK, msg & MASK, 0, mode & MASK, 0, 0, 0, 0, 0, 0, 0, 0) for sk, msg, mode in recs))

    def set_game(self, t, g):
        self.words(GT_VAL, [t])
        self.write(GT_GUARD, bytes([g]))

    def game(self):
        return [struct.unpack(">I", self.read(GT_VAL, 4))[0], self.read(GT_GUARD, 1)[0]]

    def call(self, which, count):
        r3, _, _ = T.call_ppc(self.e, FNS[which][1], [self.scoring, count & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.world = lib.tpw_fx_world(case["hq"])
        self.analyser, self.hq, self.advisor = (lib.tpw_fx_obj(i) for i in (1, 2, 3))
        self.scoring = lib.tpw_fx_newscoring()
        if "game" in case:
            lib.tpw_fx_setgame(case["game"][0], case["game"][1])
        CL.host_begin(lib, {1: self.world, 2: self.analyser, 3: self.hq, 4: self.advisor, 5: self.scoring}, case["queues"])

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setup_world(self, case):
        pass

    def slot(self, off, v):
        self.lib.tpw_fx_setslot(self.scoring, off, T.s32(v))

    def set_cursor(self, v):
        self.lib.tpw_fx_setcursor(self.scoring, T.s32(v))

    def cursor(self):
        return self.lib.tpw_fx_getcursor(self.scoring) & MASK

    def table(self, recs):
        arr = (C.c_int32 * (3 * NREC))(*[T.s32(w) for w in self.table_words(recs)])
        self.lib.tpw_fx_settable(arr)

    def set_game(self, t, g):
        pass

    def game(self):
        out = (C.c_int64 * 2)()
        self.lib.tpw_fx_getgame(out)
        return [out[0] & MASK, out[1]]

    def call(self, which, count):
        return self.lib.tpw_fx_call(which, self.scoring, T.s32(count)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for n, res, args in [
        ("alloc", C.c_uint64, [C.c_uint32]),
        ("world", C.c_uint64, [C.c_int32]),
        ("obj", C.c_uint64, [C.c_int32]),
        ("newscoring", C.c_uint64, []),
        ("setslot", None, [C.c_uint64, C.c_int32, C.c_int32]),
        ("setcursor", None, [C.c_uint64, C.c_int32]),
        ("getcursor", C.c_int32, [C.c_uint64]),
        ("settable", None, [C.POINTER(C.c_int32)]),
        ("setgame", None, [C.c_int32, C.c_int32]),
        ("getgame", None, [C.POINTER(C.c_int64)]),
        ("call", C.c_uint32, [C.c_int32, C.c_uint64, C.c_int32]),
        ("layout", C.c_int32, [C.POINTER(C.c_int64)]),
    ]:
        f = getattr(lib, "tpw_fx_" + n)
        f.restype, f.argtypes = res, args
    out = (C.c_int64 * 32)()
    n = lib.tpw_fx_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.setup_world(case)
    for off, v in case["slots"].items():
        s.slot(off, v)
    if "table" in case:
        s.table(case["table"])
        s.set_cursor(case["cursor"])
    if "game" in case:
        s.set_game(*case["game"])


def run(s, case):
    try:
        build(s, case)
        answer = s.call(case["which"], case.get("count", 0))
        out = {"answer": answer, "log": s.log(), "cursor": s.cursor()}
        if "game" in case:
            out["game"] = s.game()
        return out
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

POOLS = {
    "IsParkClosed": [0, 0, 1, 2], "GatesOpen": [0, 1], "MaxAbility": [0, 1, 3, 100, -1, 50],
    "ForHire": [0, 1, 4], "ResearcherAbility": [0, 1, 5, 50, 100, -1, 40000000], "NumStaff": [0, 1, 2, 3, -1],
    "PatrolArea": [0, 1, 3], "Percent": [0, 10, 50, 100, 101, -5, 200], "ObjInPark": [0, 1, 2, -1],
    "Outside": [0, 1, 5, -3, 100000], "LeftToResearch": [0, 1, 3], "Available": [0, 1, 2, 3, 5, -1, -2],
    "ParticularObj": [0, 1, 2, 3, 5, -2, 100], "AvgQuality": [0, 1, 5, 50, 100, -1],
    "CanQueue": [0, 1], "CGameType": [0, 1, 2, 3], "Scorer": [-5, 0, 1, 5, 10, 100],
}
SLOT_POOL = [0, 1, 2, 3, 5, 10, 50, 100, 200, -1, -5]


def base(r, which):
    queues = {k: [r.choice(p) for _ in range(r.choice([1, 2, 3]))] for k, p in POOLS.items()}
    k = r.choice([-2, -1, 0, 1, 2, 5, 100])
    ticks = (k * D + r.choice([0, 1, D - 1, 12345, D // 2])) & 0xFFFFFFFFFFFFFFFF
    queues["TickHi"], queues["TickLo"] = [ticks >> 32], [ticks & MASK]
    slots = {o: r.choice(SLOT_POOL) for o in SLOT_OFFS}
    units = k if k >= 0 else k          # the truncated quotient of the ticks (the remainder is non-negative, so k for k >= 0)
    for o in (0x120, 0x138, 0x150):
        slots[o] = units + r.choice([-1, 0, 0, 1]) if r.random() < 0.7 else r.choice(SLOT_POOL)
    return {"which": which, "queues": queues, "slots": slots, "hq": r.choice([0, 0, 1, 5, -1])}


def ghs_case(r):
    c = base(r, 7)
    recs = [[0, i, r.choice([2, 2, 2, 0, 1, 3])] for i in range(NREC)]
    cursor = r.choice([0, 1, 7, 100, NREC - 2, NREC - 1, NREC, NREC + 1, 0x7FFFFFFF, r.randrange(0, NREC)])
    c["cursor"] = cursor
    c["count"] = r.choice([0, 1, 2, 3, 5, 8])
    start = cursor if 0 <= cursor < NREC else 0
    for _ in range(r.randrange(0, 5)):
        j = (start + r.randrange(0, 8)) % NREC
        kind = r.randrange(4)
        if kind == 0:
            recs[j][0] = 1
        elif kind == 1:
            recs[j][1] = r.choice([NREC, NREC + 1, r.randrange(0, NREC)])
        elif kind == 2:                      # the number is found elsewhere: the record at its own index answers
            m = r.randrange(0, NREC)
            recs[j][1] = m
            if m != j:
                recs[m][1] = (m + 1) % NREC
                recs[m][2] = r.choice([2, 0, 1, 3])
        else:
            recs[j][2] = r.choice([0, 1, 2, 3])
    c["table"] = [tuple(x) for x in recs]
    c["game"] = (r.choice([0, 1, 2, 3]), r.choice([0, 1]))
    return c


def make(which, r):
    return ghs_case(r) if which == 7 else base(r, which)


def make_edges(which):
    def gen():
        for i in range(70):
            yield make(which, random.Random(1000 * which + i))
    return gen


def make_rnd(which):
    return lambda r: make(which, r)


TESTS = [FnTest(name, ppc, host, edge=make_edges(i), rnd=make_rnd(i)) for i, (name, _, _) in enumerate(FNS)]
