"""Differential tests for the 4-bit font render method's core and the render parameters' last members (unit w45a), against
libtpw_unit_FontRenderMethod.dylib (tests/unit_FontRenderMethod.cpp), all in the ltms library ("ltms symbols shared"):

  src/ltms/StaticUnicodeBuffer.cpp, src/ltms/TbFontRender.cpp  Tb4BitFontRenderMethod: constructor 0x10000bf0 (the vtable store, nothing the host can show),
                                  ~Tb4BitFontRenderMethod 0x10000cb4, DrawChar 0x10000d40, CalculateSourceRect 0x10001dbc, GetRotate
                                  0x10001f64; TbFontRenderMethod::~TbFontRenderMethod 0x10000c38; FourBit::FontData::I 0x100018d8 and
                                  ~FontData 0x10001d30
  include/TbFontBits.h            (inline) FourBit::NybbleIterator 0x10001cec ctor, 0x10001bc4 operator*, 0x10001c2c operator++,
                                  0x1000a68c current; FourBit::NybbleWriter 0x10001c9c ctor, 0x10001b28 Write, 0x10001ae8 Nybbles;
                                  OneBit::BitIterator 0x10001a98 ctor, 0x100019e0 operator*, 0x10001a28 operator++, BitReference
                                  0x1000197c operator unsigned char
  src/ltms/TbFontRender.cpp, src/ltms/TbFontRenderParams.cpp         TbFontRenderParams: operator= 0x100025e8, UseTexture 0x10002718, UseBlendedMode 0x100028f8, GetStyle
                                  0x10002ab8, GetType 0x10002b70

Real on both sides besides those: TbFontRenderParams::GetFont (0x10002af4) and the font's GetCharHeader and GetCharData (0x100046f8,
0x10004684). Fakes that record the same event on both sides: the font's AddRef and Release (a real font image is built for every case,
little endian for the host and big endian for the PowerPC side), LbDraw_GetClipRect (answers the case's clip rectangle),
TbRect::Intersection (the listing's formula, recording its operands) and the six pixel writers of the render method (each records the area,
every word of the parameter record and a digest of the glyph data it was handed). The array new and delete counts are compared.

DrawChar cases: one glyph drawn from a small font image into a render area of every pixel format the dispatcher knows (and one-bit
perturbations of each mask, the depth and the palettised flag), with a clip rectangle, a point, glyph headers of every flag and data
kind (raw nybbles, nybble run-length, one-bit and both), the decode buffer in every state (not made yet, made with a block smaller or
larger than the glyph's data size) and the writers' answers from a queue. The NULL-font path is not run: the original releases a font
through a NULL object there (the host returns -1 without calling anything), which the emulator cannot do either.
"""
import ctypes as C
import random
import struct
import types

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_FontRenderMethod"

MINUS = 0xFFFFFFFF
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF

CTOR4, DTOR_BASE, DTOR4, DRAWCHAR = 0x10000BF0, 0x10000C38, 0x10000CB4, 0x10000D40
FD_I, FD_DTOR = 0x100018D8, 0x10001D30
BITREF, BIT_DEREF, BIT_INC, BIT_CTOR = 0x1000197C, 0x100019E0, 0x10001A28, 0x10001A98
NW_COUNT, NW_WRITE, NW_CTOR = 0x10001AE8, 0x10001B28, 0x10001C9C
NI_DEREF, NI_INC, NI_CTOR, NI_CURRENT = 0x10001BC4, 0x10001C2C, 0x10001CEC, 0x1000A68C
CALCRECT, GETROTATE = 0x10001DBC, 0x10001F64
P_ASSIGN, P_USETEX, P_USEBLEND, P_GETSTYLE, P_GETTYPE = 0x100025E8, 0x10002718, 0x100028F8, 0x10002AB8, 0x10002B70
P_GETFONT, GET_CHAR_DATA, GET_CHAR_HEADER = 0x10002AF4, 0x10004684, 0x100046F8
REAL = [CTOR4, DTOR_BASE, DTOR4, DRAWCHAR, FD_I, FD_DTOR, BITREF, BIT_DEREF, BIT_INC, BIT_CTOR, NW_COUNT, NW_WRITE, NW_CTOR, NI_DEREF,
        NI_INC, NI_CTOR, NI_CURRENT, CALCRECT, GETROTATE, P_ASSIGN, P_USETEX, P_USEBLEND, P_GETSTYLE, P_GETTYPE, P_GETFONT,
        GET_CHAR_DATA, GET_CHAR_HEADER]

SINGLETON_SIZE, SINGLETON_BLOCK, SINGLETON_MADE = 0x100116FC, 0x10011700, 0x10011704
VTABLE_4BIT, VTABLE_BASE = 0x10011004, 0x1001102C
WRITER_SLOTS = {0x10: "To555", 0x14: "To565", 0x18: "To4444", 0x1C: "ToAny", 0x20: "To8888", 0x24: "ToBGRA"}

LAB_FONT, LAB_AREA, LAB_PARAMS, LAB_METHOD = 10, 20, 30, 40
FIRST_NEW = 100


def s32(v):
    return T.s32(v & MINUS)


def sx16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def hostfn(lib, name, restype, *argtypes):
    fn = getattr(lib, name)
    fn.restype = restype
    fn.argtypes = list(argtypes)
    return fn


def setup(runner):
    T.install_std_stubs(runner.emu)


def digest(data):
    h = 0
    for b in data[:64]:
        h = (h * 31 + b) & MINUS
    return h


def heap_ppc(e):
    kinds = [k for k, _, _ in e.alloc_log]
    return [kinds.count("new"), kinds.count("new[]"), kinds.count("delete"), kinds.count("delete[]")]


def heap_host(lib):
    return list((C.c_int32 * 4).in_dll(lib, "tpw_cl_heap"))


def intersect(a, b):
    left, top = max(a[0], b[0]), max(a[1], b[1])
    right, bottom = min(a[2], b[2]), min(a[3], b[3])
    return (left, top, max(right, left), max(bottom, top))


# ======================================================================= the PowerPC side: persistent fixtures


def persist(e):
    """Font and method vtables that live across cases; their behaviour comes from the current case (e.frc)."""
    if hasattr(e, "frp"):
        return e.frp
    p = types.SimpleNamespace()
    e.frp = p

    def slot(table, offset, fn):
        e.write(table + offset, struct.pack(">I", T.fake_function(e, fn)))

    vt = T.persist_alloc(e, 0x40)
    slot(vt, 0x08, lambda e: e.frc.L.rec("Release", (e.frc.L.lab(e.r(3)),)))
    slot(vt, 0x0C, lambda e: e.frc.L.rec("AddRef", (e.frc.L.lab(e.r(3)),)))
    slot(vt, 0x30, lambda e: e.frc.font_type(e))
    p.font_vt = vt
    mvt = T.persist_alloc(e, 0x40)
    for offset, name in WRITER_SLOTS.items():
        slot(mvt, offset, (lambda n: lambda e: e.frc.writer(e, n))(name))
    p.method_vt = mvt
    p.method = T.persist_alloc(e, 16, struct.pack(">I", mvt) + bytes(12))
    return p


class Case:
    """Per-case state of the PowerPC fakes: the log, the labels and the clip rectangle."""

    def __init__(self, e, fixed, queues=None):
        self.e = e
        e.frc = self
        self.p = persist(e)
        self.L = e.cl = CL.PpcLog(fixed, queues or {}, first_new=FIRST_NEW)
        self.clip_addr = 0
        self.glyph_data = 0
        self.expect = 0
        self.font_cfg = {}

    def clip_rect(self, e):
        self.L.rec("GetClipRect")
        e.set_r(3, self.clip_addr)

    def intersection(self, e):
        a = struct.unpack(">4i", e.read(e.r(4), 16))
        b = struct.unpack(">4i", e.read(e.r(5), 16))
        self.L.rec("Intersection", a + b)
        e.write(e.r(3), struct.pack(">4i", *intersect(a, b)))

    def font_type(self, e):
        self.L.rec("GetType", (self.L.lab(e.r(3)),))
        e.set_r(3, T.u32(self.font_cfg.get(e.r(3), 0)))

    def writer(self, e, name):
        result, area, record = e.r(3), e.r(5), e.r(6)
        point, data, per_row, source, params = e.u32(record), e.u32(record + 4), e.s32(record + 8), e.u32(record + 12), e.u32(record + 16)
        width = e.read(record + 0x14, 1)[0]
        px, py = struct.unpack(">ii", e.read(point, 8))
        src = struct.unpack(">4i", e.read(source, 16))
        self.L.rec(name, (self.L.lab(area), px, py, per_row) + src + (self.L.lab(params), width))
        made = e.read(SINGLETON_MADE, 1)[0] != 0
        if made and data == e.u32(SINGLETON_BLOCK):
            kind = 1
        elif data == self.glyph_data:
            kind = 2
        else:
            kind = 0
        self.L.rec("Data", (kind, digest(e.read(data, min(self.expect, 64))) if self.expect > 0 else 0, self.expect))
        e.write(result, struct.pack(">i", T.s32(T.u32(self.L.ret("Writer", 0)))))


def install_stubs(c):
    s = CL.Stubs()
    s.real.update(REAL)
    s.imp("LbDraw_GetClipRect__Fv", c.clip_rect)
    s.imp("Intersection__6TbRectCFRC6TbRect", c.intersection)
    return s


# ======================================================================= font images


def build_image(glyphs, end):
    """A font image: 8 bytes of header (magic, a byte, the line height, the character count = len(glyphs) - 1), the offset table of
    len(glyphs) words, then the records: a 0x18-byte header (see TbIRLE4bitFont.cpp) and the data, each record on a word boundary.
    `end` is ">" for the PowerPC side and "<" for the host. Answers the image and the offset of each record's data."""
    n = len(glyphs)
    image = struct.pack(end + "I", 0x46344642) + bytes([0, 17]) + struct.pack(end + "H", n - 1)
    start = 8 + 4 * n
    records, offsets, data_at = [], [], []
    position = start
    for g in glyphs:
        head = struct.pack(end + "HHIIIHH", g["code"], g["x2"], g["x4"], g["size"], g["flags"], g["width"], g["height"])
        head += struct.pack("bb", g["dx"], g["dy"]) + struct.pack(end + "H", g["advance"])
        assert len(head) == 0x18
        body = head + g["data"]
        body += bytes(-len(body) % 4)
        offsets.append(position)
        data_at.append(position + 0x18)
        records.append(body)
        position += len(body)
    table = b"".join(struct.pack(end + "I", o) for o in offsets)
    return image + table + b"".join(records), data_at


def rle_pixels(data):
    """The pixel count the nybble run-length decoder produces for `data` (the stream the original reads)."""
    nybbles = []
    for b in data:
        nybbles += [b >> 4, b & 15]
    i, count = 0, 0
    while True:
        n = nybbles[i]
        i += 1
        if n:
            count += 1
            continue
        run = nybbles[i]
        i += 1
        if run == 0:
            return count
        i += 1
        count += run


def rle_encode(pixels, r):
    nyb = []
    i = 0
    while i < len(pixels):
        p, run = pixels[i], 1
        while i + run < len(pixels) and pixels[i + run] == p and run < 15:
            run += 1
        if p != 0 and (run == 1 or r.random() < 0.3):
            nyb.append(p)
            run = 1
        else:
            nyb += [0, run, p]
        i += run
    nyb += [0, 0]
    if len(nyb) % 2:
        nyb.append(r.randrange(16))
    return bytes((nyb[k] << 4) | nyb[k + 1] for k in range(0, len(nyb), 2))


# ======================================================================= DrawChar


FORMATS = [
    (0x7C00, 0x3E0, 0x1F, 0x8000, 16, 0), (0x7C00, 0x3E0, 0x1F, 0, 16, 0), (0xF800, 0x7E0, 0x1F, 0, 16, 0),
    (0xF000, 0xF00, 0xF0, 0xF, 16, 0), (0xFF0000, 0xFF00, 0xFF, 0xFF000000, 32, 0), (0xFF0000, 0xFF00, 0xFF, 0, 32, 0),
    (0xFF00, 0xFF0000, 0xFF000000, 0xFF, 32, 0), (0xFF00, 0xFF0000, 0xFF000000, 0, 32, 0),
]
OTHER_FORMATS = [(0xFF0000, 0xFF00, 0xFF, 0, 24, 0), (0, 0, 0, 0, 8, 1), (0x7C00, 0x3E0, 0x1F, 0x4000, 16, 0), (0xF800, 0x7E0, 0x1F, 0x8000, 16, 0),
                 (0xF000, 0xF00, 0xF0, 0, 16, 0), (0x7C00, 0x3E0, 0x1F, 0x8000, 15, 0), (0xFF0000, 0xFF00, 0xFF, 0x80000000, 32, 0)]


def format_variants(r):
    """Every known format as it is, one perturbation of each of its six words, and a few formats the dispatcher does not know."""
    for fmt in FORMATS:
        yield fmt
        for i in range(6):
            bad = list(fmt)
            if i < 4:
                bad[i] = bad[i] ^ r.choice([1, 2, 0x100, 0x80000000])
            elif i == 4:
                bad[i] = bad[i] + r.choice([-1, 1, 8, 16])
            else:
                bad[i] = r.choice([1, 2, 0x100, MINUS, 0x80000000])
            yield tuple(v & MINUS for v in bad)
    for fmt in OTHER_FORMATS:
        yield fmt


def make_glyph(r, kind=None, width=None, height=None, code=None):
    """A glyph record. kind: 'raw', 'rle', 'bits', 'both'."""
    kind = kind or r.choice(["raw", "rle", "rle", "bits", "both"])
    width = (0 if r.random() < 0.04 else r.choice([1, 2, 3, 5, 8, 9, 12, 16, 17, 255, 256, 257][:-3 if r.random() < 0.97 else None])) if width is None else width
    height = (0 if r.random() < 0.04 else r.choice([1, 2, 4, 7, 10, 13])) if height is None else height
    per_row = (width + 1) >> 1
    g = {"code": code if code is not None else (0x20 if r.random() < 0.07 else r.choice([0x41, 0x61, 0x21, 0x1F, 0, 0xFFFF, 0x7F, 0x120])), "x2": r.getrandbits(16),
         "x4": r.getrandbits(32), "width": width, "height": height,
         "dx": r.choice([0, 0, 1, -1, 2, -3, 5, r.randint(-8, 8), r.randint(-8, 8)] if r.random() < 0.95 else [127, -128, 100, -100]),
         "dy": r.choice([0, 0, 1, -1, 3, -2, r.randint(-8, 8), r.randint(-8, 8)] if r.random() < 0.95 else [127, -128, 100, -100]),
         "advance": r.getrandbits(16), "kind": kind}
    if kind == "raw":
        g["flags"] = r.choice([0, 0, 4, 8, 0x80, 0xFFFFFFFC, 0x80000000, 0x10, 0xFFFFFF00])
        g["data"] = bytes(r.getrandbits(8) for _ in range(per_row * height + r.choice([0, 0, 3])))
        g["size"] = r.choice([len(g["data"]), 0, 5, 0x1000, 0xFFFFFFFF])
    elif kind == "rle":
        g["flags"] = r.choice([1, 1, 1, 5, 0x81, 0x80000001, 0xFFFFFFFD])
        pixels = [r.choice([0, 0, 1, 7, 15, r.randrange(16)]) for _ in range(width * height)]
        if r.random() < 0.5:
            pixels = [0] * (width * height) if r.random() < 0.5 else [r.choice([3, 3, 3, 3, 9]) for _ in range(width * height)]
        g["data"] = rle_encode(pixels, r) + bytes(r.randrange(256) for _ in range(r.choice([0, 0, 2])))
        g["size"] = r.choice([len(g["data"])] * 4 + [0, 1, 0x7FF, 0x800, 0x801, 0x1000, 0x80000000, 0xFFFFFFFF])
    elif kind == "bits":
        g["flags"] = r.choice([2, 2, 2, 6, 0x82, 0x80000002, 0xFFFFFFFE])
        g["size"] = r.choice([0, 1, 2, 3, 4, 7, 8, 16, 33, 64, 100, 0x200, 0x7FF, 0x800, 0x801, 0x900])
        g["data"] = bytes(r.getrandbits(8) for _ in range((g["size"] + 3) // 4 + r.choice([0, 1])))
    else:
        g["flags"] = r.choice([3, 3, 7, 0xFFFFFFFF])
        pixels = [r.choice([0, 1, 5, 15]) for _ in range(width * height)]
        g["data"] = rle_encode(pixels, r)
        g["size"] = r.choice([len(g["data"]), 0, 1, 7, 0x40, 0x7FF, 0x801])
        g["data"] += bytes(max((g["size"] + 3) // 4 - len(g["data"]), 0))
    return g


def plan(case):
    """What the case should leave behind: the glyph's data offset, the digest length, the bytes of the decode block to compare."""
    glyphs = case["glyphs"]
    index = case["glyph"]
    if index >= len(glyphs):
        return {"decodes": False, "expect": 0, "block": 0 if case["state"] == 0 else case["size"], "glyph": None}
    g = glyphs[index]
    flags, size = g["flags"], g["size"]
    if case["area"][5] != 0:      # a palettised area is refused before anything is decoded
        return {"decodes": False, "expect": 0, "block": 0 if case["state"] == 0 else case["size"], "glyph": g}
    extent = 0
    if flags & 1:
        extent = max(extent, (rle_pixels(g["data"]) + 1) // 2)
    if flags & 2:
        extent = max(extent, size)
    decodes = bool(flags & 3)
    expect = extent if decodes else len(g["data"])
    state, block_size = case["state"], case["size"]
    regrown = decodes and (state == 0 and sx32(size) > 0x800 or state == 1 and sx32(size) > block_size)
    if not decodes:
        block = 0 if state == 0 else block_size
    elif state == 0 or regrown:
        block = extent
    else:
        block = block_size
    return {"decodes": decodes, "expect": expect, "block": block, "glyph": g, "extent": extent, "regrown": regrown}


def sx32(v):
    v &= MINUS
    return v - (1 << 32) if v & 0x80000000 else v


def feasible(case):
    """The decoded pixels must fit the buffer the original leaves (it writes without a bound)."""
    index = case["glyph"]
    if index >= len(case["glyphs"]):
        return True
    p = plan(case)
    if not p["decodes"]:
        return True
    size = sx32(p["glyph"]["size"])
    final = case["size"] if case["state"] == 1 else 0x800
    if size > final:
        final = size
    return p["extent"] <= final and not (p["glyph"]["flags"] & 2 and size < 0)


def ppc_drawchar(e, case):
    glyphs = case["glyphs"]
    image, data_at = build_image(glyphs, ">")
    pl = plan(case)
    c = Case(e, {}, {"Writer": case["writer"]})
    p = persist(e)
    old_scratch = e.heap_in_scratch
    e.heap_in_scratch = True
    try:
        font_addr = e.alloc(0x20, bytes(0x20))
        image_addr = e.alloc(len(image) + 128, image + bytes(128))
        e.write(font_addr, struct.pack(">IiIIII", p.font_vt, 1, image_addr, image_addr, image_addr + 8, 0))
        params = e.alloc(0x50 + 32, b"\xa5" * (0x50 + 32))
        e.write(params + 4, struct.pack(">I", font_addr))
        area = e.alloc(0x28 + 32, b"\xa5" * (0x28 + 32))
        e.write(area, struct.pack(">6I", *case["area"]))
        clip = e.alloc(16, struct.pack(">4i", *case["clip"]))
        at = e.alloc(8, struct.pack(">ii", case["x"], case["y"]))
        drawn = e.alloc(16, struct.pack(">4i", -1, -2, -3, -4))
        result = e.alloc(4, struct.pack(">I", 0x77777777))
        c.L.labels.update({font_addr: LAB_FONT, area: LAB_AREA, params: LAB_PARAMS, p.method: LAB_METHOD})
        c.clip_addr = clip
        c.expect = pl["expect"]
        c.glyph_data = image_addr + data_at[case["glyph"]] if case["glyph"] < len(glyphs) else 0
        e.write(e.data_import("_lbpCurrentRA"), struct.pack(">I", area))
        # the decode buffer's state
        if case["state"] == 0:
            e.write(SINGLETON_MADE, b"\0")
            e.write(SINGLETON_SIZE, struct.pack(">I", 0x1234ABCD))
            e.write(SINGLETON_BLOCK, bytes(4))
            before = 0
        else:
            before = e.alloc(max(case["size"], 4), b"\xa5" * max(case["size"], 4))
            e.write(SINGLETON_MADE, b"\1")
            e.write(SINGLETON_SIZE, struct.pack(">I", case["size"]))
            e.write(SINGLETON_BLOCK, struct.pack(">I", before))
        e.alloc_log.clear()
        s = install_stubs(c)
        saved = s.activate(e)
        try:
            T.call_ppc(e, DRAWCHAR, [result, p.method, at, case["glyph"], drawn, params])
        finally:
            CL.Stubs.deactivate(e, saved)
        made = e.read(SINGLETON_MADE, 1)[0] != 0
        block = e.u32(SINGLETON_BLOCK)
        out = {"result": s32(e.u32(result)), "drawn": list(struct.unpack(">4i", e.read(drawn, 16))),
               "point": list(struct.unpack(">ii", e.read(at, 8))), "made": int(made), "size": e.u32(SINGLETON_SIZE) if made else 0,
               "block": e.read(block, pl["block"]) if made and pl["block"] > 0 else b"",
               "area": list(struct.unpack(">6I", e.read(area, 24))),
               "params": [c.L.lab(e.u32(params + 4)), e.u32(params + 8), e.u32(params + 12), e.u32(params + 16)],
               "log": c.L.log, "heap": heap_ppc(e)}
        return out
    finally:
        e.heap_in_scratch = old_scratch


def host_drawchar(lib, case):
    image, data_at = build_image(case["glyphs"], "<")
    pl = plan(case)
    fn = hostfn(lib, "tpw_fr_drawchar", C.c_int32, C.c_char_p, C.c_int32, C.c_void_p, C.c_void_p, C.c_int32, C.c_int32, C.c_int32,
                C.c_int32, C.c_uint32, C.c_int32, C.c_int32, C.c_void_p, C.c_void_p, C.c_int32)
    CL.host_begin(lib, {}, {"Writer": case["writer"]}, FIRST_NEW)
    area = (C.c_uint32 * 6)(*case["area"])
    clip = (C.c_int32 * 4)(*case["clip"])
    out = (C.c_int64 * 64)()
    block = C.create_string_buffer(max(pl["block"], 1))
    offset = data_at[case["glyph"]] if case["glyph"] < len(case["glyphs"]) else -1
    n = fn(image, len(image), C.addressof(area), C.addressof(clip), case["x"], case["y"], case["glyph"], case["state"], case["size"], offset,
           pl["expect"], C.addressof(out), C.addressof(block), pl["block"])
    log = CL.host_end(lib)
    v = list(out[:n])
    made = v[7]
    return {"result": v[0], "drawn": v[1:5], "point": v[5:7], "made": made, "size": v[8],
            "block": bytes(block.raw[:pl["block"]]) if made and pl["block"] > 0 else b"", "area": [x & MINUS for x in v[10:16]],
            "params": [x & MINUS if i else x for i, x in enumerate(v[16:20])], "log": log, "heap": heap_host(lib)}


def random_drawchar(r, kind=None, fmt=None, glyph_code=None, wild=None):
    """One DrawChar case. A wild one has points and clip rectangles anywhere (most of them draw nothing); a tame one mostly draws."""
    wild = r.random() < 0.3 if wild is None else wild
    for _ in range(100):
        glyphs = [make_glyph(r, kind=kind, code=glyph_code) for _ in range(r.randint(1, 3))]
        glyphs.append(make_glyph(r, kind=kind))      # the record one past the count, which the font's range check lets through
        state = r.choice([0, 1, 1])
        size = r.choice([0x10, 0x100, 0x800, 0x400, 0x7FF, 0x801, 0x1000, 0, 1])
        glyph = r.randrange(len(glyphs))
        if r.random() < 0.06:
            glyph = r.choice([len(glyphs), len(glyphs) + 5, 0xFFFF])
        if wild:
            x, y = r.randint(-40, 80), r.randint(-40, 80)
            if r.random() < 0.1:
                x = r.choice([INT_MAX, INT_MIN, INT_MAX - 3, INT_MIN + 2, 0x7FFFFFF0])
            if r.random() < 0.1:
                y = r.choice([INT_MAX, INT_MIN, INT_MAX - 3, INT_MIN + 2, 0x7FFFFFF0])
            clip = [r.choice([0, 0, 0, 2, -5, 8]), r.choice([0, 0, 0, 3, -4, 6]), r.choice([30, 40, 64, 100, 5, 12, 0]), r.choice([30, 40, 48, 100, 6, 0])]
            if r.random() < 0.2:
                clip = [r.randint(-60, 60) for _ in range(4)]
            if r.random() < 0.1:
                clip = [INT_MIN, INT_MIN, INT_MAX, INT_MAX]
        else:
            x, y = r.randint(-8, 22), r.randint(-8, 22)
            clip = [r.choice([0, 0, 0, 3, 5]), r.choice([0, 0, 0, 2, 6]), r.choice([40, 64, 100, 24]), r.choice([40, 48, 100, 20])]
        f = fmt if fmt is not None else (r.choice(FORMATS) if r.random() < 0.85 else r.choice(OTHER_FORMATS))
        case = {"glyphs": glyphs, "glyph": glyph, "x": x, "y": y, "clip": clip, "state": state, "size": size, "area": list(f),
                "writer": [r.choice([0, -1, 1, 5, -7, INT_MAX, INT_MIN]) for _ in range(2)]}
        if feasible(case):
            return case
    raise RuntimeError("no feasible case")


def drawchar_edges():
    r = random.Random(4501)
    # every format variant (the known ones several times), with one glyph of each data kind drawn at an inside point
    for fmt in format_variants(r):
        known = fmt in FORMATS
        for kind in ("raw", "rle", "bits", "both"):
            for _ in range(3 if known else 1):
                case = random_drawchar(r, kind=kind, fmt=fmt, glyph_code=0x41, wild=False)
                case["glyph"] = r.randrange(len(case["glyphs"]) - 1) if len(case["glyphs"]) > 1 else 0
                case["x"], case["y"], case["clip"] = r.randint(0, 6), r.randint(0, 6), [0, 0, 100, 100]
                g = case["glyphs"][case["glyph"]]
                g["dx"] = g["dy"] = 0
                g["width"], g["height"] = max(g["width"], 1), max(g["height"], 1)
                if feasible(case):
                    yield case
    # wide glyphs: the width byte of the record is the low byte of the width, and the bytes in a row come from the whole width
    for width in (255, 256, 257, 300, 511, 512):
        for kind in ("raw", "rle", "bits"):
            case = random_drawchar(r, kind=kind, fmt=FORMATS[r.randrange(len(FORMATS))], glyph_code=0x41, wild=False)
            case["glyph"] = 0
            case["x"], case["y"], case["clip"] = 3, 2, [0, 0, 1000, 1000]
            g = case["glyphs"][0]
            g["dx"] = g["dy"] = 0
            g["width"], g["height"] = width, 2
            if kind == "raw":
                g["data"] = bytes(r.getrandbits(8) for _ in range(((width + 1) >> 1) * 2))
            elif kind == "rle":
                g["data"] = rle_encode([r.choice([0, 3, 9]) for _ in range(width * 2)], r)
                g["size"] = len(g["data"])
            if feasible(case):
                yield case
    # a long random sweep (the clip, the offsets, the states, the flags)
    for _ in range(3000):
        yield random_drawchar(r)


def drawchar_random(r):
    return random_drawchar(r)


# ======================================================================= CalculateSourceRect


def ppc_calcrect(e, case):
    c = Case(e, {})
    p = persist(e)
    rect = e.alloc(16, struct.pack(">4i", *case["rect"]))
    point = e.alloc(8, struct.pack(">ii", *case["point"]))
    clip = e.alloc(16, struct.pack(">4i", *case["clip"]))
    area = e.alloc(0x28, b"\xa5" * 0x28)
    result = e.alloc(16, b"\x77" * 16)
    c.L.labels.update({area: LAB_AREA, p.method: LAB_METHOD})
    c.clip_addr = clip
    s = install_stubs(c)
    saved = s.activate(e)
    try:
        T.call_ppc(e, CALCRECT, [result, p.method, rect, point, area])
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"result": list(struct.unpack(">4i", e.read(result, 16))), "rect": list(struct.unpack(">4i", e.read(rect, 16))),
            "point": list(struct.unpack(">ii", e.read(point, 8))), "clip": list(struct.unpack(">4i", e.read(clip, 16))), "log": c.L.log}


def host_calcrect(lib, case):
    fn = hostfn(lib, "tpw_fr_calcrect", None, C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p)
    CL.host_begin(lib, {}, {}, FIRST_NEW)
    rect, point, clip = (C.c_int32 * 4)(*case["rect"]), (C.c_int32 * 2)(*case["point"]), (C.c_int32 * 4)(*case["clip"])
    out = (C.c_int32 * 4)()
    fn(C.addressof(rect), C.addressof(point), C.addressof(clip), C.addressof(out))
    return {"result": list(out), "rect": list(rect), "point": list(point), "clip": list(clip), "log": CL.host_end(lib)}


def coord(r, span):
    k = r.random()
    if k < 0.65:
        return r.randint(-span, span)
    if k < 0.85:
        return r.randint(-200, 200)
    return r.choice([INT_MIN, INT_MAX, -1, 0, 1, INT_MIN + 1, INT_MAX - 1, INT_MAX - 5])


def calcrect_case(r):
    return {"rect": [r.choice([0, 0, 0, 1, -2]), r.choice([0, 0, 0, 2, -1]), coord(r, 20), coord(r, 20)],
            "point": [coord(r, 30), coord(r, 30)], "clip": [coord(r, 20), coord(r, 20), coord(r, 40), coord(r, 40)]}


def calcrect_edges():
    r = random.Random(4502)
    # the point left of, inside and right of the clip, on each axis, with a glyph of width 6 and height 9
    for px in (-10, -6, -5, -1, 0, 1, 5, 9, 10, 11, 20):
        for py in (-12, -9, -8, 0, 4, 9, 10, 11):
            yield {"rect": [0, 0, 6, 9], "point": [px, py], "clip": [0, 0, 10, 10]}
            yield {"rect": [0, 0, 6, 9], "point": [px, py], "clip": [2, 3, 8, 7]}
    for rect in ([0, 0, 0, 0], [0, 0, 5, 0], [0, 0, 0, 5], [3, 4, 9, 13], [5, 5, 2, 2], [-3, -4, 3, 4], [INT_MIN, INT_MIN, INT_MAX, INT_MAX]):
        for clip in ([0, 0, 10, 10], [4, 4, 4, 4], [10, 10, 0, 0], [-5, -5, 5, 5], [INT_MIN, INT_MIN, INT_MAX, INT_MAX], [0, 0, 0, 0]):
            for point in ([0, 0], [-7, 3], [3, -7], [INT_MAX, 0], [0, INT_MAX], [INT_MIN, INT_MIN], [12, 12]):
                yield {"rect": rect, "point": point, "clip": clip}
    for _ in range(400):
        yield calcrect_case(r)


# ======================================================================= GetRotate


def ppc_rotate(e, case):
    c = Case(e, {})
    p = persist(e)
    r3, _, _ = T.call_ppc(e, GETROTATE, [p.method, case])
    return {"rotate": s32(r3)}


def host_rotate(lib, case):
    return {"rotate": hostfn(lib, "tpw_fr_getrotate", C.c_int32, C.c_uint32)(case)}


def rotate_edges():
    for k in range(32):
        yield 1 << k
        yield (1 << k) | 1
        yield ((1 << (k + 1)) - 1) & MINUS
        yield (MINUS ^ ((1 << k) - 1)) & MINUS
    yield 0
    yield MINUS
    yield 0x7FFFFFFF
    yield 0x80000000
    yield 0xFF
    yield 0x100
    yield 0x7F
    yield 0x80


def rotate_random(r):
    return r.getrandbits(32) >> r.randrange(32) << r.randrange(32) & MINUS


# ======================================================================= the lifetime of the methods and FontData


def ppc_life(e, case):
    kind, flag, is_null = case
    p = persist(e)
    c = Case(e, {})
    e.alloc_log.clear()
    this = 0 if is_null else e.alloc(16, struct.pack(">I", 0x11111111) + bytes(12))
    if kind == 0:
        T.call_ppc(e, CTOR4, [this])
        r3 = this
    else:
        r3, _, _ = T.call_ppc(e, DTOR4 if kind == 1 else DTOR_BASE, [this, flag])
    vptr = e.u32(this) if this else 0
    return {"deletes": heap_ppc(e)[2], "returns this": int(r3 == this), "vptr": vptr}


def host_life(lib, case):
    kind, flag, is_null = case
    out = (C.c_int64 * 4)()
    hostfn(lib, "tpw_fr_method_life", None, C.c_int32, C.c_int32, C.c_int32, C.c_void_p)(kind, flag, is_null, C.addressof(out))
    # The host cannot show the vtable pointer the original stores: the constants are what the listings store (the constructor the
    # 4-bit method's vtable; a destructor the base class's, unless the object is NULL).
    if is_null:
        vptr = 0
    elif kind == 0:
        vptr = VTABLE_4BIT
    else:
        vptr = VTABLE_BASE
    return {"deletes": out[0], "returns this": 1, "vptr": vptr}


def life_edges():
    for kind in (0, 1, 2):
        for flag in (1, 0, -1, 0x7FFF, 0x8000, 0x10000, 0x10001, 0xFFFF, 2, -2, 0x12345678, 0xFFFF8000, 0x7FFF0000):
            yield (kind, flag, 0)
            if kind:
                yield (kind, flag, 1)


def life_random(r):
    kind = r.randrange(3)
    return (kind, r.choice([r.getrandbits(32), r.randint(-3, 3), r.randrange(0x20000)]), 1 if kind and r.random() < 0.3 else 0)


def ppc_fontdata_i(e, case):
    state, size = case
    old = e.heap_in_scratch
    e.heap_in_scratch = True
    try:
        if state == 0:
            e.write(SINGLETON_MADE, b"\0")
            e.write(SINGLETON_SIZE, struct.pack(">I", 0x1234ABCD))
            e.write(SINGLETON_BLOCK, bytes(4))
            before = 0
        else:
            before = e.alloc(max(size, 4), b"\xa5" * max(size, 4))
            e.write(SINGLETON_MADE, b"\1")
            e.write(SINGLETON_SIZE, struct.pack(">I", size))
            e.write(SINGLETON_BLOCK, struct.pack(">I", before))
        e.alloc_log.clear()
        e.registered.clear()
        r3, _, _ = T.call_ppc(e, FD_I, [])
        heap = heap_ppc(e)
        made = e.read(SINGLETON_MADE, 1)[0]
        return {"made": int(made != 0), "size": e.u32(SINGLETON_SIZE),
                "new[]": heap[1], "delete[]": heap[3], "registered": len(e.registered), "answer": int(r3 == SINGLETON_SIZE)}
    finally:
        e.heap_in_scratch = old


def host_fontdata_i(lib, case):
    state, size = case
    out = (C.c_int64 * 8)()
    hostfn(lib, "tpw_fr_fontdata", None, C.c_int32, C.c_int32, C.c_uint32, C.c_int32, C.c_int32, C.c_void_p)(0, state, size, 0, 0, C.addressof(out))
    return {"made": out[0], "size": out[1], "new[]": out[3], "delete[]": out[4], "registered": int(state == 0), "answer": 1}


def fontdata_i_cases():
    for state in (0, 1):
        for size in (0, 1, 0x10, 0x7FF, 0x800, 0x801, 0x1000):
            yield (state, size)


def ppc_fontdata_dtor(e, case):
    size, flag, is_null = case
    e.alloc_log.clear()
    block = e.alloc(16)
    this = 0 if is_null else e.alloc(8, struct.pack(">II", size, block))
    r3, _, _ = T.call_ppc(e, FD_DTOR, [this, flag])
    heap = heap_ppc(e)
    return {"delete[]": heap[3], "delete": heap[2], "returns this": int(r3 == this)}


def host_fontdata_dtor(lib, case):
    size, flag, is_null = case
    out = (C.c_int64 * 8)()
    hostfn(lib, "tpw_fr_fontdata", None, C.c_int32, C.c_int32, C.c_uint32, C.c_int32, C.c_int32, C.c_void_p)(1, 0, size, flag, is_null, C.addressof(out))
    return {"delete[]": out[3], "delete": out[4], "returns this": 1}


def fontdata_dtor_cases():
    for flag in (1, 0, -1, 0x7FFF, 0x8000, 0x10000, 0x10001, 0xFFFF):
        yield (0x800, flag, 0)
        yield (0x800, flag, 1)


# ======================================================================= the iterators


def ppc_iterators(e, case):
    buffer, ops = case
    read = e.alloc(64, buffer)
    write = e.alloc(64, b"\xa5" * 64)
    iters = [e.alloc(8, bytes(8)) for _ in range(3)]
    writers = [e.alloc(12, bytes(12)) for _ in range(2)]
    bits = [e.alloc(8, bytes(8)) for _ in range(3)]
    for i in iters:
        T.call_ppc(e, NI_CTOR, [i, read, 0])
    for w in writers:
        T.call_ppc(e, NW_CTOR, [w, write, 0])
    for b in bits:
        T.call_ppc(e, BIT_CTOR, [b, read, 0])
    temp = e.alloc(8, bytes(8))
    out = []
    for kind, a, b, c in ops:
        a0 = a1 = 0
        if kind == 0:
            T.call_ppc(e, NI_CTOR, [iters[a], read + b, c])
        elif kind == 1:
            a0 = T.call_ppc(e, NI_DEREF, [iters[a]])[0] & 0xFF
        elif kind == 2:
            e.write(temp, bytes([0x99] * 8))
            T.call_ppc(e, NI_INC, [temp, iters[a], 0])
            a0, a1 = e.read(temp, 1)[0], e.u32(temp + 4) - read
        elif kind == 3:
            a0 = T.call_ppc(e, NI_CURRENT, [iters[a]])[0] & 0xFF
        elif kind == 4:
            T.call_ppc(e, NW_CTOR, [writers[a], write + b, c])
        elif kind == 5:
            T.call_ppc(e, NW_WRITE, [writers[a], b])
        elif kind == 6:
            a0 = T.call_ppc(e, NW_COUNT, [writers[a]])[0]
        elif kind == 7:
            T.call_ppc(e, BIT_CTOR, [bits[a], read + b, c])
        elif kind == 8:
            e.write(temp, bytes([0x99] * 8))
            T.call_ppc(e, BIT_DEREF, [temp, bits[a]])
            a0 = e.read(temp, 1)[0]
            a1 = T.call_ppc(e, BITREF, [temp])[0] & 0xFF
        elif kind == 9:
            e.write(temp, bytes([0x99] * 8))
            T.call_ppc(e, BIT_INC, [temp, bits[a], 0])
            a0, a1 = e.read(temp, 1)[0], e.u32(temp + 4) - read
        out += [a0, a1]
        for i in iters:
            out += [e.read(i, 1)[0], e.u32(i + 4) - read]
        for w in writers:
            out += [e.u32(w) - write, e.read(w + 4, 1)[0], e.read(w + 5, 1)[0], e.u32(w + 8)]
        for b2 in bits:
            out += [e.read(b2, 1)[0], e.u32(b2 + 4) - read]
        out += list(e.read(write, 64))
    return {"dump": out}


def host_iterators(lib, case):
    buffer, ops = case
    fn = hostfn(lib, "tpw_fr_iterators", C.c_int32, C.c_char_p, C.c_void_p, C.c_int32, C.c_void_p)
    flat = (C.c_int32 * (4 * len(ops)))(*[x for op in ops for x in op])
    out = (C.c_int64 * (len(ops) * 120))()
    n = fn(buffer, C.addressof(flat), len(ops), C.addressof(out))
    return {"dump": list(out[:n])}


def iterator_ops(r, n):
    ops = []
    for _ in range(n):
        kind = r.randrange(10)
        a = r.randrange(2 if kind in (4, 5, 6) else 3)
        if kind in (0, 4):
            ops.append((kind, a, r.randrange(0, 20), r.randrange(2)))
        elif kind == 7:
            ops.append((kind, a, r.randrange(0, 20), r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 31, 32, 33, 63, 64, 65, 255, r.randrange(256)])))
        elif kind == 5:
            ops.append((kind, a, r.choice([0, 1, 5, 0xA, 0xF, 0x10, 0x5F, 0xFF, r.randrange(256)]), 0))
        else:
            ops.append((kind, a, 0, 0))
    return ops


def iterator_random(r):
    return (bytes(r.getrandbits(8) for _ in range(64)), iterator_ops(r, r.randint(6, 14)))


def iterator_edges():
    r = random.Random(4503)
    pattern = bytes([0x00, 0xFF, 0xA5, 0x5A, 0x0F, 0xF0, 0x12, 0x34, 0x80, 0x01, 0x7E, 0xE7]) + bytes(r.getrandbits(8) for _ in range(52))
    # the bit iterator at every start bit, stepped a byte and a half
    for bit in list(range(10)) + [31, 32, 33, 39, 40, 63, 64, 65, 127, 128, 255]:
        yield (pattern, [(7, 0, 2, bit)] + [(9, 0, 0, 0), (8, 0, 0, 0)] * 9)
    # nybble readers from each flag, stepped
    for low in (0, 1):
        yield (pattern, [(0, 0, 3, low)] + [(2, 0, 0, 0), (1, 0, 0, 0), (3, 0, 0, 0)] * 5)
    # writers with and without the flag, 0 to 5 nybbles, with values that need masking
    for flag in (0, 1):
        for values in ([0x1], [0x1, 0x2], [0xAB, 0xCD, 0xEF], [0xFF, 0xFF, 0xFF, 0xFF, 0xFF], [0x10, 0x01, 0x11], []):
            yield (pattern, [(4, 0, 2, flag)] + [(5, 0, v, 0) for v in values] + [(6, 0, 0, 0)])
    for _ in range(200):
        yield iterator_random(r)


# ======================================================================= the render parameters' members


FONTS = 3
NPARAM = 3
PARAM_VT_SLOTS = (0x08, 0x0C, 0x30)


def param_persist(e):
    if hasattr(e, "frq"):
        return e.frq
    q = types.SimpleNamespace()
    e.frq = q

    def slot(table, offset, fn):
        e.write(table + offset, struct.pack(">I", T.fake_function(e, fn)))

    vt = T.persist_alloc(e, 0x40)
    slot(vt, 0x08, lambda e: e.frd.L.rec("Release", (e.frd.L.lab(e.r(3)),)))
    slot(vt, 0x0C, lambda e: e.frd.L.rec("AddRef", (e.frd.L.lab(e.r(3)),)))
    slot(vt, 0x30, lambda e: e.frd.gettype(e))
    q.fonts = [T.persist_alloc(e, 0x10, struct.pack(">I", vt) + bytes(12)) for _ in range(FONTS)]
    return q


class ParamsCase:
    def __init__(self, e):
        self.e = e
        e.frd = self
        q = self.q = param_persist(e)
        fixed = {a: 10 + i for i, a in enumerate(q.fonts)}
        self.L = e.cl = CL.PpcLog(fixed, {}, first_new=FIRST_NEW)
        self.type = {a: 0 for a in q.fonts}

    def gettype(self, e):
        font = e.r(3)
        self.L.rec("GetType", (self.L.lab(font),))
        e.set_r(3, T.u32(self.type[font]))


def ppc_params(e, case):
    c = ParamsCase(e)
    q = c.q
    objs = [e.alloc(0x50 + 32, b"\xa5" * (0x50 + 32)) for _ in range(NPARAM)]
    for i, o in enumerate(objs):
        c.L.labels[o] = 30 + i
    area = [0x7000 + i for i in range(10)]
    s = CL.Stubs()
    s.real.update(REAL)
    saved = s.activate(e)
    out = []
    try:
        for op in case:
            kind, a, b, cc = op[0], op[1], op[2], op[3]
            answer = 0
            if kind == 1:
                o = objs[a]
                if b == 1:
                    e.write(o + 4, struct.pack(">I", 0 if cc < 0 else q.fonts[cc]))
                elif b == 17:
                    e.write(o + 0x44, bytes((cc >> (8 * k)) & 0xFF for k in range(5)))
                else:
                    e.write(o + 4 * b, struct.pack(">I", cc & MINUS))
            elif kind == 2:
                T.call_ppc(e, P_ASSIGN, [objs[a], objs[b]])
            elif kind == 3:
                area_addr = e.alloc(0x28, struct.pack(">10I", *area))
                point = e.alloc(8, struct.pack(">II", b & MINUS, cc & MINUS))
                result = e.alloc(4, struct.pack(">I", 0x77777777))
                T.call_ppc(e, P_USETEX, [result, objs[a], area_addr, point])
                answer = s32(e.u32(result))
            elif kind == 4:
                area[:6] = [op[2 + k] & MINUS for k in range(6)]
            elif kind == 5:
                result = e.alloc(4, struct.pack(">I", 0x77777777))
                T.call_ppc(e, P_USEBLEND, [result, objs[a]])
                answer = s32(e.u32(result))
            elif kind == 6:
                answer = s32(T.call_ppc(e, P_GETSTYLE, [objs[a]])[0])
            elif kind == 7:
                answer = s32(T.call_ppc(e, P_GETTYPE, [objs[a]])[0])
            elif kind == 8:
                c.type[q.fonts[a]] = cc
            out.append(answer)
            for o in objs:
                font = e.u32(o + 4)
                out += [c.L.lab(font), e.u32(o + 8), e.u32(o + 12), e.u32(o + 16)]
                out += list(struct.unpack(">10I", e.read(o + 0x14, 40)))
                out += [e.u32(o + 0x3C), e.u32(o + 0x40)]
                out += list(e.read(o + 0x44, 5))
                out.append(e.u32(o + 0x4C))
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"dump": out, "log": c.L.log}


def host_params(lib, case):
    fn = hostfn(lib, "tpw_fr_params", C.c_int32, C.c_void_p, C.c_int32, C.c_void_p)
    CL.host_begin(lib, {}, {}, FIRST_NEW)
    flat = (C.c_int64 * (8 * len(case)))(*[x for op in case for x in op])
    out = (C.c_int64 * (len(case) * 70))()
    n = fn(C.addressof(flat), len(case), C.addressof(out))
    return {"dump": list(out[:n]), "log": CL.host_end(lib)}


def op8(*words):
    return tuple(words) + (0,) * (8 - len(words))


def params_script(r, steps):
    """A script of operations on three parameter objects: first every object gets a font word (or none), then random operations.
    An assignment needs a font in the source (the original takes a reference without testing)."""
    ops = []
    font = {}
    for i in range(NPARAM):
        f = r.choice([-1, 0, 1, 2, 0])
        ops.append(op8(1, i, 1, f))
        font[i] = f
    for i in range(FONTS):
        ops.append(op8(8, i, r.choice([0, 0xE, 4, 8, 2]), r.choice([0, 4, 5, 3])))
    for _ in range(steps):
        k = r.randrange(14)
        a = r.randrange(NPARAM)
        if k == 0:
            f = r.choice([-1, 0, 1, 2])
            ops.append(op8(1, a, 1, f))
            font[a] = f
        elif k == 1:
            ops.append(op8(1, a, 2, r.choice([0, 1, 2, 3, 4, 5, 7, 8, 0xC, 0xE, 0xF, MINUS, r.getrandbits(32)])))
        elif k == 2:
            ops.append(op8(1, a, r.choice([3, 4, 15, 16, 19]), r.choice([0, 1, 2, 3, 5, MINUS, r.getrandbits(32)])))
        elif k == 3:
            ops.append(op8(1, a, r.randint(5, 14), r.getrandbits(32)))
        elif k == 4:
            ops.append(op8(1, a, 17, r.getrandbits(40)))
        elif k in (5, 6):
            sources = [i for i in range(NPARAM) if font[i] >= 0]
            if sources:
                b = r.choice(sources)
                ops.append(op8(2, a, b))
                font[a] = font[b]
        elif k == 7:
            ops.append(op8(4, 0, *[r.getrandbits(32) for _ in range(6)]))
            ops.append(op8(3, a, r.randint(-50, 50), r.randint(-50, 50)))
        elif k == 8:
            ops.append(op8(3, a, r.getrandbits(32) & MINUS, r.getrandbits(32) & MINUS))
        elif k == 9:
            ops.append(op8(5, a))
        elif k == 10:
            ops.append(op8(6, a))
        elif k == 11:
            ops.append(op8(7, a))
        elif k == 12:
            ops.append(op8(8, r.randrange(FONTS), r.choice([0, 0xE, 4, 8, 12, 2]), r.choice([0, 4, 5, 3, MINUS, 9])))
        else:
            ops.append(op8(1, a, 2, r.choice([4, 8, 0xC, 0xE, 0xB, 0, 7])))
    return ops


def params_edges():
    r = random.Random(4504)
    # the capability bits one by one, with and without a font, for UseTexture and UseBlendedMode
    for caps in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0xFFFFFFF3, 0xFFFFFFFB, 0xFFFFFFF7, MINUS):
        for font in (-1, 0):
            yield [op8(1, 0, 1, font), op8(1, 0, 2, caps), op8(1, 0, 3, 3), op8(1, 0, 4, 1), op8(1, 1, 1, 0), op8(1, 2, 1, -1),
                   op8(4, 0, 1, 2, 3, 4, 16, 0), op8(3, 0, 11, -12), op8(5, 0), op8(6, 0), op8(7, 0), op8(3, 1, 5, 6), op8(5, 1), op8(7, 1),
                   op8(7, 2), op8(5, 2), op8(3, 2, 1, 1)]
    for _ in range(300):
        yield params_script(r, r.randint(6, 16))


def params_random(r):
    return params_script(r, r.randint(6, 16))


# ======================================================================= the tests


TESTS = [
    FnTest("Tb4BitFontRenderMethod::DrawChar", ppc_drawchar, host_drawchar, edge=lambda: list(drawchar_edges()), rnd=drawchar_random),
    FnTest("Tb4BitFontRenderMethod::CalculateSourceRect", ppc_calcrect, host_calcrect, edge=lambda: list(calcrect_edges()), rnd=calcrect_case),
    FnTest("Tb4BitFontRenderMethod::GetRotate", ppc_rotate, host_rotate, edge=lambda: list(rotate_edges()), rnd=rotate_random),
    FnTest("Tb4BitFontRenderMethod constructor and destructors", ppc_life, host_life, edge=lambda: list(life_edges()), rnd=life_random),
    FnTest("FourBit::FontData::I", ppc_fontdata_i, host_fontdata_i, edge=lambda: list(fontdata_i_cases()),
           rnd=lambda r: (r.randrange(2), r.choice([0, 1, 0x10, 0x7FF, 0x800, 0x801, 0x1000]))),
    FnTest("FourBit::FontData::~FontData", ppc_fontdata_dtor, host_fontdata_dtor, edge=lambda: list(fontdata_dtor_cases()),
           rnd=lambda r: (r.choice([0x800, 5]), r.choice([1, 0, -1, 0x8000, 0x10000, r.getrandbits(32)]), r.randrange(2))),
    FnTest("FourBit::NybbleIterator, NybbleWriter and OneBit::BitIterator (scripts)", ppc_iterators, host_iterators,
           edge=lambda: list(iterator_edges()), rnd=iterator_random),
    FnTest("TbFontRenderParams::operator=, UseTexture, UseBlendedMode, GetStyle and GetType (scripts)", ppc_params, host_params,
           edge=lambda: list(params_edges()), rnd=params_random),
]
