"""Unit v5011, GetTangentsFromZVector, against libtpw_unit_GetTangentsFromZVector.dylib (tests/unit_GetTangentsFromZVector.cpp), main app
("SimTheme Park"), src/game/rides/CoasterPylon.cpp:
  GetTangentsFromZVector 0x1003be20

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are the MSL imports sqrt, __fpclassifyf and fmod: fakes on both sides that record their arguments (a double as two words); sqrt and
fmod compute, __fpclassifyf answers from the case's queue.

The world of a case: the coaster type's float at +8, the point, the control (its floats 0x1c..0x58, its brace pointer), the control's brace and
its neighbours (flags, the floats at +0x4c, +0x54, +0xec, +0xf8, the two neighbour pointers, whose offsets differ on the host), the pylon (NULL or
a flags word), gTPGlobals' mask, scale and sine table. What is compared: the control's floats 0x1c..0x58, the point, the event log.

The fused multiply-adds (fmadds, fmadd) and the fctiwz conversions are under ROUNDING. Not tested: a brace angle or sine scale whose product
passes 2^31 (Unicorn and the hardware saturate fctiwz differently), NaN angles for the same reason, a NULL coaster, point, control or brace
(read unconditionally), a sine mask over 0xfff (past the table).
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_GetTangentsFromZVector"
MASK = 0xFFFFFFFF

FN_A = 0x1003be20
SIZES = {FN_A: 1784}
OWN = set(SIZES)

ADDR = dict(gTPGlobals=0x103b74c8)
# host layout: ctrl.mBrace, brace.mFlags, brace.mF0C, brace.mPrev, brace.mNext, brace.mFE8, globals mask, scale, table, coaster f08, pylon flags,
# sizeof control, sizeof brace
HL = []
FIXED = {0: 0x10, 1: 8, 2: 0x0c, 6: 4, 7: 8, 8: 0x0c, 10: 0}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


def canon_d(bits):
    return 0x7FF8000000000000 if (bits & 0x7FF0000000000000) == 0x7FF0000000000000 and (bits & 0xFFFFFFFFFFFFF) else bits


TABLE = [fb(math.sin(2 * math.pi * i / 4096)) for i in range(0x1000)]


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def dsplit(x):
    b = canon_d(T.f64_bits(x))
    return b >> 32, b & MASK


def imp_sqrt(e):
    x = e.f(1)
    e.cl.rec("sqrt", dsplit(x))
    e.set_f1(math.sqrt(x) if x >= 0 or x == 0 else float("nan"))


def imp_fpclassify(e):
    L = e.cl
    L.rec("fpclassify", (T.f32_bits(e.f(1)),))
    e.set_r(3, L.ret("fpclassify") & MASK)


def imp_fmod(e):
    x, y = e.f(1), e.f(2)
    e.cl.rec("fmod", dsplit(x) + dsplit(y))
    try:
        r = math.fmod(x, y)
    except ValueError:
        r = float("nan")
    e.set_f1(r)


STUBS.imp("sqrt", imp_sqrt)
STUBS.imp("__fpclassifyf", imp_fpclassify)
STUBS.imp("fmod", imp_fmod)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def ptr(self, a, value):
        """A pointer: one word on the Mac, eight bytes on the host (the arena is not 32-bit here)."""
        if self.fmt == ">I":
            self.words(a, [value])
        else:
            self.write(a, struct.pack("<Q", value))

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    prev, next, fe8, f08 = 0x64, 0x68, 0xE8, 8

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

    def call(self, coaster, pt, ctrl, a, b, pylon, flag):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN_A, [coaster, pt, ctrl, T.F(a), T.F(b), pylon, 0, 0, flag & MASK, 0])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.prev, self.next, self.fe8, self.f08 = HL[3], HL[4], HL[5], HL[9]
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gtz_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_gtz_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, coaster, pt, ctrl, a, b, pylon, flag):
        self.lib.tpw_gtz_call(coaster, pt, ctrl, a, b, pylon, 0, 0, s32(flag), 0)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gtz_addr.restype, lib.tpw_gtz_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gtz_alloc.restype, lib.tpw_gtz_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gtz_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_gtz_call.restype = None
    lib.tpw_gtz_call.argtypes = [C.c_uint64] * 3 + [C.c_float, C.c_float] + [C.c_uint64] * 3 + [C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gtz_layout(out)
    HL[:] = list(out)[:n]
    for i, v in FIXED.items():
        assert HL[i] == v, (i, HL)
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def put_brace(s, spec, size=0x140):
    """A brace of the Mac layout (the host's pointer offsets differ); neighbours are built first."""
    a = s.alloc(size)
    prv = put_brace(s, spec["prev"]) if spec.get("prev") else 0
    nxt = put_brace(s, spec["next"]) if spec.get("next") else 0
    s.words(a + 8, [spec["flags"]])
    s.words(a + 0x4C, [spec["f4c"]])
    s.words(a + 0x54, [spec["f54"]])
    s.ptr(a + s.prev, prv)
    s.ptr(a + s.next, nxt)
    s.words(a + s.fe8 + 4, [spec["fec"]])
    s.words(a + s.fe8 + 0x10, [spec["ff8"]])
    return a


def build(s, case):
    g = s.g("gTPGlobals")
    s.words(g + 4, [case["mask"]])
    s.words(g + 8, [case["scale"]])
    s.words(g + 0xC, TABLE)
    coaster = s.alloc(0x40)
    s.words(coaster + s.f08, [case["f08"]])   # the header's mF08 sits at 4 on the host (the Mac's +8)
    pt = s.alloc(16)
    s.words(pt, case["pt"])
    ctrl = s.alloc(0x60)
    s.words(ctrl + 0x1C, case["init"])
    s.words(ctrl + 0x40, case["c40"])
    brace = put_brace(s, case["brace"])
    s.ptr(ctrl + 0x10, brace)
    pylon = 0
    if case["pylon"] is not None:
        pylon = s.alloc(0x130)
        s.words(pylon, [case["pylon"]])
    return coaster, pt, ctrl, pylon


def run(s, case):
    try:
        coaster, pt, ctrl, pylon = build(s, case)
        a, b = case["args"]
        s.call(coaster, pt, ctrl, bitsf(a), bitsf(b), pylon, case["flag"])
        return {"ctrl": [canon_f(w) for w in s.rd_words(ctrl + 0x1C, 15)], "pt": s.rd_words(pt, 3), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.06:
        return r.choice(SPECIAL)
    if k < 0.8:
        return fb(r.uniform(-8, 8))
    return fb(r.uniform(-300, 300))


def rpos(r):
    return fb(r.uniform(-30, 30)) if r.random() < 0.93 else rf(r)


def neighbour(r):
    return {"flags": r.choice([0, 0x40, 0x80, 0xC0, r.randrange(0, 1 << 16)]), "f4c": rpos(r), "f54": rpos(r), "fec": 0, "ff8": 0}


def base(r, pylon=None, bflags=None, prev=None, nxt=None):
    k = r.random()
    mask = r.choice([0x3F, 0xFF, 0xFFF, 0xFFF])
    brace = {
        "flags": r.choice([0, 0x80, 0x40, 0xC0, r.randrange(0, 1 << 16)]) if bflags is None else bflags,
        "f4c": rpos(r), "f54": rpos(r),
        "fec": fb(r.uniform(-20, 20)), "ff8": fb(r.uniform(-5, 5)),
        "prev": neighbour(r) if (r.random() < 0.7 if prev is None else prev) else None,
        "next": neighbour(r) if (r.random() < 0.7 if nxt is None else nxt) else None,
    }
    return {
        "queues": {"fpclassify": [1 if r.random() < 0.15 else 0]},
        "mask": mask, "scale": fb(r.choice([651.8986, 10.0, 0.5, r.uniform(0.1, 1000)])),
        "f08": fb(r.uniform(0.1, 5)) if k < 0.9 else rf(r),
        "pt": [rf(r) if r.random() < 0.2 else fb(r.uniform(-10, 10)) for _ in range(3)] + [0],
        "init": [r.choice([0, rf(r)]) for _ in range(15)],
        "c40": [fb(r.uniform(-50, 50)) for _ in range(3)],
        "brace": brace,
        "pylon": (r.choice([0x4000, 0x4004, 0xC000]) if r.random() < 0.8 else r.choice([0, 4, 0x3FFF])) if pylon is None else pylon,
        "args": [fb(r.uniform(0.1, 20)) if r.random() < 0.9 else rf(r), fb(r.uniform(0.1, 20)) if r.random() < 0.9 else rf(r)],
        "flag": r.choice([0, 1, 5]),
    }


def edges():
    r = random.Random(5011)
    yield dict(base(r, pylon=None))
    for pyl in (0, 4, 0x3FFF):
        yield base(r, pylon=pyl)
    for bf in (0, 0x80, 0x40, 0xC0):
        for prev in (False, True):
            for nxt in (False, True):
                for pf in (0x4000, 0x4004):
                    c = base(r, pylon=pf, bflags=bf, prev=prev, nxt=nxt)
                    yield c
                    c = base(r, pylon=pf, bflags=bf, prev=prev, nxt=nxt)
                    c["queues"] = {"fpclassify": [1]}
                    yield c
    for nf in (0, 0x40):
        c = base(r, pylon=0x4000, bflags=0, prev=True, nxt=True)
        c["brace"]["prev"]["flags"] = nf
        c["brace"]["next"]["flags"] = nf
        yield c
        c = base(r, pylon=0x4000, bflags=0x80, prev=True, nxt=True)
        c["brace"]["prev"]["flags"] = nf
        c["brace"]["next"]["flags"] = nf
        yield c
    for z in ([0, 0, 0, 0], [fb(1), 0, 0, 0], [0, 0, fb(1), 0], [0x80000000, 0x80000000, 0x80000000, 0]):
        c = base(r)
        c["pt"] = z
        yield c
    # the clamps: a banking that gives fabs > 1 and fmod negative
    for ff8 in (fb(0.0), fb(0.3), fb(0.75), fb(-0.3), fb(2.6), fb(0.5)):
        c = base(r, pylon=0x4000)
        c["brace"]["ff8"] = ff8
        yield c


def rnd(r):
    return base(r)


TESTS = [
    FnTest("GetTangentsFromZVector", ppc, host, edge=edges, rnd=rnd),
]
