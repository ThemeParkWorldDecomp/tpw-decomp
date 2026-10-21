"""Unit v5764, the first-person ride helpers, against libtpw_unit_IsRideRideable.dylib (tests/unit_IsRideRideable.cpp), main app
("SimTheme Park"), src/game/TPCamera.cpp:
  StopRidingRideIn1stPerson 0x10032584, GetThingAtPosnIfEntrance 0x1003277c, IsRideRideable 0x10032a44

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory at the Mac's addresses, then the host
library's memory). The callees outside the unit (BMP_GetCameraMesh, RSSE_GetScriptPointer, RSSE_GetTourBaseID, FLY_GetCameraMesh,
IsCoaster, GetCoaster1stPersonCar, FindDataNodeIndexByID, GetThingAtPosnIfEntrance(int, int), ToggleRideRideIn1stPerson,
MoveCameraOverRideExit) are fakes on both sides that record their arguments and answer from the case's queues.

The world: the first-person flags and ride instance, the height field's size, the ride anim set table, an object block with words at
+0x20 and +0x24. Compared: the answer and the event log. No fused operations. Not tested: NaN and values past 2^31 tenths for the
float conversion (fctiwz saturation differs in the emulator), animSet indexes past the table.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_IsRideRideable"
MASK = 0xFFFFFFFF

FN_STOP, FN_GET, FN_RIDE = 0x10032584, 0x1003277C, 0x10032A44
SIZES = {FN_STOP: 100, FN_GET: 140, FN_RIDE: 248}
OWN = set(SIZES)

ADDR = dict(flags=0x102B5A40, instance=0x102B5A3C, width=0x102C6C68, height=0x102C6C64, sets=0x102C79A4)
HOST_LAYOUT = [8]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def q_stub(addr, name, spec):
    def fn(e):
        L = e.cl
        vals = []
        for i, k in enumerate(spec):
            r = e.r(3 + i)
            vals.append(s32(r) if k == "i" else (r & MASK))
        L.rec(name, tuple(vals))
        e.set_r(3, L.ret(name) & MASK)
    STUBS.internal(addr, fn)


q_stub(0x1002CF30, "BMP_GetCameraMesh", "i")
q_stub(0x10075278, "FLY_GetCameraMesh", "i")
q_stub(0x10043330, "IsCoaster", "u")
q_stub(0x100433F4, "GetCoaster1stPersonCar", "u")
q_stub(0x10032618, "GetThingAtPosnIfEntrance", "ii")
q_stub(0x101FFDC8, "FindDataNodeIndexByID", "uuu")
DUMMY_SCRIPT = 0x102C0000


def stub_script(e):
    e.cl.rec("RSSE_GetScriptPointer", (s32(e.r(3)),))
    e.set_r(3, DUMMY_SCRIPT if e.cl.ret("RSSE_GetScriptPointer") != 0 else 0)


def stub_tour(e):
    e.cl.rec("RSSE_GetTourBaseID", (1 if e.r(3) != 0 else 0,))
    e.set_r(3, e.cl.ret("RSSE_GetTourBaseID") & MASK)


def stub_toggle(e):
    L = e.cl
    L.rec("ToggleRideRideIn1stPerson", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(7)), s32(e.r(8))))
    e.write(e.r(6), struct.pack(">I", L.ret("ToggleRideRideIn1stPerson") & MASK))


def stub_move(e):
    e.cl.rec("MoveCameraOverRideExit", ())


STUBS.internal(0x100D668C, stub_script)
STUBS.internal(0x100D6BD0, stub_tour)
STUBS.internal(0x10032FF4, stub_toggle)
STUBS.internal(0x10032838, stub_move)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"
    psz = ">I"

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

    def call(self, op, args):
        if op == "stop":
            return T.call_ppc(self.e, FN_STOP, [])[0] & 0
        if op == "get":
            return T.call_ppc(self.e, FN_GET, [T.F(args[0]), T.F(args[1])])[0] & MASK
        return T.call_ppc(self.e, FN_RIDE, [args[0] & MASK, args[1] & MASK])[0] & MASK


class Host(Side):
    fmt = "<I"
    psz = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ir_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ir_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        if op == "stop":
            self.lib.tpw_ir_stop()
            return 0
        if op == "get":
            return self.lib.tpw_ir_get(args[0], args[1]) & MASK
        return self.lib.tpw_ir_ride(args[0], args[1] & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ir_addr.restype, lib.tpw_ir_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ir_alloc.restype, lib.tpw_ir_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ir_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ir_stop.restype, lib.tpw_ir_stop.argtypes = None, []
    lib.tpw_ir_get.restype, lib.tpw_ir_get.argtypes = C.c_uint32, [C.c_float, C.c_float]
    lib.tpw_ir_ride.restype, lib.tpw_ir_ride.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ir_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("flags"), [case["flags"]])
    s.words(s.g("instance"), [case["instance"]])
    s.words(s.g("width"), [case["width"]])
    s.words(s.g("height"), [case["height"]])
    sets = s.g("sets")
    size = 4 if s.psz == ">I" else 8
    for i, v in enumerate(case["sets"]):
        s.write(sets + i * size, struct.pack(s.psz, v))
    args = list(case["args"])
    if case["op"] == "ride":
        if case["obj"] is None:
            args[0] = 0
        else:
            a = s.alloc(0x40)
            s.words(a + 0x20, [case["obj"][0]])
            s.words(a + 0x24, [case["obj"][1]])
            args[0] = a
    return args


def run(s, case):
    try:
        args = build(s, case)
        ans = s.call(case["op"], args)
        return {"answer": ans, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    q = {
        "BMP_GetCameraMesh": [r.choice([0, 1, 7])],
        "RSSE_GetScriptPointer": [r.choice([0, 1])],
        "RSSE_GetTourBaseID": [r.choice([0, 3, -1])],
        "FLY_GetCameraMesh": [r.choice([0, 1, 9])],
        "IsCoaster": [r.choice([0, 1])],
        "GetCoaster1stPersonCar": [r.choice([0, 1, 5])],
        "FindDataNodeIndexByID": [r.choice([0xFFFFFFFF, 0, 4])],
        "GetThingAtPosnIfEntrance": [r.choice([0, 0x1234])],
        "ToggleRideRideIn1stPerson": [r.choice([0, 1, 5])],
    }
    return {"op": op, "queues": q, "flags": r.choice([0, 4, 0x16, 0x14, 0xFB, 0x4, 0x2]), "instance": r.choice([0, 1, 7, 63]),
            "width": r.choice([1, 50, 128, 256]), "height": r.choice([1, 40, 128, 300]),
            "sets": [r.randrange(0, 0x7FFFFFFF) for _ in range(64)], "args": [0, 0], "obj": None}


def stop_case(r, flags=None):
    c = base(r, "stop")
    if flags is not None:
        c["flags"] = flags
    return c


def stop_edges():
    r = random.Random(11)
    for f in (0, 4, 3, 0xFFFFFFFB, 0xFFFFFFFF, 4, 4):
        yield stop_case(r, f)


def stop_random(r):
    return stop_case(r)


def get_case(r, x=None, z=None):
    c = base(r, "get")
    k = r.random()

    def rv():
        if k < 0.3:
            return r.uniform(-30, 3000)
        if k < 0.7:
            return r.uniform(-5, 400)
        return r.uniform(-1e6, 1e6)
    c["args"] = [T.f32(rv()) if x is None else x, T.f32(rv()) if z is None else z]
    return c


def get_edges():
    r = random.Random(12)
    vals = [0.0, -0.0, 9.99, 10.0, -0.5, -10.0, -10.5, 9.9999, 1000.0, 1279.0, 1280.0, 1289.0, 1290.0, 3000.0, 400.0, 500.0, 1e9, -1e9]
    for x in vals:
        for z in vals:
            yield get_case(r, T.f32(x), T.f32(z))
    for w, h in ((1, 1), (50, 40)):
        for x in (0.0, 495.0, 505.0, 9.0):
            c = get_case(r, T.f32(x), T.f32(x + 5))
            c["width"], c["height"] = w, h
            yield c


def get_random(r):
    return get_case(r)


def ride_case(r, bits=None, objnull=None, mesh=None):
    c = base(r, "ride")
    if bits is not None:
        names = ["BMP_GetCameraMesh", "RSSE_GetScriptPointer", "RSSE_GetTourBaseID", "FLY_GetCameraMesh", "IsCoaster",
                 "GetCoaster1stPersonCar", "FindDataNodeIndexByID"]
        vals = [(0, 1), (0, 1), (0, 3), (0, 1), (0, 1), (0, 1), (0xFFFFFFFF, 4)]
        for i, n in enumerate(names):
            c["queues"][n] = [vals[i][(bits >> i) & 1]]
    if objnull is None:
        objnull = r.random() < 0.2
    if mesh is None:
        mesh = r.choice([0, 0, 5, r.randrange(1, MASK)])
    c["obj"] = None if objnull else (r.choice([0, 3, r.randrange(0, MASK)]), mesh)
    c["args"] = [0, r.choice([0, 1, 2, 31, 63, r.randrange(0, 64)])]
    return c


def ride_edges():
    r = random.Random(13)
    for bits in range(128):
        yield ride_case(r, bits, True)
        yield ride_case(r, bits, False, 0)
        yield ride_case(r, bits, False, 5)


def ride_random(r):
    return ride_case(r)


TESTS = [
    FnTest("StopRidingRideIn1stPerson", ppc, host, edge=stop_edges, rnd=stop_random),
    FnTest("GetThingAtPosnIfEntrance", ppc, host, edge=get_edges, rnd=get_random),
    FnTest("IsRideRideable", ppc, host, edge=ride_edges, rnd=ride_random),
]
