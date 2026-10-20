"""Unit v5006, the three states of a guest leaving the park, against libtpw_unit_CPerson_ProcessStateWaitingToCrossRoadOutOfPark.dylib
(tests/unit_CPerson_ProcessStateWaitingToCrossRoadOutOfPark.cpp), main app ("SimTheme Park"), src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark.cpp:
  CPerson::ProcessStateLeavingPark 0x1011e2e8, CPerson::ProcessStateWaitingToCrossRoadOutOfPark 0x1011e42c,
  CPerson::ProcessStateWaitingForBusOuttaHere 0x1011e654

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian, host memory little-endian). Every
callee outside the unit is a fake on both sides that records its arguments and answers from the case's queues: CMap::GetBusStopPos, the CWorld
queries and GetRand, CPeep::IsInsidePark, PeepWalks, SetDest, SpawnParticleEffect, CPerson::SetState, CThing::GetThingId and KillMe, CTRACE.

The world of a case: gWorld and gWorldMap point at blocks; the person is a block of 0x1d0 bytes with its cell bytes (+4, +6), the float at +0x19c,
the word at +0x1bc and the word at +0x188; the map is an array of 0x44 byte cells whose +0x24 halfword is the thing standing there. What is
compared: the event log (names, every argument, the CTRACE text) and the whole person block. No fused operations. Not tested: float values at or
above 2^31 in +0x19c (Unicorn's fctiwz differs from the hardware there), person cells beyond 128 by 8 (the map block is that large).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPerson_ProcessStateWaitingToCrossRoadOutOfPark"
MASK = 0xFFFFFFFF

FN_LEAVING, FN_CROSS, FN_BUS = 0x1011E2E8, 0x1011E42C, 0x1011E654
SIZES = {FN_LEAVING: 268, FN_CROSS: 480, FN_BUS: 852}
OWN = set(SIZES)
ENTRY = {"leaving": FN_LEAVING, "cross": FN_CROSS, "bus": FN_BUS}

ADDR = dict(gWorld=0x1037EC0C, gWorldMap=0x1037EC08)
HOST_LAYOUT = [0x44, 0x24]

PERSON_SIZE = 0x1D0
MAP_CELLS = 1100          # x below 128, y below 8: ids up to 1024


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)

STUBS.internal(0x1014080C, CL.recorder("ArrivalVehicle_CrossingIsClear", "L", ret="i"))
STUBS.internal(0x1013B1E4, CL.recorder("GetRand", "L", ret="i"))
STUBS.internal(0x1013FF7C, CL.recorder("IsParkClosed", "L", ret="i"))
STUBS.internal(0x101406CC, CL.recorder("ArrivalVehicle_GetStatus", "L", ret="i"))
STUBS.internal(0x10140DE0, CL.recorder("ArrivalVehicle_IsSeagoing", "L", ret="i"))
STUBS.internal(0x10115A80, CL.recorder("IsInsidePark", "L", ret="i"))
STUBS.internal(0x10114FA8, CL.recorder("PeepWalks", "L", ret="i"))
STUBS.internal(0x101154F0, CL.recorder("SetDest", "Lww", ret="i"))
STUBS.internal(0x101152A4, CL.recorder("SpawnParticleEffect", "Li"))
STUBS.internal(0x10120354, CL.recorder("SetState", "Li"))
STUBS.internal(0x1012E728, CL.recorder("KillMe", "L"))
STUBS.internal(0x1006FCBC, CL.recorder("CTRACE", "us"))


def stub_bus_stop(e):
    """CMap::GetBusStopPos(int) const: r3 the hidden result, r4 the map, r5 which."""
    L = e.cl
    L.rec("GetBusStopPos", (L.lab(e.r(4)), s32(e.r(5))))
    e.write(e.r(3), struct.pack(">H", L.ret("GetBusStopPos") & 0xFFFF))


def stub_thing_id(e):
    """CThing::GetThingId() const: r3 the hidden result, r4 this."""
    L = e.cl
    L.rec("GetThingId", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetThingId") & 0xFFFF))


STUBS.internal(0x10101A50, stub_bus_stop)
STUBS.internal(0x1012E1F8, stub_thing_id)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, v):
        """A pointer global: four bytes on the Mac, a native pointer (eight bytes) on the host."""
        self.write(a, struct.pack(">I", v & MASK) if self.fmt == ">I" else struct.pack("<Q", v))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    def log(self):
        return self.L.log

    def call(self, op, person):
        T.call_ppc(self.e, ENTRY[op], [person & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, person):
        getattr(self.lib, "tpw_cp_" + op)(person)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cp_addr.restype, lib.tpw_cp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cp_alloc.restype, lib.tpw_cp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cp_layout.argtypes = [C.POINTER(C.c_int64)]
    for op in ("leaving", "cross", "bus"):
        f = getattr(lib, "tpw_cp_" + op)
        f.restype, f.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def my_cell(case):
    return (case["x"] + (case["y"] << 7) + 1) & 0xFFFF


def build(s, case):
    world = s.alloc(16)
    gmap = s.alloc(MAP_CELLS * 0x44)
    person = s.alloc(PERSON_SIZE)
    s.ptr(s.g("gWorld"), world)
    s.ptr(s.g("gWorldMap"), gmap)
    s.write(person + 4, bytes([case["x"]]))
    s.write(person + 6, bytes([case["y"]]))
    s.words(person + 0x188, [case["w188"]])
    s.words(person + 0x19C, [case["f19c"]])
    s.words(person + 0x1BC, [case["w1bc"]])
    s.half(gmap + (my_cell(case) - 1) * 0x44 + 0x24, case["who"])
    case["person"] = person
    return person


def observe(s, case, person):
    return {"log": s.log(), "head": s.read(person, 8), "person": s.rd_words(person + 8, PERSON_SIZE // 4 - 2)}


def run(s, case):
    try:
        person = build(s, case)
        s.call(case["op"], person)
        return observe(s, case, person)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

FLOATS = [0.0, 1.0, 0.9, 255.0, 256.0, 257.5, -1.0, -255.5, 3.0, 1000.25, 65536.0, 0x7FFFFF00 * 1.0]


def base(r, op):
    x, y = r.randrange(128), r.randrange(8)
    c = {"op": op, "x": x, "y": y, "w188": r.choice([0, 0x1234, 0xFFFFFFFF]),
         "f19c": fb(r.choice(FLOATS) if r.random() < 0.7 else r.uniform(-600, 600)),
         "w1bc": r.choice([-1, 0, 1, 2, 5]), "who": r.randrange(0x10000)}
    c["queues"] = {}
    return c


def rands(r):
    return [r.choice([r.getrandbits(32), r.randrange(8)]) for _ in range(4)]


# ---- leaving
def leaving_case(r, w1bc=None, f=None, closed=None, inside=None, walks=None):
    c = base(r, "leaving")
    if w1bc is not None:
        c["w1bc"] = w1bc
    if f is not None:
        c["f19c"] = fb(f)
    c["queues"] = {"IsParkClosed": [r.choice([0, 1]) if closed is None else closed],
                   "IsInsidePark": [r.choice([0, 1]) if inside is None else inside],
                   "PeepWalks": [r.choice([0, 1, 2, 3]) if walks is None else walks]}
    return c


def leaving_edges():
    r = random.Random(11)
    for w in (-1, 0, 1):
        for f in (0.0, 0.9, 1.0, 256.0, 257.0):
            for closed in (0, 1):
                for inside in (0, 1):
                    for walks in (0, 1, 2, 3):
                        yield leaving_case(r, w, f, closed, inside, walks)


# ---- cross
def bus_cells(case, r, hit):
    """Four bus stop answers; with `hit` the stop is chosen so that cell k of the original's table is this person's cell."""
    off = [0, 1, -256, -255]
    if hit is None:
        t = r.getrandbits(16)
    else:
        t = (my_cell(case) - off[hit]) & 0xFFFF
    return [t] * 4


def cross_case(r, clear=None, hit=None):
    c = base(r, "cross")
    c["queues"] = {"GetBusStopPos": bus_cells(c, r, hit), "ArrivalVehicle_CrossingIsClear": [r.choice([0, 1]) if clear is None else clear],
                   "GetRand": rands(r), "SetDest": [r.choice([0, 1])]}
    return c


def cross_edges():
    r = random.Random(12)
    for clear in (0, 1):
        for k in range(4):
            for _ in range(2):
                c = cross_case(r, clear)
                c["queues"]["GetRand"] = [k, r.getrandbits(32), r.getrandbits(32)]
                yield c
    for t in (0, 1, 0x100, 0x101, 0xFFFF):
        c = cross_case(r, 1)
        for k in range(4):
            c["queues"]["GetBusStopPos"] = [t] * 4
            c["queues"]["GetRand"] = [k, 0xFF, 0x100]
            yield dict(c, queues=dict(c["queues"]))


# ---- bus
def bus_case(r, status=None, sea=None, hit=None, same=None):
    c = base(r, "bus")
    tid = r.randrange(1, 0x10000)
    if same is None:
        same = r.choice([0, 1])
    if status is None:
        status = r.choice([0, 1, 2, 3, 4, 4, 5, 6, -1])
    if same:
        c["who"] = tid
    c["queues"] = {"ArrivalVehicle_GetStatus": [status], "GetThingId": [tid], "ArrivalVehicle_IsSeagoing": [r.choice([0, 1]) if sea is None else sea],
                   "GetBusStopPos": bus_cells(c, r, hit), "GetRand": rands(r), "SetDest": [r.choice([0, 1])]}
    return c


def bus_edges():
    r = random.Random(13)
    for status in (-1, 0, 1, 2, 3, 4, 5, 6):
        for same in (0, 1):
            yield bus_case(r, status, None, None, same)
    for status in (1, 2, 3):
        for sea in (0, 1):
            for hit in (None, 0, 1, 2, 3):
                for _ in range(2):
                    yield bus_case(r, status, sea, hit, 0)


def r_leaving(r):
    return leaving_case(r)


def r_cross(r):
    return cross_case(r, None, r.choice([None, None, 0, 1, 2, 3]))


def r_bus(r):
    return bus_case(r, None, None, r.choice([None, None, 0, 1, 2, 3]))


TESTS = [
    FnTest("CPerson::ProcessStateLeavingPark", ppc, host, edge=leaving_edges, rnd=r_leaving),
    FnTest("CPerson::ProcessStateWaitingToCrossRoadOutOfPark", ppc, host, edge=cross_edges, rnd=r_cross),
    FnTest("CPerson::ProcessStateWaitingForBusOuttaHere", ppc, host, edge=bus_edges, rnd=r_bus),
]
