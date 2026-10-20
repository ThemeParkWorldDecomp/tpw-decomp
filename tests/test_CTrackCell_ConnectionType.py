"""Unit v5629, CTrackCell::ConnectionType (how a cell of a mode would join a track cell: the cell system's placement test), against
libtpw_unit_CTrackCell_ConnectionType.dylib (tests/unit_CTrackCell_ConnectionType.cpp), main app ("SimTheme Park"),
src/TrackCell_flags_CTrackCell_ConnectionType.cpp:
  CTrackCell::ConnectionType 0x100a1594

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee is a
fake on both sides that records its arguments and answers from the case: the cell predicates (IsRide, IsFreeform, IsExit, IsEntrance,
IsQueue, CMapCell::IsFlag, IsTrackX, IsTrack) per cell label, the others (IsBridge, CTrackCell::IsFlag, IsStraight, IsCorner, IsCrossRoad,
NumberOfNeighbours, OPPOSITE, the costs, the balance) per call; GetMapCellEquiv, both GetNeighbouringCell, GetRideInfo and GetBankAccount
answer cells and blocks by label. The byte answers come back with junk in the high bits on the PowerPC side (the original masks them).

The world of a case: the track cell (id, mode, direction, parent id), its map cell and three map neighbours (mode, parent id), three track
neighbours, six track cells of the world's grid (ids 0 to 5, the world pointer placed so that they are real), six things in gThingArray
(entry 0 empty or not), four ride records (the overwrite priority word), and every global the function reads. What is compared: the
answer, the event log and every global it reads or writes.

No floating point. Not tested: NULL cells (the original reads a neighbour's mode without a check).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CTrackCell_ConnectionType"
MASK = 0xFFFFFFFF

FN = 0x100a1594

# ---- the callees faked on both sides
GET_MAP_CELL_EQUIV = 0x100f49a0
MAP_NBR = 0x101036b0
IS_RIDE, IS_FREEFORM, IS_EXIT, IS_ENTRANCE, IS_QUEUE = 0x1009c61c, 0x1009c59c, 0x1009c524, 0x1009c4dc, 0x1009c490
MAP_IS_FLAG = 0x1009bb70
GET_OBJECT_THING_ID = 0x10103a04
GAME_ASSERTMSG = 0x1000dde8
GET_RIDE_INFO = 0x10154868
KART_COST, WATER_COST, SESSION_COST, ADD_SESSION_COST = 0x10091c54, 0x10091cd4, 0x10091d98, 0x10091d50
SET_ILLEGAL = 0x10088934
TRACK_NBR = 0x1012fcb8
IS_TRACK_X, IS_TRACK, IS_BRIDGE, TRACK_IS_FLAG = 0x100a367c, 0x100a3574, 0x100a34c4, 0x100a2918
IS_STRAIGHT, IS_CORNER, IS_CROSSROAD = 0x100a3868, 0x100a37a0, 0x100a3914
OPPOSITE = 0x10102500
NUMBER_OF_NEIGHBOURS = 0x10081744
GET_BANK_ACCOUNT, GET_BALANCE = 0x1013edfc, 0x100f32b4

# ---- the Mac's addresses of the globals
INT_GLOBALS = dict(gHoardingRight=0x1029e03c, gHoardingBottom=0x1029e040, gHoardingLeft=0x1029e044, gHoardingTop=0x1029e048,
                   gLLCCC200=0x102dc200, gLLCCC204=0x102dc204, gLLCCC20C=0x102dc20c, gLLCCC210=0x102dc210, gLLCCC214=0x102dc214,
                   gLLCCDB40=0x102ddb40, gLLCCDB60=0x102ddb60, gLLCCE0B4=0x102de0b4, gLLCCE0B8=0x102de0b8, gLLCC4858=0x102e4858,
                   gTrackNbrOn485C=0x102e485c, recording=0x1037ec44 + 0x24)
ADDR = dict(INT_GLOBALS, gLLCCDB68=0x102ddb68, gTrackCostFree=0x102ddb35, gThingArray=0x1034cbfc, gGameWorld=0x1037ebfc,
            gRideDatabase=0x1037f6c8)
# ---- the host's offsets and sizes checked against tpw_ct_layout: the cell fields, the cell sizes, the world's grids, the thing's kind and
# the object's type id (the Mac's values: the host keeps them)
HOST_LAYOUT = [0, 8, 0xd, 0x10, 0x28, 0x44, 0x2d8, 0x1102d8, 2, 0xe]
HOST_EXTRA = {}

# ---- labels
THIS = 1
MCELLS = [2, 3, 4, 5]
TCELLS = [6, 7, 8]
WT = 10       # + id: the world's track cell of the id (0 to 5)
WM = 20       # + id: the world's map cell of the id
WORLD = 30
RIDES = [31, 32, 33, 34]
BANK = 35
RIDEDB = 36


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.add(FN)


def to_addr(e, label):
    return e.ct_labels.get(label, 0)


def per_label(name, key=None):
    def fake(e):
        L = e.cl
        lab = L.lab(e.r(3))
        L.rec(name, (lab,))
        e.set_r(3, L.ret("%s:%d" % (key or name, lab)) & MASK)
    return fake


def per_call(name, byte=False):
    def fake(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)),))
        v = L.ret(name)
        e.set_r(3, ((v & 0xFF) | 0x5A3C0000) if byte else (v & MASK))
    return fake


def no_arg(name):
    def fake(e):
        L = e.cl
        L.rec(name, ())
        e.set_r(3, L.ret(name) & MASK)
    return fake


def one_int(name):
    def fake(e):
        e.cl.rec(name, (s32(e.r(3)),))
    return fake


def map_equiv(e):
    L = e.cl
    L.rec("GetMapCellEquiv", (L.lab(e.r(3)),))
    e.set_r(3, to_addr(e, L.ret("GetMapCellEquiv")))


def neighbour(name):
    def fake(e):
        L = e.cl
        p = e.r(4)
        L.rec(name, (L.lab(e.r(3)), e.s32(p), e.s32(p + 4), s32(e.r(5))))
        e.set_r(3, to_addr(e, L.ret(name)))
    return fake


def map_is_flag(e):
    L = e.cl
    lab, flag = L.lab(e.r(3)), e.r(4) & 0xFFFF
    L.rec("MapIsFlag", (lab, flag))
    e.set_r(3, L.ret("%s:%d" % ("MapIsFlag40" if flag == 0x40 else "MapIsFlag20", lab)) & MASK)


def track_is_flag(e):
    L = e.cl
    flag = e.r(4) & 0xFFFF
    L.rec("TrackIsFlag", (L.lab(e.r(3)), flag))
    e.set_r(3, L.ret("TrackIsFlag40" if flag == 0x40 else "TrackIsFlag10") & MASK)


def get_object_thing_id(e):
    """The hidden halfword result at r3, the cell in r4."""
    L = e.cl
    lab = L.lab(e.r(4))
    L.rec("GetObjectThingId", (lab,))
    e.write(e.r(3), struct.pack(">H", L.ret("GetObjectThingId:%d" % lab) & 0xFFFF))


def get_ride_info(e):
    """The id is a two-byte struct passed in the high half of r4."""
    L = e.cl
    L.rec("GetRideInfo", (L.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, to_addr(e, L.ret("GetRideInfo")))


def opposite(e):
    L = e.cl
    L.rec("OPPOSITE", (e.r(3) & 0xFF,))
    e.set_r(3, (L.ret("OPPOSITE") & 0xFF) | 0x7E120000)


def get_bank_account(e):
    L = e.cl
    L.rec("GetBankAccount", (L.lab(e.r(3)),))
    e.set_r(3, to_addr(e, L.ret("GetBankAccount")))


for _addr, _fn in [
    (GET_MAP_CELL_EQUIV, map_equiv),
    (MAP_NBR, neighbour("MapNbr")),
    (IS_RIDE, per_label("IsRide")),
    (IS_FREEFORM, per_label("IsFreeform")),
    (IS_EXIT, per_label("IsExit")),
    (IS_ENTRANCE, per_label("IsEntrance")),
    (IS_QUEUE, per_label("IsQueue")),
    (MAP_IS_FLAG, map_is_flag),
    (GET_OBJECT_THING_ID, get_object_thing_id),
    (GAME_ASSERTMSG, CL.recorder("GAME_ASSERTMSG", "is")),
    (GET_RIDE_INFO, get_ride_info),
    (KART_COST, no_arg("GetKartTrackCost")),
    (WATER_COST, no_arg("GetWaterTrackCost")),
    (SESSION_COST, no_arg("GetSessionCost")),
    (ADD_SESSION_COST, one_int("AddToSessionCost")),
    (SET_ILLEGAL, one_int("SetIllegalPlacement")),
    (TRACK_NBR, neighbour("TrackNbr")),
    (IS_TRACK_X, per_label("IsTrackX")),
    (IS_TRACK, per_label("IsTrack")),
    (IS_BRIDGE, per_call("IsBridge")),
    (TRACK_IS_FLAG, track_is_flag),
    (IS_STRAIGHT, per_call("IsStraight", True)),
    (IS_CORNER, per_call("IsCorner", True)),
    (IS_CROSSROAD, per_call("IsCrossRoad", True)),
    (OPPOSITE, opposite),
    (NUMBER_OF_NEIGHBOURS, per_call("NumberOfNeighbours")),
    (GET_BANK_ACCOUNT, get_bank_account),
    (GET_BALANCE, per_call("GetBalance")),
]:
    STUBS.internal(_addr, _fn)


def setup(runner):
    """The three compass offsets are filled by a static initialiser on the Mac (the host's are constants): (1, 0), (1, 1), (0, 1)."""
    T.install_std_stubs(runner.emu)
    for a, (x, y) in ((0x1034c284, (1, 0)), (0x1034c270, (1, 1)), (0x1034c25c, (0, 1))):
        runner.emu.write(a, struct.pack(">ii", x, y))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def word(self, a, w):
        self.write(a, struct.pack(self.fmt, w & MASK))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def byte(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    map_size = 0x44
    track_size = 0x28
    entry_size = 0x14
    ride_size = 0x58
    ride_value = 0x54
    map_base = 0x2d8
    track_base = 0x1102d8

    def __init__(self, e, case):
        self.e = e
        self.case = case

    def begin(self, labels):
        self.e.ct_labels = dict(labels)
        self.L = self.e.cl = CL.PpcLog({a: l for l, a in labels.items()}, self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if hasattr(self, "saved"):
            CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a, v):
        self.word(a, v)

    def wrap(self, v):
        return v & MASK

    def log(self):
        return self.L.log

    def call(self, cell, mode, a, b):
        r3, _, _ = T.call_ppc(self.e, FN, [cell & MASK, mode & MASK, a & MASK, b & MASK])
        return s32(r3)


class Host(Side):
    fmt = "<I"
    map_base = 0x2d8
    track_base = 0x1102d8

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)
        self.map_size = HOST_LAYOUT[5]
        self.track_size = HOST_LAYOUT[4]
        self.entry_size = HOST_EXTRA["entry"]
        self.ride_size = HOST_EXTRA["ride"]
        self.ride_value = HOST_EXTRA["value"]

    def begin(self, labels):
        for l in range(1, 64):
            self.lib.tpw_ct_label(l, labels.get(l, 0))
        CL.host_begin(self.lib, dict(labels), self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ct_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ct_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v & 0xFFFFFFFFFFFFFFFF))

    def wrap(self, v):
        return v & 0xFFFFFFFFFFFFFFFF

    def log(self):
        return CL.host_end(self.lib)

    def call(self, cell, mode, a, b):
        return self.lib.tpw_ct_call(cell, s32(mode), s32(a), b & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ct_addr.restype, lib.tpw_ct_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ct_alloc.restype, lib.tpw_ct_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ct_label.restype, lib.tpw_ct_label.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_ct_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ct_call.restype, lib.tpw_ct_call.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ct_layout(out)
    got = list(out)[:n]
    assert got[:len(HOST_LAYOUT)] == HOST_LAYOUT, got
    HOST_EXTRA.update(entry=got[10], ride=got[11], value=got[12])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def cell(s, a, c):
    """A cell's fields at the Mac's offsets: id, mode, direction, parent id."""
    s.half(a + 0, c.get("id", 0))
    s.word(a + 8, c["type"])
    s.byte(a + 0xd, c.get("dir", 0))
    s.half(a + 0x10, c["parent"])


def build(s, case):
    """The case's memory; answers the label -> address map."""
    w = case["w"]
    labels = {}
    this = s.alloc(0x28)
    cell(s, this, w["this"])
    labels[THIS] = this
    for lab, c in zip(MCELLS, w["mcells"]):
        a = s.alloc(max(s.map_size, 0x44))
        cell(s, a, c)
        labels[lab] = a
    for lab, c in zip(TCELLS, w["tcells"]):
        a = s.alloc(0x28)
        cell(s, a, c)
        labels[lab] = a
    tb = s.alloc(6 * s.track_size)
    for i, p in enumerate(w["tb"]):
        cell(s, tb + i * s.track_size, {"type": 0xb, "parent": p})
        labels[WT + i] = tb + i * s.track_size
    world = s.wrap(tb + s.track_size - s.track_base)
    labels[WORLD] = world
    for i in range(6):
        labels[WM + i] = s.wrap(world + s.map_base + (i - 1) * s.map_size)
    for lab, v in zip(RIDES, w["rides"]):
        a = s.alloc(s.ride_size)
        s.word(a + s.ride_value, v)
        labels[lab] = a
    labels[BANK] = s.alloc(0x10)
    labels[RIDEDB] = s.g("gRideDatabase")
    s.ptr(s.g("gGameWorld"), world)
    table = s.g("gThingArray")
    for i, (kind, obj) in enumerate(w["things"]):
        if i == 0 and not w["entry0"]:
            s.ptr(table, 0)
            continue
        t = s.alloc(0x20)
        s.byte(t + 2, kind)
        s.half(t + 0xe, obj)
        s.ptr(table + i * s.entry_size, t)
    for name in INT_GLOBALS:
        s.word(s.g(name), w["g"][name])
    s.half(s.g("gLLCCDB68"), w["g"]["gLLCCDB68"])
    s.byte(s.g("gTrackCostFree"), w["g"]["gTrackCostFree"])
    return labels, this


def observe(s, answer):
    out = {"answer": answer, "log": s.log()}
    for name in INT_GLOBALS:
        out[name] = s.rd_word(s.g(name))
    out["gLLCCDB68"] = struct.unpack(s.fmt[0] + "H", s.read(s.g("gLLCCDB68"), 2))[0]
    out["gTrackCostFree"] = s.read(s.g("gTrackCostFree"), 1)[0]
    return out


def run(s, case):
    try:
        labels, this = build(s, case)
        s.begin(labels)
        answer = s.call(this, case["mode"], case["a"], case["b"])
        return observe(s, answer)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

MODES = [0x39, 0xb, 0x10, 0x19, 0xf, 0xe, 0x13, 0xd, 0x12, 0, 7, 4, 0xc]
TYPES = [0, 7, 0xb, 0xd, 0x10, 0x12, 0x19, 0xc, 0xe, 4]
DIRS = [1, 4, 0x10, 0x40, 0, 2]


def flag(r, p, values=(1,)):
    return r.choice(values) if r.random() < p else 0


def make(r, mode=None, ttype=None, a=None):
    q = r.choice([0.0, 0.03, 0.1, 0.3, 0.6])      # how often a predicate answers nonzero
    db68 = r.choice([5, 0x1234, 0, 0xFFFF])

    def parent():
        return db68 if r.random() < 0.6 else r.randrange(0x10000)

    mode = r.choice(MODES + [r.randrange(0x40)]) if mode is None else mode
    ttype = r.choice(TYPES + [mode, mode]) if ttype is None else ttype
    a = r.choice([0, 0, 1, -1, 7]) if a is None else a
    d = r.choice(DIRS + [r.randrange(256)])
    if r.random() < 0.5:
        x, y = r.randrange(0x80), r.randrange(0x80)
        cid = 1 + x + (y << 7)
    else:
        cid = r.choice([0, 1, r.randrange(0x10000)])
        x, y = (cid - 1) & 0x7F, ((cid - 1) & MASK) >> 7
    b = r.choice([d, 0, r.choice(DIRS), r.randrange(256), d ^ 0x11])
    bfull = b | (r.randrange(1 << 24) << 8 if r.random() < 0.5 else 0)
    hx = [x if r.random() < 0.5 else r.randrange(0x80) for _ in range(2)]
    hy = [y if r.random() < 0.5 else r.randrange(0x80) for _ in range(2)]

    g = dict(gHoardingRight=hx[0], gHoardingBottom=hy[0], gHoardingLeft=hx[1], gHoardingTop=hy[1],
             gLLCCC200=flag(r, 0.3), gLLCCC204=flag(r, 0.3), gLLCCC20C=flag(r, 0.5), gLLCCC210=flag(r, q),
             gLLCCC214=flag(r, q), gLLCCDB40=flag(r, 0.5), gLLCCDB60=flag(r, 0.5), gLLCCE0B4=r.randrange(-3, 6),
             gLLCCE0B8=r.choice([-5, -1, 0, 1, 2, 3]), gLLCC4858=flag(r, 0.4), gTrackNbrOn485C=flag(r, 0.4),
             recording=flag(r, 0.3), gLLCCDB68=db68, gTrackCostFree=flag(r, 0.3, (1, 0x80)))

    entry0 = r.random() < 0.2
    things = [(r.choice([3, 3, 1, 4]), r.randrange(0x10000)) for _ in range(6)]
    ids = list(range(0 if entry0 else 1, 6))
    w = dict(this=dict(id=cid, type=ttype, dir=d, parent=parent()),
             mcells=[dict(type=r.choice([0, 3, 0xb, 4, 9, 10, 0x15, 0x10]) if r.random() < 0.6 else 0,
                          parent=r.randrange(6)) for _ in range(4)],
             tcells=[dict(type=r.choice([0, 0xb, 0xc, 0x11, 0x10]), parent=r.randrange(6) if r.random() < 0.5 else parent())
                     for _ in range(3)],
             tb=[parent() for _ in range(6)], rides=[r.randrange(-3, 6) for _ in range(4)], things=things, entry0=entry0, g=g)

    queues = {"GetMapCellEquiv": [MCELLS[0]], "MapNbr": MCELLS[1:], "TrackNbr": list(TCELLS), "GetBankAccount": [BANK],
              "GetRideInfo": [r.choice(RIDES) for _ in range(4)]}
    for lab in MCELLS:
        for name in ("IsRide", "IsFreeform", "IsExit", "IsEntrance", "IsQueue"):
            queues["%s:%d" % (name, lab)] = [flag(r, q / 2, (1, -1, 0x100))]
        queues["MapIsFlag40:%d" % lab] = [flag(r, q / 3, (1, 0x40))]
        queues["MapIsFlag20:%d" % lab] = [flag(r, q, (1, 0x20))]
    for lab in TCELLS:
        queues["IsTrackX:%d" % lab] = [flag(r, 0.5)]
    for lab in TCELLS + [WT + i for i in range(6)]:
        queues["IsTrack:%d" % lab] = [flag(r, 0.5, (1, -1))]
    for i in range(6):
        queues["GetObjectThingId:%d" % (WM + i)] = [r.choice(ids)]
    for name in ("IsBridge", "TrackIsFlag40", "TrackIsFlag10"):
        queues[name] = [flag(r, 0.4, (1, -1, 0x10000)) for _ in range(4)]
    for name in ("IsStraight", "IsCorner", "IsCrossRoad"):
        queues[name] = [flag(r, 0.4, (1, 0x41, 0x100, 0x80)) for _ in range(4)]
    queues["NumberOfNeighbours"] = [r.choice([0, 1, 2, 3, 4]) for _ in range(5)]
    queues["OPPOSITE"] = [r.choice([b, b, d, r.randrange(256)]) for _ in range(4)]
    queues["GetKartTrackCost"] = [r.randrange(0, 50)]
    queues["GetWaterTrackCost"] = [r.randrange(0, 50)]
    queues["GetSessionCost"] = [r.randrange(-5, 10)]
    queues["GetBalance"] = [r.randrange(-5, 10)]
    return {"mode": mode & MASK, "a": a & MASK, "b": bfull, "w": w, "queues": queues}


def edges():
    """Every mode against every cell mode, both a == 0 and not, quiet worlds and busy ones."""
    r = random.Random(5629)
    for mode in MODES:
        for ttype in TYPES:
            for a in (0, 1):
                yield make(r, mode, ttype, a)
    for _ in range(300):
        yield make(r)


def rnd(r):
    return make(r)


TESTS = [
    FnTest("CTrackCell::ConnectionType", ppc, host, edge=edges, rnd=rnd),
]
