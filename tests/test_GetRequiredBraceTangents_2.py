"""Unit v51024, GetRequiredBraceTangents, against libtpw_unit_GetRequiredBraceTangents_2.dylib (tests/unit_GetRequiredBraceTangents_2.cpp), main app
("SimTheme Park"), src/LevelSystems_GetRequiredBraceTangents_2.cpp:
  GetRequiredBraceTangents 0x1003aef4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's offsets (big-endian,
32-bit pointers) for the original, and in the host library's memory (little-endian, 64-bit pointers, the host struct offsets exported by the
library) for the translation. The callees outside the unit are fakes on both sides: NormaliseVectorXZ (normalises x and z in double,
records the three words it reads), __fpclassifyf (1 for a NaN, else 4; records the bits); sqrt and acos are the C library on both sides.

The world of a case: the brace (flags, tangents, control points, angle, position, previous and next braces, the pylon record with its flags
word and its limit at +0xb0), the type (flags, floats at +8 and +0xc), the params (first brace, whose previous brace flag 0x100000 takes).
What is compared: every word of the brace the function may write (flags, +0x0c to +0x4b, +0x58 to +0x63, the angle) and the event log.

Fused operations: fmadds, fmsubs and fmadd are hooked with HardwareRounding. Not tested: a NULL pylon data pointer on the brace (the original
reads it unconditionally), a NULL neighbour pylon data when it is reached.
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

SHIM_LIBRARY = "unit_GetRequiredBraceTangents_2"
MASK = 0xFFFFFFFF

FN = 0x1003AEF4
SIZES = {FN: 3048}
OWN = set(SIZES)

NORMALISE = 0x10045A38
FPCLASSIFY = 0x10239268

MAC = dict(flags=8, f0c=0x0c, x4c=0x4c, l50=0x50, z54=0x54, prev=0x64, next=0x68, data=0x100, f94=0x94, bsize=0x120,
           dsize=0x40, f18=0x18, head=0x18, psize=0x40, tsize=0x20, psz=4)


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


def stub_normalise(e):
    """NormaliseVectorXZ(s3DPoint&): x and z divided by sqrt(x*x + z*z) in double, rounded to float; unchanged when the length is 0."""
    L = e.cl
    a = e.r(3)
    wx, wy, wz = struct.unpack(">3I", e.read(a, 12))
    L.rec("NormaliseVectorXZ", (canon_f(wx), canon_f(wy), canon_f(wz)))
    x, z = bitsf(wx), bitsf(wz)
    ln = math.sqrt(float(x) * float(x) + float(z) * float(z))
    if ln != 0:
        wx = fb(T.f32(x / ln))
        wz = fb(T.f32(z / ln))
        e.write(a, struct.pack(">3I", wx, wy, wz))


def stub_fpclassify(e):
    L = e.cl
    x = e.f(1)
    L.rec("fpclassifyf", (canon_f(fb(x)),))
    e.set_r(3, 1 if x != x else 4)


STUBS.internal(NORMALISE, stub_normalise)
STUBS.internal(FPCLASSIFY, stub_fpclassify)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, p):
        self.write(a, struct.pack(self.pfmt, p))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.O = MAC
        e.cl = CL.PpcLog({}, {})
        self.L = e.cl
        self.saved = STUBS.activate(e)

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

    def pos_words(self, b, ws):
        self.words(b + MAC["x4c"], ws)

    def rd_pos(self, b):
        return self.rd_words(b + MAC["x4c"], 3)

    def call(self, brace, typ, params):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN, [brace, typ, params])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.O = lib.O
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_gbt_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def pos_words(self, b, ws):
        self.words(b + self.O["x4c"], [ws[0]])
        self.words(b + self.O["l50"], [ws[1]])
        self.words(b + self.O["z54"], [ws[2]])

    def rd_pos(self, b):
        return [self.rd_words(b + self.O[k], 1)[0] for k in ("x4c", "l50", "z54")]

    def call(self, brace, typ, params):
        self.lib.tpw_gbt_call(brace, typ, params)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gbt_alloc.restype, lib.tpw_gbt_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gbt_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_gbt_call.restype, lib.tpw_gbt_call.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gbt_layout(out)
    v = list(out)[:n]
    names = ["bsize", "flags", "f0c", "x4c", "l50", "z54", "prev", "next", "data", "f94", "dsize", "f18", "head", "psize", "tsize"]
    O = dict(zip(names, v))
    assert O["flags"] == 8 and O["f0c"] == 0x0c, O
    O["psz"] = 8
    lib.O = O
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_brace(s, spec):
    O = s.O
    b = s.alloc(O["bsize"])
    s.words(b + O["flags"], [spec["flags"]])
    s.words(b + O["f0c"], spec["junk"])           # 0x0c .. 0x63, 22 words
    s.pos_words(b, spec["pos"])
    d = s.alloc(O["dsize"])
    s.ptr(b + O["data"], d)
    if spec["tbl"] is not None:
        t = s.alloc(0xC0)
        s.words(t, [spec["tbl"][0]])
        s.words(t + 0xB0, [spec["tbl"][1]])
        s.ptr(d + O["f18"], t)
    s.words(b + O["f94"], [spec["angle"]])
    if spec["prev"] is not None:
        s.ptr(b + O["prev"], build_brace(s, spec["prev"]))
    if spec["next"] is not None:
        s.ptr(b + O["next"], build_brace(s, spec["next"]))
    return b


def build_and_run(s, case):
    main = build_brace(s, case["main"])
    head = build_brace(s, case["head"])
    typ = s.alloc(s.O["tsize"] if s.O["tsize"] >= 0x20 else 0x20)
    s.words(typ, case["type"])
    params = s.alloc(s.O["psize"])
    s.ptr(params + s.O["head"], head)
    s.call(main, typ, params)
    O = s.O
    junk = s.rd_words(main + O["f0c"], 22)
    keep = [canon_f(w) for i, w in enumerate(junk) if not (0x4C - 0x0C <= 4 * i < 0x58 - 0x0C)]
    return {"flags": s.rd_words(main + O["flags"], 1), "words": keep, "angle": [canon_f(w) for w in s.rd_words(main + O["f94"], 1)],
            "log": s.log()}


def run(s, case):
    try:
        return build_and_run(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the cases

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x3F800000, 0xBF800000]
LIMITS = [-1.5, -0.9995, -0.999, -0.5, 0.0, 0.5, 0.999, 0.9995, 1.5]
FLAGSETS = [0, 0x80000, 0x40000, 0x100000, 0x80, 0x40, 0xC0, 0x80000 | 0x40000, 0x100000 | 0x80, 0x1000000, 0x0E000000, 0x10000000]


def rj(r):
    return fb(r.uniform(-9, 9)) if r.random() < 0.95 else r.choice(SPECIAL)


def vec(r, scale=20.0):
    return [r.uniform(-scale, scale) for _ in range(3)]


def to_bits(v):
    return [fb(x) for x in v]


def tbl(r, f=None, lim=None):
    return (r.choice([0, 0x4000, 0x4000, 0x4001, 0x8000]) if f is None else f, fb(r.choice(LIMITS) if lim is None else lim))


def spec(r, pos, flags=None, table="rand"):
    return {"pos": to_bits(pos), "flags": r.choice(FLAGSETS) | r.choice([0, 0x200000, 0x2000]) if flags is None else flags,
            "junk": [rj(r) for _ in range(22)], "tbl": tbl(r) if table == "rand" else table, "angle": rj(r), "prev": None, "next": None}


def geometry(r, mode, pos):
    a = vec(r, 10)
    nxt = [pos[i] + a[i] for i in range(3)]
    k = r.uniform(0.3, 3)
    noise = r.choice([0.0, 1e-4, 1e-2, 0.3])
    n = vec(r, noise) if noise else [0, 0, 0]
    if mode == "rand":
        prv = [pos[i] + vec(r, 10)[i] for i in range(3)]
    elif mode == "straight":
        prv = [pos[i] - k * a[i] + n[i] for i in range(3)]
    elif mode == "hairpin":
        prv = [pos[i] + k * a[i] + n[i] for i in range(3)]
    elif mode == "flat":
        a[1] = 0.0
        nxt = [pos[i] + a[i] for i in range(3)]
        prv = [pos[0] + vec(r, 10)[0], pos[1], pos[2] + vec(r, 10)[2]]
    else:                                                       # degenerate: a leg of length 0
        prv = list(pos) if r.random() < 0.5 else [pos[i] - a[i] for i in range(3)]
        if r.random() < 0.5:
            nxt = list(pos)
    return nxt, prv


def make_case(r, flags=None, ttbl="rand", mode=None, nflag=None):
    pos = vec(r, 30)
    mode = mode or r.choice(["rand", "rand", "straight", "hairpin", "flat", "degenerate"])
    nxt, prv = geometry(r, mode, pos)
    main = spec(r, pos, flags, ttbl)
    if r.random() < 0.9:
        main["next"] = spec(r, nxt, table=None if r.random() < 0.2 else (tbl(r, nflag) if nflag is not None else "rand"))
        if r.random() < 0.8:
            nn = [nxt[i] + r.uniform(-10, 10) for i in range(3)]
            main["next"]["next"] = spec(r, nn, table=None)
    if r.random() < 0.9:
        main["prev"] = spec(r, prv, table=None if r.random() < 0.2 else (tbl(r, nflag) if nflag is not None else "rand"))
    head = spec(r, vec(r, 30), table=None)
    if r.random() < 0.7:
        head["prev"] = spec(r, prv, table=None)
    ty = [r.choice([0, 8, 8, 0x18, 1]), fb(r.uniform(-2, 2)), fb(r.uniform(0.1, 4)), fb(r.choice([r.uniform(0.5, 5), 0.0, -1.0, 1.0]))]
    return {"main": main, "head": head, "type": ty}


def edges():
    r = random.Random(5107)
    for fl in FLAGSETS[:9]:
        for tb in (None, (0, fb(0.5)), (0x4000, fb(0.5)), (0x4000, fb(-1.5)), (0x4000, fb(1.5))):
            for mode in ("rand", "straight", "hairpin", "flat"):
                for nf in (0, 0x4000):
                    c = make_case(r, fl, tb, mode, nf)
                    yield c


def rnd(r):
    return make_case(r)


TESTS = [
    FnTest("GetRequiredBraceTangents", ppc, host, edge=edges, rnd=rnd),
]
