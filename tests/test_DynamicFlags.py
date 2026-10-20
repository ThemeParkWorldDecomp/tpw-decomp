"""Unit w128, the flags of the park, against libtpw_unit_DynamicFlags.dylib (tests/unit_DynamicFlags.cpp), main app ("SimTheme Park"),
src/DynamicFlags.cpp:
  DC_AnimateFlag 0x100E1350, DC_DrawFlag 0x100E087C.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The only callee
outside the unit, DC_AddPolyFaceToList (src/DynamicFaces.cpp, unit w127), is a fake on both sides that records the texture and the type,
the twelve words of the positions, the u and v of the four texture coordinates (DC_DrawFlag never writes their z words, and the real function
does not read them) and the twelve words of the points, NaNs made one value. sqrt is the host's double square root on both sides.

The world of a case: the engine's sine table (the mask, the scale and the 4,096 floats, written with the same words on both sides), both
grids (every word) and the clamp float at 0x102A03A0. What is compared: the events and both grids after the call (DC_DrawFlag leaves them
alone, which the comparison shows), the clamp, and the sine table's header words.

The fused multiply-adds and the fctiwz of both functions get the processor's rounding through test_DynamicMesh's HardwareRounding (Unicorn
rounds fused operations twice and saturates fctiwz of large values to the wrong sign). Not tested: a table mask above 0xFFF (reads past the
table), a column or row count that runs past the 16 x 19 grids.
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

SHIM_LIBRARY = "unit_DynamicFlags"
MASK = 0xFFFFFFFF
GRID_WORDS = 16 * 19 * 3
TABLE_WORDS = 0x1000

ANIMATE, DRAW = 0x100E1350, 0x100E087C
OWN = {ANIMATE, DRAW}
ADD_POLY = 0x100E03F0

# The Mac's addresses of the globals; the host library's addresses come from tpw_dfl_addr(name).
ADDR = dict(tpg=0x103B74C8, gridA=0x102F7EC8, gridB=0x102F8D08, clamp=0x102A03A0)
HOST_LAYOUT = [4, 8, 0xC, TABLE_WORDS, 16 * 19 * 12, 16 * 19 * 12]


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    """A float's bits with every NaN made one (the processors make different NaNs)."""
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- rounding


class FlagRounding(HardwareRounding):
    """HardwareRounding aimed at the two functions (664 and 2,124 bytes)."""

    def __enter__(self):
        for begin, size in ((ANIMATE, 664), (DRAW, 2124)):
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fake

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def be_words(e, address, count):
    return [canon_f(w) for w in struct.unpack(">%dI" % count, e.read(address, 4 * count))]


def padded(words):
    return tuple(words) + (0,) * (12 - len(words))


def stub_add_poly(e):
    """DC_AddPolyFaceToList(positions, texture, uvs, points, type): the arguments the real function reads, as the unit's fake records them."""
    L = e.cl
    positions, texture, uvs, points, typ = e.r(3), s32(e.r(4)), e.r(5), e.r(6), s32(e.r(7))
    L.rec("DC_AddPolyFaceToList", (texture, typ))
    L.rec("positions", padded(be_words(e, positions, 12)))
    uv = be_words(e, uvs, 12)
    L.rec("uvs", padded([uv[3 * k + j] for k in range(4) for j in range(2)]))
    L.rec("points", padded(be_words(e, points, 12)))


STUBS.internal(ADD_POLY, stub_add_poly)


def setup(runner):
    T.install_std_stubs(runner.emu)       # the C library's sqrt (host double square root)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    """One case on the PowerPC side: the fake active, a fresh log, memory in the emulator's image."""
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, {})
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def animate(self, columns, rows, step, phase):
        with FlagRounding(self.e):
            T.call_ppc(self.e, ANIMATE, [columns & MASK, rows & MASK, T.F(bitsf(step)), T.F(bitsf(phase))])

    def draw(self, x, y, z, w, columns, rows, texture, angle):
        with FlagRounding(self.e):
            T.call_ppc(self.e, DRAW, [T.F(bitsf(x)), T.F(bitsf(y)), T.F(bitsf(z)), T.F(bitsf(w)), columns & MASK, rows & MASK,
                                      texture & MASK, T.F(bitsf(angle))])


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_dfl_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def animate(self, columns, rows, step, phase):
        self.lib.tpw_dfl_animate(s32(columns), s32(rows), bitsf(step), bitsf(phase))

    def draw(self, x, y, z, w, columns, rows, texture, angle):
        self.lib.tpw_dfl_draw(bitsf(x), bitsf(y), bitsf(z), bitsf(w), s32(columns), s32(rows), s32(texture), bitsf(angle))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_dfl_addr.restype, lib.tpw_dfl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_dfl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_dfl_animate.argtypes = [C.c_int32, C.c_int32, C.c_float, C.c_float]
    lib.tpw_dfl_draw.argtypes = [C.c_float] * 4 + [C.c_int32] * 3 + [C.c_float]
    out = (C.c_int64 * 16)()
    n = lib.tpw_dfl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    g = s.g
    s.words(g("tpg") + 4, [case["mask"], case["scale"]])
    s.words(g("tpg") + 0xC, case["table"])
    s.words(g("gridA"), case["gridA"])
    s.words(g("gridB"), case["gridB"])
    s.words(g("clamp"), [case["clamp"]])


def execute(s, case):
    if case["op"] == "animate":
        s.animate(*case["args"])
    else:
        s.draw(*case["args"])


def observe(s, case):
    g = s.g
    return {"log": s.log(), "gridA": [canon_f(w) for w in s.rd_words(g("gridA"), GRID_WORDS)],
            "gridB": [canon_f(w) for w in s.rd_words(g("gridB"), GRID_WORDS)], "clamp": s.rd_word(g("clamp")),
            "header": s.rd_words(g("tpg") + 4, 2)}


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0xFFC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x007FFFFF, 0x80000001, 0x3F800000, 0xBF800000,
           fb(0.5), fb(1.8), fb(20.0), fb(-20.0), fb(1.5707964), fb(-1.5707964), fb(1024.0), fb(-1024.0), 0x7F7FFFFF, 0xFF7FFFFF, 0x00800000]
SIN_SCALE = fb(4096 / (2 * math.pi))
_SIN = [fb(math.sin(2 * math.pi * i / 4096)) for i in range(TABLE_WORDS)]


def rf(r, wide=True):
    """A float's bits: mostly ordinary numbers of every scale, now and then a special value."""
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.5:
        return fb(r.uniform(-8, 8))
    if k < 0.8:
        return fb(r.uniform(-300, 300))
    if k < 0.9:
        return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))
    return r.getrandbits(32) if wide and r.random() < 0.5 else fb(r.uniform(-4000, 4000))


def table_words(r, kind=None):
    kind = r.choice(["sin", "sin", "random", "rough"]) if kind is None else kind
    if kind == "sin":
        return list(_SIN)
    if kind == "random":
        return [fb(r.uniform(-1, 1)) for _ in range(TABLE_WORDS)]
    words = [fb(r.uniform(-1, 1)) for _ in range(TABLE_WORDS)]
    for _ in range(40):
        words[r.randrange(TABLE_WORDS)] = r.choice([0, 0x80000000, 0x3F800000, 0xBF800000, 0x00000001, 0x80000003, fb(2.5), fb(-300.0),
                                                    0x7F800000, 0x7FC00000])
    return words


def grid_words(r, mode="random"):
    """mode: random (every kind of word), plain (finite numbers of a flag's size), zero, or special (now and then a special value)."""
    if mode == "zero":
        return [0] * GRID_WORDS
    if mode == "plain":
        return [fb(r.uniform(-12, 12)) for _ in range(GRID_WORDS)]
    if mode == "special":
        return [r.choice(SPECIAL) if r.random() < 0.05 else fb(r.uniform(-12, 12)) for _ in range(GRID_WORDS)]
    return [rf(r) for _ in range(GRID_WORDS)]


def base(r, op, mask=None, scale=None, clamp=None, grid="random"):
    return {"op": op,
            "mask": r.choice([0xFFF, 0xFFF, 0xFFF, 0x7FF, 0x3FF, 0xFF, 1, 0]) if mask is None else mask,
            "scale": r.choice([SIN_SCALE, SIN_SCALE, SIN_SCALE, fb(r.uniform(-1000, 1000)), fb(r.uniform(0, 100)), 0, 0x80000000,
                               fb(1e6), fb(-651.9)]) if scale is None else scale,
            "table": table_words(r),
            "clamp": r.choice([fb(1.0), fb(1.0), fb(1.0), fb(0.5), fb(0.0), fb(-1.0), fb(2.0), 0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000,
                               rf(r)]) if clamp is None else clamp,
            "gridA": grid_words(r, grid), "gridB": grid_words(r, grid)}


# ---- DC_AnimateFlag
def animate_case(r, columns=None, rows=None, step=None, phase=None, scale=None, mask=None, clamp=None, grid=None):
    c = base(r, "animate", mask=mask, scale=scale, clamp=clamp, grid=r.choice(["random", "plain", "zero"]) if grid is None else grid)
    c["args"] = [r.choice([7, 7, 16, 1, 2, 4, 5, 0, -1, r.randrange(1, 17)]) & MASK if columns is None else columns,
                 r.choice([5, 5, 19, 1, 2, 7, 0, -1, r.randrange(1, 20)]) & MASK if rows is None else rows,
                 r.choice([fb(1.8), fb(1.8), 0, 0x80000000, fb(r.uniform(-30, 30)), fb(r.uniform(0.1, 5)), rf(r)]) if step is None else step,
                 r.choice([0, fb(r.uniform(0, 400)), fb(r.uniform(-30, 30)), rf(r), fb(5.5)]) if phase is None else phase]
    return c


def animate_edges():
    r = random.Random(1281)
    for phase in (0, fb(1.0), fb(10.0), fb(-3.0), fb(123.456), 0x80000000, fb(1e6), fb(1e12), 0x7F800000, 0x7FC00000, fb(0.4), fb(7.77)):
        yield animate_case(r, 7, 5, fb(1.8), phase, SIN_SCALE, 0xFFF, fb(1.0))
    for step in (0, fb(1.0), fb(-1.0), fb(1.8), fb(1e30), fb(1e-30), 0x7F800000, 0x7FC00000, 0x80000000, fb(0.3333333), fb(20.0), fb(-1.8)):
        yield animate_case(r, 7, 5, step, fb(r.uniform(0, 100)), SIN_SCALE, 0xFFF, fb(1.0))
    for columns, rows in ((1, 1), (0, 5), (5, 0), (7, 1), (1, 5), (-3, 4), (4, -3), (2, 2), (6, 4), (16, 19), (4, 5), (3, 5), (5, 5), (8, 5),
                          (4, 1), (4, 0x7FFFFFFF)):
        if rows == 0x7FFFFFFF:
            continue
        yield animate_case(r, columns & MASK, rows & MASK, fb(1.8), fb(r.uniform(0, 100)), SIN_SCALE, 0xFFF, fb(1.0))
    for columns in (0x80000000, 0x80000004, 0x7FFFFFFF, 0xFFFFFFFC):     # the width's (columns - 4) wraps
        yield animate_case(r, columns, 0, fb(1.8), fb(2.0), SIN_SCALE, 0xFFF, fb(1.0))
    for scale in (0, 0x80000000, fb(1e6), fb(1e-3), fb(-651.9), fb(1e9), fb(-1e9), 0x7FC00000):
        yield animate_case(r, 7, 5, fb(1.8), fb(7.0), scale, 0xFFF, fb(1.0))
    for mask in (0, 1, 0xFF, 0x3FF):
        yield animate_case(r, 7, 5, fb(1.8), fb(7.0), SIN_SCALE, mask, fb(1.0))
    for clamp in (fb(1.0), fb(0.5), fb(0.0), fb(-1.0), fb(0.1), fb(5.0), 0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000, fb(0.3333333),
                  fb(0.6666666), 0x3F800001, 0x3F7FFFFF):
        yield animate_case(r, 7, 5, fb(1.8), fb(3.0), SIN_SCALE, 0xFFF, clamp)
    # The fraction against the clamp exactly: x / ((columns - 4) * step) with x = 0, step, 2 step ... equal to the clamp.
    for columns in (5, 6, 7, 8, 9, 12):
        for clamp in (fb(1.0), fb(0.5), fb(0.25)):
            yield animate_case(r, columns, 3, fb(1.0), fb(1.5), SIN_SCALE, 0xFFF, clamp)
    # A plain table of ones and a plain grid: the products stay finite and visible.
    c = animate_case(r, 7, 5, fb(1.8), fb(2.5), SIN_SCALE, 0xFFF, fb(1.0), "zero")
    c["table"] = [0x3F800000] * TABLE_WORDS
    yield c
    c = animate_case(r, 7, 5, fb(1.8), fb(2.5), SIN_SCALE, 0xFFF, fb(1.0), "zero")
    c["table"] = [fb(-2.5)] * TABLE_WORDS
    yield c


def animate_random(r):
    return animate_case(r)


# ---- DC_DrawFlag
def angle_bits(r):
    k = r.random()
    if k < 0.15:
        return r.choice([0, 0x80000000, fb(1.5707964), fb(-1.5707964), fb(3.1415927), fb(6.2831855), 0x00000001, fb(1e-30), fb(0.001), fb(-0.001),
                         fb(4e6), fb(-4e6), fb(3.3e6), fb(1e10), fb(-1e10), 0x7F800000, 0xFF800000, 0x7FC00000, 0x7F7FFFFF, 0xFF7FFFFF])
    if k < 0.6:
        return fb(r.uniform(-7, 7))
    if k < 0.85:
        return fb(r.uniform(-100, 100))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-3, 7))


def draw_case(r, args=None, scale=None, mask=None, grid=None):
    c = base(r, "draw", mask=mask, scale=scale, grid=r.choice(["random", "plain", "special", "plain"]) if grid is None else grid)
    if args is None:
        columns, rows = r.choice([(6, 4), (6, 4), (1, 1), (0, 3), (3, 0), (15, 18), (2, 3), (r.randrange(0, 16), r.randrange(0, 19))])
        args = [rf(r), rf(r), rf(r), r.choice([fb(0.5), fb(0.5), rf(r), fb(r.uniform(-5, 5))]), columns, rows,
                r.choice([0x12, 0x13, 0x12, 0x13, 0, 1, 0x11, 0x14, 0x1F, 0x20, -1, 0x7FFFFFFF, r.getrandbits(32)]) & MASK, angle_bits(r)]
    c["args"] = args
    return c


def draw_edges():
    r = random.Random(1282)
    # The tick's call (spot, 0.5f, 6, 4, texture, wind) over a plain grid and a sine table, several angles.
    for texture in (0x12, 0x13):
        for angle in (0, 0x80000000, fb(1.5707964), fb(-1.5707964), fb(3.1415927), fb(-3.1415927), fb(6.2831855), fb(0.7), fb(-0.7), fb(1.5707963),
                      fb(2.0), fb(-5.0), fb(1e-30), fb(4e6), fb(-4e6), fb(1e10), fb(-1e10), 0x7F800000, 0xFF800000, 0x7FC00000, fb(3.3e6),
                      fb(3.29e6), 0x00000001):
            yield draw_case(r, [fb(r.uniform(-50, 50)), fb(r.uniform(0, 20)), fb(r.uniform(-50, 50)), fb(0.5), 6, 4, texture, angle],
                            SIN_SCALE, 0xFFF, "plain")
    for texture in (0x12, 0x13, 0, 1, 0x11, 0x14, 0x20, 0x1F, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000):
        yield draw_case(r, [fb(1.0), fb(2.0), fb(3.0), fb(0.5), 6, 4, texture, fb(0.3)], SIN_SCALE, 0xFFF, "plain")
    for columns, rows in ((15, 18), (1, 1), (0, 5), (5, 0), (-2, 3), (3, -2), (2, 3), (15, 1), (1, 18), (6, 4), (4, 6), (0, 0), (7, 4), (6, 5),
                          (15, 17), (14, 18)):
        yield draw_case(r, [rf(r), rf(r), rf(r), fb(0.5), columns & MASK, rows & MASK, 0x12, fb(r.uniform(-4, 4))], SIN_SCALE, 0xFFF, "plain")
    for w in (0, 0x80000000, fb(0.5), fb(-0.5), fb(1e30), 0x7F800000, 0x7FC00000, fb(1e-30)):
        yield draw_case(r, [fb(1.0), fb(2.0), fb(3.0), w, 3, 3, 0x13, fb(0.4)], SIN_SCALE, 0xFFF, "plain")
    for x in (0, 0x80000000, fb(1.0), fb(-1e30), 0x7F800000, 0x7FC00000):
        yield draw_case(r, [x, x, x, fb(0.5), 3, 3, 0x12, fb(0.4)], SIN_SCALE, 0xFFF, "plain")
    for scale in (0, 0x80000000, fb(1e6), fb(1e-3), fb(-651.9), fb(1e9), fb(-1e9), 0x7FC00000):
        yield draw_case(r, [fb(1.0), fb(2.0), fb(3.0), fb(0.5), 2, 2, 0x12, fb(0.9)], scale, 0xFFF, "plain")
    for mask in (0, 1, 0xFF, 0x3FF):
        yield draw_case(r, [fb(1.0), fb(2.0), fb(3.0), fb(0.5), 2, 2, 0x12, fb(0.9)], SIN_SCALE, mask, "plain")
    yield draw_case(r, [fb(1.0), fb(2.0), fb(3.0), fb(0.5), 6, 4, 0x12, fb(0.9)], SIN_SCALE, 0xFFF, "zero")
    for k in range(6):
        yield draw_case(r, [rf(r), rf(r), rf(r), fb(0.5), 6, 4, 0x13, rf(r)], SIN_SCALE, 0xFFF, "special")


def draw_random(r):
    return draw_case(r)


TESTS = [
    FnTest("DC_AnimateFlag", ppc, host, edge=animate_edges, rnd=animate_random),
    FnTest("DC_DrawFlag", ppc, host, edge=draw_edges, rnd=draw_random),
]
