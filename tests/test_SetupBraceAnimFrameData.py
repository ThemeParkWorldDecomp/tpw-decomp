"""Unit v5214, SetupBraceAnimFrameData, against libtpw_unit_SetupBraceAnimFrameData.dylib (tests/unit_SetupBraceAnimFrameData.cpp), main app
("SimTheme Park"), src/game/rides/CoasterPylon.cpp:
  SetupBraceAnimFrameData 0x1003bb44

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The only callee is modf (C library): the PowerPC
side runs Python's math.modf, the host the native one.

The world: gRideAnimSets entry, the set's time-control block (0x38-byte records), a brace (flags, five floats, set index, pylon data),
the pylon data's flag and its +0x18 float table. Compared: the brace's floats, the whole time-control block, the table.

No fused operations. Not tested: a NULL brace or data (read unconditionally).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SetupBraceAnimFrameData"
MASK = 0xFFFFFFFF

FN = 0x1003bb44
SIZES = {FN: 652}
OWN = set(SIZES)
ADDR = dict(sets=0x102c79a4)
TC_BYTES = 5 * 0x38
TABLE_BYTES = 0x140

# Mac offsets: brace (flags, e8, ec, f0, f4, f8, index, data), data (flags, f18), set (time controls); sizes
MAC = dict(flags=8, e8=0xe8, ec=0xec, f0=0xf0, f4=0xf4, f8=0xf8, idx=0x70, data=0x100, bsize=0x110,
           dflags=4, f18=0x18, dsize=0x40, tc=0x10, ssize=0x200, ptr=4)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_modf(e):
    x = e.f(1)
    frac, whole = math.modf(x)
    e.write(e.r(5), struct.pack(">d", whole))
    e.set_f1(frac)


STUBS.imp("modf", stub_modf)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + ("I" if self.O["ptr"] == 4 else "Q"), v))


class Ppc(Side):
    fmt = ">I"
    O = MAC

    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, case["queues"])
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
        return self.e.cl.log

    def call(self, brace):
        T.call_ppc(self.e, FN, [0, brace & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.O = lib.O
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sb_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, brace):
        self.lib.tpw_sb_setup(brace)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sb_addr.restype, lib.tpw_sb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sb_alloc.restype, lib.tpw_sb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sb_setup.restype, lib.tpw_sb_setup.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sb_layout(out)
    v = list(out)[:n]
    lib.O = dict(flags=v[0], e8=v[1], ec=v[2], f0=v[3], f4=v[4], f8=v[5], idx=v[6], data=v[7], bsize=v[8],
                 dflags=v[9], f18=v[10], dsize=v[11], tc=v[12], ssize=v[13], ptr=8)
    lib.bound = True


def build(s, case):
    O = s.O
    brace = s.alloc(O["bsize"])
    data = s.alloc(O["dsize"])
    table = s.alloc(TABLE_BYTES)
    tc = s.alloc(TC_BYTES)
    st = s.alloc(O["ssize"])
    s.ptr(s.g("sets") + O["ptr"] * case["index"], st)
    s.ptr(st + O["tc"], tc)
    s.words(table, case["table"])
    s.words(tc, case["tc"])
    s.words(data + O["dflags"], [case["dflags"]])
    s.ptr(data + O["f18"], table - 0xd0)
    s.words(brace + O["flags"], [case["flags"]])
    s.words(brace + O["idx"], [case["index"]])
    s.ptr(brace + O["data"], data)
    for k, name in enumerate(("e8", "ec", "f0", "f4", "f8")):
        s.words(brace + O[name], [case["f"][k]])
    return brace, table, tc


def run(s, case):
    try:
        brace, table, tc = build(s, case)
        s.call(brace)
        O = s.O
        res = {"log": s.log(), "tc": [canon_f(w) for w in s.rd_words(tc, TC_BYTES // 4)],
               "table": s.rd_words(table, TABLE_BYTES // 4)}
        for name in ("e8", "ec", "f0", "f4", "f8"):
            res[name] = canon_f(s.rd_words(brace + O[name], 1)[0])
        res["flags"] = s.rd_words(brace + O["flags"], 1)[0]
        return res
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x3F000000, 0xBF000000]


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-3, 3))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def case_of(r, dflags=None, flags=None, f=None):
    table = []
    for i in range(TABLE_BYTES // 4):
        table.append(rf(r))
    # make ranges plausible most of the time
    for base in range(0, 0x50 // 4, 4):
        if r.random() < 0.7:
            lo = r.uniform(-2, 1)
            table[(0xd0 // 4) - 0 + 0] = table[0xd0 // 4]  # no-op
    return {"queues": {}, "index": r.randrange(0, 4), "dflags": r.choice([0, 0, 0, 1, 2, 3]) if dflags is None else dflags,
            "flags": (r.choice([0, 0x2000, 0x10000000, 0x10002000, 0x40, 0xFFFFFFFF, 0xFFFFDFFF]) if flags is None else flags),
            "table": table, "tc": [rf(r) for _ in range(TC_BYTES // 4)],
            "f": [rf(r) for _ in range(5)] if f is None else f}


def sane(r, c):
    """A table whose lows and highs are mostly ordered, values mostly in range."""
    t = c["table"]
    for k in range(5):
        b = k * 4
        if r.random() < 0.8:
            lo = r.uniform(-1, 1)
            t[b] = fb(lo)
            t[b + 1] = fb(lo + r.uniform(0, 2))
            t[b + 2] = fb(r.uniform(-2, 2))
            t[b + 3] = fb(r.uniform(-1, 1))
    return c


def edges():
    r = random.Random(214)
    one, zero, half = 0x3F800000, 0, 0x3F000000
    for flags in (0, 0x2000, 0x10000000, 0x10002000, 0x40):
        for dflags in (0, 1, 2, 3):
            yield sane(r, case_of(r, dflags, flags))
    for e8 in (one, fb(1.0000001), zero, 0x80000000, fb(-1.0), fb(2.0), 0x7FC00000, 0x7F800000, 0xFF800000):
        for f8 in (zero, fb(-0.25), fb(2.75), fb(-2.0), fb(0.5), 0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000):
            yield sane(r, case_of(r, 0, r.choice([0, 0x10000000]), [e8, fb(r.uniform(-2, 2)), rf(r), rf(r), f8]))
    for ec in (fb(0.5), fb(-0.5), fb(0.25), fb(-0.75), fb(1.5), zero):
        for fl in (0, 0x10000000):
            yield sane(r, case_of(r, 0, fl, [fb(0.5), ec, rf(r), rf(r), fb(0.3)]))


def rnd(r):
    return sane(r, case_of(r))


TESTS = [
    FnTest("SetupBraceAnimFrameData", ppc, host, edge=edges, rnd=rnd),
]
