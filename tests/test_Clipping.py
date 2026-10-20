"""The engine's polygon clipper (engine library, tb_clipping.cpp): CClipping::CPP_TestData,
CheckBoundingBox and the three ClipPoly overloads.

CPP_TestData and CheckBoundingBox run on vertices and boxes of special and random values. The
ClipPoly overloads run on polygons in four areas of words (the input and output buffers, the
vertex area and the area of lists and index fans); after every call the test compares the
result, the call log and the whole of the four areas.

Everything outside the unit is a fake on both sides that records its arguments: the six edge
clippers (VertexClip_*), which record the places of their three vertices and the first eight
words of the two they read and write the next words of the case's queue over the vertex they
make; the math unit's strided Transform, which records the matrix and the box corners it is
given and answers with the case's eight transformed vertices; and the assertion, which records
and leaves the call. CheckBoundingBox and ClipPoly(indices) call CPP_TestData and
ClipPoly(list) as the real functions.

The original's fused multiply-adds run under HardwareRounding, as in test_OpenGLDraw.py:
Unicorn rounds them twice.
"""
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
SHIM_LIBRARY = "unit_Clipping"

TEST_DATA, CHECK_BOX, CLIP_OUT_IN, CLIP_LIST, CLIP_INDICES = 0x1003F6DC, 0x1003F4D4, 0x10040640, 0x1004087C, 0x10040B24
# The six edge clippers by plane (the descriptors of the table at 0x10052068 point at these).
EDGES = [0x1003F8B0, 0x1003FB68, 0x1003FD94, 0x1003FFC8, 0x100401F4, 0x10040424]
TRANSFORM = 0x10041C90        # CMatrixPowerPC::Transform (strided), the slot the box test calls
MATH_UNIT = 0x100AFE94        # gei_MathUnit
GEI_CLIPPING = 0x102B3638     # gei_Clipping
MATH_UNIT_CTOR = "__ct__14CMatrixPowerPCFv"
MASK = 0xFFFFFFFF
AREA_WORDS = 2048
FILL = 0xA5A5A5A5
IN, OUT, VERTICES, INDICES = 1, 2, 3, 4
LEFT = 0x100                  # the box result when the assertion left the call


def setup(runner):
    """The standard stubs, and a real math unit for gei_MathUnit to point at (its strided
    Transform is the fake of each case)."""
    e = runner.emu
    T.install_std_stubs(e)
    obj = T.persist_alloc(e, 0x14)
    T.call_ppc(e, e.func(MATH_UNIT_CTOR), [obj, 1])
    e.cl_math_unit = e.u32(obj)


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MASK))


def canon(word):
    """A NaN (as float bits) as one value."""
    if (word & 0x7F800000) == 0x7F800000 and (word & 0x007FFFFF):
        return 0x7FC00000
    return word


# ----------------------------------------------------------------- the emulator's fused multiply-adds

LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]
CODE_RANGES = [(CLIP_OUT_IN, CLIP_OUT_IN + 500), (CLIP_LIST, CLIP_LIST + 604)]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    if isinstance(v, int):
        return v
    return struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Unicorn (QEMU 5.0) computes fmadds, fmsubs, fnmadds and fnmsubs in double precision and
    rounds the result to single, which can differ by one unit in the last place from the
    processor's single rounding of the exact result. A code hook over the functions that use
    them looks at each instruction before it runs and rewrites its target register just before
    the next one, with the C library's fmaf (one rounding)."""

    changed = 0    # results the correction replaced (all cases so far), for the notes

    def __init__(self, e):
        self.e = e
        self.pending = None
        self.hooks = []

    def __enter__(self):
        for begin, end in CODE_RANGES:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            target, value = self.pending
            self.pending = None
            corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
            if fpr(e, target) != corrected and not (value != value and fpr_double(e, target) != fpr_double(e, target)):
                HardwareRounding.changed += 1
            uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        for x in (a, c, b):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        subtract = kind in (28, 30)
        value = float(LIBM.fmaf(a, c, -b if subtract else b))
        if kind in (30, 31):
            value = -value
        self.pending = (rd, value)


# ----------------------------------------------------------------- values

fb = T.f32_bits
SPECIAL = [0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 2.0, -2.0, 1e-30, -1e-30, 3.4028234663852886e38, -3.4028234663852886e38,
           1.401298464324817e-45, -1.401298464324817e-45, 1.1754943508222875e-38, math.inf, -math.inf, 0.1, -0.1, 1e20]
# Words that are NaN in several ways: quiet and signalling, either sign, with payloads.
NAN_WORDS = [0x7FC00000, 0xFFC00000, 0x7F800001, 0xFF800001, 0x7FFFFFFF, 0xFFFFFFFF, 0x7FA00000, 0x7FC00001]


def special_word(r):
    """A special float as its bits: zeros, infinities, extremes, denormals, NaNs."""
    if r.random() < 0.2:
        return r.choice(NAN_WORDS)
    return fb(r.choice(SPECIAL))


def moderate(r, lo=-3.0, hi=3.0):
    """A float32 in a moderate range, as its bits."""
    return fb(T.f32(r.uniform(lo, hi)))


def neighbour(word, r, spread=2):
    """The word a few units in the last place away from `word` (the sign bit stays)."""
    sign = word & 0x80000000
    magnitude = (word & 0x7FFFFFFF) + r.randint(-spread, spread)
    return sign | (max(0, min(0x7FFFFFFF, magnitude)))


def negate(word):
    return word ^ 0x80000000


def boundary_vertex(r):
    """A vertex (x, y, z, w) on or next to the planes of its own w: coordinates equal to w, -w,
    |w|, -|w|, 0 or a unit in the last place off one of them, z around w, 0 and 2w + |w|."""
    w = r.choice([moderate(r, 0.1, 4.0), negate(moderate(r, 0.1, 4.0)),
                  fb(r.choice([0.0, -0.0, 1.0, -1.0, 2.0, 1e-30]))])
    magnitude = w & 0x7FFFFFFF

    def coordinate():
        base = r.choice([w, negate(w), magnitude, negate(magnitude), 0, 0x80000000])
        if r.random() < 0.7:
            return neighbour(base, r)
        return base

    z = coordinate()
    if r.random() < 0.3:
        # z - 2w is about |w| when z = w (+ rounding): a float near 3w for w < 0
        wf = T.bits_f32(w)
        z = neighbour(fb(T.f32(wf * r.choice([1.0, 2.0, 3.0, -1.0, 0.5, 1.0000001]))), r)
    return [coordinate(), coordinate(), z, w]


def random_vertex(r):
    """A vertex of any of these kinds, as four words."""
    k = r.random()
    if k < 0.35:
        return boundary_vertex(r)
    if k < 0.65:
        w = moderate(r, 0.1, 3.0)
        wf = T.bits_f32(w)
        return [fb(T.f32(wf * r.uniform(-2.5, 2.5))), fb(T.f32(wf * r.uniform(-2.5, 2.5))),
                fb(T.f32(wf * r.uniform(-0.5, 1.5))), w]
    if k < 0.8:
        return [r.getrandbits(32) for _ in range(4)]
    if k < 0.9:
        return [special_word(r) for _ in range(4)]
    w = negate(moderate(r, 0.1, 3.0))
    wf = T.bits_f32(w)
    return [fb(T.f32(wf * r.uniform(-2.5, 2.5))), fb(T.f32(wf * r.uniform(-2.5, 2.5))),
            fb(T.f32(wf * r.uniform(-1.5, 0.5))), w]


def framed_vertex(r):
    """A vertex around the view volume (x and y within 2.5 w, z from -0.5 w to 1.5 w, w from 0.1 to 3)."""
    w = moderate(r, 0.1, 3.0)
    wf = T.bits_f32(w)
    return [fb(T.f32(wf * r.uniform(-2.5, 2.5))), fb(T.f32(wf * r.uniform(-2.5, 2.5))),
            fb(T.f32(wf * r.uniform(-0.5, 1.5))), w]


def inside_vertex(r):
    """A vertex well inside all six planes."""
    w = moderate(r, 0.5, 3.0)
    wf = T.bits_f32(w)
    return [fb(T.f32(wf * r.uniform(-0.9, 0.9))), fb(T.f32(wf * r.uniform(-0.9, 0.9))),
            fb(T.f32(wf * r.uniform(0.1, 0.9))), w]


def with_tail(r, four, stride):
    """The four coordinates followed by the vertex's other words (random), `stride` bytes in all."""
    return list(four) + [r.getrandbits(32) for _ in range(stride // 4 - 4)]


def bytes_of(words, big):
    return struct.pack((">" if big else "<") + "%dI" % len(words), *words)


# ----------------------------------------------------------------- CPP_TestData


def testdata_ppc(e, case):
    words, count, stride, mask = case["words"], case["count"], case["stride"], case["mask"]
    result = e.alloc(4, b"\xa5" * 4)
    codes = e.alloc(max(count, 1) + 4, b"\xa5" * (max(count, 1) + 4))
    vertices = e.alloc(max(len(words) * 4, 4), bytes_of(words, True) if words else None)
    T.call_ppc(e, TEST_DATA, [result, 0x1234, codes, vertices, count, stride, mask])
    after = list(struct.unpack(">%dI" % len(words), e.read(vertices, 4 * len(words)))) if words else []
    # The last two bytes of the result are stack leftovers in the original.
    return dict(result=list(e.read(result, 2)), codes=list(e.read(codes, max(count, 1) + 4)), vertices=after)


def testdata_host(lib, case):
    words, count, stride, mask = case["words"], case["count"], case["stride"], case["mask"]
    fn = lib["tpw_cp_testdata"]
    fn.restype = None
    fn.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.c_int32, C.c_int32, C.c_uint32]
    result = (C.c_uint8 * 4)(*([0xA5] * 4))
    codes = (C.c_uint8 * (max(count, 1) + 4))(*([0xA5] * (max(count, 1) + 4)))
    vertices = (C.c_uint32 * max(len(words), 1))(*words)
    fn(C.addressof(result), C.addressof(codes), C.addressof(vertices), count, stride, mask)
    return dict(result=list(result[:2]), codes=list(codes), vertices=list(vertices)[:len(words)])


def testdata_case(vertices, stride=0x20, mask=0xFF):
    """A run of vertices (four coordinate words each) laid `stride` bytes apart, other words 0xa5a5a5a5."""
    words = []
    for four in vertices:
        words += list(four) + [FILL] * (stride // 4 - 4)
    return dict(words=words, count=len(vertices), stride=stride, mask=mask)


def testdata_edges():
    r = random.Random(301)
    # Every special w with a few special and boundary coordinates.
    for w in [fb(v) for v in SPECIAL] + NAN_WORDS:
        vertices = []
        for _ in range(12):
            vertices.append([special_word(r), special_word(r), special_word(r), w])
        yield testdata_case(vertices)
    # The six boundaries of each sign of w, each coordinate a few units in the last place either side.
    for w in (fb(1.0), fb(2.5), fb(-1.0), fb(-2.5), fb(1e-30), fb(-1e-30), fb(3.0e38), fb(-3.0e38), fb(0.0), fb(-0.0)):
        vertices = []
        magnitude = w & 0x7FFFFFFF
        for base in (w, negate(w), magnitude, negate(magnitude), 0, 0x80000000):
            for delta in (-2, -1, 0, 1, 2):
                coordinate = (base & 0x80000000) | (((base & 0x7FFFFFFF) + delta) & 0x7FFFFFFF)
                vertices.append([coordinate, coordinate, coordinate, w])
        yield testdata_case(vertices[:15])
        yield testdata_case(vertices[15:])
    # z - 2w against |w| for w below zero: z = w, z = 3w, and the roundings around them.
    for w in (-1.0, -3.0, -0.1, -1e-30, -1e30, -3.4028234663852886e38, -1.1754943508222875e-38,
              -1.401298464324817e-45, -math.inf):
        vertices = []
        for z in (w, 3 * w, w * 1.0000001, 3 * w * 1.0000001, 0.0, -0.0, math.inf, -math.inf, 1e30, -1e30, 1e-30):
            for word in (fb(T.f32(z)), neighbour(fb(T.f32(z)), r, 1)):
                vertices.append([fb(0.0), fb(0.0), word, fb(w)])
        yield testdata_case(vertices[:11])
        yield testdata_case(vertices[11:])
    # Not enough room to matter: counts, strides and masks.
    base = [boundary_vertex(r) for _ in range(40)]
    for count in (0, 1, 2, 3, 8, 33):
        for stride in (16, 0x18, 0x20, 0x28, 0x30):
            yield testdata_case(base[:count], stride=stride)
    for mask in (0, 0xFF, 0x3F, 0x15, 0x2A, 0x01, 0x20, 0x80, 0xFFFFFF00, 0xFFFFFFFF, 0x12345678):
        yield testdata_case(base[:20], mask=mask)
    # Planes the six codes can all be set for at once, and none.
    yield testdata_case([[fb(5.0), fb(5.0), fb(-5.0), fb(1.0)], [fb(-5.0), fb(-5.0), fb(5.0), fb(1.0)]])
    yield testdata_case([[fb(0.0), fb(0.0), fb(0.5), fb(1.0)]] * 4)
    yield testdata_case([[fb(0.0), fb(0.0), fb(0.5), fb(1.0)], [fb(9.0), fb(0.0), fb(0.5), fb(1.0)]])


def testdata_random(r):
    count = r.choice([1, 2, 3, 4, 6, 8, 12, 17, 24])
    stride = r.choice([0x20, 0x20, 0x20, 0x10, 0x18, 0x28, 0x30])
    mask = r.choice([0xFF, 0xFF, 0xFF, 0x3F, r.getrandbits(8), r.getrandbits(32)])
    vertices = [random_vertex(r) for _ in range(count)]
    case = testdata_case(vertices, stride=stride, mask=mask)
    # the other words of a vertex are random now and then
    if r.random() < 0.3:
        case["words"] = [word if i % (stride // 4) < 4 else r.getrandbits(32) for i, word in enumerate(case["words"])]
    return case


# ----------------------------------------------------------------- CheckBoundingBox


def box_ppc(e, case):
    L = CL.PpcLog({}, dict(Transformed=case["transformed"]))
    e.cl = L
    object_words, box, matrix = case["object"], case["box"], case["matrix"]
    obj = e.alloc(8, bytes_of(object_words, True))
    box_address = e.alloc(24, bytes_of(box, True))
    matrix_address = e.alloc(64, bytes_of(matrix, True))
    L.labels[matrix_address] = 5
    saved_unit = e.u32(MATH_UNIT)
    w32(e, MATH_UNIT, e.cl_math_unit if case["unit"] else 0)
    st = CL.Stubs()
    st.real = {CHECK_BOX, TEST_DATA}

    def transform(e):
        matrix_pointer, out, source = e.r(4), e.r(5), e.r(6)
        L.rec("Transform", (L.lab(matrix_pointer), int(out != 0), int(source != 0), T.s32(e.r(7)), T.s32(e.r(8)),
                            T.s32(e.r(9)), T.s32(e.r(10))))
        m = struct.unpack(">16I", e.read(matrix_pointer, 64))
        L.rec("Matrix", m[:8])
        L.rec("Matrix", m[8:])
        c = [canon(word) for word in struct.unpack(">24I", e.read(source, 96))]
        L.rec("Corners", c[:8])
        L.rec("Corners", c[8:16])
        L.rec("Corners", c[16:])
        e.write(out, struct.pack(">32I", *[L.ret("Transformed") & MASK for _ in range(32)]))

    st.internal(TRANSFORM, transform)
    st.imp("__assertion_failed", assertion_stub(L))
    saved = st.activate(e)
    try:
        r3 = T.call_ppc(e, CHECK_BOX, [obj, box_address, matrix_address])[0]
    finally:
        CL.Stubs.deactivate(e, saved)
        w32(e, MATH_UNIT, saved_unit)
    result = LEFT if any(entry[0] == "assert" for entry in L.log) else r3
    return dict(result=result, object=list(struct.unpack(">2I", e.read(obj, 8))),
                box=list(struct.unpack(">6I", e.read(box_address, 24))),
                matrix=list(struct.unpack(">16I", e.read(matrix_address, 64))), log=L.log)


def assertion_stub(L):
    """The assertion: record it and end the call, as the host's fake does."""
    def stub(e):
        L.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4))))
        e.uc.reg_write(T.P.UC_PPC_REG_PC, ppcemu.RETURN_ADDR)
    return stub


def box_host(lib, case):
    fn = lib["tpw_cp_box"]
    fn.restype = C.c_uint32
    fn.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.c_int32]
    CL.host_begin(lib, {}, dict(Transformed=case["transformed"]))
    box = (C.c_uint32 * 6)(*case["box"])
    matrix = (C.c_uint32 * 16)(*case["matrix"])
    obj = (C.c_uint32 * 2)(*case["object"])
    result = fn(C.addressof(box), C.addressof(matrix), C.addressof(obj), case["unit"])
    log = CL.host_end(lib)
    return dict(result=result, object=list(obj), box=list(box), matrix=list(matrix), log=log)


def box_cases_transformed(r, kind):
    """The eight transformed vertices (32 words) of a kind: inside, all outside one plane, mixed,
    behind the viewer, random, or with z values for the smallest-z test."""
    if kind == "inside":
        vertices = [inside_vertex(r) for _ in range(8)]
    elif kind == "outside":
        vertices = []
        for _ in range(8):
            four = inside_vertex(r)
            w = T.bits_f32(four[3])
            four[0] = fb(T.f32(w * r.uniform(1.1, 3.0)))
            vertices.append(four)
    elif kind == "mixed":
        vertices = [framed_vertex(r) for _ in range(8)]
    elif kind == "behind":
        vertices = [negate_w(framed_vertex(r)) for _ in range(8)]
    elif kind == "zs":
        vertices = []
        for _ in range(8):
            four = inside_vertex(r)
            four[2] = r.choice([fb(0.0), fb(-0.0), fb(-1.0), fb(-3.5), fb(2.0), special_word(r), moderate(r, -5, 5)])
            vertices.append(four)
    else:
        vertices = [random_vertex(r) for _ in range(8)]
    return [word for four in vertices for word in four]


def negate_w(four):
    four = list(four)
    four[3] = negate(four[3])
    return four


def box_words(r, kind):
    """A box as six words: minimum corner then maximum corner."""
    if kind == "unit":
        return [fb(v) for v in (0.0, 0.0, 0.0, 1.0, 1.0, 1.0)]
    if kind == "degenerate":
        corner = [moderate(r, -10, 10) for _ in range(3)]
        return corner + corner
    if kind == "reversed":
        low = [moderate(r, -10, 0) for _ in range(3)]
        high = [moderate(r, 0, 10) for _ in range(3)]
        return high + low
    if kind == "huge":
        return [fb(v) for v in (-3.0e38, -3.0e38, -3.0e38, 3.0e38, 3.0e38, 3.0e38)]
    if kind == "special":
        return [special_word(r) for _ in range(6)]
    if kind == "random":
        return [r.getrandbits(32) for _ in range(6)]
    low = [moderate(r, -50, 50) for _ in range(3)]
    return low + [fb(T.f32(T.bits_f32(low[i]) + r.uniform(0, 30))) for i in range(3)]


def box_case(r, box_kind="normal", transformed_kind="mixed", unit=1):
    return dict(box=box_words(r, box_kind), matrix=[moderate(r, -5, 5) for _ in range(16)],
                transformed=box_cases_transformed(r, transformed_kind), unit=unit,
                object=[r.getrandbits(32), r.choice([r.getrandbits(32), fb(5.0), fb(-7.0), fb(0.0)])])


def box_edges():
    r = random.Random(302)
    for unit in (0, 1):
        for transformed in ("inside", "outside", "mixed", "behind", "zs", "random"):
            yield box_case(r, "normal", transformed, unit)
    for box in ("unit", "degenerate", "reversed", "huge", "special", "random", "normal"):
        for transformed in ("inside", "outside", "mixed", "zs"):
            yield box_case(r, box, transformed)
    # z values: each one alone as the smallest, equal values, negative zero, NaN
    for z in (-1.0, -0.0, 0.0, 1.0, -3.5, math.inf, -math.inf, math.nan):
        case = box_case(r, "unit", "inside")
        case["transformed"][2] = fb(z) if z == z else 0x7FC00000
        yield case
    for index in range(8):
        case = box_case(r, "unit", "inside")
        case["transformed"][index * 4 + 2] = fb(-2.0 - index)
        yield case
    # every combination of the outside codes of two corners
    for code in range(0, 64):
        case = box_case(r, "unit", "inside")
        w = T.bits_f32(case["transformed"][3])
        case["transformed"][0] = fb(T.f32(w * 3.0)) if code & 8 else case["transformed"][0]
        case["transformed"][1] = fb(T.f32(w * 3.0)) if code & 32 else case["transformed"][1]
        case["transformed"][2] = fb(T.f32(-w)) if code & 1 else case["transformed"][2]
        yield case


def box_random(r):
    box = r.choice(["normal", "normal", "normal", "unit", "degenerate", "reversed", "special", "random"])
    transformed = r.choice(["inside", "outside", "mixed", "mixed", "behind", "zs", "random"])
    return box_case(r, box, transformed, 1 if r.random() < 0.95 else 0)


# ----------------------------------------------------------------- the areas of the ClipPoly tests


def pack_area(region, image, big):
    """An area's words (halfwords for the lists of area 4) as memory."""
    fmt = ("%dH" % (2 * AREA_WORDS)) if region == INDICES else ("%dI" % AREA_WORDS)
    return struct.pack((">" if big else "<") + fmt, *image)


def blank_areas(r, scramble):
    """The four areas: filled with 0xa5, or with random words (an area of halfwords for area 4)."""
    areas = {}
    for region in (IN, OUT, VERTICES, INDICES):
        size = 2 * AREA_WORDS if region == INDICES else AREA_WORDS
        if scramble:
            bits = 16 if region == INDICES else 32
            areas[region] = [r.getrandbits(bits) for _ in range(size)]
        else:
            areas[region] = [0xA5A5 if region == INDICES else FILL] * size
    return areas


def put(areas, region, byte_offset, values):
    """Writes words (halfwords in area 4) at a byte offset."""
    size = 2 if region == INDICES else 4
    start = byte_offset // size
    areas[region][start:start + len(values)] = values


def edge_queue(r, stride, calls=48, style="inside"):
    """The words the edge clippers write: a vertex of `stride` bytes per call, inside by default."""
    words = []
    for _ in range(calls):
        four = inside_vertex(r) if style == "inside" else framed_vertex(r)
        words += with_tail(r, four, stride)
    return words


def polygon_vertices(r, n, stride, style):
    """n vertices of `stride` bytes: framed around the view volume, or plainly random words."""
    vertices = []
    for _ in range(n):
        if style == "random":
            four = [r.getrandbits(32) for _ in range(4)]
        elif style == "inside":
            four = inside_vertex(r)
        elif style == "special":
            four = random_vertex(r)
        else:
            four = framed_vertex(r)
        vertices.append(with_tail(r, four, stride))
    return vertices


class Memory:
    """The four areas on the PowerPC side, and the places of pointers into them."""

    def __init__(self, e, areas):
        self.bases = {}
        for region in (IN, OUT, VERTICES, INDICES):
            self.bases[region] = e.alloc(4 * AREA_WORDS, pack_area(region, areas[region], True))

    def place(self, address):
        if address == 0:
            return 0
        for region, base in self.bases.items():
            if base <= address < base + 4 * AREA_WORDS:
                return (region << 32) + (address - base)
        return -1

    def at(self, region, byte_offset):
        return self.bases[region] + byte_offset

    def dump(self, e):
        out = {}
        for region, base in self.bases.items():
            fmt = ">%dH" % (2 * AREA_WORDS) if region == INDICES else ">%dI" % AREA_WORDS
            out[region] = list(struct.unpack(fmt, e.read(base, 4 * AREA_WORDS)))
        return out


def edge_stubs(e, L, memory, st):
    """The six edge clippers as fakes: they record and write the next words of the queue."""
    def make(which):
        def stub(e):
            out, outside, inside, stride = e.r(3), e.r(4), e.r(5), T.s32(e.r(6))
            L.rec("Edge", (which, memory.place(out), memory.place(outside), memory.place(inside), stride))
            n = min(stride // 4, 8)
            for name, vertex in (("EdgeOutside", outside), ("EdgeInside", inside)):
                words = struct.unpack(">%dI" % n, e.read(vertex, 4 * n)) if n > 0 else ()
                L.rec(name, tuple(words) + (-1,) * (8 - n))
            written = [L.ret("EdgeOut") & MASK for _ in range(stride // 4)]
            if written:
                e.write(out, struct.pack(">%dI" % len(written), *written))
        return stub

    for which, address in enumerate(EDGES):
        st.internal(address, make(which))


def host_areas(lib, areas):
    """Resets the host's areas and loads the case's."""
    lib["tpw_cp_reset"]()
    load = lib["tpw_cp_load"]
    load.restype = None
    load.argtypes = [C.c_int32, C.c_void_p, C.c_int32]
    for region in (IN, OUT, VERTICES, INDICES):
        data = pack_area(region, areas[region], False)
        load(region, data, len(data))


def host_dump(lib):
    store = lib["tpw_cp_store"]
    store.restype = None
    store.argtypes = [C.c_int32, C.c_void_p, C.c_int32]
    out = {}
    for region in (IN, OUT, VERTICES, INDICES):
        if region == INDICES:
            buffer = (C.c_uint16 * (2 * AREA_WORDS))()
        else:
            buffer = (C.c_uint32 * AREA_WORDS)()
        store(region, C.addressof(buffer), 4 * AREA_WORDS)
        out[region] = list(buffer)
    return out


def ppc_call(e, case, call):
    """Sets up the case's fakes, memory and fused-multiply-add correction on the PowerPC side, runs
    call(memory, L) and returns what it returns with the log and the areas."""
    L = CL.PpcLog({}, dict(EdgeOut=case["edge"]))
    e.cl = L
    memory = Memory(e, case["areas"])
    st = CL.Stubs()
    st.real = {TEST_DATA, CHECK_BOX, CLIP_OUT_IN, CLIP_LIST, CLIP_INDICES}
    edge_stubs(e, L, memory, st)
    saved = st.activate(e)
    try:
        with HardwareRounding(e):
            result = call(memory, L)
    finally:
        CL.Stubs.deactivate(e, saved)
    result["log"] = L.log
    result["areas"] = memory.dump(e)
    return result


def host_call(lib, case, call):
    """The same on the host: the edge clippers' queue and log, the areas, then call(lib)."""
    host_areas(lib, case["areas"])
    queue = lib["tpw_cp_edge_queue"]
    queue.restype = None
    queue.argtypes = [C.c_void_p, C.c_int32]
    words = (C.c_uint32 * max(len(case["edge"]), 1))(*case["edge"])
    queue(C.addressof(words), len(case["edge"]))
    CL.host_begin(lib, {}, {})
    try:
        result = call(lib)
    finally:
        log = CL.host_end(lib)
    result["log"] = log
    result["areas"] = host_dump(lib)
    return result


# ----------------------------------------------------------------- ClipPoly (output buffer, input buffer)


def clip1_ppc(e, case):
    def call(memory, L):
        this = e.alloc(8, b"\xa5" * 8)
        count = e.alloc(4, struct.pack(">i", case["count"]))
        r3 = T.call_ppc(e, CLIP_OUT_IN, [this, case["planes"], memory.at(OUT, case["out_offset"]),
                                         memory.at(IN, case["in_offset"]), count, case["stride"]])[0]
        return dict(result=T.s32(r3), count=e.s32(count))
    return ppc_call(e, case, call)


def clip1_host(lib, case):
    fn = lib["tpw_cp_clip1"]
    fn.restype = C.c_int32
    fn.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_void_p, C.c_int32]

    def call(lib):
        count = C.c_int32(case["count"])
        result = fn(case["planes"], case["out_offset"], case["in_offset"], C.addressof(count), case["stride"])
        return dict(result=result, count=count.value)
    return host_call(lib, case, call)


def clip1_case(r, planes, vertices, stride, style="inside", scramble=None):
    """A polygon in the input buffer (at a word offset), the output buffer scrambled or not."""
    areas = blank_areas(r, r.random() < 0.5 if scramble is None else scramble)
    in_offset = 4 * r.choice([0, 0, 1, 7, 16])
    out_offset = 4 * r.choice([0, 0, 2, 9, 32])
    put(areas, IN, in_offset, [word for vertex in vertices for word in vertex])
    return dict(planes=planes, stride=stride, count=len(vertices), in_offset=in_offset, out_offset=out_offset,
                areas=areas, edge=edge_queue(r, stride, style=style))


def plane_mask(r):
    return r.choice([0x3F, 0x3F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, r.randrange(1, 64), r.randrange(1, 64),
                     r.randrange(1, 64), 0, -1, 0x40, 0x7FFFFFFF, 0x41, -64, r.getrandbits(32)])


def clip1_edges():
    r = random.Random(303)
    # No planes, one plane at a time, all planes; triangles and quads; every stride.
    for stride in (0x20, 0x10, 0x18, 0x28, 0x30):
        for planes in (0, 1, 2, 4, 8, 16, 32, 63, 21, 42, -1, 0x7FFFFFFF, 64, 0x80, -2, -64):
            for n in (3, 4):
                yield clip1_case(r, planes, polygon_vertices(r, n, stride, "framed"), stride)
    # The polygon wholly inside (nothing to clip), wholly outside (nothing left), and an empty one.
    for planes in (63, 1, 32):
        yield clip1_case(r, planes, polygon_vertices(r, 3, 0x20, "inside"), 0x20)
    for n in (0, 1, 2):
        for planes in (0, 1, 63):
            yield clip1_case(r, planes, polygon_vertices(r, n, 0x20, "framed"), 0x20)
    outside = [with_tail(r, [fb(5.0), fb(0.0), fb(0.5), fb(1.0)], 0x20) for _ in range(3)]
    for planes in (8, 63, 4):
        yield clip1_case(r, planes, outside, 0x20)
    # Left to right in the table: each plane with a polygon crossing only it.
    for plane in range(6):
        for n in (3, 4, 5):
            vertices = polygon_vertices(r, n, 0x20, "inside")
            coordinate = (1, 1, 0, 0, 2, 2)[plane]
            for vertex in vertices[:2]:
                w = T.bits_f32(vertex[3])
                vertex[coordinate] = fb(T.f32(w * (3.0 if plane % 2 == 0 and plane < 4 else -3.0)))
            yield clip1_case(r, 1 << (5 - plane), vertices, 0x20)
    # Zero and special values.
    for style in ("special", "random"):
        for _ in range(12):
            yield clip1_case(r, plane_mask(r), polygon_vertices(r, r.choice([3, 4]), 0x20, style), 0x20)


def clip1_random(r):
    stride = r.choice([0x20, 0x20, 0x20, 0x10, 0x18, 0x28, 0x30])
    n = r.choice([3, 3, 3, 4, 4, 5, 6])
    style = r.choice(["framed", "framed", "framed", "inside", "special", "random"])
    return clip1_case(r, plane_mask(r), polygon_vertices(r, n, stride, style), stride)


# ----------------------------------------------------------------- ClipPoly (list of vertex numbers)


def clip2_ppc(e, case):
    def call(memory, L):
        this = e.alloc(8, b"\xa5" * 8)
        count = e.alloc(4, struct.pack(">i", case["count"]))
        list_pointer = memory.at(INDICES, case["list_offset"])
        record = e.alloc(8, struct.pack(">iI", case["list_count"], list_pointer))
        r3 = T.call_ppc(e, CLIP_LIST, [this, case["planes"], memory.at(VERTICES, case["vertex_offset"]), record, count,
                                       case["stride"]])[0]
        list_count, pointer = struct.unpack(">iI", e.read(record, 8))
        # the original returns the list's count minus two (the host's declaration is void)
        return dict(count=e.s32(count), list_count=list_count, list_pointer_kept=pointer == list_pointer,
                    returns_count_minus_two=T.s32(r3) == list_count - 2)
    return ppc_call(e, case, call)


def clip2_host(lib, case):
    fn = lib["tpw_cp_clip2"]
    fn.restype = None
    fn.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_void_p, C.c_void_p, C.c_int32]

    def call(lib):
        count = C.c_int32(case["count"])
        list_count = C.c_int32(case["list_count"])
        fn(case["planes"], case["vertex_offset"], case["list_offset"], C.addressof(list_count), C.addressof(count),
           case["stride"])
        return dict(count=count.value, list_count=list_count.value, list_pointer_kept=True,
                    returns_count_minus_two=True)
    return host_call(lib, case, call)


def numbered_case(r, planes, n, stride, style, extra=None, shuffled=False, with_list=True):
    """n vertices numbered from the start of the vertex area (some more that the list leaves out),
    a list naming n of them, and the vertex count the clipper appends after."""
    total = n + (r.choice([0, 0, 1, 3]) if extra is None else extra)
    vertices = polygon_vertices(r, total, stride, style)
    numbers = list(range(total))
    if shuffled:
        r.shuffle(numbers)
    numbers = numbers[:n]
    areas = blank_areas(r, r.random() < 0.5)
    vertex_offset = 4 * r.choice([0, 0, 3, 10])
    list_offset = 2 * r.choice([3, 3, 4, 10, 40])
    put(areas, VERTICES, vertex_offset, [word for vertex in vertices for word in vertex])
    put(areas, INDICES, list_offset, numbers)
    count = total + r.choice([0, 0, 0, 2])
    return dict(planes=planes, stride=stride, count=count, vertex_offset=vertex_offset, list_offset=list_offset,
                list_count=n, areas=areas, edge=edge_queue(r, stride, style="inside"), numbers=numbers)


# Triangles the fake edge clippers grow into long polygons through six planes (their vertices land
# on either side of the later planes), found by a search over seeds. The original's work list
# holds fourteen entries and a bigger one overwrites its own stack, so only seeds whose
# in-between lists fit are here: the final polygon has 8 to 19 vertices (the longest fills the
# unrolled loop of the index fan twice).
GROWN_SEEDS = [16, 3, 13, 68, 15, 551, 20, 1467, 949, 1664, 12864, 206170, 495404, 558066, 646467]


def grown_list_case(seed):
    r = random.Random(seed)
    case = numbered_case(r, 63, 3, 0x20, "framed", extra=0)
    case["edge"] = edge_queue(r, 0x20, calls=64, style="framed")
    return case


def clip2_edges():
    r = random.Random(304)
    for stride in (0x20, 0x10, 0x18, 0x28, 0x30):
        for planes in (0, 1, 2, 4, 8, 16, 32, 63, 21, 42, -1, 0x7FFFFFFF, 64, -64):
            for n in (3, 4):
                yield numbered_case(r, planes, n, stride, "framed")
    for planes in (63, 1, 32):
        yield numbered_case(r, planes, 3, 0x20, "inside")
    for n in (0, 1, 2):
        for planes in (0, 1, 63):
            yield numbered_case(r, planes, n, 0x20, "framed")
    outside = numbered_case(r, 8, 3, 0x20, "framed")
    for number in outside["numbers"]:
        start = outside["vertex_offset"] // 4 + number * 8
        outside["areas"][VERTICES][start] = fb(5.0)
        outside["areas"][VERTICES][start + 1] = fb(0.0)
        outside["areas"][VERTICES][start + 2] = fb(0.5)
        outside["areas"][VERTICES][start + 3] = fb(1.0)
    for planes in (8, 63, 4):
        case = dict(outside)
        case["planes"] = planes
        yield case
    # a shuffled list, a longer one, special and random vertices
    for n in (3, 4, 5, 6, 8):
        for planes in (63, 1, 2, 5, 0x2A):
            yield numbered_case(r, planes, n, 0x20, "framed", shuffled=True)
    for style in ("special", "random"):
        for _ in range(12):
            yield numbered_case(r, plane_mask(r), r.choice([3, 4]), 0x20, style)
    # The edge clippers' vertices land outside later planes.
    for _ in range(10):
        case = numbered_case(r, 63, 3, 0x20, "framed")
        case["edge"] = edge_queue(r, 0x20, style="framed")
        yield case
    for seed in GROWN_SEEDS:
        yield grown_list_case(seed)


def clip2_random(r):
    stride = r.choice([0x20, 0x20, 0x20, 0x10, 0x18, 0x28, 0x30])
    n = r.choice([3, 3, 3, 3, 4, 4, 5])
    style = r.choice(["framed", "framed", "framed", "inside", "special", "random"])
    case = numbered_case(r, plane_mask(r), n, stride, style, shuffled=r.random() < 0.2)
    if r.random() < 0.15:
        case["edge"] = edge_queue(r, stride, style="framed")
    return case


# ----------------------------------------------------------------- ClipPoly (index fan)


def clip3_ppc(e, case):
    def call(memory, L):
        this = e.alloc(8, b"\xa5" * 8)
        count = e.alloc(4, struct.pack(">i", case["count"]))
        saved = e.u32(GEI_CLIPPING)
        w32(e, GEI_CLIPPING, this)
        indices = memory.at(INDICES, case["index_offset"])
        try:
            r3 = T.call_ppc(e, CLIP_INDICES, [this, case["planes"], indices, memory.at(VERTICES, case["vertex_offset"]),
                                              count, case["stride"]])[0]
        finally:
            w32(e, GEI_CLIPPING, saved)
        return dict(result=memory.place(r3), count=e.s32(count))
    return ppc_call(e, case, call)


def clip3_host(lib, case):
    fn = lib["tpw_cp_clip3"]
    fn.restype = C.c_int64
    fn.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_void_p, C.c_int32]

    def call(lib):
        count = C.c_int32(case["count"])
        result = fn(case["planes"], case["index_offset"], case["vertex_offset"], C.addressof(count), case["stride"])
        return dict(result=result, count=count.value)
    return host_call(lib, case, call)


def fan_case(r, planes, stride, style, extra=None, shuffled=False):
    """A triangle whose numbers sit just before the write place of the index area."""
    case = numbered_case(r, planes, 3, stride, style, extra=extra, shuffled=shuffled)
    case["index_offset"] = case["list_offset"] + 6
    return case


def clip3_edges():
    r = random.Random(305)
    for stride in (0x20, 0x10, 0x18, 0x28, 0x30):
        for planes in (0, 1, 2, 4, 8, 16, 32, 63, 21, 42, -1, 0x7FFFFFFF, 64, -64):
            yield fan_case(r, planes, stride, "framed")
    for planes in (63, 1, 32):
        yield fan_case(r, planes, 0x20, "inside")
    outside = fan_case(r, 8, 0x20, "framed")
    for number in outside["numbers"]:
        start = outside["vertex_offset"] // 4 + number * 8
        outside["areas"][VERTICES][start:start + 4] = [fb(5.0), fb(0.0), fb(0.5), fb(1.0)]
    for planes in (8, 63, 4):
        case = dict(outside)
        case["planes"] = planes
        yield case
    for style in ("special", "random"):
        for _ in range(12):
            yield fan_case(r, plane_mask(r), 0x20, style)
    for planes in (63, 0x2A, 0x15, 3, 12, 48, 5, 10, 20, 40):
        for _ in range(4):
            yield fan_case(r, planes, 0x20, "framed", shuffled=True)
    for seed in GROWN_SEEDS:
        case = grown_list_case(seed)
        case["index_offset"] = case["list_offset"] + 6
        yield case


def clip3_random(r):
    stride = r.choice([0x20, 0x20, 0x20, 0x10, 0x18, 0x28, 0x30])
    style = r.choice(["framed", "framed", "framed", "inside", "special", "random"])
    case = fan_case(r, plane_mask(r), stride, style, shuffled=r.random() < 0.2)
    if r.random() < 0.15:
        case["edge"] = edge_queue(r, stride, style="framed")
    return case


TESTS = [
    FnTest("CClipping::CPP_TestData", testdata_ppc, testdata_host, edge=testdata_edges, rnd=testdata_random),
    FnTest("CClipping::CheckBoundingBox", box_ppc, box_host, edge=box_edges, rnd=box_random),
    FnTest("CClipping::ClipPoly (output and input buffers)", clip1_ppc, clip1_host, edge=clip1_edges, rnd=clip1_random),
    FnTest("CClipping::ClipPoly (list of vertex numbers)", clip2_ppc, clip2_host, edge=clip2_edges, rnd=clip2_random),
    FnTest("CClipping::ClipPoly (index fan)", clip3_ppc, clip3_host, edge=clip3_edges, rnd=clip3_random),
]
