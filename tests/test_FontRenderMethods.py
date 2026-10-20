"""Differential tests for the four 4-bit font render method objects and the one-colour glyph writers (unit w45b), against
libtpw_unit_FontRenderMethods.dylib (tests/unit_FontRenderMethods.cpp), all in the ltms library ("ltms symbols shared"):

  src/ltms_fontrender_onecolour_noblend.cpp  TbOneColourNoBlend4BitFontRenderMethod::DrawCharTo555 0x1000d17c, To565 0x1000d4a4,
                                             To4444 0x1000d5cc, To8888 0x1000d908, ToBGRA 0x1000dc3c, ToAny 0x1000df64
  src/ltms_fontrender_onecolour.cpp          TbOneColour4BitFontRenderMethod::DrawCharTo555 0x1000e10c, To565 0x1000e528,
                                             To4444 0x1000e93c, To8888 0x1000edb4, ToBGRA 0x1000f184, ToAny 0x1000f558
  src/ltms_fontrender_methods.cpp            the four classes' constructors (0x1000dff0, 0x1000d050, 0x1000a0c0, 0x1000b9c8) and
                                             destructors (0x1000e068, 0x1000d0d0, 0x1000a138, 0x1000ba48), SetWriteZero 0x1000d020, the
                                             four objects and the table gFontRenderMethods; the static initialiser
                                             __sinit_TbFontRender_cpp 0x10002298 (the host's equivalent is the order of the global
                                             definitions)
  include/TbFontRender.h, TbFontBits.h       (inline) WalkOneColourGlyph and FourBit::SkipNybbles, the loop the writers inline

Groups of tests:
1. Each of the twelve real writers called by hand (through the object's vtable on the PowerPC side, so that the vtable slot and the
   class are told too) over a hand-built render area, record and parameters: destination pixels of 16 or 32 bits (a buffer of
   random cells, every cell compared as a value so that the byte order of the two sides does not matter; strides that are not a
   multiple of the pixel size on a uniformly filled buffer), the colour's five bytes, the glyph's nybbles, the source rectangle,
   the point; guard bytes around the pixels and every byte outside the rectangle compared; the writer's answer; the point and
   the source rectangle afterwards. The two textured classes' writers are untranslated: a case there shows only that the right
   fake is reached (the log of the fakes, which are recorded the same way on both sides) and that nothing is drawn.
2. DrawChar of the render method (unit w45a) on an object picked from the table by (blend, style) with type 4, over a font image
   and a render area of every pixel format the dispatcher knows: the whole destination buffer, the answer and the drawn rectangle,
   and the log (the font's reference calls, the clip, the intersection, the textured fakes).
3. The objects: constructors (the class the vtable word shows), destructors with the hidden flag, the table's 30 words, the
   static initialiser (objects and registrations), SetWriteZero.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

import test_FontRenderMethod as W

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_FontRenderMethods"

MINUS = 0xFFFFFFFF
FIRST_NEW = 100
LAB_FONT, LAB_AREA, LAB_PARAMS, LAB_OBJECT = 10, 20, 30, 50

SINIT, REGISTER_GLOBAL, SET_WRITE_ZERO = 0x10002298, 0x1000F6DC, 0x1000D020
WRITE_ZERO_BYTE = 0x10011768
TABLE = 0x1001103C
DRAWCHAR = 0x10000D40
CURRENT = 0x1000A68C
INITIALISE = 0x100020E4

# The four classes in the order of the host's class numbers: one colour, one colour without blending, textured, textured without
# blending.
NAMES = ["TbOneColour4BitFontRenderMethod", "TbOneColourNoBlend4BitFontRenderMethod", "TbTextured4BitFontRenderMethod",
         "TbTexturedNoBlend4BitFontRenderMethod"]
SHORT = ["OneColour", "OneColourNoBlend", "Textured", "TexturedNoBlend"]
OBJECTS = [0x10011744, 0x10011734, 0x10011724, 0x10011714]
VTABLES = [0x100116D4, 0x10011674, 0x100115B4, 0x10011614]
CTORS = [0x1000DFF0, 0x1000D050, 0x1000A0C0, 0x1000B9C8]
DTORS = [0x1000E068, 0x1000D0D0, 0x1000A138, 0x1000BA48]
VTABLE_BASE = 0x1001102C
# Slot order of the vtable (+0x10 ..): 555, 565, 4444, Any, 8888, BGRA.
SLOTS = ["To555", "To565", "To4444", "ToAny", "To8888", "ToBGRA"]
WRITERS = [[0x1000E10C, 0x1000E528, 0x1000E93C, 0x1000F558, 0x1000EDB4, 0x1000F184],
           [0x1000D17C, 0x1000D4A4, 0x1000D5CC, 0x1000DF64, 0x1000D908, 0x1000DC3C],
           [0x1000A1DC, 0x1000A6F4, 0x1000ABA0, 0x1000B944, 0x1000B02C, 0x1000B4B8],
           [0x1000BAF4, 0x1000BF28, 0x1000C2E0, 0x1000CF98, 0x1000C718, 0x1000CB58]]
REAL_WRITERS = [a for row in WRITERS[:2] for a in row]
TEXTURED_WRITERS = {a: (cls, slot) for cls in (2, 3) for slot, a in enumerate(WRITERS[cls])}
REAL = W.REAL + REAL_WRITERS + CTORS + DTORS + [SINIT, SET_WRITE_ZERO, CURRENT, INITIALISE]

# Pixel formats by slot, as DrawChar's dispatcher knows them (masks, depth, palettised flag).
AREA_BY_SLOT = {0: (0x7C00, 0x3E0, 0x1F, 0x8000, 16, 0), 1: (0xF800, 0x7E0, 0x1F, 0, 16, 0), 2: (0xF000, 0xF00, 0xF0, 0xF, 16, 0),
                3: (0xFF0000, 0xFF00, 0xFF, 0, 24, 0), 4: (0xFF0000, 0xFF00, 0xFF, 0xFF000000, 32, 0),
                5: (0xFF00, 0xFF0000, 0xFF000000, 0xFF, 32, 0)}
BPP_BY_SLOT = {0: 2, 1: 2, 2: 2, 3: 2, 4: 4, 5: 4}
GUARD = 32
SLACK = 4096      # room after the buffer (compared, it must stay zero), so that a writer that overruns shows as a difference and not as a crash


def setup(runner):
    T.install_std_stubs(runner.emu)


def hostfn(lib, name, restype, *argtypes):
    fn = getattr(lib, name)
    fn.restype = restype
    fn.argtypes = list(argtypes)
    return fn


# ======================================================================= the PowerPC side: the objects and the fakes


def construct_objects(e, c):
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


def textured_stub(address):
    cls, slot = TEXTURED_WRITERS[address]

    def stub(e):
        c = e.frc
        result, this, area, record = e.r(3), e.r(4), e.r(5), e.r(6)
        point, per_row, source, params = e.u32(record), e.s32(record + 8), e.u32(record + 12), e.u32(record + 16)
        width = e.read(record + 0x14, 1)[0]
        px, py = struct.unpack(">ii", e.read(point, 8))
        src = struct.unpack(">4i", e.read(source, 16))
        c.L.rec(SHORT[cls] and (("Textured." if cls == 2 else "TexturedNoBlend.") + SLOTS[slot]),
                (c.L.lab(this), c.L.lab(area), px, py, per_row) + src + (c.L.lab(params), width))
        e.write(result, struct.pack(">i", T.s32(T.u32(c.L.ret("Writer", 0)))))
    return stub


def install_stubs(c):
    s = W.install_stubs(c)
    s.real.update(REAL)
    for address in TEXTURED_WRITERS:
        s.internal(address, textured_stub(address))
    s.internal(REGISTER_GLOBAL, sinit_stub)
    return s


def label_objects(c):
    for cls, address in enumerate(OBJECTS):
        c.L.labels[address] = LAB_OBJECT + cls


# ======================================================================= destination buffers


def cell_values(r, bpp, count):
    """Random pixel values with the interesting ones over-represented."""
    special = [0, 0xFFFF, 0x7FFF, 0x8000, 0x0F0F, 0xF0F0, 0x1234] if bpp == 2 else [0, MINUS, 0xFF000000, 0x00FFFFFF, 0x00FF00FF, 0xFF00FF00, 0x12345678]
    return [r.choice(special) if r.random() < 0.4 else r.getrandbits(8 * bpp) for _ in range(count)]


def make_buffer(r, bpp, total, mode):
    """The destination bytes for both sides: ('grid', ppc bytes, host bytes) from cell values, or a uniform fill (the same bytes on
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


# ======================================================================= group 1: the writers by hand


def rect_counts(case):
    rows, cols = case["geometry"][5] - case["geometry"][3], case["geometry"][4] - case["geometry"][2]
    if case["cls"] == 1 and case["slot"] == 1:      # the 565 writer without blending counts bytes
        return rows & 0xFF, cols & 0xFF
    return max(rows, 0), max(cols, 0)


def pixel_offsets(case):
    x, y = case["geometry"][0], case["geometry"][1]
    rows, cols = rect_counts(case)
    bpp = case["bpp"]
    return [GUARD + (y + row) * case["stride"] + (x + col) * bpp for row in range(rows) for col in range(cols)]


def ppc_writer(e, case):
    c = W.Case(e, {}, {"Writer": case["writer"]})
    label_objects(c)
    s = install_stubs(c)
    saved = s.activate(e)
    try:
        construct_objects(e, c)
        ppc, _ = case["buffers"]
        g = case["geometry"]
        dest = e.alloc(len(ppc) + SLACK, ppc + bytes(SLACK))
        data = e.alloc(len(case["glyph"]) + 8, case["glyph"] + b"\xa5" * 8)
        area = e.alloc(0x28, struct.pack(">6I", *case["area"]) + struct.pack(">IiiI", dest + GUARD, 0x7777, 0x6666, case["stride"] & MINUS))
        point = e.alloc(8, struct.pack(">ii", g[0], g[1]))
        source = e.alloc(16, struct.pack(">4i", *g[2:6]))
        params = e.alloc(0x50 + 32, b"\xa5" * (0x50 + 32))
        e.write(params + 0x44, bytes(case["colour"]))
        record = e.alloc(0x18 + 8, struct.pack(">IIiII", point, data, g[6], source, params) + bytes([g[7] & 0xFF]) + b"\xa5" * 7)
        result = e.alloc(4, struct.pack(">I", 0x77777777))
        this = OBJECTS[case["cls"]]
        c.L.labels.update({area: LAB_AREA, params: LAB_PARAMS})
        vtable = e.u32(this)
        target = e.u32(e.u32(vtable + 0x10 + 4 * case["slot"]))
        T.call_ppc(e, target, [result, this, area, record])
        raw = e.read(dest, len(ppc))
        slack = e.read(dest + len(ppc), SLACK)
        pixels, rest = view_pixels(raw, case["bpp"], pixel_offsets(case), ">", case["mode"] == "grid")
        found = [(i, row.index(target)) for i, row in enumerate(WRITERS) if target in row]
        return {"result": T.s32(e.u32(result)), "target": list(found[0]) if found else [-1, -1],
                "pixels": pixels, "rest": rest, "geometry": list(struct.unpack(">4i", e.read(source, 16))) + list(struct.unpack(">2i", e.read(point, 8))),
                "slack": slack, "log": c.L.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host_writer(lib, case):
    _, host = case["buffers"]
    CL.host_begin(lib, {}, {"Writer": case["writer"]}, FIRST_NEW)
    fn = hostfn(lib, "tpw_fw_writer", C.c_int32, C.c_int32, C.c_int32, C.c_void_p, C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p)
    dest = (C.c_uint8 * (len(host) + SLACK)).from_buffer_copy(host + bytes(SLACK))
    glyph = (C.c_uint8 * (len(case["glyph"]) + 8)).from_buffer_copy(case["glyph"] + b"\xa5" * 8)
    areaWords = (C.c_uint32 * 6)(*case["area"])
    geometry = (C.c_int32 * 8)(*case["geometry"])
    colour = (C.c_uint8 * 5)(*case["colour"])
    result = fn(case["cls"], case["slot"], C.addressof(areaWords), case["stride"], C.addressof(dest) + GUARD, C.addressof(geometry),
                C.addressof(glyph), C.addressof(colour))
    log = CL.host_end(lib)
    pixels, rest = view_pixels(bytes(dest)[:len(host)], case["bpp"], pixel_offsets(case), "<", case["mode"] == "grid")
    # (the host's class numbers are those of OBJECTS; "target" is the PowerPC side's vtable slot, a class number the host knows by
    # construction)
    return {"result": result, "target": [case["cls"], case["slot"]], "pixels": pixels, "rest": rest,
            "geometry": list(geometry[2:6]) + list(geometry[0:2]), "slack": bytes(dest)[len(host):], "log": log}


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
    return bytes(r.getrandbits(8) for _ in range(count))


DATA_KINDS = ["random", "ones", "zeros", "ramp", "alt", "low", "high"]
ALPHAS = [0, 1, 2, 16, 17, 85, 100, 127, 128, 170, 200, 254, 255]


def writer_case(r, cls, slot, alpha=None, kind=None, rect=None, colour=None, wide=None, odd=None):
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
    if colour is None:
        pick = lambda: r.choice([0, 255, 0xF8, 0xFC, 0xF0, 0x07, 0x80, r.getrandbits(8), r.getrandbits(8)])
        colour = [r.choice(ALPHAS) if alpha is None else alpha, pick(), pick(), pick(), r.getrandbits(8)]
    nyb = nybbles_needed(g)
    glyph = data_for(r, kind or r.choice(DATA_KINDS), nyb)
    rows, cols = max(bottom - top, 0), max(right - left, 0)
    if cls == 1 and slot == 1:
        rows, cols = rows & 0xFF, cols & 0xFF
    unaligned = (r.random() < 0.06) if odd is None else odd
    row_pixels = x + cols + r.randrange(3)
    stride = row_pixels * bpp + (r.choice([1, 3, 5]) if unaligned else bpp * r.choice([0, 0, 1, 2]))
    total = GUARD + (y + rows + 1) * stride + GUARD + 8
    total += (-total) % 4
    return {"cls": cls, "slot": slot, "bpp": bpp, "area": list(AREA_BY_SLOT[slot]), "stride": stride, "geometry": g, "glyph": glyph,
            "colour": colour, "writer": [r.choice([0, -1, 7, -9, 0x7FFFFFFF]) for _ in range(3)],
            "mode": "uniform" if unaligned else "grid", "buffers": make_buffer(r, bpp, total, "uniform" if unaligned else "grid")}


def writer_edges(cls, slot):
    r = random.Random(4600 + 16 * cls + slot)
    # every alpha with every kind of data, the colour channels and the geometry random
    for alpha in ALPHAS:
        for kind in DATA_KINDS:
            yield writer_case(r, cls, slot, alpha=alpha, kind=kind, rect=None)
    # the colour's channels at their extremes, with alpha 255 and a lower one
    for alpha in (255, 128, 1):
        for rgb in ([0, 0, 0], [255, 255, 255], [255, 0, 0], [0, 255, 0], [0, 0, 255], [0xF8, 0xFC, 0xF0], [7, 3, 15], [0x80, 0x40, 0x20]):
            for kind in ("ones", "ramp", "random"):
                yield writer_case(r, cls, slot, kind=kind, colour=[alpha] + rgb + [r.getrandbits(8)])
    # the geometry swept: glyph widths, the left and right edges, the top and bottom rows (odd and even skips, empty rectangles)
    sweep = [(w, l, rt, t, b) for w in (1, 2, 3, 4, 5) for l in range(w) for rt in range(l, w + 1) for t in (0, 1) for b in (t, t + 1, t + 3)]
    for w, l, rt, t, b in r.sample(sweep, min(len(sweep), 90)):
        yield writer_case(r, cls, slot, alpha=r.choice([255, 255, 200, 64]), kind="random", rect=(l, t, rt, b), wide=w)
    # strides that are not a multiple of the pixel size
    for _ in range(12):
        yield writer_case(r, cls, slot, odd=True)
    # rectangles of 256 and more (the 565 writer without blending counts the low byte of the width and the height)
    if cls == 1 and slot == 1:
        for width in (255, 256, 257, 300, 511, 512):
            for height in (1, 2, 255, 256, 257, 258):
                yield writer_case(r, cls, slot, rect=(0, 0, width, height), wide=8)
    else:
        for width in (200, 255):
            yield writer_case(r, cls, slot, alpha=255, rect=(0, 0, width, 2), wide=width)
            yield writer_case(r, cls, slot, alpha=100, rect=(3, 1, width, 3), wide=width)


def make_writer_test(cls, slot):
    name = "%s::DrawChar%s" % (NAMES[cls], SLOTS[slot])
    return FnTest(name, ppc_writer, host_writer, edge=lambda: list(writer_edges(cls, slot)), rnd=lambda r: writer_case(r, cls, slot))


# ======================================================================= group 2: DrawChar through the table


def ppc_drawchar(e, case):
    glyphs = case["glyphs"]
    image, data_at = W.build_image(glyphs, ">")
    c = W.Case(e, {}, {"Writer": case["writer"]})
    label_objects(c)
    p = W.persist(e)
    s = install_stubs(c)
    ppc, _ = case["buffers"]
    old_scratch = e.heap_in_scratch
    e.heap_in_scratch = True
    saved = s.activate(e)
    try:
        construct_objects(e, c)
        font_addr = e.alloc(0x20, bytes(0x20))
        image_addr = e.alloc(len(image) + 128, image + bytes(128))
        e.write(font_addr, struct.pack(">IiIIII", p.font_vt, 1, image_addr, image_addr, image_addr + 8, 0))
        dest = e.alloc(len(ppc) + SLACK, ppc + bytes(SLACK))
        params = e.alloc(0x50 + 32, b"\xa5" * (0x50 + 32))
        e.write(params + 4, struct.pack(">I", font_addr))
        e.write(params + 0xC, struct.pack(">ii", case["style"], case["blend"]))
        e.write(params + 0x44, bytes(case["colour"]))
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
                "cells": cells, "tail": tail, "log": c.L.log}
    finally:
        CL.Stubs.deactivate(e, saved)
        e.heap_in_scratch = old_scratch


def host_drawchar(lib, case):
    image, data_at = W.build_image(case["glyphs"], "<")
    _, host = case["buffers"]
    fn = hostfn(lib, "tpw_fw_drawchar", C.c_int32, C.c_char_p, C.c_int32, C.c_void_p, C.c_int32, C.c_void_p, C.c_void_p, C.c_int32,
                C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_void_p, C.c_void_p)
    CL.host_begin(lib, {}, {"Writer": case["writer"]}, FIRST_NEW)
    dest = (C.c_uint8 * (len(host) + SLACK)).from_buffer_copy(host + bytes(SLACK))
    areaWords = (C.c_uint32 * 6)(*case["area"])
    clip = (C.c_int32 * 4)(*case["clip"])
    colour = (C.c_uint8 * 5)(*case["colour"])
    out = (C.c_int64 * 16)()
    fn(image, len(image), C.addressof(areaWords), case["stride"], C.addressof(dest) + GUARD, C.addressof(clip), case["x"], case["y"], case["glyph"],
       case["blend"], case["style"], C.addressof(colour), C.addressof(out))
    log = CL.host_end(lib)
    cells, tail = view_cells(bytes(dest)[:len(host)], case["bpp"], "<")
    return {"slack": bytes(dest)[len(host):], "result": out[0], "drawn": list(out[1:5]), "point": [case["x"], case["y"]], "cells": cells, "tail": tail, "log": log}


def random_drawchar(r, fmt=None, blend=None, style=None, kind=None):
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
        case.update({"x": x, "y": y, "clip": clip, "bpp": bpp, "stride": stride, "buffers": make_buffer(r, bpp, total, "grid"),
                     "colour": [r.choice(ALPHAS), r.getrandbits(8), r.getrandbits(8), r.getrandbits(8), r.getrandbits(8)],
                     "blend": r.randrange(2) if blend is None else blend, "style": r.choice([1, 1, 2]) if style is None else style,
                     "writer": [r.choice([0, -1, 5]) for _ in range(2)]})
        return case
    raise RuntimeError("no feasible case")


def drawchar_edges():
    r = random.Random(4610)
    for fmt in W.FORMATS + W.OTHER_FORMATS[:3]:
        for blend in (0, 1):
            for style in (1, 2):
                for kind in ("raw", "rle", "bits", "both"):
                    yield random_drawchar(r, fmt=fmt, blend=blend, style=style, kind=kind)
    for blend in (0, 1):
        for fmt in W.FORMATS:
            for alpha in (0, 1, 128, 254, 255):
                case = random_drawchar(r, fmt=fmt, blend=blend, style=1)
                case["colour"][0] = alpha
                yield case
    for _ in range(300):
        yield random_drawchar(r)


# ======================================================================= group 3: the objects, the table, SetWriteZero


def ppc_life(e, case):
    cls, kind, flag, is_null = case
    c = W.Case(e, {})
    e.alloc_log.clear()
    this = 0 if is_null else e.alloc(16, struct.pack(">I", 0x11111111) + bytes(12))
    if kind == 0:
        r3, _, _ = T.call_ppc(e, CTORS[cls], [this])
    else:
        r3, _, _ = T.call_ppc(e, DTORS[cls], [this, flag])
    vptr = e.u32(this) if this else 0
    klass = VTABLES.index(vptr) if vptr in VTABLES else (-1 if kind == 0 else (-2 if vptr == VTABLE_BASE else -3))
    return {"returns this": int(r3 == this), "class": klass if kind == 0 else 0, "deletes": W.heap_ppc(e)[2], "vptr": vptr if kind else 0}


def host_life(lib, case):
    cls, kind, flag, is_null = case
    out = (C.c_int64 * 2)()
    hostfn(lib, "tpw_fw_life", None, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_void_p)(cls, kind, flag, is_null, C.addressof(out))
    # The host cannot show the vtable pointer the original stores: a destructor ends in the base class's, whose vtable it leaves.
    vptr = 0 if is_null or kind == 0 else VTABLE_BASE
    return {"returns this": 1, "class": out[0] if kind == 0 else 0, "deletes": out[1], "vptr": vptr if kind else 0}


def life_edges():
    for cls in range(4):
        yield (cls, 0, 0, 0)
        for flag in (1, 0, -1, 0x7FFF, 0x8000, 0x10000, 0x10001, 0xFFFF, 2, -2, 0x12345678, 0xFFFF8000, 0x7FFF0000):
            yield (cls, 1, flag, 0)
            yield (cls, 1, flag, 1)


def life_random(r):
    kind = r.randrange(2)
    return (r.randrange(4), kind, r.choice([r.getrandbits(32), r.randint(-3, 3), r.randrange(0x20000)]), 1 if kind and r.random() < 0.3 else 0)


def ppc_table(e, case):
    c = W.Case(e, {})
    s = install_stubs(c)
    saved = s.activate(e)
    try:
        construct_objects(e, c)
        words = struct.unpack(">30I", e.read(TABLE, 120))
        cell = [OBJECTS.index(w) + 1 if w in OBJECTS else (0 if w == 0 else -1) for w in words]
        vptrs = [e.u32(a) for a in OBJECTS]
        classes = [VTABLES.index(v) if v in VTABLES else -1 for v in vptrs]
        dtor_of = {e.u32(v + 8): i for i, v in enumerate(VTABLES)}
        registered = [(OBJECTS.index(o) if o in OBJECTS else -1, dtor_of.get(d, -1)) for o, d in e.registered]
        return {"table": cell, "objects": classes, "registered": registered}
    finally:
        CL.Stubs.deactivate(e, saved)


def host_table(lib, case):
    out = (C.c_int32 * 30)()
    lib.tpw_fw_table(out)
    # The objects are the host's globals, constructed in the order of their definitions (one colour, one colour without blending,
    # textured, textured without blending); the compiler registers their destructors, nothing the host can show: the PowerPC side's
    # registrations must be those, in that order.
    return {"table": list(out), "objects": [0, 1, 2, 3], "registered": [(0, 0), (1, 1), (2, 2), (3, 3)]}


def ppc_initialise(e, case):
    style, ptype, blend = case
    c = W.Case(e, {})
    label_objects(c)
    p = W.persist(e)
    s = install_stubs(c)
    saved = s.activate(e)
    try:
        construct_objects(e, c)
        font = e.alloc(0x20, struct.pack(">Ii", p.font_vt, 1) + bytes(0x18))
        params = e.alloc(0x50 + 32, b"\xa5" * (0x50 + 32))
        e.write(params + 4, struct.pack(">I", font))
        e.write(params + 0xC, struct.pack(">ii", style, blend))
        render = e.alloc(0x58 + 32, b"\xa5" * (0x58 + 32))
        c.L.labels.update({font: LAB_FONT, params: LAB_PARAMS})
        c.font_cfg[font] = ptype
        result = e.alloc(4, struct.pack(">I", 0x77777777))
        T.call_ppc(e, INITIALISE, [result, render, params])
        method = e.u32(render + 0x54)
        return {"result": T.s32(e.u32(result)), "method": OBJECTS.index(method) + 1 if method in OBJECTS else (0 if method == 0 else -1), "log": c.L.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host_initialise(lib, case):
    style, ptype, blend = case
    CL.host_begin(lib, {}, {}, FIRST_NEW)
    out = (C.c_int64 * 2)()
    hostfn(lib, "tpw_fw_initialise", None, C.c_int32, C.c_int32, C.c_int32, C.c_void_p)(style, ptype, blend, C.addressof(out))
    return {"result": out[0], "method": out[1], "log": CL.host_end(lib)}


def initialise_edges():
    for blend in (0, 1):
        for style in range(6):
            for ptype in range(8):
                yield (style, ptype, blend)


def ppc_writezero(e, case):
    flags = []
    for value in case:
        T.call_ppc(e, SET_WRITE_ZERO, [value])
        flags.append(e.read(WRITE_ZERO_BYTE, 1)[0])
    return {"flags": flags}


def host_writezero(lib, case):
    fn = hostfn(lib, "tpw_fw_writezero", C.c_int32, C.c_int32)
    return {"flags": [fn(value) for value in case]}


TESTS = [make_writer_test(cls, slot) for cls in (1, 0) for slot in (0, 1, 2, 4, 5, 3)]
TESTS += [make_writer_test(cls, slot) for cls in (2, 3) for slot in range(6)]
TESTS += [
    FnTest("Tb4BitFontRenderMethod::DrawChar through gFontRenderMethods[blend][4][style]", ppc_drawchar, host_drawchar,
           edge=lambda: list(drawchar_edges()), rnd=random_drawchar),
    FnTest("the four classes' constructors and destructors", ppc_life, host_life, edge=lambda: list(life_edges()), rnd=life_random),
    FnTest("__sinit_TbFontRender_cpp and the table gFontRenderMethods", ppc_table, host_table, edge=lambda: [0], rnd=lambda r: 0),
    FnTest("TbFontRender::Initialise over the real table", ppc_initialise, host_initialise, edge=lambda: list(initialise_edges()),
           rnd=lambda r: (r.randrange(6), r.randrange(8), r.randrange(2))),
    FnTest("SetWriteZero", ppc_writezero, host_writezero, edge=lambda: [[1], [0], [1, 1, 0, 0, 1, 0, 1, 1], [0, 1]],
           rnd=lambda r: [r.randrange(2) for _ in range(r.randint(1, 8))]),
]
