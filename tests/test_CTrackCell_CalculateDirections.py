"""Unit v5696, CTrackCell::CalculateDirections and CTrackCell::GetTile, against libtpw_unit_CTrackCell_CalculateDirections.dylib
(tests/unit_CTrackCell_CalculateDirections.cpp), main app ("SimTheme Park"), src/game/park/TrackCell.cpp:
  CalculateDirections 0x100a12dc, GetTile 0x100a2a0c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (GetNeighbours,
GetDirection, GetMapCellEquiv both forms, NumberOfHVNeighbours, ModifyDirection, IsStraight, IsCorner, IsCrossRoad, IsBridge, OPPOSITE,
GetRand) are fakes on both sides that record their arguments and answer from per-case queues (the last answer repeats). abs is a pure
import, computed the same on both sides.

The world of a case: the cell (id, mode, flags), the four tile tables and their counts, the hoarding corners, the two scratch words
(last, rand), the cell-system flag byte and the map-cell equivalent's mode. What is compared: the answer, the rotation out word, the
globals the code writes, the event log.

No fused operations. Not tested: a rand word of 0x1e or more (the original then uses the caller's r20, an undefined register; the game
only stores remainders of 0x1e).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CTrackCell_CalculateDirections"
MASK = 0xFFFFFFFF

FN_CD, FN_GT = 0x100A12DC, 0x100A2A0C
SIZES = {FN_CD: 392, FN_GT: 1728}
OWN = set(SIZES)

C_GETNB, C_GETDIR, C_EQC, C_EQ = 0x100816E8, 0x10081930, 0x100F4928, 0x100F49A0
C_NUMHV, C_MODDIR, C_STRAIGHT, C_CORNER = 0x10081890, 0x10081810, 0x100A3868, 0x100A37A0
C_CROSS, C_BRIDGE, C_OPP, C_RAND = 0x100A3914, 0x100A34C4, 0x10102500, 0x1013B1E4

ADDR = dict(tableA=0x1029D6E8, countA=0x1029D7A8, tableB=0x1029D7AC, countB=0x1029D86C, last=0x102E484C, rand=0x102E4850,
            tableC=0x1029D8F8, countC=0x1029DB44, right=0x1029E03C, bottom=0x1029E040, left=0x1029E044, top=0x1029E048,
            flag=0x102E4818, world=0x1037EBFC)
HOST_LAYOUT = [8, 0xE, 0, 12, 8, 0x28]
TABLES = (("tableA", "countA", 16), ("tableB", "countB", 16), ("tableC", "countC", 49))

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk_ret(name, mask=None):
    def f(e):
        e.cl.rec(name, ())
        v = e.cl.ret(name)
        e.set_r(3, (v if mask is None else v & mask) & MASK)
    return f


def stub_eq(name):
    def f(e):
        e.cl.rec(name, ())
        e.set_r(3, e.cl.eq)
    return f


def stub_moddir(e):
    e.cl.rec("ModifyDirection", (e.r(4) & 0xFF,))


def stub_opp(e):
    e.cl.rec("OPPOSITE", (e.r(3) & 0xFF,))
    e.set_r(3, e.cl.ret("OPPOSITE") & 0xFF)


def stub_rand(e):
    e.cl.rec("GetRand", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("GetRand") & MASK)


def stub_abs(e):
    v = T.s32(e.r(3))
    e.set_r(3, (-v if v < 0 else v) & MASK)


STUBS.internal(C_GETNB, mk_ret("GetNeighbours", 0xFF))
STUBS.internal(C_GETDIR, mk_ret("GetDirection", 0xFF))
STUBS.internal(C_EQC, stub_eq("GetMapCellEquivC"))
STUBS.internal(C_EQ, stub_eq("GetMapCellEquiv"))
STUBS.internal(C_NUMHV, mk_ret("NumberOfHVNeighbours"))
STUBS.internal(C_MODDIR, stub_moddir)
STUBS.internal(C_STRAIGHT, mk_ret("IsStraight", 0xFF))
STUBS.internal(C_CORNER, mk_ret("IsCorner", 0xFF))
STUBS.internal(C_CROSS, mk_ret("IsCrossRoad", 0xFF))
STUBS.internal(C_BRIDGE, mk_ret("IsBridge"))
STUBS.internal(C_OPP, stub_opp)
STUBS.internal(C_RAND, stub_rand)
STUBS.imp("abs", stub_abs)


def setup(runner):
    T.install_std_stubs(runner.emu)


PROBED = []


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def hw(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))


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

    def set_eq(self, blk):
        self.L.eq = blk

    def calldir(self, cell, a, b):
        T.call_ppc(self.e, FN_CD, [cell, a & MASK, b & MASK])
        return 0

    def gettile(self, cell, rot):
        r3, _, _ = T.call_ppc(self.e, FN_GT, [cell, rot])
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
        return self.lib.tpw_tcd_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_tcd_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_eq(self, blk):
        self.write(self.g("eq"), struct.pack("<Q", blk))

    def calldir(self, cell, a, b):
        self.lib.tpw_tcd_calcdir(cell, a, b)
        return 0

    def gettile(self, cell, rot):
        return self.lib.tpw_tcd_gettile(cell, rot) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_tcd_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "calcdir").restype, getattr(lib, p + "calcdir").argtypes = None, [C.c_uint64, C.c_int32, C.c_int32]
    getattr(lib, p + "gettile").restype, getattr(lib, p + "gettile").argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    cell = s.alloc(0x28)
    s.hw(cell, case["id"])
    s.words(cell + 8, [case["mode"]])
    s.hw(cell + 0xE, case["flags"])
    s.write(s.g("flag"), bytes([case["flag"]]))
    blk = s.alloc(16)
    s.words(blk + 8, [case["eqtype"]])
    s.set_eq(blk)
    world = s.alloc(16)
    if isinstance(s, Host):
        s.write(s.g("world"), struct.pack("<Q", world))
    else:
        s.words(s.g("world"), [world])
    for name, cname, size in TABLES:
        t = case[name]
        a = s.g(name)
        for i, (tile, rot, mask) in enumerate(t):
            s.words(a + 12 * i, [tile, rot])
            s.write(a + 12 * i + 8, bytes([mask]))
        s.words(s.g(cname), [case[cname]])
    for n in ("right", "bottom", "left", "top", "last", "rand"):
        s.words(s.g(n), [case[n]])
    return cell


def execute(s, case, cell):
    if case["op"] == "cd":
        return s.calldir(cell, *case["args"])
    rot = s.alloc(4)
    s.words(rot, [0x5A5A5A5A])
    ans = s.gettile(cell, rot)
    case["_rot"] = s.rd_word(rot)
    return ans


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "rot": case.get("_rot"),
            "last": s.rd_word(s.g("last")), "rand": s.rd_word(s.g("rand"))}


def run(s, case):
    try:
        if not PROBED and isinstance(s, Ppc):
            PROBED.append(1)
            print("JT", " ".join("%d:%x" % (i, x & 0xFFF) for i, x in enumerate(s.rd_words(0x1029E2DC, 26))))
            for base_, nm in ((0x1029D6E8, "A"), (0x1029D7AC, "B")):
                w = s.rd_words(base_, 48)
                for i in range(0, 48, 12):
                    print(nm, i // 3, " ".join("%x" % x for x in w[i:i + 12]))
        cell = build(s, case)
        return observe(s, case, execute(s, case, cell))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

NBS = [0, 1, 4, 5, 0x10, 0x11, 0x14, 0x15, 0x40, 0x41, 0x44, 0x45, 0x50, 0x51, 0x54, 0x55, 0xFF, 0x80, 0x0A]
DIRS = [0, 1, 4, 5, 0x10, 0x14, 0x40, 0x41, 0x50, 0x15, 0x45, 0x55, 0x80, 0xC4]
MODES = list(range(-3, 0x1C)) + [0x1D, 0x30, -0x10, -0x19, -0x1A, -0x80000000, 0x7FFFFFFF, 0x20, 0x1A]
HOT = [0x10, 0xB, 0x19, 1, 2, 3, 6, 8, 0xD, 0xE, 0xF, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x1A]


def lst(r, pool, lo=1, hi=3):
    return [r.choice(pool) for _ in range(r.randint(lo, hi))]


def queues(r):
    return {
        "GetNeighbours": lst(r, NBS), "GetDirection": lst(r, DIRS),
        "NumberOfHVNeighbours": lst(r, [0, 1, 2, 3, 4, -1]),
        "IsStraight": lst(r, [0, 0, 0x11, 0x44, 0x55]), "IsCorner": lst(r, [0, 0, 0x14, 0x41]),
        "IsCrossRoad": lst(r, [0, 0, 0x55]), "IsBridge": lst(r, [0, 1, 0, 2]),
        "OPPOSITE": lst(r, NBS + [r.randrange(256)]), "GetRand": lst(r, [r.randrange(1 << 32) for _ in range(4)] + [0, 0x1D, 0x1E, 0x23]),
    }


def table(r, size):
    t = []
    for _ in range(size):
        tile = r.choice([MASK, r.randrange(0, 64), r.randrange(0, 0x60000), r.randrange(1 << 32)])
        t.append((tile, r.choice([0, 90, 180, 270, r.randrange(1 << 32)]), r.choice(NBS + [r.randrange(256)])))
    return t


def base(r, op):
    c = {"op": op, "queues": queues(r), "id": r.choice([0, 1, r.randrange(0x4000), r.randrange(0x10000)]),
         "mode": r.choice(r.choice([MODES, HOT, HOT])) & MASK, "flags": r.choice([0, 1, 2, 4, 8, 0xF, r.randrange(0x10000)]),
         "flag": r.choice([0, 1, 255]), "eqtype": r.choice([0x15, 0x15, r.randrange(32)]), "last": r.choice([0, 0x14, 0x17, r.randrange(0x20)]),
         "rand": r.choice([0, 0x11, 0x12, 0x17, 0x18, 0x1D, r.randrange(0x1E), (-r.randrange(1, 9)) & MASK])}
    for name, cname, size in TABLES:
        c[name] = table(r, size)
        c[cname] = r.choice([0, 1, size, size, r.randrange(size + 1)])
    ident = c["id"]
    x, y = (ident - 1) & 0x7F, ((ident - 1) & MASK) >> 7
    k = r.randrange(4)
    c["right"], c["bottom"], c["left"], c["top"] = [r.randrange(0x80) for _ in range(4)]
    if k == 1:
        c["right"], c["bottom"] = x, y
    elif k == 2:
        c["left"], c["top"] = x, y
    elif k == 3:
        c["right"], c["bottom"], c["left"] = x, r.randrange(2), x
        c["top"] = y
    return c


def cd_case(r, mode=None, flag=None, a=None, b=None):
    c = base(r, "cd")
    if mode is not None:
        c["mode"] = mode
    if flag is not None:
        c["flag"] = flag
    c["args"] = [r.choice([-2, 2, 0, 1, -1, 3]) if a is None else a, r.choice([-2, 2, 0, 1, -1, 3]) if b is None else b]
    return c


def cd_edges():
    r = random.Random(5696)
    for flag in (0, 1):
        for mode in (0xB, 0xC, 0x10, 0x11, 0x15, 0, 0x19):
            for a in (-2, 2, 0):
                for b in (-2, 2, 1):
                    yield cd_case(r, mode, flag, a, b)


def cd_random(r):
    return cd_case(r)


def gt_case(r, mode=None):
    c = base(r, "gt")
    if mode is not None:
        c["mode"] = mode & MASK
    return c


def gt_edges():
    r = random.Random(5697)
    for mode in MODES:
        for _ in range(3):
            yield gt_case(r, mode)


def gt_random(r):
    return gt_case(r)


TESTS = [
    FnTest("CalculateDirections", ppc, host, edge=cd_edges, rnd=cd_random),
    FnTest("GetTile", ppc, host, edge=gt_edges, rnd=gt_random),
]
