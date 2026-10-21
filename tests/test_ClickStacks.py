"""Unit w204, the click stacks of the queue and track tools, against libtpw_unit_ClickStacks.dylib (tests/unit_ClickStacks.cpp), main app
("SimTheme Park"): src/game/park/LLCellSystem.cpp (LLCC_SystemBacktrack 0x10092280, LLCC_SystemConstructQueueClickStack 0x10092644,
LLCC_SystemConstructTrackClickStack 0x10092e44, LLCC_SystemConstructReverseTrackClickStack 0x10093280), src/game/park/TrackCell.cpp (IsStraight
0x100a3868, IsCrossRoad 0x100a3914), src/game/park/MapCell.cpp (CMapCell::IsCorner 0x1009c7fc) and src/game/park/LLCellRide.cpp (LLCR_OffsetStartOfTrack
0x1008c620, LLCR_OffsetEndOfTrack 0x1008c6d8).

A case is a world block (the map cells at world + 0x2d8, 0x44 each; the track cells at world + 0x1102d8, 0x28 each; the first 40 rows of both
layers are rewritten for every case, the track layer with a ring of stopper cells around a 32 by 32 arena), the cell system's words (every global
the nine functions touch, salted unless the case sets them), the click stack (a salted pattern with the case's count), an object block and a call.
The world is built twice by ONE piece of Python: in emulator memory at the Mac's addresses (big-endian) and in the host library's memory
(little-endian). Run for real on both sides: the cell base (GetNeighbours, GetDirection, NumberOfNeighbours, NumberOfHVNeighbours,
SetOverlapCounter, RemoveNeighbour), OPPOSITE, ADD_DIRECTION, CMap::IsXYValid, both GetNeighbouringCell (with MapDelta's operator*) and
CTrackCell::IsCorner. Every other callee is a fake that records the same event on both sides: the cell questions (a map cell's +0x27: 1 queue,
2 entrance, 4 path; a track cell's +0x23: 1 just track, 2 track, 4 start block; cells are named by id), SetHFieldTile, the cell system's
SetMode / SetModeNR / FixFirstBlock, the illegal placement, DeleteLastTrackBrace, LLCR_FixBridges (and Reverse), LLCM_LeftMouseButtonUp (it may
change the click stack's count and push a click), CObject::RecalculateBackOfQueue and GAME_ASSERTMSG.

What is compared: the answer, the event log, every word the functions can store, the whole click array, Backtrack's click pair, and every cell
of the 40 rows that differs from what the case built (all fields). The walks are checked by a small Python model before they run (a walk that
would never stop, or leave the rows, is thrown away), because the host side is not capped.

Not tested: a click count above 0x400 or below 0, the three bytes after gLLCC47C4 (the overflow push writes a whole word there on the Mac), the
walks of a ride whose rotation is none of the four (the Mac's answer is a register the function never set), a start cell outside the 40 rows,
a walk that leaves the grid, a world outside the first 40 rows, LLCR_FixBridges' effects (a stand-in), the real GetNeighbouringCell's use of MapDelta's
flag byte.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ClickStacks"
MASK = 0xFFFFFFFF

# ---- the nine functions: name -> (address, host wrapper)
FN = dict(
    Backtrack=(0x10092280, "Backtrack"), QueueWalk=(0x10092644, "QueueWalk"), TrackWalk=(0x10092E44, "TrackWalk"),
    ReverseWalk=(0x10093280, "ReverseWalk"), OffsetStart=(0x1008C620, "OffsetStart"), OffsetEnd=(0x1008C6D8, "OffsetEnd"),
    IsStraight=(0x100A3868, "IsStraight"), IsCrossRoad=(0x100A3914, "IsCrossRoad"), IsCorner=(0x1009C7FC, "IsCorner"),
)
OWN = {a for a, _ in FN.values()}
# the originals that run for real: GetNeighbours, GetDirection, RemoveNeighbour, SetOverlapCounter, NumberOfNeighbours, NumberOfHVNeighbours,
# CMapCell::GetNeighbouringCell(MapDelta), MapDelta::operator*, CTrackCell::GetNeighbouringCell (both), OPPOSITE, ADD_DIRECTION, IsXYValid, CTrackCell::IsCorner
REAL_OTHER = {0x100816E8, 0x10081930, 0x10081678, 0x10081A1C, 0x10081744, 0x10081890, 0x101036B0, 0x101037D0, 0x1012FCB8, 0x1012FDDC,
              0x10102500, 0x1010254C, 0x101012F4, 0x100A37A0}

# ---- the callees faked on both sides
ASSERT = 0x1000DDE8
IS_QUEUE, IS_ENTRANCE, IS_PATH, SET_HFIELD = 0x1009C490, 0x1009C4DC, 0x1009C44C, 0x1009CDB4
IS_JUST, IS_TRACK, IS_START = 0x100A3628, 0x100A3574, 0x100A36CC
SET_MODE, SET_MODE_NR, FIX_FIRST, SET_ILL, DEL_BRACE = 0x10090E18, 0x100911F0, 0x10091F5C, 0x10088934, 0x10056A80
FIX_BRIDGES, FIX_BRIDGES_REV, MOUSE_UP, RECALC = 0x1008FA34, 0x1009024C, 0x10084CA4, 0x1010952C

# ---- the Mac's addresses of the globals: name -> (address, width)
WORDS = dict(
    gLLCC4800=(0x102E4800, 4), gLLCC4804=(0x102E4804, 4), gLLCC480C=(0x102E480C, 4), gLLCC47C4=(0x102E47C4, 1), gLLCC4818=(0x102E4818, 1),
    gLLCCDB6C=(0x102DDB6C, 4), gLLCCE0CC=(0x102DE0CC, 1), gLLCCE288=(0x1029E288, 4), gLLCCE28C=(0x1029E28C, 4),
    gLLCCFirstBlockX=(0x1029E02C, 4), gLLCCFirstBlockY=(0x1029E030, 4), gLLCCClickStackCount=(0x102E47C0, 4),
    gLLCCBacktrackX=(0x102E07B0, 4), gLLCCBacktrackY=(0x102E07B4, 4),
)
ARRAYS = dict(gLLCCClickStack=0x102E27C0)
COUNT_ADDR = 0x102E47C0
STACK_ADDR = 0x102E27C0
G = dict(world=0x1037EBFC, world2=0x1037EC0C)
# sizeof map cell, track cell, offsetof(CWorld, mTrackCells), map +0x27, cell +0x23, object rotation, f32, cell x, cell y, sizeof click
# the eight compass offsets (Mac address -> (x, y)), written by __sinit_MapDelta_cpp 0x10103ec0 on the Mac
DELTAS = {0x1034C2AC: (0, -1), 0x1034C298: (1, -1), 0x1034C284: (1, 0), 0x1034C270: (1, 1), 0x1034C25C: (0, 1), 0x1034C248: (-1, 1),
          0x1034C234: (-1, 0), 0x1034C220: (-1, -1)}
HOST_LAYOUT = [0x44, 0x28, 0x1102D8, 0x27, 0x23, 0x10, 0x32, 4, 6, 8]
UNSIGNED = {"gLLCC47C4", "gLLCC4818", "gLLCCE0CC"}
FMT = {1: "B", 2: "H", 4: "I"}

ROWS = 40                 # the rows of both layers rewritten for every case
ARENA = 32                # the track layer's ring lies on the border of the first 32 by 32 cells
LO, HI = 3, 28            # the inside of the arena the structured walks keep to
HELD = 0x12345678         # a held ride (gLLCCDB6C): only ever labelled, never read

DIRS = (1, 4, 0x10, 0x40)
DXY = {1: (0, -1), 4: (1, 0), 0x10: (0, 1), 0x40: (-1, 0)}
OPP = {1: 0x10, 4: 0x40, 0x10: 1, 0x40: 4}
START_DIR = {0: 4, 0x5A: 1, 0xB4: 0x40, 0x10E: 0x10}                      # the direction bit LLCR_OffsetStartOfTrack answers
START_DELTA = {0: (4, 1), 0x5A: (1, -5), 0xB4: (-5, -2), 0x10E: (-2, 4)}
END_DELTA = {0: (-2, 1), 0x5A: (1, 1), 0xB4: (1, -2), 0x10E: (-2, -2)}
ROTS = (0, 0x5A, 0xB4, 0x10E)


def s32(v):
    return T.s32(v)


def opposite(d):
    """OPPOSITE (0x10102500) on any byte."""
    d &= 0xFF
    if d == 0:
        return 0
    if d < 0x10:
        return (d << 4) & 0xF0
    return d >> 4


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

R = CL.recorder


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def map_id(e, a):
    return (a - e.llm_world - 0x2D8) // 0x44 + 1


def track_id(e, a):
    return (a - e.llm_world - 0x1102D8) // 0x28 + 1


def build_stubs():
    S = CL.Stubs()
    S.real.update(OWN)
    S.real.update(REAL_OTHER)

    def asker(name, ident, offset, bit):
        def stub(e):
            a = e.r(3)
            e.cl.rec(name, (ident(e, a),))
            e.set_r(3, 1 if e.read(a + offset, 1)[0] & bit else 0)
        return stub

    def assert_(e):
        e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),), (e.cstr(e.r(4))[:159],))

    def set_hfield(e):
        e.cl.rec("SetHFieldTile", (map_id(e, e.r(3)),))

    def mouse_up(e):
        L = e.cl
        L.rec("LLCM_LeftMouseButtonUp", (s32(e.r(3)), s32(e.r(4)), e.r(5) & 0xFFFF, s32(e.r(6))))
        count = s32(L.ret("UpCount", e.s32(COUNT_ADDR)))
        if L.ret("UpPush", 0) != 0 and 0 <= count < 0x400:
            x = L.ret("UpX", 0)
            y = L.ret("UpY", 0)
            ws(e, STACK_ADDR + 8 * count, [x, y])
            count += 1
        ws(e, COUNT_ADDR, [count])

    S.internal(ASSERT, assert_)
    S.internal(IS_QUEUE, asker("IsQueue", map_id, 0x27, 1))
    S.internal(IS_ENTRANCE, asker("IsEntrance", map_id, 0x27, 2))
    S.internal(IS_PATH, asker("IsPath", map_id, 0x27, 4))
    S.internal(SET_HFIELD, set_hfield)
    S.internal(IS_JUST, asker("IsJustTrack", track_id, 0x23, 1))
    S.internal(IS_TRACK, asker("IsTrack", track_id, 0x23, 2))
    S.internal(IS_START, asker("IsTrackStartBlock", track_id, 0x23, 4))
    S.internal(SET_MODE, R("LLCC_SystemSetMode", "ib"))
    S.internal(SET_MODE_NR, R("LLCC_SystemSetModeNR", "ib"))
    S.internal(FIX_FIRST, R("LLCC_FixFirstBlock", "b"))
    S.internal(SET_ILL, R("LLCM_SetIllegalPlacement", "i"))
    S.internal(DEL_BRACE, R("DeleteLastTrackBrace", ""))
    S.internal(FIX_BRIDGES, R("LLCR_FixBridges", "Lbbb", ret=lambda e, L: L.ret("FixBridges", 0) & MASK))
    S.internal(FIX_BRIDGES_REV, R("LLCR_FixBridgesReverse", "Lbbb", ret=lambda e, L: L.ret("FixBridgesReverse", 0) & MASK))
    S.internal(MOUSE_UP, mouse_up)
    S.internal(RECALC, R("RecalculateBackOfQueue", "L"))
    return S


STUBS = build_stubs()


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    e.llm_world = T.persist_alloc(e, 0x1B1000)
    e.llm = None


# ---------------------------------------------------------------------------------------------------------------- the world

MAP_FMT = "HBBIiBBHHHIIIhBBHBBIIIIIII"      # 0x44 bytes: the shared 0x24, then mWho, the status byte, +0x27 and seven words
TRK_FMT = "HBBIiBBHHHIIIhBBHH"              # 0x28 bytes
FIELDS = dict(nb=(0x0C, "B"), dr=(0x0D, "B"), par=(0x10, "H"), typ=(8, "I"), kind=(2, "B"), ov=(0x20, "H"), hnb=(0x22, "B"), f23=(0x23, "B"),
              f27=(0x27, "B"))
RING = {(x, y) for x in range(ARENA) for y in range(ARENA) if x in (0, 1, ARENA - 2, ARENA - 1) or y in (0, 1, ARENA - 2, ARENA - 1)}
_TEMPLATES = {}


def templates(endian):
    """The first ROWS rows of both layers as built: ids, kinds, and the ring of track cells (just track, no neighbours) around the arena."""
    if endian not in _TEMPLATES:
        n = ROWS * 128
        mp, tp = bytearray(n * 0x44), bytearray(n * 0x28)
        for i in range(n):
            struct.pack_into(endian + "HB", mp, i * 0x44, i + 1, 1)
            struct.pack_into(endian + "HB", tp, i * 0x28, i + 1, 2)
        for x, y in RING:
            tp[(y * 128 + x) * 0x28 + 0x23] = 1
        _TEMPLATES[endian] = (bytes(mp), bytes(tp))
    return _TEMPLATES[endian]


def cell_offset(layer, x, y):
    return 0x2D8 + (y * 128 + x) * 0x44 if layer == "m" else 0x1102D8 + (y * 128 + x) * 0x28


class Side:
    """What the shared code needs from either memory."""

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def setw(self, name, v):
        a, width = WORDS[name]
        self.write(self.gaddr(name, a), struct.pack(self.fmt[0] + FMT[width], v & ((1 << (8 * width)) - 1)))

    def getw(self, name):
        a, width = WORDS[name]
        v = struct.unpack(self.fmt[0] + FMT[width], self.read(self.gaddr(name, a), width))[0]
        if name not in UNSIGNED and v >= 1 << (8 * width - 1):
            v -= 1 << (8 * width)
        return v

    def put_array(self, name, words):
        self.write(self.gaddr(name, ARRAYS[name]), struct.pack(self.fmt[0] + "I" * len(words), *words))

    def get_array(self, name):
        return list(struct.unpack(self.fmt[0] + "I" * 0x800, self.read(self.gaddr(name, ARRAYS[name]), 0x2000)))

    def poke(self, layer, x, y, d):
        base = self.world() + cell_offset(layer, x, y)
        for key, v in d.items():
            off, f = FIELDS[key]
            size = struct.calcsize(f)
            self.write(base + off, struct.pack(self.fmt[0] + f, v & ((1 << (8 * size)) - 1)))

    def region(self):
        w = self.world()
        return self.read(w + 0x2D8, ROWS * 128 * 0x44), self.read(w + 0x1102D8, ROWS * 128 * 0x28)

    def changed(self):
        """Every cell that is not as built, decoded field by field (the two memories differ in byte order)."""
        out = []
        for layer, size, fmt, was, now in (("m", 0x44, MAP_FMT, self.before[0], None), ("t", 0x28, TRK_FMT, self.before[1], None)):
            now = self.region()[0 if layer == "m" else 1]
            if now == was:
                continue
            for i in range(ROWS * 128):
                a, b = i * size, (i + 1) * size
                if now[a:b] != was[a:b]:
                    out.append((layer, i, struct.unpack(self.fmt[0] + fmt, now[a:b])))
        return out


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        self.addr = {}

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def gaddr(self, name, mac):
        return mac

    def make(self, label, size):
        a = self.e.alloc(size, b"\0" * size)
        self.name(label, a)
        return a

    def name(self, label, a):
        self.L.labels[a] = label
        self.addr[label] = a

    def world(self):
        return self.e.llm_world

    def set_world(self):
        ws(self.e, G["world"], [self.world()])
        ws(self.e, G["world2"], [self.world()])
        for address, (dx, dy) in DELTAS.items():      # the Mac builds these in __sinit_MapDelta_cpp, which the emulator never runs
            ws(self.e, address, [dx, dy])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [x & MASK for x in args])[0]

    def pair(self, x, y):
        return self.e.alloc(8, struct.pack(">ii", x, y))

    def get_pair(self, a):
        return list(struct.unpack(">ii", self.e.read(a, 8)))


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_ccs_init()
        CL.host_begin(lib, {}, case["queues"])
        self.addr = {}
        self.cache = {}

    def done(self):
        pass

    def gaddr(self, name, mac):
        if name not in self.cache:
            self.cache[name] = self.lib.tpw_ccs_gaddr(name.encode())
            assert self.cache[name], name
        return self.cache[name]

    def make(self, label, size):
        a = self.lib.tpw_ccs_obj(label, size)
        self.name(label, a)
        return a

    def name(self, label, a):
        self.lib.tpw_cl_label(label, C.c_void_p(a))
        self.addr[label] = a

    def world(self):
        return self.lib.tpw_ccs_gaddr(b"world")

    def set_world(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def pair(self, x, y):
        self.arr = (C.c_int32 * 2)(x, y)
        return C.addressof(self.arr)

    def get_pair(self, a):
        return list(self.arr)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    u64, i32, u32, i64 = C.c_uint64, C.c_int32, C.c_uint32, C.c_int64
    lib.tpw_ccs_gaddr.restype, lib.tpw_ccs_gaddr.argtypes = u64, [C.c_char_p]
    lib.tpw_ccs_init.argtypes = []
    lib.tpw_ccs_obj.restype, lib.tpw_ccs_obj.argtypes = u64, [i32, u32]
    lib.tpw_ccs_layout.argtypes = [C.POINTER(i64)]
    lib.tpw_cl_label.argtypes = [i32, C.c_void_p]
    sigs = dict(Backtrack=(i32, [u64, i32]), QueueWalk=(i32, [u64]), TrackWalk=(None, [u64]), ReverseWalk=(None, [u64]),
                OffsetStart=(u32, [u64, i32]), OffsetEnd=(u32, [u64, i32]), IsStraight=(u32, [u64]), IsCrossRoad=(u32, [u64]),
                IsCorner=(u32, [u64]))
    for name, (_, wrapper) in FN.items():
        f = getattr(lib, "tpw_ccs_" + wrapper)
        f.restype, f.argtypes = sigs[name]
    lib.tpw_ccs_init()
    out = (i64 * 32)()
    n = lib.tpw_ccs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, [hex(v) for v in list(out)[:n]]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- build, execute, observe


def default_word(name, salt):
    return (salt * 0x9E3779B1 + sum(map(ord, name)) * 0x85EBCA6B) & MASK


def stack_words(case):
    """The 0x400 clicks: a salted pattern; for the cases that read cells every click is one of the case's few positions."""
    salt = case["salt"]
    words = [((i * 0x9E3779B1) + salt) & MASK for i in range(0x800)]
    spots = case.get("spots")
    if spots:
        for i in range(0x400):
            x, y = spots[((i * 0x9E3779B1 + salt) >> 7) % len(spots)]
            words[2 * i], words[2 * i + 1] = x, y
    for index, (x, y) in case.get("stack", {}).items():
        words[2 * index], words[2 * index + 1] = x & MASK, y & MASK
    return words


def build(s, case):
    """Writes the case's world, globals and blocks through Side: the same code for both memories (offsets are the Mac's)."""
    s.set_world()
    mt, tt = templates(s.fmt[0])
    w = s.world()
    s.write(w + 0x2D8, mt)
    s.write(w + 0x1102D8, tt)
    for (x, y), d in case["m"].items():
        s.poke("m", x, y, d)
    for (x, y), d in case["t"].items():
        s.poke("t", x, y, d)
    s.before = s.region()
    salt = case["salt"]
    for name in WORDS:
        s.setw(name, case["w"].get(name, default_word(name, salt)))
    s.put_array("gLLCCClickStack", stack_words(case))
    if case["w"].get("gLLCCDB6C", 0) == HELD:
        s.name(8, HELD)


def execute(s, case):
    name = case["fn"]
    address, wrapper = FN[name]
    host = isinstance(s, Host)
    fn = None if not host else getattr(s.lib, "tpw_ccs_" + wrapper)
    if name in ("Backtrack", "QueueWalk", "TrackWalk", "ReverseWalk"):
        o = case["obj"]
        obj = 0
        if not case.get("objnull"):
            obj = s.make(7, 0x100)
            s.write(obj + 4, bytes([o.get("x", 0) & 0xFF]))
            s.write(obj + 6, bytes([o.get("y", 0) & 0xFF]))
            s.write(obj + 0x10, struct.pack(s.fmt[0] + "I", o.get("rot", 0) & MASK))
            s.write(obj + 0x32, struct.pack(s.fmt[0] + "H", o.get("f32", 0) & 0xFFFF))
        if name == "Backtrack":
            args = [obj, case["fix"]]
        else:
            args = [obj]
        r = s.call(address, *args) if not host else fn(*args)
        return (r & MASK) if name in ("Backtrack", "QueueWalk") else None
    if name in ("OffsetStart", "OffsetEnd"):
        x, y = case["xy"]
        a = s.pair(x, y)
        if not host:
            r = s.call(address, a, a + 4, case["rot"])
        else:
            r = fn(a, case["rot"])
        s.out_pair = s.get_pair(a)
        return (r & 0xFF) if case["rot"] in ROTS else None
    layer, x, y = case["cell"]
    a = s.world() + cell_offset(layer, x, y)
    r = s.call(address, a) if not host else fn(a)
    return r & 0xFF


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides; pointers as labels."""
    out = {"answer": answer, "log": s.log(), "words": {n: s.getw(n) for n in WORDS}}
    out["stack"] = s.get_array("gLLCCClickStack")
    out["cells"] = s.changed()
    if case["fn"] in ("OffsetStart", "OffsetEnd"):
        out["xy"] = s.out_pair
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


# ---------------------------------------------------------------------------------------------------------------- the model of the walks
# The host side is not capped: a walk is run only after this says it stops. The model leaves the start block stops out (they only end a walk
# earlier) and follows the rest of each loop exactly; it also refuses a walk that leaves the rows.


def nbyte(cell, g, kind):
    """CCellBase::GetNeighbours: the hoarding mask for a track cell (kind 2) while the cell system's flag byte is set."""
    if g != 0 and cell.get("kind", kind) == 2:
        return cell.get("hnb", 0)
    return cell.get("nb", 0)


def hv(v):
    return bin(v & 0x55).count("1")


def corner(v):
    if hv(v) != 2:
        return False
    return bool(((v & 1) and (v & 4)) or ((v & 1) and (v & 0x40)) or ((v & 0x10) and (v & 4)) or ((v & 0x10) and (v & 0x40)))


def inside(x, y):
    return 0 <= x < 128 and 0 <= y < ROWS


def queue_ok(case):
    m = case["m"]
    g = case["w"].get("gLLCC4818", 0)
    f32 = case["obj"]["f32"]
    x, y = (f32 - 1) & 0x7F, (f32 - 1) >> 7
    if not inside(x, y):
        return False
    start = m.get((x, y), {})
    d = nbyte(start, g, 1)
    if d in DXY:
        nx, ny = x + DXY[d][0], y + DXY[d][1]
        if 0 <= nx < 128 and 0 <= ny < 128:
            nxt = m.get((nx, ny), {})
            if nxt.get("f27", 0) & 1 and nxt.get("par", 0) != start.get("par", 0):
                return True
    if d == 0:
        return True
    seen = set()
    while True:
        if not inside(x, y):
            return False
        cell = m.get((x, y), {})
        stop = False
        v = nbyte(cell, g, 1)
        f27 = cell.get("f27", 0)
        if not f27 & 1:
            stop = True
        elif corner(v):
            d = opposite(d) ^ v
        if not f27 & 2:
            if bin(v).count("1") == 1 or f27 & 4:
                stop = True
        if stop:
            return True
        if d in DXY:
            x, y = x + DXY[d][0], y + DXY[d][1]
        if (x, y, d) in seen:
            return False
        seen.add((x, y, d))


def track_ok(case, start, d, reverse):
    t = case["t"]
    g = case["w"].get("gLLCC4818", 0)
    x, y = start
    stop = reverse and not (t.get((x, y), {"f23": 1} if (x, y) in RING else {}).get("f23", 0) & 2)
    seen = set()
    while True:
        if not inside(x, y):
            return False
        cell = t.get((x, y)) or ({"f23": 1} if (x, y) in RING else {})
        if cell.get("f23", 0) & 1:
            v = nbyte(cell, g, 2)
            if corner(v):
                d = opposite(d) ^ v
            elif v & d == 0:
                stop = True
        if stop:
            return True
        if d in DXY:
            x, y = x + 2 * DXY[d][0], y + 2 * DXY[d][1]
        if (x, y, d) in seen:
            return False
        seen.add((x, y, d))


# ---------------------------------------------------------------------------------------------------------------- case builders


def base(r, fn, ret=True, **over):
    case = dict(fn=fn, ret=ret, w={}, m={}, t={}, queues={}, stack={}, obj={}, salt=r.randrange(1 << 32), fix=0)
    case.update(over)
    return case


def put(cells, p, **kw):
    d = cells.setdefault(p, {})
    for k, v in kw.items():
        d[k] = (d.get(k, 0) | v) if k in ("nb", "hnb") else v
    return d


def link(cells, a, b, d, parent=None, **kw):
    """Cell a points to b along d, b points back."""
    put(cells, a, nb=d, **kw)
    put(cells, b, nb=OPP[d], **kw)


def walk_path(r, start, first, nsegs, step, lo=LO, hi=HI):
    """A self avoiding path of cells `step` apart from start: runs of 1 to 5 cells, a quarter turn after each."""
    pos, dirs, seen = [start], [], {start}
    d = first
    for _ in range(nsegs):
        for _ in range(r.randrange(1, 6)):
            nx, ny = pos[-1][0] + DXY[d][0] * step, pos[-1][1] + DXY[d][1] * step
            if not (lo <= nx <= hi and lo <= ny <= hi) or (nx, ny) in seen:
                break
            pos.append((nx, ny))
            dirs.append(d)
            seen.add((nx, ny))
        d = r.choice([t for t in DIRS if t != OPP[d] and t != d])
    return pos, dirs


def kinds_noise(r, cells, g, layer_kind):
    """Cell kinds and hoarding masks: the cell system's flag byte makes track cells (kind 2) answer their hoarding mask."""
    for p, d in cells.items():
        if r.random() < 0.15:
            d["kind"] = r.choice([1, 2, 3])
        if g and r.random() < 0.5:
            d["hnb"] = d.get("nb", 0) if r.random() < 0.7 else r.randrange(256)
        elif r.random() < 0.1:
            d["hnb"] = r.randrange(256)


def fix_hoarding(cells, g, layer_kind):
    """With the flag byte set, a cell of kind 2 answers its hoarding mask: copy the neighbour mask there unless the cell has its own."""
    for p, d in cells.items():
        if "hnb" not in d:
            d["hnb"] = d.get("nb", 0)


# ---- backtrack

KEY_MODES = [1, 3, 0x34, 0xB, 0x10, 0x19, 0x35, 0x15, 0xE, 0x13, 4, 0x3B, 0x16, 0, 2, 5, 0x1F]
SPOTS = [(3, 4), (5, 6)]        # the clicks of the cases that read map cells (mode 3)


def backtrack_case(r, mode, count, fix=None, objnull=None, held=None, fb=None, up=None, typ=None):
    c = base(r, "Backtrack")
    c["fix"] = r.randrange(2) if fix is None else fix
    c["objnull"] = r.random() < 0.3 if objnull is None else objnull
    held = r.random() < 0.5 if held is None else held
    c["w"].update(gLLCC480C=mode, gLLCCClickStackCount=count, gLLCCDB6C=HELD if held else 0)
    if r.random() < 0.5:
        c["w"]["gLLCCE288"] = r.choice([0x80, 0, -1, 7])
        c["w"]["gLLCCE28C"] = r.choice([0x80, 0, 5])
    fb = r.choice([0, 1, 0x100, 0xFF, 0x80000000]) if fb is None else fb
    q = {"FixBridges": [fb] if r.random() < 0.7 else [fb, r.choice([0, 1])], "FixBridgesReverse": [r.choice([0, 1, 0x200])]}
    if up is None:
        up = {}
        pick = r.random()
        if pick < 0.35:
            up["UpPush"] = [r.choice([0, 1]), r.choice([0, 1])]
        if r.random() < 0.15:
            up["UpCount"] = [r.choice([0, 1, 2, count, min(count + 1, 0x400), 0x3FF, 0x400])]   # never above 0x400 (not modelled)
        up["UpX"] = [r.choice([3, 5]), r.choice([3, 5])]
        up["UpY"] = [r.choice([4, 6]), r.choice([4, 6])]
    q.update(up)
    c["queues"] = q
    if mode == 3:
        # the cells of the clicks: the first spot is a path (mType 1) or not
        c["spots"] = SPOTS
        c["m"][SPOTS[0]] = {"typ": 1 if r.random() < 0.5 else r.choice([0, 2, 5, 7])}
        c["m"][SPOTS[1]] = {"typ": r.choice([1, 0, 2, 0x1E])}
        for p in ((3, 6), (5, 4)):
            c["m"][p] = {"typ": r.choice([1, 0])}
    return c


def backtrack_edges():
    r = random.Random(204)
    out = []
    modes = list(range(0x3D)) + [-1, 0x3D, 0x7FFFFFFF, -0x80000000]
    for mode in modes:
        for count in (0, 2, 5):
            out.append(backtrack_case(r, mode, count))
    for mode in KEY_MODES:
        for count in (1, 2, 3, 0x3FF, 0x400):
            for fix in (0, 1):
                for held in (False, True):
                    out.append(backtrack_case(r, mode, count, fix=fix, held=held, objnull=r.random() < 0.4,
                                              fb=r.choice([0, 1, 0x100]) if fix else None))
    for mode in (1, 3, 0x34, 0xB, 0x10, 0x35, 0x19):
        for objnull in (False, True):
            for held in (False, True):
                for fb in (0, 0x100):
                    out.append(backtrack_case(r, mode, 4, fix=1, objnull=objnull, held=held, fb=fb, up={"UpPush": [1, 0], "UpX": [3], "UpY": [4]}))
        for upc in (0, 1, 2):
            out.append(backtrack_case(r, mode, 4, up={"UpCount": [upc], "UpPush": [0], "UpX": [5], "UpY": [6]}))
        out.append(backtrack_case(r, mode, 2, up={"UpCount": [1], "UpPush": [1], "UpX": [5], "UpY": [6]}))
        out.append(backtrack_case(r, mode, 0x400, up={"UpCount": [0x400], "UpPush": [1], "UpX": [3], "UpY": [4]}))
    return out


def backtrack_random(r):
    mode = r.choice(KEY_MODES + list(range(0x3D)) + [r.randrange(-5, 100)])
    count = r.choice([0, 1, 2, 3, r.randrange(0x401), 0x3FF, 0x400])
    return backtrack_case(r, mode, count)


# ---- the queue walk


def queue_case(r, start_kind=None, end_kind=None, rot=None, g=None, pos_dirs=None):
    g = (1 if r.random() < 0.15 else 0) if g is None else g
    P, P2 = r.sample([1, 2, 0x1234, 0x7FFF, 0xFFFF], 2)
    start = (r.randrange(LO, HI + 1), r.randrange(LO, HI + 1))
    first = r.choice(DIRS)
    pos, dirs = pos_dirs if pos_dirs else walk_path(r, start, first, r.randrange(1, 6), 1)
    start = pos[0]
    m = {}
    n = len(pos)
    for i, p in enumerate(pos):
        put(m, p, par=P, f27=1, typ=r.choice([0, 1, 2]), ov=r.choice([0, 1, 2, 0xFFFF]))
    for i, d in enumerate(dirs):
        put(m, pos[i], nb=d)
        put(m, pos[i + 1], nb=OPP[d])
    sk = start_kind or r.choice(["entrance"] * 5 + ["queue", "nonqueue", "moved", "none", "multi", "same"])
    if sk == "moved" and n < 2:
        sk = "queue"
    if sk in ("none", "multi") and n < 1:
        sk = "queue"
    s = m[start]
    if sk == "entrance":
        s["f27"] = 3
    elif sk == "queue":
        s["f27"] = 1
    elif sk == "nonqueue":
        s["f27"] = r.choice([0, 2, 4])
    elif sk == "moved":
        s["f27"] = 3
        m[pos[1]]["par"] = P2
    elif sk == "same":
        s["f27"] = 3
        m[pos[1]]["par"] = P
    elif sk == "none":
        s["nb"] = 0
        s["f27"] = r.choice([1, 3, 0])
        s["dr"] = r.choice([0, 1, 4, 0x10, 0x40, 5, 0xFF])
    elif sk == "multi":
        s["f27"] = r.choice([1, 3])
        s["nb"] = s.get("nb", 0) | r.choice([1, 4, 0x10, 0x40, 2, 0x80, 0x14])
    if sk in ("entrance", "queue") and n >= 2 and r.random() < 0.25:
        m[pos[1]]["f27"] = r.choice([0, 1, 2, 3])
    ek = end_kind or r.choice(["open", "open", "dead", "path", "pathentr", "nonq"])
    if n >= 2:
        last = m[pos[-1]]
        d = dirs[-1]
        if ek == "open":
            last["nb"] = last.get("nb", 0) | d
        elif ek == "path":
            last["f27"] = r.choice([5, 4, 7])
            last["nb"] = last.get("nb", 0) | r.choice([0, d])
        elif ek == "pathentr":
            last["f27"] = 5
            m[pos[-2]]["f27"] = m[pos[-2]]["f27"] | 2
        elif ek == "nonq":
            last["f27"] = r.choice([0, 2])
    for p in pos[1:-1]:
        roll = r.random()
        if roll < 0.12:
            m[p]["f27"] |= 2
        elif roll < 0.18:
            m[p]["f27"] |= 4
        elif roll < 0.26:
            m[p]["f27"] |= r.choice([8, 0x80, 0x10])
        if r.random() < 0.08:
            m[p]["nb"] = m[p].get("nb", 0) | r.choice(DIRS)
    kinds_noise(r, m, g, 1)
    if g:
        fix_hoarding(m, g, 1)
    rotation = r.choice(list(ROTS) + [0x2D, 0x168, 0xFFFFFFA6]) if rot is None else rot
    sid = start[0] + (start[1] << 7) + 1
    c = base(r, "QueueWalk", m=m, obj={"f32": sid, "rot": rotation})
    c["w"].update(gLLCC4818=g, gLLCCClickStackCount=r.choice([0, 5, 0x400, 0x7FFF]))
    return c


def queue_good(r, **kw):
    for _ in range(60):
        c = queue_case(r, **kw)
        if queue_ok(c):
            return c
    return queue_case(random.Random(5), start_kind="queue", end_kind="dead")


def snake_case(r, bands, rotate=False):
    """Long zigzag bands of corner cells, 2 rows each, so the walk pushes past the 0x400 entries of the stack."""
    x0, ncols, y0 = 2, 123, 2
    path = [(x0, y0)]
    x, y, hd = x0, y0, 1
    for b in range(bands):
        ty = y0 + 2 * b
        for c in range(ncols):
            y = ty + 1 if y == ty else ty
            path.append((x, y))
            if c < ncols - 1:
                x += hd
                path.append((x, y))
        hd = -hd
        if b < bands - 1:
            while y < ty + 2:
                y += 1
                path.append((x, y))
    dirs = []
    for a, b in zip(path, path[1:]):
        dirs.append({(0, -1): 1, (1, 0): 4, (0, 1): 0x10, (-1, 0): 0x40}[(b[0] - a[0], b[1] - a[1])])
    m = {}
    for p in path:
        put(m, p, par=3, f27=1)
    for i, d in enumerate(dirs):
        put(m, path[i], nb=d)
        put(m, path[i + 1], nb=OPP[d])
    m[path[0]]["f27"] = 3
    m[path[-1]]["nb"] = OPP[dirs[-1]]
    c = base(r, "QueueWalk", m=m, obj={"f32": path[0][0] + (path[0][1] << 7) + 1, "rot": 0})
    c["w"].update(gLLCC4818=0, gLLCCClickStackCount=0x123)
    return c


def queue_edges():
    r = random.Random(205)
    out = []
    # a start cell with one neighbour: the neighbour not a queue / a queue of the same parent / a queue of another parent / outside the grid
    for d in DIRS:
        for sk in ("nonqueue", "same", "moved", "entrance"):
            for ek in ("open", "dead"):
                dx, dy = DXY[d]
                pos = [(10, 10), (10 + dx, 10 + dy), (10 + 2 * dx, 10 + 2 * dy)]
                out.append(queue_good(r, start_kind=sk, end_kind=ek, pos_dirs=(pos, [d, d]), rot=0))
        # the neighbour outside the grid: a start on the border that is not a queue / a queue with the one neighbour (the walk ends on it, it never
        # steps out: a step out of the grid reads a cell that is not there)
        sx, sy = {1: (6, 0), 4: (127, 6), 0x10: (6, 127), 0x40: (0, 6)}[d]
        if inside(sx, sy):
            for kind in (0, 1):
                m = {(sx, sy): {"nb": d, "f27": kind, "par": 1}}
                out.append(base(r, "QueueWalk", m=m, obj={"f32": sx + (sy << 7) + 1, "rot": 0}))
    for rot in list(ROTS) + [0x2D, 0x168, 0xFFFFFFA6, 0x5B]:
        for dr in (0, 1, 4, 0x10, 0x40, 5, 0xFF):
            pos = [(10, 10)]
            out.append(base(r, "QueueWalk", m={(10, 10): {"f27": 3, "par": 1, "dr": dr}}, obj={"f32": 10 + (10 << 7) + 1, "rot": rot}))
    # a start at the first cell of the grid (id 1), at the end of a row, and the last cell of the 11th row
    for sid in (1, 128 * 5, 128 * 10 + 127):
        out.append(base(r, "QueueWalk", m={}, obj={"f32": sid, "rot": 0}))
    # the ends of a walk
    for ek in ("open", "dead", "path", "pathentr", "nonq"):
        for g in (0, 1):
            for k in range(3):
                out.append(queue_good(r, start_kind="entrance", end_kind=ek, g=g))
    for k in range(40):
        out.append(queue_good(r))
    out.append(snake_case(r, 5))
    out.append(snake_case(r, 5))
    out[-1]["w"]["gLLCCClickStackCount"] = 0x7FFF
    return out


def queue_random(r):
    return queue_good(r)


# ---- the track walks


def track_case(r, reverse, rot=None, block=None, g=None, nsegs=None, end=None):
    g = (1 if r.random() < 0.12 else 0) if g is None else g
    rot = r.choice(ROTS) if rot is None else rot
    dstart = START_DIR[rot]
    d0 = OPP[dstart] if reverse else dstart
    delta = (END_DELTA if reverse else START_DELTA)[rot]
    start = (r.randrange(LO + 1, HI - 1), r.randrange(LO + 1, HI - 1))
    pos, dirs = walk_path(r, start, d0, r.randrange(1, 5) if nsegs is None else nsegs, 2)
    P, P2 = r.sample([1, 2, 0x1234, 0x7FFF], 2)
    t = {}
    for p in pos:
        put(t, p, par=P, f23=1 | (2 if r.random() < 0.85 else 0), ov=r.choice([0, 1, 2, 0xFFFF, 0x7FFF]))
    for i, d in enumerate(dirs):
        put(t, pos[i], nb=d)
        put(t, pos[i + 1], nb=OPP[d])
    n = len(pos)
    ek = end or r.choice(["dead", "dead", "open", "cross"])
    if n >= 2:
        last = t[pos[-1]]
        d = dirs[-1]
        if ek == "open":
            last["nb"] |= d
        elif ek == "cross":
            last["nb"] |= d | r.choice([x for x in DIRS if x not in (d, OPP[d])])
    # interior variations: a gap (not just track), a cross road, a start block with the same / another parent / without the way back
    for i, p in enumerate(pos):
        roll = r.random()
        if roll < 0.10 and i > 0:
            t[p]["f23"] &= ~1
        elif roll < 0.18:
            t[p]["nb"] |= r.choice(DIRS)
        elif roll < 0.22:
            t[p]["par"] = P2
        elif roll < 0.26:
            t[p]["f23"] |= 4
    if block in ("same", "other", "noback") and n >= 3:
        i = r.randrange(1, n - 1)
        nxt = t[pos[i + 1]] if i + 1 < n else None
        if nxt is not None:
            nxt["f23"] |= 4
            nxt["par"] = P2 if block == "other" else t[pos[i]]["par"]
            if block == "noback":
                nxt["nb"] &= ~OPP[dirs[i]] & 0xFF
    if r.random() < 0.3:
        t[start]["f23"] |= 4
    kinds_noise(r, t, g, 2)
    if g:
        fix_hoarding(t, g, 2)
    for d in t.values():
        if d.get("kind") == 1 or "kind" not in d:
            pass
    ox, oy = start[0] - delta[0], start[1] - delta[1]
    if r.random() < 0.2 and oy >= 1:
        ox, oy = ox + 128, oy - 1
    c = base(r, "ReverseWalk" if reverse else "TrackWalk", t=t, obj={"x": ox, "y": oy, "rot": rot})
    c["walk"] = (start, d0)
    c["w"].update(gLLCC4818=g, gLLCCClickStackCount=r.choice([0, 7, 0x400, 0x7FFF]))
    if reverse:
        pick = r.random()
        if pick < 0.5 and n >= 1:
            fx, fy = r.choice(pos)
        else:
            fx, fy = r.choice([(-1, -1), (200, 3), (5, 5), (0, 0), (31, 31), (3, 200), (-5, 7)])
        c["w"]["gLLCCFirstBlockX"], c["w"]["gLLCCFirstBlockY"] = fx, fy
    return c


def track_good(r, reverse, **kw):
    for _ in range(60):
        c = track_case(r, reverse, **kw)
        if track_ok(c, c["walk"][0], c["walk"][1], reverse):
            return c
    return track_case(random.Random(7), reverse, nsegs=1, end="dead", g=0, rot=0)


def track_snake(r, reverse, bands=9):
    """Long zigzag bands of corner cells on the lattice of every second cell, two lattice rows each, so the walk pushes past the 0x400 entries.
    The path crosses the stopper ring (the case's own cells replace the ring cells it meets)."""
    rot = 0x5A if reverse else 0x10E        # the first step goes down (0x10) in both walks
    x0, ncols, y0 = 2, 63, 4
    path = [(x0, y0)]
    x, y, hd = x0, y0, 2
    for b in range(bands):
        ty = y0 + 4 * b
        for c in range(ncols):
            y = ty + 2 if y == ty else ty
            path.append((x, y))
            if c < ncols - 1:
                x += hd
                path.append((x, y))
        hd = -hd
        if b < bands - 1:
            while y < ty + 4:
                y += 2
                path.append((x, y))
    dirs = [{(0, -2): 1, (2, 0): 4, (0, 2): 0x10, (-2, 0): 0x40}[(b[0] - a[0], b[1] - a[1])] for a, b in zip(path, path[1:])]
    t = {}
    for p in path:
        put(t, p, par=3, f23=3)
    for i, d in enumerate(dirs):
        put(t, path[i], nb=d)
        put(t, path[i + 1], nb=OPP[d])
    delta = (END_DELTA if reverse else START_DELTA)[rot]
    c = base(r, "ReverseWalk" if reverse else "TrackWalk", t=t, obj={"x": x0 - delta[0], "y": y0 - delta[1], "rot": rot})
    c["walk"] = (path[0], 0x10)
    c["w"]["gLLCC4818"] = 0
    assert track_ok(c, c["walk"][0], c["walk"][1], reverse)
    return c


def track_edges(reverse):
    def edges():
        r = random.Random(206 + reverse)
        out = []
        for rot in ROTS:
            for ek in ("dead", "open", "cross"):
                for g in (0, 1):
                    out.append(track_good(r, reverse, rot=rot, end=ek, g=g, nsegs=1))
                    out.append(track_good(r, reverse, rot=rot, end=ek, g=g))
        for block in ("same", "other", "noback"):
            for k in range(8):
                out.append(track_good(r, reverse, block=block, nsegs=r.randrange(2, 5)))
        for k in range(40):
            out.append(track_good(r, reverse))
        # long bands of corners: the pushes run past the 0x400 entries
        out.append(track_snake(r, reverse))
        out.append(track_snake(r, reverse, bands=8))
        return out
    return edges


def track_random(reverse):
    def rnd(r):
        return track_good(r, reverse)
    return rnd


# ---- the shape tests


def shape_case(r, fn, layer, nb, hnb=None, g=None, kind=None):
    g = r.choice([0, 0, 0, 1]) if g is None else g
    kind = (2 if layer == "t" else 1) if kind is None else kind
    cell = {"nb": nb, "hnb": r.randrange(256) if hnb is None else hnb, "kind": kind}
    c = base(r, fn, m={(6, 6): cell} if layer == "m" else {}, t={(6, 6): cell} if layer == "t" else {}, cell=(layer, 6, 6))
    c["w"]["gLLCC4818"] = g
    return c


def shape_edges(fn, layer):
    def edges():
        r = random.Random(210)
        out = [shape_case(r, fn, layer, v, g=0) for v in range(256)]
        out += [shape_case(r, fn, layer, v, hnb=(v ^ 0x55) & 0xFF, g=1, kind=2) for v in range(0, 256, 3)]
        out += [shape_case(r, fn, layer, v, hnb=(v ^ 0x55) & 0xFF, g=1, kind=1) for v in (0x11, 0x41, 0x05, 0x15, 0x55)]
        return out
    return edges


def shape_random(fn, layer):
    def rnd(r):
        return shape_case(r, fn, layer, r.randrange(256), kind=r.choice([1, 2, 2]))
    return rnd


# ---- the offsets


XY_VALUES = [0, 1, -1, 5, 127, 128, -128, 0x7FFFFFFF, -0x80000000, 0x7FFFFFFE, -0x7FFFFFFF, 1000, -1000, 0x10000]


def offset_case(r, fn, rot, x=None, y=None):
    return base(r, fn, ret=True, rot=rot, xy=(r.choice(XY_VALUES + [r.randrange(-1 << 31, 1 << 31)]) if x is None else x,
                                                r.choice(XY_VALUES + [r.randrange(-1 << 31, 1 << 31)]) if y is None else y))


def offset_edges(fn):
    def edges():
        r = random.Random(211)
        out = []
        for rot in list(ROTS) + [0x2D, 0x168, -90, 1, 0x5B, 0xB5, 0x10F, 0x7FFFFFFF, -0x80000000, 360, 270 + 0x100]:
            for x, y in ((0, 0), (10, 20), (-1, 0x7FFFFFFF), (0x7FFFFFFF, -0x80000000), (-0x80000000, 0x7FFFFFFF)):
                out.append(offset_case(r, fn, rot, x, y))
        return out
    return edges


def offset_random(fn):
    def rnd(r):
        return offset_case(r, fn, r.choice(list(ROTS) * 4 + [r.randrange(-1 << 31, 1 << 31), r.randrange(0, 400)]))
    return rnd


def make_tests():
    tests = []

    def add(name, fn, edge, rnd):
        tests.append(FnTest(name, ppc, host, edge=edge, rnd=rnd))
    add("LLCC_SystemBacktrack", "Backtrack", backtrack_edges, backtrack_random)
    add("LLCC_SystemConstructQueueClickStack", "QueueWalk", queue_edges, queue_random)
    add("LLCC_SystemConstructTrackClickStack", "TrackWalk", track_edges(False), track_random(False))
    add("LLCC_SystemConstructReverseTrackClickStack", "ReverseWalk", track_edges(True), track_random(True))
    add("CTrackCell_IsCrossRoad", "IsCrossRoad", shape_edges("IsCrossRoad", "t"), shape_random("IsCrossRoad", "t"))
    add("CTrackCell_IsStraight", "IsStraight", shape_edges("IsStraight", "t"), shape_random("IsStraight", "t"))
    add("CMapCell_IsCorner", "IsCorner", shape_edges("IsCorner", "m"), shape_random("IsCorner", "m"))
    add("LLCR_OffsetStartOfTrack", "OffsetStart", offset_edges("OffsetStart"), offset_random("OffsetStart"))
    add("LLCR_OffsetEndOfTrack", "OffsetEnd", offset_edges("OffsetEnd"), offset_random("OffsetEnd"))
    return tests


TESTS = make_tests()
