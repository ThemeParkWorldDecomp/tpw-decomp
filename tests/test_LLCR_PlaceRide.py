"""Unit v5635: LLCR_PlaceRide, placing the selected ride on the map (mark, check or preview its cells, the entrance and exit queue
cells, the two track blocks of a kart or water ride), against libtpw_unit_LLCR_PlaceRide.dylib (tests/unit_LLCR_PlaceRide.cpp), main
app ("SimTheme Park"), src/LLCC_DrawKartBlock_LLCR_PlaceRide.cpp:
  LLCR_PlaceRide 0x1008a248

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee is a
fake on both sides that records its arguments and answers from the case's queues: map cells as their byte offset from the grid (world +
0x2d8), track cells from the track grid (world + 0x2d8 + 0x110000), NULL as KNULL. ADD_DIRECTION, OPPOSITE and CMap::IsXYValid compute
their answer (a map of 128 by ROWS cells), the ride cell kinds and directions come from the case's 8 by 8 tables per footprint square
(0 outside; tpw_pr_set_grid on the host), LLCR_GetWidthHeight and LLCR_OffsetEndOfTrack write their out parameters from the queues.

The world of a case: the map grid's rows 0 to ROWS-1 (each cell's mType and mParentID; the rest zero) and the cell 0xff7f an exit without
an entrance reads (x = y = -1), gGameWorld, the ride the AddRideToDB fake answers (its bytes +4 and +6), the RideInfo GetRideInfo answers
(the eight adjust slots), the current object (gLLCCDB6C), the selected ride's globals, the sound bank, the recorder's flag, and random
starting values of everything the function writes. What is compared: the answer (1 for &gLLCRPlacement), the event log, gLLCCFirstBlockX/Y,
gLLCCE0CC, gLLCCC214, gLLCRPlacement (all 0x14 bytes), gLLCRPlaceRotation, gLLCRPlaceY, gLLCRPlaceX.

No fused operations; the two fctiwz (10.0f * x, 10.0f * y) see small integers. Not tested: a footprint with no square (width or height
below 1) together with a track ride (LLCC_SystemGetRideTrack 1 or 2): the original passes its caller's r26 to LLCC_DrawKartBlock then.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCR_PlaceRide"
MASK = 0xFFFFFFFF

# ---- the unit's own function
FN_PLACE = 0x1008a248
SIZES = {FN_PLACE: 6572}
OWN = set(SIZES)

# ---- the callees faked on both sides
SET_RIDE = 0x1008bc20
GET_WH = 0x1008bdcc
IS_XY_VALID = 0x101012f4
GET_RIDE_CELL = 0x1008bccc
MODIFY = 0x10097538
SET_TOP_LEFT = 0x1008bd68
ADD_RIDE = 0x1008be1c
SOUND = 0x100dd9fc
SET_CUR_OBJ = 0x10088590
GET_RIDE_DIR = 0x1008bd14
DELETE = 0x1009d0f4
PARENT_ID = 0x10099930
DELETE_MESH = 0x100a5098
T_UNSET_FLAG = 0x100a29cc
M_SET_FLAG = 0x1009bbb8
CALC_N = 0x10099998
GET_N = 0x100816e8
GET_NB_CELL = 0x10103814
ADD_DIR = 0x1010254c
MOD_DIR = 0x10081810
GET_DIR = 0x10081930
OPPOSITE = 0x10102500
IS_RIDE = 0x1009c61c
T_SET_FLAG = 0x100a2964
RIDE_QUEUE = 0x10091a08
MOD_NB = 0x10081608
BACK_OF_QUEUE = 0x1010952c
RIDE_TRACK = 0x10091ad0
GET_RIDE_INFO = 0x10154868
KART = 0x10089cfc
OFFSET_END = 0x1008c6d8

# ---- the Mac's addresses of the globals; the host library's come from tpw_pr_addr(name)
ADDR = dict(first_x=0x1029e02c, first_y=0x1029e030, e0cc=0x102de0cc, c214=0x102dc214, db6c=0x102ddb6c, e0dc=0x102de0dc,
            e0e0=0x102de0e0, c47c4=0x102e47c4, bank3=0x102f7c94 + 0xc, world=0x1037ebfc, recording=0x1037ec44 + 0x24,
            ridedb=0x1037f6c8, place=0x102de0a0, d0=0x102de0d0, d4=0x102de0d4, d8=0x102de0d8)
HOST_NAME = dict(c47c4="47c4")
# ---- the host's offsets and sizes the module relies on: CMapCell, CTrackCell, mType, mParentID, LLCRPlacement, its direction byte
HOST_LAYOUT = [0x44, 0x28, 8, 0x10, 0x14, 0x10]

KNULL = -1000000
ROWS = 6                              # the rows of the map IsXYValid lets in (128 columns)
CELL = 0x44
GRID = 0x2d8                          # the map grid in the world
TRACK = GRID + 0x110000               # the track grid
DOOR = 0xff7f                         # the cell index of x = y = -1
WORLD_SIZE = GRID + 0x10000 * CELL + 0x100
L_RIDEDB, L_CUROBJ = 5, 7
SLOTS = list(range(38, 46))           # North X/Y, East X/Y, South X/Y, West X/Y adjust
INFO_BASE = 0x0c                      # the Mac's RideInfo: slot s's integer at +0x0c + 4 * s

GRID_N = 8                            # the footprint squares the kind and direction tables hold (row major), 0 outside
STATE = {}                            # the PowerPC world block (persistent) and the case's blocks and tables


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cell(e, p):
    p &= MASK
    return KNULL if p == 0 else s32(p - (STATE["world"] + GRID))


def track(e, p):
    p &= MASK
    return KNULL if p == 0 else s32(p - (STATE["world"] + TRACK))


def add_direction(a, b):
    n = 0
    first = a & 0xff
    while first > 1:
        first >>= 1
        n += 1
    v = (b & 0xff) << n
    return (v >> 8) & 0xff if v > 0x80 else v & 0xff


def opposite(d):
    d &= 0xff
    if d == 0:
        return 0
    if d < 0x10:
        return (d << 4) & 0xf0
    return d >> 4


def answer(e, v):
    e.set_r(3, v & MASK)


def st_set_ride(e):
    e.cl.rec("SetRide", ((e.r(3) >> 16) & 0xffff,))


def st_get_wh(e):
    e.cl.rec("GetWH", ())
    pw, ph = e.r(3), e.r(4)
    e.write(pw, struct.pack(">I", e.cl.ret("W") & MASK))
    e.write(ph, struct.pack(">I", e.cl.ret("H") & MASK))


def st_is_xy_valid(e):
    x, y = s32(e.r(3)), s32(e.r(4))
    e.cl.rec("IsXYValid", (x, y))
    answer(e, 1 if 0 <= x < 128 and 0 <= y < ROWS else 0)


def grid_at(grid, col, row):
    return grid[row * GRID_N + col] if 0 <= col < GRID_N and 0 <= row < GRID_N else 0


def st_grc(e):
    col, row = s32(e.r(3)), s32(e.r(4))
    e.cl.rec("GRC", (col, row))
    answer(e, grid_at(STATE["kinds"], col, row))


def st_grd(e):
    col, row = s32(e.r(3)), s32(e.r(4))
    e.cl.rec("GRD", (col, row))
    answer(e, grid_at(STATE["dirs"], col, row) & 0xff)


def st_modify(e):
    e.cl.rec("Modify", (cell(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    answer(e, e.cl.ret("Modify"))


def st_set_top_left(e):
    e.cl.rec("SetTopLeft", ((e.r(3) >> 16) & 0xffff,))


def st_add_ride(e):
    e.cl.rec("AddRide", (s32(e.r(3)),))
    answer(e, STATE["ride"])


def st_sound(e):
    e.cl.rec("Sound", tuple(s32(e.r(i)) for i in range(3, 9)))
    answer(e, 0)


def st_set_cur_obj(e):
    e.cl.rec("SetCurObj", (e.r(3) & 0xff,))


def st_delete(e):
    e.cl.rec("Delete", (cell(e, e.r(3)), s32(e.r(4)), s32(e.r(5))))


def st_parent_id(e):
    e.cl.rec("ParentID", (cell(e, e.r(3)), (e.r(4) >> 16) & 0xffff))


def st_delete_mesh(e):
    e.cl.rec("DeleteMesh", (track(e, e.r(3)),))


def st_t_unset_flag(e):
    e.cl.rec("TUnSetFlag", (track(e, e.r(3)), e.r(4) & 0xffff))


def st_t_set_flag(e):
    e.cl.rec("TSetFlag", (track(e, e.r(3)), e.r(4) & 0xffff))


def st_m_set_flag(e):
    e.cl.rec("MSetFlag", (cell(e, e.r(3)), e.r(4) & 0xffff))


def st_calc_n(e):
    e.cl.rec("CalcN", (cell(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), e.r(7) & 0xff))


def st_get_n(e):
    e.cl.rec("GetN", (cell(e, e.r(3)),))
    answer(e, e.cl.ret("GetN") & 0xff)


def st_get_nb_cell(e):
    e.cl.rec("GNC", (cell(e, e.r(3)), e.r(4) & 0xff))
    k = e.cl.ret("GNC")
    answer(e, 0 if k == 0 else STATE["world"] + GRID + (k - 1) * CELL)


def st_add_dir(e):
    a, b = e.r(3) & 0xff, e.r(4) & 0xff
    e.cl.rec("AddDir", (a, b))
    answer(e, add_direction(a, b))


def st_opposite(e):
    d = e.r(3) & 0xff
    e.cl.rec("Opp", (d,))
    answer(e, opposite(d))


def st_mod_dir(e):
    e.cl.rec("ModDir", (cell(e, e.r(3)), e.r(4) & 0xff))


def st_get_dir(e):
    e.cl.rec("GetDir", (cell(e, e.r(3)),))
    answer(e, e.cl.ret("GetDir") & 0xff)


def st_is_ride(e):
    e.cl.rec("IsRide", (cell(e, e.r(3)),))
    answer(e, e.cl.ret("IsRide"))


def st_ride_queue(e):
    e.cl.rec("RideQueue", ())
    answer(e, e.cl.ret("RideQueue"))


def st_mod_nb(e):
    e.cl.rec("ModNb", (cell(e, e.r(3)), e.r(4) & 0xff))


def st_back_of_queue(e):
    e.cl.rec("BackOfQueue", (e.cl.lab(e.r(3)),))


def st_ride_track(e):
    e.cl.rec("RideTrack", ())
    answer(e, e.cl.ret("RideTrack"))


def st_get_ride_info(e):
    e.cl.rec("GetRideInfo", (e.cl.lab(e.r(3)), (e.r(4) >> 16) & 0xffff))
    answer(e, STATE["info"])


def st_kart(e):
    e.cl.rec("Kart", tuple(s32(e.r(i)) for i in range(3, 8)) + (e.r(8) & 0xff,))
    answer(e, e.cl.ret("Kart"))


def st_offset_end(e):
    px, py = e.r(3), e.r(4)
    e.cl.rec("OffsetEnd", (e.s32(px), e.s32(py), s32(e.r(5))))
    e.write(px, struct.pack(">I", e.cl.ret("OEX") & MASK))
    e.write(py, struct.pack(">I", e.cl.ret("OEY") & MASK))
    answer(e, 0)


for _addr, _fn in [(SET_RIDE, st_set_ride), (GET_WH, st_get_wh), (IS_XY_VALID, st_is_xy_valid), (GET_RIDE_CELL, st_grc),
                   (MODIFY, st_modify), (SET_TOP_LEFT, st_set_top_left), (ADD_RIDE, st_add_ride), (SOUND, st_sound),
                   (SET_CUR_OBJ, st_set_cur_obj), (GET_RIDE_DIR, st_grd), (DELETE, st_delete), (PARENT_ID, st_parent_id),
                   (DELETE_MESH, st_delete_mesh), (T_UNSET_FLAG, st_t_unset_flag), (M_SET_FLAG, st_m_set_flag),
                   (CALC_N, st_calc_n), (GET_N, st_get_n), (GET_NB_CELL, st_get_nb_cell), (ADD_DIR, st_add_dir),
                   (MOD_DIR, st_mod_dir), (GET_DIR, st_get_dir), (OPPOSITE, st_opposite), (IS_RIDE, st_is_ride),
                   (T_SET_FLAG, st_t_set_flag), (RIDE_QUEUE, st_ride_queue), (MOD_NB, st_mod_nb),
                   (BACK_OF_QUEUE, st_back_of_queue), (RIDE_TRACK, st_ride_track), (GET_RIDE_INFO, st_get_ride_info),
                   (KART, st_kart), (OFFSET_END, st_offset_end)]:
    STUBS.internal(_addr, _fn)


def setup(runner):
    """The world block (4.4 MB, more than the persistent region holds) gets its own mapping, outside everything the image uses."""
    T.install_std_stubs(runner.emu)
    size = (WORLD_SIZE + 0xFFFF) & ~0xFFFF
    for base in (0x50000000, 0x58000000, 0x68000000, 0x78000000, 0x48000000):
        try:
            runner.emu.uc.mem_map(base, size)
        except Exception:
            continue
        STATE["world"] = base
        return
    raise MemoryError("no room for the world block")


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def word(self, a, v):
        self.words(a, [v])

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xffff))

    def byte(self, a, v):
        self.write(a, bytes([v & 0xff]))

    def rd_byte(self, a):
        return self.read(a, 1)[0]


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.world = STATE["world"]
        self.curobj = e.alloc(0x40)
        self.ride = STATE["ride"] = e.alloc(0x10)
        self.info = STATE["info"] = e.alloc(0xd0)
        STATE["kinds"], STATE["dirs"] = case["kinds"], case["dirs"]
        self.L = e.cl = CL.PpcLog({ADDR["ridedb"]: L_RIDEDB, self.curobj: L_CUROBJ}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a, v):
        self.word(a, v)

    def set_curobj(self, on):
        self.word(self.g("db6c"), self.curobj if on else 0)

    def set_slots(self, slots):
        for s, v in zip(SLOTS, slots):
            self.word(self.info + INFO_BASE + 4 * s, v)

    def log(self):
        return self.L.log

    def call(self, args):
        r3, _, _ = T.call_ppc(self.e, FN_PLACE, [a & MASK for a in args])
        r3 &= MASK
        return 1 if r3 == ADDR["place"] else (0 if r3 == 0 else -r3)


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        if not getattr(lib, "pr_world", 0):
            lib.pr_world = lib.tpw_pr_alloc(WORLD_SIZE)
            lib.pr_curobj = lib.tpw_pr_alloc(0x40)
            lib.pr_ride = lib.tpw_pr_alloc(0x10)
            lib.pr_info = lib.tpw_pr_alloc(lib.tpw_pr_rideinfo_size())
        self.world, self.curobj, self.ride, self.info = lib.pr_world, lib.pr_curobj, lib.pr_ride, lib.pr_info
        lib.tpw_pr_set_answers(self.ride, self.info)
        n = GRID_N * GRID_N
        lib.tpw_pr_set_grid((C.c_int32 * n)(*[s32(v) for v in case["kinds"]]), (C.c_int32 * n)(*[s32(v) for v in case["dirs"]]))
        CL.host_begin(lib, {L_RIDEDB: self.g("ridedb"), L_CUROBJ: self.curobj}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_pr_addr(HOST_NAME.get(name, name).encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def set_curobj(self, on):
        self.ptr(self.g("curobj"), self.curobj if on else 0)
        self.word(self.g("db6c"), (self.curobj & MASK) if on else 0)

    def set_slots(self, slots):
        for s, v in zip(SLOTS, slots):
            self.lib.tpw_pr_set_slot(self.info, s, v)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, args):
        p = self.lib.tpw_pr_place(*[s32(a) for a in args])
        return 1 if p == self.g("place") else (0 if p == 0 else -p)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_pr_addr.restype, lib.tpw_pr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_pr_alloc.restype, lib.tpw_pr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pr_layout.restype, lib.tpw_pr_layout.argtypes = C.c_int32, [C.POINTER(C.c_int64)]
    lib.tpw_pr_rideinfo_size.restype, lib.tpw_pr_rideinfo_size.argtypes = C.c_int32, []
    lib.tpw_pr_set_answers.restype, lib.tpw_pr_set_answers.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_pr_set_grid.restype, lib.tpw_pr_set_grid.argtypes = None, [C.POINTER(C.c_int32), C.POINTER(C.c_int32)]
    lib.tpw_pr_set_slot.restype, lib.tpw_pr_set_slot.argtypes = None, [C.c_uint64, C.c_int32, C.c_int32]
    lib.tpw_pr_place.restype, lib.tpw_pr_place.argtypes = C.c_uint64, [C.c_int32] * 6
    out = (C.c_int64 * 32)()
    n = lib.tpw_pr_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def cells_bytes(s, rng, n):
    """n map cells with a random mType (+8) and mParentID (+0x10), the rest zero, in the side's byte order."""
    out = bytearray(n * CELL)
    e = s.fmt[0]
    for i in range(n):
        struct.pack_into(e + "I", out, i * CELL + 8, rng.choice([1, 1, 1, 0, 2, 3, 0x15]))
        struct.pack_into(e + "H", out, i * CELL + 0x10, rng.randrange(0x10000))
    return bytes(out)


def build(s, case):
    """The case's globals and blocks through the Side helpers: the same code for both memories (offsets are the Mac's)."""
    rng = random.Random(case["world"])
    s.write(s.world + GRID, cells_bytes(s, rng, 128 * ROWS))
    s.write(s.world + GRID + DOOR * CELL, cells_bytes(s, rng, 1))
    s.ptr(s.g("world"), s.world)
    g = case["g"]
    s.word(s.g("first_x"), g["first_x"])
    s.word(s.g("first_y"), g["first_y"])
    s.byte(s.g("e0cc"), g["e0cc"])
    s.word(s.g("c214"), g["c214"])
    s.word(s.g("e0dc"), g["e0dc"])
    s.half(s.g("e0e0"), g["e0e0"])
    s.byte(s.g("c47c4"), g["c47c4"])
    s.word(s.g("bank3"), g["bank3"])
    s.word(s.g("recording"), g["recording"])
    s.words(s.g("place"), g["place"][:4])
    s.write(s.g("place") + 0x10, bytes(g["place_tail"]))
    s.word(s.g("d0"), g["d0"])
    s.word(s.g("d4"), g["d4"])
    s.word(s.g("d8"), g["d8"])
    s.set_curobj(case["curobj"])
    s.write(s.ride, bytes(case["ride"]))
    s.set_slots(case["slots"])


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides."""
    return {"answer": answer, "log": s.log(),
            "first": [s32(v) for v in s.rd_words(s.g("first_x"), 1) + s.rd_words(s.g("first_y"), 1)],
            "e0cc": s.rd_byte(s.g("e0cc")), "c214": s32(s.rd_word(s.g("c214"))),
            "place": [s32(v) for v in s.rd_words(s.g("place"), 4)], "place_tail": list(s.read(s.g("place") + 0x10, 4)),
            "d0d4d8": [s32(s.rd_word(s.g(n))) for n in ("d0", "d4", "d8")]}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["args"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

KINDS = [0, 1, 4, 9, 10, 11, 16, 23, 2, 24, -1, 3, 0x17, 5]
DIRS = [0, 1, 4, 0x10, 0x40, 2, 0x80, 0x41]
ROTS = [0, 0x5a, 0xb4, 0x10e, 1, -0x5a]


def make(r, rot=None, build=None, check=None, kind=None, w=None, h=None, x=None, y=None, track=None, rq=None, modify=None,
         kart=None, **g):
    if w is None:
        w = r.choice([1, 2, 3, 4, 5, 5, 3, 2]) if r.random() < 0.93 else r.choice([-1, 0, 6, 7, 8])
    if h is None:
        h = r.choice([1, 2, 3, 4, 5, 5, 3, 2]) if r.random() < 0.93 else r.choice([-1, 0, 6, 7, 8])
    if track is None:
        track = r.choice([0, 1, 2, 3, -1, 1, 2])
        if w <= 0 or h <= 0:
            track = r.choice([0, 3, -1])
    q = {"W": [w], "H": [h], "RideTrack": [track]}
    kinds, dirs = [], []
    for row in range(GRID_N):
        for col in range(GRID_N):
            if callable(kind):
                kinds.append(kind(col, row))
            elif kind is not None and r.random() < 0.7:
                kinds.append(kind)
            else:
                kinds.append(r.choice(KINDS))
            dirs.append(r.choice(DIRS))
    p1 = r.choice([0.5, 0.8, 0.95, 1.0])
    q["Modify"] = modify if modify is not None else [1 if r.random() < p1 else 0 for _ in range(60)]
    q["GetN"] = [r.choice([0, 0xff, 0x55, 1, 4, 0x10, 0x40]) for _ in range(20)]
    q["GNC"] = [0 if r.random() < 0.2 else r.randrange(1, 128 * ROWS + 1) for _ in range(30)]
    q["GetDir"] = [r.choice(DIRS) for _ in range(6)]
    q["IsRide"] = [r.choice([0, 0, 1]) for _ in range(10)]
    q["RideQueue"] = rq if rq is not None else [r.choice([0, 1, 2]), r.choice([0, 1])]
    q["Kart"] = kart if kart is not None else [r.choice([1, 1, 0, 0x100, 0x101]), r.choice([1, 0, 0x100, 0x2])]
    q["OEX"] = [r.randrange(-50, 200)]
    q["OEY"] = [r.randrange(-50, 50)]
    glob = dict(first_x=r.randrange(1 << 32), first_y=r.randrange(1 << 32), e0cc=r.randrange(256),
                c214=r.choice([0, 0, 5, -1]), e0dc=r.choice([0, 0, 1]), e0e0=r.randrange(0x10000),
                c47c4=r.choice([0, 0, 0, 1, 0x80]), bank3=r.randrange(-100, 100), recording=r.choice([0, 0, 0, 1]),
                place=[r.randrange(1 << 32) for _ in range(4)], place_tail=[r.randrange(256) for _ in range(4)],
                d0=r.randrange(1 << 32), d4=r.randrange(1 << 32), d8=r.randrange(1 << 32))
    glob.update(g)
    args = [r.randrange(-3, 131) if x is None else x,
            r.randrange(-3, ROWS + 4) if y is None else y,
            r.randrange(-5, 5),
            r.choice(ROTS) if rot is None else rot,
            r.choice([0, 1, 2]) if build is None else build,
            r.choice([0, 1]) if check is None else check]
    return {"args": args, "queues": q, "kinds": kinds, "dirs": dirs, "g": glob, "world": r.randrange(1 << 30), "curobj": r.random() < 0.85,
            "ride": [r.randrange(256) for _ in range(16)], "slots": [r.randrange(-8, 8) for _ in SLOTS]}


def edges():
    """Every branch of the asm at least once: each kind under each mode and rotation, the queue paths, the track blocks, the sizes."""
    r = random.Random(5635)
    # every kind under every mode (build, check) and every rotation
    for kind in KINDS:
        for build, check in ((0, 0), (0, 1), (1, 0), (1, 1)):
            for rot in ROTS:
                yield make(r, rot=rot, build=build, check=check, kind=kind, x=r.randrange(3, 125), y=r.randrange(1, 5))
    # the sound's four block counts (all squares kind 4, all on the map), recorder on and off
    for w, h in ((1, 3), (2, 3), (3, 3), (4, 4), (5, 5), (2, 2)):
        for rec in (0, 1):
            yield make(r, rot=0, build=1, check=0, kind=lambda c, rw: 4, w=w, h=h, x=60, y=2, recording=rec, c47c4=0)
    # the entrance and exit with every queue answer, under every mode
    def door(c, rw):
        return 9 if (c, rw) == (1, 0) else 10 if (c, rw) == (2, 2) else 1
    for build, check in ((0, 0), (0, 1), (1, 0), (1, 1)):
        for rq in ([0, 0], [1, 1], [1, 0], [0, 1]):
            for rot in (0, 0x5a, 0xb4, 0x10e):
                yield make(r, rot=rot, build=build, check=check, kind=door, w=3, h=3, x=64, y=2, rq=rq)
    # an exit without an entrance (the door at x = y = -1), an entrance without an exit
    for build, check in ((1, 0), (0, 0), (0, 1)):
        for rq in ([1, 1], [0, 1]):
            yield make(r, rot=0, build=build, check=check, kind=lambda c, rw: 10 if (c, rw) == (0, 0) else 0, w=2, h=2,
                       x=30, y=2, rq=rq)
            yield make(r, rot=0, build=build, check=check, kind=lambda c, rw: 9 if (c, rw) == (0, 1) else 0, w=2, h=2,
                       x=30, y=2, rq=rq)
    # the entrance and exit queue cells off the map
    for x, y in ((0, 0), (127, 5), (0, 5), (127, 0)):
        for build in (0, 1):
            yield make(r, rot=0, build=build, check=0, kind=lambda c, rw: 9 if c == 0 else 10, w=1, h=1, x=x, y=y, rq=[1, 1])
    # the track blocks: every LLCC_SystemGetRideTrack answer, every pair of DrawKartBlock answers, every rotation
    for track in (-1, 0, 1, 2, 3):
        for kart in ([0], [1, 0], [0x100, 1], [1, 1], [0x101, 0x201]):
            for rot in (0, 0x5a, 0xb4, 0x10e, 7):
                yield make(r, rot=rot, track=track, kart=kart, w=r.choice([1, 2, 3, 4, 5]), h=r.choice([1, 2, 3, 4, 5]))
    # the widths and heights that round the margins, and none at all
    for w in (-1, 0, 1, 2, 3, 4, 5, 6, 7, 8):
        for h in (-1, 0, 3, 6, 8):
            yield make(r, w=w, h=h, track=0 if w <= 0 or h <= 0 else None)
    # the placement origin (gLLCC47C4), the current object, the track selection, all Modify answers 0 or 1
    for c47 in (0, 1, 0x80):
        for rot in ROTS:
            yield make(r, rot=rot, c47c4=c47)
    for on in (0, 1):
        yield dict(make(r, rot=0, build=1, check=0, kind=door, w=3, h=3, x=64, y=2, rq=[1, 1]), curobj=bool(on))
    for e0dc in (0, 1):
        yield make(r, e0dc=e0dc)
    for m in ([0], [1]):
        for build, check in ((0, 0), (0, 1), (1, 0)):
            yield make(r, build=build, check=check, modify=m, x=64, y=2)
    # the map's corners
    for x in (-3, -1, 0, 1, 126, 127, 128, 130):
        for y in (-3, -1, 0, 5, 6, 9):
            yield make(r, x=x, y=y)


def random_case(r):
    return make(r)


TESTS = [
    FnTest("LLCR_PlaceRide", ppc, host, edge=edges, rnd=random_case),
]
