"""The last stand-ins of the lobby's frame (engine library, src/engine/tb_hfield.cpp,
src/engine/Gamut.cpp, src/engine/TextureCache.cpp and src/engine/RenderMemory.cpp), original versus translation:
CHField::TestConQuad, CycleFrequencyTable, CTextureLink::CalcUsage, CTextureCache::AutoMipmap,
Entry_Compress, BalanceCache_FreeLockedTextureSpace, BalanceCache_LevelSurfaceUsage and the render
memory's CRenderMemory::ExecuteUnrenderedPrimitives with its three sorts.

The render memory. A case is one call on a table of primitives (0x38-byte records on the Mac, described
by flags, blending mode, texture and depth), with the table's counts, the renderer, the display's flags
and the case's list of record numbers and range for the sorts. The fakes (the same on both sides) are
CDrawPrimitive::ExecutePrimitiveIfInstant (it marks the records the case names as drawn) and ExternExecute
(it answers from the case's list), the engine's malloc and free callbacks (free logs a hash of the block:
the sorted list), and CErrorHandler::Push. Every event records the record's number, flags, blending mode,
texture and depth.

Each case builds its world twice, in emulator memory and through tests/unit_FrameRest.cpp, from one
description, runs the function on both and compares what comes out: the answer, the recorded calls of
the fakes, the words the fakes were given, and the state of everything the call could change.

TestConQuad. The world is a height field object with a record (origin, cell size, lowest and highest
height), its rows of grid points in view, the far plane and a box of cells. The fakes (the same on both
sides) are the math unit's strided Transform, CClipping::CPP_TestData and the assertion. Transform writes
the third word of each transformed corner from the case's list of depths (cyclic) and the other three from
the words it was given; CPP_TestData writes the AND and OR of the eight outside codes from the case's list
of pairs and then, so that a box is cut in four any number of times, from a hash of the corners it was
given and two thresholds. Both dump every word they were given, so a wrong corner shows in the dump as
well as in the log. A missing math unit asserts and then reads through the NULL: the PowerPC side faults
there (the test checks the fault is at an address below 0x1000) and the host's assertion throws.

The cache functions. A case is one call on a small world (the cache with six rows of pages, up to 96 pages
with a link and an OpenGL texture each, up to 480 entries, four spare place records, an array of entries)
described as a flat list; the host library reads the list, the PowerPC side is written into emulator memory.
The fakes (the same on both sides, recording every argument) are the entries' StoreAsWrapping (a first and a
later answer per entry), CTextureBase::SetFlags, the texture's GetPixelCount, the cache's
Entry_Specific_Compress (it can change mF18C4, mF18E0, the flags and mF18E4), BalanceCache_ClearTexture (it can
empty the page), SortTextures, SortEntries and the assertion. The OpenGL texture's constructor and
CTextureBase::IsPositionLocked, IsWrapping and IsUsingMaxCompression run as the original. After the call the
cache, every page and texture, every entry, the spare places and the array are compared, with the log.

Unicorn computes fmadds in double precision and rounds twice (FINDINGS 2026-10-10 and 2026-10-11);
HardwareRounding (one code hook per fused instruction) gives each the processor's single rounding, and
corrects fctiwz of a value above INT32_MAX (the processor saturates to 0x7fffffff, Unicorn gives
0x80000000). NaNs are one value wherever a word is dumped, mixed or compared.

Not tested: TestConQuad with rows outside 0..127 (the original writes through the bytes in front of or behind
the row arrays: big-endian words on the Mac, little-endian here) and boxes that make the recursion run for
ever; BalanceCache_LevelSurfaceUsage where the original overwrites its own lists (a limit of 16 with two or more
candidate pages) or runs memcpy with a negative size (a negative limit with a candidate page): the pages of such
sizes are locked (make_level_safe), so none is a candidate. FreeLockedTextureSpace never has more than 16 pages
qualify with a limit of 16 or more.
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
SHIM_LIBRARY = "unit_FrameRest"
MASK = 0xffffffff
NAN, INF = math.nan, math.inf

# ---- the functions and the engine globals
TEST_CON_QUAD, TEST_CON_QUAD_SIZE = 0x1000dd14, 1024
CYCLE, CYCLE_SIZE = 0x10009770, 360
CPP_TEST_DATA = 0x1003f6dc
SYSTEM, MATH_VAR, FINAL_VAR, CLIP_VAR = 0x10165f80, 0x100afe94, 0x100aff70, 0x102b3638
DISPLAY, FREQ_INDEX, FREQ_TABLE = 0x10165634, 0x10052d74, 0x10052d78
MALLOC_SLOT, FREE_SLOT, DRAW_VAR = 0x10051f70, 0x10051f74, 0x10052b3c
GUARDED = [(SYSTEM, 0x40), (MATH_VAR, 4), (FINAL_VAR, 4), (CLIP_VAR, 4), (DISPLAY, 4), (DISPLAY + 0x80, 4), (FREQ_INDEX, 68),
           (MALLOC_SLOT, 8), (DRAW_VAR, 4)]

FIELD_SIZE = 0x12974        # CHField on the Mac
RECORD_SIZE = 0x30          # sHField
POISON = 0xa5a5a5a5


def fb(x):
    """A float's bits, as the processor holds it."""
    return struct.unpack(">I", struct.pack(">f", T.f32(x)))[0]


def canon(w):
    """Any NaN as one value."""
    return 0x7fc00000 if w & 0x7f800000 == 0x7f800000 and w & 0x007fffff else w


def ws(e, a, values):
    e.write(a, struct.pack(">%dI" % len(values), *[v & MASK for v in values]))


def rs(e, a, n):
    return list(struct.unpack(">%dI" % n, e.read(a, 4 * n)))


def signed64(v):
    v &= 0xFFFFFFFFFFFFFFFF
    return v - (1 << 64) if v >= (1 << 63) else v


def s8list(values):
    return [struct.unpack("b", struct.pack("B", v & 0xff))[0] for v in values]


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
    test and nowhere else (as in test_TextureSort.py and test_HFieldHelpers.py):
    - fmadds, fmsubs, fnmadds and fnmsubs are computed in double precision and rounded to single, which can
      differ by one unit in the last place from the processor's single rounding of the exact result; the
      result is replaced by the C library's fmaf;
    - fctiwz of a value above INT32_MAX (or +infinity) gives 0x80000000; the processor saturates to
      0x7fffffff.
    A code hook looks at each instruction before it runs and rewrites its target register just before the
    next one. ranges: the (begin, end) address ranges of the functions under test."""

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
                uc.reg_write(ppcemu.FPR[target], (fpr(e, target) & 0xffffffff00000000) | 0x7fffffff)
            else:
                corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
                if fpr(e, target) != corrected and not (value != value and fpr_double(e, target) != fpr_double(e, target)):
                    HardwareRounding.changed += 1
                uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        if word >> 26 == 63 and (word >> 1) & 0x3ff == 15:          # fctiwz frD, frB
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


# ----------------------------------------------------------------- the PowerPC side: common


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    # The math unit: an object with the vtable pointer at +4; the strided Transform is the slot at vtable
    # +0x10 (a pointer to a function descriptor), called through __ptr_glue.
    vtable = T.persist_alloc(e, 0x40)
    e.fr_math = T.persist_alloc(e, 0x10)
    e.fr_side = None
    e.write(e.fr_math + 4, struct.pack(">I", vtable))
    e.write(vtable + 0x10, struct.pack(">I", T.fake_function(e, lambda em: em.fr_side.transform(em))))
    # The engine's malloc and free callbacks (function descriptors the slots 0x10051f70 and 0x10051f74 hold), and the
    # draw primitive object (any address: only its label is logged).
    e.fr_render = None
    e.fr_malloc = T.fake_function(e, lambda em: em.fr_render.malloc(em))
    e.fr_free = T.fake_function(e, lambda em: em.fr_render.free(em))
    e.fr_draw = T.persist_alloc(e, 0x40)


@contextlib.contextmanager
def guarded(e):
    """The engine globals a case overwrites, put back afterwards (other modules share them)."""
    saved = [(a, e.read(a, n)) for a, n in GUARDED]
    try:
        yield
    finally:
        for a, data in saved:
            e.write(a, data)


def host_dump(lib):
    n = lib.tpw_fr_dump_n()
    dump = (C.c_uint32 * max(n, 1))()
    lib.tpw_fr_dump_copy(dump)
    return list(dump)[:n]


def bind(lib):
    """argtypes of the unit's calls (once per library)."""
    if getattr(lib, "fr_bound", False):
        return
    lib.fr_bound = True
    lib.tpw_fr_pointer.restype = C.c_void_p
    lib.tpw_fr_pointer.argtypes = [C.c_int32]
    lib.tpw_fr_dump_n.restype = C.c_uint32
    lib.tpw_fr_dump_copy.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_fr_conquad.restype = C.c_int32
    lib.tpw_fr_conquad.argtypes = [C.c_void_p, C.c_void_p, C.c_int32]
    lib.tpw_fr_cycle.argtypes = [C.c_int32, C.c_uint32, C.POINTER(C.c_int32), C.c_void_p]
    lib.tpw_fr_run.restype = C.c_int64
    lib.tpw_fr_run.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p, C.c_int32]
    lib.tpw_fr_count.restype = C.c_int32
    lib.tpw_fr_draw_pointer.restype = C.c_void_p
    lib.tpw_fr_render.restype = C.c_int32
    lib.tpw_fr_render.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_int32]


# ================================================================= CHField::TestConQuad

LABEL_MATH, LABEL_MATRIX, LABEL_CLIP = 1, 2, 3


def mix_transform(w, depth, i):
    return [(w[0] * 3 + 1) & MASK, (w[1] * 5 + 2) & MASK, depth & MASK, (w[0] + w[1] + w[2] + i) & MASK]


def hash_words(words):
    h = 0
    for w in words[:32]:
        h = (h * 1000003 + w) & MASK
    return h


class ConQuadFakes:
    """The math unit's Transform and CClipping::CPP_TestData on the PowerPC side: the same events, dumps
    and results as tests/unit_FrameRest.cpp."""

    def __init__(self, e, fixed, c):
        self.e, self.L, self.dump, self.c = e, CL.PpcLog(fixed), [], c
        self.depth_at = 0
        self.calls = 0

    def stubs(self):
        st = CL.Stubs()
        st.internal(CPP_TEST_DATA, self.test_data)
        st.imp("__assertion_failed",
               lambda e: self.L.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))
        return st

    def transform(self, e):
        L, depths = self.L, self.c["depths"]
        this, matrix, out, source = e.r(3), e.r(4), e.r(5), e.r(6)
        out_stride, in_stride, count, kind = T.s32(e.r(7)), T.s32(e.r(8)), T.s32(e.r(9)), T.s32(e.r(10))
        L.rec("Transform", (L.lab(this), L.lab(matrix), out_stride, in_stride, count, kind))
        for i in range(count):
            w = [canon(x) for x in rs(e, source + i * in_stride, 3)]
            self.dump.extend(w)
            depth = depths[self.depth_at % len(depths)] if depths else 0
            self.depth_at += 1
            ws(e, out + i * out_stride, mix_transform(w, depth, i))

    def test_data(self, e):
        L, c = self.L, self.c
        result, clipping, codes, vertices = e.r(3), e.r(4), e.r(5), e.r(6)
        count, stride, mask = T.s32(e.r(7)), T.s32(e.r(8)), e.r(9)
        L.rec("CPP_TestData", (L.lab(clipping), count, stride, mask))
        words = []
        for i in range(count):
            w = [canon(x) for x in rs(e, vertices + i * stride, 4)]
            self.dump.extend(w)
            words.extend(w)
            e.write(codes + i, bytes([((w[2] >> 7) & 0xff) & mask]))
        call, self.calls = self.calls, self.calls + 1
        answers = c["answers"]
        if call < len(answers):
            anded, ored = answers[call]
        else:
            h = hash_words(words)
            anded = 0x20 if (h & 0xff) < c["thresholds"][0] else 0
            ored = 0x04 if ((h >> 8) & 0xff) < c["thresholds"][1] else 0
        e.write(result, bytes([anded & 0xff, ored & 0xff]))


def conquad_case(r=None, box=(0, 0, 1, 1), rec=None, f1c=100.0, f20=0x7f, f24=-1, low=None, high=None, depths=None,
                 answers=(), thresholds=(0, 0), no_math=False):
    return dict(rec=rec or dict(ox=-10.0, oz=-20.0, cx=2.0, cz=2.0, lo=-5.0, hi=50.0), f1c=f1c, f20=f20, f24=f24,
                low=list(low) if low is not None else [0x7f] * 128, high=list(high) if high is not None else [-1] * 128,
                box=tuple(box), depths=list(depths) if depths is not None else [fb(v) for v in (1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0)],
                answers=[tuple(a) for a in answers], thresholds=tuple(thresholds), no_math=no_math)


def ppc_conquad(e, c):
    with guarded(e):
        field = e.alloc(FIELD_SIZE, b"\xa5" * FIELD_SIZE)
        record = e.alloc(RECORD_SIZE, b"\xa5" * RECORD_SIZE)
        rec = c["rec"]
        ws(e, record + 8, [fb(rec["ox"]), fb(rec["oz"]), fb(rec["cx"]), fb(rec["cz"])])
        ws(e, record + 0x20, [fb(rec["lo"]), fb(rec["hi"])])
        ws(e, field, [record])
        ws(e, field + 0x1c, [fb(c["f1c"]) if isinstance(c["f1c"], float) else c["f1c"], c["f20"], c["f24"]])
        e.write(field + 0x28, struct.pack("128b", *s8list(c["low"])) + struct.pack("128b", *s8list(c["high"])))
        matrix, clip = e.alloc(0x40, b"\xa5" * 0x40), e.alloc(8, b"\xa5" * 8)
        ws(e, MATH_VAR, [0 if c["no_math"] else e.fr_math])
        ws(e, FINAL_VAR, [matrix])
        ws(e, CLIP_VAR, [clip])
        side = ConQuadFakes(e, {e.fr_math: LABEL_MATH, matrix: LABEL_MATRIX, clip: LABEL_CLIP}, c)
        field_before, record_before = e.read(field, FIELD_SIZE), e.read(record, RECORD_SIZE)
        e.fr_side = side
        stopped = 0
        stubs = side.stubs()
        stubs.real = {TEST_CON_QUAD}
        saved = stubs.activate(e)
        try:
            with HardwareRounding(e, [(TEST_CON_QUAD, TEST_CON_QUAD + TEST_CON_QUAD_SIZE)]):
                T.call_ppc(e, TEST_CON_QUAD, [field] + [v & MASK for v in c["box"]])
        except RuntimeError:
            # the original reads through the NULL math unit after the assertion: the emulator faults there
            assert c["no_math"] and e.last_fault is not None and e.last_fault[1] < 0x1000, e.last_fault
            stopped = 1
        finally:
            e.fr_side = None
            CL.Stubs.deactivate(e, saved)
        after = e.read(field, FIELD_SIZE)
        state = [stopped, canon(e.u32(field + 0x1c)), T.s32(e.u32(field + 0x20)), T.s32(e.u32(field + 0x24))]
        state += list(struct.unpack("256b", after[0x28:0x128]))
        state += [int(field_before[:0x1c] + field_before[0x128:] == after[:0x1c] + after[0x128:]),
                  int(e.read(record, RECORD_SIZE) == record_before)]
        return {"state": state, "log": side.L.log, "dump": side.dump}


def conquad_description(c):
    rec = c["rec"]
    d = [fb(rec["ox"]), fb(rec["oz"]), fb(rec["cx"]), fb(rec["cz"]), fb(rec["lo"]), fb(rec["hi"])]
    d += [fb(c["f1c"]) if isinstance(c["f1c"], float) else c["f1c"], c["f20"], c["f24"]]
    d += s8list(c["low"]) + s8list(c["high"]) + list(c["box"]) + [1 if c["no_math"] else 0]
    d += [len(c["depths"])] + list(c["depths"])
    d += [len(c["answers"])] + [v for pair in c["answers"] for v in pair]
    d += list(c["thresholds"])
    return d


def host_conquad(lib, c):
    bind(lib)
    CL.host_begin(lib, {LABEL_MATH: lib.tpw_fr_pointer(0), LABEL_MATRIX: lib.tpw_fr_pointer(1),
                        LABEL_CLIP: lib.tpw_fr_pointer(2)})
    description = conquad_description(c)
    d = (C.c_int64 * len(description))(*[signed64(v) for v in description])
    out = (C.c_int64 * 512)()
    n = lib.tpw_fr_conquad(C.addressof(d), C.addressof(out), 512)
    assert n == 4 + 256 + 2, n
    return {"state": [int(v) for v in out[:n]], "log": CL.host_end(lib), "dump": host_dump(lib)}


# ---- the cases

CELL_SIZES = [(1.0, 1.0), (2.0, 2.0), (2.0, 3.0), (0.5, 0.5), (8.0, 8.0), (1.0, 0.25), (-1.0, 1.0), (3.5, -2.5)]
HEIGHTS = [(-5.0, 50.0), (0.0, 0.0), (-100.0, 100.0), (7.0, -7.0), (-1.0, 1.0), (0.0, 255.0)]
DEPTH_SETS = [
    [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0],
    [200.0] * 8,
    [-3.0, -2.0, -1.0, 0.0, 0.5, 99.5, 100.0, 100.5],
    [100.0] * 8,
    [NAN] * 8,
    [INF, -INF, 1.0, NAN, 0.0, -0.0, 3.0e38, 1.0e-40],
]


def rdepths(r, n=8):
    out = []
    for _ in range(n):
        k = r.random()
        if k < 0.15:
            out.append(r.choice([100.0, 99.999, 100.001, 0.0, -0.0, INF, -INF, NAN, 1.0e30, -1.0e30]))
        elif k < 0.6:
            out.append(T.f32(r.uniform(0.0, 250.0)))
        else:
            out.append(T.f32(r.uniform(-50.0, 120.0)))
    return [fb(v) for v in out]


def rbytes(r, n=128, special=0.3):
    out = []
    for _ in range(n):
        k = r.random()
        if k < special:
            out.append(r.choice([0x7f, -1, 0, 1, -128, 127, 64, -64]))
        else:
            out.append(r.randint(-128, 127))
    return out


def random_record(r):
    cx, cz = r.choice(CELL_SIZES)
    lo, hi = r.choice(HEIGHTS)
    ox = r.choice([-10.0, 0.0, 33.25, -4096.0, T.f32(r.uniform(-300.0, 300.0))])
    oz = r.choice([-20.0, 0.0, 12.5, 1000.0, T.f32(r.uniform(-300.0, 300.0))])
    if r.random() < 0.06:
        ox = r.choice([NAN, INF, -INF, 1.0e30, 3.3333333e-39])
    if r.random() < 0.06:
        cx = r.choice([NAN, INF, 0.0, -0.0, 1.0e-30])
    if r.random() < 0.06:
        lo = r.choice([NAN, INF, -INF, -0.0])
    if r.random() < 0.06:
        hi = r.choice([NAN, INF, -INF, 1.0e30])
    return dict(ox=ox, oz=oz, cx=cx, cz=cz, lo=lo, hi=hi)


def random_box(r, big=False):
    if big:
        w, h = r.randint(1, 28), r.randint(1, 28)
    else:
        w, h = r.choice([0, 1, 1, 2, 3, 4, 5, 7, 8, 11, 16]), r.choice([0, 1, 1, 2, 3, 4, 5, 7, 8, 11, 16])
    x0 = r.choice([0, 1, 5, 17, 60, r.randint(0, 127 - w)])
    z0 = r.choice([0, 1, 5, 17, 60, r.randint(0, 127 - h)])
    x0, z0 = min(x0, 127 - w), min(z0, 127 - h)
    return (x0, z0, x0 + w, z0 + h)


def conquad_random(r):
    big = r.random() < 0.12
    c = conquad_case(box=random_box(r, big), rec=random_record(r))
    c["f1c"] = r.choice([100.0, 100.0, 0.0, -5.0, 0.5, 99.999, INF, NAN, -INF, T.f32(r.uniform(0.0, 300.0))])
    k = r.random()
    if k < 0.5:
        c["f20"], c["f24"] = 0x7f, -1
        c["low"], c["high"] = [0x7f] * 128, [-1] * 128
    elif k < 0.8:
        c["f20"], c["f24"] = r.choice([0x7f, 0, 3, 50, 127, -1]), r.choice([-1, 0, 3, 50, 127, 126])
        c["low"], c["high"] = rbytes(r), rbytes(r)
    else:
        c["f20"], c["f24"] = r.randint(-5, 130), r.randint(-5, 130)
        c["low"], c["high"] = rbytes(r, special=0.8), rbytes(r, special=0.8)
    c["depths"] = rdepths(r, r.choice([1, 2, 8, 8, 8, 16, 24]))
    k = r.random()
    if big:
        c["thresholds"] = (r.choice([30, 60, 100]), r.choice([100, 160, 220]))
    elif k < 0.3:
        c["thresholds"] = (0, 0)
    elif k < 0.6:
        c["thresholds"] = (r.choice([0, 20, 60, 128, 255]), 0)
    else:
        c["thresholds"] = (r.choice([0, 20, 60, 128]), r.choice([64, 128, 200, 255]))
    n = r.choice([0, 0, 1, 2, 3, 5])
    c["answers"] = [(r.choice([0, 0, 0, 0x20, 1, 0xff]), r.choice([0, 4, 1, 0xff])) for _ in range(n)]
    return c


def conquad_edges():
    r = random.Random(7)
    cases = []
    # The three answers on one cell: out of view, wholly in view, partly in view (a single cell is never cut).
    for answers in ([(0x20, 0x04)], [(0xff, 0xff)], [(1, 0)], [(0, 0)], [(0, 4)], [(0, 0xff)]):
        for box in ((3, 4, 4, 5), (0, 0, 1, 1), (126, 126, 127, 127)):
            cases.append(conquad_case(box=box, answers=answers))
    # Wholly in view: bigger boxes widen the rows and the far plane.
    for box in ((0, 0, 4, 4), (10, 20, 12, 26), (5, 5, 5, 9), (5, 5, 9, 5), (127, 0, 127, 127), (0, 0, 127, 127), (30, 40, 31, 40)):
        cases.append(conquad_case(box=box, answers=[(0, 0)]))
    # Partly in view, cut in four: every shape from 2 by 1 up, with the answers given for the first calls only
    # and the rest accepted whole.
    for box in ((0, 0, 2, 1), (0, 0, 1, 2), (0, 0, 2, 2), (0, 0, 3, 3), (4, 4, 6, 9), (0, 0, 3, 1), (0, 0, 1, 3),
                (10, 10, 13, 17), (0, 0, 8, 8), (20, 3, 25, 4), (7, 7, 8, 13), (1, 1, 2, 2)):
        cases.append(conquad_case(box=box, answers=[(0, 4)]))
        cases.append(conquad_case(box=box, answers=[(0, 4), (0x20, 4), (0, 0), (0, 4), (0, 0)]))
        cases.append(conquad_case(box=box, thresholds=(60, 200)))
        cases.append(conquad_case(box=box, thresholds=(0, 255)))
    # Boxes with no width or no depth are never cut.
    for box in ((5, 5, 5, 5), (5, 5, 5, 9), (5, 5, 9, 5)):
        cases.append(conquad_case(box=box, answers=[(0, 4)]))
        cases.append(conquad_case(box=box, answers=[(0, 0)]))
    # Boxes the wrong way round: the rows loop does not run, the product of the extents is 1 for two
    # negative ones, and for a product that wraps to 1.
    for box in ((5, 5, 4, 4), (5, 5, 3, 3), (9, 9, 8, 8), (5, 5, 4, 9), (5, 9, 9, 5), (5, 5, 6, 4)):
        for answers in ([(0, 4)], [(0, 0)]):
            cases.append(conquad_case(box=box, answers=answers))
    cases.append(conquad_case(box=(0, 0, 3, -1431655765), answers=[(0, 4)]))
    cases.append(conquad_case(box=(0, 0, -1431655765, 3), answers=[(0, 4)]))
    # The rows: existing values below, equal to and above the box, for the low column and the high column.
    for existing_low, existing_high in ((0x7f, -1), (3, 9), (4, 8), (5, 7), (-2, 20), (4, 10), (127, -128), (-128, 127)):
        for f20, f24 in ((0x7f, -1), (5, 8), (4, 9), (6, 7), (-1, 200), (10, 3)):
            low = [existing_low] * 128
            high = [existing_high] * 128
            cases.append(conquad_case(box=(4, 5, 8, 8), answers=[(0, 0)], low=low, high=high, f20=f20, f24=f24))
    # Columns beyond a byte: stored as the low byte, compared as the whole number.
    for box in ((126, 3, 130, 5), (200, 3, 203, 4), (-5, 3, -2, 4), (-130, 3, -127, 4), (255, 3, 256, 4), (127, 3, 128, 4)):
        cases.append(conquad_case(box=box, answers=[(0, 0)]))
        cases.append(conquad_case(box=box, answers=[(0, 0)], low=[-128] * 128, high=[127] * 128))
        cases.append(conquad_case(box=box, answers=[(0, 0)], low=[100] * 128, high=[-100] * 128))
    # Cell numbers over 2^24 do not convert exactly.
    for x0 in (0x1000001, 0x1000003, 0x7ffffff, 16777217, 33554435, 0x40000007, -16777217, -33554433):
        cases.append(conquad_case(box=(x0, 5, x0 + 1, 6), answers=[(0, 0)], rec=dict(ox=1.5, oz=0.25, cx=0.3333333, cz=3.0, lo=0.0, hi=10.0)))
        cases.append(conquad_case(box=(x0, 5, x0 + 3, 6), answers=[(0, 4), (0, 0)]))
    # The far plane: each corner's depth in turn above, equal to and below it; NaN and infinities.
    for position in range(8):
        for value in (150.0, 100.0, 99.0, INF, NAN, -INF, 100.0000076):
            depths = [fb(50.0)] * 8
            depths[position] = fb(value)
            cases.append(conquad_case(box=(2, 2, 3, 3), answers=[(0, 0)], depths=depths))
    for f1c in (100.0, 0.0, -1.0, INF, -INF, NAN, 0.5):
        for depths in DEPTH_SETS:
            cases.append(conquad_case(box=(2, 2, 4, 4), answers=[(0, 0)], f1c=f1c, depths=[fb(v) for v in depths]))
    # Depth lists longer than one call take: the far plane follows the corners of every accepted piece.
    cases.append(conquad_case(box=(0, 0, 4, 4), thresholds=(0, 255), depths=rdepths(r, 24)))
    cases.append(conquad_case(box=(0, 0, 4, 4), thresholds=(30, 255), depths=rdepths(r, 40)))
    # Records: the lowest and highest height, cell sizes, special values.
    for record in (dict(ox=0.0, oz=0.0, cx=1.0, cz=1.0, lo=0.0, hi=0.0), dict(ox=NAN, oz=0.0, cx=1.0, cz=1.0, lo=-1.0, hi=1.0),
                   dict(ox=0.0, oz=INF, cx=1.0, cz=1.0, lo=-1.0, hi=1.0), dict(ox=0.0, oz=0.0, cx=NAN, cz=1.0, lo=-1.0, hi=1.0),
                   dict(ox=0.0, oz=0.0, cx=1.0, cz=-0.0, lo=NAN, hi=1.0), dict(ox=1.0e30, oz=0.0, cx=1.0e30, cz=1.0, lo=-1.0, hi=INF),
                   dict(ox=-0.0, oz=-0.0, cx=-0.0, cz=-0.0, lo=-0.0, hi=-0.0), dict(ox=3.0e-39, oz=1.0e-40, cx=1.0e-38, cz=5.0e-39, lo=1.0e-40, hi=-1.0e-40)):
        cases.append(conquad_case(box=(2, 3, 5, 6), rec=record, answers=[(0, 4)]))
        cases.append(conquad_case(box=(2, 3, 3, 4), rec=record, answers=[(0, 0)]))
    # Random fused-multiply stress: the position of a corner whose cell, size and origin make the single and the
    # double rounding differ.
    for _ in range(12):
        rec = dict(ox=T.f32(r.uniform(-1.0e3, 1.0e3)), oz=T.f32(r.uniform(-1.0e3, 1.0e3)), cx=T.f32(r.uniform(0.01, 9.0)),
                   cz=T.f32(r.uniform(0.01, 9.0)), lo=-1.0, hi=1.0)
        cases.append(conquad_case(box=(r.randint(0, 100), r.randint(0, 100), r.randint(101, 127), r.randint(101, 127)), rec=rec,
                                  answers=[(0, 0)]))
    # No math unit: the assertion (tb_matrix.h line 0x98) and then a fault.
    for box in ((0, 0, 1, 1), (3, 3, 9, 9), (5, 5, 4, 4)):
        cases.append(conquad_case(box=box, no_math=True))
    # All eight outside at the first level, nothing changed; and the partly-outside trees at depth.
    cases.append(conquad_case(box=(0, 0, 16, 16), answers=[(0x20, 0x04)]))
    cases.append(conquad_case(box=(0, 0, 16, 16), thresholds=(255, 255)))
    cases.append(conquad_case(box=(0, 0, 16, 16), thresholds=(40, 255)))
    cases.append(conquad_case(box=(0, 0, 32, 32), thresholds=(90, 255)))
    cases.append(conquad_case(box=(0, 0, 127, 127), thresholds=(250, 255)))
    return cases


# ================================================================= CycleFrequencyTable


def ppc_cycle(e, c):
    with guarded(e):
        ws(e, DISPLAY + 0x80, [c["frame"]])
        ws(e, FREQ_INDEX, [c["index"]] + list(c["table"]))
        stubs = CL.Stubs()
        stubs.real = {CYCLE}
        saved = stubs.activate(e)
        try:
            T.call_ppc(e, CYCLE, [])
        finally:
            CL.Stubs.deactivate(e, saved)
        return {"state": [e.u32(FREQ_INDEX)] + [T.s32(v) for v in rs(e, FREQ_TABLE, 16)]}


def host_cycle(lib, c):
    bind(lib)
    table = (C.c_int32 * 16)(*[T.s32(v) for v in c["table"]])
    out = (C.c_int64 * 17)()
    lib.tpw_fr_cycle(T.s32(c["frame"]), c["index"], table, C.addressof(out))
    return {"state": [int(v) for v in out]}


def cycle_case(frame, index=0xdeadbeef, table=None):
    return dict(frame=frame & MASK, index=index, table=list(table) if table is not None else [0x5a5a5a5a] * 16)


def cycle_edges():
    cases = [cycle_case(f) for f in list(range(0, 40)) + [255, 256, 0x7fffffff, 0x80000000, 0xffffffff, 0xfffffff0, 0xfffffff1,
                                                           1000000, 123456789]]
    cases.append(cycle_case(7, index=3, table=list(range(16))))
    cases.append(cycle_case(7, index=15, table=[-1] * 16))
    return cases


def cycle_random(r):
    return cycle_case(r.getrandbits(32), index=r.getrandbits(32), table=[r.getrandbits(32) for _ in range(16)])


# ================================================================= the cache functions
# Pointers in the description are numbers: an entry 1..N, a page 1..N (0: null), a spare place 1..4 (0: the entry's
# own). A case is one call on a small world: the cache with six rows of pages, up to 96 pages (a link and an OpenGL
# texture each), up to 480 texture entries, four spare place records and an array of entries.

LINKS_MAX, ENTRIES_MAX, PLACES = 96, 480, 4
CALC_USAGE, CALC_USAGE_SIZE = 0x10038670, 544
AUTO_MIPMAP, AUTO_MIPMAP_SIZE = 0x10035244, 120
ENTRY_COMPRESS, ENTRY_COMPRESS_SIZE = 0x1003514c, 176
FREE_LOCKED, FREE_LOCKED_SIZE = 0x1003780c, 392
LEVEL_USAGE, LEVEL_USAGE_SIZE = 0x100379e0, 2024
FUNCS = {CALC_USAGE, AUTO_MIPMAP, ENTRY_COMPRESS, FREE_LOCKED, LEVEL_USAGE}
STORE_WRAP, SET_FLAGS, GET_PIXELS = 0x10041848, 0x10032bd0, 0x10046a94
SPECIFIC_COMPRESS, CLEAR_TEXTURE, SORT_TEXTURES, SORT_ENTRIES = 0x10034e94, 0x1003770c, 0x1003335c, 0x10035300
# The members they reach that run as the original: the OpenGL texture's and CTextureBase's constructors and
# CTextureBase::IsPositionLocked, IsWrapping and IsUsingMaxCompression.
INNER = [0x10045d88, 0x10032b58, 0x10032c14, 0x10032c58, 0x10032c94]

# Entry flag bits.
ONPAGE, QUEUED, LOCKED, DROPPED = 0x1000, 0x2000, 0x4000, 0x8000
BLEND, ASKED, COMPRESSED = 0x80000, 0x40000000, 0x10000000
RESHUFFLE = 1                    # cache flag
LEVEL1, LEVEL2, LEVEL3 = 0x10, 0x20, 0x30
PAGE_SIDES = [256, 128, 64, 32, 16, 8]


def blank_place(rnd=None):
    if rnd is None:
        return dict(bytes=[0] * 16, f10=0, f14=0)
    return dict(bytes=[rnd.choice([0, 1, 2, 0xfe, 0xff, rnd.randrange(256)]) for _ in range(16)],
                f10=rnd.choice([0, 1, -1, 17, 255, 4080, 0x7fffffff, -0x80000000, rnd.randint(-1000, 1000)]),
                f14=rnd.choice([0, 1, 0x8000, 0x7fff, -1, -2, -0x80000000, rnd.randint(-0x80000, 0x80000)]))


def blank_entry():
    return dict(flags=0, w=32, h=32, cw=32, ch=32, page=0, texture=0, usize=0.0, vsize=0.0, uoff=0.0, voff=0.0,
                prio=1.0, pitch=32, sortkey=0, next=0, wrap=0, place=0, own=blank_place())


def blank_link():
    return dict(entries=0, f08=0, f0c=0, f10=0, f14=-1, f18=0, f1c=0, tflags=0, tw=256, th=256, tpitch=1024, pixels=0)


def no_effect():
    return (0, 0, 0, 0, 0, 0, 0, 0)


def blank_world(nl=0, ne=0):
    return dict(frame=0, renderer=3, clear=0, comp=[],
                cache=dict(flags=0, f18e0=0, f18e4=0, f18bc=0, f18c0=0, f18c4=0, f1860=4, f1818=[0] * 6),
                rows=[[] for _ in range(6)], links=[blank_link() for _ in range(nl)], places=[blank_place() for _ in range(PLACES)],
                entries=[blank_entry() for _ in range(ne)], array=[])


def flat(w):
    """The description as the host library reads it (the order of tests/unit_FrameRest.cpp)."""
    c = w["cache"]
    d = [len(w["links"]), len(w["entries"]), len(w["array"])]
    d += [w["frame"], w["renderer"], w["clear"], len(w["comp"])]
    for effect in w["comp"]:
        d += list(effect)
    d += [c["flags"], c["f18e0"], c["f18e4"], c["f18bc"], c["f18c0"], c["f18c4"], c["f1860"]]
    d += list(c["f1818"]) + [len(row) for row in w["rows"]]
    for row in w["rows"]:
        d += row
    for link in w["links"]:
        d += [link[k] for k in ("entries", "f08", "f0c", "f10", "f14", "f18", "f1c", "tflags", "tw", "th", "tpitch", "pixels")]
    for place in w["places"]:
        d += list(place["bytes"]) + [place["f10"], place["f14"]]
    for en in w["entries"]:
        d += [en[k] for k in ("flags", "w", "h", "cw", "ch", "page", "texture")]
        d += [fb(en[k]) for k in ("usize", "vsize", "uoff", "voff", "prio")]
        d += [en["pitch"], en["sortkey"], en["next"], en["wrap"], en["wrap"] if en.get("wrap2") is None else en["wrap2"], en["place"]]
        d += list(en["own"]["bytes"]) + [en["own"]["f10"], en["own"]["f14"]]
    d += list(w["array"])
    return d


def sections(values, w):
    """The state, as the two sides list it, cut into named parts."""
    pos = 0

    def take(n):
        nonlocal pos
        part = list(values[pos:pos + n])
        pos += n
        return part

    nl, ne, na = len(w["links"]), len(w["entries"]), len(w["array"])
    d = {"cache": take(7 + 6 + 6)}
    d["rows"] = [take(len(row) + 2) for row in w["rows"]]
    d["links"] = [take(11) for _ in range(nl)]
    d["places"] = [take(18) for _ in range(PLACES)]
    d["entries"] = [take(34) for _ in range(ne)]
    d["array"] = take(na)
    assert pos == len(values), f"state of {len(values)} numbers, expected {pos}"
    return d


def hash_entries(numbers):
    h = 0
    for n in numbers:
        h = (h * 1000003 + n + 1) & ((1 << 60) - 1)
    return h


class Ppc:
    """The world in emulator memory, the fakes, the call and the state."""

    def __init__(self, e, w):
        self.e, self.w = e, w
        self.nl, self.ne, self.na = len(w["links"]), len(w["entries"]), len(w["array"])
        self.compress_calls = 0

    # -- numbering
    def eno(self, a):
        if a == 0:
            return 0
        off = a - self.entries
        if 0 <= off < self.ne * 0x5c and off % 0x5c == 0:
            return off // 0x5c + 1
        return -1

    def lno(self, a):
        if a == 0:
            return 0
        off = a - self.links
        if 0 <= off < self.nl * 0x20 and off % 0x20 == 0:
            return off // 0x20 + 1
        return -1

    def tno(self, a):
        """A texture given as the complete object."""
        if a == 0:
            return 0
        try:
            return self.textures.index(a) + 1
        except ValueError:
            return -1

    def bno(self, a):
        """A texture given as its CTextureBase."""
        if a == 0:
            return 0
        try:
            return self.bases.index(a) + 1
        except ValueError:
            return -1

    def entry_at(self, n):
        return self.entries + 0x5c * (n - 1) if n > 0 else 0

    def link_at(self, n):
        return self.links + 0x20 * (n - 1) if n > 0 else 0

    def build(self):
        e, w = self.e, self.w
        c = w["cache"]
        ws(e, DISPLAY + 0x80, [w["frame"]])
        ws(e, SYSTEM + 0x1c, [w["renderer"]])
        self.cache = e.alloc(0x1910, b"\xa5" * 0x1910)
        self.links = e.alloc(max(self.nl, 1) * 0x20, b"\xa5" * (max(self.nl, 1) * 0x20))
        self.places = e.alloc(PLACES * 0x18, b"\xa5" * (PLACES * 0x18))
        self.entries = e.alloc(max(self.ne, 1) * 0x5c, b"\xa5" * (max(self.ne, 1) * 0x5c))
        self.arr = e.alloc(max(self.na, 1) * 4 + 4, b"\xa5" * (max(self.na, 1) * 4 + 4))

        cc = self.cache
        ws(e, cc + 0x18b4, [c["flags"]])
        for off, key in ((0x18e0, "f18e0"), (0x18e4, "f18e4"), (0x18bc, "f18bc"), (0x18c0, "f18c0"), (0x18c4, "f18c4"),
                         (0x1860, "f1860")):
            ws(e, cc + off, [c[key]])
        for k, v in enumerate(c["f1818"]):
            ws(e, cc + 0x1818 + 4 * k, [v])
        for i, row in enumerate(w["rows"]):
            ws(e, cc + 0x1800 + 4 * i, [len(row)])
            e.write(cc + 0x400 * i, b"".join(struct.pack(">I", self.link_at(n)) for n in row) + b"\x00" * 8)

        self.textures, self.bases = [], []
        for i, link in enumerate(w["links"]):
            obj = e.alloc(0x34, b"\xa5" * 0x34)
            T.call_ppc(e, 0x10045d88, [obj, 1])            # the original's constructor makes the vtables
            base = obj + 0x1c
            self.textures.append(obj)
            self.bases.append(base)
            e.write(base, struct.pack(">IIII", link["tflags"] & MASK, link["tw"] & MASK, link["th"] & MASK, link["tpitch"] & MASK))
            e.write(self.links + 0x20 * i, struct.pack(">IIIIIIII", base, self.entry_at(link["entries"]), link["f08"] & MASK,
                                                       link["f0c"] & MASK, link["f10"] & MASK, link["f14"] & MASK,
                                                       link["f18"] & MASK, link["f1c"] & MASK))
        for k, place in enumerate(w["places"]):
            self.write_place(self.places + 0x18 * k, place)
        for i, en in enumerate(w["entries"]):
            a = self.entries + 0x5c * i
            place = self.places + 0x18 * (en["place"] - 1) if en["place"] > 0 else a + 0x44
            texture = self.textures[en["texture"] - 1] if en["texture"] > 0 else 0
            own = en["own"]
            e.write(a, struct.pack(">IIIhhhhIIIIIIIhxxIIII16sII", place, self.entry_at(en["next"]), en["flags"] & MASK,
                                   T.s16(en["w"]), T.s16(en["h"]), T.s16(en["cw"]), T.s16(en["ch"]), self.link_at(en["page"]),
                                   texture, fb(en["usize"]), fb(en["vsize"]), fb(en["uoff"]), fb(en["voff"]), fb(en["prio"]),
                                   T.s16(en["pitch"]), en["sortkey"] & MASK, 0, 0, 0, bytes(own["bytes"]), own["f10"] & MASK,
                                   own["f14"] & MASK))
        e.write(self.arr, b"".join(struct.pack(">I", self.entry_at(n)) for n in w["array"]))
        self.wrap = [(en["wrap"], en["wrap"] if en.get("wrap2") is None else en["wrap2"]) for en in w["entries"]]
        self.wrap_calls = [0] * self.ne

    def write_place(self, a, p):
        self.e.write(a, bytes(p["bytes"]) + struct.pack(">II", p["f10"] & MASK, p["f14"] & MASK))

    # -- the fakes
    def stubs(self):
        st = CL.Stubs()
        st.real = FUNCS | set(INNER)
        st.internal(STORE_WRAP, self.store_wrap)
        st.internal(SET_FLAGS, self.set_flags)
        st.internal(GET_PIXELS, self.get_pixels)
        st.internal(SPECIFIC_COMPRESS, self.specific_compress)
        st.internal(CLEAR_TEXTURE, self.clear_texture)
        st.internal(SORT_TEXTURES, lambda e: e.cl.rec("SortTextures"))
        st.internal(SORT_ENTRIES, lambda e: e.cl.rec("SortEntries"))
        st.imp("__assertion_failed", lambda e: e.cl.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))
        return st

    def store_wrap(self, e):
        n = self.eno(e.r(3))
        e.cl.rec("StoreAsWrapping", (n,))
        answer = 0
        if n > 0:
            answer = self.wrap[n - 1][0 if self.wrap_calls[n - 1] == 0 else 1]
            self.wrap_calls[n - 1] += 1
        e.set_r(3, answer)

    def set_flags(self, e):
        this, flags = e.r(3), T.s32(e.r(4))
        e.cl.rec("SetFlags", (self.bno(this), flags))
        ws(e, this, [(flags & MASK) | (e.u32(this) & 0x10000000)])

    def get_pixels(self, e):
        n = self.tno(e.r(3))
        e.cl.rec("GetPixelCount", (n,))
        e.set_r(3, T.u32(self.w["links"][n - 1]["pixels"]) if n > 0 else 0)

    def specific_compress(self, e):
        this, entries, count, level, available = e.r(3), e.r(4), T.s32(e.r(5)), T.s32(e.r(6)), T.s32(e.r(7))
        numbers = [self.eno(e.u32(entries + 4 * i)) for i in range(count)]
        e.cl.rec("Entry_Specific_Compress", (count, hash_entries(numbers), numbers[0] if numbers else 0, level, available,
                                             T.s32(e.u32(this + 0x18c4)), T.s32(e.u32(this + 0x18e0)), e.u32(this + 0x18b4)))
        comp = self.w["comp"]
        if comp:
            effect = comp[min(self.compress_calls, len(comp) - 1)]
            for slot, off in ((0, 0x18c4), (2, 0x18e0), (4, 0x18b4), (6, 0x18e4)):
                if effect[slot] != 0:
                    ws(e, this + off, [effect[slot + 1]])
        self.compress_calls += 1

    def clear_texture(self, e):
        link = e.r(4)
        n = self.lno(link)
        e.cl.rec("ClearTexture", (n,))
        if self.w["clear"] == 1:
            ws(e, link + 4, [0])
            ws(e, link + 0xc, [0])
            ws(e, link + 0x1c, [0])

    # -- the call and the state
    def call(self, op, a):
        e, c = self.e, self.cache
        if op == 1:
            T.call_ppc(e, CALC_USAGE, [self.link_at(a[0]), a[1] & MASK])
        elif op == 2:
            T.call_ppc(e, AUTO_MIPMAP, [c, self.arr, a[0] & MASK])
        elif op == 3:
            T.call_ppc(e, ENTRY_COMPRESS, [c, self.arr, a[0] & MASK])
        elif op == 4:
            T.call_ppc(e, FREE_LOCKED, [c])
        else:
            return T.s32(T.call_ppc(e, LEVEL_USAGE, [c])[0])      # the original leaves 1 in r3 (void here)
        return 0

    def state(self):
        e, u, s = self.e, self.e.u32, T.s32
        c = self.cache
        out = [u(c + 0x18b4)] + [s(u(c + off)) for off in (0x18e0, 0x18e4, 0x18bc, 0x18c0, 0x18c4, 0x1860)]
        out += [s(u(c + 0x1818 + 4 * k)) for k in range(6)]
        counts = [s(u(c + 0x1800 + 4 * k)) for k in range(6)]
        out += counts
        for i in range(6):
            out += [self.lno(u(c + 0x400 * i + 4 * k)) for k in range(counts[i] + 2)]
        for i in range(self.nl):
            a = self.links + 0x20 * i
            out.append(self.eno(u(a + 4)))
            out += [s(u(a + off)) for off in (8, 0xc, 0x10, 0x14, 0x18, 0x1c)]
            base = self.bases[i]
            out += [u(base), s(u(base + 4)), s(u(base + 8)), s(u(base + 0xc))]
        for k in range(PLACES):
            out += self.place(self.places + 0x18 * k)
        if self.ne:
            block = e.read(self.entries, self.ne * 0x5c)
            for i in range(self.ne):
                a = self.entries + 0x5c * i
                (place, nxt, flags, w_, h_, cw, ch, page, texture, usize, vsize, uoff, voff, prio, pitch, sortkey, _f38, _data,
                 _f40, own, ownf10, ownf14) = struct.unpack_from(">IIIhhhhIIIIIIIhxxIIII16sII", block, 0x5c * i)
                if place == a + 0x44:
                    code = 0
                elif self.places <= place < self.places + 0x18 * PLACES and (place - self.places) % 0x18 == 0:
                    code = (place - self.places) // 0x18 + 1
                else:
                    code = -1
                out += [code, self.eno(nxt), flags, w_, h_, cw, ch, self.lno(page), self.tno(texture), canon(usize), canon(vsize),
                        canon(uoff), canon(voff), canon(prio), pitch, s(sortkey)]
                out += list(own) + [s(ownf10), s(ownf14)]
        out += [self.eno(u(self.arr + 4 * i)) for i in range(self.na)]
        return out

    def place(self, a):
        return list(self.e.read(a, 16)) + [T.s32(self.e.u32(a + 0x10)), T.s32(self.e.u32(a + 0x14))]


def ppc_run(e, case):
    w = case["world"]
    with guarded(e):
        p = Ppc(e, w)
        st = p.stubs()
        saved = st.activate(e)
        try:
            p.build()
            e.cl = CL.PpcLog({}, {})
            with HardwareRounding(e, [(LEVEL_USAGE, LEVEL_USAGE + LEVEL_USAGE_SIZE)]):
                result = p.call(case["op"], case["args"])
            return dict(result=result, state=sections(p.state(), w), log=e.cl.log)
        finally:
            CL.Stubs.deactivate(e, saved)


def host_run(lib, case):
    bind(lib)
    w = case["world"]
    CL.host_begin(lib, {}, {})
    description = flat(w)
    d = (C.c_int64 * len(description))(*[signed64(v) for v in description])
    args = (C.c_int64 * 2)(*[signed64(v) for v in (list(case["args"]) + [0, 0])[:2]])
    capacity = 4096 + 40 * len(w["entries"]) + 40 * len(w["links"])
    out = (C.c_int64 * capacity)()
    result = lib.tpw_fr_run(case["op"], args, d, out, capacity)
    n = lib.tpw_fr_count()
    log = CL.host_end(lib)
    assert n <= capacity, "the state buffer is too small"
    return dict(result=result, state=sections(list(out[:n]), w), log=log)


# ----------------------------------------------------------------- random worlds


def rfl(r):
    """A float for a field: mostly moderate, some edge values."""
    k = r.random()
    if k < 0.2:
        return r.choice([0.0, -0.0, 1.0, -1.0, 0.5, 1.5, 2.0, 4.0, 0.25, 0.75, 1024.0, 1e-3, 100.0, -100.0, 3.0e9, -3.0e9,
                         1.0e-40, NAN, INF, -INF, 3.4028234663852886e38, -3.4028234663852886e38, 1.0e30, -1.0e30])
    if k < 0.6:
        return T.f32(r.uniform(-1.0, 3.0))
    return T.f32(r.uniform(-2000.0, 2000.0))


def rpriority(r):
    """A priority: mostly in 0..1.5, with the edges of the sort key's range."""
    k = r.random()
    if k < 0.55:
        return T.f32(r.uniform(0.0, 1.5))
    if k < 0.75:
        return r.choice([0.0, 1.0, 1.5, 0.5, 1.4999, 1.5001, 2.0, -0.0])
    return rfl(r)


def rsize(r):
    return r.choice([8, 16, 32, 64, 128, 256, r.randint(1, 300), r.randint(1, 40), 1, 7, 9, 0, -3, 0x7fff, -0x8000,
                     r.randint(-0x8000, 0x7fff)])


def rkey(r):
    k = r.random()
    if k < 0.35:
        return r.randrange(6)
    if k < 0.55:
        return r.choice([0, 1, 0x7fffffff, 0x80000000, 0x80000001, 0xffffffff, 0x20000000, 0x40000000, 0x0fff0000])
    return r.getrandbits(32)


def rflags(r, asked=0.5, locked=0.3, levels=None):
    """An entry's flags: random bits with the ones the functions test set on purpose."""
    keep = r.getrandbits(32) & r.getrandbits(32) & ~(ONPAGE | QUEUED | LOCKED | DROPPED | BLEND | ASKED | COMPRESSED | 0xf0 | 0x30000)
    for bit, p in ((ASKED, asked), (LOCKED, locked), (QUEUED, 0.3), (DROPPED, 0.15), (ONPAGE, 0.3), (BLEND, 0.25),
                   (COMPRESSED, 0.25), (0x10000, 0.1), (0x20000, 0.1), (0x40, 0.3), (0x80, 0.3)):
        if r.random() < p:
            keep |= bit
    keep |= (r.randrange(4) if levels is None else levels) << 4
    return keep & MASK


def random_entry(r, nl, asked=0.5, locked=0.3):
    width, height = rsize(r), rsize(r)
    en = blank_entry()
    en.update(flags=rflags(r, asked, locked), w=width, h=height, cw=r.choice([width, (width + 7) & ~7, rsize(r), 8, 16, 32]),
              ch=r.choice([height, (height + 7) & ~7, rsize(r), 8, 16, 32]), page=r.randint(0, nl) if nl else 0,
              texture=r.randint(0, nl) if nl else 0, usize=rfl(r), vsize=rfl(r), uoff=rfl(r), voff=rfl(r), prio=rpriority(r),
              pitch=r.choice([0, 32, r.randint(-0x8000, 0x7fff)]), sortkey=T.s32(rkey(r)),
              wrap=1 if r.random() < 0.3 else 0, place=r.choice([0, 0, 0, 1, 2, 3, 4]), own=blank_place(r))
    if r.random() < 0.15:
        en["wrap2"] = 1 - en["wrap"]     # a fake that answers differently the second time
    return en


def random_world(r, ne, nl=0, asked=0.5, locked=0.3):
    w = blank_world(nl=nl, ne=ne)
    w["entries"] = [random_entry(r, nl, asked, locked) for _ in range(ne)]
    w["frame"] = r.choice([0, 1, 15, 16, 100, 1000, -1, r.randint(-50, 5000), 0x7fffffff, -0x80000000])
    w["places"] = [blank_place(r) for _ in range(PLACES)]
    return w


def set_chain(w, order):
    """Chains the entries (numbers) through mNext; returns the head."""
    for k, n in enumerate(order):
        w["entries"][n - 1]["next"] = order[k + 1] if k + 1 < len(order) else 0
    return order[0] if order else 0


def case(op, args, w):
    return dict(op=op, args=list(args), world=w)


CALC, MIPMAP, COMPRESS, FREE, LEVEL = range(1, 6)


def rpixels(r, area=65536):
    return r.choice([area, area, area // 4, 0, 1, -1, 0x7fffffff, r.randint(0, 2 * area), 2 * area])


def rlink(r, ne=0):
    link = blank_link()
    link.update(tflags=r.choice([0, 0, 1, 2, 3, 4, 7, 0x10000000, 0x10000007, r.getrandbits(32)]) & MASK,
                f08=r.randint(-99, 99), f0c=r.randint(-99, 99), f10=r.randint(-99, 99), f14=r.choice([-1, 0, 5, r.randint(-30, 30)]),
                f18=r.randint(-9, 9), f1c=r.randint(-9, 9), tw=r.choice([8, 256]), th=r.choice([8, 256]), tpitch=r.randint(0, 99),
                pixels=rpixels(r))
    return link


# ---- CalcUsage

def calc_world(r, n_entries, links=1, asked=0.5, locked=0.3):
    w = random_world(r, n_entries, nl=links, asked=asked, locked=locked)
    w["links"] = [rlink(r) for _ in range(links)]
    ids = list(range(1, n_entries + 1))
    w["links"][0]["entries"] = set_chain(w, ids)
    for en in w["entries"]:
        en["page"], en["texture"] = 1, 1 if r.random() < 0.8 else 0
    return w


def calc_case(r, n_entries, **kw):
    w = calc_world(r, n_entries, links=r.choice([1, 1, 2, 3]), **kw)
    return case(CALC, [1, r.choice([0, 1, 4, -1, r.getrandbits(32) & 0x7fffffff])], w)


def calc_edges():
    r = random.Random(21)
    cases = []
    # the page: empty, one entry, several; the flags of the entry in turn
    for n in (0, 1, 2, 3, 5):
        for _ in range(3):
            cases.append(calc_case(r, n))
    # every combination of the flags the loop reads on one entry: locked, asked for, compressed, store level, wrapping
    for locked in (0, LOCKED):
        for asked in (0, ASKED):
            for compressed in (0, COMPRESSED):
                for level in (0, LEVEL1, LEVEL2, LEVEL3):
                    for wrap in (0, 1):
                        for width in (8, 16, 9, 7, 0):
                            w = calc_world(r, 1)
                            en = w["entries"][0]
                            en.update(flags=locked | asked | compressed | level | r.choice([0, 0x80000, 0x2000]), wrap=wrap,
                                      cw=width, ch=r.choice([8, 16]))
                            cases.append(case(CALC, [1, 0], w))
    # the age of the page: frame count against the last update (mF14), around 16 and wrapping
    for frame, updated in ((100, 100), (100, 99), (100, 85), (100, 84), (100, 83), (100, 0), (0, -1), (5, 100), (-1, 0),
                           (0x7fffffff, -1), (-0x80000000, 1), (16, 0), (15, 0), (17, 0), (0, 15), (0, 16), (0, 17)):
        w = calc_world(r, 2)
        w["frame"], w["links"][0]["f14"] = frame, updated
        cases.append(case(CALC, [1, 0], w))
    # sums that wrap: huge sizes and use counts
    for _ in range(6):
        w = calc_world(r, 4)
        for en in w["entries"]:
            en.update(cw=r.choice([0x7fff, -0x8000, 0x4000]), ch=r.choice([0x7fff, -0x8000, 0x4000]), flags=ASKED | LOCKED | COMPRESSED)
            en["own"]["f10"] = r.choice([0x7fffffff, -0x80000000, 0x10000, 77777])
            en["place"] = 0
        w["links"][0]["pixels"] = r.choice([0x7fffffff, -0x80000000, 5])
        cases.append(case(CALC, [1, 0], w))
    # a texture flags word with bits set before (0x10000000 stays)
    for tflags in (0, 1, 2, 4, 7, 0x10000000, 0x10000007, 0xffffffff, 0xeffffff8):
        w = calc_world(r, 2)
        w["links"][0]["tflags"] = tflags
        cases.append(case(CALC, [1, 0], w))
    # other pages around the one asked about, and a spare place record
    for _ in range(4):
        w = calc_world(r, 3, links=3)
        w["entries"][1]["place"] = 2
        cases.append(case(CALC, [1, 0], w))
    return cases


def calc_random(r):
    return calc_case(r, r.choice([0, 1, 1, 2, 3, 4, 6, 9]), asked=r.choice([0.2, 0.5, 0.8]), locked=r.choice([0.0, 0.2, 0.6]))


# ---- AutoMipmap and Entry_Compress

def comp_effect(r, kind="c4"):
    """What one call of the fake Entry_Specific_Compress does: assigns mF18C4, mF18E0, mFlags or mF18E4."""
    return (1 if kind in ("c4", "all") else 0, r.choice([0, 1, 5, 100, 1000, -5, 0x7fffffff, -0x80000000, r.randint(-500, 3000)]),
            1 if kind in ("e0", "all") else 0, r.choice([0, 1, 2, 3, 11, 12, 13, -1, r.randint(-2, 15)]),
            1 if kind in ("flags", "all") else 0, r.choice([0, 4, 5, 0xffffffff, r.getrandbits(32)]),
            1 if kind in ("e4", "all") else 0, r.choice([0, 1, 10, 1000, -1, r.randint(-100, 3000)]))


def array_world(r, n):
    w = random_world(r, max(n, 1) + r.randint(0, 2), nl=1)
    w["array"] = r.sample(range(1, len(w["entries"]) + 1), n)
    return w


def mipmap_case(r, level=None, n=None, effects=None):
    n = r.choice([0, 1, 2, 3, 5]) if n is None else n
    w = array_world(r, n)
    w["cache"]["f18e0"] = r.choice([0, 1, 2, 3, 4, 7, 11, 12, 13, -1, -5]) if level is None else level
    w["cache"]["flags"] = r.choice([0, 4, 5, r.getrandbits(32)]) & MASK
    w["cache"]["f18c4"] = r.choice([0, 100, -3, 0x7fffffff])
    w["comp"] = [] if effects is None else effects
    return case(MIPMAP, [n], w)


def mipmap_edges():
    r = random.Random(31)
    cases = [mipmap_case(r, level, n) for level in (0, 1, 2, 3, 5, 12, 13, -1, -7) for n in (0, 1, 3)]
    # the loop bound is read again after every call: the fake changes mF18E0 (down, up, to the same, to below the level)
    for level in (1, 2, 3, 4, 5):
        for new in (0, 1, 2, 3, 5, 9, -1):
            cases.append(mipmap_case(r, level, 2, effects=[(0, 0, 1, new, 0, 0, 0, 0)]))
            cases.append(mipmap_case(r, level, 2, effects=[(0, 0, 0, 0, 0, 0, 0, 0), (0, 0, 1, new, 0, 0, 0, 0)]))
    return cases


def mipmap_random(r):
    effects = [] if r.random() < 0.6 else [comp_effect(r, r.choice(["c4", "e0", "flags", "e4", "all"])) for _ in range(r.randint(1, 3))]
    return mipmap_case(r, effects=effects)


def compress_case(r, level=None, available=None, effects=None, flags=None, limit=None, area=None, n=None):
    n = r.choice([0, 1, 2, 3, 5]) if n is None else n
    w = array_world(r, n)
    c = w["cache"]
    c["f18e0"] = r.choice([0, 1, 2, 3, 5, 8, 10, 11, 12, 13, 14, -1, -3]) if level is None else level
    c["f18bc"] = r.choice([0, 100, -50, 1000000, 0x7fffffff, -0x80000000, r.randint(-500, 5000)])
    c["f18c0"] = r.choice([0, 100, 5000, 0x7fffffff, -0x80000000, r.randint(-500, 100000)]) if available is None else \
        available + c["f18bc"]
    c["f18c4"] = r.choice([0, 100, 1000, -1, 0x7fffffff, -0x80000000, r.randint(-500, 100000)]) if area is None else area
    c["f18e4"] = r.choice([0, 100, 1000, -1, 0x7fffffff, -0x80000000, r.randint(-500, 100000)]) if limit is None else limit
    c["flags"] = r.choice([0, 4, 1, 5, 0xfffffffb, r.getrandbits(32)]) & MASK if flags is None else flags
    w["comp"] = [] if effects is None else effects
    return case(COMPRESS, [n], w)


def compress_edges():
    r = random.Random(41)
    cases = []
    # level 12 does nothing; levels around it
    for level in (12, 11, 13, 14, 0, -1):
        cases.append(compress_case(r, level=level, available=100, area=50, flags=0))
        cases.append(compress_case(r, level=level, available=100, area=500, flags=0, effects=[(1, 500, 0, 0, 0, 0, 0, 0)]))
    # the area against the available pixels, equal, above and below, with and without flag 4 and the limit
    for area, available in ((50, 100), (100, 100), (101, 100), (-5, -5), (-4, -5), (-6, -5), (0x7fffffff, -0x80000000), (-0x80000000, 0x7fffffff)):
        for flags in (0, 4, 5):
            for limit in (0, 10, 49, 50, 51, -100, 0x7fffffff):
                cases.append(compress_case(r, level=0, available=available, area=area, flags=flags, limit=limit, n=2))
    # the loop runs through the levels while the fake leaves the area over, and stops when it assigns a fitting one
    over = (1, 500, 0, 0, 0, 0, 0, 0)
    fits = (1, 10, 0, 0, 0, 0, 0, 0)
    for level in (0, 5, 10, 11):
        cases.append(compress_case(r, level=level, available=100, area=500, flags=0, effects=[over], n=2))
        for stop in range(0, 4):
            cases.append(compress_case(r, level=level, available=100, area=500, flags=0, effects=[over] * stop + [fits], n=2))
        cases.append(compress_case(r, level=level, available=100, area=50, flags=4, limit=60, effects=[over, (1, 70, 0, 0, 0, 0, 0, 0), fits], n=2))
    # flag 4 and mF18E4 are read again after each call: the fake changes them
    for new_flags in (0, 4, 5):
        for new_limit in (0, 49, 50, 51, 1000):
            cases.append(compress_case(r, level=3, available=100, area=50, flags=4, limit=10,
                                       effects=[(0, 0, 0, 0, 1, new_flags, 1, new_limit)] * 4, n=1))
            cases.append(compress_case(r, level=3, available=100, area=50, flags=0, limit=10,
                                       effects=[(0, 0, 0, 0, 1, new_flags, 1, new_limit)] * 4, n=1))
    # the level is taken once: the fake changes mF18E0
    cases.append(compress_case(r, level=2, available=100, area=500, flags=0, effects=[(1, 500, 1, 9, 0, 0, 0, 0)] * 3, n=1))
    cases.append(compress_case(r, level=2, available=100, area=500, flags=0, effects=[(1, 500, 1, 12, 0, 0, 0, 0)] * 3, n=1))
    return cases


def compress_random(r):
    k = r.random()
    if k < 0.5:
        effects = []
    else:
        effects = [comp_effect(r, r.choice(["c4", "c4", "e0", "flags", "e4", "all"])) for _ in range(r.randint(1, 5))]
    return compress_case(r, effects=effects)


# ---- the balancing

def pages_world(r, counts, locked_p=0.4, entries_per_page=(0, 3), asked=0.5, entry_locked=0.3):
    """A world with counts[i] pages of size i (in any order), each with a chain of entries."""
    nl = sum(counts)
    chains = [r.randint(*entries_per_page) for _ in range(nl)]
    ne = sum(chains)
    w = random_world(r, ne, nl=nl, asked=asked, locked=entry_locked)
    w["links"] = [rlink(r) for _ in range(nl)]
    ids = list(range(1, nl + 1))
    r.shuffle(ids)
    at = 0
    for i, row_count in enumerate(counts):
        for _ in range(row_count):
            w["rows"][i].append(ids[at])
            at += 1
    entry_no = 1
    for link_no in range(1, nl + 1):
        k = chains[link_no - 1]
        order = list(range(entry_no, entry_no + k))
        entry_no += k
        w["links"][link_no - 1]["entries"] = set_chain(w, order)
        for n_ in order:
            w["entries"][n_ - 1]["page"] = link_no
            w["entries"][n_ - 1]["texture"] = link_no
    for link in w["links"]:
        if r.random() < locked_p:
            link["tflags"] |= 1
        else:
            link["tflags"] &= ~1
    return w


def f32_div(a, b):
    """(float)a / (float)b in single precision, with the processor's answers for a zero divisor."""
    fa, fb_ = T.f32(float(a)), T.f32(float(b))
    if fb_ == 0.0:
        return math.nan if fa == 0.0 else math.copysign(math.inf, fa)
    return T.f32(fa / fb_)


def fctiwz(x):
    if x != x:
        return -0x80000000
    if x >= 2147483648.0:
        return 0x7fffffff
    if x <= -2147483649.0:
        return -0x80000000
    return int(x)


def level_limit(here, other):
    """The number of pages BalanceCache_LevelSurfaceUsage keeps when it compares a size's share with a neighbour's."""
    limit = T.s32(fctiwz(T.f32(0.25 * T.f32(here - other))) + 1)
    return min(limit, 16)


def make_level_safe(w):
    """The original runs memcpy with a negative size when the number of pages to keep is negative, and
    overwrites one of its own lists when it keeps 16 of two or more. No game state gets there (the shares are
    fractions); the cases here never do either: the pages of such a size are locked, so none is a candidate
    (all but the first for a limit of 16)."""
    c = w["cache"]
    shares = [f32_div(c["f1818"][i], len(w["rows"][i])) for i in range(6)]
    for i in range(6):
        here = shares[i]
        if not here < T.f32(0.67):
            continue
        for applies, other in ((i > 0 and T.f32(1.5 * here) < shares[i - 1], shares[i - 1] if i > 0 else 0.0),
                               (i < 5 and T.f32(1.5 * here) < shares[i + 1], shares[i + 1] if i < 5 else 0.0)):
            if not applies:
                continue
            limit = level_limit(here, other)
            row = w["rows"][i]
            if limit < 0 or (limit == 16 and len(row) > 1):
                c["flags"] &= ~RESHUFFLE & MASK
                for n in (row if limit < 0 else row[1:]):
                    w["links"][n - 1]["tflags"] |= 1
    return w


def rows_counts(r, maxn=6, sizes=None):
    return [r.randint(0, maxn) if (sizes is None or i in sizes) else 0 for i in range(6)]


def free_world(r, limit_choices=None, maxn=6, near=0.7):
    counts = rows_counts(r, maxn)
    w = pages_world(r, counts, entries_per_page=(0, 2))
    w["cache"]["f1860"] = r.choice(limit_choices or [4, 4, 1, 2, 3, 5, 6, 0, -1, 16, 17, 100])
    for i, row in enumerate(w["rows"]):
        area = PAGE_SIDES[i] ** 2
        for n in row:
            link = w["links"][n - 1]
            if r.random() < near:
                link["f1c"] = r.choice([0, 1, -1, area // 4 - 1, area // 4, area // 4 + 1, area // 8, area // 2, r.randint(1, area // 4)])
                link["f0c"] = r.choice([0, 1, -1, area // 2 - 1, area // 2, area // 2 + 1, area // 4, area, r.randint(0, area // 2),
                                        0x40000000, -0x40000000, 0x3fffffff])
            if r.random() < 0.1:
                link["f1c"] = r.choice([0x20000000, 0x40000000, 0x3fffffff, -0x80000000, 0x7fffffff])
    if w["cache"]["f1860"] >= 16:
        # the original overwrites its own list when a 17th page qualifies: at most 16 pages a size
        for row in w["rows"]:
            del row[16:]
    w["clear"] = r.choice([0, 0, 1])
    return case(FREE, [], w)


def free_edges():
    r = random.Random(51)
    cases = []
    for size in range(6):
        area = PAGE_SIDES[size] ** 2
        # the three tests on a page (entries, locked space under a quarter, asked-for space under half) around their limits
        for f1c in (-1, 0, 1, area // 4 - 1, area // 4, area // 4 + 1, 0x40000000 + 1, 0x3fffffff):
            for f0c in (-1, 0, area // 2 - 1, area // 2, area // 2 + 1, 0x40000000, 0x3fffffff):
                for has_entries in (0, 1):
                    counts = [0] * 6
                    counts[size] = 1
                    w = pages_world(r, counts, entries_per_page=(has_entries, has_entries))
                    w["links"][0]["f1c"], w["links"][0]["f0c"] = f1c, f0c
                    w["cache"]["f1860"] = 4
                    cases.append(case(FREE, [], w))
    # the order of the list and its limit: pages with the same and different pixels asked for
    for limit in (0, 1, 2, 3, 4, 5, 16, 17, -1):
        for keys in ([5, 4, 3, 2, 1], [1, 2, 3, 4, 5], [3, 1, 4, 1, 5], [2, 2, 2, 2, 2], [9, 1, 9, 1, 9], [0], [], [1, 1]):
            counts = [0] * 6
            counts[r.randrange(6)] = len(keys)
            w = pages_world(r, counts, entries_per_page=(1, 1))
            if keys:
                row = next(row for row in w["rows"] if row)
                for n, key in zip(row, keys):
                    w["links"][n - 1]["f1c"] = 1
                    w["links"][n - 1]["f0c"] = key
            w["cache"]["f1860"] = limit
            w["clear"] = r.choice([0, 1])
            cases.append(case(FREE, [], w))
    # up to 16 pages qualify with a limit of 16 and more
    for n in (14, 15, 16):
        counts = [0] * 6
        counts[3] = n
        w = pages_world(r, counts, entries_per_page=(1, 1))
        keys = [r.randint(0, 200) for _ in range(n)]
        for link, key in zip(w["rows"][3], keys):
            w["links"][link - 1]["f1c"], w["links"][link - 1]["f0c"] = 1, key
        for limit in (16, 17, 40, 15, 14):
            w2 = pages_world(random.Random(n), counts, entries_per_page=(1, 1))
            for link, key in zip(w2["rows"][3], keys):
                w2["links"][link - 1]["f1c"], w2["links"][link - 1]["f0c"] = 1, key
            w2["cache"]["f1860"] = limit
            cases.append(case(FREE, [], w2))
    # more pages than the limit, in each of the six sizes, with the clearing changing the pages
    for _ in range(10):
        cases.append(free_world(r, maxn=8))
    return cases


def free_random(r):
    k = r.random()
    if k < 0.6:
        return free_world(r)
    if k < 0.9:
        return free_world(r, limit_choices=[1, 2, 3, 4], maxn=10)
    return free_world(r, limit_choices=[16, 17, 30], maxn=15)


# ---- LevelSurfaceUsage

def level_world(r, shares=None, maxn=8, entries_per_page=(0, 3), clear=None):
    """shares: for each size, (mF1818, page count) or None for random."""
    counts = []
    f1818 = []
    for i in range(6):
        if shares is not None and shares[i] is not None:
            f1818.append(shares[i][0])
            counts.append(shares[i][1])
        else:
            n = r.randint(0, maxn)
            counts.append(n)
            f1818.append(r.choice([r.randint(-1, n + 1), r.randint(0, n), 0, n]))
    w = pages_world(r, counts, locked_p=r.choice([0.0, 0.3, 0.7]), entries_per_page=entries_per_page,
                    asked=r.choice([0.2, 0.5, 0.8]), entry_locked=0.2)
    w["cache"]["f1818"] = f1818
    w["cache"]["flags"] = r.choice([0, 0, 1, 1, 4, 5, r.getrandbits(32)]) & MASK
    w["renderer"] = 3 if r.random() < 0.9 else r.choice([0, 1, 2, 4, 5, -1, 3])
    w["clear"] = r.choice([0, 1]) if clear is None else clear
    for i, row in enumerate(w["rows"]):
        area = PAGE_SIDES[i] ** 2
        for n in row:
            link = w["links"][n - 1]
            link["f0c"] = r.choice([0, 1, area // 2 - 1, area // 2, area // 2 + 1, -1, area, r.randint(0, area), 0x40000000,
                                    0x3fffffff, -0x40000000])
            link["tflags"] = r.choice([0, 0, 1, 2, 3, 4, 6, 7, 0x10000000, 0x10000002, 0x10000006]) & MASK
            if link["entries"]:
                # a few entries with something left to cut, some wrapping at 8 or wider
                n_ = link["entries"]
                while n_:
                    en = w["entries"][n_ - 1]
                    if r.random() < 0.7:
                        en["flags"] = (en["flags"] & ~0x30 & MASK) | (r.randrange(4) << 4)
                    if r.random() < 0.4:
                        en["cw"] = r.choice([8, 16, 9, 7, 32, 256])
                        en["ch"] = r.choice([8, 16, 9, 7, 32, 256, 0, -1])
                    n_ = en["next"]
    return case(LEVEL, [], make_level_safe(w))


def level_edges():
    r = random.Random(61)
    cases = []
    # the shares: a size with a low share beside a size with a high one, on either side; each size in turn
    for focus in range(6):
        for other in ((6, 10), (10, 10), (9, 10), (0, 10)):
            for here in ((0, 10), (1, 10), (3, 10), (6, 10), (7, 10), (0, 1), (1, 1), (-1, 10), (2, 0), (0, 0)):
                shares = [None] * 6
                shares[focus] = here
                if focus > 0:
                    shares[focus - 1] = other
                if focus < 5:
                    shares[focus + 1] = other
                cases.append(level_world(r, shares=shares))
    # the 1.5 test: shares in a ratio of exactly 1.5, just under and just over
    for here, prev in ((2, 3), (3, 4), (4, 6), (4, 5), (1, 1), (2, 2), (4, 7), (1, 2)):
        shares = [None] * 6
        shares[1] = (here, 10)
        shares[0] = (prev, 10)
        cases.append(level_world(r, shares=shares))
    # the 0.67 test: shares just under, at and over it, beside fuller sizes on both sides
    for here in ((2, 3), (4, 6), (33, 50), (5, 8), (7, 10), (13, 20), (14, 21), (1, 1), (3, 4), (2, 2), (11, 16), (17, 25), (9, 13),
                 (27, 40), (41, 60)):
        for prev in ((20, 10), (30, 10)):
            shares = [None] * 6
            shares[1], shares[2], shares[3] = prev, here, prev
            cases.append(level_world(r, shares=shares, maxn=3, entries_per_page=(1, 2)))
    # a share of exactly 0.67 needs 100 pages: a row of four pages listed 25 times
    for prev in ((20, 10), (30, 10)):
        shares = [None] * 6
        shares[1], shares[2], shares[3] = prev, (0, 4), prev
        c = level_world(r, shares=shares, maxn=3, entries_per_page=(1, 2))
        w = c["world"]
        w["rows"][2] = (w["rows"][2] * 25)[:100]
        w["cache"]["f1818"][2] = 67
        make_level_safe(w)
        cases.append(c)
    # the renderer
    for renderer in (0, 1, 2, 3, 4, 5, -1, 0x7fffffff):
        for _ in range(2):
            c = level_world(r, shares=[(0, 5), (5, 5), (0, 5), (5, 5), (0, 5), (5, 5)])
            c["world"]["renderer"] = renderer
            cases.append(c)
    # the limit of pages to clear: a size whose share is negative (SortTextures leaves a negative index for a size with
    # no pages ... the shares of the game are fractions) next to one a little fuller; the limit is a quarter of the
    # difference plus one, and 16 at most
    for scale in (1, 2, 4, 8, 16, 32, 40, 64, 100, 200, 400):
        for side in (-1, 1):
            for pages in (1, 2, 10):
                shares = [None] * 6
                shares[2] = (-10 * scale, 10) if pages == 10 else (-pages * scale, pages)
                shares[2 + side] = (-14 * scale, 10) if pages == 10 else (-int(1.4 * scale * pages), pages)
                cases.append(level_world(r, shares=shares, maxn=pages, entries_per_page=(1, 2)))
    # positive shares: the limit is 1 (a difference under 4) or 0 (4 to 8, nothing is kept); an empty size
    for here, other in (((0, 10), (10, 10)), ((0, 10), (50, 10)), ((0, 10), (79, 10)), ((0, 10), (80, 10)), ((0, 0), (10, 10)),
                        ((-5, 0), (10, 10)), ((-5, 0), (5, 0)), ((3, 0), (-5, 0))):
        for side in (-1, 1):
            shares = [None] * 6
            shares[2] = here
            shares[2 + side] = other
            cases.append(level_world(r, shares=shares, maxn=4, entries_per_page=(1, 2)))
    return cases


def level_random(r):
    k = r.random()
    if k < 0.7:
        return level_world(r)
    if k < 0.9:
        return level_world(r, maxn=3, entries_per_page=(0, 2))
    return level_world(r, maxn=14, entries_per_page=(0, 2))


# ================================================================= the render memory's deferred drawing

EXECUTE, EXECUTE_SIZE = 0x10027a8c, 784
SORT_TEXTURE, SORT_ASCENDING, SORT_DESCENDING, SORT_SIZE = 0x100275cc, 0x10027764, 0x100278f8, 332
INSTANT, EXTERN_EXECUTE, PUSH_ERROR = 0x10007444, 0x10006eb4, 0x10008904
RENDER_FUNCS = {EXECUTE, SORT_TEXTURE, SORT_ASCENDING, SORT_DESCENDING}
RECORD_SIZE_MAC = 0x38
DRAWN, SORT_SKIPPED = 0x10, 0x1000
LABEL_DRAW = 1
EXEC_OP, TEXTURE_OP, ASCENDING_OP, DESCENDING_OP = 1, 2, 3, 4
UNUSED = 0x00000200          # the flags of a record outside the used ones (not drawn)


def texture_address(label):
    """A texture's address on the PowerPC side (never followed): later labels lie higher, and labels 2 and up lie above
    0x80000000, where the unsigned compare matters."""
    return 0 if label == 0 else 0x7ffffc00 + 0x200 * label


def texture_label(address):
    return 0 if address == 0 else (address - 0x7ffffc00) // 0x200 if (address - 0x7ffffc00) % 0x200 == 0 else -1


def block_hash(words):
    h = 0
    for w in words:
        h = (h * 1000003 + w) & ((1 << 60) - 1)
    return h


def record(flags=0, blend=0, texture=0, depth=0.0):
    return dict(flags=flags, blend=blend, texture=texture, depth=depth)


def render_case(op, records, dynamic=0, static=0, renderer=3, display=0, malloc_fails=False, instant=(), answers=(), order=(),
                low=0, high=0):
    """records: the whole table. The dynamic records are numbers 0 .. dynamic - 1 and the `static` static ones the numbers
    from the end of the table down (mNextStatic = table size - 1 - static)."""
    return dict(op=op, records=list(records), count=len(records), dynamic=dynamic, next_static=len(records) - 1 - static,
                renderer=renderer, display=display & MASK, malloc_fails=malloc_fails, instant=list(instant),
                answers=list(answers), order=list(order), low=low, high=high)


def render_description(c):
    d = [len(c["records"]), c["count"], c["dynamic"], c["next_static"], c["renderer"], c["display"], 1 if c["malloc_fails"] else 0]
    d += [len(c["instant"])] + list(c["instant"]) + [len(c["answers"])] + list(c["answers"])
    for r_ in c["records"]:
        d += [r_["flags"], r_["blend"], r_["texture"], fb(r_["depth"])]
    d += [len(c["order"])] + list(c["order"]) + [c["low"], c["high"]]
    return d


class RenderPpc:
    """The table in emulator memory, the fakes, the call and the state."""

    def __init__(self, e, c):
        self.e, self.c, self.L = e, c, CL.PpcLog({e.fr_draw: LABEL_DRAW})
        self.block, self.bytes, self.calls = None, 0, 0

    def number(self, address):
        off = address - self.infos
        return off // RECORD_SIZE_MAC if 0 <= off < len(self.c["records"]) * RECORD_SIZE_MAC and off % RECORD_SIZE_MAC == 0 else -1

    def record_events(self, name, e):
        this, prim = e.r(3), e.r(4)
        flags, blend, texture = e.u32(prim), e.u32(prim + 4), e.u32(prim + 8)
        depth = canon(e.u32(prim + 0x30))
        self.L.rec(name, (self.L.lab(this), self.number(prim), flags, blend, texture_label(texture), depth))

    def instant(self, e):
        self.record_events("Instant", e)
        prim = e.r(4)
        if self.number(prim) in self.c["instant"]:
            ws(e, prim, [e.u32(prim) | DRAWN])
        e.set_r(3, 1)

    def extern_execute(self, e):
        self.record_events("ExternExecute", e)
        answers = self.c["answers"]
        answer = 1 if not answers else answers[min(self.calls, len(answers) - 1)]
        self.calls += 1
        e.set_r(3, answer & MASK)

    def malloc(self, e):
        size = e.r(3)
        self.L.rec("Malloc", (size,))
        if self.c["malloc_fails"]:
            e.set_r(3, 0)
            return
        self.block, self.bytes = e.alloc(size, b"\xa5" * size), size
        e.set_r(3, self.block)

    def free(self, e):
        slot = e.r(3)
        block = e.u32(slot)
        is_block = block == self.block and block != 0
        self.L.rec("Free", (1 if is_block else 0, self.bytes // 4, block_hash(rs(e, block, self.bytes // 4)) if is_block else 0))
        ws(e, slot, [0])

    def stubs(self):
        st = CL.Stubs()
        st.real = RENDER_FUNCS
        st.internal(INSTANT, self.instant)
        st.internal(EXTERN_EXECUTE, self.extern_execute)
        st.internal(PUSH_ERROR, lambda e: self.L.rec("Error", (T.s32(e.r(4)), T.s32(e.r(5)))))
        return st

    def build(self):
        e, c = self.e, self.c
        n = len(c["records"])
        self.memory = e.alloc(0x68, b"\xa5" * 0x68)
        self.infos = e.alloc(max(n, 1) * RECORD_SIZE_MAC, b"\xa5" * (max(n, 1) * RECORD_SIZE_MAC))
        ws(e, self.memory + 0x44, [self.infos, c["count"], 0xa5a5a5a5, c["dynamic"], c["next_static"]])
        for i, r_ in enumerate(c["records"]):
            at = self.infos + RECORD_SIZE_MAC * i
            ws(e, at, [r_["flags"], r_["blend"], texture_address(r_["texture"])])
            ws(e, at + 0x30, [fb(r_["depth"])])
        self.list = e.alloc(max(len(c["order"]), 1) * 4 + 8, b"\xa5" * (max(len(c["order"]), 1) * 4 + 8))
        ws(e, self.list, c["order"])
        ws(e, SYSTEM + 0x1c, [c["renderer"]])
        ws(e, DISPLAY, [c["display"]])
        ws(e, DRAW_VAR, [e.fr_draw])
        ws(e, MALLOC_SLOT, [e.fr_malloc, e.fr_free])
        self.before = e.read(self.infos, n * RECORD_SIZE_MAC)

    def call(self):
        e, c = self.e, self.c
        if c["op"] == EXEC_OP:
            return T.s32(T.call_ppc(e, EXECUTE, [self.memory])[0])
        entry = {TEXTURE_OP: SORT_TEXTURE, ASCENDING_OP: SORT_ASCENDING, DESCENDING_OP: SORT_DESCENDING}[c["op"]]
        T.call_ppc(e, entry, [self.memory, self.list, c["low"] & MASK, c["high"] & MASK])
        return 1

    def state(self):
        e, c = self.e, self.c
        out = [T.s32(e.u32(self.memory + 0x48)), T.s32(e.u32(self.memory + 0x50)), T.s32(e.u32(self.memory + 0x54))]
        after = e.read(self.infos, len(c["records"]) * RECORD_SIZE_MAC)
        for i in range(len(c["records"])):
            now = after[RECORD_SIZE_MAC * i:RECORD_SIZE_MAC * (i + 1)]
            was = self.before[RECORD_SIZE_MAC * i:RECORD_SIZE_MAC * (i + 1)]
            flags, blend, texture = struct.unpack(">III", now[:12])
            out += [flags, blend, texture_label(texture), canon(struct.unpack(">I", now[0x30:0x34])[0]),
                    int(now[4:] == was[4:])]
        out += [T.s32(v) for v in rs(e, self.list, len(c["order"]))]
        return out


def ppc_render(e, c):
    with guarded(e):
        side = RenderPpc(e, c)
        e.fr_render = side
        stubs = side.stubs()
        saved = stubs.activate(e)
        try:
            side.build()
            result = side.call()
            return {"result": result, "state": side.state(), "log": side.L.log}
        finally:
            e.fr_render = None
            CL.Stubs.deactivate(e, saved)


def host_render(lib, c):
    bind(lib)
    CL.host_begin(lib, {LABEL_DRAW: lib.tpw_fr_draw_pointer()})
    description = render_description(c)
    d = (C.c_int64 * len(description))(*[signed64(v) for v in description])
    capacity = 64 + 6 * len(c["records"]) + len(c["order"])
    out = (C.c_int64 * capacity)()
    n = lib.tpw_fr_render(c["op"], C.addressof(d), C.addressof(out), capacity)
    assert n <= capacity, "the state buffer is too small"
    return {"result": int(out[0]), "state": [int(v) for v in out[1:n]], "log": CL.host_end(lib)}


# ---- the cases

DEPTHS = [0.0, -0.0, 1.0, -1.0, 2.5, 100.0, 0.5, 3.0e38, -3.0e38, 1.0e-40, INF, -INF, NAN]
FEW_DEPTHS = [1.0, 2.0, 3.0, 1.0, 2.0]


def rdepth(r, few=False):
    if few:
        return r.choice(FEW_DEPTHS)
    k = r.random()
    if k < 0.12:
        return r.choice(DEPTHS)
    if k < 0.3:
        return float(r.randint(-3, 3))
    return T.f32(r.uniform(-50.0, 200.0))


def rrecord(r, few=False, drawn=0.25, skipped=0.4, textures=6):
    flags = (r.getrandbits(32) & r.getrandbits(32) & ~DRAWN & MASK) | (DRAWN if r.random() < drawn else 0)
    blend = (r.getrandbits(32) & r.getrandbits(32) & ~SORT_SKIPPED & MASK) | (SORT_SKIPPED if r.random() < skipped else 0)
    texture = r.randint(0, textures) if r.random() < 0.9 else r.randint(0, 24)
    return record(flags, blend, texture, rdepth(r, few))


def execute_random_case(r, max_records=40):
    n = r.choice([0, 1, 2, 3, 4, 5, 6, 8, 10, 14, 20, r.randint(0, max_records)])
    few = r.random() < 0.4
    records = [rrecord(r, few, drawn=r.choice([0.0, 0.2, 0.5, 0.9]), skipped=r.choice([0.0, 0.3, 0.5, 0.8, 1.0]),
                       textures=r.choice([1, 2, 6, 24])) for _ in range(n)]
    dynamic = r.randint(0, n)
    static = r.randint(0, n - dynamic)
    for i in range(dynamic, n - static):
        records[i]["flags"] = records[i]["flags"] & ~DRAWN & MASK | UNUSED       # not in use: never listed
    statics = list(range(n - static, n))
    instant = [i for i in statics if r.random() < r.choice([0.0, 0.3, 0.7, 1.0])]
    answers = [] if r.random() < 0.7 else [1] * r.randint(0, 5) + [r.choice([0, 0, 1, -1, 7])] + [1]
    display = r.choice([0, 0xc, 0x20000000, 0x2000000c, 0x8, 0x4, 0x2000000d, 0x20000008, r.getrandbits(32)])
    return render_case(EXEC_OP, records, dynamic, static, renderer=r.choice([4, 4, 3, 3, 3, 1, 2, 0, 5, -1]), display=display,
                       malloc_fails=r.random() < 0.04, instant=instant, answers=answers)


def sort_random_case(r, op=None):
    n = r.choice([1, 2, 3, 4, 5, 6, 7, 8, 12, 20, 33, r.randint(1, 60)])
    few = r.random() < 0.5
    records = [rrecord(r, few, textures=r.choice([1, 2, 3, 24])) for _ in range(n)]
    m = r.choice([1, 2, 3, 4, 5, 8, 13, 24, r.randint(1, 60)])
    order = [r.randrange(n) for _ in range(m)] if r.random() < 0.3 else r.sample(range(n), min(m, n))
    m = len(order)
    if r.random() < 0.6:
        low, high = 0, m - 1
    else:
        low = r.randrange(m)
        high = r.randrange(low, m)
    return render_case(op or r.choice([TEXTURE_OP, ASCENDING_OP, DESCENDING_OP]), records, order=order, low=low, high=high)


def execute_edges():
    r = random.Random(71)
    cases = []
    # nothing in the table; one record each way; the table full of drawn records
    cases.append(render_case(EXEC_OP, [], 0, 0))
    cases.append(render_case(EXEC_OP, [record()], 1, 0))
    cases.append(render_case(EXEC_OP, [record()], 0, 1))
    cases.append(render_case(EXEC_OP, [record(DRAWN)] * 3, 3, 0))
    cases.append(render_case(EXEC_OP, [record(DRAWN)] * 3, 0, 3, instant=[]))
    cases.append(render_case(EXEC_OP, [record(0), record(0)], 2, 0, malloc_fails=True))
    # the one-count case that goes to the sort and the execution: dynamic and static, instant ones, every renderer
    for renderer in (3, 4, 1, 0, 2, 5, -1):
        for dyn, stat in ((3, 0), (0, 3), (2, 2), (1, 1), (4, 3)):
            records = [record(0, SORT_SKIPPED if i % 2 else 0, 1 + i % 3, float(5 - i)) for i in range(dyn + stat + 1)]
            cases.append(render_case(EXEC_OP, records, dyn, stat, renderer=renderer,
                                     instant=[len(records) - 1 - i for i in range(stat) if i % 2 == 0]))
    # the display flags: which of 0xc and 0x20000000 are set (not 3 or 4)
    for display in (0, 0x4, 0x8, 0xc, 0x20000000, 0x20000004, 0x20000008, 0x2000000c, 0x2000000d, 0xffffffff, 0x1000000c):
        records = [record(0, 0, 1, d) for d in (3.0, 1.0, 2.0, 5.0, 4.0)]
        cases.append(render_case(EXEC_OP, records, 5, 0, renderer=3, display=display))
        cases.append(render_case(EXEC_OP, records, 2, 3, renderer=3, display=display))
    # renderer 4: every arrangement of the records with and without the sort-skipped bit, lists of 0 to 6 records
    for n in range(0, 7):
        for mask in range(1 << n):
            records = [record(0, SORT_SKIPPED if mask >> i & 1 else 0, (i * 5) % 4 + 1, float((i * 3) % 4)) for i in range(n)]
            cases.append(render_case(EXEC_OP, records, n, 0, renderer=4))
    # an execution that fails at each position, with and without the rest to do
    for n in (1, 2, 3, 5):
        for fail_at in range(n):
            records = [record(0, 0, 1, float(i)) for i in range(n)]
            cases.append(render_case(EXEC_OP, records, n, 0, renderer=3, answers=[1] * fail_at + [0]))
    cases.append(render_case(EXEC_OP, [record(0, 0, 1, 1.0)] * 3, 3, 0, answers=[0]))
    # static records that the instant fake draws, all, some, none; unused records between the two kinds are never listed
    for drawn in ((), (0,), (1,), (0, 1, 2, 3), (2,), (3, 1)):
        records = [record(0, 0, 1 + i, float(i)) for i in range(4)] + [record(UNUSED, 0, 1, 9.0)] * 2 + \
                  [record(0, 0, 1 + i, float(10 + i)) for i in range(4)]
        cases.append(render_case(EXEC_OP, records, 4, 4, renderer=3, instant=[10 - 1 - d for d in drawn]))
    # random
    for _ in range(40):
        cases.append(execute_random_case(r))
    return cases


def sorts_edges(op):
    def gen():
        r = random.Random(80 + op)
        cases = []
        key_sets = {
            TEXTURE_OP: lambda n: [record(0, 0, 1 + (i * 7) % 5, 0.0) for i in range(n)],
            ASCENDING_OP: lambda n: [record(0, 0, 1, float((i * 7) % 5)) for i in range(n)],
            DESCENDING_OP: lambda n: [record(0, 0, 1, float((i * 7) % 5)) for i in range(n)],
        }
        # every length from 1 to 9 over every range, the records with ties
        for n in range(1, 10):
            records = key_sets[op](n)
            for low in range(0, n):
                for high in range(low, n):
                    cases.append(render_case(op, records, order=list(range(n)), low=low, high=high))
        # sorted, reversed, constant and organ-pipe lists of 24
        n = 24
        for pattern in ("ascending", "descending", "constant", "pipe"):
            values = {"ascending": list(range(n)), "descending": list(range(n, 0, -1)), "constant": [3] * n,
                      "pipe": list(range(n // 2)) + list(range(n // 2, 0, -1))}[pattern]
            if op == TEXTURE_OP:
                records = [record(0, 0, 1 + v % 24, 0.0) for v in values]
            else:
                records = [record(0, 0, 1, float(v)) for v in values]
            cases.append(render_case(op, records, order=list(range(n)), low=0, high=n - 1))
            cases.append(render_case(op, records, order=list(reversed(range(n))), low=0, high=n - 1))
        # the texture addresses 1 and 24 (the high bit) against null; depths of NaN, infinities and zeros
        if op == TEXTURE_OP:
            records = [record(0, 0, t, 0.0) for t in (24, 0, 2, 1, 23, 3, 0, 2)]
        else:
            records = [record(0, 0, 1, d) for d in (NAN, 1.0, -0.0, 0.0, INF, -INF, NAN, 2.0, -1.0, 1.0)]
        n = len(records)
        for _ in range(8):
            order = r.sample(range(n), n)
            cases.append(render_case(op, records, order=order, low=0, high=n - 1))
        # an empty range inside the list (the pivot is read all the same) and a list with a record twice
        records = key_sets[op](6)
        cases.append(render_case(op, records, order=[0, 1, 2, 3, 4, 5], low=3, high=2))
        cases.append(render_case(op, records, order=[0, 1, 2, 3, 4, 5], low=4, high=0))
        cases.append(render_case(op, records, order=[2, 2, 3, 3, 2, 5], low=0, high=5))
        for _ in range(40):
            cases.append(sort_random_case(r, op))
        return cases
    return gen


# ================================================================= the tests

TESTS = [
    FnTest("CHField::TestConQuad", ppc_conquad, host_conquad, edge=conquad_edges, rnd=conquad_random),
    FnTest("CycleFrequencyTable", ppc_cycle, host_cycle, edge=cycle_edges, rnd=cycle_random),
    FnTest("CTextureLink::CalcUsage", ppc_run, host_run, edge=calc_edges, rnd=calc_random),
    FnTest("CTextureCache::AutoMipmap", ppc_run, host_run, edge=mipmap_edges, rnd=mipmap_random),
    FnTest("CTextureCache::Entry_Compress", ppc_run, host_run, edge=compress_edges, rnd=compress_random),
    FnTest("CTextureCache::BalanceCache_FreeLockedTextureSpace", ppc_run, host_run, edge=free_edges, rnd=free_random),
    FnTest("CTextureCache::BalanceCache_LevelSurfaceUsage", ppc_run, host_run, edge=level_edges, rnd=level_random),
    FnTest("CRenderMemory::ExecuteUnrenderedPrimitives", ppc_render, host_render, edge=execute_edges,
           rnd=lambda r: execute_random_case(r)),
    FnTest("CRenderMemory::qSortPrimitivesAscending_Texture", ppc_render, host_render, edge=sorts_edges(TEXTURE_OP),
           rnd=lambda r: sort_random_case(r, TEXTURE_OP)),
    FnTest("CRenderMemory::qSortPrimitivesAscending_ZPos", ppc_render, host_render, edge=sorts_edges(ASCENDING_OP),
           rnd=lambda r: sort_random_case(r, ASCENDING_OP)),
    FnTest("CRenderMemory::qSortPrimitivesDescending_ZPos", ppc_render, host_render, edge=sorts_edges(DESCENDING_OP),
           rnd=lambda r: sort_random_case(r, DESCENDING_OP)),
]
