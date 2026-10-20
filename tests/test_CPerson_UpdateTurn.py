"""Unit v5217, CPerson::UpdateTurn and CPerson::FindNearestFireworks, against libtpw_unit_CPerson_UpdateTurn.dylib
(tests/unit_CPerson_UpdateTurn.cpp), main app ("SimTheme Park"), src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_UpdateTurn.cpp:
  CPerson::UpdateTurn 0x1011f7d0, CPerson::FindNearestFireworks 0x1011ed54

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (CPeep::UpdateTurn, CThing::GetThingId, CMap::GetRegionalEffectCell, CPeep::IsInsidePark, CWorld::GetRand, DoSomeThinking,
MakeBalloonPop, ProcessThoughtBubbles, InQueueingState, UpdateTemporaryHistory, GAME_ASSERTMSG, RSSE_GetScriptPointer, RSSE_Getvar) are
fakes on both sides that record their arguments ("this" as an offset from the person; the assert text as the sum of its bytes) and answer
from the case's queues; labs is a pure import faked on the PowerPC side and is libc's on the host.

The world of a case: a world block (the turn at +0x1da70c, the first thing id at +0x1da746), the person block, the effect cell, the thing
array entries 0..15 with their thing blocks, and the globals (gPersonTable, the three new halfwords). What is compared: the answer, the
person's fields the function writes (five floats, +0x1bc, +0xc2, +0x214, +0x228, +0x22a), the ids found, and the event log.

No fused operations. fctiwz only meets values below 2^31 (the fields are clamped to 0..100 or start in a small range). Not tested: NaN
fields (Unicorn and the host may disagree on payloads and fctiwz of NaN), a list that never reaches the end id.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPerson_UpdateTurn"
MASK = 0xFFFFFFFF

FN_UPDATE, FN_FIND = 0x1011F7D0, 0x1011ED54
SIZES = {FN_UPDATE: 1464, FN_FIND: 396}
OWN = set(SIZES)

ADDR = dict(gWorld=0x1037EC0C, gWorldMap=0x1037EC08, gThingArray=0x1034CBFC, gPersonTable=0x102A0D54,
            gThingListEnd=0x102A1008, gQueueLinkA=0x102A100A, gQueueLinkB=0x102A100C)
HOST_MTURN = 0x1DA70C
TURN_OFF = 0x1DA70C
HEAD_OFF = 0x1DA746
WORLD_SIZE = 0x1DB000

STATE = {}
STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def rel(e, v):
    return s32((v - STATE["tperson"]) & MASK)


def stub_peep_update(e):
    e.cl.rec("CPeep::UpdateTurn", (rel(e, e.r(3)),))


def stub_inside(e):
    e.cl.rec("IsInsidePark", (rel(e, e.r(3)),))
    e.set_r(3, e.cl.ret("IsInsidePark") & MASK)


def stub_thing_id(e):
    e.cl.rec("GetThingId", (rel(e, e.r(4)),))
    e.write(e.r(3), struct.pack(">H", e.cl.ret("GetThingId") & 0xFFFF))


def stub_cell(e):
    e.cl.rec("GetRegionalEffectCell", (s32((e.r(3) - STATE["tmap"]) & MASK), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, STATE["tcell"])


def stub_rand(e):
    e.cl.rec("GetRand", (s32((e.r(3) - STATE["world"]) & MASK),))
    e.set_r(3, e.cl.ret("GetRand") & MASK)


def stub_plain(name):
    def fn(e):
        e.cl.rec(name, (rel(e, e.r(3)),))
    return fn


def stub_queueing(e):
    e.cl.rec("InQueueingState", (rel(e, e.r(3)),))
    e.set_r(3, e.cl.ret("InQueueingState") & MASK)


def stub_getscript(e):
    h = s32(e.r(3))
    e.cl.rec("RSSE_GetScriptPointer", (h,))
    e.set_r(3, 0x5000 + (h & 0xFF))


def stub_getvar(e):
    e.cl.rec("RSSE_Getvar", (e.r(3), s32(e.r(4))))
    e.set_r(3, e.cl.ret("RSSE_Getvar") & MASK)


def stub_assert(e):
    text = e.cstr(e.r(4))
    if isinstance(text, bytes):
        text = text.decode("latin-1")
    total = sum(ord(c) for c in text)
    if text.startswith("Person"):
        e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)), total, s32(e.r(5)), s32(e.r(6)), s32(e.r(7)), s32(e.r(8))))
    else:
        e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)), total))


STUBS.internal(0x101158C8, stub_peep_update)
STUBS.internal(0x1012E1F8, stub_thing_id)
STUBS.internal(0x10101640, stub_cell)
STUBS.internal(0x10115A80, stub_inside)
STUBS.internal(0x1013B1E4, stub_rand)
STUBS.internal(0x10117FD4, stub_plain("DoSomeThinking"))
STUBS.internal(0x1011B11C, stub_plain("MakeBalloonPop"))
STUBS.internal(0x1012F058, stub_plain("ProcessThoughtBubbles"))
STUBS.internal(0x10120B2C, stub_queueing)
STUBS.internal(0x10119B68, stub_plain("UpdateTemporaryHistory"))
STUBS.internal(0x1000DDE8, stub_assert)
STUBS.internal(0x100D668C, stub_getscript)
STUBS.internal(0x100D6C58, stub_getvar)
STUBS.imp("labs", lambda e: e.set_r(3, abs(s32(e.r(3))) & MASK))


def setup(runner):
    T.install_std_stubs(runner.emu)
    STATE["ppc_world"] = T.persist_alloc(runner.emu, WORLD_SIZE)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def byte(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"
    stride = 20

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def world(self):
        STATE["world"] = STATE["ppc_world"]
        return STATE["world"]

    def ptr(self, a, v):
        self.words(a, [v])

    def ref(self, name, v):
        STATE[name] = v

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_update(self, person):
        T.call_ppc(self.e, FN_UPDATE, [person])
        return 0

    def call_find(self, person):
        out = self.e.alloc(4)
        T.call_ppc(self.e, FN_FIND, [out, person])
        return struct.unpack(">H", self.e.read(out, 2))[0]


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.stride = lib.stride
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ut_addr(name.encode())

    def world(self):
        if "host_world" not in STATE:
            STATE["host_world"] = self.lib.tpw_ut_alloc(WORLD_SIZE)
        return STATE["host_world"]

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def ref(self, name, v):
        self.ptr(self.g(name), v)

    def alloc(self, n):
        return self.lib.tpw_ut_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_update(self, person):
        self.lib.tpw_ut_update(person)
        return 0

    def call_find(self, person):
        return self.lib.tpw_ut_fireworks(person) & 0xFFFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, "tpw_ut_addr").restype, getattr(lib, "tpw_ut_addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, "tpw_ut_alloc").restype, getattr(lib, "tpw_ut_alloc").argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ut_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ut_update.restype, lib.tpw_ut_update.argtypes = None, [C.c_uint64]
    lib.tpw_ut_fireworks.restype, lib.tpw_ut_fireworks.argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ut_layout(out)
    assert n == 2 and out[0] == HOST_MTURN, list(out)[:n]
    lib.stride = out[1]
    lib.bound = True


FLOAT_OFFS = (0x19C, 0x1A4, 0x1A8, 0x1AC, 0x1B0)


def build_common(s, case):
    world = s.world()
    s.ptr(s.g("gWorld"), world)
    s.words(world + TURN_OFF, [case["turn"]])
    s.half(world + HEAD_OFF, case["head"])
    m = s.alloc(16)
    s.ptr(s.g("gWorldMap"), m)
    s.ref("tmap", m)
    s.half(s.g("gThingListEnd"), case["term"])
    s.half(s.g("gQueueLinkA"), case["qa"])
    s.half(s.g("gQueueLinkB"), case["qb"])
    s.half(s.g("gPersonTable"), case["tbl"][0])
    s.half(s.g("gPersonTable") + 2, case["tbl"][1])
    person = s.alloc(0x240)
    s.ref("tperson", person)
    s.byte(person + 4, case["pb4"])
    s.byte(person + 6, case["pb6"])
    return person


def build_update(s, case):
    person = build_common(s, case)
    cell = s.alloc(8)
    s.ref("tcell", cell)
    for i, v in enumerate(case["cell"]):
        s.half(cell + 2 * i, v)
    for off, v in zip(FLOAT_OFFS, case["floats"]):
        s.words(person + off, [fb(v)])
    s.words(person + 0x1BC, [case["f1bc"]])
    s.words(person + 0x214, [case["f214"]])
    s.words(person + 0x220, [case["f220"]])
    s.half(person + 0xC2, case["h0c2"])
    s.half(person + 0x1DC, case["h1dc"])
    s.half(person + 0x228, case["h228"])
    s.half(person + 0x22A, case["h22a"])
    return person


def build_find(s, case):
    person = build_common(s, case)
    arr = s.g("gThingArray")
    s.write(arr, bytes(16 * s.stride))
    blocks = {}
    for t in case["things"]:
        b = s.alloc(0x40)
        s.byte(b + 4, t["b4"])
        s.byte(b + 6, t["b6"])
        s.half(b + 0xC, t["next"])
        s.words(b + 0x20, [t["handle"]])
        s.half(b + 0x2E, t["flags"])
        blocks[t["id"]] = b
        s.ptr(arr + t["id"] * s.stride, b)
    if case["bad0"] and 0 not in blocks:
        s.ptr(arr, s.alloc(0x40))
    return person


def observe_update(s, person):
    return {
        "floats": s.rd_words(person + 0x19C, 1) + s.rd_words(person + 0x1A4, 1) + s.rd_words(person + 0x1A8, 1)
        + s.rd_words(person + 0x1AC, 1) + s.rd_words(person + 0x1B0, 1),
        "w1bc": s.rd_word(person + 0x1BC),
        "w214": s.rd_word(person + 0x214),
        "w220": s.rd_word(person + 0x220),
        "halves": [s.rd_half(person + o) for o in (0xC2, 0x1DC, 0x228, 0x22A)],
    }


def run(s, case):
    try:
        if case["op"] == "u":
            person = build_update(s, case)
            s.call_update(person)
            obs = observe_update(s, person)
        else:
            person = build_find(s, case)
            obs = {"found": s.call_find(person)}
        obs["log"] = s.log()
        return obs
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

FLOAT_PICKS = [0.0, 1.0, 2.0, 80.0, 80.5, 80.9, 81.0, 79.0, 97.5, 98.0, 98.9, 99.0, 99.5, 99.9, 100.0, 100.9, 356.0, -156.0, 50.0, 12.25]
CELL_PICKS = [0, 1, -1, 2, -2, 100, -100, 99, -99, 32767, -32768, 50]


def pickf(r):
    k = r.random()
    if k < 0.6:
        return r.choice(FLOAT_PICKS)
    if k < 0.85:
        return float(r.randint(0, 100))
    return r.uniform(0, 100)


def base(r, op):
    return {"op": op, "turn": r.getrandbits(32), "head": 0, "term": 0, "qa": 0, "qb": 0,
            "tbl": [r.choice([0, 25, 7]), r.choice([25, 9, 0xFFFF])], "pb4": r.randrange(0, 128), "pb6": r.randrange(0, 128),
            "queues": {}}


def u_case(r, **ov):
    c = base(r, "u")
    nib = r.choice([0, 0, 4, r.randrange(16)])
    c["turn"] = (c["turn"] & ~0xF & MASK) | nib
    id1 = (c["turn"] & 3) if r.random() < 0.6 else r.randrange(4)
    id1 |= r.randrange(0, 0x4000) << 2
    id2 = r.randrange(0x10000)
    c["floats"] = [pickf(r) for _ in range(5)]
    c["cell"] = [r.choice(CELL_PICKS) if r.random() < 0.7 else r.randint(-120, 120) for _ in range(3)]
    c["f1bc"] = r.choice([0, 1, r.getrandbits(32)])
    c["f214"] = r.choice([0, 1, 2, 5, r.getrandbits(32)])
    c["f220"] = r.choice([0x10, 0x11, 0xF, 0x12, 0, r.getrandbits(32)])
    c["h0c2"] = r.randrange(0x10000)
    c["h1dc"] = r.randrange(0x10000)
    c["h228"] = r.choice([0, r.randrange(0x10000)])
    c["h22a"] = r.choice([0, r.randrange(0x10000)])
    c["qa"] = c["h228"] if r.random() < 0.5 else r.randrange(0x10000)
    c["qb"] = c["h22a"] if r.random() < 0.5 else r.randrange(0x10000)
    c["queues"] = {"GetThingId": [id1, id2], "InQueueingState": [r.choice([0, 1])], "IsInsidePark": [r.choice([0, 1, 5])],
                   "GetRand": [r.choice([0, 10, 20, 3, 9, 11, 0xFFFFFFFA, r.getrandbits(32)])]}
    c.update(ov)
    return c


def match_turn(turn, **ov):
    q = {"GetThingId": [turn & 3, 0x1234]}
    q.update(ov)
    return q


def u_edges():
    r = random.Random(5217)
    for off in range(5):
        for v in (100.0, 99.0, 98.0, 99.9, 356.0, 0.0, 50.0, -156.0):
            for nib in (0, 4):
                floats = [0.0] * 5
                floats[off] = v
                turn = 0x40 | nib
                yield u_case(r, turn=turn, floats=floats, cell=[0, 0, 0], f220=0x12,
                             queues={"GetThingId": [turn & 3, 7], "InQueueingState": [1], "IsInsidePark": [0], "GetRand": [1]})
    for v in (80.0, 80.5, 81.0, 79.0, 80.9, 336.0, 0.0):
        for tbl in ([0, 25], [3, 77]):
            turn = 0x41
            yield u_case(r, turn=turn, floats=[0, 0, 0, v, 0], cell=[0, 0, 0], tbl=tbl, f220=0x10,
                         queues={"GetThingId": [1, 7], "InQueueingState": [1]})
    for st in (0x10, 0x11, 0x12, 0xF, 0):
        for inside in (0, 1):
            for balloon in (0, 1, 2):
                for rnd in (0, 10, 3):
                    turn = 0x20
                    yield u_case(r, turn=turn, f220=st, f214=balloon, cell=[0, 0, 0],
                                 queues={"GetThingId": [0, 7], "InQueueingState": [1], "IsInsidePark": [inside], "GetRand": [rnd]})
    for qa_ok in (0, 1):
        for qb_ok in (0, 1):
            for queue in (0, 1):
                for mt in (0, 1):
                    h228, h22a = 0x123, 0x456
                    turn = 8
                    ids = [(turn & 3) if mt else 1, 0xBEEF]
                    yield u_case(r, turn=turn, h228=h228, h22a=h22a, h1dc=0x77, qa=h228 if qa_ok else 1, qb=h22a if qb_ok else 2,
                                 queues={"GetThingId": ids, "InQueueingState": [queue], "IsInsidePark": [1], "GetRand": [5]})
    for a, b, c in ((100, 100, 100), (-100, -100, -100), (32767, 32767, 32767), (-32768, -32768, -32768), (5, -5, 0)):
        for fl in (0.0, 50.0, 100.0, 99.5):
            turn = 0x30
            yield u_case(r, turn=turn, cell=[a, b, c], floats=[fl] * 5,
                         queues={"GetThingId": [0, 7], "InQueueingState": [1]})


def u_random(r):
    return u_case(r)


def mk_thing(r, pb4, pb6, dx=None, dy=None, flag=True, var=0, tid=1):
    if dx is None:
        dx = r.choice([-3, -2, -1, 0, 0, 1, 2, 3])
    if dy is None:
        dy = r.choice([-3, -2, -1, 0, 0, 1, 2, 3])
    return {"id": tid, "b4": (pb4 + dx) & 0xFF, "b6": (pb6 + dy) & 0xFF,
            "flags": (r.getrandbits(16) | 0x80) if flag else (r.getrandbits(16) & ~0x80 & 0xFFFF),
            "handle": r.getrandbits(32), "var": var, "next": 0}


def f_finish(c, things, term=0):
    for i, t in enumerate(things):
        t["next"] = things[i + 1]["id"] if i + 1 < len(things) else term
    c["things"] = things
    c["term"] = term
    c["head"] = things[0]["id"] if things else term
    q = [t["var"] for t in things if t["flags"] & 0x80]
    c["queues"] = {"RSSE_Getvar": q or [0]}
    return c


def f_case(r, **ov):
    c = base(r, "f")
    if r.random() < 0.2:
        c["pb4"] = r.randrange(256)
        c["pb6"] = r.randrange(256)
    n = r.choice([0, 1, 2, 3, 4, 6])
    term = 0 if r.random() < 0.8 else 0xFFFF
    pool = list(range(1 if term == 0 else 0, 15))
    ids = r.sample(pool, n)
    things = []
    for i in ids:
        t = mk_thing(r, c["pb4"], c["pb6"], flag=r.random() < 0.8, var=r.choice([0, 0, 0, 1, 5]), tid=i)
        if r.random() < 0.15:
            t["b4"] = r.randrange(256)
            t["b6"] = r.randrange(256)
        things.append(t)
    c["bad0"] = r.random() < 0.15
    f_finish(c, things, term)
    c.update(ov)
    return c


def f_edges():
    r = random.Random(1011)
    for _ in range(3):
        yield f_finish(dict(base(r, "f"), bad0=False), [])
    for dx, dy in ((0, 0), (2, 0), (0, 2), (1, 1), (2, 1), (1, 2), (3, 0), (-2, 0), (0, -2), (-1, -1), (2, 2)):
        for flag in (True, False):
            for var in (0, 1):
                c = dict(base(r, "f"), bad0=False)
                c["pb4"], c["pb6"] = 50, 60
                yield f_finish(c, [mk_thing(r, 50, 60, dx, dy, flag, var, 3)])
    for d1, d2 in (((1, 0), (0, 1)), ((2, 0), (1, 0)), ((1, 0), (2, 0)), ((1, 1), (0, 2)), ((0, 0), (0, 0)), ((3, 1), (0, 1))):
        c = dict(base(r, "f"), bad0=False)
        c["pb4"], c["pb6"] = 20, 30
        yield f_finish(c, [mk_thing(r, 20, 30, d1[0], d1[1], True, 0, 2), mk_thing(r, 20, 30, d2[0], d2[1], True, 0, 5)])
    for bad0 in (False, True):
        c = dict(base(r, "f"), bad0=bad0)
        c["pb4"], c["pb6"] = 127, 127
        yield f_finish(c, [mk_thing(r, 127, 127, 0, 0, True, 0, 0), mk_thing(r, 127, 127, 1, 1, True, 0, 4)], 0xFFFF)
    c = dict(base(r, "f"), bad0=False)
    c["pb4"], c["pb6"] = 0, 0
    yield f_finish(c, [mk_thing(r, 0, 0, 0, 0, True, 0, 1), mk_thing(r, 0, 0, -1, -1, True, 0, 2)])
    c = dict(base(r, "f"), bad0=False)
    c["pb4"], c["pb6"] = 255, 255
    yield f_finish(c, [mk_thing(r, 255, 255, 1, 0, True, 0, 1), mk_thing(r, 255, 255, 0, 0, True, 0, 2)])


def f_random(r):
    return f_case(r)


TESTS = [
    FnTest("CPerson::UpdateTurn", ppc, host, edge=u_edges, rnd=u_random),
    FnTest("CPerson::FindNearestFireworks", ppc, host, edge=f_edges, rnd=f_random),
]
