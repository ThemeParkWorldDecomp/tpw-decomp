"""Unit v5763, three coaster train functions, against libtpw_unit_DeactivateCoasterTrain.dylib (tests/unit_DeactivateCoasterTrain.cpp), main app,
src/game/rides/CoasterTrain.cpp:
  DeactivateCoasterTrain 0x1004aa70, GetTrainFrontAndRearSamples 0x1004a7d0, GetCarWantedSpeedAtHeight 0x1004aedc

A case is a world and a call, built by ONE piece of Python code in emulator memory (big-endian, 4-byte pointers, Mac offsets) and in host
memory (little-endian, 8-byte pointers, the host structs' offsets from the library's layout export). DisableMeshDraw (both forms, the mesh
is identified by the word at its start) and StopTrainSounds are recording fakes on both sides.

The world: a train with up to 8 cars, a coaster instance's scale, car types, up to 7 ride anim sets in gRideAnimSets. Compared: the log,
the train's flags and samples, every car's flags, the speed answer. Fused operations: fmadds in GetCarWantedSpeedAtHeight (HardwareRounding).
Not tested: a car count that walks past the 8 allocated cars.

GetCarWantedSpeedAtHeight's coaster type: with the port fixes on (unit coaster64) the translation reads its three floats as
tag_sCoasterType's mF24, mF28 and mF2C, so the host world puts the case's words there (the layout export says where).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_DeactivateCoasterTrain"
MASK = 0xFFFFFFFF

FN_DEACT, FN_SAMPLES, FN_SPEED = 0x1004AA70, 0x1004A7D0, 0x1004AEDC
SIZES = {FN_DEACT: 200, FN_SAMPLES: 116, FN_SPEED: 132}
OWN = set(SIZES)

DMD1, DMD3, STOPSND = 0x100653A4, 0x1006548C, 0x100471BC

ADDR = dict(gRideAnimSets=0x102C79A4)
NCARS = 8
NRIDES = 8

# Mac offsets
MAC = dict(car_size=0x60, car_type=4, car_ride=0x0C, car_f38=0x38,
           train_size=0x80, train_f08=8, train_n=0x0C, train_front=0x20, train_rear=0x24, train_cars=0x28, train_pos=0x74,
           coaster_fac=0xAC, coaster_size=0x200, ct_f24=0x24, ct_f28=0x28, ct_size=0x40,
           rs_flags=4, rs_mesh=8, rs_fc0=0xC0, rs_fc4=0xC4, rs_size=0x100, mesh_size=0x20, type_f24=0x24, type_f28=0x28, type_f2c=0x2C,
           type_size=0x100, psz=4)
HOST = {}
HOST_KEYS = ["car_size", "car_type", "car_ride", "car_f38", "train_size", "train_f08", "train_n", "train_front", "train_rear", "train_cars",
             "train_pos", "coaster_fac", "coaster_size", "ct_f24", "ct_f28", "ct_size", "rs_flags", "rs_mesh", "rs_fc0", "rs_fc4", "rs_size",
             "mesh_size", "type_f24", "type_f28", "type_f2c", "type_size"]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_dmd1(e):
    e.cl.rec("DisableMeshDraw", (e.u32(e.r(3)),))


def stub_dmd3(e):
    e.cl.rec("DisableMeshDraw3", (e.u32(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))


def stub_stop(e):
    L = e.cl
    L.rec("StopTrainSounds", (L.lab(e.r(3)), L.lab(e.r(4))))


STUBS.internal(DMD1, stub_dmd1)
STUBS.internal(DMD3, stub_dmd3)
STUBS.internal(STOPSND, stub_stop)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + ("I" if self.o["psz"] == 4 else "Q"), v))

    def hf(self, a, w):
        self.words(a, [w])


class Ppc(Side):
    fmt = ">I"
    o = MAC

    def __init__(self, e, case):
        self.e = e
        self.coaster = e.alloc(MAC["coaster_size"])
        self.train = e.alloc(MAC["train_size"])
        self.L = e.cl = CL.PpcLog({self.coaster: 1, self.train: 2}, case["queues"])
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

    def call2(self, fn):
        with ROUNDING(self.e):
            T.call_ppc(self.e, fn, [self.coaster, self.train])

    def speed(self, tb, a, b, c, d):
        with ROUNDING(self.e):
            _, _, f1 = T.call_ppc(self.e, FN_SPEED, [tb, T.F(bitsf(a)), T.F(bitsf(b)), T.F(bitsf(c)), T.F(bitsf(d))])
        return canon_f(fb(f1))


class Host(Side):
    fmt = "<I"
    o = HOST

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.coaster = lib.tpw_dct_alloc(HOST["coaster_size"])
        self.train = lib.tpw_dct_alloc(HOST["train_size"])
        CL.host_begin(lib, {1: self.coaster, 2: self.train}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_dct_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_dct_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def hf(self, a, w):
        self.write(a, struct.pack("<HH", (w >> 16) & 0xFFFF, w & 0xFFFF))

    def log(self):
        return CL.host_end(self.lib)

    def call2(self, fn):
        (self.lib.tpw_dct_deactivate if fn == FN_DEACT else self.lib.tpw_dct_samples)(self.coaster, self.train)

    def speed(self, tb, a, b, c, d):
        return canon_f(fb(self.lib.tpw_dct_speed(tb, bitsf(a), bitsf(b), bitsf(c), bitsf(d))))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_dct_addr.restype, lib.tpw_dct_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_dct_alloc.restype, lib.tpw_dct_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_dct_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_dct_deactivate.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_dct_samples.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_dct_speed.restype = C.c_float
    lib.tpw_dct_speed.argtypes = [C.c_uint64] + [C.c_float] * 4
    out = (C.c_int64 * 32)()
    n = lib.tpw_dct_layout(out)
    assert n == len(HOST_KEYS), n
    HOST.clear()
    HOST.update(zip(HOST_KEYS, list(out)[:n]))
    HOST["psz"] = 8
    HOST["coaster_size"] = max(HOST["coaster_size"], 0x200)
    lib.bound = True


def build(s, case):
    o = s.o
    t = case["train"]
    # car types
    ctypes_ = []
    for ct in case["ctypes"]:
        a = s.alloc(o["ct_size"])
        s.words(a + o["ct_f24"], [ct[0]])
        s.words(a + o["ct_f28"], [ct[1]])
        ctypes_.append(a)
    # ride sets
    arr = s.g("gRideAnimSets")
    for i in range(NRIDES):
        rd = case["rides"][i]
        if rd is None:
            s.ptr(arr + i * o["psz"], 0)
            continue
        flags, meshid, fc0, fc4 = rd
        mesh = s.alloc(o["mesh_size"])
        s.words(mesh, [meshid])
        rs = s.alloc(o["rs_size"])
        s.words(rs + o["rs_flags"], [flags])
        s.ptr(rs + o["rs_mesh"], mesh)
        s.hf(rs + o["rs_fc0"], fc0)
        s.hf(rs + o["rs_fc4"], fc4)
        s.ptr(arr + i * o["psz"], rs)
    # cars (one spare car before and after)
    cars = s.alloc(o["car_size"] * (NCARS + 2)) + o["car_size"]
    for i, c in enumerate(case["cars"]):
        a = cars + i * o["car_size"]
        s.words(a, [c["flags"]])
        s.ptr(a + o["car_type"], ctypes_[c["type"]])
        s.words(a + o["car_ride"], [c["ride"]])
        s.words(a + o["car_f38"], [c["f38"]])
    s.words(s.train, [t["flags"]])
    s.words(s.train + o["train_f08"], [t["f08"]])
    s.words(s.train + o["train_n"], [t["n"]])
    s.words(s.train + o["train_front"], [t["front"]])
    s.words(s.train + o["train_rear"], [t["rear"]])
    s.ptr(s.train + o["train_cars"], cars)
    s.words(s.train + o["train_pos"], [t["pos"]])
    s.words(s.coaster + o["coaster_fac"], [case["fac"]])
    s.cars = cars


def observe(s, case):
    o = s.o
    t = s.train
    return {
        "log": s.log(),
        "flags": s.rd_words(t, 1),
        "front": [canon_f(x) for x in s.rd_words(t + o["train_front"], 1)],
        "rear": [canon_f(x) for x in s.rd_words(t + o["train_rear"], 1)],
        "carflags": [s.rd_words(s.cars + i * o["car_size"], 1)[0] for i in range(NCARS)],
    }


def run(s, case):
    try:
        if case["op"] == "speed":
            o = s.o
            tb = s.alloc(o["type_size"])
            s.words(tb + o["type_f24"], [case["t24"]])
            s.words(tb + o["type_f28"], [case["t28"]])
            s.words(tb + o["type_f2c"], [case["t2c"]])
            return {"speed": s.speed(tb, *case["args"])}
        build(s, case)
        s.call2(FN_DEACT if case["op"] == "deact" else FN_SAMPLES)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.06:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def rnorm(r):
    return fb(r.uniform(-50, 50))


def world(r, op, n=None, f08=None, ridefix=None):
    t = dict(flags=r.choice([0, 2, 3, 0xFFFFFFFF, 0x12345678]) , f08=r.randrange(0, 4) if f08 is None else f08,
             n=r.randrange(0, 5) if n is None else n, front=r.getrandbits(32), rear=r.getrandbits(32), pos=rnorm(r))
    if t["n"] == 0 and t["f08"] == 0:
        t["f08"] = 1
    rides = [None]
    for i in range(1, NRIDES):
        rides.append((r.choice([0, 0x4000, 0x4001, 0xFFFFBFFF, 0xFFFFFFFF, 0x20]), 0x1000 + i * 17 + r.randrange(5000) * 8,
                      r.getrandbits(32), r.getrandbits(32)))
    cars = []
    for i in range(NCARS):
        ride = r.choice([0, 0, r.randrange(1, NRIDES)])
        if ridefix is not None:
            ride = ridefix
        cars.append(dict(flags=r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)]), type=r.randrange(3), ride=ride, f38=rnorm(r)))
    fac = r.choice([fb(3.0), fb(1.0), fb(r.uniform(0.1, 9)), rf(r)])
    return {"op": op, "queues": {}, "train": t, "rides": rides, "cars": cars, "fac": fac,
            "ctypes": [(rnorm(r), rnorm(r)) for _ in range(3)]}


def deact_edges():
    r = random.Random(5763)
    for n in (0, 1, 2, 4):
        for f08 in (0, 1, 3):
            yield world(r, "deact", n, f08)
    for ride in range(1, NRIDES):
        yield world(r, "deact", 3, 1, ridefix=ride)


def deact_random(r):
    return world(r, "deact")


def samples_edges():
    r = random.Random(5764)
    for n in (1, 2, 4):
        for f08 in (0, 1, 3):
            yield world(r, "samples", n, f08)
    c = world(r, "samples", 0, 2)
    yield c
    c = world(r, "samples", 2, 1)
    c["fac"] = 0
    yield c
    c = world(r, "samples", 2, 1)
    c["fac"] = 0x7FC00000
    yield c


def samples_random(r):
    return world(r, "samples")


def speed_case(r, a=None, b=None, c=None, d=None):
    k = r.random()
    return {"op": "speed", "queues": {}, "t24": rf(r), "t28": rf(r), "t2c": rf(r),
            "args": [rf(r) if a is None else a, rf(r) if b is None else b, rf(r) if c is None else c, rf(r) if d is None else d]}


def speed_edges():
    r = random.Random(5765)
    one, two, zero, neg = fb(1.0), fb(2.0), 0, fb(-1.0)
    for c, d in ((one, two), (two, one), (one, one), (zero, 0x80000000), (0x7FC00000, one), (one, 0x7FC00000)):
        for b in (neg, zero, fb(100.0), 0x7FC00000):
            cs = speed_case(r, fb(3.0), b, c, d)
            cs["t24"], cs["t28"], cs["t2c"] = one, two, fb(0.5)
            yield cs
            yield speed_case(r, None, b, c, d)
    for bb in (fb(5.0), fb(5.0000005)):
        cs = speed_case(r, fb(5.0), bb, one, one)
        cs["t24"], cs["t28"], cs["t2c"] = one, one, one
        yield cs


def speed_random(r):
    return speed_case(r)


TESTS = [
    FnTest("DeactivateCoasterTrain", ppc, host, edge=deact_edges, rnd=deact_random),
    FnTest("GetTrainFrontAndRearSamples", ppc, host, edge=samples_edges, rnd=samples_random),
    FnTest("GetCarWantedSpeedAtHeight", ppc, host, edge=speed_edges, rnd=speed_random),
]
