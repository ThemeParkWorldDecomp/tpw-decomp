"""Unit v51013, the entrance camera and the collision object helpers, against libtpw_unit_GetThingAtPosnIfEntrance.dylib
(tests/unit_GetThingAtPosnIfEntrance.cpp), main app ("SimTheme Park"), src/lobby_exit_IsRideRideable_GetThingAtPosnIfEntrance.cpp:
  GetThingAtPosnIfEntrance 0x10032618, MoveCameraOverRideExit 0x10032838, FreeTrackForRide 0x1002ef00,
  BMP_StartColObject 0x1002edfc, BMP_StartColObjectForRide 0x1002ed74

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (GAME_ASSERTMSG, TRACE, CMapCell::IsEntrance, CObject::GetDBInfo, CCellBase::GetDirection, GetThingAtPosnIfEntrance(float, float))
are fakes on both sides that record their arguments (cells as their index in the map, things by a tag word at +0x20) and answer from the
case's queues or from blocks the case set up.

The world of a case: the map cells (link halfword +0x10, first thing id +0x24), the thing table rows, things (kind byte +2, next id +0x0a,
cell +0x34), the terminator halfword, the camera position and angle, a ride record with its track pieces and links, the collision object
table. What is compared: the answer (an index of the thing or object built), the globals and blocks read back, the event log.

MoveCameraOverRideExit has fused multiply-adds: the PowerPC side runs under HardwareRounding. Not tested: a NULL object table walked by
BMP_StartColObjectForRide (the original writes through NULL + 0x14); a thing id past the table or a cell outside the world block.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_GetThingAtPosnIfEntrance"
MASK = 0xFFFFFFFF

FN_GET, FN_MOVE, FN_FREE, FN_START, FN_STARTRIDE = 0x10032618, 0x10032838, 0x1002EF00, 0x1002EDFC, 0x1002ED74
SIZES = {FN_GET: 308, FN_MOVE: 356, FN_FREE: 100, FN_START: 216, FN_STARTRIDE: 72}
OWN = set(SIZES)

ASSERT, TRACE, ISENTRANCE, GETDBINFO, GETDIRECTION, GETTHINGF = 0x1000DDE8, 0x1006FCFC, 0x1009C4DC, 0x10107CD4, 0x10081930, 0x1003277C

ADDR = dict(world=0x1037EBFC, end=0x1029CC8C, camera=0x1029CC78, angle=0x102B5BF4, collist=0x102B56C0)
THINGS = 0x1034CBFC
HOST_LAYOUT = [0x20, 0xD0, 0x18, 0x1C, 0x44]
WORLD_SIZE = 0x2D8 + 0x44 * 0x210
OBJ_COUNT = 0x800


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cell_index(e, p):
    return (p - (e.u32(ADDR["world"]) + 0x2D8)) // 0x44


def stub_isentrance(e):
    L = e.cl
    L.rec("IsEntrance", (s32(cell_index(e, e.r(3))),))
    e.set_r(3, L.ret("IsEntrance") & MASK)


def stub_getdbinfo(e):
    L = e.cl
    L.rec("GetDBInfo", (s32(e.u32(e.r(3) + 0x20)),))
    e.set_r(3, L.rets[L.ret("GetDBInfo") & 1])


def stub_getdirection(e):
    L = e.cl
    L.rec("GetDirection", (s32(cell_index(e, e.r(3))),))
    e.set_r(3, L.ret("GetDirection") & 0xFF)


def stub_getthingf(e):
    L = e.cl
    L.rec("GetThingAtPosn", (s32(canon_f(fb(e.f(1)))), s32(canon_f(fb(e.f(2))))))
    e.set_r(3, L.rets[2])


STUBS.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(TRACE, CL.recorder("TRACE", "s"))
STUBS.internal(ISENTRANCE, stub_isentrance)
STUBS.internal(GETDBINFO, stub_getdbinfo)
STUBS.internal(GETDIRECTION, stub_getdirection)
STUBS.internal(GETTHINGF, stub_getthingf)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


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


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.rets = {}
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

    def toword(self, a):
        return a

    def fromword(self, w):
        return w

    def set_world(self, a):
        self.words(ADDR["world"], [a])

    def set_ret(self, k, a):
        self.L.rets[k] = a

    def set_thing(self, idx, a):
        self.words(THINGS + idx * 0x14, [a])

    def call(self, fn, args):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_gtp_base()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gtp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_gtp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def toword(self, a):
        return 0 if a == 0 else a - self.base

    def fromword(self, w):
        return 0 if w == 0 else w + self.base

    def set_world(self, a):
        self.lib.tpw_gtp_set_world(a)

    def set_ret(self, k, a):
        self.lib.tpw_gtp_set_ret(k, a)

    def set_thing(self, idx, a):
        self.lib.tpw_gtp_set_thing(idx, a)

    def call(self, fn, args):
        l = self.lib
        if fn == FN_GET:
            return l.tpw_gtp_get(s32(args[0]), s32(args[1]))
        if fn == FN_MOVE:
            l.tpw_gtp_move()
            return 0
        if fn == FN_FREE:
            l.tpw_gtp_free(args[0])
            return 0
        if fn == FN_START:
            return l.tpw_gtp_start(s32(args[0]), s32(args[1]), s32(args[2]))
        l.tpw_gtp_startride(args[0], s32(args[1]), s32(args[2]), s32(args[3]))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_gtp_"
    lib.tpw_gtp_addr.restype, lib.tpw_gtp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gtp_alloc.restype, lib.tpw_gtp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gtp_base.restype, lib.tpw_gtp_base.argtypes = C.c_uint64, []
    lib.tpw_gtp_set_world.argtypes = [C.c_uint64]
    lib.tpw_gtp_set_ret.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_gtp_set_thing.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_gtp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_gtp_get.restype, lib.tpw_gtp_get.argtypes = C.c_uint64, [C.c_int32, C.c_int32]
    lib.tpw_gtp_move.argtypes = []
    lib.tpw_gtp_free.argtypes = [C.c_uint64]
    lib.tpw_gtp_start.restype, lib.tpw_gtp_start.argtypes = C.c_uint64, [C.c_int32] * 3
    lib.tpw_gtp_startride.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gtp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    world = s.alloc(WORLD_SIZE)
    s.set_world(world)
    s.labs = {}
    op = case["op"]
    if op == "get":
        A, B = s.alloc(0x120), s.alloc(0x120)
        s.words(B + 0x118, [case["flag"]])
        s.set_ret(0, A)
        s.set_ret(1, B)
        s.half(s.g("end"), case["end"])
        for idx, link, first in case["cells"]:
            cell = world + 0x2D8 + idx * 0x44
            if link is not None:
                s.half(cell + 0x10, link)
            if first is not None:
                s.half(cell + 0x24, first)
        s.set_thing(0, s.alloc(0x40) if case["entry0"] else 0)
        for n, t in enumerate(case["things"]):
            a = s.alloc(0x40)
            s.byte(a + 2, t["kind"])
            s.half(a + 0xA, t["next"])
            s.words(a + 0x20, [100 + n])
            s.set_thing(t["id"], a)
            s.labs[a] = n + 1
    elif op == "move":
        s.words(s.g("camera"), case["pos"])
        s.words(s.g("angle"), [case["angle"]])
        a = s.alloc(0x40)
        s.half(a + 0x34, case["cell"])
        s.set_ret(2, a if case["present"] else 0)
        s.labs[a] = 1
    elif op == "free":
        ride = s.ride = s.alloc(0xD0)
        s.words(s.g("collist") + 0x1C, [case["links"]])
        s.words(s.g("collist") + 0x2C, [case["pieces"]])
        s.pieces, s.links = [], []
        for n, piece in enumerate(case["tracks"]):
            a = s.alloc(0x28)
            s.pieces.append(a)
            s.labs[a] = 1 + n
            piece["addr"] = a
            piece["subaddrs"] = []
            for m, flags in enumerate(piece["subs"]):
                b = s.alloc(0x18)
                s.links.append(b)
                s.labs[b] = 100 + 10 * n + m
                piece["subaddrs"].append(b)
                s.words(b, [flags])
        for n, piece in enumerate(case["tracks"]):
            a = piece["addr"]
            subs = piece["subaddrs"]
            nxt = case["tracks"][n + 1]["addr"] if n + 1 < len(case["tracks"]) else 0
            s.words(a, [piece["flags"]])
            s.words(a + 0x1C, [s.toword(subs[0]) if subs else 0])
            s.words(a + 0x24, [s.toword(nxt)])
            for m, b in enumerate(subs):
                s.words(b + 0x14, [s.toword(subs[m + 1]) if m + 1 < len(subs) else 0])
        s.words(ride + 0xBC, [s.toword(case["tracks"][0]["addr"]) if case["tracks"] else 0])
    else:
        ride = s.ride = s.alloc(0xD0)
        s.words(ride + 0xC0, [case["head"]])
        s.words(s.g("collist") + 0x1C, [case["count"]])
        if case["table"]:
            arr = s.arr = s.alloc(OBJ_COUNT * 0x20)
            data = b"".join(struct.pack(s.fmt[0] + "8I", *w) for w in case["objects"])
            s.write(arr, data)
            s.words(s.g("collist") + 0x18, [s.toword(arr)])
        else:
            s.arr = 0
            s.words(s.g("collist") + 0x18, [0])


def objlab(s, w):
    a = s.fromword(w)
    if a == 0:
        return 0
    if s.arr <= a < s.arr + OBJ_COUNT * 0x20:
        return 1 + (a - s.arr) // 0x20
    return -1


def lab(s, w):
    a = s.fromword(w)
    return 0 if a == 0 else s.labs.get(a, -1)


def execute(s, case):
    op = case["op"]
    if op == "get":
        return s.call(FN_GET, case["args"])
    if op == "move":
        return s.call(FN_MOVE, [])
    if op == "free":
        return s.call(FN_FREE, [s.ride])
    if op == "start":
        return s.call(FN_START, case["args"])
    return s.call(FN_STARTRIDE, [s.ride] + case["args"])


def observe(s, case, answer):
    op = case["op"]
    out = {"log": s.log()}
    if op == "get":
        out["answer"] = 0 if answer == 0 else s.labs.get(answer, -1)
    elif op == "move":
        out["pos"] = [canon_f(w) for w in s.rd_words(s.g("camera"), 3)]
        out["angle"] = canon_f(s.rd_word(s.g("angle")))
    elif op == "free":
        out["head"] = lab(s, s.rd_word(s.ride + 0xBC))
        out["counts"] = [s.rd_word(s.g("collist") + 0x1C), s.rd_word(s.g("collist") + 0x2C)]
        out["pieces"] = [[s.rd_word(a), lab(s, s.rd_word(a + 0x1C)), lab(s, s.rd_word(a + 0x24))] for a in s.pieces]
        out["links"] = [[s.rd_word(b), lab(s, s.rd_word(b + 0x14))] for b in s.links]
    else:
        out["count"] = s.rd_word(s.g("collist") + 0x1C)
        if op == "start":
            out["answer"] = 0 if answer == 0 else 1 + (answer - s.arr) // 0x20
        else:
            out["head"] = objlab(s, s.rd_word(s.ride + 0xC0))
        if s.arr:
            out["objects"] = s.rd_words(s.arr, OBJ_COUNT * 8)
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

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def get_case(r, x=None, z=None, entrance=None, n=None, kinds=None, flags=None, entry0=None, link=None):
    x = r.choice([0, 1, 2, 3, 3 + 0x10000, 0x10000 - 1]) if x is None else x
    z = r.choice([0, 1, 2, 0x200]) if z is None else z
    index = ((x + (z << 7) + 1) & 0xFFFF) - 1
    if index >= 0x1F0 or index < -1:
        index = -1
        x, z = 0xFFFF, 0
    entrance = r.choice([0, 1, 1, 1]) if entrance is None else entrance
    n = r.randrange(0, 5) if n is None else n
    end = r.choice([0, 0xFFFF, 0x1234, 7])
    if index < 0:
        n, end, link = 0, 0, 0
    link = r.randrange(1, 0x1F0) if link is None else link
    ids = r.sample([i for i in range(1, 0x2800) if i != end], n)
    things = []
    for k, i in enumerate(ids):
        kind = 3 if kinds is None else kinds[k]
        if kinds is None and r.random() < 0.3:
            kind = r.choice([0, 1, 2, 4, 0xFF])
        things.append(dict(id=i, kind=kind, next=ids[k + 1] if k + 1 < n else end))
    cells = []
    if index >= 0:
        cells = [(index, link, None), (link - 1, None, ids[0] if n else end)]
    sel = [r.choice([0, 1]) for _ in range(max(n, 1))] if flags is None else flags
    return {"op": "get", "args": [x, z], "end": end, "cells": cells, "things": things, "flag": r.choice([1, 5, 0x80000000]),
            "entry0": (r.random() < 0.15) if entry0 is None else entry0,
            "queues": {"IsEntrance": [entrance], "GetDBInfo": sel}}


def get_edges():
    r = random.Random(11)
    yield get_case(r, 3, 1, entrance=0)
    yield get_case(r, 0xFFFF, 0, entrance=1)
    yield get_case(r, 0xFFFFFFFF, 0, entrance=1)
    yield get_case(r, 5, 1, entrance=1, n=0)
    yield get_case(r, 5, 1, entrance=1, n=1, kinds=[3], flags=[0])
    yield get_case(r, 5, 1, entrance=1, n=1, kinds=[3], flags=[1])
    yield get_case(r, 5, 1, entrance=1, n=1, kinds=[2], flags=[0])
    yield get_case(r, 5, 1, entrance=1, n=3, kinds=[3, 3, 3], flags=[1, 1, 0])
    yield get_case(r, 5, 1, entrance=1, n=3, kinds=[2, 3, 3], flags=[1, 0, 0], entry0=True)
    yield get_case(r, 5, 1, entrance=1, n=4, kinds=[3, 3, 3, 4], flags=[1, 1, 1, 1])
    yield get_case(r, 0, 0, entrance=1, n=2, kinds=[3, 3], flags=[1, 0], link=1)
    yield get_case(r, 0, 0, entrance=1, n=2, link=0x1EF)


def get_random(r):
    return get_case(r)


def move_case(r, cell=None, direction=None, present=None):
    c = r.choice([1, 2, 0x80, 0x81, 0x7F, 0x100, 0xFFFF, 0x4000, r.randrange(0, 0x10000), 0]) if cell is None else cell
    d = r.choice([1, 4, 0x10, 0x40, 0, 0x55, 0x02, 0x11, 0xAA, r.randrange(256)]) if direction is None else direction
    return {"op": "move", "pos": [rf(r), rf(r), rf(r)], "angle": rf(r), "cell": c,
            "present": (r.random() < 0.85) if present is None else present,
            "queues": {"GetDirection": [d]}}


def move_edges():
    r = random.Random(12)
    yield move_case(r, present=False)
    for d in (1, 4, 0x10, 0x40, 0, 0x55, 0xFF, 0x05, 0x14, 0x41, 0x02, 0xAA):
        yield move_case(r, 0x81, d, True)
    for c in (0, 1, 0x80, 0x7F, 0xFFFF, 0x4000):
        yield move_case(r, c, 1, True)


def move_random(r):
    return move_case(r)


def free_case(r, ntracks=None):
    n = r.randrange(0, 4) if ntracks is None else ntracks
    fl = lambda: r.choice([1, 3, 0x1F, 0, r.getrandbits(32)])
    tracks = [{"flags": fl(), "subs": [fl() for _ in range(r.randrange(0, 4))]} for _ in range(n)]
    return {"op": "free", "tracks": tracks, "links": r.getrandbits(32), "pieces": r.getrandbits(32), "queues": {}}


def free_edges():
    r = random.Random(13)
    for n in (0, 1, 2, 3):
        yield free_case(r, n)


def free_random(r):
    return free_case(r)


def objects(r, freepos):
    objs = []
    for i in range(OBJ_COUNT):
        flags = r.getrandbits(32) | 1
        if i == freepos:
            flags &= ~1
        objs.append([flags] + [r.getrandbits(32) for _ in range(7)])
    return objs


def start_case(r, op, freepos="any", table=True):
    if freepos == "any":
        freepos = r.choice([0, 1, 2, 7, 0x7FF, None, 0x400])
    objs = objects(r, freepos)
    x, y, rad = [r.choice([0, -1, 0x7FFFFFFF, -0x80000000, r.randrange(-2 ** 31, 2 ** 31)]) for _ in range(3)]
    c = {"op": op, "table": table, "objects": objs, "count": r.getrandbits(32), "head": r.getrandbits(32), "queues": {}}
    c["args"] = [x & MASK, y & MASK, rad & MASK]
    if op == "startride":
        if freepos is None or not table:
            c["table"] = True
            c["objects"] = objects(r, 3)
        c["args"] = [x & MASK, y & MASK, rad & MASK]
    return c


def start_edges():
    r = random.Random(14)
    for fp in (0, 1, 0x7FF, None):
        yield start_case(r, "start", fp)
    yield start_case(r, "start", 0, table=False)


def start_random(r):
    return start_case(r, "start")


def ride_edges():
    r = random.Random(15)
    for fp in (0, 1, 0x7FF, 0x123):
        yield start_case(r, "startride", fp)


def ride_random(r):
    return start_case(r, "startride", r.choice([0, 1, 5, 0x7FF, 0x300]))


TESTS = [
    FnTest("GetThingAtPosnIfEntrance", ppc, host, edge=get_edges, rnd=get_random),
    FnTest("MoveCameraOverRideExit", ppc, host, edge=move_edges, rnd=move_random),
    FnTest("FreeTrackForRide", ppc, host, edge=free_edges, rnd=free_random),
    FnTest("BMP_StartColObject", ppc, host, edge=start_edges, rnd=start_random),
    FnTest("BMP_StartColObjectForRide", ppc, host, edge=ride_edges, rnd=ride_random),
]
