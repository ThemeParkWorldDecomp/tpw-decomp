"""Unit v5880, one tick of every fly of every fly base (the state machine, the motion, the avoidance, the steering and the sound
position), against libtpw_unit_FLY_Tick.dylib (tests/unit_FLY_Tick.cpp), main app ("SimTheme Park"), src/hlsprite_zone_FLY_Tick.cpp:
  FLY_Tick 0x1007567c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit (EmployEric, SetCurRideAnim, GAME_ASSERTMSG, RSSE_GetSoundEventCode, SOUND_SetParamValue, GetNextHoldingWaypoint,
GetNextWaypoint, CWorld::GetRand, GetWorldGSquareHeight, PAR_KillEmitter, DestroyRideInstance, SOUND_StopSoundEffect,
SOUND_MoveSoundEffect) are fakes on both sides that record their arguments (the fly and base pointers, the ride sets and the world as
labels) and answer from the case's queues; sqrt, atan2 and labs are the C library's on both sides.

The world of a case: one to three fly bases (0x122c bytes each, every word set: random words under the fields the code reads) in slots
of gFlyBases, a sine table of 4096 random words, the fly zone, the heightfield size, the three lowest heights, seven ride sets and the
world. What is compared: every word of every base after the call and the event log.

No fused operations; fctiwz only of square roots, of 652 * atan2 and of the ground heights, all far inside 32 bits. Not tested: squared
distances that overflow 32 bits (positions stay within about 500000 of each other, so the roots never see a negative number), a zone
whose span is 0 or negative (the original divides by it).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FLY_Tick"
MASK = 0xFFFFFFFF

# ---- the unit's own function
FN_TICK = 0x1007567c
OWN = {FN_TICK}

# ---- the callees faked on both sides
EMPLOY_ERIC = 0x10074d9c
SET_CUR_RIDE_ANIM = 0x100c6670
GAME_ASSERTMSG = 0x1000dde8
RSSE_GET_SOUND_EVENT_CODE = 0x100d6d04
SOUND_SET_PARAM_VALUE = 0x100dd380
GET_NEXT_HOLDING_WAYPOINT = 0x10074458
GET_NEXT_WAYPOINT = 0x100745ec
WORLD_GET_RAND = 0x1013b1e4
GET_WORLD_GSQUARE_HEIGHT = 0x1005f160
PAR_KILL_EMITTER = 0x100be1e8
DESTROY_RIDE_INSTANCE = 0x1006a3ec
SOUND_STOP_SOUND_EFFECT = 0x100de058
SOUND_MOVE_SOUND_EFFECT = 0x100ddf60

ADDR = dict(gFlyBases=0x102da964, gFlySinTable=0x102daafc, gFlyZoneD=0x1029d2ac, gFlyZoneB=0x1029d2b0, gFlyZoneA=0x1029d2b4,
            gFlyZoneC=0x1029d2b8, gFlyApproachMinHeight=0x1029d2c4, gFlyTurnMinHeight=0x1029d2c8, gFlyTakeOffMinHeight=0x1029d2cc,
            gHeightfieldHeight=0x102c6c64, gHeightfieldWidth=0x102c6c68, gRideAnimSets=0x102c79a4, gGameWorld=0x1037ebfc)
INT_GLOBALS = ["gFlyZoneA", "gFlyZoneB", "gFlyZoneC", "gFlyZoneD", "gFlyApproachMinHeight", "gFlyTurnMinHeight", "gFlyTakeOffMinHeight",
               "gHeightfieldWidth", "gHeightfieldHeight"]
HOST_LAYOUT = [0x122c, 0xdc, 8, 8]

BASE_SIZE = 0x122c
BASE_WORDS = BASE_SIZE // 4
FLY0, FLY_STRIDE, NFLY = 0x5c, 0xe4, 20
NRIDES = 8
TEXT_ADDR = 0x1023dc85
JUMP_TABLE = 0x1029d2d0


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def ground_height(e, log):
    e.set_f1(bitsf(log.ret("GetWorldGSquareHeight")))
    return 0


def labs(e):
    v = s32(e.r(3))
    e.set_r(3, (-v if v < 0 else v) & MASK)


def atan2(e):
    e.set_f1(math.atan2(e.f(1), e.f(2)))


STUBS.internal(EMPLOY_ERIC, CL.recorder("EmployEric", "i", ret="i"))
STUBS.internal(SET_CUR_RIDE_ANIM, CL.recorder("SetCurRideAnim", "Liiifi", ret="i"))
STUBS.internal(GAME_ASSERTMSG, CL.recorder("GAME_ASSERTMSG", "isi"))
STUBS.internal(RSSE_GET_SOUND_EVENT_CODE, CL.recorder("RSSE_GetSoundEventCode", "ii", ret="i"))
STUBS.internal(SOUND_SET_PARAM_VALUE, CL.recorder("SOUND_SetParamValue", "iii", ret="i"))
STUBS.internal(GET_NEXT_HOLDING_WAYPOINT, CL.recorder("GetNextHoldingWaypoint", "L"))
STUBS.internal(GET_NEXT_WAYPOINT, CL.recorder("GetNextWaypoint", "L"))
STUBS.internal(WORLD_GET_RAND, CL.recorder("GetRand", "L", ret="i"))
STUBS.internal(GET_WORLD_GSQUARE_HEIGHT, CL.recorder("GetWorldGSquareHeight", "iiL", ret=ground_height))
STUBS.internal(PAR_KILL_EMITTER, CL.recorder("PAR_KillEmitter", "i"))
STUBS.internal(DESTROY_RIDE_INSTANCE, CL.recorder("DestroyRideInstance", "i"))
STUBS.internal(SOUND_STOP_SOUND_EFFECT, CL.recorder("SOUND_StopSoundEffect", "i"))
STUBS.internal(SOUND_MOVE_SOUND_EFFECT, CL.recorder("SOUND_MoveSoundEffect", "iiii", ret="i"))
STUBS.imp("labs", labs)
STUBS.imp("atan2", atan2)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def ptr(self, a, i, v):
        self.write(a + self.psize * i, struct.pack(self.pfmt, v))


class Ppc(Side):
    fmt, pfmt, psize = ">I", ">I", 4

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.saved = None
        self.L = None

    def begin(self, labels):
        self.L = self.e.cl = CL.PpcLog(dict(labels), self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
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

    def tick(self):
        T.call_ppc(self.e, FN_TICK, [])


HOST_BLOCKS = {}


class Host(Side):
    fmt, pfmt, psize = "<I", "<Q", 8

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        self.n = 0
        bind(lib)

    def begin(self, labels):
        CL.host_begin(self.lib, {lab: a for a, lab in labels.items()}, self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fly_addr(name.encode())

    def alloc(self, n):
        """The same blocks every case (the arena never frees): the case writes every byte the comparison reads."""
        key = (self.n, n)
        self.n += 1
        if key not in HOST_BLOCKS:
            HOST_BLOCKS[key] = self.lib.tpw_fly_alloc(n)
        return HOST_BLOCKS[key]

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def tick(self):
        self.lib.tpw_fly_tick()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_fly_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "tick").restype, getattr(lib, p + "tick").argtypes = None, []
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

_tr = random.Random(4096)
SIN_TABLE = [_tr.randrange(-1100, 1100) for _ in range(4096)]


def run(s, case):
    try:
        bases = [s.alloc(BASE_SIZE) for _ in case["bases"]]
        table = s.alloc(0x4000)
        world = s.alloc(16)
        rides = s.alloc(16 * NRIDES)
        labels = {world: 5}
        for r in range(1, NRIDES):
            labels[rides + 16 * r] = 10 + r
        for k, a in enumerate(bases):
            labels[a] = 20 + 21 * k
            for i in range(NFLY):
                labels[a + FLY0 + FLY_STRIDE * i] = 20 + 21 * k + 1 + i
        s.begin(labels)
        for name in INT_GLOBALS:
            s.words(s.g(name), [case["globals"][name]])
        slots = [0] * 100
        for k, (index, _) in enumerate(case["bases"]):
            slots[index] = bases[k]
        for i, v in enumerate(slots):
            s.ptr(s.g("gFlyBases"), i, v)
        s.ptr(s.g("gFlySinTable"), 0, table)
        s.words(table, SIN_TABLE)
        s.ptr(s.g("gGameWorld"), 0, world)
        for r in range(NRIDES):
            s.ptr(s.g("gRideAnimSets"), r, rides + 16 * r if r else 0)
        for k, (_, ws) in enumerate(case["bases"]):
            s.words(bases[k], ws)
        s.tick()
        return {"log": s.log(), "bases": [s.rd_words(a, BASE_WORDS) for a in bases]}
    finally:
        s.done()


def ppc(e, case):
    if case.get("debug"):
        print("TEXT", repr(e.cstr(TEXT_ADDR)))
        print("TABLE", [hex(e.u32(JUMP_TABLE + 4 * i)) for i in range(10)])
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

STATES = [1, 2, 3, 4, 5, 6, 7, 8, 9]
ODD_STATES = [0, 10, 11, 0xFFFFFFFF]


def make_fly(r, st, cx, cz, by, near=None, flags=None, spread=None, exact=None):
    f = {}
    f[0x80] = st
    spread = r.choice([1500, 4000, 20000, 150000]) if spread is None else spread
    x = cx + r.randint(-spread, spread)
    z = cz + r.randint(-spread, spread)
    f[4], f[0xc] = x, z
    f[8] = by + r.randint(-3000, 30000)
    if near is None:
        near = r.random() < 0.5
    if exact is not None:
        f[0x4c], f[0x54] = x + exact, z
    elif near:
        f[0x4c], f[0x54] = x + r.randint(-3400, 3400), z + r.randint(-3400, 3400)
    else:
        f[0x4c], f[0x54] = cx + r.randint(-200000, 200000), cz + r.randint(-200000, 200000)
    f[0x50] = f[8] + r.choice([r.randint(-15000, 15000), r.randint(-200, 200), 0])
    f[0x10] = r.choice([r.randint(0, 0xfff), r.randint(-100000, 100000)])
    f[0x14] = r.choice([r.randint(-200, 200), 0, 1, 60, 59, -1])
    f[0x18] = r.randint(-6000, 6000)
    f[0x70] = r.randint(-3000, 3000)
    f[0x74] = r.choice([r.randint(-700, 700), 0x200, -0x200])
    f[0x78] = r.randint(0, 4000)
    f[0x7c] = r.randint(0, 3000)
    f[0x84] = r.choice([r.randint(-3, 50), 0, 1])
    f[0x8c] = r.randint(-2, 2)
    if flags is None:
        flags = r.randrange(8)
    f[0xb4] = flags & 1
    f[0xb8] = (flags >> 1) & 1
    if flags & 4:
        f[0x8c] = 0
    f[0xbc] = r.randint(0, 0xfff)
    f[0xc0] = r.choice([0, 1, 2, 3, 4, 5, 6, 7])
    f[0xc4] = r.choice([0, r.randint(1, 1000)])
    f[0xcc] = r.choice([3, 4, 5, 6, 9, -1])
    f[0xd0] = r.choice([0, 0, 1])
    f[0xd4] = r.choice([0, 1])
    f[0xe0] = r.choice([0, r.randint(1, 999)])
    return f


def make_base(r, index, st=None, near=None, flags=None, spread=None, exact=None, inzone=None, zone=None, over=None):
    ws = [r.getrandbits(32) for _ in range(BASE_WORDS)]
    b = {}
    if inzone:
        bx, bz = r.randint(zone[0] + 25000, zone[1] - 25000), r.randint(zone[2] + 25000, zone[3] - 25000)
    else:
        bx, bz = r.randint(-100000, 300000), r.randint(-100000, 300000)
    b[0x00] = r.randint(0, 21)
    b[0x0c] = r.choice([r.randint(0, 21), 1, 0])
    b[0x10] = r.randint(-5, 200)
    b[0x14], b[0x18], b[0x1c] = bx, r.randint(0, 20000), bz
    b[0x20] = r.randint(0, 2048)
    b[0x24] = r.randint(0, 2048)
    b[0x28] = r.randint(-100, 3000)
    b[0x2c] = r.choice([0, 1])
    b[0x30] = r.randint(0, 30000)
    b[0x34] = r.choice([0, 1, 2, 3, 3, 4])
    b[0x38] = r.choice([0, 1])
    b[0x3c] = r.choice([0, 1])
    b[0x40] = r.randint(0, 0xfff)
    b[0x44] = r.choice([0, 1])
    b[0x4c] = r.choice([0, 1])
    b[0x50] = r.randint(0, 20)
    b[0x54] = r.randint(0, 200)
    b.update(over or {})
    for off, v in b.items():
        ws[off // 4] = v & MASK
    for i in range(NFLY):
        if st is None:
            s = r.choice(STATES + STATES + ODD_STATES + [0, 0])
        elif isinstance(st, list):
            s = r.choice(st)
        else:
            s = st
        f = make_fly(r, s, bx, bz, b[0x18], near, flags, spread, exact)
        a = FLY0 + FLY_STRIDE * i
        for off, v in f.items():
            ws[(a + off) // 4] = v & MASK
    return ws


def make_case(r, indices=None, globals_over=None, over=None, **kw):
    zb = r.randint(-60000, 100000)
    zc = r.randint(-60000, 100000)
    g = dict(gFlyZoneB=zb, gFlyZoneD=zb + r.randint(60000, 250000), gFlyZoneC=zc, gFlyZoneA=zc + r.randint(60000, 250000),
             gFlyApproachMinHeight=r.choice([0x36b0, r.randint(0, 40000)]), gFlyTurnMinHeight=r.choice([0x36b0, r.randint(0, 40000)]),
             gFlyTakeOffMinHeight=r.choice([0x36b0, r.randint(0, 40000)]), gHeightfieldWidth=r.choice([1, 16, 64, 128, r.randint(1, 200)]),
             gHeightfieldHeight=r.choice([1, 16, 64, 128, r.randint(1, 200)]))
    if globals_over:
        g.update(globals_over)
    if indices is None:
        indices = r.sample(range(1, 100), r.choice([1, 1, 2, 3]))
    zone = (g["gFlyZoneB"], g["gFlyZoneD"], g["gFlyZoneC"], g["gFlyZoneA"])
    if kw.get("inzone") is None:
        kw["inzone"] = r.random() < 0.4
    bases = [(i, make_base(r, i, zone=zone, over=over, **kw)) for i in indices]
    queues = {
        "EmployEric": [r.choice([0, 1])],
        "SetCurRideAnim": [r.choice([0, 1, r.randint(0, 5000)]) for _ in range(5)],
        "RSSE_GetSoundEventCode": [r.randint(0, 100) for _ in range(5)],
        "SOUND_SetParamValue": [r.choice([0, r.randint(1, 999)]) for _ in range(8)],
        "SOUND_MoveSoundEffect": [r.choice([0, r.randint(1, 999)]) for _ in range(8)],
        "GetRand": [s32(r.getrandbits(32)) for _ in range(40)] + [r.choice([0, 1, -1, -0x80000000, 0x7fffffff])],
        "GetWorldGSquareHeight": [fb(r.choice([r.uniform(-20, 60), float(r.randint(-5, 40)), 0.0, -0.5])) for _ in range(6)],
    }
    return {"queues": queues, "globals": g, "bases": bases}


def tick_edges():
    r = random.Random(5880)
    c = make_case(r, indices=[1])
    c["debug"] = True
    yield c
    for st in STATES + ODD_STATES:
        for near in (False, True):
            for flags in (0, 1, 2, 3, 4, 5, 6, 7):
                yield make_case(r, indices=[r.choice([1, 99, r.randint(2, 98)])], st=st, near=near, flags=flags,
                                spread=r.choice([1500, 20000]))
    # the 5000 boundary of the target distance: 156 * 32 is inside, 157 * 32 is not
    for st in STATES:
        for d in (4992, -4992, 5024, -5024, 5023, 4991):
            yield make_case(r, indices=[2], st=st, exact=d)
    # landed flies taking off: the ground height of the target, the clamps of the heightfield
    for w, h in ((1, 1), (2, 3), (64, 64), (200, 200)):
        for zb in (-60000, 0, 80000):
            yield make_case(r, indices=[3], st=1, inzone=True, globals_over=dict(gHeightfieldWidth=w, gHeightfieldHeight=h, gFlyZoneB=zb,
                            gFlyZoneD=zb + 200000, gFlyZoneC=zb, gFlyZoneA=zb + 200000), over={0x3c: 0})
    for keep in (0, 1):
        for live in (0, 1, 2):
            yield make_case(r, indices=[4], st=1, flags=0, over={0x38: keep, 0x0c: live, 0x3c: 0})
    # crowded bases: every pair close, all flying
    for st in ([3], [4, 5, 6], [7, 8, 9], [2, 3, 7]):
        for sc in (0, 1024, 2048):
            yield make_case(r, indices=[5, 6], st=st, spread=1500, over={0x20: sc})
    yield make_case(r, indices=list(range(1, 100, 7)))


def tick_random(r):
    return make_case(r)


TESTS = [
    FnTest("FLY_Tick", ppc, host, edge=tick_edges, rnd=tick_random),
]
