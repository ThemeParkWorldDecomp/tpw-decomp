"""Unit v5932: LLCR_OverRide (the pointer overload), LLCM_TrackCellBoundary, LLCR_SetRide and LLCR_AddRideToDB, against
libtpw_unit_LLCR_OverRide_3.dylib (tests/unit_LLCR_OverRide_3.cpp), main app ("SimTheme Park"), src/game/park/LLCellMouse.cpp, src/game/park/LLCellRide.cpp:
  LLCR_OverRide 0x1008905c, LLCM_TrackCellBoundary 0x100889ac, LLCR_SetRide 0x1008bc20, LLCR_AddRideToDB 0x1008be1c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (IsXYValid, GetObjectThingId, GAME_ASSERTMSG, GetRideInfo, LLCR_OffsetStartOfTrack, BMP_CreateRide, MapDelta::Rotate, the CObject
constructor, SetUpgradeParent, LLCM_GetCurrentObject, GetThingId, SetSelectedObject) are fakes on both sides that record their arguments
and answer from the case's queues.

The world of a case: the world's map cells (kind at +8, linked cell id at +0x10), the thing array (entry 0 and the entry of the case's
thing), the ride record (words 0x18, 0xa0, 0xa4 to 0xc0, 0x288), the words 0x1029e04c, 0x102de092 to 0x102de09c and 0x102de0dc. What is
compared: the answers, x and y, the words of the unit, the boundary outputs and the event log.

No fused operations. Not tested: a rotation other than 0, 0x5a, 0xb4, 0x10e in LLCR_AddRideToDB (the Mac leaves r28, the track kind,
unset: it is the caller's register); a cell id 0 in LLCR_AddRideToDB with a ride that builds (the float result passes 2^31 and the
emulator's fctiwz saturates differently).

Unit v5L28: the host ride record is the host RideInfo (8-byte balance values behind a 64-bit table header), so the host side writes each
word at the host offset of the same balance slot (tpw_lcro3_rideoff) and the Rotate fake names the map offset by the Mac's 0x7ac. The
shape slot (0x18) holds a native pointer on the host: LLCR_SetRide keeps it whole in gLLCRRideShape beside the word 0x102de094, and the
case compares which block both name (the shape block, none, or another).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCR_OverRide_3"
MASK = 0xFFFFFFFF

FN_OVER, FN_BOUND, FN_SET, FN_ADD = 0x1008905C, 0x100889AC, 0x1008BC20, 0x1008BE1C
SIZES = {FN_OVER: 396, FN_BOUND: 128, FN_SET: 92, FN_ADD: 740}
OWN = set(SIZES)

IS_XY_VALID, GET_OBJ_THING_ID, GAME_ASSERTMSG = 0x101012F4, 0x10103A04, 0x1000DDE8
GET_RIDE_INFO, OFFSET_TRACK, BMP_CREATE_RIDE = 0x10154868, 0x1008C620, 0x1002756C
ROTATE, CTOR, SET_UPGRADE_PARENT = 0x10103D88, 0x10105D74, 0x1010D9F8
GET_CURRENT, GET_THING_ID, SET_SELECTED = 0x100888FC, 0x1012E1F8, 0x101F0000

ADDR = dict(gworld=0x1037EBFC, tarr=0x1034CBFC, g04C=0x1029E04C, g092=0x102DE092, g094=0x102DE094, g098=0x102DE098,
            g09C=0x102DE09C, g0DC=0x102DE0DC, rdb=0x1037F6C8, ui=0x10396440)
HOST_LAYOUT = [0x44, 0x2D8, 4]
STATE = {}          # the PowerPC stubs' view of the case: ride, cur, world (addresses)
MAP_CELLS = 2100


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)

STUBS.internal(IS_XY_VALID, CL.recorder("IsXYValid", "ii", ret="i"))
STUBS.internal(GAME_ASSERTMSG, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(BMP_CREATE_RIDE, CL.recorder("BMP_CreateRide", "iiiiii", ret="i"))
STUBS.internal(CTOR, CL.recorder("CObject", "Nwwii"))
STUBS.internal(SET_UPGRADE_PARENT, CL.recorder("SetUpgradeParent", "Lw"))
STUBS.internal(SET_SELECTED, CL.recorder("SetSelectedObject", "LL"))


def stub_get_object_thing_id(e):
    L = e.cl
    L.rec("GetObjectThingId", (s32(e.r(4) - STATE["world"]),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetObjectThingId") & 0xFFFF))


def stub_get_ride_info(e):
    L = e.cl
    L.rec("GetRideInfo", (L.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, STATE["ride"])


def stub_offset(e):
    L = e.cl
    xa, ya = e.r(3), e.r(4)
    x, y = e.s32(xa), e.s32(ya)
    L.rec("OffsetStartOfTrack", (x, y, s32(e.r(5))))
    e.write(xa, struct.pack(">I", (x + L.ret("OffX")) & MASK))
    e.write(ya, struct.pack(">I", (y + L.ret("OffY")) & MASK))
    e.set_r(3, 0)


def stub_rotate(e):
    L = e.cl
    L.rec("Rotate", (s32(e.r(4) - STATE["ride"]), s32(e.r(5))))
    e.write(e.r(3), struct.pack(">IIB", L.ret("RotX") & MASK, L.ret("RotY") & MASK, 0))


def stub_get_current(e):
    e.cl.rec("LLCM_GetCurrentObject", ())
    e.set_r(3, STATE["cur"])


def stub_get_thing_id(e):
    L = e.cl
    L.rec("GetThingId", (e.u32(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetThingId") & 0xFFFF))


STUBS.internal(GET_OBJ_THING_ID, stub_get_object_thing_id)
STUBS.internal(GET_RIDE_INFO, stub_get_ride_info)
STUBS.internal(OFFSET_TRACK, stub_offset)
STUBS.internal(ROTATE, stub_rotate)
STUBS.internal(GET_CURRENT, stub_get_current)
STUBS.internal(GET_THING_ID, stub_get_thing_id)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"
    estride = 0x14

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["ui"]: 1, ADDR["rdb"]: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
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
        self.write(a, struct.pack(">I", v))

    ride_size = 0x900

    def roff(self, off):
        return off

    def set_shape(self, ride, blk):
        self.ptr(ride + 0x18, blk)

    def shape_label(self, blk):
        return lab(self, self.rd_words(self.g("g094"), 1)[0], blk)

    def rd_ptr(self, a):
        return self.rd_words(a, 1)[0]

    def setvar(self, name, v):
        STATE[name] = v

    def log(self):
        return self.L.log

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.estride = lib.estride
        CL.host_begin(lib, {1: self.g("ui"), 2: self.g("rdb")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lcro3_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_lcro3_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    @property
    def ride_size(self):
        return self.lib.ridesize

    def roff(self, off):
        return self.lib.tpw_lcro3_rideoff(off)

    def set_shape(self, ride, blk):
        self.ptr(ride + self.roff(0x18), blk)

    def shape_label(self, blk):
        whole = self.rd_ptr(self.g("shape"))
        if self.rd_words(self.g("g094"), 1)[0] != whole & MASK:
            return -2
        return lab(self, whole, blk)

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def setvar(self, name, v):
        if name in ("ride", "cur"):
            self.ptr(self.g(name), v)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        lib = self.lib
        if entry == FN_OVER:
            lib.tpw_lcro3_over(*args)
            return 0
        if entry == FN_BOUND:
            lib.tpw_lcro3_boundary(s32(args[0]), s32(args[1]), *args[2:])
            return 0
        if entry == FN_SET:
            lib.tpw_lcro3_setride((args[0] >> 16) & 0xFFFF)
            return 0
        return lib.tpw_lcro3_addride(s32(args[0]))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    U64, I32 = C.c_uint64, C.c_int32
    lib.tpw_lcro3_addr.restype, lib.tpw_lcro3_addr.argtypes = U64, [C.c_char_p]
    lib.tpw_lcro3_alloc.restype, lib.tpw_lcro3_alloc.argtypes = U64, [C.c_uint32]
    lib.tpw_lcro3_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lcro3_over.restype, lib.tpw_lcro3_over.argtypes = None, [U64, U64, U64]
    lib.tpw_lcro3_boundary.restype, lib.tpw_lcro3_boundary.argtypes = None, [I32, I32, U64, U64, U64, U64]
    lib.tpw_lcro3_setride.restype, lib.tpw_lcro3_setride.argtypes = None, [C.c_uint32]
    lib.tpw_lcro3_addride.restype, lib.tpw_lcro3_addride.argtypes = U64, [I32]
    lib.tpw_lcro3_rideoff.restype, lib.tpw_lcro3_rideoff.argtypes = C.c_int64, [C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lcro3_layout(out)
    assert list(out)[:3] == HOST_LAYOUT, list(out)[:n]
    lib.estride = out[3]
    lib.ridesize = out[4]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

def lab(s, v, ride=None):
    if v == 0:
        return 0
    return 1 if v == ride else -1


def build_common(s, c):
    s.half(s.g("g04C"), c["g04C"])
    s.half(s.g("g092"), c["g092"])
    s.words(s.g("g094"), [c["g094"]])
    s.half(s.g("g09C"), c["g09C"])
    s.words(s.g("g0DC"), [c["g0DC"]])
    s.ptr(s.g("g098"), 0)


def build_ride(s, c):
    ride = s.alloc(s.ride_size)
    for off, v in c["ride"].items():
        s.words(ride + s.roff(off), [v])
    shape = s.alloc(0x40) if c.get("shape") else 0
    s.set_shape(ride, shape)
    s.setvar("ride", ride)
    return ride, shape


def build_over(s, c):
    build_common(s, c)
    world = s.alloc(0x2D8 + 0x44 * MAP_CELLS)
    s.setvar("world", world)
    s.ptr(s.g("gworld"), world)
    base = world + 0x2D8
    cell = base + (c["id"] - 1) * 0x44
    s.words(cell + 8, [c["kind"]])
    s.half(cell + 0x10, c["link"])
    tarr = s.g("tarr")
    th0 = s.alloc(0x40) if c["e0"] else 0
    s.ptr(tarr, th0)
    thing = s.alloc(0x40)
    s.half(thing + 0x30, c["thalf"])
    s.ptr(tarr + s.estride * c["tid"], thing)
    ptrs = s.alloc(16)
    s.half(ptrs, 0xBEEF)
    s.words(ptrs + 4, [c["x"]])
    s.words(ptrs + 8, [c["y"]])
    return ptrs


def build_boundary(s, c):
    blk = s.alloc(16)
    s.words(blk, [c["cx"], c["cy"], 0x11111111, 0x22222222])
    return blk


def observe_common(s, op):
    res = {"g04C": s.rd_half(s.g("g04C")), "g092": s.rd_half(s.g("g092")), "g09C": s.rd_half(s.g("g09C"))}
    if op != "set":
        res["g094"] = s.rd_words(s.g("g094"), 1)
    return res


def run(s, c):
    try:
        op = c["op"]
        if op == "over":
            ptrs = build_over(s, c)
            s.call(FN_OVER, [ptrs, ptrs + 4, ptrs + 8])
            return {"out": s.rd_half(ptrs), "xy": s.rd_words(ptrs + 4, 2), "log": s.log()}
        if op == "bound":
            blk = build_boundary(s, c)
            s.call(FN_BOUND, [c["x"], c["y"], blk, blk + 4, blk + 8, blk + 12])
            return {"w": s.rd_words(blk, 4), "log": s.log()}
        build_common(s, c)
        ride, shape = build_ride(s, c)
        s.setvar("cur", 0)
        if op == "set":
            s.call(FN_SET, [c["id"] << 16])
            res = {"shape": s.shape_label(shape)}
        else:
            cur = s.alloc(0x40)
            s.words(cur, [c["marker"]])
            s.setvar("cur", cur)
            res = {"ans": 0 if s.call(FN_ADD, [c["rot"]]) == 0 else 1}
        res.update(observe_common(s, op))
        res["g098"] = lab(s, s.rd_ptr(s.g("g098")), ride)
        res["log"] = s.log()
        return res
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the cases

def rid(r):
    return r.choice([0, 1, 2, 0x10, 0x7FFF, 0x8000, 0xFFFF, r.randrange(0x10000)])


def over_case(r, x=None, y=None, **kw):
    x = r.choice([-1, 0, 1, 5, 126, 127, r.randrange(128)]) if x is None else x
    y = r.choice([0, 1, 2, 15, r.randrange(16)]) if y is None else y
    valid = kw.get("valid", r.choice([1, 1, 1, 0]))
    if not valid:
        x, y = r.choice([x, 500, -7]), r.choice([y, -3, 4000])
    cid = (x + (y << 7) + 1) & 0xFFFF
    kind = kw.get("kind", r.choice([4, 9, 10, 0, 1, 3, 5, 11]))
    link = kw.get("link", r.choice([cid, cid, cid, r.randrange(0, 2000), 0, 1]))
    tid = kw.get("tid", r.choice([0, 1, r.randrange(0x2800), r.randrange(30)]))
    g04C = kw.get("g04C", r.choice([tid, tid, (tid + 1) & 0xFFFF, 0, r.randrange(0x10000)]))
    thalf = kw.get("thalf", r.choice([0, 1, 5, 128, 129, 2000, 0xFFFF, r.randrange(0x10000)]))
    c = {"op": "over", "x": x, "y": y, "id": cid if valid and cid >= 1 else 1, "kind": kind, "link": link, "tid": tid,
         "g04C": g04C, "thalf": thalf, "e0": kw.get("e0", r.choice([0, 1])), "g092": 0, "g094": 0, "g09C": 0, "g0DC": 0}
    if valid and cid == 0:
        # the cell before the array (index -1): put it in the world's header
        c["id"] = 0
    c["queues"] = {"IsXYValid": [valid], "GetObjectThingId": [tid]}
    return c


def over_edges():
    r = random.Random(5932)
    yield over_case(r, 3, 2, valid=0)
    for kind in (4, 9, 10, 0, 3, 11):
        yield over_case(r, 3, 2, valid=1, kind=kind, link=3 + 256 + 1, tid=7, g04C=7)
    yield over_case(r, 3, 2, valid=1, kind=4, link=3 + 256 + 1, tid=7, g04C=8, thalf=129, e0=0)
    yield over_case(r, 3, 2, valid=1, kind=9, link=5, tid=0, g04C=1, thalf=129, e0=1)
    yield over_case(r, 3, 2, valid=1, kind=10, link=1, tid=9, g04C=2, thalf=0)
    yield over_case(r, -1, 0, valid=1, kind=4, link=0, tid=2, g04C=3, thalf=1)
    yield over_case(r, 127, 15, valid=1, kind=4, tid=0x27FF, g04C=0, thalf=0xFFFF)


def over_random(r):
    return over_case(r)


def bound_case(r, v=None):
    v = v or [r.choice([0, 1, -1, 2, -2, 3, -3, 100, -100, r.randrange(-500, 500)]) for _ in range(4)]
    return {"op": "bound", "x": v[0], "y": v[1], "cx": v[2], "cy": v[3], "queues": {}}


def bound_edges():
    vals = (0, 1, -1, 2, -2, 3, -3, 4, -4, 0x7FFFFFFE, -0x7FFFFFFF)
    for a in vals:
        for b in vals[:7]:
            yield bound_case(None, [0, 0, a, b])
    yield bound_case(None, [5, -5, 4, -4])
    yield bound_case(None, [-7, 7, -8, 8])


def bound_random(r):
    return bound_case(r)


def ride_words(r, build=True):
    d = {0xA0: r.choice([0, 1, 5, 77]) if build else 0, 0x288: r.choice([0, 1, 2, 3])}
    for off in range(0xA4, 0xC4, 4):
        d[off] = r.choice([0, 1, -1, 5, -9, r.randrange(-30, 30)])
    return d


def set_case(r, i=None, shape=None):
    c = {"op": "set", "id": rid(r) if i is None else i, "g04C": 0, "g092": 0, "g094": r.randrange(1 << 32), "g09C": rid(r),
         "g0DC": 0, "ride": ride_words(r), "shape": r.choice([0, 1, 1]) if shape is None else shape, "queues": {}}
    return c


def set_edges():
    r = random.Random(5933)
    for i in (0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x1234):
        yield set_case(r, i, 1)
    yield set_case(r, 2, 0)


def set_random(r):
    return set_case(r)


def add_case(r, **kw):
    rot = kw.get("rot", r.choice([0, 0x5A, 0xB4, 0x10E]))
    build = kw.get("build", r.choice([1, 1, 0]))
    ride = ride_words(r, build)
    ride.update(kw.get("ride", {}))
    g092 = kw.get("g092", r.choice([1, 2, 128, 129, 0x1234, r.randrange(1, 0x10000)]))
    if not build:
        g092 = kw.get("g092", r.choice([0, g092]))
    q = {"GetThingId": [r.randrange(0x10000)], "OffX": [r.randrange(-6, 6)], "OffY": [r.randrange(-6, 6)],
         "BMP_CreateRide": [r.choice([0, 1, -1, r.randrange(1 << 31)])], "RotX": [r.randrange(-40, 40)],
         "RotY": [r.randrange(-40, 40)]}
    return {"op": "add", "rot": rot, "g04C": 0, "g092": g092, "g094": 0, "g09C": r.randrange(0x10000),
            "g0DC": kw.get("g0DC", r.choice([0, 1, -1, 7])), "ride": ride, "marker": r.randrange(1 << 32), "queues": q}


def add_edges():
    r = random.Random(5934)
    for rot in (0, 0x5A, 0xB4, 0x10E):
        for g0DC in (0, 3):
            for build in (0, 1):
                yield add_case(r, rot=rot, g0DC=g0DC, build=build)
    yield add_case(r, g0DC=1, ride={0x288: 2})
    yield add_case(r, g0DC=1, ride={0x288: 1})
    yield add_case(r, g0DC=0, g092=0, build=0)
    yield add_case(r, g092=0xFFFF, rot=0)


def add_random(r):
    return add_case(r)


TESTS = [
    FnTest("LLCR_OverRide", ppc, host, edge=over_edges, rnd=over_random),
    FnTest("LLCM_TrackCellBoundary", ppc, host, edge=bound_edges, rnd=bound_random),
    FnTest("LLCR_SetRide", ppc, host, edge=set_edges, rnd=set_random),
    FnTest("LLCR_AddRideToDB", ppc, host, edge=add_edges, rnd=add_random),
]
