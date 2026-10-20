"""Unit w206, the hoarding edit (the fill of the park's inside and the legality and cost check of an edit, with four cell tests) against
libtpw_unit_HoardingEdit.dylib (tests/unit_HoardingEdit.cpp), main app ("SimTheme Park"): src/LLCellSystem_hoardedit.cpp (LLCC_FillInsideOfPark
0x10093b0c and LLCC_IsHoardingEditLegal 0x1009464c), src/TrackCell_isflag.cpp (CTrackCell::IsFlag 0x100a2918) and src/MapCell_kinds.cpp
(CMapCell::UnSetFlag 0x1009bc18, IsFreeform 0x1009c59c, IsWater 0x1009c408): 6 functions.

A case is the cell system's words (the hoarding extents, gLLCC4818, the width and height of the fill, the four edge counts, the 0x200 bytes of edge
marks, the cost word, the recording word and the rest), the world (a raw block of 0x1B0000 bytes from world + 0x2d8: the 16384 map cells of 0x44 bytes,
then the 16384 track cells of 0x28 bytes; every cell has its id and kind, the case patches the fields it needs), the scripted answers of the fakes
(LLCC_SoIsItLegal and CBankAccount::GetBalance) and a call. The world is built twice by ONE piece of Python: in emulator memory at the Mac's addresses
(big endian) and in the host library's memory (little endian). The original runs for real on the PowerPC side: the six functions, CCellBase::
GetNeighbours, NumberOfNeighbours, NumberOfHVNeighbours, SetOverlapCounter, GetMapCellEquiv and GetTrackCellEquiv, both CTrackCell::GetNeighbouringCell,
CMapCell::GetNeighbouringCell, MapDelta::operator*, OPPOSITE, CMap::IsXYValid, and the landed CTrackCell::IsHoarding, IsTrackX, IsCorner, IsStraight
and CMapCell::IsFlag (the host copies the bodies of the small ones from src/MapDelta_map.cpp; the eight compass deltas are written by this module,
the Mac builds them at startup). Every other callee is a fake on both sides that records its arguments (a cell is named by its index in its block, the
bank by a label): CTrackCell::Delete and QuickDelete, CMapCell::SetHFieldTile, LLCC_CopyCurrentToNewImage, LLCC_AddNewAreaToImage, LLCC_SoIsItLegal
(answers from a queue), CWorld::GetBankAccount, CBankAccount::GetBalance (answers from a queue), CTRACE and GAME_ASSERTMSG.

What is compared: the answer, the event log, every cell of the world (both blocks, so the flags the fill clears and the overlap counters it resets), the
0x200 mark bytes and every global the functions can store (the edge counts too). The fill runs only on scenes that a Python model of the walk and its
scans says are safe: the walk ends, every scan ends at a locked cell (or the top of the level), every unlocked cell a scan or the second part touches
has x and y of at most 0x7e (so that its three neighbours exist) and every parent id is a real cell. The scenes are rings with a gap (the latch of the
north scan too), polylines, single cells (also off the right edge), grids of corners, noise, lines with walls put where the model asks for them,
strays (unlocked hoarding cells, some behind track X cells), map cells with the flag 0x40 in patterns, overlap counters and region sizes from empty to
the whole level. The edit check is built from the exit it should take: a corner out of the level, a rectangle two cells wide or less, the four gates
against the hoarding extents (and near misses), two freeform cells in a row, two hoarding corners and no straight piece, an area all inside or all outside,
a refusal of the image check (its low byte), the cost against the bank (balances around the total, the recording flag) and the legal end.

Not tested: a NULL neighbour of a cell at x or y above 0x7e (the Mac reads through it; cells that reach it are kept out), a scan or a walk that never
ends or leaves the world block, cell ids past 16384 or a parent id outside 1 to 16384, the bytes after the 0x200 marks, junk in the upper half of a
16 bit flag argument, a charge value other than 0 or 1, additions that overflow 32 bits (the wrap is the same on both sides, one case covers it).
"""
import ctypes as C
import random
import struct
import zlib

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_HoardingEdit"
MASK = 0xFFFFFFFF

# ---- the six functions: name -> (address, host wrapper, argument count)
FN = dict(
    FillInside=(0x10093B0C, "FillInside", 0), EditLegal=(0x1009464C, "EditLegal", 5),
    TrackIsFlag=(0x100A2918, "TrackIsFlag", 2), UnSetFlag=(0x1009BC18, "UnSetFlag", 2),
    IsFreeform=(0x1009C59C, "IsFreeform", 1), IsWater=(0x1009C408, "IsWater", 1),
)
OWN = {a for a, _, _ in FN.values()}
TRACK_FNS = {"TrackIsFlag"}
MAP_FNS = {"UnSetFlag", "IsFreeform", "IsWater"}
ANSWER_MASK = dict(EditLegal=MASK, TrackIsFlag=MASK, IsFreeform=MASK, IsWater=MASK)

# in-binary callees that run for real: OPPOSITE, CMap::IsXYValid, CCellBase::GetNeighbours, NumberOfNeighbours, NumberOfHVNeighbours, GetMapCellEquiv,
# GetTrackCellEquiv, both CTrackCell::GetNeighbouringCell, MapDelta::operator*, CMapCell::GetNeighbouringCell (by delta), CTrackCell::IsHoarding, IsTrackX,
# IsCorner, CMapCell::IsFlag, CTrackCell::IsStraight and CCellBase::SetOverlapCounter
REAL_OTHER = {0x10102500, 0x101012F4, 0x100816E8, 0x10081744, 0x10081890, 0x100F49A0, 0x100F48C4, 0x1012FDDC, 0x1012FCB8, 0x101037D0, 0x101036B0,
              0x100A35DC, 0x100A367C, 0x100A37A0, 0x1009BB70, 0x100A3868, 0x10081A1C}

# ---- the callees faked on both sides
ASSERT, CTRACE = 0x1000DDE8, 0x1006FCBC
TRACK_DELETE, TRACK_QUICK_DELETE, SET_TILE = 0x100A40C4, 0x100A4E88, 0x1009CDB4
COPY_IMAGE, ADD_AREA, SO_LEGAL = 0x10096838, 0x100968D8, 0x10096958
GET_BANK, GET_BALANCE = 0x1013EDFC, 0x100F32B4

# ---- the Mac's addresses of the globals: name -> (address, width)
WORDS = dict(
    gHoardingRight=(0x1029E03C, 4), gHoardingBottom=(0x1029E040, 4), gHoardingLeft=(0x1029E044, 4), gHoardingTop=(0x1029E048, 4),
    gLLCCE0CC=(0x102DE0CC, 1), gLLCC4818=(0x102E4818, 1), gLLCCWidth=(0x102E47FC, 4), gLLCCHeight=(0x102E47F8, 4),
    gLLCC47CC=(0x102E47CC, 4), gLLCCC204=(0x102DC204, 4),
    edge0=(0x102E05A0, 4), edge1=(0x102E05A4, 4), edge2=(0x102E05A8, 4), edge3=(0x102E05AC, 4),
    cost=(0x102B4B34, 4), recording=(0x1037EC68, 4),
)
UNSIGNED = {"gLLCCE0CC", "gLLCC4818"}
FMT = {1: "B", 2: "H", 4: "I"}
MARKS, MARK_BYTES = 0x102E05B0, 0x200       # gLLCCEdgeMarks: four arrays of 0x80 bytes
G = dict(game_world=0x1037EBFC, world_map=0x1037EC08, world=0x1037EC0C)
DELTAS = {0x1034C220: (-1, -1), 0x1034C234: (-1, 0), 0x1034C248: (-1, 1), 0x1034C25C: (0, 1), 0x1034C270: (1, 1), 0x1034C284: (1, 0),
          0x1034C298: (1, -1), 0x1034C2AC: (0, -1)}
HOST_LAYOUT = [0x28, 0x1102D8, 0x10, 0x44, 0xE, 0x22, 0x20, 0x10, 0x200]

NCELL = 16384
MAP_BYTES, TRACK_BYTES = 0x44 * NCELL, 0x28 * NCELL
BLOB = MAP_BYTES + TRACK_BYTES          # from world + 0x2d8; the track cells start at +0x110000
FIELDS = dict(kind=(2, "B"), type=(8, "I"), nb=(0xC, "B"), dir=(0xD, "B"), flags=(0xE, "H"), parent=(0x10, "H"), overlap=(0x20, "H"), hnb=(0x22, "B"))


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

R = CL.recorder


def build_stubs():
    S = CL.Stubs()
    S.real.update(OWN)
    S.real.update(REAL_OTHER)

    def ctrace(e):
        text = e.cstr(e.r(4))[:159]
        ints = [e.r(3) & MASK]
        if "%d" in text:
            ints.append(s32(e.r(5)))
        e.cl.rec("CTRACE", ints, (text,))

    def assert_(e):
        e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),), (e.cstr(e.r(4))[:159],))

    def delete(e):
        e.cl.rec("Delete", [(e.r(3) - (e.hed_world + 0x1102D8)) // 0x28, s32(e.r(4)), s32(e.r(5)), e.r(6) & 0xFF], ())

    def quick_delete(e):
        e.cl.rec("QuickDelete", [(e.r(3) - (e.hed_world + 0x1102D8)) // 0x28], ())

    def set_tile(e):
        e.cl.rec("SetHFieldTile", [(e.r(3) - (e.hed_world + 0x2D8)) // 0x44], ())

    def get_bank(e):
        e.cl.rec("GetBankAccount", [1 if e.r(3) == e.hed_world else 0], ())
        e.set_r(3, e.hed_bank)

    S.internal(CTRACE, ctrace)
    S.internal(ASSERT, assert_)
    S.internal(TRACK_DELETE, delete)
    S.internal(TRACK_QUICK_DELETE, quick_delete)
    S.internal(SET_TILE, set_tile)
    S.internal(COPY_IMAGE, R("CopyCurrentToNewImage"))
    S.internal(ADD_AREA, R("AddNewAreaToImage", "iiii"))
    S.internal(SO_LEGAL, R("SoIsItLegal", "", ret=lambda e, L: L.ret("SoIsItLegal", 0) & MASK))
    S.internal(GET_BANK, get_bank)
    S.internal(GET_BALANCE, R("GetBalance", "L", ret=lambda e, L: L.ret("GetBalance", 0) & MASK))
    return S


STUBS = build_stubs()


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    e.hed_world = T.persist_alloc(e, 0x1B1000)
    e.hed_bank = T.persist_alloc(e, 16)


# ---------------------------------------------------------------------------------------------------------------- the two sides

def make_template(order, size, kind):
    b = bytearray(size * NCELL)
    for i in range(NCELL):
        struct.pack_into(order + "H", b, i * size, i + 1)
        b[i * size + 2] = kind
    return b


TEMPLATES = {order: bytes(make_template(order, 0x44, 1)) + bytes(make_template(order, 0x28, 2)) for order in (">", "<")}


class Side:
    """What the shared code needs from either memory."""

    def setw(self, name, v):
        a, width = WORDS[name][0], WORDS[name][1]
        self.write(self.gaddr(name, a), struct.pack(self.order + FMT[width], v & ((1 << (8 * width)) - 1)))

    def getw(self, name):
        a, width = WORDS[name][0], WORDS[name][1]
        v = struct.unpack(self.order + FMT[width], self.read(self.gaddr(name, a), width))[0]
        if name not in UNSIGNED and v >= 1 << (8 * width - 1):
            v -= 1 << (8 * width)
        return v

    def put_marks(self, data):
        self.write(self.gaddr("gLLCCEdgeMarks", MARKS), data)

    def get_marks(self):
        return self.read(self.gaddr("gLLCCEdgeMarks", MARKS), MARK_BYTES)

    def put_world(self, case):
        blob = bytearray(TEMPLATES[self.order])
        apply_patches(blob, self.order, 0, 0x44, case["maps"])
        apply_patches(blob, self.order, MAP_BYTES, 0x28, case["tracks"])
        self.initial = bytes(blob)
        self.write(self.world() + 0x2D8, self.initial)

    def get_world(self):
        """The cells that differ from what the case wrote, each as (index, bytes) with the multi byte fields in host order, per block."""
        now = self.read(self.world() + 0x2D8, BLOB)
        out = []
        for base, size, count in ((0, 0x44, NCELL), (MAP_BYTES, 0x28, NCELL)):
            changed = []
            a, b = self.initial[base:base + size * count], now[base:base + size * count]
            if a != b:
                for i in range(count):
                    if a[i * size:(i + 1) * size] != b[i * size:(i + 1) * size]:
                        changed.append((i, canon(self.order, b[i * size:(i + 1) * size])))
            out.append(changed)
        return out


def canon(order, cell):
    """A cell with its id, type, flags, parent and overlap counter in host byte order (the Mac's are big endian)."""
    if order == "<":
        return bytes(cell)
    c = bytearray(cell)
    for off, n in ((0, 2), (8, 4), (0xE, 2), (0x10, 2), (0x20, 2)):
        c[off:off + n] = c[off:off + n][::-1]
    return bytes(c)


def apply_patches(blob, order, base, size, patches):
    for cid, fields in patches.items():
        o = base + (cid - 1) * size
        for name, v in fields.items():
            off, code = FIELDS[name]
            width = {"B": 1, "H": 2, "I": 4}[code]
            struct.pack_into(order + code, blob, o + off, v & ((1 << (8 * width)) - 1))


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    order = ">"

    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        self.L = e.cl = CL.PpcLog({e.hed_bank: 1}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def gaddr(self, name, mac):
        return mac

    def world(self):
        return self.e.hed_world

    def set_world(self):
        e = self.e
        for key, value in (("game_world", self.world()), ("world_map", self.world() + 0x2D8), ("world", self.world())):
            e.write(G[key], struct.pack(">I", value))
        for address, (x, y) in DELTAS.items():
            e.write(address, struct.pack(">ii", x, y) + b"\0" * 12)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [x & MASK for x in args])[0]

    def cell(self, fn, cid):
        if fn in MAP_FNS:
            return self.world() + 0x2D8 + (cid - 1) * 0x44
        return self.world() + 0x1102D8 + (cid - 1) * 0x28


class Host(Side):
    """The same on the host library."""
    order = "<"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_hed_init()
        CL.host_begin(lib, {1: lib.tpw_hed_gaddr(b"bank")}, case["queues"])
        self.cache = {}

    def done(self):
        pass

    def gaddr(self, name, mac):
        if name not in self.cache:
            self.cache[name] = self.lib.tpw_hed_gaddr(name.encode())
            assert self.cache[name], name
        return self.cache[name]

    def world(self):
        return self.gaddr("world", 0)

    def set_world(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def cell(self, fn, cid):
        return cid


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    u64, i32, i64 = C.c_uint64, C.c_int32, C.c_int64
    lib.tpw_hed_gaddr.restype, lib.tpw_hed_gaddr.argtypes = u64, [C.c_char_p]
    lib.tpw_hed_init.argtypes = []
    lib.tpw_hed_layout.argtypes = [C.POINTER(i64)]
    lib.tpw_cl_label.argtypes = [i32, C.c_void_p]
    for name, (_, wrapper, argc) in FN.items():
        f = getattr(lib, "tpw_hed_" + wrapper)
        f.restype = i32
        f.argtypes = [i32] * argc
    lib.tpw_hed_init()
    out = (i64 * 32)()
    n = lib.tpw_hed_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, [hex(v) for v in list(out)[:n]]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the run

def build(s, case):
    """Writes the case's globals and blocks through Side: the same code for both memories (offsets are the Mac's)."""
    s.set_world()
    s.put_world(case)
    for name, v in case["words"].items():
        s.setw(name, v)
    s.put_marks(case["marks"])


def execute(s, case):
    name, args = case["fn"], list(case["args"])
    address, wrapper, _ = FN[name]
    if name in TRACK_FNS or name in MAP_FNS:
        args[0] = s.cell(name, args[0])
    if isinstance(s, Ppc):
        r = s.call(address, *args)
    else:
        r = getattr(s.lib, "tpw_hed_" + wrapper)(*[s32(a) for a in args])
    mask = ANSWER_MASK.get(name)
    return None if mask is None else (r & mask)


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides."""
    out = {"answer": answer, "log": s.log(), "words": {n: s.getw(n) for n in WORDS}}
    out["marks"] = s.get_marks()
    out["maps"], out["tracks"] = s.get_world()
    return out


def run(s, case):
    try:
        build(s, case)
        answer = execute(s, case)
        return observe(s, case, answer)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SMALL = [0, 1, -1, 2, 3, 0x7F, 0x80, 0x100, 0x1000, -2, -0x100, 0x3FFFFFFF, -0x3FFFFFFF]
# map cell types: water is 2, freeform is 7 or 0x1e; the rest is for the near misses
KIND_TYPES = [2, 7, 0x1E, 0, 1, 3, 4, 5, 6, 8, 9, 0xA, 0xB, 0xC, 0x11, 0x15, 0x19, 0x1D, 0x1F, 0x20, 0x102, 0x107, 0x1000007, 0x8000001E, 0x80000002,
              0x7FFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0x80000000, 0x2000000, 0x1E00]


class Draft:
    """The cells a case patches: track and map cells by 1 based id."""

    def __init__(self):
        self.t = {}
        self.m = {}

    def ts(self, cid, **f):
        self.t.setdefault(cid, {}).update(f)

    def ms(self, cid, **f):
        self.m.setdefault(cid, {}).update(f)


def cid_of(x, y):
    return x + (y << 7) + 1


def junk(r):
    return r.choice(SMALL + [r.randrange(1 << 32) - (1 << 31)])


def rnd_marks(r):
    mode = r.random()
    if mode < 0.3:
        return bytes(r.randrange(256) for _ in range(MARK_BYTES))
    return bytes([r.choice([0xFF, 0xFF, 0, 1, r.randrange(256)])]) * MARK_BYTES


def base_words(r):
    w = dict(gHoardingRight=r.randint(0, 127), gHoardingBottom=r.randint(0, 127), gHoardingLeft=r.randint(0, 127), gHoardingTop=r.randint(0, 127))
    w["gLLCCE0CC"] = r.choice([0, 1, 4, 0x80, 0xFF, r.randrange(256)])
    w["gLLCC4818"] = r.choice([0, 0, 1, 1, 2, 0xFF])
    w["gLLCCWidth"] = r.choice([0, 1, 2, 3, 0x10, 0x40, 0x7F, 0x80, r.randrange(0, 0x85)])
    w["gLLCCHeight"] = r.choice([0, 1, 2, 3, 0x10, 0x40, 0x7F, 0x80, r.randrange(0, 0x85)])
    w["gLLCC47CC"] = junk(r)
    w["gLLCCC204"] = junk(r)
    for i in range(4):
        w["edge%d" % i] = junk(r)
    w["cost"] = junk(r)
    w["recording"] = r.choice([0, 1, 2, -1, r.randrange(1 << 32) - (1 << 31)])
    return w


def base(r, fn, args=(), draft=None, **over):
    d = draft or Draft()
    case = dict(fn=fn, args=list(args), words=base_words(r), marks=rnd_marks(r), tracks=d.t, maps=d.m, queues={})
    case.update(over)
    return case


def opposite(d):
    if d == 0:
        return 0
    if d < 0x10:
        return (d << 4) & 0xF0
    return (d >> 4) & 0xFF


def popcount(v):
    return bin(v & 0xFF).count("1")


def is_corner(nb):
    if popcount(nb & 0x55) != 2:
        return False
    return bool((nb & 1 and nb & 4) or (nb & 1 and nb & 0x40) or (nb & 0x10 and nb & 4) or (nb & 0x10 and nb & 0x40))


# ---------------------------------------------------------------------------------------------------------------- the fill: scenes and their model

SINGLE = {1: (0, -1), 2: (1, -1), 4: (1, 0), 8: (1, 1), 0x10: (0, 1), 0x20: (-1, 1), 0x40: (-1, 0), 0x80: (-1, -1)}
DIRB = {"N": (1, 0, -1), "E": (4, 1, 0), "S": (0x10, 0, 1), "W": (0x40, -1, 0)}
BACKBIT = {1: 0x10, 4: 0x40, 0x10: 1, 0x40: 4}
EDGE = 126               # the largest x or y of a cell whose three neighbours (+x, +y, +x +y) exist


def cid16(x, y):
    """The cell id of the original's arithmetic: 16 bits, x and y not bounded."""
    return (1 + x + (y << 7)) & 0xFFFF


def xy_of(cid):
    return (cid - 1) & 0x7F, (cid - 1) >> 7


def tfield(tracks, cid, name):
    return tracks.get(cid, {}).get(name, 0)


class Unsafe(Exception):
    """A scan the original would send out of the world or on for ever: `cells` are the ids it visited, in order (a locked cell among them ends it)."""

    def __init__(self, cells):
        Exception.__init__(self)
        self.cells = cells


def scan_ids(d, x, y, top):
    """The ids a scan for heading d visits, in order; `top` is the latch (one element list) that outlives the scan."""
    if d == 0x10:
        k = x
        while True:
            yield cid16(k - 2, y)
            k -= 2
    elif d == 4:
        k = y
        while True:
            yield cid16(x, k + 2)
            k += 2
    elif d == 1:
        k = x
        while True:
            yield cid16(k + 2, y)
            k += 2
    else:
        k = y - 2
        while True:
            cid = cid16(x, k)
            k -= 2
            if k < 0:
                top[0] = True
            yield cid
            if top[0]:
                return


def fill_walk(w, tracks, mask, limit=300, scan_cap=48, cell_cap=450):
    """The original's walk with its scans. Returns the ids of the unlocked cells the scans clear, in order; raises Unsafe when a scan needs a wall and
    ValueError when the scene cannot work."""
    x, y = w["gHoardingRight"], w["gHoardingBottom"]
    d, steps, top, scanned = 4, 0, [False], []
    nbname = "hnb" if mask else "nb"
    while True:
        if steps > limit:
            raise ValueError("walk")
        cid = cid16(x, y)
        if not 1 <= cid <= NCELL:
            raise ValueError("cell")
        nb = tfield(tracks, cid, nbname)
        if tfield(tracks, cid, "type") == 0x19:
            if d in (0x10, 4, 1, 0x40):
                seen = []
                for sid in scan_ids(d, x, y, top):
                    if not 1 <= sid <= NCELL:
                        if seen:
                            raise Unsafe(seen)
                        raise ValueError("scan")
                    seen.append(sid)
                    if tfield(tracks, sid, "flags") & 0x20:
                        break
                    sx, sy = xy_of(sid)
                    if sx > EDGE or sy > EDGE or len(seen) >= scan_cap:
                        raise Unsafe(seen)
                    scanned.append(sid)
                    if len(scanned) > cell_cap:
                        raise ValueError("long")
            if is_corner(nb):
                d = opposite(d) ^ nb
        stop = False
        if d in SINGLE:
            i = cid - 1
            nx, ny = (i & 0x7F) + 2 * SINGLE[d][0], (i >> 7) + 2 * SINGLE[d][1]
            if 0 <= nx < 128 and 0 <= ny < 128 and tfield(tracks, cid_of(nx, ny), "type") != 0x19:
                stop = True
        if popcount(nb) == 1 and steps != 0:
            stop = True
        if stop:
            return scanned
        if d == 0x10:
            y += 2
        elif d == 4:
            x += 2
        elif d == 1:
            y -= 2
        elif d == 0x40:
            x -= 2
        steps += 1


def fill_region(w, tracks, cap=140):
    """The second part: the hoarding cells it deletes (unlocked, found directly or through a track X cell's parent). Raises ValueError for a cell whose
    neighbours would not exist or a parent that is no cell. Returns the ids of the cells it deletes (a cell found through two track X cells twice)."""
    wd, ht = w["gLLCCWidth"], w["gLLCCHeight"]
    ids = []
    for cid, f in tracks.items():
        t = f.get("type", 0)
        if t not in (0x19, 0xC, 0x11):
            continue
        x, y = xy_of(cid)
        if x > wd - 2 or y > ht - 2:
            continue
        if t != 0x19:
            cid = f.get("parent", 0)
            if not 1 <= cid <= NCELL:
                raise ValueError("parent")
            t = tfield(tracks, cid, "type")
            if t != 0x19:
                continue
        if tfield(tracks, cid, "flags") & 0x20:
            continue
        px, py = xy_of(cid)
        if px > EDGE or py > EDGE:
            raise ValueError("stray")
        ids.append(cid)
    if len(ids) > cap:
        raise ValueError("many")
    return ids


def locked_flags(r, locked):
    if locked:
        return r.choice([0x20, 0x20, 0x20, 0x20, 0xFFFF, 0x8020, 0x21, 0x1234 | 0x20, r.randrange(0x10000) | 0x20])
    return r.choice([0, 0, 0, 0xFFDF, 0x8000, 0x1, 0x1F, r.randrange(0x10000) & ~0x20])


def lay_path(d, r, sx, sy, moves, mask, p_locked, noise=True):
    """Hoarding cells at the start and after each move of two cells; each cell's neighbour bits (in the byte the walk reads) point to the previous
    and next cell, the other byte holds noise; the cells are locked with the probability p_locked."""
    pts, bits = [(sx, sy)], [0]
    x, y = sx, sy
    for m in moves:
        bit, dx, dy = DIRB[m]
        bits[-1] |= bit
        x += 2 * dx
        y += 2 * dy
        pts.append((x, y))
        bits.append(BACKBIT[bit])
    read, other = ("hnb", "nb") if mask else ("nb", "hnb")
    for (x, y), b in zip(pts, bits):
        if not (0 <= x <= 127 and 0 <= y <= 127):
            raise ValueError("off the map")
        f = {"type": 0x19, read: b, "flags": locked_flags(r, r.random() < p_locked)}
        if noise:
            f[other] = r.randrange(256)
        if r.random() < 0.3:
            f["overlap"] = r.choice([1, 2, 7, 0x7FFF, 0x8000, 0xFFFF])
        d.ts(cid_of(x, y), **f)
        if r.random() < 0.3:
            d.ms(cid_of(x, y), type=0x15)
    return pts


def random_moves(r, sx, sy, n, first_east=True):
    visited = {(sx, sy)}
    x, y = sx, sy
    moves, last = [], None
    for k in range(n):
        options = [m for m in "NESW" if last is None or BACKBIT[DIRB[m][0]] != DIRB[last][0]]
        r.shuffle(options)
        if k == 0 and first_east and r.random() < 0.85:
            options.insert(0, "E")
        for m in options:
            nx, ny = x + 2 * DIRB[m][1], y + 2 * DIRB[m][2]
            if 0 <= nx <= EDGE and 0 <= ny <= EDGE and (nx, ny) not in visited:
                moves.append(m)
                visited.add((nx, ny))
                x, y, last = nx, ny, m
                break
        else:
            break
    return moves


def ring_scene(r, d, mask, p_locked, latch):
    """A ring walked clockwise from a start on the top edge, open at a gap of g cells (g = 0: the one link missing); the latch scenes put the top edge
    at row 2 to 9 so that the north scans from the bottom edge reach the top of the level through the gap."""
    a, b = r.randint(0, 4), r.randint(0, 4)
    g = r.randint(1, 3) if latch else r.randint(0, 3)
    h = r.randint(1, 6)
    wd = a + b + 1 + g
    moves = ["E"] * a + ["S"] * h + ["W"] * wd + ["N"] * h + ["E"] * b
    left = 2 * (b + 1 + g)
    if latch:
        sy = r.randint(2, 9)
    elif r.random() < 0.5:
        sy = r.randint(0, 8)
    else:
        sy = r.randint(0, EDGE - 2 * h)
    sx = left + r.randint(0, 4) if r.random() < 0.5 else r.randint(left, EDGE - 2 * a)
    lay_path(d, r, sx, sy, moves, mask, p_locked)
    return sx, sy


def polyline_scene(r, d, mask, p_locked):
    sx, sy = r.randint(0, EDGE), r.randint(0, EDGE)
    if r.random() < 0.4:
        sx, sy = r.randint(0, 20), r.randint(2, 20)
    moves = random_moves(r, sx, sy, r.randint(0, 24))
    pts = lay_path(d, r, sx, sy, moves, mask, p_locked)
    if len(pts) > 3 and r.random() < 0.25:
        x, y = r.choice(pts[1:-1])       # a gap or another kind in the middle
        d.ts(cid_of(x, y), type=r.choice([0, 1, 0x18, 0x1A, 0x119, 0xC, 0x11, 0x10019, 0x80000019, 0x1000019]))
    return sx, sy


def single_scene(r, d, mask):
    sx = r.choice([r.randint(0, EDGE), r.randint(0, EDGE), 125, 126, 127, 0, 1])
    sy = r.randint(0, EDGE)
    f = {"type": r.choice([0x19, 0x19, 0x19, 0, 0x18, 0x1A, 0x119, 0x10019, 0x80000019]),
         ("hnb" if mask else "nb"): r.choice([0, 1, 4, 0x10, 0x40, 5, 0x41, 0x14, 0x50, 0x45, 0xFF, r.randrange(256)]),
         "flags": locked_flags(r, r.random() < 0.5)}
    d.ts(cid_of(sx, sy), **f)
    return sx, sy


def corners_scene(r, d, mask):
    """A grid of hoarding cells (two apart) whose bits are corners and ends: wrong headings, asserts, turns in place."""
    sx, sy = r.randint(2, 100), r.randint(2, 100)
    bits = [0x05, 0x11, 0x14, 0x41, 0x44, 0x50, 0x15, 0x45, 0x01, 0x04, 0x10, 0x40, 0x54, 0x55, 0x00, 0x14, 0x41, 0x50, 0x05]
    for gy in range(-3, 4):
        for gx in range(-3, 4):
            x, y = sx + 2 * gx, sy + 2 * gy
            if 0 <= x <= EDGE and 0 <= y <= EDGE and r.random() < 0.8:
                f = {"type": 0x19, ("hnb" if mask else "nb"): r.choice(bits), ("nb" if mask else "hnb"): r.randrange(256),
                     "flags": locked_flags(r, r.random() < 0.8)}
                d.ts(cid_of(x, y), **f)
    d.ts(cid_of(sx, sy), type=0x19, **{("hnb" if mask else "nb"): r.choice(bits)})
    return sx, sy


def noise_scene(r, d, mask):
    sx, sy = r.randint(2, 110), r.randint(2, 110)
    p = r.choice([0.15, 0.4, 0.7])
    nbs = [0, 1, 4, 0x10, 0x40, 5, 0x41, 0x14, 0x50, 0x45, 0x15, 0x55, 0xFF, 0x44, 0x11, r.randrange(256)]
    for dy in range(-6, 7):
        for dx in range(-6, 7):
            x, y = sx + dx, sy + dy
            if 0 <= x <= EDGE and 0 <= y <= EDGE and r.random() < p:
                d.ts(cid_of(x, y), type=r.choice([0x19] * 6 + [0, 1, 0xC, 0x11, 0x18]), flags=locked_flags(r, r.random() < 0.6),
                     **{"hnb" if mask else "nb": r.choice(nbs), "nb" if mask else "hnb": r.randrange(256)})
    d.ts(cid_of(sx, sy), type=r.choice([0x19, 0x19, 0x19, 0]))
    return sx, sy


def wall(r, d, cells):
    """A locked cell among the ones a scan visited (the first ones likelier): the scan ends there."""
    n = len(cells)
    i = min(n - 1, int(n * r.random() ** 2))
    d.ts(cells[i], flags=locked_flags(r, True))


def region_and_strays(r, d, w, region):
    """The width and height of the second part and the unlocked hoarding cells in it (some behind track X cells), with overlap counters and map kinds."""
    pts = [xy_of(c) for c in d.t] or [(0, 0)]
    mx = max(x for x, _ in pts)
    my = max(y for _, y in pts)
    if region is None:
        wd = r.choice([mx + 2, mx + 2, mx + 2, mx + 1, mx + 1, mx, mx + 3, mx + 4, r.randint(0, mx + 2), 0, 1, 2, 3, r.randint(0, 20), -1])
        ht = r.choice([my + 2, my + 2, my + 2, my + 1, my + 1, my, my + 3, my + 4, r.randint(0, my + 2), 0, 1, 2, 3, r.randint(0, 20), -1])
        wd, ht = min(wd, 129), min(ht, 129)
        while max(wd - 1, 0) * max(ht - 1, 0) > 1800:       # the second part visits every cell of the region: keep it small
            if wd >= ht:
                wd = wd * 2 // 3
            else:
                ht = ht * 2 // 3
    else:
        wd, ht = region
    w["gLLCCWidth"], w["gLLCCHeight"] = wd, ht
    if wd < 2 or ht < 2:
        return
    stray_ids = []
    for _ in range(r.choice([0, 0, 1, 2, 3, 6, 12])):
        x, y = r.randint(0, min(wd - 2, EDGE)), r.randint(0, min(ht - 2, EDGE))
        cid = cid_of(x, y)
        q = r.random()
        if q < 0.5 or not stray_ids:
            d.ts(cid, type=0x19, flags=locked_flags(r, r.random() < 0.15), overlap=r.choice([0, 0, 1, 7, 0x7FFF, 0x8000, 0xFFFF]))
            d.ms(cid, type=r.choice([0x15, 0x15, 0, 1, 2, 7, 0x19, 0x14, 0x16]))
            stray_ids.append(cid)
        else:
            parent = r.choice([r.choice(stray_ids), r.choice(stray_ids), cid, cid_of(r.randint(0, EDGE), r.randint(0, EDGE)), r.randint(1, NCELL)])
            d.ts(cid, type=r.choice([0xC, 0x11]), parent=parent, flags=r.choice([0, 0x20, 0xFFFF]))


def decorate_flags(r, d, scanned, freed):
    """The flag 0x40 on the cells the scans clear and on the cells the second part frees (and on their +y, +x +y and +x neighbours), in patterns."""
    p2 = r.choice([0.0, 0.5, 0.9, 1.0])
    for cid in freed:
        if r.random() < p2:
            pattern = r.randrange(1, 16)
            for i, c in enumerate([cid, cid + 128, cid + 129, cid + 1]):
                if pattern >> i & 1:
                    d.ms(c, flags=0x40 | (r.randrange(0x10000) & 0xFFBF if r.random() < 0.4 else 0))
    p = r.choice([0.0, 0.3, 0.7, 1.0, 1.0])
    for cid in scanned:
        x, y = xy_of(cid)
        square = [cid, cid + 128, cid + 129, cid + 1]
        if r.random() < p:
            for c in square:
                if r.random() < 0.55:
                    d.ms(c, flags=0x40 | (r.randrange(0x10000) & 0xFFBF if r.random() < 0.4 else 0))
            d.ms(r.choice(square), flags=0x40 | (r.randrange(0x10000) & 0xFFBF if r.random() < 0.4 else 0))
        elif r.random() < 0.3:
            d.ms(cid, flags=r.randrange(0x10000) & 0xFFBF)


FILL_KINDS = ["ring"] * 6 + ["latch"] * 2 + ["polyline"] * 5 + ["single"] * 2 + ["corners", "noise"]


def fill_try(r, kind, mask, region, p_locked):
    d = Draft()
    w = base_words(r)
    if mask is None:
        mask = r.random() < 0.4
    w["gLLCC4818"] = r.choice([1, 1, 1, 2, 0x80, 0xFF]) if mask else 0
    if p_locked is None:
        p_locked = r.choice([1.0, 1.0, 1.0, 0.9, 0.5, 0.0])
    if kind == "ring":
        start = ring_scene(r, d, mask, p_locked, False)
    elif kind == "latch":
        start = ring_scene(r, d, mask, p_locked, True)
    elif kind == "polyline":
        start = polyline_scene(r, d, mask, p_locked)
    elif kind == "single":
        start = single_scene(r, d, mask)
    elif kind == "corners":
        start = corners_scene(r, d, mask)
    else:
        start = noise_scene(r, d, mask)
    w["gHoardingRight"], w["gHoardingBottom"] = start
    region_and_strays(r, d, w, region)
    scanned = None
    for _ in range(80):
        try:
            scanned = fill_walk(w, d.t, mask)
            break
        except Unsafe as u:
            wall(r, d, u.cells)
    if scanned is None:
        raise ValueError("walls")
    freed = fill_region(w, d.t)
    decorate_flags(r, d, scanned, freed)
    return base(r, "FillInside", [], d, words=w)


def fill_case(r, kind=None, mask=None, region=None, p_locked=None):
    kind = kind or r.choice(FILL_KINDS)
    for attempt in range(60):
        try:
            return fill_try(r, kind, mask, region, p_locked)
        except ValueError:
            continue
    d = Draft()
    w = base_words(r)
    w["gLLCC4818"] = 0
    lay_path(d, r, 10, 10, ["E", "E", "S", "S"], False, 1.0)
    d.ts(cid_of(10, 14), flags=0x20)
    w["gHoardingRight"], w["gHoardingBottom"] = 10, 10
    w["gLLCCWidth"], w["gLLCCHeight"] = 20, 20
    return base(r, "FillInside", [], d, words=w)


def fill_edges():
    r = random.Random(51)
    out = []
    for kind in ("ring", "latch", "polyline", "single", "corners", "noise"):
        for mask in (False, True):
            for _ in range(30):
                out.append(fill_case(r, kind, mask=mask))
    for p_locked in (0.0, 0.5, 0.9):
        for kind in ("ring", "polyline"):
            for _ in range(10):
                out.append(fill_case(r, kind, p_locked=p_locked))
    for region in ((0, 0), (1, 1), (2, 2), (-3, 5), (5, -3), (0, 9), (9, 0), (3, 40), (40, 3), (30, 30), (129, 129)):
        for _ in range(6):
            out.append(fill_case(r, "ring", region=region))
    for _ in range(150):                                            # tangles of corners: wrong headings and turns on the spot
        out.append(fill_case(r, "corners"))
    for _ in range(60):                                             # rings that reach the top of the level through the gap
        out.append(fill_case(r, "latch"))
    for _ in range(100):
        out.append(fill_case(r))
    return out


def fill_random(r):
    return fill_case(r)


# ---------------------------------------------------------------------------------------------------------------- the edit check

EDIT_KINDS = ["pass"] * 20 + ["gate1", "gate2", "gate3", "gate4"] * 3 + ["lo"] * 3 + ["hi"] * 3 + ["narrow"] * 3 + ["short"] * 3 + ["near"] * 5 + ["wild"] * 3
INTENTS = ["late"] * 6 + ["random"] * 5 + ["bad"] + ["corners2"] * 2 + ["count"] * 2 + ["areahoard"] * 2 + ["fixdecide"] * 2 + ["cost"] * 3 + ["allflagged"] + ["fixgrid"] * 2 + ["corners3"]
WILD = [-1, 0, 1, 2, 3, 5, 60, 125, 126, 127, 128, 129, 255, 1000, 0x7FFFFFFF, -0x80000000, -5, -0x7FFFFFFF]
BAD_LOW = [-1, -2, -7, -100, -0x80000000, -0x7FFFFFFF]
BAD_HIGH = [128, 129, 130, 255, 256, 1000, 0x7FFFFFFF]
DIMS = [3, 3, 3, 4, 4, 5, 5, 6, 7, 8, 9, 10, 12, 14, 16, 20, 24, 30, 40]
OTHER_BYTES = [0x44, 0x11, 0x05, 0x14, 0x41, 0x50, 0x15, 0x55, 0x01, 0x10, 0x04, 0x40, 0x00, 0xFF, 0x13, 0x91, 0x46, 0x0F, 0x31, 0x51]
NOT_HOARDING = [0, 0, 0, 0, 1, 5, 0xC, 0x11, 0x18, 0x1A, 0x119, 0x80000019, 0x10019, 0x1E]
MAP_KINDS = [0, 0, 1, 3, 4, 5, 6, 8, 0x15, 0x19, 0x1D, 0x1F]


def corner_args(r, lox, loy, hix, hiy, order=None):
    """The two corners in one of the four orders (the original takes the minima and maxima)."""
    k = r.randrange(4) if order is None else order
    pairs = [((lox, loy), (hix, hiy)), ((hix, hiy), (lox, loy)), ((lox, hiy), (hix, loy)), ((hix, loy), (lox, hiy))]
    (x1, y1), (x2, y2) = pairs[k]
    return [x1, y1, x2, y2]


def side_cells(lox, loy, hix, hiy):
    """The cells the edge loops visit: (x, y) -> [(side, at an end)], the sides 0 loy row, 1 hiy row, 2 lox column, 3 hix column."""
    cells = {}
    for x in range(lox, hix + 1, 2):
        end = x == lox or x == hix
        cells.setdefault((x, loy), []).append((0, end))
        cells.setdefault((x, hiy), []).append((1, end))
    for y in range(loy, hiy + 1, 2):
        end = y == loy or y == hiy
        cells.setdefault((lox, y), []).append((2, end))
        cells.setdefault((hix, y), []).append((3, end))
    return cells


def edit_profile(r, intent):
    p = dict(ph=[r.choice([0, 0.15, 0.5, 0.9, 1.0]) for _ in range(4)], pstr=[r.choice([0, 0.4, 0.9, 1.0]) for _ in range(4)],
             pflag=r.choice([0, 0.1, 0.5, 0.9, 1.0]), pfree=r.choice([0, 0, 0.05, 0.3, 0.7]), pwater=r.choice([0, 0.1]),
             parea_h=r.choice([0, 0, 0.02, 0.15]), parea_x=r.choice([0, 0, 0.03, 0.15]), corners=r.choice(["free", "free", "none", "all", "one"]))
    if intent == "late":
        p.update(pfree=r.choice([0, 0, 0.02]), pflag=r.choice([0.2, 0.5, 0.8]), corners=r.choice(["none", "all", "all", "free"]))
    elif intent == "bad":
        p.update(pfree=r.choice([0.5, 0.9]))
    elif intent == "corners2":
        p.update(corners="one", pfree=0, ph=[r.choice([0, 0, 0.15]) for _ in range(4)])
    elif intent == "count":
        p.update(pflag=r.choice([0, 1.0]), pfree=0, ph=[r.choice([0, 0, 0.15, 0.5]) for _ in range(4)], corners="none", parea_h=0)
    elif intent == "areahoard":       # nothing outside, so the exit depends on the sides that hold two hoarding cells
        p.update(pflag=0, pfree=0, pwater=0, corners="none", parea_h=0, parea_x=r.choice([0, 0, 0.1]), hcount=[r.choice([0, 1, 2, 3]) for _ in range(4)])
    elif intent == "fixdecide":       # one hoarding corner and sides of other bytes: the fix after the loops decides the two corner exit
        p.update(corners="one", pfree=0, pwater=0, ph=[r.choice([0.5, 0.8, 1.0]) for _ in range(4)], pstr=[r.choice([0, 0, 0.15, 0.3]) for _ in range(4)],
                 pflag=r.choice([0.3, 0.6]), parea_h=0)
    elif intent == "cost":            # a flagged inside and no other reason to leave early
        p.update(pflag=r.choice([0.3, 0.5, 0.8, 1.0]), pfree=0, pwater=0, corners="none", ph=[r.choice([0.3, 1.0]) for _ in range(4)], parea_h=0.1)
    elif intent == "corners3":        # three hoarding corners
        p.update(corners="three", pfree=0, pwater=0, pflag=r.choice([0.3, 0.6]), parea_h=0)
    elif intent == "allflagged":      # the flag everywhere and no hoarding: nothing is inside
        p.update(pflag=1.0, pfree=0, pwater=0, corners="none", ph=[0, 0, 0, 0], parea_h=0, parea_x=r.choice([0, 0, 0.05]))
    elif intent == "fixgrid":         # the number of straight and other cells on each side is chosen, and the byte of the one corner
        p.update(corners="one", pfree=0, pwater=0, pflag=r.choice([0.3, 0.6]), parea_h=0,
                 hspec=[(r.choice([0, 0, 1]), r.choice([0, 1, 1, 2])) for _ in range(4)], cbyte=r.choice([0x00, 0x11, 0x44, 0x05, 0x15, 0x41]))
    return p


def edit_world(r, d, lox, loy, hix, hiy, mask, prof):
    """The cells of the rectangle: hoarding on the sides (straight or not), map kinds for the freeform test, the flag 0x40 over the area, strays."""
    nbf, otf = ("hnb", "nb") if mask else ("nb", "hnb")
    cells = side_cells(lox, loy, hix, hiy)
    corners = [(lox, loy), (hix, loy), (lox, hiy), (hix, hiy)]
    lone = r.choice(corners)
    mode = prof["corners"]
    corner_h = {c: mode == "all" or (mode == "one" and c == lone) or (mode == "three" and c != lone) or (mode == "free" and r.random() < 0.5) for c in corners}
    hoard = {}
    for c in corners:
        hoard[c] = corner_h[c]
    counts, spec = prof.get("hcount"), prof.get("hspec")
    chosen = {}
    if counts is not None:                  # exactly this many hoarding cells among the cells of each side that are no corner
        for si in range(4):
            middle = sorted(c for c, e in cells.items() if e[0][0] == si and c not in hoard)
            chosen.update({c: True for c in r.sample(middle, min(counts[si], len(middle)))})
    if spec is not None:                    # this many cells of the straight byte and of another byte on each side
        for si in range(4):
            middle = sorted(c for c, e in cells.items() if e[0][0] == si and c not in hoard)
            r.shuffle(middle)
            for k, c in enumerate(middle[:spec[si][0] + spec[si][1]]):
                chosen[c] = "straight" if k < spec[si][0] else "other"
    for (x, y), entries in cells.items():
        if (x, y) not in hoard:
            hoard[(x, y)] = chosen.get((x, y), False) if (counts is not None or spec is not None) else (r.random() < prof["ph"][entries[0][0]])
    for (x, y), h in hoard.items():
        cid = cid_of(x, y)
        entries = cells.get((x, y))
        si = entries[0][0] if entries else 0
        if h:
            straight = 0x11 if si < 2 else 0x44
            if h == "straight":
                b = straight
            elif h == "other":
                b = r.choice([v for v in OTHER_BYTES + [r.randrange(256)] if v != straight])
            elif (x, y) in corners and prof.get("cbyte") is not None:
                b = prof["cbyte"]
            else:
                b = straight if r.random() < prof["pstr"][si] else r.choice(OTHER_BYTES + [r.randrange(256)])
            f = {"type": 0x19, nbf: b}
            if r.random() < 0.5:
                f[otf] = r.randrange(256)
            if r.random() < 0.3:
                f["flags"] = r.randrange(0x10000)
            d.ts(cid, **f)
        else:
            t = r.choice(NOT_HOARDING)
            if t:
                d.ts(cid, type=t, **{nbf: r.randrange(256)})
        if entries:
            q = r.random()
            if q < prof["pfree"]:
                mt = r.choice([7, 0x1E])
            elif q < prof["pfree"] + prof["pwater"]:
                mt = 2
            else:
                mt = r.choice(MAP_KINDS)
            if mt:
                d.ms(cid, type=mt)
    hoarding = [cid_of(x, y) for (x, y), h in hoard.items() if h]
    for y in range(loy, hiy + 1):
        for x in range(lox, hix + 1):
            cid = cid_of(x, y)
            if r.random() < prof["pflag"]:
                d.ms(cid, flags=0x40 | (r.randrange(0x10000) & 0xFFBF if r.random() < 0.3 else 0))
            elif r.random() < 0.15:
                d.ms(cid, flags=r.randrange(0x10000) & 0xFFBF)
            if (x, y) in hoard:
                continue
            if r.random() < prof["parea_h"]:
                d.ts(cid, type=0x19, **{nbf: r.randrange(256)})
            elif r.random() < prof["parea_x"]:
                parent = r.choice(hoarding + [cid_of(r.randint(lox, hix), r.randint(loy, hiy)), r.randint(1, NCELL)])
                d.ts(cid, type=r.choice([0xC, 0x11]), parent=parent)


def edit_case(r, kind=None, intent=None, rect=None, order=None, charge=None, gsel=None, hcount=None):
    kind = kind or r.choice(EDIT_KINDS)
    intent = intent or r.choice(INTENTS)
    d = Draft()
    w = base_words(r)
    w["gLLCC4818"] = r.choice([0, 0, 0, 1, 1, 2, 0x80, 0xFF])
    mask = w["gLLCC4818"] != 0
    full = kind in ("pass", "gate1", "gate2", "gate3", "gate4", "near")
    lox = loy = hix = hiy = 0
    if rect is not None or full:
        if rect is None:
            dx, dy = r.choice(DIMS), r.choice(DIMS)
            lox, loy = r.randint(0, 127 - dx), r.randint(0, 127 - dy)
            rect = (lox, loy, lox + dx, loy + dy)
        lox, loy, hix, hiy = rect
        args = corner_args(r, lox, loy, hix, hiy, order)
    elif kind == "lo":
        if r.random() < 0.5:
            xs = [r.choice(BAD_LOW), r.choice(BAD_LOW + [r.randint(0, 127)])] if r.random() < 0.7 else [r.choice(BAD_HIGH), r.choice(BAD_HIGH)]
            ys = [r.randint(-3, 130), r.randint(-3, 130)]
        else:
            ys = [r.choice(BAD_LOW), r.choice(BAD_LOW + [r.randint(0, 127)])] if r.random() < 0.7 else [r.choice(BAD_HIGH), r.choice(BAD_HIGH)]
            xs = [r.randint(-3, 130), r.randint(-3, 130)]
        args = [xs[0], ys[0], xs[1], ys[1]]
    elif kind == "hi":
        a, b = r.randint(0, 120), r.randint(0, 120)
        c, e = r.choice(BAD_HIGH), r.randint(0, 127)
        args = [a, b, c, e] if r.random() < 0.5 else [a, b, e, c]
        if r.random() < 0.5:
            args[0], args[1], args[2], args[3] = args[0], args[1], args[2], args[3]
    elif kind in ("narrow", "short"):
        lox, loy = r.randint(0, 100), r.randint(0, 100)
        small, big = r.choice([0, 1, 2]), r.choice(DIMS + [0, 1, 2])
        dx, dy = (small, big) if kind == "narrow" else (big, small)
        args = corner_args(r, lox, loy, lox + dx, loy + dy, order)
    else:                                  # wild: four values of the boundary pool (never a big valid rectangle)
        for _ in range(6):
            args = [r.choice(WILD) for _ in range(4)]
            xs, ys = sorted([args[0], args[2]]), sorted([args[1], args[3]])
            valid = xs[0] >= 0 and ys[0] >= 0 and xs[1] < 128 and ys[1] < 128
            if not valid or (xs[1] - xs[0] + 1) * (ys[1] - ys[0] + 1) <= 700:
                break
        else:
            args[0] = -1
    if kind in ("gate1", "gate2", "gate3", "gate4", "near"):
        g = r.randrange(4) if kind == "near" else int(kind[-1]) - 1

        def pick(options):                  # gsel: which of the two rows or columns the gate sits on
            return options[gsel] if gsel is not None else r.choice(options)
        if g == 0:
            w["gHoardingRight"], w["gHoardingBottom"] = lox + 2 * r.randint(0, (hix - lox) // 2), pick([loy, hiy])
        elif g == 1:
            w["gHoardingLeft"], w["gHoardingTop"] = lox + 2 * r.randint(0, (hix - lox) // 2), pick([loy, hiy])
        elif g == 2:
            w["gHoardingRight"], w["gHoardingBottom"] = pick([lox, hix]), loy + 2 * r.randint(0, (hiy - loy) // 2)
        else:
            w["gHoardingLeft"], w["gHoardingTop"] = pick([lox, hix]), loy + 2 * r.randint(0, (hiy - loy) // 2)
        if kind == "near":
            names = ("gHoardingRight", "gHoardingBottom") if g in (0, 2) else ("gHoardingLeft", "gHoardingTop")
            n = r.choice(names)
            w[n] += r.choice([-2, -1, 1, 2, 5])
    if full:
        prof = edit_profile(r, intent)
        if hcount is not None:
            prof["hcount"] = hcount
        edit_world(r, d, lox, loy, hix, hiy, mask, prof)
    else:
        for _ in range(r.choice([0, 0, 3, 12])):
            cid = cid_of(r.randint(0, 127), r.randint(0, 127))
            d.ts(cid, type=r.choice([0x19, 0, 0xC]))
            d.ms(cid, flags=r.choice([0, 0x40]), type=r.choice([0, 7, 2]))
    # the answers of the fakes and the cost
    queues = {"SoIsItLegal": [s32(r.choice([1, 1, 1, 1, 0x101, 0x100, 0xFF, 0x80, 0x7FFFFF00, -1, 2, 0, r.randint(-5, 5)]))]}
    charge = r.choice([0, 1, 1]) if charge is None else charge
    cost = r.choice([0, 1, 5, 25, 100, 1000, -3, 0x7FFFFFFF, -0x80000000, r.randint(-50, 5000)])
    w["cost"] = cost
    w["recording"] = r.choice([0, 0, 0, 1, 2, -1, r.randrange(1 << 32) - (1 << 31)])
    n = 0
    if full:
        n = sum(1 for cid, f in d.m.items() if f.get("flags", 0) & 0x40 and lox + 2 <= xy_of(cid)[0] < hix and loy + 2 <= xy_of(cid)[1] < hiy)
    if n > 1500 and w["recording"] == 0:        # two events a cell: the host's log holds 4096 events, so a big area is charged only while recording
        w["recording"] = r.choice([1, 2, -1])
    total = n * cost
    options = [total - 1, total, total + 1, 0, -1, 0x7FFFFFFF, -0x80000000, cost, 2 * cost, r.randint(-100, 5000), total // 2, total * 2]
    steps = [cost * r.randint(0, max(n, 1)) + r.choice([-1, 0, 0, 1]) for _ in range(3)]        # the running total at some call, give or take one
    queues["GetBalance"] = [s32(r.choice(options + steps)) for _ in range(r.choice([1, 1, 2, 3, 4]))]
    return base(r, "EditLegal", args + [charge], d, words=w, queues=queues)


DIM_PAIRS = [(2, 6), (3, 6), (6, 2), (6, 3), (3, 3), (2, 2), (0, 0), (1, 7), (7, 1), (3, 2), (2, 3), (4, 4), (5, 5), (3, 4), (4, 3), (3, 5), (5, 3), (4, 5),
             (5, 4), (6, 6), (7, 7), (8, 8), (2, 40), (40, 2), (1, 1), (2, 0), (0, 2), (6, 0), (0, 6), (3, 0), (0, 3)]


def edit_edges():
    r = random.Random(41)
    out = []
    for kind in ("lo", "hi", "narrow", "short", "gate1", "gate2", "gate3", "gate4", "near", "wild"):
        for _ in range(12):
            out.append(edit_case(r, kind))
    for intent in sorted(set(INTENTS)):
        for _ in range(40):
            out.append(edit_case(r, "pass", intent))
    for kind in ("gate1", "gate2", "gate3", "gate4"):              # every gate on each of its two rows or columns
        for gsel in (0, 1):
            for intent in ("late", "random", "count"):
                for _ in range(3):
                    out.append(edit_case(r, kind, intent, gsel=gsel))
    for dx, dy in DIM_PAIRS:                                        # the size test with the four orders of the corners
        for order in range(4):
            for _ in range(2):
                lox, loy = r.randint(0, 127 - dx), r.randint(0, 127 - dy)
                out.append(edit_case(r, "pass", r.choice(["late", "random", "areahoard", "fixdecide"]), rect=(lox, loy, lox + dx, loy + dy), order=order))
    for subset in range(16):                                        # which sides hold two or more hoarding cells
        for _ in range(8):
            counts = [r.randint(2, 4) if subset >> i & 1 else r.randint(0, 1) for i in range(4)]
            dx, dy = r.choice([6, 7, 8, 10, 12, 16]), r.choice([6, 7, 8, 10, 12, 16])
            lox, loy = r.randint(0, 127 - dx), r.randint(0, 127 - dy)
            out.append(edit_case(r, "pass", "areahoard", rect=(lox, loy, lox + dx, loy + dy), hcount=counts))
    for order in range(4):                                          # the corner fix: one hoarding corner, sides of even length
        for intent, reps in (("fixdecide", 12), ("fixgrid", 150)):
            for _ in range(reps):
                dx, dy = r.choice([4, 6, 8, 10, 12]), r.choice([4, 6, 8, 10, 12])
                lox, loy = r.randint(0, 127 - dx), r.randint(0, 127 - dy)
                out.append(edit_case(r, "pass", intent, rect=(lox, loy, lox + dx, loy + dy), order=order))
    for _ in range(80):                                             # the end cells of the sides: counted once or twice, by the parity of the sides
        dx, dy = r.choice([4, 5, 6, 7, 8, 9]), r.choice([4, 5, 6, 7, 8, 9])
        lox, loy = r.randint(0, 127 - dx), r.randint(0, 127 - dy)
        out.append(edit_case(r, "pass", r.choice(["late", "random", "fixdecide", "corners3", "corners3"]), rect=(lox, loy, lox + dx, loy + dy)))
    for dx, dy in [(3, 3), (4, 3), (3, 4), (4, 4), (5, 3), (3, 5), (5, 4), (4, 5), (5, 5), (6, 3), (3, 6)]:       # a few cells to charge
        for _ in range(8):
            lox, loy = r.randint(0, 127 - dx), r.randint(0, 127 - dy)
            out.append(edit_case(r, "pass", "cost", rect=(lox, loy, lox + dx, loy + dy), charge=1))
    for rect in [(0, 0, 127, 127), (0, 0, 126, 126), (1, 1, 127, 127), (0, 60, 127, 63), (50, 0, 53, 127), (0, 0, 63, 63)]:
        out.append(edit_case(r, "pass", "late", rect=rect, charge=1))
    return out


def edit_random(r):
    return edit_case(r)


# ---------------------------------------------------------------------------------------------------------------- the four cell tests

def cell_ids(r):
    return r.choice([1, 2, 3, 127, 128, 129, 0x1000, 0x2000, NCELL - 1, NCELL, r.randint(1, NCELL)])


def cell_case(fn, r, args, tfields=None, mfields=None, **words):
    d = Draft()
    cid = args[0]
    if tfields is not None:
        d.ts(cid, **tfields)
    if mfields is not None:
        d.ms(cid, **mfields)
    # a few neighbours with other data, to see that nothing else is written
    for k in (-1, 1):
        if 1 <= cid + k <= NCELL and r.random() < 0.5:
            d.ts(cid + k, type=r.choice(KIND_TYPES), flags=r.randrange(0x10000))
            d.ms(cid + k, type=r.choice(KIND_TYPES), flags=r.randrange(0x10000))
    c = base(r, fn, args, d)
    c["words"].update(words)
    return c


FLAG_VALUES = [0, 1, 0x20, 0x40, 0x80, 0x8000, 0x7FFF, 0xFFFF, 0x1234, 0x5555, 0xAAAA, 0xFFDF, 0x21, 0x100]


def flag_edges(fn, which):
    def edges():
        r = random.Random(zlib.crc32(fn.encode()))
        out = []
        for cid in (1, 777, NCELL):
            for old in FLAG_VALUES[:8]:
                for arg in FLAG_VALUES:
                    out.append(cell_case(fn, r, [cid, arg], **{which: dict(flags=old)}))
        return out
    return edges


def flag_random(fn, which):
    def rnd(r):
        cid = cell_ids(r)
        arg = r.choice(FLAG_VALUES + [r.randrange(0x10000), r.randrange(0x10000)])
        return cell_case(fn, r, [cid, arg], **{which: dict(flags=r.choice(FLAG_VALUES + [r.randrange(0x10000)]), type=r.choice(KIND_TYPES))})
    return rnd


def type_edges(fn):
    def edges():
        r = random.Random(zlib.crc32(fn.encode()))
        out = []
        for cid in (1, 5000, NCELL):
            for t in KIND_TYPES:
                out.append(cell_case(fn, r, [cid], mfields=dict(type=t, flags=r.randrange(0x10000))))
        return out
    return edges


def type_random(fn):
    def rnd(r):
        t = r.choice(KIND_TYPES + [r.randrange(1 << 32), r.randrange(0x40)])
        return cell_case(fn, r, [cell_ids(r)], mfields=dict(type=t))
    return rnd


def make_tests():
    tests = []

    def add(name, edge, rnd):
        tests.append(FnTest(name, ppc, host, edge=edge, rnd=rnd))
    add("LLCC_FillInsideOfPark", fill_edges, fill_random)
    add("LLCC_IsHoardingEditLegal", edit_edges, edit_random)
    add("CTrackCell_IsFlag", flag_edges("TrackIsFlag", "tfields"), flag_random("TrackIsFlag", "tfields"))
    add("CMapCell_UnSetFlag", flag_edges("UnSetFlag", "mfields"), flag_random("UnSetFlag", "mfields"))
    add("CMapCell_IsFreeform", type_edges("IsFreeform"), type_random("IsFreeform"))
    add("CMapCell_IsWater", type_edges("IsWater"), type_random("IsWater"))
    return tests


TESTS = make_tests()
