"""Unit w35b, the main-app half: the interface's text drawing, against libtpw_unit_UIDrawText.dylib (tests/unit_UIDrawText.cpp), in the
main app ("SimTheme Park"):
  src/UI_drawtext.cpp   UI_DrawText 0x1018b578, MakeRenderArea 0x1018b360, AdjustJustificationForHebrew 0x1018b2c8 (ui_utils.cpp),
                        UIMain_ConvertString 0x10181a7c (ui_main.cpp), InterfaceFont::GetWidth 0x1016915c, GetCharWidth 0x10169264 and
                        LastCharIn 0x101692d4

Every callee outside the unit is a fake that records the same event on both sides: the ltms text formatter (constructor, destructor, the
two justification setters, Draw), TbFontRenderParams::SetFont and UseReColour, CText::IsHebrew, the Bullfrog drawing calls LbDraw_SetCurrent
and LbDraw_SetClipRect, the Unicode to multibyte converter and wcslen. The converter's fake writes the low bytes of the characters into the
destination, the formatter's Draw writes an extent from the case's queue. The world UI_DrawText reads is the game's text object (the
resolution group, 52 fonts, the converter pointer), gUIRenderAreaInUse and the multibyte buffer, set by the case on both sides. Compared
after every call: the call log (names, objects by label, rectangles, colour bytes, texts), the extent the caller passed, the whole multibyte
buffer, the in-use word and the heap traffic (none). A font number outside 0..12 makes the original read the word at address 0x14; the
test maps a page of zeros at address 0 for such a case (and unmaps it afterwards), which is what the translation's NULL stands for.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIDrawText"
MASK = 0xFFFFFFFF
DRAW_TEXT, MAKE_AREA, ADJUST, CONVERT = 0x1018B578, 0x1018B360, 0x1018B2C8, 0x10181A7C
GET_WIDTH, GET_CHAR_WIDTH, LAST_CHAR_IN = 0x1016915C, 0x10169264, 0x101692D4
IS_HEBREW = 0x1015EA40          # CText::IsHebrew, called in the main app
UNIT = [DRAW_TEXT, MAKE_AREA, ADJUST, CONVERT, GET_WIDTH, GET_CHAR_WIDTH, LAST_CHAR_IN]
# the game's text object and the interface's globals (include/Text.h, include/UI_Base.h)
G_TEXT = 0x1037FF18
G_GROUP, G_FONTS, G_PARAMS, G_CONVERTER = G_TEXT + 4, G_TEXT + 0x1C, G_TEXT + 0x4FC, G_TEXT + 0x550
FONT_STRIDE, FONT_OFFSET = 0x18, 0x14
AREA, IN_USE, MB_BUFFER = 0x1038CD00, 0x1038CD28, 0x10388C48
MB_SIZE = 0x4000
CONVERTER = 0x7C400000          # the converter object: never dereferenced
MBFILL = 40

# ---- the imports and the in-binary callee the original calls
IMP_CTOR, IMP_DTOR = "__ct__15TbTextFormatterFv", "__dt__15TbTextFormatterFv"
IMP_HJUST = "SetHorizontalJustification__15TbTextFormatterFQ215TbTextFormatter14eJustification"
IMP_VJUST = "SetVerticalJustification__15TbTextFormatterFQ215TbTextFormatter14eJustification"
IMP_DRAW = "Draw__15TbTextFormatterFRC6TbRectPCUcR18TbFontRenderParamsP6TbRect"
IMP_FONT, IMP_COLOUR = "SetFont__18TbFontRenderParamsFP7TbIFont", "UseReColour__18TbFontRenderParamsFRC8TbColour"
IMP_CURRENT, IMP_CLIP = "LbDraw_SetCurrent__FP12TbRenderArea", "LbDraw_SetClipRect__FPC6TbRect"
IMP_CONVERT = "ConvertString__14TbIUnicodeToMBFPUcPCwUs"


def s32(v):
    return T.s32(v)


def font_token(n):
    return 0x7C300000 + 0x40 * (n - 1)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------- the fakes of the PowerPC side

def wide_chars(e, addr, limit=1 << 17):
    """The 16-bit characters at addr up to the terminator."""
    out = []
    while len(out) < limit:
        chunk = struct.unpack(">512H", e.read(addr + 2 * len(out), 1024))
        for ch in chunk:
            if ch == 0:
                return out
            out.append(ch)
    return out


def make_stubs(e):
    st = CL.Stubs()
    st.real = set(UNIT)

    def colour(em):
        L, a = em.cl, em.r(5)
        L.rec("UseReColour", [L.lab(em.r(4)), L.lab(a), *em.read(a, 5)])

    def clip(em):
        L, a = em.cl, em.r(3)
        L.rec("LbDraw_SetClipRect", [0, 0, 0, 0, 0] if a == 0 else [1, *struct.unpack(">4i", em.read(a, 16))])

    def draw(em):
        L = em.cl
        rect, text, extent = em.r(5), em.r(6), em.r(8)
        shown = CL.narrow(em.cstr(text, limit=200)[:159]) if text else "(null)"
        L.rec("Draw", [L.lab(em.r(4)), L.lab(rect), *struct.unpack(">4i", em.read(rect, 16)), L.lab(em.r(7)), L.lab(text), L.lab(extent)],
              [shown])
        if extent:
            em.write(extent, struct.pack(">4I", *[L.ret("Draw") & MASK for _ in range(4)]))

    def convert(em):
        L = em.cl
        this, out, source, size = em.r(4), em.r(5), em.r(6), em.r(7) & 0xFFFF
        chars = wide_chars(em, source)
        L.rec("ConvertString", [L.lab(this), L.lab(out), size, len(chars)], [CL.wide_text(em, source)])
        room = (size - 2) & 0xFFFF
        data = bytes(c & 0xFF for c in chars[:room]) + b"\0"
        em.write(out, data)

    def wcslen(em):
        source = em.r(3)
        chars = wide_chars(em, source)
        em.cl.rec("wcslen", [], [CL.wide_text(em, source)])
        em.set_r(3, len(chars))

    st.imp(IMP_CTOR, CL.recorder("TbTextFormatter::TbTextFormatter", "N"))
    st.imp(IMP_DTOR, CL.recorder("TbTextFormatter::~TbTextFormatter", "D"))
    st.imp(IMP_HJUST, CL.recorder("SetHorizontalJustification", "xLi"))
    st.imp(IMP_VJUST, CL.recorder("SetVerticalJustification", "xLi"))
    st.imp(IMP_FONT, CL.recorder("SetFont", "xLL"))
    st.imp(IMP_COLOUR, colour)
    st.imp(IMP_CURRENT, CL.recorder("LbDraw_SetCurrent", "L"))
    st.imp(IMP_CLIP, clip)
    st.imp(IMP_DRAW, draw)
    st.imp(IMP_CONVERT, convert)
    st.imp("wcslen", wcslen)
    st.internal(IS_HEBREW, CL.recorder("IsHebrew", "L", ret="i"))
    return st


def heap_ppc(e):
    kinds = [k for k, _, _ in e.alloc_log]
    return [kinds.count("new"), kinds.count("new[]"), kinds.count("delete"), kinds.count("delete[]")]


def heap_host(lib):
    return list((C.c_int32 * 4).in_dll(lib, "tpw_cl_heap"))


def heap_clear_host(lib):
    for i in range(4):
        (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")[i] = 0


def put_world(e, w):
    """The case's text object, in-use word and multibyte buffer, written into the emulator's memory."""
    e.write(G_GROUP, struct.pack(">I", w["group"]))
    for i, n in enumerate(w["fonts"]):
        e.write(G_FONTS + FONT_STRIDE * i + FONT_OFFSET, struct.pack(">I", font_token(n) if n else 0))
    e.write(G_CONVERTER, struct.pack(">I", CONVERTER if w["converter"] else 0))
    e.write(IN_USE, struct.pack(">I", w["in_use"] & MASK))
    e.write(MB_BUFFER, bytes(MB_SIZE))
    e.write(MB_BUFFER, bytes(w["mb_fill"]))


def fixed_labels(e, w, extra):
    fixed = {G_PARAMS: 1, AREA: 2, MB_BUFFER: 3, G_TEXT: 5, **extra}
    if w["converter"]:
        fixed[CONVERTER] = 4
    for n in set(w["fonts"]):
        if n:
            fixed[font_token(n)] = 10 + n - 1
    return fixed


def host_world(lib, w):
    fonts = (C.c_int32 * 52)(*w["fonts"])
    fill = (C.c_uint8 * len(w["mb_fill"]))(*w["mb_fill"])
    lib.tpw_dt_world(w["group"], fonts, 1 if w["converter"] else 0, T.s32(w["in_use"]), fill, len(w["mb_fill"]))


def host_mb(lib):
    buf = (C.c_uint8 * MB_SIZE)()
    lib.tpw_dt_mb(buf, MB_SIZE)
    return bytes(buf)


def wide_buffer(codes):
    return (C.c_uint16 * (len(codes) + 1))(*codes, 0)


def new_wide_codes(e, codes):
    return e.alloc(2 * len(codes) + 2, b"".join(struct.pack(">H", c) for c in codes) + b"\0\0")


# ---------------------------------------------------------------- UI_DrawText

def map_zero_page(e):
    """A page of zeros at address 0: what the original reads at 0x14 for a font number outside 0..12."""
    try:
        e.uc.mem_map(0, 0x1000)
    except Exception:
        pass
    e.write(0, bytes(0x1000))


def unmap_zero_page(e):
    try:
        e.uc.mem_unmap(0, 0x1000)
    except Exception:
        pass


def ppc_draw(e, c):
    w = c["world"]
    e.alloc_log.clear()
    put_world(e, w)
    text = new_wide_codes(e, c["text"]) if c["text"] is not None else 0
    rect = e.alloc(16, struct.pack(">4i", *c["rect"]))
    colour = e.alloc(5, bytes(c["colour"]))
    extent = e.alloc(16, b"\xa5" * 16) if c["extent"] else 0
    e.cl = CL.PpcLog(fixed_labels(e, w, {colour: 6}), queues={"IsHebrew": [c["hebrew"]], "Draw": c["draw"]})
    st = make_stubs(e)
    old = st.activate(e)
    out_of_range = not 0 <= T.s8(c["font"]) < 13
    if out_of_range:
        map_zero_page(e)
    try:
        T.call_ppc(e, DRAW_TEXT, [text, rect, colour, T.S8(c["font"]), T.S8(c["horizontal"]), T.S8(c["vertical"]), c["wrap"] & MASK, extent,
                                  c["current"] & MASK])
    finally:
        if out_of_range:
            unmap_zero_page(e)
        CL.Stubs.deactivate(e, old)
    return {"log": e.cl.log, "extent": list(struct.unpack(">4i", e.read(extent, 16))) if extent else None, "mb": e.read(MB_BUFFER, MB_SIZE),
            "in_use": e.u32(IN_USE), "heap": heap_ppc(e)}


def host_draw(lib, c):
    w = c["world"]
    CL.host_begin(lib, {}, queues={"IsHebrew": [c["hebrew"]], "Draw": c["draw"]})
    heap_clear_host(lib)
    host_world(lib, w)
    rect = (C.c_int32 * 4)(*c["rect"])
    colour = (C.c_uint8 * 5)(*c["colour"])
    extent = (C.c_int32 * 4)(*([T.s32(0xA5A5A5A5)] * 4)) if c["extent"] else None
    text = wide_buffer(c["text"]) if c["text"] is not None else None
    lib.tpw_dt_draw(text, rect, colour, c["font"], c["horizontal"], c["vertical"], c["wrap"], extent, c["current"])
    log = CL.host_end(lib)
    return {"log": log, "extent": list(extent) if extent is not None else None, "mb": host_mb(lib),
            "in_use": lib_in_use(lib), "heap": heap_host(lib)}


def lib_in_use(lib):
    return C.c_int32.in_dll(lib, "gUIRenderAreaInUse").value & MASK


def world(group=0, fonts=None, converter=True, in_use=1, mb_fill=None):
    return {"group": group, "fonts": fonts if fonts is not None else [i + 1 for i in range(52)], "converter": converter, "in_use": in_use,
            "mb_fill": mb_fill if mb_fill is not None else [0x30 + (i % 10) for i in range(MBFILL)]}


def draw_case(text="Hello", rect=(0, 0, 100, 20), colour=(255, 1, 2, 3, 4), font=0, horizontal=0, vertical=0, wrap=1, extent=True,
              current=0, hebrew=0, draw=(11, 12, 13, 14), w=None):
    codes = None if text is None else [ord(ch) for ch in text] if isinstance(text, str) else list(text)
    return {"text": codes, "rect": list(rect), "colour": list(colour), "font": font, "horizontal": horizontal, "vertical": vertical,
            "wrap": wrap, "extent": extent, "current": current, "hebrew": hebrew, "draw": list(draw), "world": w or world()}


def draw_edges():
    rects = [(0, 0, 100, 20), (10, 20, 110, 70), (0, 0, 0, 0), (50, 50, 10, 10), (-30, -40, 30, 40), (5, 5, 6, 6), (0, 0, 7, 9),
             (0x7FFFFF00, 0x7FFFFF00, 0x7FFFFFFF, 0x7FFFFFFF), (-0x80000000, -0x80000000, 0x7FFFFFFF, 0x7FFFFFFF),
             (0x7FFFFFFF, 0, -0x80000000, 5), (3, 4, 4, 5), (-1, -1, 0, 0), (1, 1, 200, 3)]
    # every justification pair with and without wrapping, in a Hebrew language and not, on several rectangles
    for hebrew in (0, 1):
        for horizontal in (-128, -2, -1, 0, 1, 2, 3, 4, 127):
            for vertical in (-128, -1, 0, 1, 2, 3, 100, 127):
                for wrap in (0, 1):
                    yield draw_case(horizontal=horizontal, vertical=vertical, wrap=wrap, hebrew=hebrew, rect=(7, 9, 77, 99))
    for rect in rects:
        for horizontal in (0, 1, 2):
            for wrap in (0, 1):
                yield draw_case(rect=rect, horizontal=horizontal, wrap=wrap, vertical=horizontal)
                yield draw_case(rect=rect, horizontal=horizontal, wrap=wrap, hebrew=1, current=1)
    # the early outs: nothing in use and drawing on our area, no text, and each of them with the other flag set
    for in_use in (0, 1, -1, 7):
        for current in (0, 1, -1, 99):
            for text in ("Hi", None, ""):
                yield draw_case(text=text, current=current, w=world(in_use=in_use))
    # the font number: every value of a signed byte that matters, in each resolution group
    for group in range(4):
        for font in list(range(-3, 18)) + [-128, 127, 100, -100, 64, 63]:
            yield draw_case(font=font, w=world(group=group), vertical=1, horizontal=1)
    # fonts that are not set
    yield draw_case(font=3, w=world(fonts=[0] * 52))
    yield draw_case(font=12, w=world(fonts=[1] * 52, group=3))
    # the converter missing: the multibyte buffer stays as it was
    for current in (0, 1):
        yield draw_case(w=world(converter=False), current=current)
        yield draw_case(w=world(converter=False, mb_fill=[]), current=current)
    # texts: long ones (the converter keeps at most 0x3ffd characters), characters outside ASCII and every control character
    yield draw_case(text="x" * 200)
    yield draw_case(text="y" * 0x3FFD)
    yield draw_case(text="z" * 0x3FFE)
    yield draw_case(text="w" * 0x4100)
    yield draw_case(text=[0x41, 0xE9, 0x100, 0x20AC, 0xFFFF, 0xD800, 0x7F, 0x80, 0xFF, 0x1, 0x1F])
    yield draw_case(text=list(range(1, 200)))
    # colours, draw results (the extent), no extent asked for
    for colour in ((0, 0, 0, 0, 0), (255, 255, 255, 255, 255), (1, 2, 3, 4, 5), (0x80, 0x7F, 0xFF, 0x01, 0xFE)):
        yield draw_case(colour=colour)
    for draw in ((0, 0, 0, 0), (-1, -2, -3, -4), (0x7FFFFFFF, -0x80000000, 1, 2), (5, 6, 7, 8)):
        yield draw_case(draw=draw)
        yield draw_case(draw=draw, extent=False)


def draw_rnd(r):
    kind = r.random()
    if kind < 0.1:
        text = None
    elif kind < 0.2:
        text = ""
    elif kind < 0.9:
        text = [r.choice([r.randint(0x20, 0x7E), r.randint(1, 0xFFFF), r.randint(0x20, 0x7E)]) for _ in range(r.randint(1, 60))]
    else:
        text = [r.randint(1, 0xFFFF) for _ in range(r.randint(0x3F00, 0x4100))]
    span = r.choice([50, 200, 2000])
    if r.random() < 0.15:
        rect = [r.choice([-0x80000000, 0x7FFFFFFF, 0, -1, r.getrandbits(32) - 0x80000000]) for _ in range(4)]
    else:
        rect = [r.randint(-span, span) for _ in range(4)]
    fonts = [r.choice([0, 0, r.randint(1, 52)]) for _ in range(52)] if r.random() < 0.3 else [i + 1 for i in range(52)]
    mb_fill = [r.getrandbits(8) for _ in range(r.randint(0, MBFILL))]
    return draw_case(text=text, rect=rect, colour=[r.getrandbits(8) for _ in range(5)],
                     font=r.choice([r.randint(0, 12), r.randint(0, 12), r.randint(-128, 127)]),
                     horizontal=r.choice([0, 1, 2, r.randint(-128, 127)]), vertical=r.choice([0, 1, 2, r.randint(-128, 127)]),
                     wrap=r.choice([0, 1, 0, r.randint(-5, 5)]), extent=r.random() < 0.7, current=r.choice([0, 0, 1, r.randint(-3, 3)]),
                     hebrew=r.randint(0, 1), draw=[r.randint(-1000, 1000) for _ in range(4)],
                     w=world(group=r.randint(0, 3), fonts=fonts, converter=r.random() < 0.8, in_use=r.choice([0, 1, 1, r.randint(-3, 3)]),
                             mb_fill=mb_fill))


# ---------------------------------------------------------------- UIMain_ConvertString

def ppc_convert(e, c):
    w = c["world"]
    e.alloc_log.clear()
    put_world(e, w)
    text = new_wide_codes(e, c["text"]) if c["text"] is not None else 0
    e.cl = CL.PpcLog(fixed_labels(e, w, {}))
    st = make_stubs(e)
    old = st.activate(e)
    try:
        r3, _, _ = T.call_ppc(e, CONVERT, [text])
    finally:
        CL.Stubs.deactivate(e, old)
    return {"log": e.cl.log, "is_buffer": int(r3 == MB_BUFFER), "mb": e.read(MB_BUFFER, MB_SIZE), "heap": heap_ppc(e)}


def host_convert(lib, c):
    w = c["world"]
    CL.host_begin(lib, {})
    heap_clear_host(lib)
    host_world(lib, w)
    text = wide_buffer(c["text"]) if c["text"] is not None else None
    lib.tpw_dt_convert.restype = C.c_int32
    answer = lib.tpw_dt_convert(text)
    return {"log": CL.host_end(lib), "is_buffer": answer, "mb": host_mb(lib), "heap": heap_host(lib)}


def convert_case(text="abc", w=None):
    codes = None if text is None else [ord(ch) for ch in text] if isinstance(text, str) else list(text)
    return {"text": codes, "world": w or world()}


def convert_edges():
    for text in ("", "a", "Hello, world", "x" * 200):
        for converter in (True, False):
            for fill in ([], [0x41] * 5, [0xFF] * MBFILL):
                yield convert_case(text, world(converter=converter, mb_fill=fill))
    yield convert_case(None, world(converter=False))
    yield convert_case(None, world(converter=False, mb_fill=[]))
    for n in (0x3FFC, 0x3FFD, 0x3FFE, 0x3FFF, 0x4000, 0x4001, 0x8000):
        yield convert_case("q" * n)
    yield convert_case([0x41, 0xE9, 0x100, 0x20AC, 0xFFFF, 0xD800, 0x7F, 0x80, 0xFF, 0x1, 0x1F])


def convert_rnd(r):
    n = r.choice([r.randint(0, 40), r.randint(0, 300), r.randint(0x3FF0, 0x4010)])
    text = None if r.random() < 0.05 else [r.randint(1, 0xFFFF) for _ in range(n)]
    converter = r.random() < 0.8
    if text is None:
        converter = False
    return convert_case(text, world(converter=converter, mb_fill=[r.getrandbits(8) for _ in range(r.randint(0, MBFILL))]))


# ---------------------------------------------------------------- MakeRenderArea

def ppc_make(e, c):
    e.alloc_log.clear()
    gei = e.data_import("gei_TextureSettings")
    e.write(gei + 0x30, struct.pack(">8I", *[v & MASK for v in c["format"]]))
    area = e.alloc(0x28, b"\xa5" * 0x28)
    T.call_ppc(e, MAKE_AREA, [c["pixels"] & MASK, area if c["has_area"] else 0, c["width"] & MASK, c["height"] & MASK])
    return {"area": list(struct.unpack(">10I", e.read(area, 0x28))) if c["has_area"] else None,
            "format": list(struct.unpack(">8I", e.read(gei + 0x30, 32))), "heap": heap_ppc(e)}


def host_make(lib, c):
    CL.host_begin(lib, {})
    heap_clear_host(lib)
    fmt = (C.c_uint32 * 8)(*[v & MASK for v in c["format"]])
    out = (C.c_uint32 * 10)()
    lib.tpw_dt_make.argtypes = [C.c_int32, C.c_uint32, C.c_int32, C.c_int32, C.c_void_p, C.c_void_p]
    lib.tpw_dt_make(1 if c["has_area"] else 0, c["pixels"] & MASK, T.s32(c["width"]), T.s32(c["height"]), fmt, out)
    CL.host_end(lib)
    return {"area": list(out) if c["has_area"] else None, "format": list(fmt), "heap": heap_host(lib)}


def make_case(pixels=0x40000000, width=64, height=48, format=(32, 0x41, 0, 16, 0xF000, 0x0F00, 0x00F0, 0x000F), has_area=True):
    return {"pixels": pixels, "width": width, "height": height, "format": list(format), "has_area": has_area}


def make_edges():
    formats = [(32, 0x41, 0, 16, 0xF000, 0x0F00, 0x00F0, 0x000F), (32, 0x40, 0, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000),
               (32, 0x20, 0, 8, 0, 0, 0, 0), (32, 0x40, 0, 24, 0xFF0000, 0xFF00, 0xFF, 0), (32, 0x60, 0, 15, 0x7C00, 0x3E0, 0x1F, 0),
               (0, 0, 0, 0, 0, 0, 0, 0), (0xFFFFFFFF,) * 8, (32, 0x1F, 5, 7, 1, 2, 3, 4), (32, 0xFFFFFFDF, 0, 0xFFFFFFFF, 9, 8, 7, 6),
               (32, 0x20, 0, 9, 0, 0, 0, 0), (32, 0x20, 0, 0x80000000, 1, 1, 1, 1)]
    for fmt in formats:
        for width, height in ((64, 48), (0, 0), (1, 1), (-5, 7), (7, -5), (0x7FFFFFFF, 3), (-0x80000000, -0x80000000), (4096, 4096), (3, 0x10000000)):
            yield make_case(width=width, height=height, format=fmt)
    yield make_case(has_area=False)
    yield make_case(has_area=False, format=formats[1])
    for pixels in (0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x12345678):
        yield make_case(pixels=pixels)


def make_rnd(r):
    return make_case(pixels=r.getrandbits(32), width=r.choice([r.randint(-5, 300), r.getrandbits(32) - 0x80000000]),
                     height=r.choice([r.randint(-5, 300), r.getrandbits(32) - 0x80000000]),
                     format=[r.getrandbits(32) for _ in range(8)] if r.random() < 0.6 else
                     [32, r.choice([0x20, 0x40, 0x41, 0x60, r.getrandbits(32)]), 0, r.choice([8, 15, 16, 24, 32, r.getrandbits(32)])] +
                     [r.getrandbits(32) for _ in range(4)], has_area=r.random() < 0.9)


# ---------------------------------------------------------------- AdjustJustificationForHebrew

def ppc_adjust(e, c):
    e.alloc_log.clear()
    word = e.alloc(4, struct.pack(">i", c["value"]))
    e.cl = CL.PpcLog({G_TEXT: 5}, queues={"IsHebrew": [c["hebrew"]]})
    st = make_stubs(e)
    old = st.activate(e)
    try:
        T.call_ppc(e, ADJUST, [word])
    finally:
        CL.Stubs.deactivate(e, old)
    return {"value": struct.unpack(">i", e.read(word, 4))[0], "log": e.cl.log, "heap": heap_ppc(e)}


def host_adjust(lib, c):
    CL.host_begin(lib, {}, queues={"IsHebrew": [c["hebrew"]]})
    heap_clear_host(lib)
    host_world(lib, world())
    lib.tpw_dt_adjust.restype = C.c_int32
    value = lib.tpw_dt_adjust(c["value"])
    return {"value": value, "log": CL.host_end(lib), "heap": heap_host(lib)}


def adjust_edges():
    for hebrew in (0, 1):
        for value in (0, 1, 2, 3, -1, -2, 4, 100, 0x7FFFFFFF, -0x80000000, 0x100, 0x10000, 0x100000002):
            yield {"value": T.s32(value), "hebrew": hebrew}


def adjust_rnd(r):
    return {"value": r.choice([r.randint(-3, 5), T.s32(r.getrandbits(32))]), "hebrew": r.randint(0, 1)}


# ---------------------------------------------------------------- InterfaceFont's widths

# One glyph table of 65,536 entries of 8 bytes for every case (a character the table does not cover reads the next one): the first byte
# of an entry is the width (a signed byte), the rest is noise. Written once into persistent memory on the PowerPC side.
TABLE = random.Random(35).randbytes(0x10000 * 8)
TABLE_ADDR = []


def table_address(e):
    if not TABLE_ADDR:
        TABLE_ADDR.append(T.persist_alloc(e, len(TABLE), TABLE))
    return TABLE_ADDR[0]


def ppc_font(e, c):
    e.alloc_log.clear()
    table = table_address(e)
    font = e.alloc(0x20, b"\xa5" * 0x20)
    e.write(font, struct.pack(">HH", c["count"] & 0xFFFF, c["spacing"] & 0xFFFF))
    e.write(font + 8, struct.pack(">I", table if c["has_table"] else 0))
    text = new_wide_codes(e, c["text"]) if c["text"] is not None else 0
    e.cl = CL.PpcLog({})
    st = make_stubs(e)
    old = st.activate(e)
    try:
        if c["op"] == 0:
            r3, _, _ = T.call_ppc(e, GET_WIDTH, [font, text, T.S16(c["first"]), T.S16(c["last"])])
        elif c["op"] == 1:
            r3, _, _ = T.call_ppc(e, GET_CHAR_WIDTH, [font, T.U16(c["char"])])
        else:
            r3, _, _ = T.call_ppc(e, LAST_CHAR_IN, [font, text, T.S16(c["first"]), T.S16(c["last"])])
    finally:
        CL.Stubs.deactivate(e, old)
    pointer = e.u32(font + 8)
    return {"answer": s32(r3), "log": e.cl.log, "font": [e.read(font, 2)[0] << 8 | e.read(font, 2)[1], e.read(font + 2, 2)[0] << 8 | e.read(font + 2, 2)[1],
                                                         2 if pointer == 0 else 1 if pointer == table else 0], "heap": heap_ppc(e)}


def host_font(lib, c):
    CL.host_begin(lib, {})
    heap_clear_host(lib)
    if not hasattr(host_font, "table"):
        host_font.table = C.create_string_buffer(TABLE, len(TABLE))
    text = wide_buffer(c["text"]) if c["text"] is not None else None
    if c["op"] == 1:
        text = wide_buffer([c["char"]])
    lib.tpw_dt_font.restype = C.c_int32
    after = (C.c_int32 * 3)()
    answer = lib.tpw_dt_font(c["op"], c["count"] & 0xFFFF, c["spacing"] & 0xFFFF, 1 if c["has_table"] else 0, host_font.table, text, c["first"],
                             c["last"], after)
    return {"answer": answer, "log": CL.host_end(lib), "font": list(after), "heap": heap_host(lib)}


def font_case(op, text, first=0, last=-1, count=256, spacing=2, has_table=True, char=0):
    codes = None if text is None else [ord(ch) for ch in text] if isinstance(text, str) else list(text)
    return {"op": op, "text": codes, "first": first, "last": last, "count": count, "spacing": spacing, "has_table": has_table, "char": char}


def font_edges():
    texts = ["Hello", "", "a", "ab", "abc", "Theme Park World", [0, 1, 255, 256, 0xFFFF, 0x7F, 0x80]]
    for text in texts:
        n = len(text)
        ranges = [(0, -1), (0, n - 1), (0, n), (0, n + 1), (-1, 3), (-1, -1), (n, n + 1), (n + 1, n + 3), (2, 2), (3, 1), (1, 3), (0, 1),
                  (1, 2), (n - 1, n), (n - 1, n - 1), (n - 2, n - 1), (n - 2, n), (0, 0), (1, 0), (2, 4), (-32768, 32767), (32767, -32768),
                  (n, -1), (n, 0), (n + 1, -1), (-5, -3)]
        for first, last in ranges:
            for has_table in (True, False):
                yield font_case(0, text, first, last, has_table=has_table)
            yield font_case(0, text, first, last, spacing=0xFFFF)
            yield font_case(0, text, first, last, spacing=0, count=0)
    yield font_case(0, None, 0, 3)
    yield font_case(0, None, 0, 3, has_table=False)
    # texts that make the 16-bit positions wrap: 40,000 and 70,000 characters
    for n in (32766, 32767, 32768, 32769, 40000, 65535, 65536, 70000):
        yield font_case(0, [0x41 + (i % 50) for i in range(n)], 0, -1)
        yield font_case(0, [0x41 + (i % 50) for i in range(n)], 1, 5)
    for ch in (0, 1, 0x41, 0xFE, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF):
        for count in (0, 1, 0x41, 0x100, 0xFFFF, 0xFFFE):
            for has_table in (True, False):
                yield font_case(1, None, count=count, char=ch, has_table=has_table)
    for spacing in (0, 1, 2, 0xFFFF, 0x8000, 0x7FFF):
        yield font_case(1, None, count=0x100, char=0x20, spacing=spacing)
        yield font_case(0, "Spacing test", 0, -1, spacing=spacing)
    for text in ("", "a", "Hello", None):
        yield font_case(2, text, 0, 3)
        yield font_case(2, text, -5, 70000 % 32768, has_table=False)


def font_rnd(r):
    op = r.choice([0, 0, 0, 1, 1, 2])
    n = r.choice([r.randint(0, 6), r.randint(0, 40), r.randint(0, 200)])
    text = None if r.random() < 0.05 else [r.choice([r.randint(0x20, 0x7E), r.randint(1, 0xFFFF), r.randint(1, 300)]) for _ in range(n)]
    length = len(text) if text is not None else 0
    first = r.choice([r.randint(-2, length + 2), r.randint(-40000, 40000), 0])
    last = r.choice([r.randint(-2, length + 2), -1, length - 1, length, r.randint(-40000, 40000)])
    return font_case(op, text, T.s16(first), T.s16(last), count=r.choice([0, 0x41, 0x100, 0xFFFF, r.getrandbits(16)]),
                     spacing=r.choice([0, 2, r.getrandbits(16)]), has_table=r.random() < 0.9, char=r.choice([r.randint(0, 300), r.getrandbits(16)]))


TESTS = [
    FnTest("UI_DrawText", ppc_draw, host_draw, edge=lambda: list(draw_edges()), rnd=draw_rnd),
    FnTest("UIMain_ConvertString", ppc_convert, host_convert, edge=lambda: list(convert_edges()), rnd=convert_rnd),
    FnTest("MakeRenderArea", ppc_make, host_make, edge=lambda: list(make_edges()), rnd=make_rnd),
    FnTest("AdjustJustificationForHebrew", ppc_adjust, host_adjust, edge=lambda: list(adjust_edges()), rnd=adjust_rnd),
    FnTest("InterfaceFont::GetWidth, GetCharWidth and LastCharIn", ppc_font, host_font, edge=lambda: list(font_edges()), rnd=font_rnd),
]
