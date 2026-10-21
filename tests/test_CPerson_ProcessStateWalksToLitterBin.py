"""Unit v5529, three states of the person (the walk to a litter bin, the stand in a ride's queue, the wait for the ride's script to admit
the person), against libtpw_unit_CPerson_ProcessStateWalksToLitterBin.dylib (tests/unit_CPerson_ProcessStateWalksToLitterBin.cpp), main app
("SimTheme Park"), src/CPerson_..._CPerson_ProcessStateWalksToLitterBin.cpp:
  CPerson::ProcessStateWalksToLitterBin 0x1011d400, CPerson::ProcessStateStandingInQueue 0x1011d5ec,
  CPerson::ProcessStateWaitingForScriptToAdmitMe 0x1011dfc8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee
outside the unit (PeepWalks, SetDest, SetState, LeaveQueue, the CObject queries, the thoughts, GetRand, PlaceLitter, the RSSE calls, the
asserts and CTRACE with their texts) is a fake on both sides that records its arguments (the assert and trace texts too) and answers from
the case's queues. CThing::GetThingId answers a number made of the label of the thing asked.

The world of a case: the person block (cell bytes, angle, happiness floats, ride, queue place, patience, admit flag, stamps), the ride
object block (+0x20 script handle, +0x28, +0x2e), the ride record word +0x9c, gThingArray entries 0 and the ride's, the world turn, the
map base, gLitterKindCount and the two new halfword globals. What is compared: the event log and the person's words at +0x1c, +0x1b4,
+0x1f4 and +0x1f8 (the only ones the code writes). The ride record word is written at the Mac's +0x9c on the PPC side and at the host
field of the same name on the host side (tests/ridelayout.py).

No fused operations. fctiwz: the happiness floats stay inside the int range (Unicorn answers 0x80000000 above it) and are never NaN.
Not tested: a ride index of 0x2800 or more (the host table ends there), a person whose cell id wraps to 0 (the bytes cannot reach it).
"""
import ctypes as C
import random
import struct
from itertools import product

import calllog as CL
import ridelayout
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPerson_ProcessStateWalksToLitterBin"
MASK = 0xFFFFFFFF

FN_WALK, FN_QUEUE, FN_SCRIPT = 0x1011D400, 0x1011D5EC, 0x1011DFC8
SIZES = {FN_WALK: 432, FN_QUEUE: 1484, FN_SCRIPT: 216}
OWN = set(SIZES)

ADDR = dict(gworld=0x1037EC0C, gmap=0x1037EC08, kinds=0x102A0B20, ffc=0x102A0FFC, ffe=0x102A0FFE)
THINGS = 0x1034CBFC
MAP = 0x30000000
SCRIPT_PTR = 0x70001234
HOST_LAYOUT = [0x1DA70C]
P = "tpw_u5529_"

CUR = {}
STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def text(e, addr):
    s = e.cstr(addr)
    return s.decode("latin1") if isinstance(s, bytes) else s


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

R = CL.recorder
ASSERT_R = [R("Assert", "bs" + "u" * n) for n in range(3)]
TRACE_R = [R("CTRACE", "us" + "u" * n) for n in range(3)]
GETID_R = R("GetThingId", "xL")
STAND_R = R("GetStandPoint", "Li")


def n_conv(e):
    return min(2, text(e, e.r(4)).count("%"))


def stub_assert(e):
    ASSERT_R[n_conv(e)](e)


def stub_trace(e):
    TRACE_R[n_conv(e)](e)


def stub_getid(e):
    GETID_R(e)
    label = e.cl.lab(e.r(4))
    e.write(e.r(3), struct.pack(">H", (0x1230 + 0x11 * label) & 0xFFFF))


def stub_standpoint(e):
    v = e.cl.ret("StandPoint") & MASK
    first, second = e.r(5), e.r(6)
    STAND_R(e)
    e.write(first, struct.pack(">H", v >> 16))
    e.write(second, struct.pack(">H", v & 0xFFFF))


STUBS.internal(0x1000DDE8, stub_assert)
STUBS.internal(0x1006FCBC, stub_trace)
STUBS.internal(0x1012E1F8, stub_getid)
STUBS.internal(0x1010A4FC, stub_standpoint)
STUBS.internal(0x10114FA8, R("PeepWalks", "L", "i"))
STUBS.internal(0x101154F0, R("SetDest", "Lww", "i"))
STUBS.internal(0x10120354, R("SetState", "Li"))
STUBS.internal(0x1011DBF4, R("LeaveQueue", "L"))
STUBS.internal(0x1011EF14, R("FindQueueDestination", "L", "i"))
STUBS.internal(0x10117E64, R("SetSpotAnim", "Li"))
STUBS.internal(0x1013B1E4, R("GetRand", "L", "i"))
STUBS.internal(0x10103010, R("PlaceLitter", "uu"))
STUBS.internal(0x100D668C, R("GetScript", "i", lambda e, log: SCRIPT_PTR))
STUBS.internal(0x100D6734, R("Setvar", "uii"))
STUBS.internal(0x1010D3EC, R("ReadyToAdmit", "Lw", "b"))
STUBS.internal(0x1010D464, R("PersonLeftDuringAdmittance", "Lw"))
STUBS.internal(0x1010C900, R("IsDirty", "L", "i"))
STUBS.internal(0x10108F5C, R("GetPositionInQueue", "Lw", "i"))
STUBS.internal(0x1010C878, R("IsBrokenDown", "L", "b"))
STUBS.internal(0x10108624, R("GetIdealQueueLength", "L", "i"))
STUBS.internal(0x10110CD8, R("IsCoaster", "L", "i"))
STUBS.internal(0x10052954, R("IsCoasterUseable", "L", "i"))
STUBS.internal(0x10107CD4, R("GetDBInfo", "L", lambda e, log: CUR["db"]))
STUBS.internal(0x1010D35C, R("FinishedAdmitting", "L", "i"))
STUBS.internal(0x10108C20, R("RemovePersonFromQueue", "Lw"))
STUBS.internal(0x1012F0D8, R("ShowThoughtBubble", "Lib"))
STUBS.internal(0x1012F3D4, R("RecordEvent", "Liw"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def put(self, a, kind, v):
        size = struct.calcsize(kind)
        self.write(a, struct.pack(self.fmt[0] + kind, v & ((1 << (8 * size)) - 1)))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def setup_blocks(self):
        self.person = self.alloc(0x240)
        self.obj = self.alloc(0x60)
        self.db = self.alloc(self.rec_size())
        self.turnblk = self.alloc(16)
        self.world = (self.turnblk - 0x1DA70C) & 0xFFFFFFFFFFFFFFFF
        self.fixed = {1: self.person, 2: self.obj, 3: self.person + 0x30, 4: self.db, 5: self.world}
        self.used = []


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.setup_blocks()
        self.world &= MASK
        self.fixed[5] = self.world
        CUR["db"] = self.db
        self.L = e.cl = CL.PpcLog({v: k for k, v in self.fixed.items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def setptrs(self):
        self.words(ADDR["gworld"], [self.world])
        self.words(ADDR["gmap"], [MAP])

    def rec(self, mac):
        return mac

    def rec_size(self):
        return 0xA0

    def thing(self, index, address):
        self.used.append(index)
        self.words(THINGS + index * 0x14, [address])

    def call(self, op):
        entry = {"walk": FN_WALK, "queue": FN_QUEUE, "script": FN_SCRIPT}[op]
        T.call_ppc(self.e, entry, [self.person])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.setup_blocks()
        CL.host_begin(lib, self.fixed, case["queues"])

    def done(self):
        for i in self.used:
            self.lib.tpw_u5529_thing(i, 0)

    def g(self, name):
        return getattr(self.lib, P + "addr")(name.encode())

    def alloc(self, n):
        return getattr(self.lib, P + "alloc")(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setptrs(self):
        self.lib.tpw_u5529_set(self.world, MAP, self.db)

    def rec(self, mac):
        return ridelayout.host_offset(self.lib, mac)

    def rec_size(self):
        return max(0xA0, ridelayout.host_size(self.lib))

    def thing(self, index, address):
        self.used.append(index)
        self.lib.tpw_u5529_thing(index, address)

    def call(self, op):
        getattr(self.lib, P + op)(self.person)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, P + "addr").restype, getattr(lib, P + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, P + "alloc").restype, getattr(lib, P + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, P + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_u5529_set.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_u5529_thing.argtypes = [C.c_uint32, C.c_uint64]
    ridelayout.bind(lib)
    for op in ("walk", "queue", "script"):
        getattr(lib, P + op).argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, P + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

FIELDS = [("b4", 0x4, "B"), ("b6", 0x6, "B"), ("w1c", 0x1C, "I"), ("f19c", 0x19C, "I"), ("f1ac", 0x1AC, "I"), ("f1b4", 0x1B4, "I"),
          ("ride", 0x1DC, "H"), ("q1f1", 0x1F1, "B"), ("w1f4", 0x1F4, "I"), ("w1f8", 0x1F8, "I"), ("w1fc", 0x1FC, "I"),
          ("w208", 0x208, "I"), ("h228", 0x228, "H")]
OBJ_FIELDS = [("o20", 0x20, "I"), ("o28", 0x28, "I"), ("o2e", 0x2E, "H")]


def build(s, case):
    s.setptrs()
    s.put(s.g("kinds"), "I", case["kinds"])
    s.put(s.g("ffc"), "H", case["ffc"])
    s.put(s.g("ffe"), "H", case["ffe"])
    s.thing(0, s.person if case["thing0"] else 0)
    s.thing(case["ride"], s.obj)
    for name, off, kind in FIELDS:
        s.put(s.person + off, kind, case[name])
    for name, off, kind in OBJ_FIELDS:
        s.put(s.obj + off, kind, case[name])
    s.put(s.db + s.rec(0x9C), "I", case["db9c"])
    s.put(s.turnblk, "I", case["turn"])


def observe(s, case):
    return {"log": s.log(), "words": [s.rd_word(s.person + o) for o in (0x1C, 0x1B4, 0x1F4, 0x1F8)]}


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


FLOATS = [0.0, 5.0, 9.99, 10.0, 19.0, 19.99, 20.0, 80.0, 80.5, 81.0, 100.0, 255.0, 256.0, 266.0, 336.0, 337.0, -1.0, -200.0, 1e6, 0.5]


def rfl(r):
    if r.random() < 0.7:
        return fb(r.choice(FLOATS))
    return fb(r.uniform(-300, 500))


def base(r, op):
    q = r.choice([0, 0, 1, 2, 3, 5, 200, 255])
    ride = r.choice([1, 2, 7, 0x27FF, r.randrange(1, 0x2800)])
    turn = r.choice([10000, r.randrange(1 << 32)])
    pos = r.choice([q, q, q, q, q + 1, q - 1, q + 2, q + 3, q - 2, 0xFFFEFFFF, 0xFFFFFFFF, r.randrange(300)])
    first = r.randrange(0, (1 << 32) // 10) * 10
    queues = {
        "PeepWalks": [r.choice([0, 0, 1, 2, 2, 3, -1])],
        "SetDest": [r.choice([0, 1])],
        "FindQueueDestination": [r.choice([0, 1])],
        "GetRand": [s32(first if r.random() < 0.5 else r.randrange(1 << 32)), s32(r.randrange(1 << 32))],
        "ReadyToAdmit": [r.choice([0, 1]), r.choice([0, 1])],
        "StandPoint": [s32(r.randrange(1 << 32))],
        "IsDirty": [r.choice([0, 0, 0, 1])],
        "GetPositionInQueue": [s32(pos)],
        "IsBrokenDown": [r.choice([0, 1])],
        "GetIdealQueueLength": [s32(r.choice([q, q - 1, q + 1, 100, r.randrange(300), 0]))],
        "IsCoaster": [r.choice([0, 1])],
        "IsCoasterUseable": [r.choice([0, 1])],
        "FinishedAdmitting": [r.choice([0, 1])],
    }
    h228 = r.randrange(0x10000)
    return {
        "op": op, "queues": queues,
        "thing0": 1 if r.random() < 0.1 else 0,
        "ride": ride, "kinds": r.choice([6, 6, 1, 2, 0, 7, 0xFFFFFFFF, r.randrange(1 << 32)]),
        "ffc": r.choice([ride, ride, 0, r.randrange(0x10000)]),
        "ffe": r.choice([h228, h228, 0, r.randrange(0x10000)]),
        "b4": r.randrange(256), "b6": r.randrange(256),
        "w1c": r.choice([0, 0x7FF, 0x400, r.randrange(0x800)]),
        "f19c": rfl(r), "f1ac": rfl(r), "f1b4": r.choice([0, fb(1.5), r.randrange(1 << 32)]),
        "q1f1": q, "w1f4": r.choice([0, 1, 2, 5, 0xFFFFFFFF]), "w1f8": r.choice([0, 0, 1, 5, 0xFFFFFFFF]),
        "turn": turn,
        "w208": (turn - r.choice([0, 29, 30, 31, 100, 5000, -5])) & MASK,
        "w1fc": (turn - r.choice([0, 50, 99, 100, 101, 300, -5])) & MASK,
        "h228": h228,
        "o20": r.randrange(1 << 32), "o28": r.choice([0, 1, r.randrange(1 << 32)]), "o2e": r.randrange(0x10000),
        "db9c": r.choice([1, 0, 2, r.randrange(1 << 32)]),
    }


def make(r, op, queues=None, **kw):
    c = base(r, op)
    if queues:
        for k, v in queues.items():
            c["queues"][k] = [s32(x) for x in v]
    c.update(kw)
    return c


# ---- walk
def walk_edges():
    r = random.Random(11)
    for walks, same, kinds, rand in product([0, 1, 2, 3], [True, False], [6, 1, 2, 0, 7, 0xFFFFFFFF], [0, 5, 0x7FFFFFFF, 0xFFFFFFFF]):
        c = make(r, "walk", {"PeepWalks": [walks], "GetRand": [rand]}, kinds=kinds)
        c["ffc"] = c["ride"] if same else (c["ride"] + 1) & 0xFFFF
        yield c
    for thing0, b4, b6 in product([0, 1], [0, 255], [0, 255]):
        c = make(r, "walk", {"PeepWalks": [0]}, thing0=thing0, b4=b4, b6=b6, ffc=0)
        yield c


def walk_random(r):
    return make(r, "walk")


# ---- script
def script_edges():
    r = random.Random(12)
    for fin, same, thing0 in product([0, 1], [True, False], [0, 1]):
        c = make(r, "script", {"FinishedAdmitting": [fin]}, thing0=thing0)
        c["ffe"] = c["h228"] if same else (c["h228"] + 1) & 0xFFFF
        yield c


def script_random(r):
    return make(r, "script")


# ---- queue
def queue_edges():
    r = random.Random(13)
    quiet = {"IsDirty": [0], "IsBrokenDown": [1], "IsCoaster": [0], "GetIdealQueueLength": [100]}
    # the admit start (place 0 and the flag set)
    for w1f8, ready, sd, stand in product([0, 1, 0xFFFFFFFF], [(0, 0), (1, 1), (0, 1), (1, 0)], [0, 1], [0x01020304, 0xFFFF0000]):
        yield make(r, "queue", dict(quiet, ReadyToAdmit=ready, SetDest=[sd], StandPoint=[stand], GetPositionInQueue=[0]), q1f1=0, w1f8=w1f8)
    for q1f1, w1f8 in product([1, 255], [0, 1]):
        yield make(r, "queue", dict(quiet, ReadyToAdmit=[0, 0], GetPositionInQueue=[q1f1]), q1f1=q1f1, w1f8=w1f8)
    # dirty, place lost
    for dirty, pos in product([0, 1], [0xFFFEFFFF, 0xFFFFFFFF, 0xFFFF0000, 0xFFFFFFFE, 0x0001FFFF, 0]):
        yield make(r, "queue", dict(quiet, IsDirty=[dirty], GetPositionInQueue=[pos]), q1f1=3, w1f8=0)
    # the place is the right one: the queue too long, the coaster and ride record checks
    for q1f1, ideal, coaster, useable, db9c, o28 in product([2, 5], [1, 5, 9], [0, 1], [0, 1], [1, 0, 2], [0, 7]):
        yield make(r, "queue", dict(quiet, GetPositionInQueue=[q1f1], GetIdealQueueLength=[ideal], IsCoaster=[coaster],
                                    IsCoasterUseable=[useable]), q1f1=q1f1, w1f8=0, db9c=db9c, o28=o28,
                   w208=0, turn=0, w1fc=0)
    # the place is another one: patience, a broken ride, the queue destination
    for q1f1, pos, w1f4, broken, find in product([5], [5, 4, 3, 2, 6, 7, 0], [0, 1, 2], [0, 1], [0, 1]):
        yield make(r, "queue", dict(quiet, GetPositionInQueue=[pos], IsBrokenDown=[broken], FindQueueDestination=[find]),
                   q1f1=q1f1, w1f4=w1f4, w1f8=0, turn=0, w208=0, w1fc=0)
    # the timers: boredom, the wander, the happiness
    for d208, d1fc, rand, w1c in product([0, 30, 31], [100, 101, 0], [0, 10, 7, 20], [0, 0x7FF, 0x500, 0x3FF]):
        turn = 100000
        yield make(r, "queue", dict(quiet, GetPositionInQueue=[1], GetRand=[rand, 0]), q1f1=1, w1f8=0,
                   turn=turn, w208=turn - d208, w1fc=turn - d1fc, w1c=w1c)
    for rand2, w1c in product([0, 399, 400, 401, 799, 5000], [0, 0x7FF, 0x7FE, 0x1, 0x7FF - 399, 0x7FF - 400]):
        yield make(r, "queue", dict(quiet, GetPositionInQueue=[1], GetRand=[10, rand2]), q1f1=1, w1f8=0,
                   turn=1000, w208=1000, w1fc=1000, w1c=w1c)
    for f19c, f1ac, o2e in product(FLOATS, [0.0, 80.0, 81.0, 100.0, 336.0, 337.0], [0, 1, 2, 0xFFFF]):
        yield make(r, "queue", dict(quiet, GetPositionInQueue=[1]), q1f1=1, w1f8=0, turn=1000, w208=0, w1fc=1000,
                   f19c=fb(f19c), f1ac=fb(f1ac), o2e=o2e)


def queue_random(r):
    return make(r, "queue")


TESTS = [
    FnTest("CPerson::ProcessStateWalksToLitterBin", ppc, host, edge=walk_edges, rnd=walk_random),
    FnTest("CPerson::ProcessStateStandingInQueue", ppc, host, edge=queue_edges, rnd=queue_random),
    FnTest("CPerson::ProcessStateWaitingForScriptToAdmitMe", ppc, host, edge=script_edges, rnd=script_random),
]
