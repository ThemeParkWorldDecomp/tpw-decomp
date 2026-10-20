"""Unit v5926, the bump AI and its helpers, against libtpw_unit_BMP_MaskCar_2.dylib (tests/unit_BMP_MaskCar_2.cpp), main app
("SimTheme Park"), src/BMP_InsideArea_PointOutAllInverse_BMP_Distance_BMP_MaskCar_2.cpp:
  BMP_MaskCar 0x1002ae74, BMP_Thrust 0x10029ac8, SetNextBuoyTarget 0x100296f4, BMP_AI 0x10029128

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit (BMP_Distance, PointInColObject, PointInAnyColObject, ClosestIntersectingColObjectToPoint, SetNavRoute, SetControlType,
BMP_LeaveBump, SOUND_FadeSoundEffect, BMP_DOCK, TRACE, CWorld::GetRand, the mesh draw calls and the ride head calls) are fakes on both
sides that record their arguments and answer from the case's queues.

The world of a case: the collision list words (bumps, buoys, count), a bump with its target bump, ride record, buoys and collision
objects (pointers are Mac addresses on the PowerPC side and arena offsets on the host), the world object, the sine table, the ride anim
sets. What is compared: the whole bump (pointer words as names), the ride record's state word, the event log.

No fused operations. Not tested: a ride record with a zero 0x68 (a divide by zero), a NULL answer of ClosestIntersectingColObjectToPoint
(read unconditionally), buoy chains that never end.
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BMP_MaskCar_2"
MASK = 0xFFFFFFFF

FN_MASK, FN_THRUST, FN_BUOY, FN_AI = 0x1002AE74, 0x10029AC8, 0x100296F4, 0x10029128
SIZES = {FN_MASK: 572, FN_THRUST: 168, FN_BUOY: 156, FN_AI: 1444}
OWN = set(SIZES)

ADDR = dict(list=0x102B56C0, sine=0x102B4EC0, gworld=0x1037EBFC, sets=0x102C79A4)
HOST_LAYOUT = [0x20, 0x20]

BLK = {"B": 0xAC, "BUMPS": 4 * 0xAC, "OWN": 0xD0, "BUOYS": 4 * 0x1C, "CA": 0x20, "CT": 0x20, "CBU": 0x20, "CC": 0x20, "WORLD": 0x20,
       "SEED": 0x10, "S1": 0x200, "S2": 0x200, "S3": 0x200, "M1": 0x20, "M2": 0x20, "M3": 0x20, "N0": 0x14, "N1": 0x14, "N2": 0x14}
LAB = {"B": 1, "OWN": 2, "CT": 3, "CBU": 4, "CC": 5, "WORLD": 6, "CA": 7, "M1": 8, "M2": 9, "M3": 10}

CC_ADDR = [0]
DUMPED = [False]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(0x1002B318, CL.recorder("BMP_Distance", "ii", ret="i"))
STUBS.internal(0x1002BC34, CL.recorder("PointInColObject", "iiL", ret="i"))
STUBS.internal(0x1002BA94, CL.recorder("PointInAnyColObject", "ii", ret="i"))
STUBS.internal(0x1002B4CC, CL.recorder("ClosestIntersectingColObjectToPoint", "Liii", ret=lambda e, log: CC_ADDR[0]))
STUBS.internal(0x100297CC, CL.recorder("SetNavRoute", "Lii", ret="i"))
STUBS.internal(0x1002D404, CL.recorder("SetControlType", "LL"))
STUBS.internal(0x1002E370, CL.recorder("BMP_LeaveBump", "L"))
STUBS.internal(0x100DE0FC, CL.recorder("SOUND_FadeSoundEffect", "i"))
STUBS.internal(0x1002E740, CL.recorder("BMP_DOCK", "L", ret="i"))
STUBS.internal(0x1006FCFC, CL.recorder("TRACE", "s"))
STUBS.internal(0x1013B1E4, CL.recorder("GetRand", "L", ret="i"))
STUBS.internal(0x1006548C, CL.recorder("DisableMeshDraw3", "Luu"))
STUBS.internal(0x100653A4, CL.recorder("DisableMeshDraw1", "L"))
STUBS.internal(0x100650C4, CL.recorder("EnableMeshDraw3", "Luu"))
STUBS.internal(0x10065178, CL.recorder("EnableMeshDraw1", "L"))
STUBS.internal(0x1020039C, CL.recorder("DisableRideHead", "Lu"))
STUBS.internal(0x10200230, CL.recorder("EnableRideHead", "Luu"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def init_blocks(self):
        self.blk = {n: self.alloc(sz) for n, sz in BLK.items()}
        self.names = {}

    def p(self, name, off=0):
        """The Ptr32 word of a block (registered under its name)."""
        w = self.pa(self.blk[name] + off)
        self.names[w] = name if off == 0 else "%s+%d" % (name, off)
        return w

    def norm(self, ws):
        return [self.names.get(w, w) for w in ws]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.init_blocks()
        CC_ADDR[0] = self.blk["CC"]
        self.L = e.cl = CL.PpcLog({self.blk[n]: l for n, l in LAB.items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def pa(self, a):
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_world(self):
        self.words(ADDR["gworld"], [self.blk["WORLD"]])

    def set_set(self, i, flags, mesh, fc0, fc4):
        a = self.blk["S%d" % i]
        self.words(ADDR["sets"] + 4 * i, [a])
        self.words(a + 4, [flags])
        self.words(a + 8, [self.blk[mesh]])
        self.words(a + 0xC0, [fc0, fc4])

    def call(self, op, args):
        if op == "mask":
            T.call_ppc(self.e, FN_MASK, [self.blk["B"], args & MASK])
        elif op == "thrust":
            T.call_ppc(self.e, FN_THRUST, [self.blk["B"], args & MASK])
        elif op == "buoy":
            T.call_ppc(self.e, FN_BUOY, [args, self.blk["B"]])
        else:
            T.call_ppc(self.e, FN_AI, [self.blk["B"]])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.init_blocks()
        lib.tpw_mc2_setcc(self.blk["CC"])
        CL.host_begin(lib, {l: self.blk[n] for n, l in LAB.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_mc2_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_mc2_alloc(n)

    def pa(self, a):
        return self.lib.tpw_mc2_off(a)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_world(self):
        self.lib.tpw_mc2_setworld(self.blk["WORLD"])

    def set_set(self, i, flags, mesh, fc0, fc4):
        a = self.blk["S%d" % i]
        self.lib.tpw_mc2_setrideset(i, a)
        self.lib.tpw_mc2_setanim(a, flags, self.blk[mesh], fc0, fc4)

    def call(self, op, args):
        L = self.lib
        if op == "mask":
            L.tpw_mc2_mask(self.blk["B"], s32(args))
        elif op == "thrust":
            L.tpw_mc2_thrust(self.blk["B"], s32(args))
        elif op == "buoy":
            L.tpw_mc2_buoy(args, self.blk["B"])
        else:
            L.tpw_mc2_ai(self.blk["B"])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_mc2_"
    def typ(n, res, args):
        f = getattr(lib, p + n)
        f.restype, f.argtypes = res, args
    typ("addr", C.c_uint64, [C.c_char_p])
    typ("alloc", C.c_uint64, [C.c_uint32])
    typ("off", C.c_uint32, [C.c_uint64])
    typ("setworld", None, [C.c_uint64])
    typ("setcc", None, [C.c_uint64])
    typ("setrideset", None, [C.c_uint32, C.c_uint64])
    typ("setanim", None, [C.c_uint64, C.c_uint32, C.c_uint64, C.c_uint32, C.c_uint32])
    typ("layout", C.c_int32,[C.POINTER(C.c_int64)])
    typ("ai", None, [C.c_uint64])
    typ("thrust", None, [C.c_uint64, C.c_int32])
    typ("buoy", None, [C.c_uint64, C.c_uint64])
    typ("mask", None, [C.c_uint64, C.c_int32])
    out = (C.c_int64 * 32)()
    n = lib.tpw_mc2_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

SINE = [s32(int(256 * math.sin(3.14159 * 2 * i / 512))) for i in range(512)]


def build(s, c):
    B = s.blk
    if c["op"] == "mask":
        s.words(B["B"], c["bw"])
        s.words(B["B"] + 0x30, [s.p("N0") if c["nodes"] else 0])
        for i, v in enumerate(c["nodes"]):
            s.words(B["N%d" % i], [0, 0, v, 0, s.p("N%d" % (i + 1)) if i + 1 < len(c["nodes"]) else 0])
        for i in (1, 2, 3):
            fl, fc0, fc4 = c["sets"][i - 1]
            s.set_set(i, fl, "M%d" % i, fc0, fc4)
        return
    s.words(B["B"], c["bw"])
    s.words(B["B"] + 0x98, [s.p("CA") if c["op"] == "thrust" else s.p("CT") if c["tcol"] else 0, s.p("OWN")])
    s.words(B["OWN"], c["ow"])
    if c["op"] == "thrust":
        s.words(s.g("sine"), SINE)
        s.words(B["CA"], [c["colflags"]])
        return
    s.words(B["OWN"] + 0xA4, [s.p("SEED"), s.p("BUOYS")])
    s.words(B["SEED"], [0, c["seedw"]])
    s.words(B["CC"], c["cc"])
    for i in range(4):
        s.words(B["BUMPS"] + 0xAC * i, c["tw"][i])
        s.words(B["BUMPS"] + 0xAC * i + 0x98, [s.p("CT")])
    for i, (fl, w4, w8, wc, ring, hascol) in enumerate(c["bu"]):
        nxt = s.p("BUOYS", 0x1C * (i + 1)) if i < 3 else (s.p("BUOYS") if ring else 0)
        s.words(B["BUOYS"] + 0x1C * i, [fl, w4, w8, wc, 0, nxt, s.p("CBU") if hascol else 0])
    s.words(s.g("list") + 0x10, [s.p("BUMPS")])
    s.words(s.g("list") + 0x20, [s.p("BUOYS") if c["haveBuoys"] else 0, c["count"]])
    s.set_world()


def execute(s, c):
    if c["op"] == "buoy":
        k = c["arg"]
        s.call("buoy", 0 if k < 0 else s.blk["BUOYS"] + 0x1C * k)
    else:
        s.call(c["op"], c.get("arg", 0))


def observe(s, c):
    n = 0x2B if c["op"] != "mask" else 0x2B
    out = {"b": s.norm(s.rd_words(s.blk["B"], n)), "log": s.log()}
    if c["op"] in ("ai", "buoy", "thrust"):
        out["o50"] = s.rd_word_at(s.blk["OWN"] + 0x50) if hasattr(s, "rd_word_at") else s.rd_words(s.blk["OWN"] + 0x50, 1)
    return out


def dump_tables(e):
    if DUMPED[0]:
        return
    DUMPED[0] = True
    print("table1", ["%08x" % e.u32(0x1029CAF0 + 4 * i) for i in range(14)])
    print("table2", ["%08x" % e.u32(0x1029CAB8 + 4 * i) for i in range(14)])


def run(s, c):
    try:
        build(s, c)
        execute(s, c)
        return observe(s, c)
    finally:
        s.done()


def ppc(e, c):
    dump_tables(e)
    return run(Ppc(e, c), c)


def host(lib, c):
    return run(Host(lib, c), c)


# ---------------------------------------------------------------------------------------------------------------- case generators

BOFF = dict(flags=0, c20=0x20, c74=0x74, c78=0x78, c7c=0x7C, c80=0x80, c88=0x88, c8c=0x8C, c90=0x90)
OOFF = dict(kind=0, o50=0x50, o54=0x54, o68=0x68, o90=0x90, o94=0x94)


def base_queues(r, o68, q):
    d = {"BMP_Distance": [r.randint(0, 300) for _ in range(3)],
         "PointInColObject": [r.choice([0, 1]) for _ in range(2)],
         "PointInAnyColObject": [r.choice([0, 5])],
         "SetNavRoute": [o68 + r.randint(-4, 4) if r.random() < 0.6 else r.randint(0, 500)],
         "BMP_DOCK": [0],
         "GetRand": [r.getrandbits(32)]}
    d.update(q)
    return d


def ai_case(r, op="ai", **kw):
    bw = [r.randint(-50, 50) for _ in range(43)]
    ow = [r.randint(-50, 50) for _ in range(0x34)]
    for off in (0x34, 0x38, 0x3C, 0x40):
        bw[off // 4] = r.randint(-300, 300)
    bw[0x80 // 4] = r.randint(0, 3)
    bw[0x7C // 4] = r.randint(0, 3)
    bw[0x8C // 4] = r.randint(-1, 4)
    bw[0x88 // 4] = r.choice([-1, 0, 1, 2, 5])
    bw[0] = sum(b for b in (1, 2, 4, 8, 0x10, 0x20, 0x40, 0x100, 0x4000, 0x8000, 0x10000, 0x20000, 0x40000, 0x80000) if r.random() < 0.3)
    ow[0] = r.randint(-16, 2)
    ow[0x50 // 4] = r.choice([0, 1, 2, 2, 3])
    ow[0x54 // 4] = r.choice([0, 1, 2])
    ow[0x68 // 4] = r.randint(1, 400)
    for key, v in kw.items():
        if key in BOFF:
            bw[BOFF[key] // 4] = v
        elif key in OOFF:
            ow[OOFF[key] // 4] = v
    tw = [[r.randint(-300, 300) for _ in range(43)] for _ in range(4)]
    tfl = kw.get("tfl", r.choice([1, 1, 0x21, 0x40001, 0, 3]))
    for i in range(4):
        tw[i][0] = tfl if i == bw[0x80 // 4] else r.choice([0, 1, 5])
    bu = []
    for i in range(4):
        fl = kw.get("bflags", [r.getrandbits(3) for _ in range(4)])[i]
        if i == 3:
            fl &= ~2
        bu.append((fl, r.randint(0, 3), r.randint(-200, 200), r.randint(-200, 200), r.random() < 0.5, r.random() < 0.5))
    if "bu" in kw:
        bu = kw["bu"]
    c = dict(op=op, bw=bw, ow=ow, tw=tw, bu=bu, tcol=kw.get("tcol", True), haveBuoys=kw.get("haveBuoys", r.random() < 0.9),
             count=r.randint(1, 8), seedw=r.randint(0, 3), cc=[0, r.randint(-300, 300), r.randint(-300, 300), r.randint(1, 100), 0, 0, 0, 0],
             arg=kw.get("arg", r.randint(-1, 3)))
    o68 = ow[0x68 // 4]
    q = dict(kw.get("q", {}))
    if "rc" in kw:
        q["SetNavRoute"] = [o68 + kw["rc"]]
    c["queues"] = base_queues(r, o68, q)
    return c


def buoy_mode(r):
    """A bump in the buoy branch (neither 4 nor 0x20 set) with the flags the branch tests."""
    fl = r.choice([8, 0x10, 0x40, 0x48, 0x58, 0x8, 0x1008, 0x10 | 0x8000])
    fl |= r.choice([0, 0, 0x20000, 0x8000 | 0x40000, 0x40000, 0x80000, 0x10000])
    buf = [r.choice([1, 5, 1, 0, 3, 7]) for _ in range(4)]
    return ai_case(r, flags=fl, bflags=buf, c88=r.choice([-2, -1, 0, 1, 2]), o50=r.choice([0, 1, 2, 3]),
                   kind=r.choice([-7, -9, -10, -4, -8, -3, -11, -1, 0, -14]), c8c=r.randint(0, 3), c7c=r.randint(0, 3))


def ai_edges():
    r = random.Random(926)
    for kind in range(-16, 3):
        for fl in (0, 0x4000, 0x20, 0x4000 | 0x20):
            yield ai_case(r, flags=fl, kind=kind, c88=1, o50=2, o54=0, rc=r.choice([-3, 0, 5]))
    for c88 in (-1, 0, 2):
        yield ai_case(r, flags=0x4000, kind=-5, c88=c88, o50=2, o54=0)
    for o50, o54 in ((1, 0), (2, 2), (3, 1)):
        yield ai_case(r, flags=0x4000, kind=-5, c88=1, o50=o50, o54=o54)
    for tfl in (1, 0, 0x21):
        for c8c in (0, 1, 3):
            for pic in (0, 1):
                for pia in (0, 5):
                    for f2 in (0, 0x20000):
                        for rel in (-1, 0, 1):
                            yield ai_case(r, flags=4 | f2, tfl=tfl, c8c=c8c, o68=100, rc=rel,
                                          q={"PointInColObject": [pic], "PointInAnyColObject": [pia], "BMP_Distance": [r.randint(0, 3), r.randint(0, 100), r.randint(0, 100)]})
    for d in ([0, 0, 0], [1, 50, 0], [5, 300, 7], [30, 10, 31]):
        yield ai_case(r, flags=4, tfl=1, c8c=3, q={"PointInColObject": [0], "PointInAnyColObject": [0], "BMP_Distance": d})
    for kind in range(-16, 3):
        for f2 in (0, 0x20000):
            for rel in (-1, 0, 1):
                yield ai_case(r, flags=0x20 | f2, kind=kind, o68=100, rc=rel, c88=r.choice([-1, 5]))
    for fl in (0x48000 | 8, 0x48000 | 0x40, 0x8000 | 8):
        for rel in (-1, 1):
            yield ai_case(r, flags=fl, o68=100, rc=rel)
    for _ in range(150):
        yield buoy_mode(r)
    yield ai_case(r, flags=8, haveBuoys=False)
    yield ai_case(r, flags=0, haveBuoys=True)


def ai_random(r):
    return ai_case(r)


def buoy_edges():
    r = random.Random(927)
    for k in (-1, 0, 1, 2, 3):
        for ring in (False, True):
            for fl in ([1, 1, 1, 1], [0, 2, 2, 0], [2, 2, 2, 0], [0, 0, 0, 0]):
                bu = [(fl[i], r.randint(0, 3), 1, 2, ring, False) for i in range(4)]
                yield ai_case(r, "buoy", arg=k, bu=bu)


def buoy_random(r):
    return ai_case(r, "buoy")


def thrust_case(r, flags=None, o54=None, colflags=None, amount=None, b50=None):
    bw = [r.randint(-50, 50) for _ in range(43)]
    bw[0] = r.choice([0x4000, 0x4000, 0x84000, 0, 0x4001, 0x80000]) if flags is None else flags
    bw[0x50 // 4] = r.choice([0, 0x1FF, 0x200, 0x180, 0x7F, 0xFFFFFFFF, r.getrandbits(32), r.randint(0, 600)]) if b50 is None else b50
    bw[0x68 // 4] = r.randint(-300, 300)
    bw[0x3C // 4] = r.randint(-1000, 1000)
    bw[0x40 // 4] = r.randint(-1000, 1000)
    ow = [r.randint(-50, 50) for _ in range(0x34)]
    ow[0x54 // 4] = r.choice([0, 1, 2]) if o54 is None else o54
    return dict(op="thrust", bw=bw, ow=ow, queues={}, tcol=True,
                colflags=r.choice([0x1E, 0x1F, 0, 0x1C, 0x5E, 0x1E]) if colflags is None else colflags,
                arg=r.choice([0, 1, -1, 3, -3, 4, -4, r.randint(-500, 500), r.randint(-3000000, 3000000)]) if amount is None else amount)


def thrust_edges():
    r = random.Random(928)
    for fl in (0x4000, 0x84000, 0, 0x80000, 0x4001):
        for o54 in (2, 0):
            for cf in (0x1E, 0x1C, 0x1F):
                for amt in (0, 1, -1, 3, -3, 5, -5, 1000):
                    yield thrust_case(r, fl, o54, cf, amt)
    for b50 in (0, 0x7F, 0x80, 0x1FF, 0x200, 0x17F, 0x180, 0xFFFFFFFF, 0x80000000):
        for amt in (100, -100, 7):
            yield thrust_case(r, 0x4000, 0, 0x1C, amt, b50)


def thrust_random(r):
    return thrust_case(r)


def mask_case(r, flags=None, mask=None):
    bw = [r.randint(-50, 50) for _ in range(43)]
    bw[0] = r.choice([0, 0x200, 0x2000, 0x2000000, 0x2002000, 0x202000, 0x200 | 0x2000000]) if flags is None else flags
    bw[2] = r.randint(1, 3)
    bw[3] = r.choice([0, 1, 2, 3])
    n = r.randint(0, 3)
    nodes = [r.choice([0xFFFFFFFF, 0, 1, 7, 0xFFFEFFFF, r.getrandbits(32)]) for _ in range(n)]
    sets = [(r.choice([0x4000, 0, 0x4001, 0x3000]), r.getrandbits(32), r.getrandbits(32)) for _ in range(3)]
    return dict(op="mask", bw=bw, queues={}, nodes=nodes, sets=sets, arg=r.choice([0, 1, -1, 5]) if mask is None else mask)


def mask_edges():
    r = random.Random(929)
    for fl in (0, 0x200, 0x2000, 0x2000000, 0x2002000, 0x202000, 0x200 | 0x2000):
        for mask in (0, 1, -1):
            for _ in range(4):
                yield mask_case(r, fl, mask)


def mask_random(r):
    return mask_case(r)


TESTS = [
    FnTest("BMP_MaskCar", ppc, host, edge=mask_edges, rnd=mask_random),
    FnTest("BMP_Thrust", ppc, host, edge=thrust_edges, rnd=thrust_random),
    FnTest("SetNextBuoyTarget", ppc, host, edge=buoy_edges, rnd=buoy_random),
    FnTest("BMP_AI", ppc, host, edge=ai_edges, rnd=ai_random),
]
