"""The regular sprite draw path (engine library, src/tb_sprites_draw.cpp, src/tb_rotatevertex.cpp and
src/tb_drawprimitive_addsprite.cpp), original versus translation: SSpriteGameData::Prepare (0x1002f3d0),
SetupVertices (0x1002f688), CalcRegular (0x1003042c), DrawRegular (0x10030a38) and AddRegular (0x10030ac0),
RotateVertex_Y (0x10038cd0) and RotateVertex_Z (0x10038e9c), CDrawPrimitive::AddSprite (0x10008230) and the
vertex record's ReturnPtr (0x100087e8), SetUV (0x10008824), SetDiffuse (0x10008864) and SetVertex (0x1000889c).

The rotations and the four vertex leaves are tested on their own, on buffers of random floats. Everything else
runs in one world built twice from one description, in emulator memory for the original and through
tests/unit_SpriteDraw.cpp for the translation: four sprite instances, three engine sprites, six texture entries,
three arrays of partial entries, two sprite lists, the sprite manager, the height field, the display, the light
manager, the final matrix, the draw object with four primitive records and its vertex and index memory, a sprite
record for AddSprite and a scratch record with four vertices for CalcRegular and SetupVertices. A case is a script of
operations on that world (a function under test, a call of the manager, or a change to the world); after every
operation the test compares the answer and the state of everything the operations could change (the draw object, the
records, both memories, the sprites, engine sprites, entries and arrays, the manager's tables and the tree of kinds,
the hot sprite, the heap traffic), and at the end the recorded calls of the fakes.

Real on both sides: the sprite manager (Reset, AddSprite, AddPartialSprite, AddList, Prepare and Render with its two
renderers), CTextureEntry (GetBlendingMode and, for the destruction of arrays of partial entries, its destructor),
DrawSprite, AddPoly_TLCTEX and the clipper, so the vertices and indices that land in the primitive's memory are the
ones the whole chain makes. Fakes that record every argument (floats by their bits, a NaN as one value, a pointer into
the world as a place): the texture cache's Prepare and Remove, CMatrix::TransformVertex (it writes the four words the
queue "TV" gives), the math unit (the strided Transform turns the words of each vertex round and gives it a w from the
queue; Project answers from the queue), the primitive buffer (GetNextPrimitive hands out the next record made fresh,
or the one the queue names; both ExecuteCurrentPrimitive clear the current primitive; SaveInfo),
CalcVertexFogIntensities, GeneratePartialTexture and the three shadow and partial drawing members (unit w41b's) and
the assertion handler.

The stack. The vertices of AddSprite's clipped path and of DrawRegular and AddRegular are built in the original's stack
frame, and the specular colour of a vertex (and the w of a flat sprite) is never written: stack leftovers. The test clears
the PowerPC stack below the call before every operation and, with a code hook, at the entry of DrawRegular, AddRegular
and AddSprite (so that the second sprite of a frame does not find the first one's leftovers), and the translation
zeroes its arrays, so that both carry the same words.

Unicorn computes the fused multiply-adds in double precision and rounds twice, and gives fctiwz of a value above
INT32_MAX as 0x80000000 where the processor saturates (FINDINGS 2026-10-10 and 2026-10-11): the code hook
HardwareRounding corrects both inside the functions under test and the clipper. NaNs are one value wherever a float is
dumped, mixed or compared. The libm sine and cosine of RotateVertex_Z are the host's on both sides (tpwtest's stubs).

NaN. A NaN's sign is not reproduced by the translation (the host compiler turns x * -1.0 into a negation, which flips it, and
the processor does not) and the clipper's outside codes depend on it, so the worlds of the drawing tests (DrawRegular,
AddRegular, the scripts) have no NaN, no infinity and no zero used size (see no_nan); the tests of the functions that only
compare (Prepare) or are handed the corners (AddSprite) keep them. A sprite added twice to the manager since its last Reset
would link itself into its own kind and RenderSprites would walk the ring for ever (the original too): such a frame is not
rendered by the test (the operation answers -2 on both sides).

The height field has 192 samples with its own beginning at the 65th, so that a sprite outside the grid (up to three cells) reads
defined samples before or after them on both sides.

Not tested: CalcRegular and SetupVertices on a sprite without engine data (the original reads low memory), a NULL gei_MathUnit
in CalcRegular (the original calls through the null pointer after the assertion) and address arithmetic that wraps at 4 GB.
"""
import contextlib
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_SpriteDraw"
MASK = 0xFFFFFFFF
NAN, INF = math.nan, math.inf

# ---- the functions under test
PREPARE, SETUP, CALC, DRAW_REGULAR, ADD_REGULAR = 0x1002F3D0, 0x1002F688, 0x1003042C, 0x10030A38, 0x10030AC0
ADD_SPRITE, RET_PTR, SET_UV, SET_DIFFUSE, SET_VERTEX = 0x10008230, 0x100087E8, 0x10008824, 0x10008864, 0x1000889C
ROT_Y, ROT_Z = 0x10038CD0, 0x10038E9C
SIZES = {PREPARE: 648, SETUP: 844, CALC: 1472, DRAW_REGULAR: 84, ADD_REGULAR: 84, ADD_SPRITE: 1392, RET_PTR: 12, SET_UV: 20,
         SET_DIFFUSE: 8, SET_VERTEX: 52, ROT_Y: 404, ROT_Z: 428}
# ---- real callees: the sprite manager, DrawSprite, AddPoly_TLCTEX, the clipper, the texture entry
MGR_RESET, MGR_RENDER, MGR_ADD_SPRITE, MGR_ADD_PARTIAL, MGR_ADD_LIST, MGR_PREPARE = (
    0x10031770, 0x100317E4, 0x10031874, 0x100318E4, 0x1003195C, 0x100319C8)
MGR_RENDER_SPRITES, MGR_RENDER_PARTIAL = 0x10031A74, 0x10031F08
DRAW_SPRITE, ADD_POLY = 0x10007EDC, 0x100078C0
CPP_TEST_DATA, CLIP_POLY = 0x1003F6DC, 0x10040640
EDGE_CLIPPERS = (0x1003F8B0, 0x1003FAE4, 0x1003FB68, 0x1003FD94, 0x1003FFC8, 0x100401F4, 0x10040424)
PTR_GLUE, CVT_UNSIGNED = 0x1004C858, 0x1004B9FC
ENTRY = dict(ctor=0x10040D30, dtor=0x10040DFC, Create=0x10040E84, Close=0x10040F7C, GetBlendingMode=0x100410A4, BestSize=0x1004117C,
             GetBestSize=0x10041204, GetData=0x10041274, GetCorrectedData=0x10041334, SetSize=0x10041560,
             FlushEntryFlags=0x1004168C, Lock=0x10041738, Unlock=0x100417C0, StoreAsWrapping=0x10041848)
REAL = (set(SIZES) | {MGR_RESET, MGR_RENDER, MGR_ADD_SPRITE, MGR_ADD_PARTIAL, MGR_ADD_LIST, MGR_PREPARE, MGR_RENDER_SPRITES,
                      MGR_RENDER_PARTIAL, DRAW_SPRITE, ADD_POLY, CPP_TEST_DATA, CLIP_POLY, PTR_GLUE, CVT_UNSIGNED}
        | set(EDGE_CLIPPERS) | set(ENTRY.values()))
# ---- callees that are fakes
TRANSFORM_VERTEX, CACHE_PREPARE, CACHE_REMOVE, GEN_PARTIAL = 0x1001C584, 0x10032DDC, 0x10034610, 0x1002EBB8
GET_NEXT, EXEC_LONG, EXEC_SHORT, SAVE_INFO, FOG = 0x10006A58, 0x10006F40, 0x10007360, 0x10027460, 0x10014D38
DRAW_SHADOW, ADD_SHADOW, DRAW_PARTIAL = 0x1002FF20, 0x10030210, 0x10030B48
# ---- engine globals
DISPLAY, SYSTEM = 0x10165634, 0x10165F80
G_MANAGER, G_TCACHE, G_FINAL, G_HFIELD, G_MATH, G_DRAW, G_LIGHT, G_CLIPPING, G_RENDER = (
    0x10173154, 0x10173184, 0x100AFF70, 0x100AF11C, 0x100AFE94, 0x10052B3C, 0x100AF1B8, 0x102B3638, 0x10165630)
G_VIEW_ANGLE, G_SINE, G_FREE, G_GROUPS = 0x100AFE98, 0x101731C0, 0x10051F74, 0x10169154
GUARDED = [(DISPLAY, 0x94C), (SYSTEM, 0x20), (G_MANAGER, 4), (G_TCACHE, 4), (G_FINAL, 4), (G_HFIELD, 4), (G_MATH, 4), (G_VIEW_ANGLE, 4),
           (G_DRAW, 4), (G_LIGHT, 4), (G_CLIPPING, 4), (G_RENDER, 4), (G_SINE, 0x400), (G_FREE, 4), (G_GROUPS, 0x800 * 0x14)]
# The code ranges whose fused multiply-adds and fctiwz run under HardwareRounding.
ROUNDED = [(SETUP, SETUP + SIZES[SETUP]), (CALC, CALC + SIZES[CALC]), (ROT_Y, ROT_Y + SIZES[ROT_Y]), (ROT_Z, ROT_Z + SIZES[ROT_Z]),
           (CPP_TEST_DATA, 0x10040D00)]

# ---- the world (tests/unit_SpriteDraw.cpp has the same numbers)
SPRITES, ENGINES, ENTRIES, ARRAYS, MAX_PART, LISTS, RECORDS, MARKERS = 4, 3, 6, 3, 4, 2, 4, 8
VERTEX_BYTES, INDEX_ENTRIES, INPUT_WORDS, HEIGHTS, HEIGHTS_BEFORE = 0x2000, 2048, 128, 192, 64
SHOWN, SHOWN_GROUPS = 16, 8
NO_OFFSET = 0x80000000
PAGE_BASE = 0x7D100000       # a page marker's address on the PowerPC side (never dereferenced)
RECORD_SIZE, SPRITE_SIZE, ENGINE_SIZE, ENTRY_SIZE = 0x38, 0x44, 0x18, 0x5C
L_DRAW, L_MATH, L_RENDER, L_LIGHT, L_CACHE, L_MATRIX, L_MANAGER, L_HFIELD = 1, 2, 3, 4, 5, 6, 7, 8
L_MARKER, L_ENTRY, L_SPRITE, L_ENGINE, L_ARRAY, L_LIST = 11, 21, 31, 41, 51, 61
R_VERTICES, R_INDICES, R_INPUT, R_SCRATCH_VERTICES, R_SCRATCH_RECORD = 1, 2, 3, 4, 5
# The words of the state, in order (tests/unit_SpriteDraw.cpp, PutState).
SECTIONS = [("result", 1), ("draw", 5), ("hot", 1), ("recs", RECORDS * 13), ("vertices", VERTEX_BYTES // 4),
            ("indices", INDEX_ENTRIES // 2), ("sprite_rec", 11), ("scratch_rec", 11), ("scratch_vertices", 16),
            ("sprites", SPRITES * 18), ("engines", ENGINES * 9), ("entries", ENTRIES * 3), ("arrays", ARRAYS * (1 + MAX_PART * 3)),
            ("manager", 4 + SHOWN + SHOWN + 4), ("groups", SHOWN_GROUPS * 5), ("heap", 4)]
STATE_WORDS = sum(n for _, n in SECTIONS)

# Operations (tpw_sd_run).
(OP_PREPARE, OP_CALC, OP_SETUP, OP_DRAW, OP_ADD, OP_ADD_SPRITE, OP_RESET, OP_ADD_LIST, OP_MPREPARE, OP_MRENDER, OP_SET_SPRITE,
 OP_SET_ENGINE, OP_SET_DISPLAY, OP_SET_ENTRY) = range(1, 15)
OP_NAMES = {OP_PREPARE: "Prepare", OP_CALC: "CalcRegular", OP_SETUP: "SetupVertices", OP_DRAW: "DrawRegular", OP_ADD: "AddRegular",
            OP_ADD_SPRITE: "AddSprite", OP_RESET: "Reset", OP_ADD_LIST: "AddList", OP_MPREPARE: "ManagerPrepare",
            OP_MRENDER: "ManagerRender", OP_SET_SPRITE: "SetSprite", OP_SET_ENGINE: "SetEngine", OP_SET_DISPLAY: "SetDisplay",
            OP_SET_ENTRY: "SetEntry"}


def fb(x):
    """A float's bits, as the processor holds it (any NaN is the same bits)."""
    return T.f32_bits(T.f32(x)) if x == x else 0x7FC00000


def bf(word):
    return struct.unpack(">f", struct.pack(">I", word & MASK))[0]


def canon(w):
    """Any NaN as one value."""
    return 0x7FC00000 if w & 0x7F800000 == 0x7F800000 and w & 0x007FFFFF else w & MASK


def cfb(x):
    """Canonical bits of a float register's value."""
    return 0x7FC00000 if math.isnan(x) else struct.unpack(">I", struct.pack(">f", x))[0]


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def w16(e, a, v):
    e.write(a, struct.pack(">H", v & 0xFFFF))


def ws(e, a, values):
    e.write(a, struct.pack(">%dI" % len(values), *[v & MASK for v in values]))


def rs(e, a, n):
    return list(struct.unpack(">%dI" % n, e.read(a, 4 * n)))


def signed64(v):
    v &= 0xFFFFFFFFFFFFFFFF
    return v - (1 << 64) if v >= (1 << 63) else v


def i64(values):
    values = [signed64(v) for v in values]
    return (C.c_int64 * max(len(values), 1))(*values)


def little(words):
    return struct.pack("<%dI" % len(words), *[w & MASK for w in words])


# ----------------------------------------------------------------- the emulator's float corrections

LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Two things Unicorn (QEMU 5.0) does not execute as the processor does, corrected for the code under
    test and nowhere else (as in test_RainRender.py and test_FrameStandins.py):
    - fmadds, fmsubs, fnmadds and fnmsubs are computed in double precision and rounded to single, which can
      differ by one unit in the last place from the processor's single rounding of the exact result; the
      result is replaced by the C library's fmaf;
    - fctiwz of a value above INT32_MAX (or +infinity) gives 0x80000000; the processor saturates to
      0x7fffffff.
    A code hook looks at each instruction before it runs and rewrites its target register just before
    the next one. ranges: the (begin, end) address ranges of the functions under test."""

    changed = 0    # results the correction replaced (all cases so far), for the notes

    def __init__(self, e, ranges):
        self.e, self.pending, self.hooks, self.ranges = e, None, [], ranges

    def __enter__(self):
        for begin, end in self.ranges:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            kind, target, value = self.pending
            self.pending = None
            if kind == "fctiwz":
                HardwareRounding.changed += 1
                uc.reg_write(ppcemu.FPR[target], (fpr(e, target) & 0xFFFFFFFF00000000) | 0x7FFFFFFF)
            else:
                corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
                if fpr(e, target) != corrected and not (value != value and fpr_double(e, target) != fpr_double(e, target)):
                    HardwareRounding.changed += 1
                uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        if word >> 26 == 63 and (word >> 1) & 0x3FF == 15:          # fctiwz frD, frB
            if fpr_double(e, rb) >= 2147483648.0:
                self.pending = ("fctiwz", rd, None)
            return
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        for x in (a, c, b):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        subtract = kind in (28, 30)
        value = float(LIBM.fmaf(a, c, -b if subtract else b))
        if kind in (30, 31):
            value = -value
        self.pending = ("fused", rd, value)


def stack_cleared(e):
    """Zeroes the stack below the call's frame: the vertices' specular words are never written."""
    sp = ppcemu.STACK_BASE + ppcemu.STACK_SIZE - 0x1000 - 0x400
    e.write(sp - 0x10000, b"\0" * 0x10000)


class StackClearedAtEntry:
    """A code hook that zeroes the stack below the caller at the entry of the given functions: DrawRegular, AddRegular and AddSprite
    keep their quad's vertices in their frames and never write the specular word (and a ground sprite's w), so a second call in the
    same operation would find the first one's leftovers, which the translation (zeroed arrays) cannot have."""

    def __init__(self, e, entries):
        self.e, self.entries, self.hooks = e, entries, []

    def __enter__(self):
        for address in self.entries:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=address, end=address))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        self.e.write(self.e.r(1) - 0x1000, b"\0" * 0x1000)


def callback_stub(fn):
    """An import stub that calls PowerPC code back (tpwtest.stub_cb, but for one case's stubs): fn(emu, call)."""
    def hook(e):
        e.pending_cb = fn
        e.uc.reg_write(T.P.UC_PPC_REG_PC, ppcemu.RETURN_ADDR)
    return hook


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if hasattr(e, "sd_math"):
        return
    # The math unit: +4 is the vtable pointer; the strided Transform is the slot at +0x10, Project the one at +0x14.
    transform = T.fake_function(e, lambda em: em.sd_side.transform(em))
    project = T.fake_function(e, lambda em: em.sd_side.project(em))
    vtable = T.persist_alloc(e, 0x40)
    e.write(vtable + 0x10, struct.pack(">II", transform, project))
    e.sd_math = T.persist_alloc(e, 0x10)
    e.write(e.sd_math, struct.pack(">II", 0, vtable))
    e.sd_free = T.fake_function(e, lambda em: em.sd_side.free(em))
    e.sd_side = None


@contextlib.contextmanager
def guarded(e):
    """The engine globals a case overwrites, put back afterwards (other modules share them)."""
    saved = [(a, e.read(a, n)) for a, n in GUARDED]
    try:
        yield
    finally:
        for a, data in saved:
            e.write(a, data)


# ----------------------------------------------------------------- the leaves and the rotations, on their own


def rotate_cases_vertices(r):
    """Four vertices of four floats: moderate values, now and then an edge value."""
    return [rfl(r, 0.06, -50.0, 50.0) for _ in range(16)]


RFL_SPECIALS = [0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 0.25, 4.0, -4.0, 1e30, -1e30, NAN, INF, -INF, 1e-40, 3.4028234663852886e38, 16384.0, 0.3,
                100.0, 20.0, -20.0, 1e-3]


def rfl(r, special=0.08, lo=-10.0, hi=10.0):
    """A float for a field: mostly moderate, now and then an edge value."""
    if r.random() < special:
        return r.choice(RFL_SPECIALS)
    return T.f32(r.uniform(lo, hi))


NO_NAN = False


@contextlib.contextmanager
def no_nan():
    """Worlds generated in this context have no NaN, no infinity and nothing that overflows to one (and no zero used size, whose
    division gives one). A NaN that reaches the clipper through the arithmetic of the drawing has a sign the PowerPC and the host do
    not agree on (the host compiler turns x * -1.0 into a negation, which flips a NaN's sign; the PowerPC multiplies), and the
    clipper's outside codes depend on that sign, so the drawing tests (which run the clipper on computed corners) keep them out of
    their worlds; the tests of the functions that only compare (Prepare) or that are given the corners (AddSprite) keep NaN."""
    global RFL_SPECIALS, NO_NAN
    saved = RFL_SPECIALS, NO_NAN
    RFL_SPECIALS = [v for v in saved[0] if math.isfinite(v) and abs(v) < 1e6]
    NO_NAN = True
    try:
        yield
    finally:
        RFL_SPECIALS, NO_NAN = saved


def sine_table(kind, r=None):
    """The 256-entry table: the real sines, or distinct random values (so that a wrong index shows)."""
    if kind == "sine":
        return [T.f32(math.sin(3.1415927 * (2.0 * i * 0.00390625))) for i in range(256)]
    if kind == "identity":
        return [float(i) for i in range(256)]
    return [rfl(r, 0.02, -2.0, 2.0) for _ in range(256)]


class RotateTest:
    """RotateVertex_Y (axis 'y': the table, the angle a fraction of a turn) and RotateVertex_Z (axis 'z': the library's sine and
    cosine, the angle in radians): 16 floats in, 16 floats out, compared bit for bit with NaNs as one value."""

    def __init__(self, axis):
        self.axis = axis
        self.name = "RotateVertex_Y" if axis == "y" else "RotateVertex_Z"

    def case(self, r, verts=None, angle=None, centre=None, table=None):
        axis = self.axis
        if angle is None:
            angle = r.choice([0.0, 0.25, -0.25, 0.5, -0.5, 1.0, 0.125, 1.0 / 3.0, r.uniform(-2, 2), r.uniform(-1000, 1000), rfl(r, 1.0)]) \
                if axis == "y" else r.choice([0.0, math.pi / 2, -math.pi / 2, math.pi, -math.pi, 2 * math.pi, 1.0, r.uniform(-7, 7),
                                              r.uniform(-1e5, 1e5), rfl(r, 1.0)])
        verts = verts or rotate_cases_vertices(r)
        if centre is None and axis == "y":
            centre = r.choice([None, None, [rfl(r, 0.05, -20, 20) for _ in range(4)]])
        table = table or (r.choice(["sine", "sine", "random", "identity"]) if axis == "y" else None)
        return dict(verts=[T.f32(v) for v in verts], angle=T.f32(angle), centre=centre and [T.f32(v) for v in centre], table=table,
                    table_values=sine_table(table, r) if table else None)

    def edge_cases(self):
        r = random.Random(11 if self.axis == "y" else 12)
        verts = [-3.0, 1.5, 2.0, 1.0, 5.0, 1.5, 2.0, 1.0, 5.0, 7.5, 2.0, 1.0, -3.0, 7.5, 2.0, 1.0]
        if self.axis == "y":
            angles = [0.0, -0.0, 0.25, -0.25, 0.5, 0.75, 1.0, 0.125, 0.001, 0.0038, 0.004, 0.00390625, 255 / 256, 256 / 256, 1.0 / 3.0,
                      2.0, 100.25, -100.25, 8388608.0, 16777216.0, 2147483648.0 / 256, 1e10, -1e10, NAN, INF, -INF, 3.4028234663852886e38]
        else:
            angles = [0.0, -0.0, math.pi / 2, -math.pi / 2, math.pi, -math.pi, 2 * math.pi, 1.0, -1.0, 0.5, 1e-8, 100.0, 1e5, 1e10, 1e30,
                      NAN, INF, -INF, 3.4028234663852886e38]
        for angle in angles:
            for centre in ([None, [1.0, 2.0, 3.0, 4.0]] if self.axis == "y" else [None]):
                yield self.case(r, verts=verts, angle=angle, centre=centre, table="sine" if self.axis == "y" else None)
        for table in (("identity", "random") if self.axis == "y" else ()):
            for angle in (0.0, 0.1, 0.25, 0.5, -0.1):
                yield self.case(r, verts=verts, angle=angle, centre=None, table=table)
        # special vertex values
        for _ in range(12):
            yield self.case(r, verts=[rfl(r, 0.5, -5, 5) for _ in range(16)], angle=r.choice([0.0, 0.1, 0.25]) if self.axis == "y" else 0.7)

    def random_case(self, r):
        return self.case(r)

    def run_case(self, runner, case):
        e, lib = runner.emu, runner.lib
        e.reset_scratch()
        axis = self.axis
        verts = [fb(v) for v in case["verts"]]
        centre = case["centre"] and [fb(v) for v in case["centre"]]
        # the original
        a = e.alloc(0x40)
        ws(e, a, verts)
        c = 0
        if centre:
            c = e.alloc(0x10)
            ws(e, c, centre)
        with guarded(e), HardwareRounding(e, [(ROT_Y, ROT_Y + SIZES[ROT_Y]), (ROT_Z, ROT_Z + SIZES[ROT_Z])]):
            if case["table"]:
                ws(e, G_SINE, [fb(v) for v in case["table_values"]])
            try:
                if axis == "y":
                    T.call_ppc(e, ROT_Y, [a, T.F(case["angle"]), c])
                else:
                    T.call_ppc(e, ROT_Z, [a, T.F(case["angle"])])
            except ppcemu.ImportCalled as ex:
                return [f"PPC side called unstubbed import {ex}"]
            after = [canon(w) for w in rs(e, a, 16)]
        # the translation
        buf = (C.c_float * 16)(*case["verts"])
        centre_buf = (C.c_float * 4)(*case["centre"]) if centre else None
        if axis == "y":
            table = (C.c_float * 256)(*(case["table_values"] or [0.0] * 256))
            lib.tpw_sd_rotate_y.argtypes = [C.c_void_p, C.c_float, C.c_void_p, C.c_void_p]
            lib.tpw_sd_rotate_y(buf, case["angle"], centre_buf, table)
        else:
            lib.tpw_sd_rotate_z.argtypes = [C.c_void_p, C.c_float]
            lib.tpw_sd_rotate_z(buf, case["angle"])
        host = [canon(w) for w in struct.unpack("<16I", bytes(buf))]
        return [] if after == host else [f"vertices differ: ppc={[hex(w) for w in after]} host={[hex(w) for w in host]}"]


class LeafTest:
    """SFVF_TLCTEX::ReturnPtr, SetUV, SetDiffuse and SetVertex on a block of eight records (0x20 bytes each): a byte for byte
    comparison of the block afterwards, and ReturnPtr's answer as an offset."""

    def __init__(self, which):
        self.which = which
        self.name = {"ret": "SFVF_TLCTEX::ReturnPtr", "uv": "SFVF_TLCTEX::SetUV", "diffuse": "SFVF_TLCTEX::SetDiffuse",
                     "vertex": "SFVF_TLCTEX::SetVertex"}[which]

    def edge_cases(self):
        r = random.Random(21)
        if self.which == "ret":
            for size in (0x20, 0x10, 0, 1, -0x20, 0x7FFFFFFF, -0x80000000, 0x10000):
                for index in (0, 1, 2, 3, -1, -2, 0x10000, 0x7FFFFFFF, -0x80000000):
                    yield dict(a=size, b=index)
            for _ in range(4):
                yield dict(a=0x20, b=1)
        elif self.which == "uv":
            for set_ in (0, 1, 2, 3, -1, -2, 7):
                for u, v in ((0.0, 1.0), (-0.0, NAN), (0.5, -0.5), (INF, -INF), (1e-40, 1e30)):
                    yield dict(set=set_, u=u, v=v, base=r.choice([0, 1, 2]))
        elif self.which == "diffuse":
            for colour in (0, 1, MASK, 0x80000000, 0xFF00FF00, 0x12345678):
                yield dict(colour=colour, base=r.choice([0, 1, 2]))
        else:
            for _ in range(10):
                yield dict(vertex=[rfl(r, 0.4, -9, 9) for _ in range(4)], base=r.choice([0, 1, 2]))

    def random_case(self, r):
        if self.which == "ret":
            return dict(a=r.choice([0x20, 0x20, 0x10, r.randint(-1 << 31, (1 << 31) - 1), r.randint(-64, 64)]),
                        b=r.choice([1, 0, 2, r.randint(-1 << 31, (1 << 31) - 1), r.randint(-64, 64)]))
        if self.which == "uv":
            return dict(set=r.choice([0, 0, 0, 1, 2, -1, r.randint(-3, 3)]), u=rfl(r, 0.2), v=rfl(r, 0.2), base=r.choice([0, 1, 2]))
        if self.which == "diffuse":
            return dict(colour=r.getrandbits(32), base=r.choice([0, 1, 2]))
        return dict(vertex=[rfl(r, 0.2, -50, 50) for _ in range(4)], base=r.choice([0, 1, 2]))

    def run_case(self, runner, case):
        e, lib = runner.emu, runner.lib
        e.reset_scratch()
        which = self.which
        # A block of 0x300 bytes: zeros, 0x100 bytes of a pattern, zeros; the record is somewhere in the pattern.
        original = bytes(0x80) + bytes((7 * i + 3) & 0xFF for i in range(0x100)) + bytes(0x80) + bytes(0x80)
        base = 0x80 + 0x20 * case.get("base", 0)
        ppc_block = e.alloc(0x300, original)
        host_buffer = C.create_string_buffer(original, 0x300)
        ppc_this, host_this = ppc_block + base, C.addressof(host_buffer) + base
        if which == "ret":
            a, b = T.s32(case["a"]), T.s32(case["b"])
            r3 = T.call_ppc(e, RET_PTR, [ppc_this, T.I(a), T.I(b)])[0]
            lib.tpw_sd_return_ptr.restype = C.c_int64
            lib.tpw_sd_return_ptr.argtypes = [C.c_void_p, C.c_int32, C.c_int32]
            h = lib.tpw_sd_return_ptr(host_this, a, b)
            offset = T.s32(r3 - ppc_this)
            return [] if offset == h else [f"ReturnPtr: ppc offset {offset} host {h}"]
        if which == "uv":
            T.call_ppc(e, SET_UV, [ppc_this, T.I(case["set"]), T.F(case["u"]), T.F(case["v"])])
            lib.tpw_sd_set_uv.argtypes = [C.c_void_p, C.c_int32, C.c_float, C.c_float]
            lib.tpw_sd_set_uv(host_this, case["set"], case["u"], case["v"])
        elif which == "diffuse":
            T.call_ppc(e, SET_DIFFUSE, [ppc_this, T.I(case["colour"])])
            lib.tpw_sd_set_diffuse.argtypes = [C.c_void_p, C.c_uint32]
            lib.tpw_sd_set_diffuse(host_this, case["colour"])
        else:
            words = [fb(v) for v in case["vertex"]]
            T.call_ppc(e, SET_VERTEX, [ppc_this] + [T.I(w) for w in words])
            lib.tpw_sd_set_vertex.argtypes = [C.c_void_p, C.c_void_p]
            lib.tpw_sd_set_vertex(host_this, (C.c_uint32 * 4)(*words))
        ppc_after, host_after = e.read(ppc_block, 0x300), bytes(host_buffer.raw)
        # The pattern is bytes, so an untouched word is the same bytes on both sides; a word that was written holds a number,
        # big-endian on the PowerPC side and little-endian on the host.
        diffs = []
        for i in range(0, 0x300, 4):
            before = original[i:i + 4]
            p_changed, h_changed = ppc_after[i:i + 4] != before, host_after[i:i + 4] != before
            if p_changed != h_changed:
                diffs.append(f"word at {i:#x} changed on one side only (ppc {p_changed})")
            elif p_changed:
                pv, hv = struct.unpack(">I", ppc_after[i:i + 4])[0], struct.unpack("<I", host_after[i:i + 4])[0]
                if canon(pv) != canon(hv):
                    diffs.append(f"word at {i:#x}: ppc={pv:#x} host={hv:#x}")
        return diffs


# ----------------------------------------------------------------- the case description


def blank_record(vdata=0, idata=0):
    """A primitive record's 13 words: flags, blending mode, texture marker, two filler words, type, format, vertex count, index
    count, vertex data offset, index data offset, depth bits, last field."""
    return [0, 0, 0, 0xA5A5A5A5, 0xA5A5A5A5, 0, 0, 0, 0, vdata, idata, 0, 0]


def blank_spriterec():
    """A sprite record's 11 words: flags, vertices (an offset in the input), colour, blending mode, texture marker, filler, u0, v0,
    u1, v1, last word."""
    return [0, 0, MASK, 0, 0, 0xA5A5A5A5, fb(0.0), fb(0.0), fb(1.0), fb(1.0), 0]


def blank_entry():
    return dict(flags=0, marker=0, uoff=0.0, voff=0.0, usize=0.0, vsize=0.0, data=0)


def blank_engine():
    """texture (0 none, 1..6 an entry, 100 + k the array of slot k), data (a pixel block), aspect, width, height, used width, used
    height, x offset, y offset or the part count."""
    return [0, 0, fb(1.0), 32, 32, 32, 32, -16, -16]


def blank_array():
    return [0] + [0, 0, 0] * MAX_PART


def blank_sprite():
    """x, y, depth, w, flags, position x y z, filler 0x20, colour, engine, size, angle word, used width, used height, used x, used y,
    filler 0x38, filler 0x3c, mode (1: the field at 0x30 holds the used width and height, 0: the angle)."""
    return [fb(0.0), fb(0.0), fb(1.0), fb(1.0), 0x10000000, fb(0.0), fb(0.0), fb(0.0), 0xA5A5A5A5, MASK, 1, fb(1.0), fb(0.0), 0, 0, 0, 0,
            0xA5A5A5A5, 0xA5A5A5A5, 0]


def new_case(**kw):
    ident = [fb(1.0 if i % 5 == 0 else 0.0) for i in range(16)]
    c = dict(ops=[], disp=0, aspect=fb(0.75), f938=fb(0.5), hotx=fb(0.0), hoty=fb(0.0), hot=0, renderer=3, view_angle=fb(0.0),
             light=MASK, matrix=ident,
             field=dict(present=0, columns=4, rows=4, invx=fb(1.0), invz=fb(1.0), ox=fb(0.0), oz=fb(0.0), cx=fb(1.0), cz=fb(1.0),
                        heights=[fb(float(i % 7)) for i in range(HEIGHTS)]),
             draw=dict(vsize=0x20, current=0, vend=VERTEX_BYTES, iend=INDEX_ENTRIES * 2, next=0),
             recs=[blank_record(vdata=k * 0x800, idata=k * 0x400) for k in range(RECORDS)], vfill=0xA5A5A5A5, ifill=0xA5A5,
             inp=[0] * INPUT_WORDS, sprite_rec=blank_spriterec(), scratch_rec=blank_spriterec(), scratch_verts=[0xA5A5A5A5] * 16,
             sine=[fb(v) for v in sine_table("sine")],
             entries=[blank_entry() for _ in range(ENTRIES)], engines=[blank_engine() for _ in range(ENGINES)],
             arrays=[blank_array() for _ in range(ARRAYS)], sprites=[blank_sprite() for _ in range(SPRITES)],
             lists=[[0, 0, 0, 0, 0] for _ in range(LISTS)], blob=0, q={})
    c.update(kw)
    return c


def flat(c):
    d = [len(c["ops"])]
    for op in c["ops"]:
        d += list(op) + [0] * (8 - len(op))
    d += [c["disp"], c["aspect"], c["f938"], c["hotx"], c["hoty"], c["hot"], c["renderer"], c["view_angle"], c["light"]] + list(c["matrix"])
    f = c["field"]
    d += [f["present"], f["columns"], f["rows"], f["invx"], f["invz"], f["ox"], f["oz"], f["cx"], f["cz"]] + list(f["heights"])
    dr = c["draw"]
    d += [dr["vsize"], dr["current"], dr["vend"], dr["iend"], dr["next"]]
    for record in c["recs"]:
        d += record
    d += [c["vfill"], c["ifill"]] + c["inp"] + c["sprite_rec"] + c["scratch_rec"] + c["scratch_verts"] + c["sine"]
    for en in c["entries"]:
        d += [en["flags"], en["marker"], fb(en["uoff"]), fb(en["voff"]), fb(en["usize"]), fb(en["vsize"]), en["data"]]
    for engine in c["engines"]:
        d += engine
    for array in c["arrays"]:
        d += array
    for sprite in c["sprites"]:
        d += sprite
    for lst in c["lists"]:
        d += lst
    d += [c["blob"]]
    return d


def sections(data):
    """Splits one operation's state (little-endian words as bytes) into its named sections."""
    out, at = {}, 0
    for name, n in SECTIONS:
        out[name] = data[at:at + 4 * n]
        at += 4 * n
    return out


def vertex_size_of(fmt):
    return {0xC4: 0x18, 0x1C4: 0x20, 0x2C4: 0x28, 0x3C4: 0x30}.get(fmt, 0)


# ----------------------------------------------------------------- the PowerPC side


class PpcWorld:
    """One case on the PowerPC side: the world laid out as the Mac does, the fakes, the operations and the state."""

    def __init__(self, e, case):
        self.e = e
        c = self.case = case
        e.sd_side = self
        self.vtx = e.alloc(VERTEX_BYTES)
        self.idx = e.alloc(INDEX_ENTRIES * 2)
        self.inp = e.alloc(INPUT_WORDS * 4)
        self.recs = e.alloc(RECORDS * RECORD_SIZE)
        self.draw = e.alloc(0x40, b"\xa5" * 0x40)
        self.sprite_rec = e.alloc(0x2C)
        self.scratch_rec = e.alloc(0x2C)
        self.scratch_v = e.alloc(0x40)
        self.sprites = [e.alloc(SPRITE_SIZE, b"\xa5" * SPRITE_SIZE) for _ in range(SPRITES)]
        self.engines = [e.alloc(ENGINE_SIZE, b"\xa5" * ENGINE_SIZE) for _ in range(ENGINES)]
        self.entries = [e.alloc(ENTRY_SIZE, b"\xa5" * ENTRY_SIZE) for _ in range(ENTRIES)]
        self.lists = [e.alloc(0x28, b"\xa5" * 0x28) for _ in range(LISTS)]
        self.list_arrays = [e.alloc(16) for _ in range(LISTS)]
        self.manager = e.alloc(0x2410)
        self.hfield = e.alloc(0x40, b"\xa5" * 0x40)
        self.hrec = e.alloc(0x30, b"\xa5" * 0x30)
        self.heights = e.alloc(HEIGHTS * 4)
        self.matrix = e.alloc(0x40)
        self.cache, self.render, self.light, self.clipping = (e.alloc(0x10, b"\xa5" * 0x10) for _ in range(4))
        self.pixel = e.alloc(16)
        self.pages = [PAGE_BASE + 0x100 * k for k in range(1, MARKERS + 1)]
        self.arrays = [None] * ARRAYS                  # slot -> dict(block, first, count) while the array is alive
        self.heap = [0, 0, 0, 0]
        self.next_record = c["draw"]["next"]
        fixed = {self.draw: L_DRAW, e.sd_math: L_MATH, self.render: L_RENDER, self.light: L_LIGHT, self.cache: L_CACHE,
                 self.matrix: L_MATRIX, self.manager: L_MANAGER, self.hfield: L_HFIELD}
        fixed.update({p: L_MARKER + i for i, p in enumerate(self.pages)})
        fixed.update({a: L_ENTRY + i for i, a in enumerate(self.entries)})
        fixed.update({a: L_SPRITE + i for i, a in enumerate(self.sprites)})
        fixed.update({a: L_ENGINE + i for i, a in enumerate(self.engines)})
        fixed.update({a: L_LIST + i for i, a in enumerate(self.lists)})
        self.L = e.cl = CL.PpcLog(fixed, c["q"])
        self.regions = [(R_VERTICES, self.vtx, VERTEX_BYTES), (R_INDICES, self.idx, INDEX_ENTRIES * 2), (R_INPUT, self.inp, INPUT_WORDS * 4),
                        (R_SCRATCH_VERTICES, self.scratch_v, 0x40), (R_SCRATCH_RECORD, self.scratch_rec, 0x2C)]
        self.regions += [(L_SPRITE + i, a, SPRITE_SIZE) for i, a in enumerate(self.sprites)]
        self.build()

    # -- addresses

    def page(self, number):
        return self.pages[number - 1] if 1 <= number <= MARKERS else 0

    def marker(self, address):
        return self.pages.index(address) + 1 if address in self.pages else (0 if address == 0 else -1)

    def vertex_at(self, offset):
        return 0 if offset == NO_OFFSET else (self.vtx + T.s32(offset)) & MASK

    def index_at(self, offset):
        return 0 if offset == NO_OFFSET else (self.idx + T.s32(offset)) & MASK

    def record(self, number):
        return self.recs + (number - 1) * RECORD_SIZE if 1 <= number <= RECORDS else 0

    def record_id(self, address):
        if address == 0:
            return 0
        offset = address - self.recs
        if offset < 0 or offset >= RECORDS * RECORD_SIZE or offset % RECORD_SIZE:
            return -1
        return offset // RECORD_SIZE + 1

    def offset_in(self, base, address):
        return NO_OFFSET if address == 0 else (address - base) & MASK

    def where(self, address):
        if address == 0:
            return 0
        for label, base, size in self.regions:
            if base <= address < base + size:
                return (label << 16) + (address - base)
        return -1

    def sprite_number(self, address):
        if address == 0:
            return 0
        return self.sprites.index(address) + 1 if address in self.sprites else -1

    def engine_number(self, address):
        if address == 0:
            return 0
        return self.engines.index(address) + 1 if address in self.engines else -1

    def entry_label(self, address):
        if address == 0:
            return 0
        for a, array in enumerate(self.arrays):
            if array is not None and array["first"] <= address < array["first"] + array["count"] * ENTRY_SIZE \
                    and (address - array["first"]) % ENTRY_SIZE == 0:
                return (L_ARRAY + a) * 100 + (address - array["first"]) // ENTRY_SIZE
        return L_ENTRY + self.entries.index(address) if address in self.entries else -1

    def texture_ref(self, value):
        """An engine sprite's texture field from the description: 0, an entry (1..6) or the array of slot value - 100."""
        if value >= 100:
            array = self.arrays[value - 100]
            return array["first"] if array else 0
        return self.entries[value - 1] if 1 <= value <= ENTRIES else 0

    # -- building

    def make_array(self, slot, count, flags, pages, data):
        """What `new CTextureEntry[count]` and the fields do: the block with its cookie, the entries blank but for the flags, the
        page marker and the data pointer."""
        e = self.e
        block = e.alloc(8 + ENTRY_SIZE * count, bytes(8 + ENTRY_SIZE * count))
        ws(e, block, [ENTRY_SIZE, count])
        first = block + 8
        for i in range(count):
            a = first + i * ENTRY_SIZE
            w32(e, a + 8, flags[i])
            w32(e, a + 0x18, self.page(pages[i]))
            w32(e, a + 0x3C, self.pixel if data[i] else 0)
        self.L.labels[block] = L_ARRAY + slot
        self.arrays[slot] = dict(block=block, first=first, count=count)
        self.heap[1] += 1
        return first

    def build(self):
        e, c = self.e, self.case
        e.write(self.vtx, struct.pack(">I", c["vfill"]) * (VERTEX_BYTES // 4))
        e.write(self.idx, struct.pack(">H", c["ifill"] & 0xFFFF) * INDEX_ENTRIES)
        ws(e, self.inp, c["inp"])
        for k, w in enumerate(c["recs"]):
            ws(e, self.recs + k * RECORD_SIZE, [w[0], w[1], self.page(w[2]), w[3], w[4], 0xA5A5A5A5, w[5], w[6], w[7], w[8],
                                                 self.vertex_at(w[9]), self.index_at(w[10]), w[11], w[12]])
        dr = c["draw"]
        ws(e, self.draw + 0x18, [dr["vsize"], self.record(dr["current"]), 0, self.vertex_at(dr["vend"]), self.index_at(dr["iend"])])
        s = c["sprite_rec"]
        ws(e, self.sprite_rec, [s[0], 0 if s[1] == NO_OFFSET else self.inp + T.s32(s[1]), s[2], s[3], self.page(s[4]), s[5], s[6], s[7],
                                s[8], s[9], s[10]])
        s = c["scratch_rec"]
        ws(e, self.scratch_rec, [s[0], self.scratch_v, s[2], s[3], self.page(s[4]), s[5], s[6], s[7], s[8], s[9], s[10]])
        ws(e, self.scratch_v, c["scratch_verts"])
        ws(e, G_SINE, c["sine"])
        for en, a in zip(c["entries"], self.entries):
            w32(e, a + 8, en["flags"])
            w32(e, a + 0x18, self.page(en["marker"]))
            ws(e, a + 0x1C, [fb(en["usize"]), fb(en["vsize"]), fb(en["uoff"]), fb(en["voff"])])
            w32(e, a + 0x3C, self.pixel if en["data"] else 0)
        for slot, array in enumerate(c["arrays"]):
            count = array[0]
            if count > 0:
                n = min(count, MAX_PART)
                self.make_array(slot, n, [array[1 + 3 * i] for i in range(n)], [array[2 + 3 * i] for i in range(n)],
                                [array[3 + 3 * i] for i in range(n)])
        self.heap = [0, 0, 0, 0]
        for engine, a in zip(c["engines"], self.engines):
            w32(e, a, self.texture_ref(engine[0]))
            w32(e, a + 4, self.pixel if engine[1] else 0)
            w32(e, a + 8, engine[2])
            for off, v in zip((0xC, 0xE, 0x10, 0x12, 0x14, 0x16), engine[3:9]):
                w16(e, a + off, v)
        for sp, a in zip(c["sprites"], self.sprites):
            ws(e, a, sp[0:8])
            ws(e, a + 0x20, [sp[8], sp[9], self.engines[sp[10] - 1] if 1 <= sp[10] <= ENGINES else 0, sp[11], sp[12]])
            if sp[19]:
                w16(e, a + 0x30, sp[13])
                w16(e, a + 0x32, sp[14])
            w16(e, a + 0x34, sp[15])
            w16(e, a + 0x36, sp[16])
            ws(e, a + 0x38, [sp[17], sp[18], 0])
        self.modes = [bool(sp[19]) for sp in c["sprites"]]
        for l, (lst, a, array) in enumerate(zip(c["lists"], self.lists, self.list_arrays)):
            ws(e, a + 4, [lst[0]])
            ws(e, a + 0x14, [array])
            ws(e, array, [self.sprites[n - 1] if 1 <= n <= SPRITES else 0 for n in lst[1:5]])
        e.write(self.manager, bytes(0x2410))
        w32(e, self.manager + 0x2400, self.entries[c["blob"] - 1] if 1 <= c["blob"] <= ENTRIES else 0)
        f = c["field"]
        ws(e, self.hfield, [self.hrec, 0xA5A5A5A5, f["columns"], f["rows"], f["invx"], f["invz"]])
        ws(e, self.hrec + 8, [f["ox"], f["oz"], f["cx"], f["cz"]])
        w32(e, self.hrec + 0x28, self.heights + 4 * HEIGHTS_BEFORE)
        ws(e, self.heights, f["heights"])
        ws(e, self.matrix, c["matrix"])
        ws(e, self.light, [0, c["light"]])
        ws(e, DISPLAY, [c["disp"], c["aspect"]])
        ws(e, DISPLAY + 0x938, [c["f938"], c["hotx"], c["hoty"], 0xA5A5A5A5, self.sprites[c["hot"] - 1] if 1 <= c["hot"] <= SPRITES else 0])
        w32(e, SYSTEM + 0x1C, c["renderer"])
        w32(e, G_VIEW_ANGLE, c["view_angle"])
        e.write(G_GROUPS, bytes(0x800 * 0x14))
        for address, value in ((G_MANAGER, self.manager), (G_TCACHE, self.cache), (G_FINAL, self.matrix), (G_HFIELD, self.hfield),
                               (G_MATH, e.sd_math), (G_DRAW, self.draw), (G_LIGHT, self.light), (G_CLIPPING, self.clipping),
                               (G_RENDER, self.render), (G_FREE, e.sd_free)):
            w32(e, address, value)
        e.write(self.clipping, b"\0" * 8)

    # -- the fakes

    def transform_vertex(self, e):
        L = self.L
        matrix, out, src, kind = e.r(3), e.r(4), e.r(5), T.s32(e.r(6))
        w = rs(e, src, 3)
        L.rec("TransformVertex", (L.lab(matrix), self.where(out), self.where(src), kind, canon(w[0]), canon(w[1]), canon(w[2])))
        ws(e, out, [L.ret("TV", 0) & MASK for _ in range(4)])

    def cache_prepare(self, e):
        self.L.rec("Cache.Prepare", (self.L.lab(e.r(3)), self.entry_label(e.r(4)), cfb(e.f(1))))

    def cache_remove(self, e):
        self.L.rec("Cache.Remove", (self.L.lab(e.r(3)), self.entry_label(e.r(4))))
        e.set_r(3, 1)

    def gen_partial(self, e):
        L = self.L
        engine, x, y = e.r(3), T.s32(e.r(4)), T.s32(e.r(5))
        L.rec("GeneratePartialTexture", (L.lab(engine), x, y))
        count = T.s32(L.ret("PartCount", 0) & MASK)
        if count <= 0:
            e.set_r(3, 0)
            return
        n = min(count, MAX_PART)
        slot = self.engine_number(engine) - 1
        flags, pages = [], []
        for _ in range(n):
            flags.append(L.ret("PartFlags", 0) & MASK)
            pages.append(T.s32(L.ret("PartPage", 0) & MASK))
        first = self.make_array(slot, n, flags, pages, [0] * n)
        w32(e, engine, first)
        w16(e, engine + 0x16, n)
        e.set_r(3, 1)

    def get_next(self, e):
        L = self.L
        this, flags, ptype, fmt, mode, texture = e.r(3), T.s32(e.r(4)), T.s32(e.r(5)), e.r(6), T.s32(e.r(7)), e.r(8)
        L.rec("GetNextPrimitive", (L.lab(this), flags, ptype, fmt, mode, self.marker(texture)))
        wanted = T.s32(L.ret("GetNextPrimitive", 0) & MASK)
        if wanted == -1:
            w32(e, this + 0x1C, 0)
            e.set_r(3, 0)
            return
        if 1 <= wanted <= RECORDS:
            record = self.record(wanted)
        else:
            k = self.next_record % RECORDS
            self.next_record += 1
            record = self.record(k + 1)
            w32(e, record, 0)
            ws(e, record + 0x20, [0, 0, self.vtx + k * (VERTEX_BYTES // RECORDS), self.idx + k * (INDEX_ENTRIES // RECORDS) * 2, fb(0.0)])
        w32(e, this + 0x1C, record)
        ws(e, record + 4, [mode & MASK])
        w32(e, record + 8, texture)
        ws(e, record + 0x18, [ptype & MASK, fmt])
        w32(e, record + 0x34, MASK)
        w32(e, this + 0x18, vertex_size_of(fmt))
        e.set_r(3, record)

    def execute_long(self, e):
        self.L.rec("ExecuteCurrentPrimitive", (self.L.lab(e.r(3)),))
        w32(e, e.r(3) + 0x1C, 0)
        e.set_r(3, T.u32(self.L.ret("Execute", 1)))

    def execute_short(self, e):
        this = e.r(3)
        self.L.rec("ExecuteCurrentPrimitiveDepth", (self.L.lab(this), cfb(e.f(1))))
        current = e.u32(this + 0x1C)
        if current == 0 or e.u32(current + 0x20) == 0 or e.u32(current + 0x24) == 0:
            e.set_r(3, 1)
            return
        w32(e, this + 0x1C, 0)
        e.set_r(3, T.u32(self.L.ret("ExecuteDepth", 1)))

    def save_info(self, e):
        self.L.rec("SaveInfo", (self.L.lab(e.r(3)), self.where(e.r(4)), self.where(e.r(5))))

    def fog(self, e):
        self.L.rec("CalcVertexFogIntensities", (self.L.lab(e.r(3)), self.where(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))))
        e.set_r(3, self.L.ret("Fog", 0) & 0xFF)

    def draw_shadow(self, e):
        self.L.rec("DrawRegularShadow", (self.L.lab(e.r(3)),))
        e.set_r(3, T.u32(self.L.ret("Shadow", 1)))

    def add_shadow(self, e):
        self.L.rec("AddRegularShadow", (self.L.lab(e.r(3)), self.entry_label(e.r(4))))
        e.set_r(3, T.u32(self.L.ret("AddShadow", 1)))

    def draw_partial(self, e):
        self.L.rec("DrawPartial", (self.L.lab(e.r(3)),))
        e.set_r(3, T.u32(self.L.ret("DrawPartial", 1)))

    def assertion(self, e):
        self.L.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4))))

    def free(self, e):
        self.L.rec("Free", (self.L.lab(e.u32(e.r(3))),))
        w32(e, e.r(3), 0)

    def transform(self, e):
        L = self.L
        this, matrix, out, src, out_stride, in_stride, count, kind = (e.r(3), e.r(4), e.r(5), e.r(6), T.s32(e.r(7)), T.s32(e.r(8)),
                                                                       T.s32(e.r(9)), T.s32(e.r(10)))
        L.rec("Transform", (L.lab(this), L.lab(matrix), self.where(out), self.where(src), out_stride, in_stride, count, kind))
        order = [(0, 1, 2), (1, 2, 0), (2, 0, 1), (0, 2, 1), (2, 1, 0), (1, 0, 2)]
        n = 0 if count < 0 else min(count, 16)
        words = []
        for i in range(n):
            w = rs(e, src + i * in_stride, 4)
            w[:3] = [canon(x) for x in w[:3]]   # (see tests/unit_SpriteDraw.cpp)
            words.append(w)
            L.rec("TransformIn", (canon(w[0]), canon(w[1]), canon(w[2]), canon(w[3])))
        for i in range(n):
            mode = L.ret("TransformMode", 0)
            perm, flips = order[mode % 6], (mode // 6) % 8
            result = [words[i][perm[k]] ^ (0x80000000 if (flips >> k) & 1 else 0) for k in range(3)]
            result.append(L.ret("TransformW", 0x3F800000) & MASK)
            ws(e, out + i * out_stride, result)
            L.rec("TransformOut", tuple(canon(v) for v in result))

    def project(self, e):
        L = self.L
        L.rec("Project", (L.lab(e.r(3)), self.where(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), T.s32(e.r(7))))
        e.set_f(1, bf(L.ret("Project", 0)))

    def destroy_array(self, e, call):
        """__destroy_new_array: the destructors from the last entry back, then the block."""
        base, dtor = e.r(3), e.r(4)
        size, n = e.u32(base - 8), e.u32(base - 4)
        for i in range(n - 1, -1, -1):
            call(dtor, base + i * size, 0xFFFFFFFF)
        self.heap[3] += 1
        if self.L.lab(base - 8) > 0:
            self.L.rec("delete", (self.L.lab(base - 8),))
        self.L.forget(base - 8)
        for slot, array in enumerate(self.arrays):
            if array is not None and array["block"] == base - 8:
                self.arrays[slot] = None
        return None

    # -- the operations

    def sprite_at(self, n):
        return self.sprites[n - 1]

    def do(self, op):
        e = self.e
        code, a = op[0], op[1:]
        call = lambda addr, *args: T.call_ppc(e, addr, list(args))
        if code == OP_PREPARE:
            return call(PREPARE, self.sprite_at(a[0]))[0]
        if code == OP_CALC:
            return call(CALC, self.sprite_at(a[0]), self.scratch_rec)[0]
        if code == OP_SETUP:
            return call(SETUP, self.sprite_at(a[0]), self.scratch_v, T.F(bf(a[1])), T.F(bf(a[2])))[0]
        if code == OP_DRAW:
            return call(DRAW_REGULAR, self.sprite_at(a[0]))[0]
        if code == OP_ADD:
            return call(ADD_REGULAR, self.sprite_at(a[0]))[0]
        if code == OP_ADD_SPRITE:
            return call(ADD_SPRITE, self.draw, self.sprite_rec, T.I(a[0]))[0]
        if code == OP_RESET:
            call(MGR_RESET, self.manager)
            return 0
        if code == OP_ADD_LIST:
            return call(MGR_ADD_LIST, self.manager, self.lists[a[0] - 1])[0]
        if code == OP_MPREPARE:
            return call(MGR_PREPARE, self.manager)[0]
        if code == OP_MRENDER:
            count = e.u32(self.manager + 0x2404)
            table = rs(e, self.manager, count) if 0 < count < 0x800 else []
            if len(set(table)) != len(table):   # (tests/unit_SpriteDraw.cpp says why a frame with a sprite twice is not rendered)
                return -2 & MASK
            return call(MGR_RENDER, self.manager)[0]
        if code == OP_SET_SPRITE:
            base, offset, value = self.sprite_at(a[0]), a[1], a[2] & MASK
            if offset == 0x28:
                w32(e, base + offset, self.engines[value - 1] if 1 <= value <= ENGINES else 0)
            elif offset == 0x30:
                w32(e, base + offset, value)
                self.modes[a[0] - 1] = False
            elif offset == 0x100:
                w16(e, base + 0x30, value >> 16)
                w16(e, base + 0x32, value)
                self.modes[a[0] - 1] = True
            elif offset in (0x34, 0x36):
                w16(e, base + offset, value)
            else:
                w32(e, base + offset, value)
            return 0
        if code == OP_SET_ENGINE:
            base, offset, value = self.engines[a[0] - 1], a[1], a[2] & MASK
            if offset == 0x00:
                w32(e, base, self.texture_ref(value))
            elif offset == 0x04:
                w32(e, base + 4, self.pixel if value else 0)
            else:
                w16(e, base + offset, value)
            return 0
        if code == OP_SET_DISPLAY:
            w32(e, DISPLAY, a[0])
            return 0
        if code == OP_SET_ENTRY:
            w32(e, self.entries[a[0] - 1] + 8, a[1])
            return 0
        raise ValueError(code)

    # -- the state

    def state(self, result):
        e, L = self.e, self.L
        w = [result & MASK]
        draw = rs(e, self.draw, 0x40 // 4)
        w += [draw[6], self.record_id(draw[7]) & MASK, self.offset_in(self.vtx, draw[9]), self.offset_in(self.idx, draw[10]), self.next_record]
        w.append(self.sprite_number(e.u32(DISPLAY + 0x948)) & MASK)
        for k in range(RECORDS):
            r = rs(e, self.recs + k * RECORD_SIZE, RECORD_SIZE // 4)
            w += [r[0], r[1], self.marker(r[2]) & MASK, r[3], r[4], r[6], r[7], r[8], r[9], self.offset_in(self.vtx, r[10]),
                  self.offset_in(self.idx, r[11]), canon(r[12]), r[13]]
        w += [canon(v) for v in rs(e, self.vtx, VERTEX_BYTES // 4)]
        indices = struct.unpack(">%dH" % INDEX_ENTRIES, e.read(self.idx, INDEX_ENTRIES * 2))
        packed = [indices[i] | (indices[i + 1] << 16) for i in range(0, INDEX_ENTRIES, 2)]
        for rec_address, vertices in ((self.sprite_rec, None), (self.scratch_rec, self.scratch_v)):
            r = rs(e, rec_address, 11)
            if r[1] == 0:
                v = NO_OFFSET
            elif r[1] == self.scratch_v:
                v = 0xFFFFFFFE
            else:
                v = (r[1] - self.inp) & MASK
            record = [r[0], v, r[2], r[3], self.marker(r[4]) & MASK, r[5], canon(r[6]), canon(r[7]), canon(r[8]), canon(r[9]), r[10]]
            if rec_address == self.sprite_rec:
                sprite_record = record
            else:
                scratch_record = record
        w += packed  # indices
        w += sprite_record + scratch_record
        w += [canon(v) for v in rs(e, self.scratch_v, 16)]
        for number, a in enumerate(self.sprites):
            s = rs(e, a, 0x44 // 4)
            half = struct.unpack(">HH", e.read(a + 0x34, 4))
            v30 = s[12] if self.modes[number] else canon(s[12])
            w += [canon(s[0]), canon(s[1]), canon(s[2]), canon(s[3]), s[4], canon(s[5]), canon(s[6]), canon(s[7]), s[8], s[9],
                  self.engine_number(s[10]) & MASK, canon(s[11]), v30, half[0], half[1], s[14], s[15],
                  self.sprite_number(s[16]) & MASK]
        for a in self.engines:
            en = rs(e, a, 0x18 // 4)
            h = struct.unpack(">6H", e.read(a + 0xC, 12))
            w += [self.entry_label(en[0]) & MASK, 1 if en[1] else 0, canon(en[2]), h[0], h[1], h[2], h[3], h[4], h[5]]
        for a in self.entries:
            flags = e.u32(a + 8)
            w += [flags, self.marker(e.u32(a + 0x18)) & MASK, 1 if e.u32(a + 0x3C) else 0]
        for array in self.arrays:
            if array is None:
                w += [0] * (1 + MAX_PART * 3)
                continue
            w.append(array["count"])
            for i in range(MAX_PART):
                if i < array["count"]:
                    a = array["first"] + i * ENTRY_SIZE
                    w += [e.u32(a + 8), self.marker(e.u32(a + 0x18)) & MASK, 1 if e.u32(a + 0x3C) else 0]
                else:
                    w += [0, 0, 0]
        m = self.manager
        counts = rs(e, m + 0x2404, 3)
        w += counts + [L.lab(e.u32(m + 0x2400)) & MASK]
        sprites = rs(e, m, SHOWN)
        w += [self.sprite_number(sprites[i]) & MASK if i < counts[0] else 0 for i in range(SHOWN)]
        partial = rs(e, m + 0x2000, SHOWN)
        w += [self.sprite_number(partial[i]) & MASK if i < counts[1] else 0 for i in range(SHOWN)]
        lists = rs(e, m + 0x2200, 4)
        for i in range(4):
            number = 0
            if i < counts[2] and lists[i] in self.lists:
                number = self.lists.index(lists[i]) + 1
            w.append(number)
        for g in range(SHOWN_GROUPS):
            above, below, page, first, blend = rs(e, G_GROUPS + 0x14 * g, 5)

            def index(p):
                if p == 0:
                    return 0xFFFFFFFE
                return 0xFFFFFFFF if p == MASK else ((p - G_GROUPS) // 0x14) & MASK
            w += [index(above), index(below), 0xFFFFFFFF if page == MASK else self.marker(page) & MASK,
                  self.sprite_number(first) & MASK if first != MASK else MASK, blend]
        w += self.heap
        assert len(w) == STATE_WORDS, (len(w), STATE_WORDS)
        return little(w)


def trim_queues(case):
    """The host keeps 4,096 answers of a queue and repeats the last one after them, as the PowerPC side does with its list."""
    case["q"] = {key: list(values)[:4096] for key, values in case["q"].items()}


def run_ppc(e, case):
    trim_queues(case)
    with guarded(e):
        side = PpcWorld(e, case)
        st = CL.Stubs()
        st.real = set(REAL)
        for address, fn in ((TRANSFORM_VERTEX, side.transform_vertex), (CACHE_PREPARE, side.cache_prepare), (CACHE_REMOVE, side.cache_remove),
                            (GEN_PARTIAL, side.gen_partial), (GET_NEXT, side.get_next), (EXEC_LONG, side.execute_long),
                            (EXEC_SHORT, side.execute_short), (SAVE_INFO, side.save_info), (FOG, side.fog),
                            (DRAW_SHADOW, side.draw_shadow), (ADD_SHADOW, side.add_shadow), (DRAW_PARTIAL, side.draw_partial)):
            st.internal(address, fn)
        st.imp("__assertion_failed", side.assertion)
        st.imp("__destroy_new_array", callback_stub(side.destroy_array))
        saved = st.activate(e)
        states = []
        try:
            with HardwareRounding(e, ROUNDED), StackClearedAtEntry(e, (DRAW_REGULAR, ADD_REGULAR, ADD_SPRITE)):
                for n, op in enumerate(case["ops"]):
                    stack_cleared(e)
                    side.L.rec("op", (n, op[0]))
                    states.append(side.state(T.s32(side.do(op))))
        finally:
            CL.Stubs.deactivate(e, saved)
            e.sd_side = None
        return dict(states=states, log=side.L.log)


def run_host(lib, case):
    lib.tpw_sd_run.restype = C.c_int32
    lib.tpw_sd_run.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_uint32)]
    n = len(case["ops"])
    out = (C.c_uint32 * (STATE_WORDS * max(n, 1) + 16))()
    CL.host_begin(lib, {}, case["q"])
    ran = lib.tpw_sd_run(i64(flat(case)), out)
    log = CL.host_end(lib)
    assert ran == n, (ran, n)
    raw = bytes(out)
    return dict(states=[raw[4 * STATE_WORDS * i:4 * STATE_WORDS * (i + 1)] for i in range(n)], log=log)


def compare(case, ppc, host):
    problems = []
    for i, (p, h) in enumerate(zip(ppc["states"], host["states"])):
        if p == h:
            continue
        a, b = sections(p), sections(h)
        names = [name for name, _ in SECTIONS if a[name] != b[name]]
        detail = []
        for name in names[:3]:
            x = struct.unpack("<%dI" % (len(a[name]) // 4), a[name])
            y = struct.unpack("<%dI" % (len(b[name]) // 4), b[name])
            k = next(j for j in range(len(x)) if x[j] != y[j])
            detail.append(f"{name}[{k}] ppc={x[k]:#x} host={y[k]:#x}")
        op = case["ops"][i]
        problems.append(f"op {i} ({OP_NAMES.get(op[0], op[0])} {list(op[1:4])}): state differs in {names}: " + "; ".join(detail))
        break
    difference = CL.first_difference(ppc["log"], host["log"])
    if difference:
        problems.append("log: " + difference)
    return problems


class WorldTest:
    """A group of cases on the world: a generator of edge cases and one of random cases."""

    def __init__(self, name, edge, rnd):
        self.name, self._edge, self._rnd = name, edge, rnd

    def edge_cases(self):
        return list(self._edge())

    def random_case(self, r):
        return self._rnd(r)

    def run_case(self, runner, case):
        e, lib = runner.emu, runner.lib
        e.reset_scratch()
        try:
            ppc = run_ppc(e, case)
        except ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        host = run_host(lib, case)
        return compare(case, ppc, host)


# ----------------------------------------------------------------- generators

# Sprite flags (SSpriteGameData::mF10; src/tb_sprites_draw.cpp, src/tb_sprites.cpp)
F_HIDDEN, F_OWN_BLEND, F_FLIPX, F_FLIPY_A, F_FLIPY_B, F_ROTATE, F_HOT, F_TERRAIN = 0x1, 0x4, 0x10, 0x20, 0x40, 0x80, 0x200, 0x400
F_TURN, F_ADDITIVE, F_FLIPV, F_NOCLIP, F_GROUND, F_LISTED = 0x1000, 0x2000, 0x8000, 0x40000, 0x80000, 0x10000000
F_KEYS = (0x200000, 0x20000, 0x4000, 0x10000)
# Display flags
D_FOG, D_VFOG, D_HOT, D_EACH, D_NOSHADOW, D_BLOB = 0x2000, 0x4000, 0x1000000, 0x80, 0x20000, 0x40000
# Texture entry flags
E_PAGE = 0x1000

def rand_entry(r):
    on_page = r.random() < 0.7
    flags = E_PAGE if on_page else 0
    flags |= r.choice([0, 0, 0x100, 0x200, 0x300, 0x100, 0x200])
    if r.random() < 0.15:
        flags |= r.choice([0x10000, 0x20000, 0x30000, 0x30000])
    if r.random() < 0.1:
        flags |= 0x4000000
    if r.random() < 0.15:
        flags |= 0x100000
    if r.random() < 0.08:
        flags |= r.getrandbits(32) & 0x0EFFFFFF
    return dict(flags=flags, marker=r.randint(1, MARKERS) if on_page or r.random() < 0.3 else 0, uoff=rfl(r, 0.05, 0.0, 1.0),
                voff=rfl(r, 0.05, 0.0, 1.0), usize=rfl(r, 0.05, 0.0, 0.5), vsize=rfl(r, 0.05, 0.0, 0.5), data=0)


def rand_engine(r, texture=None):
    w = r.choice([1, 2, 8, 16, 32, 64, 100, 128, 255])
    h = r.choice([1, 2, 8, 16, 32, 64, 100, 128, 255])
    uw = r.choice([w, w, w, max(1, w // 2), w + 3, 0xFFFF, 0 if not NO_NAN else 1])
    uh = r.choice([h, h, h, max(1, h // 2), h + 3, 0xFFFF, 0 if not NO_NAN else 1])
    ox = r.choice([-w // 2, -w // 2, 0, -12, r.randint(-70, 70), -1, 32767, -32768])
    oy = r.choice([-h // 2, -h // 2, 0, -27, r.randint(-70, 70), -1, 32767, -32768])
    return [r.randint(1, ENTRIES) if texture is None else texture, 0, fb(rfl(r, 0.05, 0.1, 4.0)), w, h, uw, uh, ox, oy]


def rand_array(r, count=None):
    count = r.randint(0, MAX_PART) if count is None else count
    out = [count]
    for i in range(MAX_PART):
        if i < count:
            flags = r.choice([0, E_PAGE, E_PAGE, E_PAGE | 0x100, 0x100, E_PAGE | 0x200000, 0x200000, E_PAGE | 0x4000])
            out += [flags, r.randint(0, MARKERS) if flags & E_PAGE else r.choice([0, 3]), r.choice([0, 1, 1])]
        else:
            out += [0, 0, 0]
    return out


def screen_point(r, kind="inside"):
    """x, y, depth, w of a projected sprite: inside the view, across its edge or outside."""
    w = T.f32(r.uniform(0.5, 8.0))
    if kind == "inside":
        return [T.f32(r.uniform(-0.8 * w, 0.8 * w)), T.f32(r.uniform(-0.8 * w, 0.8 * w)), T.f32(r.uniform(0.05 * w, 0.95 * w)), w]
    if kind == "edge":
        return [T.f32(r.choice([-1, 1]) * r.uniform(0.9 * w, 1.3 * w)), T.f32(r.uniform(-1.2 * w, 1.2 * w)), T.f32(r.uniform(0.05 * w, 0.95 * w)), w]
    if kind == "behind":
        return [T.f32(r.uniform(-w, w)), T.f32(r.uniform(-w, w)), T.f32(r.uniform(-w, 0.0)), w]
    return [rfl(r, 0.5, -20, 20) for _ in range(4)]


def rand_flags(r, kind=None, listed=0.92):
    flags = F_LISTED if r.random() < listed else 0
    kind = kind or r.choice(["plain"] * 6 + ["panel", "ground", "terrain"])
    if kind == "panel":
        flags |= F_NOCLIP
    elif kind == "ground":
        flags |= F_GROUND
    elif kind == "terrain":
        flags |= F_TERRAIN
    for bit, chance in ((F_FLIPX, 0.25), (F_FLIPY_A, 0.15), (F_FLIPY_B, 0.15), (F_ROTATE, 0.3), (F_HOT, 0.35), (F_TURN, 0.2),
                        (F_FLIPV, 0.2), (F_OWN_BLEND, 0.15), (F_ADDITIVE, 0.1), (F_HIDDEN, 0.04)):
        if r.random() < chance:
            flags |= bit
    for bit in F_KEYS:
        if r.random() < 0.1:
            flags |= bit
    if r.random() < 0.15:
        flags |= r.choice([F_TERRAIN, F_GROUND | F_TERRAIN, F_NOCLIP | F_TERRAIN])
    return flags


def rand_sprite(r, screen="inside", engine=None, flags=None, size=None):
    x, y, d, w = screen_point(r, screen)
    flags = rand_flags(r) if flags is None else flags
    size = T.f32(r.choice([0.05, 0.1, 0.25, 0.5, 1.0, 2.0, r.uniform(0.01, 3.0)])) if size is None else size
    angle = r.choice([0.0, 0.1, 0.25, 0.5, -0.25, 1.0, 3.14159, r.uniform(-4, 4), rfl(r, 1.0)])
    return [fb(x), fb(y), fb(d), fb(w), flags, fb(rfl(r, 0.04, -20, 20)), fb(rfl(r, 0.04, -20, 20)), fb(rfl(r, 0.04, 0.1, 20)),
            0xA5A5A5A5, r.choice([MASK, MASK, 0xFFFF0000, 0x80808080, r.getrandbits(32), 0]), r.randint(1, ENGINES) if engine is None else engine,
            fb(size), fb(angle), r.randint(0, 64), r.randint(0, 64), r.randint(-40, 40), r.randint(-40, 40), 0xA5A5A5A5, 0xA5A5A5A5, 0]


def fix_world(c, r=None, strict=True, relocate=True):
    """Makes the engine sprites and the sprites consistent: a block sprite (data) has no texture or the live array of its slot with
    the part count to match (not checked with strict off, for the cases that set both on purpose), and a sprite of a block sprite
    holds its used width and height at Mac 0x30 (so no rotation)."""
    for slot, engine in enumerate(c["engines"]):
        array = c["arrays"][slot]
        if engine[1] and strict:
            if engine[0] == 100 + slot and array[0] > 0:
                engine[8] = min(engine[8], array[0]) if engine[8] > 0 else array[0]
            else:
                engine[0], engine[8] = 0, 0
        elif engine[0] >= 100 and not engine[1]:
            engine[0] = 1
    for sprite in c["sprites"]:
        if sprite[4] & F_TERRAIN:
            if not c["field"]["present"]:
                sprite[4] &= ~F_TERRAIN
            elif r is not None and relocate:
                x, z = terrain_position(r, c["field"], edge=r.random() < 0.3)
                sprite[5], sprite[7] = fb(x), fb(z)
        if 1 <= sprite[10] <= ENGINES and c["engines"][sprite[10] - 1][1]:
            sprite[19] = 1
            sprite[4] &= ~F_ROTATE
            engine = c["engines"][sprite[10] - 1]
            if r is not None:
                sprite[13], sprite[14] = r.choice([(engine[5], engine[6]), (engine[5], engine[6]), (engine[5] + 1, engine[6]),
                                                   (engine[5], engine[6] + 2), (r.randint(0, 70), r.randint(0, 70))])
    return c


def rand_field(r, present=True):
    columns = r.randint(2, 7)
    rows = r.randint(2, min(7, 64 // columns - 1))
    cx, cz = T.f32(r.uniform(0.5, 4.0)), T.f32(r.uniform(0.5, 4.0))
    return dict(present=1 if present else 0, columns=columns, rows=rows, invx=fb(1.0 / cx), invz=fb(1.0 / cz),
                ox=fb(T.f32(r.uniform(-10, 10))), oz=fb(T.f32(r.uniform(-10, 10))), cx=fb(cx), cz=fb(cz),
                heights=[fb(rfl(r, 0.04, -5, 12)) for _ in range(HEIGHTS)])


def terrain_position(r, field, edge=False, wild=False):
    """A position (x, z) whose cell is inside the field's heights: column in 1..columns-2 and a fraction away from the
    cell's edges, or (edge) any column 0..columns-1 and a fraction of 0 or nearly 1, or (wild) a cell up to three outside the
    grid (the samples the original reads there are the 64 before and 64 after the field's own)."""
    columns, rows = field["columns"], field["rows"]
    ox, oz, cx, cz = bf(field["ox"]), bf(field["oz"]), bf(field["cx"]), bf(field["cz"])
    if wild:
        ix, iz = r.randint(-3, columns + 1), r.randint(-3, rows + 1)
        fx, fz = r.uniform(0.05, 0.95), r.uniform(0.05, 0.95)
    elif edge:
        ix, iz = r.randint(0, columns - 1), r.randint(0, rows - 1)
        fx, fz = r.choice([0.0, 0.5, 0.97]), r.choice([0.0, 0.5, 0.97])
    else:
        ix, iz = r.randint(1, max(1, columns - 2)), r.randint(1, max(1, rows - 2))
        fx, fz = r.uniform(0.05, 0.95), r.uniform(0.05, 0.95)
    return T.f32(ox + (ix + fx) * cx), T.f32(oz + (iz + fz) * cz)


def rand_display(r):
    flags = 0
    for bit, chance in ((D_FOG, 0.35), (D_VFOG, 0.5), (D_HOT, 0.4), (D_EACH, 0.2), (D_NOSHADOW, 0.3), (D_BLOB, 0.3)):
        if r.random() < chance:
            flags |= bit
    return flags | (r.getrandbits(32) & 0x00FF0200 if r.random() < 0.1 else 0)


def rand_queues(r, ops=8, n=1):
    """The queues the fakes answer from; `n` scales how many answers each has (a script uses many)."""
    q = {"TV": [fb(v) for _ in range(ops) for v in screen_point(r, r.choice(["inside", "inside", "edge", "behind", "wild"]))],
         "TransformMode": [r.randrange(48) for _ in range(8 * n)], "TransformW": [fb(T.f32(r.uniform(0.5, 4.0))) for _ in range(8 * n)],
         "Project": [fb(rfl(r, 0.1, -100.0, 100.0)) for _ in range(4 * n)], "Fog": [r.choice([0, 1, 1, 0xFF, 0x100]) for _ in range(4 * n)],
         "Execute": [r.choice([1, 1, 1, 0, 7, -1]) for _ in range(4 * n)], "ExecuteDepth": [r.choice([1, 1, 1, 1, 0, 7]) for _ in range(4 * n)],
         "GetNextPrimitive": [r.choice([0, 0, 0, 0, 0, 1, 2, -1]) for _ in range(6 * n)],
         "PartCount": [r.choice([0, 1, 2, 3, 4, 6]) for _ in range(4 * n)],
         "PartFlags": [r.choice([0, E_PAGE, E_PAGE, 0x100, E_PAGE | 0x100]) for _ in range(8 * n)],
         "PartPage": [r.randint(0, MARKERS) for _ in range(8 * n)],
         "Shadow": [r.choice([1, 1, 1, 0]) for _ in range(n)], "AddShadow": [r.choice([1, 1, 1, 0]) for _ in range(n)],
         "DrawPartial": [r.choice([1, 1, 1, 0]) for _ in range(n)]}
    return q


def rand_world(r, texture=True, field=True, queues=True):
    """A world with random contents: nothing is wrong with any of it, but not every part is used by every operation."""
    c = new_case()
    c["disp"] = rand_display(r)
    c["aspect"] = fb(T.f32(r.choice([0.75, 1.0, 1.3333, r.uniform(0.3, 2.0)])))
    c["f938"] = fb(rfl(r, 0.05, -1.0, 1.0))
    c["hotx"], c["hoty"] = fb(rfl(r, 0.05, -1.0, 1.0)), fb(rfl(r, 0.05, -1.0, 1.0))
    c["hot"] = r.choice([0, 0, 0, 1, 2, 3, 4])
    c["renderer"] = r.choice([3, 3, 4, 0, 1])
    c["view_angle"] = fb(r.choice([0.0, 0.25, 0.1, r.uniform(-1, 1), rfl(r, 1.0)]))
    c["light"] = r.choice([MASK, MASK, 0xFFFF0000, 0xFF00FF00, 0x80808080, 0x7F7F7F7F, r.getrandbits(32)])
    c["matrix"] = [fb(rfl(r, 0.03, -2.0, 2.0)) for _ in range(16)]
    c["field"] = rand_field(r, present=field)
    c["entries"] = [rand_entry(r) for _ in range(ENTRIES)]
    c["arrays"] = [rand_array(r) for _ in range(ARRAYS)]
    c["engines"] = []
    for slot in range(ENGINES):
        engine = rand_engine(r, None if texture else 0)
        array = c["arrays"][slot]
        if array[0] > 0 and r.random() < 0.5:
            engine[0], engine[1], engine[8] = 100 + slot, 1, array[0]
        elif r.random() < 0.2:
            engine[1] = 1
        c["engines"].append(engine)
    c["sprites"] = [rand_sprite(r) for _ in range(SPRITES)]
    c["lists"] = [[r.randint(0, SPRITES)] + r.sample(range(1, SPRITES + 1), SPRITES) for _ in range(LISTS)]
    c["blob"] = r.choice([0, 1, 2, 3])
    c["sine"] = [fb(v) for v in sine_table(r.choice(["sine", "sine", "random"]), r)]
    c["scratch_verts"] = [fb(rfl(r, 0.04, -5, 5)) for _ in range(16)]
    c["scratch_rec"] = [r.getrandbits(32), 0, r.getrandbits(32), r.getrandbits(32), r.randint(0, MARKERS), r.getrandbits(32),
                        fb(rfl(r, 0.1)), fb(rfl(r, 0.1)), fb(rfl(r, 0.1)), fb(rfl(r, 0.1)), r.getrandbits(32)]
    c["draw"]["vsize"] = r.choice([0x20, 0x20, 0x20, 0x18, 0x28, 0x30])
    if queues:
        c["q"] = rand_queues(r)
    return fix_world(c, r)


# ----------------------------------------------------------------- building blocks for the groups

SPRITE_FIELDS = dict(x=0, y=1, depth=2, w=3, flags=4, px=5, py=6, pz=7, colour=9, engine=10, size=11, angle=12, uw=13, uh=14, ux=15,
                     uy=16, mode=19)
FLOAT_FIELDS = ("x", "y", "depth", "w", "px", "py", "pz", "size", "angle")


def sprite_with(base=None, **kw):
    """A sprite's 20 words with fields set by name (floats given as Python floats)."""
    words = list(base or blank_sprite())
    for name, value in kw.items():
        words[SPRITE_FIELDS[name]] = fb(value) if name in FLOAT_FIELDS else value
    return words


def nextafter32(x, toward):
    """The float32 next to x in the direction of `toward`."""
    bits = T.f32_bits(T.f32(x))
    if x == toward:
        return x
    up = (toward > x)
    if x == 0.0:
        return T.bits_f32(1) if up else -T.bits_f32(1)
    if (x > 0) == up:
        return T.bits_f32(bits + 1)
    return T.bits_f32(bits - 1)


def engine_with(base=None, **kw):
    names = dict(texture=0, data=1, aspect=2, w=3, h=4, uw=5, uh=6, ox=7, oy=8)
    words = list(base or blank_engine())
    for name, value in kw.items():
        words[names[name]] = fb(value) if name == "aspect" else value
    return words


def array_with(count, flags=None, pages=None, data=None):
    flags = flags or [E_PAGE] * count
    pages = pages or [1 + i for i in range(count)]
    data = data or [1] * count
    out = [count]
    for i in range(MAX_PART):
        out += [flags[i], pages[i], data[i]] if i < count else [0, 0, 0]
    return out


def queue_tv(*points):
    return [fb(v) for point in points for v in point]


def tv_queue(c, *points):
    c["q"]["TV"] = queue_tv(*points)


# ----------------------------------------------------------------- Prepare


def prepare_regular_case(r, point, flags=F_LISTED, size=0.25, texture=1, engine=1, ops=None, **kw):
    """One regular sprite (sprite 1, engine `engine`) prepared with the projection `point` = (x, y, depth, w)."""
    c = rand_world(r)
    c["engines"][engine - 1 if engine else 0] = engine_with(texture=texture, data=0, w=32, h=32, uw=32, uh=32, ox=-16, oy=-16)
    c["sprites"][0] = sprite_with(flags=flags, engine=engine, size=size, mode=0)
    c["sprites"][1] = sprite_with(c["sprites"][1], flags=F_LISTED, engine=2, mode=0)
    c["engines"][1] = engine_with(texture=2, data=0)
    tv_queue(c, point, (0.0, 0.0, 1.0, 2.0), (1.0, 1.0, 1.0, 4.0))
    c["ops"] = ops or [(OP_PREPARE, 1)]
    c.update(kw)
    return fix_world(c)


def edge_prepare():
    r = random.Random(101)
    w, reach = 8.0, 5.0
    edge_x = w + reach
    points = [(0.0, 0.0, 4.0, w)]
    # each of the six comparisons at the edge and one step inside it
    for d in (0.0, -0.0, 1e-30, 2.0 ** -126, w, nextafter32(w, 0.0), nextafter32(w, 99.0), 9.0, -1.0):
        points.append((0.0, 0.0, d, w))
    for v in (edge_x, nextafter32(edge_x, 0.0), nextafter32(edge_x, 99.0), -edge_x, nextafter32(-edge_x, 0.0), nextafter32(-edge_x, -99.0),
              40.0, -40.0, 0.0):
        points.append((v, 0.0, 4.0, w))
        points.append((0.0, v, 4.0, w))
        points.append((v, v, 4.0, w))
    # special values in each of the four words
    for special in (NAN, INF, -INF, 1e30, -1e30, 1e-40):
        for k in range(4):
            point = [0.0, 0.0, 4.0, w]
            point[k] = special
            points.append(tuple(point))
    points += [(0.0, 0.0, 0.0, 0.0), (0.0, 0.0, -4.0, -8.0), (0.0, 0.0, 4.0, -8.0), (1.0, 1.0, 1.0, 1.0)]
    for point in points:
        for flags in (F_LISTED, 0):
            yield prepare_regular_case(r, point, flags=flags)
    # the flags and the objects around the projection
    inside = (0.0, 0.0, 4.0, w)
    for flags in (F_HIDDEN, F_HIDDEN | F_LISTED, F_NOCLIP, F_NOCLIP | F_LISTED, F_NOCLIP | F_HIDDEN, F_LISTED | F_NOCLIP | F_ROTATE,
                  F_LISTED | F_GROUND, F_LISTED | F_ROTATE | F_TURN):
        for texture in (1, 0):
            yield prepare_regular_case(r, inside, flags=flags, texture=texture)
    yield prepare_regular_case(r, inside, engine=0)
    yield prepare_regular_case(r, inside, engine=0, flags=F_HIDDEN)
    yield prepare_regular_case(r, inside, engine=0, flags=F_NOCLIP)
    yield prepare_regular_case(r, inside, texture=0)
    yield prepare_regular_case(r, inside, ops=[(OP_PREPARE, 1), (OP_PREPARE, 1)])
    yield prepare_regular_case(r, (0.0, 0.0, 0.0, 8.0), ops=[(OP_PREPARE, 1), (OP_PREPARE, 1)])
    yield prepare_regular_case(r, inside, flags=F_LISTED | F_NOCLIP, ops=[(OP_PREPARE, 1), (OP_PREPARE, 2), (OP_PREPARE, 1)])
    # sizes: the reach is 20 times the size
    for size in (0.0, -0.25, 0.25, 1.0, 100.0, NAN, INF, 1e-30):
        for point in ((2.0, 0.0, 4.0, w), (9.0, 0.0, 4.0, w), (13.5, 0.0, 4.0, w)):
            yield prepare_regular_case(r, point, size=size)
    # partial sprites
    for case in partial_prepare_cases(r):
        yield case


def partial_prepare_cases(r):
    """Block sprites (engine 2, data set) prepared: array absent, present with the same used part, a different width, height or both,
    on and off the page, with no parts counted, and Generate making none, one or several parts."""
    def partial(array, sprite_used, engine_used=(32, 32), flags=F_LISTED, count=None, queues=None, ops=None, hidden=False):
        c = rand_world(r)
        slot = 1
        c["arrays"][slot] = array
        have = array[0] > 0
        c["engines"][slot] = engine_with(texture=(100 + slot) if have else 0, data=1, uw=engine_used[0], uh=engine_used[1],
                                         oy=count if count is not None else (array[0] if have else 0))
        c["sprites"][0] = sprite_with(flags=flags | (F_HIDDEN if hidden else 0), engine=2, uw=sprite_used[0], uh=sprite_used[1],
                                      ux=r.randint(-30, 30), uy=r.randint(-30, 30), mode=1)
        c["q"].update(queues or {})
        c["ops"] = ops or [(OP_PREPARE, 1)]
        return fix_world(c, strict=False)
    size_changes = [((32, 32), (32, 32)), ((16, 32), (32, 32)), ((32, 16), (32, 32)), ((16, 16), (32, 32)), ((0xFFFF, 32), (0xFFFF, 32)),
                    ((0xFFFF, 32), (65535, 32)), ((-1 & 0xFFFF, 0), (0xFFFF, 0)), ((0, 0), (0, 0)), ((33, 32), (33, 32)), ((32, 33), (32, 32))]
    for sprite_used, engine_used in size_changes:
        for count in (0, 1, 2, 4):
            if count == 0:
                arrays = [array_with(0)]
            else:
                arrays = [array_with(count), array_with(count, flags=[0] * count), array_with(count, flags=[E_PAGE, 0, E_PAGE, 0][:count], data=[1, 0, 1, 0][:count]),
                          array_with(count, flags=[E_PAGE | 0x100] * count, pages=[0] * count, data=[0] * count)]
            for array in arrays:
                for part_count in ((1, 3, 0) if array[0] == 0 or sprite_used != engine_used else (0,)):
                    yield partial(array, sprite_used, engine_used,
                                  queues={"PartCount": [part_count], "PartFlags": [E_PAGE, 0, E_PAGE | 0x100, E_PAGE], "PartPage": [2, 0, 5, 7]})
    # an array that is present with no parts counted, and with parts counted but no array
    yield partial(array_with(2), (16, 16), (32, 32), count=0)
    yield partial(array_with(0), (16, 16), (32, 32), count=3, queues={"PartCount": [2]})
    # hidden and not listed
    yield partial(array_with(2), (16, 16), hidden=True)
    yield partial(array_with(2), (32, 32), flags=0)
    # two block sprites and two calls
    yield partial(array_with(2), (16, 32), ops=[(OP_PREPARE, 1), (OP_PREPARE, 1), (OP_PREPARE, 1)], queues={"PartCount": [3, 2]})
    # more parts asked than an array holds
    yield partial(array_with(0), (16, 32), queues={"PartCount": [9]})


def random_prepare(r):
    c = rand_world(r)
    n = r.randint(1, 4)
    c["ops"] = [(OP_PREPARE, r.randint(1, SPRITES)) for _ in range(n)]
    return c



# ----------------------------------------------------------------- CalcRegular and SetupVertices


def regular_case(r, ops, sprite=None, engine=None, entry=None, display=0, point=None, field=None, **kw):
    """Sprite 1 on engine 1 on entry 1, with the pieces replaced: `sprite` (fields by name), `engine`, `entry`, the display's flags."""
    c = rand_world(r)
    engine_fields = dict(texture=1, data=0)
    engine_fields.update(engine or {})
    c["engines"][0] = engine_with(**engine_fields)
    entry_fields = dict(flags=E_PAGE, marker=2, uoff=0.25, voff=0.5, usize=0.125, vsize=0.25, data=0)
    entry_fields.update(entry or {})
    c["entries"][0] = entry_fields
    fields = dict(flags=F_LISTED, engine=1, size=1.0, mode=0, x=0.5, y=-0.25, depth=3.0, w=6.0, px=1.5, py=2.0, pz=-1.0, colour=0x80FF8040)
    fields.update(sprite or {})
    c["sprites"][0] = sprite_with(**fields)
    c["disp"] = display
    c["hot"] = 0
    if field:
        c["field"].update(field)
    c["ops"] = ops
    c.update(kw)
    return fix_world(c, relocate=False)


def terrain_sprite(r, field, **kw):
    x, z = terrain_position(r, field, edge=kw.pop("edge", False))
    return dict(px=x, pz=z, py=kw.pop("py", 2.0), **kw)


def edge_calc():
    r = random.Random(102)
    calc = [(OP_CALC, 1)]
    # the picture: no texture, off and on the page, every blending mode the entry can answer
    yield regular_case(r, calc, engine=dict(texture=0))
    yield regular_case(r, calc, sprite=dict(flags=F_LISTED | F_FLIPV), engine=dict(texture=0))
    for page in (0, E_PAGE):
        for blend in (0, 0x100, 0x200, 0x300):
            for wrap in (0, 0x10000, 0x20000, 0x30000):
                for renderer in (3, 4):
                    for extra in (0, 0x4000000, 0x100000, 0x4100000):
                        yield regular_case(r, calc, entry=dict(flags=page | blend | wrap | extra, marker=r.randint(0, MARKERS)),
                                           renderer=renderer, sprite=dict(flags=F_LISTED | r.choice([0, F_FLIPV])))
    yield regular_case(r, calc, entry=dict(flags=E_PAGE, uoff=0.0, voff=1.0, usize=-0.5, vsize=NAN))
    yield regular_case(r, calc, entry=dict(flags=E_PAGE, uoff=NAN, voff=INF, usize=1e30, vsize=-1e30))
    # not listed this frame: the assertion
    for flags in (0, F_HIDDEN, F_NOCLIP, F_GROUND, F_TERRAIN & 0, 0x0FFFFFFF):
        yield regular_case(r, calc, sprite=dict(flags=flags))
    # every flag of the position and the quad
    base = F_LISTED
    for flags in (base, base | F_HOT, base | F_FLIPX, base | F_FLIPY_A, base | F_FLIPY_B, base | F_FLIPX | F_FLIPY_A | F_ROTATE, base | F_ROTATE,
                  base | F_TURN, base | F_ROTATE | F_TURN, base | F_FLIPV, base | F_HOT | F_FLIPV | F_FLIPX,
                  base | F_NOCLIP, base | F_NOCLIP | F_FLIPX, base | F_NOCLIP | F_ROTATE | F_FLIPV, base | F_NOCLIP | F_GROUND,
                  base | F_GROUND, base | F_GROUND | F_ROTATE, base | F_GROUND | F_FLIPX | F_FLIPY_A, base | F_GROUND | F_HOT | F_TURN,
                  base | F_GROUND | F_FLIPV, 0xFFFFFFFF & ~F_TERRAIN & ~F_HIDDEN, base | F_HOT | F_ROTATE | F_TURN):
        for display in (0, D_FOG, D_FOG | D_VFOG, D_HOT):
            yield regular_case(r, calc, sprite=dict(flags=flags, angle=r.choice([0.0, 0.25, 1.0, 0.7])), display=display,
                               q=dict(rand_queues(r)))
    # the panel: z lowered only above 0.00125, y negated
    for z in (0.0, 0.00125, 0.00126, 0.001, -0.5, 5.0, NAN, INF, -INF, 1e-30, 0.0012499999720603228, 0.0012500000186264515):
        for size in (0.0, 0.5, 2.0):
            yield regular_case(r, calc, sprite=dict(flags=F_LISTED | F_NOCLIP, pz=z, size=size, py=r.choice([2.0, -0.0, 0.0, NAN])))
    # the depth push of a lit sprite
    for z in (0.0, 1.0, -1.0, 3.0, -0.999, NAN, 1e-30, 100.0, -2.0):
        yield regular_case(r, calc, sprite=dict(flags=F_LISTED | F_HOT, x=0.5, y=0.5, depth=z, w=8.0))
        yield regular_case(r, calc, sprite=dict(flags=F_LISTED | F_HOT, x=0.5, y=0.5, depth=z, w=8.0), display=D_FOG)
    # the terrain: every cell the field has, the clamps of the next column and row, the edge fractions
    for i in range(24):
        field = rand_field(r)
        sprite = terrain_sprite(r, field, edge=i % 2 == 1, flags=F_LISTED | F_TERRAIN | r.choice([0, F_HOT, F_GROUND, F_NOCLIP]))
        yield regular_case(r, calc, sprite=sprite, field=field, display=r.choice([0, D_FOG]))
    field = rand_field(r)
    field.update(columns=3, rows=2)
    for ix in range(3):
        for iz in range(2):
            for fx, fz in ((0.0, 0.0), (0.5, 0.5), (0.99, 0.0), (0.0, 0.99)):
                x = bf(field["ox"]) + (ix + fx) * bf(field["cx"])
                z = bf(field["oz"]) + (iz + fz) * bf(field["cz"])
                yield regular_case(r, calc, sprite=dict(flags=F_LISTED | F_TERRAIN, px=T.f32(x), pz=T.f32(z)), field=field)
    # a position outside the grid: the next column and row are clamped to 0 and to the grid's size
    for ix in (-3, -2, -1, 0, 1, 2, 3, 4, 5):
        for iz in (-3, -2, -1, 0, 1, 2, 3, 4):
            field = rand_field(r)
            field.update(columns=3, rows=2)
            x = bf(field["ox"]) + (ix + 0.5) * bf(field["cx"])
            z = bf(field["oz"]) + (iz + 0.5) * bf(field["cz"])
            yield regular_case(r, calc, sprite=dict(flags=F_LISTED | F_TERRAIN, px=T.f32(x), pz=T.f32(z)), field=field)
    for _ in range(30):
        field = rand_field(r)
        x, z = terrain_position(r, field, wild=True)
        yield regular_case(r, calc, sprite=dict(flags=F_LISTED | F_TERRAIN, px=x, pz=z), field=field)
    # the scratch record and vertices carry stale words
    for _ in range(6):
        yield regular_case(r, calc * 2, sprite=dict(flags=F_LISTED | F_GROUND, angle=0.3), scratch_verts=[r.getrandbits(32) for _ in range(16)])
    # two operations in a row, a change between
    yield regular_case(r, [(OP_CALC, 1), (OP_SET_SPRITE, 1, 0x10, 0), (OP_CALC, 1), (OP_SET_SPRITE, 1, 0x10, F_LISTED | F_NOCLIP), (OP_CALC, 1)])


def random_calc(r):
    c = rand_world(r)
    c["ops"] = [(OP_CALC, r.randint(1, SPRITES)) for _ in range(r.randint(1, 3))]
    return c


def hot_case(r, w, hot_w, distance, size_xy=(8.0, 8.0), flags=F_LISTED | F_HOT, display=D_HOT, hy=0.0):
    """SetupVertices on a quad (sprite 1) 16 pixels of an image of 16, with its offset at (-8, -8) and scales `size_xy`, whose
    projected position is the origin of the screen, with the cursor at (distance, hy) and sprite 2 the hot one with w `hot_w`
    (None: no hot sprite)."""
    xs, ys = size_xy
    left = T.f32(xs * T.f32(-8 / 16))
    c = regular_case(r, [(OP_SETUP, 1, fb(xs), fb(ys))], sprite=dict(flags=flags, x=0.0, y=0.0, depth=2.0, w=w), display=display,
                     hotx=fb(distance), hoty=fb(hy), scratch_verts=[fb(-left), fb(0.0), fb(2.0), fb(w)] * 4,
                     engine=dict(ox=-8, oy=-8, w=16, h=16, uw=16, uh=16))
    if hot_w is not None:
        c["sprites"][1] = sprite_with(c["sprites"][1], w=hot_w)
        c["hot"] = 2
    return c


def edge_setup():
    r = random.Random(103)

    def setup_op(xs=0.0, ys=0.0):
        return [(OP_SETUP, 1, fb(xs), fb(ys))]
    scratch = [fb(v) for v in (1.0, 2.0, 3.0, 4.0)] * 4
    # scales: zero means 20 times the size (and the aspect)
    for xs, ys in ((0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 2.0), (-1.0, 3.0), (-0.0, -0.0), (NAN, 1.0), (1.0, NAN), (INF, 1.0), (1e30, 1e30),
                   (1e-30, 1e-30), (0.5, -0.5), (0.0, NAN), (NAN, 0.0)):
        for size in (0.0, 0.25, 1.0, 10.0):
            yield regular_case(r, setup_op(xs, ys), sprite=dict(size=size), scratch_verts=scratch)
    # every flag
    base = F_LISTED
    for flags in (base, base | F_FLIPX, base | F_FLIPY_A, base | F_FLIPY_B, base | F_FLIPY_A | F_FLIPY_B, base | F_FLIPX | F_FLIPY_A,
                  base | F_ROTATE, base | F_TURN, base | F_ROTATE | F_TURN, base | F_GROUND, base | F_GROUND | F_FLIPX,
                  base | F_GROUND | F_FLIPY_B, base | F_GROUND | F_ROTATE, base | F_GROUND | F_FLIPX | F_FLIPY_A | F_ROTATE,
                  base | F_GROUND | F_TURN, base | F_HOT, base | F_HOT | F_GROUND, base | F_ROTATE | F_FLIPX | F_HOT | F_TURN, 0x7FFFFFFF & ~F_TERRAIN):
        for angle in (0.0, 0.25, 0.5, -0.25, 1.0, 3.14159, 100.0, NAN):
            for view in (0.0, 0.25):
                yield regular_case(r, setup_op(1.0, 2.0), sprite=dict(flags=flags, angle=angle, size=1.0), view_angle=fb(view),
                                   scratch_verts=[fb(v) for v in (1.0, 2.0, 3.0, 4.0)] * 4)
    # the image: used part, size and offset
    for engine in (dict(uw=0, uh=0), dict(uw=0), dict(uh=0), dict(uw=1, uh=1, w=1, h=1, ox=0, oy=0), dict(ox=32767, oy=-32768),
                   dict(ox=-32768, oy=32767, w=65535, h=65535), dict(uw=65535, uh=65535, w=65535, h=65535), dict(w=0, h=0),
                   dict(ox=-7, oy=-11, w=17, h=19, uw=13, uh=23)):
        for flags in (F_LISTED, F_LISTED | F_FLIPX, F_LISTED | F_GROUND):
            yield regular_case(r, setup_op(1.5, 2.5), engine=engine, sprite=dict(flags=flags), scratch_verts=scratch)
    # the hot sprite: the sprite's projected position ((x + left) / w, y / w) is (0, 0) and the cursor is `distance` away to the right;
    # the quad is 8 across and high, so the sprite is hot when the distance is under 0.16 (0.0256 squared); the other hot sprite
    # (sprite 2) has a w of its own, or there is none
    for hot_w, w in ((None, 4.0), (8.0, 4.0), (4.0, 4.0), (2.0, 4.0), (NAN, 4.0), (8.0, NAN), (INF, 4.0), (8.0, 0.0), (-2.0, -4.0)):
        for distance in (0.0, 0.1, 0.15, 0.1599, 0.16, 0.1601, 0.2, 1.0, NAN):
            for display, flags in ((D_HOT, F_LISTED | F_HOT), (D_HOT, F_LISTED), (0, F_LISTED | F_HOT), (D_HOT | D_FOG, F_LISTED | F_HOT)):
                yield hot_case(r, w, hot_w, distance, display=display, flags=flags)
    for hy in (0.1, -0.1, 0.16, NAN):
        yield hot_case(r, 4.0, 8.0, 0.0, hy=hy)
        yield hot_case(r, 4.0, None, 0.1, hy=hy)
    # the size test at its edge: the squared distance of the cursor against the squared size times 0.0002
    for sx, sy in ((8.0, 8.0), (16.0, 8.0), (1.0, 1.0), (0.5, 0.0), (0.0, 0.5)):
        for distance in (0.0, 0.01, 0.02, 0.0283, 0.0284, 0.03, 0.05, 0.1, 0.11, 0.16, 0.17, 0.23, 0.2263, 0.2264, 1.0):
            yield hot_case(r, 4.0, None, distance, size_xy=(sx, sy))
    # the hot sprite test's two fused multiply-adds, at a tie the unfused sums break the other way (inputs found by
    # a one-off script the size side (dx, dy across, the squared distance a square) and the
    # distance side (the cursor at (ox, oy), the squared size a square)
    for dx, dy, hx, hy in ((0.8078798651695251, 1.7658287286758423, 0.0, 0.027462048456072807),
                           (0.0, 15.21757984161377, 0.027594134211540222, 0.2134326845407486)):
        yield regular_case(r, setup_op(dx, dy), sprite=dict(flags=F_LISTED | F_HOT, x=0.0, y=0.0, depth=2.0, w=1.0), display=D_HOT,
                           hotx=fb(hx), hoty=fb(hy), scratch_verts=[fb(0.0), fb(0.0), fb(2.0), fb(1.0)] * 4,
                           engine=dict(ox=0, oy=0, w=1 if dx else 0, h=1, uw=1, uh=1))
    # the ground lifts the corners by the display's +0x938
    for f938 in (0.5, 0.0, -1.0, NAN, 1e30):
        yield regular_case(r, setup_op(2.0, 2.0), sprite=dict(flags=F_LISTED | F_GROUND), f938=fb(f938), scratch_verts=scratch)
    # the corners as they come from CalcRegular and as random words
    for _ in range(10):
        yield regular_case(r, setup_op(rfl(r, 0.1), rfl(r, 0.1)), sprite=dict(flags=rand_flags(r) & ~F_TERRAIN | F_LISTED),
                           scratch_verts=[fb(rfl(r, 0.1, -5, 5)) for _ in range(16)])


def random_setup(r):
    c = rand_world(r)
    c["ops"] = [(OP_SETUP, r.randint(1, SPRITES), fb(r.choice([0.0, 0.0, rfl(r, 0.1, -4, 4)])), fb(r.choice([0.0, 0.0, rfl(r, 0.1, -4, 4)])))
                for _ in range(r.randint(1, 3))]
    return c



# ----------------------------------------------------------------- AddSprite, DrawRegular and AddRegular


def corner_words(r, kind, count=4, stride=0x10):
    """The words of `count` corners (x, y, z, w) `stride` bytes apart."""
    words = []
    for _ in range(count):
        if kind == "inside":
            w = T.f32(r.uniform(0.5, 3.0))
            v = [T.f32(r.uniform(-w, w)), T.f32(r.uniform(-w, w)), T.f32(r.uniform(0.0, w)), w]
        elif kind == "outside":
            w = T.f32(r.uniform(0.5, 2.0))
            v = [T.f32(r.uniform(w * 1.01, w * 4)), T.f32(r.uniform(-w, w)), T.f32(r.uniform(0.0, w)), w]
        elif kind == "mixed":
            w = T.f32(r.uniform(0.5, 2.0))
            v = [T.f32(r.uniform(-2 * w, 2 * w)), T.f32(r.uniform(-2 * w, 2 * w)), T.f32(r.uniform(-w, 2 * w)), w]
        elif kind == "behind":
            w = T.f32(r.uniform(0.5, 2.0))
            v = [T.f32(r.uniform(-w, w)), T.f32(r.uniform(-w, w)), T.f32(r.uniform(-2 * w, -0.01)), w]
        elif kind == "negative_w":
            w = -T.f32(r.uniform(0.5, 2.0))
            v = [T.f32(r.uniform(-2 * w, 2 * w)), T.f32(r.uniform(-2 * w, 2 * w)), T.f32(r.uniform(w, 2 * w)), w]
        else:
            v = [rfl(r, 0.6, -3, 3) for _ in range(4)]
        words += [fb(x) for x in v] + [r.getrandbits(32) for _ in range(stride // 4 - 4)]
    return words


def addsprite_case(r, kind="inside", mode=-1, flags=0, display=0, current=1, counts=(0, 0), vsize=0x20, blend=None, ops=None, offset=0,
                   tight=None, queues=None, texture=None, colour=None, **kw):
    """AddSprite on the sprite record (corners of `kind`) with the draw object holding record `current` (0: none) with `counts`
    vertices and indices; tight: (vertex bytes, index bytes) of room beyond what is needed (None: roomy)."""
    c = rand_world(r)
    inp = [0] * INPUT_WORDS
    words = corner_words(r, kind)
    inp[offset // 4:offset // 4 + 16] = words
    c["inp"] = inp
    c["sprite_rec"] = [flags, offset, MASK if colour is None else colour, r.getrandbits(32) & 0x0F3FFFFF if blend is None else blend,
                       r.randint(0, MARKERS) if texture is None else texture, r.getrandbits(32), fb(0.125), fb(0.25), fb(0.75), fb(0.875),
                       r.choice([0, 2, 1, 3])]
    c["disp"] = display
    c["draw"].update(vsize=vsize, current=current)
    if current:
        rec = c["recs"][current - 1]
        rec[0] = r.choice([0, 0, 0x21, 0x400])
        rec[1] = r.getrandbits(32) & 0x0FFFFFFF
        rec[9], rec[10] = (current - 1) * 0x800 + r.choice([0, 0, 0x40]), (current - 1) * 0x400 + r.choice([0, 0, 2])
        # The corners go 0x20 apart whatever the object's vertex size is, and the room test uses the object's, so counts that
        # the test lets through can write past the memories; keep what three sprites (and the clipper's work space) write inside.
        most_vertices = (VERTEX_BYTES - rec[9]) // 0x20 - 24
        most_indices = (INDEX_ENTRIES * 2 - rec[10]) // 2 - 50
        rec[7], rec[8] = min(counts[0], most_vertices), min(counts[1], most_indices)
    if tight is not None:
        base = c["recs"][current - 1] if current else None
        vdata, idata, vcount, icount = (base[9], base[10], base[7], base[8]) if base else (0, 0, 0, 0)
        c["draw"]["vend"] = max(0, min(VERTEX_BYTES, vdata + vsize * (vcount + 4) + tight[0]))
        c["draw"]["iend"] = max(0, min(INDEX_ENTRIES * 2, idata + (icount + 6) * 2 + tight[1]))
    c["ops"] = ops or [(OP_ADD_SPRITE, mode)]
    q = rand_queues(r)
    q.update(queues or {})
    c["q"] = q
    c.update(kw)
    return c


def edge_addsprite():
    r = random.Random(104)
    kinds = ("inside", "mixed", "outside", "behind", "negative_w", "bits")
    modes = (-1, -3, 0, 0x3F, 1, 2, 4, 8, 0x10, 0x20, 0x15, 0x2A, -2, -5, 0x7FFFFFFF)
    for kind in kinds:
        for mode in modes:
            for flags in (0, 2):
                yield addsprite_case(r, kind, mode, flags=flags)
    # the primitive: present with counts, a new one by default, none to give, a named one
    for kind in ("inside", "mixed"):
        for current in (1, 2, 4, 0):
            for counts in ((0, 0), (4, 6), (5, 15), (17, 9), (60, 500)):
                for queues in ({}, {"GetNextPrimitive": [-1]}, {"GetNextPrimitive": [3]}):
                    if current or queues or counts == (0, 0):
                        yield addsprite_case(r, kind, -1, flags=r.choice([0, 2]), current=current, counts=counts, queues=queues)
    for vsize in (0x18, 0x20, 0x28, 0x30):
        for kind in ("inside", "mixed"):
            yield addsprite_case(r, kind, -1, vsize=vsize, counts=(3, 9))
    # the room: exactly enough, one too little (a byte, an index), for the inside and the clipped sprite
    for kind in ("inside", "mixed"):
        for dv in (-1, 0, 1, -32, 32, -4 * 32, 3, -0x10):
            for di in (-2, 0, -1, 2, -12, 12):
                yield addsprite_case(r, kind, -1, current=1, counts=(r.choice([0, 3, 8]), r.choice([0, 6, 12])), tight=(dv, di))
    for dv in (-1, 0, 5):
        yield addsprite_case(r, "inside", -1, current=0, tight=None)
    # the fog: the switches, the answers, the projection
    for kind in ("inside", "mixed"):
        for display in (0, D_FOG, D_VFOG, D_FOG | D_VFOG, 0x6000 | 0x1000):
            for fog in (0, 1, 0x100, 0xFF, 0x1FF):
                for blend in (0, 0x100000, 0x80000, 0x180000, 0x1256):
                    yield addsprite_case(r, kind, -1, display=display, blend=blend, queues={"Fog": [fog], "Project": [fb(r.choice([0.0, 1.5, -2.25, 1e30, NAN]))]})
    # the light mask and the colour
    for light in (MASK, 0, 0x80808080, 0xFF00FF00, 0x00FFFFFF):
        for colour in (MASK, 0, 0x12345678, 0x80000000):
            yield addsprite_case(r, "inside", -1, colour=colour, light=light)
    # offsets of the corners in the input, a texture that is none, page 8
    for offset in (0, 4, 0x40, 0x100):
        yield addsprite_case(r, "mixed", -1, offset=offset)
    for texture in (0, 1, 8):
        yield addsprite_case(r, "inside", -1, texture=texture, current=0)
    # several in a row make a batch; one that is clipped away adds nothing
    for kinds_ in (("inside", "inside", "inside"), ("inside", "mixed", "outside", "inside"), ("mixed", "mixed"), ("outside", "outside")):
        c = addsprite_case(r, kinds_[0], -1, current=0, ops=[(OP_ADD_SPRITE, -1)] * len(kinds_))
        yield c
    # the sprite record's winding flag with the corners clipped
    for flags in (2, 3, 0xFFFFFFFF, 0x400, 0xFFFFFFFD):
        for kind in ("inside", "mixed"):
            yield addsprite_case(r, kind, -1, flags=flags)


def random_addsprite(r):
    kind = r.choice(["inside", "inside", "mixed", "mixed", "outside", "behind", "negative_w", "bits"])
    counts = r.choice([(0, 0), (0, 0), (4, 6), (17, 9), (60, 500), (r.randint(0, 100), r.randint(0, 600))])
    current = r.choice([1, 1, 2, 3, 4, 0])
    tight = r.choice([None, None, None, (r.choice([-1, 0, 1, -32, 32]), r.choice([-2, 0, 2, -12, 12]))])
    return addsprite_case(r, kind, r.choice([-1, -1, -3, 0, 0x3F, r.randint(-5, 80)]), flags=r.choice([0, 0, 2, 3, r.getrandbits(32)]),
                          display=rand_display(r), current=current, counts=counts, vsize=r.choice([0x20, 0x20, 0x20, 0x18, 0x28, 0x30]),
                          tight=tight, offset=r.choice([0, 0, 4, 0x40]), light=r.choice([MASK, MASK, 0x80808080, r.getrandbits(32)]),
                          ops=[(OP_ADD_SPRITE, r.choice([-1, -3, 0x3F]))] * r.randint(1, 3))


def draw_add_case(r, op, point, size=0.05, flags=F_LISTED, display=0, count=1, current=0, queues=None, engine=None, entry=None, **kw):
    """DrawRegular or AddRegular on sprite 1 at the projection `point` = (x, y, depth, w) (a flat panel and a sprite on the ground use
    the position instead), `count` times in a row."""
    sprite = dict(flags=flags, x=point[0], y=point[1], depth=point[2], w=point[3], size=size, px=0.5, py=1.0, pz=0.25, mode=0)
    c = regular_case(r, [(op, 1)] * count, sprite=sprite, display=display, engine=engine, entry=entry)
    c["draw"]["current"] = current
    if current:
        rec = c["recs"][current - 1]
        rec[9], rec[10] = (current - 1) * 0x800, (current - 1) * 0x400
    c["q"].update(queues or {})
    c.update(kw)
    return c


def edge_draw_add():
    with no_nan():
        yield from edge_draw_add_cases()


def edge_draw_add_cases():
    r = random.Random(105)
    points = {"inside": (0.0, 0.0, 4.0, 8.0), "left": (-7.9, 0.0, 4.0, 8.0), "right": (8.5, 0.5, 4.0, 8.0), "far": (100.0, 0.0, 4.0, 8.0),
              "behind": (0.0, 0.0, -1.0, 8.0), "top": (0.0, 7.0, 7.9, 8.0), "wild": (1e5, -1e5, 4.0, 8.0),
              "beyond": (0.0, 0.0, 8.5, 8.0)}
    for op in (OP_DRAW, OP_ADD):
        for name, point in points.items():
            for size in (0.05, 0.5, 2.0):
                for display in (0, D_FOG, D_FOG | D_VFOG):
                    yield draw_add_case(r, op, point, size=size, display=display)
        for flags in (F_LISTED, F_LISTED | F_NOCLIP, F_LISTED | F_GROUND, F_LISTED | F_HOT, F_LISTED | F_FLIPX | F_ROTATE, F_LISTED | F_FLIPV,
                      F_LISTED | F_GROUND | F_FLIPY_A | F_ROTATE, F_LISTED | F_NOCLIP | F_FLIPX, F_LISTED | F_FLIPY_B | F_TURN,
                      F_LISTED | F_HOT | F_TURN | F_FLIPX | F_FLIPY_A, 0):
            for size in (0.05, 1.0):
                for display in (0, D_FOG | D_VFOG):
                    yield draw_add_case(r, op, points["inside"], size=size, flags=flags, display=display, count=2,
                                        queues={"TransformMode": [r.randrange(48)], "TransformW": [fb(r.uniform(0.5, 3))]})
        for current in (0, 1, 3):
            for queues in ({}, {"GetNextPrimitive": [-1]}, {"Execute": [0]}, {"Fog": [1], "Project": [fb(2.5)]}, {"ExecuteDepth": [0]}):
                yield draw_add_case(r, op, points["inside"], display=D_FOG | D_VFOG, current=current, count=3, queues=queues)
        # a full vertex memory
        yield draw_add_case(r, op, points["inside"], count=2, vend=0x30, iend=0x20)
        yield draw_add_case(r, op, points["inside"], count=2, vend=0, iend=0)


def random_draw_add(r):
    with no_nan():
        return random_draw_add_case(r)


def random_draw_add_case(r):
    c = rand_world(r)
    c["ops"] = [(r.choice([OP_DRAW, OP_ADD]), r.randint(1, SPRITES)) for _ in range(r.randint(1, 4))]
    for sprite in c["sprites"]:
        point = screen_point(r, r.choice(["inside", "inside", "edge", "behind", "wild"]))
        sprite[0], sprite[1], sprite[2], sprite[3] = [fb(v) for v in point]
        sprite[11] = fb(T.f32(r.choice([0.02, 0.05, 0.1, 0.5, 1.0])))
        sprite[4] |= F_LISTED
        if r.random() < 0.2:
            sprite[4] &= ~F_LISTED
    c["draw"]["current"] = r.choice([0, 0, 1, 2])
    return c



# ----------------------------------------------------------------- scripts through the sprite manager


def script_world(r, length):
    """A world for a script: every regular sprite has a texture (RenderSprites reads it), terrain sprites keep their cells.
    No NaN anywhere (see no_nan)."""
    with no_nan():
        return script_world_nan_free(r, length)


def script_world_nan_free(r, length):
    c = rand_world(r, queues=False)
    c["q"] = rand_queues(r, ops=min(900, length * 4), n=max(1, length // 6))
    inp = [0] * INPUT_WORDS
    inp[0:16] = corner_words(r, r.choice(["inside", "mixed", "outside"]))
    c["inp"] = inp
    c["sprite_rec"] = [r.choice([0, 2]), 0, MASK, r.getrandbits(32) & 0x0F3FFFFF, r.randint(0, MARKERS), 0, fb(0.1), fb(0.2), fb(0.8), fb(0.9),
                       r.choice([0, 2])]
    return c


def script_frame(r):
    """One frame the way the game makes it: reset, the lists, prepare, render."""
    ops = [(OP_RESET,)]
    for l in r.sample(range(1, LISTS + 1), r.randint(0, LISTS)):
        ops.append((OP_ADD_LIST, l))
    ops += [(OP_MPREPARE,), (OP_MRENDER,)]
    return ops


def random_script(r, length=30):
    with no_nan():
        return random_script_nan_free(r, length)


def random_script_nan_free(r, length=30):
    c = script_world(r, length)
    fixed = [bool(sp[4] & F_TERRAIN) for sp in c["sprites"]]
    ops = []
    while len(ops) < length:
        k = r.random()
        n = r.randint(1, SPRITES)
        sprite = c["sprites"][n - 1]
        partial = bool(c["engines"][sprite[10] - 1][1]) if 1 <= sprite[10] <= ENGINES else False
        if k < 0.22:
            ops.append((OP_PREPARE, n))
        elif k < 0.42:
            ops += script_frame(r)
        elif k < 0.52:
            ops.append((r.choice([OP_DRAW, OP_ADD]), n))
        elif k < 0.58:
            ops.append((r.choice([OP_MPREPARE, OP_MRENDER, OP_RESET]),))
        elif k < 0.62:
            ops.append((OP_ADD_LIST, r.randint(1, LISTS)))
        elif k < 0.66:
            ops.append((OP_ADD_SPRITE, r.choice([-1, -3, 0x3F])))
        elif k < 0.70:
            ops.append((OP_SET_DISPLAY, rand_display(r)))
        elif k < 0.74:
            ops.append((OP_SET_ENTRY, r.randint(1, ENTRIES), rand_entry(r)["flags"]))
        elif k < 0.92:
            # a change to a sprite
            choice = r.randrange(9)
            if choice == 0 and not fixed[n - 1]:
                flags = rand_flags(r) & ~F_TERRAIN
                if partial:
                    flags &= ~F_ROTATE
                ops.append((OP_SET_SPRITE, n, 0x10, flags))
            elif choice == 1:
                point = screen_point(r, r.choice(["inside", "inside", "edge", "behind"]))
                for offset, v in zip((0x00, 0x04, 0x08, 0x0C), point):
                    ops.append((OP_SET_SPRITE, n, offset, fb(v)))
            elif choice == 2 and not fixed[n - 1]:
                ops.append((OP_SET_SPRITE, n, r.choice([0x14, 0x18, 0x1C]), fb(rfl(r, 0.05, -20, 20))))
            elif choice == 3:
                ops.append((OP_SET_SPRITE, n, 0x2C, fb(T.f32(r.choice([0.05, 0.25, 1.0, r.uniform(0.01, 3.0)])))))
            elif choice == 4:
                ops.append((OP_SET_SPRITE, n, 0x24, r.getrandbits(32)))
            elif choice == 5 and partial:
                ops.append((OP_SET_SPRITE, n, 0x100, (r.randint(0, 70) << 16) | r.randint(0, 70)))
            elif choice == 6 and not partial and not any(c["engines"][i][1] for i in range(ENGINES) if False):
                regular = [i + 1 for i, e in enumerate(c["engines"]) if not e[1]]
                if regular:
                    ops.append((OP_SET_SPRITE, n, 0x28, r.choice(regular)))
                    sprite[10] = ops[-1][3]
            elif choice == 7 and not partial:
                ops.append((OP_SET_SPRITE, n, 0x30, fb(r.choice([0.0, 0.25, 1.0, r.uniform(-4, 4)]))))
            elif choice == 8:
                # the engine sprite's used part and offsets
                e = r.randint(1, ENGINES)
                offset = r.choice([0x10, 0x12, 0x14, 0x0C, 0x0E])
                # (a used size of zero divides by zero: no NaN in a drawing script, see no_nan)
                ops.append((OP_SET_ENGINE, e, offset, r.choice([1, 16, 32, r.randint(1, 100), 0xFFFF])))
        else:
            ops.append((OP_PREPARE, n))
    c["ops"] = [tuple(op) for op in ops[:length + 6]]
    return c


def edge_scripts():
    r = random.Random(106)
    # frames on lists of every shape, over worlds with the display's switches in every combination
    for _ in range(20):
        c = script_world(r, 40)
        c["ops"] = script_frame(r) + script_frame(r) + script_frame(r)
        yield c
    for display in (0, D_EACH, D_EACH | D_NOSHADOW, D_NOSHADOW, D_BLOB, D_BLOB | D_NOSHADOW, D_FOG | D_VFOG | D_HOT, D_EACH | D_FOG | D_VFOG):
        for _ in range(3):
            c = script_world(r, 40)
            c["disp"] = display
            c["ops"] = script_frame(r) * 3
            yield c
    # the long scripts
    for _ in range(12):
        yield random_script(r, 200)


def random_script_case(r):
    return random_script(r, r.choice([12, 20, 30, 40]))


TESTS = [RotateTest("y"), RotateTest("z"), LeafTest("ret"), LeafTest("uv"), LeafTest("diffuse"), LeafTest("vertex"),
         WorldTest("SSpriteGameData::Prepare", edge_prepare, random_prepare),
         WorldTest("SSpriteGameData::CalcRegular", edge_calc, random_calc),
         WorldTest("SSpriteGameData::SetupVertices", edge_setup, random_setup),
         WorldTest("CDrawPrimitive::AddSprite", edge_addsprite, random_addsprite),
         WorldTest("SSpriteGameData::DrawRegular and AddRegular", edge_draw_add, random_draw_add),
         WorldTest("the sprite pass through CSpriteManager (scripts)", edge_scripts, random_script_case)]
