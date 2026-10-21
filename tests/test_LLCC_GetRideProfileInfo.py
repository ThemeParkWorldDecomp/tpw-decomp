"""Unit v5691, the ride profile table and the pylon move start, against libtpw_unit_LLCC_GetRideProfileInfo.dylib
(tests/unit_LLCC_GetRideProfileInfo.cpp), main app, src/game/park/LLCellRide.cpp:
  LLCC_GetRideProfileInfo 0x1008f880, LLCC_StartPylonMove 0x1008f0b4

A case is a world and a call, built by one piece of Python for both memories. CCellBase::GetTrackCellEquiv and CMapCell::Delete are fakes
on both sides that record the cell's offset from the world (and Delete's arguments).

The world: the profile table filled with junk, a ride whose word at 0x18 points at a shape (size, then kinds at 8 + z * 0xa0 + x * 8);
or the world block, the track cell block and the globals. Compared: the answer, the table, the globals, the track cell, the event log.
No fused operations. Not tested: a shape wider or higher than 16 (the original writes past the table).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCC_GetRideProfileInfo"
MASK = 0xFFFFFFFF

FN_P, FN_S = 0x1008F880, 0x1008F0B4
SIZES = {FN_P: 280, FN_S: 196}
OWN = set(SIZES)

CALLEE_TRACK = 0x100F48C4
CALLEE_DELETE = 0x1009D0F4

ADDR = dict(tbl=0x102DDB7C, e0b4=0x1029E0B4, e0b8=0x1029E0B8, e08c=0x102DE08C, e090=0x102DE090, world=0x1037EBFC)
HOST_LAYOUT = [0x408, 4]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_track(e):
    L = e.cl
    L.rec("GetTrackCellEquiv", (s32(e.r(3) - L.world),))
    e.set_r(3, L.trk)


def stub_delete(e):
    L = e.cl
    L.rec("Delete", (s32(e.r(3) - L.world), s32(e.r(4)), s32(e.r(5))))


STUBS.internal(CALLEE_TRACK, stub_track)
STUBS.internal(CALLEE_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

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

    def set_shape(self, ride, shape):
        self.words(ride + 0x18, [shape])

    def ptr(self, name, val):
        setattr(self.L, name, val)
        if name in ADDR:
            self.write(ADDR[name], struct.pack(">I", val))

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_p(self, ride):
        r3, _, _ = T.call_ppc(self.e, FN_P, [ride & MASK])
        return r3 & MASK

    def call_s(self, x, z, f):
        T.call_ppc(self.e, FN_S, [x & MASK, z & MASK, f & MASK])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def set_shape(self, ride, shape):
        self.write(ride + self.lib.shape_off, struct.pack("<Q", shape))

    def g(self, name):
        return self.lib.tpw_llp_addr(name.encode())

    def ptr(self, name, val):
        self.write(self.g(name), struct.pack("<Q", val))

    def alloc(self, n):
        return self.lib.tpw_llp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_p(self, ride):
        return self.lib.tpw_llp_profile(ride)

    def call_s(self, x, z, f):
        self.lib.tpw_llp_start(s32(x), s32(z), s32(f))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_llp_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "profile").restype, getattr(lib, p + "profile").argtypes = C.c_uint64, [C.c_uint64]
    getattr(lib, p + "start").restype, getattr(lib, p + "start").argtypes = None, [C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:2] == HOST_LAYOUT, list(out)[:n]
    lib.shape_off = out[2]
    lib.bound = True


def build(s, case):
    s.words(s.g("tbl"), case["junk"])
    if case["op"] == "p":
        w, h, kinds = case["shape"]
        shape = s.alloc(8 + 0xA0 * 20 + 16)
        s.words(shape, [w, h])
        for (x, z), k in kinds.items():
            s.words(shape + 8 + z * 0xA0 + x * 8, [k, 0x77])
        ride = s.alloc(0x40)
        s.set_shape(ride, shape)
        s.ride = ride
    else:
        world = s.alloc(0x400)
        trk = s.alloc(16)
        s.words(trk, case["trk"])
        s.ptr("world", world)
        s.ptr("trk", trk)
        s.trk = trk
        s.words(s.g("e0b4"), [5])
        s.words(s.g("e0b8"), [6])
        s.words(s.g("e08c"), [case["e08c"]])
        s.write(s.g("e090"), b"\x00")


def execute(s, case):
    if case["op"] == "p":
        return s.call_p(s.ride)
    return s.call_s(*case["args"])


def observe(s, case, answer):
    r = {"log": s.log(), "tbl": s.rd_words(s.g("tbl"), 0x102)}
    if case["op"] == "p":
        r["answer"] = answer == s.g("tbl")
    else:
        r["g"] = s.rd_words(s.g("e0b4"), 1) + s.rd_words(s.g("e0b8"), 1) + s.rd_words(s.g("e08c"), 1)
        r["e090"] = s.read(s.g("e090"), 1)
        r["trk"] = s.rd_words(s.trk, 4)
    return r


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


def base(r, op):
    return {"op": op, "queues": {}, "junk": [r.getrandbits(32) for _ in range(0x102)]}


def p_case(r, w=None, h=None, kind=None):
    c = base(r, "p")
    w = r.randrange(0, 17) if w is None else w
    h = r.randrange(0, 17) if h is None else h
    kinds = {}
    for z in range(h):
        for x in range(w):
            kinds[(x, z)] = kind if kind is not None else r.choice([r.randrange(0, 0x1A), r.randrange(0, 0x18), 0xFFFFFFFF, 0x80000000])
    c["shape"] = (w, h, kinds)
    return c


def p_edges():
    r = random.Random(11)
    for k in list(range(0, 0x1A)) + [0xFFFFFFFF, 0x80000000]:
        yield p_case(r, 1, 1, k)
        yield p_case(r, 3, 2, k)
    for w, h in ((0, 0), (0, 5), (5, 0), (16, 16), (16, 1), (1, 16)):
        yield p_case(r, w, h)


def s_case(r, x=None, z=None, f=None):
    c = base(r, "s")
    c["args"] = [r.randrange(-200, 300) if x is None else x, r.randrange(-3, 130) if z is None else z, r.choice([0, 1, 2, -1]) if f is None else f]
    c["trk"] = [r.getrandbits(32), r.getrandbits(32), r.getrandbits(32), r.getrandbits(32)]
    c["e08c"] = r.getrandbits(32)
    return c


def s_edges():
    r = random.Random(12)
    for x, z in ((0, 0), (-1, 0), (127, 0), (0, 127), (-1, -1), (128, 0), (65535, 3), (-70000, 5)):
        for f in (0, 1, -1):
            yield s_case(r, x, z, f)


TESTS = [
    FnTest("LLCC_GetRideProfileInfo", ppc, host, edge=p_edges, rnd=lambda r: p_case(r)),
    FnTest("LLCC_StartPylonMove", ppc, host, edge=s_edges, rnd=lambda r: s_case(r)),
]
