"""Differential tests for the twelve textured glyph writers (unit w45c), against libtpw_unit_FontRenderTextured.dylib
(tests/unit_FontRenderTextured.cpp), all in the ltms library ("ltms symbols shared"):

  src/ltms/TbTextured4BitFontRenderMethod.cpp          TbTextured4BitFontRenderMethod::DrawCharTo555 0x1000a1dc, To565 0x1000a6f4, To4444 0x1000aba0,
                                            To8888 0x1000b02c, ToBGRA 0x1000b4b8, ToAny 0x1000b944 (they blend the texel into the destination)
  src/ltms/TbTexturedNoBlend4BitFontRenderMethod.cpp  TbTexturedNoBlend4BitFontRenderMethod::DrawCharTo555 0x1000baf4, To565 0x1000bf28,
                                            To4444 0x1000c2e0, To8888 0x1000c718, ToBGRA 0x1000cb58, ToAny 0x1000cf98 (they copy it)
  include/TbFontRender.h                    (inline) TextureOf, TextureIsA8R8G8B8, QuotientBy15Signed and Unsigned, WalkTexturedGlyph:
                                            the loop the writers inline

Groups of tests:
1. Each of the twelve writers called by hand (through the object's VTABLE on the PowerPC side, so that the vtable slot and the class are
   told too) over a hand-built render area, record and parameters. The parameters hold the texture (a 32-bit area of the case's width,
   height and row length, random texels with every alpha, guard texels around it) at +0x14..+0x3b and its origin at +0x3c and +0x40;
   TbPixFormat32 (the PowerPC side's import and the host's global) is planted with the six words the case names. Destination pixels
   of 16 or 32 bits (a buffer of random cells, every cell compared as a value so that the byte order of the two sides does not
   matter; strides that are not a multiple of the pixel size on a uniformly filled buffer), the glyph's nybbles, the source rectangle,
   the point; guard bytes around the pixels and every byte outside the rectangle compared; the writer's answer (-1 for a texture
   whose format differs in any of the six words, then nothing is drawn); the point and the source rectangle afterwards; the log.
2. DrawChar of the render method (unit w45a) on an object picked from the table by (blend, 2) with type 4, style 2, over a font image
   and a render area of every pixel format the dispatcher knows, the texture in the parameters: the whole destination buffer, the
   answer and the drawn rectangle, and the log (the font's reference calls, the clip, the intersection).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

import test_FontRenderMethod as W

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_FontRenderTextured"

MINUS = 0xFFFFFFFF
FIRST_NEW = 100
LAB_FONT, LAB_AREA, LAB_PARAMS, LAB_OBJECT = 10, 20, 30, 50

SINIT, REGISTER_GLOBAL = 0x10002298, 0x1000F6DC
TABLE = 0x1001103C
DRAWCHAR = 0x10000D40

NAMES = ["TbOneColour4BitFontRenderMethod", "TbOneColourNoBlend4BitFontRenderMethod", "TbTextured4BitFontRenderMethod",
         "TbTexturedNoBlend4BitFontRenderMethod"]
OBJECTS = [0x10011744, 0x10011734, 0x10011724, 0x10011714]
SLOTS = ["To555", "To565", "To4444", "ToAny", "To8888", "ToBGRA"]
WRITERS = [[0x1000E10C, 0x1000E528, 0x1000E93C, 0x1000F558, 0x1000EDB4, 0x1000F184],
           [0x1000D17C, 0x1000D4A4, 0x1000D5CC, 0x1000DF64, 0x1000D908, 0x1000DC3C],
           [0x1000A1DC, 0x1000A6F4, 0x1000ABA0, 0x1000B944, 0x1000B02C, 0x1000B4B8],
           [0x1000BAF4, 0x1000BF28, 0x1000C2E0, 0x1000CF98, 0x1000C718, 0x1000CB58]]
ALL_WRITERS = [a for row in WRITERS for a in row]
REAL = W.REAL + ALL_WRITERS + [SINIT, W.NI_CURRENT]

# TbPixFormat32 (bullfrog 0x10073e88): the four masks, the depth, the palettised flag.
PIXFORMAT32 = (0xFF0000, 0xFF00, 0xFF, 0xFF000000, 32, 0)

# Pixel formats by slot, as DrawChar's dispatcher knows them (masks, depth, palettised flag).
AREA_BY_SLOT = {0: (0x7C00, 0x3E0, 0x1F, 0x8000, 16, 0), 1: (0xF800, 0x7E0, 0x1F, 0, 16, 0), 2: (0xF000, 0xF00, 0xF0, 0xF, 16, 0),
                3: (0xFF0000, 0xFF00, 0xFF, 0, 24, 0), 4: (0xFF0000, 0xFF00, 0xFF, 0xFF000000, 32, 0),
                5: (0xFF00, 0xFF0000, 0xFF000000, 0xFF, 32, 0)}
BPP_BY_SLOT = {0: 2, 1: 2, 2: 2, 3: 2, 4: 4, 5: 4}
GUARD = 32
SLACK = 4096      # room after the buffer (compared, it must stay zero), so that a writer that overruns shows as a difference and not as a crash
TEXGUARD = 64     # texels of random data before and after the texture's own (a writer that reads outside the texture shows as a difference)


def setup(runner):
    T.install_std_stubs(runner.emu)


def hostfn(lib, name, restype, *argtypes):
    fn = getattr(lib, name)
    fn.restype = restype
    fn.argtypes = list(argtypes)
    return fn


# ======================================================================= the PowerPC side: the objects and the fakes


def construct_objects(e):
    """Runs __sinit_TbFontRender_cpp (the only way the four objects get their vtables): the registration of the destructors with
    __register_global_object is recorded in e.registered, not in the case's log."""
    for address in OBJECTS:
        e.write(address, struct.pack(">I", 0xA5A5A5A5))
    e.registered = []
    T.call_ppc(e, SINIT, [])


def sinit_stub(e):
    # (the glue the stub replaces saves the TOC pointer in the caller's frame, where the caller restores it from)
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
    e.registered.append((e.r(3), e.r(4)))


def install_stubs(c):
    s = W.install_stubs(c)
    s.real.update(REAL)
    s.internal(REGISTER_GLOBAL, sinit_stub)
    return s


def label_objects(c):
    for cls, address in enumerate(OBJECTS):
        c.L.labels[address] = LAB_OBJECT + cls


def plant_pixformat(e, words):
    """TbPixFormat32 as the case wants it (the ltms library reaches it through its TOC import pointer)."""
    e.write(e.data_import("TbPixFormat32"), struct.pack(">4I2I", *[w & MINUS for w in words]))


# ======================================================================= destination buffers


def cell_values(r, bpp, count):
    """Random pixel values with the interesting ones over-represented."""
    special = [0, 0xFFFF, 0x7FFF, 0x8000, 0x0F0F, 0xF0F0, 0x1234] if bpp == 2 else [0, MINUS, 0xFF000000, 0x00FFFFFF, 0x00FF00FF, 0xFF00FF00, 0x12345678]
    return [r.choice(special) if r.random() < 0.4 else r.getrandbits(8 * bpp) for _ in range(count)]


def make_buffer(r, bpp, total, mode):
    """The destination bytes for both sides: from cell values (each side in its own byte order), or a uniform fill (the same bytes on
    both sides, for strides that are not a multiple of the pixel size, where one pixel's bytes straddle two cells)."""
    if mode == "uniform":
        fill = bytes([r.choice([0, 0xFF, 0x5A, 0xA5, 0x37, 0x80])]) * total
        return fill, fill
    cells = total // bpp
    values = cell_values(r, bpp, cells)
    fmt = "H" if bpp == 2 else "I"
    ppc = struct.pack(">%d%s" % (cells, fmt), *values) + bytes(total - cells * bpp)
    host = struct.pack("<%d%s" % (cells, fmt), *values) + bytes(total - cells * bpp)
    return ppc, host


def view_pixels(raw, bpp, offsets, end, grid):
    """The values of the pixels at `offsets` and the rest of the buffer with those bytes zeroed: as cell values for a buffer of
    cells (whose byte order differs between the sides), else as bytes (a uniform fill)."""
    fmt = end + ("H" if bpp == 2 else "I")
    pixels = [struct.unpack_from(fmt, raw, o)[0] for o in offsets]
    rest = bytearray(raw)
    for o in offsets:
        rest[o:o + bpp] = bytes(bpp)
    if grid:
        return pixels, view_cells(bytes(rest), bpp, end)
    return pixels, bytes(rest)


def view_cells(raw, bpp, end):
    cells = len(raw) // bpp
    return list(struct.unpack(end + ("%d%s" % (cells, "H" if bpp == 2 else "I")), raw[:cells * bpp])), raw[cells * bpp:]


# ======================================================================= textures


TEXEL_SPECIAL = [0, MINUS, 0xFF000000, 0x00FFFFFF, 0x00FF00FF, 0xFF00FF00, 0x80808080, 0x01010101, 0xFEFEFEFE, 0x00F8FCF8, 0xFFF8FCF8]


def make_texture(r, width, height, extra_stride=0, stride=None, fill=None):
    """A texture's texels as a list of words (guard texels, then `height` rows of the stride's length) and the row length in bytes.
    The words between rows (when the stride is longer than the width) and the guards are random too."""
    row_words = (stride // 4) if stride is not None else width + extra_stride
    row_words = max(row_words, 0)
    count = height * row_words + width + 2
    if fill is not None:
        words = [fill(i) for i in range(count)]
    else:
        words = [r.choice(TEXEL_SPECIAL) if r.random() < 0.25 else r.getrandbits(32) for _ in range(count)]
    before = [r.getrandbits(32) for _ in range(TEXGUARD)]
    after = [r.getrandbits(32) for _ in range(TEXGUARD)]
    return {"words": before + words + after, "stride": row_words * 4}


def texture_bytes(texture, end):
    return struct.pack(end + "%dI" % len(texture["words"]), *texture["words"])


def origin_for(r, x, y, mode=None):
    """A texture origin relative to the destination point: exactly at it, inside the glyph, before it, far past it (negative
    differences), far before it, or at the extremes of a 32-bit number."""
    def one(p):
        pick = mode or r.choice(["at", "near", "before", "past", "far", "wild", "zero"])
        if pick == "at":
            return p
        if pick == "near":
            return p + r.randint(-9, 9)
        if pick == "before":
            return p - r.randint(1, 300)
        if pick == "past":
            return p + r.randint(1, 300)
        if pick == "far":
            return p + r.choice([-1, 1]) * r.randint(1000, 100000)
        if pick == "wild":
            return r.choice([-0x80000000, 0x7FFFFFFF, -1, 0x40000000, -0x40000000, p - 0x7FFFFFFF, p + 0x7FFFFFFF])
        return 0
    return one(x), one(y)


# ======================================================================= group 1: the writers by hand


def pixel_offsets(case):
    x, y = case["geometry"][0], case["geometry"][1]
    rows, cols = max(case["geometry"][5] - case["geometry"][3], 0), max(case["geometry"][4] - case["geometry"][2], 0)
    bpp = case["bpp"]
    return [GUARD + (y + row) * case["stride"] + (x + col) * bpp for row in range(rows) for col in range(cols)]


def spec_of(case):
    """The texture's description both sides take: six format words, width, height, row length, origin x, origin y."""
    t = case["texture"]
    return list(t["format"]) + [t["width"], t["height"], t["stride"], t["origin"][0], t["origin"][1]]


def ppc_writer(e, case):
    c = W.Case(e, {}, {})
    label_objects(c)
    s = install_stubs(c)
    saved = s.activate(e)
    try:
        construct_objects(e)
        plant_pixformat(e, case["pixformat"])
        ppc, _ = case["buffers"]
        g = case["geometry"]
        dest = e.alloc(len(ppc) + SLACK, ppc + bytes(SLACK))
        data = e.alloc(len(case["glyph"]) + 8, case["glyph"] + b"\xa5" * 8)
        area = e.alloc(0x28, struct.pack(">6I", *case["area"]) + struct.pack(">IiiI", dest + GUARD, 0x7777, 0x6666, case["stride"] & MINUS))
        point = e.alloc(8, struct.pack(">ii", g[0], g[1]))
        source = e.alloc(16, struct.pack(">4i", *g[2:6]))
        raw = texture_bytes(case["texture"], ">")
        texels = e.alloc(len(raw), raw)
        t = case["texture"]
        params = e.alloc(0x50 + 32, b"\xa5" * (0x50 + 32))
        e.write(params + 0x14, struct.pack(">6I", *[w & MINUS for w in t["format"]]) + struct.pack(">I", texels + 4 * TEXGUARD) +
                struct.pack(">3I", t["width"] & MINUS, t["height"] & MINUS, t["stride"] & MINUS))
        e.write(params + 0x3C, struct.pack(">2I", t["origin"][0] & MINUS, t["origin"][1] & MINUS))
        record = e.alloc(0x18 + 8, struct.pack(">IIiII", point, data, g[6], source, params) + bytes([g[7] & 0xFF]) + b"\xa5" * 7)
        result = e.alloc(4, struct.pack(">I", 0x77777777))
        this = OBJECTS[case["cls"]]
        c.L.labels.update({area: LAB_AREA, params: LAB_PARAMS})
        vtable = e.u32(this)
        target = e.u32(e.u32(vtable + 0x10 + 4 * case["slot"]))
        T.call_ppc(e, target, [result, this, area, record])
        raw_dest = e.read(dest, len(ppc))
        slack = e.read(dest + len(ppc), SLACK)
        pixels, rest = view_pixels(raw_dest, case["bpp"], pixel_offsets(case), ">", case["mode"] == "grid")
        found = [(i, row.index(target)) for i, row in enumerate(WRITERS) if target in row]
        return {"result": T.s32(e.u32(result)), "target": list(found[0]) if found else [-1, -1],
                "pixels": pixels, "rest": rest, "geometry": list(struct.unpack(">4i", e.read(source, 16))) + list(struct.unpack(">2i", e.read(point, 8))),
                "slack": slack, "texture": e.read(texels, len(raw)), "log": c.L.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host_writer(lib, case):
    _, host = case["buffers"]
    CL.host_begin(lib, {}, {}, FIRST_NEW)
    fn = hostfn(lib, "tpw_ft_writer", C.c_int32, C.c_int32, C.c_int32, C.c_void_p, C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p,
                C.c_void_p, C.c_void_p)
    dest = (C.c_uint8 * (len(host) + SLACK)).from_buffer_copy(host + bytes(SLACK))
    glyph = (C.c_uint8 * (len(case["glyph"]) + 8)).from_buffer_copy(case["glyph"] + b"\xa5" * 8)
    areaWords = (C.c_uint32 * 6)(*case["area"])
    geometry = (C.c_int32 * 8)(*case["geometry"])
    raw = texture_bytes(case["texture"], "<")
    texels = (C.c_uint8 * len(raw)).from_buffer_copy(raw)
    spec = (C.c_int32 * 11)(*[T.s32(w & MINUS) for w in spec_of(case)])
    pixformat = (C.c_uint32 * 6)(*[w & MINUS for w in case["pixformat"]])
    result = fn(case["cls"], case["slot"], C.addressof(areaWords), case["stride"], C.addressof(dest) + GUARD, C.addressof(geometry),
                C.addressof(glyph), C.addressof(spec), C.addressof(texels) + 4 * TEXGUARD, C.addressof(pixformat), None)
    log = CL.host_end(lib)
    pixels, rest = view_pixels(bytes(dest)[:len(host)], case["bpp"], pixel_offsets(case), "<", case["mode"] == "grid")
    return {"result": result, "target": [case["cls"], case["slot"]], "pixels": pixels, "rest": rest,
            "geometry": list(geometry[2:6]) + list(geometry[0:2]), "slack": bytes(dest)[len(host):],
            "texture": bytes(raw_to_ppc_order(bytes(texels))), "log": log}


def raw_to_ppc_order(raw):
    """The host's texture bytes with every word swapped back, so that it compares with the PowerPC side's big-endian copy."""
    count = len(raw) // 4
    return struct.pack(">%dI" % count, *struct.unpack("<%dI" % count, raw))


def nybbles_needed(g):
    """The nybbles a writer reads: up to the end of the last row it draws."""
    left, top, right, bottom, width = g[2], g[3], g[4], g[5], g[7]
    if bottom <= top or right <= left:
        return left + top * width
    return (bottom - 1) * width + right


def data_for(r, kind, nyb):
    count = (nyb + 1) // 2 + 3
    if kind == "ones":
        return bytes([0xFF]) * count
    if kind == "zeros":
        return bytes(count)
    if kind == "ramp":
        return bytes(((2 * i) & 15) << 4 | ((2 * i + 1) & 15) for i in range(count))
    if kind == "alt":
        return bytes([0xF0, 0x0F]) * (count // 2 + 1)
    if kind == "low":
        return bytes(r.randrange(4) << 4 | r.randrange(4) for _ in range(count))
    if kind == "high":
        return bytes(r.randrange(8, 16) << 4 | r.randrange(8, 16) for _ in range(count))
    if kind == "odd":     # only odd nybbles (bit 0 set): the 555 writer without blending draws every pixel
        return bytes((r.randrange(8) * 2 + 1) << 4 | (r.randrange(8) * 2 + 1) for _ in range(count))
    if kind == "even":    # only even nybbles: the 555 writer without blending draws nothing
        return bytes((r.randrange(8) * 2) << 4 | (r.randrange(8) * 2) for _ in range(count))
    if kind.startswith("n"):   # a constant weight
        n = int(kind[1:])
        return bytes([n << 4 | n]) * count
    return bytes(r.getrandbits(8) for _ in range(count))


DATA_KINDS = ["random", "ones", "zeros", "ramp", "alt", "low", "high", "odd", "even"]


def writer_case(r, cls, slot, kind=None, rect=None, wide=None, odd=None, tex=None, origin=None, pixformat=None, format=None, big=False):
    bpp = BPP_BY_SLOT[slot]
    width = r.choice([1, 2, 3, 4, 5, 7, 8, 9, 12, 16, 17, 31]) if wide is None else wide
    if rect is None:
        left = r.randrange(width) if r.random() < 0.8 else 0
        right = r.randint(left, width)
        top = r.randrange(5)
        bottom = r.randint(top, top + 7)
    else:
        left, top, right, bottom = rect
    x, y = r.randrange(6), r.randrange(5)
    g = [x, y, left, top, right, bottom, (width + 1) >> 1, width]
    nyb = nybbles_needed(g)
    glyph = data_for(r, kind or r.choice(DATA_KINDS), nyb)
    rows, cols = max(bottom - top, 0), max(right - left, 0)
    unaligned = (r.random() < 0.06) if odd is None else odd
    row_pixels = x + cols + r.randrange(3)
    stride = row_pixels * bpp + (r.choice([1, 3, 5]) if unaligned else bpp * r.choice([0, 0, 1, 2]))
    total = GUARD + (y + rows + 1) * stride + GUARD + 8
    total += (-total) % 4
    # the texture
    if tex is None:
        tw = r.choice([1, 1, 2, 3, 4, 5, 6, 7, 7, 8, 13, 40]) if r.random() < 0.9 else r.randint(1, 100)
        th = r.choice([1, 1, 2, 3, 4, 5, 6, 7, 7, 8, 13, 40]) if r.random() < 0.9 else r.randint(1, 100)
    else:
        tw, th = tex
    stride_choice = r.random()
    extra = 0 if stride_choice < 0.7 else r.randint(1, 5)
    texture = make_texture(r, tw, th, extra_stride=extra)
    ox, oy = origin if origin is not None else origin_for(r, x, y)
    texture.update({"width": tw, "height": th, "origin": (ox, oy), "format": list(PIXFORMAT32) if format is None else list(format)})
    return {"cls": cls, "slot": slot, "bpp": bpp, "area": list(AREA_BY_SLOT[slot]), "stride": stride, "geometry": g, "glyph": glyph,
            "texture": texture, "pixformat": list(PIXFORMAT32) if pixformat is None else list(pixformat),
            "mode": "uniform" if unaligned else "grid", "buffers": make_buffer(r, bpp, total, "uniform" if unaligned else "grid")}


def wrong_formats():
    """The texture's format words, each of the six made different from TbPixFormat32 in turn (and a few other mismatches)."""
    std = list(PIXFORMAT32)
    for index in range(4):
        for delta in (0x1, 0x80, 0x80000000, 0xFF000000):
            bad = list(std)
            bad[index] ^= delta
            yield bad
    for depth in (0, 8, 16, 24, 31, 33, 64, 0xFFFFFFFF):
        bad = list(std)
        bad[4] = depth
        yield bad
    for pal in (1, 2, 0x100, 0x80000000, MINUS):
        bad = list(std)
        bad[5] = pal
        yield bad
    yield [std[1], std[0], std[2], std[3], std[4], std[5]]       # red and green swapped
    yield [std[2], std[1], std[0], std[3], std[4], std[5]]       # red and blue swapped (BGRA order)
    yield [std[0], std[1], std[2], 0, 32, 0]                      # no alpha mask (a8r8g8b8 without alpha)
    yield [0, 0, 0, 0, 0, 0]
    yield [0x7C00, 0x3E0, 0x1F, 0, 16, 0]



# The channels the blended writers weight, by slot: the texel byte (0 blue, 1 green, 2 red) and the destination field it is blended into
# (shift, bits). The 555 writer blends 5-5-5, the 565 one 5-6-5, the 4444 one 4-4-4, the 8888 one 8-8-8 and the BGRA one the three upper
# bytes (the first channel at 24, the second at 16, the third at 8).
BLEND_FIELDS = {0: {2: (10, 5), 1: (5, 5), 0: (0, 5)}, 1: {2: (11, 5), 1: (5, 6), 0: (0, 5)}, 2: {2: (8, 4), 1: (4, 4), 0: (0, 4)},
                4: {2: (16, 8), 1: (8, 8), 0: (0, 8)}, 5: {0: (24, 8), 1: (16, 8), 2: (8, 8)}}


def sweep_case(r, slot, byte, weight, half):
    """One channel swept over every pair (destination field value, texel value) at one weight: the destination field of row y holds
    y, the texture's byte in column x holds x scaled to the byte (a 128-column half of the texture per case), the other fields and
    bytes are random. Shows each rounding of the weighted quotient."""
    shift, bits = BLEND_FIELDS[slot][byte]
    bpp = BPP_BY_SLOT[slot]
    values = 1 << bits
    columns = min(values, 128)
    rows = values
    fmask = (values - 1) << shift
    stride = columns * bpp
    total = GUARD + (rows + 1) * stride + GUARD + 8
    total += (-total) % 4
    cells = total // bpp
    cell_list = cell_values(r, bpp, cells)
    base = GUARD // bpp
    for y in range(rows):
        for x in range(columns):
            i = base + y * (stride // bpp) + x
            cell_list[i] = (cell_list[i] & ~fmask) | (y << shift)
    fmt = "H" if bpp == 2 else "I"
    ppc = struct.pack(">%d%s" % (cells, fmt), *cell_list) + bytes(total - cells * bpp)
    host = struct.pack("<%d%s" % (cells, fmt), *cell_list) + bytes(total - cells * bpp)
    texels = []
    for x in range(values):
        word = r.getrandbits(32)
        scaled = (x << (8 - bits)) | (r.getrandbits(8 - bits) if bits < 8 else 0)
        word = (word & ~(0xFF << (8 * byte))) | (scaled << (8 * byte))
        texels.append(word)
    guard = [r.getrandbits(32) for _ in range(TEXGUARD)]
    texture = {"words": guard + texels + guard + [0, 0], "stride": values * 4, "width": values, "height": 1,
               "origin": (-128 * half, 0), "format": list(PIXFORMAT32)}
    g = [0, 0, 0, 0, columns, rows, (columns + 1) >> 1, columns]
    glyph = data_for(r, "n%d" % weight, nybbles_needed(g))
    return {"cls": 2, "slot": slot, "bpp": bpp, "area": list(AREA_BY_SLOT[slot]), "stride": stride, "geometry": g, "glyph": glyph,
            "texture": texture, "pixformat": list(PIXFORMAT32), "mode": "grid", "buffers": (ppc, host)}


def writer_edges(cls, slot):
    r = random.Random(4700 + 16 * cls + slot)
    # the blended writers: each channel swept over every pair of destination and texel values, at every weight
    if cls == 2 and slot != 3:
        for byte in (0, 1, 2):
            for weight in range(16):
                for half in ((0, 1) if BLEND_FIELDS[slot][byte][1] == 8 else (0,)):
                    yield sweep_case(r, slot, byte, weight, half)
    # every nybble weight alone (0 to 15) with random texels and destination, and every kind of data
    for n in range(16):
        yield writer_case(r, cls, slot, kind="n%d" % n)
        yield writer_case(r, cls, slot, kind="n%d" % n, tex=(r.randint(1, 3), r.randint(1, 3)))
    for kind in DATA_KINDS:
        for _ in range(4):
            yield writer_case(r, cls, slot, kind=kind)
    # the texture's size: every width and height from 1 to 7, with the origin at the point, before it and past it
    for tw in range(1, 8):
        for th in range(1, 8):
            for mode in ("at", "before", "past"):
                yield writer_case(r, cls, slot, kind=r.choice(["random", "ramp", "ones"]), tex=(tw, th), rect=(0, 0, r.randint(1, 9), r.randint(1, 9)),
                                  wide=9, origin=origin_for(r, r.randrange(6), r.randrange(5), mode))
    # the origin exactly at the destination point, at the point's neighbours, and at the extremes of a 32-bit number
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            case = writer_case(r, cls, slot, kind="ramp", tex=(3, 4), rect=(0, 0, 8, 8), wide=8)
            g = case["geometry"]
            case["texture"]["origin"] = (g[0] + dx, g[1] + dy)
            yield case
    for mode in ("wild", "far", "zero"):
        for _ in range(4):
            yield writer_case(r, cls, slot, tex=(r.randint(1, 7), r.randint(1, 7)), origin=origin_for(r, r.randrange(6), r.randrange(5), mode))
    # a texture of one texel, of one column, of one row; a glyph much wider and higher than the texture
    for tex in ((1, 1), (1, 5), (5, 1), (2, 2)):
        yield writer_case(r, cls, slot, kind="random", tex=tex, rect=(0, 0, 31, 7), wide=31)
        yield writer_case(r, cls, slot, kind="ones", tex=tex, rect=(1, 1, 30, 6), wide=31)
    # a texture that is wider than the glyph (no wrap), a texture with a longer row than its width, a row length of 0 and of one texel
    for tex in ((40, 40), (100, 3), (3, 100)):
        yield writer_case(r, cls, slot, kind="random", tex=tex)
    for stride in (0, 4, 8, 12):
        case = writer_case(r, cls, slot, kind="ones", tex=(3, 3))
        words = case["texture"]["words"]
        case["texture"]["stride"] = stride
        yield case
    # the six format words each wrong in turn (the writer answers -1 and draws nothing), also with an empty rectangle
    for bad in wrong_formats():
        yield writer_case(r, cls, slot, kind="ones", format=bad, tex=(3, 3), rect=(0, 0, 4, 4), wide=4)
        yield writer_case(r, cls, slot, kind="random", format=bad)
    # TbPixFormat32 itself planted differently (the palettised flag set, so a texture with any non-zero flag is the same format; a texture with
    # flag 0 is not), and a plain mismatch of the planted depth
    for planted in ([PIXFORMAT32[0], PIXFORMAT32[1], PIXFORMAT32[2], PIXFORMAT32[3], 32, 1],
                    [PIXFORMAT32[0], PIXFORMAT32[1], PIXFORMAT32[2], PIXFORMAT32[3], 32, 0x40],
                    [0x7C00, 0x3E0, 0x1F, 0, 16, 0], [0, 0, 0, 0, 8, 1]):
        for pal in (0, 1, 7, MINUS):
            fmt = list(planted)
            fmt[5] = pal
            yield writer_case(r, cls, slot, kind="ones", pixformat=planted, format=fmt, tex=(3, 3), rect=(0, 0, 4, 4), wide=4)
        yield writer_case(r, cls, slot, kind="ones", pixformat=planted, format=planted, tex=(3, 3), rect=(0, 0, 4, 4), wide=4)
    # the geometry swept: glyph widths, the left and right edges, the top and bottom rows (odd and even skips, empty rectangles)
    sweep = [(w, l, rt, t, b) for w in (1, 2, 3, 4, 5) for l in range(w) for rt in range(l, w + 1) for t in (0, 1) for b in (t, t + 1, t + 3)]
    for w, l, rt, t, b in r.sample(sweep, min(len(sweep), 90)):
        yield writer_case(r, cls, slot, kind="random", rect=(l, t, rt, b), wide=w)
    # empty rectangles (zero width, zero height)
    for rect in ((2, 1, 2, 4), (0, 3, 5, 3), (1, 1, 1, 1)):
        yield writer_case(r, cls, slot, kind="random", rect=rect, wide=5)
    # strides that are not a multiple of the pixel size
    for _ in range(12):
        yield writer_case(r, cls, slot, odd=True)
    # rectangles of 200 and more pixels, so that the texture wraps many times
    for width in (200, 250, 255):
        yield writer_case(r, cls, slot, kind="random", rect=(0, 0, width, 2), wide=width, tex=(r.randint(1, 7), r.randint(1, 7)))
        yield writer_case(r, cls, slot, kind="ramp", rect=(3, 1, width, 3), wide=width, tex=(r.randint(1, 7), r.randint(1, 7)))


def make_writer_test(cls, slot):
    name = "%s::DrawChar%s" % (NAMES[cls], SLOTS[slot])
    return FnTest(name, ppc_writer, host_writer, edge=lambda: list(writer_edges(cls, slot)), rnd=lambda r: writer_case(r, cls, slot))


# ======================================================================= group 2: DrawChar through the table


def ppc_drawchar(e, case):
    glyphs = case["glyphs"]
    image, data_at = W.build_image(glyphs, ">")
    c = W.Case(e, {}, {})
    label_objects(c)
    p = W.persist(e)
    s = install_stubs(c)
    ppc, _ = case["buffers"]
    old_scratch = e.heap_in_scratch
    e.heap_in_scratch = True
    saved = s.activate(e)
    try:
        construct_objects(e)
        plant_pixformat(e, case["pixformat"])
        font_addr = e.alloc(0x20, bytes(0x20))
        image_addr = e.alloc(len(image) + 128, image + bytes(128))
        e.write(font_addr, struct.pack(">IiIIII", p.font_vt, 1, image_addr, image_addr, image_addr + 8, 0))
        dest = e.alloc(len(ppc) + SLACK, ppc + bytes(SLACK))
        t = case["texture"]
        raw = texture_bytes(t, ">")
        texels = e.alloc(len(raw), raw)
        params = e.alloc(0x50 + 32, b"\xa5" * (0x50 + 32))
        e.write(params + 4, struct.pack(">I", font_addr))
        e.write(params + 0xC, struct.pack(">ii", case["style"], case["blend"]))
        e.write(params + 0x14, struct.pack(">6I", *[w & MINUS for w in t["format"]]) + struct.pack(">I", texels + 4 * TEXGUARD) +
                struct.pack(">3I", t["width"] & MINUS, t["height"] & MINUS, t["stride"] & MINUS))
        e.write(params + 0x3C, struct.pack(">2I", t["origin"][0] & MINUS, t["origin"][1] & MINUS))
        area = e.alloc(0x28 + 32, b"\xa5" * (0x28 + 32))
        e.write(area, struct.pack(">6I", *case["area"]) + struct.pack(">IiiI", dest + GUARD, 0x7777, 0x6666, case["stride"] & MINUS))
        clip = e.alloc(16, struct.pack(">4i", *case["clip"]))
        at = e.alloc(8, struct.pack(">ii", case["x"], case["y"]))
        drawn = e.alloc(16, struct.pack(">4i", -1, -2, -3, -4))
        result = e.alloc(4, struct.pack(">I", 0x77777777))
        c.L.labels.update({font_addr: LAB_FONT, area: LAB_AREA, params: LAB_PARAMS})
        c.clip_addr = clip
        e.write(e.data_import("_lbpCurrentRA"), struct.pack(">I", area))
        block = e.alloc(0x800, bytes(0x800))
        e.write(W.SINGLETON_MADE, b"\1")
        e.write(W.SINGLETON_SIZE, struct.pack(">I", 0x800))
        e.write(W.SINGLETON_BLOCK, struct.pack(">I", block))
        e.alloc_log.clear()
        method = e.u32(TABLE + 4 * ((case["blend"] * 5 + 4) * 3 + case["style"]))
        T.call_ppc(e, DRAWCHAR, [result, method, at, case["glyph"], drawn, params])
        cells, tail = view_cells(e.read(dest, len(ppc)), case["bpp"], ">")
        slack = e.read(dest + len(ppc), SLACK)
        return {"slack": slack, "result": W.s32(e.u32(result)), "drawn": list(struct.unpack(">4i", e.read(drawn, 16))), "point": list(struct.unpack(">ii", e.read(at, 8))),
                "cells": cells, "tail": tail, "texture": e.read(texels, len(raw)), "log": c.L.log}
    finally:
        CL.Stubs.deactivate(e, saved)
        e.heap_in_scratch = old_scratch


def host_drawchar(lib, case):
    image, data_at = W.build_image(case["glyphs"], "<")
    _, host = case["buffers"]
    fn = hostfn(lib, "tpw_ft_drawchar", C.c_int32, C.c_char_p, C.c_int32, C.c_void_p, C.c_int32, C.c_void_p, C.c_void_p, C.c_int32,
                C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p)
    CL.host_begin(lib, {}, {}, FIRST_NEW)
    dest = (C.c_uint8 * (len(host) + SLACK)).from_buffer_copy(host + bytes(SLACK))
    areaWords = (C.c_uint32 * 6)(*case["area"])
    clip = (C.c_int32 * 4)(*case["clip"])
    raw = texture_bytes(case["texture"], "<")
    texels = (C.c_uint8 * len(raw)).from_buffer_copy(raw)
    spec = (C.c_int32 * 11)(*[T.s32(w & MINUS) for w in spec_of(case)])
    pixformat = (C.c_uint32 * 6)(*[w & MINUS for w in case["pixformat"]])
    out = (C.c_int64 * 16)()
    fn(image, len(image), C.addressof(areaWords), case["stride"], C.addressof(dest) + GUARD, C.addressof(clip), case["x"], case["y"], case["glyph"],
       case["blend"], case["style"], C.addressof(spec), C.addressof(texels) + 4 * TEXGUARD, C.addressof(pixformat), C.addressof(out))
    log = CL.host_end(lib)
    cells, tail = view_cells(bytes(dest)[:len(host)], case["bpp"], "<")
    return {"slack": bytes(dest)[len(host):], "result": out[0], "drawn": list(out[1:5]), "point": [case["x"], case["y"]], "cells": cells, "tail": tail,
            "texture": raw_to_ppc_order(bytes(texels)), "log": log}


def random_drawchar(r, fmt=None, blend=None, kind=None, format=None):
    for _ in range(100):
        fmt_ = fmt if fmt is not None else (r.choice(W.FORMATS) if r.random() < 0.9 else r.choice(W.OTHER_FORMATS))
        glyphs = [W.make_glyph(r, kind=kind, width=r.choice([1, 2, 3, 5, 8, 9, 12]), height=r.choice([1, 2, 4, 7])) for _ in range(r.randint(1, 3))]
        glyphs.append(W.make_glyph(r, kind=kind, width=3, height=2))
        glyph = r.randrange(len(glyphs) - 1)
        case = {"glyphs": glyphs, "glyph": glyph, "state": 1, "size": 0x800, "area": list(fmt_)}
        g = glyphs[glyph]
        # (a glyph that makes the decode buffer grow would leave a new, uninitialised block to the writer)
        if not W.feasible(case) or (g["flags"] & 3 and W.sx32(g["size"]) > 0x800):
            continue
        width, height = r.randint(20, 40), r.randint(12, 24)
        bpp = fmt_[4] // 8 if fmt_[4] in (16, 32) else 4
        stride = width * bpp + bpp * r.choice([0, 0, 1, 3])
        x, y = r.randint(-4, width + 2), r.randint(-4, height + 2)
        clip = [r.choice([0, 0, 2, 5]), r.choice([0, 0, 1, 4]), r.choice([width, width - 3, 10]), r.choice([height, height - 2, 8])]
        total = GUARD + (height + 1) * stride + GUARD
        total += (-total) % 4
        tw, th = r.randint(1, 7), r.randint(1, 7)
        texture = make_texture(r, tw, th)
        ox, oy = origin_for(r, x, y)
        texture.update({"width": tw, "height": th, "origin": (ox, oy), "format": list(PIXFORMAT32) if format is None else list(format)})
        case.update({"x": x, "y": y, "clip": clip, "bpp": bpp, "stride": stride, "buffers": make_buffer(r, bpp, total, "grid"),
                     "blend": r.randrange(2) if blend is None else blend, "style": 2, "texture": texture, "pixformat": list(PIXFORMAT32)})
        return case
    raise RuntimeError("no feasible case")


def drawchar_edges():
    r = random.Random(4710)
    for fmt in W.FORMATS + W.OTHER_FORMATS[:3]:
        for blend in (0, 1):
            for kind in ("raw", "rle", "bits", "both"):
                yield random_drawchar(r, fmt=fmt, blend=blend, kind=kind)
    for blend in (0, 1):
        for fmt in W.FORMATS:
            for bad in list(wrong_formats())[:8]:
                yield random_drawchar(r, fmt=fmt, blend=blend, format=bad)
    for _ in range(300):
        yield random_drawchar(r)


TESTS = [make_writer_test(cls, slot) for cls in (3, 2) for slot in (0, 1, 2, 4, 5, 3)]
TESTS += [FnTest("Tb4BitFontRenderMethod::DrawChar through gFontRenderMethods[blend][4][2] (the textured classes)", ppc_drawchar, host_drawchar,
                 edge=lambda: list(drawchar_edges()), rnd=random_drawchar)]
