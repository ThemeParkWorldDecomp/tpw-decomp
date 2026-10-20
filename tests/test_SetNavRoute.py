"""Unit v5656, the bump's route and steering, against libtpw_unit_SetNavRoute.dylib (tests/unit_SetNavRoute.cpp), main app, src/BMP_Move_SetNavRoute.cpp:
  SetNavRoute 0x100297cc, BumpToPoint 0x1002992c

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian, host memory little-endian).
Callees outside the unit (PointInAnyColObject, DoAreasIntersect, ClosestIntersectingColObjectToPoint, NavPointInArea, TRACE, BMP_Distance,
BMP_Thrust) are fakes on both sides that record their arguments (collision objects as the tag in their x word) and answer from the case's
queues; atan2 is the real libm on both sides.

The world: a 0xac-byte bump with random words, an owner block (words 0x20, 0x28, 0x54), three collision objects, the two thrust limits.
Compared: the answer, the whole bump block, the limits and the event log.

BumpToPoint has fctiwz: HardwareRounding aims at the unit. Not tested: a NULL owner (read unconditionally).
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_SetNavRoute"
MASK = 0xFFFFFFFF

FN_SET, FN_BUMP = 0x100297CC, 0x1002992C
SIZES = {FN_SET: 308, FN_BUMP: 368}
OWN = set(SIZES)

PIA, DAI, CLOSEST, NAVP, TRACE, DIST, THRUST = 0x1002BA94, 0x1002B6C0, 0x1002B4CC, 0x1002B408, 0x1006FCFC, 0x1002B318, 0x10029AC8

ADDR = dict(limitA=0x1029CB28, limitB=0x1029CB2C)
HOST_LAYOUT = [0x98, 0x9C, 4]
BUMP_WORDS = 0xAC // 4


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def tag(e, p):
    return e.s32(p + 4) if p else 0


def objptr(e, i):
    return e.world.objs[i & 3]


def st_pia(e):
    e.cl.rec("PointInAnyColObject", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, objptr(e, e.cl.ret("PointInAnyColObject")))


def st_dai(e):
    e.cl.rec("DoAreasIntersect", (tag(e, e.r(3)), tag(e, e.r(4))))
    e.set_r(3, e.cl.ret("DoAreasIntersect") & MASK)


def st_closest(e):
    e.cl.rec("Closest", (tag(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, objptr(e, e.cl.ret("Closest")))


def st_nav(e):
    e.cl.rec("NavPointInArea", (tag(e, e.r(3)),))
    e.write(e.r(4), struct.pack(">ii", s32(e.cl.ret("NavX")), s32(e.cl.ret("NavY"))))
    e.set_r(3, e.r(4))


def st_trace(e):
    e.cl.rec("TRACE", ())


def st_dist(e):
    e.cl.rec("BMP_Distance", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, e.cl.ret("BMP_Distance") & MASK)


def st_thrust(e):
    e.cl.rec("BMP_Thrust", (s32(e.r(4)), e.u32(e.r(3))))


STUBS.internal(PIA, st_pia)
STUBS.internal(DAI, st_dai)
STUBS.internal(CLOSEST, st_closest)
STUBS.internal(NAVP, st_nav)
STUBS.internal(TRACE, st_trace)
STUBS.internal(DIST, st_dist)
STUBS.internal(THRUST, st_thrust)
STUBS.imp("atan2", lambda emu: emu.set_f1(math.atan2(emu.f(1), emu.f(2))))


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
        e.world = self
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

    def ptr(self, a):
        return a

    def set_objs(self, a, b):
        self.objs = [0, a, b, 0]

    def call(self, fn, bump, x, y):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, fn, [bump, x & MASK, y & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_snr_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_snr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptr(self, a):
        return self.lib.tpw_snr_off(a)

    def set_objs(self, a, b):
        self.lib.tpw_snr_objs(a, b)

    def call(self, fn, bump, x, y):
        f = self.lib.tpw_snr_set if fn == FN_SET else self.lib.tpw_snr_bump
        return f(bump, s32(x), s32(y)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_snr_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "off").restype, getattr(lib, p + "off").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "objs").argtypes = [C.c_uint64, C.c_uint64]
    for n in ("set", "bump"):
        getattr(lib, p + n).restype, getattr(lib, p + n).argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("limitA"), [case["limA"]])
    s.words(s.g("limitB"), [case["limB"]])
    owner = s.alloc(0x60)
    s.words(owner, case["owner"])
    objs = []
    for t in (11, 22, 33):
        o = s.alloc(0x20)
        s.words(o, [1, t, 0, 0, 0])
        objs.append(o)
    s.set_objs(objs[1], objs[2])
    bump = s.alloc(0xAC)
    w = list(case["bump"])
    w[0x98 // 4] = s.ptr(objs[0]) if case["hascol"] else 0
    w[0x9C // 4] = s.ptr(owner)
    s.words(bump, w)
    return bump


def run(s, case):
    try:
        bump = build(s, case)
        x, y = case["args"]
        ans = s.call(case["fn"], bump, x, y)
        w = s.rd_words(bump, BUMP_WORDS)
        # the pointers are addresses: keep only whether they are set
        w[0x98 // 4] = 1 if w[0x98 // 4] else 0
        w[0x9C // 4] = 1
        return {"answer": ans, "log": s.log(), "bump": w,
                "limits": s.rd_words(s.g("limitA"), 1) + s.rd_words(s.g("limitB"), 1)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, fn):
    bump = T.rwords(r, BUMP_WORDS)
    bump[0] = r.choice([0, 0x10000, 0x4000, 0x14000, 0xFFFFFFFF, 0x20000, r.getrandbits(32)])
    bump[0x34 // 4] = r.choice([0, r.randrange(-3000, 3000), r.randrange(-2 ** 20, 2 ** 20)]) & MASK
    bump[0x38 // 4] = r.choice([0, r.randrange(-3000, 3000), r.randrange(-2 ** 20, 2 ** 20)]) & MASK
    bump[0x50 // 4] = r.choice([0, r.randrange(0, 512), r.getrandbits(32)])
    for off in (0x6C, 0x70, 0x74, 0x78):
        bump[off // 4] = r.choice([0, r.randrange(-500, 500), r.getrandbits(32)]) & MASK
    owner = T.rwords(r, 0x18)
    rate = r.choice([0, 1, 2, 3, 5, 16, 40, 100, 255, 256, 512, -4, -1, r.randrange(-300, 300)])
    owner[0x20 // 4] = r.getrandbits(32)
    owner[0x28 // 4] = rate & MASK
    owner[0x54 // 4] = r.choice([0, 1, 2, 3])
    dist = r.choice([0, 1, 511, 512, 513, -1, -513, r.randrange(0, 40000), r.randrange(-40000, 40000), r.getrandbits(31)])
    return {
        "fn": fn,
        "bump": bump,
        "owner": owner,
        "hascol": r.choice([0, 1, 1, 1]),
        "limA": r.choice([0x30, 0x16, 0, 1, 5, -2 & MASK, r.randrange(0, 100)]),
        "limB": r.choice([0x16, 0x30, 0, 3, 20, r.randrange(0, 100)]),
        "args": [r.choice([0, 1, -1, r.randrange(-3000, 3000), r.randrange(-2 ** 20, 2 ** 20)]),
                 r.choice([0, 1, -1, r.randrange(-3000, 3000), r.randrange(-2 ** 20, 2 ** 20)])],
        "queues": {
            "PointInAnyColObject": [r.choice([0, 1, 2])],
            "DoAreasIntersect": [r.choice([0, 1, -1])],
            "Closest": [r.choice([0, 1, 2])],
            "NavX": [r.randrange(-3000, 3000)],
            "NavY": [r.randrange(-3000, 3000)],
            "BMP_Distance": [dist & MASK],
        },
    }


def edge_fn(fn, seed):
    def gen():
        r = random.Random(seed)
        for pia in (0, 1):
            for dai in (0, 1):
                for cl in (0, 1):
                    for flags in (0, 0x10000, 0x14000):
                        for hascol in (0, 1):
                            c = mk(r, fn)
                            c["queues"]["PointInAnyColObject"] = [pia]
                            c["queues"]["DoAreasIntersect"] = [dai]
                            c["queues"]["Closest"] = [cl]
                            c["bump"][0] = flags
                            c["hascol"] = hascol
                            yield c
        for dx, dy in ((10, 0), (0, 10), (-10, 0), (0, -10), (7, 7), (-7, 7), (7, -7), (-7, -7), (0, 0)):
            for rate in (0, 1, 8, 20):
                for dist in (0, 511, 512, 0x3000, -513):
                    c = mk(r, fn)
                    c["bump"][0] = 0x4000
                    c["bump"][0x34 // 4] = 0
                    c["bump"][0x38 // 4] = 0
                    c["bump"][0x50 // 4] = r.choice([0, 128, 256, 300, 511])
                    c["bump"][0x6C // 4] = dx & MASK
                    c["bump"][0x70 // 4] = dy & MASK
                    c["args"] = [dx, dy]
                    c["owner"][0x28 // 4] = rate
                    c["owner"][0x54 // 4] = r.choice([0, 1])
                    c["limA"] = r.choice([0x30, 2])
                    c["limB"] = r.choice([0x16, 1000])
                    c["queues"]["BMP_Distance"] = [dist & MASK]
                    yield c
    return gen


TESTS = [
    FnTest("SetNavRoute", ppc, host, edge=edge_fn(FN_SET, 5), rnd=lambda r: mk(r, FN_SET)),
    FnTest("BumpToPoint", ppc, host, edge=edge_fn(FN_BUMP, 6), rnd=lambda r: mk(r, FN_BUMP)),
]
