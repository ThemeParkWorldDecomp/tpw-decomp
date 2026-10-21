"""Unit v5633, the cell system's mouse move handler (per mode: snap and draw the preview line, place the ride preview, clamp the hoarding
edit, check clear land / patrol / pylon legality, edit track; then the ride under the cell and the cursor), against
libtpw_unit_LLCM_MouseMoved.dylib (tests/unit_LLCM_MouseMoved.cpp), main app ("SimTheme Park"),
src/game/park/LLCellMouse.cpp:
  LLCM_MouseMoved 0x10083180

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee is a
fake on both sides that records what the real one reads (cells as their byte offset from the world, objects as labels, the words behind
out pointers) and answers from the case's queues; the line and boundary fakes move the words behind their out pointers by queued deltas,
the pylon area fake writes its four words, GetObjectThingId writes its hidden halfword result. abs (a glue import) is faked with C
semantics and records nothing.

The world of a case: every global the function reads or writes (the mode words, the drag start, the selected ride, the hoarding and
clamp bounds, the track edit words, the action recorder's flag), a world block whose map and track cells carry fixed parent ids, a ride
record (balance slots 18, 38..46, 107) every GetRideInfo answers, a bank account, the current object (its rotation) and things 0..7 of
the thing table (kind, object type; entry 0 empty or not). What is compared: the event log and every global read back after the call.

No fused operations. Not tested: cells outside the world block (the cell coordinates stay inside the 128 x 128 map, as the game keeps
them), an empty thing table entry under a ride (the original reads it without a check), a NULL current object in ride mode (the same).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCM_MouseMoved"
MASK = 0xFFFFFFFF

# ---- the unit's own function
FN_MOVED = 0x10083180
SIZES = {FN_MOVED: 6228}
OWN = set(SIZES)

# ---- the callees faked on both sides
SET_ILLEGAL = 0x10088934
RESET_FACES = 0x100e0260
IS_MODE = 0x10091568
GET_MODE = 0x100914f4
DRAW_LINE = 0x1009c0d0
GET_LAST_MODE = 0x1009152c
GET_RIDE_INFO = 0x10154868
HAS_BEEN_BOUGHT = 0x100fbfec
SET_SESSION_COST = 0x10091d14
GET_BANK = 0x1013edfc
GET_BALANCE = 0x100f32b4
GET_SESSION_COST = 0x10091d98
IS_XY_VALID = 0x101012f4
GET_RIDE_ROTATION = 0x10090de0
TRACK_BOUNDARY = 0x100889ac
TRACK_IS_TRACKX = 0x100a367c
TRACK_IS_TRACK = 0x100a3574
GET_DIRECTION = 0x10081930
SET_RIDE_ROTATION = 0x10090d40
PLACE_RIDE = 0x1008a248
DRAW_TRACK_LINE = 0x100a30f8
HOARDING_LEGAL = 0x1009464c
CLEAR_LAND_LEGAL = 0x10096a0c
PATROL_LEGAL = 0x10096a44
START_TRACK_EDIT = 0x10088a64
HOARDING_STACK = 0x100936f4
LEFT_UP = 0x10084ca4
SET_MODE_NR = 0x100911f0
GET_SEGMENT = 0x100a399c
CONNECTION_TYPE = 0x100a1594
TRACE_TRACK = 0x1008ddfc
VALID_PYLON = 0x100411e4
PYLON_AREA = 0x10041170
IS_FLAG = 0x1009bb70
IS_HOARDING = 0x1009ca9c
MAP_IS_TRACK = 0x1009c96c
IS_ENTRANCE = 0x1009c4dc
IS_EXIT = 0x1009c524
IS_QUEUE = 0x1009c490
IS_RIDE = 0x1009c61c
IS_FREEFORM = 0x1009c59c
IS_PATH = 0x1009c44c
GET_PYLON_INDEX = 0x1009d860
GET_THING_ID = 0x10103a04
GAME_ASSERTMSG = 0x1000dde8
SET_CURSOR = 0x1009179c

# ---- the Mac's addresses of the globals; the host library's come from tpw_mm_addr(name)
ADDR = dict(C214=0x102dc214, C210=0x102dc210, C20C=0x102dc20c, C208=0x102dc208, C204=0x102dc204, C200=0x102dc200,
            DB54=0x102ddb54, DB50=0x102ddb50, DE0B8=0x102de0b8, DE0B4=0x102de0b4, **{"4804": 0x102e4804, "4800": 0x102e4800,
            "E0E0": 0x102de0e0, "E0DC": 0x102de0dc, "9F34": 0x1029df34, "9F18": 0x1029df18, "9F1C": 0x1029df1c,
            "47C8": 0x102e47c8, "F9BD4": 0x102f9bd4, "9F38": 0x1029df38, "9F3C": 0x1029df3c, "9F40": 0x1029df40,
            "9F24": 0x1029df24, "9F26": 0x1029df26, "E090": 0x102de090, "9F14": 0x1029df14},
            CostFree=0x102ddb35, DB3C=0x102ddb3c, DB38=0x102ddb38, Width=0x102e47fc, Height=0x102e47f8,
            HRight=0x1029e03c, HBottom=0x1029e040, DB68=0x102ddb68, Revise=0x1029df30, DB58=0x102ddb58, DB5A=0x102ddb5a,
            DB60=0x102ddb60, Recording=0x1037ec44 + 0x24, DB6C=0x102ddb6c, GameWorld=0x1037ebfc, ThingArray=0x1034cbfc,
            RideDB=0x1037f6c8)
WIDTH = {"E0E0": 2, "9F34": 2, "DB68": 2, "9F40": 2, "9F24": 2, "9F26": 2, "DB58": 2, "DB5A": 2, "CostFree": 1, "E090": 1}
# The globals read back after the call (every one the case writes but the pointers).
OBSERVED = ["C214", "C210", "C20C", "C208", "C204", "C200", "DB54", "DB50", "DE0B8", "DE0B4", "4804", "4800", "CostFree", "E0E0",
            "E0DC", "9F34", "9F18", "9F1C", "47C8", "DB3C", "DB38", "F9BD4", "9F38", "9F3C", "Width", "Height", "HRight", "HBottom",
            "DB68", "9F40", "9F24", "9F26", "Revise", "DB58", "DB5A", "E090", "9F14", "DB60", "Recording"]
# ---- the host's offsets and sizes the module relies on: CObject mObjectType, mRotation; CThing mKind; CCellBase mParentID;
# sizeof CMapCell, CTrackCell; CWorld mMapCells, mTrackCells
HOST_LAYOUT = [0xe, 0x10, 2, 0x10, 0x44, 0x28, 0x2d8, 0x1102d8]

MAP_CELLS, TRACK_CELLS = 0x2d8, 0x1102d8
CELLS = 0x4000
WORLD_SIZE = TRACK_CELLS + CELLS * 0x28 + 0x40
RIDE_SLOTS = [18, 38, 39, 40, 41, 42, 43, 44, 45, 46, 107]
PPC_WORLD = [0]


def s32(v):
    return T.s32(v)


def world_image(h):
    """The world block: every map and track cell's parent id (cells 0..0x4000; cell 0 is the slot before the first)."""
    buf = bytearray(WORLD_SIZE)
    for i in range(CELLS + 1):
        struct.pack_into(h, buf, MAP_CELLS + (i - 1) * 0x44 + 0x10, (i * 37 + 11) % CELLS + 1)
    for i in range(CELLS + 1):
        struct.pack_into(h, buf, TRACK_CELLS + (i - 1) * 0x28 + 0x10, (i * 53 + 7) % CELLS + 1)
    return bytes(buf)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def off(a):
    """A cell as its byte offset from the world block."""
    return s32((a - PPC_WORLD[0]) & MASK)


def rec_ret(name, spec_fn, key=None):
    """A fake that records spec_fn(e) and answers the queue `key` (None: no answer)."""
    def fn(e):
        L = e.cl
        L.rec(name, tuple(spec_fn(e)))
        if key is not None:
            e.set_r(3, L.ret(key) & MASK)
    return fn


def stub_is_mode(e):
    L = e.cl
    m = s32(e.r(3))
    L.rec("LLCC_SystemIsMode", (m,))
    e.set_r(3, 1 if L.ret("Mode") == m else 0)


def stub_line(name):
    def fn(e):
        L = e.cl
        px, py = e.r(6), e.r(7)
        L.rec(name, (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), e.s32(px), e.s32(py)))
        e.write(px, struct.pack(">I", (e.s32(px) + L.ret("LineDX")) & MASK))
        e.write(py, struct.pack(">I", (e.s32(py) + L.ret("LineDY")) & MASK))
    return fn


def stub_boundary(e):
    L = e.cl
    ps = [e.r(5), e.r(6), e.r(7), e.r(8)]
    L.rec("LLCM_TrackCellBoundary", (s32(e.r(3)), s32(e.r(4))) + tuple(e.s32(p) for p in ps))
    for p, key in zip(ps, ("BX", "BY", "BX2", "BY2")):
        e.write(p, struct.pack(">I", (e.s32(p) + L.ret(key)) & MASK))


def stub_ride_info(e):
    L = e.cl
    L.rec("CRideDatabase::GetRideInfo", (L.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, e.mm_ride)


def stub_bank(e):
    L = e.cl
    L.rec("CWorld::GetBankAccount", (L.lab(e.r(3)),))
    e.set_r(3, e.mm_bank)


def stub_area(e):
    L = e.cl
    L.rec("GetPotentialPylonPositionArea", ())
    for reg, key in zip((3, 4, 5, 6), ("AreaX0", "AreaY0", "AreaX1", "AreaY1")):
        e.write(e.r(reg), struct.pack(">I", L.ret(key) & MASK))


def stub_is_flag(e):
    L = e.cl
    flag = e.r(4) & 0xFFFF
    L.rec("CMapCell::IsFlag", (off(e.r(3)), flag))
    e.set_r(3, L.ret("Flag40" if flag == 0x40 else "Flag20") & MASK)


def stub_thing_id(e):
    L = e.cl
    L.rec("CMapCell::GetObjectThingId", (off(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("ThingId") & 0xFFFF))


def stub_abs(e):
    v = s32(e.r(3))
    e.set_r(3, abs(v) & MASK)


def cell_fake(name, key):
    return rec_ret(name, lambda e: (off(e.r(3)),), key)


STUBS.imp("abs", stub_abs)
for addr, fn in [
    (SET_ILLEGAL, rec_ret("LLCM_SetIllegalPlacement", lambda e: (s32(e.r(3)),))),
    (RESET_FACES, rec_ret("DC_ResetSquareFaceListPtr", lambda e: ())),
    (IS_MODE, stub_is_mode),
    (GET_MODE, rec_ret("LLCC_SystemGetMode", lambda e: (), "GetMode")),
    (GET_LAST_MODE, rec_ret("LLCC_SystemGetLastMode", lambda e: (), "GetLastMode")),
    (DRAW_LINE, stub_line("LLCC_DrawLine")),
    (DRAW_TRACK_LINE, stub_line("LLCC_DrawTrackLine")),
    (GET_RIDE_INFO, stub_ride_info),
    (HAS_BEEN_BOUGHT, rec_ret("CGoldenTicketControl::HasBeenBought", lambda e: (e.cl.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF), "Bought")),
    (SET_SESSION_COST, rec_ret("LLCC_SystemSetSessionCost", lambda e: (s32(e.r(3)),))),
    (GET_BANK, stub_bank),
    (GET_BALANCE, rec_ret("CBankAccount::GetBalance", lambda e: (e.cl.lab(e.r(3)),), "Balance")),
    (GET_SESSION_COST, rec_ret("LLCC_SystemGetSessionCost", lambda e: (), "SessionCost")),
    (IS_XY_VALID, rec_ret("CMap::IsXYValid", lambda e: (s32(e.r(3)), s32(e.r(4))), "XYValid")),
    (GET_RIDE_ROTATION, rec_ret("LLCC_GetRideRotation", lambda e: (), "Rotation")),
    (TRACK_BOUNDARY, stub_boundary),
    (TRACK_IS_TRACKX, cell_fake("CTrackCell::IsTrackX", "IsTrackX")),
    (TRACK_IS_TRACK, cell_fake("CTrackCell::IsTrack", "TrackIsTrack")),
    (GET_DIRECTION, cell_fake("CCellBase::GetDirection", "Direction")),
    (SET_RIDE_ROTATION, rec_ret("LLCC_SetRideRotation", lambda e: (s32(e.r(3)), e.r(4) & 0xFF))),
    (PLACE_RIDE, rec_ret("LLCR_PlaceRide", lambda e: tuple(s32(e.r(i)) for i in range(3, 9)), "PlaceRide")),
    (HOARDING_LEGAL, rec_ret("LLCC_IsHoardingEditLegal", lambda e: tuple(s32(e.r(i)) for i in range(3, 7)) + (e.r(7) & 0xFF,), "Legal")),
    (CLEAR_LAND_LEGAL, rec_ret("LLCC_IsClearLandLegal", lambda e: tuple(s32(e.r(i)) for i in range(3, 7)), "Legal")),
    (PATROL_LEGAL, rec_ret("LLCC_IsPatrolAreaLegal", lambda e: tuple(s32(e.r(i)) for i in range(3, 7)), "Legal")),
    (START_TRACK_EDIT, rec_ret("LLCM_StartTrackEditMode", lambda e: (s32(e.r(3)), s32(e.r(4)), e.r(5) & 0xFFFF))),
    (HOARDING_STACK, rec_ret("LLCC_SystemConstructHoardingClickStack", lambda e: ())),
    (LEFT_UP, rec_ret("LLCM_LeftMouseButtonUp", lambda e: (s32(e.r(3)), s32(e.r(4)), e.r(5) & 0xFFFF, s32(e.r(6))))),
    (SET_MODE_NR, rec_ret("LLCC_SystemSetModeNR", lambda e: (s32(e.r(3)), e.r(4) & 0xFF))),
    (GET_SEGMENT, cell_fake("CTrackCell::GetSegmentNumber", "Segment")),
    (CONNECTION_TYPE, rec_ret("CTrackCell::ConnectionType", lambda e: (off(e.r(3)), s32(e.r(4)), s32(e.r(5)), e.r(6) & 0xFF), "Conn")),
    (TRACE_TRACK, rec_ret("LLCC_TraceTrack", lambda e: (off(e.r(3)), off(e.r(4)), s32(e.r(5))))),
    (VALID_PYLON, rec_ret("IsValidPylonPosition", lambda e: (e.r(3), e.r(4), e.cl.lab(e.r(5)), e.r(6), e.r(7), e.r(8), e.r(9)), "Pylon")),
    (PYLON_AREA, stub_area),
    (IS_FLAG, stub_is_flag),
    (IS_HOARDING, cell_fake("CMapCell::IsHoarding", "Hoarding")),
    (MAP_IS_TRACK, cell_fake("CMapCell::IsTrack", "MapIsTrack")),
    (IS_ENTRANCE, cell_fake("CMapCell::IsEntrance", "Entrance")),
    (IS_EXIT, cell_fake("CMapCell::IsExit", "Exit")),
    (IS_QUEUE, cell_fake("CMapCell::IsQueue", "Queue")),
    (IS_RIDE, cell_fake("CMapCell::IsRide", "IsRide")),
    (IS_FREEFORM, cell_fake("CMapCell::IsFreeform", "Freeform")),
    (IS_PATH, cell_fake("CMapCell::IsPath", "Path")),
    (GET_PYLON_INDEX, cell_fake("CMapCell::GetPylonIndex", "PylonIndex")),
    (GET_THING_ID, stub_thing_id),
    (GAME_ASSERTMSG, CL.recorder("GAME_ASSERTMSG", "is")),
    (SET_CURSOR, rec_ret("LLCC_SystemSetCursor", lambda e: ())),
]:
    STUBS.internal(addr, fn)


def setup(runner):
    T.install_std_stubs(runner.emu)
    PPC_WORLD[0] = T.persist_alloc(runner.emu, WORLD_SIZE, world_image(">H"))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def put(self, name, v):
        w = WIDTH.get(name, 4)
        self.write(self.g(name), struct.pack(self.fmt[0] + {4: "I", 2: "H", 1: "B"}[w], v & ((1 << (8 * w)) - 1)))

    def get(self, name):
        w = WIDTH.get(name, 4)
        v = struct.unpack(self.fmt[0] + {4: "I", 2: "H", 1: "B"}[w], self.read(self.g(name), w))[0]
        return s32(v) if w == 4 else v


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"
    stride = 0x14

    def __init__(self, e, case):
        self.e = e
        self.world = PPC_WORLD[0]
        self.ride = e.mm_ride = e.alloc(0x200)
        self.bank = e.mm_bank = e.alloc(0x40)
        self.L = e.cl = CL.PpcLog({self.world: 1, ADDR["RideDB"]: 2, self.bank: 3, self.ride: 4, self.world + 0x2c0: 5}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def slot(self, n):
        return 0xc + 4 * n

    def ptr(self, a, v):
        self.words(a, [v])

    def current(self, obj):
        pass

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, x, y, flag):
        T.call_ppc(self.e, FN_MOVED, [x & MASK, y & MASK, flag & 0xFFFF])


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.world, self.ride, self.bank = lib.mm_world, lib.mm_ride, lib.mm_bank
        self.stride = lib.tpw_mm_size(0)
        self.write(self.ride, bytes(lib.tpw_mm_size(1)))
        self.ptr(self.g("FakeRide"), self.ride)
        self.ptr(self.g("FakeBank"), self.bank)
        CL.host_begin(lib, {1: self.world, 2: self.g("RideDB"), 3: self.bank, 4: self.ride, 5: self.world + 0x2c0}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_mm_addr(name.encode())

    def slot(self, n):
        return self.lib.tpw_mm_slot(n)

    def ptr(self, a, v):
        """A native pointer (8 bytes on the host)."""
        self.write(a, struct.pack("<Q", v))

    def current(self, obj):
        """The host keeps the whole current object pointer beside gLLCCDB6C's low word."""
        self.ptr(self.g("CurObj"), obj)

    def alloc(self, n):
        return self.lib.tpw_mm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, x, y, flag):
        self.lib.tpw_mm_moved(s32(x), s32(y), flag & 0xFFFF)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here; the world, ride and bank blocks are made once."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_mm_addr.restype, lib.tpw_mm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_mm_alloc.restype, lib.tpw_mm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_mm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_mm_size.restype, lib.tpw_mm_size.argtypes = C.c_int64, [C.c_int32]
    lib.tpw_mm_slot.restype, lib.tpw_mm_slot.argtypes = C.c_int64, [C.c_int32]
    lib.tpw_mm_moved.restype, lib.tpw_mm_moved.argtypes = None, [C.c_int32, C.c_int32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_mm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.mm_world = lib.tpw_mm_alloc(WORLD_SIZE)
    C.memmove(lib.mm_world, world_image("<H"), WORLD_SIZE)
    lib.mm_ride = lib.tpw_mm_alloc(lib.tpw_mm_size(1))
    lib.mm_bank = lib.tpw_mm_alloc(0x40)
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's globals and blocks through Side: the same code for both memories (offsets are the Mac's)."""
    for name, v in case["g"].items():
        s.put(name, v)
    s.ptr(s.g("GameWorld"), s.world)
    for n, v in case["slots"].items():
        s.words(s.ride + s.slot(n), [v])
    obj = s.alloc(0x20)
    s.words(obj + 0x10, [case["rot"]])
    s.words(s.g("DB6C"), [obj])
    s.current(obj)
    for i, (kind, otype) in enumerate(case["things"]):
        a = 0
        if i or case["t0"]:
            a = s.alloc(0x20)
            s.write(a + 2, bytes([kind]))
            s.write(a + 0xe, struct.pack(s.fmt[0] + "H", otype))
        s.ptr(s.g("ThingArray") + i * s.stride, a)


def execute(s, case):
    s.call(*case["args"])


def observe(s, case):
    out = {"log": s.log()}
    for name in OBSERVED:
        out[name] = s.get(name)
    return out


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

MODES = [0, 1, 3, 0x38, 4, 0x3b, 0xb, 0x10, 0x19, 0x39, 0x3a, 0x36, 0xe, 0x13, 0x1a, 0x15, 0x16, 0x99]


def bits(r, n):
    return [r.choice([0, 1]) for _ in range(n)]


def deltas(r):
    return [r.randrange(-2, 3) for _ in range(r.randrange(0, 4))] + [0]


def mm_case(r, mode, x=None, y=None):
    x = r.randrange(8, 113) if x is None else x
    y = r.randrange(8, 113) if y is None else y
    k = r.random()
    if k < 0.2:
        sx, sy = -1, -1
    elif k < 0.3:
        sx, sy = -1, r.randrange(8, 113)
    elif k < 0.4:
        sx, sy = r.randrange(8, 113), -1
    elif k < 0.5:
        sx, sy = x, y
    elif k < 0.6:
        sx, sy = x, r.randrange(8, 113)
    elif k < 0.7:
        sx, sy = r.randrange(8, 113), y
    elif k < 0.8:
        d = r.randrange(0, 6)
        sx, sy = x + r.choice([d, -d]), y + r.choice([d, -d])
    else:
        sx, sy = r.randrange(8, 113), r.randrange(8, 113)
    e0e0 = r.randrange(1, 4)
    t0 = r.random() < 0.3
    g = {"C214": r.randrange(3), "C210": r.randrange(3), "C20C": r.randrange(3), "C208": r.randrange(3), "C204": r.randrange(3),
         "C200": r.randrange(3), "DB54": r.randrange(-5, 200), "DB50": r.randrange(-5, 200), "DE0B8": r.randrange(9),
         "DE0B4": r.randrange(9), "4804": sx, "4800": sy, "CostFree": r.choice([0, 1, 7]), "E0E0": e0e0,
         "E0DC": r.choice([0, 1, 1, 5]), "9F34": e0e0 if r.random() < 0.2 else r.randrange(0, 4),
         "9F18": r.randrange(-1, 120), "9F1C": r.randrange(-1, 120), "47C8": r.randrange(3), "DB3C": r.randrange(-5, 200),
         "DB38": r.randrange(-5, 200), "F9BD4": r.randrange(3), "9F38": r.randrange(4, 12), "9F3C": r.randrange(4, 12),
         "Width": r.randrange(60, 129), "Height": r.randrange(60, 129), "HRight": r.randrange(0, 128),
         "HBottom": r.randrange(0, 128), "DB68": r.randrange(3), "9F40": r.randrange(3),
         "9F24": r.choice([0xFFFF, 0, 1, 3, 5, 0x8000, 0x7FFF]), "9F26": r.randrange(0x10000), "Revise": r.choice([0, 1, 1, 2]),
         "DB58": r.randrange(1, CELLS), "DB5A": r.randrange(1, CELLS), "E090": r.choice([0, 1, 0x80]),
         "9F14": r.choice([-1, -1, 0, 5]), "DB60": r.randrange(3), "Recording": r.choice([0, 0, 1])}
    slots = {n: r.randrange(-3, 4) for n in range(38, 46)}
    slots[18] = r.randrange(-50, 1000)
    slots[46] = r.choice([-1, 0, 1, 5])
    slots[107] = r.randrange(-100, 5000)
    others = [m for m in MODES if m != mode]
    modeq = [mode]
    if r.random() < 0.15:
        modeq = [mode] * r.randrange(1, 9) + [r.choice(others)]
    q = {
        "Mode": modeq,
        "GetMode": [r.choice([1, 3, 0x39, 0x36, 0xb, 0x1a, 0x200]) for _ in range(r.randrange(1, 5))],
        "GetLastMode": [r.choice([1, 3, 0x38, 0x22])],
        "LineDX": deltas(r), "LineDY": deltas(r),
        "Bought": [r.choice([0, 1])],
        "Balance": [r.randrange(-100, 5000)],
        "SessionCost": [r.randrange(-100, 5000)],
        "XYValid": bits(r, 3),
        "Rotation": [r.choice([0, 0x10e, 0xb4, 0x5a, 7]) for _ in range(4)],
        "BX": [r.randrange(-2, 3)], "BY": [r.randrange(-2, 3)], "BX2": [r.randrange(-2, 3)], "BY2": [r.randrange(-2, 3)],
        "IsTrackX": bits(r, 1), "TrackIsTrack": bits(r, 1),
        "Direction": [r.choice([0x10, 4, 1, 0x40, 0, 2, 8])],
        "PlaceRide": bits(r, 2),
        "Legal": bits(r, 1),
        "Segment": [r.choice([-1, 0, 1, 3, 5, 0x7FFF, -0x8000])],
        "Conn": [r.choice([9, 9, 0, 1, 8])],
        "Pylon": bits(r, 12),
        "AreaX0": [r.randrange(0, 4)], "AreaY0": [r.randrange(0, 4)],
        "Flag40": bits(r, 8), "Flag20": bits(r, 8), "Hoarding": bits(r, 8), "MapIsTrack": bits(r, 8), "Entrance": bits(r, 8),
        "Exit": bits(r, 8), "Queue": bits(r, 8), "IsRide": bits(r, 8), "Freeform": bits(r, 8), "Path": bits(r, 8),
        "PylonIndex": [r.choice([-1, 0, 5, 0x7FFF, -0x8000])],
        "ThingId": [r.randrange(0 if t0 else 1, 8)],
    }
    for key in list(q):
        if key in ("Pylon", "Flag40", "Hoarding", "MapIsTrack", "Entrance", "Exit", "Queue", "Freeform"):
            q[key] = [v if r.random() < 0.5 else 0 for v in q[key]] + [0]
    q["AreaX1"] = [q["AreaX0"][0] + r.randrange(0, 4)]
    q["AreaY1"] = [q["AreaY0"][0] + r.randrange(0, 4)]
    things = [(r.choice([3, 3, 1, 4]), r.randrange(0, 40)) for _ in range(8)]
    return {"args": [x, y, r.choice([0, 1, 0x200, 0xFFFF])], "g": g, "slots": slots, "rot": r.choice([0, 0xb4, 0x5a, 0x10e, 1]),
            "things": things, "t0": t0, "queues": q}


def mm_edges():
    """Every mode, each with variants that turn the branches both ways (drag start -1 or set, legal or not, the ride words)."""
    r = random.Random(5633)
    for mode in MODES:
        for _ in range(12):
            yield mm_case(r, mode)
    # The hoarding clamps: below the low bounds and above Width - 2 / Height - 3.
    for x, y, w, h in [(5, 6, 100, 100), (120, 120, 100, 90), (8, 120, 128, 60), (99, 97, 101, 100)]:
        c = mm_case(r, 0x39, x, y)
        c["g"].update({"9F38": 8, "9F3C": 7, "Width": w, "Height": h, "4804": 20, "4800": 30})
        yield c
    # Track edit at (0, 0) does nothing.
    c = mm_case(r, 0xe, 0, 0)
    c["queues"]["XYValid"] = [0]
    yield c
    # Ride mode: same ride as before, no track, free ticket, recording, every rotation of the current object.
    for rot in (0, 0xb4, 0x5a, 0x10e, 3):
        c = mm_case(r, 4)
        c["g"].update({"E0E0": 2, "9F34": 1, "E0DC": 1, "Recording": 0})
        c["rot"] = rot
        c["queues"]["XYValid"] = [1]
        yield c
    c = mm_case(r, 4)
    c["g"].update({"E0E0": 2, "9F34": 2})
    yield c
    # The track edit drag: the halfwords equal, no trace (0xffff), the revise answer 0 with and without the mode change.
    for db68, f24, revise, modeq in [(1, 1, 1, [0x1a]), (0, 0xFFFF, 1, [0x1a]), (0, 3, 0, [0x1a]), (0, 3, 0, [0x1a] * 15 + [0x19]),
                                     (0, 5, 1, [0x1a]), (0, 0x8000, 1, [0x1a])]:
        c = mm_case(r, 0x1a)
        c["g"].update({"DB68": db68, "9F40": 1 - db68 if db68 == 0 else db68, "9F24": f24, "Revise": revise})
        c["queues"]["Mode"] = modeq
        yield c
    # The cursor word: the cell equal to the drag start.
    for mode in (0, 0xe, 0x99):
        c = mm_case(r, mode, 40, 50)
        c["g"].update({"4804": 40, "4800": 50})
        yield c


def mm_random(r):
    return mm_case(r, r.choice(MODES))


TESTS = [
    FnTest("LLCM_MouseMoved", ppc, host, edge=mm_edges, rnd=mm_random),
]
