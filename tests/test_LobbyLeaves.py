"""Unit w40, the main-app half: the lobby's remaining leaves, against libtpw_unit_LobbyLeaves.dylib (tests/unit_LobbyLeaves.cpp), in the main
app ("SimTheme Park"):
  src/game/ui/ui_main.cpp      GetFontTableEntry 0x10180270, UI_CreateVersionNumber 0x101874d0
  src/game/ui/ui_utils.cpp      UI_MeasureText 0x1018b8e4
  src/game/online/lobby.cpp      LB_Lobby_MailIconMessagesFn 0x100a59ec
  src/game/online/FS_Online.cpp  CGameType::CGameType 0x1016f5d8, CGameType::SetGameType 0x1016f6a0
(the ltms string buffer is test_LobbyLeavesLtms.py, in the ltms library, against the same host library).

Every callee outside the unit is a fake that records the same event on both sides: the ltms text formatter (constructor, destructor, the two
justification setters, Measure), TbFontRenderParams::SetFont, CText::IsHebrew, UIMain_ConvertString (it writes the low bytes of the characters
into the multibyte buffer so that what reaches Measure shows), the window's CreateObject, SetClientDrawObject, SetFont, SetClientDrawText and
CallDefaultWindowHandler, the text draw object's constructor, Sam's Utilities' UString and UVersion, swprintf, the online system's constructor
and AreWeLoggedIn, CreateOnlineOutboxPanel, UIMain_GetString, UI_DialogBox and GAME_ASSERTMSG. UI_CreateVersionNumber runs UI_MeasureText for
real on both sides, so the measured size (the case's queue "Measure") drives the window rectangle. The world is the game's text object (the
resolution group, the 52 font slots), the interface's windows and screen size, the online system and the option word, set by the case on
both sides. Compared after every call: the call log (names, objects by label, values, rectangles and texts), the answer, the caller's
extent, the multibyte buffer, the option word and the heap traffic. A font number outside 0..12 makes UI_MeasureText read the word at
address 0x14; the test maps a page of zeros at address 0 for such a case, which is what the translation's NULL stands for.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyLeaves"
MASK = 0xFFFFFFFF

GET_FONT, MEASURE, VERSION, MAIL = 0x10180270, 0x1018B8E4, 0x101874D0, 0x100A59EC
GT_CTOR, GT_SET = 0x1016F5D8, 0x1016F6A0
IS_HEBREW, CONVERT = 0x1015EA40, 0x10181A7C
CREATE_OBJECT, TEXT_SPRITE_CTOR, SET_CLIENT_DRAW_OBJECT = 0x101DC750, 0x1018D11C, 0x101DB624
WINDOW_SET_FONT, SET_CLIENT_DRAW_TEXT, DEFAULT_HANDLER = 0x101DA118, 0x101DE130, 0x101DE318
ONLINE_CTOR, ARE_WE_LOGGED_IN, OUTBOX, GET_STRING, DIALOG = 0x1016F7D0, 0x1017178C, 0x101E4F28, 0x10180468, 0x101F1800
ASSERT = 0x1000DDE8
# the game's text object, the interface's globals (include/Text.h, include/UI_Base.h, include/lobby.h, include/FS_Online.h, include/TPMain.h)
G_TEXT = 0x1037FF18
G_GROUP, G_FONTS, G_PARAMS = G_TEXT + 4, G_TEXT + 0x1C, G_TEXT + 0x4FC
FONT_STRIDE, FONT_OFFSET = 0x18, 0x14
MB_BUFFER, MB_SIZE = 0x10388C48, 0x4000
G_ROOT, G_LOBBY, G_SCREEN_W, G_SCREEN_H = 0x10395C24, 0x102E4888, 0x10395BBC, 0x10395BBA
G_ONLINE, G_OPTIONS = 0x10380AB0, 0x102B2B48
DEFAULT_RESOURCE = "kDefaultVersionResource__Q213SamsUtilities8UVersion"
# addresses the PowerPC side uses as objects: never dereferenced by the code under test
ROOT_WIN, LOBBY_WIN, CREATED_WIN, MAIL_WIN = 0x7C500000, 0x7C500100, 0x7C500200, 0x7C500300
MBSHOWN = 128

IMP_CTOR, IMP_DTOR = "__ct__15TbTextFormatterFv", "__dt__15TbTextFormatterFv"
IMP_HJUST = "SetHorizontalJustification__15TbTextFormatterFQ215TbTextFormatter14eJustification"
IMP_VJUST = "SetVerticalJustification__15TbTextFormatterFQ215TbTextFormatter14eJustification"
IMP_MEASURE = "Measure__15TbTextFormatterFRC6TbRectPCUcR18TbFontRenderParamsP6TbRect"
IMP_FONT = "SetFont__18TbFontRenderParamsFP7TbIFont"
IMP_UVERSION = "__ct__Q213SamsUtilities8UVersionFUsUcUcUs"
IMP_LOAD = "LoadFromResource__Q213SamsUtilities8UVersionFRCQ213SamsUtilities11UResourceIdUc"
IMP_USTRING, IMP_USTRING_DTOR = "__ct__Q213SamsUtilities7UStringFv", "__dt__Q213SamsUtilities7UStringFv"
IMP_USTRING_ASSIGN = "__as__Q213SamsUtilities7UStringFRCQ213SamsUtilities7UString"


def s32(v):
    return T.s32(v)


def font_token(n):
    return 0x7C300000 + 0x40 * (n - 1)


def setup(runner):
    T.install_std_stubs(runner.emu)


def heap_ppc(e):
    kinds = [k for k, _, _ in e.alloc_log]
    return [kinds.count("new"), kinds.count("new[]"), kinds.count("delete"), kinds.count("delete[]")]


def heap_host(lib):
    return list((C.c_int32 * 4).in_dll(lib, "tpw_cl_heap"))


def heap_clear_host(lib):
    for i in range(4):
        (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")[i] = 0


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


def new_wide_codes(e, codes):
    return e.alloc(2 * len(codes) + 2, b"".join(struct.pack(">H", c) for c in codes) + b"\0\0")


def wide_buffer(codes):
    return (C.c_uint16 * (len(codes) + 1))(*codes, 0)


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


# ---------------------------------------------------------------- the world of the text object

def world(group=0, fonts=None):
    return {"group": group, "fonts": fonts if fonts is not None else [i + 1 for i in range(52)]}


def put_world(e, w):
    """The case's resolution group and font slots, and a clean multibyte buffer, in the emulator's memory."""
    e.write(G_GROUP, struct.pack(">I", w["group"]))
    for i, n in enumerate(w["fonts"]):
        e.write(G_FONTS + FONT_STRIDE * i + FONT_OFFSET, struct.pack(">I", font_token(n) if n else 0))
    e.write(MB_BUFFER, bytes(MB_SIZE))


def fixed_labels(w, extra=None):
    fixed = {G_PARAMS: 1, MB_BUFFER: 3, G_TEXT: 5, **(extra or {})}
    for i in range(52):
        fixed[G_FONTS + FONT_STRIDE * i] = 10 + i
    for n in set(w["fonts"]):
        if n:
            fixed[font_token(n)] = 62 + n - 1
    return fixed


def host_world(lib, w):
    fonts = (C.c_int32 * 52)(*w["fonts"])
    lib.tpw_ll_text_world(w["group"], fonts)


def host_mb(lib):
    buf = (C.c_uint8 * MBSHOWN)()
    lib.tpw_ll_mb(buf, MBSHOWN)
    return bytes(buf)


# ---------------------------------------------------------------- the PowerPC side's fakes

def measure_stubs(st):
    """The callees of UI_MeasureText: the formatter, the font parameters, CText::IsHebrew and UIMain_ConvertString."""
    def measure(em):
        L = em.cl
        rect, text, extent = em.r(5), em.r(6), em.r(8)
        shown = CL.narrow(em.cstr(text, limit=200)[:159]) if text else "(null)"
        L.rec("Measure", [L.lab(em.r(4)), L.lab(rect), *struct.unpack(">4i", em.read(rect, 16)), L.lab(em.r(7)), L.lab(text), L.lab(extent)],
              [shown])
        if extent:
            em.write(extent, struct.pack(">4I", *[L.ret("Measure") & MASK for _ in range(4)]))

    def convert(em):
        text = em.r(3)
        chars = wide_chars(em, text) if text else []
        em.cl.rec("UIMain_ConvertString", [], [CL.wide_text(em, text) if text else "(null)"])
        em.write(MB_BUFFER, bytes(c & 0xFF for c in chars[:MB_SIZE - 2]) + b"\0")
        em.set_r(3, MB_BUFFER)

    st.imp(IMP_CTOR, CL.recorder("TbTextFormatter::TbTextFormatter", "N"))
    st.imp(IMP_DTOR, CL.recorder("TbTextFormatter::~TbTextFormatter", "D"))
    st.imp(IMP_HJUST, CL.recorder("SetHorizontalJustification", "xLi"))
    st.imp(IMP_VJUST, CL.recorder("SetVerticalJustification", "xLi"))
    st.imp(IMP_FONT, CL.recorder("SetFont", "xLL"))
    st.imp(IMP_MEASURE, measure)
    st.internal(IS_HEBREW, CL.recorder("IsHebrew", "L", ret="i"))
    st.internal(CONVERT, convert)


# ---------------------------------------------------------------- GetFontTableEntry

def ppc_font(e, c):
    e.write(G_GROUP, struct.pack(">I", c["group"]))
    value = T.call_ppc(e, GET_FONT, [c["index"] & MASK])[0]
    if value == 0:
        return {"entry": -1}
    offset = (value - (G_TEXT + 0x1C)) & MASK
    assert offset % FONT_STRIDE == 0, hex(value)
    return {"entry": offset // FONT_STRIDE}


def host_font(lib, c):
    lib.tpw_ll_text_world.argtypes = [C.c_int32, C.POINTER(C.c_int32)]
    lib.tpw_ll_font_entry.restype = C.c_int64
    lib.tpw_ll_font_entry.argtypes = [C.c_uint32]
    CL.host_begin(lib, {})
    host_world(lib, world(c["group"]))
    return {"entry": lib.tpw_ll_font_entry(c["index"] & MASK)}


def font_edges():
    for group in range(6):
        for index in list(range(0, 20)) + [31, 32, 100, 255, 256, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]:
            yield {"index": index, "group": group}


def font_rnd(r):
    return {"index": r.choice([r.randint(0, 12), r.randint(0, 12), r.randint(0, 40), r.getrandbits(32)]), "group": r.choice([0, 1, 2, 3, 3, r.randint(0, 5)])}


# ---------------------------------------------------------------- UI_MeasureText

def measure_case(text="Hello", rect=(0, 0, 100, 20), font=0, horizontal=0, vertical=0, wrap=1, extent=True, hebrew=0,
                 measure=(11, 12, 13, 14), w=None):
    codes = None if text is None else [ord(ch) for ch in text] if isinstance(text, str) else list(text)
    return {"text": codes, "rect": list(rect), "font": font, "horizontal": horizontal, "vertical": vertical, "wrap": wrap,
            "extent": extent, "hebrew": hebrew, "measure": list(measure), "world": w or world()}


def ppc_measure(e, c):
    w = c["world"]
    e.alloc_log.clear()
    put_world(e, w)
    text = new_wide_codes(e, c["text"]) if c["text"] is not None else 0
    rect = e.alloc(16, struct.pack(">4i", *c["rect"]))
    extent = e.alloc(16, b"\xa5" * 16) if c["extent"] else 0
    e.cl = CL.PpcLog(fixed_labels(w), queues={"IsHebrew": [c["hebrew"]], "Measure": c["measure"]}, first_new=200)
    st = CL.Stubs()
    st.real = {MEASURE}
    measure_stubs(st)
    old = st.activate(e)
    out_of_range = not 0 <= T.s8(c["font"]) < 13
    if out_of_range:
        map_zero_page(e)
    try:
        T.call_ppc(e, MEASURE, [text, rect, T.S8(c["font"]), T.S8(c["horizontal"]), T.S8(c["vertical"]), c["wrap"] & MASK, extent])
    finally:
        if out_of_range:
            unmap_zero_page(e)
        CL.Stubs.deactivate(e, old)
    return {"log": e.cl.log, "extent": list(struct.unpack(">4i", e.read(extent, 16))) if extent else None,
            "mb": e.read(MB_BUFFER, MBSHOWN), "heap": heap_ppc(e)}


def host_measure(lib, c):
    w = c["world"]
    lib.tpw_ll_measure.argtypes = [C.c_void_p, C.POINTER(C.c_int32), C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.POINTER(C.c_int32)]
    CL.host_begin(lib, {}, queues={"IsHebrew": [c["hebrew"]], "Measure": c["measure"]}, first_new=200)
    heap_clear_host(lib)
    host_world(lib, w)
    ctypes_rect = (C.c_int32 * 4)(*c["rect"])
    extent = (C.c_int32 * 4)(*([T.s32(0xA5A5A5A5)] * 4)) if c["extent"] else None
    text = wide_buffer(c["text"]) if c["text"] is not None else None
    lib.tpw_ll_measure(text, ctypes_rect, c["font"], c["horizontal"], c["vertical"], c["wrap"], extent)
    log = CL.host_end(lib)
    return {"log": log, "extent": list(extent) if extent is not None else None, "mb": host_mb(lib), "heap": heap_host(lib)}


def measure_edges():
    rects = [(0, 0, 100, 20), (10, 20, 110, 70), (0, 0, 0, 0), (50, 50, 10, 10), (-30, -40, 30, 40), (5, 5, 6, 6), (0, 0, 7, 9),
             (0x7FFFFF00, 0x7FFFFF00, 0x7FFFFFFF, 0x7FFFFFFF), (-0x80000000, -0x80000000, 0x7FFFFFFF, 0x7FFFFFFF),
             (0x7FFFFFFF, 0, -0x80000000, 5), (3, 4, 4, 5), (-1, -1, 0, 0), (1, 1, 200, 3), (0, 0, 32000, 32000)]
    # every justification pair with and without wrapping, in a Hebrew language and not
    for hebrew in (0, 1):
        for horizontal in (-128, -2, -1, 0, 1, 2, 3, 4, 127):
            for vertical in (-128, -1, 0, 1, 2, 3, 100, 127):
                for wrap in (0, 1):
                    yield measure_case(horizontal=horizontal, vertical=vertical, wrap=wrap, hebrew=hebrew, rect=(7, 9, 77, 99))
    for rect in rects:
        for horizontal in (0, 1, 2):
            for wrap in (0, 1, -1, 5):
                yield measure_case(rect=rect, horizontal=horizontal, wrap=wrap, vertical=horizontal)
                yield measure_case(rect=rect, horizontal=horizontal, wrap=wrap, hebrew=1)
    # no text (the extent is not written), an empty text, no extent asked for
    for hebrew in (0, 1):
        yield measure_case(text=None, hebrew=hebrew)
        yield measure_case(text=None, extent=False)
        yield measure_case(text="", hebrew=hebrew)
        yield measure_case(text="x", extent=False, hebrew=hebrew)
    # the font number: every value of a signed byte that matters, in each resolution group
    for group in range(4):
        for font in list(range(-3, 18)) + [-128, 127, 100, -100, 64, 63]:
            yield measure_case(font=font, w=world(group=group), vertical=1, horizontal=1)
    # fonts that are not set
    yield measure_case(font=3, w=world(fonts=[0] * 52))
    yield measure_case(font=12, w=world(fonts=[1] * 52, group=3))
    # texts: long ones, characters outside ASCII (their low bytes reach the formatter) and a character whose low byte is 0
    yield measure_case(text="x" * 200)
    yield measure_case(text="y" * 0x3FFD)
    yield measure_case(text="w" * 0x4100)
    yield measure_case(text=[0x41, 0xE9, 0x100, 0x20AC, 0xFFFF, 0xD800, 0x7F, 0x80, 0xFF, 0x1, 0x1F])
    yield measure_case(text=list(range(1, 200)))
    # what the formatter measured
    for measure in ((0, 0, 0, 0), (-1, -2, -3, -4), (0x7FFFFFFF, -0x80000000, 1, 2), (5, 6, 7, 8)):
        yield measure_case(measure=measure)
        yield measure_case(measure=measure, extent=False)


def measure_rnd(r):
    kind = r.random()
    if kind < 0.1:
        text = None
    elif kind < 0.2:
        text = ""
    elif kind < 0.95:
        text = [r.choice([r.randint(0x20, 0x7E), r.randint(1, 0xFFFF), r.randint(0x20, 0x7E)]) for _ in range(r.randint(1, 60))]
    else:
        text = [r.randint(1, 0xFFFF) for _ in range(r.randint(0x3F00, 0x4100))]
    span = r.choice([50, 200, 2000])
    if r.random() < 0.15:
        rect = [r.choice([-0x80000000, 0x7FFFFFFF, 0, -1, r.getrandbits(32) - 0x80000000]) for _ in range(4)]
    else:
        rect = [r.randint(-span, span) for _ in range(4)]
    fonts = [r.choice([0, 0, r.randint(1, 52)]) for _ in range(52)] if r.random() < 0.3 else [i + 1 for i in range(52)]
    return measure_case(text=text, rect=rect, font=r.choice([r.randint(0, 12), r.randint(0, 12), r.randint(-128, 127)]),
                        horizontal=r.choice([0, 1, 2, r.randint(-128, 127)]), vertical=r.choice([0, 1, 2, r.randint(-128, 127)]),
                        wrap=r.choice([0, 1, 0, r.randint(-5, 5)]), extent=r.random() < 0.7, hebrew=r.randint(0, 1),
                        measure=[r.randint(-1000, 1000) for _ in range(4)], w=world(group=r.randint(0, 3), fonts=fonts))


# ---------------------------------------------------------------- UI_CreateVersionNumber

def version_case(show=0, screen=(2048, 1536), text="2.04b3", hebrew=0, measure=(0, 0, 300, 40), create=1, w=None):
    return {"show": show, "screen": list(screen), "text": text, "hebrew": hebrew, "measure": list(measure), "create": create,
            "world": w or world()}


def version_stubs(e, c, text_ptr):
    st = CL.Stubs()
    st.real = {MEASURE, VERSION}
    measure_stubs(st)

    def uversion(em):
        L, this = em.cl, em.r(3)
        L.rec("UString", [L.newlab(this + 0xC)])
        L.rec("UString", [L.newlab(this + 0x18)])
        L.rec("UVersion", [L.newlab(this), em.r(4) & 0xFFFF, em.r(5) & 0xFF, em.r(6) & 0xFF, em.r(7) & 0xFFFF])

    def load(em):
        L, this = em.cl, em.r(3)
        L.rec("LoadFromResource", [L.lab(this), int(em.r(4) == em.data_import(DEFAULT_RESOURCE)), em.r(5) & 0xFF])
        em.write(this + 0xC + 4, struct.pack(">II", len(c["text"].encode("latin-1")), text_ptr))

    def assign(em):
        L, this, other = em.cl, em.r(3), em.r(4)
        L.rec("UString=", [L.lab(this), L.lab(other)])
        em.write(this + 4, em.read(other + 4, 8))
        em.set_r(3, this)

    def swprintf(em):
        L, buffer, size, fmt, arg = em.cl, em.r(3), em.r(4), em.r(5), em.r(6)
        format_chars = wide_chars(em, fmt)
        text = []
        i = 0
        while i < len(format_chars):
            if format_chars[i] == ord("%") and i + 1 < len(format_chars) and format_chars[i + 1] == ord("s"):
                text += list(em.cstr(arg, limit=1000).encode("latin-1"))
                i += 2
            else:
                text.append(format_chars[i])
                i += 1
        shown_format = CL.narrow("".join(chr(ch) for ch in format_chars))
        shown_text = CL.narrow("".join(chr(ch) for ch in text))
        L.rec("swprintf", [L.newlab(buffer), size], [shown_format, shown_text])
        if size == 0:
            em.set_r(3, MASK)
            return
        written = min(len(text), size - 1)
        em.write(buffer, b"".join(struct.pack(">H", ch) for ch in text[:written]) + b"\0\0")
        em.set_r(3, len(text) if len(text) < size else MASK)

    def create(em):
        L = em.cl
        L.rec("CreateObject", [L.lab(em.r(3)), s32(em.r(4)), s32(em.r(5)), s32(em.r(6)), T.s16(em.r(7)), T.s16(em.r(8)), T.s16(em.r(9)),
                               T.s16(em.r(10))])
        em.set_r(3, CREATED_WIN if L.ret("CreateObject") else 0)

    st.imp(IMP_UVERSION, uversion)
    st.imp(IMP_LOAD, load)
    st.imp(IMP_USTRING, lambda em: em.cl.rec("UString", [em.cl.newlab(em.r(3))]))
    st.imp(IMP_USTRING_DTOR, CL.recorder("~UString", "D"))
    st.imp(IMP_USTRING_ASSIGN, assign)
    st.imp("swprintf", swprintf)
    st.internal(CREATE_OBJECT, create)
    st.internal(TEXT_SPRITE_CTOR, CL.recorder("InterfaceTextObjectSprite", "NL"))
    st.internal(SET_CLIENT_DRAW_OBJECT, CL.recorder("SetClientDrawObject", "LL"))
    st.internal(WINDOW_SET_FONT, CL.recorder("Window.SetFont", "LL"))
    st.internal(SET_CLIENT_DRAW_TEXT, CL.recorder("SetClientDrawText", "LS"))
    return st


def ppc_version(e, c):
    w = c["world"]
    e.alloc_log.clear()
    put_world(e, w)
    lobby = e.alloc(0x40, b"\xa5" * 0x40)
    e.write(lobby + 4, struct.pack(">I", LOBBY_WIN))
    e.write(G_LOBBY, struct.pack(">I", lobby))
    e.write(G_ROOT, struct.pack(">I", ROOT_WIN))
    e.write(G_SCREEN_W, struct.pack(">h", c["screen"][0]))
    e.write(G_SCREEN_H, struct.pack(">h", c["screen"][1]))
    data = c["text"].encode("latin-1") + b"\0"
    text_ptr = e.alloc(len(data), data)
    fixed = fixed_labels(w, {ROOT_WIN: 30, LOBBY_WIN: 31, CREATED_WIN: 32, lobby: 33})
    e.cl = CL.PpcLog(fixed, queues={"IsHebrew": [c["hebrew"]], "Measure": c["measure"], "CreateObject": [c["create"]]}, first_new=200)
    st = version_stubs(e, c, text_ptr)
    old = st.activate(e)
    try:
        T.call_ppc(e, VERSION, [c["show"]])
    finally:
        CL.Stubs.deactivate(e, old)
    return {"log": e.cl.log, "mb": e.read(MB_BUFFER, MBSHOWN), "heap": heap_ppc(e)}


def host_version(lib, c):
    w = c["world"]
    lib.tpw_ll_version_world.argtypes = [C.c_int32, C.c_int32, C.c_char_p]
    CL.host_begin(lib, {}, queues={"IsHebrew": [c["hebrew"]], "Measure": c["measure"], "CreateObject": [c["create"]]}, first_new=200)
    heap_clear_host(lib)
    host_world(lib, w)
    lib.tpw_ll_version_world(c["screen"][0], c["screen"][1], c["text"].encode("latin-1"))
    lib.tpw_ll_version(c["show"])
    log = CL.host_end(lib)
    return {"log": log, "mb": host_mb(lib), "heap": heap_host(lib)}


def version_edges():
    extents = [(0, 0, 300, 40), (0, 0, 0, 0), (10, 20, 10, 20), (5, 7, 130, 61), (-30, -40, 30, 40), (0, 0, 1, 1),
               (0, 0, 0x100000, 0x100000), (0, 0, 0x7FFFFFFF, 0x7FFFFFFF), (-0x80000000, -0x80000000, 0x7FFFFFFF, 0x7FFFFFFF),
               (0, 0, 33, 17), (3, 3, 2, 2)]
    screens = [(2048, 1536), (640, 480), (800, 600), (1024, 768), (1280, 960), (1600, 1200), (3, 5), (1, 1), (32767, 32767), (-2, -640),
               (-640, 480), (-32768, 32767), (2048, 480)]
    # both parents, the Hebrew swap, every extent on a usual screen
    for show in (0, 1):
        for hebrew in (0, 1):
            for extent in extents:
                yield version_case(show=show, hebrew=hebrew, measure=extent)
    # every screen size
    for screen in screens:
        for extent in extents[:5]:
            yield version_case(show=1, screen=screen, measure=extent)
    # the version text: empty, short, up to and past the label's 63 characters
    for length in (0, 1, 6, 30, 60, 61, 62, 63, 64, 100, 200, 390):
        yield version_case(text=("2.04b3" * 70)[:length])
        yield version_case(text=("v" * 400)[:length], show=1, hebrew=1)
    yield version_case(text="\xe9\xff\x80z")
    # CreateObject finds no window
    for show in (0, 1):
        yield version_case(show=show, create=0)
    # the resolution group (the font the window is given) and a text object's fonts
    for group in range(4):
        yield version_case(w=world(group=group))
    yield version_case(w=world(fonts=[0] * 52))


def version_rnd(r):
    screen = r.choice([(2048, 1536), (640, 480), (800, 600), (1024, 768), (1600, 1200), (r.choice([1, 2, 7, 100, 4000, -3, -500]),
                                                                                              r.choice([1, 2, 7, 100, 3000, -3, -500]))])
    measure = r.choice([[r.randint(-100, 100), r.randint(-100, 100), r.randint(-100, 600), r.randint(-100, 300)],
                        [r.randint(-0x80000000, 0x7FFFFFFF) for _ in range(4)], [0, 0, r.randint(0, 5000), r.randint(0, 5000)]])
    length = r.choice([0, 6, 6, 6, r.randint(0, 120)])
    text = "".join(chr(r.choice([r.randint(0x20, 0x7E), r.randint(1, 255)])) for _ in range(length)) if length != 6 else "2.04b3"
    text = text.replace("\0", "x")
    return version_case(show=r.randint(0, 1), screen=screen, text=text, hebrew=r.randint(0, 1), measure=measure, create=r.choice([1, 1, 1, 0]),
                        w=world(group=r.randint(0, 3)))


# ---------------------------------------------------------------- LB_Lobby_MailIconMessagesFn

def mail_case(message=0x10006, a=0, b=0, online=0, logged_in=0, string="You have no mail", present=1, result=77):
    return {"message": message, "a": a, "b": b, "online": online, "logged_in": logged_in, "string": string, "present": present,
            "result": result}


def ppc_mail(e, c):
    e.alloc_log.clear()
    online = e.alloc(0x410, b"\xa5" * 0x410)
    e.write(G_ONLINE, struct.pack(">I", online if c["online"] else 0))
    data = [ord(ch) for ch in c["string"]]
    string_ptr = new_wide_codes(e, data) if c["present"] else 0
    fixed = {online: 40, MAIL_WIN: 41}
    e.cl = CL.PpcLog(fixed, queues={"AreWeLoggedIn": [c["logged_in"]], "CallDefaultWindowHandler": [c["result"]]}, first_new=200)
    st = CL.Stubs()
    st.real = {MAIL}

    def get_string(em):
        em.cl.rec("UIMain_GetString", [s32(em.r(3))])
        em.set_r(3, string_ptr)

    def dialog(em):
        L, params = em.cl, em.r(3)
        text = struct.unpack(">I", em.read(params, 4))[0]
        red, green, blue = em.read(params + 4, 3)
        v8, vc = struct.unpack(">ii", em.read(params + 8, 8))
        L.rec("UI_DialogBox", [red, green, blue, v8, vc, s32(em.r(4)), s32(em.r(6)), s32(em.r(8)), int(em.r(5) != 0), int(em.r(7) != 0),
                               int(em.r(9) != 0), int(em.r(10) & 0xFF != 0)], [CL.wide_text(em, text) if text else "(null)"])

    st.internal(ONLINE_CTOR, CL.recorder("CFSOnlineSystem", "N"))
    st.internal(ARE_WE_LOGGED_IN, CL.recorder("AreWeLoggedIn", "L", ret="i"))
    st.internal(OUTBOX, CL.recorder("CreateOnlineOutboxPanel", ""))
    st.internal(GET_STRING, get_string)
    st.internal(DIALOG, dialog)
    st.internal(DEFAULT_HANDLER, CL.recorder("CallDefaultWindowHandler", "Liii", ret="i"))
    old = st.activate(e)
    try:
        answer = T.call_ppc(e, MAIL, [MAIL_WIN, c["message"] & MASK, c["a"] & MASK, c["b"] & MASK])[0]
    finally:
        CL.Stubs.deactivate(e, old)
    return {"result": s32(answer), "online": e.cl.lab(e.u32(G_ONLINE)), "log": e.cl.log, "heap": heap_ppc(e)}


def host_mail(lib, c):
    lib.tpw_ll_mail_world.argtypes = [C.c_int32, C.c_int32, C.c_void_p]
    lib.tpw_ll_mail.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.POINTER(C.c_int32)]
    lib.tpw_ll_mail.restype = C.c_int32
    CL.host_begin(lib, {}, queues={"AreWeLoggedIn": [c["logged_in"]], "CallDefaultWindowHandler": [c["result"]]}, first_new=200)
    heap_clear_host(lib)
    lib.tpw_ll_mail_world(c["online"], c["present"], wide_buffer([ord(ch) for ch in c["string"]]))
    label = C.c_int32(-5)
    answer = lib.tpw_ll_mail(s32(c["message"]), s32(c["a"]), s32(c["b"]), C.byref(label))
    log = CL.host_end(lib)
    return {"result": answer, "online": label.value, "log": log, "heap": heap_host(lib)}


def mail_edges():
    for message in (0x10006, 0, 1, 6, 0x10005, 0x10007, 0x10006 + 0x100000, 0x6, -1, 0x7FFFFFFF, -0x80000000, 0x20006, 0x10, 0x11, 0x22):
        for online in (0, 1):
            for logged_in in (0, 1, -1, 0x60, 0x100):
                yield mail_case(message=message, online=online, logged_in=logged_in)
    for present in (0, 1):
        for string in ("", "x", "No mail", "A" * 150, "\xe9€Āz"):
            yield mail_case(string=string, present=present)
            yield mail_case(string=string, present=present, online=1)
    for result in (0, 1, -1, 0x7FFFFFFF, -0x80000000):
        yield mail_case(result=result)
        yield mail_case(message=3, result=result)
    for a, b in ((0, 0), (1, -1), (0x7FFFFFFF, -0x80000000), (-5, 123456)):
        yield mail_case(a=a, b=b)
        yield mail_case(message=2, a=a, b=b)


def mail_rnd(r):
    message = r.choice([0x10006, 0x10006, r.getrandbits(32), r.randint(0, 40), 0x10000 + r.randint(0, 20)])
    return mail_case(message=message, a=r.randint(-0x80000000, 0x7FFFFFFF), b=r.randint(-0x80000000, 0x7FFFFFFF), online=r.randint(0, 1),
                     logged_in=r.choice([0, 1, r.randint(-5, 5)]), present=r.choice([1, 1, 0]),
                     string="".join(chr(r.randint(1, 0x2FF)) for _ in range(r.randint(0, 40))), result=r.randint(-0x80000000, 0x7FFFFFFF))


# ---------------------------------------------------------------- CGameType

def type_case(op, type_=0, options=0):
    return {"op": op, "type": type_, "options": options}


def ppc_type(e, c):
    block = e.alloc(4, b"\xa5\xa5\xa5\xa5" if c["op"] == 0 else struct.pack(">I", 0x12345678))
    e.write(G_OPTIONS, struct.pack(">I", c["options"] & MASK))
    e.cl = CL.PpcLog({block: 1})
    st = CL.Stubs()
    st.real = {GT_CTOR, GT_SET}
    st.internal(ASSERT, lambda em: em.cl.rec("GAME_ASSERTMSG", (s32(em.r(3)),), (em.cstr(em.r(4))[:159],)))
    old = st.activate(e)
    try:
        if c["op"] == 0:
            returned = T.call_ppc(e, GT_CTOR, [block])[0]
        else:
            T.call_ppc(e, GT_SET, [block, c["type"] & MASK])
            returned = block
    finally:
        CL.Stubs.deactivate(e, old)
    return {"type": s32(e.u32(block)), "options": e.u32(G_OPTIONS), "this": int(returned == block), "log": e.cl.log}


def host_type(lib, c):
    lib.tpw_ll_gametype.argtypes = [C.c_int32, C.c_int32, C.c_uint32, C.POINTER(C.c_int32), C.POINTER(C.c_uint32)]
    CL.host_begin(lib, {})
    type_out, options_out = C.c_int32(0), C.c_uint32(0)
    lib.tpw_ll_gametype(c["op"], c["type"], c["options"] & MASK, C.byref(type_out), C.byref(options_out))
    return {"type": type_out.value, "options": options_out.value, "this": 1, "log": CL.host_end(lib)}


def type_edges():
    # the constructor from every combination of the three option bits, with the other bits clear and set
    for bits in range(8):
        base = (0x800000 if bits & 1 else 0) | (0x1000000 if bits & 2 else 0) | (0x2000000 if bits & 4 else 0)
        for other in (0, 0xFC7FFFFF, 0x12345678 & ~0x3800000, 1, 0x80000000):
            yield type_case(0, options=base | other)
    # SetGameType for -1, 0, 1, 2, 3 and further, from several option words
    options_words = [0, 0x800000, 0x1000000, 0x2000000, 0x3800000, 0xFFFFFFFF, 0xC7FFFFFF, 0x12345678]
    for type_ in (-1, 0, 1, 2, 3, 4, 7, 100, -100, 0x7FFFFFFF, -0x80000000, 0x10000, 0x12345678):
        for options in options_words:
            yield type_case(1, type_, options)


def type_rnd(r):
    return type_case(r.randint(0, 1), r.choice([-1, 0, 1, 2, 3, r.randint(-5, 6), r.getrandbits(32) - 0x80000000]), r.getrandbits(32))


TESTS = [
    FnTest("GetFontTableEntry", ppc_font, host_font, edge=font_edges, rnd=font_rnd),
    FnTest("UI_MeasureText", ppc_measure, host_measure, edge=measure_edges, rnd=measure_rnd),
    FnTest("UI_CreateVersionNumber", ppc_version, host_version, edge=version_edges, rnd=version_rnd),
    FnTest("LB_Lobby_MailIconMessagesFn", ppc_mail, host_mail, edge=mail_edges, rnd=mail_rnd),
    FnTest("CGameType", ppc_type, host_type, edge=type_edges, rnd=type_rnd),
]
