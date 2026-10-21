"""Unit w201, the park's hoarding (the walk, the lock, the redraw, the image of the park and the legality checks) against
libtpw_unit_HoardingImage.dylib (tests/unit_HoardingImage.cpp), main app ("SimTheme Park"): src/game/park/LLCellSystem.cpp (the 12 LLCC functions
of LLCellSystem.cpp 0x100936f4 to 0x10096a44 that are listed below), src/game/park/TrackCell.cpp (CTrackCell::GetFlag, SetFlag, UnSetFlag, IsHoarding,
IsTrackX, IsCorner) and src/game/park/MapCell.cpp (CMapCell::IsFlag): 19 functions.

A case is the cell system's words (the hoarding extents, the click stack count, gLLCC4818 and the rest), the 0x2312 bytes of the two images (the new
image directly followed by the current one), the click stack (filled with a pattern), the world (a raw block of 0x1B0000 bytes from world + 0x2d8: the
16384 map cells of 0x44 bytes, then the 16384 track cells of 0x28 bytes; every cell has its id and kind, the case patches the fields it needs) and
a call. The world is built twice by ONE piece of Python: in emulator memory at the Mac's addresses (big-endian) and in the host library's memory
(little-endian). The original runs for real on the PowerPC side: the 19 functions, CCellBase::GetNeighbours, NumberOfNeighbours,
NumberOfHVNeighbours, GetMapCellEquiv and GetTrackCellEquiv, both CTrackCell::GetNeighbouringCell, MapDelta::operator*, OPPOSITE and
CMap::IsXYValid (the host copies the last three's bodies; the eight compass deltas are written by this module, the Mac builds them at startup).
Every other callee is a fake on both sides that records its arguments: the mouse click LLCM_LeftMouseButtonUp, LLCC_SystemSetMode,
UI_SetGameWindowCursor, LLCC_BuildHoardingDB (answers from a queue), LLCC_FillInsideOfPark, RecalculateParkDarkening, CTRACE and GAME_ASSERTMSG.

What is compared: the answer, the event log, every cell of the world (both blocks), the 0x2312 image bytes, the whole click stack, and every global the
functions can store. Walks (ConstructClickStack, LockHoarding, ReviseHoarding) only run on layouts that a Python model of the walk says end (the walk has
no step limit): polylines, rings with a gap, a long zigzag (more than 0x400 corners, to hit the click stack's overflow), single cells, runs off the map's
edge and noise. FloodFill's depth: see FILL_AREA.

Not tested: a negative click count before the stack overflow, the three bytes after gLLCC47C4 (the overflow writes a whole word on the Mac), cell ids
past 16384 or a parent id outside 1 to 16384, a walk that leaves the world block, images indexed outside the 0x2312 bytes, a fill value of ' ' or '@'
on an open cell (it never ends), additions that overflow 32 bits.
"""
import ctypes as C
import random
import struct
import zlib

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_HoardingImage"
MASK = 0xFFFFFFFF

# ---- the 19 functions: name -> (address, host wrapper, argument count)
FN = dict(
    ConstructClickStack=(0x100936F4, "ConstructClickStack", 0), LockHoarding=(0x10093934, "LockHoarding", 1),
    DrawDefaultHoardings=(0x100956A8, "DrawDefaultHoardings", 0), DrawFromBalanceFile=(0x100957A4, "DrawFromBalanceFile", 1),
    ReviseHoarding=(0x100961C0, "ReviseHoarding", 4), FloodFill=(0x10096398, "FloodFill", 4),
    CreateCurrentImage=(0x1009668C, "CreateCurrentImage", 0), CopyCurrentToNew=(0x10096838, "CopyCurrentToNew", 0),
    AddNewArea=(0x100968D8, "AddNewArea", 4), SoIsItLegal=(0x10096958, "SoIsItLegal", 0),
    IsClearLandLegal=(0x10096A0C, "IsClearLandLegal", 4), IsPatrolAreaLegal=(0x10096A44, "IsPatrolAreaLegal", 4),
    GetFlag=(0x100A2998, "GetFlag", 1), SetFlag=(0x100A2964, "SetFlag", 2), UnSetFlag=(0x100A29CC, "UnSetFlag", 2),
    IsHoarding=(0x100A35DC, "IsHoarding", 1), IsTrackX=(0x100A367C, "IsTrackX", 1), IsCorner=(0x100A37A0, "IsCorner", 1),
    IsFlag=(0x1009BB70, "IsFlag", 2),
)
OWN = {a for a, _, _ in FN.values()}
TRACK_FNS = {"GetFlag", "SetFlag", "UnSetFlag", "IsHoarding", "IsTrackX", "IsCorner"}
MAP_FNS = {"IsFlag"}
ANSWER_MASK = dict(GetFlag=0xFFFF, IsCorner=0xFF, IsPatrolAreaLegal=0xFF, SoIsItLegal=MASK, IsClearLandLegal=MASK, IsHoarding=MASK, IsTrackX=MASK,
                   IsFlag=MASK)

# in-binary callees that run for real: OPPOSITE, CMap::IsXYValid, CCellBase::GetNeighbours, NumberOfNeighbours, NumberOfHVNeighbours, GetMapCellEquiv,
# GetTrackCellEquiv (the non-const forms), both CTrackCell::GetNeighbouringCell (by direction and by delta) and MapDelta::operator*
REAL_OTHER = {0x10102500, 0x101012F4, 0x100816E8, 0x10081744, 0x10081890, 0x100F49A0, 0x100F48C4, 0x1012FDDC, 0x1012FCB8, 0x101037D0}

# ---- the callees faked on both sides
ASSERT, CTRACE = 0x1000DDE8, 0x1006FCBC
LEFT_UP, SET_MODE, CURSOR, BUILD_DB, FILL_INSIDE, DARKEN = 0x10084CA4, 0x10090E18, 0x101A8C58, 0x1008E6E0, 0x10093B0C, 0x1005C59C

# ---- the Mac's addresses of the globals: name -> (address, width)
WORDS = dict(
    gHoardingRight=(0x1029E03C, 4), gHoardingBottom=(0x1029E040, 4), gHoardingLeft=(0x1029E044, 4), gHoardingTop=(0x1029E048, 4),
    gLLCCE0CC=(0x102DE0CC, 1), gLLCC4818=(0x102E4818, 1), gLLCC47C4=(0x102E47C4, 1), gLLCCWidth=(0x102E47FC, 4), gLLCCHeight=(0x102E47F8, 4),
    gLLCC4800=(0x102E4800, 4), gLLCC4804=(0x102E4804, 4), gLLCCClickStackCount=(0x102E47C0, 4), gLLCCReviseAnswer=(0x1029DF30, 4),
)
UNSIGNED = {"gLLCCE0CC", "gLLCC4818", "gLLCC47C4"}
FMT = {1: "B", 2: "H", 4: "I"}
STACK = 0x102E27C0           # gLLCCClickStack: 0x400 entries of two words
IMAGES = 0x102DE198          # the new image (0x1189 bytes), directly followed by the current one (0x102DF321)
IMAGE_BYTES = 0x2312
G = dict(game_world=0x1037EBFC, world_map=0x1037EC08, world=0x1037EC0C)
DELTAS = {0x1034C220: (-1, -1), 0x1034C234: (-1, 0), 0x1034C248: (-1, 1), 0x1034C25C: (0, 1), 0x1034C270: (1, 1), 0x1034C284: (1, 0),
          0x1034C298: (1, -1), 0x1034C2AC: (0, -1)}
HOST_LAYOUT = [0x28, 0x1102D8, 0x10, 0x44, 8, IMAGE_BYTES, 0xE, 0x22]

NCELL = 16384
MAP_BYTES, TRACK_BYTES = 0x44 * NCELL, 0x28 * NCELL
BLOB = MAP_BYTES + TRACK_BYTES          # from world + 0x2d8; the track cells start at +0x110000
FIELDS = dict(kind=(2, "B"), type=(8, "I"), nb=(0xC, "B"), dir=(0xD, "B"), flags=(0xE, "H"), parent=(0x10, "H"), hnb=(0x22, "B"))


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

    S.internal(CTRACE, ctrace)
    S.internal(ASSERT, assert_)
    S.internal(LEFT_UP, R("LeftMouseButtonUp", "iiHi"))
    S.internal(SET_MODE, R("SystemSetMode", "ib"))
    S.internal(CURSOR, R("UI_SetGameWindowCursor", "h"))
    S.internal(BUILD_DB, R("BuildHoardingDB", "iiiibb", ret=lambda e, L: L.ret("BuildHoardingDB", 0) & MASK))
    S.internal(FILL_INSIDE, R("FillInsideOfPark"))
    S.internal(DARKEN, R("RecalculateParkDarkening"))
    return S


STUBS = build_stubs()


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    e.hbi_world = T.persist_alloc(e, 0x1B1000)


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

    def put_stack(self, words):
        self.write(self.gaddr("gLLCCClickStack", STACK), struct.pack(self.order + "I" * len(words), *words))

    def get_stack(self):
        return list(struct.unpack(self.order + "I" * 0x800, self.read(self.gaddr("gLLCCClickStack", STACK), 0x2000)))

    def put_image(self, data):
        self.write(self.gaddr("gLLCCHoardingImages", IMAGES), data)

    def get_image(self):
        return self.read(self.gaddr("gLLCCHoardingImages", IMAGES), IMAGE_BYTES)

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
    """A cell with its id, type, flags and parent in host byte order (the Mac's are big endian)."""
    if order == "<":
        return bytes(cell)
    c = bytearray(cell)
    for off, n in ((0, 2), (8, 4), (0xE, 2), (0x10, 2)):
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
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def gaddr(self, name, mac):
        return mac

    def world(self):
        return self.e.hbi_world

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

    def balance(self, values):
        return self.e.alloc(8 + 4 * 130, b"\0" * 8 + struct.pack(">" + "i" * 130, *values))

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
        lib.tpw_hbi_init()
        CL.host_begin(lib, {}, case["queues"])
        self.cache = {}

    def done(self):
        pass

    def gaddr(self, name, mac):
        if name not in self.cache:
            self.cache[name] = self.lib.tpw_hbi_gaddr(name.encode())
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

    def balance(self, values):
        return (C.c_int32 * 130)(*[s32(v) for v in values])

    def cell(self, fn, cid):
        return cid


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    u64, i32, i64 = C.c_uint64, C.c_int32, C.c_int64
    lib.tpw_hbi_gaddr.restype, lib.tpw_hbi_gaddr.argtypes = u64, [C.c_char_p]
    lib.tpw_hbi_init.argtypes = []
    lib.tpw_hbi_layout.argtypes = [C.POINTER(i64)]
    lib.tpw_cl_label.argtypes = [i32, C.c_void_p]
    for name, (_, wrapper, argc) in FN.items():
        f = getattr(lib, "tpw_hbi_" + wrapper)
        f.restype = i32
        f.argtypes = [C.POINTER(i32)] if name == "DrawFromBalanceFile" else [i32] * argc
    lib.tpw_hbi_init()
    out = (i64 * 32)()
    n = lib.tpw_hbi_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, [hex(v) for v in list(out)[:n]]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the run

def build(s, case):
    """Writes the case's globals and blocks through Side: the same code for both memories (offsets are the Mac's)."""
    s.set_world()
    s.put_world(case)
    for name, v in case["words"].items():
        s.setw(name, v)
    s.put_image(case["image"])
    salt = case["salt"]
    s.put_stack([((i * 0x9E3779B1) + salt) & MASK for i in range(0x800)])


def execute(s, case):
    name, args = case["fn"], list(case["args"])
    address, wrapper, _ = FN[name]
    if name == "DrawFromBalanceFile":
        args = [s.balance(case["bal"])]
    elif name in TRACK_FNS or name in MAP_FNS:
        args[0] = s.cell(name, args[0])
    if isinstance(s, Ppc):
        r = s.call(address, *args)
    else:
        r = getattr(s.lib, "tpw_hbi_" + wrapper)(*[a if name == "DrawFromBalanceFile" else s32(a) for a in args])
    mask = ANSWER_MASK.get(name)
    return None if mask is None else (r & mask)


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides."""
    out = {"answer": answer, "log": s.log(), "words": {n: s.getw(n) for n in WORDS}}
    out["stack"] = s.get_stack()
    out["image"] = s.get_image()
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
TYPES = [0x19, 0x18, 0x1A, 0, 1, 2, 5, 7, 0xB, 0xC, 0xD, 0x10, 0x11, 0x12, 0x1E, 0x119, 0x80000019, 0x7FFFFFFF, 0xFFFFFFFF, 0x1C, 0x20]
FILL_AREA = 66        # the whole grid: a 66 by 66 open area and a snake of 2211 cells fill without trouble on the emulator's stack (measured)


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


def rbytes(r, n, choices):
    return bytes(r.choice(choices) for _ in range(n))


def rnd_image(r):
    """A mixed image (no pattern that makes the fill run long)."""
    mode = r.random()
    if mode < 0.2:
        return bytes(r.randrange(256) for _ in range(IMAGE_BYTES))
    return rbytes(r, IMAGE_BYTES, [0x20, 0x40, 0x23, 0x23, 0x23, 0x23, 0x41, 0x00, 0xFF, 0x7E])


def base_words(r):
    w = dict(gHoardingRight=r.randint(0, 127), gHoardingBottom=r.randint(0, 127), gHoardingLeft=r.randint(0, 127), gHoardingTop=r.randint(0, 127))
    w["gLLCCE0CC"] = r.choice([0, 1, 4, 0x80, 0xFF, r.randrange(256)])
    w["gLLCC4818"] = r.choice([0, 0, 1, 1, 2, 0xFF])
    w["gLLCC47C4"] = r.randrange(256)
    w["gLLCCWidth"] = r.choice([0, 1, 2, 3, 0x10, 0x40, 0x7F, 0x80, r.randrange(0, 0x85)])
    w["gLLCCHeight"] = r.choice([0, 1, 2, 3, 0x10, 0x40, 0x7F, 0x80, r.randrange(0, 0x85)])
    w["gLLCC4800"] = r.choice(SMALL + [r.randrange(1 << 32) - (1 << 31)])
    w["gLLCC4804"] = r.choice(SMALL + [r.randrange(1 << 32) - (1 << 31)])
    w["gLLCCClickStackCount"] = r.choice([0, 1, 2, 5, 0x3FE, 0x3FF, 0x400, 0x401, -1, r.randrange(0x400)])
    w["gLLCCReviseAnswer"] = r.choice(SMALL + [r.randrange(1 << 32) - (1 << 31)])
    return w


def base(r, fn, args=(), draft=None, **over):
    d = draft or Draft()
    case = dict(fn=fn, args=list(args), words=base_words(r), image=rnd_image(r), salt=r.randrange(1 << 32), tracks=d.t, maps=d.m, queues={},
                bal=[0] * 130)
    case.update(over)
    return case


# ---- the walk: a Python model that says whether the walk ends (the original has no step limit)

SINGLE = {1: (0, -1), 2: (1, -1), 4: (1, 0), 8: (1, 1), 0x10: (0, 1), 0x20: (-1, 1), 0x40: (-1, 0), 0x80: (-1, -1)}


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
    return (nb & 1 and nb & 4) or (nb & 1 and nb & 0x40) or (nb & 0x10 and nb & 4) or (nb & 0x10 and nb & 0x40)


def cell_field(tracks, cid, name):
    return tracks.get(cid, {}).get(name, 0)


def walk_model(w, tracks, mask, limit):
    """Number of iterations the walk makes, or None when it does not end within `limit` iterations or leaves the world block."""
    x, y = w["gHoardingRight"], w["gHoardingBottom"]
    d, steps = 4, 0
    while steps < limit:
        cid = (x + (y << 7) + 1) & 0xFFFF
        if not 1 <= cid <= NCELL:
            return None
        nb = cell_field(tracks, cid, "hnb" if mask else "nb")
        if cell_field(tracks, cid, "type") == 0x19 and is_corner(nb):
            d = opposite(d) ^ nb
        stop = False
        if d in SINGLE:
            i = cid - 1
            nx, ny = (i & 0x7F) + 2 * SINGLE[d][0], (i >> 7) + 2 * SINGLE[d][1]
            if 0 <= nx < 128 and 0 <= ny < 128 and cell_field(tracks, cid_of(nx, ny), "type") != 0x19:
                stop = True
        if popcount(nb) == 1 and steps != 0:
            stop = True
        steps += 1
        if stop:
            return steps
        if d == 0x10:
            y += 2
        elif d == 4:
            x += 2
        elif d == 1:
            y -= 2
        elif d == 0x40:
            x -= 2
    return None


DIRB = {"N": (1, 0, -1), "E": (4, 1, 0), "S": (0x10, 0, 1), "W": (0x40, -1, 0)}
BACKBIT = {1: 0x10, 4: 0x40, 0x10: 1, 0x40: 4}


def lay_path(d, r, sx, sy, moves, mask, noise=True):
    """Hoarding cells at the start and after each move of two cells; each cell's neighbour bits (in the byte the walk reads) point to the previous
    and next cell, the other byte holds noise."""
    pts, bits = [(sx, sy)], [0]
    x, y = sx, sy
    for m in moves:
        bit, dx, dy = DIRB[m]
        bits[-1] |= bit
        x += 2 * dx
        y += 2 * dy
        pts.append((x, y))
        bits.append(BACKBIT[bit])
    for (x, y), b in zip(pts, bits):
        if not (0 <= x < 128 and 0 <= y < 128):
            raise ValueError
        read, other = ("hnb", "nb") if mask else ("nb", "hnb")
        f = {"type": 0x19, read: b}
        if noise:
            f[other] = r.randrange(256)
            if r.random() < 0.5:
                f["flags"] = r.choice([0x20, 0xFFDF, 0xFFFF, 0x8000, r.randrange(0x10000)])
        d.ts(cid_of(x, y), **f)
    return pts


def random_moves(r, sx, sy, n, first_east=True):
    visited = {(sx, sy)}
    x, y = sx, sy
    moves, last = [], None
    for k in range(n):
        options = [m for m in "NESW" if (last is None or BACKBIT[DIRB[m][0]] != DIRB[last][0])]
        r.shuffle(options)
        if k == 0 and first_east and r.random() < 0.85:
            options.insert(0, "E")
        for m in options:
            nx, ny = x + 2 * DIRB[m][1], y + 2 * DIRB[m][2]
            if 0 <= nx < 128 and 0 <= ny < 128 and (nx, ny) not in visited:
                moves.append(m)
                visited.add((nx, ny))
                x, y, last = nx, ny, m
                break
        else:
            break
    return moves


def ring_moves(r):
    a, b, g = r.randint(0, 6), r.randint(0, 6), r.randint(0, 3)
    h, w = r.randint(1, 10), a + b + 1 + g
    return ["E"] * a + ["S"] * h + ["W"] * w + ["N"] * h + ["E"] * b, 2 * (w - a) + 2


def zigzag_moves(columns):
    """A sawtooth of corners down and up the map: 124 corners a column pair."""
    moves = []
    down = True
    for c in range(columns):
        v = "S" if down else "N"
        for _ in range(31):
            moves += ["E", v, "W", v]
        down = not down
        if c != columns - 1:
            moves += ["E", "E"]
    return moves


def walk_layout(r, w, d, mask, kind):
    """Fills the draft with one walk layout and sets the start (right, bottom) in `w`; returns the iteration limit."""
    limit = 500
    if kind == "polyline":
        sx, sy = r.randint(0, 127), r.randint(0, 127)
        moves = random_moves(r, sx, sy, r.randint(0, 40))
        pts = lay_path(d, r, sx, sy, moves, mask)
        if len(pts) > 3 and r.random() < 0.3:
            x, y = r.choice(pts[1:-1])       # a gap or another kind in the middle
            d.ts(cid_of(x, y), type=r.choice(TYPES))
    elif kind == "ring":
        moves, lead = ring_moves(r)
        sx = r.randint(0, 127)
        sy = r.randint(0, 127)
        sx = max(sx, 2 * r.randint(0, 6))
        pts = lay_path(d, r, sx, sy, moves, mask)
    elif kind == "zigzag":
        moves = zigzag_moves(r.choice([1, 2, 3, 9, 10, 12, 17]))
        sx, sy = 0, 0
        pts = lay_path(d, r, 0, 0, moves, mask)
        limit = 4000
    elif kind == "single":
        sx, sy = r.randint(0, 127), r.randint(0, 127)
        d.ts(cid_of(sx, sy), type=r.choice(TYPES), **{("hnb" if mask else "nb"): r.choice([0, 1, 4, 0x10, 0x40, 5, 0x41, 0x14, 0x50, 0x45, 0xFF, r.randrange(256)])})
    elif kind == "corners":      # a grid of hoarding cells (two apart) whose bits are all corners and ends: wrong headings, asserts, turns in place
        sx, sy = r.randint(0, 120), r.randint(0, 120)
        bits = [0x05, 0x11, 0x14, 0x41, 0x44, 0x50, 0x15, 0x45, 0x01, 0x04, 0x10, 0x40, 0x54, 0x55, 0x00, 0x14, 0x41]
        for gy in range(-3, 4):
            for gx in range(-3, 4):
                x, y = sx + 2 * gx, sy + 2 * gy
                if 0 <= x < 128 and 0 <= y < 128 and r.random() < 0.8:
                    f = {"type": 0x19, ("hnb" if mask else "nb"): r.choice(bits), ("nb" if mask else "hnb"): r.randrange(256)}
                    d.ts(cid_of(x, y), **f)
        d.ts(cid_of(sx, sy), type=0x19, **{("hnb" if mask else "nb"): r.choice(bits)})
    elif kind == "edge":
        sx, sy = r.choice([126, 127, 125]), r.randint(0, 120)
        for k in range(r.randint(1, 4)):
            x = sx + 2 * k - 2
            if 0 <= x < 128:
                d.ts(cid_of(x, sy), type=0x19, **{("hnb" if mask else "nb"): r.choice([0x44, 0x04, 0x40, r.randrange(256)])})
        d.ts(cid_of(sx, sy), type=r.choice([0x19, 0x19, 0]), **{("hnb" if mask else "nb"): r.choice([0x04, 0x40, 0x44, 0x14, 0x41])})
    else:   # noise around the start
        sx, sy = r.randint(0, 127), r.randint(0, 127)
        p = r.choice([0.15, 0.4, 0.7])
        nbs = [0, 1, 4, 0x10, 0x40, 5, 0x41, 0x14, 0x50, 0x45, 0x15, 0x55, 0xFF, 0x44, 0x11, r.randrange(256)]
        for dy in range(-6, 7):
            for dx in range(-6, 7):
                x, y = sx + dx, sy + dy
                if 0 <= x < 128 and 0 <= y < 128 and r.random() < p:
                    d.ts(cid_of(x, y), type=r.choice([0x19] * 6 + TYPES), **{"hnb" if mask else "nb": r.choice(nbs), "nb" if mask else "hnb": r.randrange(256)})
        d.ts(cid_of(sx, sy), type=r.choice([0x19, 0x19, 0x19, 0]))
    w["gHoardingRight"], w["gHoardingBottom"] = sx, sy
    return limit


WALK_KINDS = ["polyline"] * 4 + ["ring"] * 3 + ["corners"] * 2 + ["single", "edge", "noise", "noise", "zigzag"]


def walk_case(r, fn, args=(), kind=None, mask=None, **over):
    """A case whose walk the model says ends (fallback: a straight run)."""
    wanted = r.choice([1, 1, 3, 5, 10, 20])        # the walk's length in iterations: a few long ones, some that stop at once
    for attempt in range(80):
        d = Draft()
        w = base_words(r)
        m = (w["gLLCC4818"] != 0) if mask is None else mask
        k = kind or r.choice(WALK_KINDS[:-1] if r.random() < 0.97 else WALK_KINDS)
        try:
            limit = walk_layout(r, w, d, m, k)
        except ValueError:
            continue
        steps = walk_model(w, d.t, m, limit)
        if steps is not None and (steps >= wanted or attempt > 50 or k in ("single", "edge")):
            c = base(r, fn, args, d, **dict(over, words=dict(w, **over.get("words", {}))))
            c["steps"] = steps
            return c
    d = Draft()
    w = base_words(r)
    lay_path(d, r, 10, 10, ["E", "E", "S"], bool(w["gLLCC4818"]))
    w["gHoardingRight"], w["gHoardingBottom"] = 10, 10
    return base(r, fn, args, d, **dict(over, words=w))


# ---- cell scatter (patrol area, current image)

def scatter(r, d, x0, y0, x1, y1, p_hoard, p_x, p_flag, p_cell=0.6):
    xs = [(x, y) for y in range(max(y0, 0), min(y1, 127) + 1) for x in range(max(x0, 0), min(x1, 127) + 1)]
    ids = [cid_of(x, y) for x, y in xs]
    for (x, y), cid in zip(xs, ids):
        if r.random() > p_cell:
            continue
        q = r.random()
        if q < p_hoard:
            d.ts(cid, type=0x19)
        elif q < p_hoard + p_x:
            d.ts(cid, type=r.choice([0xC, 0x11]), parent=r.choice(ids + [r.randint(1, NCELL)]))
        else:
            d.ts(cid, type=r.choice([0, 1, 2, 5, 0x1E, 0x18, 0x1A, 0xB]))
        if r.random() < p_flag:
            d.ms(cid, flags=r.choice([0x40, 0xFFFF, 0x41, 0x8040, r.randrange(0x10000)]))
        elif r.random() < 0.3:
            d.ms(cid, flags=r.choice([0, 0x3F, 0x80, 0x20, r.randrange(0x10000) & ~0x40]))


# ---------------------------------------------------------------------------------------------------------------- per function

def rnd_int(r):
    return r.choice(SMALL + [r.randint(-300, 300), r.randrange(1 << 32) - (1 << 31), r.randint(-20, 140)])


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
            d.ts(cid + k, type=r.choice(TYPES), flags=r.randrange(0x10000))
            d.ms(cid + k, flags=r.randrange(0x10000))
    c = base(r, fn, args, d)
    c["words"].update(words)
    return c


FLAG_VALUES = [0, 1, 0x20, 0x40, 0x80, 0x8000, 0x7FFF, 0xFFFF, 0x1234, 0x5555, 0xAAAA, 0xFFDF, 0x21, 0x100]


def flag_edges(fn):
    def edges():
        r = random.Random(zlib.crc32(fn.encode()))
        out = []
        for cid in (1, 777, NCELL):
            for old in FLAG_VALUES[:8]:
                for arg in FLAG_VALUES:
                    if fn == "GetFlag" and arg != 0:
                        continue
                    args = [cid] if fn == "GetFlag" else [cid, arg]
                    out.append(cell_case(fn, r, args, tfields=dict(flags=old)))
        return out
    return edges


def flag_random(fn):
    def rnd(r):
        cid = cell_ids(r)
        args = [cid] if fn == "GetFlag" else [cid, r.choice(FLAG_VALUES + [r.randrange(0x10000), r.randrange(0x10000)])]
        return cell_case(fn, r, args, tfields=dict(flags=r.choice(FLAG_VALUES + [r.randrange(0x10000)]), type=r.choice(TYPES)))
    return rnd


def type_edges(fn, values):
    def edges():
        r = random.Random(zlib.crc32(fn.encode()))
        out = []
        for cid in (1, 5000, NCELL):
            for t in values:
                out.append(cell_case(fn, r, [cid], tfields=dict(type=t, flags=r.randrange(0x10000))))
        return out
    return edges


def type_random(fn):
    def rnd(r):
        t = r.choice(TYPES + [r.randrange(1 << 32), r.randrange(0x40)])
        return cell_case(fn, r, [cell_ids(r)], tfields=dict(type=t))
    return rnd


def corner_edges():
    r = random.Random(21)
    out = []
    for nb in range(256):
        for masked in (0, 1):
            cid = cell_ids(r)
            f = dict(type=0x19, kind=2, nb=nb, hnb=(nb if r.random() < 0.3 else r.randrange(256)))
            if masked:
                f["nb"], f["hnb"] = f["hnb"], nb
            out.append(cell_case("IsCorner", r, [cid], tfields=f, gLLCC4818=masked))
    return out


def corner_random(r):
    nb = r.choice([0x41, 0x14, 0x05, 0x50, 0x11, 0x44, 0x45, 0x55, 0x01, 0x15, r.randrange(256), r.randrange(256)])
    f = dict(kind=r.choice([2, 2, 2, 1, 0, 3]), nb=nb, hnb=r.randrange(256), type=r.choice(TYPES))
    if r.random() < 0.5:
        f["nb"], f["hnb"] = f["hnb"], f["nb"]
    return cell_case("IsCorner", r, [cell_ids(r)], tfields=f, gLLCC4818=r.choice([0, 1, 2, 0xFF]))


def isflag_edges():
    r = random.Random(22)
    out = []
    for cid in (1, 4000, NCELL):
        for mf in (0, 0x40, 0xFFFF, 0x3F, 0x8000, 0xFFBF):
            for flag in (0x40, 0, 1, 0xFFFF, 0x8000, 0x3F, 0xFFBF, 0x80):
                out.append(cell_case("IsFlag", r, [cid, flag], mfields=dict(flags=mf)))
    return out


def isflag_random(r):
    return cell_case("IsFlag", r, [cell_ids(r), r.choice(FLAG_VALUES + [r.randrange(0x10000)])],
                     mfields=dict(flags=r.choice(FLAG_VALUES + [r.randrange(0x10000)])))


def clear_land_edges():
    r = random.Random(23)
    return [base(r, "IsClearLandLegal", [a, b, c, d]) for a in SMALL[:6] for b in SMALL[:4] for c in SMALL[:3] for d in SMALL[:2]][:40]


def clear_land_random(r):
    return base(r, "IsClearLandLegal", [rnd_int(r) for _ in range(4)])


def walk_edges(fn, mkargs):
    def edges():
        r = random.Random(zlib.crc32(fn.encode()))
        out = []
        for kind in ("polyline", "ring", "corners", "single", "edge", "noise"):
            for mask in (False, True):
                for _ in range(8):
                    out.append(walk_case(r, fn, mkargs(r), kind=kind, mask=mask, words=dict(gLLCC4818=int(mask))))
        for cols in (1, 2, 9, 10, 17):   # the zigzag: 1 column pair is short, 9 and more overflow the click stack
            d = Draft()
            w = base_words(r)
            lay_path(d, r, 0, 0, zigzag_moves(cols), False)
            w["gHoardingRight"], w["gHoardingBottom"], w["gLLCC4818"] = 0, 0, 0
            assert walk_model(w, d.t, False, 6000) is not None
            out.append(base(r, fn, mkargs(r), d, words=w))
        return out
    return edges


def walk_random(fn, mkargs):
    def rnd(r):
        return walk_case(r, fn, mkargs(r))
    return rnd


def lock_args(r):
    return [r.choice([0, 1])]          # a bool: the caller passes 0 or 1


def revise_args(r):
    a = r.choice([0, 1, 2, 5, 10, 50, 100, 127, 128, -1, -5, 0x7FFFFFFF, -0x80000000, r.randint(-300, 300), rnd_int(r)])
    b = r.choice([0, 1, 2, 5, 10, 50, 100, 127, 128, -1, -5, 0x7FFFFFFF, -0x80000000, r.randint(-300, 300), rnd_int(r)])
    c = a if r.random() < 0.15 else rnd_int(r)
    dd = b if r.random() < 0.15 else rnd_int(r)
    if r.random() < 0.3:
        c, dd = r.randint(0, 127), r.randint(0, 127)
    return [a, b, c, dd]


def revise_case(r, kind=None):
    c = walk_case(r, "ReviseHoarding", revise_args(r), kind=kind, mask=True)
    c["queues"] = {"BuildHoardingDB": [r.choice([0, 1, -1, 0x7FFFFFFF, r.randrange(1 << 32) - (1 << 31)])]}
    return c


def revise_edges():
    r = random.Random(31)
    out = []
    for kind in ("polyline", "ring", "corners", "single", "edge", "noise"):
        for _ in range(12):
            out.append(revise_case(r, kind))
    for a, b, c, dd in [(5, 5, 5, 5), (0, 0, 0, 0), (9, 3, 3, 9), (3, 9, 9, 3), (-4, -4, 4, 4), (4, 4, -4, -4), (7, 2, 7, 9), (7, 2, 9, 2),
                        (0x7FFFFFFF, 0x7FFFFFFF, -0x80000000, -0x80000000), (-0x80000000, 0x7FFFFFFF, 0x7FFFFFFF, -0x80000000)]:
        cs = revise_case(r, "ring")
        cs["args"] = [a, b, c, dd]
        out.append(cs)
    return out


def default_hoard_case(r):
    c = base(r, "DrawDefaultHoardings")
    c["words"]["gHoardingRight"] = r.choice([0, 1, -1, 5, 127, -10, 0x3FFFFFFF, -0x3FFFFFFF, 0x7FFFFF00, -0x7FFFFF00, r.randint(-300, 300)])
    c["words"]["gHoardingBottom"] = r.choice([0, 1, -1, 5, 127, -10, 0x3FFFFFFF, -0x3FFFFFFF, 0x7FFFFF00, -0x7FFFFF00, r.randint(-300, 300)])
    return c


def balance_case(r, term=-2):
    vals = [r.choice([r.randint(-300, 300), r.randint(-40, 40), 0, 0x200, 0x1FF, 0x201, -0x200, 0x100000, -0x100000]) for _ in range(130)]
    if term == -2:
        term = r.choice([None, 0, 1, 2, 3, 10, 30, 62, 63, 64, 65, r.randint(0, 64)])
    for i in range(0, 65):
        if vals[1 + 2 * i] == 0x200:
            vals[1 + 2 * i] = 0x1FF           # no terminator unless the case sets one
    if term is not None and term <= 64:
        vals[1 + 2 * term] = 0x200
    if r.random() < 0.3:
        for i in range(0, 65):
            if r.random() < 0.2:
                vals[2 + 2 * i if 2 + 2 * i < 130 else 129] = 0x200      # 0x200 in a y slot does not end the list
    c = base(r, "DrawFromBalanceFile", [0])
    c["bal"] = vals
    c["words"]["gHoardingRight"] = r.choice([0, 5, -9, 100, 0x1000000, -0x1000000, r.randint(-300, 300)])
    c["words"]["gHoardingBottom"] = r.choice([0, 5, -9, 100, 0x1000000, -0x1000000, r.randint(-300, 300)])
    return c


def balance_edges():
    r = random.Random(41)
    return [balance_case(r, t) for t in (None, 0, 1, 2, 3, 62, 63, 64, 65) for _ in range(4)]


# ---- the images

def room_image(r, rooms, big=False):
    """'#' with open rooms (' ' and '@') carved in: the fill runs through connected rooms only."""
    img = bytearray(b"#" * IMAGE_BYTES)
    for _ in range(rooms):
        w, h = r.choice([r.randint(1, 8), r.randint(1, FILL_AREA)]), r.choice([r.randint(1, 8), r.randint(1, FILL_AREA)])
        x0, y0 = r.randint(0, 0x41 - 1), r.randint(0, 0x41 - 1)
        for x in range(x0, min(x0 + w, 0x42)):
            for y in range(y0, min(y0 + h, 0x42)):
                img[x * 0x43 + y] = r.choice([0x20, 0x20, 0x20, 0x40])
    for _ in range(r.randint(0, 40)):
        img[r.randrange(IMAGE_BYTES)] = r.choice([0x20, 0x40, 0x23, 0x41, 0x00, 0xFF])
    return bytes(img)


def fill_args(r, img):
    """Start cell: inside an open cell most of the time, else anywhere (including off the grid)."""
    opens = [i for i in range(0x42 * 0x43) if img[i] in (0x20, 0x40) and (i % 0x43) < 0x42]
    q = r.random()
    if q < 0.75 and opens:
        i = r.choice(opens)
        x, y = i // 0x43, i % 0x43
    elif q < 0.9:
        x, y = r.randint(-2, 0x44), r.randint(-2, 0x44)
    else:
        x, y = r.choice([0, 1, 0x40, 0x41, 0x42, -1]), r.choice([0, 1, 0x40, 0x41, 0x42, -1])
    fill = r.choice([0x23, 0x23, 0x41, 0x00, 0xFF, 0x7E, 0x30, 0x21])
    old = r.choice([0x20, 0x20, 0x00, 0xFF, 0x40, 0x23])
    return [x, y, old, fill]


def fill_case(r):
    img = room_image(r, r.randint(0, 6))
    return base(r, "FloodFill", fill_args(r, img), image=img)


def fill_edges():
    r = random.Random(51)
    out = []
    # a start on a closed cell or off the grid, the corners, a fill value of ' ' on a closed cell, a full row and a full column of open cells
    for x, y in [(-1, 0), (0, -1), (0x42, 0), (0, 0x42), (0x41, 0x41), (0, 0), (0x41, 0), (0, 0x41), (5, 5), (0x7FFFFFFF, 0), (0, -0x80000000)]:
        for fill in (0x23, 0x20, 0x40, 0x00):
            img = bytearray(room_image(r, 3))
            if fill in (0x20, 0x40) and 0 <= x < 0x42 and 0 <= y < 0x42:
                img[x * 0x43 + y] = 0x23      # a fill of ' ' or '@' on an open cell would never end
            elif (x, y) == (5, 5):
                img[5 * 0x43 + 5] = 0x23
            out.append(base(r, "FloodFill", [x, y, 0x20, fill], image=bytes(img)))
    for kind in range(6):
        img = bytearray(b"#" * IMAGE_BYTES)
        if kind == 0:
            for y in range(0x42):
                img[7 * 0x43 + y] = 0x20          # a full row (x = 7)
        elif kind == 1:
            for x in range(0x42):
                img[x * 0x43 + 9] = 0x40          # a full column
        elif kind == 2:
            for x in range(1, FILL_AREA + 1):
                for y in range(1, FILL_AREA + 1):
                    img[x * 0x43 + y] = 0x20      # a square room, filled from its corner
        elif kind == 3:
            for x in range(0x42):
                for y in range(0x42):
                    if (x + y) % 2 == 0:
                        img[x * 0x43 + y] = 0x20  # a checkerboard: no two open cells touch
        elif kind == 4:
            for x in range(0, 0x42, 2):
                for y in range(0x42):
                    img[x * 0x43 + y] = 0x40      # stripes of rows
        else:
            for x in range(FILL_AREA):
                for y in range(FILL_AREA):
                    img[x * 0x43 + y] = 0x20 if (x * 7 + y * 3) % 5 else 0x23
        start = [(7, 3), (3, 9), (1, 1), (0, 0), (0, 0), (2, 2)][kind]
        out.append(base(r, "FloodFill", [start[0], start[1], 0x20, 0x23], image=bytes(img)))
    img = bytearray(b"#" * IMAGE_BYTES)                  # the whole grid open, filled from several starts
    for x in range(0x42):
        for y in range(0x42):
            img[x * 0x43 + y] = r.choice([0x20, 0x40])
    for start in ((0, 0), (0x41, 0x41), (33, 33), (0, 0x41)):
        out.append(base(r, "FloodFill", [start[0], start[1], 0x20, 0x23], image=bytes(img)))
    for step in (2, 4, 8):                               # a snake of one cell wide corridors: the deepest recursion (2211 cells at step 2)
        img = bytearray(b"#" * IMAGE_BYTES)
        for x in range(0x42):
            if x % step == 0:
                for y in range(0x42):
                    img[x * 0x43 + y] = 0x20
            else:
                img[x * 0x43 + (0x41 if (x // step) % 2 else 0)] = 0x20
        out.append(base(r, "FloodFill", [0, 0, 0x20, 0x23], image=bytes(img)))
        out.append(base(r, "FloodFill", [0x41, 0x41 if step == 2 else 0, 0x20, 0x41], image=bytes(img)))
    return out


def current_image_case(r):
    d = Draft()
    w = base_words(r)
    big = r.random() < 0.15
    w["gLLCCWidth"] = r.choice([0, 1, 2, 3, 5, 8, 12, 20, r.randint(0, 24)]) if not big else r.choice([0x7F, 0x80, 0x84, 0x70, r.randint(0x40, 0x84)])
    w["gLLCCHeight"] = r.choice([0, 1, 2, 3, 5, 8, 12, 20, r.randint(0, 24)]) if not big else r.choice([0x7F, 0x80, 0x84, 0x70, r.randint(0x40, 0x84)])
    width, height = w["gLLCCWidth"], w["gLLCCHeight"]
    scatter(r, d, 0, 0, max(width - 1, 0), max(height - 1, 0), r.choice([0.1, 0.3, 0.5]), r.choice([0, 0.1, 0.3]), r.choice([0, 0.2, 0.5]),
            p_cell=r.choice([0.3, 0.7, 1.0]) if not big else 0.3)
    return base(r, "CreateCurrentImage", [], d, words=w)


def patrol_case(r):
    d = Draft()
    x1, y1 = r.randint(-3, 128), r.randint(-3, 128)
    big = r.random() < 0.08
    ex = r.choice([0, 1, 2, 3, 5, 8, 12]) if not big else r.randint(30, 127)
    ey = r.choice([0, 1, 2, 3, 5, 8, 12]) if not big else r.randint(30, 127)
    x2, y2 = x1 + ex, y1 + ey
    if r.random() < 0.12:
        x2 = x1 - r.randint(1, 4)
    if r.random() < 0.12:
        y2 = y1 - r.randint(1, 4)
    q = r.random()
    p_h, p_x, p_f = (1.0, 0, 0) if q < 0.1 else (0, 0, 0) if q < 0.2 else (r.choice([0.3, 0.6, 0.9]), r.choice([0, 0.1, 0.4]), r.choice([0, 0.2, 0.6]))
    scatter(r, d, min(x1, x2), min(y1, y2), max(x1, x2), max(y1, y2), p_h, p_x, p_f, p_cell=1.0 if q < 0.2 else r.choice([0.5, 0.9, 1.0]))
    return base(r, "IsPatrolAreaLegal", [x1, y1, x2, y2], d)


def patrol_edges():
    r = random.Random(61)
    out = [patrol_case(r) for _ in range(60)]
    for x1, y1, x2, y2 in [(0, 0, 0, 0), (127, 127, 127, 127), (-1, -1, 0, 0), (126, 126, 128, 128), (3, 3, 2, 3), (3, 3, 3, 2), (5, 5, 4, 4), (0, 0, 127, 0),
                           (0, 0, 0, 127), (-2, 5, 130, 5), (60, 60, 61, 61)]:
        for p_h, p_f in ((1.0, 0.0), (0.0, 0.0), (0.0, 1.0), (0.5, 0.2)):
            d = Draft()
            scatter(r, d, max(min(x1, x2), 0), max(min(y1, y2), 0), min(max(x1, x2), 127), min(max(y1, y2), 127), p_h, 0.1, p_f, p_cell=1.0)
            out.append(base(r, "IsPatrolAreaLegal", [x1, y1, x2, y2], d))
    return out


def add_area_case(r):
    x1, y1 = r.randint(-1, 70), r.randint(-1, 70)
    x2, y2 = x1 + r.randint(-3, 40), y1 + r.randint(-3, 40)
    if r.random() < 0.5:
        x2, y2 = min(x2, 126), min(y2, 126)
    while (x2 + 1 >> 1) * 0x43 + (y2 + 1 >> 1) >= IMAGE_BYTES or (x2 + 1 >> 1) < 0 or (y2 + 1 >> 1) < 0:
        x2, y2 = x2 // 2, y2 // 2
        if x2 < x1 and y2 < y1:
            break
    return base(r, "AddNewArea", [x1, y1, x2, y2])


def add_area_edges():
    r = random.Random(71)
    out = []
    for x1, y1, x2, y2 in [(0, 0, 0, 0), (-1, -1, 0, 0), (0, 0, 1, 1), (0, 0, 2, 2), (1, 1, 2, 2), (3, 3, 2, 2), (4, 4, 9, 9), (-1, -1, 125, 125),
                           (0, 0, 126, 126), (10, 10, 9, 20), (10, 10, 20, 9), (-1, 0, 5, 0), (0, -1, 0, 5), (2, 2, 3, 3), (63, 63, 126, 126),
                           (100, 20, 125, 40), (-1, -1, -1, -1), (0, 0, -1, -1)]:
        out.append(base(r, "AddNewArea", [x1, y1, x2, y2]))
    out += [add_area_case(r) for _ in range(40)]
    return out


def legal_image(r):
    """The new image as CopyCurrentToNewImage leaves it: a ring of ' ' round the grid, a park of '#' inside with pockets."""
    img = bytearray(rbytes(r, IMAGE_BYTES, [0x23]))
    for i in range(0x42):
        for idx in (i, i + 0x1103, i * 0x43, i * 0x43 + 0x41):
            img[idx] = 0x20
    q = r.random()
    if q < 0.6:
        for _ in range(r.randint(0, 6)):
            x0, y0 = r.randint(2, 0x3C), r.randint(2, 0x3C)
            w, h = r.randint(1, 4), r.randint(1, 4)
            for x in range(x0, min(x0 + w, 0x40)):
                for y in range(y0, min(y0 + h, 0x40)):
                    img[x * 0x43 + y] = r.choice([0x20, 0x40])
    elif q < 0.8:
        for _ in range(r.randint(1, 12)):
            img[r.randint(1, 0x40) * 0x43 + r.randint(1, 0x40)] = r.choice([0x20, 0x40, 0x41, 0x00])
    elif q < 0.9:
        for x in range(1, 0x41):                    # a gap in the park's wall lets the fill in
            img[x * 0x43 + r.randint(1, 0x40)] = 0x20
    if r.random() < 0.3:
        img[0x41 * 0x43] = r.choice([0x23, 0x40, 0x00])    # the fill's start corner
    return bytes(img)


def legal_case(r):
    return base(r, "SoIsItLegal", [], image=legal_image(r))


def copy_case(r):
    return base(r, "CopyCurrentToNew", [], image=rnd_image(r))


def simple_edges(make, n):
    def edges():
        r = random.Random(81 + n)
        return [make(r) for _ in range(n)]
    return edges


def make_tests():
    tests = []

    def add(fn, name, edge, rnd):
        tests.append(FnTest(name, ppc, host, edge=edge, rnd=rnd))
    add("ConstructClickStack", "LLCC_SystemConstructHoardingClickStack", walk_edges("ConstructClickStack", lambda r: []),
        walk_random("ConstructClickStack", lambda r: []))
    add("LockHoarding", "LLCC_LockHoarding", walk_edges("LockHoarding", lock_args), walk_random("LockHoarding", lock_args))
    add("DrawDefaultHoardings", "LLCC_DrawDefaultHoardings", simple_edges(default_hoard_case, 30), default_hoard_case)
    add("DrawFromBalanceFile", "LLCC_DrawHoardingsFromBalanceFile", balance_edges, balance_case)
    add("ReviseHoarding", "LLCC_ReviseHoarding", revise_edges, revise_case)
    add("FloodFill", "LLCC_FloodFill", fill_edges, fill_case)
    add("CreateCurrentImage", "LLCC_CreateCurrentHoardingImage", simple_edges(current_image_case, 30), current_image_case)
    add("CopyCurrentToNew", "LLCC_CopyCurrentToNewImage", simple_edges(copy_case, 20), copy_case)
    add("AddNewArea", "LLCC_AddNewAreaToImage", add_area_edges, add_area_case)
    add("SoIsItLegal", "LLCC_SoIsItLegal", simple_edges(legal_case, 30), legal_case)
    add("IsClearLandLegal", "LLCC_IsClearLandLegal", clear_land_edges, clear_land_random)
    add("IsPatrolAreaLegal", "LLCC_IsPatrolAreaLegal", patrol_edges, patrol_case)
    add("GetFlag", "CTrackCell_GetFlag", flag_edges("GetFlag"), flag_random("GetFlag"))
    add("SetFlag", "CTrackCell_SetFlag", flag_edges("SetFlag"), flag_random("SetFlag"))
    add("UnSetFlag", "CTrackCell_UnSetFlag", flag_edges("UnSetFlag"), flag_random("UnSetFlag"))
    add("IsHoarding", "CTrackCell_IsHoarding", type_edges("IsHoarding", TYPES), type_random("IsHoarding"))
    add("IsTrackX", "CTrackCell_IsTrackX", type_edges("IsTrackX", TYPES), type_random("IsTrackX"))
    add("IsCorner", "CTrackCell_IsCorner", corner_edges, corner_random)
    add("IsFlag", "CMapCell_IsFlag", isflag_edges, isflag_random)
    return tests


TESTS = make_tests()
