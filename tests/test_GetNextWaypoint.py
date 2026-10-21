"""Unit v5198, GetNextWaypoint 0x100745ec, against libtpw_unit_GetNextWaypoint.dylib (tests/unit_GetNextWaypoint.cpp), main app
("SimTheme Park"), src/game/rides/FlyingRide.cpp.

A case is a fly and the globals around it, built twice by one piece of Python code (emulator memory big-endian, host memory
little-endian). CWorld::GetRand, HeightAtPos_NAV, GetWorldGSquareHeight are fakes on both sides recording their arguments and answering
from queues; labs is an import faked on the PowerPC side (the translation computes the absolute value itself).

The world: the fly block (fields 4, 8, 0xc, 0x4c..0x60), the four fly zone limits, the minimum height, the heightfield size and the
game world pointer. Compared: the whole fly block and the event log.

No fused operations. Not tested: nothing excluded (float heights stay small, below 2^31).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_GetNextWaypoint"
MASK = 0xFFFFFFFF

FN = 0x100745ec
SIZES = {FN: 1920}
OWN = set(SIZES)

GETRAND = 0x1013b1e4
HEIGHT_NAV = 0x10074230
GSQUARE = 0x1005f160

ADDR = dict(gFlyZoneD=0x1029d2ac, gFlyZoneB=0x1029d2b0, gFlyZoneA=0x1029d2b4, gFlyZoneC=0x1029d2b8, gFlyMinHeight=0x1029d2c0,
            gHeightfieldHeight=0x102c6c64, gHeightfieldWidth=0x102c6c68, gGameWorld=0x1037ebfc)
HOST_LAYOUT = [4, 8, 0xc, 0x4c, 0x60]
FLY_WORDS = 0x68 // 4


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_getrand(e):
    L = e.cl
    L.rec("GetRand", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetRand") & MASK)


def stub_height(e):
    L = e.cl
    L.rec("HeightAtPos_NAV", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("HeightAtPos_NAV") & MASK)


def stub_gsquare(e):
    L = e.cl
    L.rec("GetWorldGSquareHeight", (s32(e.r(3)), s32(e.r(4)), L.lab(e.r(5))))
    e.set_f1(struct.unpack(">f", struct.pack(">I", L.ret("GetWorldGSquareHeight") & MASK))[0])


def stub_labs(e):
    v = s32(e.r(3))
    e.set_r(3, (-v) & MASK if v < 0 else v)


STUBS.internal(GETRAND, stub_getrand)
STUBS.internal(HEIGHT_NAV, stub_height)
STUBS.internal(GSQUARE, stub_gsquare)
STUBS.imp("labs", stub_labs)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


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

    def call(self, fly):
        T.call_ppc(self.e, FN, [fly])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gnw_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_gnw_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fly):
        self.lib.tpw_gnw_run(fly)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_gnw_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "run").restype, getattr(lib, p + "run").argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    for name in ("gFlyZoneA", "gFlyZoneB", "gFlyZoneC", "gFlyZoneD", "gFlyMinHeight", "gHeightfieldWidth", "gHeightfieldHeight"):
        s.words(s.g(name), [case[name]])
    world = s.alloc(0x40)
    s.words(s.g("gGameWorld"), [world])
    fly = s.alloc(0x80)
    s.words(fly, case["fly"])
    return fly


def run(s, case):
    try:
        fly = build(s, case)
        s.call(fly)
        return {"fly": s.rd_words(fly, FLY_WORDS), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mkfly(x4, m8, zc, tx, th, tz, px, ph, pz):
    w = [0] * FLY_WORDS
    w[1], w[2], w[3] = x4, m8, zc
    w[0x4c // 4], w[0x50 // 4], w[0x54 // 4] = tx, th, tz
    w[0x58 // 4], w[0x5c // 4], w[0x60 // 4] = px, ph, pz
    return [v & MASK for v in w]


def pos(r):
    k = r.random()
    if k < 0.15:
        return r.randrange(-60000, 30000)
    if k < 0.85:
        return r.randrange(20000, 220000)
    return r.randrange(150000, 400000)


def base(r, **kw):
    m8 = r.randrange(-3000, 30000)
    limit = m8 + 5000
    hs = [r.choice([limit - 1, limit, limit + 1, limit + r.randrange(-8000, 20000)]) for _ in range(r.randrange(0, 7))]
    hs.append(limit - r.randrange(1, 100000))
    gs = [fb(r.choice([float(r.randrange(-5, 80)), r.uniform(-3, 80), 0.0, -0.5, 0.99])) for _ in range(r.randrange(1, 5))]
    px, pz = pos(r), pos(r)
    tx, tz = pos(r), pos(r)
    if r.random() < 0.3:
        tx = px + r.choice([0, 20000, -20000])
    if r.random() < 0.3:
        tz = pz + r.choice([0, 20000, -20000])
    if r.random() < 0.15:
        tz = pz + abs(tx - px) * r.choice([1, -1])
    c = {"fly": mkfly(pos(r), m8, pos(r), tx, r.randrange(0, 100000), tz, px, r.randrange(0, 100000), pz),
         "gFlyZoneA": 0x222e0, "gFlyZoneB": 0x11170, "gFlyZoneC": 0x7530, "gFlyZoneD": 0x2e630,
         "gFlyMinHeight": r.choice([0x2ee0, 0x2ee0, 0, 100000]),
         "gHeightfieldWidth": r.choice([1, 5, 60, 300]), "gHeightfieldHeight": r.choice([1, 7, 60, 300]),
         "queues": {"GetRand": [r.choice([0, 4, -4, 1, 2, 3, -1, 7, r.randrange(-1 << 31, 1 << 31)]) & MASK
                                for _ in range(1)],
                    "HeightAtPos_NAV": hs, "GetWorldGSquareHeight": gs}}
    c.update(kw)
    return c


def edges():
    r = random.Random(1983)
    for rnd in (0, 4, 1, 3, -1, 0x80000000):
        for _ in range(6):
            c = base(r)
            c["queues"]["GetRand"] = [rnd & MASK]
            yield c
    # equal diffs, zero diffs, INT_MIN-like
    for tx, tz, px, pz in ((50000, 50000, 50000, 50000), (90000, 80000, 50000, 60000), (50000, 90000, 60000, 50000),
                           (-5000, -5000, 0, 0), (100000, 50000, 100000, 60000)):
        c = base(r)
        c["fly"] = mkfly(80000, 1000, 60000, tx, 0, tz, px, 0, pz)
        yield c
    # zone borders and heightfield clamps
    for x, z in ((0x11170, 0x7530), (0x2e62f, 0x222df), (0x2e630, 0x222df), (0x2e62f, 0x222e0), (0x1116f, 0x7530), (0x11170, 0x752f)):
        c = base(r)
        c["fly"] = mkfly(x, 1000, z, x, 0, z, x, 0, z)
        c["queues"]["GetRand"] = [1]
        yield c


def rnd_case(r):
    return base(r)


TESTS = [
    FnTest("GetNextWaypoint", ppc, host, edge=edges, rnd=rnd_case),
]
