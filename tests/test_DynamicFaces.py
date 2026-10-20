"""Unit w127, the dynamic face list and the water, against libtpw_unit_DynamicFaces.dylib (tests/unit_DynamicFaces.cpp), main app
("SimTheme Park"), src/DynamicFaces.cpp:
  sort 0x100DFB68, DC_ResetSquareFaceListPtr 0x100E0260, DC_AddSquareFaceToList 0x100E02A0, DC_AddPolyFaceToList 0x100E03F0,
  HDZ_RotatePointZ 0x100E0544, DC_AnimateWater 0x100E10F4, DC_DrawWater 0x100E060C.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The seven functions
call each other for real (DrawWater runs the poly adder, which is why DrawWater is checked by the face list it leaves). The only callee
outside the unit, CTRACE (the "Too many ... dynamic faces !" lines), is a fake that records its level and text on both sides.

The world of a case: the list's count, the flag byte and the other word the adder reads, gDynamicCellRandom, the engine's sine table (the
mask, the scale and the 4,096 floats, written with the same words on both sides), both grids (every word), a window of the face list and its
keys (filled with one byte so that bytes the function leaves alone are compared too) and, for sort and the rotation, a block of words with a
canary. What is compared: the events, the count, the keys and records of the window, the two grids, the block, the flag word and the second
global. Floats the functions compute are compared with every NaN made one value; bytes the adders copy are compared as they are.

The fused multiply-adds and the fctiwz of the rotation and the water get the processor's rounding through test_DynamicMesh's
HardwareRounding (Unicorn rounds fused operations twice and saturates fctiwz of large values to the wrong sign). Not tested: a count below
zero in the adders (the Mac writes before the list), grid cells past the grids in DrawWater, a table mask above 0xFFF (reads past the
table), the size 0 for sort (the Mac's unsigned count wraps).
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

SHIM_LIBRARY = "unit_DynamicFaces"
MASK = 0xFFFFFFFF
FACE_MAX = 0x806
RECORD = 0xA0
RW = RECORD // 4            # words in a record
GRID_WORDS = 16 * 19 * 3
TABLE_WORDS = 0x1000

SORT, RESET, ADD_SQUARE, ADD_POLY, ROTATE, DRAW_WATER, ANIM_WATER = (0x100DFB68, 0x100E0260, 0x100E02A0, 0x100E03F0, 0x100E0544,
                                                                      0x100E060C, 0x100E10F4)
OWN = {SORT, RESET, ADD_SQUARE, ADD_POLY, ROTATE, DRAW_WATER, ANIM_WATER}
CTRACE = 0x1006FCBC

# The Mac's addresses of the globals; the host library's addresses come from tpw_df_addr(name).
ADDR = dict(faces=0x102FBDC0, keys=0x102F9DA4, count=0x102FBDBC, f9bd4=0x102F9BD4, flag=0x102A031C, random=0x102F9BDC, tpg=0x103B74C8,
            gridA=0x102F7EC8, gridB=0x102F8D08)
HOST_LAYOUT = [4, 8, 0xC, TABLE_WORDS, 16 * 19 * 12, RECORD, FACE_MAX * RECORD]


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


class WaterRounding(HardwareRounding):
    """HardwareRounding aimed at the rotation (148 bytes) and the water's animation (560 bytes)."""

    def __enter__(self):
        for begin, size in ((ROTATE, 148), (ANIM_WATER, 560)):
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fake

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ctrace(e):
    e.cl.rec("CTRACE", (e.r(3),), (e.cstr(e.r(4))[:159],))


STUBS.internal(CTRACE, stub_ctrace)


def setup(runner):
    T.install_std_stubs(runner.emu)       # the C library's sqrt (host double square root) for the normals of the water


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
    """One case on the PowerPC side: the fake active, a fresh log, memory in the emulator's scratch area and image."""
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, {})
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, size, fill=0xA5):
        return self.e.alloc(size, bytes([fill & 0xFF]) * size)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, address, *args, rounding=False):
        if rounding:
            with WaterRounding(self.e):
                return T.call_ppc(self.e, address, list(args))[0]
        return T.call_ppc(self.e, address, list(args))[0]

    def sort(self, block, n):
        self.call(SORT, block, n)

    def reset_list(self):
        self.call(RESET)

    def add_square(self, x, y, z, side, texture, rotation, unused1, large, unused2, keep, typ):
        self.call(ADD_SQUARE, T.F(bitsf(x)), T.F(bitsf(y)), T.F(bitsf(z)), side, texture & MASK, rotation & MASK, T.F(bitsf(unused1)),
                  large & MASK, unused2, keep & MASK, typ & MASK)

    def add_poly(self, positions, texture, uvs, points, typ):
        self.call(ADD_POLY, positions, texture & MASK, uvs, points, typ & MASK)

    def rotate(self, block, angle):
        self.call(ROTATE, block, T.F(bitsf(angle)), rounding=True)

    def animate(self, columns, rows, step, phase):
        self.call(ANIM_WATER, columns & MASK, rows & MASK, T.F(bitsf(step)), T.F(bitsf(phase)), rounding=True)

    def draw(self, x, y, z, a, b, c, d, e, f):
        self.call(DRAW_WATER, T.F(bitsf(x)), T.F(bitsf(y)), T.F(bitsf(z)), a & MASK, b & MASK, c & MASK, d & MASK, e & MASK, f & MASK)


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_df_reset()
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_df_addr(name.encode())

    def alloc(self, size, fill=0xA5):
        return self.lib.tpw_df_alloc(size, fill & 0xFF)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def sort(self, block, n):
        self.lib.tpw_df_sort(block, n)

    def reset_list(self):
        self.lib.tpw_df_reset_list()

    def add_square(self, x, y, z, side, texture, rotation, unused1, large, unused2, keep, typ):
        self.lib.tpw_df_add_square(bitsf(x), bitsf(y), bitsf(z), side, s32(texture), s32(rotation), bitsf(unused1), s32(large), unused2,
                                   s32(keep), s32(typ))

    def add_poly(self, positions, texture, uvs, points, typ):
        self.lib.tpw_df_add_poly(positions, s32(texture), uvs, points, s32(typ))

    def rotate(self, block, angle):
        self.lib.tpw_df_rotate(block, bitsf(angle))

    def animate(self, columns, rows, step, phase):
        self.lib.tpw_df_animate(s32(columns), s32(rows), bitsf(step), bitsf(phase))

    def draw(self, x, y, z, a, b, c, d, e, f):
        self.lib.tpw_df_draw(bitsf(x), bitsf(y), bitsf(z), s32(a), s32(b), s32(c), s32(d), s32(e), s32(f))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_df_alloc.restype, lib.tpw_df_alloc.argtypes = C.c_uint64, [C.c_uint64, C.c_uint32]
    lib.tpw_df_addr.restype, lib.tpw_df_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_df_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_df_sort.argtypes = [C.c_uint64, C.c_uint32]
    lib.tpw_df_add_square.argtypes = [C.c_float, C.c_float, C.c_float, C.c_uint32, C.c_int32, C.c_int32, C.c_float, C.c_int32, C.c_uint32,
                                      C.c_int32, C.c_int32]
    lib.tpw_df_add_poly.argtypes = [C.c_uint64, C.c_int32, C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_df_rotate.argtypes = [C.c_uint64, C.c_float]
    lib.tpw_df_animate.argtypes = [C.c_int32, C.c_int32, C.c_float, C.c_float]
    lib.tpw_df_draw.argtypes = [C.c_float, C.c_float, C.c_float] + [C.c_int32] * 6
    out = (C.c_int64 * 16)()
    n = lib.tpw_df_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    g = s.g
    s.words(g("count"), [case["count"]])
    s.write(g("flag"), bytes([case["flag"]]))
    s.words(g("f9bd4"), [case["f9bd4"]])
    s.words(g("random"), case["random"])
    s.words(g("tpg") + 4, [case["mask"], case["scale"]])
    if case["table"] is not None:
        s.words(g("tpg") + 0xC, case["table"])
    s.words(g("gridA"), case["gridA"])
    s.words(g("gridB"), case["gridB"])
    lo, hi = case["win"]
    if hi > lo:
        s.write(g("faces") + RECORD * lo, bytes([case["fill"]]) * (RECORD * (hi - lo)))
        s.words(g("keys") + 4 * lo, [case["fill"] * 0x01010101] * (hi - lo))


def execute(s, case):
    op = case["op"]
    block = None
    if op == "sort":
        n = len(case["keys"])
        block = s.alloc(4 * (n + 2))
        s.words(block, [case["head"]] + case["keys"] + [case["head"] ^ MASK])
        s.sort(block, n)
        return block, n + 2
    if op == "reset":
        s.reset_list()
    elif op == "square":
        s.add_square(*case["args"])
    elif op == "poly":
        block = s.alloc(0x90 + 4)
        s.words(block, case["points"])
        s.add_poly(block, case["texture"], block + 0x30, block + 0x60, case["type"])
        return block, 0x90 // 4
    elif op == "rotate":
        block = s.alloc(60)
        s.words(block, case["points"] + [0x600DCAFE])
        s.rotate(block, case["angle"])
        return block, 13
    elif op == "animate":
        s.animate(*case["args"])
    elif op == "draw":
        s.draw(*case["args"])
    return None


def faces_of(s, case, lo, hi):
    """The window's records as words, except the words holding bytes (the texture, and for a square the side and the last flag byte), which
    are the four bytes in memory order: a word read with either side's byte order would differ."""
    if hi <= lo:
        return []
    raw = s.read(s.g("faces") + RECORD * lo, RECORD * (hi - lo))
    byte_words = {39} | ({3, 7} if case["op"] == "square" else set())
    out = []
    for i in range(RW * (hi - lo)):
        chunk = raw[4 * i:4 * i + 4]
        if i % RW in byte_words:
            out.append(tuple(chunk))
        else:
            w = struct.unpack(s.fmt, chunk)[0]
            out.append(canon_f(w) if case["canon"] and i % RW < 36 else w)
    return out


def observe(s, case, block):
    g = s.g
    lo, hi = case["win"]
    out = {"log": s.log(), "count": s.rd_word(g("count")), "flag": s.read(g("flag"), 1), "f9bd4": s.rd_word(g("f9bd4")),
           "keys": s.rd_words(g("keys") + 4 * lo, hi - lo) if hi > lo else []}
    out["faces"] = faces_of(s, case, lo, hi)
    out["gridA"] = [canon_f(w) for w in s.rd_words(g("gridA"), GRID_WORDS)]
    out["gridB"] = [canon_f(w) for w in s.rd_words(g("gridB"), GRID_WORDS)]
    if block is not None:
        words = s.rd_words(block[0], block[1])
        out["block"] = [canon_f(w) for w in words] if case["op"] == "rotate" else words
    return out


def run(s, case):
    try:
        build(s, case)
        block = execute(s, case)
        return observe(s, case, block)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0xFFC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x007FFFFF, 0x80000001, 0x3F800000, 0xBF800000,
           fb(0.5), fb(1.7), fb(20.0), fb(-20.0), fb(1.5707964), fb(-1.5707964), fb(1024.0), fb(-1024.0), 0x7F7FFFFF, 0xFF7FFFFF, 0x00800000]
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


def finite_f(r):
    b = rf(r, wide=False)
    return 0x3F800000 if (b & 0x7F800000) == 0x7F800000 else b


def table_words(r, kind=None):
    kind = r.choice(["sin", "sin", "random", "rough"]) if kind is None else kind
    if kind == "sin":
        return list(_SIN)
    if kind == "random":
        return [fb(r.uniform(-1, 1)) for _ in range(TABLE_WORDS)]
    words = [fb(r.uniform(-1, 1)) for _ in range(TABLE_WORDS)]
    for _ in range(40):
        words[r.randrange(TABLE_WORDS)] = r.choice([0, 0x80000000, 0x3F800000, 0xBF800000, 0x00000001, 0x80000003, fb(2.5), fb(-300.0)])
    return words


def grid_words(r, mode="random"):
    if mode == "zero":
        return [0] * GRID_WORDS
    return [rf(r) for _ in range(GRID_WORDS)]


def window(count, extra):
    """The records [lo, hi) the case fills and reads: around the count, inside the array."""
    if count < 0 or count >= FACE_MAX:
        return FACE_MAX - 3, FACE_MAX
    return max(count - 1, 0), min(count + extra, FACE_MAX)


def base(r, op, count=None, extra=2, mask=None, scale=None, table=False, canon=False, flag=None, f9bd4=None, grid="random"):
    count = r.choice([0, 1, 2, 5, 100, 0x7FD, 0x7FE, 0x7FF, 0x800, 0x804, 0x805, 0x806, 0x807, 0x7FFFFFFF]) if count is None else count
    if count == 0x7FFFFFFF and r.random() < 0.5:
        count = r.randrange(0, FACE_MAX)
    return {"op": op, "count": count & MASK, "flag": r.choice([0, 1, 1, 2, 0xFF]) if flag is None else flag,
            "f9bd4": r.choice([0, 0, 1, MASK, r.getrandbits(32)]) if f9bd4 is None else f9bd4,
            "random": [r.choice([15, 16, 17, 1, 0, 19, 0x1F, 0x20, 0x3F, 0x100, 0x11F, r.getrandbits(32)]) for _ in range(110)],
            "mask": r.choice([0xFFF, 0xFFF, 0xFFF, 0x7FF, 0x3FF, 0xFF, 1, 0]) if mask is None else mask,
            "scale": r.choice([SIN_SCALE, SIN_SCALE, SIN_SCALE, fb(r.uniform(-1000, 1000)), fb(r.uniform(0, 100)), 0, 0x80000000,
                               fb(1e6), fb(-651.9)]) if scale is None else scale,
            "table": table_words(r) if table is True else (table or None),
            "gridA": grid_words(r, grid), "gridB": grid_words(r, grid), "win": window(count, extra), "fill": r.choice([0xA5, 0x5A, 0x00, 0xFF]),
            "canon": canon, "queues": {}}


# ---- sort
def sort_keys(r, n, pattern=None):
    pattern = r.choice(["random", "low", "ascending", "descending", "equal", "two", "few", "list"]) if pattern is None else pattern
    if pattern == "random":
        return [r.getrandbits(32) for _ in range(n)]
    if pattern == "low":
        return [r.randrange(32) | (r.getrandbits(27) << 5) for _ in range(n)]
    if pattern == "ascending":
        return [(k << 5 | (k * 7 // max(n, 1)) % 20) & MASK for k in range(n)]
    if pattern == "descending":
        return [(k << 5 | (19 - k * 20 // max(n, 1))) & MASK for k in range(n)]
    if pattern == "equal":
        return [(k << 5 | 7) for k in range(n)]
    if pattern == "two":
        return [(k << 5 | r.choice([3, 4])) for k in range(n)]
    if pattern == "few":
        return [(k << 5 | r.choice([0, 1, 1, 1, 15, 15, 16, 17, 19])) for k in range(n)]
    return [(k << 5 | (r.choice([15, 16, 17]) if r.random() < 0.7 else r.randrange(20))) for k in range(n)]


def sort_case(r, n=None, pattern=None, keys=None):
    n = int(2 ** r.uniform(0, 11)) if n is None else n
    c = base(r, "sort", count=0)
    c["keys"] = sort_keys(r, n, pattern) if keys is None else keys
    c["head"] = r.getrandbits(32)
    c["win"] = (0, 0)
    c["table"] = None
    return c


def sort_edges():
    r = random.Random(1270)
    sizes = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 20, 31, 32, 33, 64, 100, 255, 256, 257, 1000, 2048, FACE_MAX]
    for n in sizes:
        for pattern in ("random", "ascending", "descending", "equal", "few", "list"):
            yield sort_case(r, n, pattern)
    yield sort_case(r, keys=[0xFFFFFFFF, 0, 0x1F, 0x20, 0x3F, 0x7FFFFFFF, 0x80000000, 0xFFFFFFE0])


def sort_random(r):
    return sort_case(r)


# ---- the list's small functions
def reset_case(r):
    c = base(r, "reset")
    c["win"] = (0, 0)
    return c


def square_args(r, texture=None, side=None, keep=None, typ=None):
    return [rf(r), rf(r), rf(r), r.choice([0, 1, 4, 0x10, 0x40, r.getrandbits(8)]) if side is None else side,
            r.choice([0, 1, 1, 2, 5, 15, 16, 17, 19, 20, 31, 32, 33, 0x7FFFFFFF, MASK, 0x80000000, r.getrandbits(32)]) if texture is None else texture,
            r.choice([0, 1, 90, 180, 270, 0x1F, r.getrandbits(32)]), r.choice([0, 0x3F800000, fb(2.5), 0x7FC00000, r.getrandbits(32) & 0x7FFFFFFF if False else fb(r.uniform(-9, 9))]),
            r.choice([0, 1, MASK, r.getrandbits(32)]), r.choice([0, 1, 0x80, 0xFF, r.getrandbits(8)]),
            r.choice([0, 1, 2, MASK, r.getrandbits(32)]) if keep is None else keep,
            r.choice([0, 1, 3, 2, 7, MASK, r.getrandbits(32)]) if typ is None else typ]


def square_case(r, count=None, texture=None, flag=None, f9bd4=None, keep=None, side=None, typ=None):
    c = base(r, "square", count=count, flag=flag, f9bd4=f9bd4)
    c["args"] = square_args(r, texture, side, keep, typ)
    return c


def square_edges():
    r = random.Random(1271)
    for count in (0, 1, 7, 0x7FF, 0x805, 0x806, 0x807, 0x7FFFFFFF):
        for texture in (1, 0, 2, 20):
            for flag in (0, 1):
                yield square_case(r, count=count, texture=texture, flag=flag, f9bd4=0)
    for f9bd4 in (0, 1, MASK, 0x80000000, 0x100):
        for keep in (0, 1, 5, MASK):
            yield square_case(r, count=r.randrange(0, 50), texture=r.choice([0, 2, 15]), f9bd4=f9bd4, keep=keep, flag=1)
    for texture in (0, 1, 2, 15, 16, 17, 19, 31, 32, 33, 63, 255, 256, 0x7FFFFFFF, MASK, 0x80000000):
        yield square_case(r, count=r.randrange(0, 200), texture=texture, flag=1)
        yield square_case(r, count=r.randrange(0, 200), texture=texture, flag=0)
    for count in (0x7FD, 0x7FE, 0x7FF, 0x800, 0x801, 0x804, 0x805):
        yield square_case(r, count=count, texture=15, flag=1)
    for side in (0, 1, 4, 0x10, 0x40, 0xFF):
        yield square_case(r, count=5, side=side, texture=3)
    for typ in (0, 1, 2, 3, 4, 0x7FFFFFFF, MASK):
        yield square_case(r, count=9, typ=typ, texture=4)
    yield reset_case(r)
    yield reset_case(r)


def square_random(r):
    return square_case(r)


def poly_points(r):
    return [rf(r) for _ in range(36)]


def poly_case(r, count=None, texture=None, typ=None, quiet=True):
    c = base(r, "poly", count=count)
    c["points"] = poly_points(r)
    if quiet:
        c["points"] = [0x7FC00000 if (w & 0x7F800000) == 0x7F800000 and (w & 0x7FFFFF) else w for w in c["points"]]
    c["texture"] = r.choice([0, 1, 15, 16, 17, 19, 31, 32, 0x7FFFFFFF, MASK, 0x80000000, r.getrandbits(32)]) if texture is None else texture
    c["type"] = r.choice([0, 1, 2, 3, MASK, r.getrandbits(32)]) if typ is None else typ
    return c


def poly_edges():
    r = random.Random(1272)
    for count in (0, 1, 2, 0x7FF, 0x805, 0x806, 0x807, 0x7FFFFFFF):
        yield poly_case(r, count=count, texture=15, typ=1)
        yield poly_case(r, count=count, texture=1, typ=1)
    for texture in (0, 1, 2, 15, 16, 17, 19, 31, 32, 33, 255, 256, 0x7FFFFFFF, MASK, 0x80000000):
        yield poly_case(r, count=r.randrange(0, 100), texture=texture)
    for typ in (0, 1, 3, MASK):
        yield poly_case(r, count=3, typ=typ)


def poly_random(r):
    return poly_case(r)


# ---- the rotation
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


def rotate_case(r, angle=None, points=None, scale=None, mask=None):
    c = base(r, "rotate", table=True, canon=True, scale=scale, mask=mask)
    c["win"] = (0, 0)
    c["angle"] = angle_bits(r) if angle is None else angle
    c["points"] = [r.choice([rf(r), fb(r.uniform(-100, 100)), fb(r.uniform(-100, 100))]) for _ in range(12)] if points is None else points
    return c


def rotate_edges():
    r = random.Random(1273)
    for angle in (0, 0x80000000, fb(1.5707964), fb(-1.5707964), fb(3.1415927), fb(-3.1415927), fb(6.2831855), fb(4e6), fb(-4e6), fb(1e10),
                  fb(-1e10), 0x7F800000, 0xFF800000, 0x7FC00000, fb(3.3e6), fb(3.29e6), fb(1e-30), 0x00000001):
        yield rotate_case(r, angle=angle, scale=SIN_SCALE, mask=0xFFF)
        yield rotate_case(r, angle=angle)
    for k in range(12):
        pts = [fb(r.uniform(-50, 50)) for _ in range(12)]
        yield rotate_case(r, angle=fb(r.uniform(-4, 4)), points=pts, scale=SIN_SCALE, mask=0xFFF)
    yield rotate_case(r, angle=fb(0.7), points=[0] * 12, scale=SIN_SCALE, mask=0xFFF)
    yield rotate_case(r, angle=fb(0.7), points=[0x80000000] * 12, scale=SIN_SCALE, mask=0xFFF)
    yield rotate_case(r, angle=fb(0.7), points=[0x3F800000, 0, 0xDEADBEEF & 0x7FFFFF] * 4, scale=SIN_SCALE, mask=0xFFF)


def rotate_random(r):
    return rotate_case(r)


# ---- the water
def animate_case(r, columns=None, rows=None, step=None, phase=None, scale=None, mask=None):
    c = base(r, "animate", table=True, canon=True, scale=scale, mask=mask)
    c["win"] = (0, 0)
    c["args"] = [r.choice([16, 16, 1, 2, 5, 0, -1, r.randrange(1, 17)]) & MASK if columns is None else columns,
                 r.choice([19, 19, 1, 2, 7, 0, -1, r.randrange(1, 20)]) & MASK if rows is None else rows,
                 r.choice([fb(20.0), fb(20.0), 0, 0x80000000, fb(r.uniform(-30, 30)), rf(r)]) if step is None else step,
                 r.choice([0, fb(r.uniform(0, 400)), fb(r.uniform(-30, 30)), rf(r), fb(5.5)]) if phase is None else phase]
    return c


def animate_edges():
    r = random.Random(1274)
    for phase in (0, fb(1.0), fb(10.0), fb(-3.0), fb(123.456), 0x80000000, fb(1e6), fb(1e12), 0x7F800000, 0x7FC00000):
        yield animate_case(r, 16, 19, fb(20.0), phase, SIN_SCALE, 0xFFF)
    for step in (0, fb(1.0), fb(-1.0), fb(20.0), fb(1e30), fb(1e-30), 0x7F800000, 0x7FC00000, 0x80000000, fb(0.3333333)):
        yield animate_case(r, 16, 19, step, fb(r.uniform(0, 100)), SIN_SCALE, 0xFFF)
    for columns, rows in ((1, 1), (0, 5), (5, 0), (16, 1), (1, 19), (-3, 4), (4, -3), (2, 2), (15, 18), (16, 19)):
        yield animate_case(r, columns & MASK, rows & MASK, fb(20.0), fb(r.uniform(0, 100)), SIN_SCALE, 0xFFF)
    for scale in (0, 0x80000000, fb(1e6), fb(1e-3), fb(-651.9), fb(1e9)):
        yield animate_case(r, 16, 19, fb(20.0), fb(7.0), scale, 0xFFF)
    for mask in (0, 1, 0xFF, 0x3FF):
        yield animate_case(r, 16, 19, fb(20.0), fb(7.0), SIN_SCALE, mask)


def animate_random(r):
    return animate_case(r)


def draw_case(r, count=None, args=None, grid="random"):
    c = base(r, "draw", count=count, canon=True, grid=grid)
    if args is None:
        a, b = r.choice([(10, 11), (4, 6), (1, 1), (0, 3), (3, 0), (16 - 1, 19 - 1), (r.randrange(0, 16), r.randrange(0, 19))])
        a = min(a, 15)
        b = min(b, 18, 110 // max(a, 1))
        args = [rf(r), rf(r), rf(r), a, b, r.randrange(0, 16 - a), r.randrange(0, 19 - b), r.getrandbits(32), r.getrandbits(32)]
    if 0 < args[3] < 1 << 31 and 0 < args[4] < 1 << 31 and args[3] * args[4] > 110:     # gDynamicCellRandom has 110 words
        args = args[:4] + [110 // args[3]] + args[5:]
    c["args"] = args
    cells = max(s32(args[3]), 0) * max(s32(args[4]), 0)
    c["win"] = window(c["count"], cells + 1)
    if c["count"] < FACE_MAX and c["count"] + cells > FACE_MAX:
        c["win"] = (max(c["count"] - 1, 0), FACE_MAX)
    return c


def draw_edges():
    r = random.Random(1275)
    for count in (0, 1, 0x700, 0x7FF, 0x7FC, 0x7FD, 0x805, 0x806, 0x807, 0x7FFFFFFF):
        yield draw_case(r, count=count, args=[fb(1.0), fb(2.0), fb(3.0), 10, 11, 0, 0, 0x10, 0x13])
        yield draw_case(r, count=count, args=[fb(-5.5), fb(0.25), fb(100.0), 4, 6, 10, 0, 0x10, 0x13])
    for a, b, c, d in ((15, 18, 0, 0), (1, 1, 14, 17), (1, 1, 0, 0), (0, 5, 0, 0), (5, 0, 0, 0), (-2, 3, 0, 0), (3, -2, 0, 0), (2, 3, 13, 15),
                       (15, 1, 0, 17), (1, 18, 14, 0), (10, 11, 5, 7)):
        yield draw_case(r, count=r.randrange(0, 1000), args=[rf(r), rf(r), rf(r), a & MASK, b & MASK, c, d, 0, 0])
    for x in (0, 0x80000000, fb(1.0), fb(-1e30), 0x7F800000, 0x7FC00000):
        yield draw_case(r, count=3, args=[x, x, x, 3, 3, 1, 1, 0, 0])
    for count in (0x7FD, 0x7FE, 0x7FF, 0x800, 0x801):
        yield draw_case(r, count=count, args=[fb(1.0), fb(2.0), fb(3.0), 4, 6, 10, 0, 0x10, 0x13], grid="random")


def draw_random(r):
    return draw_case(r)


TESTS = [
    FnTest("sort", ppc, host, edge=sort_edges, rnd=sort_random),
    FnTest("DC_ResetSquareFaceListPtr", ppc, host, edge=lambda: [reset_case(random.Random(5)) for _ in range(3)], rnd=reset_case),
    FnTest("DC_AddSquareFaceToList", ppc, host, edge=square_edges, rnd=square_random),
    FnTest("DC_AddPolyFaceToList", ppc, host, edge=poly_edges, rnd=poly_random),
    FnTest("HDZ_RotatePointZ", ppc, host, edge=rotate_edges, rnd=rotate_random),
    FnTest("DC_AnimateWater", ppc, host, edge=animate_edges, rnd=animate_random),
    FnTest("DC_DrawWater", ppc, host, edge=draw_edges, rnd=draw_random),
]
