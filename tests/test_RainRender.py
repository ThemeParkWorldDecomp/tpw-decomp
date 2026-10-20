"""The weather's drawing and the draw primitive's polygon adder and sprite (engine library,
src/tb_drawprimitive_sprite.cpp and src/tb_rain_render.cpp), original versus translation:
CDrawPrimitive::AddPoly_TLCTEX (0x100078c0) and DrawSprite (0x10007edc), CRain::__LightningFlash
(0x10022b20), __RenderLightning (0x10022c10), __RenderRain (0x10022efc) and __RenderSnow
(0x100233f8), GetCycleCount (0x10038b14), and the bolts' own members (src/tb_rain_lightning.cpp)
CRain::AddLightning (0x100243cc) and SLightning::Init (0x100244a0; run on bolt slot 2, with the
real GetCycleCount over the faked timer and the host's sqrt on both sides).

One world is built twice from one description, in emulator memory and through tests/unit_RainRender.cpp:
a draw primitive object, four primitive records, a vertex memory, an index memory, 512 words of input
vertices, a sprite record, the weather object (every word of its Mac layout) with three texture entries,
the display's flags and aspect, the light manager's colour and a reproducible random sequence. A case runs
one function on both sides and compares the answer, the recorded calls of the fakes and the state of
everything the call could change: the draw object, the records, the whole vertex and index memory, the input
vertices (the clipper uses them as work space), the sprite, the weather object, the random state and the words
the math unit was asked to transform.

What is real on both sides: AddPoly_TLCTEX and DrawSprite wherever the weather draws, and under them the
clipper (CPP_TestData, ClipPoly and the six edge clippers, with InterpolateColour), so the vertices and indices
that land in the primitive's memory are the ones the whole chain makes. What is a fake on both sides, recording
every argument (floats by their bits, a NaN as one value): the primitive buffer (GetNextPrimitive, which fills
the record's header in as the real one does, both ExecuteCurrentPrimitive, ProjectCurrentPrimitive, the render
memory's SaveInfo), the light manager's CalcVertexFogIntensities, the math unit (its strided Transform keeps its
input and moves the words of each point round by a permutation and sign flips from a queue, its Project answers
from a queue), CRain::RenderLightning2, the platform timer (GetAbsoluteCycles, so GetCycleCount runs for real
and its low-32-bit cut is tested), rand (a congruence both sides run, the state and number of calls compared)
and the assertion handler.

The stack. The weather's and the sprite's vertices are built in the original's stack frames and the specular
colour of a vertex is never written (stack leftovers). The test clears the stack below the call on the PowerPC
side, the translation clears its arrays, and both then carry the same words, including the ones the clipper
leaves in the arrays it uses as work space between one drop and the next.

Unicorn computes the fused multiply-adds in double precision and rounds twice, and gives fctiwz of a value above
INT32_MAX as 0x80000000 where the processor saturates (FINDINGS 2026-10-10 and 2026-10-11): the code hook
HardwareRounding corrects both inside the functions under test and the clipper's ClipPoly. NaNs are one value
wherever a float is dumped, mixed or compared.

Not tested: AddPoly_TLCTEX with no current primitive (the original reads low memory), a gei_MathUnit that is NULL
(the original calls through the null pointer after the assertion), polygons whose vertices or indices the
original's 32-bit addressing would wrap, a bolt count outside 0..16, more drops than the stack arrays hold (1,024)
and a negative drop count (the original loops four billion times).
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
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_RainRender"
MASK = 0xFFFFFFFF
NAN, INF = math.nan, math.inf

# ---- the functions under test
ADD_POLY, DRAW_SPRITE = 0x100078C0, 0x10007EDC
FLASH, RENDER_LIGHTNING, RENDER_RAIN, RENDER_SNOW, GET_CYCLE = 0x10022B20, 0x10022C10, 0x10022EFC, 0x100233F8, 0x10038B14
ADD_LIGHTNING, LIGHTNING_INIT = 0x100243CC, 0x100244A0
SIZES = {FLASH: 192, RENDER_LIGHTNING: 700, RENDER_RAIN: 1156, RENDER_SNOW: 2844, ADD_POLY: 824, DRAW_SPRITE: 776, GET_CYCLE: 40,
         ADD_LIGHTNING: 144, LIGHTNING_INIT: 368}
# The clipper's functions, __ptr_glue and the double -> unsigned routine run as the original.
CPP_TEST_DATA, CLIP_POLY = 0x1003F6DC, 0x10040640
EDGE_CLIPPERS = (0x1003F8B0, 0x1003FAE4, 0x1003FB68, 0x1003FD94, 0x1003FFC8, 0x100401F4, 0x10040424)
PTR_GLUE, CVT_UNSIGNED = 0x1004C858, 0x1004B9FC
REAL = set(SIZES) | {CPP_TEST_DATA, CLIP_POLY, PTR_GLUE, CVT_UNSIGNED} | set(EDGE_CLIPPERS)
# ---- callees that are fakes
GET_NEXT, EXECUTE_LONG, EXECUTE_SHORT, PROJECT_CURRENT = 0x10006A58, 0x10006F40, 0x10007360, 0x10007DF4
SAVE_INFO, FOG, RENDER_LIGHTNING2 = 0x10027460, 0x10014D38, 0x1002294C
# ---- engine globals
DISPLAY, RENDER_MEMORY, LIGHT_MANAGER, MATH_UNIT, CLIPPING, DRAW_VAR = (
    0x10165634, 0x10165630, 0x100AF1B8, 0x100AFE94, 0x102B3638, 0x10052B3C)
GUARDED = [(DISPLAY, 0x88), (RENDER_MEMORY, 4), (LIGHT_MANAGER, 4), (MATH_UNIT, 4), (CLIPPING, 4), (DRAW_VAR, 4)]
# The code ranges whose fused multiply-adds and fctiwz run under HardwareRounding.
ROUNDED = [(RENDER_LIGHTNING, RENDER_LIGHTNING + 700), (RENDER_RAIN, RENDER_RAIN + 1156),
           (RENDER_SNOW, RENDER_SNOW + 2844), (CLIP_POLY, CLIP_POLY + 500), (LIGHTNING_INIT, LIGHTNING_INIT + 368)]

# ---- the world (tests/unit_RainRender.cpp has the same numbers)
RECORDS, RECORD_SIZE = 4, 0x38
VERTEX_BYTES, INDEX_ENTRIES, INPUT_WORDS = 0x6000, 4096, 512
RAIN_BYTES = 0xA1CC
RAIN_WORDS = RAIN_BYTES // 4
ENTRIES, MARKERS = 3, 8
TRANSFORM_WORDS = 6144
NO_OFFSET = 0x80000000
PAGE_BASE = 0x7D100000       # a page marker's address on the PowerPC side (never dereferenced)
POINTER_FIELDS = (0x3348, 0x334C, 0x3350)
SNOW_TIMES, SNOW_CUBICS = 0x3558, 0x8558
VERTEX_REGION, INDEX_REGION, INPUT_REGION = 1, 2, 3
L_DRAW, L_MATH, L_RENDER, L_LIGHT, L_RAIN, L_MARKER, L_ENTRY = 1, 2, 3, 4, 5, 11, 21
# The words of the state, in order.
SECTIONS = [("result", 1), ("draw", 4), ("recs", RECORDS * 13), ("vertices", VERTEX_BYTES // 4), ("indices", INDEX_ENTRIES // 2),
            ("input", INPUT_WORDS), ("sprite", 11), ("rain", RAIN_WORDS), ("random", 2), ("transform", 1 + TRANSFORM_WORDS)]
OUT_WORDS = sum(n for _, n in SECTIONS)

# Operations (tpw_rr_run).
OP_ADD_POLY, OP_SPRITE, OP_FLASH, OP_LIGHTNING, OP_RAIN, OP_SNOW, OP_CYCLES = 1, 2, 3, 4, 5, 6, 7
OP_ADD_LIGHTNING, OP_BOLT_INIT = 8, 9
BOLTS, BOLT_SIZE, BOLT_FLOATS = 0x3008, 0x30, 0x24      # the bolt array; a bolt's first 9 words are floats
INIT_SLOT = 2                                           # the bolt slot op 9 sets up


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
    test and nowhere else (as in test_OpenGLDraw.py and test_FrameStandins.py):
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


# ----------------------------------------------------------------- the case description


def blank_record(vdata=0, idata=0):
    """A record's 13 words: flags, blending mode, texture marker, two filler words, type, format, vertex
    count, index count, vertex data offset, index data offset, depth bits, last field."""
    return [0, 0, 0, 0xA5A5A5A5, 0xA5A5A5A5, 0, 0, 0, 0, vdata, idata, 0, 0]


def blank_sprite():
    """A sprite's 11 words: flags, vertices (an offset in the input), colour, blending mode, texture marker,
    filler, u0, v0, u1, v1, last word."""
    return [0, 0, MASK, 0, 0, 0xA5A5A5A5, fb(0.0), fb(0.0), fb(1.0), fb(1.0), 0]


def blank_entry():
    return dict(flags=0, page=0, uoff=0.0, voff=0.0, usize=0.0, vsize=0.0)


def new_case(op, args=(), **kw):
    case = dict(op=op, args=list(args) + [0] * (8 - len(args)), disp=0, aspect=fb(0.75), light=MASK, seed=12345,
                vsize=0x20, current=0, vend=VERTEX_BYTES, iend=INDEX_ENTRIES * 2,
                recs=[blank_record() for _ in range(RECORDS)], vfill=0xA5A5A5A5, ifill=0xA5A5, inp=[0] * INPUT_WORDS,
                sprite=blank_sprite(), rain={}, entries=[blank_entry() for _ in range(ENTRIES)], q={})
    case.update(kw)
    return case


def flat(c):
    d = [c["op"]] + list(c["args"]) + [c["disp"], c["aspect"], c["light"], c["seed"], c["vsize"], c["current"], c["vend"], c["iend"]]
    for record in c["recs"]:
        d += record
    d += [c["vfill"], c["ifill"]] + c["inp"] + c["sprite"]
    rain = c["rain"]
    d += [rain.get(4 * i, 0) for i in range(RAIN_WORDS)]
    for en in c["entries"]:
        d += [en["flags"], en["page"], fb(en["uoff"]), fb(en["voff"]), fb(en["usize"]), fb(en["vsize"])]
    return d


def sections(data):
    """Splits a state (little-endian words as bytes) into its named sections."""
    out, at = {}, 0
    for name, n in SECTIONS:
        out[name] = data[at:at + 4 * n]
        at += 4 * n
    return out


def vertex_size_of(fmt):
    return {0xC4: 0x18, 0x1C4: 0x20, 0x2C4: 0x28, 0x3C4: 0x30}.get(fmt, 0)


# ----------------------------------------------------------------- the PowerPC side

PERMUTATIONS = [(0, 1, 2), (1, 2, 0), (2, 0, 1), (0, 2, 1), (2, 1, 0), (1, 0, 2)]


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if hasattr(e, "rr_math"):
        return
    # The math unit: +4 is the vtable pointer; the strided Transform is the slot at +0x10, Project the one at +0x14.
    transform = T.fake_function(e, lambda em: em.rr_side.transform(em))
    project = T.fake_function(e, lambda em: em.rr_side.project(em))
    vtable = T.persist_alloc(e, 0x40)
    e.write(vtable + 0x10, struct.pack(">II", transform, project))
    e.rr_math = T.persist_alloc(e, 0x10)
    e.write(e.rr_math, struct.pack(">II", 0, vtable))
    e.rr_side = None


@contextlib.contextmanager
def guarded(e):
    """The engine globals a case overwrites, put back afterwards (other modules share them)."""
    saved = [(a, e.read(a, n)) for a, n in GUARDED]
    try:
        yield
    finally:
        for a, data in saved:
            e.write(a, data)


class PpcSide:
    """One case on the PowerPC side: the world laid out as the Mac does and the fakes."""

    def __init__(self, e, case):
        self.e = e
        c = self.case = case
        self.vtx = e.alloc(VERTEX_BYTES)
        self.idx = e.alloc(INDEX_ENTRIES * 2)
        self.inp = e.alloc(INPUT_WORDS * 4)
        self.recs = e.alloc(RECORDS * RECORD_SIZE)
        self.draw = e.alloc(0x40, b"\xa5" * 0x40)
        self.sprite = e.alloc(0x2C)
        self.rain = e.alloc(RAIN_BYTES)
        self.entries = [e.alloc(0x5C, b"\xa5" * 0x5C) for _ in range(ENTRIES)]
        self.render, self.light, self.clipping, self.clip = (e.alloc(0x10, b"\xa5" * 0x10) for _ in range(4))
        self.pages = [PAGE_BASE + 0x100 * k for k in range(1, MARKERS + 1)]
        e.write(self.vtx, struct.pack(">I", c["vfill"]) * (VERTEX_BYTES // 4))
        e.write(self.idx, struct.pack(">H", c["ifill"] & 0xFFFF) * INDEX_ENTRIES)
        ws(e, self.inp, c["inp"])
        for k, w in enumerate(c["recs"]):
            ws(e, self.recs + k * RECORD_SIZE, [w[0], w[1], self.page(w[2]), w[3], w[4], 0xA5A5A5A5, w[5], w[6], w[7], w[8],
                                                 self.vertex_at(w[9]), self.index_at(w[10]), w[11], w[12]])
        ws(e, self.draw + 0x18, [c["vsize"], self.record(c["current"]), 0, self.vertex_at(c["vend"]), self.index_at(c["iend"])])
        ws(e, self.draw + 0x2C, [0])
        s = c["sprite"]
        ws(e, self.sprite, [s[0], 0 if s[1] == NO_OFFSET else self.inp + T.s32(s[1]), s[2], s[3], self.page(s[4]), s[5],
                            s[6], s[7], s[8], s[9], s[10]])
        words = [c["rain"].get(4 * i, 0) for i in range(RAIN_WORDS)]
        for offset in POINTER_FIELDS:
            n = words[offset // 4]
            words[offset // 4] = self.entries[n - 1] if 1 <= n <= ENTRIES else 0
        ws(e, self.rain, words)
        for en, a in zip(c["entries"], self.entries):
            w32(e, a + 8, en["flags"])
            w32(e, a + 0x18, self.page(en["page"]))
            ws(e, a + 0x1C, [fb(en["usize"]), fb(en["vsize"]), fb(en["uoff"]), fb(en["voff"])])
        ws(e, DISPLAY, [c["disp"], c["aspect"]])
        ws(e, self.light, [0, c["light"]])
        ws(e, RENDER_MEMORY, [self.render])
        ws(e, LIGHT_MANAGER, [self.light])
        ws(e, MATH_UNIT, [e.rr_math])
        ws(e, CLIPPING, [self.clipping])
        ws(e, DRAW_VAR, [self.draw])
        e.write(self.clipping, b"\0" * 8)
        e.write(self.clip, bytes([c["args"][4] & 0xFF, c["args"][5] & 0xFF, 0xA5, 0xA5]))
        fixed = {self.draw: L_DRAW, e.rr_math: L_MATH, self.render: L_RENDER, self.light: L_LIGHT, self.rain: L_RAIN}
        fixed.update({p: L_MARKER + i for i, p in enumerate(self.pages)})
        fixed.update({a: L_ENTRY + i for i, a in enumerate(self.entries)})
        e.cl = self.L = CL.PpcLog(fixed, c["q"])
        self.rand_state, self.rand_calls = c["seed"] & MASK, 0
        self.transform_words = []
        e.rr_side = self

    # -- addresses

    def page(self, number):
        return self.pages[number - 1] if 1 <= number <= MARKERS else 0

    def marker(self, address):
        return self.pages.index(address) + 1 if address in self.pages else (0 if address == 0 else -1)

    def vertex_at(self, offset):
        return 0 if offset == NO_OFFSET else self.vtx + T.s32(offset)

    def index_at(self, offset):
        return 0 if offset == NO_OFFSET else self.idx + T.s32(offset)

    def record(self, number):
        return self.recs + (number - 1) * RECORD_SIZE if 1 <= number <= RECORDS else 0

    def record_id(self, address):
        if address == 0:
            return 0
        offset = address - self.recs
        if offset < 0 or offset >= RECORDS * RECORD_SIZE or offset % RECORD_SIZE:
            return -1
        return offset // RECORD_SIZE + 1

    def place(self, region, address):
        if address == 0:
            return 0
        base = {VERTEX_REGION: self.vtx, INDEX_REGION: self.idx, INPUT_REGION: self.inp}[region]
        return (region << 32) + T.s32(address - base)

    def offset_in(self, base, address):
        return NO_OFFSET if address == 0 else (address - base) & MASK

    # -- the fakes

    def get_next(self, e):
        L = self.L
        this, flags, ptype, fmt, mode, texture = e.r(3), T.s32(e.r(4)), T.s32(e.r(5)), e.r(6), T.s32(e.r(7)), e.r(8)
        L.rec("GetNextPrimitive", (L.lab(this), flags, ptype, fmt, mode, L.lab(texture)))
        record = self.record(L.ret("GetNextPrimitive", 1))
        w32(e, this + 0x1C, record)
        if record:
            ws(e, record + 4, [mode & MASK])
            w32(e, record + 8, texture)
            ws(e, record + 0x18, [ptype & MASK, fmt, ])
            w32(e, record + 0x34, MASK)
            w32(e, this + 0x18, vertex_size_of(fmt))
        e.set_r(3, record)

    def execute_long(self, e):
        self.L.rec("ExecuteCurrentPrimitive", (self.L.lab(e.r(3)),))
        e.set_r(3, T.u32(self.L.ret("Execute", 1)))

    def execute_short(self, e):
        self.L.rec("ExecuteCurrentPrimitiveDepth", (self.L.lab(e.r(3)), cfb(e.f(1))))
        e.set_r(3, T.u32(self.L.ret("ExecuteDepth", 1)))

    def project_current(self, e):
        self.L.rec("ProjectCurrentPrimitive", (self.L.lab(e.r(3)), T.s32(e.r(4))))

    def save_info(self, e):
        self.L.rec("SaveInfo", (self.L.lab(e.r(3)), self.place(VERTEX_REGION, e.r(4)), self.place(INDEX_REGION, e.r(5))))

    def fog(self, e):
        self.L.rec("CalcVertexFogIntensities", (self.L.lab(e.r(3)), self.place(VERTEX_REGION, e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))))
        e.set_r(3, self.L.ret("Fog", 0) & 0xFF)

    def lightning2(self, e):
        L = self.L

        def where(p):
            offset = p - (self.rain + 0x3008)
            return offset // 12 if 0 <= offset < 0x300 else -1
        word = lambda offset: e.u32(self.rain + offset)
        from_, to = rs(e, e.r(4), 3), rs(e, e.r(5), 3)
        L.rec("RenderLightning2", (L.lab(e.r(3)), where(e.r(4)), where(e.r(5)), T.s32(word(0x9DA0)), T.s32(word(0xA198)),
                                   word(0xA19C), T.s32(word(0xA1A0)), T.s32(word(0x3308)), T.s32(word(0x9D9C))))
        L.rec("RenderLightning2Points", tuple(canon(v) for v in from_ + to))
        e.set_r(3, T.u32(L.ret("RenderLightning2", 1)))

    def transform(self, e):
        L = self.L
        matrix, out, src, out_stride, in_stride, count, kind = e.r(4), e.r(5), e.r(6), T.s32(e.r(7)), T.s32(e.r(8)), T.s32(e.r(9)), T.s32(e.r(10))
        L.rec("Transform", (L.lab(e.r(3)), int(matrix == self.rain + 0x9D58), int(out == src), out_stride, in_stride, count, kind))
        m = rs(e, matrix, 16)
        L.rec("TransformMatrix", tuple(canon(v) for v in m[:8]))
        L.rec("TransformMatrix", tuple(canon(v) for v in m[8:]))
        mode = L.ret("TransformMode", 0)
        order, flips = PERMUTATIONS[mode % 6], (mode // 6) % 8
        rows = []
        for i in range(max(count, 0)):
            rows.append(rs(e, src + i * in_stride, 3))
        self.transform_words = []
        for w in rows:
            for k in range(3):
                if len(self.transform_words) < TRANSFORM_WORDS:
                    self.transform_words.append(canon(w[k]))
        for i, w in enumerate(rows):
            ws(e, out + i * out_stride, [w[order[k]] ^ (0x80000000 if (flips >> k) & 1 else 0) for k in range(3)])

    def project(self, e):
        L = self.L
        L.rec("Project", (L.lab(e.r(3)), self.place(VERTEX_REGION, e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), T.s32(e.r(7))))
        e.set_f(1, bf(L.ret("Project", 0)))

    def rand(self, e):
        self.rand_state = (self.rand_state * 1664525 + 1013904223) & MASK
        self.rand_calls += 1
        e.set_r(3, (self.rand_state >> 16) & 0x7FFF)

    def cycles(self, e):
        self.L.rec("GetAbsoluteCycles")
        v = self.L.ret("Cycles", 0) & 0xFFFFFFFFFFFFFFFF
        e.set_r(3, v >> 32)
        e.set_r(4, v & MASK)

    def assertion(self, e):
        self.L.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4))))

    # -- the state

    def state(self, result):
        e, c = self.e, self.case
        s = {}
        s["result"] = little([result])
        draw = rs(e, self.draw, 0x40 // 4)
        s["draw"] = little([draw[6], self.record_id(draw[7]), self.offset_in(self.vtx, draw[9]), self.offset_in(self.idx, draw[10])])
        recs = []
        for k in range(RECORDS):
            w = rs(e, self.recs + k * RECORD_SIZE, RECORD_SIZE // 4)
            recs += [w[0], w[1], self.marker(w[2]), w[3], w[4], w[6], w[7], w[8], w[9], self.offset_in(self.vtx, w[10]),
                     self.offset_in(self.idx, w[11]), canon(w[12]), w[13]]
        s["recs"] = little(recs)
        s["vertices"] = little([canon(v) for v in rs(e, self.vtx, VERTEX_BYTES // 4)])
        s["indices"] = struct.pack("<%dH" % INDEX_ENTRIES, *struct.unpack(">%dH" % INDEX_ENTRIES, e.read(self.idx, INDEX_ENTRIES * 2)))
        s["input"] = little([canon(v) for v in rs(e, self.inp, INPUT_WORDS)])
        sp = rs(e, self.sprite, 11)
        s["sprite"] = little([sp[0], self.offset_in(self.inp, sp[1]), sp[2], sp[3], self.marker(sp[4]), sp[5], canon(sp[6]),
                              canon(sp[7]), canon(sp[8]), canon(sp[9]), sp[10]])
        words = rs(e, self.rain, RAIN_WORDS)
        for offset in POINTER_FIELDS:
            words[offset // 4] = self.L.lab(words[offset // 4]) & MASK
        for i in range(SNOW_TIMES // 4, SNOW_TIMES // 4 + 1024):
            words[i] = canon(words[i])
        for k in range(16):
            for j in range(BOLT_FLOATS // 4):
                i = (BOLTS + BOLT_SIZE * k + 4 * j) // 4
                words[i] = canon(words[i])
        s["rain"] = little(words)
        s["random"] = little([self.rand_state, self.rand_calls])
        tw = self.transform_words
        s["transform"] = little([len(tw)] + tw + [0] * (TRANSFORM_WORDS - len(tw)))
        return s


def stack_cleared(e):
    """Zeroes the stack below the call's frame: the vertices' specular words are never written."""
    sp = ppcemu.STACK_BASE + ppcemu.STACK_SIZE - 0x1000 - 0x400
    e.write(sp - 0x10000, b"\0" * 0x10000)


def run_ppc(e, case):
    op = case["op"]
    with guarded(e):
        side = PpcSide(e, case)
        st = CL.Stubs()
        st.real = set(REAL)
        st.internal(GET_NEXT, side.get_next)
        st.internal(EXECUTE_LONG, side.execute_long)
        st.internal(EXECUTE_SHORT, side.execute_short)
        st.internal(PROJECT_CURRENT, side.project_current)
        st.internal(SAVE_INFO, side.save_info)
        st.internal(FOG, side.fog)
        st.internal(RENDER_LIGHTNING2, side.lightning2)
        st.imp("rand", side.rand)
        st.imp("__assertion_failed", side.assertion)
        st.imp("GetAbsoluteCycles__Q213SamsUtilities6UTimerFv", side.cycles)
        saved = st.activate(e)
        stack_cleared(e)
        a = case["args"]
        try:
            with HardwareRounding(e, ROUNDED):
                if op == OP_ADD_POLY:
                    r3 = T.call_ppc(e, ADD_POLY, [side.draw, side.inp + a[0], T.I(a[1]), T.I(a[2]), side.clip if a[3] else 0, T.I(a[6])])[0]
                elif op == OP_SPRITE:
                    r3 = T.call_ppc(e, DRAW_SPRITE, [side.draw, side.sprite, T.I(a[0])])[0]
                elif op == OP_FLASH:
                    r3 = T.call_ppc(e, FLASH, [side.rain])[0]
                elif op == OP_LIGHTNING:
                    r3 = T.call_ppc(e, RENDER_LIGHTNING, [side.rain])[0]
                elif op == OP_ADD_LIGHTNING:
                    r3 = T.call_ppc(e, ADD_LIGHTNING, [side.rain] + [T.I(v) for v in a[:8]])[0]
                elif op == OP_BOLT_INIT:
                    r3 = T.call_ppc(e, LIGHTNING_INIT, [side.rain + BOLTS + BOLT_SIZE * INIT_SLOT] + [T.I(v) for v in a[:8]])[0]
                    r3 = 0                  # Init answers nothing: r3 holds a leftover
                elif op in (OP_RAIN, OP_SNOW):
                    r3 = T.call_ppc(e, RENDER_RAIN if op == OP_RAIN else RENDER_SNOW,
                                    [side.rain, T.F(bf(a[0])), T.F(bf(a[1])), T.F(bf(a[2]))])[0]
                else:
                    r3 = T.call_ppc(e, GET_CYCLE, [])[0]
        finally:
            CL.Stubs.deactivate(e, saved)
            e.rr_side = None
        return dict(state=side.state(T.s32(r3)), log=side.L.log)


# ----------------------------------------------------------------- the host side


def run_host(lib, case):
    lib.tpw_rr_run.restype = C.c_int32
    lib.tpw_rr_run.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_uint32)]
    out = (C.c_uint32 * (OUT_WORDS + 16))()
    CL.host_begin(lib, {}, case.get("q"))
    n = lib.tpw_rr_run(i64(flat(case)), out)
    log = CL.host_end(lib)
    assert n == OUT_WORDS, (n, OUT_WORDS)
    return dict(state=sections(bytes(out)[:4 * OUT_WORDS]), log=log)


# ----------------------------------------------------------------- cases


def rfl(r, special=0.08, lo=-10.0, hi=10.0):
    """A float for a field: mostly moderate, now and then an edge value."""
    if r.random() < special:
        return r.choice([0.0, -0.0, 1.0, -1.0, 0.5, 4.0, -4.0, 8.0, 3.9999, 4.0001, -4.0001, 12.0, -12.0, 1e30, -1e30, NAN, INF, -INF,
                         1e-40, 3.4028234663852886e38, 16384.0, 131072.0, 0.3, 100.0, 6.0, 7.0, 20.0, -20.0])
    return T.f32(r.uniform(lo, hi))


def rmat(r, special=0.04, scale=2.0):
    return [fb(T.f32(r.uniform(-scale, scale))) if r.random() > special else fb(rfl(r, 1.0)) for _ in range(16)]


def put_floats(rain, offset, values):
    for i, v in enumerate(values):
        rain[offset + 4 * i] = fb(v)


def rand_entry(r, page_chance=0.7):
    on_page = r.random() < page_chance
    return dict(flags=r.choice([0x1000, 0x1000, 0x100000, 0x1000 | 0x100000, 0xFFFFFFFF, 0x2000]) if on_page else r.choice([0, 0x2000, 0x100000]),
                page=r.randint(0, MARKERS), uoff=rfl(r, 0.1, 0.0, 1.0), voff=rfl(r, 0.1, 0.0, 1.0), usize=rfl(r, 0.1, 0.0, 0.5),
                vsize=rfl(r, 0.1, 0.0, 0.5))


def rand_light(r):
    return r.choice([MASK, MASK, 0xFFFF0000, 0xFF00FF00, 0x80808080, 0x00000000, 0x7F7F7F7F, r.getrandbits(32)])


def rand_display(r, extra=0):
    flags = r.choice([0, 0x2, 0x100, 0x102, 0x2000, 0x6000, 0x4000, 0x410100, r.getrandbits(32)]) | extra
    return flags & MASK


def world_limits(r, c):
    """The primitive's memory ends: roomy mostly, tight now and then (the polygon adder refuses without room)."""
    k = r.random()
    if k < 0.8:
        return
    vdata = c["recs"][0][9]
    idata = c["recs"][0][10]
    vcount, icount = c["recs"][0][7], c["recs"][0][8]
    vlimit = vdata + (vcount + 10) * c["vsize"]
    ilimit = idata + (icount + 0x18) * 2
    c["vend"] = max(0, min(VERTEX_BYTES, vlimit + r.choice([-1, 0, 0, 1, 31, 32, 33, 0x40 * r.randint(0, 40)])))
    c["iend"] = max(0, min(INDEX_ENTRIES * 2, ilimit + r.choice([-1, 0, 0, 1, 2, 3, 0x10 * r.randint(0, 40)])))


def set_queues(r, c, **extra):
    q = {"GetNextPrimitive": [r.choice([1, 1, 1, 1, 0, 2])], "Execute": [r.choice([1, 1, 1, 0, 7, -1])],
         "ExecuteDepth": [r.choice([1, 1, 1, 0, 7, -1])], "Fog": [r.choice([0, 1, 1, 0xFF, 0x100])],
         "Project": [fb(rfl(r, 0.15, -100.0, 100.0))], "TransformMode": [r.randrange(48)],
         "RenderLightning2": [r.choice([1, 1, 1, 0])], "Cycles": [r.choice([0, 1, 0xFFFFFFFF, 1 << 32, r.getrandbits(64), 123456789])]}
    q.update(extra)
    c["q"] = q


# ---- AddPoly_TLCTEX

STRIDES_CLIPPED = (0x18, 0x20, 0x28, 0x30, 0x38)


def poly_words(r, count, stride, kind, w_sign=1):
    """The words of `count` vertices `stride` bytes apart, in the input's memory."""
    words = []
    for _ in range(count):
        if kind == "inside":
            w = T.f32(r.uniform(0.5, 3.0))
            v = [T.f32(r.uniform(-w, w)), T.f32(r.uniform(-w, w)), T.f32(r.uniform(0.0, w)), w]
        elif kind == "outside":
            w = T.f32(r.uniform(0.5, 2.0))
            v = [T.f32(r.uniform(w * 1.01, w * 4)), T.f32(r.uniform(-w, w)), T.f32(r.uniform(0.0, w)), w]
        elif kind == "mixed":
            w = T.f32(r.uniform(0.5, 2.0) * w_sign)
            v = [T.f32(r.uniform(-2 * abs(w), 2 * abs(w))), T.f32(r.uniform(-2 * abs(w), 2 * abs(w))),
                 T.f32(r.uniform(-abs(w), 2 * abs(w))), w]
        else:
            v = [bf(r.getrandbits(32)) for _ in range(4)]
        row = [fb(x) for x in v] + [r.getrandbits(32) for _ in range(stride // 4 - 4)]
        words += row
    return words


def poly_case(r, count=None, stride=None, kind=None, clip=None, flags=None, offset=None, limits=True):
    count = r.choice([3, 3, 4, 4, 4, 5, 6, 7, 8]) if count is None else count
    stride = r.choice(STRIDES_CLIPPED + (0x20, 0x20)) if stride is None else stride
    kind = r.choice(["inside", "inside", "outside", "mixed", "mixed", "mixed", "bits"]) if kind is None else kind
    if clip is None:
        clip = r.choice([None, None, (0, 0), (0, r.randrange(1, 64)), (0, 0x3F), (0, r.getrandbits(8)), (1, r.getrandbits(8)), (0, 0xFF)])
    flags = r.choice([0, 0, 2, 2, 1, 3, 0x10, 0xFFFFFFFF, r.getrandbits(32)]) if flags is None else flags
    offset = r.choice([0, 0, 0, 4, 0x40, 0x100]) if offset is None else offset
    inp = [0] * INPUT_WORDS
    words = poly_words(r, count, stride, kind)
    inp[offset // 4:offset // 4 + len(words)] = words
    c = new_case(OP_ADD_POLY, [offset, count, stride, 0 if clip is None else 1, 0 if clip is None else clip[0], 0 if clip is None else clip[1],
                               flags], inp=inp)
    vcount = r.choice([0, 0, 1, 5, 17, 100])
    icount = r.choice([0, 0, 3, 9, 30, 301])
    c["current"] = 1
    c["vsize"] = r.choice([0x20, 0x20, 0x20, 0x18, 0x28, 0x30])
    c["recs"][0] = blank_record(vdata=r.choice([0, 0, 0x20, 0x100]), idata=r.choice([0, 0, 2, 0x40]))
    c["recs"][0][7], c["recs"][0][8] = vcount, icount
    c["recs"][0][0] = r.choice([0, 0x21, 0x400])
    if limits:
        world_limits(r, c)
    return c


def edge_poly():
    r = random.Random(701)
    # Every count with clipping off, both windings and the three kinds of polygon.
    for count in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10):
        for flags in (0, 2):
            for clip in ((0, 0), None):
                yield poly_case(r, count=count, stride=0x20, kind="inside", clip=clip, flags=flags)
    # Clipped polygons: each plane alone, all planes, the codes worked out and given.
    for mask in (1, 2, 4, 8, 16, 32, 0x3F, 0xFF, 0x15, 0x2A):
        for count in (3, 4, 6):
            yield poly_case(r, count=count, stride=0x20, kind="mixed", clip=(0, mask), flags=r.choice([0, 2]))
            yield poly_case(r, count=count, stride=0x20, kind="mixed", clip=None, flags=r.choice([0, 2]))
    # The outside test: all outside (nothing added), given and worked out.
    for clip in ((1, 0), (0xFF, 0x3F), (0x20, 0x20), None):
        yield poly_case(r, count=4, stride=0x20, kind="outside", clip=clip)
    # Every stride the clipper takes, and others where nothing is clipped.
    for stride in STRIDES_CLIPPED:
        for kind in ("inside", "mixed"):
            yield poly_case(r, count=4, stride=stride, kind=kind, clip=None)
    for stride in (0x10, 0x14, 0x1C, 0x24, 0x40):
        yield poly_case(r, count=4, stride=stride, kind="inside", clip=(0, 0))
    # The room test at its edges: one byte or entry short, exact, more than enough.
    for vdelta in (-0x20, -1, 0, 1, 0x40):
        for idelta in (-2, -1, 0, 1, 4):
            c = poly_case(r, count=4, stride=0x20, kind="inside", clip=(0, 0), flags=0, limits=False)
            c["vsize"] = 0x20
            c["recs"][0][7], c["recs"][0][8] = 5, 9
            c["recs"][0][9], c["recs"][0][10] = 0x20, 4
            c["vend"] = 0x20 + (5 + 10) * 0x20 + vdelta
            c["iend"] = 4 + (9 + 0x18) * 2 + idelta
            yield c
    # An existing count of vertices and indices (the indices count from the vertex count).
    for vcount in (0, 1, 7, 100, 250):
        c = poly_case(r, count=6, stride=0x20, kind="inside", clip=(0, 0), flags=0)
        c["recs"][0][7] = vcount
        c["recs"][0][8] = vcount * 3
        yield c
    # Special coordinates.
    for kind in ("bits", "bits", "bits"):
        for _ in range(8):
            yield poly_case(r, kind=kind, clip=None)
    for _ in range(12):
        yield poly_case(r, kind="mixed", clip=None)


def random_poly(r):
    return poly_case(r)


# ---- DrawSprite and the flash

def sprite_case(r, op=OP_SPRITE, kind=None, mode=None):
    kind = r.choice(["inside", "inside", "mixed", "mixed", "outside", "edge"]) if kind is None else kind
    c = new_case(op)
    vertices = []
    for i in range(4):
        w = T.f32(r.uniform(0.5, 1.5))
        if kind == "inside":
            v = [T.f32(r.uniform(-w, w)), T.f32(r.uniform(-w, w)), T.f32(r.uniform(0.0, w)), w]
        elif kind == "outside":
            v = [T.f32(r.uniform(w, 3 * w)), T.f32(r.uniform(-w, w)), T.f32(r.uniform(0.0, w)), w]
        elif kind == "edge":
            v = [r.choice([-w, w, 0.0]), r.choice([-w, w, 0.0]), r.choice([0.0, w, -0.0]), w]
        else:
            v = [T.f32(r.uniform(-2 * w, 2 * w)), T.f32(r.uniform(-2 * w, 2 * w)), T.f32(r.uniform(-0.5, 1.5 * w)), w]
        vertices += [fb(x) for x in v]
    c["inp"][0:16] = vertices
    c["inp"][16:] = [r.getrandbits(32) for _ in range(INPUT_WORDS - 16)]
    mode = r.choice([MASK, MASK, MASK, 0xFFFFFFFD, 0, 0x3F, 0xFFFFFFFE, r.getrandbits(32)]) if mode is None else mode
    c["args"][0] = mode
    c["sprite"] = [r.choice([0, 1, 5, 2, 7, 0x400, 0x405, r.getrandbits(32)]), 0, r.choice([MASK, 0x80FFFFFF, r.getrandbits(32)]),
                   r.choice([0x1256, 0x556, 0, r.getrandbits(32)]), r.randint(0, MARKERS), 0xA5A5A5A5, fb(rfl(r, 0.1, 0.0, 1.0)),
                   fb(rfl(r, 0.1, 0.0, 1.0)), fb(rfl(r, 0.1, 0.0, 1.0)), fb(rfl(r, 0.1, 0.0, 1.0)), r.choice([0, 1, 2, 3, 5])]
    c["disp"] = rand_display(r)
    c["light"] = rand_light(r)
    c["current"] = r.choice([0, 1, 2])
    c["vsize"] = r.choice([0x20, 0x20, 0x18])
    c["recs"][0] = blank_record(vdata=r.choice([0, 0x20]), idata=r.choice([0, 4]))
    c["recs"][0][7], c["recs"][0][8] = r.choice([0, 0, 0, 3, 40]), r.choice([0, 0, 0, 3, 30])
    c["recs"][0][11] = fb(rfl(r, 0.1, -50.0, 50.0))
    c["recs"][0][0] = r.choice([0, 0x20, 0x400])
    c["recs"][0][1] = r.choice([0, 0x100000, 0x80000, r.getrandbits(32)])
    world_limits(r, c)
    set_queues(r, c)
    return c


def edge_sprite():
    r = random.Random(702)
    for kind in ("inside", "mixed", "outside", "edge"):
        for mode in (MASK, 0xFFFFFFFD, 0, 0x3F):
            for disp in (0, 0x2000, 0x6000, 0x4000):
                c = sprite_case(r, kind=kind, mode=mode)
                c["disp"] = disp
                yield c
    # The far plane left out: a quad behind the far plane (z > w) is drawn with -3, dropped with -1.
    for z in (1.5, 2.0, 1.0001):
        for mode in (MASK, 0xFFFFFFFD):
            c = sprite_case(r, kind="inside", mode=mode)
            for i in range(4):
                c["inp"][4 * i + 2] = fb(z)
                c["inp"][4 * i + 3] = fb(1.0)
            yield c
    # Each flag, each texture, each record, each answer.
    for flags in (0, 1, 2, 4, 5, 0x400, 0x402):
        c = sprite_case(r, kind="inside", mode=MASK)
        c["sprite"][0] = flags
        yield c
    for texture in range(0, MARKERS + 1):
        c = sprite_case(r, kind="inside", mode=MASK)
        c["sprite"][4] = texture
        yield c
    for record in (0, 1, 2, 3, 4):
        c = sprite_case(r, kind="inside", mode=MASK)
        c["q"]["GetNextPrimitive"] = [record]
        yield c
    for answer in (0, 1, 7, -1):
        c = sprite_case(r, kind="inside", mode=MASK)
        c["q"]["Execute"] = [answer]
        yield c
    # Fog on and off, found and not; the projection of a primitive with and without vertices.
    for disp in (0, 0x2000, 0x4000, 0x6000):
        for fog in (0, 1, 0x100, 0xFF):
            for count in (0, 3):
                c = sprite_case(r, kind="inside", mode=MASK)
                c["disp"] = disp
                c["q"]["Fog"] = [fog]
                c["recs"][0][7] = count
                c["recs"][0][8] = count
                c["recs"][0][1] = 0x180000
                yield c
    # No room for the polygon: the primitive stays as it was.
    for vcount, icount in ((0, 0), (3, 9), (3, 0), (0, 3)):
        c = sprite_case(r, kind="inside", mode=MASK)
        c["vend"] = 0x20
        c["recs"][0][7], c["recs"][0][8] = vcount, icount
        yield c
    # The colour and the light.
    for colour in (MASK, 0x80FFFFFF, 0, 0x12345678):
        for light in (MASK, 0xFFFF0000, 0, 0x0F0F0F0F):
            c = sprite_case(r, kind="inside", mode=MASK)
            c["sprite"][2] = colour
            c["light"] = light
            yield c
    for _ in range(20):
        yield sprite_case(r)


def random_sprite(r):
    return sprite_case(r)


def edge_flash():
    r = random.Random(703)
    for disp in (0, 0x2000, 0x6000, 0x4000):
        for light in (MASK, 0xFF00FF00, 0):
            for flash in (0, 1, 7):
                c = new_case(OP_FLASH, disp=disp, light=light)
                c["rain"] = {4: flash}
                set_queues(r, c)
                c["current"] = 1
                yield c
    for q in ({"GetNextPrimitive": [0]}, {"Execute": [0]}, {"Execute": [7]}, {"Fog": [1]}, {"Project": [fb(2.5)]}, {"GetNextPrimitive": [3]}):
        c = new_case(OP_FLASH, disp=0x6000)
        set_queues(r, c, **q)
        yield c
    for vcount, icount, vend in ((0, 0, VERTEX_BYTES), (5, 9, VERTEX_BYTES), (0, 0, 0x20), (0, 0, 10 * 0x20 + 0x20 - 1), (0, 0, 10 * 0x20 + 0x20)):
        c = new_case(OP_FLASH, disp=0x2000)
        c["recs"][0][7], c["recs"][0][8], c["vend"] = vcount, icount, vend
        set_queues(r, c)
        yield c
    for _ in range(10):
        yield flash_case(r)


def flash_case(r):
    c = new_case(OP_FLASH)
    c["disp"] = rand_display(r)
    c["light"] = rand_light(r)
    c["rain"] = {4: r.choice([0, 1, 2, r.getrandbits(32)])}
    c["recs"][0][7], c["recs"][0][8] = r.choice([0, 0, 3, 20]), r.choice([0, 0, 6, 30])
    c["vsize"] = r.choice([0x20, 0x18])
    world_limits(r, c)
    set_queues(r, c)
    return c


def random_flash(r):
    return flash_case(r)


# ---- __RenderLightning and GetCycleCount


def lightning_case(r, count=None, delta=None, seed_zero=None, flash=None):
    count = r.choice([0, 1, 2, 3, 5, 8, 15, 16]) if count is None else count
    c = new_case(OP_LIGHTNING)
    rain = c["rain"]
    delta = r.choice([0, 1, 16, 33, 100, 250, 499, 500, 501, 1000]) if delta is None else delta
    rain[0x3308] = count
    rain[0x9D9C] = delta & MASK
    rain[4] = r.choice([0, 0, 1, 2]) if flash is None else flash
    rain[0x9DA0] = r.getrandbits(32)
    rain[0xA198], rain[0xA19C], rain[0xA1A0] = r.getrandbits(32), r.getrandbits(32), r.getrandbits(32)
    for k in range(16):
        base = 0x3008 + 0x30 * k
        put_floats(rain, base, [rfl(r, 0.05, -50.0, 50.0) for _ in range(9)])
        remainder = r.choice([0, 1, 2, 100, 250, 400, 449, 450, 451, 480, 498, 499])
        time = r.choice([delta - 1, delta, delta + 1, delta + 1 + remainder, delta + 1 + 500 + remainder, delta + 2000 + remainder,
                         r.randint(-20, 3000)])
        if r.random() < 0.05:
            time = r.choice([0x7FFFFFFF, -0x80000000, 0x80000000 - 1, MASK])
        rain[base + 0x24] = time & MASK
        rain[base + 0x28] = r.getrandbits(32)
        seed_zero_now = (r.random() < 0.35) if seed_zero is None else seed_zero
        rain[base + 0x2C] = 0 if seed_zero_now else r.choice([r.getrandbits(32), 1, MASK])
    c["seed"] = r.getrandbits(32)
    c["disp"] = rand_display(r)
    c["light"] = rand_light(r)
    c["vsize"] = r.choice([0x20, 0x18])
    set_queues(r, c)
    c["q"]["RenderLightning2"] = [r.choice([1, 1, 1, 1, 1, 0]) for _ in range(r.randint(1, 4))]
    c["q"]["Cycles"] = [r.choice([0, 1, 0xFFFFFFFF, 1 << 32, r.getrandbits(64), 123456789, 0xFFFFFFFF12345678])]
    c["q"]["Execute"] = [r.choice([1, 1, 1, 0])]
    c["recs"][0][7], c["recs"][0][8] = r.choice([0, 0, 4]), r.choice([0, 0, 6])
    world_limits(r, c)
    return c


def edge_lightning():
    r = random.Random(704)
    # The fraction of the cycle at every step around 0.9 (450 of 500), both seeds.
    for remainder in (0, 1, 2, 100, 224, 225, 226, 400, 448, 449, 450, 451, 452, 475, 498, 499):
        for cycles in (0, 1, 3):
            c = lightning_case(r, count=1, delta=0, seed_zero=False, flash=0)
            c["rain"][0x302C] = (500 * cycles + remainder + 1) & MASK
            c["rain"][0x9D9C] = 1
            yield c
    # Expiry: a bolt whose time runs out (to 0, below 0), the last bolt moving in, several at once.
    for count in (1, 2, 3, 4, 16):
        for expiring in range(0, 1 << min(count, 4)):
            c = lightning_case(r, count=count, delta=10, seed_zero=False, flash=0)
            for k in range(count):
                dead = (expiring >> (k % 4)) & 1
                c["rain"][0x3008 + 0x30 * k + 0x24] = (10 if dead else 11 + 7 * k) & MASK
                c["rain"][0x3008 + 0x30 * k + 0x2C] = 0x1000 + k
            yield c
    # The flash, with the draw failing; with no bolts.
    for flash in (0, 1):
        for execute in (1, 0):
            for count in (0, 2):
                c = lightning_case(r, count=count, delta=5, flash=flash)
                c["q"]["Execute"] = [execute]
                yield c
    # RenderLightning2 failing on the first, second and third bolt.
    for answers in ([0], [1, 0], [1, 1, 0], [1, 1, 1], [0, 1]):
        c = lightning_case(r, count=4, delta=1, seed_zero=False, flash=0)
        for k in range(4):
            c["rain"][0x3008 + 0x30 * k + 0x24] = 1000 + k
        c["q"]["RenderLightning2"] = answers
        yield c
    # The cycle counter's value is cut to 32 bits (no seed).
    for cycles in (0, 1, 0xFFFFFFFF, 1 << 32, (1 << 32) + 5, 0x123456789ABCDEF0, 0xFFFFFFFFFFFFFFFF, 0x80000000):
        c = lightning_case(r, count=1, delta=1, seed_zero=True, flash=0)
        c["rain"][0x3008 + 0x24] = 100
        c["q"]["Cycles"] = [cycles]
        yield c
    # Delta times and bolt times at the ends of 32 bits.
    for time in (0x7FFFFFFF, -0x80000000, 1, 0, -1, 501, 500, 499):
        for delta in (0, 1, MASK, 0x80000000, 500):
            c = lightning_case(r, count=1, delta=delta, seed_zero=False, flash=0)
            c["rain"][0x3008 + 0x24] = time & MASK
            yield c
    for _ in range(30):
        yield lightning_case(r)


def random_lightning(r):
    return lightning_case(r)


def edge_cycles():
    r = random.Random(705)
    for value in (0, 1, 0xFFFFFFFF, 1 << 32, (1 << 32) + 1, 0x7FFFFFFF, 0x80000000, 0x123456789ABCDEF0, 0xFFFFFFFFFFFFFFFF, 0xDEADBEEF):
        c = new_case(OP_CYCLES)
        c["q"] = {"Cycles": [value]}
        yield c
    for _ in range(10):
        yield random_cycles(r)


def random_cycles(r):
    c = new_case(OP_CYCLES)
    c["q"] = {"Cycles": [r.getrandbits(64)]}
    return c


# ---- __RenderRain and __RenderSnow

def drops_world(r, c, count, scale):
    rain = c["rain"]
    rain[0x3318] = count & MASK
    for i in range(max(count, 0)):
        put_floats(rain, 8 + 12 * i, [rfl(r, 0.04, -scale, scale) for _ in range(3)])
    rain[0x330C] = r.choice([0, 2, 2, 2, 3, r.getrandbits(32)])
    put_floats(rain, 0x331C, [rfl(r, 0.1, -1.0, 1.0) for _ in range(3)])
    rain.update({0x9D58 + 4 * i: v for i, v in enumerate(rmat(r))})
    c["light"] = rand_light(r)
    c["disp"] = rand_display(r)
    c["vsize"] = r.choice([0x20, 0x20, 0x20, 0x18])
    c["current"] = r.choice([0, 1])
    c["recs"][0][7], c["recs"][0][8] = r.choice([0, 0, 0, 8]), r.choice([0, 0, 0, 12])
    c["recs"][0][11] = fb(rfl(r, 0.1, -10.0, 10.0))
    set_queues(r, c)
    c["q"]["TransformMode"] = [r.choice([0, 0, 1, 2, 3, 4, 5, 6, 7, 9, 12, 18, 25, r.randrange(48)])]
    c["q"]["GetNextPrimitive"] = [r.choice([1, 1, 1, 1, 1, 1, 0, 2])]
    c["q"]["ExecuteDepth"] = [r.choice([1, 1, 1, 0, 7, -1])]
    c["seed"] = r.getrandbits(32)
    world_limits(r, c)
    return c


def camera_args(r, scale=6.0):
    return [fb(rfl(r, 0.12, -scale, scale)) for _ in range(3)]


def rain_case(r, count=None, camera=None):
    count = r.choice([0, 1, 1, 2, 3, 5, 8, 13, 30, 60]) if count is None else count
    c = new_case(OP_RAIN, camera if camera is not None else camera_args(r))
    drops_world(r, c, count, 6.0)
    rain = c["rain"]
    put_floats(rain, 0x3314, [rfl(r, 0.1, 0.0, 1.0)])
    rain[0x3350] = r.choice([0, 1, 2, 3, 3])
    c["entries"] = [rand_entry(r) for _ in range(ENTRIES)]
    return c


def edge_rain():
    r = random.Random(706)
    identity = [fb(1.0 if i % 5 == 0 else 0.0) for i in range(16)]

    def one(count=3, **kw):
        c = rain_case(r, count=count, camera=[fb(0.25), fb(-0.5), fb(1.0)])
        c["rain"].update({0x9D58 + 4 * i: v for i, v in enumerate(identity)})
        c["disp"], c["light"] = kw.pop("disp", 0), kw.pop("light", MASK)
        c["q"]["TransformMode"] = [kw.pop("mode", 0)]
        c["q"]["GetNextPrimitive"] = [kw.pop("record", 1)]
        for k, v in kw.items():
            c[k] = v
        return c

    # The count of drops, up to what the primitive's memory takes and the arrays hold.
    for count in (0, 1, 2, 3, 4, 10, 50, 190, 200, 1024):
        c = one(count)
        for i in range(count):
            put_floats(c["rain"], 8 + 12 * i, [r.uniform(-3, 3), r.uniform(-3, 3), r.uniform(0.2, 3)])
        yield c
    # Every permutation and sign of the transform (which coordinate is the depth, which side up).
    for mode in range(48):
        c = one(6, mode=mode)
        for i in range(6):
            put_floats(c["rain"], 8 + 12 * i, [r.uniform(-3, 3), r.uniform(-3, 3), r.uniform(-3, 3)])
        yield c
    # The display's flag 2 chooses the blending mode; the texture entry's four states.
    for disp in (0, 2, 0x102, 0xFFFFFFFD):
        for texture in (0, 1, 2, 3):
            c = one(3, disp=disp)
            c["rain"][0x3350] = texture
            c["entries"] = [dict(flags=0x1000, page=3, uoff=0.25, voff=0.5, usize=0.125, vsize=0.0625),
                            dict(flags=0, page=4, uoff=0.1, voff=0.2, usize=0.3, vsize=0.4),
                            dict(flags=0x1000, page=0, uoff=0.5, voff=0.5, usize=0.25, vsize=0.25)]
            yield c
    # GetNextPrimitive refusing; the answer of ExecuteCurrentPrimitive; the flags it is given.
    for record in (0, 1, 2, 3, 4):
        yield one(3, record=record)
    for answer in (0, 1, 7, -1):
        c = one(3)
        c["q"]["ExecuteDepth"] = [answer]
        yield c
    for flags in (0, 1, 2, 3, 0xFFFFFFFF):
        c = one(2)
        c["rain"][0x330C] = flags
        yield c
    # The intensity of the streak: depth 0 and below (skipped), small, up to 6 and beyond (the grey is cut at 0).
    for z in (-1.0, 0.0, -0.0, 1e-30, 0.5, 1.0, 3.0, 5.99, 6.0, 6.01, 7.0, 3.9999, 20.0, 1e30, INF, NAN, -INF):
        c = one(1)
        put_floats(c["rain"], 8, [0.0, 0.0, z])
        c["q"]["TransformMode"] = [0]
        # a camera that leaves the point alone
        c["args"][0:3] = [fb(0.0), fb(0.0), fb(0.0)]
        yield c
    # The wrap of each coordinate at both edges and beyond (a camera at the origin leaves the point's value alone, so
    # the edges are hit exactly).
    for v in (4.0, 4.0001, 3.9999, -4.0, -4.0001, -3.9999, 12.0, -12.0, 13.0, -13.0, 100.0, NAN, INF, -INF, 0.0):
        for axis in range(3):
            c = one(1)
            coords = [0.5, 0.5, 0.5]
            coords[axis] = v
            put_floats(c["rain"], 8, coords)
            c["args"][0:3] = [fb(0.0)] * 3
            yield c
    # The camera's offset is taken from x and z and added to y.
    for camera in ([1.0, 2.0, 3.0], [-9.0, 9.0, 0.5], [0.0, 0.0, 0.0], [NAN, 1.0, 1.0], [1e30, -1e30, 1e30]):
        c = one(2)
        c["args"][0:3] = [fb(v) for v in camera]
        yield c
    # The streak: length and direction.
    for length in (0.0, 0.1, 1.0, -1.0, NAN, INF, 1e30):
        for direction in ((0.0, -1.0, 0.0), (1.0, 1.0, 1.0), (0.3, -0.4, 0.5), (NAN, 0.0, 1.0)):
            c = one(2)
            put_floats(c["rain"], 0x3314, [length])
            put_floats(c["rain"], 0x331C, list(direction))
            yield c
    # The light mask and the lights of the colour.
    for light in (MASK, 0, 0x80808080, 0xFF000000, 0x7F7F7F7F, 0x00FFFFFF):
        yield one(4, light=light)
    for _ in range(40):
        yield rain_case(r)


def random_rain(r):
    return rain_case(r)


def snow_case(r, count=None, cubic=None, camera=None):
    count = r.choice([0, 1, 1, 2, 3, 5, 8, 13, 17, 30, 60]) if count is None else count
    c = new_case(OP_SNOW, camera if camera is not None else camera_args(r))
    drops_world(r, c, count, 6.0)
    rain = c["rain"]
    cubic = (r.random() < 0.6) if cubic is None else cubic
    c["disp"] = (c["disp"] | 0x100) if cubic else (c["disp"] & ~0x100) & MASK
    c["aspect"] = fb(rfl(r, 0.1, 0.3, 2.0))
    put_floats(rain, 0x3354, [rfl(r, 0.1, 0.0, 0.5)])
    rain[0x3348] = r.choice([0, 1, 2, 3, 3])
    c["entries"] = [rand_entry(r) for _ in range(ENTRIES)]
    # The snow tables.
    n = max(count, 16)
    for i in range(n):
        rain[SNOW_TIMES + 4 * i] = fb(rfl(r, 0.04, 0.0, 90.0))
    for i in range(128 * 12):
        rain[SNOW_CUBICS + 4 * i] = fb(rfl(r, 0.01, -0.3, 0.3))
    put_floats(rain, 0x9D98, [rfl(r, 0.1, 0.0, 0.5)])
    put_floats(rain, 0xA18C, [rfl(r, 0.1, -1.0, 10.0) for _ in range(3)])
    return c


def zero_jitter_seed():
    """A seed for which the first three draws of the random jitter are zero (the low five bits of each number are 16)."""
    for seed in range(1 << 20):
        state, ok = seed, True
        for _ in range(3):
            state = (state * 1664525 + 1013904223) & MASK
            if ((state >> 16) & 0x7FFF) & 0x1F != 16:
                ok = False
                break
        if ok:
            return seed
    raise AssertionError("no seed found")


def edge_snow():
    r = random.Random(707)
    identity = [fb(1.0 if i % 5 == 0 else 0.0) for i in range(16)]

    def one(count=3, cubic=True, **kw):
        c = snow_case(r, count=count, cubic=cubic, camera=[fb(0.25), fb(-0.5), fb(1.0)])
        c["rain"].update({0x9D58 + 4 * i: v for i, v in enumerate(identity)})
        c["disp"] = (kw.pop("disp", 0) | (0x100 if cubic else 0)) & MASK
        c["light"] = kw.pop("light", MASK)
        c["q"]["TransformMode"] = [kw.pop("mode", 0)]
        c["q"]["GetNextPrimitive"] = [kw.pop("record", 1)]
        for k, v in kw.items():
            c[k] = v
        return c

    for cubic in (False, True):
        for count in (0, 1, 2, 3, 4, 15, 16, 17, 40, 250, 1024):
            c = one(count, cubic)
            for i in range(count):
                put_floats(c["rain"], 8 + 12 * i, [r.uniform(-3, 3), r.uniform(-3, 3), r.uniform(0.2, 3)])
            yield c
        for mode in range(0, 48, 3):
            c = one(8, cubic, mode=mode)
            for i in range(8):
                put_floats(c["rain"], 8 + 12 * i, [r.uniform(-3, 3), r.uniform(-3, 3), r.uniform(-3, 3)])
            yield c
        for texture in (0, 1, 2, 3):
            c = one(5, cubic)
            c["rain"][0x3348] = texture
            c["entries"] = [dict(flags=0x1000, page=3, uoff=0.25, voff=0.5, usize=0.125, vsize=0.0625),
                            dict(flags=0, page=4, uoff=0.1, voff=0.2, usize=0.3, vsize=0.4),
                            dict(flags=0x1000, page=0, uoff=0.5, voff=0.5, usize=0.25, vsize=0.25)]
            yield c
        for record in (0, 1, 2, 3, 4):
            yield one(3, cubic, record=record)
        for answer in (0, 1, 7, -1):
            c = one(3, cubic)
            c["q"]["ExecuteDepth"] = [answer]
            yield c
        for z in (-1.0, 0.0, -0.0, 1e-30, 0.5, 3.9999, 20.0, 1e30, INF, NAN, -INF):
            c = one(1, cubic)
            put_floats(c["rain"], 8, [0.0, 0.0, z])
            c["args"][0:3] = [fb(0.0), fb(0.0), fb(0.0)]
            yield c
        for v in (4.0, 4.0001, 3.9999, -4.0, -4.0001, -3.9999, 12.0, -12.0, 100.0, NAN, INF, -INF):
            for axis in range(3):
                c = one(1, cubic)
                coords = [0.5, 0.5, 0.5]
                coords[axis] = v
                put_floats(c["rain"], 8, coords)
                yield c
        for aspect in (0.0, 0.5, 1.0, 1.333, NAN, INF, -1.0):
            for size in (0.0, 0.1, 0.5, NAN, -0.2):
                c = one(3, cubic)
                c["aspect"] = fb(aspect)
                put_floats(c["rain"], 0x3354, [size])
                yield c
        for light in (MASK, 0, 0x80808080, 0x0000FFFF):
            yield one(3, cubic, light=light)
    # The wrap at its edges exactly: with the jitter drawn as zero (a seed found below) and the camera at the origin
    # the point keeps the value it was given.
    seed = zero_jitter_seed()
    for v in (4.0, 4.0001, 3.9999, -4.0, -4.0001, -3.9999, 12.0, -12.0, 0.5):
        for axis in range(3):
            c = one(1, False)
            coords = [0.5, 0.5, 0.5]
            coords[axis] = v
            put_floats(c["rain"], 8, coords)
            c["args"][0:3] = [fb(0.0)] * 3
            c["seed"] = seed
            yield c
    # The cubic tables: the time at the edges of fctiwz and of the index (modulo 128), the step, the parameters.
    for t in (0.0, 0.5, 1.0, 127.5, 128.0, 129.25, 255.99, -0.5, -1.0, -127.5, -128.25, 1e9, -1e9, 2147483647.0, 2147483648.0, 3e9, -3e9,
              1e30, -1e30, INF, -INF, NAN, 16777216.0, 16777217.0, 4e7):
        for dt in (0.0, 0.1, 1.0):
            c = one(3, True)
            for i in range(20):
                c["rain"][SNOW_TIMES + 4 * i] = fb(t + i * 0.37 if abs(t) < 1e8 else t)
            put_floats(c["rain"], 0x9D98, [dt])
            yield c
    for dt in (0.0, 0.016, 0.5, 1.0, 100.0, -0.1, NAN, INF, 1e30):
        c = one(20, True)
        put_floats(c["rain"], 0x9D98, [dt])
        yield c
    for params in ((0.4, 6.0, 13.0), (0.0, 0.0, 0.0), (1.0, 1.0, 1.0), (-1.0, 2.0, 3.0), (NAN, 1.0, 1.0), (1.0, NAN, 1.0), (1.0, 1.0, NAN),
                   (INF, 1.0, 1.0), (1e30, 1e30, 1e30)):
        c = one(20, True)
        put_floats(c["rain"], 0xA18C, list(params))
        yield c
    for direction in ((0.0, -1.0, 0.0), (1.0, 1.0, 1.0), (NAN, 0.0, 0.0), (0.2, -0.3, 0.4), (1e30, 1e30, -1e30)):
        c = one(20, True)
        put_floats(c["rain"], 0x331C, list(direction))
        yield c
    # The random jitter: the number of draws and their order.
    for seed in (0, 1, 0x7FFFFFFF, 0x80000000, MASK, 12345):
        c = one(10, False)
        c["seed"] = seed
        yield c
    for _ in range(40):
        yield snow_case(r)


def random_snow(r):
    return snow_case(r)


# ---- CRain::AddLightning and SLightning::Init

# Float bit patterns that matter to the bolt's arithmetic: zeros, units, values whose squares overflow or
# underflow in single precision, denormals, infinities and NaNs.
BOLT_SPECIAL = [0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x7149F2CA, 0xF149F2CA, 0x0DA24260, 0x00000001, 0x80000001,
                0x00400000, 0x7F7FFFFF, 0xFF7FFFFF, 0x7F800000, 0xFF800000, 0x7FC00000, 0xFFC00001, 0x7F800001, 0x5F800000,
                0x1F800000, 0x20000000, 0x5F7FFFFF, 0x1E800000, 0x43FA0000, 0xC3FA0000, 0x3F000000, 0x3F800001, 0x40490FDB]


def bolt_word(r, special=0.25):
    """A bolt end's coordinate: an edge pattern, a mantissa-rich value of some size, or any bits."""
    k = r.random()
    if k < special:
        return r.choice(BOLT_SPECIAL)
    if k < special + 0.55:
        return fb(r.uniform(-1.0, 1.0) * 10.0 ** r.randint(-6, 4))
    if k < special + 0.65:
        return r.getrandbits(32)
    return fb(float(r.randint(-600, 600)))


def bolt_args(r, special=0.25, seeded=None, time=None):
    """The eight arguments: start, end, the time and the seed flag."""
    words = [bolt_word(r, special) for _ in range(6)]
    if r.random() < 0.15:
        words[3:6] = words[0:3]                      # equal ends: a length of zero
    if r.random() < 0.1:
        words[3:6] = [w ^ 0x80000000 if r.random() < 0.5 else w for w in words[0:3]]
    t = r.choice([500, 1, 0, -1, 0x7FFFFFFF, -0x80000000, 1000, r.randint(-5000, 5000)]) if time is None else time
    s = r.choice([0, 1, 1, -1, 2, 0x80000000, 0x7FFFFFFF, 0x100, r.getrandbits(32)]) if seeded is None else seeded
    return words + [t & MASK, s & MASK]


def bolt_rain(r, c):
    """A rain object of random words around the bolts (so a slot's stale content shows) and a cycle counter."""
    rain = c["rain"]
    for offset in range(BOLTS - 0x60, BOLTS + 16 * BOLT_SIZE + 0x40, 4):
        rain[offset] = r.getrandbits(32)
    c["q"] = {"Cycles": [r.choice([0, 1, 0xFFFFFFFF, 1 << 32, r.getrandbits(64), 123456789, 0xFFFFFFFE, 0x7FFFFFFF, 0xFFFFFFFF00000001])]}


def init_case(r, args=None):
    c = new_case(OP_BOLT_INIT, args if args is not None else bolt_args(r))
    bolt_rain(r, c)
    return c


def add_case(r, count=None, args=None):
    c = new_case(OP_ADD_LIGHTNING, args if args is not None else bolt_args(r))
    bolt_rain(r, c)
    c["rain"][0x3308] = (r.choice([0, 1, 2, 7, 14, 15, 15, 16, 16, 17, 100, 0x7FFFFFFF, -1, -2, -100]) if count is None else count) & MASK
    return c


def edge_init():
    r = random.Random(801)
    one, zero, neg = fb(1.0), 0, fb(-1.0)
    # Unit and axis vectors, the same ends, both seeds, the time at the ends.
    for start, end in (([one, zero, zero], [zero, zero, zero]), ([zero, one, zero], [zero, zero, zero]), ([zero, zero, one], [zero, zero, zero]),
                       ([one, one, one], [zero, zero, zero]), ([zero, zero, zero], [zero, zero, zero]), ([one, zero, zero], [one, zero, zero]),
                       ([zero, zero, zero], [one, one, one]), ([fb(3.0), fb(4.0), zero], [zero, zero, zero]),
                       ([0x80000000] * 3, [zero] * 3), ([fb(5.0), fb(5.0), fb(5.0)], [fb(5.0), fb(5.0), fb(5.0)])):
        for seeded in (0, 1):
            yield init_case(r, start + end + [500, seeded])
    # Squares that overflow (length infinity, scale 0), underflow (length 0) or sit at the edge.
    for word in (0x7149F2CA, 0x5F800000, 0x5F7FFFFF, 0x7F7FFFFF, 0x1F800000, 0x1E800000, 0x0DA24260, 0x00000001, 0x00400000, 0x20000000):
        yield init_case(r, [word, zero, zero, zero, zero, zero, 7, 0])
        yield init_case(r, [zero, word, zero, zero, zero, zero, 7, 0])
        yield init_case(r, [zero, zero, word, zero, zero, zero, 7, 0])
        yield init_case(r, [word, word, word, zero, zero, zero, 7, 1])
    # Infinities and NaNs in each place (the NaN length is unequal to zero: the division runs).
    for word in (0x7F800000, 0xFF800000, 0x7FC00000, 0xFFC00001, 0x7F800001):
        for place in range(6):
            args = [one, zero, neg, zero, one, zero, 3, 1]
            args[place] = word
            yield init_case(r, args)
    # Inf minus inf, and a NaN end.
    yield init_case(r, [0x7F800000, zero, zero, 0x7F800000, zero, zero, 1, 0])
    yield init_case(r, [0x7F800000, zero, zero, 0xFF800000, zero, zero, 1, 0])
    # The fused sum: x*x + y*y rounds once. Triples (found by searching with the C library's fmaf) whose length differs between the
    # fused and the separately rounded sum, so that a plain multiply and add shows in the direction.
    fused = random.Random(802)
    found = 0
    while found < 50:
        x, y, z = (T.f32(fused.uniform(-3.0, 3.0) * 10.0 ** fused.randint(-2, 2)) for _ in range(3))
        yy, zz = T.f32(y * y), T.f32(z * z)
        together = T.f32(zz + float(LIBM.fmaf(x, x, yy)))
        apart = T.f32(zz + T.f32(T.f32(x * x) + yy))
        if T.f32(math.sqrt(together)) != T.f32(math.sqrt(apart)):
            found += 1
            yield init_case(r, [fb(x), fb(y), fb(z), 0, 0, 0, 100, 0])
    # The seed: the counter's low 32 bits ORed with 1, and zero for no seed; every time.
    for cycles in (0, 1, 2, 0xFFFFFFFF, 1 << 32, (1 << 32) + 2, 0x123456789ABCDEF0, 0xFFFFFFFFFFFFFFFF, 0x80000000, 0x7FFFFFFE):
        for seeded in (0, 1, 5, -1 & MASK, 0x80000000):
            c = init_case(r, [one, zero, zero, zero, zero, zero, 1, seeded])
            c["q"] = {"Cycles": [cycles]}
            yield c
    for t in (0, 1, -1, 500, 0x7FFFFFFF, -0x80000000, 0x80000001):
        yield init_case(r, [one, one, zero, zero, zero, one, t & MASK, 1])


def random_init(r):
    return init_case(r, bolt_args(r, special=r.choice([0.0, 0.1, 0.4])))


def edge_add():
    r = random.Random(803)
    unit = bolt_args(r, 0.0, seeded=1, time=500)
    # The count around the capacity, and negative (a signed compare adds to a slot below the array).
    for count in (0, 1, 2, 3, 8, 14, 15, 16, 17, 31, 32, 100, 0x7FFFFFFF, -1, -2, -3, -100, -256):
        for seeded in (0, 1):
            c = add_case(r, count=count, args=bolt_args(r, 0.0, seeded=seeded))
            yield c
    # The lobby's call: the ground point, then 500 above, then 500 and 1.
    for count in (0, 5, 15, 16):
        c = add_case(r, count=count, args=[fb(12.5), 0, fb(-30.25), fb(12.5), fb(500.0), fb(-30.25), 500, 1])
        yield c
    # Stale content of the slot and the argument words in the right places (distinct words everywhere).
    c = add_case(r, count=4, args=[0x3F800001, 0x40000001, 0x40400001, 0x40800001, 0x40A00001, 0x40C00001, 0x1234567, 3])
    yield c
    c = add_case(r, count=4, args=unit)
    yield c
    # Special vectors through the whole path.
    for _ in range(40):
        yield add_case(r, count=r.choice([0, 3, 15, 16]), args=bolt_args(r, 0.5))


def random_add(r):
    return add_case(r, args=bolt_args(r, special=r.choice([0.0, 0.1, 0.4])))


TESTS = [
    FnTest("CDrawPrimitive::AddPoly_TLCTEX", run_ppc, run_host, edge=edge_poly, rnd=random_poly),
    FnTest("CDrawPrimitive::DrawSprite", run_ppc, run_host, edge=edge_sprite, rnd=random_sprite),
    FnTest("CRain::__LightningFlash", run_ppc, run_host, edge=edge_flash, rnd=random_flash),
    FnTest("CRain::__RenderLightning", run_ppc, run_host, edge=edge_lightning, rnd=random_lightning),
    FnTest("GetCycleCount", run_ppc, run_host, edge=edge_cycles, rnd=random_cycles),
    FnTest("CRain::AddLightning", run_ppc, run_host, edge=edge_add, rnd=random_add),
    FnTest("SLightning::Init", run_ppc, run_host, edge=edge_init, rnd=random_init),
    FnTest("CRain::__RenderRain", run_ppc, run_host, edge=edge_rain, rnd=random_rain),
    FnTest("CRain::__RenderSnow", run_ppc, run_host, edge=edge_snow, rnd=random_snow),
]
