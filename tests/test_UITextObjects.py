"""Differential tests for the text draw objects of the interface, against libtpw_unit_UITextObjects.dylib
(tests/unit_UITextObjects.cpp):

  src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_textmanager.cpp   InterfaceTextObjectSprite: constructor, destructor, UpdateTextures, Change
                              InterfaceTitleTextObject: constructor, destructor, UpdateTextures
                              InterfaceCursorPosTextObjectSprite: constructor, destructor, Query

Real on both sides: those ten, the sprite draw object under them (src/game/ui/InterfaceDrawObjectSprite.cpp: constructor, destructor, Change,
Show, CreateTextures, ...) and the rectangles (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp: CoolRect::Set). Fakes that record the same event on both sides: the draw
object base class, the engine's sprite surfaces (SSpriteGameData, SSpriteEngineData, CSpriteList) and CMapWho, the text formatter
(TbTextFormatter: constructor, destructor, the two justifications, the character positions, Draw and Measure), the font parameters
(SetFont, UseReColour, UsePreColoured, UseAlphaWriteMode), the render area (constructor, MakeRenderArea, LbDraw_SetCurrent), the
palette search, the Hebrew swap, the string converter and the two callbacks a text object calls around its drawing.

A case is a script of operations on a world of six text objects (of the three kinds), a pixel buffer, a font table, six texts, a table
of character positions and a staged style and change record. After each operation the answer, every object, the globals, the pixels,
the heap traffic and the length of the call log are compared, and the whole call log at the end. The formatter's Draw marks the surface
it draws into (and the callbacks mark it too), so the order of the clearing, the callbacks and the drawing shows in the pixels.

Operations the original cannot be asked: a division by a screen size of zero or less, a text address that is not a text, a rectangle
that the sprite's Change would read through NULL. They are skipped on both sides. Two results the original leaves to the registers its
caller had are tested with those registers cleared: a justification code outside 0 to 2 (the text box is then four zeros) and a font
id outside 0 to 12 (the font word is read from address 0x14, which the test maps as a page of zeros).

Unit v5L49: textobj_callee::CursorDtor (the password object's call of 0x1018f2b8, src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_textmanager.cpp) is the cursor object's
destructor test again with the host going through that glue (operation 24) instead of the language's delete.

Quirks the tests pin: the text objects do not call UI_DrawText but do the same job inline; the Title draws the text twice (grey, two
pixels left and down, then white) and passes the second draw what the first reported; Change ignores the style unless one of the
flags 0x100 or 0x400..0x40000 is given; flag 0x40000 also writes the sprite's colour word; Query(1) answers the pen position for an
index that is negative, past the positions or when there are none; Query(2) cuts the bottom of the rectangle to the object's height.
"""
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UITextObjects"

MINUS = 0xFFFFFFFF

# ---- the unit's functions (all run as the original on the PowerPC side)
FN = dict(
    TextCtor=0x1018D11C, TextDtor=0x1018D1DC, TextUpdate=0x1018D2C4, TextChange=0x1018D798,
    TitleCtor=0x1018FBE8, TitleDtor=0x1018FCC4, TitleUpdate=0x1018FDB8,
    CursorCtor=0x1018F1D0, CursorDtor=0x1018F2B8, CursorQuery=0x1018F3B4)
# what they call that is real too: the sprite draw object (all its members), the base class's Query and CoolRect::Set, __ptr_glue
SPRITE_FUNCTIONS = [0x1018C978, 0x1018CA88, 0x1018CB48, 0x1018CBB4, 0x1018CC5C, 0x1018CCC0, 0x1018CF10, 0x1018CFF0, 0x1018D038, 0x1018D0BC]
BASE_QUERY, COOLRECT_SET, PTR_GLUE = 0x101CCFAC, 0x101C756C, 0x1023BAA8
REAL_HELPERS = [BASE_QUERY, COOLRECT_SET, PTR_GLUE, 0x101C7448, 0x101C7494, 0x101C74E0, 0x101C75A8, 0x101C7618, 0x101C7678, 0x101C7710,
                0x101C7788, 0x101C7A1C, 0x101C7A78]
# the fakes (in-binary functions)
DRAW_CTOR, DRAW_DTOR = 0x101CCCC0, 0x101CCD9C
ADJUST_HEBREW, MAKE_RENDER_AREA, CONVERT_STRING = 0x1018B2C8, 0x1018B360, 0x10181A7C

# ---- the original's data
GTEXT = 0x1037FF18
G = dict(width=0x10395BBC, height=0x10395BBA, group=GTEXT + 4, fonts=GTEXT + 0x1C, params=GTEXT + 0x4FC, query_rect=0x1038CD5C)
FONT_TABLE_BYTES = 52 * 0x18
CORE_VTABLE = 0x102AF8FC   # CoolRect's vtable

# ---- the world
SLOTS, FONTS, TEXTS, TOKENS = 6, 6, 6, 3
POSITIONS = 8
OBJ_SIZE = 0x19C
PIXEL_BYTES = 20000
CONVERTED_BYTES = 200
MAX_QUEUE = 64
KIND_NONE, KIND_TEXT, KIND_TITLE, KIND_CURSOR = 0, 1, 2, 3
CTOR = {KIND_TEXT: FN["TextCtor"], KIND_TITLE: FN["TitleCtor"], KIND_CURSOR: FN["CursorCtor"]}
DTOR = {KIND_TEXT: FN["TextDtor"], KIND_TITLE: FN["TitleDtor"], KIND_CURSOR: FN["CursorDtor"]}
UPDATE = {KIND_TEXT: FN["TextUpdate"], KIND_TITLE: FN["TitleUpdate"], KIND_CURSOR: FN["TextUpdate"]}   # (the cursor object's is the text object's)
QUERY = {KIND_TEXT: BASE_QUERY, KIND_TITLE: BASE_QUERY, KIND_CURSOR: FN["CursorQuery"]}

# ---- labels (the host's tpw_uitx_labels)
LAB_OBJECT, LAB_GAME, LAB_SURFACE, LAB_LIST, LAB_FORMATTER, LAB_INSTANCES = 30, 40, 50, 60, 70, 80
LAB_PIXELS, LAB_MAP, LAB_PARAMS, LAB_PALETTE, LAB_CONVERTED, LAB_QUERY = 90, 91, 92, 93, 94, 95
LAB_FONT, LAB_TEXT, LAB_TOKEN = 100, 110, 120
FIRST_NEW = 300

OBJECT_FIELDS, GLOBAL_FIELDS = 60, 9


def s16(v):
    return T.s16(v & 0xFFFF)


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MINUS))


def w16(e, address, value):
    e.write(address, struct.pack(">h", s16(value)))


# ======================================================================= the PowerPC side


class Ppc:
    """One case on the PowerPC side: the fixtures and the operations."""

    def __init__(self, e, case):
        self.e = e
        e.us = self
        self.case = case
        self.heap = [0, 0, 0, 0]
        # the globals the case changes: saved here, put back by restore()
        self.saved = {}
        for address, size in ((G["width"], 2), (G["height"], 2), (G["group"], 4), (G["fonts"], FONT_TABLE_BYTES), (G["query_rect"] + 4, 8)):
            self.saved[(address, size)] = bytes(e.read(address, size))
        self.map_slot = e.data_import("gei_MapWho")
        self.saved[(self.map_slot, 4)] = bytes(e.read(self.map_slot, 4))
        self.palette = e.data_import("_lbGlobalPalette")
        self.objects = [0] * SLOTS
        self.kinds = [KIND_NONE] * SLOTS
        self.pixels = e.alloc(PIXEL_BYTES, b"\xa5" * PIXEL_BYTES)
        self.map = e.alloc(16, bytes(16))
        self.converted = e.alloc(CONVERTED_BYTES, bytes(CONVERTED_BYTES))
        self.instances = [e.alloc(16, bytes(16)) for _ in range(SLOTS)]
        self.tokens = [e.alloc(4, bytes(4)) for _ in range(TOKENS)]
        self.fonts = [e.alloc(4, bytes(4)) for _ in range(FONTS)]
        self.texts = [CL.new_wide(e, t) for t in case.get("texts", [])[:TEXTS]]
        self.positions = e.alloc(8 * POSITIONS, bytes(8 * POSITIONS))
        self.style = e.alloc(0x14, b"\xa5" * 0x14)
        w32(e, self.style, 0)
        self.change = e.alloc(0x28, b"\xa5" * 0x28)
        self.staged_rect = e.alloc(16, struct.pack(">I4h", CORE_VTABLE, 0, 0, 0, 0))
        self.style_null = False
        self.rect_null = False
        self.current = 0       # the render area LbDraw_SetCurrent was last given
        self.draw_count = 0    # how many times the formatter was told to draw
        fixed = {}
        fixed.update({a: LAB_INSTANCES + i for i, a in enumerate(self.instances)})
        fixed.update({a: LAB_TOKEN + i for i, a in enumerate(self.tokens)})
        fixed.update({a: LAB_FONT + i for i, a in enumerate(self.fonts)})
        fixed.update({a: LAB_TEXT + i for i, a in enumerate(self.texts)})
        fixed[self.pixels] = LAB_PIXELS
        fixed[self.map] = LAB_MAP
        fixed[G["params"]] = LAB_PARAMS
        fixed[self.palette] = LAB_PALETTE
        fixed[self.converted] = LAB_CONVERTED
        fixed[G["query_rect"]] = LAB_QUERY
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        self.L = e.cl = CL.PpcLog(fixed, queues, first_new=FIRST_NEW)
        # the globals as the Mac's data has them (zero), the map the fakes
        w16(e, G["width"], 0)
        w16(e, G["height"], 0)
        w32(e, G["group"], 0)
        for i in range(52):
            w32(e, G["fonts"] + 0x18 * i + 0x14, 0)
        e.write(G["query_rect"] + 4, bytes(8))
        w32(e, self.map_slot, self.map)

    def restore(self):
        for (address, size), data in self.saved.items():
            self.e.write(address, data)

    # ---- helpers
    def obj(self, slot):
        return self.objects[slot] if 0 <= slot < SLOTS and self.kinds[slot] != KIND_NONE else 0

    def call(self, address, *args):
        # (the registers a justification outside 0 to 2 leaves the text box as are cleared, so that the box is four zeros)
        for register in range(22, 28):
            self.e.set_r(register, 0)
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def screen(self):
        e = self.e
        return T.s16(struct.unpack(">H", e.read(G["width"], 2))[0]), T.s16(struct.unpack(">H", e.read(G["height"], 2))[0])

    def screen_ok(self):
        width, height = self.screen()
        return width > 0 and height > 0

    def text_address(self, token):
        return 0 if token < 0 else self.texts[token]

    # ---- the dumps (the order is tpw_uitx_dump_object in the library)
    def dump_object(self, slot):
        e, L = self.e, self.L
        address = self.objects[slot]
        d = bytes(e.read(address, OBJ_SIZE))
        s16_ = lambda o: struct.unpack(">h", d[o:o + 2])[0]   # noqa: E731
        u16_ = lambda o: struct.unpack(">H", d[o:o + 2])[0]   # noqa: E731
        s32_ = lambda o: struct.unpack(">i", d[o:o + 4])[0]   # noqa: E731
        u32_ = lambda o: struct.unpack(">I", d[o:o + 4])[0]   # noqa: E731
        out = [self.kinds[slot], L.lab(u32_(0))]
        out += [s16_(4), s16_(6), s16_(8), s16_(0xA), s16_(0xC), s16_(0xE)]
        out += [s16_(0x14), s16_(0x16), s16_(0x18), s16_(0x1A), s16_(0x20), s16_(0x22), s16_(0x24), s16_(0x26)]
        out += [s16_(0x28), s16_(0x2A), s16_(0x2C), s16_(0x2E), s16_(0x30), s16_(0x32), s16_(0x34)]
        out += [s16_(0x3C), s16_(0x3E), int(d[0x40:0x44] == b"\xa5" * 4), s16_(0x44)]
        out += [d[0x46 + i] for i in range(6)]
        out += [int(u32_(0x4C) != 0), int(u32_(0x50) != 0)]
        out += [s32_(0x64), u32_(0x68), u32_(0x6C), u32_(0x70), L.lab(u32_(0x7C)), u32_(0x80), s32_(0x78)]
        array = u32_(0x98)
        out += [L.lab(array), L.lab(e.u32(array)) if array else 0, s32_(0xB8)]
        out += [L.lab(u32_(0x9C)), L.lab(u32_(0xA0)), u16_(0xA8), u16_(0xAA)]
        out += [T.s8(d[0xDC]), T.s8(d[0xDD]), d[0xDE], d[0xDF], T.s8(d[0xE0]), d[0xE1], s16_(0xE2), s16_(0xE4)]
        out += [d[0x170], s32_(0x174), s32_(0x178)]
        assert len(out) == OBJECT_FIELDS
        return out

    def dump_globals(self):
        e, L = self.e, self.L
        width, height = self.screen()
        rect = struct.unpack(">4h", e.read(G["query_rect"] + 4, 8))
        return [width, height, T.s32(e.u32(G["group"])), L.lab(e.u32(self.map_slot))] + list(rect) + [self.draw_count]

    def dump_state(self):
        return dict(objects=[self.dump_object(s) if self.kinds[s] != KIND_NONE else [0] for s in range(SLOTS)], globals=self.dump_globals(),
                    pixels=bytes(self.e.read(self.pixels, PIXEL_BYTES)), heap=list(self.heap), log=len(self.L.log))

    # ---- the operations
    def op(self, code, a):
        """Runs one operation. Returns the answer (five words) or None when the state does not allow it."""
        e, L = self.e, self.L
        if code == 1:   # an object: slot, kind, data token (-1 NULL)
            slot, kind, token = a[0], a[1], a[2]
            if not 0 <= slot < SLOTS or self.kinds[slot] != KIND_NONE or kind not in CTOR or not -1 <= token < TOKENS:
                return None
            block = e.alloc(OBJ_SIZE, b"\xa5" * OBJ_SIZE)
            L.labels[block] = LAB_OBJECT + slot
            L.labels[block + 0x54] = LAB_GAME + slot
            L.labels[block + 0x9C] = LAB_SURFACE + slot
            L.labels[block + 0xB4] = LAB_LIST + slot
            L.labels[block + 0xB4 + 0x20] = LAB_LIST + slot   # (the list's link base, 0x20 into it, is the list for the log)
            L.labels[block + 0xE8] = LAB_FORMATTER + slot
            self.objects[slot] = block
            self.kinds[slot] = kind
            self.call(CTOR[kind], block, self.tokens[token] if token >= 0 else 0)
            return [L.lab(block), 0, 0, 0, 0]
        if code in (2, 3, 4, 7, 8) or 20 <= code <= 24:
            if not self.obj(a[0]):
                return None
        if code in (2, 3, 4, 7, 8):
            return self.setup_object(code, a)
        if code in (5, 6, 9, 10, 11, 12):
            return self.setup_world(code, a)
        return self.function(code, a)

    def setup_object(self, code, a):
        e = self.e
        s = self.objects[a[0]]
        if code == 2:   # the text style: slot, justify, vjustify, wrap, byte 0xdf, font, colour mode, offset x, y, four colour bytes
            e.write(s + 0xDC, bytes([a[1] & 0xFF, a[2] & 0xFF, a[3] & 0xFF, a[4] & 0xFF, a[5] & 0xFF, a[6] & 0xFF]))
            w16(e, s + 0xE2, a[7])
            w16(e, s + 0xE4, a[8])
            e.write(s + 0x48, bytes([v & 0xFF for v in a[9:13]]))
        elif code == 3:   # the sprite's words
            w16(e, s + 0x44, a[1])
            w16(e, s + 0x3C, a[2])
            w16(e, s + 0x3E, a[3])
            w16(e, s + 4, a[4])
            e.write(s + 0x14, struct.pack(">4h", *[s16(v) for v in a[5:9]]))
            w16(e, s + 0x34, a[9])
            w16(e, s + 0x2C, a[10])
            w16(e, s + 0xA8, a[11])
            w16(e, s + 0xAA, a[12])
            w32(e, s + 0xA0, 0 if a[13] == 0 else self.pixels)
        elif code == 4:   # the second rectangle
            e.write(s + 0x20, struct.pack(">4h", *[s16(v) for v in a[1:5]]))
        elif code == 7:   # the formatter's words
            e.write(s + 0x170, bytes([a[1] & 0xFF]))
            w32(e, s + 0x174, a[2])
            w32(e, s + 0x178, a[3])
        elif code == 8:   # the callbacks
            w32(e, s + 0x4C, e.uitx_pre_draw if a[1] else 0)
            w32(e, s + 0x50, e.uitx_post_draw if a[2] else 0)
        return [0, 0, 0, 0, 0]

    def setup_world(self, code, a):
        e = self.e
        if code == 5:   # a global: which (1 screen width, 2 screen height, 3 resolution group), value
            if a[0] == 1:
                w16(e, G["width"], a[1])
            elif a[0] == 2:
                w16(e, G["height"], a[1])
            elif a[0] == 3:
                if not 0 <= a[1] < 4:
                    return None
                w32(e, G["group"], a[1])
        elif code == 6:   # a group of the font table: group, then the font token (-1 none) of each of the 13 fonts
            if not 0 <= a[0] < 4 or any(not -1 <= t < FONTS for t in a[1:14]):
                return None
            for i in range(13):
                w32(e, G["fonts"] + 0x18 * (a[0] * 13 + i) + 0x14, self.fonts[a[1 + i]] if a[1 + i] >= 0 else 0)
        elif code == 9:   # the character positions: the count, then the pairs
            if not 0 <= a[0] <= POSITIONS:
                return None
            for i in range(POSITIONS):
                x, y = (a[1 + 2 * i], a[2 + 2 * i]) if i < a[0] else (0, 0)
                e.write(self.positions + 8 * i, struct.pack(">2i", T.s32(x & MINUS), T.s32(y & MINUS)))
        elif code == 10:   # the staged text style
            if a[12] >= len(self.texts):
                return None
            e.write(self.style, b"\xa5" * 0x14)
            w32(e, self.style, self.text_address(a[12]))
            e.write(self.style + 4, bytes([a[0] & 0xFF, a[1] & 0xFF, a[2] & 0xFF, a[3] & 0xFF, a[4] & 0xFF, a[5] & 0xFF]))
            w16(e, self.style + 0xA, a[6])
            w16(e, self.style + 0xC, a[7])
            e.write(self.style + 0x10, bytes([v & 0xFF for v in a[8:12]]))
            self.style_null = a[13] != 0
        elif code == 11:   # the staged change record
            e.write(self.change, b"\xa5" * 0x28)
            w16(e, self.change + 6, a[0])
            w16(e, self.change + 8, a[1])
            e.write(self.staged_rect + 4, struct.pack(">4h", *[s16(v) for v in a[2:6]]))
            self.rect_null = a[6] != 0
        elif code == 12:   # the query rectangle
            e.write(G["query_rect"] + 4, struct.pack(">4h", *[s16(v) for v in a[0:4]]))
        return [0, 0, 0, 0, 0]

    def change_reaches_update(self, a):
        """Whether Change(flags) will call UpdateTextures with a text."""
        flags = a[1] & MINUS
        if a[2] or flags == 0 or flags & 0x7FD00 == 0 or self.style_null or not flags & 0x100:
            return False
        return T.u32(self.e.u32(self.style)) != 0

    def allowed(self, code, a):
        """False for an operation the state does not allow (see the module comment)."""
        if code == 20:
            return a[1] < len(self.texts) and (a[1] < 0 or self.screen_ok())
        if code == 21:
            flags = a[1] & MINUS
            if not a[2] and flags & 0x10 and self.rect_null:
                return False
            return not self.change_reaches_update(a) or self.screen_ok()
        if code == 22:
            what, kind, value = a[1] & MINUS, a[2], a[3]
            if self.kinds[a[0]] != KIND_CURSOR:
                return True
            if what == 1:
                return self.screen_ok()
            if what == 2:
                if kind != 1:
                    return value == 0
                return value >= len(self.texts) or value < 0 or self.screen_ok()
            return True
        if code == 24:
            return self.kinds[a[0]] == KIND_CURSOR
        return True

    def function(self, code, a):
        if not self.allowed(code, a):
            return None
        e, L = self.e, self.L
        s = self.objects[a[0]]
        kind = self.kinds[a[0]]
        answer = [0, 0, 0, 0, 0]
        if code == 20:   # UpdateTextures: slot, text token (-1 NULL)
            self.call(UPDATE[kind], s, self.text_address(a[1]))
        elif code == 21:   # Change: slot, flags, 1 for no record
            w32(e, self.change + 0x10, 0 if self.style_null else self.style)
            w32(e, self.change + 0x14, 0 if self.rect_null else self.staged_rect)
            self.call(FN["TextChange"], s, a[1], 0 if a[2] else self.change)
        elif code == 22:   # Query: slot, what, value kind (0 a number, 1 a text token), value
            value = self.text_address(a[3]) if a[2] == 1 else a[3] & MINUS
            result = self.call(QUERY[kind], s, a[1], value)
            answer[0] = LAB_QUERY if result == G["query_rect"] else T.s32(result)
        elif code in (23, 24):   # the destructor: slot, flag (24: the host goes through textobj_callee::CursorDtor)
            self.call(DTOR[kind], s, a[1])
            if s16(a[1]) > 0:
                for offset in (0x54, 0x9C, 0xB4, 0xB4 + 0x20, 0xE8):
                    L.labels.pop(s + offset, None)   # (the parts' labels go with a deleted object)
            self.objects[a[0]] = 0   # (gone, deleted or not)
            self.kinds[a[0]] = KIND_NONE
        else:
            raise ValueError(code)
        return answer


# ======================================================================= the fakes (PowerPC side)


def make_fakes(e):
    """The two callbacks a text object calls around its drawing, as PowerPC-callable functions in persistent memory."""
    if hasattr(e, "uitx_pre_draw"):
        return

    def read_area(emu):
        depth, _, data, width, height, stride = struct.unpack(">iiIiii", emu.read(emu.r(3) + 0x10, 24))
        return depth, data, width, height, stride

    def pre_draw(emu):
        L = emu.cl
        depth, data, width, height, stride = read_area(emu)
        L.rec("PreDraw", (depth, L.lab(data), width, height, stride))
        if data:
            emu.write(data + 200, bytes([0xC3]))
        emu.set_r(3, T.u32(L.ret("PreDraw", 0)))

    def post_draw(emu):
        L = emu.cl
        depth, data, width, height, stride = read_area(emu)
        L.rec("PostDraw", (depth, L.lab(data), width, height, stride))
        if data:
            emu.write(data + 201, bytes([0x3C]))
        emu.set_r(3, 0)

    e.uitx_pre_draw = T.fake_function(e, pre_draw)
    e.uitx_post_draw = T.fake_function(e, post_draw)


def build_stubs(emu):
    S = CL.Stubs()
    S.real.update(FN.values())
    S.real.update(SPRITE_FUNCTIONS)
    S.real.update(REAL_HELPERS)
    R = CL.recorder

    # the draw object base class
    def draw_ctor(e):
        this, data = e.r(3), e.r(4)
        e.cl.rec("DrawObject.ctor", (e.cl.lab(this), e.cl.lab(data)))
        e.write(this + 4, bytes(0x34))
        w32(e, this, data)

    S.internal(DRAW_CTOR, draw_ctor)
    S.internal(DRAW_DTOR, R("DrawObject.dtor", "L"))

    # the main app's helpers
    def adjust_hebrew(e):
        address = e.r(3)
        value = T.s32(e.u32(address))
        e.cl.rec("AdjustJustification", (value,))
        if e.cl.ret("Hebrew", 0) != 0:
            if value == 0:
                w32(e, address, 2)
            elif value == 2:
                w32(e, address, 0)

    S.internal(ADJUST_HEBREW, adjust_hebrew)

    def make_render_area(e):
        pixels, area, width, height = e.r(3), e.r(4), T.s32(e.r(5)), T.s32(e.r(6))
        e.cl.rec("MakeRenderArea", (e.cl.lab(pixels), width, height))
        if area == 0:
            return
        depth = T.s32(e.cl.ret("Depth", 16) & MINUS)
        e.write(area, bytes(0x10))
        w32(e, area + 0x10, depth)
        w32(e, area + 0x14, 0)
        w32(e, area + 0x18, pixels)
        w32(e, area + 0x1C, width)
        w32(e, area + 0x20, height)
        w32(e, area + 0x24, T.s32(((depth & MINUS) >> 3) * width & MINUS))

    S.internal(MAKE_RENDER_AREA, make_render_area)

    def convert_string(e):
        text = e.r(3)
        e.cl.rec("ConvertString", (e.cl.lab(text),), (CL.wide_text(e, text) if text else "(null)",))
        chars = []
        while text and len(chars) + 1 < CONVERTED_BYTES:
            c = struct.unpack(">H", e.read(text + 2 * len(chars), 2))[0]
            if c == 0:
                break
            chars.append(c if c < 0x80 else ord("?"))
        e.write(e.us.converted, bytes(chars) + b"\0")
        e.set_r(3, e.us.converted)

    S.internal(CONVERT_STRING, convert_string)

    # the text formatter
    def formatter_ctor(e):
        this = e.r(3)
        e.cl.rec("Formatter.ctor", (e.cl.lab(this),))
        e.write(this + 0x88, bytes(1))
        w32(e, this + 0x8C, 0)
        w32(e, this + 0x90, 0)

    S.imp("__ct__15TbTextFormatterFv", formatter_ctor)
    S.imp("__dt__15TbTextFormatterFv", R("Formatter.dtor", "L"))
    S.imp("SetVerticalJustification__15TbTextFormatterFQ215TbTextFormatter14eJustification", R("Formatter.SetVertical", "xLi"))
    S.imp("SetHorizontalJustification__15TbTextFormatterFQ215TbTextFormatter14eJustification", R("Formatter.SetHorizontal", "xLi"))

    def get_positions(e):
        e.cl.rec("Formatter.GetCharPositionArray", (e.cl.lab(e.r(3)),))
        e.set_r(3, e.us.positions)

    S.imp("GetCharPositionArray__15TbTextFormatterCFv", get_positions)
    S.imp("GetCharPositionArraySize__15TbTextFormatterCFv",
          R("Formatter.GetCharPositionArraySize", "L", ret=lambda e, L: L.ret("PositionCount", 0)))

    def lay(name, draws):
        def stub(e):
            L = e.cl
            buffer, formatter, box, text, params, bounds = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7), e.r(8)
            left, top, right, bottom = struct.unpack(">4i", e.read(box, 16))
            incoming = struct.unpack(">4i", e.read(bounds, 16)) if bounds else (0, 0, 0, 0)
            L.rec(name, (L.lab(formatter), left, top, right, bottom, L.lab(text), L.lab(params), 1 if bounds else 0) + incoming,
                  ((e.cstr(text)[:159] if text else "(null)"),))
            if bounds:
                values = (L.ret("Bounds", T.s32(left + 3)), L.ret("Bounds", T.s32(top + 5)), L.ret("Bounds", T.s32(right - 7)),
                          L.ret("Bounds", T.s32(bottom - 11)))
                e.write(bounds, struct.pack(">4i", *[T.s32(v & MINUS) for v in values]))
            w32(e, buffer, L.ret("Lay", 0))
            if draws:
                # the mark: four bytes of the box, in the surface the last LbDraw_SetCurrent named, at a place that moves with each draw
                data = e.u32(e.us.current + 0x18) if e.us.current else 0
                if data:
                    e.write(data + 4 * (e.us.draw_count & 31) + 8, bytes([left & 0xFF, top & 0xFF, right & 0xFF, bottom & 0xFF]))
                e.us.draw_count += 1
        return stub

    S.imp("Draw__15TbTextFormatterFRC6TbRectPCUcR18TbFontRenderParamsP6TbRect", lay("Formatter.Draw", True))
    S.imp("Measure__15TbTextFormatterFRC6TbRectPCUcR18TbFontRenderParamsP6TbRect", lay("Formatter.Measure", False))

    # the font parameters
    S.imp("UseAlphaWriteMode__18TbFontRenderParamsFv", R("UseAlphaWriteMode", "xL"))
    S.imp("SetFont__18TbFontRenderParamsFP7TbIFont", R("SetFont", "xLL"))
    S.imp("UsePreColoured__18TbFontRenderParamsFv", R("UsePreColoured", "xL"))

    def use_recolour(e):
        colour = e.read(e.r(5), 5)
        e.cl.rec("UseReColour", (e.cl.lab(e.r(4)),) + tuple(colour))

    S.imp("UseReColour__18TbFontRenderParamsFRC8TbColour", use_recolour)

    # the render area, the drawing surface and the palette
    def area_ctor(e):
        e.cl.rec("RenderArea.ctor")
        e.write(e.r(3) + 0x10, bytes(0x18))

    S.imp("__ct__12TbRenderAreaFv", area_ctor)

    def set_current(e):
        area = e.r(3)
        depth, _, data, width, height, stride = struct.unpack(">iiIiii", e.read(area + 0x10, 24))
        e.cl.rec("SetCurrent", (depth, width, height, stride, e.cl.lab(data)))
        e.us.current = area

    S.imp("LbDraw_SetCurrent__FP12TbRenderArea", set_current)

    def find_colour(e):
        L = e.cl
        L.rec("FindColour", (L.lab(e.r(3)), e.r(4) & 0xFF, e.r(5) & 0xFF, e.r(6) & 0xFF))
        e.set_r(3, L.ret("FindColour", 0x5A) & 0xFF)

    S.imp("LbPalette_FindColour__FPC9TbPaletteUcUcUc", find_colour)

    # the engine's sprite surfaces and the map (imports)
    S.imp("__ct__15SSpriteGameDataFv", R("GameData.ctor", "L"))

    def surface_ctor(e):
        e.cl.rec("Surface.ctor", (e.cl.lab(e.r(3)),))
        e.write(e.r(3), bytes(0x18))

    S.imp("__ct__17SSpriteEngineDataFv", surface_ctor)
    S.imp("__dt__17SSpriteEngineDataFv", R("Surface.dtor", "L"))

    def list_ctor(e):
        this = e.r(3)
        e.cl.rec("List.ctor", (e.cl.lab(this),))
        w32(e, this, this + 0x20)   # the pointer to the link base the original stores at +0

    S.imp("__ct__11CSpriteListFv", list_ctor)
    S.imp("__dt__11CSpriteListFv", R("List.dtor", "L"))

    def list_init(e):
        this = e.r(3)
        e.cl.rec("List.Init", (e.cl.lab(this), T.s32(e.r(4))))
        slot = e.cl.lab(this) - LAB_LIST
        e.set_r(3, e.us.instances[slot] if 0 <= slot < SLOTS else 0)

    S.imp("Init__11CSpriteListFi", list_init)
    S.imp("SetUsedPosition__15SSpriteGameDataFii", R("SetUsedPosition", "Lii"))
    S.imp("SetUsedSize__15SSpriteGameDataFii", R("SetUsedSize", "Lii"))

    def surface_init(e):
        this, width, height = e.r(3), T.s32(e.r(4)), T.s32(e.r(5))
        e.cl.rec("Surface.Init", (e.cl.lab(this), width, height))
        made = e.cl.ret("Surface.Init", 1) != 0 and (width & 0xFFFF) * (height & 0xFFFF) * 4 <= PIXEL_BYTES
        w16(e, this + 0xC, width)
        w16(e, this + 0xE, height)
        w32(e, this + 4, e.us.pixels if made else 0)
        e.set_r(3, 1 if made else 0)

    S.imp("Init__17SSpriteEngineDataFii", surface_init)
    S.imp("RefreshPartialTexture__17SSpriteEngineDataFv", R("RefreshPartialTexture", "L"))
    S.imp("Link__7CMapWhoFP12CMapLinkBasei", R("MapWho.Link", "LLi", ret=lambda e, L: 1))
    S.imp("Unlink__7CMapWhoFP12CMapLinkBasei", R("MapWho.Unlink", "LLi", ret=lambda e, L: 1))

    def operator_delete(e):
        block = e.r(3)
        if block:
            e.us.heap[2] += 1
            label = e.cl.lab(block)
            if label > 0:
                e.cl.rec("delete", (label,))
                e.cl.forget(block)

    S.imp("__dl__FPv", operator_delete)
    return S


# ======================================================================= the host side


def open_host(lib):
    if getattr(lib, "_uitx_open", False):
        return
    import ctypes as C
    lib.tpw_uitx_op.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_uitx_dump_object.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_uitx_dump_globals.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uitx_heap.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uitx_text.argtypes = [C.c_int32, C.POINTER(C.c_uint16), C.c_int32]
    lib.tpw_uitx_pixels.restype = C.c_void_p
    lib._uitx_open = True


class Host:
    """One case on the host side: the same operations through the library."""

    def __init__(self, lib, case):
        import ctypes as C
        self.C = C
        self.lib = lib
        open_host(lib)
        lib.tpw_uitx_reset()
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        CL.host_begin(lib, {}, queues, first_new=FIRST_NEW)
        lib.tpw_uitx_labels()
        for index, text in enumerate(case.get("texts", [])[:TEXTS]):
            chars = (C.c_uint16 * (len(text) + 1))(*[ord(c) for c in text])
            lib.tpw_uitx_text(index, chars, len(text))

    def op(self, code, a):
        C = self.C
        args = (C.c_int64 * 24)(*a, *([0] * (24 - len(a))))
        out = (C.c_int64 * 5)()
        self.lib.tpw_uitx_op(code, args, out)
        return list(out)

    def dump_state(self, step):
        C, lib = self.C, self.lib
        buf = (C.c_int64 * 256)()
        objects = []
        for slot in range(SLOTS):
            if step["kinds"][slot] != KIND_NONE:
                lib.tpw_uitx_dump_object(slot, buf)
                objects.append(list(buf[:OBJECT_FIELDS]))
            else:
                objects.append([0])
        lib.tpw_uitx_dump_globals(buf)
        globals_ = list(buf[:GLOBAL_FIELDS])
        pixels = C.string_at(lib.tpw_uitx_pixels(), PIXEL_BYTES)
        lib.tpw_uitx_heap(buf)
        return dict(objects=objects, globals=globals_, pixels=pixels, heap=list(buf[:4]), log=C.c_int32.in_dll(lib, "tpw_cl_n").value)


# ======================================================================= running a script

RAN = {}   # what the PowerPC side decided to run, for the host side (by case)
STUBS = None


def with_stubs(fn):
    def run(e, case):
        saved = STUBS.activate(e)
        try:
            return fn(e, case)
        finally:
            CL.Stubs.deactivate(e, saved)
    return run


@with_stubs
def ui_ppc(e, case):
    # (a font id outside 0 to 12 makes the original read the word at address 0x14: a page of zeros there for the case)
    mapped = False
    try:
        e.uc.mem_map(0, 0x1000)
        mapped = True
    except Exception:
        pass
    world = None
    try:
        world = Ppc(e, case)
        steps, ran = [], []
        for op in case["ops"]:
            if len(world.L.log) > 3900:
                break   # (the host's log holds 4,096 events)
            code, a = op
            answer = world.op(code, a)
            if answer is None:
                continue
            ran.append(dict(op=op, kinds=list(world.kinds)))
            steps.append(dict(op=code, answer=answer, state=world.dump_state()))
        RAN[id(case)] = ran
        return dict(steps=steps, log=list(world.L.log))
    finally:
        if world is not None:
            world.restore()
        if mapped:
            e.uc.mem_unmap(0, 0x1000)


def ui_host(lib, case):
    host = Host(lib, case)
    steps = []
    try:
        for step in RAN.pop(id(case)):
            code, a = step["op"]
            answer = host.op(code, a)
            steps.append(dict(op=code, answer=answer, state=host.dump_state(step)))
    finally:
        log = CL.host_end(lib)
    return dict(steps=steps, log=log)


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    make_fakes(e)
    STUBS = build_stubs(e)


# ======================================================================= scripts: building blocks

import random as _random

TESTS = []

SMALL = [0, 0, 1, -1, 2, -3, 7, -8, 20, -33, 100, -250, 0x7FFF, -0x8000, 0x8000, 0xFFFF, 0x12345, -0x12345]
TEXT_SAMPLES = ["", "a", "Hello", "Name\nsecond line", "x" * 120, "café 中文", "  ", "A B C D E F G H I J K L M N O P", "\n", "Tab\there"]


def case_of(ops, q=None, texts=None):
    case = dict(ops=ops)
    if q:
        case["q"] = q
    case["texts"] = list(texts) if texts is not None else ["Hello", "", "Name\nsecond line", "x" * 120, "café 中", "A B C"]
    return case


def make(slot, kind, token=0):
    return (1, (slot, kind, token))


def style(slot, justify=0, vjustify=1, wrap=0, f0df=0, font=0, mode=0, ox=0, oy=0, colour=(255, 255, 255, 0)):
    return (2, (slot, justify, vjustify, wrap, f0df, font, mode, ox, oy) + tuple(colour))


def words(slot, created=1, pw=40, ph=30, shown=0, rect=(100, 100, 900, 700), depth=1, f2c=0, sw=40, sh=30, pixels=1):
    return (3, (slot, created, pw, ph, shown) + tuple(rect) + (depth, f2c, sw, sh, pixels))


def screen(width, height, group=0):
    return [(5, (1, width)), (5, (2, height)), (5, (3, group))]


def fonts(group=0, tokens=None):
    tokens = tokens if tokens is not None else [i % FONTS for i in range(13)]
    return (6, (group,) + tuple(tokens))


def callbacks(slot, pre=0, post=0):
    return (8, (slot, pre, post))


def update(slot, text=0):
    return (20, (slot, text))


def change(slot, flags, no_record=0):
    return (21, (slot, flags, no_record))


def query(slot, what, value, kind=0):
    return (22, (slot, what, kind, value))


def destroy(slot, flag=1):
    return (23, (slot, flag))


def glue_destroy(slot, flag=1):
    return (24, (slot, flag))


def staged_style(justify=0, vjustify=1, wrap=0, f0df=0, font=0, mode=0, ox=0, oy=0, colour=(255, 255, 255, 0), text=0, no_style=0):
    return (10, (justify, vjustify, wrap, f0df, font, mode, ox, oy) + tuple(colour) + (text, no_style))


def staged_change(depth=1, word=0, rect=(100, 100, 900, 700), no_rect=0):
    return (11, (depth, word) + tuple(rect) + (no_rect,))


def positions(pairs):
    flat = [v for pair in pairs for v in pair]
    return (9, (len(pairs),) + tuple(flat) + (0,) * (2 * POSITIONS - len(flat)))


# ======================================================================= test 1 to 3: the constructors

KINDS = (KIND_TEXT, KIND_TITLE, KIND_CURSOR)


def ctor_edges(kind):
    def edges():
        for token in (-1, 0, 1, 2):
            yield case_of([make(0, kind, token)])
        # every slot, every token
        yield case_of([make(s, kind, s % 3 - 1 if s % 3 else 0) for s in range(SLOTS)])
        # the first one destroyed and made again in the same slot
        yield case_of([make(0, kind), destroy(0), make(0, kind, 1), destroy(0, 0), make(0, kind, 2)])
        # made, written over by the script's setters, then another made beside it (the constructors set what they set, nothing else)
        yield case_of([make(0, kind), style(0, 2, 2, 1, 9, 5, 1, 100, -100, (1, 2, 3, 4)), make(1, kind, 1)])
    return edges


def ctor_random(kind):
    def rnd(r):
        slots = r.sample(range(SLOTS), r.randint(1, SLOTS))
        ops = [make(s, kind, r.choice([-1, 0, 1, 2])) for s in slots]
        if r.random() < 0.3:
            ops.append(destroy(r.choice(slots), r.choice([1, 0])))
        return case_of(ops)
    return rnd


TESTS.append(FnTest("InterfaceTextObjectSprite::InterfaceTextObjectSprite", ui_ppc, ui_host, edge=ctor_edges(KIND_TEXT), rnd=ctor_random(KIND_TEXT)))
TESTS.append(FnTest("InterfaceTitleTextObject::InterfaceTitleTextObject", ui_ppc, ui_host, edge=ctor_edges(KIND_TITLE), rnd=ctor_random(KIND_TITLE)))
TESTS.append(FnTest("InterfaceCursorPosTextObjectSprite::InterfaceCursorPosTextObjectSprite", ui_ppc, ui_host, edge=ctor_edges(KIND_CURSOR),
                    rnd=ctor_random(KIND_CURSOR)))


# ======================================================================= scenes

BOUNDS_POOL = [0, 1, -1, 3, 17, 40, 100, -100, 639, 4096, 40000, -40000, 0x7FFFF, 0x100000, 0x1FFFFF, 0x7FFFFFFF, -0x80000000, 1 << 21]
DEPTHS = [0, 1, 8, 15, 16, 24, 32]
JUSTIFY_BAD = [-1, 3, 4, 127, -128, 255]


def scene(kind, w=80, h=60, group=0, tokens=None, st=None, sprite=None, pre=0, post=0, slot=0):
    """A screen, a font table, and one object of this kind in `slot` with a style, a surface and the callbacks."""
    ops = screen(w, h, group)
    ops.append(fonts(group, tokens))
    ops.append(make(slot, kind, 0))
    ops.append(style(slot, **(st or {})))
    ops.append(words(slot, **(sprite or {})))
    if pre or post:
        ops.append(callbacks(slot, pre, post))
    return ops


def pick_justify(r):
    return r.choice([0, 1, 2]) if r.random() < 0.9 else r.choice(JUSTIFY_BAD)


def pick_font(r):
    return r.choice(list(range(-1, 13)) * 3 + [13, 14, 100, 127, -128, 255])


def pick_offset(r):
    return r.choice(SMALL + [r.randint(-3000, 3000), 32767, -32768])


def pick_colour(r):
    return tuple(r.choice([0, 255, 128, r.randrange(256)]) for _ in range(4))


def random_style(r):
    return dict(justify=pick_justify(r), vjustify=pick_justify(r) if r.random() < 0.9 else r.choice([0, 1, 2]), wrap=r.choice([0, 0, 1, 0x80, 0xFF, 0x7F]),
                f0df=r.randrange(256), font=pick_font(r), mode=r.choice([0, 0, 1, 1, 2, 0x80, 0xFF]), ox=pick_offset(r), oy=pick_offset(r),
                colour=pick_colour(r))


def random_sprite(r):
    width, height = r.choice([0, 1, 5, 40, 64, 33]), r.choice([0, 1, 7, 30, 64, 48])
    return dict(created=r.choice([0, 1, 1, 1, 2]), pw=r.choice([0, 1, 40, 64, -3]), ph=r.choice([0, 1, 30, 64, 5]), shown=r.choice([0, 1, 1, -1]),
                rect=tuple(r.choice(SMALL + [r.randint(0, 2048)]) for _ in range(4)), depth=r.choice([0, 1, 5, -4, 300]), f2c=r.choice(SMALL),
                sw=width, sh=height, pixels=r.choice([1, 1, 1, 0]))


def random_queues(r, positions=False):
    q = {}
    if r.random() < 0.4:
        q["Hebrew"] = [r.choice([0, 1]) for _ in range(r.randint(1, 4))]
    if r.random() < 0.7:
        q["Depth"] = [r.choice(DEPTHS) for _ in range(r.randint(1, 3))]
    if r.random() < 0.5:
        q["FindColour"] = [r.randrange(256) for _ in range(r.randint(1, 3))]
    if r.random() < 0.6:
        q["Bounds"] = [r.choice(BOUNDS_POOL) for _ in range(r.randint(1, 24))]
    if r.random() < 0.3:
        q["PreDraw"] = [r.choice([0, 1, -1, 0x100, 0]) for _ in range(r.randint(1, 4))]
    if r.random() < 0.2:
        q["Surface.Init"] = [r.choice([0, 1]) for _ in range(r.randint(1, 3))]
    if r.random() < 0.2:
        q["Lay"] = [r.choice([0, 1, -1]) for _ in range(r.randint(1, 3))]
    if positions:
        q["PositionCount"] = [r.choice([0, 1, 2, 3, 5, 8, -1]) for _ in range(r.randint(1, 4))]
    return q


def random_texts(r):
    texts = [r.choice(TEXT_SAMPLES) for _ in range(TEXTS)]
    if r.random() < 0.3:
        texts[r.randrange(TEXTS)] = "".join(chr(r.choice([65 + r.randrange(26), 32, 10, 0xE9, 0x4E2D, 0x7F, 0x80])) for _ in range(r.randint(1, 60)))
    return texts


def random_screen(r):
    return r.choice([1, 2, 7, 40, 64, 80, 100, 160, 2048, 1600, 1200, 32767]), r.choice([1, 2, 5, 30, 48, 60, 100, 120, 1536, 1200, 32767])


# ======================================================================= test 4 and 5: UpdateTextures of the text object and the title

def update_edges(kind):
    def edges():
        # the justifications, the wrap byte and the offsets
        for justify in (0, 1, 2):
            for vjustify in (0, 1, 2):
                for wrap in (0, 1):
                    yield case_of(scene(kind, st=dict(justify=justify, vjustify=vjustify, wrap=wrap, ox=37, oy=-29, font=1, colour=(10, 20, 30, 40))) + [update(0, 0), update(0, 3)])
        for wrap in (0x80, 0xFF, 0x7F, 2):
            yield case_of(scene(kind, st=dict(justify=1, wrap=wrap, ox=-37, oy=29)) + [update(0, 2)])
        # codes outside 0 to 2: the box is four zeros; the vertical code is not told to the formatter
        for bad in JUSTIFY_BAD:
            yield case_of(scene(kind, st=dict(justify=bad, vjustify=1)) + [update(0, 0)])
            yield case_of(scene(kind, st=dict(justify=1, vjustify=bad)) + [update(0, 0)])
        # Hebrew swaps left and right
        for justify in (0, 1, 2, 3):
            yield case_of(scene(kind, st=dict(justify=justify)) + [update(0, 0)], {"Hebrew": [1]})
        # the offsets: negative ones truncate toward zero, in both axes, whichever the justification
        for justify in (0, 1, 2):
            for ox, oy in ((1, 1), (-1, -1), (2047, 1535), (-2047, -1535), (32767, 32767), (-32768, -32768), (2048, 1536), (-2048, -1536), (0, 0)):
                for w, h in ((80, 60), (1, 1), (2048, 1536), (1600, 1200)):
                    yield case_of(scene(kind, w=w, h=h, st=dict(justify=justify, ox=ox, oy=oy, wrap=1)) + [update(0, 0)])
        # the font: the id picks a font of the group the resolution says
        for group in range(4):
            for font in (-1, 0, 1, 5, 6, 11, 12, 13, 14, 100, 127, -128, 255):
                tokens = [(i + group) % FONTS if i % 5 != 4 else -1 for i in range(13)]
                yield case_of(scene(kind, group=group, tokens=tokens, st=dict(font=font)) + [update(0, 0)])
        # the colour mode: 0 searches the palette for white and recolours, 1 keeps the font's colours, anything else leaves the parameters
        for mode in (0, 1, 2, 0x80, 0xFF):
            for index in (0, 0x5A, 255):
                yield case_of(scene(kind, st=dict(mode=mode, colour=(1, 2, 3, 4))) + [update(0, 0)], {"FindColour": [index]})
        # the surface: none, empty, small, large; the callbacks; the depth
        for pixels in (0, 1):
            for sw, sh in ((0, 0), (1, 1), (40, 30), (64, 64), (0, 40), (40, 0), (33, 7)):
                for depth in DEPTHS:
                    for pre, post in ((0, 0), (1, 0), (0, 1), (1, 1)):
                        for answer in ((0,) if not pre else (0, 1, -1)):
                            q = {"Depth": [depth], "PreDraw": [answer]}
                            yield case_of(scene(kind, sprite=dict(sw=sw, sh=sh, pixels=pixels), pre=pre, post=post) + [update(0, 0)], q)
        # the texts
        for token in range(TEXTS):
            yield case_of(scene(kind) + [update(0, token), update(0, token)])
        yield case_of(scene(kind) + [update(0, -1), update(0, 0), update(0, -1)])
        yield case_of(scene(kind, w=0, h=0) + [update(0, -1)])
        yield case_of(scene(kind) + [update(0, 5), update(0, 5)], texts=["a"] * 5 + ["z" * 150])
        # the rectangle the text object sits at, and what the formatter reports
        for rect in ((0, 0, 0, 0), (100, 100, 900, 700), (-100, -200, 50, 60), (32767, 32767, -32768, -32768), (2048, 1536, 0, 0)):
            for bounds in ([], [0, 0, 0, 0], [5, 6, 7, 8], [-5, -6, -7, -8], [1 << 20, 1 << 21, 0x7FFFFFFF, -0x80000000], [100, 100, 3000, 2000]):
                q = {"Bounds": bounds} if bounds else {}
                for w, h in ((80, 60), (1600, 1200), (3, 7)):
                    yield case_of(scene(kind, w=w, h=h, sprite=dict(rect=rect)) + [update(0, 0)], q)
        # the formatter's answers
        yield case_of(scene(kind) + [update(0, 0), update(0, 0)], {"Lay": [1, -1, 0]})
        # second update after a change of font, colour and size (the object keeps its second rectangle between them)
        yield case_of(scene(kind) + [update(0, 0), style(0, 2, 2, 1, 3, 2, 1, 5, 6, (9, 8, 7, 6)), update(0, 2), words(0, sw=10, sh=10), update(0, 1)])
    return edges


def update_random(kind):
    def rnd(r):
        w, h = random_screen(r)
        group = r.randrange(4)
        tokens = [r.choice([-1, 0, 1, 2, 3, 4, 5]) for _ in range(13)]
        ops = scene(kind, w=w, h=h, group=group, tokens=tokens, st=random_style(r), sprite=random_sprite(r), pre=r.choice([0, 1]), post=r.choice([0, 1]))
        for _ in range(r.randint(1, 5)):
            k = r.random()
            if k < 0.25:
                ops.append(style(0, **random_style(r)))
            elif k < 0.4:
                ops.append(words(0, **random_sprite(r)))
            elif k < 0.5:
                ops.append(fonts(group, [r.choice([-1, 0, 1, 2, 3, 4, 5]) for _ in range(13)]))
            elif k < 0.6:
                ops.append(callbacks(0, r.choice([0, 1]), r.choice([0, 1])))
            elif k < 0.65:
                ops.append((5, (r.choice([1, 2]), r.choice([1, 2, 7, 64, 100, 2048]))))
            ops.append(update(0, r.choice([-1, 0, 1, 2, 3, 4, 5])))
        return case_of(ops, random_queues(r), random_texts(r))
    return rnd


TESTS.append(FnTest("InterfaceTextObjectSprite::UpdateTextures", ui_ppc, ui_host, edge=update_edges(KIND_TEXT), rnd=update_random(KIND_TEXT)))
TESTS.append(FnTest("InterfaceTitleTextObject::UpdateTextures", ui_ppc, ui_host, edge=update_edges(KIND_TITLE), rnd=update_random(KIND_TITLE)))


# ======================================================================= test 6: the destructors

FLAGS_DTOR = [1, 0, -1, 2, 0x7FFF, 0x8000, -0x8000, 0x10000, 0x10001, 0xFFFF, 0x18000, -2]


def dtor_edges(kind, destroy=destroy):
    def edges():
        for flag in FLAGS_DTOR:
            yield case_of([make(0, kind), destroy(0, flag)])
        # after use: drawn into, shown, linked
        for flag in (1, 0, -1):
            yield case_of(scene(kind, sprite=dict(shown=1)) + [update(0, 0), destroy(0, flag)])
            yield case_of(scene(kind) + [make(1, kind, 1), update(1, 2), destroy(0, flag), update(1, 3), destroy(1, flag)])
        # all slots, destroyed in an order, one made again
        yield case_of([make(s, kind, s % 3 - 1) for s in range(SLOTS)] + [destroy(s, 1) for s in (3, 1, 5, 0, 2, 4)] + [make(2, kind, 2), destroy(2, 1)])
    return edges


def dtor_random(kind, destroy=destroy):
    def rnd(r):
        w, h = random_screen(r)
        ops = scene(kind, w=w, h=h, st=random_style(r), sprite=random_sprite(r))
        live = {0}
        for _ in range(r.randint(1, 10)):
            k = r.random()
            if k < 0.3 and len(live) < SLOTS:
                s = r.choice([x for x in range(SLOTS) if x not in live])
                ops.append(make(s, kind, r.choice([-1, 0, 1, 2])))
                live.add(s)
            elif k < 0.6 and live:
                s = r.choice(sorted(live))
                ops.append(destroy(s, r.choice(FLAGS_DTOR)))
                live.discard(s)
            elif live:
                ops.append(update(r.choice(sorted(live)), r.choice([-1, 0, 1])))
        return case_of(ops, random_queues(r), random_texts(r))
    return rnd


TESTS.append(FnTest("InterfaceTextObjectSprite::~InterfaceTextObjectSprite", ui_ppc, ui_host, edge=dtor_edges(KIND_TEXT), rnd=dtor_random(KIND_TEXT)))
TESTS.append(FnTest("InterfaceTitleTextObject::~InterfaceTitleTextObject", ui_ppc, ui_host, edge=dtor_edges(KIND_TITLE), rnd=dtor_random(KIND_TITLE)))
TESTS.append(FnTest("InterfaceCursorPosTextObjectSprite::~InterfaceCursorPosTextObjectSprite", ui_ppc, ui_host, edge=dtor_edges(KIND_CURSOR),
                    rnd=dtor_random(KIND_CURSOR)))
TESTS.append(FnTest("textobj_callee::CursorDtor", ui_ppc, ui_host, edge=dtor_edges(KIND_CURSOR, glue_destroy),
                    rnd=dtor_random(KIND_CURSOR, glue_destroy)))


# ======================================================================= test 7: Change

STYLE_FLAGS = 0x400 | 0x800 | 0x1000 | 0x2000 | 0x4000 | 0x8000 | 0x10000 | 0x20000 | 0x40000
FLAG_BITS = [0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000, 0x10000, 0x20000, 0x40000,
             0x80000, 0x100000, 0x80000000]


def change_world(kind, made, **style_args):
    """An object of this kind (with a surface already made, or fresh), the staged style and record."""
    ops = screen(80, 60) + [fonts(0)]
    ops.append(make(0, kind, 1))
    ops.append(style(0, 1, 1, 0, 1, 2, 0, 3, 4, (5, 6, 7, 8)))
    if made:
        ops.append(words(0))
    ops.append(staged_style(**style_args) if style_args else staged_style(justify=2, vjustify=0, wrap=1, f0df=7, font=3, mode=1, ox=-77, oy=55,
                                                                           colour=(11, 22, 33, 44), text=2))
    ops.append(staged_change(depth=3, word=9, rect=(200, 300, 1200, 900)))
    return ops


def change_edges():
    for kind in KINDS:
        for made in (False, True):
            for bit in FLAG_BITS:
                yield case_of(change_world(kind, made) + [change(0, bit)])
            for flags in (0, STYLE_FLAGS, STYLE_FLAGS | 0x100, 0x100, 0x7FD00, 0x7FFFF, 0xFFFFFFFF, 0x200, 0x7FD00 | 0x10 | 2 | 4, 0x80000 | 0x40 | 1):
                yield case_of(change_world(kind, made) + [change(0, flags), change(0, flags)])
        # the text: none, empty, a text
        for text in (-1, 0, 1, 5):
            yield case_of(change_world(kind, True, justify=1, vjustify=2, wrap=0, font=4, text=text) + [change(0, 0x100), change(0, STYLE_FLAGS | 0x100)])
        # no style, no record, no rectangle
        yield case_of(change_world(kind, True) + [staged_style(no_style=1), change(0, STYLE_FLAGS | 0x100), change(0, 0x100), change(0, 0x10)])
        yield case_of(change_world(kind, True) + [change(0, STYLE_FLAGS | 0x100, 1), change(0, 0x10, 1), change(0, 0, 1)])
        yield case_of(change_world(kind, False) + [staged_change(no_rect=1), change(0, 0x2), change(0, 0x4), change(0, STYLE_FLAGS | 0x100), change(0, 0x1000)])
        # a fresh object gets its surface from the first rectangle, and the text is drawn into it
        yield case_of(change_world(kind, False) + [change(0, 0x10 | 0x2), change(0, 0x100 | STYLE_FLAGS), change(0, 0x100), change(0, 0x40000)])
        yield case_of(change_world(kind, False) + [change(0, 0x100 | STYLE_FLAGS | 0x10), change(0, 0x100 | 0x10)], {"Surface.Init": [0, 1]})
        # the colour word: 0x40000 writes the four colour bytes as one word, the sprite's colour
        for colour in ((0, 0, 0, 0), (255, 255, 255, 255), (1, 2, 3, 4), (128, 0, 255, 17)):
            yield case_of(change_world(kind, True, colour=colour, text=-1) + [change(0, 0x40000), change(0, 0x40000 | 0x100)])
        # the offsets and the bytes keep their signs
        for ox, oy in ((0, 0), (-1, 1), (32767, -32768), (-32768, 32767), (256, -256)):
            for font in (-1, 0, 12, 13, 255, 128):
                yield case_of(change_world(kind, True, ox=ox, oy=oy, font=font, text=0, justify=255, vjustify=128) + [change(0, 0x4000 | 0x8000 | 0x10000 | 0x400 | 0x800 | 0x100), update(0, 0)])


def change_random(kind_choice):
    def rnd(r):
        kind = r.choice(KINDS) if kind_choice is None else kind_choice
        w, h = random_screen(r)
        ops = screen(w, h, 0) + [fonts(0, [r.choice([-1, 0, 1, 2, 3, 4, 5]) for _ in range(13)])]
        ops.append(make(0, kind, r.choice([-1, 0, 1])))
        ops.append(style(0, **random_style(r)))
        if r.random() < 0.6:
            ops.append(words(0, **random_sprite(r)))
        if r.random() < 0.4:
            ops.append(callbacks(0, r.choice([0, 1]), r.choice([0, 1])))
        for _ in range(r.randint(1, 6)):
            if r.random() < 0.7:
                st = random_style(r)
                ops.append(staged_style(st["justify"], st["vjustify"], st["wrap"], st["f0df"], st["font"], st["mode"], st["ox"], st["oy"], st["colour"],
                                        text=r.choice([-1, 0, 1, 2, 3, 4, 5]), no_style=int(r.random() < 0.1)))
            if r.random() < 0.7:
                ops.append(staged_change(depth=r.choice(SMALL), word=r.choice(SMALL), rect=tuple(r.choice([0, 100, 500, 1024, 2048, -50, r.randint(-100, 2100)]) for _ in range(4)),
                                         no_rect=int(r.random() < 0.15)))
            flags = 0
            for bit in FLAG_BITS:
                if r.random() < (0.5 if bit & STYLE_FLAGS or bit == 0x100 else 0.25):
                    flags |= bit
            if r.random() < 0.1:
                flags = r.getrandbits(32)
            ops.append(change(0, flags, int(r.random() < 0.08)))
            if r.random() < 0.3:
                ops.append(update(0, r.choice([-1, 0, 1, 2])))
        return case_of(ops, random_queues(r), random_texts(r))
    return rnd


TESTS.append(FnTest("InterfaceTextObjectSprite::Change", ui_ppc, ui_host, edge=change_edges, rnd=change_random(None)))


# ======================================================================= test 8: Query

QUERY_WHAT = [1, 2, 0, 3, 4, 0x100, 0xFFFFFFFF, 0x80000000, 0x10001]
INDEXES = [-1, 0, 1, 2, 3, 4, 7, 8, 9, 100, 0x7FFFFFFF, -0x80000000, -2]
PEN_POSITIONS = [(0, 0), (37, 21), (-5, -9), (639, 479), (1 << 20, 1 << 21), (0x7FFFFFFF, -0x80000000), (100000, -100000)]


def pairs_of(r, n):
    return [(r.choice([0, 1, 10, 33, 79, -4, 639, 5000, r.randint(-100, 700)]), r.choice([0, 1, 8, 29, 59, -4, 479, 5000, r.randint(-100, 500)])) for _ in range(n)]


def query_scene(kind, pairs, pen, **kwargs):
    ops = scene(kind, **kwargs)
    ops.append(positions(pairs))
    ops.append((7, (0, 1, pen[0], pen[1])))
    return ops


def query_edges():
    # the position of a character: inside the positions, at the edges, outside; the pen position when none is there
    for count in (0, 1, 3, 8, -1):
        pairs = [(3 + 11 * i, 5 + 7 * i) for i in range(max(min(count, 8), 0))]
        for pen in PEN_POSITIONS[:5]:
            for index in INDEXES:
                yield case_of(query_scene(KIND_CURSOR, pairs, pen) + [query(0, 1, index)], {"PositionCount": [count]})
    for w, h in ((1, 1), (80, 60), (2048, 1536), (1600, 1200), (7, 3)):
        yield case_of(query_scene(KIND_CURSOR, [(-5, -9), (100, 200), (639, 479)], (-33, 77), w=w, h=h) +
                      [query(0, 1, 0), query(0, 1, 1), query(0, 1, 2), query(0, 1, 3), query(0, 1, -1)], {"PositionCount": [3]})
    # the codes
    for kind in KINDS:
        for what in QUERY_WHAT:
            for value in (0, 1, 5):
                yield case_of(query_scene(kind, [(1, 2), (3, 4), (5, 6)], (7, 8)) + [query(0, what, value), query(0, what, value, 1)], {"PositionCount": [3]})
    # the measure of a text: the justifications, the font, the colour mode, the offsets; the bottom cut to the object's height
    for justify in (0, 1, 2, 3, -1):
        for vjustify in (0, 1, 2, 3):
            for wrap in (0, 1):
                yield case_of(scene(KIND_CURSOR, st=dict(justify=justify, vjustify=vjustify, wrap=wrap, ox=37, oy=-29, font=1)) + [query(0, 2, 0, 1), query(0, 2, 2, 1)])
    for bad in JUSTIFY_BAD:
        yield case_of(scene(KIND_CURSOR, st=dict(justify=bad)) + [query(0, 2, 0, 1)])
    for justify in (0, 1, 2, 3):
        yield case_of(scene(KIND_CURSOR, st=dict(justify=justify)) + [query(0, 2, 0, 1)], {"Hebrew": [1]})
    for mode in (0, 1, 2, 0x80, 0xFF, 3):
        for colour in ((255, 255, 255, 0), (1, 2, 3, 4), (0, 0, 0, 0)):
            yield case_of(scene(KIND_CURSOR, st=dict(mode=mode, colour=colour)) + [query(0, 2, 0, 1)])
    for group in range(4):
        for font in (-1, 0, 5, 12, 13, 255, 127, -128):
            tokens = [(i + group) % FONTS if i % 5 != 4 else -1 for i in range(13)]
            yield case_of(scene(KIND_CURSOR, group=group, tokens=tokens, st=dict(font=font)) + [query(0, 2, 1, 1)])
    for token in range(TEXTS):
        yield case_of(scene(KIND_CURSOR) + [query(0, 2, token, 1), query(0, 2, token, 1)])
    yield case_of(scene(KIND_CURSOR, w=0, h=0) + [query(0, 2, -1, 1), query(0, 2, 0), query(0, 0, 0, 1)])
    # the cut: the query rectangle's bottom against the height of the object (mRect's bottom less its top)
    for rect in ((0, 0, 100, 100), (0, 50, 100, 51), (0, 100, 100, 50), (0, 0, 100, 0), (0, -5, 100, 5), (32767, -32768, 0, 32767), (0, 0, 0, 0)):
        for bounds in ([0, 0, 0, 0], [3, 4, 5, 6], [0, 0, 640, 480], [0, 0, 640, 100], [0, 0, 640, 101], [0, 0, 640, 99], [0, -100, 640, -50], [1, 2, 3, 0x7FFFFFFF]):
            yield case_of(scene(KIND_CURSOR, w=64, h=48, sprite=dict(rect=rect)) + [query(0, 2, 0, 1)], {"Bounds": bounds})
    # two queries in a row, one of each, the query rectangle is the one global
    yield case_of(query_scene(KIND_CURSOR, [(9, 9), (20, 30)], (4, 4)) + [query(0, 1, 1), query(0, 2, 0, 1), query(0, 1, 5), (12, (1, 2, 3, 4)), query(0, 0, 0)],
                  {"PositionCount": [2]})
    yield case_of(query_scene(KIND_CURSOR, [(9, 9), (20, 30)], (4, 4)) + [make(1, KIND_CURSOR, 0), query(1, 1, 0), query(0, 1, 1)], {"PositionCount": [0, 2]})


def query_random(r):
    kind = r.choice([KIND_CURSOR] * 6 + [KIND_TEXT, KIND_TITLE])
    w, h = random_screen(r)
    pairs = pairs_of(r, r.randint(0, POSITIONS))
    ops = query_scene(kind, pairs, r.choice(PEN_POSITIONS), w=w, h=h, group=r.randrange(4), tokens=[r.choice([-1, 0, 1, 2, 3, 4, 5]) for _ in range(13)],
                      st=random_style(r), sprite=random_sprite(r))
    for _ in range(r.randint(1, 8)):
        k = r.random()
        if k < 0.15:
            ops.append(style(0, **random_style(r)))
        elif k < 0.25:
            ops.append(positions(pairs_of(r, r.randint(0, POSITIONS))))
        elif k < 0.3:
            ops.append((7, (0, r.choice([0, 1, 255]), r.choice(SMALL), r.choice(SMALL))))
        elif k < 0.4:
            ops.append((12, tuple(r.choice(SMALL) for _ in range(4))))
        elif k < 0.45:
            ops.append(words(0, **random_sprite(r)))
        what = r.choice(QUERY_WHAT + [1, 1, 2, 2, 2])
        if what == 1:
            ops.append(query(0, 1, r.choice(INDEXES + [r.randint(-3, 10)])))
        elif what == 2:
            ops.append(query(0, 2, r.choice([-1, 0, 1, 2, 3, 4, 5]), 1))
        else:
            ops.append(query(0, what, r.choice(SMALL)))
    q = random_queues(r, positions=True)
    return case_of(ops, q, random_texts(r))


TESTS.append(FnTest("InterfaceCursorPosTextObjectSprite::Query", ui_ppc, ui_host, edge=query_edges, rnd=query_random))


# ======================================================================= test 9: scripts of 200 operations over the whole world

def long_world(r):
    """Six objects of random kinds with styles and surfaces, a screen, the font table, character positions and the staged records."""
    w, h = random_screen(r)
    ops = screen(w, h, r.randrange(4))
    for group in range(4):
        ops.append(fonts(group, [r.choice([-1, 0, 1, 2, 3, 4, 5]) for _ in range(13)]))
    for slot in range(SLOTS):
        ops += [make(slot, r.choice(KINDS), r.choice([-1, 0, 1, 2])), style(slot, **random_style(r)), words(slot, **random_sprite(r))]
        if r.random() < 0.4:
            ops.append(callbacks(slot, r.choice([0, 1]), r.choice([0, 1])))
    ops.append(positions(pairs_of(r, r.randint(0, POSITIONS))))
    st = random_style(r)
    ops.append(staged_style(st["justify"], st["vjustify"], st["wrap"], st["f0df"], st["font"], st["mode"], st["ox"], st["oy"], st["colour"], text=r.randrange(TEXTS)))
    ops.append(staged_change(depth=r.choice(SMALL), word=r.choice(SMALL), rect=(100, 100, 900, 700)))
    return ops


def long_flags(r):
    flags = 0
    for bit in FLAG_BITS:
        if r.random() < (0.45 if bit & STYLE_FLAGS or bit == 0x100 else 0.2):
            flags |= bit
    return r.getrandbits(32) if r.random() < 0.08 else flags


def long_op(r):
    k = r.random()
    s = r.randrange(SLOTS)
    if k < 0.2:
        return update(s, r.choice([-1, 0, 1, 2, 3, 4, 5]))
    if k < 0.34:
        return change(s, long_flags(r), int(r.random() < 0.05))
    if k < 0.48:
        what = r.choice([1, 1, 2, 2, 2, 0, 3, 0x100])
        if what == 2:
            return query(s, 2, r.choice([-1, 0, 1, 2, 3, 4, 5]), 1)
        return query(s, what, r.choice(INDEXES))
    if k < 0.5:
        return destroy(s, r.choice(FLAGS_DTOR))
    if k < 0.6:
        return make(s, r.choice(KINDS), r.choice([-1, 0, 1, 2]))
    if k < 0.65:
        return style(s, **random_style(r))
    if k < 0.7:
        return words(s, **random_sprite(r))
    if k < 0.73:
        return (4, (s,) + tuple(r.choice(SMALL) for _ in range(4)))
    if k < 0.76:
        return (5, (r.choice([1, 2, 3]), r.choice([1, 2, 7, 64, 100, 2048]) if r.random() < 0.97 else r.choice([0, -5, 3])))
    if k < 0.79:
        return fonts(r.randrange(4), [r.choice([-1, 0, 1, 2, 3, 4, 5]) for _ in range(13)])
    if k < 0.82:
        return (7, (s, r.choice([0, 1, 255]), r.choice(SMALL), r.choice(SMALL)))
    if k < 0.86:
        return callbacks(s, r.choice([0, 1]), r.choice([0, 1]))
    if k < 0.9:
        return positions(pairs_of(r, r.randint(0, POSITIONS)))
    if k < 0.95:
        st = random_style(r)
        return staged_style(st["justify"], st["vjustify"], st["wrap"], st["f0df"], st["font"], st["mode"], st["ox"], st["oy"], st["colour"],
                            text=r.choice([-1, 0, 1, 2, 3, 4, 5]), no_style=int(r.random() < 0.1))
    if k < 0.99:
        return staged_change(depth=r.choice(SMALL), word=r.choice(SMALL), rect=tuple(r.choice([0, 100, 500, 1024, 2048, -50, r.randint(-100, 2100)]) for _ in range(4)),
                             no_rect=int(r.random() < 0.15))
    return (12, tuple(r.choice(SMALL) for _ in range(4)))


def long_script(r):
    ops = long_world(r)
    for _ in range(200):
        ops.append(long_op(r))
    return case_of(ops, random_queues(r, positions=True), random_texts(r))


def long_edges():
    for seed in range(8):
        yield long_script(_random.Random(3000 + seed))


TESTS.append(FnTest("scripts of 200 operations over the three kinds of text object", ui_ppc, ui_host, edge=long_edges, rnd=long_script))
