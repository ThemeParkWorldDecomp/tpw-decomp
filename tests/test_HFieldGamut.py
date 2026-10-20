"""The height field's view gamut (src/tb_hfield_gamut.cpp, src/tb_polygon.cpp), original versus
translation: CGamut::CalculateGamut, CGamut::GetHFieldIntersection, CPolygon::DumpData,
CPolygon::GetNewVertexOutsideStatus, CPolygon::IsOutsideOfPlane, GenerateClipXVector and
GenerateClipZVector.

Each case builds its world twice, in emulator memory and through the extern "C" calls of
tests/unit_HFieldGamut.cpp, from one description, runs the function on both and compares what
comes out: the answer, every word and byte of the gamut or the polygon, the globals the unit
only reads, and the call log. Floats are compared bit for bit (a NaN is one value).

The unit's callees are fakes that record the same events on both sides: CMatrix::TransformVertex
(records the matrix, the type and the input, and answers the next three words of the case's
"dir" queue, so a case chooses every corner ray), CMatrix::AdjustProjection (records the matrix
and the far plane) and the MSL assertion (the clip vectors' parameter check). tan and sqrt are
the host's libm on both sides (tpwtest's standard stubs); memcpy, memmove and memset are
faithful.

Unicorn computes fmadds in double precision and rounds twice (FINDINGS 2026-10-10 and
2026-10-11) and rounds a fctiwz above INT32_MAX to 0x80000000 where the processor saturates:
HardwareRounding puts a code hook on every fused instruction of the unit and on every fctiwz of
CalculateGamut and gives the processor's result.

Rows of the gamut: the walk of CalculateGamut writes the byte columns at row indexes that the
polygon gives; a case keeps the rows inside 0..127 (the zero row of a map and at most 127 rows),
because a row outside the arrays reaches the neighbouring words, which the Mac keeps big-endian
and the host little-endian. Columns may leave -128..127 (they are stored truncated).
"""
import contextlib
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE
from unicorn import ppc_const as P

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_HFieldGamut"
MASK = 0xffffffff

# The unit (run as the original on the PowerPC side) and its sizes.
CALCULATE, INTERSECTION = 0x10009f04, 0x10009d60
DUMP_DATA, CLIP_X, CLIP_Z, NEW_STATUS, OUTSIDE = 0x1000b748, 0x1000b7dc, 0x1000b930, 0x1000ba74, 0x1000bb9c
UNIT = {CALCULATE: 6168, INTERSECTION: 356, DUMP_DATA: 96, CLIP_X: 272, CLIP_Z: 256, NEW_STATUS: 228, OUTSIDE: 104}

# The fakes (by address) and the engine data the unit reads.
TRANSFORM_VERTEX, ADJUST_PROJECTION = 0x1001c584, 0x1001c4a8
VIEW_PTR, PROJECTION, FOV, DISPLAY = 0x100b0078, 0x100b007c, 0x100afe8c, 0x10165634
GUARDED = [(VIEW_PTR, 4), (PROJECTION, 0x40), (FOV, 4), (DISPLAY + 4, 4), (DISPLAY + 0x900, 4)]

LAB_GAMUT, LAB_VIEW, LAB_PROJECTION = 1, 2, 3

# CGamut: 0x2d8 bytes. The words are everything but the two arrays of row bytes.
GAMUT_SIZE = 0x2d8
ROW_RANGES = [(0xcc, 0x1cc), (0x1d4, 0x2d4)]
WORD_OFFSETS = [o for o in range(0, GAMUT_SIZE, 4) if not any(a <= o < b for a, b in ROW_RANGES)]
WORD_INDEX = {o: i for i, o in enumerate(WORD_OFFSETS)}
POLYGON_SIZE = 0xd8

SPECIAL_BITS = [0x00000000, 0x80000000, 0x3f800000, 0xbf800000, 0x7f800000, 0xff800000, 0x7fc00000, 0x00000001,
                0x80000001, 0x7f7fffff, 0xff7fffff, 0x00800000, 0x3f000000, 0x40000000, 0x3effffff, 0x3f800001]


# ----------------------------------------------------------------- helpers


def fb(x):
    """A float's bits, as the processor holds it."""
    try:
        return struct.unpack(">I", struct.pack(">f", T.f32(x)))[0]
    except (OverflowError, struct.error):
        return 0x7f800000 if x > 0 else 0xff800000


def bf(w):
    """The float of a bit pattern."""
    return struct.unpack(">f", struct.pack(">I", w & MASK))[0]


def canon(w):
    """Any NaN as one value."""
    return 0x7fc00000 if w & 0x7f800000 == 0x7f800000 and w & 0x007fffff else w & MASK


def ws(e, a, values):
    e.write(a, struct.pack(">%dI" % len(values), *[v & MASK for v in values]))


def rs(e, a, n):
    return list(struct.unpack(">%dI" % n, e.read(a, 4 * n)))


def arr(ctype, values):
    return (ctype * max(1, len(values)))(*values)


def fpr_double(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    bits = v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]
    return struct.unpack(">d", struct.pack(">Q", bits))[0]


def fpr_bits(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


class HardwareRounding:
    """The processor's rounding where Unicorn differs, for the unit's code: each fmadds gets a
    code hook that writes fmaf's result (one rounding) to the destination and steps over the
    instruction, and each fctiwz of CalculateGamut a hook that gives 0x7fffffff for a value
    above INT32_MAX. Operands that are not finite are left to the emulator (an infinity or a NaN
    comes out either way)."""

    def __init__(self, e):
        self.e, self.fused, self.convert = e, {}, {}
        for start, size in UNIT.items():
            words = struct.unpack(">%dI" % (size // 4), e.read(start, size))
            for k, word in enumerate(words):
                address = start + 4 * k
                if word >> 26 == 59 and (word >> 1) & 31 == 29:        # fmadds frD, frA, frC, frB
                    self.fused[address] = ((word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31)
                if word >> 26 == 63 and (word >> 1) & 0x3ff == 15:     # fctiwz frD, frB
                    self.convert[address] = ((word >> 21) & 31, (word >> 11) & 31)
                assert not (word >> 26 == 59 and (word >> 1) & 31 in (28, 30, 31)), "another fused form"
        for address in sorted(self.fused):
            e.uc.hook_add(UC_HOOK_CODE, self.fuse, begin=address, end=address)
        for address in sorted(self.convert):
            e.uc.hook_add(UC_HOOK_CODE, self.truncate, begin=address, end=address)

    def fuse(self, uc, address, size, user):
        rd, ra, rb, rc = self.fused[address]
        a, c, b = fpr_double(self.e, ra), fpr_double(self.e, rc), fpr_double(self.e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        assert all(struct.unpack(">f", struct.pack(">f", x))[0] == x for x in (a, c, b)), "fmadds operand is not a single"
        value = float(LIBM.fmaf(a, c, b))
        uc.reg_write(ppcemu.FPR[rd], struct.unpack(">Q", struct.pack(">d", value))[0])
        uc.reg_write(P.UC_PPC_REG_PC, address + 4)

    def truncate(self, uc, address, size, user):
        rd, rb = self.convert[address]
        if fpr_double(self.e, rb) >= 2147483648.0:
            uc.reg_write(ppcemu.FPR[rd], (fpr_bits(self.e, rd) & 0xFFFFFFFF00000000) | 0x7FFFFFFF)
            uc.reg_write(P.UC_PPC_REG_PC, address + 4)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    HardwareRounding(e)


@contextlib.contextmanager
def guarded(e):
    """The engine globals a case overwrites, put back afterwards."""
    saved = [(a, e.read(a, n)) for a, n in GUARDED]
    try:
        yield
    finally:
        for a, data in saved:
            e.write(a, data)


class Fakes:
    """The fakes of the PowerPC side: the same events as tests/unit_HFieldGamut.cpp."""

    def __init__(self, fixed, queues=None):
        self.L = CL.PpcLog(fixed, queues)
        self.gamut = 0

    def stubs(self):
        st = CL.Stubs()
        st.internal(TRANSFORM_VERTEX, self.transform_vertex)
        st.internal(ADJUST_PROJECTION, self.adjust_projection)
        st.imp("__assertion_failed",
               lambda e: self.L.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))
        return st

    def transform_vertex(self, e):
        L = self.L
        this, out, source, kind = e.r(3), e.r(4), e.r(5), T.s32(e.r(6))
        L.rec("TransformVertex", (L.lab(this), kind, source - self.gamut) + tuple(canon(w) for w in rs(e, source, 3)))
        ws(e, out, [L.ret("dir") for _ in range(3)])

    def adjust_projection(self, e):
        self.L.rec("AdjustProjection", (self.L.lab(e.r(3)), fb(e.f(1))))


def run(e, fn, args, fakes):
    """Calls the original `fn` with the unit's own functions real and the fakes in place."""
    stubs = fakes.stubs()
    stubs.real = set(UNIT)
    saved = stubs.activate(e)
    try:
        return T.call_ppc(e, fn, args)
    finally:
        CL.Stubs.deactivate(e, saved)


def bind(lib):
    """argtypes of the unit's calls (once per library)."""
    if getattr(lib, "hg_bound", False):
        return
    lib.hg_bound = True
    W = C.POINTER(C.c_uint32)
    lib.tpw_hg_pointer.restype = C.c_void_p
    lib.tpw_hg_pointer.argtypes = [C.c_int32]
    lib.tpw_hg_globals.argtypes = [W, C.c_uint32, C.c_uint32, C.c_uint32]
    lib.tpw_hg_globals_back.argtypes = [W]
    lib.tpw_hg_gamut_set.argtypes = [C.c_char_p]
    lib.tpw_hg_gamut_get.argtypes = [C.c_char_p]
    lib.tpw_hg_gamut_run.restype = C.c_int32
    lib.tpw_hg_gamut_run.argtypes = [C.c_int32, C.c_int32, W]
    lib.tpw_hg_clip.argtypes = [C.c_int32, W, W, C.c_uint32, W]
    lib.tpw_hg_polygon.restype = C.c_uint32
    lib.tpw_hg_polygon.argtypes = [C.c_int32, W, C.POINTER(C.c_int8), C.POINTER(C.c_int32), W, C.c_int32]


def host_labels(lib):
    return {LAB_GAMUT: lib.tpw_hg_pointer(0), LAB_VIEW: lib.tpw_hg_pointer(1), LAB_PROJECTION: lib.tpw_hg_pointer(2)}


# ----------------------------------------------------------------- the clip vectors


def clip_case(a, b, plane):
    """Two vectors (three words each) and the plane's bits."""
    return {"a": list(a), "b": list(b), "plane": plane}


def run_clip(e, fn, c):
    out = e.alloc(12, b"\xa5" * 12)
    fakes = Fakes({})
    with guarded(e):
        run(e, fn, [out] + c["a"] + c["b"] + [T.F(bf(c["plane"]))], fakes)
    return {"vector": [canon(w) for w in rs(e, out, 3)], "log": fakes.L.log}


def ppc_clip_x(e, c):
    return run_clip(e, CLIP_X, c)


def ppc_clip_z(e, c):
    return run_clip(e, CLIP_Z, c)


def host_clip(lib, c, axis):
    bind(lib)
    CL.host_begin(lib, {})
    out = arr(C.c_uint32, [0xa5a5a5a5] * 3)
    lib.tpw_hg_clip(axis, arr(C.c_uint32, c["a"]), arr(C.c_uint32, c["b"]), c["plane"], out)
    return {"vector": [canon(w) for w in out[:3]], "log": CL.host_end(lib)}


def host_clip_x(lib, c):
    return host_clip(lib, c, 0)


def host_clip_z(lib, c):
    return host_clip(lib, c, 1)


def clip_between(r, axis, t, special=0.0):
    """A case whose parameter is about t: random vectors, the plane between them."""
    a = [fb(r.uniform(-200, 200)) for _ in range(3)]
    b = [fb(r.uniform(-200, 200)) for _ in range(3)]
    low, high = bf(b[axis * 2]), bf(a[axis * 2])
    plane = fb(low + t * (high - low))
    c = clip_case(a, b, plane)
    if r.random() < special:
        c["a"][r.randrange(3)] = r.choice(SPECIAL_BITS)
    if r.random() < special:
        c["b"][r.randrange(3)] = r.choice(SPECIAL_BITS)
    if r.random() < special:
        c["plane"] = r.choice(SPECIAL_BITS)
    return c


def clip_edges(axis):
    r = random.Random(11 + axis)
    cases = []
    key = axis * 2   # the coordinate the plane cuts: x for X, z for Z
    one = fb(1.0)
    # the parameter exactly 0, 1 and -1, just inside and just outside 1 and -1 (lattice vectors)
    for t_num, t_den in ((0, 1), (1, 1), (-1, 1), (1, 2), (-1, 2), (3, 2), (-3, 2), (2, 1), (-2, 1)):
        for base in (0.0, 4.0, -6.0):
            a = [fb(base + 8.0), fb(3.0), fb(-5.0)]
            b = [fb(base), fb(-7.0), fb(2.0)]
            a[0], a[2] = (a[0], a[2]) if axis == 0 else (a[2], a[0])
            b[0], b[2] = (b[0], b[2]) if axis == 0 else (b[2], b[0])
            plane = fb(bf(b[key]) + 8.0 * t_num / t_den)
            cases.append(clip_case(a, b, plane))
    for delta in (1, -1, 2, -2):
        for edge in (1.0, -1.0):
            a = [fb(10.0), fb(1.0), fb(1.0)]
            b = [fb(2.0), fb(0.0), fb(0.0)]
            if axis == 1:
                a, b = [a[2], a[1], a[0]], [b[2], b[1], b[0]]
            plane_bits = fb(bf(b[key]) + 8.0 * edge)
            cases.append(clip_case(a, b, plane_bits + delta))
    # the edge has no extent along the axis: 0/0 and x/0
    for plane in (fb(3.0), fb(2.0), fb(-1.0), 0, 0x7f800000, 0x7fc00000):
        a = [fb(2.0), fb(5.0), fb(-3.0)]
        b = [fb(2.0), fb(1.0), fb(4.0)]
        cases.append(clip_case(a, b, plane))
    # specials in every slot
    for slot in range(7):
        for bits in SPECIAL_BITS:
            c = clip_between(r, axis, 0.5)
            words = c["a"] + c["b"] + [c["plane"]]
            words[slot] = bits
            c["a"], c["b"], c["plane"] = words[:3], words[3:6], words[6]
            cases.append(c)
    # parameters between -1.5 and 1.5, and a lot inside (the fused products show on full mantissas)
    for _ in range(200):
        cases.append(clip_between(r, axis, r.uniform(-1.5, 1.5)))
    for _ in range(100):
        cases.append(clip_between(r, axis, r.uniform(-1.0, 1.0)))
    return cases


def clip_random(r, axis):
    kind = r.random()
    if kind < 0.7:
        return clip_between(r, axis, r.uniform(-1.2, 1.2), special=0.05)
    words = [r.getrandbits(32) for _ in range(7)]
    return clip_case(words[:3], words[3:6], words[6])


# ----------------------------------------------------------------- the polygon members


def polygon_case(r, count, special=0.0, lattice=False):
    """A polygon: 16 vertices of words (those past `count` are only noise), 16 status bytes (the
    ones the member writes are checked), the counts, and the point v."""
    def coordinate():
        if lattice:
            return fb(r.randint(-4, 4))
        if r.random() < special:
            return r.choice(SPECIAL_BITS)
        return fb(r.uniform(-50, 50))
    vertices = [coordinate() for _ in range(48)]
    c = {"vertices": vertices, "status": [r.choice([0, 1, 0xa5, 0xff, 0, 1]) for _ in range(16)], "count": count,
         "outside": r.randint(-3, 9), "v": [coordinate() for _ in range(3)]}
    if lattice and count and r.random() < 0.6:
        # the point on one of the polygon's own vertices or on the line of an edge
        k = r.randrange(count)
        c["v"] = list(vertices[3 * k:3 * k + 3])
        if r.random() < 0.5:
            c["v"][0] = vertices[3 * ((k + 1) % count)]
    return c


def ppc_polygon(e, c, op, index):
    state = e.alloc(POLYGON_SIZE, b"".join([struct.pack(">I", w) for w in c["vertices"]]) +
                    bytes(c["status"]) + struct.pack(">II", c["count"] & MASK, c["outside"] & MASK) +
                    b"\xa5" * (POLYGON_SIZE - 0xd8))
    fakes = Fakes({})
    with guarded(e):
        if op == 0:
            r3, _, f1 = run(e, OUTSIDE, [state] + c["v"] + [index], fakes)
            answer = canon(fb(f1))
        elif op == 1:
            r3, _, _ = run(e, NEW_STATUS, [state] + c["v"], fakes)
            answer = r3
        else:
            run(e, DUMP_DATA, [state, index], fakes)
            answer = 0
    after = e.read(state, POLYGON_SIZE)
    return {"answer": answer, "vertices": [canon(w) for w in struct.unpack(">48I", after[:0xc0])],
            "status": list(after[0xc0:0xd0]), "counts": list(struct.unpack(">II", after[0xd0:0xd8])),
            "log": fakes.L.log}


def host_polygon(lib, c, op, index):
    bind(lib)
    CL.host_begin(lib, {})
    vertices = arr(C.c_uint32, c["vertices"])
    status = arr(C.c_int8, [b - 256 if b > 127 else b for b in c["status"]])
    counts = arr(C.c_int32, [T.s32(c["count"]), T.s32(c["outside"])])
    answer = lib.tpw_hg_polygon(op, vertices, status, counts, arr(C.c_uint32, c["v"]), index)
    return {"answer": answer & MASK, "vertices": [canon(w) for w in list(vertices)[:48]],
            "status": [b & 0xff for b in list(status)[:16]], "counts": [counts[0] & MASK, counts[1] & MASK],
            "log": CL.host_end(lib)}


def ppc_outside(e, c):
    return ppc_polygon(e, c, 0, c["index"])


def host_outside(lib, c):
    return host_polygon(lib, c, 0, c["index"])


def ppc_status(e, c):
    return ppc_polygon(e, c, 1, 0)


def host_status(lib, c):
    return host_polygon(lib, c, 1, 0)


def ppc_dump(e, c):
    return ppc_polygon(e, c, 2, c["pointer"])


def host_dump(lib, c):
    return host_polygon(lib, c, 2, c["pointer"])


def with_index(r, c):
    count = max(c["count"], 1)
    c["index"] = r.choice([0, 1, 2, count - 1, r.randrange(16)]) if r.random() < 0.8 else r.randrange(16)
    if c["index"] == 0 and c["count"] < 1:
        c["count"] = 1
    return c


def outside_edges():
    r = random.Random(21)
    cases = []
    for count in range(1, 9):
        for index in range(count):
            for _ in range(3):
                c = polygon_case(r, count, lattice=True)
                c["index"] = index
                cases.append(c)
    for count in (3, 5, 16):
        for _ in range(20):
            cases.append(with_index(r, polygon_case(r, count, special=0.1)))
    # the point exactly on the edge's line, a unit step either side of it
    for dz in (-1, 0, 1):
        c = polygon_case(r, 3, lattice=True)
        c["vertices"][:9] = [fb(0), fb(0), fb(0), fb(4), fb(0), fb(4), fb(2), fb(0), fb(0)]
        c["v"] = [fb(2), fb(7), fb(2 + dz)]
        c["index"] = 1
        cases.append(c)
    return cases


def outside_random(r):
    kind = r.random()
    if kind < 0.4:
        return with_index(r, polygon_case(r, r.randint(1, 8), lattice=True))
    return with_index(r, polygon_case(r, r.randint(1, 16), special=0.05))


def status_edges():
    r = random.Random(22)
    cases = []
    for count in range(0, 10):
        for _ in range(6):
            cases.append(polygon_case(r, count, lattice=True))
    for count in (3, 4, 5, 16):
        for _ in range(12):
            cases.append(polygon_case(r, count, special=0.08))
    # a square: the point inside, on each edge's line, outside one edge, outside two
    square = [fb(v) for v in (0, 0, 0, 0, 0, 4, 4, 0, 4, 4, 0, 0)] + [fb(0)] * 36
    for point in ((2, 2), (0, 2), (4, 2), (2, 0), (2, 4), (-1, 2), (5, 2), (2, -1), (2, 5), (-1, -1), (5, 5), (0, 0)):
        c = polygon_case(r, 4, lattice=True)
        c["vertices"] = list(square)
        c["v"] = [fb(point[0]), fb(1), fb(point[1])]
        cases.append(c)
    return cases


def status_random(r):
    if r.random() < 0.4:
        return polygon_case(r, r.randint(0, 9), lattice=True)
    return polygon_case(r, r.randint(0, 16), special=0.05)


def dump_edges():
    r = random.Random(23)
    cases = []
    for count in list(range(0, 20)) + [100, 1000, 4097]:
        for pointer in (0, 1):
            c = polygon_case(r, count)
            c["pointer"] = pointer
            cases.append(c)
    return cases


def dump_random(r):
    c = polygon_case(r, r.choice([0, 1, 5, 8, 9, 16, 17, 100, r.randint(0, 3000)]))
    c["pointer"] = r.choice([0, 1])
    return c


# ----------------------------------------------------------------- the gamut

def set_word(c, offset, word):
    c["words"][WORD_INDEX[offset]] = word & MASK


def pack_gamut(c, big):
    image = bytearray(GAMUT_SIZE)
    fmt = ">I" if big else "<I"
    for offset, word in zip(WORD_OFFSETS, c["words"]):
        struct.pack_into(fmt, image, offset, word & MASK)
    for (start, end), rows in zip(ROW_RANGES, c["rows"]):
        image[start:end] = bytes(rows)
    return bytes(image)


def unpack_gamut(image, big):
    fmt = ">I" if big else "<I"
    return {"words": [canon(struct.unpack_from(fmt, image, o)[0]) for o in WORD_OFFSETS],
            "rows": [bytes(image[a:b]) for a, b in ROW_RANGES]}


def vector_words(values):
    return [fb(v) for v in values]


def gamut_case(r, fov=60.0, aspect=0.75, distance=400.0, camera=(0.0, 10.0, 0.0), y_clip=(0.0, 5.0),
               x_map=(0.0, 64.0, 1.0), z_map=(0.0, 64.0, 1.0), directions=None, counts=None, view=None):
    """x_map and z_map are (low, high, cell) as CGamut::SetX and SetZ get them; the counts
    (mF54) follow them unless given. directions: four vectors, the rays CMatrix::TransformVertex
    will answer for the four corners in order."""
    c = {"words": [r.getrandbits(32) for _ in WORD_OFFSETS],
         "rows": [[r.getrandbits(8) for _ in range(256)] for _ in ROW_RANGES]}
    set_word(c, 0x30, fb(y_clip[0]))
    set_word(c, 0x34, fb(y_clip[1]))
    xl, xh, xc = x_map
    zl, zh, zc = z_map
    for offset, v in ((0x3c, xl), (0x40, xh), (0x44, zl), (0x48, zh), (0x4c, xc), (0x50, zc)):
        set_word(c, offset, fb(v))
    if counts is None:
        xn = int(T.f32((xh - xl) / xc)) if xc else 0
        zn = int(T.f32((zh - zl) / zc)) if zc else 0
        counts = [xn, zn, xn - 1, zn - 1]
    for offset, v in zip((0x54, 0x58, 0x5c, 0x60), counts):
        set_word(c, offset, v)
    if view is None:
        view = [r.getrandbits(32) for _ in range(16)]
    view = list(view)
    view[12:15] = vector_words(camera)
    c["view"] = view
    c["fov"], c["aspect"], c["distance"] = fb(fov), fb(aspect), fb(distance)
    c["dir"] = [w for d in (directions if directions is not None else [(0, -1, 1)] * 4) for w in vector_words(d)]
    tie_end_row(r, c)
    return c


def tie_end_row(r, c):
    """The grid points' end row (0x1d0) is compared with the map's z count (0x58): often make
    them equal, or a row apart."""
    count = T.s32(c["words"][WORD_INDEX[0x58]])
    pick = r.random()
    if pick < 0.25:
        set_word(c, 0x1d0, count)
    elif pick < 0.4:
        set_word(c, 0x1d0, count + r.choice([-1, 1]))
    elif pick < 0.5:
        set_word(c, 0x1d0, r.randint(0, 130))


def globals_ppc(e, c, view_address):
    ws(e, view_address, c["view"])
    ws(e, VIEW_PTR, [view_address])
    ws(e, FOV, [c["fov"]])
    ws(e, DISPLAY + 4, [c["aspect"]])
    ws(e, DISPLAY + 0x900, [c["distance"]])


def read_globals_ppc(e, view_address):
    return [canon(w) for w in rs(e, view_address, 16) + rs(e, FOV, 1) + rs(e, DISPLAY + 4, 1) +
            rs(e, DISPLAY + 0x900, 1)]


def ppc_gamut_world(e, c):
    gamut = e.alloc(GAMUT_SIZE, pack_gamut(c, True))
    view = e.alloc(0x40, b"\xa5" * 0x40)
    return gamut, view


def ppc_calculate(e, c):
    gamut, view = ppc_gamut_world(e, c)
    fakes = Fakes({gamut: LAB_GAMUT, view: LAB_VIEW, PROJECTION: LAB_PROJECTION}, {"dir": c["dir"]})
    fakes.gamut = gamut
    with guarded(e):
        globals_ppc(e, c, view)
        r3, _, _ = run(e, CALCULATE, [gamut], fakes)
        result = {"answer": r3, "gamut": unpack_gamut(e.read(gamut, GAMUT_SIZE), True),
                  "globals": read_globals_ppc(e, view), "log": fakes.L.log}
    return result


def host_calculate(lib, c):
    bind(lib)
    labels = host_labels(lib)
    lib.tpw_hg_gamut_set(pack_gamut(c, False))
    lib.tpw_hg_globals(arr(C.c_uint32, c["view"]), c["fov"], c["aspect"], c["distance"])
    CL.host_begin(lib, labels, {"dir": c["dir"]})
    answer = lib.tpw_hg_gamut_run(0, 0, arr(C.c_uint32, [0] * 3))
    log = CL.host_end(lib)
    image = C.create_string_buffer(GAMUT_SIZE)
    lib.tpw_hg_gamut_get(image)
    back = arr(C.c_uint32, [0] * 19)
    lib.tpw_hg_globals_back(back)
    return {"answer": answer, "gamut": unpack_gamut(image.raw, False), "globals": [canon(w) for w in list(back)[:19]],
            "log": log}


def ppc_intersection(e, c):
    gamut, view = ppc_gamut_world(e, c)
    out = e.alloc(12, b"\xa5" * 12)
    fakes = Fakes({gamut: LAB_GAMUT, view: LAB_VIEW, PROJECTION: LAB_PROJECTION}, {"dir": c["dir"]})
    fakes.gamut = gamut
    with guarded(e):
        globals_ppc(e, c, view)
        run(e, INTERSECTION, [out, gamut, gamut + 12 * c["corner"]], fakes)
        result = {"vector": [canon(w) for w in rs(e, out, 3)], "gamut": unpack_gamut(e.read(gamut, GAMUT_SIZE), True),
                  "globals": read_globals_ppc(e, view), "log": fakes.L.log}
    return result


def host_intersection(lib, c):
    bind(lib)
    labels = host_labels(lib)
    lib.tpw_hg_gamut_set(pack_gamut(c, False))
    lib.tpw_hg_globals(arr(C.c_uint32, c["view"]), c["fov"], c["aspect"], c["distance"])
    CL.host_begin(lib, labels, {"dir": c["dir"]})
    out = arr(C.c_uint32, [0xa5a5a5a5] * 3)
    lib.tpw_hg_gamut_run(1, c["corner"], out)
    log = CL.host_end(lib)
    image = C.create_string_buffer(GAMUT_SIZE)
    lib.tpw_hg_gamut_get(image)
    back = arr(C.c_uint32, [0] * 19)
    lib.tpw_hg_globals_back(back)
    return {"vector": [canon(w) for w in list(out)[:3]], "gamut": unpack_gamut(image.raw, False),
            "globals": [canon(w) for w in list(back)[:19]], "log": log}


# ---- GetHFieldIntersection cases

def intersection_case(r, special=0.0, lattice=False):
    if lattice:
        c = gamut_case(r, camera=(r.randint(-3, 3), r.randint(1, 9), r.randint(-3, 3)),
                       y_clip=(r.choice([0, 1, 2, 3]), r.choice([0, 1, 2, 4, 8])),
                       distance=r.choice([8.0, 16.0, 100.0, 400.0]),
                       directions=[(r.choice([-2, -1, -0.5, 0, 0.5, 1, 2]), r.choice([-4, -2, -1, -0.5, 0, 0.5, 1]),
                                    r.choice([-2, -1, 0, 1, 2])) for _ in range(1)])
    else:
        c = gamut_case(r, camera=(r.uniform(-50, 50), r.uniform(-5, 60), r.uniform(-50, 50)),
                       y_clip=(r.uniform(-3, 8), r.uniform(-3, 20)), distance=r.uniform(10, 800),
                       directions=[(r.uniform(-2, 2), r.uniform(-2, 1), r.uniform(-2, 2)) for _ in range(1)])
    c["corner"] = r.randrange(4)
    if r.random() < special:
        c["dir"][r.randrange(3)] = r.choice(SPECIAL_BITS)
    if r.random() < special:
        set_word(c, r.choice((0x30, 0x34)), r.choice(SPECIAL_BITS))
    if r.random() < special:
        c["distance"] = r.choice(SPECIAL_BITS)
    if r.random() < special:
        c["view"][r.choice((12, 13, 14))] = r.choice(SPECIAL_BITS)
    return c


def intersection_edges():
    r = random.Random(31)
    cases = []
    # no y in the ray: replaced by 0.001 (and -0.0 counts as no y too)
    for y_bits in (0x00000000, 0x80000000, 0x00000001, 0x3a83126f):
        for corner in range(4):
            c = intersection_case(r)
            c["dir"][1] = y_bits
            c["corner"] = corner
            cases.append(c)
    # the two planes' parameters: both inside the half distance and equal (the planes meet), both
    # inside and different, one below 0, one above the half, both clamped, exactly 0 and half
    for low, high in ((2, 2), (2, 6), (6, 2), (-2, 6), (6, -2), (-2, -3), (500, 600), (0, 0), (0, 200), (200, 0), (200, 200),
                      (199, 201), (201, 199), (3, 600), (600, 3), (-1, -1)):
        c = gamut_case(r, camera=(1.0, 10.0, -2.0), y_clip=(10.0 - low, 10.0 - high), distance=400.0,
                       directions=[(1.0, -1.0, 0.5)])
        c["corner"] = r.randrange(4)
        cases.append(c)
    # the camera on the plane, a step either side of the half distance
    for step in (-1.0, 0.0, 1.0):
        c = gamut_case(r, camera=(0.0, 10.0, 0.0), y_clip=(10.0 - 200.0 + step, 10.0), distance=400.0,
                       directions=[(0.25, -1.0, 0.75)])
        c["corner"] = 2
        cases.append(c)
    for special in (0.3, 0.6):
        for _ in range(10):
            cases.append(intersection_case(r, special=special))
    for _ in range(150):
        cases.append(intersection_case(r, lattice=True))
    for _ in range(100):
        cases.append(intersection_case(r))
    return cases


def intersection_random(r):
    k = r.random()
    if k < 0.3:
        return intersection_case(r, lattice=True)
    if k < 0.4:
        return intersection_case(r, special=0.3)
    return intersection_case(r)


# ---- CalculateGamut cases

def unit_ray(r, forward, sx, sy, tan_y, aspect):
    """The ray of a view corner: forward + sx * (tan / aspect) right + sy * tan up, rounded to
    single precision; yaw and pitch come with forward as (yaw, pitch) in degrees."""
    yaw, pitch = math.radians(forward[0]), math.radians(forward[1])
    f = (math.sin(yaw) * math.cos(pitch), math.sin(pitch), math.cos(yaw) * math.cos(pitch))
    right = (math.cos(yaw), 0.0, -math.sin(yaw))
    up = (-math.sin(yaw) * math.sin(pitch), math.cos(pitch), -math.cos(yaw) * math.sin(pitch))
    tan_x = tan_y / aspect
    return tuple(f[i] + sx * tan_x * right[i] + sy * tan_y * up[i] for i in range(3))


def natural_case(r):
    """A view like the lobby's: a camera above a map of 4 to 120 cells a side, looking down at it."""
    fov = r.choice([45.0, 60.0, 70.0, 75.0, 90.0, 100.0, r.uniform(30, 120)])
    aspect = r.choice([0.75, 0.5625, 0.8, 1.0, r.uniform(0.4, 1.6)])
    cell_x = r.choice([1.0, 2.0, 4.0, 8.0, 0.5, 16.0 / 3.0, 1.0])
    cell_z = cell_x if r.random() < 0.6 else r.choice([1.0, 2.0, 4.0, 8.0, 0.5])
    nx, nz = r.randint(4, 120), r.randint(4, 120)
    x_low = cell_x * r.choice([0, 0, 1, -3, 5, -20])
    z_low = cell_z * r.choice([0, 0, 0, 1, 3])
    x_map = (x_low, x_low + nx * cell_x, cell_x)
    z_map = (z_low, z_low + nz * cell_z, cell_z)
    width, depth = x_map[1] - x_map[0], z_map[1] - z_map[0]
    camera = (r.uniform(x_map[0] - 0.3 * width, x_map[1] + 0.3 * width), r.uniform(1, 60),
              r.uniform(z_map[0] - 0.3 * depth, z_map[1] + 0.3 * depth))
    y_low = r.uniform(-1, 4)
    y_clip = (y_low, y_low + r.uniform(0, 20))
    tan_y = math.tan(math.radians(fov) / 2)
    forward = (r.uniform(0, 360), r.uniform(-75, 15))
    directions = [unit_ray(r, forward, sx, sy, tan_y, aspect) for sx, sy in ((-1, 1), (1, 1), (1, -1), (-1, -1))]
    c = gamut_case(r, fov=fov, aspect=aspect, distance=r.choice([200.0, 325.0, 450.0, 700.0, r.uniform(20, 800)]),
                   camera=camera, y_clip=y_clip, x_map=x_map, z_map=z_map, directions=directions)
    if r.random() < 0.3:
        # the map's counts need not follow its extent (a wrong z count cuts the grid points' end row)
        counts = [r.randint(-3, 130) for _ in range(4)]
        for offset, v in zip((0x54, 0x58, 0x5c, 0x60), counts):
            set_word(c, offset, v)
        tie_end_row(r, c)
    return c


def wild_case(r):
    """Anything goes inside the safe limits: rays and cameras in every direction, clip planes
    anywhere, the map extents from a single cell to the full 127 rows."""
    cell_x = r.choice([1.0, 2.0, 0.25, 3.0])
    cell_z = r.choice([1.0, 2.0, 0.25, 3.0])
    z_offset = r.randint(0, 20)
    nx, nz = r.randint(1, 127), r.randint(1, 126 - z_offset)
    x_low = cell_x * r.randint(-60, 10)
    z_low = cell_z * z_offset
    x_map = (x_low, x_low + nx * cell_x, cell_x)
    z_map = (z_low, z_low + nz * cell_z, cell_z)
    camera = (r.uniform(x_map[0] - 20, x_map[1] + 20), r.uniform(-10, 40), r.uniform(z_map[0] - 20, z_map[1] + 20))
    y_clip = (r.uniform(-10, 40), r.uniform(-10, 40))
    directions = [(r.uniform(-3, 3), r.choice([r.uniform(-3, 3), r.uniform(-3, -0.1)]), r.uniform(-3, 3))
                  for _ in range(4)]
    return gamut_case(r, fov=r.uniform(20, 120), aspect=r.uniform(0.3, 2.0), distance=r.uniform(5, 900),
                      camera=camera, y_clip=y_clip, x_map=x_map, z_map=z_map, directions=directions)


def lattice_case(r):
    """Small dyadic numbers: rays and points coincide, line up and meet the planes exactly."""
    cx, cz = r.randint(-2, 8), r.randint(0, 8)
    cam_y = r.choice([2, 4, 8, 16])
    y_low = r.choice([0, 0, 1, 2])
    y_high = r.choice([0, 2, 4, 8, y_low])
    dirs = [(r.choice([-2, -1, -0.5, 0, 0.5, 1, 2]), r.choice([-4, -2, -1, -1, -0.5]), r.choice([-2, -1, 0, 0.5, 1, 2]))
            for _ in range(4)]
    if r.random() < 0.3:
        dirs = [dirs[0]] * 4
    elif r.random() < 0.3:
        dirs = [(dirs[0][0] * k, dirs[0][1], dirs[0][2]) for k in (1, 2, -1, 0.5)]
    elif r.random() < 0.3:
        dirs = [(k, dirs[0][1], dirs[0][2]) for k in (-1, 1, 2, -2)]
    xn, zn = r.randint(2, 20), r.randint(2, 20)
    x_low, z_low = r.choice([0, -2, 3]), r.choice([0, 0, 1])
    return gamut_case(r, fov=90.0, aspect=1.0, distance=r.choice([16.0, 64.0, 400.0]), camera=(cx, cam_y, cz),
                      y_clip=(y_low, y_high), x_map=(x_low, x_low + xn, 1.0), z_map=(z_low, z_low + zn, 1.0),
                      directions=dirs)


def tie_case(r, camera_x, step_x):
    """The pre-step of an edge of the walk (a fused multiply-add) lands exactly between two
    singles when the product is rounded first: slope -(2^24-1) * 2^-46 times 8388609 * 2^-24,
    added to a value 4 - 2^-22 or 3: fused it rounds below the whole number and truncates one
    less, unfused it ties to the whole number. The polygon is the camera and one point
    (all four rays alike): the edges from the camera to the point and back are the two walks."""
    camera_z = 0.5 - 2.0 ** -24
    return gamut_case(r, fov=90.0, aspect=1.0, camera=(camera_x, 10.0, camera_z), y_clip=(9.0, 9.0),
                      x_map=(0.0, 16.0, 1.0), z_map=(0.0, 16.0, 1.0),
                      directions=[(step_x, -1.0, 3.0 + 2.0 ** -22)] * 4)


def calculate_edges():
    r = random.Random(41)
    cases = []
    # the fused pre-step of a rising edge and of a falling one (see tie_case)
    cases.append(tie_case(r, 3.0, -3.0 * 2.0 ** -22))
    cases.append(tie_case(r, 3.0 + 4.0 * 2.0 ** -22, -3.0 * 2.0 ** -22))
    # a lobby-like view
    cases.append(gamut_case(r, directions=[(-1.0, -1.0, 1.0), (1.0, -1.0, 1.0), (1.0, -1.0, 2.0), (-1.0, -1.0, 2.0)],
                            camera=(32.0, 10.0, 0.0), x_map=(0.0, 64.0, 1.0), z_map=(0.0, 64.0, 1.0),
                            y_clip=(0.0, 5.0)))
    # all four rays alike, the planes meeting at the camera's height, an empty map
    c = gamut_case(r, directions=[(0.5, -1.0, 0.5)] * 4)
    cases.append(c)
    cases.append(gamut_case(r, directions=[(0.5, -1.0, 0.5)] * 4, y_clip=(10.0, 10.0)))
    cases.append(gamut_case(r, directions=[(0.5, -1.0, 0.5)] * 4, x_map=(0.0, 1.0, 1.0), z_map=(0.0, 1.0, 1.0)))
    cases.append(gamut_case(r, directions=[(0.5, -1.0, 0.5)] * 4, x_map=(100.0, 164.0, 1.0)))
    # the camera outside the map on each side
    for cam in ((-30.0, 8.0, 20.0), (100.0, 8.0, 20.0), (20.0, 8.0, -30.0), (20.0, 8.0, 100.0), (20.0, 100.0, 20.0)):
        cases.append(gamut_case(r, camera=cam, directions=[(-1.0, -1.0, 1.0), (1.0, -1.0, 1.0), (1.0, -2.0, 2.0),
                                                           (-1.0, -2.0, 2.0)]))
    # a NaN or an infinity in one coordinate of one ray (CPolygon::AddVertex: a NaN side takes the collinear path; the walk skips
    # an edge whose z is NaN and the clip puts every cut on its plane, so the rows stay inside)
    lobby = [(-1.0, -1.0, 1.0), (1.0, -1.0, 1.0), (1.0, -1.0, 2.0), (-1.0, -1.0, 2.0)]
    for corner in range(4):
        for component in range(3):
            for special in (math.nan, math.inf, -math.inf):
                directions = [list(d) for d in lobby]
                directions[corner][component] = special
                cases.append(gamut_case(r, directions=directions, camera=(32.0, 10.0, 0.0)))
    for _ in range(600):
        cases.append(natural_case(r))
    for _ in range(500):
        cases.append(lattice_case(r))
    for _ in range(500):
        cases.append(wild_case(r))
    return cases


def calculate_random(r):
    k = r.random()
    if k < 0.4:
        return natural_case(r)
    if k < 0.7:
        return lattice_case(r)
    return wild_case(r)


# ----------------------------------------------------------------- the tests

TESTS = [
    FnTest("GenerateClipXVector", ppc_clip_x, host_clip_x, edge=lambda: clip_edges(0), rnd=lambda r: clip_random(r, 0)),
    FnTest("GenerateClipZVector", ppc_clip_z, host_clip_z, edge=lambda: clip_edges(1), rnd=lambda r: clip_random(r, 1)),
    FnTest("CPolygon::IsOutsideOfPlane", ppc_outside, host_outside, edge=outside_edges, rnd=outside_random),
    FnTest("CPolygon::GetNewVertexOutsideStatus", ppc_status, host_status, edge=status_edges, rnd=status_random),
    FnTest("CPolygon::DumpData", ppc_dump, host_dump, edge=dump_edges, rnd=dump_random),
    FnTest("CGamut::GetHFieldIntersection", ppc_intersection, host_intersection, edge=intersection_edges,
           rnd=intersection_random),
    FnTest("CGamut::CalculateGamut", ppc_calculate, host_calculate, edge=calculate_edges, rnd=calculate_random),
]
