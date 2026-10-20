"""Unit v5275, RoundUp and GetCoasterTrackStats, against libtpw_unit_RoundUp.dylib (tests/unit_RoundUp.cpp), main app
("SimTheme Park"), src/TPConfig_stats_RoundUp.cpp:
  RoundUp 0x1004da1c, GetCoasterTrackStats 0x1004d970

A case is a call. RoundUp: modf is the C library's on both sides (math.modf in the PowerPC fake, the glue import), the answer is compared.
GetCoasterTrackStats: GetCoasterInstanceFromCObject (0x100527e0) is a fake on both sides that records its argument (a label) and answers
an instance block the case builds (Mac offsets 0x108..0x114 on the PowerPC side, the host struct's offsets on the host side, both
written by the same code); the four out words are compared with the event log, and the instance block's four fields are read back.

No fused operations. Not tested: RoundUp of NaN, infinities or values at and beyond 2^31 (fctiwz saturates differently in Unicorn).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RoundUp"
MASK = 0xFFFFFFFF
TOC = 0x102648E0 + 0x33E0       # r2 of the main app: the constant 0.0f is at TOC - 0x33e0

FN_ROUND, FN_STATS = 0x1004DA1C, 0x1004D970
SIZES = {FN_ROUND: 92, FN_STATS: 116}
OWN = set(SIZES)
CALLEE_INST = 0x100527E0

MAC_OFF = {"loops": 0x108, "length": 0x10C, "height": 0x110, "speed": 0x114}
MAC_SIZE = 0x14C
KEYS = ("loops", "length", "height", "speed")
SENT = 0xA5A5A5A5


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_modf(e):
    """modf(double, double*): fraction in f1, whole part stored at r4... (r5 here: the double is in f1, the pointer is the third slot)."""
    frac, whole = math.modf(e.f(1))
    e.write(e.r(5), struct.pack(">d", whole))
    e.set_f1(frac)
    e.write(e.r(1) + 0x14, struct.pack(">I", TOC))      # the glue saves the TOC where the caller reloads it from


def stub_inst(e):
    """GetCoasterInstanceFromCObject(void*) -> instance."""
    e.cl.rec("GetCoasterInstanceFromCObject", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.inst)


STUBS.imp("modf", stub_modf)
STUBS.internal(CALLEE_INST, stub_inst)


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
        self.L = e.cl = CL.PpcLog({}, {})
        self.saved = STUBS.activate(e)
        self.off = MAC_OFF
        self.size = MAC_SIZE

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_inst(self, a):
        self.e.inst = a

    def round_up(self, bits):
        r3, _, _ = T.call_ppc(self.e, FN_ROUND, [T.F(bitsf(bits))])
        return T.s32(r3)

    def stats(self, obj, a, b, c, d):
        T.call_ppc(self.e, FN_STATS, [obj, a, b, c, d])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, {})
        self.off = lib.host_off
        self.size = lib.host_size

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_rup_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_inst(self, a):
        self.lib.tpw_rup_setinst(a)

    def round_up(self, bits):
        return self.lib.tpw_rup_roundup(bitsf(bits))

    def stats(self, obj, a, b, c, d):
        self.lib.tpw_rup_stats(obj, a, b, c, d)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rup_alloc.restype, lib.tpw_rup_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rup_setinst.restype, lib.tpw_rup_setinst.argtypes = None, [C.c_uint64]
    lib.tpw_rup_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rup_roundup.restype, lib.tpw_rup_roundup.argtypes = C.c_int32, [C.c_float]
    lib.tpw_rup_stats.restype, lib.tpw_rup_stats.argtypes = None, [C.c_uint64] * 5
    out = (C.c_int64 * 32)()
    n = lib.tpw_rup_layout(out)
    assert n == 5, n
    lib.host_off = dict(zip(KEYS, list(out)[:4]))
    lib.host_size = out[4]
    lib.bound = True


def run_round(s, case):
    try:
        return {"answer": s.round_up(case["bits"])}
    finally:
        s.done()


def run_stats(s, case):
    try:
        inst = s.alloc(s.size)
        for k in KEYS:
            s.words(inst + s.off[k], [case[k]])
        s.set_inst(inst)
        outs = [s.alloc(4) for _ in range(4)]
        for o in outs:
            s.words(o, [SENT])
        obj = case["obj"]
        s.stats(obj, *outs)
        res = {"log": s.log()}
        for i, o in enumerate(outs):
            res["out%d" % i] = canon(s.rd_words(o, 1)[0]) if i < 3 else s.rd_words(o, 1)[0]
        for k in KEYS:
            res["in_" + k] = s.rd_words(inst + s.off[k], 1)[0]
        return res
    finally:
        s.done()


def ppc_round(e, case):
    return run_round(Ppc(e, case), case)


def host_round(lib, case):
    return run_round(Host(lib, case), case)


def ppc_stats(e, case):
    return run_stats(Ppc(e, case), case)


def host_stats(lib, case):
    return run_stats(Host(lib, case), case)


SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def round_edges():
    for v in (0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 1.5, -1.5, 2.25, -2.75, 1e-30, -1e-30, 1.0000001, 0.99999994, 16777216.0, 16777215.0,
              1073741824.0, -1073741824.0, 2147483520.0, -2147483648.0, -2147483520.0, 8388607.5, 100.0, 3.0e-45):
        yield {"bits": fb(v)}


def round_random(r):
    k = r.random()
    if k < 0.3:
        return {"bits": fb(float(r.randrange(-100000, 100000)))}
    if k < 0.6:
        return {"bits": fb(r.uniform(-1e5, 1e5))}
    if k < 0.8:
        return {"bits": fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 9))}
    return {"bits": fb(r.uniform(-2e9, 2e9))}


def stats_case(r, obj=None):
    c = {"obj": r.choice([0, 0x1000, 0x2468]) if obj is None else obj}
    for k in KEYS:
        c[k] = r.choice(SPECIAL) if r.random() < 0.15 else r.getrandbits(32) if k == "loops" else rf(r)
    return c


def stats_edges():
    r = random.Random(5275)
    for obj in (0, 0x1000):
        yield stats_case(r, obj)
    yield {"obj": 0x1000, "loops": MASK, "length": 0x7FC00000, "height": 0x80000000, "speed": 0xFF800000}
    yield {"obj": 0, "loops": 0, "length": 0, "height": 0x3F800000, "speed": 0xBF800000}


def stats_random(r):
    return stats_case(r)


TESTS = [
    FnTest("RoundUp", ppc_round, host_round, edge=round_edges, rnd=round_random),
    FnTest("GetCoasterTrackStats", ppc_stats, host_stats, edge=stats_edges, rnd=stats_random),
]
