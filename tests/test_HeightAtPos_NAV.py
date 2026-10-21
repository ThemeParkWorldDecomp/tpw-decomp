"""Unit v5216, the fly navigation height and the holding waypoint, against libtpw_unit_HeightAtPos_NAV.dylib (tests/unit_HeightAtPos_NAV.cpp), main app,
src/game/rides/FlyingRide.cpp:
  HeightAtPos_NAV 0x10074230, GetNextHoldingWaypoint 0x10074458

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory, big-endian; host memory, little-endian).
GetWorldGSquareHeight is a fake on both sides recording (x, y, extra) and answering a float from the case; sqrt is the C library's.

The world: the four fly zone limits, the heightfield size, the fly base table (native pointers on the host, words on the Mac) with base objects
(x +0x14, height +0x18, z +0x1c), the hold limit and a fly block of 0x90 bytes. Compared: the answer, the log, the fly block, the globals.

No fused operations. Not tested: a fly index past the table or onto a NULL base (the original reads through it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_HeightAtPos_NAV"
MASK = 0xFFFFFFFF

FN_H, FN_G = 0x10074230, 0x10074458
SIZES = {FN_H: 512, FN_G: 352}
OWN = set(SIZES)
GWSH = 0x1005F160

ADDR = dict(zoneD=0x1029D2AC, zoneB=0x1029D2B0, zoneA=0x1029D2B4, zoneC=0x1029D2B8, hfH=0x102C6C64, hfW=0x102C6C68,
            bases=0x102DA964, holdMax=0x1029D2BC)
HOST_LAYOUT = [0x4C, 0x60]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_gwsh(e):
    L = e.cl
    L.rec("GetWorldGSquareHeight", (s32(e.r(3)), s32(e.r(4)), L.lab(e.r(5))))
    e.set_f1(struct.unpack(">f", struct.pack(">I", L.ret("GetWorldGSquareHeight") & MASK))[0])


STUBS.internal(GWSH, stub_gwsh)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    psize = 4

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

    def height(self, x, z):
        r3, _, _ = T.call_ppc(self.e, FN_H, [x & MASK, z & MASK])
        return r3 & MASK

    def hold(self, fly):
        T.call_ppc(self.e, FN_G, [fly])
        return 0


class Host(Side):
    fmt = "<I"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_hn_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_hn_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def height(self, x, z):
        return self.lib.tpw_hn_height(s32(x), s32(z)) & MASK

    def hold(self, fly):
        self.lib.tpw_hn_hold(fly)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_hn_addr.restype, lib.tpw_hn_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_hn_alloc.restype, lib.tpw_hn_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_hn_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_hn_height.restype, lib.tpw_hn_height.argtypes = C.c_uint32, [C.c_int32, C.c_int32]
    lib.tpw_hn_hold.restype, lib.tpw_hn_hold.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_hn_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("zoneD"), [case["zone"][0]])
    s.words(s.g("zoneB"), [case["zone"][1]])
    s.words(s.g("zoneA"), [case["zone"][2]])
    s.words(s.g("zoneC"), [case["zone"][3]])
    s.words(s.g("hfW"), [case["hf"][0]])
    s.words(s.g("hfH"), [case["hf"][1]])
    s.words(s.g("holdMax"), [case["hold"]])
    table = s.g("bases")
    ptrs = [0] * 100
    for idx, (bx, bh, bz) in case["bases"].items():
        a = s.alloc(0x20)
        s.words(a + 0x14, [bx, bh, bz])
        ptrs[idx] = a
    for i, p in enumerate(ptrs):
        if s.psize == 4:
            s.words(table + 4 * i, [p])
        else:
            s.write(table + 8 * i, struct.pack("<Q", p))
    s.fly = s.alloc(0x90)
    s.words(s.fly, case["fly"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "fly": s.rd_words(s.fly, 36),
            "globals": [s.rd_words(s.g(n), 1)[0] for n in ("zoneD", "zoneB", "zoneA", "zoneC", "hfW", "hfH", "holdMax")]}


def run(s, case):
    try:
        build(s, case)
        if case["op"] == "h":
            ans = s.height(*case["args"])
        else:
            ans = s.hold(s.fly)
        return observe(s, case, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def fbits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def base(r, op):
    zb = r.choice([70000, -30000, r.randrange(-50000, 100000)])
    zc = r.choice([30000, -20000, r.randrange(-50000, 60000)])
    zone = [zb + r.randrange(10000, 150000), zb, zc + r.randrange(10000, 150000), zc]
    bases = {}
    for _ in range(r.choice([0, 0, 1, 3, 10])):
        bases[r.randrange(0, 100)] = (r.randrange(-100000, 250000), r.randrange(-100000, 100000), r.randrange(-100000, 250000))
    fly = [0] * 36
    fly[0] = r.randrange(1, 100)
    for i in (0x4C // 4, 0x50 // 4, 0x54 // 4, 0x58 // 4, 0x5C // 4, 0x60 // 4, 0x88 // 4):
        fly[i] = r.choice([0, 1, 2, 3, r.randrange(0, 1 << 32), r.randrange(0, 100000)])
    fly[0x88 // 4] = r.choice([0, 1, 2, 3, 0xFFFFFFFF, r.randrange(0, 1 << 32)])
    hv = r.uniform(-300, 300)
    return {"op": op, "zone": zone, "hf": [r.randrange(1, 80), r.randrange(1, 80)], "hold": r.choice([0x36B0, 0, r.randrange(-20000, 40000)]),
            "bases": bases, "fly": fly, "queues": {"GetWorldGSquareHeight": [fbits(hv) if r.random() < 0.7 else fbits(float(r.randrange(-500, 500)))]}}


def h_case(r, x=None, z=None, c=None):
    c = c or base(r, "h")
    zd, zb, za, zc = c["zone"]
    c["args"] = [r.choice([r.randrange(zb - 25000, zd + 25000), zb, zd - 1, zd, zb - 20000, zd + 19999, zd + 20000])
                 if x is None else x,
                 r.choice([r.randrange(zc - 25000, za + 25000), zc, za - 1, za, zc - 20000, za + 19999, za + 20000])
                 if z is None else z]
    if c["bases"] and r.random() < 0.6:
        k = r.choice(list(c["bases"]))
        if k:
            bx, bh, bz = c["bases"][k]
            c["args"] = [bx + r.randrange(-11000, 11000), bz + r.randrange(-11000, 11000)]
    return c


def h_edges():
    r = random.Random(5216)
    for x in (None, 0, -1, 29999, 30000, 300000, -400000, 189999):
        for z in (None, 0, -1, 31000, 140000, -100000):
            yield h_case(r, x, z)
    # base distance boundary (312 cells of 32 = 9984 < 10000, 313 -> 10016), with negative offsets for the rounding toward zero
    for d in (9983, 9984, 9985, 10000, 10015, 10016, -9983, -9984, -10015, -10016, -31, -32, -33, 31, 33):
        for axis in (0, 1):
            c = base(r, "h")
            c["zone"] = [190000, 70000, 140000, 30000]
            c["bases"] = {r.randrange(1, 100): (100000, 5, 100000)}
            c["args"] = [100000 + (d if axis == 0 else 0), 100000 + (d if axis == 1 else 0)]
            yield c
    # base at index 0 is ignored; far bases; sqrt of 0
    c = base(r, "h")
    c["zone"] = [190000, 70000, 140000, 30000]
    c["bases"] = {0: (100000, 0, 100000)}
    c["args"] = [100000, 100000]
    yield c
    c = base(r, "h")
    c["zone"] = [190000, 70000, 140000, 30000]
    c["bases"] = {99: (100000, 0, 100000)}
    c["args"] = [100000, 100000]
    yield c
    # clamps of the cell: 0 at the low end, the heightfield size at the high end
    for w, hh in ((1, 1), (5, 7), (80, 80)):
        for x, z in ((70000, 30000), (189999, 139999), (69999 - 60000, 30000)):
            c = base(r, "h")
            c["zone"] = [190000, 10000, 140000, 30000]
            c["bases"] = {}
            c["hf"] = [w, hh]
            c["args"] = [x, z]
            yield c
    # zone checks one by one, the exact limits
    for dx, dz in ((0, 0), (-1, 0), (0, -1), (19999, 0), (20000, 0), (0, 19999), (0, 20000)):
        for corner in ((190000, 140000), (70000, 30000)):
            c = base(r, "h")
            c["zone"] = [190000, 70000, 140000, 30000]
            c["bases"] = {}
            c["args"] = [corner[0] + dx, corner[1] + dz]
            yield c
            c = base(r, "h")
            c["zone"] = [190000, 70000, 140000, 30000]
            c["bases"] = {}
            c["args"] = [corner[0] - 20000 + dx, corner[1] - 20000 + dz]
            yield c


def h_random(r):
    return h_case(r)


def g_case(r, c=None):
    c = c or base(r, "g")
    c["bases"][c["fly"][0]] = (r.randrange(-200000, 200000), r.randrange(-100000, 100000), r.randrange(-200000, 200000))
    return c


def g_edges():
    r = random.Random(52162)
    for st in (0, 1, 2, 3, 4, 7, 0xFFFFFFFF, 0xFFFFFFFE, 0x80000000):
        for hold in (0x36B0, 0, -1):
            c = g_case(r)
            c["fly"][0x88 // 4] = st
            c["hold"] = hold
            yield c
    # the hold limit against height + 5000 on both sides and equal
    for bh in (-5000, 0, 100, 0x36B0 - 5000, 0x36B0 - 5001, 0x36B0 - 4999, -100000):
        c = g_case(r)
        c["hold"] = 0x36B0
        c["bases"][c["fly"][0]] = (10, bh, 20)
        yield c


def g_random(r):
    return g_case(r)


TESTS = [
    FnTest("HeightAtPos_NAV", ppc, host, edge=h_edges, rnd=h_random),
    FnTest("GetNextHoldingWaypoint", ppc, host, edge=g_edges, rnd=g_random),
]
