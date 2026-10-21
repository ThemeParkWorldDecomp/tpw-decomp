"""Differential tests for unit w197, the ride's queue (src/game/Object.cpp). See difftest.py and tpwtest.py.

The fifteen CObject members AddPersonToQueue, RemovePersonFromQueue, GetNumPeopleInQueue, GetPositionInQueue, GetFrontOfPhysicalQueue,
GetBackOfPhysicalQueue, GetBackOfVirtualQueue, GetBackOfQueue, RecalculateBackOfQueue, IsBOQConnected, IsExitCellConnected, GetQCellBehind and the three
Convert*QPositionToMapPos. Both sides work in ONE 6.25 MB block (the PowerPC side maps it at RB, the host library's gRegion is the same layout): the call
trace at 0x100 (tag, value pairs, pointers as offsets from the block), the ride object (0x448 bytes), 64 person blocks of 0x240 bytes, the out area, a
script block and three world-sized slots (WG: gGameWorld, WW: gWorld and gWorldMap, WM: what CWorldSavvy::GetWorld and GetMap answer), each map at slot +
0x2d8 (cells of 0x44 bytes, ids from 1, 128 wide; the first 48 rows are written). The three maps carry the same queue but differ in the places only one
function reads (the entry cell's neighbour byte in WG, a third of the cells' bytes in WW), so a function that reads the wrong map answers differently.
Every callee outside the unit is a fake that appends the same (tag, value) to the trace on both sides (the assert, CTRACE with its arguments, the
script variables, the world accessors, the random generator, the ride predicates, the wall test, CPerson::InQueueingState and EvaluateQueueEdit, the three
CMapCell predicates, answered from bits 1 to 3 of the cell's halfword at +0x42). The cell code (CCellBase::GetDirection and GetNeighbours,
CMapCell::GetNeighbouringCell in both forms, OPPOSITE, CMap::MakeNewTimeStamp and SetTimeStamp, the compass offsets) and CThing::GetThingId run as the
original. A case compares the answer (and the hidden halfword), the whole trace, the ride object (the named fields as numbers, the rest as bytes), the
people's link halfwords and blocks, the out area, the time stamp tables of all three maps and the world's stamp counter.
Not tested: a thing row without a thing in a queue walk (GetPositionInQueue then loops for ever, as the original does), a rotation other than 0, 90, 180 and
270 in IsBOQConnected (the original leaves a register unset), a cell with no neighbour at the map's border in IsBOQConnected, IsExitCellConnected and the
Convert functions (the original dereferences the null), track cells with the cell system's flag set (gLLCC4818 stays 0), a map larger than 48 rows.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ObjectQueue"

MASK = 0xFFFFFFFF
RB = 0x70000000
REGION = 0x640000
TRACE, CAP = 0x100, 0x10000
OBJ, PERS, OUT, SCRIPT = 0x90000, 0x91000, 0x9A000, 0x9B000
WG, WW, WM = 0xA0000, 0x280000, 0x460000
RES = OUT + 0x20
OBJ_SIZE, PSIZE, NSLOT, NROWS = 0x448, 0x240, 64, 256
MAP_AT, ZONE_AT, TURN_AT = 0x2D8, 0x294, 0x1DA70C
STAMP_AT, NSTAMP = 0x2D8 + 0x1B0000, 240
CELL, NIDS = 0x44, 128 * 48
TABLE, G_MAP, G_WORLD, G_GAMEWORLD = 0x1034CBFC, 0x1037EC08, 0x1037EC0C, 0x1037EBFC
G_STAMP, G_EMPTY, G_LLCC = 0x1034C21C, 0x102A0A9E, 0x102E4818
DELTAS = {0x1034C2AC: (0, -1), 0x1034C298: (1, -1), 0x1034C284: (1, 0), 0x1034C270: (1, 1), 0x1034C25C: (0, 1), 0x1034C248: (-1, 1),
          0x1034C234: (-1, 0), 0x1034C220: (-1, -1)}

ASSERT, CTRACE, GETSCRIPT, GETVAR, SETVAR = 0x1000DDE8, 0x1006FCBC, 0x100D668C, 0x100D6C58, 0x100D6734
GETRAND, GETWORLD, GETMAP, WALL = 0x1013B1E4, 0x1014205C, 0x10142098, 0x10101CA4
QSTATE, EVALUATE, ISCLOSED, ISOPENABLE, ISTRACK, REOPEN = 0x10120B2C, 0x1011F2F8, 0x1010CAE8, 0x1010AAAC, 0x10110D50, 0x1010ACDC
PRED = {2: 0x1009C44C, 3: 0x1009C490, 4: 0x1009C4DC}
ADD, REMOVE, NUM, POSITION = 0x10108A28, 0x10108C20, 0x10108EF4, 0x10108F5C
FRONT, BACKP, BACK, BACKV, RECALC = 0x10109144, 0x10109374, 0x101092F4, 0x101094CC, 0x1010952C
BOQ, EXIT, BEHIND, CONVERT, CONVP, CONVV = 0x10109764, 0x10109960, 0x10109AF0, 0x10109DA8, 0x10109EB4, 0x1010A2C0
OWN = {ADD, REMOVE, NUM, POSITION, FRONT, BACKP, BACK, BACKV, RECALC, BOQ, EXIT, BEHIND, CONVERT, CONVP, CONVV}

# sizeof CObject, offsets of mObjectType, mRotation, mF20, mF28, mFlags, mF32, mF34, mF36, mQueuedPerson, mF3C, mF5A, mF64, mPersonLoaded, CWorld::mTurn,
# sizeof CMapCell, offsets of mF42, mCellId, mNeighbours, mDirection, mKind
HOST_LAYOUT = [0x448, 0x0E, 0x10, 0x20, 0x28, 0x2E, 0x32, 0x34, 0x36, 0x38, 0x3C, 0x5A, 0x64, 0x68, 0x1DA70C, 0x44, 0x42, 0, 0x0C, 0x0D, 2]

OBJ_FIELDS = {"tid": (0x00, "H"), "type": (0x0E, "H"), "rotation": (0x10, "i"), "f20": (0x20, "I"), "f28": (0x28, "i"), "flags": (0x2E, "H"), "f32": (0x32, "H"),
              "f34": (0x34, "H"), "f36": (0x36, "H"), "queued": (0x38, "H"), "f3c": (0x3C, "I"), "f5a": (0x5A, "H"), "f64": (0x64, "I"),
              "loaded": (0x68, "H")}
SIZES = {"H": 2, "I": 4, "i": 4}
TRUTHY = [1, 1, 1, 2, 0x100, -1, -0x80000000]
STATES = [1, 1, 1, 2, 0x100, 0x10000, 0xFFFFFFFF, 0x80000000]

DIRS = (1, 4, 0x10, 0x40)
PRI = (1, 0x10, 0x40, 4)
DX = {1: 0, 4: 1, 0x10: 0, 0x40: -1}
DY = {1: -1, 4: 0, 0x10: 1, 0x40: 0}
SIDE_DELTA = {1: -128, 2: -127, 4: 1, 8: 129, 0x10: 128, 0x20: 127, 0x40: -1, 0x80: -129}
OPP = {1: 0x10, 2: 0x20, 4: 0x40, 8: 0x80, 0x10: 1, 0x20: 2, 0x40: 4, 0x80: 8}
PATH, Q, ENT = 2, 4, 8
ANYDIR = (1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80)
LONG_ROW = 100


def put(buf, e, fmt, off, v):
    if fmt == "i":
        v = T.s32(v)
    else:
        v &= (1 << (8 * SIZES[fmt])) - 1
    struct.pack_into(e + fmt, buf, off, v)


def get(buf, e, fmt, off):
    return struct.unpack_from(e + fmt, buf, off)[0]


def cid(x, y):
    return 1 + x + 128 * y


def cxy(i):
    return (i - 1) & 0x7F, (i - 1) >> 7


# ------------------------------------------------------------------ the two sides


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
        self.lib, self.base = lib, lib.tpw_oqu_base()

    def write(self, off, data):
        data = bytes(data)
        C.memmove(C.c_void_p(self.base + off), data, len(data))

    def read(self, off, n):
        return C.string_at(self.base + off, n)


_TEMPLATE = {}


def template(big):
    if big not in _TEMPLATE:
        e = ">" if big else "<"
        buf = bytearray(NIDS * CELL)
        for k in range(NIDS):
            struct.pack_into(e + "H", buf, k * CELL, k)
        _TEMPLATE[big] = bytes(buf)
    return _TEMPLATE[big]


def zone_bytes(cells, big):
    e = ">" if big else "<"
    buf = bytearray(template(big))
    for k, (nb, direction, f42) in cells.items():
        buf[k * CELL + 0x0C] = nb & 0xFF
        buf[k * CELL + 0x0D] = direction & 0xFF
        struct.pack_into(e + "H", buf, k * CELL + 0x42, f42 & 0xFFFF)
    return bytes(buf)


def obj_bytes(c, big):
    e = ">" if big else "<"
    buf = bytearray(c["objfill"])
    for name, (off, fmt) in OBJ_FIELDS.items():
        put(buf, e, fmt, off, c["obj"][name])
    return bytes(buf)


def person_bytes(p, big):
    e = ">" if big else "<"
    buf = bytearray(p["fill"])
    put(buf, e, "H", 0x00, p["tid"])
    put(buf, e, "I", 0x220, p["state"])
    put(buf, e, "H", 0x228, p["behind"])
    put(buf, e, "H", 0x22A, p["ahead"])
    return bytes(buf)


def build(side, c):
    e = ">" if side.big else "<"
    side.write(OBJ, obj_bytes(c, side.big))
    for slot, p in c["people"].items():
        side.write(PERS + PSIZE * slot, person_bytes(p, side.big))
    out = bytearray(c["outfill"])
    put(out, e, "H", 0, c["outhw"])
    side.write(OUT, out)
    for base, key in ((WG, "g"), (WW, "w"), (WM, "m")):
        side.write(base + ZONE_AT, zone_bytes(c["cells"][key], side.big))
        side.write(base + STAMP_AT, struct.pack(e + "%dI" % NSTAMP, *c["stamps"]))
    side.write(WM + TURN_AT, struct.pack(e + "I", c["turn"] & MASK))


def observe(side, c, ret, res, counter, n, tags, vals):
    e = ">" if side.big else "<"
    raw = bytearray(side.read(OBJ, OBJ_SIZE))
    obj = {}
    for name, (off, fmt) in OBJ_FIELDS.items():
        obj[name] = get(raw, e, fmt, off)
        raw[off:off + SIZES[fmt]] = bytes(SIZES[fmt])
    people = []
    for slot in sorted(c["people"]):
        blk = bytearray(side.read(PERS + PSIZE * slot, PSIZE))
        people.append([get(blk, e, "H", 0), get(blk, e, "I", 0x220), get(blk, e, "H", 0x228), get(blk, e, "H", 0x22A)])
        blk[0:2] = bytes(2)
        blk[0x220:0x22C] = bytes(12)
        people.append(bytes(blk))
    out = bytearray(side.read(OUT, 8))
    cell = get(out, e, "H", 0)
    out[0:2] = bytes(2)
    stamps = []
    for base in (WG, WW, WM):
        stamps.extend(struct.unpack(e + "%dI" % NSTAMP, side.read(base + STAMP_AT, 4 * NSTAMP)))
    return dict(ret=ret, res=res, counter=counter, n=n, tags=tags, vals=vals, obj=obj, rest=bytes(raw), people=people, cell=cell, out=bytes(out),
                stamps=stamps)


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
    state = {"rand": 0}
    rands, walls, fill = c["rands"], c["walls"], c["wallfill"] & MASK

    def assertion(emu):
        tr(emu, 20, emu.r(3))
        tr(emu, 21, text_hash(emu.cstr(emu.r(4))))

    def ctrace(emu):
        tr(emu, 11, emu.r(3))
        fmt = emu.cstr(emu.r(4))
        tr(emu, 12, text_hash(fmt))
        reg, i = 5, 0
        while i < len(fmt):
            if fmt[i] != "%":
                i += 1
                continue
            i += 1
            if i >= len(fmt):
                break
            if fmt[i] == "%":
                i += 1
                continue
            v = emu.r(reg)
            reg += 1
            tr(emu, 13, text_hash(emu.cstr(v)) if fmt[i] == "s" else v)
            i += 1

    def getscript(emu):
        tr(emu, 30, emu.r(3))
        emu.set_r(3, RB + SCRIPT)

    def getvar(emu):
        tr(emu, 31, rel(emu.r(3)))
        tr(emu, 32, emu.r(4))
        emu.set_r(3, c["var0"] & MASK)

    def setvar(emu):
        tr(emu, 33, rel(emu.r(3)))
        tr(emu, 34, emu.r(4))
        tr(emu, 35, emu.r(5))

    def queueing(emu):
        tr(emu, 40, rel(emu.r(3)))
        emu.set_r(3, emu.u32(emu.r(3) + 0x220))

    def evaluate(emu):
        tr(emu, 41, rel(emu.r(3)))

    def isclosed(emu):
        tr(emu, 42, rel(emu.r(3)))
        emu.set_r(3, 1 if emu.u32(emu.r(3) + 0x64) == 0 else 0)

    def isopenable(emu):
        tr(emu, 43, rel(emu.r(3)))
        emu.set_r(3, c["openable"] & MASK)

    def istrack(emu):
        tr(emu, 44, rel(emu.r(3)))
        emu.set_r(3, c["track"] & MASK)

    def reopen(emu):
        tr(emu, 45, rel(emu.r(3)))

    def getworld(emu):
        tr(emu, 50, rel(emu.r(3)))
        emu.set_r(3, RB + WM)

    def getmap(emu):
        tr(emu, 51, rel(emu.r(3)))
        emu.set_r(3, RB + WM + MAP_AT)

    def getrand(emu):
        tr(emu, 52, rel(emu.r(3)))
        i = state["rand"]
        state["rand"] += 1
        emu.set_r(3, rands[min(i, len(rands) - 1)] & MASK)

    def wall(emu):
        x, z, d, mode = emu.r(3) & 0xFF, emu.r(4) & 0xFF, emu.r(5) & 0xFF, emu.r(6)
        tr(emu, 53, x)
        tr(emu, 54, z)
        tr(emu, 55, d)
        tr(emu, 56, mode)
        mask = fill
        for wx, wz, wm in walls:
            if wx == x and wz == z:
                mask = wm & MASK
        emu.set_r(3, ((mask >> d) & 1) if d < 8 else 0)

    def predicate(tag):
        def fn(emu):
            this = emu.r(3)
            tr(emu, tag, rel(this))
            half = struct.unpack(">H", emu.read(this + 0x42, 2))[0]
            emu.set_r(3, (c["truthy"] & MASK) if half & (1 << (tag - 1)) else 0)
        return fn

    S = CL.Stubs()
    S.real = OWN
    for addr, fn in ((ASSERT, assertion), (CTRACE, ctrace), (GETSCRIPT, getscript), (GETVAR, getvar), (SETVAR, setvar), (QSTATE, queueing),
                     (EVALUATE, evaluate), (ISCLOSED, isclosed), (ISOPENABLE, isopenable), (ISTRACK, istrack), (REOPEN, reopen),
                     (GETWORLD, getworld), (GETMAP, getmap), (GETRAND, getrand), (WALL, wall)):
        S.internal(addr, fn)
    for tag, addr in PRED.items():
        S.internal(addr, predicate(tag))
    return S


def setup(runner):
    emu = runner.emu
    T.install_std_stubs(emu)
    emu.uc.mem_map(RB, REGION)
    emu.write(G_MAP, struct.pack(">I", RB + WW + MAP_AT))
    emu.write(G_WORLD, struct.pack(">I", RB + WW))
    emu.write(G_GAMEWORLD, struct.pack(">I", RB + WG))
    emu.write(G_EMPTY, bytes(2))
    emu.write(G_LLCC, bytes(1))
    for addr, (dx, dy) in DELTAS.items():
        emu.write(addr, struct.pack(">ii", dx, dy) + bytes(4))
    bind(runner.lib)
    runner.lib.tpw_oqu_init()


def bind(lib):
    lib.tpw_oqu_base.restype = C.c_uint64
    lib.tpw_oqu_begin.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_uint32, C.c_uint32]
    lib.tpw_oqu_rows.argtypes = [C.c_void_p, C.c_int32]
    lib.tpw_oqu_rands.argtypes = [C.c_void_p, C.c_int32]
    lib.tpw_oqu_wall.argtypes = [C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_oqu_stamp.restype = C.c_uint32
    lib.tpw_oqu_trace_n.restype = C.c_uint32
    lib.tpw_oqu_trace_tags.restype = C.c_void_p
    lib.tpw_oqu_trace_vals.restype = C.c_void_p
    lib.tpw_oqu_layout.argtypes = [C.c_void_p]
    lib.tpw_oqu_layout.restype = C.c_int32
    for name in ("add", "remove", "position", "behind", "convert", "convert_physical", "convert_virtual"):
        getattr(lib, "tpw_oqu_" + name).argtypes = [C.c_uint32]
    for name in ("num", "position", "boq", "exit", "convert", "convert_physical", "convert_virtual"):
        getattr(lib, "tpw_oqu_" + name).restype = C.c_int32
    for name in ("back", "front", "back_physical", "back_virtual", "behind"):
        getattr(lib, "tpw_oqu_" + name).restype = C.c_uint32
    out = (C.c_int64 * 32)()
    n = lib.tpw_oqu_layout(out)
    got = [out[i] for i in range(n)]
    assert got == HOST_LAYOUT, [hex(v) for v in got]


def ppc_trace(e):
    import array
    n = e.u32(RB)
    raw = e.read(RB + TRACE, 8 * min(n, CAP))
    a = array.array("I")
    a.frombytes(raw)
    a.byteswap()
    return n, a[0::2].tobytes(), a[1::2].tobytes()


def host_trace(lib):
    n = lib.tpw_oqu_trace_n()
    k = min(n, CAP)
    return n, C.string_at(lib.tpw_oqu_trace_tags(), 4 * k), C.string_at(lib.tpw_oqu_trace_vals(), 4 * k)


def hword(v):
    return (v & 0xFFFF) << 16


def make_test(title, addr, ppc_args, host_call, rnd, edges, kind="s32"):
    """kind: s32 (the answer is r3), tag (the answer is the halfword behind the hidden pointer r3 = RB + RES), none."""
    def ppc(e, c):
        side = PpcSide(e)
        e.write(RB, struct.pack(">I", 0))
        e.write(G_STAMP, struct.pack(">I", c["stamp"] & MASK))
        build(side, c)
        rows = bytearray(NROWS * 0x14)
        for row, slot in enumerate(c["rows"]):
            struct.pack_into(">I", rows, 0x14 * row, RB + PERS + PSIZE * slot if slot >= 0 else 0)
        e.write(TABLE, bytes(rows))
        if kind == "tag":
            e.write(RB + RES, b"\xa5\xa5")
        S = stubs_for(e, c)
        saved = S.activate(e)
        try:
            r3, _, _ = T.call_ppc(e, addr, ppc_args(c))
        finally:
            CL.Stubs.deactivate(e, saved)
        res = struct.unpack(">H", e.read(RB + RES, 2))[0] if kind == "tag" else 0
        ret = T.s32(r3) if kind == "s32" else 0
        n, tags, vals = ppc_trace(e)
        return observe(side, c, ret, res, e.u32(G_STAMP), n, tags, vals)

    def host(lib, c):
        side = HostSide(lib)
        lib.tpw_oqu_begin(T.s32(c["truthy"]), T.s32(c["var0"]), T.s32(c["openable"]), T.s32(c["track"]), c["stamp"] & MASK, c["wallfill"] & MASK)
        build(side, c)
        rows = (C.c_int32 * NROWS)(*c["rows"])
        lib.tpw_oqu_rows(rows, NROWS)
        rands = (C.c_uint32 * 16)(*[v & MASK for v in c["rands"]])
        lib.tpw_oqu_rands(rands, len(c["rands"]))
        for x, z, m in c["walls"]:
            lib.tpw_oqu_wall(x, z, m & MASK)
        ret = host_call(lib, c)
        res = (ret & 0xFFFF) if kind == "tag" else 0
        ret = T.s32(ret) if kind == "s32" else 0
        n, tags, vals = host_trace(lib)
        return observe(side, c, ret, res, lib.tpw_oqu_stamp(), n, tags, vals)

    return FnTest(title, ppc, host, edge=edges, rnd=rnd)


# ------------------------------------------------------------------ case generators: people


def new_case(r):
    c = dict(
        truthy=r.choice(TRUTHY), var0=0, openable=r.choice((0, 1, 1, 2)), track=r.choice((0, 1, 7)), stamp=r.getrandbits(32), wallfill=0, walls=[],
        rands=[r.getrandbits(32)], g=r.getrandbits(16), turn=r.getrandbits(32), arg=0, pos=0,
        objfill=r.randbytes(OBJ_SIZE), outfill=r.randbytes(8), outhw=r.getrandbits(16), stamps=[r.getrandbits(32) for _ in range(NSTAMP)],
        obj=dict(tid=r.getrandbits(16), type=r.getrandbits(16), rotation=r.choice((0, 90, 180, 270)), f20=r.getrandbits(32), f28=r.choice((0, 1, -1, r.getrandbits(32))),
                 flags=r.getrandbits(16) & ~8, f32=0, f34=0, f36=0, queued=0, f3c=r.getrandbits(32), f5a=r.getrandbits(16), f64=r.choice((0, 1, 2)),
                 loaded=0),
        people={}, rows=[-1] * NROWS, cells=dict(g={}, m={}, w={}), queue=[], extra=[], entry=0, path=[])
    return c


def make_people(r, c, n, extras=3, cut=None, link_junk=0.3):
    count = n + extras
    ids = r.sample(range(1, 250), count)
    slots = r.sample(range(NSLOT), count)
    c["queue"], c["extra"], c["slot_of"] = ids[:n], ids[n:], dict(zip(ids, slots))
    for k, (pid, slot) in enumerate(zip(ids, slots)):
        if k < n:
            ahead = ids[k - 1] if k > 0 else 0
            behind = ids[k + 1] if k + 1 < n else 0
        else:
            ahead = r.choice(ids) if r.random() < link_junk else 0
            behind = r.choice(ids) if r.random() < link_junk else 0
        state = r.choice(STATES)
        if cut is not None and k == cut:
            state = 0
        c["people"][slot] = dict(fill=r.randbytes(PSIZE), state=state, behind=behind, ahead=ahead,
                                 tid=pid if r.random() < 0.9 else r.getrandbits(16))
        c["rows"][pid] = slot
    c["obj"]["queued"] = ids[0] if n else 0
    return c


def pick_n(r):
    return r.choice((0, 1, 2, 5, 30, 1, 2, 3, 4, 7, 12, 20))


def person_case(r, n=None, extras=3, cut=None):
    c = new_case(r)
    n = pick_n(r) if n is None else n
    make_people(r, c, n, extras, cut)
    return c


def add_case(r, n=None, member=None):
    c = person_case(r, n)
    q, ex = c["queue"], c["extra"]
    if member is None:
        member = r.random() < 0.06
    c["arg"] = r.choice(q) if (member and q) else r.choice(ex)
    if r.random() < 0.05:
        c["rows"][0] = c["slot_of"][c["arg"]]
    return c


def add_edges():
    out = []
    for i, (n, member) in enumerate([(0, False), (1, False), (2, False), (5, False), (30, False), (1, True), (2, True), (5, True), (30, True), (3, False),
                                     (4, False), (0, False)]):
        c = add_case(random.Random(100 + i), n, member)
        if i == 11:
            c["rows"][0] = c["slot_of"][c["arg"]]
        out.append(c)
    return out


def remove_case(r, n=None, where=None):
    c = person_case(r, max(1, pick_n(r)) if n is None else n)
    q, ex = c["queue"], c["extra"]
    if where is None:
        where = r.choice(("head", "tail", "mid", "mid", "extra", "head", "tail"))
    if where == "extra" or not q:
        p = r.choice(ex)
    elif where == "head":
        p = q[0]
    elif where == "tail":
        p = q[-1]
    else:
        p = q[len(q) // 2] if len(q) > 2 else q[0]
    c["arg"] = p
    other = r.choice(q + ex)
    c["var0"] = r.choice((p, p, p, other, 0, -1, p ^ 0x10000, p + 0x100000000 - 0x100000000, other))
    if r.random() < 0.05:
        c["rows"][0] = c["slot_of"][p]
    return c


def remove_edges():
    out = []
    i = 0
    for n in (1, 2, 5, 30):
        for where in ("head", "mid", "tail", "extra"):
            out.append(remove_case(random.Random(200 + i), n, where))
            i += 1
    c = remove_case(random.Random(300), 5, "mid")
    c["var0"] = 0
    out.append(c)
    return out


def position_case(r, n=None, target=None):
    c = person_case(r, n)
    q, ex = c["queue"], c["extra"]
    if q and r.random() < 0.35:
        cut = r.randrange(len(q))
        c["people"][c["slot_of"][q[cut]]]["state"] = 0
    if target is None:
        target = r.choice(([r.choice(q)] * 3 if q else []) + ex + [0, 250, 255])
    c["arg"] = target
    return c


def position_edges():
    out = []
    i = 0
    for n in (0, 1, 2, 5, 30):
        c = position_case(random.Random(400 + i), n, 0)
        out.append(c)
        i += 1
        c = position_case(random.Random(400 + i), n, None)
        c["arg"] = c["extra"][0]
        out.append(c)
        i += 1
        for k in sorted({0, n // 2, n - 1}):
            if 0 <= k < n:
                c = position_case(random.Random(400 + i), n, 0)
                for slot in c["people"]:
                    pass
                c["arg"] = c["queue"][k]
                out.append(c)
                i += 1
    c = position_case(random.Random(499), 5, 0)
    c["people"][c["slot_of"][c["queue"][1]]]["state"] = 0
    c["arg"] = c["queue"][3]
    out.append(c)
    return out


def num_case(r, n=None):
    c = position_case(r, n, 0)
    return c


def num_edges():
    out = []
    for i, n in enumerate((0, 1, 2, 5, 30)):
        out.append(num_case(random.Random(500 + i), n))
    c = num_case(random.Random(510), 5)
    for p in c["people"].values():
        p["state"] = 1
    c["people"][c["slot_of"][c["queue"][2]]]["state"] = 0
    out.append(c)
    c = num_case(random.Random(511), 30)
    for p in c["people"].values():
        p["state"] = 0x10000
    out.append(c)
    c = num_case(random.Random(512), 5)
    for p in c["people"].values():
        p["state"] = 0
    out.append(c)
    return out


# ------------------------------------------------------------------ case generators: the map


def free_nbrs(x, y, used):
    out = []
    for s in DIRS:
        nx, ny = x + DX[s], y + DY[s]
        if 3 <= nx <= 124 and 3 <= ny <= 44 and cid(nx, ny) not in used:
            out.append((s, cid(nx, ny)))
    return out


def side_of(a, b):
    return {-128: 1, 1: 4, 128: 0x10, -1: 0x40}[b - a]


def walk(r, entry, side, n, turn_p):
    ex, ey = cxy(entry)
    path = [cid(ex + DX[side], ey + DY[side])]
    used = {entry, path[0]}
    heading = side
    while len(path) < n:
        x, y = cxy(path[-1])
        opts = free_nbrs(x, y, used)
        if not opts:
            break
        straight = [o for o in opts if o[0] == heading]
        s, nxt = straight[0] if (straight and r.random() >= turn_p) else r.choice(opts)
        heading = s
        path.append(nxt)
        used.add(nxt)
    return path


def line_cells(r, cells, entry, path, ring=False, entrance_at=None, extras=True):
    for i, p in enumerate(path):
        prev = path[i - 1] if i > 0 else (path[-1] if ring else entry)
        nxt = path[i + 1] if i + 1 < len(path) else (path[0] if ring else None)
        d = side_of(p, prev)
        nb = d | (side_of(p, nxt) if nxt else 0)
        if extras:
            nb |= (r.getrandbits(4) << 1) & 0xAA
        f42 = Q | (PATH if r.random() < 0.5 else 0) | (ENT if i == entrance_at else 0)
        cells[p] = (nb, d, f42)


def decorate(r, cells, path, entry, decoys, junk):
    used = set(path) | {entry}
    for _ in range(decoys):
        if not path:
            break
        j = r.randrange(len(path))
        x, y = cxy(path[j])
        opts = free_nbrs(x, y, used)
        if not opts:
            continue
        s, d = r.choice(opts)
        used.add(d)
        flow = OPP[s]
        kind = r.randrange(4)
        if kind == 0:
            cells[d] = (r.getrandbits(8), flow, PATH if r.random() < 0.5 else 0)
        elif kind == 1:
            cells[d] = (r.getrandbits(8), flow, Q | ENT)
        elif kind == 2:
            cells[d] = (r.getrandbits(8), r.choice([v for v in ANYDIR if v != flow]), Q)
        else:
            cells[d] = (r.getrandbits(8), flow, Q)
    for _ in range(junk):
        i = cid(r.randrange(3, 125), r.randrange(3, 45))
        if i not in used:
            cells[i] = (r.getrandbits(8), r.choice(ANYDIR + (0, 3)), r.getrandbits(16))


def variants(r, base, entry, coherent):
    g, m, w = dict(base), dict(base), dict(base)
    if not coherent:
        nb, d, f = base.get(entry, (0, 0, 0))
        g[entry] = (nb ^ (r.getrandbits(8) | 1), d, f)
        for i, (nb, d, f) in base.items():
            if r.random() < 0.3:
                k = r.randrange(3)
                if k == 0:
                    nb ^= r.getrandbits(8)
                elif k == 1:
                    d = r.choice(ANYDIR)
                else:
                    f ^= r.getrandbits(3) << 1
                w[i] = (nb, d, f)
    return g, m, w


def add_queue(r, c, n=None, style=None, ring=0, snake=0, decoys=None, junk=None, coherent=None, entry=None, sides=None, entrance_at=None,
              cycle_ok=True):
    """The queue line of the case: the entry cell (mF32), the line behind it (the physical queue), decoys and junk cells, the three maps."""
    if n is None:
        n = r.choice((0, 1, 2, 3, 5, 8, 13, 21, 30, 40, r.randrange(41), r.randrange(1, 9)))
    if entry is None:
        entry = cid(r.randrange(8, 120), r.randrange(8, 40))
    if sides is None:
        sides = [s for s in DIRS if r.random() < 0.3] if n else [s for s in DIRS if r.random() < 0.15 and False]
        if (n or ring or snake) and not sides:
            sides = [r.choice(DIRS)]
    front_side = next((s for s in PRI if s in sides), 0)
    cells = {}
    path = []
    if snake:
        entry = cid(9, 4)
        front_side = 4
        sides = [4]
        x = 9
        for k in range(snake):
            row, col = divmod(k, LONG_ROW)
            x = 9 + 1 + col if row % 2 == 0 else 9 + LONG_ROW - col
            path.append(cid(x, 4 + row))
    elif ring:
        a, b = r.randrange(2, 6), r.randrange(2, 6)
        dx = -1 if front_side == 0x40 else 1
        dy = -1 if front_side == 1 else 1
        ex, ey = cxy(entry)
        fx, fy = ex + DX[front_side], ey + DY[front_side]
        perimeter = [(i, 0) for i in range(a)] + [(a - 1, j) for j in range(1, b)] + [(i, b - 1) for i in range(a - 2, -1, -1)] + \
                    [(0, j) for j in range(b - 2, 0, -1)]
        path = [cid(fx + ox * dx, fy + oy * dy) for ox, oy in perimeter]
    elif front_side:
        path = walk(r, entry, front_side, n, 0.08 if (style or r.choice(("straight", "winding"))) == "straight" else 0.5)
    line_cells(r, cells, entry, path, ring=bool(ring), entrance_at=entrance_at)
    nb_entry = sum(sides) | ((r.getrandbits(4) << 1) & 0xAA)
    entry_dir, entry_f = r.choice(ANYDIR), r.choice((0, 0, PATH, ENT, Q, Q | ENT))
    if not cycle_ok and front_side and entry_dir == front_side and (entry_f & Q) and not (entry_f & ENT):
        entry_dir = r.choice([d for d in ANYDIR if d != front_side])   # else entry and front cell are each other's cell behind, for ever
    cells[entry] = (nb_entry, entry_dir, entry_f)
    decorate(r, cells, path, entry, r.randrange(0, 6) if decoys is None else decoys, r.randrange(0, 30) if junk is None else junk)
    if coherent is None:
        coherent = r.random() < 0.5
    g, m, w = variants(r, cells, entry, coherent)
    c["cells"] = dict(g=g, m=m, w=w)
    c["entry"], c["path"] = entry, path
    c["obj"]["f32"] = entry
    c["obj"]["f34"] = r.choice((entry, path[-1] if path else entry, entry))
    return c


def cached_back(r, c, cached):
    if cached:
        c["obj"]["f36"] = c["path"][-1] if (c["path"] and r.random() < 0.5) else r.randrange(1, NIDS)
    else:
        c["obj"]["f36"] = 0


def back_case(r, flavour, **kw):
    c = new_case(r)
    add_queue(r, c, **kw)
    cached_back(r, c, r.random() < 0.2)
    if flavour == "generic":
        c["obj"]["flags"] = (c["obj"]["flags"] & ~8) | (8 if r.random() < 0.5 else 0)
    return c


def back_edges(flavour):
    out = []
    i = 0
    specs = [dict(n=0), dict(n=1, sides=[1]), dict(n=1, sides=[0x10]), dict(n=1, sides=[0x40]), dict(n=1, sides=[4]), dict(n=2, sides=[1, 4]),
             dict(n=5, style="straight"), dict(n=40, style="straight"), dict(n=40, style="winding"), dict(n=12, entrance_at=5), dict(n=12, entrance_at=0),
             dict(n=7, decoys=5), dict(ring=4), dict(ring=12), dict(snake=999), dict(snake=1000), dict(snake=1001), dict(snake=1002), dict(snake=30)]
    for sp in specs:
        c = back_case(random.Random(600 + i), flavour, coherent=True, **sp)
        c["obj"]["f36"] = 0
        out.append(c)
        i += 1
    c = back_case(random.Random(699), flavour, n=6, coherent=True)
    c["obj"]["f36"] = 0x1234
    out.append(c)
    c = back_case(random.Random(698), flavour, n=6, coherent=False)
    c["obj"]["f36"] = 0
    out.append(c)
    return out


def front_case(r, entry=None, nb=None):
    c = new_case(r)
    if entry is None:
        entry = r.choice((r.randrange(NIDS), r.randrange(1, NIDS), cid(r.randrange(128), r.randrange(48))))
    if nb is None:
        nb = r.choice((r.getrandbits(8), r.getrandbits(8) & 0x55, r.getrandbits(8) & 0x55, r.choice((1, 0x10, 0x40, 4))))
    cells = {entry: (nb, r.choice(ANYDIR), r.getrandbits(16))}
    for _ in range(r.randrange(8)):
        cells[r.randrange(NIDS)] = (r.getrandbits(8), r.choice(ANYDIR), r.getrandbits(16))
    cells[entry] = (nb, cells[entry][1], cells[entry][2])
    base = dict(cells)
    w = dict(cells)
    w[entry] = (nb ^ (r.getrandbits(8) | 1), 0, 0)
    g = dict(cells)
    g[entry] = (nb ^ (r.getrandbits(8) | 1), 0, 0)
    c["cells"] = dict(g=g, m=base, w=w)
    c["obj"]["f32"] = entry
    c["entry"] = entry
    return c


def front_edges():
    out = []
    i = 0
    for nb in range(16):
        bits = (1 if nb & 1 else 0) | (0x10 if nb & 2 else 0) | (0x40 if nb & 4 else 0) | (4 if nb & 8 else 0)
        out.append(front_case(random.Random(700 + i), cid(60, 20), bits | (0xAA if nb == 5 else 0)))
        i += 1
    for entry in (0, 1, 2, 128, 129, 130, NIDS - 1, NIDS - 128, 127, 6000):
        for bits in (1, 4, 0x40, 0x10):
            out.append(front_case(random.Random(700 + i), entry, bits))
            i += 1
    return out


def stamp_case(r):
    return r.choice((r.getrandbits(32), 0, 0xFFFFFFFF, 0xFFFFFFFE, 1))


def recalc_case(r, n=None, qlen=None, **kw):
    c = new_case(r)
    make_people(r, c, pick_n(r) if n is None else n, 2)
    add_queue(r, c, n=qlen, **kw)
    c["obj"]["flags"] = (c["obj"]["flags"] & ~8) | (8 if r.random() < 0.5 else 0)
    c["obj"]["rotation"] = r.choice((0, 90, 180, 270))
    c["obj"]["f64"] = r.choice((0, 0, 1, 3))
    c["obj"]["f28"] = r.choice((0, 0, 1, -1, 0x10000))
    c["obj"]["f36"] = r.choice((0, 0, 0x55, r.randrange(NIDS)))
    c["obj"]["f5a"] = r.choice((0, 0xFFFF, r.getrandbits(16)))
    q = c["queue"]
    c["obj"]["loaded"] = r.choice((0, q[r.randrange(len(q))] if q else 0, q[r.randrange(len(q))] if q else 250, 250, q[0] if q else 0))
    c["stamp"] = stamp_case(r)
    if r.random() < 0.5:
        virtual_setup(r, c)
    return c


def recalc_edges():
    out = []
    i = 0
    for n in (0, 1, 2, 5, 30):
        for loaded in ("none", "head", "tail", "absent"):
            c = recalc_case(random.Random(800 + i), n, qlen=r_n(i))
            q = c["queue"]
            c["obj"]["loaded"] = {"none": 0, "head": q[0] if q else 0, "tail": q[-1] if q else 0, "absent": 251}[loaded]
            out.append(c)
            i += 1
    for k, (closed, openable, track, f28) in enumerate([(0, 1, 0, 0), (0, 1, 1, 0), (0, 1, 1, 1), (0, 0, 0, 0), (1, 1, 0, 0), (0, 2, 1, -1), (1, 1, 1, 1)]):
        c = recalc_case(random.Random(850 + k), 3, qlen=5)
        c["obj"]["f64"], c["openable"], c["track"], c["obj"]["f28"] = closed, openable, track, f28
        out.append(c)
    return out


def r_n(i):
    return (1, 2, 4, 6, 9, 14)[i % 6]


# ------------------------------------------------------------------ case generators: connection tests


def virtual_setup(r, c, connected=None):
    """The ride without queue cells: the entry cell and the cell across its rotation, set so the three conditions of IsBOQConnected come out true
    with a good chance."""
    entry = c["entry"]
    rot = c["obj"]["rotation"]
    direction = {0: 0x10, 90: 4, 180: 1, 270: 0x40}.get(rot, 0x10)
    opposite = OPP[direction]
    across = entry + SIDE_DELTA[opposite]
    p = 0.8 if connected is None else (1.0 if connected else 0.0)
    w = c["cells"]["w"]
    nb, d, f = w.get(entry, (0, 0, 0))
    nb = (nb | opposite) if r.random() < p else (nb & ~opposite)
    w[entry] = (nb, d, f)
    nb, d, f = w.get(across, (0, 0, 0))
    nb = (nb | direction) if r.random() < p else (nb & ~direction)
    f = (f | PATH) if r.random() < p else (f & ~PATH)
    w[across] = (nb, d, f)


def boq_case(r, virtual=None, connected=None):
    c = new_case(r)
    virtual = (r.random() < 0.5) if virtual is None else virtual
    n = r.choice((0, 1, 2, 3, 5, 8, 20, r.randrange(1, 12)))
    add_queue(r, c, n=n)
    c["obj"]["flags"] = (c["obj"]["flags"] & ~8) | (0 if virtual else 8)
    cached_back(r, c, r.random() < 0.15)
    if virtual:
        virtual_setup(r, c, connected)
    else:
        w = c["cells"]["w"]
        if c["path"] and r.random() < 0.3:
            b = c["path"][-1]
            nb, d, f = w.get(b, (0, 0, 0))
            w[b] = (d if r.random() < 0.7 else nb, d, f)
            w[b] = (d, d, f)
    return c


def boq_edges():
    out = []
    i = 0
    for virtual in (False, True):
        for conn in (None, True, False):
            for rot in (0, 90, 180, 270):
                c = boq_case(random.Random(900 + i), virtual, conn)
                c["obj"]["rotation"] = rot
                if virtual:
                    virtual_setup(random.Random(950 + i), c, conn)
                out.append(c)
                i += 1
    for k, sp in enumerate([dict(n=0), dict(ring=4), dict(n=1, sides=[1])]):
        c = new_case(random.Random(990 + k))
        add_queue(random.Random(990 + k), c, coherent=True, **sp)
        c["obj"]["flags"] |= 8
        out.append(c)
    return out


def exit_case(r, same=None, walls=None):
    c = new_case(r)
    x, y = r.randrange(3, 125), r.randrange(3, 45)
    exit_cell = cid(x, y)
    direction = r.choice(ANYDIR)
    same = (r.random() < 0.4) if same is None else same
    entry = exit_cell if same else cid(r.randrange(3, 125), r.randrange(3, 45))
    side = OPP[direction] if same else direction
    nxt = exit_cell + SIDE_DELTA[side]
    cells = {exit_cell: (r.getrandbits(8), direction, r.getrandbits(16)), nxt: (r.getrandbits(8), r.choice(ANYDIR), r.getrandbits(16))}
    if entry not in cells:
        cells[entry] = (r.getrandbits(8), r.choice(ANYDIR), r.getrandbits(16))
    c["cells"] = dict(g=dict(cells), m=dict(cells), w=dict(cells))
    c["obj"]["f32"], c["obj"]["f34"] = entry, exit_cell
    c["wallfill"] = r.choice((0, 0, 0xF, 0xFF, r.getrandbits(32), 1 << r.randrange(8)))
    nx, nz = cxy(nxt)
    c["walls"] = []
    if walls is None:
        walls = r.random() < 0.6
    if walls:
        c["walls"].append((nx, nz, r.choice((0, 0xF, r.getrandbits(8), 1 << r.randrange(4), 0xF ^ (1 << r.randrange(4))))))
        if r.random() < 0.3:
            c["walls"].append((nx, nz, r.getrandbits(8)))
        if r.random() < 0.3:
            c["walls"].append((r.getrandbits(7), r.getrandbits(6), r.getrandbits(8)))
    c["entry"] = entry
    return c


def exit_edges():
    out = []
    i = 0
    for same in (False, True):
        for direction in ANYDIR:
            c = exit_case(random.Random(1000 + i), same, False)
            ec = c["obj"]["f34"]
            nb, _, f = c["cells"]["w"][ec]
            c["cells"]["w"][ec] = (nb, direction, f)
            side = OPP[direction] if same else direction
            nxt = ec + SIDE_DELTA[side]
            nx, nz = cxy(nxt)
            for block in range(4):
                c2 = dict(c)
                c2["walls"] = [(nx, nz, 0xF ^ (1 << block))]
                c2["wallfill"] = 0
                out.append(c2)
            c3 = dict(c)
            c3["walls"] = [(nx, nz, 0xF)]
            out.append(c3)
            i += 1
    return out


def behind_case(r, cell=None):
    c = new_case(r)
    add_queue(r, c, coherent=True)
    if cell is None:
        pool = list(c["path"]) + [c["entry"]]
        cell = r.choice(pool + [r.randrange(NIDS), r.randrange(1, NIDS), cid(r.randrange(128), r.randrange(48))])
        if r.random() < 0.5 and c["path"]:
            cell = r.choice(c["path"])
    c["arg"] = cell
    return c


def behind_edges():
    out = []
    for i, cell in enumerate((0, 1, 2, 128, 129, 127, NIDS - 1, NIDS - 128, 5000, 300)):
        out.append(behind_case(random.Random(1100 + i), cell))
    for i in range(14):
        c = behind_case(random.Random(1120 + i))
        c["arg"] = c["path"][min(i % 5, len(c["path"]) - 1)] if c["path"] else c["entry"]
        out.append(c)
    return out


# Positions up to 0x1FFFFFF: beyond 33685504 the product with 255 / 4 passes 2**31, where the emulator's fctiwz and the ISA's differ (not tested).
HUGE = (0x1FFFFFF, 1 << 24, (1 << 24) + 1, (1 << 24) + 3, 0xFFFFFF, 255, 256, 0x7FFFFF, 0x1000003)


def positions(r, nlen):
    k = r.randrange(9)
    if k < 3:
        return r.randrange(4)
    if k < 6:
        return r.randrange(4 * nlen + 4)
    if k == 6:
        return 4 * (nlen - 1) + 3 if nlen else 3
    if k == 7:
        return r.randrange(4 * nlen + 4, 4 * nlen + 40)
    return r.choice(HUGE)


def fix_directions(r, c):
    """Make the entry and back cells of the ride's own map mostly carry real directions and path neighbours."""
    w = c["cells"]["w"]
    for cell in c["path"][-1:] + [c["entry"]]:
        nb, d, f = w.get(cell, (0, 0, 0))
        if r.random() < 0.85:
            d = r.choice(DIRS)
        w[cell] = (nb, d, f)
    if c["path"]:
        b = c["path"][-1]
        nb, d, f = w.get(b, (0, 0, 0))
        for s in DIRS:
            if r.random() < 0.5:
                nb |= s
            nx = b + SIDE_DELTA[s]
            nbn, dn, fn = w.get(nx, (0, 0, 0))
            if r.random() < 0.5:
                fn |= PATH
            w[nx] = (nbn, dn, fn)
        w[b] = (nb, d, f)


def convert_case(r, flavour, n=None, position=None, **kw):
    c = new_case(r)
    if n is None:
        n = r.choice((0, 1, 2, 3, 5, 8, 12, r.randrange(1, 15)))
    add_queue(r, c, n=n, cycle_ok=False, **kw)
    nlen = len(c["path"])
    c["pos"] = positions(r, nlen) if position is None else position
    if c["pos"] >= 4 * nlen + 64:   # a long walk along the line: no decoys or junk cells that could make a loop of cells
        kw.pop("decoys", None)
        kw.pop("junk", None)
        add_queue(r, c, n=n, cycle_ok=False, decoys=0, junk=0, **kw)
        nlen = len(c["path"])
    if n and r.random() < 0.7:
        fix_directions(r, c)
    if flavour == "virtual":
        c["pos"] = r.choice((0, 1, 2, 3, c["pos"], c["pos"]))
    c["rands"] = [r.getrandbits(32) for _ in range(r.randrange(1, 4))]
    c["obj"]["flags"] = (c["obj"]["flags"] & ~8) | (8 if flavour in ("physical", "generic8") else 0)
    if flavour == "generic":
        c["obj"]["flags"] |= 8 if r.random() < 0.5 else 0
        if not c["obj"]["flags"] & 8:
            c["pos"] = r.choice((0, 1, 2, 3, 3, c["pos"]))
    if flavour == "virtual" and r.random() < 0.2:
        w = c["cells"]["w"]
        nb, d, f = w.get(c["entry"], (0, 0, 0))
        w[c["entry"]] = (nb, r.choice((0, 2, 8)), f)
    cached_back(r, c, r.random() < 0.15)
    return c


def convert_edges(flavour):
    out = []
    i = 0
    for n in (1, 2, 5, 12):
        for pos in (0, 1, 2, 3, 4, 7, 4 * n - 1, 4 * n, 4 * n + 3, 4 * n + 4, 0x1FFFFFF):
            out.append(convert_case(random.Random(1200 + i), flavour, n, pos, coherent=(i % 2 == 0)))
            i += 1
    for d in (0x10, 4, 1, 0x40, 0, 2):
        for pos in (0, 3):
            c = convert_case(random.Random(1200 + i), flavour, 4, pos, coherent=True)
            for cell in c["path"] + [c["entry"]]:
                nb, _, f = c["cells"]["w"].get(cell, (0, 0, 0))
                c["cells"]["w"][cell] = (nb, d, f)
            out.append(c)
            i += 1
    if flavour != "virtual":
        for pos in (3, 7, 11, 15):
            for k in range(3):
                c = convert_case(random.Random(1300 + i), flavour, pos // 4 + 1, pos, coherent=(k == 0))
                out.append(c)
                i += 1
    return out


# ------------------------------------------------------------------ the tests


def conv_args(c):
    return [RB + OBJ, c["pos"] & MASK, RB + OUT, RB + OUT + 4, RB + OUT + 5]


def conv_call(name):
    return lambda lib, c: getattr(lib, "tpw_oqu_" + name)(c["pos"] & MASK)


def arg_hw(c):
    return hword(c["arg"]) | c["g"]


TESTS = [
    make_test("CObject::AddPersonToQueue", ADD, lambda c: [RB + OBJ, arg_hw(c)], lambda lib, c: lib.tpw_oqu_add(c["arg"]), add_case, add_edges, kind="none"),
    make_test("CObject::RemovePersonFromQueue", REMOVE, lambda c: [RB + OBJ, arg_hw(c)], lambda lib, c: lib.tpw_oqu_remove(c["arg"]), remove_case,
              remove_edges, kind="none"),
    make_test("CObject::GetNumPeopleInQueue", NUM, lambda c: [RB + OBJ], lambda lib, c: lib.tpw_oqu_num(), num_case, num_edges),
    make_test("CObject::GetPositionInQueue", POSITION, lambda c: [RB + OBJ, arg_hw(c)], lambda lib, c: lib.tpw_oqu_position(c["arg"]), position_case,
              position_edges),
    make_test("CObject::GetFrontOfPhysicalQueue", FRONT, lambda c: [RB + RES, RB + OBJ], lambda lib, c: lib.tpw_oqu_front(), front_case, front_edges,
              kind="tag"),
    make_test("CObject::GetBackOfPhysicalQueue", BACKP, lambda c: [RB + RES, RB + OBJ], lambda lib, c: lib.tpw_oqu_back_physical(),
              lambda r: back_case(r, "physical"), lambda: back_edges("physical"), kind="tag"),
    make_test("CObject::GetBackOfVirtualQueue", BACKV, lambda c: [RB + RES, RB + OBJ], lambda lib, c: lib.tpw_oqu_back_virtual(),
              lambda r: back_case(r, "virtual"), lambda: back_edges("virtual"), kind="tag"),
    make_test("CObject::GetBackOfQueue", BACK, lambda c: [RB + RES, RB + OBJ], lambda lib, c: lib.tpw_oqu_back(), lambda r: back_case(r, "generic"),
              lambda: back_edges("generic"), kind="tag"),
    make_test("CObject::RecalculateBackOfQueue", RECALC, lambda c: [RB + OBJ], lambda lib, c: lib.tpw_oqu_recalc(), recalc_case, recalc_edges, kind="none"),
    make_test("CObject::IsBOQConnected", BOQ, lambda c: [RB + OBJ], lambda lib, c: lib.tpw_oqu_boq(), boq_case, boq_edges),
    make_test("CObject::IsExitCellConnected", EXIT, lambda c: [RB + OBJ], lambda lib, c: lib.tpw_oqu_exit(), exit_case, exit_edges),
    make_test("CObject::GetQCellBehind", BEHIND, lambda c: [RB + RES, arg_hw(c)], lambda lib, c: lib.tpw_oqu_behind(c["arg"]), behind_case, behind_edges,
              kind="tag"),
    make_test("CObject::ConvertQPositionToMapPos", CONVERT, conv_args, conv_call("convert"), lambda r: convert_case(r, "generic"),
              lambda: convert_edges("generic")),
    make_test("CObject::ConvertPhysicalQPositionToMapPos", CONVP, conv_args, conv_call("convert_physical"), lambda r: convert_case(r, "physical"),
              lambda: convert_edges("physical")),
    make_test("CObject::ConvertVirtualQPositionToMapPos", CONVV, conv_args, conv_call("convert_virtual"), lambda r: convert_case(r, "virtual"),
              lambda: convert_edges("virtual")),
]
