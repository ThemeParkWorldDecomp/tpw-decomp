"""Unit v5124, TrackRideIdToThingId against libtpw_unit_TrackRideIdToThingId.dylib (tests/unit_TrackRideIdToThingId.cpp), main app
("SimTheme Park"), src/game/rides/BumperRide.cpp:
  TrackRideIdToThingId 0x10030020

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (GetUsedThingHead,
GetNextUsedThing, GetThingId, GAME_ASSERTMSG) are fakes on both sides that record their arguments and answer from the case's queues.

The world of a case: gGameWorld, the end id gUsedListEnd, gThingArray entries 0..8 (thing blocks with id, kind and the word at +0x24),
the used-list order as the queues. What is compared: the answer halfword and the event log.

No fused operations. Not tested: a walk reaching an empty table entry (a NULL thing is read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_TrackRideIdToThingId"
MASK = 0xFFFFFFFF

FN = 0x10030020
SIZES = {FN: 240}
OWN = set(SIZES)

GET_HEAD, GET_NEXT, GET_ID, ASSERT = 0x1013adb0, 0x1013ae08, 0x1012e1f8, 0x1000dde8

ADDR = dict(gGameWorld=0x1037ebfc, gThingArray=0x1034cbfc, gUsedListEnd=0x1029cc10)
MAC_STRIDE = 20
NENT = 9
HOST_LAYOUT = None   # filled from the library: [stride, mThing offset, mThingId offset, mKind offset]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def u16be(e, a):
    return struct.unpack(">H", e.read(a, 2))[0]


def world_ok(e):
    return 1 if e.r(4) == e.u32(ADDR["gGameWorld"]) else 0


def stub_head(e):
    L = e.cl
    L.rec("GetUsedThingHead", (world_ok(e),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetUsedThingHead") & 0xFFFF))


def stub_next(e):
    L = e.cl
    L.rec("GetNextUsedThing", (world_ok(e), u16be(e, e.r(5))))
    e.write(e.r(3), struct.pack(">H", L.ret("GetNextUsedThing") & 0xFFFF))


def stub_id(e):
    L = e.cl
    L.rec("GetThingId", (u16be(e, e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetThingId") & 0xFFFF))


STUBS.internal(GET_HEAD, stub_head)
STUBS.internal(GET_NEXT, stub_next)
STUBS.internal(GET_ID, stub_id)
STUBS.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def h16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_h16(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def ptr(self, a, p):
        """A pointer word: 4 bytes on the Mac, 8 on the host (the host arena sits above 4 GB)."""
        self.write(a, struct.pack(">I", p) if self.fmt == ">I" else struct.pack("<Q", p))


class Ppc(Side):
    fmt = ">I"
    stride = MAC_STRIDE
    off_thing = 0

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

    def log(self):
        return self.L.log

    def call(self, out, ride):
        r3, _, _ = T.call_ppc(self.e, FN, [out, ride & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.stride, self.off_thing = HOST_LAYOUT[0], HOST_LAYOUT[1]
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_trti_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_trti_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, out, ride):
        self.lib.tpw_trti_run(out, s32(ride))


def s32(v):
    return T.s32(v)


def bind(lib):
    global HOST_LAYOUT
    if getattr(lib, "bound", False):
        return
    lib.tpw_trti_addr.restype, lib.tpw_trti_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_trti_alloc.restype, lib.tpw_trti_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_trti_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_trti_run.restype, lib.tpw_trti_run.argtypes = None, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_trti_layout(out)
    HOST_LAYOUT = list(out)[:n]
    assert HOST_LAYOUT[2:] == [0, 2], HOST_LAYOUT
    lib.bound = True


def build(s, case):
    world = s.alloc(16)
    s.ptr(s.g("gGameWorld"), world)
    s.h16(s.g("gUsedListEnd"), case["end"])
    base = s.g("gThingArray")
    for i in range(NENT):
        t = case["things"][i]
        if t is None:
            s.ptr(base + i * s.stride + s.off_thing, 0)
            continue
        tid, kind, word = t
        b = s.alloc(0x30)
        s.h16(b, tid)
        s.write(b + 2, bytes([kind]))
        s.words(b + 0x24, [word])
        s.ptr(base + i * s.stride + s.off_thing, b)
    out = s.alloc(8)
    s.h16(out, 0xBEEF)
    return out


def run(s, case):
    try:
        out = build(s, case)
        s.call(out, case["ride"])
        return {"out": s.rd_h16(out), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make(r, vis, end, kinds, words, ride, present0=False, ids=None):
    things = []
    for i in range(NENT):
        if i == 0 and not present0:
            things.append(None)
        else:
            things.append((ids[i] if ids else r.randrange(0x10000), kinds[i], words[i]))
    head = vis[0] if vis else end
    nxt = list(vis[1:]) + [end]
    return {"queues": {"GetUsedThingHead": [head], "GetNextUsedThing": nxt, "GetThingId": [r.randrange(0x10000)]},
            "end": end, "things": things, "ride": ride & MASK}


def rand_case(r, forced=None):
    present0 = r.random() < 0.3
    end = r.choice([0, 9, 0xFFFF, 0x1234])
    pool = list(range(1, NENT)) + ([0] if present0 and end != 0 else [])
    vis = [r.choice(pool) for _ in range(r.randrange(0, 6))]
    ride = r.choice([0, 1, -1, 5, 0x80000000, r.randrange(1 << 32)])
    kinds = [r.choice([3, 3, 3, 1, 2, 4, 0x13, 0]) for _ in range(NENT)]
    words = [r.choice([ride, ride, 0, 1, 5, r.randrange(1 << 32)]) & MASK for _ in range(NENT)]
    return make(r, vis, end, kinds, words, ride, present0)


def edges():
    r = random.Random(124)
    yield make(r, [], 0, [3] * NENT, [5] * NENT, 5)                    # empty list
    yield make(r, [1], 0, [3] * NENT, [5] * NENT, 5)                   # first matches
    yield make(r, [1, 2], 0, [3, 1, 3, 3, 3, 3, 3, 3, 3], [5, 5, 5, 0, 0, 0, 0, 0, 0], 5)   # kind mismatch then match
    yield make(r, [1, 2], 0, [3] * NENT, [4, 6, 0, 0, 0, 0, 0, 0, 0], 5)   # word mismatch, none match
    yield make(r, [0, 1], 9, [3] * NENT, [5, 0, 0, 0, 0, 0, 0, 0, 0], 5, present0=True)   # entry 0 allocated (assert fails)
    yield make(r, [1, 2, 3], 0, [3, 3, 3, 3, 0, 0, 0, 0, 0], [0, 7, 7, 7, 0, 0, 0, 0, 0], -1)
    yield make(r, [2], 0xFFFF, [3, 3, 3, 4, 0, 0, 0, 0, 0], [0, 0, 0x80000000, 0, 0, 0, 0, 0, 0], 0x80000000)


def rnd(r):
    return rand_case(r)


TESTS = [
    FnTest("TrackRideIdToThingId", ppc, host, edge=edges, rnd=rnd),
]
