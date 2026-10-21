"""Unit v5558, the ride creation, against libtpw_unit_BMP_CreateRide.dylib (tests/unit_BMP_CreateRide.cpp), main app
("SimTheme Park"), src/game/rides/BumperRide.cpp:
  BMP_CreateRide 0x1002756c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit
(BMP_StartColObject, BMP_AddBuoyToRide, BMP_StartColObjectForRide, TRACE) are fakes on both sides that record their arguments (a ride as its
index in the list); the first two answer a block the case allocates, so the flag bits the function sets on them are compared.

The world of a case: the list (magic, rides pointer, counters), 256 ride records, the 15 ride templates, the two performance clamps and
the sine table. What is compared: the answer, the list words, every ride record, the two answer blocks, the clamps and the event log.

No fused operations. Not tested: no free ride record (or a NULL rides pointer), where the original reads a ride at address 0 (low memory
on the Mac, a fault on the host); a kind outside -1 to -14 (the template index leaves the table).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BMP_CreateRide"
MASK = 0xFFFFFFFF

FN = 0x1002756C
SIZES = {FN: 2712}
OWN = set(SIZES)

COL_OBJ, BUOY, FOR_RIDE, TRACE = 0x1002EDFC, 0x1002EBD0, 0x1002ED74, 0x1006FCFC

ADDR = dict(world=0x102B56C0, templ=0x1029BE00, pmax=0x1029CA24, pmin=0x1029CA28, sine=0x102B4EC0)
NT = 14     # the templates: kinds -1 to -14, template (-kind - 1)
HOST_LAYOUT = [0x20, 0xD0, NT * 0xD0]
MAGIC = 0x4A454647
NREC = 256
CUR = {}
DUMPED = []


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def idx_of(e, p):
    rides = e.u32(ADDR["world"] + 8)
    return ((p - rides) & MASK) // 0xD0


def stub_col(e):
    e.cl.rec("BMP_StartColObject", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, CUR["col"])


def stub_buoy(e):
    e.cl.rec("BMP_AddBuoyToRide", (idx_of(e, e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, CUR["buoy"])


def stub_for_ride(e):
    e.cl.rec("BMP_StartColObjectForRide", (idx_of(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))


STUBS.internal(COL_OBJ, stub_col)
STUBS.internal(BUOY, stub_buoy)
STUBS.internal(FOR_RIDE, stub_for_ride)
STUBS.internal(TRACE, CL.recorder("TRACE", "si"))


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

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def p32(self, a):
        return a

    def answers(self, col, buoy):
        CUR["col"], CUR["buoy"] = col, buoy

    def call(self, args):
        r3, _, _ = T.call_ppc(self.e, FN, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_cr_base()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cr_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def p32(self, a):
        return a - self.base

    def answers(self, col, buoy):
        self.write(self.g("colret"), struct.pack("<Q", col))
        self.write(self.g("buoyret"), struct.pack("<Q", buoy))

    def call(self, args):
        return self.lib.tpw_cr_create(*[s32(a) for a in args]) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cr_addr.restype, lib.tpw_cr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cr_base.restype, lib.tpw_cr_base.argtypes = C.c_uint64, []
    lib.tpw_cr_alloc.restype, lib.tpw_cr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cr_create.restype, lib.tpw_cr_create.argtypes = C.c_uint32, [C.c_int32] * 6
    out = (C.c_int64 * 32)()
    n = lib.tpw_cr_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


SINE = [int(256 * math.sin(3.14159 * 2 * i / 512)) & MASK for i in range(512)]


def build(s, case):
    rides = s.alloc(NREC * 0xD0)
    col = s.alloc(0x20)
    buoy = s.alloc(0x20)
    s.answers(col, buoy)
    s.words(rides, case["rides"])
    s.words(col, case["col"])
    s.words(buoy, case["buoyw"])
    s.words(s.g("world"), [case["magic"], case["w1"], s.p32(rides), case["count"], case["w4"], case["w5"]])
    s.words(s.g("templ"), case["templ"])
    s.words(s.g("pmax"), [case["pmax"]])
    s.words(s.g("pmin"), [case["pmin"]])
    s.words(s.g("sine"), SINE)
    return rides, col, buoy


def run(s, case):
    try:
        rides, col, buoy = build(s, case)
        answer = s.call(case["args"])
        w = s.rd_words(s.g("world"), 6)
        return {"answer": answer, "log": s.log(), "world": [w[0], w[1], w[3], w[4], w[5]],
                "rides": s.rd_words(rides, NREC * 0x34), "col": s.rd_words(col, 8), "buoy": s.rd_words(buoy, 8),
                "perf": s.rd_words(s.g("pmax"), 1) + s.rd_words(s.g("pmin"), 1)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

def rnd_word(r):
    return r.choice([0, 1, MASK, 0x80000000, 0x7FFFFFFF, r.getrandbits(32), r.getrandbits(32)])


def small(r, lo, hi):
    return r.randrange(lo, hi + 1) & MASK


def base(r, tkind=None):
    c = {"magic": MAGIC if r.random() > 0.04 else r.choice([0, MAGIC + 1, MASK])}
    c.update(w1=r.getrandbits(32), count=r.choice([0, 1, r.randrange(0, 1000), MASK]), w4=r.getrandbits(32), w5=r.getrandbits(32))
    rides = [r.getrandbits(32) if r.random() < 0.7 else 0 for _ in range(NREC * 0x34)]
    for i in range(NREC):
        rides[i * 0x34] = r.choice([0, 0, r.randrange(1, 20), r.getrandbits(32)]) if r.random() < 0.5 else MASK & -r.randrange(1, 15)
    free = r.randrange(0, 64)
    rides[free * 0x34] = 0
    c["rides"] = rides
    t = [rnd_word(r) for _ in range(NT * 0x34)]
    for k in range(NT):
        o = k * 0x34
        t[o] = (r.randrange(-16, 2) if r.random() < 0.2 else -r.randrange(1, 15)) & MASK
        t[o + 7] = r.choice([small(r, -50, 200), small(r, 0, 100), rnd_word(r)])
        t[o + 25] = small(r, -2, 7)
        for j in range(12, 20):
            t[o + j] = r.choice([small(r, -100000, 100000), small(r, -300, 300), rnd_word(r)])
    c["templ"] = t
    c["pmax"] = r.choice([100, 100, small(r, -20, 200), rnd_word(r)])
    c["pmin"] = r.choice([0, 0, small(r, -20, 120), rnd_word(r)])
    c["col"] = [r.getrandbits(32) for _ in range(8)]
    c["buoyw"] = [r.getrandbits(32) for _ in range(8)]
    c["queues"] = {}
    return c


def call(r, c, kind=None, tword0=None, rot=None, id_=None):
    kind = -r.randrange(1, NT + 1) if kind is None else kind
    if tword0 is not None:
        c["templ"][(-kind - 1) * 0x34] = tword0 & MASK
    rot = r.choice([r.randrange(8), r.randrange(8), r.getrandbits(32)]) if rot is None else rot
    if id_ is None:
        id_ = 0 if r.random() < 0.6 else (((kind << 8) | r.randrange(0, NREC)) & MASK)
    c["args"] = [kind & MASK, small(r, -0x20000, 0x20000), small(r, -0x20000, 0x20000), rot, id_, r.getrandbits(32)]
    return c


def edges():
    r = random.Random(5558)
    for tw in range(-16, 2):
        for kind in (-1, -7, -14):
            yield call(r, base(r), kind=kind, tword0=tw)
    for rot in range(8):
        for tw in (-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12, -13, -14):
            yield call(r, base(r), tword0=tw, rot=rot, id_=0)
    for tw in (-1, -5, -14):
        for idv in ((-3 << 8) | 1, (-3 << 8) | 255, (-3 << 8) | 5):
            yield call(r, base(r), kind=-3, tword0=tw, id_=idv & MASK)
    c = base(r)
    c["magic"] = 0
    yield call(r, c)
    for pmax, pmin, v in ((100, 0, 50), (100, 0, 150), (100, 0, -5), (10, 20, 15), (20, 10, 30), (100, 0, 100), (100, 0, 0)):
        for tw in (-1, -2, -3, -4, -8):
            c = base(r)
            c["pmax"], c["pmin"] = pmax & MASK, pmin & MASK
            k = -r.randrange(1, NT + 1)
            c["templ"][(-k - 1) * 0x34 + 7] = v & MASK
            yield call(r, c, kind=k, tword0=tw, id_=0)


def rnd(r):
    return call(r, base(r))


TESTS = [
    FnTest("BMP_CreateRide", ppc, host, edge=edges, rnd=rnd),
]
