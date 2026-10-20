"""Unit v5543, CPerson::ModelState, OnAdmittance, GetDebagged and MakeMajorDecision, against libtpw_unit_CPerson_ModelState.dylib
(tests/unit_CPerson_ModelState.cpp), main app ("SimTheme Park"), src/CPerson_..._CPerson_ModelState.cpp:
  ModelState 0x1011fdb4, OnAdmittance 0x1011f114, GetDebagged 0x1011b594, MakeMajorDecision 0x10118430

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (the peep, the person's
other states, the world, the map, the objects, the guard, CTRACE and GAME_ASSERTMSG) is a fake on both sides that records its arguments
and answers from the case's queues.

The world of a case: the person block (random bytes, typed fields on top), four object blocks chained through ids of the thing table, the
world's turn and list heads, gWorld, gWorldMap, gThingArray rows 0 to 40, the sound bank word and the unit's id globals. What is compared:
the event log, the person's written fields and every other byte of the person block (unchanged).

No fused operations. Not tested: a thing id that is not in the table's rows 0 to 40 (the original would read a NULL row).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPerson_ModelState"
MASK = 0xFFFFFFFF

FN_MODEL, FN_ADMIT, FN_DEBAG, FN_MAJOR = 0x1011FDB4, 0x1011F114, 0x1011B594, 0x10118430
SIZES = {FN_MODEL: 1396, FN_ADMIT: 120, FN_DEBAG: 280, FN_MAJOR: 408}
OWN = set(SIZES)

ADDR = dict(gWorld=0x1037EC0C, gWorldMap=0x1037EC08, arr=0x1034CBFC, banks=0x102F7C94, noadmit=0x102A0FFE,
            majorEnd=0x102A0F18, majorNoQ=0x102A0F1A, majorNoR=0x102A0F1C, modelEnd=0x102A100E)
HOST_LAYOUT = [0x1DA70C, 0x1DA744, 0x1DA746, 0x1DA748]
NROWS = 41
PERSON_SIZE = 0x240

STUBS = CL.Stubs()
STUBS.real.update(OWN)

# (address, name, spec, ret)
RECORDED = [
    (0x10114FA8, "PeepWalks", "L", "i"),
    (0x10113870, "SetRandomDestination", "L", "i"),
    (0x10115400, "SetDest", "Lw", "i"),
    (0x101154F0, "SetDestXY", "Lww", None),
    (0x101159D0, "Teleport", "Lww", None),
    (0x10115B80, "PlaySound", "Lii", None),
    (0x1014080C, "CrossingIsClear", "L", "i"),
    (0x1013B1E4, "GetRand", "L", "i"),
    (0x1013FF7C, "IsParkClosed", "L", "i"),
    (0x1013FFB4, "ParkGates_GetStatus", "L", "i"),
    (0x10140E64, "GetNewPersonIndex", "L", "i"),
    (0x10140DE0, "IsSeagoing", "L", "i"),
    (0x101083E0, "IsAvailableToChoose", "L", "i"),
    (0x1010D35C, "FinishedAdmitting", "L", "i"),
    (0x10108C20, "RemovePersonFromQueue", "Lw", None),
    (0x100FE300, "IsCarrying", "L", "i"),
    (0x1012E728, "KillMe", "L", None),
    (0x10120354, "SetState", "Li", None),
    (0x1011C88C, "WalkingToTicketBooths", "L", None),
    (0x1011CCE4, "PayingAtBooth", "L", None),
    (0x1011C9F8, "WaitingForParkToOpen", "L", None),
    (0x1011D400, "WalksToLitterBin", "L", None),
    (0x1011BAA8, "Idle", "L", None),
    (0x1011CF18, "WalksToMajor", "L", None),
    (0x1011D5EC, "StandingInQueue", "L", None),
    (0x1011DCC0, "BeingLoaded", "L", None),
    (0x1011E0E4, "BeingUnloaded", "L", None),
    (0x1011E2E8, "LeavingPark", "L", None),
    (0x1011E42C, "WaitingToCross", "L", None),
    (0x1011E654, "WaitingForBus", "L", None),
    (0x10117F80, "EndSpotAnim", "L", None),
    (0x101185F8, "CalculateOptionScore", "LL", "i"),
    (0x1000DDE8, "GAME_ASSERTMSG", "is", None),
]
for _addr, _name, _spec, _ret in RECORDED:
    STUBS.internal(_addr, CL.recorder(_name, _spec, _ret))

_CT1 = CL.recorder("CTRACE", "us")
_CT2 = CL.recorder("CTRACE", "usuu")


def stub_ctrace(e):
    (_CT2 if "%d" in e.cstr(e.r(4)) else _CT1)(e)


STUBS.internal(0x1006FCBC, stub_ctrace)


def hidden_tag(name, addr, args):
    """A callee that answers a two byte struct through the hidden pointer in r3; its arguments start at r4."""
    def stub(e):
        L = e.cl
        L.rec(name, tuple(args(e, L)))
        e.write(e.r(3), struct.pack(">H", L.ret(name) & 0xFFFF))
    STUBS.internal(addr, stub)


hidden_tag("GetThingId", 0x1012E1F8, lambda e, L: (L.lab(e.r(4)),))
hidden_tag("GetTicketBoothPos", 0x101019BC, lambda e, L: (L.lab(e.r(4)), T.s32(e.r(5))))
hidden_tag("GetCrossingParkSidePos", 0x10101B74, lambda e, L: (L.lab(e.r(4)), T.s32(e.r(5))))
hidden_tag("GetBackOfQueue", 0x101092F4, lambda e, L: (L.lab(e.r(4)),))


def stub_spawn(e):
    e.cl.rec("SpawnSpriteScript", ((e.r(3) - PERS["person"]) & MASK, T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))))


STUBS.internal(0x100FAB9C, stub_spawn)

PERS = {}


def setup(runner):
    T.install_std_stubs(runner.emu)
    for name, size in (("person", 0x240), ("obj0", 0x240), ("obj1", 0x240), ("obj2", 0x240), ("obj3", 0x240), ("world", 0x1DA748 + 16),
                       ("map", 0x100)):
        PERS[name] = T.persist_alloc(runner.emu, size)


LABELS = ["person", "map", "world", "obj0", "obj1", "obj2", "obj3"]      # labels 1 to 7


class Side:
    def put(self, a, size, v):
        self.write(a, struct.pack(self.fmt[0] + ("H" if size == 2 else "I"), v & (0xFFFF if size == 2 else MASK)))

    def get(self, a, size):
        return struct.unpack(self.fmt[0] + ("H" if size == 2 else "I"), self.read(a, size))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {PERS[n]: i + 1 for i, n in enumerate(LABELS)}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def a(self, name):
        return PERS[name]

    def g(self, name):
        return ADDR[name]

    def ptr(self, a, v):
        self.put(a, 4, v)

    def row(self, i):
        return ADDR["arr"] + 20 * i

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {i + 1: self.a(n) for i, n in enumerate(LABELS)}, case["queues"])

    def done(self):
        pass

    def a(self, name):
        return self.lib.tpw_ms_addr(name.encode())

    def g(self, name):
        return self.lib.tpw_ms_addr(name.encode())

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def row(self, i):
        return self.g("arr") + i * (self.g("arr1") - self.g("arr"))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        if fn == FN_MODEL:
            self.lib.tpw_ms_model()
        elif fn == FN_ADMIT:
            self.lib.tpw_ms_admit()
        elif fn == FN_DEBAG:
            self.lib.tpw_ms_debag(args[1] >> 16)
        else:
            self.lib.tpw_ms_major()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ms_addr.restype, lib.tpw_ms_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ms_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ms_debag.argtypes = [C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ms_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

PFIELDS = [(0x1C, 4), (0x188, 4), (0x1D8, 4), (0x1DC, 2), (0x1F8, 4)]       # what the four functions may write


def build(s, case):
    p = s.a("person")
    s.write(p, case["pbytes"])
    for off, (size, v) in case["P"].items():
        s.put(p + off, size, v)
    for k in range(4):
        o = s.a("obj%d" % k)
        s.write(o, case["obytes"][k])
        for off, (size, v) in case["O"][k].items():
            s.put(o + off, size, v)
    w = s.a("world")
    s.put(w + 0x1DA70C, 4, case["turn"])
    s.put(w + 0x1DA744, 2, case["head44"])
    s.put(w + 0x1DA746, 2, case["head46"])
    s.ptr(s.g("gWorld"), w)
    s.ptr(s.g("gWorldMap"), s.a("map"))
    for i in range(NROWS):
        k = case["things"].get(i)
        s.ptr(s.row(i), s.a("obj%d" % k) if k is not None else 0)
    for name in ("noadmit", "majorEnd", "majorNoQ", "majorNoR", "modelEnd"):
        s.put(s.g(name), 2, case[name])
    s.put(s.g("banks") + 4, 4, case["bank1"])


def observe(s, case):
    p = s.a("person")
    fields = [s.get(p + off, size) for off, size in PFIELDS]
    rest = bytearray(s.read(p, PERSON_SIZE))
    for off, size in list(PFIELDS) + [(o, v[0]) for o, v in case["P"].items()]:
        rest[off:off + size] = b"\0" * size
    return {"log": s.log(), "fields": fields, "rest": bytes(rest)}


def run(s, case):
    try:
        build(s, case)
        p = s.a("person")
        op = case["op"]
        if op == "model":
            s.call(FN_MODEL, [p])
        elif op == "admit":
            s.call(FN_ADMIT, [p])
        elif op == "debag":
            s.call(FN_DEBAG, [p, (case["cell"] << 16) & MASK])
        else:
            s.call(FN_MAJOR, [p])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SCORES = [0, 5, 9, 10, 11, 12, 20, 0x7FFFFFFF, 0x80000000, 0xFFFFFFF0, 0xFFFFFFFF]
IDS = [0, 1, 2, 0x7F, 0x80, 0x81, 0x4000, 0x7FFF, 0x8000, 0xFFFF]


def lst(r, vals, n=None):
    return [r.choice(vals) for _ in range(n if n is not None else r.randrange(1, 5))]


def chain(r, n, linkoff, end):
    objs = r.sample(range(4), n)
    ids = r.sample(range(1, NROWS), n)
    O = [dict() for _ in range(4)]
    things = {}
    for k in range(n):
        things[ids[k]] = objs[k]
        O[objs[k]][linkoff] = (2, ids[k + 1] if k + 1 < n else end)
    return (ids[0] if n else end), O, things


def base(r, op):
    c = {"op": op, "queues": {}, "P": {}, "pbytes": bytes(r.randrange(256) for _ in range(PERSON_SIZE)),
         "obytes": [bytes(r.randrange(256) for _ in range(0x240)) for _ in range(4)], "O": [dict() for _ in range(4)],
         "turn": r.choice([0, 1, 2, 3, r.getrandbits(32), MASK]), "head44": r.choice(IDS), "head46": r.choice(IDS), "things": {},
         "noadmit": r.choice([0, 1, 7]), "majorEnd": 0, "majorNoQ": 0, "majorNoR": 0, "modelEnd": 0, "bank1": r.getrandbits(32)}
    c["P"][0x1DC] = (2, r.choice([0, 1, 5, 40, r.randrange(1, NROWS)]))
    c["P"][0x228] = (2, r.choice([0, 1, 7]))
    for off in (0x1C, 0x20, 0x24, 0x188, 0x1D8, 0x1F8, 0x208):
        c["P"][off] = (4, r.choice([0, 1, 0x400, r.getrandbits(32)]))
    if r.random() < 0.2:
        c["things"][0] = r.randrange(4)
    return c


def model_case(r, state, pw=None, flag=None):
    c = base(r, "model")
    c["P"][0x220] = (4, state)
    flag = r.randrange(2) if flag is None else flag
    pid = r.choice([1, 2, 0x1234, 0xFFFF])
    ride = r.randrange(1, NROWS)
    c["P"][0x1DC] = (2, ride)
    c["things"][ride] = r.randrange(4)
    q = c["queues"]
    q["PeepWalks"] = [r.choice([0, 1, 2]) if pw is None else pw]
    q["CrossingIsClear"] = [flag]
    q["GetRand"] = lst(r, [0, 1, 2, 3, 4, 0xFF, 0x100, 0x1FF, r.getrandbits(32)])
    q["GetTicketBoothPos"] = [r.choice(IDS)]
    q["IsParkClosed"] = [flag * r.choice([1, 0x10000])]
    q["ParkGates_GetStatus"] = [r.choice([0, 1, 1, 2, -1])]
    q["GetNewPersonIndex"] = [r.getrandbits(31)]
    q["SetRandomDestination"] = [flag]
    q["IsSeagoing"] = [flag]
    q["FinishedAdmitting"] = [flag]
    q["GetThingId"] = [pid]
    c["noadmit"] = c["P"][0x228][1] if r.random() < 0.5 else r.choice([0, 1, 7])
    if state == 17:
        n = r.randrange(0, 5)
        end = r.choice([0, 0x7777, 0xFFFF])
        c["modelEnd"] = end
        head, O, things = chain(r, n, 0x20C, end)
        c["head44"] = head
        c["O"] = O
        c["things"].update(things)
        for k in range(4):
            c["O"][k][0x20E] = (2, pid if r.random() < 0.4 else r.choice(IDS))
        q["IsCarrying"] = lst(r, [0, 1])
    elif state == 8:
        stamp = c["P"][0x208][1]
        c["turn"] = (stamp + 10 + r.choice([-2, -1, 0, 1, 2, 100])) & MASK
    return c


def model_edges():
    r = random.Random(5543)
    for st in list(range(0x16)) + [0x16, 0x17, 0x100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]:
        for pw in (0, 1, 2):
            for flag in (0, 1):
                for _ in range(3):
                    yield model_case(r, st, pw, flag)


def model_random(r):
    return model_case(r, r.choice(list(range(0x16)) * 3 + [0x16, 0xFFFFFFFF]))


def admit_case(r):
    c = base(r, "admit")
    c["queues"]["GetThingId"] = [r.choice(IDS)]
    return c


def debag_case(r, cell=None):
    c = base(r, "debag")
    c["cell"] = r.choice(IDS) if cell is None else cell
    q = c["queues"]
    q["GetRand"] = lst(r, [0, 1, 2, 3, r.getrandbits(32)], 1)
    q["GetCrossingParkSidePos"] = [r.choice(IDS)]
    q["SetDest"] = [r.choice([0, 1])]
    return c


def debag_edges():
    r = random.Random(5544)
    for cell in IDS:
        for sd in (0, 1):
            c = debag_case(r, cell)
            c["queues"]["SetDest"] = [sd]
            yield c


def major_case(r):
    c = base(r, "major")
    n = r.randrange(0, 5)
    end = r.choice([0, 0x7777, 0xFFFF])
    c["majorEnd"] = end
    c["majorNoQ"] = r.choice([0, 3, 0x1111])
    head, O, things = chain(r, n, 0xC, end)
    c["head46"] = head
    c["O"] = O
    c["things"].update(things)
    ids = list(things)
    c["majorNoR"] = r.choice([0, 0, 0x7777] + ids)
    for k in range(4):
        c["O"][k][0x2E] = (2, r.choice([4, 4, 4, 0, 0xFFFB, 0xFFFF, 0x2A4]))
    c["P"][0x1DC] = (2, r.choice([0, 0x55]))
    q = c["queues"]
    q["GetBackOfQueue"] = lst(r, [c["majorNoQ"], 5, 0x1111, 0x22])
    q["CalculateOptionScore"] = lst(r, SCORES)
    q["IsAvailableToChoose"] = lst(r, [0, 1, 1])
    q["SetDest"] = lst(r, [0, 1, 1])
    return c


TESTS = [
    FnTest("CPerson::ModelState", ppc, host, edge=model_edges, rnd=model_random),
    FnTest("CPerson::OnAdmittance", ppc, host, rnd=admit_case),
    FnTest("CPerson::GetDebagged", ppc, host, edge=debag_edges, rnd=debag_case),
    FnTest("CPerson::MakeMajorDecision", ppc, host, rnd=major_case),
]
