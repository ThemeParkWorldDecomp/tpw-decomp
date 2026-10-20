"""Unit v5344, the ride track counting, performance and teardown of the advisor world, against
libtpw_unit_BMP_CountTrackSections.dylib (tests/unit_BMP_CountTrackSections.cpp), main app, src/BMP_FixRide_BMP_CountTrackSections.cpp:
  BMP_CountTrackSections 0x10026f20, BMP_FreeTrack 0x100270e0, BMP_SetRidePerformance 0x10026c98, BMP_DestroyRide 0x10027274

A case is a world and a call. The world (the list at 0x102b56c0: magic, ride table, live ride count, bump slots, bump count; 256 ride
records of 0xd0 bytes, a track node chain for the ride, bump slots of 0xac bytes) is built twice by ONE piece of Python code. The callees
outside the unit (BMP_DOCK, BMP_CloseRide, TRACE, FreeBuoysForRide, FreeTrackForRide, FreeColObjectsForRide) are fakes on both sides that
log their arguments (pointers as table indexes).

What is compared: the answer, the out words, the first 8 words of the world, the ride record (minus its node pointer), the two clamp
globals and the event log. No fused operations. Not tested: a NULL record with a matching id (ride 0 with a dead world) for the
functions that write through it.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BMP_CountTrackSections"
MASK = 0xFFFFFFFF
MAGIC = 0x4A454647

FN_COUNT, FN_FREE, FN_PERF, FN_DESTROY = 0x10026F20, 0x100270E0, 0x10026C98, 0x10027274
SIZES = {FN_COUNT: 392, FN_FREE: 368, FN_PERF: 428, FN_DESTROY: 392}
OWN = set(SIZES)

C_DOCK, C_CLOSE, C_BUOYS, C_TRACKF, C_COL, C_TRACE = 0x1002E740, 0x10026118, 0x10027494, 0x1002EF00, 0x10027424, 0x1006FCFC
ADDR = dict(world=0x102B56C0, pmax=0x1029CA24, pmin=0x1029CA28)
HOST_LAYOUT = [0xD0, 0xAC, 0x20]
NREC = 256


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def idx(e, p, table_off, size):
    if p == 0:
        return -1
    base = e.u32(0x102B56C0 + table_off)
    return int(s32((p - base) & MASK) / size)


def stub_dock(e):
    e.cl.rec("DOCK", (idx(e, e.r(3), 0x10, 0xAC),))
    e.set_r(3, 0)


def stub_close(e):
    e.cl.rec("CloseRide", (s32(e.r(3)),))
    e.set_r(3, 0)


def stub_buoys(e):
    e.cl.rec("FreeBuoys", (idx(e, e.r(3), 8, 0xD0), s32(e.r(4))))


def stub_trackf(e):
    e.cl.rec("FreeTrack", (idx(e, e.r(3), 8, 0xD0),))


def stub_col(e):
    e.cl.rec("FreeColObjects", (idx(e, e.r(3), 8, 0xD0),))


STUBS.internal(C_DOCK, stub_dock)
STUBS.internal(C_CLOSE, stub_close)
STUBS.internal(C_BUOYS, stub_buoys)
STUBS.internal(C_TRACKF, stub_trackf)
STUBS.internal(C_COL, stub_col)
STUBS.internal(C_TRACE, CL.recorder("TRACE", "si"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    base = 0

    def ptr(self, a):
        """A pointer word as the side stores it: the Mac's address, the host's arena offset."""
        return (a - self.base) & MASK if a else 0

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

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

    def call(self, op, ride, outs, speed):
        if op == "count":
            r3, _, _ = T.call_ppc(self.e, FN_COUNT, [ride & MASK] + outs)
            return r3 & MASK
        fn = {"free": FN_FREE, "destroy": FN_DESTROY}.get(op)
        if fn:
            T.call_ppc(self.e, fn, [ride & MASK])
        else:
            T.call_ppc(self.e, FN_PERF, [ride & MASK, speed & MASK])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_bmpct_base()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bmpct_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_bmpct_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, ride, outs, speed):
        L = self.lib
        if op == "count":
            return L.tpw_bmpct_count(s32(ride), *outs) & MASK
        if op == "free":
            L.tpw_bmpct_free(s32(ride))
        elif op == "destroy":
            L.tpw_bmpct_destroy(s32(ride))
        else:
            L.tpw_bmpct_perf(s32(ride), s32(speed))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bmpct_addr.restype, lib.tpw_bmpct_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bmpct_alloc.restype, lib.tpw_bmpct_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bmpct_base.restype = C.c_uint64
    lib.tpw_bmpct_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bmpct_count.restype = C.c_int32
    lib.tpw_bmpct_count.argtypes = [C.c_int32, C.c_uint64, C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_bmpct_free.argtypes = [C.c_int32]
    lib.tpw_bmpct_destroy.argtypes = [C.c_int32]
    lib.tpw_bmpct_perf.argtypes = [C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_bmpct_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

def build(s, case):
    s.words(s.g("pmax"), [case["pmax"]])
    s.words(s.g("pmin"), [case["pmin"]])
    s.alloc(16)    # the host arena may hand out offset 0 first, which a Ptr32 reads as NULL
    rides = s.alloc(NREC * 0xD0)
    ctx = {"rides": rides}
    ride = case["ride"]
    rec = rides + (ride & 0xFF) * 0xD0
    ctx["rec"] = rec
    words = list(case["rec"])
    # the node chain, built from the tail
    nxt = 0
    for node in reversed(case["nodes"]):
        a = s.alloc(0x28)
        s.words(a + 4, [node[0]])
        s.words(a + 8, [node[1]])
        s.words(a + 0x24, [s.ptr(nxt)])
        nxt = a
    words[0xBC // 4] = s.ptr(nxt)
    s.words(rec, words)
    # bump slots
    owner_of = {"rec": rec, "other": rides + ((ride + 1) & 0xFF) * 0xD0, "zero": 0}
    bumps = 0
    if case["slots"]:
        bumps = s.alloc(0xAC * len(case["slots"]))
        for i, (flags, who) in enumerate(case["slots"]):
            s.words(bumps + i * 0xAC, [flags])
            s.words(bumps + i * 0xAC + 0x9C, [s.ptr(owner_of[who])])
    ctx["bumps"] = bumps
    world = [case["magic"], 0, s.ptr(rides), case["live"], s.ptr(bumps), case["bcount"], 0, 0]
    s.words(s.g("world"), world)
    ctx["outs"] = []
    for present in case["outs"]:
        if present:
            o = s.alloc(4)
            s.words(o, [0x5A5A5A5A])
            ctx["outs"].append(o)
        else:
            ctx["outs"].append(0)
    return ctx


def execute(s, case, ctx):
    return s.call(case["op"], case["ride"], ctx["outs"], case["speed"])


def observe(s, case, ctx, answer):
    rec = s.rd_words(ctx["rec"], 0xD0 // 4)
    rec[0xBC // 4] = 0
    return {"answer": answer, "log": s.log(), "world": [w if i not in (2, 4) else 0 for i, w in enumerate(s.rd_words(s.g("world"), 8))],
            "rec": rec, "outs": [s.rd_word(o) if o else None for o in ctx["outs"]],
            "pmax": s.rd_word(s.g("pmax")), "pmin": s.rd_word(s.g("pmin"))}


def run(s, case):
    try:
        ctx = build(s, case)
        return observe(s, case, ctx, execute(s, case, ctx))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- generators

def mk(r, op, live_world=None, match=None, ride=None):
    c = {"op": op, "queues": {}}
    live = r.random() < 0.9 if live_world is None else live_world
    c["magic"] = MAGIC if live else r.choice([0, MAGIC + 1, MAGIC ^ 0x100])
    idx_ = r.randrange(256)
    kind = r.choice([0, 1, -1, -5, -14, 0x1234, r.randrange(-3000, 3000)])
    if ride is None:
        ride = ((kind << 8) | idx_) & MASK
        rec0 = kind & MASK
        if match is None:
            match = r.random() < 0.8
        if not match:
            ride ^= r.choice([0x100, 0x1, 0x80000000, 0x10000])
        ride = s32(ride)
    else:
        rec0 = (ride >> 8) & MASK
    c["ride"] = ride
    rec = [r.getrandbits(32) for _ in range(0xD0 // 4)]
    rec[0] = rec0
    c["rec"] = rec
    c["live"] = r.choice([0, 1, 5, 0x7FFFFFFF, MASK])
    # a dead world with a matching id needs ride 0 and is only run by the ops that do not write through NULL
    c["pmax"] = r.choice([100, 0, 50, -5, 200, r.randrange(-300, 300)])
    c["pmin"] = r.choice([0, 10, -10, 60, r.randrange(-300, 300)])
    c["speed"] = r.choice([0, 100, -1, 50, 150, 1000, r.randrange(-300, 300), r.randrange(-100000, 100000)])
    # nodes
    nodes = []
    for _ in range(r.choice([0, 0, 1, 2, 3, 5, 8, 12])):
        t = r.choice([0, 1, 5, 8, 9, 10, 11, 11, 12, 13, 14, 15, 16, 0xFFFF])
        nodes.append(((r.getrandbits(16) << 16) | t, r.choice([0, 0, 1, r.getrandbits(32)])))
    c["nodes"] = nodes
    # bump slots
    slots = []
    for _ in range(r.choice([0, 0, 1, 2, 4, 6])):
        slots.append((r.choice([0, 1, 1, 3, 0xFFFFFFFE, r.getrandbits(32)]), r.choice(["rec", "rec", "other", "zero"])))
    c["slots"] = slots
    liveslots = sum(1 for f, _ in slots if f & 1)
    c["bcount"] = r.choice([liveslots, liveslots, liveslots, max(0, liveslots - 1)])
    c["outs"] = [r.random() < 0.7 for _ in range(4)]
    return c


def fix_dead(c, op):
    """A dead world with a matching id (the lookup answers record NULL and id 0) means ride 0; writing ops cannot take it."""
    if c["magic"] != MAGIC and op in ("count", "perf", "destroy"):
        c["ride"] = 1 if c["ride"] == 0 else c["ride"]
    return c


def gen(op):
    def rnd(r):
        return fix_dead(mk(r, op), op)

    def edges():
        r = random.Random(hash(op) & 0xFFFF)
        r = random.Random({"count": 11, "free": 12, "perf": 13, "destroy": 14}[op])
        for live in (True, False):
            for match in (True, False):
                for _ in range(6):
                    yield fix_dead(mk(r, op, live, match), op)
        if op in ("free", "destroy"):
            c = mk(r, op, False, True, 0)
            c["magic"] = 0
            c["ride"] = 0
            yield c if op == "free" else fix_dead(c, op)
        if op == "perf":
            for speed in (0, 100, 101, -1, 99, 50, 1, 2**31 - 1, -2**31):
                for pmax, pmin in ((100, 0), (100, 100), (0, 0), (50, 60), (-5, -10)):
                    c = mk(r, op, True, True)
                    c["speed"], c["pmax"], c["pmin"] = speed, pmax, pmin
                    yield c
            for v in (1, 99, 100, 101, 7, -7, 55):
                c = mk(r, op, True, True)
                c["pmax"], c["pmin"], c["speed"] = 1000, -1000, v
                yield c
    return edges, rnd


TESTS = []
for _name, _op in (("BMP_CountTrackSections", "count"), ("BMP_FreeTrack", "free"), ("BMP_SetRidePerformance", "perf"),
                   ("BMP_DestroyRide", "destroy")):
    _e, _r = gen(_op)
    TESTS.append(FnTest(_name, ppc, host, edge=_e, rnd=_r))
