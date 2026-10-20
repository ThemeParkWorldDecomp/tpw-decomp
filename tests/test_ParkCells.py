"""Differential tests for unit w156, the park analyser's cell and map queries (src/ParkAnalyser_cells.cpp). See difftest.py and tpwtest.py.

The three counters of CParkAnalyserCell, the three park sums, GetAnalyserCell, IsNearbyThingOfType, GetNearbyLitteredCell, GetPercentageLitteredCells,
ClearMapData, MarkAreaPatrolledBy, PercentageOfAreaPatrolledBy, PercentageOfParkCoveredByCameras and NumberOfCellsCoveredByObjects (15 functions).

Both sides work in ONE 8 MB block (the PowerPC side maps it at RB, the host library's gRegion is the same layout): the call trace at 0x100 (tag, value pairs,
pointers as offsets), the region effect table, the analyser, eight thing blocks and four world-sized slots (W1 the world GetWorld answers, W2 gWorld, W3 the
block gWorldMap points into, W4 the map GetMap answers; each map at slot + 0x2d8). Every callee outside the unit is a fake that appends the same (tag, value)
to the trace on both sides. The eleven CMapCell predicates and the effect-cell lookup would be too slow as Python stubs (up to 16384 cells per case), so on the
PowerPC side the original code at their addresses is replaced by a few machine code instructions that append the same trace entry and answer from bit
(tag - 1) of the cell's halfword at +0x42 (the host fake does the same on CMapCell::mF42). A case compares the answer, the whole trace, the analyser block
(the litter list), and the maps' bytes (checksums). CTimeSig::SecondsSinceLastUpdate (0x1015f124) runs as the original over the world accessor's fake.
Not tested: hit counts near 0x147ae15 (a percent product that wraps), mMapSquaresPerCell 0, patrol end ids 0 or above 0x4000, a worked-out flag other than 0 or 1.
"""
import array
import ctypes as C
import random
import struct
import zlib

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ParkCells"

MASK = 0xFFFFFFFF
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF
RB = 0x70000000
REGION = 0x800000
TRACE, CAP = 0x100, 0x30000
EFFECT, ANAL, ANAL_SIZE, THINGS = 0x1A0000, 0x1B0000, 0x32000, 0x1F0000
W1, W2, W3, W4 = 0x200000, 0x3E0000, 0x500000, 0x620000
SLOTS = {1: W1, 2: W2, 3: W3, 4: W4}
MAP_AT, ZONE_AT, ZONE_LEN, TURN_AT = 0x2D8, 0x294, 0x4001 * 0x44, 0x1DA70C
TABLE, G_MAP, G_WORLD = 0x1034CBFC, 0x1037EC08, 0x1037EC0C
CODE = 0x00100000

ASSERT, GETWORLD, GETMAP, VALGET, VALSET = 0x1000DDE8, 0x1014205C, 0x10142098, 0x100EF3C8, 0x100EF400
HEAD, NEXT, HASPATROL, GETPATROL, COUNTN = 0x1013ADB0, 0x1013AE08, 0x10127A78, 0x1012798C, 0x100E6A6C
EFFECTFN, SECONDS = 0x10101640, 0x1015F124
PRED = {1: 0x1009C408, 2: 0x1009C44C, 3: 0x1009C490, 4: 0x1009C4DC, 5: 0x1009C524, 6: 0x1009C568, 7: 0x1009C59C, 8: 0x1009C5EC, 9: 0x1009C61C,
        10: 0x1009C7B0, 11: 0x1009C96C}
CELL_FN = (0x100E659C, 0x100E670C, 0x100E686C)
SUM_FN = (0x100E571C, 0x100E57D4, 0x100E588C)
GETCELL, NEARTYPE, NEARLIT, PCTLIT = 0x100E4C38, 0x100E6C90, 0x100E6D38, 0x100ED738
MARK, PATROLLED, CAMERAS, OBJECTS = 0x100EE694, 0x100EE924, 0x100EECD8, 0x100EEED0
CLEAR = 0x100EEE30
OWN = set(CELL_FN) | set(SUM_FN) | {GETCELL, NEARTYPE, NEARLIT, PCTLIT, MARK, PATROLLED, CAMERAS, CLEAR, OBJECTS, SECONDS}
HOST_LAYOUT = [0xC, 0x10, 0x20, 0x1F020, 0x21C48, 0x7C, 0x4C, 0x58, 0x64, 0x2D8, 0x1DA70C, 0x1DA748, 0x2C, 0x30, 0x3C, 0x42, 2, 0x44]
TRUTHY = [1, 1, 1, 2, 0x100, -1, INT_MIN]
WHO = 0x0123
NV = 10
SETUP_V = (20, 21, 22, 23)


def s32(v):
    return T.s32(v)


def rep(b, n):
    return int.from_bytes(bytes([b]) * n, "big")


# ------------------------------------------------------------------ the maps (a variant is a seed; the bytes depend on the side's byte order)

ENABLE = [(0xFF, 7), (0xFF, 7), (0xFF, 7), (0x0F, 0), (0xF0, 7), (0x03, 0), (0xFC, 7), (0x1B, 5)]
_cache = {}


def field(ba, off, nbytes, pool, big, sel):
    for i in range(nbytes):
        shift = 8 * (nbytes - 1 - i) if big else 8 * i
        table = bytes((pool[b] >> shift) & 0xFF for b in range(256))
        ba[off + i::0x44] = sel.translate(table)


def genmap(v, big):
    """The 0x4001 entries from the id 0 guard to the id 0x4000 cell; the predicate bits, litter, collector and patrol mark vary per variant."""
    key = (v, big)
    if key in _cache:
        return _cache[key]
    r = random.Random(7000 + v)
    n = 0x4001
    ba = bytearray(r.randbytes(n * 0x44))

    def stream():
        return int.from_bytes(r.randbytes(n), "big")

    def dense(mode):
        a, b, c = stream(), stream(), stream()
        return [a, a & b, a & b & c, a | b][mode]

    mode = v % 4
    lo_en, hi_en = ENABLE[v % 8]
    lo = dense(mode) & rep(lo_en, n)
    hi = dense((mode + 1) % 4) & rep(hi_en | 0xF8, n)
    if v == 8:
        lo, hi = 0, hi & rep(0xF8, n)
    if v == 9:
        lo, hi = rep(0xFF, n), rep(7, n) | (hi & rep(0xF8, n))
    lob, hib = lo.to_bytes(n, "big"), hi.to_bytes(n, "big")
    ba[0x42::0x44] = hib if big else lob
    ba[0x43::0x44] = lob if big else hib
    zc = 256 if v == 8 else [128, 200, 240, 60][v % 4]
    nz = [1, 2, 3, 8, 0x01000000, 0x100, 0x80000000, 0xFFFFFFFF, 0x10000, 0x7FFFFFFF, 5, 0x00FF0000]
    pool = [0] * zc + [nz[i % len(nz)] for i in range(256 - zc)]
    r.shuffle(pool)
    field(ba, 0x2C, 4, pool, big, r.randbytes(n))
    cv = [0, WHO, 0x456, 0x2301, 1, 0x100, 0xFFFF, 0x8000]
    w = [[40, 120, 30, 10, 10, 10, 10, 26], [200, 30, 10, 4, 3, 3, 3, 3], [10, 200, 10, 6, 6, 8, 8, 8]][v % 3]
    pool = [x for x, k in zip(cv, w) for _ in range(k)]
    r.shuffle(pool)
    field(ba, 0x30, 2, pool, big, r.randbytes(n))
    fv = [0, 1, 2, 0xFFFFFFFF, 0x80000000, 5, 0x7FFFFFFF, 0xFFFF]
    w = [100, 60, 20, 20, 20, 20, 8, 8]
    pool = [x for x, k in zip(fv, w) for _ in range(k)]
    r.shuffle(pool)
    field(ba, 0x3C, 4, pool, big, r.randbytes(n))
    _cache[key] = bytes(ba)
    return _cache[key]


def things_bytes(c):
    ba = bytearray(b"\xa5" * (8 * 0x100))
    for i, kind in enumerate(c.get("kinds", [])):
        ba[0x100 * i + 2] = kind & 0xFF
    return bytes(ba)


def effect_bytes(c, big):
    out = b""
    for cell in c["effect"]:
        out += struct.pack((">" if big else "<") + "5h", *cell)
    return out


def analyser_bytes(c, big):
    e = ">" if big else "<"
    ba = bytearray(b"\xa5" * ANAL_SIZE)
    if "squares" in c:
        struct.pack_into(e + "ii", ba, 0xC, c["squares"], c["gridx"])
    cells = sum_cells(c) if "cs" in c else c.get("cells", {})
    for idx, (rect, vals) in cells.items():
        off = 0x20 + 0x7C * idx
        struct.pack_into(e + "iiii", ba, off, *rect)
        for at, (stamp, val, worked) in zip((0x4C, 0x58, 0x64), vals):
            struct.pack_into(e + "IiB", ba, off + at, stamp & MASK, val, worked)
    return bytes(ba)


def valtriple(r, turn):
    delta = r.choice([0, 0, 1, 2, 5, 6, 7, 100, 101, -1, -100, 0x1000])
    return ((turn - 4 * delta + r.randrange(4)) & MASK, r.randint(INT_MIN, INT_MAX), r.choice([0, 1, 1, 1]))


def sum_cells(c):
    r = random.Random(c["cs"])
    out = {}
    for i in range(0x400):
        sx, sy = r.randint(-1, 126), r.randint(0, 126)
        rect = (sx, sy, r.choice([0, 1, 1, 2, -1]), r.choice([1, 1, 2, 0]))
        out[i] = (rect, [valtriple(r, c["turn"]) for _ in range(3)])
    return out


# ------------------------------------------------------------------ the two sides' memory, written from one description


class PpcSide:
    big = True

    def __init__(self, e):
        self.e = e

    def write(self, off, data):
        self.e.write(RB + off, bytes(data))

    def read(self, off, n):
        return self.e.read(RB + off, n)


class HostSide:
    big = False

    def __init__(self, lib):
        self.lib, self.base = lib, lib.tpw_pl_base()

    def write(self, off, data):
        data = bytes(data)
        C.memmove(C.c_void_p(self.base + off), data, len(data))

    def read(self, off, n):
        return C.string_at(self.base + off, n)


def build(side, c, spec):
    if spec["analyser"]:
        side.write(ANAL, analyser_bytes(c, side.big))
    if spec["things"]:
        side.write(THINGS, things_bytes(c))
    if spec["slot"]:
        side.write(SLOTS[spec["slot"]] + ZONE_AT, genmap(c["V"], side.big))
    if "turn" in c:
        side.write(W1 + TURN_AT, struct.pack(">I" if side.big else "<I", c["turn"] & MASK))
    if "effect" in c:
        side.write(EFFECT, effect_bytes(c, side.big))


def row_slot(c, row):
    s = c.get("rows", {}).get(row, 0)
    return s


def trace_of(side, tags_vals):
    return tags_vals


def observe(side, c, spec, ret, n, tags, vals):
    out = dict(ret=ret, n=n, tags=tags, vals=vals)
    if spec["obs_analyser"]:
        raw = side.read(ANAL, ANAL_SIZE)
        if not side.big:   # the list of halfword ids: native on the host, big-endian on the PowerPC side
            words = array.array("H")
            words.frombytes(raw[0x1F020:])
            words.byteswap()
            raw = raw[:0x1F020] + words.tobytes()
        out["analyser"] = raw
    slots = (1, 2, 3, 4) if spec["writes"] else ((spec["slot"],) if spec["slot"] else ())
    order = range(4) if side.big else range(3, -1, -1)
    out["maps"] = []
    for s in slots:
        zone = side.read(SLOTS[s] + ZONE_AT, ZONE_LEN)
        out["maps"].append(zlib.crc32(b"".join(zone[0x3C + i::0x44] for i in order)))
    return out


# ------------------------------------------------------------------ the PowerPC side


def tr(e, tag, val):
    n = e.u32(RB)
    e.write(RB + TRACE + 8 * n, struct.pack(">II", tag, val & MASK))
    e.write(RB, struct.pack(">I", n + 1))


def rel(p):
    return (p - RB) & MASK


def text_hash(s):
    h = 0
    for ch in s.encode("latin-1"):
        h = (h * 31 + ch) & MASK
    return h


def stubs_for(e, c):
    walk = c.get("walk", [])
    state = {"at": 0}
    seed, count = c["seed"] & MASK, c.get("count", 0) & MASK
    patrol = c.get("patrol", {})

    def assertion(emu):
        tr(emu, 20, emu.r(3))
        tr(emu, 21, text_hash(emu.cstr(emu.r(4))))

    def getworld(emu):
        tr(emu, 22, rel(emu.r(3)))
        emu.set_r(3, RB + W1)

    def getmap(emu):
        tr(emu, 23, rel(emu.r(3)))
        emu.set_r(3, RB + W4 + MAP_AT)

    def valget(emu):
        off = rel(emu.r(3))
        tr(emu, 24, off)
        emu.set_r(3, ((off + 1) * 0x9E3779B1 ^ seed) & MASK)

    def valset(emu):
        tr(emu, 25, rel(emu.r(3)))
        tr(emu, 26, emu.r(4))

    def head(emu):
        tr(emu, 27, rel(emu.r(4)))
        state["at"] = 0
        e.write(emu.r(3), struct.pack(">H", walk[0] if walk else 0))

    def nxt(emu):
        tr(emu, 28, rel(emu.r(4)))
        tr(emu, 29, rel(emu.r(5)))
        state["at"] += 1
        e.write(emu.r(3), struct.pack(">H", walk[state["at"]] if state["at"] < len(walk) else 0))

    def block(thing):
        b = rel(thing) - THINGS
        return b // 0x100 if 0 <= b < 8 * 0x100 else -1

    def haspatrol(emu):
        tr(emu, 30, rel(emu.r(3)))
        b = block(emu.r(3))
        emu.set_r(3, patrol[b][0] & MASK if b in patrol else 0)

    def getpatrol(emu):
        tr(emu, 31, rel(emu.r(3)))
        b = block(emu.r(3))
        start, end = (patrol[b][1], patrol[b][2]) if b in patrol else (0, 0)
        e.write(emu.r(4), struct.pack(">H", start & 0xFFFF))
        e.write(emu.r(5), struct.pack(">H", end & 0xFFFF))

    def countn(emu):
        tr(emu, 32, rel(emu.r(3)))
        tr(emu, 33, emu.r(4))
        tr(emu, 34, emu.r(5) >> 16)
        tr(emu, 35, emu.r(6))
        tr(emu, 36, emu.r(7))
        emu.set_r(3, count)

    S = CL.Stubs()
    S.real = OWN
    for addr, fn in ((ASSERT, assertion), (GETWORLD, getworld), (GETMAP, getmap), (VALGET, valget), (VALSET, valset), (HEAD, head), (NEXT, nxt),
                     (HASPATROL, haspatrol), (GETPATROL, getpatrol), (COUNTN, countn)):
        S.internal(addr, fn)
    S.imp("labs", lambda emu: emu.set_r(3, T.u32(abs(s32(emu.r(3))))))
    return S


def enc(*words):
    return b"".join(struct.pack(">I", w & MASK) for w in words)


def D(op, rt, ra, imm):
    return (op << 26) | (rt << 21) | (ra << 16) | (imm & 0xFFFF)


def X(rt, ra, rb, xo):
    return (31 << 26) | (rt << 21) | (ra << 16) | (rb << 11) | (xo << 1)


def rlwinm(ra, rs, sh, mb, me):
    return (21 << 26) | (rs << 21) | (ra << 16) | (sh << 11) | (mb << 6) | (me << 1)


def predicate_code(tag):
    """r11 = RB; the entry n = *RB is appended at RB + 0x100 + 8n (tag, this - RB); answers *(RB + 4) when bit tag - 1 of the halfword at this + 0x42 is set."""
    return enc(D(15, 11, 0, 0x7000), D(32, 12, 11, 0), rlwinm(10, 12, 3, 0, 28), X(10, 11, 10, 266), D(14, 12, 12, 1), D(36, 12, 11, 0),
               D(14, 12, 0, tag), D(36, 12, 10, 0x100), X(12, 11, 3, 40), D(36, 12, 10, 0x104), D(40, 0, 3, 0x42), D(28, 0, 0, 1 << (tag - 1)),
               (16 << 26) | (12 << 21) | (2 << 16) | 12, D(32, 3, 11, 4), 0x4E800020, D(14, 3, 0, 0), 0x4E800020)


def effect_code():
    return enc(D(15, 11, 0, 0x7000), D(32, 12, 11, 0), rlwinm(10, 12, 3, 0, 28), X(10, 11, 10, 266), D(14, 9, 0, 40), D(36, 9, 10, 0x100), X(9, 11, 3, 40),
               D(36, 9, 10, 0x104), D(14, 9, 0, 41), D(36, 9, 10, 0x108), rlwinm(9, 4, 16, 16, 31), D(36, 9, 10, 0x10C), D(14, 12, 12, 2),
               D(36, 12, 11, 0), D(28, 9, 12, 15), D(7, 12, 12, 10), D(15, 3, 11, 0x1A), X(3, 3, 12, 266), 0x4E800020)


def ba_to(addr):
    return enc((18 << 26) | (addr & 0x03FFFFFC) | 2)


def setup(runner):
    emu = runner.emu
    T.install_std_stubs(emu)
    emu.uc.mem_map(RB, REGION)
    emu.uc.mem_map(CODE, 0x1000)
    k = 0
    for tag, addr in sorted(PRED.items()):
        emu.write(CODE + 0x80 * k, predicate_code(tag))
        emu.write(addr, ba_to(CODE + 0x80 * k))
        k += 1
    emu.write(CODE + 0x80 * k, effect_code())
    emu.write(EFFECTFN, ba_to(CODE + 0x80 * k))
    emu.write(G_MAP, struct.pack(">I", RB + W3 + MAP_AT))
    emu.write(G_WORLD, struct.pack(">I", RB + W2))
    for slot, v in zip((1, 2, 3, 4), SETUP_V):
        emu.write(RB + SLOTS[slot] + ZONE_AT, genmap(v, True))
    bind(runner.lib)
    side = HostSide(runner.lib)
    for slot, v in zip((1, 2, 3, 4), SETUP_V):
        side.write(SLOTS[slot] + ZONE_AT, genmap(v, False))
    runner.lib.tpw_pl_init()


def bind(lib):
    lib.tpw_pl_base.restype = C.c_uint64
    lib.tpw_pl_begin.argtypes = [C.c_int32, C.c_uint32, C.c_int32]
    lib.tpw_pl_row.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_pl_walk.argtypes = [C.c_void_p, C.c_int32]
    lib.tpw_pl_patrol.argtypes = [C.c_int32, C.c_int32, C.c_uint32, C.c_uint32]
    lib.tpw_pl_trace_n.restype = C.c_uint32
    lib.tpw_pl_trace_tags.restype = C.c_void_p
    lib.tpw_pl_trace_vals.restype = C.c_void_p
    lib.tpw_pl_layout.argtypes = [C.c_void_p]
    lib.tpw_pl_layout.restype = C.c_int32
    for name in ("cell_walk", "cell_littered", "cell_developed"):
        getattr(lib, "tpw_pl_" + name).argtypes = [C.c_int32, C.c_uint32]
        getattr(lib, "tpw_pl_" + name).restype = C.c_int32
    for name in ("sum_walk", "sum_littered", "sum_developed"):
        getattr(lib, "tpw_pl_" + name).argtypes = [C.c_uint32]
        getattr(lib, "tpw_pl_" + name).restype = C.c_int32
    lib.tpw_pl_analyser_cell.argtypes = [C.c_uint32]
    lib.tpw_pl_analyser_cell.restype = C.c_uint32
    lib.tpw_pl_nearby_type.argtypes = [C.c_int32, C.c_uint32, C.c_int32]
    lib.tpw_pl_nearby_type.restype = C.c_int32
    lib.tpw_pl_nearby_litter.argtypes = [C.c_uint32, C.c_uint32, C.c_int32]
    lib.tpw_pl_nearby_litter.restype = C.c_uint32
    lib.tpw_pl_mark.argtypes = [C.c_int32]
    lib.tpw_pl_pct_patrolled.argtypes = [C.c_int32, C.c_int32]
    for name in ("pct_littered", "pct_patrolled", "pct_cameras", "objects"):
        getattr(lib, "tpw_pl_" + name).restype = C.c_int32
    out = (C.c_int64 * 32)()
    n = lib.tpw_pl_layout(out)
    got = [out[i] for i in range(n)]
    assert got == HOST_LAYOUT, [hex(v) for v in got]


def ppc_trace(e):
    n = e.u32(RB)
    raw = e.read(RB + TRACE, 8 * min(n, CAP))
    a = array.array("I")
    a.frombytes(raw)
    a.byteswap()
    return n, a[0::2].tobytes(), a[1::2].tobytes()


def host_trace(lib):
    n = lib.tpw_pl_trace_n()
    k = min(n, CAP)
    return n, C.string_at(lib.tpw_pl_trace_tags(), 4 * k), C.string_at(lib.tpw_pl_trace_vals(), 4 * k)


def hword(v):
    return ((v & 0xFFFF) << 16)


def make_test(title, addr, spec, ppc_args, host_call, rnd, edges, kind="s32"):
    """spec: slot (the map slot written), writes (maps change), analyser/things (blocks written), obs_analyser; kind: s32, cell (offset from the analyser),
    half (the result halfword at RB + 0x80), none."""
    def ppc(e, c):
        side = PpcSide(e)
        e.write(RB, struct.pack(">II", 0, c["truthy"] & MASK))
        build(side, c, spec)
        for row in range(16):
            s = row_slot(c, row)
            e.write(TABLE + 0x14 * row, struct.pack(">I", RB + THINGS + 0x100 * (s - 1) if s else 0))
        if kind == "half":
            e.write(RB + 0x80, b"\xa5\xa5")
        S = stubs_for(e, c)
        saved = S.activate(e)
        try:
            r3, _, _ = T.call_ppc(e, addr, ppc_args(c))
        finally:
            CL.Stubs.deactivate(e, saved)
        if kind == "cell":
            ret = (r3 - (RB + ANAL)) & MASK
        elif kind == "half":
            ret = struct.unpack(">H", e.read(RB + 0x80, 2))[0]
        elif kind == "none":
            ret = 0
        else:
            ret = s32(r3)
        n, tags, vals = ppc_trace(e)
        return observe(side, c, spec, ret, n, tags, vals)

    def host(lib, c):
        side = HostSide(lib)
        lib.tpw_pl_begin(T.s32(c["truthy"]), c["seed"] & MASK, T.s32(c.get("count", 0)))
        build(side, c, spec)
        for row in range(16):
            lib.tpw_pl_row(row, row_slot(c, row))
        walk = c.get("walk", [])
        arr = (C.c_uint16 * 64)(*walk)
        lib.tpw_pl_walk(arr, len(walk))
        for b, (has, start, end) in c.get("patrol", {}).items():
            lib.tpw_pl_patrol(b, T.s32(has), start & 0xFFFF, end & 0xFFFF)
        ret = host_call(lib, c)
        if kind == "none":
            ret = 0
        n, tags, vals = host_trace(lib)
        return observe(side, c, spec, ret, n, tags, vals)

    return FnTest(title, ppc, host, edge=edges, rnd=rnd)


# ------------------------------------------------------------------ case generators


def spec(slot=None, writes=False, analyser=False, things=False, obs_analyser=False):
    return dict(slot=slot, writes=writes, analyser=analyser, things=things, obs_analyser=obs_analyser)


def common(r):
    return dict(V=r.randrange(NV), truthy=r.choice(TRUTHY), seed=r.getrandbits(32), count=0, g=r.getrandbits(16))


def dflt(**kw):
    c = dict(V=0, truthy=1, seed=0x12345678, count=0, g=0x5555)
    c.update(kw)
    return c


AGES = [0, 1, 5, 6, 100, 101, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF]
TURNS = [0x1000, 0x400, 0x7FFFFFFC, 0xFFFFFFFC, 0x20, 0]


def rect(r):
    k = r.random()
    if k < 0.08:
        return (r.randint(-1, 127), r.randint(0, 127), r.choice([0, -1, -5, 0x7FFFFFFF]), r.choice([0, 3, -2]))
    if k < 0.7:
        s = r.randint(1, 6)
    elif k < 0.95:
        s = r.randint(7, 30)
    else:
        s = r.randint(60, 129)
    sx, sy = r.randint(-1, 128 - min(s, 128)), r.randint(0, 128 - min(s, 128))
    szx, szy = min(r.randint(1, s), 128 - sx), min(r.randint(1, s), 128 - sy)
    return (sx, sy, szx, szy)


def cell_case(r):
    c = common(r)
    turn = r.choice(TURNS)
    idx = r.choice([0, 1, 5, 0x3FF, 0x3E8, r.randrange(0x400)])
    c.update(turn=turn, idx=idx, age=r.choice(AGES) if r.random() < 0.85 else r.getrandbits(32), g=0)
    c["cells"] = {idx: (rect(r), [valtriple(r, turn) for _ in range(3)])}
    return c


def cell_edges():
    out = []
    i = 0
    for worked in (0, 1):
        for delta in (0, 1, 5, 6, 100, -1):
            for age in (0, 1, 5, 100, 0xFFFFFFFF, 0x80000000):
                i += 1
                turn = 0x1000
                stamp = turn - 4 * delta + (i % 4)
                vals = [(stamp, 7 + i, worked)] * 3
                out.append(dflt(V=i % NV, truthy=TRUTHY[i % 7], turn=turn, idx=(i * 37) % 0x400, age=age, g=0,
                                cells={(i * 37) % 0x400: ((2 + i % 3, 3, 2 + i % 2, 3), vals)}))
    rects = [(-1, 0, 1, 1), (127, 127, 1, 1), (0, 0, 0, 5), (0, 0, 5, 0), (3, 3, -4, 4), (5, 5, 0x7FFFFFFF, 2), (5, 5, 2, 0x7FFFFFFF), (0, 0, 128, 3),
             (-1, 0, 128, 128), (0, 126, 4, 2), (1, 1, 2, 2), (-1, 0, 129 - 1, 1), (60, 60, 3, 3)]
    for j, rc in enumerate(rects):
        out.append(dflt(V=j % NV, truthy=TRUTHY[j % 7], turn=0x1000, idx=j, age=0xFFFFFFFF, g=0, cells={j: (rc, [(0x1000, 3, 0)] * 3)}))
        out.append(dflt(V=(j + 3) % NV, truthy=TRUTHY[(j + 2) % 7], turn=0x1000, idx=j, age=5, g=0, cells={j: (rc, [(0x1000, 3, 1)] * 3)}))
    return out


def sum_case(r):
    c = common(r)
    c.update(turn=r.choice(TURNS), cs=r.getrandbits(32), age=r.choice(AGES) if r.random() < 0.85 else r.getrandbits(32), g=0)
    return c


def sum_edges():
    return [dflt(V=i % NV, truthy=TRUTHY[i % 7], turn=TURNS[i % 6], cs=i * 77 + 1, age=AGES[i % len(AGES)], g=0) for i in range(len(AGES) * 2)]


IDS = [0, 1, 2, 0x7F, 0x80, 0x81, 0xFF, 0x100, 0x1000, 0x2000, 0x3FFF, 0x4000, 0x4001, 0x8000, 0xFFFE, 0xFFFF]


def getcell_case(r):
    c = common(r)
    c.update(squares=r.choice([1, 4, -1, 7, 2, 3, 0x10, -5, 0x7F, 0x80, 0x7FFFFFFF, INT_MIN, 0x20, 5]), gridx=r.choice([0x20, 1, -3, 0, 0x7FFFFFFF, INT_MIN, 100, 0x1000, 7]),
             id=r.choice(IDS) if r.random() < 0.5 else r.getrandbits(16))
    return c


def getcell_edges():
    return [dflt(squares=sq, gridx=gx, id=i) for sq in (1, 4, -1, 7, 0x80, INT_MIN) for gx in (0x20, 0, -3, INT_MIN) for i in IDS]


def neartype_case(r):
    c = common(r)
    c.update(model=r.choice([0, 1, 4, 8, 0x104, -1, INT_MIN, r.getrandbits(32)]), id=r.choice(IDS) if r.random() < 0.5 else r.getrandbits(16),
             a=r.choice([0, 1, -1, INT_MAX, INT_MIN, r.getrandbits(32)]), count=r.choice([0, 1, -1, 5, INT_MIN, r.getrandbits(32)]))
    return c


def neartype_edges():
    return [dflt(model=m, id=i, a=a, count=n) for m in (0, 4, -1) for i in (0, 1, 0x4000, 0xFFFF) for a in (0, 1, -1) for n in (0, 3, -2)]


RADII = [-1, 0, 1, 2, 3, 4, 5, 6, 7, 100, 1000, INT_MAX, INT_MIN, INT_MIN + 1, -5]


def nearlit_case(r):
    c = common(r)
    big = r.random() < 0.1
    c.update(who=r.choice([WHO, WHO, WHO, 0, 1, 0x100, 0xFFFF, 0x456, 0x2301, r.getrandbits(16)]), g2=r.getrandbits(16),
             id=r.randint(1, 0x4000) if r.random() < 0.85 else r.choice(IDS),
             radius=r.choice([6, 7, 100, 1000]) if big else r.choice(RADII[:7] + [r.randint(-3, 8)]))
    return c


def nearlit_edges():
    out = []
    for i, rad in enumerate(RADII):
        for j, id in enumerate((1, 0x4000, 0x40 + 0x80 * 0x40, 0x7F, 0x80, 0, 0x4001, 0xFFFF)):
            out.append(dflt(V=(i + j) % NV, truthy=TRUTHY[(i + j) % 7], who=WHO if (i + j) % 3 else 0, g2=0x1234, id=id, radius=rad))
    return out


def fullmap_case(r):
    return common(r)


def fullmap_edges():
    return [dflt(V=v, truthy=TRUTHY[v % 7], seed=0x777 + v) for v in range(NV)]


def staff_case(r):
    c = common(r)
    c["V"] = r.randrange(NV)
    c["walk"] = [r.randint(1, 8) for _ in range(r.choice([0, 2, 3, 5, 8, 8, 8]))]
    c["rows"] = {row: r.randint(1, 8) for row in range(1, 9)}
    for row in (0, 9, 10, 15):
        if r.random() < 0.3:
            c["rows"][row] = r.randint(1, 8)
    model = r.choice([4, 5, 6, 7, 8] * 4 + [0, 3, 9, 0x104, -1, 0x405])
    c["kinds"] = [(model & 0xFF) if r.random() < 0.6 else r.choice([4, 5, 6, 7, 8, 0, 1, 3, 9, 255]) for _ in range(8)]
    c["patrol"] = {}
    for b in range(8):
        if r.random() < 0.6:   # a proper rectangle: the start corner above and left of the end corner
            x1, y1 = r.randint(0, 127), r.randint(0, 127)
            x2, y2 = r.randint(x1, min(127, x1 + r.choice([3, 20, 127]))), r.randint(y1, min(127, y1 + r.choice([3, 20, 127])))
            start, end = x1 + (y1 << 7) + 1, x2 + (y2 << 7) + 1
        else:
            end = r.randint(1, 0x4000)
            start = r.choice([r.randint(1, 0x4000), r.randint(1, end), 0, 0xFFFF, 0x4001, 0x8000, end])
        c["patrol"][b] = (r.choice([0, 1, 1, 1, 1, 2, -1, 0x100]), start, end)
    c["model"] = model
    c["area"] = r.choice([0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, -1, 4, 5, INT_MIN, INT_MAX])
    return c


def staff_edges():
    out = []
    small = {b: (1, 1 + 0x80 * b + b, 1 + 0x80 * (b + 2) + b + 5) for b in range(8)}
    big = {b: (1, 1, 0x4000) for b in range(8)}
    odd = {0: (1, 0x4000, 1), 1: (1, 0xFFFF, 0x10), 2: (0, 1, 0x4000), 3: (-1, 5, 5), 4: (2, 0x4001, 0x100), 5: (0x100, 0x81, 0x81), 6: (1, 0, 0x40), 7: (1, 0x80, 0x7F)}
    for i, (patrol, kinds) in enumerate(((small, [4, 5, 6, 7, 8, 4, 5, 6]), (big, [4] * 8), (odd, [4, 4, 4, 4, 4, 4, 4, 4]), (small, [8, 7, 6, 5, 4, 3, 9, 255]),
                                         (odd, [4, 5, 6, 7, 8, 5, 5, 4]))):
        for model in (4, 5, 8, 0x104, 9):
            out.append(dflt(V=(i + model) % NV, truthy=TRUTHY[(i + model) % 7], walk=[1, 2, 3, 4, 5, 6, 7, 8] if i % 2 == 0 else [8, 1, 8],
                            rows={r_: ((r_ - 1) % 8) + 1 for r_ in range(1, 9)}, kinds=kinds, patrol=patrol, model=model, area=(i + model) % 4))
    out.append(dflt(walk=[], rows={}, kinds=[4] * 8, patrol=small, model=4, area=0))
    out.append(dflt(walk=[1], rows={0: 3, 1: 2}, kinds=[4] * 8, patrol=small, model=4, area=1))
    for area in (-1, 0, 1, 2, 3, 4, INT_MIN, INT_MAX):
        for v in (8, 9, 0, 3):
            out.append(dflt(V=v, walk=[1, 2], rows={1: 1, 2: 2}, kinds=[4, 4] + [0] * 6, patrol=big, model=4, area=area))
    return out


def effect_pool(r):
    return [[r.choice([-1, 0, 1, 0x7FFF, -0x8000, r.randint(-0x8000, 0x7FFF)]) for _ in range(5)] for _ in range(16)]


def cameras_case(r):
    c = common(r)
    c["effect"] = effect_pool(r)
    return c


def cameras_edges():
    r = random.Random(5)
    out = []
    for v in range(NV):
        out.append(dflt(V=v, truthy=TRUTHY[v % 7], effect=effect_pool(r)))
    out.append(dflt(V=0, effect=[[0] * 5] * 16))
    out.append(dflt(V=0, effect=[[1] * 5] * 16))
    out.append(dflt(V=1, effect=[[0, 0, 0, -1, 0]] * 16))
    return out


# ------------------------------------------------------------------ the fifteen tests


def cellfn(i, name):
    return make_test("CParkAnalyserCell::" + name, CELL_FN[i], spec(4, analyser=True),
                     lambda c: [RB + ANAL + 0x20 + 0x7C * c["idx"], c["age"]],
                     lambda lib, c: getattr(lib, ["tpw_pl_cell_walk", "tpw_pl_cell_littered", "tpw_pl_cell_developed"][i])(c["idx"], c["age"]),
                     cell_case, cell_edges)


def sumfn(i, name):
    return make_test("CParkAnalyser::" + name, SUM_FN[i], spec(4, analyser=True), lambda c: [RB + ANAL, c["age"]],
                     lambda lib, c: getattr(lib, ["tpw_pl_sum_walk", "tpw_pl_sum_littered", "tpw_pl_sum_developed"][i])(c["age"]), sum_case, sum_edges)


def mk_args(extra):
    return lambda c: [RB + ANAL] + extra(c)


TESTS = [
    make_test("CParkAnalyser::GetAnalyserCell", GETCELL, spec(analyser=True), lambda c: [RB + ANAL, hword(c["id"]) | c["g"]],
              lambda lib, c: lib.tpw_pl_analyser_cell(c["id"]), getcell_case, getcell_edges, kind="cell"),
    sumfn(0, "GetNumWalkableCells"), sumfn(1, "GetNumLitteredCells"), sumfn(2, "GetNumDevelopedCells"),
    cellfn(0, "GetNumWalkableCells"), cellfn(1, "GetNumLitteredCells"), cellfn(2, "GetNumDevelopedCells"),
    make_test("CParkAnalyser::IsNearbyThingOfType", NEARTYPE, spec(analyser=True), lambda c: [RB + ANAL, c["model"], hword(c["id"]) | c["g"], c["a"]],
              lambda lib, c: lib.tpw_pl_nearby_type(T.s32(c["model"]), c["id"], T.s32(c["a"])), neartype_case, neartype_edges),
    make_test("CParkAnalyser::GetNearbyLitteredCell", NEARLIT, spec(3, analyser=True, obs_analyser=True),
              lambda c: [RB + 0x80, RB + ANAL, hword(c["who"]) | c["g"], hword(c["id"]) | c.get("g2", 0), c["radius"]],
              lambda lib, c: lib.tpw_pl_nearby_litter(c["who"], c["id"], c["radius"]), nearlit_case, nearlit_edges, kind="half"),
    make_test("CParkAnalyser::GetPercentageLitteredCells", PCTLIT, spec(1), lambda c: [RB + ANAL], lambda lib, c: lib.tpw_pl_pct_littered(), fullmap_case, fullmap_edges),
    make_test("CParkAnalyser::MarkAreaPatrolledBy", MARK, spec(2, writes=True, things=True), lambda c: [RB + ANAL, c["model"]],
              lambda lib, c: lib.tpw_pl_mark(T.s32(c["model"])), staff_case, staff_edges, kind="none"),
    make_test("CParkAnalyser::PercentageOfAreaPatrolledBy", PATROLLED, spec(2, writes=True, things=True), lambda c: [RB + ANAL, c["model"], c["area"]],
              lambda lib, c: lib.tpw_pl_pct_patrolled(T.s32(c["model"]), T.s32(c["area"])), staff_case, staff_edges),
    make_test("CParkAnalyser::PercentageOfParkCoveredByCameras", CAMERAS, spec(2), lambda c: [RB + ANAL], lambda lib, c: lib.tpw_pl_pct_cameras(),
              cameras_case, cameras_edges),
    make_test("CParkAnalyser::ClearMapData", CLEAR, spec(2, writes=True), lambda c: [RB + ANAL], lambda lib, c: lib.tpw_pl_clear(), fullmap_case, fullmap_edges,
              kind="none"),
    make_test("CParkAnalyser::NumberOfCellsCoveredByObjects", OBJECTS, spec(2), lambda c: [RB + ANAL], lambda lib, c: lib.tpw_pl_objects(), fullmap_case,
              fullmap_edges),
]
