"""The height field's helpers and the face sorter (src/tb_hfield_helpers.cpp), original versus
translation: CHField::CheckGamutArea, ProcessTextureParameters, QSort_textures, qSortTextures_lpTex,
GetTexturesUsed, CalcFullLighting and CalcLine, and QSortFaces_float.

Each case builds its world twice, in emulator memory and through the extern "C" calls of
tests/unit_HFieldHelpers.cpp, from one description, runs the function on both and compares what
comes out: the answer, the arrays and fields it may write (with poison around them, so a write
off the used part shows), whether the object and the records are unchanged, and the log and the
dump of what the fakes saw.

The fakes (the same on both sides, in the C++ file and in the stubs below) are the light
manager's CalcIntensities, the math unit's strided Transform, CClipping::CPP_TestData and the
assertion. Each records every argument and dumps the words it was given; CalcIntensities,
Transform and CPP_TestData also write results that mix those words, so a wrong input shows in
the output as well. CsTexture::GetTextureEntry and CTextureEntry::GetTexture are inline in the
headers; the PowerPC side runs the originals' copies and the host the header's.

Unicorn computes fmadds in double precision and rounds twice (FINDINGS 2026-10-10 and
2026-10-11); HardwareRounding (one code hook per fused instruction of CalcLine) gives each the
processor's single rounding. NaNs are one value wherever a word is dumped, mixed or compared.

A missing math unit in CalcLine asserts and then reads through the NULL: the PowerPC side faults
there (the test checks the fault is at an address below 0x1000) and the host's assertion fake
throws, so the log up to the assertion and the vertices written so far are compared.

Not tested: index arithmetic that wraps 32 bits; a grid wider than the 128 points of
CalcFullLighting's arrays; rows of CheckGamutArea below 0 whose bytes are a multi-byte count's
(they are only used where the count's bytes read the same big- and little-endian).
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
SHIM_LIBRARY = "unit_HFieldHelpers"
MASK = 0xffffffff

CHECK_GAMUT, LIGHTING, CALC_LINE, QSORT_TEXTURES = 0x1000e760, 0x1000e824, 0x1000ea40, 0x1000f060
USED, PROCESS, QSORT_QUADS, QSORT_FACES = 0x1000f1b0, 0x1000f3f8, 0x1000f4ec, 0x10006d40
CALC_INTENSITIES, CPP_TEST_DATA, GET_TEXTURE_ENTRY, PTR_GLUE = 0x10014938, 0x1003f6dc, 0x100171ec, 0x1004c858
CALC_LINE_SIZE = 672

SYSTEM, SETTINGS, LIGHT_VAR, MATH_VAR, FINAL_VAR, CLIP_VAR = 0x10165f80, 0x10165fc8, 0x100af1b8, 0x100afe94, 0x100aff70, 0x102b3638
GUARDED = [(SYSTEM, 8), (SETTINGS, 4), (LIGHT_VAR, 4), (MATH_VAR, 4), (FINAL_VAR, 4), (CLIP_VAR, 4)]

FIELD_SIZE = 0x12974        # CHField on the Mac
RECORD_SIZE = 0x30          # sHField
ENTRY_SIZE = 0x5c           # CTextureEntry
COLOURS_AT, MARK_AT = 0x2968, 0x12968
POISON = 0xa5a5a5a5
LABEL_FIELD, LABEL_LIGHT, LABEL_MATH, LABEL_MATRIX, LABEL_CLIP = 1, 2, 3, 4, 5
N_ENTRIES, N_PAGES = 16, 8


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


def arr(ctype, values):
    return (ctype * max(1, len(values)))(*values)


def page_address(label):
    """A page's address on the PowerPC side (never followed): later labels lie higher, and
    labels 2 and up lie above 0x80000000, where an unsigned compare matters."""
    return 0 if label == 0 else 0x7ffffc00 + 0x200 * label


# ----------------------------------------------------------------- the emulator's float corrections

LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr_double(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    bits = v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]
    return struct.unpack(">d", struct.pack(">Q", bits))[0]


class HardwareRounding:
    """fmadds as the processor rounds it (once), for CalcLine's code: each such instruction gets a
    code hook that writes fmaf's result to the destination register and steps over the
    instruction. Operands that are not finite are left to the emulator (the result is an
    infinity or a NaN either way)."""

    def __init__(self, e):
        self.e, self.hooks, self.sites = e, [], {}
        words = struct.unpack(">%dI" % (CALC_LINE_SIZE // 4), e.read(CALC_LINE, CALC_LINE_SIZE))
        for k, word in enumerate(words):
            if word >> 26 == 59 and (word >> 1) & 31 == 29:      # fmadds frD, frA, frC, frB
                self.sites[CALC_LINE + 4 * k] = ((word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31,
                                                 (word >> 6) & 31)

    def __enter__(self):
        for address in sorted(self.sites):
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=address, end=address))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        rd, ra, rb, rc = self.sites[address]
        a, c, b = fpr_double(self.e, ra), fpr_double(self.e, rc), fpr_double(self.e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        assert all(struct.unpack(">f", struct.pack(">f", x))[0] == x for x in (a, c, b)), "fmadds operand is not a single"
        value = float(LIBM.fmaf(a, c, b))
        uc.reg_write(ppcemu.FPR[rd], struct.unpack(">Q", struct.pack(">d", value))[0])
        uc.reg_write(P.UC_PPC_REG_PC, address + 4)


# ----------------------------------------------------------------- the PowerPC side: common


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    # The math unit: an object with the vtable pointer at +4; the strided Transform is the slot at
    # vtable +0x10 (a pointer to a function descriptor), called through __ptr_glue.
    vtable = T.persist_alloc(e, 0x40)
    e.hh_math = T.persist_alloc(e, 0x10)
    e.hh_side = None
    e.write(e.hh_math + 4, struct.pack(">I", vtable))
    e.write(vtable + 0x10, struct.pack(">I", T.fake_function(e, lambda em: em.hh_side.transform(em))))


@contextlib.contextmanager
def guarded(e):
    """The engine globals a case overwrites, put back afterwards (other modules share them)."""
    saved = [(a, e.read(a, n)) for a, n in GUARDED]
    try:
        yield
    finally:
        for a, data in saved:
            e.write(a, data)


def run(e, fn, args, real, fakes=None):
    """Calls the original `fn` with the unit's own functions real (other modules leave stand-ins
    on some) and `fakes` (a CL.Stubs) in place."""
    stubs = fakes or CL.Stubs()
    stubs.real = set(real)
    saved = stubs.activate(e)
    try:
        return T.call_ppc(e, fn, args)[0]
    finally:
        CL.Stubs.deactivate(e, saved)


def new_field(e):
    return e.alloc(FIELD_SIZE, b"\xa5" * FIELD_SIZE)


def bind(lib):
    """argtypes of the unit's calls (once per library)."""
    if getattr(lib, "hh_bound", False):
        return
    lib.hh_bound = True
    lib.tpw_hh_pointer.restype = C.c_void_p
    lib.tpw_hh_pointer.argtypes = [C.c_int32]
    lib.tpw_hh_dump_n.restype = C.c_uint32
    lib.tpw_hh_dump_copy.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_hh_gamut.restype = C.c_int32
    lib.tpw_hh_gamut.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int8), C.POINTER(C.c_uint8), C.POINTER(C.c_int8)]
    lib.tpw_hh_process.restype = C.c_int32
    lib.tpw_hh_process.argtypes = [C.POINTER(C.c_uint8), C.c_uint16, C.c_uint16]
    lib.tpw_hh_qsort_textures.argtypes = [C.POINTER(C.c_uint32)] + [C.c_int32] * 4
    lib.tpw_hh_qsort_quads.argtypes = [C.POINTER(C.c_uint32)] + [C.c_int32] * 4
    lib.tpw_hh_qsort_faces.argtypes = [C.POINTER(C.c_uint32), C.POINTER(C.c_uint16)] + [C.c_int32] * 4
    lib.tpw_hh_used_field.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int8), C.c_uint32, C.POINTER(C.c_uint16),
                                      C.c_int32, C.c_int32, C.c_uint32, C.POINTER(C.c_int32), C.POINTER(C.c_int32),
                                      C.c_int32]
    lib.tpw_hh_used_textures.argtypes = [C.POINTER(C.c_int32), C.c_int32]
    lib.tpw_hh_used_object.argtypes = [C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_hh_used_entry.argtypes = [C.c_int32, C.c_uint32, C.c_int32]
    lib.tpw_hh_used_run.restype = C.c_int32
    lib.tpw_hh_used_run.argtypes = [C.c_int32, C.POINTER(C.c_int32), C.c_int32, C.POINTER(C.c_int32), C.c_int32,
                                    C.POINTER(C.c_int32)]
    lib.tpw_hh_grid.argtypes = [C.c_int32, C.c_int32, C.c_uint32, C.POINTER(C.c_uint32), C.POINTER(C.c_uint32),
                                C.c_int32, C.c_int32]
    lib.tpw_hh_lighting.restype = C.c_int32
    lib.tpw_hh_lighting.argtypes = [C.c_int32, C.c_uint32, C.c_int32, C.POINTER(C.c_uint32), C.POINTER(C.c_int32)]
    lib.tpw_hh_line.restype = C.c_int32
    lib.tpw_hh_line.argtypes = [C.POINTER(C.c_uint8), C.c_int32, C.POINTER(C.c_uint32), C.c_int32,
                                C.POINTER(C.c_uint32), C.c_int32, C.c_int32, C.c_int32, C.c_int32]


def host_labels(lib):
    return {LABEL_FIELD: lib.tpw_hh_pointer(0), LABEL_LIGHT: lib.tpw_hh_pointer(1), LABEL_MATH: lib.tpw_hh_pointer(2),
            LABEL_MATRIX: lib.tpw_hh_pointer(3), LABEL_CLIP: lib.tpw_hh_pointer(4)}


def host_dump(lib):
    n = lib.tpw_hh_dump_n()
    dump = (C.c_uint32 * max(n, 1))()
    lib.tpw_hh_dump_copy(dump)
    return list(dump)[:n]


# ----------------------------------------------------------------- CheckGamutArea

def gamut_case(first, last, low, high, area, before=None, r=None):
    """mF128 and mF12C, the 128 low and 128 high columns (signed bytes), the area (mF90's four
    signed bytes) and the 0x100 bytes in front of the row bytes (what a row below 0 reads)."""
    low = (list(low) + [0] * 128)[:128]
    high = (list(high) + [0] * 128)[:128]
    if before is None:
        before = [r.getrandbits(8) for _ in range(0x100)] if r else [0x5a] * 0x100
    return {"first": first, "last": last, "low": low, "high": high, "area": list(area), "before": before}


def ppc_gamut(e, c):
    field = new_field(e)
    e.write(field + 0x28, bytes(c["before"]))
    ws(e, field + 0x128, [c["first"], c["last"]])
    e.write(field + 0x130, bytes(b & 0xff for b in c["low"] + c["high"]))
    area = e.alloc(8, bytes(b & 0xff for b in c["area"]) + b"\xa5" * 4)
    field_before, area_before = e.read(field, FIELD_SIZE), e.read(area, 4)
    r3 = run(e, CHECK_GAMUT, [field, area], {CHECK_GAMUT})
    return {"ret": r3, "object unchanged": int(e.read(field, FIELD_SIZE) == field_before),
            "area unchanged": int(e.read(area, 4) == area_before), "log": []}


def host_gamut(lib, c):
    bind(lib)
    CL.host_begin(lib, {})
    r = lib.tpw_hh_gamut(c["first"], c["last"], arr(C.c_int8, c["low"] + c["high"]), arr(C.c_uint8, c["before"]),
                         arr(C.c_int8, c["area"]))
    return {"ret": r & 1, "object unchanged": r >> 8 & 1, "area unchanged": r >> 9 & 1, "log": CL.host_end(lib)}



def gamut_edges():
    cases = []
    ones = [4] * 128
    # mF128 above the area's last row (answers 0 at once), equal to it, below it
    for first in (5, 4, 3):
        cases.append(gamut_case(first, 10, [0] * 128, ones, (4, 1, 5, 0)))
    # mF12C below the area's first row, equal to it, above it
    for last in (0, 1, 2):
        cases.append(gamut_case(0, last, [0] * 128, ones, (6, 1, 5, 0)))
    # the area's first row above its last (an empty row range)
    cases.append(gamut_case(0, 10, [0] * 128, ones, (2, 5, 5, 0)))
    # one row: a hit, the cells' low column above the area's last column (equal is a hit), the high
    # column below the area's first column (equal is a hit)
    for low, high, area_x1, area_x0 in ((3, 6, 5, 2), (3, 6, 3, 2), (3, 6, 2, 2), (3, 6, 5, 6), (3, 6, 5, 7),
                                        (-5, -2, -3, -4), (-5, -2, -6, -4)):
        cases.append(gamut_case(0, 10, [low] * 128, [high] * 128, (3, 3, area_x1, area_x0)))
    # several rows: the hit on the first, a middle and the last row of the area, and no hit
    for hit in (1, 2, 3, 4, None):
        low = [9] * 128
        high = [0] * 128
        if hit is not None:
            low[hit], high[hit] = 0, 9
        cases.append(gamut_case(0, 10, low, high, (4, 1, 5, 3)))
    # rows past the ones in view are still looked at
    low = [9] * 128
    high = [0] * 128
    low[20], high[20] = 0, 9
    cases.append(gamut_case(0, 3, low, high, (20, 18, 5, 3)))
    cases.append(gamut_case(5, 8, low, high, (127, 19, 5, 3)))
    # extreme bytes in the area and the rows
    cases.append(gamut_case(0, 127, [-128] * 128, [127] * 128, (127, 0, 127, -128)))
    cases.append(gamut_case(0, 127, [127] * 128, [-128] * 128, (127, 0, -128, 127)))
    cases.append(gamut_case(0, 127, [-128] * 128, [-128] * 128, (127, 0, -128, -128)))
    # the row counts are signed 32-bit words
    cases.append(gamut_case(-0x80000000, 0x7fffffff, [0] * 128, [5] * 128, (3, 1, 5, 0)))
    cases.append(gamut_case(0x7fffffff, 0x7fffffff, [0] * 128, [5] * 128, (3, 1, 5, 0)))
    cases.append(gamut_case(-0x80000000, -0x80000000, [0] * 128, [5] * 128, (3, 1, 5, 0)))
    cases.append(gamut_case(-0x80000000, 5, [0] * 128, [5] * 128, (3, 0, 5, 0)))
    # rows below 0 read the bytes in front of the row bytes: the whole area from -128, and a few more
    r = random.Random(7)
    for first, last in ((-1, -1), (0, 0), (-1, 0), (0x7f7f7f7f, 0x7f7f7f7f), (-1, 0x7f7f7f7f), (0, -1)):
        low = [r.randint(-128, 127) for _ in range(128)]
        high = [r.randint(-128, 127) for _ in range(128)]
        for area in ((127, -128, 127, -128), (5, -9, 100, -100), (-1, -128, 0, 0), (-8, -9, 127, -128), (3, -3, 0, 0)):
            if first > area[0] or last < area[1]:
                continue
            cases.append(gamut_case(first, last, low, high, area, r=r))
    return cases


def gamut_random(r):
    """Rows below 0 read the bytes in front of the row arrays: the last bytes of mF128 and mF12C
    first. The Mac keeps those words big-endian and the host little-endian, so a case may name a
    row below 0 only where both counts have four alike bytes (-1, 0, 0x7f7f7f7f); the other
    cases keep to rows from 0 up."""
    kind = r.random()
    if kind < 0.6:
        first = r.randint(-1, 40)
        last = first + r.randint(-3, 40)
        low = [r.randint(-4, 30) for _ in range(128)]
        high = [low[i] + r.randint(-5, 12) for i in range(128)]
        top = r.randint(first - 3, last + 30)
        bottom = max(first - 6, 0)
        area = (top, r.randint(bottom, max(bottom, top + 2)), r.randint(-4, 35), r.randint(-4, 35))
    elif kind < 0.8:
        first = r.choice([-1, 0, 0, -1, 0x7f7f7f7f])
        last = r.choice([-1, 0, 0x7f7f7f7f, 0x7f7f7f7f])
        low = [r.randint(-128, 127) for _ in range(128)]
        high = [r.randint(-128, 127) for _ in range(128)]
        top = r.randint(max(first, -128) if first <= 127 else 0, 127)
        bottom = r.randint(-128, min(top, last) if r.random() < 0.9 else 127)
        area = (top, bottom, r.randint(-128, 127), r.randint(-128, 127))
    else:
        first, last = T.rint(r), T.rint(r)
        low = [r.randint(-128, 127) for _ in range(128)]
        high = [r.randint(-128, 127) for _ in range(128)]
        area = (r.randint(0, 127), r.randint(0, 127), r.randint(-128, 127), r.randint(-128, 127))
    area = tuple(max(-128, min(127, v)) for v in area)
    low = [max(-128, min(127, v)) for v in low]
    high = [max(-128, min(127, v)) for v in high]
    return gamut_case(first, last, low, high, area, r=r)


# ----------------------------------------------------------------- ProcessTextureParameters

def ppc_process(e, c):
    corners = e.alloc(8, bytes(c["corners"]) + b"\xa5" * 4)   # the four corners, then poison
    quad = e.alloc(8, struct.pack(">HH", c["flags"], c["second"]) + b"\xa5" * 4)
    quad_before = e.read(quad, 4)
    field = new_field(e)
    run(e, PROCESS, [field, corners, quad], {PROCESS})
    return {"corners": list(e.read(corners, 8)), "quad unchanged": int(e.read(quad, 4) == quad_before), "log": []}


def host_process(lib, c):
    bind(lib)
    CL.host_begin(lib, {})
    corners = arr(C.c_uint8, c["corners"] + [0xa5] * 4)
    same = lib.tpw_hh_process(corners, c["flags"], c["second"])
    return {"corners": list(corners)[:8], "quad unchanged": same, "log": CL.host_end(lib)}


def process_edges():
    cases = []
    for bits in range(16):
        flags = (bits & 1) * 0x08 + (bits >> 1 & 1) * 0x10 + (bits >> 2 & 1) * 0x20 + (bits >> 3 & 1) * 0x40
        for other in (0x0000, 0x0087, 0xff87, 0x8000, 0x0001):
            for corners in ((0, 1, 2, 3), (3, 2, 1, 0), (0x11, 0x22, 0x33, 0x44), (7, 7, 9, 9)):
                cases.append({"flags": flags | other, "second": 0x1234 + bits, "corners": list(corners)})
    return cases


def process_random(r):
    return {"flags": r.getrandbits(16), "second": r.getrandbits(16), "corners": [r.getrandbits(8) for _ in range(4)]}


# ----------------------------------------------------------------- the sorts

# keys that test the compare: zero, high bit set, the sign boundary, the top
PAGE_VALUES = [0, 0x1000, 0x7ffffe00, 0x7fffffff, 0x80000000, 0x80000001, 0xa5a5a5a5, 0xfffffffe, 0xffffffff]


def sort_case(r, items, pad=2, rng=None):
    """items: the words (or tuples of words) to sort; the sort works on the part after `pad`
    poison items. rng: (low, high) or None for the whole part."""
    n = len(items)
    if rng is None:
        rng = (0, n - 1)
    return {"items": list(items), "pad": pad, "low": rng[0], "high": rng[1]}


def random_range(r, n):
    k = r.random()
    if n == 0:
        low = r.randint(0, 3)
        return (0, -1) if k < 0.5 else (low, low - r.randint(1, 3))   # always empty: nothing to sort
    if k < 0.65:
        return (0, n - 1)
    if k < 0.85:
        low = r.randint(0, n - 1)
        return (low, r.randint(low, n - 1))
    if k < 0.93:
        return (r.randint(0, n), r.randint(-1, n - 1))   # often empty (low above high)
    return (0, -1)


def sorts_edges(make):
    """Edge inputs for the three sorts; make(words) gives the sort's item for a page word."""
    cases = []
    arrays = [[], [5], [5, 5], [3, 9], [9, 3], [1, 2, 3, 4, 5, 6], [6, 5, 4, 3, 2, 1], [4, 4, 4, 4, 4], [5, 3, 9, 9],
              [9, 9, 3, 5], [2, 1, 2, 1, 2, 1, 2], [0, 0xffffffff, 0x80000000, 0x7fffffff, 1],
              [0x80000001, 0x7fffffff, 0x80000000, 0x7ffffffe], [3, 1, 2, 3, 1, 2, 3, 1, 2], [0, 0, 1, 0, 0],
              [7, 7, 7, 1, 7, 7, 7, 9, 7], [10, 20, 30, 40, 50, 25, 35], [50, 40, 30, 20, 10, 25, 15, 35]]
    for words in arrays:
        n = len(words)
        cases.append(sort_case(None, [make(w, i) for i, w in enumerate(words)]))
        for rng in ((0, n - 2), (1, n - 1), (1, n - 2), (n, n - 1), (n - 1, n - 1), (0, -1), (2, 0)):
            if n and (rng[0] < 0 or rng[1] >= n or rng[0] > n):
                continue
            cases.append(sort_case(None, [make(w, i) for i, w in enumerate(words)], rng=rng))
    return cases


def poison_words(count):
    return [POISON] * count


# QSort_textures: pages as words
def textures_item(word, index):
    return word


def ppc_sort_textures(e, c):
    pad, items = c["pad"], c["items"]
    words = poison_words(pad) + items + poison_words(2)
    base = e.alloc(4 * len(words))
    ws(e, base, words)
    field = new_field(e)
    run(e, QSORT_TEXTURES, [field, base + 4 * pad, c["low"], c["high"]], {QSORT_TEXTURES})
    return {"words": rs(e, base, len(words)), "log": []}


def host_sort_textures(lib, c):
    bind(lib)
    CL.host_begin(lib, {})
    pad, items = c["pad"], c["items"]
    words = arr(C.c_uint32, poison_words(pad) + items + poison_words(2))
    lib.tpw_hh_qsort_textures(words, pad + len(items) + 2, pad, c["low"], c["high"])
    return {"words": list(words)[:pad + len(items) + 2], "log": CL.host_end(lib)}


def sort_textures_random(r):
    n = r.choice([0, 1, 2, 3, 4, 5, 6, 8, 10, 13, 17, 24, 33, 40])
    alphabet = r.choice([2, 3, 5, 9, 30])
    values = PAGE_VALUES + [r.getrandbits(32) for _ in range(30)]
    values = values[:alphabet] if alphabet < 9 else values[:alphabet]
    items = [r.choice(values) for _ in range(n)]
    return sort_case(r, items, rng=random_range(r, n))


# qSortTextures_lpTex: a record is four words, the page first and three words that tell the records apart
def quads_item(word, index):
    return (word, index, (index * 0x01010101) & MASK, ((index & 0x7fff) << 16) | ((index * 3) & 0xffff))


def ppc_sort_quads(e, c):
    pad, items = c["pad"], c["items"]
    rec = [(POISON,) * 4] * pad + items + [(POISON,) * 4] * 2
    flat = [w for q in rec for w in q]
    base = e.alloc(4 * len(flat))
    ws(e, base, flat)
    field = new_field(e)
    run(e, QSORT_QUADS, [field, base + 16 * pad, c["low"], c["high"]], {QSORT_QUADS})
    return {"words": rs(e, base, len(flat)), "log": []}


def host_sort_quads(lib, c):
    bind(lib)
    CL.host_begin(lib, {})
    pad, items = c["pad"], c["items"]
    flat = [w for q in [(POISON,) * 4] * pad + items + [(POISON,) * 4] * 2 for w in q]
    words = arr(C.c_uint32, flat)
    lib.tpw_hh_qsort_quads(words, pad + len(items) + 2, pad, c["low"], c["high"])
    return {"words": list(words)[:len(flat)], "log": CL.host_end(lib)}


def sort_quads_random(r):
    n = r.choice([0, 1, 2, 3, 4, 5, 6, 8, 10, 13, 17, 24, 33])
    values = PAGE_VALUES + [r.getrandbits(32) for _ in range(30)]
    values = values[:r.choice([2, 3, 5, 9, 30])]
    items = [quads_item(r.choice(values), i) for i in range(n)]
    return sort_case(r, items, rng=random_range(r, n))


# QSortFaces_float: a key is a float's bits, a face three halfwords that tell the faces apart
KEY_VALUES = [0x00000000, 0x80000000, 0x3f800000, 0xbf800000, 0x40200000, 0x42c80000, 0xc2c80000, 0x7f800000,
              0xff800000, 0x7fc00000, 0xffc00000, 0x00000001, 0x80000001, 0x7f7fffff, 0x3f800001, 0x7fc00123]


def face_item(key, index):
    return (key, ((index * 7 + 1) & 0xffff, (index * 13 + 2) & 0xffff, 0xf000 + index))


def ppc_sort_faces(e, c):
    pad, items = c["pad"], c["items"]
    keys = [POISON] * pad + [k for k, _ in items] + [POISON] * 2
    faces = [0xa5a5] * (3 * pad) + [h for _, f in items for h in f] + [0xa5a5] * 6
    kbase, fbase = e.alloc(4 * len(keys)), e.alloc(2 * len(faces))
    ws(e, kbase, keys)
    e.write(fbase, struct.pack(">%dH" % len(faces), *faces))
    run(e, QSORT_FACES, [kbase + 4 * pad, fbase + 6 * pad, c["low"], c["high"]], {QSORT_FACES})
    return {"keys": [canon(k) for k in rs(e, kbase, len(keys))],
            "faces": list(struct.unpack(">%dH" % len(faces), e.read(fbase, 2 * len(faces)))), "log": []}


def host_sort_faces(lib, c):
    bind(lib)
    CL.host_begin(lib, {})
    pad, items = c["pad"], c["items"]
    keys = arr(C.c_uint32, [POISON] * pad + [k for k, _ in items] + [POISON] * 2)
    faces = arr(C.c_uint16, [0xa5a5] * (3 * pad) + [h for _, f in items for h in f] + [0xa5a5] * 6)
    lib.tpw_hh_qsort_faces(keys, faces, pad + len(items) + 2, pad, c["low"], c["high"])
    return {"keys": [canon(k) for k in list(keys)[:pad + len(items) + 2]],
            "faces": list(faces)[:3 * (pad + len(items) + 2)], "log": CL.host_end(lib)}


def sort_faces_random(r):
    n = r.choice([0, 1, 2, 3, 4, 5, 6, 8, 10, 13, 17, 24, 33])
    values = KEY_VALUES[:r.choice([2, 3, 4, 6, 10, 16])] + [fb(r.uniform(-100, 100)) for _ in range(20)]
    values = r.sample(values, min(len(values), r.choice([2, 3, 5, 9, 30])))
    items = [face_item(r.choice(values), i) for i in range(n)]
    return sort_case(r, items, rng=random_range(r, n))


def faces_edges():
    cases = []
    for words in ([], [0x3f800000], [0x3f800000, 0x40200000], [0x40200000, 0x3f800000], [0x3f800000] * 5,
                  [0x7fc00000, 0x3f800000, 0x40200000], [0x3f800000, 0x7fc00000, 0x40200000, 0x42c80000],
                  [0x00000000, 0x80000000, 0x00000000, 0x80000000], [0x7f800000, 0xff800000, 0x3f800000, 0x7f800000],
                  [0x40200000, 0x7fc00000, 0x7fc00000, 0x3f800000, 0x7fc00000],
                  [0x00000001, 0x80000001, 0x00000000, 0x80000000, 0x00000002],
                  [0x42c80000, 0x41200000, 0x41f00000, 0x40a00000, 0x42480000, 0x41a00000],
                  [0x40a00000, 0x41200000, 0x41a00000, 0x41f00000, 0x42480000, 0x42c80000]):
        n = len(words)
        cases.append(sort_case(None, [face_item(w, i) for i, w in enumerate(words)]))
        for rng in ((0, n - 2), (1, n - 1), (1, n - 2), (0, -1), (n, n - 1)):
            if n and (rng[0] < 0 or rng[1] >= n or rng[0] > n):
                continue
            cases.append(sort_case(None, [face_item(w, i) for i, w in enumerate(words)], rng=rng))
    return cases


# ----------------------------------------------------------------- the fakes of the PowerPC side

def mix_lighting(v, n, i, calls):
    return (v[0] * 3 + v[1] * 5 + v[2] * 7 + n[0] * 11 + n[1] * 13 + n[2] * 17 + i * 19 + calls * 23) & MASK


def mix_transform(w, i):
    k = i * 16
    return [(w[0] * 3 + 1 + k) & MASK, (w[1] * 5 + 2 + k) & MASK, (w[2] * 7 + 3 + k) & MASK,
            (w[3] * 9 + w[0] + w[1] + w[2] + 4) & MASK]


def mix_code(w, mask):
    return ((((w[0] + w[1] * 3 + w[2] * 5 + w[3] * 7) & MASK) >> 5) & 0xff) & mask


class Fakes:
    """CalcIntensities, the math unit's strided Transform and CClipping::CPP_TestData on the
    PowerPC side: the same events, dumps and results as unit_HFieldHelpers.cpp."""

    def __init__(self, e, fixed):
        self.e, self.L, self.dump, self.calls = e, CL.PpcLog(fixed), [], 0
        self.colours_base = self.vbase = self.cbase = 0

    def stubs(self):
        st = CL.Stubs()
        st.internal(CALC_INTENSITIES, self.calc_intensities)
        st.internal(CPP_TEST_DATA, self.test_data)
        st.imp("__assertion_failed",
               lambda e: self.L.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))
        return st

    def calc_intensities(self, e):
        L = self.L
        this, colours, normals, vertices, count = e.r(3), e.r(4), e.r(5), e.r(6), T.s32(e.r(7))
        L.rec("CalcIntensities", (L.lab(this), (colours - self.colours_base) // 4, count))
        self.calls += 1
        for i in range(count):
            v = [canon(w) for w in rs(e, vertices + 16 * i, 3)]
            n = [canon(w) for w in rs(e, normals + 12 * i, 3)]
            self.dump.extend(v + n)
            ws(e, colours + 4 * i, [mix_lighting(v, n, i, self.calls)])

    def transform(self, e):
        L = self.L
        this, matrix, out, source = e.r(3), e.r(4), e.r(5), e.r(6)
        out_stride, in_stride, count, kind = T.s32(e.r(7)), T.s32(e.r(8)), T.s32(e.r(9)), T.s32(e.r(10))
        L.rec("Transform", (L.lab(this), L.lab(matrix), out - self.vbase, source - self.vbase, out_stride, in_stride,
                            count, kind))
        for i in range(count):
            w = [canon(x) for x in rs(e, source + i * in_stride, 4)]
            ws(e, out + i * out_stride, mix_transform(w, i))

    def test_data(self, e):
        L = self.L
        clipping, codes, vertices, count, stride, mask = e.r(4), e.r(5), e.r(6), T.s32(e.r(7)), T.s32(e.r(8)), e.r(9)
        L.rec("CPP_TestData", (L.lab(clipping), codes - self.cbase, vertices - self.vbase, count, stride, mask))
        for i in range(count):
            w = [canon(x) for x in rs(e, vertices + i * stride, 4)]
            self.dump.extend(w)
            e.write(codes + i, bytes([mix_code(w, mask)]))


# ----------------------------------------------------------------- GetTexturesUsed

def used_case(rows=(), first=0, cells=10, quads=None, before=0, settings=0, rect=(0, 0, 0, 0), proc=(),
              textures=None, entries=None, count_before=77, extra_low=None):
    """rows: (low, high) of each row from `first` on (the others are 0, 0). quads: (flags, texture
    number) of each cell in memory (default all (0, 0)). textures: for each texture record None or
    (first entry, second entry). entries: label -> (flags, page label)."""
    low, high = [0] * 128, [0] * 128
    for k, (lo, hi) in enumerate(rows):
        low[first + k], high[first + k] = lo, hi
    if textures is None:
        textures = [(1, 2)]
    if entries is None:
        entries = {1: (0x1000, 1), 2: (0x1000, 2)}
    if quads is None:
        quads = [(0, 0)] * (cells * (first + len(rows)) + 4)
    return {"first": first, "end": first + len(rows), "low": low, "high": high, "cells": cells, "quads": list(quads),
            "before": before, "settings": settings, "rect": list(rect), "proc": list(proc),
            "textures": list(textures), "entries": dict(entries), "count_before": count_before}


def cells_in_view(c):
    return [(z, x) for z in range(c["first"], c["end"]) for x in range(c["low"][z], c["high"][z])]


def finish_used(c):
    """Slots for the page and entry lists: the cells in view and then some."""
    c["page_slots"] = 16
    c["entry_slots"] = len(cells_in_view(c)) + 6
    return c


def sequence_case(labels, **kw):
    """One row of cells whose pages are `labels` (1..8) in that order, each through its own texture."""
    entries = {k: (0x1000, k) for k in range(1, N_PAGES + 1)}
    textures = [(k, 0) for k in range(1, N_PAGES + 1)]
    quads = [(0, k - 1) for k in labels] + [(0, 0)] * 4
    return finish_used(used_case(rows=[(0, len(labels))], cells=len(labels) + 1, quads=quads, entries=entries,
                                 textures=textures, **kw))


def ppc_used(e, c):
    with guarded(e):
        field = new_field(e)
        entry_at, label_of = {0: 0}, {0: 0}
        for label, (flags, page) in c["entries"].items():
            a = e.alloc(ENTRY_SIZE, b"\xa5" * ENTRY_SIZE)
            ws(e, a + 8, [flags])
            ws(e, a + 0x18, [page_address(page)])
            entry_at[label], label_of[a] = a, label
        page_label = {page_address(k): k for k in range(N_PAGES + 1)}
        records = e.alloc(8 * len(c["textures"]) + 8)
        for i, t in enumerate(c["textures"]):
            if t is None:
                continue
            entries = e.alloc(8)
            ws(e, entries, [entry_at[t[0]], entry_at.get(t[1], 0)])
            texture = e.alloc(0x10, b"\xa5" * 0x10)
            ws(e, texture + 8, [entries])
            ws(e, records + 8 * i + 4, [texture])
        record = e.alloc(RECORD_SIZE, b"\xa5" * RECORD_SIZE)
        quads = e.alloc(4 * len(c["quads"]) + 4)
        e.write(quads, b"".join(struct.pack(">HH", a, b) for a, b in c["quads"]))
        ws(e, record + 0x18, [c["cells"]])
        ws(e, record + 0x2c, [quads + 4 * c["before"]])
        ws(e, field, [record, records])
        ws(e, field + 0x128, [c["first"], c["end"]])
        e.write(field + 0x130, bytes(b & 0xff for b in c["low"] + c["high"]))
        ws(e, field + 0x234, c["rect"])
        ws(e, field + 0x248, [entry_at[label] for label in c["proc"]] + [0] * (2500 - len(c["proc"])))
        ws(e, SETTINGS, [c["settings"]])
        count = e.alloc(4)
        ws(e, count, [c["count_before"]])
        pages = e.alloc(4 * c["page_slots"])
        ws(e, pages, [POISON] * c["page_slots"])
        entries_out = e.alloc(4 * c["entry_slots"])
        ws(e, entries_out, [POISON] * c["entry_slots"])
        field_before = e.read(field, FIELD_SIZE)
        run(e, USED, [field, pages, count, entries_out], {USED, QSORT_TEXTURES, GET_TEXTURE_ENTRY})
        return {"count": T.s32(e.u32(count)),
                "pages": [page_label.get(w, -1) for w in rs(e, pages, c["page_slots"])],
                "entries": [label_of.get(w, -1) for w in rs(e, entries_out, c["entry_slots"])],
                "rest unchanged": int(e.read(field, FIELD_SIZE) == field_before), "log": []}


def host_used(lib, c):
    bind(lib)
    quads = [v for q in c["quads"] for v in q]
    lib.tpw_hh_used_field(c["first"], c["end"], arr(C.c_int8, c["low"] + c["high"]), c["cells"],
                          arr(C.c_uint16, quads), len(c["quads"]), c["before"], c["settings"], arr(C.c_int32, c["rect"]),
                          arr(C.c_int32, c["proc"]), len(c["proc"]))
    lib.tpw_hh_used_textures(arr(C.c_int32, [0 if t is None else 1 for t in c["textures"]]), len(c["textures"]))
    for i, t in enumerate(c["textures"]):
        if t is not None:
            lib.tpw_hh_used_object(i, t[0], t[1])
    for label, (flags, page) in c["entries"].items():
        lib.tpw_hh_used_entry(label, flags, page)
    CL.host_begin(lib, {})
    pages = (C.c_int32 * c["page_slots"])()
    entries = (C.c_int32 * c["entry_slots"])()
    rest = C.c_int32()
    count = lib.tpw_hh_used_run(c["count_before"], pages, c["page_slots"], entries, c["entry_slots"], C.byref(rest))
    return {"count": count, "pages": list(pages), "entries": list(entries), "rest unchanged": rest.value,
            "log": CL.host_end(lib)}


def used_edges():
    cases = []
    # no cells in view: the rows run the wrong way, are equal, or have no columns
    cases.append(finish_used(used_case(rows=[])))
    cases.append(finish_used(used_case(rows=[(2, 2), (3, 3)])))
    cases.append(finish_used(used_case(rows=[(4, 1), (6, 2)])))
    c = used_case(rows=[(0, 3)], first=3)
    c["end"] = 2
    cases.append(finish_used(c))
    # every quad flagged 1 (other flag bits do not matter): nothing is listed
    cases.append(finish_used(used_case(rows=[(0, 3), (1, 4)], quads=[(1, 0)] * 30)))
    cases.append(finish_used(used_case(rows=[(0, 3), (1, 4)], quads=[(0xffff, 0)] * 30)))
    cases.append(finish_used(used_case(rows=[(0, 3), (1, 4)], quads=[(0xfffe, 0)] * 30)))
    # one cell, every way its entry can come out
    for textures, entries in (([None], {}), ([(0, 0)], {}), ([(1, 2)], {1: (0x1000, 3)}), ([(1, 2)], {1: (0x0000, 3)}),
                              ([(1, 2)], {1: (0x1000, 0)}), ([(1, 2)], {1: (0xffffefff, 3)}),
                              ([(1, 2)], {1: (0xffffffff, 8)})):
        cases.append(finish_used(used_case(rows=[(0, 1)], textures=textures, entries=entries, quads=[(0, 0)] * 8)))
    # pages in many orders (the sort, with its quirk, and the list of different pages)
    for labels in ([1], [2, 2, 2], [1, 2, 3], [3, 2, 1], [5, 3, 8, 8], [8, 8, 3, 5], [2, 1, 2, 1, 2], [4, 1, 3, 1, 5, 2, 6, 5, 3, 5],
                   [8, 7, 6, 5, 4, 3, 2, 1], [1, 2, 3, 4, 5, 6, 7, 8], [3, 3, 1, 3, 3, 3, 2, 3], [5, 3, 7, 7], [7, 7, 3, 5],
                   [2, 8, 1, 7, 3, 6, 4, 5, 8, 2]):
        cases.append(sequence_case(labels))
    # the first and last row in view, several rows, a row with the cells in view and the next without
    entries = {k: (0x1000, k) for k in range(1, N_PAGES + 1)}
    textures = [(k, 0) for k in range(1, N_PAGES + 1)]
    quads = [(0 if (i * 7) % 5 else 1, i % 8) for i in range(130)]
    cases.append(finish_used(used_case(rows=[(1, 4), (0, 0), (2, 6), (3, 5)], first=2, cells=9, quads=quads, before=2,
                                       entries=entries, textures=textures)))
    cases.append(finish_used(used_case(rows=[(0, 9)], first=127, cells=10, quads=[(0, i % 8) for i in range(1300)],
                                       entries=entries, textures=textures)))
    # the procedural rectangle: setting 4 on or off, each bound crossed by one cell, flagged cells inside it
    for settings in (0, 4, 0xfffffffb, 0xffffffff, 0x10000004):
        for rect in ((0, 0, 7, 7), (2, 1, 4, 2), (3, 3, 3, 3), (0, 0, 7, 1), (5, 0, 7, 7), (0, 0, 2, 7), (0, 2, 7, 7), (4, 1, 2, 2),
                     (1, 1, 1, 2), (3, 1, 4, 1), (-5, -5, 0, 0)):
            quads = [(1 if (i * 5) % 7 == 0 else 0, i % 8) for i in range(130)]
            c = used_case(rows=[(1, 6), (0, 5), (2, 7), (1, 4)], first=0, cells=11, quads=quads, entries=entries,
                          textures=textures, settings=settings, rect=rect)
            used = 0
            for z, x in cells_in_view(c):
                if c["quads"][x + z * 11 + 0][0] & 1 == 0 and settings & 4 and rect[0] <= x <= rect[2] and rect[1] <= z <= rect[3]:
                    used += 1
            c["proc"] = [(i * 3 + 1) % 9 if i % 5 else 0 for i in range(used + 1)]
            cases.append(finish_used(c))
    return cases


def used_random(r):
    first = r.randint(0, 6)
    span = r.randint(-1, 5)
    ntex = r.randint(1, 5)
    n_entries = r.randint(2, 12)
    entries = {}
    for label in range(1, n_entries + 1):
        flags = r.getrandbits(32) & ~0x1000 | (0x1000 if r.random() < 0.65 else 0)
        entries[label] = (flags, r.randint(0, N_PAGES) if r.random() < 0.9 else 0)
    pick = lambda: r.randint(0, n_entries) if r.random() < 0.85 else 0
    textures = [None if r.random() < 0.2 else (pick(), pick()) for _ in range(ntex)]
    cells = r.randint(13, 20)
    rows = []
    for k in range(max(span, 0)):
        lo = r.randint(0, 6)
        rows.append((lo, lo + r.randint(-1, 6)))
    quads = [(r.getrandbits(16) & ~1 | (1 if r.random() < 0.25 else 0), r.randint(0, ntex - 1)) for _ in range(cells * 13 + 6)]
    x0, z0 = r.randint(-1, 6), r.randint(-1, 6)
    rect = (x0, z0, x0 + r.randint(-1, 6), z0 + r.randint(-1, 6))
    settings = r.getrandbits(32) & ~4 | (4 if r.random() < 0.65 else 0)
    c = used_case(rows=rows, first=first, cells=cells, quads=quads, before=r.randint(0, 3), settings=settings, rect=rect,
                  textures=textures, entries=entries, count_before=r.choice([0, 5, 77, -1, 0x7fffffff]))
    c["end"] = first + span
    used = sum(1 for z, x in cells_in_view(c) if c["quads"][x + z * cells + c["before"]][0] & 1 == 0 and settings & 4
               and rect[0] <= x <= rect[2] and rect[1] <= z <= rect[3])
    c["proc"] = [pick() for _ in range(used + r.randint(0, 2))]
    return finish_used(c)


# ----------------------------------------------------------------- the grid: CalcFullLighting and CalcLine

SPECIAL_BITS = [0x00000000, 0x80000000, fb(1.0), fb(-1.0), 0x7f800000, 0xff800000, 0x7fc00000, 0x00000001, 0x80000001,
                0x7f7fffff, fb(1e-30), fb(1e30), fb(0.5), fb(-0.5)]


def rbits(r, lo, hi, special=0.03):
    """A random float's bits in [lo, hi], or now and then an edge value."""
    if r.random() < special:
        return r.choice(SPECIAL_BITS)
    return fb(r.uniform(lo, hi))


def grid_case(r, width, height, frames=None, before=130, after=None, rows_wanted=None, moderate=True, special=0.03):
    """The record's four floats (origin x and z, cell size x and z), the inverse height range and the
    heights with `before` floats in front of the grid (the last row of CalcFullLighting reads 128
    back) and `after` behind it."""
    after = width + 4 if after is None else after
    count = before + max(width, 0) * max(height, 0) + after
    heights = [rbits(r, -50, 50, special) if moderate else r.getrandbits(32) for _ in range(count)]
    return {"w": width, "h": height, "inv": rbits(r, 0.001, 0.5, special), "record": [rbits(r, -300, 300, special),
            rbits(r, -300, 300, special), rbits(r, 0.5, 30, special), rbits(r, 0.5, 30, special)], "heights": heights,
            "before": before}


def new_grid(e, c):
    """The field with its record and heights in emulator memory; returns (field, record)."""
    field = new_field(e)
    record = e.alloc(RECORD_SIZE, b"\xa5" * RECORD_SIZE)
    heights = e.alloc(4 * len(c["heights"]))
    ws(e, heights, c["heights"])
    ws(e, record + 0x08, c["record"])
    ws(e, record + 0x28, [heights + 4 * c["before"]])
    ws(e, field, [record])
    ws(e, field + 0x08, [c["w"], c["h"]])
    ws(e, field + 0x18, [c["inv"]])
    return field, record


def host_grid(lib, c):
    lib.tpw_hh_grid(c["w"], c["h"], c["inv"], arr(C.c_uint32, c["record"]), arr(C.c_uint32, c["heights"]),
                    len(c["heights"]), c["before"])


# CalcFullLighting: the system's light frame and flags, and the object's mark. Equal and no
# flag 0x40000000: nothing happens.

def lighting_case(r, width, height, work=None, **kw):
    c = grid_case(r, width, height, **kw)
    frame = r.choice([0, 1, 5, 77, -1, 0x7fffffff])
    flags = r.getrandbits(32) & ~0x40000000
    if work is None:
        work = r.random() < 0.8
    if work:
        kind = r.randrange(3)
        if kind == 0:
            mark = frame ^ r.choice([1, 2, 0x100, -0x80000000])
        elif kind == 1:
            mark = frame
            flags |= 0x40000000
        else:
            mark = r.choice([-1, 0, 5]) if r.random() < 0.5 else frame
            if mark == frame:
                flags |= 0x40000000
    else:
        mark = frame
    c.update({"frame": frame, "flags": flags, "mark": mark})
    return c


def ppc_lighting(e, c):
    with guarded(e):
        field, record = new_grid(e, c)
        light = e.alloc(8, b"\xa5" * 8)
        ws(e, field + MARK_AT, [c["mark"]])
        ws(e, SYSTEM, [c["flags"], c["frame"]])
        ws(e, LIGHT_VAR, [light])
        side = Fakes(e, {field: LABEL_FIELD, light: LABEL_LIGHT})
        side.colours_base = field + COLOURS_AT
        before = e.read(field, FIELD_SIZE)
        run(e, LIGHTING, [field], {LIGHTING}, side.stubs())
        after = bytearray(e.read(field, FIELD_SIZE))
        colours = e.read(field + COLOURS_AT, 0x10000)
        after[COLOURS_AT:COLOURS_AT + 0x10000] = before[COLOURS_AT:COLOURS_AT + 0x10000]
        after[MARK_AT:MARK_AT + 4] = before[MARK_AT:MARK_AT + 4]
        return {"colours": list(struct.unpack(">16384I", colours)), "mark": T.s32(e.u32(field + MARK_AT)),
                "rest unchanged": int(bytes(after) == before), "log": side.L.log, "dump": side.dump}


def host_lighting(lib, c):
    bind(lib)
    host_grid(lib, c)
    CL.host_begin(lib, {LABEL_FIELD: lib.tpw_hh_pointer(0), LABEL_LIGHT: lib.tpw_hh_pointer(1)})
    colours = (C.c_uint32 * 16384)()
    mark = C.c_int32()
    rest = lib.tpw_hh_lighting(c["frame"], c["flags"], c["mark"], colours, C.byref(mark))
    return {"colours": list(colours), "mark": mark.value, "rest unchanged": rest, "log": CL.host_end(lib),
            "dump": host_dump(lib)}


def lighting_edges():
    r = random.Random(11)
    cases = []
    for w, h in ((1, 1), (1, 2), (2, 1), (2, 2), (3, 3), (4, 3), (3, 4), (5, 5), (1, 5), (5, 1), (0, 0), (0, 3), (3, 0),
                 (9, 2), (2, 9), (7, 7), (16, 4), (128, 3), (3, 128), (127, 2), (128, 1), (1, 128)):
        cases.append(lighting_case(r, w, h, work=True))
    # the same sizes with nothing to do, and a flag that forces the work with the frame equal
    for w, h in ((3, 3), (1, 1), (128, 128)):
        cases.append(lighting_case(r, w, h, work=False))
    for frame, flags, mark in ((5, 0, 5), (5, 0x40000000, 5), (5, 0xffffffff, 5), (5, 0xbfffffff, 5), (5, 0, 6), (5, 0, -5),
                               (-0x80000000, 0, 0x7fffffff), (-1, 0x40000000, -1)):
        c = lighting_case(r, 4, 4, work=True)
        c.update({"frame": frame, "flags": flags, "mark": mark})
        cases.append(c)
    # the biggest grid, special values, zero and infinite ranges
    cases.append(lighting_case(r, 128, 128, work=True, moderate=True))
    for inv in (0x00000000, 0x7f800000, 0x7fc00000, 0x80000000, fb(1.0)):
        c = lighting_case(r, 5, 4, work=True)
        c["inv"] = inv
        cases.append(c)
    for special in (0.3, 1.0):
        cases.append(lighting_case(r, 6, 5, work=True, special=special))
    return cases


def lighting_random(r):
    width = r.choice([1, 2, 3, 4, 5, 6, 8, 11, 17, 33])
    height = r.choice([1, 2, 3, 4, 5, 6, 8, 11, 17])
    return lighting_case(r, width, height, moderate=r.random() < 0.9)


# CalcLine: the grid width (only used to place the row), the heights, the row and the columns.
# The arrays hold 128 items with `pad` items in front of and behind them.

def line_case(r, width, row, low, high, pad=8, special=0.03):
    need = width * row + max(high, low) + 1
    c = grid_case(r, width, 1, before=pad + 4, after=pad + 4, special=special)
    rows_needed = max(need, 0) + 2 * pad + 4
    c["heights"] = [rbits(r, -50, 50, special) for _ in range(c["before"] + rows_needed)]
    c.update({"row": row, "low": low, "high": high, "pad": pad, "no_math": False,
              "vertices": [POISON] * (4 * (128 + 2 * pad)), "codes": [0xa5] * (128 + 2 * pad), "spare": [POISON] * 128})
    return c


def ppc_line(e, c):
    with guarded(e):
        field, record = new_grid(e, c)
        pad = c["pad"]
        codes = e.alloc(len(c["codes"]), bytes(c["codes"]))
        vertices = e.alloc(4 * len(c["vertices"]))
        ws(e, vertices, c["vertices"])
        spare = e.alloc(4 * 128)
        ws(e, spare, c["spare"])
        matrix, clip = e.alloc(0x40, b"\xa5" * 0x40), e.alloc(8, b"\xa5" * 8)
        ws(e, MATH_VAR, [0 if c["no_math"] else e.hh_math])
        ws(e, FINAL_VAR, [matrix])
        ws(e, CLIP_VAR, [clip])
        side = Fakes(e, {field: LABEL_FIELD, e.hh_math: LABEL_MATH, matrix: LABEL_MATRIX, clip: LABEL_CLIP})
        side.vbase, side.cbase = vertices + 16 * pad, codes + pad
        field_before, record_before = e.read(field, FIELD_SIZE), e.read(record, RECORD_SIZE)
        e.hh_side = side
        abandoned = 0
        try:
            with HardwareRounding(e):
                run(e, CALC_LINE, [field, side.cbase, side.vbase, spare, c["low"], c["high"], c["row"]], {CALC_LINE},
                    side.stubs())
        except RuntimeError:
            # the original reads through the NULL math unit after the assertion: the emulator faults there
            assert c["no_math"] and e.last_fault is not None and e.last_fault[1] < 0x1000, e.last_fault
            abandoned = 1
        finally:
            e.hh_side = None
        return {"abandoned": abandoned, "vertices": [canon(w) for w in rs(e, vertices, len(c["vertices"]))],
                "codes": list(e.read(codes, len(c["codes"]))), "spare": rs(e, spare, 128),
                "object unchanged": int(e.read(field, FIELD_SIZE) == field_before and
                                        e.read(record, RECORD_SIZE) == record_before),
                "log": side.L.log, "dump": side.dump}


def host_line(lib, c):
    bind(lib)
    host_grid(lib, c)
    CL.host_begin(lib, host_labels(lib))
    codes = arr(C.c_uint8, c["codes"])
    vertices = arr(C.c_uint32, c["vertices"])
    spare = arr(C.c_uint32, c["spare"])
    same = lib.tpw_hh_line(codes, c["pad"], vertices, c["pad"], spare, c["low"], c["high"], c["row"],
                           1 if c["no_math"] else 0)
    return {"abandoned": same >> 1, "vertices": [canon(w) for w in list(vertices)[:len(c["vertices"])]],
            "codes": list(codes)[:len(c["codes"])], "spare": list(spare)[:128], "object unchanged": same & 1,
            "log": CL.host_end(lib), "dump": host_dump(lib)}


def line_edges():
    r = random.Random(13)
    cases = []
    # every count from 1 to 12 (the unrolled loop and its remainder), at several places
    for count in list(range(1, 13)) + [15, 16, 17, 31, 32, 33, 63, 64, 65, 100, 127, 128]:
        for low in (0, 1, 50):
            high = low + count - 1
            if high > 127:
                continue
            cases.append(line_case(r, r.choice([1, 9, 128]), r.choice([0, 1, 7, 127]), low, high))
    # the end of the row, and the early return from 127 on
    for low, high in ((126, 126), (126, 127), (125, 127), (127, 127), (127, 130), (128, 140), (200, 100), (1000, 5)):
        cases.append(line_case(r, 5, 3, low, high))
    # an empty range: the count is 0 or less, and the transform and the test still run
    for low, high in ((5, 4), (5, 3), (5, 0), (0, -1), (60, 59), (126, 125), (100, 2)):
        cases.append(line_case(r, 5, 3, low, high))
    # columns below 0 (the arrays have items in front)
    for low, high in ((-1, 2), (-4, -1), (-8, 3), (-3, -3)):
        cases.append(line_case(r, 5, 2, low, high))
    # special values in the record and the heights
    for special in (0.3, 1.0):
        for _ in range(3):
            cases.append(line_case(r, 7, 5, 0, 20, special=special))
    # big rows
    cases.append(line_case(r, 128, 127, 0, 127))
    cases.append(line_case(r, 128, 127, 100, 127))
    # no math unit: the assertion (tb_matrix.h line 0x98) and then a fault, except on the early return
    for low, high in ((0, 5), (3, 2), (126, 127), (127, 127), (200, 100)):
        c = line_case(r, 5, 3, low, high)
        c["no_math"] = True
        cases.append(c)
    return cases


def line_random(r):
    width = r.choice([1, 2, 5, 9, 33, 64, 127, 128])
    row = r.choice([0, 1, 2, 3, 9, 40, 126, 127])
    k = r.random()
    if k < 0.1:
        low = r.randint(120, 140)
        high = r.randint(low - 3, 140)
        if low < 127:
            high = min(high, 127)   # the arrays hold 128 items
    elif k < 0.2:
        low = r.randint(-5, 5)
        high = r.randint(low - 2, 20)
    else:
        low = r.randint(0, 126)
        high = r.randint(max(low - 2, -1), 127)
    return line_case(r, width, row, low, high)


# ----------------------------------------------------------------- the tests

TESTS = [
    FnTest("CHField::CheckGamutArea", ppc_gamut, host_gamut, edge=gamut_edges, rnd=gamut_random),
    FnTest("CHField::ProcessTextureParameters", ppc_process, host_process, edge=process_edges, rnd=process_random),
    FnTest("CHField::QSort_textures", ppc_sort_textures, host_sort_textures,
           edge=lambda: sorts_edges(textures_item), rnd=sort_textures_random),
    FnTest("CHField::qSortTextures_lpTex", ppc_sort_quads, host_sort_quads,
           edge=lambda: sorts_edges(quads_item), rnd=sort_quads_random),
    FnTest("QSortFaces_float", ppc_sort_faces, host_sort_faces, edge=faces_edges, rnd=sort_faces_random),
    FnTest("CHField::GetTexturesUsed", ppc_used, host_used, edge=used_edges, rnd=used_random),
    FnTest("CHField::CalcFullLighting", ppc_lighting, host_lighting, edge=lighting_edges, rnd=lighting_random),
    FnTest("CHField::CalcLine", ppc_line, host_line, edge=line_edges, rnd=line_random),
]
