"""Unit w84, the ride sign's image filters and shaders, against libtpw_unit_SignShaders.dylib (tests/unit_SignShaders.cpp), in the main
app ("SimTheme Park"):
  src/rsse_sign_filters.cpp   erodebuffer_erode 0x100c8e40, make_gaussian_filter_2d 0x100c90d8, copy_offset_alpha 0x100c922c,
                              dilate 0x100c9404, erode 0x100c9650, blur 0x100c98e4
  src/rsse_sign_shaders.cpp   PlasticShader::clear 0x100ca6a0, ~PlasticShader 0x100ca558, load_settings 0x100ca724, bevel 0x100ca95c,
                              blur_with_mask 0x100cab6c, plastic_shade 0x100caf40, ShadowGlow::shadow_glow 0x100cb468,
                              ~ShadowGlow 0x100cb7c0

A case is a description of small bitmaps (width, height, bytes per pixel, buffer bytes or none; a buffer may be longer than the
bitmap, the spare bytes being what the original reads past its last row) and numbers, built twice: in emulator memory at the Mac's
layouts (a bitmap is four words, the buffer a separate block) for the original, and by the unit's entry points (tpw_w84_*) for the
translation. Every callee outside the unit is a fake that records the same event on both sides: Bitmap::clear and realloc (with
the real ones' effects), Bitmap::descimate (the tile's bytes come from a scripted "Texel" queue), TPFile_Read ("Word" queue) and
_USwapBlock32, and operator new[] and delete[] (every new[] with its label and size, every delete of a labelled block), so the two
sides' heap traffic is compared in order. The dump of every case holds every bitmap of the operation, buffer bytes included.

The C library: sin, cos, exp and sqrt of the PowerPC side are Python's math functions and pow is math.pow, which are the host's libm,
the same functions the translation calls. pow is recorded on both sides with its two arguments, which shows the rounding of the float
arithmetic upstream of it (plastic_shade's normal and half vector). A difference between the Mac's libm and the host's is therefore NOT visible here (nothing
can show it without the Mac's library); the float order of operations, the fused multiply-adds and the conversions are.

Float operations of bevel and plastic_shade run under HardwareRounding (test_RainRender): the emulator rounds fused single
multiply-adds twice, the processor once.

Not tested: a positive mOffsetY in plastic_shade (the rows above it get a negative row number, whose remainder is negative, and the
texel is read before the tile's buffer: the original's bug, reading memory the two sides do not share), the failure branches that need operator new[] to answer NULL (dilate, erode, blur, shadow_glow: the host's new[] throws,
and so does the original's, see src/Bitmap.cpp), the destructors on a NULL object (the Mac tests for it; C++ never calls with NULL),
negative or NaN sigma in blur and blur_with_mask (the kernel's new[] would be enormous), copies that would write outside the
destination (copy_offset_alpha with a move of a whole width or more: the original's bug, not reproducible without corrupting memory).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_RainRender import HardwareRounding

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_SignShaders"
MASK = 0xFFFFFFFF

ERODE_BUFFER, GAUSSIAN, COPY_OFFSET, DILATE, ERODE, BLUR = 0x100C8E40, 0x100C90D8, 0x100C922C, 0x100C9404, 0x100C9650, 0x100C98E4
SHADER_CLEAR, SHADER_DTOR, LOAD_SETTINGS, BEVEL = 0x100CA6A0, 0x100CA558, 0x100CA724, 0x100CA95C
BLUR_WITH_MASK, PLASTIC_SHADE, SHADOW_GLOW, GLOW_DTOR = 0x100CAB6C, 0x100CAF40, 0x100CB468, 0x100CB7C0
TPFILE_READ = 0x10004260
BEVEL_END, SHADE_END = BEVEL + 468, PLASTIC_SHADE + 1260
SHADER_LABEL, GLOW_LABEL, FILE_LABEL = 20, 21, 22
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def rs(e, a, n):
    return list(struct.unpack(">" + "I" * n, e.read(a, 4 * n)))


def fbits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def dbits(x):
    return struct.unpack(">Q", struct.pack(">d", x))[0]


def to_i64(v):
    # The host passes descriptions as signed 64-bit numbers.
    return v - (1 << 64) if v >= (1 << 63) else v


def canon_double(bits):
    # A NaN as one value (the emulator and the host differ in NaN sign and payload only).
    if (bits & 0x7FF0000000000000) == 0x7FF0000000000000 and (bits & 0x000FFFFFFFFFFFFF):
        return 0x7FF8000000000000
    return bits


# ---------------------------------------------------------------------------------------------------------------- the fakes


def build_stubs():
    S = CL.Stubs()
    S.real = {ERODE_BUFFER, GAUSSIAN, COPY_OFFSET, DILATE, ERODE, BLUR, SHADER_CLEAR, SHADER_DTOR, LOAD_SETTINGS, BEVEL,
              BLUR_WITH_MASK, PLASTIC_SHADE, SHADOW_GLOW, GLOW_DTOR}

    def new_array(e):
        # operator new[]: a zeroed block of the scratch area, a fresh label, the event the host's observer records.
        p = e.w84
        size = e.r(3)
        block = p.alloc(size, bytes([0xA5]) * size)
        e.set_r(3, block)
        p.L.rec("new[]", (p.L.newlab(block), size))

    def delete_block(e):
        # operator delete[] and delete: the event for a labelled block.
        L, block = e.cl, e.r(3)
        if block and L.lab(block) > 0:
            L.rec("delete", (L.lab(block),))
            L.forget(block)

    def bitmap_clear(e):
        # Bitmap::clear: the four words zero, the buffer given back.
        this = e.r(3)
        e.cl.rec("Bitmap.clear", (e.cl.lab(this),))
        data = e.u32(this + 12)
        ws(e, this, [0, 0, 0])
        if data:
            delete_block_of(e, data)
        ws(e, this + 12, [0])

    def delete_block_of(e, block):
        L = e.cl
        if L.lab(block) > 0:
            L.rec("delete", (L.lab(block),))
            L.forget(block)

    def bitmap_realloc(e):
        # Bitmap::realloc(width, height, bytes per pixel): a new zeroed buffer when anything differs or there is none, else zeroed.
        p = e.w84
        this, width, height, bpp = e.r(3), e.r(4), e.r(5), e.r(6)
        p.L.rec("Bitmap.realloc", (p.L.lab(this), width, height, bpp))
        cur_w, cur_h, cur_bpp, data = rs(e, this, 4)
        size = (bpp * width * height) & MASK
        if width != cur_w or height != cur_h or bpp != cur_bpp or data == 0:
            if data:
                delete_block_of(e, data)
            data = p.alloc(size, bytes([0xA5]) * size)
            p.L.rec("new[]", (p.L.newlab(data), size))
            ws(e, this, [width, height, bpp, data])
        e.write(data, bytes(size))
        e.set_r(3, 1)

    def bitmap_descimate(e):
        # Bitmap::descimate(dest): the destination's bytes from the "Texel" queue.
        p = e.w84
        this, dest = e.r(3), e.r(4)
        tw, th, tbpp = rs(e, this, 3)
        dw, dh, dbpp, data = rs(e, dest, 4)
        p.L.rec("Bitmap.descimate", (p.L.lab(this), p.L.lab(dest), tw, th, tbpp, dw, dh, dbpp))
        count = (dw * dh * dbpp) & MASK
        e.write(data, bytes(p.L.ret("Texel", 0) & 0xFF for _ in range(count)))
        e.set_r(3, 1)

    def tpfile_read(e):
        # TPFile_Read(dest, size, count, file): four little-endian bytes of the "Word" queue.
        p = e.w84
        dest, size, count, file = e.r(3), e.r(4), e.r(5), e.r(6)
        p.L.rec("TPFile_Read", (dest - p.base, size, count, p.L.lab(file)))
        e.write(dest, struct.pack("<I", p.L.ret("Word", 0) & MASK))
        e.set_r(3, 4)

    def swap_block(e):
        # _USwapBlock32(words, count): swaps in place, and records where.
        p = e.w84
        words, count = e.r(3), T.s32(e.r(4))
        p.L.rec("USwapBlock32", (words - p.base, count))
        for i in range(max(count, 0)):
            e.write(words + 4 * i, e.read(words + 4 * i, 4)[::-1])

    def power(e):
        # pow(x, y) of the C library (the host's, through Python).
        x, y = e.f(1), e.f(2)
        e.cl.rec("pow", (to_i64(canon_double(dbits(x))), to_i64(canon_double(dbits(y)))))
        try:
            value = math.pow(x, y)
        except ValueError:
            value = math.nan
        except OverflowError:
            value = math.inf
        e.set_f1(value)

    S.imp("__nwa__FUl", new_array)
    S.imp("__dla__FPv", delete_block)
    S.imp("__dl__FPv", delete_block)
    S.imp("clear__6BitmapFv", bitmap_clear)
    S.imp("realloc__6BitmapFUiUiUi", bitmap_realloc)
    S.imp("descimate__6BitmapFR6Bitmap", bitmap_descimate)
    S.imp("_USwapBlock32__13SamsUtilitiesFPUll", swap_block)
    S.imp("pow", power)
    S.internal(TPFILE_READ, tpfile_read)
    return S


STUBS = build_stubs()


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    """One case on the PowerPC side: the fakes active, a fresh log, the sizes of the blocks (to dump buffers)."""

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case.get("q"))
        e.w84 = self
        self.sizes = {}
        self.base = 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)
        self.e.w84 = None

    def alloc(self, size, data=None):
        # A block of at least four bytes (two blocks never share an address), zero filled unless data is given.
        block = self.e.alloc(max(size, 4), data)
        self.sizes[block] = size
        return block

    def call(self, address, *args, rounding_end=None):
        if rounding_end is None:
            return T.call_ppc(self.e, address, list(args))
        with HardwareRounding(self.e, [(address, rounding_end)]):
            return T.call_ppc(self.e, address, list(args))

    def bitmap(self, spec, label, at=None):
        # A Bitmap (four words) of the description at `at` (or a new block); its buffer is labelled 10 + label.
        w, h, bpp, data = spec
        e = self.e
        address = at if at is not None else self.alloc(16)
        buffer = 0
        if data is not None:
            buffer = self.alloc(len(data), bytes(data))
            self.L.labels[buffer] = 10 + label
        ws(e, address, [w, h, bpp, buffer])
        self.L.labels[address] = label
        return address

    def shader(self, words, bitmaps):
        # A PlasticShader of 0x6c bytes: eleven words, then the mask, file, bevel and shaded bitmaps.
        address = self.alloc(0x6C)
        ws(self.e, address, words)
        for k, offset in enumerate((0x2C, 0x3C, 0x4C, 0x5C)):
            self.bitmap(bitmaps[k], k + 1, address + offset)
        self.L.labels[address] = SHADER_LABEL
        return address

    def dump_bitmap(self, address):
        w, h, bpp, buffer = rs(self.e, address, 4)
        if buffer == 0:
            return [w, h, bpp, -1]
        size = self.sizes[buffer]
        return [w, h, bpp, size] + list(self.e.read(buffer, size))

    def dump_shader(self, address, words_only=False):
        out = rs(self.e, address, 11)
        if not words_only:
            for offset in (0x2C, 0x3C, 0x4C, 0x5C):
                out += self.dump_bitmap(address + offset)
        return out


def flat_bitmap(spec):
    w, h, bpp, data = spec
    return [w, h, bpp, -1 if data is None else len(data)] + ([] if data is None else list(data))


def flat_shader(words, bitmaps):
    out = list(words)
    for spec in bitmaps:
        out += flat_bitmap(spec)
    return out


def host_run(lib, entry, description, queues=None):
    fn = getattr(lib, entry)
    fn.restype = C.c_int32
    fn.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    values = (C.c_int64 * max(len(description), 1))(*[to_i64(v) for v in description])
    out = (C.c_int64 * 16384)()
    CL.host_begin(lib, {}, queues)
    n = fn(values, out)
    log = CL.host_end(lib)
    return [int(out[i]) for i in range(n)], log


def pointer_arrays_halved(log, above):
    # The two pointer arrays of blur_with_mask (the ring, then the rows below, after the kernel and the `above` row copies) are four
    # bytes an entry on the Mac and eight here: the host's sizes are halved to compare with the original's.
    fixed = []
    index = 0
    for name, ints, texts in log:
        if name == "new[]":
            if index in (1, 2 + above):
                ints = (ints[0], ints[1] // 2)
            index += 1
        fixed.append((name, ints, texts))
    return fixed


# ---------------------------------------------------------------------------------------------------------------- generators


def rnd_data(r, n, mode):
    # mode: 0 random, 1 only 0 and 255, 2 small values, 3 all 255, 4 zeros, 5 mostly 255 with holes, 6 mostly 0 with spots
    if mode == 0:
        return bytes(r.getrandbits(8) for _ in range(n))
    if mode == 1:
        return bytes(r.choice((0, 255)) for _ in range(n))
    if mode == 2:
        return bytes(r.randint(0, 3) for _ in range(n))
    if mode == 3:
        return bytes([255]) * n
    if mode == 4:
        return bytes(n)
    if mode == 5:
        return bytes(0 if r.random() < 0.2 else 255 for _ in range(n))
    return bytes(r.randint(1, 255) if r.random() < 0.2 else 0 for _ in range(n))


MODES = 7
DIMS = [(1, 1), (2, 1), (1, 2), (2, 2), (3, 2), (2, 3), (3, 3), (4, 3), (3, 4), (4, 4), (5, 3), (5, 5), (6, 4), (7, 5), (8, 8),
        (9, 6), (12, 1), (1, 12), (10, 10), (11, 3)]


def bitmap_of(r, w, h, bpp, mode=None, spare=0):
    mode = r.randrange(MODES) if mode is None else mode
    return (w, h, bpp, rnd_data(r, w * h * bpp + spare, mode))


def anything(r):
    # A bitmap the function must not read (or only a dump of): odd sizes, with or without a buffer.
    w, h = r.randint(0, 5), r.randint(0, 5)
    bpp = r.choice((0, 1, 2, 4))
    if r.random() < 0.3:
        return (w, h, bpp, None)
    return (w, h, bpp, rnd_data(r, w * h * bpp, 0))


def i32w(v):
    return ((v + 2 ** 31) % 2 ** 32) - 2 ** 31


def clamp_window(v, top):
    return (0 if v < 0 else v) if v <= top else top


def copy_is_safe(w, h, dx, dy):
    # copy_offset_alpha writes where the Mac does; the case must stay inside the destination buffer.
    row0, row1 = clamp_window(i32w(-dy), h - 1), clamp_window(i32w(h - dy - 1), h - 1)
    col0, col1 = clamp_window(i32w(-dx), w - 1), clamp_window(i32w(w - dx - 1), w - 1)
    if row1 < row0 or col1 < col0:
        return True
    start = w * max(dy, 0) + max(dx, 0)
    return 0 <= start and start + (row1 - row0) * w + (col1 - col0) < w * h


def safe_offset(r, w, h, dx=None, dy=None):
    for _ in range(50):
        x = r.choice([0, 0, 1, -1, 2, -2, w - 1, -(w - 1), w, -w, w + 1, -(w + 1), INT_MIN, INT_MAX, r.randint(-w - 2, w + 2)]) \
            if dx is None else dx
        y = r.choice([0, 0, 1, -1, 2, -2, h - 1, -(h - 1), h, -h, h + 1, -(h + 1), INT_MIN, INT_MAX, r.randint(-h - 2, h + 2)]) \
            if dy is None else dy
        if copy_is_safe(w, h, x, y):
            return x, y
    return 0, 0


FLOAT_WORDS = [0.0, 1.0, -1.0, 0.5, 2.0, 10.0, 3.7, -5.0, 100.0, 0.2]


def settings_words(r):
    # Eleven words of a settings block: mBevelWidth, six floats, two angles, two ints; any bit pattern.
    return [r.getrandbits(32) for _ in range(11)]


def shader_spec(r, mask=None, file=None, bevel=None, shaded=None, words=None):
    words = settings_words(r) if words is None else words
    bitmaps = [mask or anything(r), file or anything(r), bevel or anything(r), shaded or anything(r)]
    return words, bitmaps


# ---------------------------------------------------------------------------------------------------------------- erodebuffer_erode


def erodebuffer_ppc(e, case):
    p = Ppc(e, case)
    try:
        value, work, mask, out = case["value"], case["work"], case["mask"], case["out"]
        a_work, a_mask, a_out = p.bitmap(work, 1), p.bitmap(mask, 2), p.bitmap(out, 3)
        p.call(ERODE_BUFFER, a_work, a_mask, value, a_out)
        return dict(state=p.dump_bitmap(a_work) + p.dump_bitmap(a_mask) + p.dump_bitmap(a_out), log=p.L.log)
    finally:
        p.done()


def erodebuffer_host(lib, case):
    description = [case["value"]] + flat_bitmap(case["work"]) + flat_bitmap(case["mask"]) + flat_bitmap(case["out"])
    state, log = host_run(lib, "tpw_w84_erode_buffer", description)
    return dict(state=state, log=log)


def erodebuffer_case(r, dims=None, mode=None, value=None):
    w, h = r.choice(DIMS + [(0, 3), (3, 0), (0, 0)]) if dims is None else dims
    mode = r.randrange(MODES) if mode is None else mode
    # Two bytes a pixel: the current byte from the mode, the next byte garbage.
    current = rnd_data(r, w * h, mode)
    junk = rnd_data(r, w * h, 0)
    data = bytes(b for pair in zip(current, junk) for b in pair)
    work = (w, h, r.choice((2, 2, 2, 1, 4, 0)), data)
    out = (w, h, 1, rnd_data(r, w * h + r.choice((0, 0, 3)), r.randrange(MODES)))
    value = r.choice([0, 1, 0x55, 0xAA, 0xFF, r.randrange(256)]) if value is None else value
    return dict(value=value, work=work, mask=anything(r), out=out)


def edge_erodebuffer():
    r = random.Random(8401)
    # Every small size, with the bytes that make the minimum come from each of the eight neighbours in turn.
    for w, h in [(1, 1), (2, 1), (1, 2), (2, 2), (3, 3), (4, 4), (5, 5), (3, 6), (6, 3)]:
        for mode in (3, 5, 1, 2):
            yield erodebuffer_case(r, (w, h), mode, 0xFF)
    for w, h in [(3, 3), (5, 5), (4, 5)]:
        for k in range(w * h):
            # a single hole in a field of 255
            current = bytearray([255]) * (w * h)
            current[k] = 7
            data = bytes(b for c in current for b in (c, 0x11))
            yield dict(value=0x80, work=(w, h, 2, data), mask=anything(r), out=(w, h, 1, bytes([0x33]) * (w * h)))
    for dims in [(0, 0), (0, 3), (3, 0)]:
        yield erodebuffer_case(r, dims)
    for value in (0, 1, 255):
        yield erodebuffer_case(r, (4, 3), 5, value)


def random_erodebuffer(r):
    return erodebuffer_case(r)


# ---------------------------------------------------------------------------------------------------------------- make_gaussian_filter_2d


def gaussian_ppc(e, case):
    p = Ppc(e, case)
    try:
        size, sigma, spare = case["size"], case["sigma"], 2
        count = (size * size if size > 0 else 0) + spare
        values = struct.pack(">d", -12345.0) * count
        address = p.alloc(8 * count + 8, values)
        p.call(GAUSSIAN, address, size & MASK, T.D(sigma))
        bits = [to_i64(canon_double(struct.unpack(">Q", e.read(address + 8 * i, 8))[0])) for i in range(count)]
        return dict(state=bits, log=p.L.log)
    finally:
        p.done()


def gaussian_host(lib, case):
    state, log = host_run(lib, "tpw_w84_gaussian", [case["size"], dbits(case["sigma"]), 2])
    return dict(state=state, log=log)


SIGMAS = [0.5, 1.0, 1.5, 2.0, 0.1, 3.7, 10.0, 0.3, 0.7, 1.9, 2.4, 1e-3, 100.0, 0.0, -1.0, 1e-160, math.inf, math.nan, 1e150]


def edge_gaussian():
    for size in (-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12):
        for sigma in SIGMAS:
            yield dict(size=size, sigma=sigma)


def random_gaussian(r):
    return dict(size=r.randint(0, 11), sigma=r.choice(SIGMAS + [r.uniform(0.01, 8.0) for _ in range(6)]))


# ---------------------------------------------------------------------------------------------------------------- copy_offset_alpha


def copy_ppc(e, case):
    p = Ppc(e, case)
    try:
        a_dest, a_src = p.bitmap(case["dest"], 1), p.bitmap(case["src"], 2)
        answer = p.call(COPY_OFFSET, a_dest, a_src, case["dx"] & MASK, case["dy"] & MASK)[0]
        return dict(state=[answer] + p.dump_bitmap(a_dest) + p.dump_bitmap(a_src), log=p.L.log)
    finally:
        p.done()


def copy_host(lib, case):
    description = [case["dx"], case["dy"]] + flat_bitmap(case["dest"]) + flat_bitmap(case["src"])
    state, log = host_run(lib, "tpw_w84_copy_offset", description)
    return dict(state=state, log=log)


def dest_before(r, w, h):
    # The destination the call finds: none, the right size, another size, another pixel size.
    kind = r.randrange(5)
    if kind == 0:
        return (r.randint(0, 3), r.randint(0, 3), 1, None)
    if kind == 1:
        return (w, h, 1, rnd_data(r, w * h, 0))
    if kind == 2:
        return (w + 1, h, 1, rnd_data(r, (w + 1) * h, 0))
    if kind == 3:
        return (w, h, 4, rnd_data(r, w * h * 4, 0))
    other_w, other_h = r.randint(1, 5), r.randint(1, 5)
    return (other_w, other_h, 2, rnd_data(r, other_w * other_h * 2, 0))


def copy_case(r, dims=None, dx=None, dy=None, bpp=None):
    w, h = r.choice(DIMS) if dims is None else dims
    bpp = r.choice((1, 1, 4, 2, 3)) if bpp is None else bpp
    dx, dy = safe_offset(r, w, h, dx, dy)
    return dict(dx=dx, dy=dy, dest=dest_before(r, w, h), src=bitmap_of(r, w, h, bpp, 0))


def edge_copy():
    r = random.Random(8402)
    for w, h in [(1, 1), (2, 2), (3, 3), (4, 3), (5, 4), (6, 6)]:
        # every move with the window inside, partly outside on each side, and just off the edge
        for dx in range(-w - 1, w + 2):
            for dy in range(-h - 1, h + 2):
                if copy_is_safe(w, h, dx, dy):
                    yield dict(dx=dx, dy=dy, dest=dest_before(r, w, h), src=bitmap_of(r, w, h, 1, 0))
    for dx, dy in [(INT_MIN, INT_MIN), (INT_MIN, 0), (0, INT_MIN), (INT_MAX, INT_MIN), (INT_MIN, INT_MAX)]:
        if copy_is_safe(4, 3, dx, dy):
            yield copy_case(r, (4, 3), dx, dy)
    for bpp in (1, 2, 3, 4):
        yield copy_case(r, (5, 4), 1, 1, bpp)
        yield copy_case(r, (5, 4), -2, -1, bpp)
    # Moves of a whole width or height or more (the window clamps to one pixel and the write lands past the row or the buffer): the
    # destination gets spare bytes up to the last byte written, the final read of the original's write position.
    for w, h in [(4, 1), (1, 4), (3, 3), (5, 2), (1, 1)]:
        for dx in range(-w - 1, w + 3):
            for dy in range(-h - 1, h + 3):
                if copy_is_safe(w, h, dx, dy):
                    continue
                row0, row1 = clamp_window(-dy, h - 1), clamp_window(h - dy - 1, h - 1)
                col0, col1 = clamp_window(-dx, w - 1), clamp_window(w - dx - 1, w - 1)
                last = w * max(dy, 0) + max(dx, 0) + (row1 - row0) * w + (col1 - col0)
                if last < 0:
                    continue
                yield dict(dx=dx, dy=dy, dest=(w, h, 1, rnd_data(r, max(w * h, last + 1), 0)), src=bitmap_of(r, w, h, 1, 0))


def random_copy(r):
    return copy_case(r)


# ---------------------------------------------------------------------------------------------------------------- dilate, erode


def morph_ppc(address):
    def run(e, case):
        p = Ppc(e, case)
        try:
            a_bitmap = p.bitmap(case["bitmap"], 1)
            answer = p.call(address, a_bitmap, case["radius"] & MASK)[0]
            return dict(state=[answer] + p.dump_bitmap(a_bitmap), log=p.L.log)
        finally:
            p.done()
    return run


def morph_host(entry):
    def run(lib, case):
        state, log = host_run(lib, entry, [case["radius"]] + flat_bitmap(case["bitmap"]))
        return dict(state=state, log=log)
    return run


RADII = [0, 1, 2, 3, 4, 7, -1, -2, 100, INT_MAX, INT_MIN, 0x40000000]


def morph_case(r, dims=None, bpp=None, radius=None, mode=None):
    w, h = r.choice(DIMS + [(0, 4), (4, 0), (0, 0)]) if dims is None else dims
    bpp = r.choice((1, 1, 1, 4, 2)) if bpp is None else bpp
    radius = r.choice(RADII + [r.randint(0, 6)]) if radius is None else radius
    return dict(radius=radius, bitmap=bitmap_of(r, w, h, bpp, mode))


def edge_morph():
    r = random.Random(8403)
    for w, h in [(1, 1), (5, 1), (1, 5), (3, 3), (4, 6), (7, 5), (0, 4), (4, 0)]:
        for radius in RADII:
            yield morph_case(r, (w, h), 1, radius, 6)
            yield morph_case(r, (w, h), 1, radius, 5)
    for bpp in (2, 3, 4):
        for radius in (0, 1, 2, -1):
            yield morph_case(r, (5, 4), bpp, radius)


def random_morph(r):
    return morph_case(r)


# ---------------------------------------------------------------------------------------------------------------- blur


def blur_ppc(e, case):
    p = Ppc(e, case)
    try:
        a_bitmap = p.bitmap(case["bitmap"], 1)
        answer = p.call(BLUR, a_bitmap, T.D(case["sigma"]))[0]
        return dict(state=[answer] + p.dump_bitmap(a_bitmap), log=p.L.log)
    finally:
        p.done()


def blur_host(lib, case):
    state, log = host_run(lib, "tpw_w84_blur", [dbits(case["sigma"])] + flat_bitmap(case["bitmap"]))
    return dict(state=state, log=log)


# sigma giving kernels of 3 to 10 taps: (int)(2.5 * sigma) + 3
BLUR_SIGMAS = [0.0, 0.1, 0.39, 0.4, 0.5, 0.79, 0.8, 1.0, 1.19, 1.2, 1.5, 1.59, 1.6, 1.9, 2.0, 2.39, 2.4, 2.8, 3.2, 4.0, 7.3, 30.0]


def blur_case(r, dims=None, bpp=None, sigma=None, mode=None):
    w, h = r.choice(DIMS + [(0, 4), (4, 0), (0, 0)]) if dims is None else dims
    bpp = r.choice((1, 1, 1, 3)) if bpp is None else bpp
    sigma = r.choice(BLUR_SIGMAS + [r.uniform(0.0, 5.0)]) if sigma is None else sigma
    return dict(sigma=sigma, bitmap=bitmap_of(r, w, h, bpp, mode))


def edge_blur():
    r = random.Random(8404)
    for sigma in BLUR_SIGMAS:
        for dims in [(1, 1), (6, 1), (1, 6), (5, 4), (9, 7), (0, 3), (3, 0)]:
            yield blur_case(r, dims, 1, sigma, 0)
    for mode in range(MODES):
        yield blur_case(r, (7, 6), 1, 1.3, mode)
    for bpp in (2, 3, 4):
        yield blur_case(r, (5, 4), bpp, 1.0)
    # Negative sigma: the squared sigma is the same, the tap count (int)(2.5 * sigma) + 3 truncates toward zero, so -0.4 gives 2 taps,
    # -0.8 and -1.0 one, -1.2 and -1.5 none (an empty kernel, every sum 0); a sigma of -1.6 or less would ask for a negative count.
    for sigma in (-0.1, -0.39, -0.4, -0.79, -0.8, -1.0, -1.19, -1.2, -1.5, -0.0, 1e-200, -1e-200):
        for dims in [(1, 1), (6, 1), (5, 4), (0, 3)]:
            yield blur_case(r, dims, 1, sigma, 0)


def random_blur(r):
    return blur_case(r)


# ---------------------------------------------------------------------------------------------------------------- PlasticShader::clear, destructor


def shader_ppc(address, flag=False):
    def run(e, case):
        p = Ppc(e, case)
        try:
            shader = p.shader(*case["shader"])
            if flag:
                answer = p.call(address, shader, case["flag"] & MASK)[0]
                freed = int(p.L.lab(shader) <= 0)
                state = [freed] if freed else [0] + p.dump_shader(shader)
                return dict(state=state, log=p.L.log, answer=int(answer == shader))
            p.call(address, shader)
            return dict(state=p.dump_shader(shader), log=p.L.log)
        finally:
            p.done()
    return run


def shader_host(entry, flag=False):
    def run(lib, case):
        description = ([case["flag"]] if flag else []) + flat_shader(*case["shader"])
        state, log = host_run(lib, entry, description)
        if flag:
            return dict(state=state, log=log, answer=1)
        return dict(state=state, log=log)
    return run


def clear_case(r):
    return dict(shader=shader_spec(r, *[bitmap_of(r, *r.choice(DIMS), r.choice((1, 4))) if r.random() < 0.7 else anything(r)
                                        for _ in range(4)]))


def edge_clear():
    r = random.Random(8405)
    empty = (0, 0, 0, None)
    for mask in range(16):
        specs = [bitmap_of(r, 3, 2, 1, 0) if (mask >> k) & 1 else empty for k in range(4)]
        yield dict(shader=shader_spec(r, *specs))
    yield dict(shader=shader_spec(r, (3, 2, 1, None), (0, 0, 0, b""), (2, 2, 4, bytes(16)), (5, 5, 1, bytes(25))))


def random_clear(r):
    return clear_case(r)


FLAGS = [1, 0, -1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x7FFFFFFF, 0x80000000, 0xFFFF0001, 0xFFFF8000, 0x12345678]


def dtor_case(r, flag=None):
    flag = r.choice(FLAGS + [r.getrandbits(32), r.getrandbits(16)]) if flag is None else flag
    return dict(flag=flag & MASK, shader=clear_case(r)["shader"])


def edge_dtor():
    r = random.Random(8406)
    for flag in FLAGS:
        yield dtor_case(r, flag)


def random_dtor(r):
    return dtor_case(r)


# ---------------------------------------------------------------------------------------------------------------- load_settings


def load_ppc(e, case):
    p = Ppc(e, case)
    try:
        shader = p.shader(*case["shader"])
        p.base = shader
        file = p.alloc(16)
        p.L.labels[file] = FILE_LABEL
        answer = p.call(LOAD_SETTINGS, shader, file)[0]
        return dict(state=[answer] + p.dump_shader(shader), log=p.L.log)
    finally:
        p.done()


def load_host(lib, case):
    state, log = host_run(lib, "tpw_w84_load_settings", flat_shader(*case["shader"]), case["q"])
    return dict(state=state, log=log)


def load_case(r, words=None):
    words = [r.getrandbits(32) for _ in range(11)] if words is None else words
    return dict(shader=shader_spec(r), q={"Word": words})


def edge_load():
    r = random.Random(8407)
    yield load_case(r, [0] * 11)
    yield load_case(r, [MASK] * 11)
    yield load_case(r, [0x3F800000 + i for i in range(11)])
    yield load_case(r, [1 << i for i in range(11)])
    yield load_case(r, [0x01020304 * (i + 1) & MASK for i in range(11)])
    yield load_case(r, [3, fbits(10.0), fbits(1.0), fbits(0.3), fbits(0.7), fbits(1.0), fbits(20.0), fbits(30.0), fbits(45.0), 40, 5])


def random_load(r):
    return load_case(r)


# ---------------------------------------------------------------------------------------------------------------- PlasticShader::bevel


def bevel_ppc(e, case):
    p = Ppc(e, case)
    try:
        shader = p.shader(*case["shader"])
        a_mask, a_out = p.bitmap(case["mask"], 5), p.bitmap(case["out"], 6)
        p.call(BEVEL, shader, a_mask, case["width"], a_out, rounding_end=BEVEL_END)
        return dict(state=p.dump_shader(shader) + p.dump_bitmap(a_mask) + p.dump_bitmap(a_out), log=p.L.log)
    finally:
        p.done()


def bevel_host(lib, case):
    description = flat_shader(*case["shader"]) + [case["width"]] + flat_bitmap(case["mask"]) + flat_bitmap(case["out"])
    state, log = host_run(lib, "tpw_w84_bevel", description)
    return dict(state=state, log=log)


def bevel_case(r, dims=None, width=None, mode=None, spare=0):
    w, h = r.choice(DIMS + [(0, 3), (3, 0)]) if dims is None else dims
    width = r.choice([0, 1, 2, 3, 4, 5, 6, 9]) if width is None else width
    mode = r.choice((1, 5, 6, 0)) if mode is None else mode
    out = (w, h, 1, rnd_data(r, w * h + spare, 0))
    return dict(shader=shader_spec(r), width=width, mask=bitmap_of(r, w, h, 1, mode), out=out)


def edge_bevel():
    r = random.Random(8408)
    for width in range(0, 8):
        for dims in [(1, 1), (5, 5), (9, 6), (10, 10), (2, 7)]:
            yield bevel_case(r, dims, width, 5)
            yield bevel_case(r, dims, width, 1)
    for width in (1, 2, 3, 4):
        yield bevel_case(r, (8, 8), width, 3)
        yield bevel_case(r, (8, 8), width, 4)
        yield bevel_case(r, (7, 5), width, 1, spare=5)
    yield bevel_case(r, (0, 3), 3)
    yield bevel_case(r, (3, 0), 3)
    # The curve (t * 2t^2 - 3t^2 with t = 1 - i / width) rounded once (fmsubs) or twice differs in a truncated byte only for a wide
    # bevel: from width 3664 on, round 1 (found by a search over widths to 4,500) gives -255 fused and -256 unfused, so the stamp's
    # low byte is 1 against 0. A mask with a hole at the edge erodes from it, one layer a round, so round 1 stamps the second pixel.
    for width in (3664, 3667):
        for mask in (bytes([0, 255, 255, 255, 255]), bytes([255, 255, 255, 255, 0])):
            yield dict(shader=shader_spec(r), width=width, mask=(5, 1, 1, mask), out=(5, 1, 1, bytes([7, 7, 7, 7, 7])))
        yield dict(shader=shader_spec(r), width=width, mask=(4, 4, 1, bytes([0, 255, 255, 255] * 4)), out=(4, 4, 1, bytes([9]) * 16))


def random_bevel(r):
    return bevel_case(r)


# ---------------------------------------------------------------------------------------------------------------- PlasticShader::blur_with_mask


def blur_mask_ppc(e, case):
    p = Ppc(e, case)
    try:
        shader = p.shader(*case["shader"])
        a_bitmap, a_mask = p.bitmap(case["bitmap"], 5), p.bitmap(case["mask"], 6)
        p.call(BLUR_WITH_MASK, shader, a_bitmap, a_mask, T.F(case["sigma"]))
        return dict(state=p.dump_shader(shader) + p.dump_bitmap(a_bitmap) + p.dump_bitmap(a_mask), log=p.L.log)
    finally:
        p.done()


def blur_mask_host(lib, case):
    sigma_bits = fbits(case["sigma"])
    description = flat_shader(*case["shader"]) + [sigma_bits] + flat_bitmap(case["bitmap"]) + flat_bitmap(case["mask"])
    state, log = host_run(lib, "tpw_w84_blur_with_mask", description)
    taps = int(2.5 * case["sigma"]) + 3
    return dict(state=state, log=pointer_arrays_halved(log, (taps + 1) // 2))


MASK_SIGMAS = [0.0, 0.2, 0.39, 0.4, 0.79, 0.8, 1.0, 1.19, 1.2, 1.59, 1.6, 1.99, 2.0, 2.39, 2.4, 3.0, 3.6]


def blur_mask_case(r, dims=None, sigma=None, mode=None, wider=None):
    w, h = r.choice(DIMS + [(0, 1), (0, 3)]) if dims is None else dims
    sigma = r.choice(MASK_SIGMAS + [f32(r.uniform(0.0, 4.0))]) if sigma is None else sigma
    mode = r.choice((0, 1, 5, 6, 2)) if mode is None else mode
    wider = r.choice((0, 0, 0, 1, 3)) if wider is None else wider
    mask = (w + wider, h, 1, rnd_data(r, (w + wider) * h, r.choice((1, 5, 6, 3))))
    return dict(shader=shader_spec(r), sigma=sigma, bitmap=bitmap_of(r, w, h, r.choice((1, 1, 2)), mode), mask=mask)


def f32(x):
    return T.f32(x)


def edge_blur_mask():
    r = random.Random(8409)
    for sigma in MASK_SIGMAS:
        for dims in [(1, 1), (6, 1), (1, 6), (5, 4), (9, 7), (0, 2)]:
            yield blur_mask_case(r, dims, f32(sigma), 0, 0)
    # masks with holes, all clear, all set
    for mode in (1, 5, 6, 3, 4):
        case = blur_mask_case(r, (7, 6), f32(1.3), 0, 0)
        case["mask"] = (7, 6, 1, rnd_data(r, 42, mode))
        yield case
    for wider in (1, 2, 5):
        yield blur_mask_case(r, (5, 4), f32(1.0), 0, wider)
    # Height 0 with a width: the ring copies read `width` bytes from the bitmap (spare bytes follow its empty buffer) and no row runs.
    for sigma in (0.0, 1.0, 2.0, 3.6):
        for w in (1, 4, 7):
            case = blur_mask_case(r, (w, 0), f32(sigma), 0, 0)
            case["bitmap"] = (w, 0, 1, rnd_data(r, w, 0))
            yield case
    # Negative sigma (see edge_blur): the kernel is (n + 3) squared taps, so -0.4 gives 2 x 2; a smaller size than 2 writes before
    # the pointer array below (the original's bug: the rows below are none), so -0.8 and less are not tested.
    for sigma in (-0.1, -0.39, -0.4, -0.0):
        for dims in [(1, 1), (6, 1), (5, 4), (0, 2)]:
            yield blur_mask_case(r, dims, f32(sigma), 0, 0)


def random_blur_mask(r):
    return blur_mask_case(r)


# ---------------------------------------------------------------------------------------------------------------- PlasticShader::plastic_shade


def shade_ppc(e, case):
    p = Ppc(e, case)
    try:
        shader = p.shader(*case["shader"])
        a_bevel = p.bitmap(case["bevel"], 5)
        p.call(PLASTIC_SHADE, shader, a_bevel, rounding_end=SHADE_END)
        return dict(state=p.dump_shader(shader) + p.dump_bitmap(a_bevel), log=p.L.log)
    finally:
        p.done()


def shade_host(lib, case):
    state, log = host_run(lib, "tpw_w84_plastic_shade", flat_shader(*case["shader"]) + flat_bitmap(case["bevel"]), case["q"])
    return dict(state=state, log=log)


def shade_words(height_scale=10.0, ambient=0.3, diffuse=0.7, specular=1.0, shininess=20.0, yaw=30.0, pitch=45.0, period=3, offset=0,
                bevel_width=3, blur=1.0):
    return [bevel_width, fbits(height_scale), fbits(blur), fbits(ambient), fbits(diffuse), fbits(specular), fbits(shininess),
            fbits(yaw), fbits(pitch), period & MASK, offset & MASK]


def shade_case(r, dims=None, words=None, mask_mode=None, file_width=None, bevel_mode=None):
    w, h = r.choice(DIMS) if dims is None else dims
    period = r.choice((1, 2, 3, 4, 5, 8))
    file_width = r.choice((1, 2, 3, 4, 5, 7)) if file_width is None else file_width
    if words is None:
        words = shade_words(
            height_scale=r.choice(FLOAT_WORDS + [r.uniform(-20, 20)]),
            ambient=r.choice([0.0, 0.2, 0.5, 1.0, 2.0, -0.5, r.random()]),
            diffuse=r.choice([0.0, 0.5, 1.0, 2.0, r.random()]),
            specular=r.choice([0.0, 0.5, 1.0, 3.0, r.random()]),
            shininess=r.choice([0.0, 1.0, 2.0, 8.0, 32.0, 100.0, 0.5, -2.0, r.uniform(0, 50)]),
            yaw=r.choice([0.0, 30.0, -45.0, 90.0, 180.0, 359.0, 720.0, 12.5, r.uniform(-360, 360)]),
            pitch=r.choice([0.0, 30.0, -45.0, 90.0, 180.0, 12.5, r.uniform(-180, 180)]),
            period=period, offset=r.choice([0, -1, -3, -7, -100, r.randint(-20, 0)]))
    period = T.s32(words[9])
    mask_mode = r.choice((1, 5, 6, 3, 0)) if mask_mode is None else mask_mode
    bevel = (w, h, 1, rnd_data(r, w * h + w + 1, r.choice((0, 2, 0, 5, 1)) if bevel_mode is None else bevel_mode))
    mask = (w, h, 1, rnd_data(r, w * h, mask_mode))
    shaded = (w, h, 4, rnd_data(r, w * h * 4, 0))
    file = (file_width, r.randint(1, 6), 4, r.choice((None, rnd_data(r, 16, 0))))
    tile_bytes = file_width * max(period, 0) * 4
    return dict(shader=(words, [mask, file, anything(r), shaded]), bevel=bevel,
                q={"Texel": [r.getrandbits(8) for _ in range(tile_bytes)]})


def shade_fixed(words, w, h, bevel_bytes, texels):
    # A case from explicit numbers: every pixel lit, a one pixel wide tile of two rows (the texels are its eight bytes).
    mask = (w, h, 1, bytes([255]) * (w * h))
    bevel = (w, h, 1, bytes(bevel_bytes))
    return dict(shader=(words, [mask, (1, 2, 4, None), (w, h, 1, None), (w, h, 4, bytes(w * h * 4))]), bevel=bevel,
                q={"Texel": list(texels)})


# Found by a search over random light directions, slopes and colours for inputs where the output byte depends on whether N.L is
# built with fused multiply-adds: one for each of the two (the inner, unitY * lightX + unitX * lightY, and the outer, plus
# unitZ * lightZ). Each differs from the same arithmetic with separately rounded products in one colour byte.
SHADE_FUSED_INNER = ([3, 1105234879, 1065353216, 1045098868, 1073741824, 0, 1036474055, 1074249658, 3273125057, 2, 0], 3, 2,
                     [24, 234, 138, 115, 80, 222, 75, 45, 119, 206], [84, 230, 56, 247, 224, 4, 73, 65])
SHADE_FUSED_OUTER = ([3, 1071729315, 1065353216, 1036647031, 1073741824, 0, 1077586322, 3276271267, 3273548506, 2, 0], 2, 1,
                     [2, 1, 1, 2, 2], [150, 176, 156, 146, 211, 117, 77, 150])


def edge_shade():
    r = random.Random(8410)
    # the period 0 skip, and each light direction with a bevel ramp
    yield shade_case(r, (4, 4), shade_words(period=0))
    for yaw in (0.0, 90.0, 180.0, -90.0, 45.0):
        for pitch in (0.0, 45.0, 90.0, -45.0):
            yield shade_case(r, (5, 4), shade_words(yaw=yaw, pitch=pitch))
    for shininess in (0.0, 1.0, 5.0, 50.0, 0.25, -1.0):
        yield shade_case(r, (4, 4), shade_words(shininess=shininess, specular=1.5))
    for ambient, diffuse, specular in [(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1), (1, 1, 1), (2, 2, 2), (-1, 0, 0), (0.2, 0.7, 5.0)]:
        yield shade_case(r, (5, 5), shade_words(ambient=ambient, diffuse=diffuse, specular=specular))
    for scale in (0.0, 1.0, 10.0, 100.0, 1000.0, -10.0):
        yield shade_case(r, (6, 5), shade_words(height_scale=scale))
    for offset in (0, -1, -2, -3, -7, -100):
        for period in (1, 2, 3, 5):
            yield shade_case(r, (4, 6), shade_words(period=period, offset=offset))
    for file_width in (1, 2, 3, 6):
        yield shade_case(r, (7, 3), file_width=file_width)
    for mode in (1, 5, 6, 3, 4):
        yield shade_case(r, (8, 6), mask_mode=mode)
    # extremes in the float words: NaN, infinities
    nan, inf = math.nan, math.inf
    for kw in ({"height_scale": nan}, {"ambient": nan}, {"diffuse": inf}, {"specular": inf}, {"shininess": nan}, {"shininess": inf},
               {"yaw": nan}, {"pitch": inf}, {"height_scale": inf}, {"ambient": inf, "specular": -inf}, {"diffuse": -inf, "ambient": 1e30}):
        yield shade_case(r, (4, 3), shade_words(**kw))
    yield shade_fixed(*SHADE_FUSED_INNER)
    yield shade_fixed(*SHADE_FUSED_OUTER)
    # N.L exactly 0 with an infinite diffuse term (inf * 0 is NaN, which the test `0 < N.L` keeps out of the sum): the light at yaw 0
    # and pitch 0 is (1, 0, 0) and a flat bevel (every byte alike) makes the surface normal (0, 0, 1), so N.L is 0 on every pixel.
    # Each flat byte, a mask that lights every pixel, and ambient above 0 so the colour shows.
    for flat in (3, 4):
        for ambient in (0.5, 1.0):
            yield shade_case(r, (5, 4), shade_words(diffuse=inf, yaw=0.0, pitch=0.0, ambient=ambient), mask_mode=3, bevel_mode=flat)
    # A height scale so large that the squared slope overflows: the normal's length is infinite, its scale 0, the unit normal (0, 0, 0)
    # and both N.L and N.H are 0 on every pixel the bevel has a slope at. The terms diffuse * N.L and specular * pow(N.H, shininess)
    # must then be left out (shininess 0 would add the whole specular term, an infinite specular or diffuse a NaN).
    for scale in (1e30, -1e30, 1e20):
        for shininess, specular, diffuse in [(0.0, 1.0, 0.7), (20.0, inf, 0.7), (2.0, 1.0, inf), (-2.0, 1.0, 0.7)]:
            yield shade_case(r, (5, 4), shade_words(height_scale=scale, shininess=shininess, specular=specular, diffuse=diffuse),
                             mask_mode=3, bevel_mode=0)


def random_shade(r):
    return shade_case(r)


# ---------------------------------------------------------------------------------------------------------------- ShadowGlow::shadow_glow


def glow_ppc(e, case):
    p = Ppc(e, case)
    try:
        colours, grow, blur_value, ox, oy = case["colours"], case["grow"], case["blur"], case["ox"], case["oy"]
        glow = p.alloc(0x14)
        e.write(glow, bytes(colours))
        ws(e, glow + 4, [grow, fbits(blur_value), ox, oy])
        p.L.labels[glow] = GLOW_LABEL
        a_src, a_out = p.bitmap(case["src"], 1), p.bitmap(case["out"], 2)
        answer = p.call(SHADOW_GLOW, glow, a_src, a_out)[0]
        words = rs(e, glow, 5)
        state = [answer, words[0], T.s32(words[1]), words[2], T.s32(words[3]), T.s32(words[4])]
        return dict(state=state + p.dump_bitmap(a_src) + p.dump_bitmap(a_out), log=p.L.log)
    finally:
        p.done()


def glow_host(lib, case):
    description = list(case["colours"]) + [case["grow"], fbits(case["blur"]), case["ox"], case["oy"]]
    description += flat_bitmap(case["src"]) + flat_bitmap(case["out"])
    state, log = host_run(lib, "tpw_w84_shadow_glow", description)
    return dict(state=state, log=log)


GROWS = [0, 0, 1, -1, 2, -2, 3, -3, 5, -5, 100, -100]
GLOW_BLURS = [0.0, -0.0, 0.5, 1.0, 1.7, 2.5, 0.01, 4.0, -1e-30]


def glow_case(r, dims=None, grow=None, blur_value=None, none=None, offset=None):
    w, h = r.choice(DIMS) if dims is None else dims
    grow = r.choice(GROWS) if grow is None else grow
    blur_value = r.choice(GLOW_BLURS) if blur_value is None else blur_value
    ox, oy = safe_offset(r, w, h) if offset is None else offset
    none = r.random() < 0.1 if none is None else none
    bpp = r.choice((1, 4, 2))
    src = (w, h, bpp, None if none else rnd_data(r, w * h * bpp, r.choice((0, 1, 5, 6))))
    if not copy_is_safe(w, h, ox, oy):
        ox, oy = 0, 0
    return dict(colours=[r.randrange(256) for _ in range(4)], grow=grow, blur=f32(blur_value), ox=ox, oy=oy, src=src,
                out=dest_before(r, w, h))


def edge_glow():
    r = random.Random(8411)
    for grow in (0, 1, -1, 2, -2, 6, -6):
        for blur_value in (0.0, 1.0, 2.2):
            yield glow_case(r, (6, 5), grow, blur_value, False, (0, 0))
    yield glow_case(r, (4, 4), 0, 0.0, True, (0, 0))
    yield glow_case(r, (4, 4), 3, 1.0, True, (0, 0))
    for blur_value in (0.0, -0.0, 1e-30, 1.5):
        yield glow_case(r, (3, 3), 1, blur_value, False, (0, 0))
    for offset in [(1, 0), (0, 1), (-1, 0), (0, -1), (2, -1), (-2, 2), (1, 1), (-1, -1)]:
        yield glow_case(r, (6, 5), 0, 0.0, False, offset)
        yield glow_case(r, (6, 5), -1, 0.0, False, offset)
        yield glow_case(r, (6, 5), 1, 0.0, False, offset)
        yield glow_case(r, (6, 5), 0, 1.0, False, offset)


def random_glow(r):
    return glow_case(r)


# ---------------------------------------------------------------------------------------------------------------- ~ShadowGlow


def glow_dtor_ppc(e, case):
    p = Ppc(e, case)
    try:
        glow = p.alloc(0x14)
        p.L.labels[glow] = GLOW_LABEL
        answer = p.call(GLOW_DTOR, glow, case["flag"] & MASK)[0]
        return dict(state=[int(p.L.lab(glow) <= 0)], log=p.L.log, answer=int(answer == glow))
    finally:
        p.done()


def glow_dtor_host(lib, case):
    state, log = host_run(lib, "tpw_w84_glow_dtor", [case["flag"]])
    return dict(state=state, log=log, answer=1)


def edge_glow_dtor():
    for flag in FLAGS:
        yield dict(flag=flag & MASK)


def random_glow_dtor(r):
    return dict(flag=r.choice(FLAGS + [r.getrandbits(32), r.getrandbits(16)]) & MASK)


TESTS = [
    FnTest("erodebuffer_erode", erodebuffer_ppc, erodebuffer_host, edge=edge_erodebuffer, rnd=random_erodebuffer),
    FnTest("make_gaussian_filter_2d", gaussian_ppc, gaussian_host, edge=edge_gaussian, rnd=random_gaussian),
    FnTest("copy_offset_alpha", copy_ppc, copy_host, edge=edge_copy, rnd=random_copy),
    FnTest("dilate", morph_ppc(DILATE), morph_host("tpw_w84_dilate"), edge=edge_morph, rnd=random_morph),
    FnTest("erode", morph_ppc(ERODE), morph_host("tpw_w84_erode"), edge=edge_morph, rnd=random_morph),
    FnTest("blur", blur_ppc, blur_host, edge=edge_blur, rnd=random_blur),
    FnTest("PlasticShader::clear", shader_ppc(SHADER_CLEAR), shader_host("tpw_w84_shader_clear"), edge=edge_clear, rnd=random_clear),
    FnTest("PlasticShader::~PlasticShader", shader_ppc(SHADER_DTOR, True), shader_host("tpw_w84_shader_dtor", True), edge=edge_dtor,
           rnd=random_dtor),
    FnTest("PlasticShader::load_settings", load_ppc, load_host, edge=edge_load, rnd=random_load),
    FnTest("PlasticShader::bevel", bevel_ppc, bevel_host, edge=edge_bevel, rnd=random_bevel),
    FnTest("PlasticShader::blur_with_mask", blur_mask_ppc, blur_mask_host, edge=edge_blur_mask, rnd=random_blur_mask),
    FnTest("PlasticShader::plastic_shade", shade_ppc, shade_host, edge=edge_shade, rnd=random_shade),
    FnTest("ShadowGlow::shadow_glow", glow_ppc, glow_host, edge=edge_glow, rnd=random_glow),
    FnTest("ShadowGlow::~ShadowGlow", glow_dtor_ppc, glow_dtor_host, edge=edge_glow_dtor, rnd=random_glow_dtor),
]
