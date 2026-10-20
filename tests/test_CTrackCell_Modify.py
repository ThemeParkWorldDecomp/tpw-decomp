"""Unit v5881, CTrackCell::Modify: the track cell system's one entry for a change to a track or hoarding cell (the blueprint faces under
bit 0x100, else delete, neighbours, hoarding flags, directions, parent ids, water flags, bridges, ride deletion or a type change),
against libtpw_unit_CTrackCell_Modify.dylib (tests/unit_CTrackCell_Modify.cpp), main app ("SimTheme Park"),
src/TrackCell_flags_CTrackCell_Modify.cpp:
  CTrackCell::Modify 0x1009dfe4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee is a
fake on both sides that records its arguments (pointers as labels, MapDelta as its x and y, floats as bits, the tag ids as the halfword)
and answers from the case's queues; the fakes that answer a cell answer a cell number (0 NULL, k the k-th of five cell blocks).

The world of a case: the track cell (id, type, parent id), five cell blocks (id, type), the cell system's globals (gLLCC4858, gLLCC4854,
gLLCCDC21C, gLLCC4800, gLLCC4804, gLLCC4818, gLLCCDB48, gLLCCDB68, the hoarding ends, the two no-parent ids, gLLCCDE0B4/B8, gGameWorld
as an address never dereferenced), entries 0 and k of gThingArray, and on the Mac the five compass offsets. What is compared: the answer
(r3), the event log, gLLCC4854, gLLCCDC21C, gLLCC4800 and gLLCC4804.

No fused operations; fctiwz runs under HardwareRounding (heights stay small so the wall loops stay short; GetWorldGSquareHeight also
answers NaN and -inf). Not tested: +inf or huge ground heights (the original's fctiwz + 9 overflows), NaN or infinite cell heights
(the wall loop would run 2^31 / 10 times), a NULL map cell (modes 0x81, 0x83 and 0x85 read it; mode 0x87 calls Delete on the
neighbours without a NULL test), thing ids past the table.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CTrackCell_Modify"
MASK = 0xFFFFFFFF

# ---- the unit's own function
FN_MODIFY = 0x1009dfe4
SIZES = {FN_MODIFY: 10252}
OWN = set(SIZES)

# ---- the callees faked on both sides
GET_WORLD_GSQUARE_HEIGHT = 0x1005f160
GET_MAP_CELL_EQUIV = 0x100f49a0
GET_TRACK_MAP_CELL_HEIGHT = 0x1009d8d0
CONNECTION_TYPE = 0x100a1594
DC_ADD_SQUARE_FACE = 0x100e02a0
TRACK_GET_NEIGHBOURING_CELL = 0x1012fcb8
LLCM_SET_ILLEGAL_PLACEMENT = 0x10088934
CALCULATE_DIRECTIONS = 0x100a12dc
CALCULATE_NEIGHBOURS = 0x100a0bd4
GET_DIRECTION = 0x10081930
GET_NEIGHBOURS = 0x100816e8
IS_STRAIGHT = 0x100a3868
IS_CORNER = 0x100a37a0
IS_CROSS_ROAD = 0x100a3914
MAP_UNSET_FLAG = 0x1009bc18
MAP_SET_HFIELD_TILE = 0x1009cdb4
MAP_GET_FLAG = 0x1009bbe8
MAP_SET_FLAG = 0x1009bbb8
MAP_GET_NEIGHBOURING_CELL = 0x101036b0
TRACK_UNSET_FLAG = 0x100a29cc
TRACK_SET_FLAG = 0x100a2964
TRACK_SET_HFIELD_TILE = 0x100a3bac
MODIFY_PARENT_ID = 0x100a0b68
MODIFY_TYPE = 0x100a0828
IS_WATER = 0x1009c408
IS_FREEFORM = 0x1009c59c
IS_TRACK = 0x100a3574
IS_HOARDING = 0x100a35dc
GET_OBJECT_THING_ID = 0x10103a04
GAME_ASSERTMSG = 0x1000dde8
LLCR_FIX_BRIDGES = 0x1008fa34
LLCR_FIX_BRIDGES_REVERSE = 0x1009024c
TRACK_DELETE = 0x100a40c4
LLCC_SYSTEM_GET_MODE = 0x100914f4
LLCR_DELETE_RIDE = 0x100892c4
MAP_DELETE = 0x1009d0f4
LLCC_SYSTEM_SET_MODE_NR = 0x100911f0

# ---- the Mac's addresses of the globals
ADDR = dict(gLLCC4858=0x102e4858, gLLCC4854=0x102e4854, gLLCCDC21C=0x102dc21c, gLLCC4800=0x102e4800, gLLCC4804=0x102e4804,
            gLLCC4818=0x102e4818, gLLCCDB48=0x102ddb48, gLLCCDB68=0x102ddb68, gHoardingRight=0x1029e03c, gHoardingBottom=0x1029e040,
            gHoardingLeft=0x1029e044, gHoardingTop=0x1029e048, gModifyNoParent=0x1029e284, gModifyNoThing=0x1029e286,
            gLLCCDE0B4=0x102de0b4, gLLCCDE0B8=0x102de0b8, gGameWorld=0x1037ebfc, gThingArray=0x1034cbfc)
WORDS = ["gLLCC4858", "gLLCC4854", "gLLCCDC21C", "gLLCC4800", "gLLCC4804", "gHoardingRight", "gHoardingBottom", "gHoardingLeft",
         "gHoardingTop", "gLLCCDE0B4", "gLLCCDE0B8"]
HALVES = ["gLLCCDB68", "gModifyNoParent", "gModifyNoThing"]
# The compass offsets (only the Mac's need writing; the host's are constants of the test library).
DELTAS = {0x1034c284: (1, 0), 0x1034c270: (1, 1), 0x1034c25c: (0, 1), 0x1034c2ac: (0, -1), 0x1034c234: (-1, 0)}
WORLD = 0x01000000            # gGameWorld: an address the code only computes with
GRID = 0x2d8                  # CWorld::mMapCells
THING_STRIDE = 0x14           # a Mac gThingArray entry
HOST_LAYOUT = [0, 8, 0x10, 0x28, 0x44]

SELF, OBJ = 1, 7              # labels: the track cell, cells 1..5 are labels 2..6, the ride object
NCELLS = 5
CELLS = []                    # the PowerPC cell blocks of the current case (the pointer fakes answer them)
FORMAT = b"Thing has been allocated in ThingArray index 0 - shouldn't happen\n"


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    """A float's bits with every NaN made one (the processors make different NaNs)."""
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- rounding


class ROUNDING(HardwareRounding):
    """HardwareRounding aimed at the unit's function only (Unicorn saturates fctiwz wrongly)."""

    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cell_of(n):
    return CELLS[n - 1] if 1 <= n <= len(CELLS) else 0


def fl(e, n):
    return canon_f(fb(e.f(n)))


def rd32(e, a):
    return struct.unpack(">I", e.read(a, 4))[0]


def simple(name, *regs, answer=None):
    """A fake that records `this` (r3) as a label and the given registers (masks) and answers from the queue."""
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)),) + tuple((s32(e.r(rg)) if m is None else e.r(rg) & m) for rg, m in regs))
        if answer == "word":
            e.set_r(3, L.ret(name) & MASK)
        elif answer == "byte":
            e.set_r(3, L.ret(name) & 0xFF)
        elif answer == "half":
            e.set_r(3, L.ret(name) & 0xFFFF)
        elif answer == "cell":
            e.set_r(3, cell_of(L.ret(name)))
    return stub


def stub_ground(e):
    """float GetWorldGSquareHeight(int x, int y, uint32_t* extra)."""
    L = e.cl
    L.rec("GetWorldGSquareHeight", (s32(e.r(3)), s32(e.r(4)), L.lab(e.r(5))))
    e.set_f1(bitsf(L.ret("GetWorldGSquareHeight")))


def stub_height(e):
    """float GetTrackMapCellHeight(CMapCell* cell, float height, int x, int y): the float takes r4's slot."""
    L = e.cl
    L.rec("GetTrackMapCellHeight", (L.lab(e.r(3)), fl(e, 1), s32(e.r(5)), s32(e.r(6))))
    e.set_f1(bitsf(L.ret("GetTrackMapCellHeight")))


def stub_face(e):
    """DC_AddSquareFaceToList(float x, y, z, uint8 side, int texture, int rotation, float unused1, int large, uint8 unused2,
    int keep, DYN_Type type): f1-f3, r6-r8, f4, r10, then the stack words 0x38, 0x3c, 0x40."""
    L = e.cl
    sp = e.r(1)
    L.rec("DC_AddSquareFaceToList", (fl(e, 1), fl(e, 2), fl(e, 3), e.r(6) & 0xFF, s32(e.r(7)), s32(e.r(8)), fl(e, 4), s32(e.r(10)),
                                     rd32(e, sp + 0x38) & 0xFF, s32(rd32(e, sp + 0x3C)), s32(rd32(e, sp + 0x40))))


def neighbour(name):
    """GetNeighbouringCell(MapDelta (a pointer to it in r4), int offset): answers a cell number's block."""
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), s32(rd32(e, e.r(4))), s32(rd32(e, e.r(4) + 4)), s32(e.r(5))))
        e.set_r(3, cell_of(L.ret(name)))
    return stub


def stub_parent(e):
    """CTrackCell::ModifyParentID(tag_IDX_TYPE id): the halfword in the high half of r4."""
    L = e.cl
    L.rec("ModifyParentID", (L.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))


def stub_thing(e):
    """tag_IDX_TYPE CMapCell::GetObjectThingId() const: hidden result in r3, this in r4; records the cell's index in the grid."""
    L = e.cl
    L.rec("GetObjectThingId", ((s32(e.r(4) - WORLD - GRID)) // 0x44,))
    e.write(e.r(3), struct.pack(">H", L.ret("GetObjectThingId") & 0xFFFF))


def stub_assert(e):
    """GAME_ASSERTMSG(int condition, const char* format, ...): the condition and whether the format is the expected text."""
    L = e.cl
    text = e.read(e.r(4), len(FORMAT) + 1)
    L.rec("GAME_ASSERTMSG", (s32(e.r(3)), 1 if text == FORMAT + b"\0" else 0))


def stub_get_mode(e):
    """int LLCC_SystemGetMode(): also overwrites the drag start words (Modify puts them back)."""
    L = e.cl
    L.rec("LLCC_SystemGetMode", ())
    e.write(ADDR["gLLCC4800"], struct.pack(">I", 0x11111111))
    e.write(ADDR["gLLCC4804"], struct.pack(">I", 0x22222222))
    e.set_r(3, L.ret("LLCC_SystemGetMode") & MASK)


def stub_free(name, *regs, answer=None):
    """A free function's fake: the registers (masks) and an optional word answer."""
    def stub(e):
        L = e.cl
        L.rec(name, tuple((s32(e.r(rg)) if m is None else e.r(rg) & m) for rg, m in regs))
        if answer == "word":
            e.set_r(3, L.ret(name) & MASK)
    return stub


def stub_bridges(name):
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), e.r(4) & 0xFF, e.r(5) & 0xFF, e.r(6) & 0xFF))
        e.set_r(3, L.ret(name) & 0xFF)
    return stub


STUBS.internal(GET_WORLD_GSQUARE_HEIGHT, stub_ground)
STUBS.internal(GET_MAP_CELL_EQUIV, simple("GetMapCellEquiv", answer="cell"))
STUBS.internal(GET_TRACK_MAP_CELL_HEIGHT, stub_height)
STUBS.internal(CONNECTION_TYPE, simple("ConnectionType", (4, None), (5, None), (6, 0xFF), answer="word"))
STUBS.internal(DC_ADD_SQUARE_FACE, stub_face)
STUBS.internal(TRACK_GET_NEIGHBOURING_CELL, neighbour("CTrackCell::GetNeighbouringCell"))
STUBS.internal(MAP_GET_NEIGHBOURING_CELL, neighbour("CMapCell::GetNeighbouringCell"))
STUBS.internal(LLCM_SET_ILLEGAL_PLACEMENT, stub_free("LLCM_SetIllegalPlacement", (3, None)))
STUBS.internal(CALCULATE_DIRECTIONS, simple("CalculateDirections", (4, None), (5, None)))
STUBS.internal(CALCULATE_NEIGHBOURS, simple("CalculateNeighbours", (4, None), (5, None), (6, None)))
STUBS.internal(GET_DIRECTION, simple("GetDirection", answer="byte"))
STUBS.internal(GET_NEIGHBOURS, simple("GetNeighbours", answer="byte"))
STUBS.internal(IS_STRAIGHT, simple("IsStraight", answer="byte"))
STUBS.internal(IS_CORNER, simple("IsCorner", answer="byte"))
STUBS.internal(IS_CROSS_ROAD, simple("IsCrossRoad", answer="byte"))
STUBS.internal(MAP_UNSET_FLAG, simple("CMapCell::UnSetFlag", (4, 0xFFFF)))
STUBS.internal(MAP_SET_HFIELD_TILE, simple("CMapCell::SetHFieldTile"))
STUBS.internal(MAP_GET_FLAG, simple("CMapCell::GetFlag", answer="half"))
STUBS.internal(MAP_SET_FLAG, simple("CMapCell::SetFlag", (4, 0xFFFF)))
STUBS.internal(TRACK_UNSET_FLAG, simple("CTrackCell::UnSetFlag", (4, 0xFFFF)))
STUBS.internal(TRACK_SET_FLAG, simple("CTrackCell::SetFlag", (4, 0xFFFF)))
STUBS.internal(TRACK_SET_HFIELD_TILE, simple("CTrackCell::SetHFieldTile"))
STUBS.internal(MODIFY_PARENT_ID, stub_parent)
STUBS.internal(MODIFY_TYPE, simple("ModifyType", (4, None), answer="word"))
STUBS.internal(IS_WATER, simple("IsWater", answer="word"))
STUBS.internal(IS_FREEFORM, simple("IsFreeform", answer="word"))
STUBS.internal(IS_TRACK, simple("IsTrack", answer="word"))
STUBS.internal(IS_HOARDING, simple("IsHoarding", answer="word"))
STUBS.internal(GET_OBJECT_THING_ID, stub_thing)
STUBS.internal(GAME_ASSERTMSG, stub_assert)
STUBS.internal(LLCR_FIX_BRIDGES, stub_bridges("LLCR_FixBridges"))
STUBS.internal(LLCR_FIX_BRIDGES_REVERSE, stub_bridges("LLCR_FixBridgesReverse"))
STUBS.internal(TRACK_DELETE, simple("CTrackCell::Delete", (4, None), (5, None), (6, 0xFF)))
STUBS.internal(LLCC_SYSTEM_GET_MODE, stub_get_mode)
STUBS.internal(LLCR_DELETE_RIDE, stub_free("LLCR_DeleteRide", (3, None), (4, None), (5, None)))
STUBS.internal(MAP_DELETE, simple("CMapCell::Delete", (4, None), (5, None)))
STUBS.internal(LLCC_SYSTEM_SET_MODE_NR, stub_free("LLCC_SystemSetModeNR", (3, None), (4, 0xFF)))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def byte(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.self_cell = e.alloc(0x44)
        self.cells = [e.alloc(0x44) for _ in range(NCELLS)]
        self.obj = e.alloc(0x40)
        CELLS[:] = self.cells
        labels = {self.self_cell: SELF, self.obj: OBJ}
        for i, c in enumerate(self.cells):
            labels[c] = i + 2
        self.L = e.cl = CL.PpcLog(labels, case["queues"])
        self.saved = STUBS.activate(e)
        for a, (x, y) in DELTAS.items():
            self.words(a, [x, y, 0])

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ptr(self, a, v):
        self.words(a, [v])

    def thing(self, index):
        return ADDR["gThingArray"] + index * THING_STRIDE

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_modify(self, mode, a, b):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN_MODIFY, [self.self_cell, mode & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.self_cell = lib.tpw_tcm_alloc(0x44)
        self.cells = [lib.tpw_tcm_alloc(0x44) for _ in range(NCELLS)]
        self.obj = lib.tpw_tcm_alloc(0x40)
        lib.tpw_tcm_cells((C.c_uint64 * NCELLS)(*self.cells), NCELLS)
        labels = {SELF: self.self_cell, OBJ: self.obj}
        for i, c in enumerate(self.cells):
            labels[i + 2] = c
        CL.host_begin(lib, labels, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_tcm_addr(name.encode())

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def thing(self, index):
        return self.lib.tpw_tcm_thing(index)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_modify(self, mode, a, b):
        return self.lib.tpw_tcm_modify(self.self_cell, s32(mode), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_tcm_addr.restype, lib.tpw_tcm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_tcm_alloc.restype, lib.tpw_tcm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_tcm_thing.restype, lib.tpw_tcm_thing.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_tcm_cells.restype, lib.tpw_tcm_cells.argtypes = None, [C.POINTER(C.c_uint64), C.c_int32]
    lib.tpw_tcm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_tcm_modify.restype, lib.tpw_tcm_modify.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_tcm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """The globals, the cells and the thing table entries (offsets are the Mac's)."""
    g = case["g"]
    for name in WORDS:
        s.words(s.g(name), [g[name]])
    for name in HALVES:
        s.half(s.g(name), g[name])
    s.byte(s.g("gLLCC4818"), g["gLLCC4818"])
    s.ptr(s.g("gLLCCDB48"), s.self_cell if g["gLLCCDB48"] else 0)
    s.ptr(s.g("gGameWorld"), WORLD)
    s.half(s.self_cell, case["self"]["id"])
    s.words(s.self_cell + 8, [case["self"]["type"]])
    s.half(s.self_cell + 0x10, case["self"]["parent"])
    for c, cell in zip(s.cells, case["cells"]):
        s.half(c, cell["id"])
        s.words(c + 8, [cell["type"]])
    s.ptr(s.thing(0), s.obj if case["thing0"] else 0)
    if case["thing"] != 0:
        s.ptr(s.thing(case["thing"]), s.obj if case["thingval"] else 0)


def execute(s, case):
    return s.call_modify(case["mode"], case["a"], case["b"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "gLLCC4854": s.rd_words(s.g("gLLCC4854"), 1),
            "gLLCCDC21C": s.rd_words(s.g("gLLCCDC21C"), 1), "gLLCC4800": s.rd_words(s.g("gLLCC4800"), 1),
            "gLLCC4804": s.rd_words(s.g("gLLCC4804"), 1)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

TYPES = [0, 1, 3, 4, 7, 9, 0xA, 0xB, 0xC, 0xD, 0x10, 0x11, 0x12, 0x15, 0x19]
HEIGHTS = [0.0, -0.0, 10.0, 20.0, -10.0, 5.5, 35.25, -23.75, 1.0, 59.9]


def rid(r):
    return r.choice([0, 1, 2, 0x80, 0x81, 0x3FFF, 0x4000, 0xFFFF, r.randrange(1, 0x4001), r.randrange(0x10000)])


def rtype(r):
    return r.choice(TYPES + [r.randrange(0x40)])


def rground(r):
    k = r.random()
    if k < 0.06:
        return r.choice([0x7FC00000, 0xFF800000, 0x80000000, 0])
    if k < 0.5:
        return fb(r.uniform(-50, 50))
    return fb(r.uniform(-2000, 2000))


def rheight(r):
    return fb(r.choice(HEIGHTS) if r.random() < 0.5 else r.uniform(-60, 60))


def rmode(r):
    k = r.random()
    if k < 0.45:
        return r.choice([0x32, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87])
    if k < 0.7:
        return 0x100 | r.choice([4, 0xB, 0x10, 0x19, 0xC, r.randrange(0x100)])
    if k < 0.8:
        return r.choice([0x19, 0x219, 0x281, 0x387, r.randrange(0x10000)])
    return r.randrange(0x100)


def base(r, mode=None, a=None, b=None):
    """A random world: the queues hold more answers than any path asks for."""
    self_cell = {"id": rid(r), "type": rtype(r), "parent": r.choice([0, 1, 5, 0x1234, 0xFFFF, r.randrange(0x10000)])}
    cells = [{"id": rid(r), "type": rtype(r)} for _ in range(NCELLS)]
    g = {name: r.randrange(-3, 4) for name in WORDS}
    g.update(gLLCC4858=r.choice([0, 1, 7]), gLLCC4854=r.choice([0, 1]), gLLCCDC21C=r.choice([0, 0x19, r.randrange(0x400)]),
             gLLCC4800=r.randrange(-1, 128), gLLCC4804=r.randrange(-1, 128), gLLCC4818=r.choice([0, 0, 1, 0x80]),
             gLLCCDB48=r.choice([0, 1]), gLLCCDB68=r.randrange(0x10000),
             gModifyNoParent=r.choice([self_cell["parent"], 0xFFFF, r.randrange(0x10000)]), gModifyNoThing=r.choice([0xFFFF, 0, 3]))
    for name in ("gHoardingRight", "gHoardingLeft"):
        g[name] = r.randrange(0, 128)
    for name in ("gHoardingBottom", "gHoardingTop"):
        g[name] = r.randrange(0, 128)
    if r.random() < 0.3:
        # Every cell at one square, which is one end of the hoarding.
        x, y = r.randrange(128), r.randrange(128)
        for c in cells:
            c["id"] = 1 + x + y * 128
        if r.random() < 0.5:
            g["gHoardingRight"], g["gHoardingBottom"] = x, y
        else:
            g["gHoardingLeft"], g["gHoardingTop"] = x, y
    thing = r.choice([0, 1, 5, r.randrange(1, 0x2800)])
    q = {
        "GetWorldGSquareHeight": [rground(r) for _ in range(12)],
        "GetTrackMapCellHeight": [rheight(r) for _ in range(12)],
        "GetMapCellEquiv": [r.randint(1, NCELLS) for _ in range(12)],
        "ConnectionType": [r.choice([0, 1, 0xD, 2, 5, 0x10, r.randrange(-5, 40)]) for _ in range(3)],
        "CTrackCell::GetNeighbouringCell": [r.randint(0, NCELLS) for _ in range(8)],
        "CMapCell::GetNeighbouringCell": [r.randint(0, NCELLS) for _ in range(8)],
        "GetDirection": [r.choice([1, 4, 0x10, 0x40, 0, 2, r.randrange(0x100)]) for _ in range(2)],
        "GetNeighbours": [r.choice([0x41, 0x14, 5, 0x50, 0, r.randrange(0x100)]) for _ in range(2)],
        "IsStraight": [r.choice([0, 0, 1, 5, 0x80]) for _ in range(2)],
        "IsCorner": [r.choice([0, 0, 1, 5, 0x80]) for _ in range(2)],
        "IsCrossRoad": [r.choice([0, 0, 1, 5, 0x80]) for _ in range(2)],
        "CMapCell::GetFlag": [r.randrange(0x10000) for _ in range(6)],
        "ModifyType": [r.choice([0, 1, 2, -1]) for _ in range(4)],
        "IsWater": [r.choice([0, 0, 1]) for _ in range(5)],
        "IsFreeform": [r.choice([0, 0, 1]) for _ in range(5)],
        "IsTrack": [r.choice([0, 1, 2]) for _ in range(2)],
        "IsHoarding": [r.choice([0, 0, 1]) for _ in range(2)],
        "GetObjectThingId": [r.choice([thing, thing, g["gModifyNoThing"]])],
        "LLCR_FixBridges": [r.choice([0, 1])],
        "LLCR_FixBridgesReverse": [r.choice([0, 1])],
        "LLCC_SystemGetMode": [r.randrange(-1, 20)],
    }
    return {"mode": rmode(r) if mode is None else mode,
            "a": r.choice([-2, 2, 5, 0, 1, -1, r.randrange(-100, 100)]) if a is None else a,
            "b": r.choice([-2, 2, 5, 0, 1, -1, r.randrange(-100, 100)]) if b is None else b,
            "self": self_cell, "cells": cells, "g": g, "thing": thing, "thing0": r.random() < 0.15, "thingval": r.random() < 0.85,
            "queues": q}


def fix(c):
    """Mode 0x87 deletes the three map neighbours without a NULL test: their queue answers cells only."""
    if (c["mode"] & 0x1FF) == 0x87:
        c["queues"]["CMapCell::GetNeighbouringCell"] = [n or 1 for n in c["queues"]["CMapCell::GetNeighbouringCell"]]
    return c


def mk(r, mode, a=None, b=None, self_type=None, cell_types=None, g=None, q=None, **over):
    c = base(r, mode, a, b)
    if self_type is not None:
        c["self"]["type"] = self_type
    if cell_types is not None:
        for cell, t in zip(c["cells"], cell_types):
            cell["type"] = t
    if g:
        c["g"].update(g)
    if q:
        c["queues"].update(q)
    c.update(over)
    return fix(c)


def modify_edges():
    r = random.Random(5881)
    # The simple modes, and the default range on both sides of every case value.
    for mode in (0x32, 0x80, 0x82, 0x232, 0x380):
        yield mk(r, mode)
    for mode in (0, 1, 0x19, 0x31, 0x33, 0x7F, 0x84, 0x88, 0xFF, 0x219, 0x200, 0xFE19):
        for t in (0xD, 0x12, 0xB, 0x10, 0x19, 0xC, 0):
            for g4858 in (0, 1):
                for ok in (0, 1):
                    yield mk(r, mode, self_type=t, g={"gLLCC4858": g4858}, q={"ModifyType": [ok]})
    # 0x81: locked, the hoarding's every direction and kind, the other cells, the two exits.
    for t in (0xB, 0xC, 0x10, 0x11, 0x19, 0xD, 0):
        for lock in (0, 1):
            for mct in (0x15, 0):
                cells = [mct] * NCELLS
                yield mk(r, 0x81, self_type=t, cell_types=cells, g={"gLLCC4818": lock})
    for d in (1, 4, 0x10, 0x40, 0, 2, 0x80):
        for nb in (0x41, 0x14, 5, 0x50, 0, 0x11):
            for straight, corner in ((1, 0), (0, 1), (0, 0)):
                for end in (0, 1, 2):
                    c = mk(r, 0x81, self_type=0x19, g={"gLLCC4818": 0},
                           q={"GetDirection": [d], "GetNeighbours": [nb], "IsStraight": [straight], "IsCorner": [corner]})
                    x, y = r.randrange(128), r.randrange(128)
                    for cell in c["cells"]:
                        cell["id"] = 1 + x + y * 128
                    c["g"].update(gHoardingRight=(x + 1) % 128, gHoardingBottom=y, gHoardingLeft=x, gHoardingTop=(y + 1) % 128)
                    if end == 1:
                        c["g"].update(gHoardingRight=x, gHoardingBottom=y)
                    elif end == 2:
                        c["g"].update(gHoardingLeft=x, gHoardingTop=y)
                    yield c
    for corner in (0, 1):
        for cross in (0, 1):
            for t in (0, 0xD, 0x12, 4):
                for g4858, g4854 in ((0, 0), (1, 0), (0, 1), (1, 1)):
                    yield mk(r, 0x81, self_type=t, g={"gLLCC4818": 0, "gLLCC4858": g4858, "gLLCC4854": g4854},
                             q={"IsCorner": [corner], "IsCrossRoad": [cross]})
    # 0x83.
    for t in (0xB, 0xC, 0x10, 0x11, 4):
        for lock in (0, 1):
            for db48 in (0, 1):
                yield mk(r, 0x83, self_type=t, cell_types=[0] * NCELLS, g={"gLLCC4818": lock, "gLLCCDB48": db48})
    for db48 in (1, 1, 1, 0):
        yield mk(r, 0x83, self_type=4, g={"gLLCC4818": 0, "gLLCCDB48": db48},
                 q={"CTrackCell::GetNeighbouringCell": [r.randint(0, NCELLS) for _ in range(3)]})
    # 0x85.
    for corner, cross in ((1, 0), (0, 1), (0, 0), (0, 0)):
        for t in (0x19, 4):
            yield mk(r, 0x85, self_type=t, q={"IsCorner": [corner], "IsCrossRoad": [cross]})
    for i in range(16):
        yield mk(r, 0x85, self_type=0x19, q={"IsCorner": [0], "IsCrossRoad": [0],
                                             "IsWater": [(i >> k) & 1 for k in range(4)], "IsFreeform": [(i >> (3 - k)) & 1 for k in range(4)]})
    for types in ([0] * 5, [1] * 5, [3] * 5, [0, 0, 0, 0, 1], [2] * 5, [0, 3, 0, 1, 0]):
        for nbs in ([1, 2, 3], [0, 2, 3], [1, 0, 0], [5, 4, 3]):
            yield mk(r, 0x85, self_type=4, cell_types=types, q={"IsCorner": [0], "IsCrossRoad": [0], "GetMapCellEquiv": [1] * 4,
                                                                  "CMapCell::GetNeighbouringCell": nbs})
    # 0x86: every exit, the assertion, both bridge answers.
    for track, hoard, lock in ((0, 0, 0), (1, 1, 0), (1, 0, 1), (1, 0, 0), (2, 0, 0)):
        yield mk(r, 0x86, g={"gLLCC4818": lock}, q={"IsTrack": [track], "IsHoarding": [hoard]})
    for parent in (0, 1, 0x4000, 0xFFFF, 0x1234):
        for same in (0, 1):
            for nothing in (0, 1):
                c = mk(r, 0x86, g={"gLLCC4818": 0}, q={"IsTrack": [1], "IsHoarding": [0]})
                c["self"]["parent"] = parent
                c["g"]["gModifyNoParent"] = parent if same else (parent + 1) & 0xFFFF
                c["queues"]["GetObjectThingId"] = [c["g"]["gModifyNoThing"] if nothing else c["thing"]]
                yield c
    for thing0 in (0, 1):
        for fixed in (0, 1):
            for thing in (0, 1, 0x27FF):
                c = mk(r, 0x86, g={"gLLCC4818": 0, "gModifyNoThing": 0xFFFF}, q={"IsTrack": [1], "IsHoarding": [0],
                                                                                 "LLCR_FixBridges": [fixed], "GetObjectThingId": [thing]},
                       thing0=bool(thing0), thing=thing)
                c["self"]["parent"] = 7
                c["g"]["gModifyNoParent"] = 0xFFFF
                yield c
    # 0x87.
    for b8, b4 in ((1, 0), (0, 0), (0, 1), (-1, -2), (-2, -1)):
        for track in (0, 1):
            for g4858 in (0, 1):
                yield mk(r, 0x87, g={"gLLCCDE0B8": b8, "gLLCCDE0B4": b4, "gLLCC4858": g4858}, q={"IsTrack": [track]})
    for cell_id in (0, 1, 0x80, 0x4000, 0xFFFF):
        c = mk(r, 0x87, g={"gLLCCDE0B8": 2, "gLLCCDE0B4": 1, "gLLCC4858": 1}, q={"IsTrack": [0]})
        c["self"]["id"] = cell_id
        yield c
    # 0x100: the face texture's special modes, every direction byte, every connection kind, every base height type.
    for m in (4, 0xB, 0x10, 0x19, 0xC, 0x84):
        for a in (5, 4, -2, 2):
            yield mk(r, 0x100 | m, a, 3, q={"ConnectionType": [0, 0]})
    for a in (-2, 2, 0, 5):
        for b in (-2, 2, 0, 1):
            for conn in (0, 1, 0xD, 7):
                yield mk(r, 0x104, a, b, q={"ConnectionType": [conn, 9]})
    for t in TYPES + [8, 0xE, 0x20]:
        for conn in (0, 1, 0xD):
            yield mk(r, 0x10B, 5, 2, self_type=t, cell_types=[t] * NCELLS, q={"ConnectionType": [conn, 0]})
    for same in (0, 1):
        h = [fb(20.0)] * 12 if same else [fb(40.0), fb(0.0), fb(-15.5), fb(40.0), fb(39.9), fb(61.0)] * 2
        for conn in (0, 1, 0xD):
            yield mk(r, 0x110, 5, -2, q={"GetTrackMapCellHeight": h, "ConnectionType": [conn, 1],
                                         "CTrackCell::GetNeighbouringCell": [1, 2, 3, 4]})
    for cell_id in (0, 1, 0x80, 0x4000, 0xFFFF):
        c = mk(r, 0x119, 0, 0, self_type=0x19, q={"ConnectionType": [0, 0]})
        c["self"]["id"] = cell_id
        yield c


def modify_random(r):
    return fix(base(r))


TESTS = [
    FnTest("CTrackCell::Modify", ppc, host, edge=modify_edges, rnd=modify_random),
]
