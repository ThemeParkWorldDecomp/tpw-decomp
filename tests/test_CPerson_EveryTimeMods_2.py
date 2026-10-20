"""Unit v51016, the person's per-use bookkeeping and minor-destination search, against libtpw_unit_CPerson_EveryTimeMods_2.dylib
(tests/unit_CPerson_EveryTimeMods_2.cpp), main app ("SimTheme Park"),
src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_CPerson_EveryTimeMods_2.cpp:
  CPerson::EveryTimeMods 0x1011a364, UpdateTemporaryHistory 0x10119b68, UseObject 0x101196a0, GetTimeInPark 0x1011962c,
  LookForMinorDest 0x1011915c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (the object's
GetDBInfo, HadCustomer, HadAWinner, ExpressSatisfaction, GetBackOfQueue, IsAvailableToChoose, the person's CalculateOptionScore, PayPrice,
WinningMods, LosingMods, AddRideToRecentHistory, the analyser, the ride database, the thoughts, GetThingId, MAP_count_walking_distance, SetDest,
CTRACE and GAME_ASSERTMSG) is a fake on both sides that records its arguments (objects as their offset in the case's heap block) and answers from
the case's queues. GetWorld runs as the original on the PowerPC side and is a plain fake on the host. EveryTimeMods called by UseObject is the
unit's own function on both sides.

The world of a case: a heap block (the person at +0, objects from +0x400 in 0x80 steps, two ride records at +0x1000 and +0x1800, the analyser
at +0x2400), the world block (the turn at +0x1da70c), the map (the cell owners at +0x24 of 0x44 byte cells), the thing array rows, the five
"none" halfwords. What is compared: the answer, the event log and the person's fields (words, halfwords, bytes, floats canonical).

No fused operations. Not tested: float values outside the int range for UseObject's fctiwz (Unicorn and the hardware answer differently),
a thing row that is NULL where the original dereferences it, circular object lists (the original would not end).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPerson_EveryTimeMods_2"
MASK = 0xFFFFFFFF

FN = dict(every=0x1011a364, uth=0x10119b68, use=0x101196a0, gtip=0x1011962c, lfmd=0x1011915c)
SIZES = {0x1011a364: 276, 0x10119b68: 108, 0x101196a0: 864, 0x1011962c: 68, 0x1011915c: 1184}
OWN = set(SIZES)

POOL = 0x102442BE
RIDE_DB = 0x1037F6C8
ADDR = dict(gWorld=0x1037EC0C, gWorldMap=0x1037EC08, gThingArray=0x1034CBFC, NoRide=0x102A0FB0, NoQueue=0x102A0FB2,
            ListEnd=0x102A0FB4, NoDest=0x102A0FB6, NoDest2=0x102A0FB8)
HOST_LAYOUT = [48, 0, 0x1DA70C, 8]
HEAP = [0]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


def lab(v):
    v &= MASK
    return 0xFFFFFFFF if v == 0 else (v - HEAP[0]) & MASK


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def reg(addr, name, args, ret=None, out=None):
    def fn(e):
        e.cl.rec(name, tuple(args(e)))
        if out:
            out(e)
        if ret:
            e.set_r(3, ret(e) & MASK)
    STUBS.internal(addr, fn)


def q(e, key):
    return e.cl.ret(key)


def write_half(e, addr, v):
    e.write(addr & MASK, struct.pack(">H", v & 0xFFFF))


reg(0x10107cd4, "GetDBInfo", lambda e: (lab(e.r(3)),), ret=lambda e: HEAP[0] + 0x1000)
reg(0x1010ef0c, "HadCustomer", lambda e: (lab(e.r(3)),))
reg(0x1010f4b8, "HadAWinner", lambda e: (lab(e.r(3)),))
reg(0x1010f974, "ExpressSatisfaction", lambda e: (lab(e.r(3)), s32(e.r(4))))
reg(0x101092f4, "GetBackOfQueue", lambda e: (lab(e.r(4)),), out=lambda e: write_half(e, e.r(3), q(e, "GetBackOfQueue")))
reg(0x101083e0, "IsAvailableToChoose", lambda e: (lab(e.r(3)),), ret=lambda e: q(e, "IsAvailableToChoose"))
reg(0x101185f8, "CalculateOptionScore", lambda e: (lab(e.r(3)), lab(e.r(4))), ret=lambda e: q(e, "CalculateOptionScore"))
reg(0x1011a4ac, "PayPricePerUseForObject", lambda e: (lab(e.r(3)), lab(e.r(4))))
reg(0x1011a570, "WinningMods", lambda e: (lab(e.r(3)), lab(e.r(4))))
reg(0x1011b004, "LosingMods", lambda e: (lab(e.r(3)), lab(e.r(4))))
reg(0x10119a30, "AddRideToRecentHistory", lambda e: (lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))
reg(0x1013ebc8, "GetParkAnalyser", lambda e: (1 if e.r(3) == e.u32(ADDR["gWorld"]) else 0,), ret=lambda e: HEAP[0] + 0x2400)
reg(0x100e4360, "OfferOpinionOn", lambda e: (lab(e.r(3)), s32(e.r(4)), e.r(5) & 0xFF))
reg(0x10154868, "GetRideInfo", lambda e: (1 if e.r(3) == RIDE_DB else 0, (e.r(4) >> 16) & 0xFFFF), ret=lambda e: HEAP[0] + 0x1800)
reg(0x1012f0d8, "ShowThoughtBubble", lambda e: (lab(e.r(3)), s32(e.r(4)), e.r(5) & 0xFF))
reg(0x1012f3d4, "RecordEvent", lambda e: (lab(e.r(3)), s32(e.r(4)), (e.r(5) >> 16) & 0xFFFF))
reg(0x1012e1f8, "GetThingId", lambda e: (lab(e.r(4)),), out=lambda e: write_half(e, e.r(3), q(e, "GetThingId")))
reg(0x10102394, "MAP_count_walking_distance", lambda e: ((e.r(3) >> 16) & 0xFFFF, (e.r(4) >> 16) & 0xFFFF, s32(e.r(5))),
    ret=lambda e: q(e, "MAP_count_walking_distance"))
reg(0x10115400, "SetDest", lambda e: (lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF), ret=lambda e: q(e, "SetDest"))


def ctrace(e):
    off = (e.r(4) - POOL) & MASK
    ints = []
    if off == 0x51F:
        ints = [s32(e.r(5)), s32(e.r(6)), s32(e.r(7))]
    elif off == 0x4F4:
        ints = [s32(e.r(5)), s32(e.r(6)), s32(e.r(7)), s32(e.r(8))]
    e.cl.rec("CTRACE", tuple([e.r(3) & MASK, off] + ints))


STUBS.internal(0x1006FCBC, ctrace)
STUBS.internal(0x1000DDE8, lambda e: e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)), (e.r(4) - POOL) & MASK)))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def byte(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def rd_byte(self, a):
        return self.read(a, 1)[0]


class Ppc(Side):
    fmt = ">I"
    stride = 0x14

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    def ptr(self, a, v):
        self.words(a, [v])

    def set_heap(self, h):
        HEAP[0] = h

    def log(self):
        return self.L.log

    def call(self, name, args):
        r3, _, _ = T.call_ppc(self.e, FN[name], [a & MASK for a in args])
        return r3 & MASK


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
        return self.lib.tpw_cem2_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cem2_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def set_heap(self, h):
        self.lib.tpw_cem2_setheap(h)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, name, args):
        r = getattr(self.lib, "tpw_cem2_" + name)(*args)
        return 0 if r is None else r & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cem2_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "setheap").argtypes = [C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    for n, k in (("every", 2), ("use", 2), ("uth", 1), ("lfmd", 1)):
        f = getattr(lib, p + n)
        f.restype, f.argtypes = None, [C.c_uint64] * k
    getattr(lib, p + "gtip").restype, getattr(lib, p + "gtip").argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.stride = out[0]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

OBJ = 0x400
OBJ_STEP = 0x80
PW_OBS = [0x188, 0x19C, 0x1A0, 0x1B8, 0x1C4, 0x1C8, 0x1CC, 0x1D4, 0x20C]
PF_OBS = {0x19C, 0x1B8, 0x20C}
PH_OBS = [0x1DC, 0x1DE, 0x1E6, 0x1E8, 0x1EA, 0x1EC, 0x1EE]
PB_OBS = [4, 6, 0x1F1]


def build(s, case):
    heap = s.alloc(0x2800)
    s.heap = heap
    s.set_heap(heap)
    world = s.alloc(0x1DA720)
    s.ptr(s.g("gWorld"), world)
    s.words(world + 0x1DA70C, [case["turn"]])
    for off, v in case["pw"].items():
        s.words(heap + off, [v])
    for off, v in case["ph"].items():
        s.half(heap + off, v)
    for off, v in case["pb"].items():
        s.byte(heap + off, v)
    for k, ob in enumerate(case["objs"]):
        base = heap + OBJ + k * OBJ_STEP
        for off, v in ob["b"].items():
            s.byte(base + off, v)
        for off, v in ob["h"].items():
            s.half(base + off, v)
    for off, v in case["ra"].items():
        s.words(heap + 0x1000 + off, [v])
    for off, v in case["rb"].items():
        s.words(heap + 0x1800 + off, [v])
    for name, v in zip(("NoRide", "NoQueue", "ListEnd", "NoDest", "NoDest2"), case["sent"]):
        s.half(s.g(name), v)
    mp = s.alloc(0x110000)
    s.ptr(s.g("gWorldMap"), mp)
    for cell, who in case["cells"].items():
        s.half(mp + (cell - 1) * 0x44 + 0x24, who)
    arr = s.g("gThingArray")
    s.ptr(arr, heap + OBJ if case["row0"] else 0)
    for i, k in case["things"].items():
        s.ptr(arr + i * s.stride, heap + OBJ + k * OBJ_STEP)


def execute(s, case):
    person, obj = s.heap, s.heap + OBJ
    op = case["op"]
    if op in ("every", "use"):
        s.call(op, [person, obj])
        return 0
    answer = s.call(op, [person])
    return answer if op == "gtip" else 0


def observe(s, case, answer):
    p = s.heap
    out = {"answer": answer, "log": s.log()}
    for off in PW_OBS:
        v = s.rd_word(p + off)
        out["w%x" % off] = canon_f(v) if off in PF_OBS else v
    for off in PH_OBS:
        out["h%x" % off] = s.rd_half(p + off)
    for off in PB_OBS:
        out["b%x" % off] = s.rd_byte(p + off)
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def ri(r):
    return r.choice([0, 1, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, r.randrange(0, 50), r.getrandbits(32)])


def rfloat(r, lo=-150, hi=300):
    k = r.random()
    if k < 0.1:
        return r.choice([0, 0x80000000, fb(100.0), fb(0.5), fb(-0.5), fb(255.0), fb(256.0), fb(-1.0)])
    return fb(r.uniform(lo, hi))


def queues(r, **kw):
    qs = {
        "GetBackOfQueue": [r.choice([0, 0xFFFF, r.randrange(1, 16385)]) for _ in range(4)],
        "IsAvailableToChoose": [r.choice([0, 1, 1, 1]) for _ in range(12)],
        "CalculateOptionScore": [r.choice([-3, 0, 1, 2, 3, 3, 5, 10]) for _ in range(12)],
        "GetThingId": [r.choice([0, 1, 0x1234, 0xFFFF, r.randrange(0, 0x10000)]) for _ in range(6)],
        "MAP_count_walking_distance": [r.choice([-1, 3, 5, 5, 10, 20]) for _ in range(4)],
        "SetDest": [r.choice([0, 1])],
    }
    qs.update(kw)
    return qs


def base(r, op):
    return {"op": op, "turn": r.choice([0, 1, 2, 19, 20, 21, r.getrandbits(32), r.randrange(0, 200)]), "queues": queues(r),
            "pw": {}, "ph": {}, "pb": {}, "objs": [{"b": {}, "h": {0xE: r.randrange(0, 0x10000)}}], "ra": {}, "rb": {},
            "sent": (0, 0, 0, 0, 0), "cells": {}, "things": {}, "row0": False}


# ---- EveryTimeMods
UIS = [-1, 0, 1, 2, 3]


def every_case(r, ui=None, happy=None, cost=None):
    c = base(r, "every")
    c["pw"] = {0x1B8: rfloat(r, -50, 150) if happy is None else happy, 0x1C4: ri(r), 0x1C8: ri(r), 0x1CC: ri(r)}
    c["ra"] = {0x7A8: r.choice(UIS) & MASK if ui is None else ui & MASK,
               0xE8: (r.choice([0, 1, -1, 50, 100, -50, 0x7FFFFFFF, -0x80000000, 16777217, r.randrange(-200, 200)]) if cost is None else cost) & MASK}
    return c


def every_edges():
    r = random.Random(101)
    pairs = [(fb(0.0), 0), (fb(100.0), 0), (fb(50.0), 50), (fb(50.0), -50), (fb(50.0), -51), (fb(50.0), 51), (fb(0.0), 1), (fb(0.0), -1),
             (0x80000000, 0), (0x7FC00000, 5), (fb(40.0), 0x7FFFFFFF), (fb(40.0), -0x80000000), (fb(100.0), 16777217), (fb(99.5), -1)]
    for ui in UIS:
        for h, cst in pairs:
            yield every_case(r, ui, h, cst)


def every_random(r):
    return every_case(r)


# ---- UpdateTemporaryHistory
def uth_case(r, turn=None):
    c = base(r, "uth")
    if turn is not None:
        c["turn"] = turn
    else:
        c["turn"] = r.choice([r.randrange(0, 50) * 20, r.getrandbits(32), r.randrange(0, 500) * 20 + r.randrange(0, 20)])
    c["ph"] = {off: r.getrandbits(16) for off in (0x1E6, 0x1E8, 0x1EA, 0x1EC, 0x1EE)}
    return c


def uth_edges():
    r = random.Random(102)
    for t in (0, 1, 19, 20, 21, 39, 40, 0xFFFFFFF0, 0xFFFFFFEC, 0xFFFFFFFF, 0xCCCCCCCD, 0x80000000, 100):
        yield uth_case(r, t)


def uth_random(r):
    return uth_case(r)


# ---- GetTimeInPark
def gtip_case(r, turn=None, stamp=None):
    c = base(r, "gtip")
    c["turn"] = r.choice([0, r.getrandbits(32), r.randrange(0, 2000)]) if turn is None else turn
    c["pw"] = {0x1D4: r.choice([0, r.getrandbits(32), r.randrange(0, 2000)]) if stamp is None else stamp}
    return c


def gtip_edges():
    r = random.Random(103)
    for t, st in ((0, 0), (3, 0), (4, 0), (7, 0), (8, 0), (0, 1), (5, 5), (0xFFFFFFFF, 0), (0, 0xFFFFFFFF), (100, 0xFFFFFFF0), (0x80000000, 1)):
        yield gtip_case(r, t, st)


def gtip_random(r):
    return gtip_case(r)


# ---- UseObject
def use_case(r, winner=None, ui=None, a=None, b=None, rb=None, f19c=None, f20c=None):
    c = every_case(r)
    c["pb"] = {0x1F1: r.choice([0, 1, 2, 0xFF]) if winner is None else winner}
    c["pw"][0x19C] = rfloat(r, -300, 600) if f19c is None else f19c
    c["pw"][0x20C] = rfloat(r, -300, 600) if f20c is None else f20c
    c["op"] = "use"
    c["ra"][0x7A8] = (r.choice(UIS) if ui is None else ui) & MASK
    c["ra"][0x158] = (r.choice([-1, 0, 1, 2, 3, 4, 5]) if a is None else a) & MASK
    c["ra"][0x15C] = (r.choice([-1, 0, 1, 2, 3, 4]) if b is None else b) & MASK
    c["rb"] = {0x7A8: (r.choice([2, 2, 0, 1, -1]) if rb is None else rb) & MASK}
    return c


def use_edges():
    r = random.Random(104)
    for win in (1, 0):
        for ui in UIS:
            for rb in (2, 1):
                yield use_case(r, win, ui, 1, 1, rb)
        for a in range(-1, 6):
            for b in range(-1, 5):
                yield use_case(r, win, 1, a, b, 2 if (a + b) & 1 else 0)
    for f1, f2 in ((fb(10.0), fb(10.0)), (fb(300.0), fb(1.0)), (fb(1.0), fb(300.0)), (fb(-1.5), fb(0.5)), (fb(255.9), fb(256.1)), (0, 0),
                   (fb(-256.0), fb(255.0))):
        yield use_case(r, 1, 1, 1, 1, 2, f1, f2)


def use_random(r):
    return use_case(r)


# ---- LookForMinorDest
def lfmd_case(r, sent=None, ride_none=False, back_none=False, dist=None, scores=None, avail=None, row0=None):
    c = base(r, "lfmd")
    if sent is None:
        sent = tuple(r.choice([0, 0, 0, 0xFFFF]) for _ in range(5))
        if r.random() < 0.3:
            sent = sent[:4] + (r.randrange(1, 8),)
    c["sent"] = sent
    end = sent[2]
    # a "none" destination other than 0 lets the original read thing row 0 (bestId stays 0): that row must exist
    c["row0"] = True if sent[3] != 0 else (r.random() < 0.2 if row0 is None else row0)
    cell = r.randrange(1, 16385)
    b4, b6 = (cell - 1) & 0x7F, (cell - 1) >> 7
    if r.random() < 0.1:
        b4 = r.randrange(0, 256)
    here = (b4 + b6 * 128 + 1) & 0xFFFF
    x0, y0 = (here - 1) & 0x7F, (here - 1) >> 7
    c["pb"] = {4: b4, 6: b6}
    ride = r.randrange(1, 8)
    if ride_none:
        ride = sent[0]
    c["ph"] = {0x1DC: ride, 0x1DE: r.getrandbits(16)}
    c["pw"] = {0x188: r.randrange(0, 4)}
    objs = []
    for k in range(8):
        kind = 3 if r.random() < 0.85 else r.choice([2, 4, 0])
        nxt = r.choice([end] + list(range(k + 1, 8))) if k >= 1 else end
        objs.append({"b": {2: kind, 4: r.randrange(0, 128), 6: r.randrange(0, 128)},
                     "h": {0xA: nxt, 0xE: r.randrange(0, 0x10000), 0x32: r.randrange(0, 0x10000)}})
    c["objs"] = objs
    c["things"] = {k: k for k in range(1, 8)}
    cells = {}
    for cx in range(x0 - 2, x0 + 2):
        for cy in range(y0 - 2, y0 + 2):
            if 0 <= cx < 128 and 0 <= cy < 128:
                cells[cx + cy * 128 + 1] = r.choice([end, end] + list(range(1, 8)))
    c["cells"] = cells
    back = sent[1] if back_none else r.choice([min(16384, max(1, here + r.randrange(-300, 300))), r.randrange(1, 16385), 0, here])
    c["queues"]["GetBackOfQueue"] = [back, back]
    if dist is not None:
        c["queues"]["MAP_count_walking_distance"] = list(dist)
    else:
        c["queues"]["MAP_count_walking_distance"] = [r.choice([-1, 3, 5, 10, 20]), r.choice([-1, 2, 5, 10])]
    if scores is not None:
        c["queues"]["CalculateOptionScore"] = list(scores)
    if avail is not None:
        c["queues"]["IsAvailableToChoose"] = list(avail)
    return c


def lfmd_edges():
    r = random.Random(105)
    yield lfmd_case(r, ride_none=True)
    yield lfmd_case(r, sent=(0xFFFF, 0, 0, 0, 0xFFFF), ride_none=True)
    yield lfmd_case(r, back_none=True)
    yield lfmd_case(r, sent=(0, 0xFFFF, 0, 0, 0), back_none=True)
    for sent in ((0, 0, 0, 0, 0), (0, 0, 0xFFFF, 0xFFFF, 0xFFFF), (0, 0xFFFF, 0, 0xFFFF, 0), (0xFFFF, 0xFFFF, 0xFFFF, 0, 0xFFFF)):
        for row0 in (False, True):
            for dist in ((10, 5), (5, 10), (5, 5), (-1, 5), (5, -1), (-1, -1), (7, 3)):
                yield lfmd_case(r, sent=sent, dist=dist, row0=row0, avail=[1] * 12, scores=[3, 3, 5, 0, 2, 3, 1, 3, 0, 1, 2, 3])
    for sc in ([0] * 12, [-2] * 12, [1, 1, 1, 1, 1, 1], [5, 4, 3, 2, 1, 0]):
        for turn in (0, 1):
            c = lfmd_case(r, sent=(0, 0, 0, 0, 0), dist=(9, 2), avail=[1] * 12, scores=sc)
            c["turn"] = turn
            yield c


def lfmd_random(r):
    return lfmd_case(r)


TESTS = [
    FnTest("CPerson::EveryTimeMods", ppc, host, edge=every_edges, rnd=every_random),
    FnTest("CPerson::UpdateTemporaryHistory", ppc, host, edge=uth_edges, rnd=uth_random),
    FnTest("CPerson::UseObject", ppc, host, edge=use_edges, rnd=use_random),
    FnTest("CPerson::GetTimeInPark", ppc, host, edge=gtip_edges, rnd=gtip_random),
    FnTest("CPerson::LookForMinorDest", ppc, host, edge=lfmd_edges, rnd=lfmd_random),
]
