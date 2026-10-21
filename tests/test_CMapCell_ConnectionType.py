"""Unit v51027, CMapCell::ConnectionType 0x1009b158 (the legality of placing a mode on a cell), against
libtpw_unit_CMapCell_ConnectionType.dylib (tests/unit_CMapCell_ConnectionType.cpp), main app ("SimTheme Park"),
src/MapDelta_map_CMapCell_ConnectionType.cpp:
  CMapCell::ConnectionType 0x1009b158

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for the
original, and in the host library's memory (little-endian) for the translation. One block stands for gGameWorld: the map cells at +0x2d8 (0x44
each), the track cells 0x110000 further (0x28 each), a ride record, eight things and a bank account behind them. The callees outside the unit
(IsFlag, GetObjectThingId, GAME_ASSERTMSG, GetRideInfo, the LLCC cost getters, GetTrackCellEquiv, the track cell predicates, ValidTypeChange,
CMapCell::IsTrack, the neighbour getters, GetBankAccount, GetBalance, LLCM_SetIllegalPlacement) are fakes on both sides that record their
arguments (cells as the offset from the world block) and answer from the case's queues. The ride record's overwrite priority is written at
the Mac's +0x54 on the PPC side and at the host field of the same name on the host side (tests/ridelayout.py); it stays below the things.

The world of a case: the mode and flag, the cell's type, parent id, the track cells and map cells, the cell system's globals, the thing table,
the action recorder's state. What is compared: the answer, the five reason flags, gLLCCDE0B4 and the event log.

No fused operations. Not tested: a thing id of 0 or a NULL thing slot used for a ride (the original dereferences it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import ridelayout
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CMapCell_ConnectionType"
MASK = 0xFFFFFFFF

FN = 0x1009B158
SIZES = {FN: 2524}
OWN = set(SIZES)

CALLEES = dict(
    IsFlag=0x1009BB70, GetObjectThingId=0x10103A04, Assert=0x1000DDE8, GetRideInfo=0x10154868, PathCost=0x10091B60,
    QueueCost=0x10091BD8, GetTrackCellEquiv=0x100F48C4, TrackIsTrackX=0x100A367C, TrackIsTrack=0x100A3574,
    ValidTypeChange=0x1009B054, AddToSessionCost=0x10091D50, SessionCost=0x10091D98, GetBankAccount=0x1013EDFC,
    GetBalance=0x100F32B4, SetIllegalPlacement=0x10088934, TrackIsCorner=0x100A37A0, TrackIsCrossRoad=0x100A3914,
    MapIsTrack=0x1009C96C, NumberOfNeighbours=0x10081744, GetNeighbours=0x100816E8, GetNeighbouringCell=0x10103814,
)

ADDR = dict(c200=0x102DC200, c204=0x102DC204, c208=0x102DC208, c210=0x102DC210, c214=0x102DC214, costfree=0x102DDB35,
            db40=0x102DDB40, db4c=0x102DDB4C, db60=0x102DDB60, db68=0x102DDB68, de0b4=0x102DE0B4, de0b8=0x102DE0B8,
            e0dc=0x102DE0DC, lock=0x102E4834, join=0x102E4838, gameworld=0x1037EBFC, thingarray=0x1034CBFC,
            recording=0x1037EC44 + 0x24)
HOST_LAYOUT = [8, 0x10, 0, 0x44, 0x28, 8, 0x10]      # the last host word (the thing table entry size) is read in bind

MAPBASE = 0x2D8
TRACKBASE = 0x2D8 + 0x110000
EXTRA = TRACKBASE + 0x28 * 16
RIDE = EXTRA + 0x100
THINGS = EXTRA + 0x200
BANK = EXTRA + 0x400
WORLD_SIZE = EXTRA + 0x1000
THIS_INDEX = 3
ASSERT_TEXT = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rel(e, p):
    return s32(p - e.u32(ADDR["gameworld"]))


def ev(e, name, *ints):
    e.cl.rec(name, tuple(ints))


def q(e, name):
    return e.cl.ret(name)


def st_isflag(e):
    ev(e, "IsFlag", rel(e, e.r(3)), e.r(4) & 0xFFFF)
    e.set_r(3, q(e, "IsFlag") & MASK)


def st_thingid(e):
    ev(e, "GetObjectThingId", rel(e, e.r(4)))
    e.write(e.r(3), struct.pack(">H", q(e, "GetObjectThingId") & 0xFFFF))


def st_ride(e):
    ev(e, "GetRideInfo", (e.r(4) >> 16) & 0xFFFF)
    e.set_r(3, (e.u32(ADDR["gameworld"]) + RIDE) & MASK)


def simple(name, key=None):
    def fn(e):
        ev(e, name)
        e.set_r(3, q(e, name) & MASK)
    return fn


def cellfn(name, mask=MASK):
    def fn(e):
        ev(e, name, rel(e, e.r(3)))
        e.set_r(3, q(e, name) & mask)
    return fn


def st_equiv(e):
    ev(e, "GetTrackCellEquiv", rel(e, e.r(3)))
    e.set_r(3, (e.u32(ADDR["gameworld"]) + TRACKBASE + (q(e, "GetTrackCellEquiv") * 0x28)) & MASK)


def st_valid(e):
    ev(e, "ValidTypeChange", rel(e, e.r(3)), s32(e.r(4)))
    e.set_r(3, q(e, "ValidTypeChange") & MASK)


def st_add(e):
    ev(e, "AddToSessionCost", s32(e.r(3)))


def st_bank(e):
    ev(e, "GetBankAccount")
    e.set_r(3, (e.u32(ADDR["gameworld"]) + BANK) & MASK)


def st_balance(e):
    ev(e, "GetBalance")
    e.set_r(3, q(e, "GetBalance") & MASK)


def st_illegal(e):
    ev(e, "SetIllegalPlacement", s32(e.r(3)))


def st_ncell(e):
    ev(e, "GetNeighbouringCell", rel(e, e.r(3)), e.r(4) & 0xFF)
    e.set_r(3, (e.u32(ADDR["gameworld"]) + MAPBASE + q(e, "GetNeighbouringCell") * 0x44) & MASK)


def st_getn(e):
    ev(e, "GetNeighbours", rel(e, e.r(3)))
    e.set_r(3, q(e, "GetNeighbours") & 0xFF)


STUBS.internal(CALLEES["IsFlag"], st_isflag)
STUBS.internal(CALLEES["GetObjectThingId"], st_thingid)
STUBS.internal(CALLEES["Assert"], CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(CALLEES["GetRideInfo"], st_ride)
STUBS.internal(CALLEES["PathCost"], simple("GetPathCost"))
STUBS.internal(CALLEES["QueueCost"], simple("GetQueueCost"))
STUBS.internal(CALLEES["GetTrackCellEquiv"], st_equiv)
STUBS.internal(CALLEES["TrackIsTrackX"], cellfn("TrackIsTrackX"))
STUBS.internal(CALLEES["TrackIsTrack"], cellfn("TrackIsTrack"))
STUBS.internal(CALLEES["ValidTypeChange"], st_valid)
STUBS.internal(CALLEES["AddToSessionCost"], st_add)
STUBS.internal(CALLEES["SessionCost"], simple("GetSessionCost"))
STUBS.internal(CALLEES["GetBankAccount"], st_bank)
STUBS.internal(CALLEES["GetBalance"], st_balance)
STUBS.internal(CALLEES["SetIllegalPlacement"], st_illegal)
STUBS.internal(CALLEES["TrackIsCorner"], cellfn("TrackIsCorner", 0xFF))
STUBS.internal(CALLEES["TrackIsCrossRoad"], cellfn("TrackIsCrossRoad", 0xFF))
STUBS.internal(CALLEES["MapIsTrack"], cellfn("MapIsTrack"))
STUBS.internal(CALLEES["NumberOfNeighbours"], cellfn("NumberOfNeighbours"))
STUBS.internal(CALLEES["GetNeighbours"], st_getn)
STUBS.internal(CALLEES["GetNeighbouringCell"], st_ncell)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def byte(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"
    stride = 0x14

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ptr(self, a, v):
        self.words(a, [v])

    def alloc(self, n):
        return self.e.alloc(n, bytes(n))

    def rec(self, mac):
        return mac

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, this, mode, flag):
        r3, _, _ = T.call_ppc(self.e, FN, [this & MASK, mode & MASK, flag & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.stride = lib.stride
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cmct_addr(name.encode())

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def alloc(self, n):
        return self.lib.tpw_cmct_alloc(n)

    def rec(self, mac):
        return ridelayout.host_offset(self.lib, mac)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, this, mode, flag):
        return self.lib.tpw_cmct_ct(this, s32(mode), s32(flag)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cmct_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "ct").restype, getattr(lib, p + "ct").argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n - 1] == HOST_LAYOUT, list(out)[:n]
    lib.stride = out[n - 1]
    ridelayout.bind(lib)
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    w = s.alloc(WORLD_SIZE)
    s.ptr(s.g("gameworld"), w)
    for i in range(-1, 10):
        c = w + MAPBASE + i * 0x44
        s.half(c, case["mapid"][i + 1])
        s.words(c + 8, [case["maptype"][i + 1]])
        t = w + TRACKBASE + i * 0x28
        s.words(t + 8, [case["trtype"][i + 1]])
        s.half(t + 0x10, case["trparent"][i + 1])
    this = w + MAPBASE + THIS_INDEX * 0x44
    s.half(this, 4)
    s.words(this + 8, [case["mtype"]])
    s.half(this + 0x10, case["mparent"])
    s.ptr(s.g("db4c"), w + MAPBASE + case["db4c"] * 0x44)
    for i in range(8):
        th = w + THINGS + i * 0x20
        s.byte(th + 2, case["kind"][i])
        s.half(th + 0xE, case["objtype"][i])
        s.ptr(s.g("thingarray") + i * s.stride, 0 if (i == 0 and case["slot0null"]) else th)
    assert s.rec(0x54) + 4 <= THINGS - RIDE
    s.words(w + RIDE + s.rec(0x54), [case["ride"]])
    for name in ("c200", "c204", "c208", "c210", "c214", "db40", "db60", "de0b4", "de0b8", "e0dc", "lock", "join", "recording"):
        s.words(s.g(name), [case[name]])
    s.half(s.g("db68"), case["db68"])
    s.byte(s.g("costfree"), case["costfree"])
    return this


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(),
            "flags": [s.rd_word(s.g(n)) for n in ("c200", "c204", "c208", "c210", "c214", "de0b4")]}


def run(s, case):
    try:
        this = build(s, case)
        return observe(s, case, s.call(this, case["mode"], case["flag"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

MODES = [0x1F, 0x3A, 4, 9, 10, 0x18, 0x36, 0x15, 0x16, 1, 3, 0xB, 0x10, 0x14, 0, 2, 0x19, 7]
TYPES = [0, 1, 3, 4, 9, 10, 0x15, 0x19, 2, 0xB, 0x10]


def qv(r, choices, n=None):
    n = n or r.randrange(1, 5)
    return [r.choice(choices) for _ in range(n)]


def base(r, mode=None, mtype=None):
    flag = r.choice([0, 1])
    if mode is None:
        mode = r.choice(MODES)
    if mtype is None:
        mtype = mode if r.random() < 0.3 else r.choice(TYPES)
    c = {"mode": mode, "flag": flag, "mtype": mtype, "mparent": r.choice([0, 1, 2, 5, 8, 9]),
         "mapid": [r.choice([0, 1, 5, 0x423F, 4, 2, 8, 9, 3]) for _ in range(11)],
         "maptype": [r.choice(TYPES) for _ in range(11)],
         "trtype": [r.choice([0x19, 0, 1, 3, 0xC, 0x11, 2]) for _ in range(11)],
         "trparent": [r.choice([0, 1, 2, 3, 4, 5, 8, 9]) for _ in range(11)],
         "db4c": r.randrange(0, 8), "kind": [r.choice([3, 3, 3, 1, 5]) for _ in range(8)],
         "objtype": [r.randrange(0, 0x10000) for _ in range(8)], "slot0null": r.choice([0, 1]),
         "ride": r.choice([0, 3, 7, -2, 100, 0x7FFFFFFF]),
         "costfree": r.choice([0, 0, 1])}
    for n in ("c200", "c204", "c208", "c210", "c214", "db40", "db60", "e0dc", "lock", "join", "recording"):
        c[n] = r.choice([0, 0, 1])
    c["c214"] = r.choice([0, 0, 0, 1])
    c["de0b4"] = r.choice([0, 3, -2, 100])
    c["de0b8"] = r.choice([0, 3, 7, 100, -5])
    c["db68"] = r.choice([0, 1, 2, 3, 4, 5, 8, 9, 0xFFFF])
    if r.random() < 0.5:                                   # make the cell's parent id match the cell system's often
        c["db68"] = c["mparent"]
    c["queues"] = {
        "IsFlag": qv(r, [0, 0, 1]), "GetObjectThingId": qv(r, [1, 2, 3, 4, 5, 6, 7]),
        "GetPathCost": qv(r, [0, 10, -5, 100]), "GetQueueCost": qv(r, [0, 20, -7, 300]),
        "GetTrackCellEquiv": qv(r, list(range(8))), "TrackIsTrackX": qv(r, [0, 1]), "TrackIsTrack": qv(r, [0, 1, 1]),
        "ValidTypeChange": qv(r, [0, 1, 1]), "GetSessionCost": qv(r, [0, 0, 5, 50, -4]),
        "GetBalance": qv(r, [-10, 0, 20, 100, 1000]), "TrackIsCorner": qv(r, [0, 0, 1, 0x40]),
        "TrackIsCrossRoad": qv(r, [0, 0, 1, 0x14]), "MapIsTrack": qv(r, [0, 1]),
        "NumberOfNeighbours": qv(r, [0, 1, 2, 3]), "GetNeighbours": qv(r, [0, 1, 4, 0x40, 0x80]),
        "GetNeighbouringCell": qv(r, list(range(8))),
    }
    return c


def edges():
    r = random.Random(51027)
    for mode in MODES:
        for mtype in TYPES:
            for _ in range(3):
                yield base(r, mode, mtype)


def rnd(r):
    return base(r)


TESTS = [
    FnTest("CMapCell::ConnectionType", ppc, host, edge=edges, rnd=rnd),
]
