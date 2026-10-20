"""Differential tests for the ltms text formatter and font renderer, against libtpw_unit_TextFormatter.dylib
(tests/unit_TextFormatter.cpp):

  src/ltms_textformatter.cpp  TbTextFormatter: constructor, destructor, SetVerticalJustification, SetHorizontalJustification,
                              GetCharPositionArray, GetCharPositionArraySize, EmptyStringProcess, Draw, Measure, DrawText,
                              CreateInfoList, GetCharType, Layout, LayoutLines, GetCharIdxFromPosition and the three
                              std::vector tear-downs (characters, lines, points)
  src/ltms_fontrender.cpp     TbFontRender: constructor, destructor, Initialise, DrawChar; TbFontRenderParams: UseReColour,
                              UsePreColoured, SetFont, GetFont (w44), operator=, GetStyle, GetType (w45a: real here since that unit)

Real on both sides: those, plus the parameters' constructors and destructor (src/tb_fontrenderparams.cpp), which the formatter's
render runs. Fakes that record the same event on both sides: the three fonts (a font's AddRef, Release, GetCharSize,
GetVerticalSpacing, GetCaps, GetType and GetMBToUnicode) and the three multibyte converters they hand out, thirty render methods
behind the method table, the vectors' clear, push_back and resize (a block that doubles from 1; a block is a labelled operator new
block), ProcessLine (leaves, reverses or rotates a line on request), TbRect::Bounding and the render area's constructor.

A case is a script of operations on a small world: two formatters, three stand-alone parameter objects, two stand-alone renders, three
fonts, three converters, strings. After every operation the answer, the output words, the call log of that operation, the heap
traffic and every live object (as a flat list of numbers: the Mac's words by their offsets, the vectors with their elements) are
compared. The PowerPC side runs the original; objects are built over a filler byte, so a field nothing sets is compared as untouched.

Quirks the tests pin: SetHorizontalJustification stores 0 to 2 and answers -1 all the same; the vertical values are 2, 4 and 5;
CreateInfoList takes a reference on the font and never lets it go, reads the second byte of a double-byte character even when it is
the terminator, numbers characters in a halfword (a string of 65,537 characters wraps) and leaves the padding of its list entries
as it finds it; Layout never fails; LayoutLines does not mark the characters of the last line hidden (its marking loop starts at the
end of the run), compares in unsigned arithmetic and breaks a line that has no space in front of the character that overflows;
DrawText centres a line with a logical shift of an unsigned difference, indexes the positions array by the character's own number
and skips the line-break characters and the hidden ones; Draw answers 0 whatever DrawText says; Measure answers -1 without an
output rectangle and writes none for an empty string; Initialise accepts a negative style or type and reads the method table as a
flat run of thirty words.
"""
import ctypes as C
import random
import struct
import types

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_TextFormatter"

MINUS = 0xFFFFFFFF
# ---- the unit's functions (all run as the original on the PowerPC side)
F_CTOR, F_DTOR, SET_V, SET_H = 0x10005DC4, 0x100061A8, 0x10006288, 0x1000632C
GET_ARR, GET_ARR_SIZE, EMPTY, DRAW, MEASURE = 0x100063B4, 0x100063FC, 0x10006448, 0x100065B8, 0x100066F4
DRAWTEXT, CREATE, CHARTYPE, LAYOUT, LAYOUT_LINES, IDX = 0x1000700C, 0x10007514, 0x100076EC, 0x1000776C, 0x10007994, 0x10007C64
TD_CHARS, TD_LINES, TD_POINTS = 0x10007CB4, 0x1000839C, 0x1000879C
R_CTOR, R_DTOR, R_INIT, R_DRAWCHAR = 0x10001FDC, 0x10002050, 0x100020E4, 0x100021FC
P_RECOLOUR, P_PRE, P_SETFONT = 0x10002800, 0x10002888, 0x100029D0
P_CTOR, P_DTOR, P_COPY, P_ALPHA = 0x10002350, 0x10002400, 0x100024AC, 0x1000296C   # real: the render runs them
P_GETFONT = 0x10002AF4   # real since unit w44 (the font's AddRef is the fake)
P_ASSIGN, P_GETSTYLE, P_GETTYPE = 0x100025E8, 0x10002AB8, 0x10002B70   # real since unit w45a
REAL = [F_CTOR, F_DTOR, SET_V, SET_H, GET_ARR, GET_ARR_SIZE, EMPTY, DRAW, MEASURE, DRAWTEXT, CREATE, CHARTYPE, LAYOUT, LAYOUT_LINES, IDX,
        TD_CHARS, TD_LINES, TD_POINTS, R_CTOR, R_DTOR, R_INIT, R_DRAWCHAR, P_RECOLOUR, P_PRE, P_SETFONT, P_CTOR, P_DTOR, P_COPY, P_ALPHA, P_GETFONT, P_ASSIGN, P_GETSTYLE, P_GETTYPE]
# the fakes (in-binary functions)
CHAR_CLEAR, CHAR_PUSH, LINE_CLEAR, LINE_PUSH, PT_CLEAR, PT_RESIZE = 0x10007D74, 0x10007E0C, 0x10008454, 0x100084E4, 0x1000884C, 0x100088D4
PROCESS_LINE = 0x10006CFC
AREA_CTOR, OPERATOR_DELETE = 0x1000F724, 0x1000F694
METHOD_TABLE = 0x1001103C   # 2 x 5 x 3 words
BOUNDING = "Bounding__6TbRectCFRC6TbRect"

SIZE_FORMATTER, SIZE_PARAMS, SIZE_RENDER = 0xB4, 0x50, 0x58
GUARD = 32
GAP = 32   # behind every block the fakes make (the host's kGap)
NFONT, NCONV, NFORM, NPARAM, NREND, NMETH = 3, 3, 2, 3, 2, 30

# ---- labels (the host's constants in unit_TextFormatter.cpp)
LAB_FONT, LAB_F, LAB_FP, LAB_P, LAB_R, LAB_RP, LAB_M = 10, 20, 30, 40, 50, 54, 60
FIRST_NEW = 100
MAX_QUEUE = 64
BIG = 0x40000000   # the vector blocks of the PowerPC side (a region of its own, reset by every case)
BIG_SIZE = 0x2000000

OPS = ["f_ctor", "f_dtor", "set_v", "set_h", "get_arr", "get_arr_size", "idx", "char_type", "create", "layout", "lines", "drawtext",
       "empty", "draw", "measure", "r_ctor", "r_dtor", "r_init", "r_drawchar", "p_ctor", "p_dtor", "recolour", "precol", "setfont",
       "teardown", "getfont"]
SETUP_OPS = ["chars", "poke", "mode"]   # operations that are not functions of the unit: they set the world up on both sides
STATELESS = {"char_type", "idx", "get_arr_size"}   # no dump after these (the last operation of a script always dumps)


def s32(v):
    return T.s32(v & MINUS)


def u16(d, o):
    return struct.unpack(">H", d[o:o + 2])[0]


def rect_for(glyph, x, y):
    """What the fake methods draw for a glyph at a position (the host's RectFor)."""
    w = 3 + glyph % 9
    h = 5 + glyph % 4
    m = glyph % 7
    if m == 3:
        return (x, y, x, y)
    if m == 5:
        return (x, y, x, y + h)
    if m == 6:
        return (x, y, x + w, y)
    if m == 0:
        return (x, y, x + w, y + h)
    return (x + 1, y - 2, x + w + 3, y + h)


# ======================================================================= the PowerPC side: persistent fixtures


def persist(e):
    """Fonts, converters and render methods that live across cases; their behaviour comes from the current case (e.tfc)."""
    if hasattr(e, "tfp"):
        return e.tfp
    p = types.SimpleNamespace()
    e.tfp = p
    vt = T.persist_alloc(e, 0x40)

    def slot(table, offset, fn):
        e.write(table + offset, struct.pack(">I", T.fake_function(e, fn)))

    slot(vt, 0x08, lambda e: e.tfc.font_release(e.r(3)))
    slot(vt, 0x0C, lambda e: e.tfc.font_addref(e.r(3)))
    slot(vt, 0x24, lambda e: e.tfc.font_charsize(e))
    slot(vt, 0x28, lambda e: e.set_r(3, e.tfc.font_spacing(e.r(3))))
    slot(vt, 0x2C, lambda e: e.set_r(3, e.tfc.font_caps(e.r(3))))
    slot(vt, 0x30, lambda e: e.set_r(3, T.u32(e.tfc.font_type(e.r(3)))))
    slot(vt, 0x34, lambda e: e.set_r(3, e.tfc.font_converter(e.r(3))))
    p.fonts = [T.persist_alloc(e, 0x10, struct.pack(">I", vt) + bytes(12)) for _ in range(NFONT)]
    p.conv = [T.persist_alloc(e, 0x10) for _ in range(NCONV)]
    p.tables = [T.persist_alloc(e, 2 * 4096) for _ in range(NCONV)]
    mvt = T.persist_alloc(e, 0x20)
    slot(mvt, 0x0C, lambda e: e.tfc.method_drawchar(e))
    p.methods = [T.persist_alloc(e, 0x10, struct.pack(">I", mvt) + bytes(12)) for _ in range(NMETH)]
    e.uc.mem_map(BIG, BIG_SIZE)
    p.saved_table = bytes(e.read(METHOD_TABLE, 120))
    return p


# ======================================================================= the PowerPC side: one case


class Ppc:
    """One case on the PowerPC side: the world, the fakes (as methods) and the operations."""

    def __init__(self, e, case):
        self.e = e
        e.tfc = self
        self.case = case
        self.p = persist(e)
        self.heap = [0, 0, 0, 0]
        self.big_top = BIG
        self.quiet = False
        self.mmode = 0
        self.F = [0] * NFORM
        self.P = [0] * NPARAM
        self.R = [0] * NREND
        self.font_of = {a: i for i, a in enumerate(self.p.fonts)}
        self.cfg = case["fonts"]
        # the converters
        for i, c in enumerate(case["convs"]):
            e.write(self.p.tables[i], struct.pack(">%dH" % len(c["table"]), *c["table"]) + bytes(2 * (4096 - len(c["table"]))))
            e.write(self.p.conv[i], struct.pack(">IIBBHI", 0, 1, c["lead"], 0, c["count"], self.p.tables[i]))
        # the method table
        e.write(METHOD_TABLE, struct.pack(">30I", *[0 if m == 0 else self.p.methods[m - 1] for m in case["table"]]))
        # strings
        self.texts = [e.alloc(len(t) + 16, t + b"\0Q" + bytes(14)) for t in case["texts"]]
        fixed = {}
        fixed.update({a: LAB_FONT + i for i, a in enumerate(self.p.fonts)})
        fixed.update({a: LAB_M + i for i, a in enumerate(self.p.methods)})
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("queues", {}).items()}
        self.L = e.cl = CL.PpcLog(fixed, queues, first_new=FIRST_NEW)
        self.saved = None

    def finish(self):
        self.e.write(METHOD_TABLE, self.p.saved_table)

    # ---- the fakes
    def label(self, label, address):
        for a in [a for a, l in self.L.labels.items() if l == label]:
            del self.L.labels[a]
        if address:
            self.L.labels[address] = label

    def delete(self, address):
        if address:
            self.heap[2] += 1
            if self.L.lab(address) > 0:
                self.L.rec("delete", (self.L.lab(address),))
                self.L.forget(address)

    def bump(self, size):
        address = self.big_top
        self.big_top += (size + 15) & ~15
        if self.big_top > BIG + BIG_SIZE:
            raise MemoryError("vector blocks exhausted")
        return address

    def font_addref(self, font):
        self.L.rec("AddRef", (self.L.lab(font),))

    def font_release(self, font):
        self.L.rec("Release", (self.L.lab(font),))

    def font_charsize(self, e):
        font, index = e.r(4), e.r(5) & 0xFFFF
        if not self.quiet:
            self.L.rec("GetCharSize", (self.L.lab(font), index))
        n = self.font_of[font]
        c = self.cfg[n]
        if c.get("widths"):
            width = c["widths"][index] if index < len(c["widths"]) else 0
        else:
            width = (c["wbase"] + (index * c["wmul"] + n * 5) % c["wmod"]) & MINUS
        e.write(e.r(3), struct.pack(">II", width, 0x7000 + index))

    def font_spacing(self, font):
        self.L.rec("GetVerticalSpacing", (self.L.lab(font),))
        return self.cfg[self.font_of[font]]["spacing"]

    def font_caps(self, font):
        self.L.rec("GetCaps", (self.L.lab(font),))
        return self.cfg[self.font_of[font]]["caps"]

    def font_type(self, font):
        self.L.rec("GetType", (self.L.lab(font),))
        return self.cfg[self.font_of[font]]["type"]

    def font_converter(self, font):
        self.L.rec("GetMBToUnicode", (self.L.lab(font),))
        c = self.cfg[self.font_of[font]]["conv"]
        return self.p.conv[c] if 0 <= c < NCONV else 0

    def method_drawchar(self, e):
        method, at, glyph, rect, params = e.r(4), e.r(5), e.r(6) & 0xFFFF, e.r(7), e.r(8)
        x, y = e.s32(at), e.s32(at + 4)
        self.L.rec("Method::DrawChar", (self.L.lab(method), x, y, glyph, self.L.lab(params)))
        if self.mmode == 0:
            e.write(rect, struct.pack(">4i", *rect_for(glyph, x, y)))
        elif self.mmode == 2:
            e.write(rect, struct.pack(">4i", x, y, x, y))
        elif self.mmode == 3:
            e.write(rect, struct.pack(">4i", x, y, x + 5, y + 6))
        e.write(e.r(3), struct.pack(">i", T.s32(T.u32(self.L.ret("MethodDrawChar", 0)))))

    def owner(self, address):
        for i, f in enumerate(self.F):
            if f and f <= address < f + SIZE_FORMATTER:
                return i
        return -1

    def owner_label(self, address):
        i = self.owner(address)
        return 0 if i < 0 else LAB_F + i

    def char_index(self, vector, pointer):
        i = self.owner(vector)
        if i < 0 or self.e.u32(self.F[i] + 0x64) == 0:
            return 0
        return (pointer - self.e.u32(self.F[i] + 0x64)) // 16

    def grow(self, v, size_of):
        e = self.e
        size, cap = e.u32(v + 4), e.u32(v)
        if size < cap:
            return
        ncap = 1 if cap == 0 else cap * 2
        block = self.bump(ncap * size_of + GAP)
        e.write(block, b"\xa5" * (ncap * size_of))
        self.heap[0] += 1
        self.L.rec("alloc", (ncap, self.L.newlab(block)))
        old = e.u32(v + 8)
        if size:
            e.write(block, e.read(old, size * size_of))
        e.write(v + 8, struct.pack(">I", block))
        self.delete(old)
        e.write(v, struct.pack(">I", ncap))

    def vclear(self, kind):
        def fake(e):
            v = e.r(3)
            self.L.rec("clear", (kind, self.owner_label(v), e.u32(v + 4)))
            e.write(v + 4, bytes(4))
        return fake

    def push_chars(self, e):
        v, el = e.r(3), e.r(4)
        size = e.u32(v + 4)
        d = e.read(el, 16)
        if not self.quiet:
            self.L.rec("push_back", (0, self.owner_label(v), size, u16(d, 0), u16(d, 2), d[4], struct.unpack(">i", d[8:12])[0], u16(d, 12)))
        self.grow(v, 16)
        e.write(e.u32(v + 8) + 16 * size, d)
        e.write(v + 4, struct.pack(">I", size + 1))

    def push_lines(self, e):
        v, el = e.r(3), e.r(4)
        size = e.u32(v + 4)
        d = e.read(el, 12)
        begin, end, width = struct.unpack(">III", d)
        self.L.rec("push_back", (1, self.owner_label(v), size, self.char_index(v, begin), self.char_index(v, end), width))
        self.grow(v, 12)
        e.write(e.u32(v + 8) + 12 * size, d)
        e.write(v + 4, struct.pack(">I", size + 1))

    def resize_points(self, e):
        v, n = e.r(3), e.r(4)
        size = e.u32(v + 4)
        self.L.rec("resize", (2, self.owner_label(v), n, size))
        if n > size:
            if n > e.u32(v):
                block = self.bump(n * 8 + GAP)
                self.heap[0] += 1
                self.L.rec("alloc", (n, self.L.newlab(block)))
                old = e.u32(v + 8)
                if size:
                    e.write(block, e.read(old, size * 8))
                e.write(v + 8, struct.pack(">I", block))
                self.delete(old)
                e.write(v, struct.pack(">I", n))
            e.write(e.u32(v + 8) + 8 * size, bytes(8 * (n - size)))
        e.write(v + 4, struct.pack(">I", n))

    def process_line(self, e):
        first, last = e.r(3), e.r(4)
        mode = self.L.ret("ProcessLine", 0)
        owner, data = -1, 0
        for i, f in enumerate(self.F):
            if f:
                d, size = e.u32(f + 0x64), e.u32(f + 0x60)
                if d and first >= d and last <= d + size * 16:
                    owner, data = i, d
        self.L.rec("ProcessLine", (LAB_F + owner if owner >= 0 else 0, (first - data) // 16 if owner >= 0 else 0,
                                   (last - data) // 16 if owner >= 0 else 0, mode))
        n = (last - first) // 16
        records = [e.read(first + 16 * i, 16) for i in range(n)]
        if mode == 1:
            records.reverse()
        elif mode == 2 and n >= 2:
            records = records[1:] + records[:1]
        e.write(first, b"".join(records))

    def bounding(self, e):
        a = struct.unpack(">4i", e.read(e.r(4), 16))
        b = struct.unpack(">4i", e.read(e.r(5), 16))
        self.L.rec("Bounding", a + b)
        e.write(e.r(3), struct.pack(">4i", min(a[0], b[0]), min(a[1], b[1]), max(a[2], b[2]), max(a[3], b[3])))

    def area_ctor(self, e):
        this = e.r(3)
        self.L.rec("TbRenderArea::TbRenderArea", (self.L.newlab(this),))
        e.write(this, b"".join(struct.pack(">I", 0x7A000000 + i) for i in range(10)))

    def stubs(self):
        s = CL.Stubs()
        s.real.update(REAL)
        s.imp(BOUNDING, self.bounding)
        for address, fn in ((CHAR_CLEAR, self.vclear(0)), (CHAR_PUSH, self.push_chars), (LINE_CLEAR, self.vclear(1)),
                            (LINE_PUSH, self.push_lines), (PT_CLEAR, self.vclear(2)), (PT_RESIZE, self.resize_points),
                            (PROCESS_LINE, self.process_line), (AREA_CTOR, self.area_ctor),
                            (OPERATOR_DELETE, lambda e: self.delete(e.r(3)))):
            s.internal(address, fn)
        return s

    # ---- the operations
    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def returning(self, address, *args):
        """A call whose answer is a TbError through a hidden pointer."""
        buffer = self.e.alloc(4, struct.pack(">I", 0x77777777))
        self.call(address, buffer, *args)
        return s32(self.e.u32(buffer))

    def params_at(self, ref):
        if ref >= 200:
            return self.R[ref - 200] + 4
        if ref >= 100:
            return self.F[ref - 100] + 8
        return self.P[ref]

    def rect_buffer(self, rect):
        return self.e.alloc(16, struct.pack(">4i", *rect))

    def construct(self, kind, n, fill, size, slots, label, ctor, extra=()):
        e = self.e
        address = e.alloc(size + GUARD, bytes([fill & 0xFF]) * (size + GUARD))
        slots[n] = address
        self.label(label + n, address)
        for lab, off in extra:
            self.label(lab + n, address + off)
        self.call(ctor, address)
        return address

    def op_f_ctor(self, n, fill):
        self.construct("f", n, fill, SIZE_FORMATTER, self.F, LAB_F, F_CTOR, ((LAB_FP, 8),))
        return 0, [0] * 8

    def op_f_dtor(self, n, flag):
        f = self.F[n]
        self.call(F_DTOR, f, flag)
        self.F[n] = 0
        return 0, [0] * 8

    def op_set_v(self, n, v):
        return self.returning(SET_V, self.F[n], v), [0] * 8

    def op_set_h(self, n, v):
        return self.returning(SET_H, self.F[n], v), [0] * 8

    def op_get_arr(self, n):
        return self.L.lab(self.call(GET_ARR, self.F[n])), [0] * 8

    def op_get_arr_size(self, n):
        return self.call(GET_ARR_SIZE, self.F[n]), [0] * 8

    def op_idx(self, n, x, y):
        return s32(self.call(IDX, self.F[n], x, y)), [0] * 8

    def op_char_type(self, n, ch):
        return self.call(CHARTYPE, self.F[n], ch), [0] * 8

    def op_create(self, n, text, params):
        return self.returning(CREATE, self.F[n], self.texts[text], self.params_at(params)), [0] * 8

    def op_layout(self, n, l, t, r, b):
        return self.returning(LAYOUT, self.F[n], l, t, r, b), [0] * 8

    def op_lines(self, n, i, j):
        data = self.e.u32(self.F[n] + 0x64)
        return s32(self.call(LAYOUT_LINES, self.F[n], data + 16 * i, data + 16 * j)), [0] * 8

    def op_drawtext(self, n, params, want, draw):
        e = self.e
        bounds = self.rect_buffer((-1, -2, -3, -4))
        result = self.returning(DRAWTEXT, self.F[n], self.params_at(params), bounds if want else 0, 1 if draw else 0)
        return result, list(struct.unpack(">4i", e.read(bounds, 16))) + [0] * 4

    def op_empty(self, n, l, t, r, b):
        return self.returning(EMPTY, self.F[n], self.rect_buffer((l, t, r, b))), [0] * 8

    def op_draw(self, n, l, t, r, b, text, params, want, address=DRAW):
        e = self.e
        out = self.rect_buffer((-1, -2, -3, -4))
        result = self.returning(address, self.F[n], self.rect_buffer((l, t, r, b)), self.texts[text], self.params_at(params),
                                out if want else 0)
        return result, list(struct.unpack(">4i", e.read(out, 16))) + [0] * 4

    def op_measure(self, n, l, t, r, b, text, params, want):
        return self.op_draw(n, l, t, r, b, text, params, want, MEASURE)

    def op_r_ctor(self, n, fill):
        self.construct("r", n, fill, SIZE_RENDER, self.R, LAB_R, R_CTOR, ((LAB_RP, 4),))
        return 0, [0] * 8

    def op_r_dtor(self, n, flag):
        r = self.R[n]
        self.call(R_DTOR, r, flag)
        if flag > 0:
            self.R[n] = 0
        return 0, [0] * 8

    def op_r_init(self, n, params):
        return self.returning(R_INIT, self.R[n], self.params_at(params)), [0] * 8

    def op_r_drawchar(self, n, x, y, glyph):
        e = self.e
        out = self.rect_buffer((-1, -2, -3, -4))
        at = e.alloc(8, struct.pack(">ii", x, y))
        result = self.returning(R_DRAWCHAR, self.R[n], at, glyph, out)
        return result, list(struct.unpack(">4i", e.read(out, 16))) + [0] * 4

    def op_p_ctor(self, n, fill):
        self.construct("p", n, fill, SIZE_PARAMS, self.P, LAB_P, P_CTOR)
        return 0, [0] * 8

    def op_p_dtor(self, n, flag):
        self.call(P_DTOR, self.P[n], flag)
        if flag > 0:
            self.P[n] = 0
        return 0, [0] * 8

    def op_recolour(self, n, c0, c1, c2, c3, c4):
        colour = self.e.alloc(8, bytes([c0, c1, c2, c3, c4]) + bytes(3))
        return self.returning(P_RECOLOUR, self.P[n], colour), [0] * 8

    def op_precol(self, n):
        return self.returning(P_PRE, self.P[n]), [0] * 8

    def op_setfont(self, n, font):
        return self.returning(P_SETFONT, self.P[n], 0 if font < 0 else self.p.fonts[font]), [0] * 8

    def op_getfont(self, n):
        return self.L.lab(self.call(P_GETFONT, self.P[n])), [0] * 8

    def op_teardown(self, kind, cap, size, nbytes):
        e = self.e
        block = 0
        if nbytes:
            block = e.alloc(nbytes, bytes(nbytes))
            self.L.newlab(block)
        vector = e.alloc(12, struct.pack(">III", cap, size, block))
        self.call((TD_CHARS, TD_LINES, TD_POINTS)[kind], vector)
        return 0, [0] * 8

    # ---- the setup operations
    def op_chars(self, n, spec):
        e = self.e
        block = self.bump(max(len(spec), 1) * 16 + GAP)
        self.L.newlab(block)
        for i, (ch, index, kind, width, position) in enumerate(spec):
            e.write(block + 16 * i, struct.pack(">HHBBHiHH", ch & 0xFFFF, index & 0xFFFF, kind & 0xFF, 0, 0, width, position & 0xFFFF, 0))
        e.write(self.F[n] + 0x5C, struct.pack(">III", len(spec), len(spec), block))
        return 0, [0] * 8

    FORMATTER_FIELDS = {0: (0x88, "B"), 1: (0x94, "I"), 2: (0x98, "I"), 3: (0x9C, "I"), 4: (0xB0, "I"), 5: (0xA0, "I"), 6: (0xA4, "I"),
                        7: (0xA8, "I"), 8: (0xAC, "I"), 9: (0x68, "H"), 10: (0x78, "I")}

    def op_poke(self, what, index, field, value):
        e = self.e
        if what == 0:
            offset, fmt = self.FORMATTER_FIELDS[field]
            e.write(self.F[index] + offset, struct.pack(">" + fmt, value & {"B": 0xFF, "H": 0xFFFF, "I": MINUS}[fmt]))
        elif what == 1:
            params = self.params_at(index)
            if field == 0:
                e.write(params + 4, struct.pack(">I", 0 if value < 0 else self.p.fonts[value]))
            elif field == 16:
                e.write(params + 0x44, bytes((value >> (8 * i)) & 0xFF for i in range(5)))
            else:
                e.write(params + 4 + 4 * field, struct.pack(">I", value & MINUS))
        elif what == 2:
            vector, word = divmod(field, 3)
            e.write(self.F[index] + (0x5C, 0x6C, 0x7C)[vector] + 4 * word, struct.pack(">I", value & MINUS))
        elif what == 3:
            e.write(self.R[index] + 0x54, struct.pack(">I", 0 if value == 0 else self.p.methods[value - 1]))
        return 0, [0] * 8

    def op_mode(self, quiet, method_mode):
        self.quiet = bool(quiet)
        self.mmode = method_mode
        return 0, [0] * 8

    # ---- the dumps (the host's order)
    def dump_params(self, address):
        e, L = self.e, self.L
        w = lambda o: e.u32(address + o)   # noqa: E731
        out = [L.lab(w(4)), w(8), w(0xC), w(0x10)]
        out += [w(0x14 + 4 * i) for i in range(10)]
        out += [w(0x3C), w(0x40)]
        out += list(e.read(address + 0x44, 5))
        out += [w(0x4C)]
        return out

    def dump_formatter(self, f):
        e, L = self.e, self.L
        w = lambda o: e.u32(f + o)   # noqa: E731
        s = lambda o: s32(e.u32(f + o))   # noqa: E731
        out = [L.lab(w(0x58))]
        out += self.dump_params(f + 8)
        cap, size, data = w(0x5C), w(0x60), w(0x64)
        out += [cap, size, L.lab(data)]
        for ch, index, kind, _, _, width, position, _ in struct.iter_unpack(">HHBBHiHH", e.read(data, 16 * size)) if size and L.lab(data) > 0 else []:
            out += [ch, index, kind, width, position]
        out += [struct.unpack(">H", e.read(f + 0x68, 2))[0]]
        cap, size, ldata = w(0x6C), w(0x70), w(0x74)
        out += [cap, size, L.lab(ldata)]
        for begin, end, width in struct.iter_unpack(">III", e.read(ldata, 12 * size)) if size and L.lab(ldata) > 0 else []:
            inside = lambda a: (a - data) // 16 if data and data <= a <= data + 16 * w(0x60) else -1   # noqa: E731
            out += [inside(begin), inside(end), width]
        out += [w(0x78)]
        cap, size, pdata = w(0x7C), w(0x80), w(0x84)
        out += [cap, size, L.lab(pdata)]
        for x, y in struct.iter_unpack(">ii", e.read(pdata, 8 * size)) if size and L.lab(pdata) > 0 else []:
            out += [x, y]
        out += [e.read(f + 0x88, 1)[0], s(0x8C), s(0x90), s(0x94), s(0x98), w(0x9C), s(0xA0), s(0xA4), s(0xA8), s(0xAC), s(0xB0)]
        return out

    def dump_render(self, r):
        return [self.L.lab(self.e.u32(r + 0x54))] + self.dump_params(r + 4)

    def dump(self, final):
        out = {}
        for n, f in enumerate(self.F):
            if f:
                out[f"F{n}"] = self.dump_formatter(f)
        for n, p in enumerate(self.P):
            if p:
                out[f"P{n}"] = self.dump_params(p)
        for n, r in enumerate(self.R):
            if r:
                out[f"R{n}"] = self.dump_render(r)
        return out


# ======================================================================= the host side


def hostfn(lib, name, restype, *args):
    f = getattr(lib, name)
    f.restype, f.argtypes = restype, args
    return f


class Host:
    """One case on the host side: the same world and operations through tpw_tf_op."""

    BUFFER = (C.c_int64 * 600000)()

    def __init__(self, lib, case):
        self.lib = lib
        hostfn(lib, "tpw_tf_reset", None)()
        hostfn(lib, "tpw_tf_font", C.c_void_p, C.c_int32)
        hostfn(lib, "tpw_tf_method", C.c_void_p, C.c_int32)
        hostfn(lib, "tpw_tf_configure_font", None, C.c_int32, C.c_int32, C.c_uint32, C.c_uint32, C.c_int32, C.c_uint32, C.c_uint32,
               C.c_uint32)
        hostfn(lib, "tpw_tf_configure_widths", None, C.c_int32, C.c_void_p, C.c_int32)
        hostfn(lib, "tpw_tf_configure_converter", None, C.c_int32, C.c_int32, C.c_int32, C.c_void_p, C.c_int32)
        hostfn(lib, "tpw_tf_configure_table", None, C.c_void_p)
        hostfn(lib, "tpw_tf_set_text", None, C.c_int32, C.c_char_p, C.c_int32)
        hostfn(lib, "tpw_tf_set_mode", None, C.c_int32, C.c_int32)
        hostfn(lib, "tpw_tf_set_chars", None, C.c_int32, C.c_void_p, C.c_int32)
        hostfn(lib, "tpw_tf_poke", None, C.c_int32, C.c_int32, C.c_int32, C.c_int64)
        hostfn(lib, "tpw_tf_dump", C.c_int32, C.c_int32, C.c_int32, C.c_void_p, C.c_int32)
        hostfn(lib, "tpw_tf_op", C.c_int64, C.c_int32, C.c_void_p, C.c_void_p)
        for i, f in enumerate(case["fonts"]):
            lib.tpw_tf_configure_font(i, f["spacing"], f["caps"], f["type"], f["conv"], f["wbase"], f["wmod"], f["wmul"])
        for i, f in enumerate(case["fonts"]):
            if f.get("widths"):
                widths = (C.c_uint32 * len(f["widths"]))(*f["widths"])
                lib.tpw_tf_configure_widths(i, C.addressof(widths), len(f["widths"]))
            else:
                lib.tpw_tf_configure_widths(i, None, 0)
        for i, c in enumerate(case["convs"]):
            table = (C.c_uint16 * len(c["table"]))(*c["table"])
            lib.tpw_tf_configure_converter(i, c["lead"], c["count"], C.addressof(table), len(c["table"]))
        cells = (C.c_int32 * 30)(*case["table"])
        lib.tpw_tf_configure_table(C.addressof(cells))
        for i, t in enumerate(case["texts"]):
            data = t + b"\0Q" + bytes(14)
            lib.tpw_tf_set_text(i, data, len(data))
        fixed = {LAB_FONT + i: lib.tpw_tf_font(i) for i in range(NFONT)}
        fixed.update({LAB_M + i: lib.tpw_tf_method(i) for i in range(NMETH)})
        CL.host_begin(lib, fixed, {k: v[:MAX_QUEUE] for k, v in case.get("queues", {}).items()}, FIRST_NEW)
        self.alive = {"F": [False] * NFORM, "P": [False] * NPARAM, "R": [False] * NREND}
        self.n = C.c_int32.in_dll(lib, "tpw_cl_n")
        self.heap = (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")

    def log_slice(self):
        n = self.n.value
        log = (CL.TpwCallEvent * 4096).in_dll(self.lib, "tpw_cl_log")
        out = []
        for i in range(min(n, 4096)):
            ev = log[i]
            out.append((ev.name.decode("latin-1"), tuple(ev.ints[k] for k in range(ev.nints)),
                        tuple(ev.texts[k].value.decode("latin-1") for k in range(ev.ntexts))))
        if n > 4096:
            out.append(("OVERFLOW", (n,), ()))
        return out

    def run(self, number, *args):
        a = (C.c_int64 * 8)(*([x for x in args] + [0] * (8 - len(args))))
        out = (C.c_int64 * 8)()
        result = self.lib.tpw_tf_op(number, a, out)
        return result, list(out)

    def dump_one(self, what, index):
        n = self.lib.tpw_tf_dump(what, index, C.addressof(self.BUFFER), len(self.BUFFER))
        if n > len(self.BUFFER):
            raise MemoryError("dump too long")
        return list(self.BUFFER[:n])

    def dump(self):
        out = {}
        for what, key in ((0, "F"), (1, "P"), (2, "R")):
            for n, alive in enumerate(self.alive[key]):
                if alive:
                    out[f"{key}{n}"] = self.dump_one(what, n)
        return out


# ======================================================================= running a script on both sides

def apply_presence(alive, op):
    """Tracks which objects exist (for the dumps): a constructor makes one; a deleting destructor, or a formatter's destructor of
    any kind (its blocks are gone), ends it."""
    name = op[0]
    if name == "f_ctor":
        alive["F"][op[1]] = True
    elif name == "f_dtor":
        alive["F"][op[1]] = False
    elif name == "p_ctor":
        alive["P"][op[1]] = True
    elif name == "p_dtor" and op[2] > 0:
        alive["P"][op[1]] = False
    elif name == "r_ctor":
        alive["R"][op[1]] = True
    elif name == "r_dtor" and op[2] > 0:
        alive["R"][op[1]] = False


def run_ppc(e, case):
    c = Ppc(e, case)
    saved = c.stubs().activate(e)
    trace = []
    try:
        for i, op in enumerate(case["script"]):
            name, args = op[0], op[1:]
            c.L.log = []
            c.heap = [0, 0, 0, 0]
            result, out = getattr(c, "op_" + name)(*args)
            step = dict(result=result, out=out, log=c.L.log, heap=list(c.heap))
            c.L.log = []
            if name not in STATELESS or i == len(case["script"]) - 1:
                step["dump"] = c.dump(False)
            trace.append(step)
    finally:
        CL.Stubs.deactivate(e, saved)
        c.finish()
    return trace


def run_host(lib, case):
    h = Host(lib, case)
    trace = []
    for i, op in enumerate(case["script"]):
        name, args = op[0], op[1:]
        h.n.value = 0
        for k in range(4):
            h.heap[k] = 0
        if name in OPS:
            result, out = h.run(OPS.index(name), *args)
        elif name == "chars":
            spec = (C.c_int32 * (5 * len(args[1])))(*[v for item in args[1] for v in item])
            h.lib.tpw_tf_set_chars(args[0], C.addressof(spec), len(args[1]))
            result, out = 0, [0] * 8
        elif name == "poke":
            h.lib.tpw_tf_poke(*args)
            result, out = 0, [0] * 8
        else:
            h.lib.tpw_tf_set_mode(*args)
            result, out = 0, [0] * 8
        step = dict(result=result, out=out, log=h.log_slice(), heap=list(h.heap))
        apply_presence(h.alive, op)
        if name not in STATELESS or i == len(case["script"]) - 1:
            step["dump"] = h.dump()
        trace.append(step)
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    return trace


def ppc_wrap(e, case):
    return dict(trace=run_ppc(e, case))


def host_wrap(lib, case):
    return dict(trace=run_host(lib, case))


class Group(FnTest):
    """One function's cases: its edge scripts and its random ones."""

    def __init__(self, name, edge, rnd):
        super().__init__(name, ppc_wrap, host_wrap, edge=edge, rnd=rnd)


# ======================================================================= the worlds

def natural_table():
    """Converter 0: byte b is index b - 1; the table maps it to ASCII, to Hebrew from 0xe0 (0x5d0 on), and the control bytes 1 to 8 to
    the edges of the Hebrew range (0x5af, 0x5b0, 0x5b1, 0x5ea, 0x5eb, 0x5ec, none, 0xffff)."""
    edges = [0x5AF, 0x5B0, 0x5B1, 0x5EA, 0x5EB, 0x5EC, 0, 0xFFFF]
    table = []
    for i in range(256):
        b = i + 1
        table.append(edges[b - 1] if b <= 8 else b if b < 0xE0 else 0x5D0 + (b - 0xE0))
    return table


INTERESTING = [0, 9, 0xA, 0xD, 0x20, 0x41, 0x61, 0x5AF, 0x5B0, 0x5B1, 0x5D0, 0x5EA, 0x5EB, 0xFFFF, 0x30, 0x2E, 0x3A]


def random_table(r, count):
    return [r.choice(INTERESTING) if r.random() < 0.6 else r.randrange(65536) for _ in range(count)]


def font_cfg(spacing=12, caps=3, type=4, conv=0, wbase=5, wmod=7, wmul=1):
    return dict(spacing=spacing, caps=caps, type=type, conv=conv, wbase=wbase, wmod=wmod, wmul=wmul)


TABLE_SPARSE = [0] * 30
for _cell, _m in ((13, 1), (14, 2), (28, 3), (29, 4)):
    TABLE_SPARSE[_cell] = _m
TABLE_FULL = list(range(1, 31))
TABLE_NONE = [0] * 30


def edge_world(**over):
    """The same world for the edge scripts: font 0 reads converter 0 (ASCII), font 1 a double-byte one, font 2 has no converter."""
    world = dict(
        fonts=[font_cfg(12, 3, 4, 0, 5, 7, 1), font_cfg(20, 1, 4, 1, 3, 11, 3), font_cfg(0, 2, 2, -1, 0, 1, 1)],
        convs=[dict(lead=0xFF, count=256, table=natural_table()),
               dict(lead=0x80, count=3000, table=[0x41 + (i * 7) % 26 if i % 5 else 0x20 for i in range(3000)]),
               dict(lead=0x40, count=40, table=[0x61 + i % 26 for i in range(40)])],
        table=TABLE_FULL, texts=[b"", b"A", b"Hello world", b"One two\nthree four\n\nfive", b"a\r\nb\n\rc\r\rd", b"\x01\x02\x03\x04\x05\x06\x07\x08",
                                  b"abc \xe0\xe1\xe2 def", b"x" * 60], queues={}, script=[])
    world.update(over)
    return world


def random_world(r, ntexts=4):
    convs = [dict(lead=0xFF, count=256, table=natural_table()),
             dict(lead=r.choice([0x01, 0x20, 0x41, 0x80, 0xC0, 0xFE]), count=r.choice([0, 7, 300, 2500, 4096]), table=[]),
             dict(lead=r.choice([0x00, 0x40, 0x80, 0xFF]), count=r.choice([0, 40, 200, 1000]), table=[])]
    for c in convs[1:]:
        c["table"] = random_table(r, min(max(c["count"], 1), 4096))
    fonts = []
    for i in range(NFONT):
        fonts.append(font_cfg(r.choice([0, 1, 8, 13, 16, 24, 255]), r.randrange(16), r.choice([0, 1, 2, 3, 4, 4, 4, 5, 7]),
                              r.choice([0, 0, 1, 2, -1]), r.choice([0, 3, 5, 10]), r.choice([1, 2, 5, 13, 40]), r.choice([1, 3, 7])))
    fonts[0]["conv"] = 0
    table = r.choice([TABLE_SPARSE, TABLE_FULL, TABLE_FULL, [r.choice([0, 0, r.randrange(1, 31)]) for _ in range(30)]])
    queues = {"ProcessLine": [r.choice([0, 0, 1, 2]) for _ in range(r.randrange(1, 8))],
              "MethodDrawChar": [r.choice([0, 0, 0, -1, 7]) for _ in range(r.randrange(1, 5))]}
    return dict(fonts=fonts, convs=convs, table=table, texts=[random_text(r, convs, fonts) for _ in range(ntexts)], queues=queues, script=[])


def random_text(r, convs, fonts, maxlen=14):
    """A string of bytes the font-0 converter reads one by one, with sometimes high bytes, line breaks and Hebrew."""
    n = r.choice([0, 1, 2, 3, 5, 8, maxlen])
    alphabet = b"abcdefgh  \t\n\r12.:" + bytes(range(0xE0, 0xFB)) + bytes([0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0xFF])
    return bytes(r.choice(alphabet) if r.random() < 0.9 else r.randrange(1, 256) for _ in range(n))


# ======================================================================= the builder of scripts


class Builder:
    """Appends operations to a script and keeps just enough state to keep them valid: which objects exist, whether a formatter's lines
    describe its characters (DrawText follows them), how many characters it has."""

    def __init__(self, r, world):
        self.r = r
        self.world = world
        self.ops = world["script"]
        self.f = [False] * NFORM
        self.p = [False] * NPARAM
        self.rn = [False] * NREND
        self.lines_ok = [False] * NFORM
        self.nchars = [0] * NFORM

    def add(self, *op):
        self.ops.append(op)
        return self

    # objects
    def p_ctor(self, n=0, fill=0xA5):
        self.p[n] = True
        return self.add("p_ctor", n, fill)

    def f_ctor(self, n=0, fill=0xA5):
        self.f[n] = True
        self.lines_ok[n] = True
        self.nchars[n] = 0
        return self.add("f_ctor", n, fill)

    def r_ctor(self, n=0, fill=0xA5):
        self.rn[n] = True
        return self.add("r_ctor", n, fill)

    def style(self, p, style, blend=1):
        """Parameters with the font's style and blend words set directly."""
        self.add("poke", 1, p, 2, style)
        return self.add("poke", 1, p, 3, blend)

    def params(self, p=0, font=0, style=1, blend=1, fill=0xA5):
        self.p_ctor(p, fill)
        self.add("setfont", p, font)
        return self.style(p, style, blend)

    def formatter(self, n=0, v=4, h=0, fill=0xA5):
        self.f_ctor(n, fill)
        self.add("set_v", n, v)
        return self.add("set_h", n, h)

    def chars(self, n, spec):
        self.add("chars", n, spec)
        self.lines_ok[n] = False
        self.nchars[n] = len(spec)
        return self

    def create(self, n, text, p):
        self.lines_ok[n] = False
        return self.add("create", n, text, p)

    def layout(self, n, rect):
        self.lines_ok[n] = True
        return self.add("layout", n, *rect)


def char(ch, kind=1, width=6, position=None, index=None):
    """A made character: (unicode, font index, type bits, width, number)."""
    return (ch, ch if index is None else index, kind, width, 0 if position is None else position)


def numbered(spec):
    """The same characters numbered 0, 1, 2 ... as CreateInfoList numbers them."""
    return [(c, i, k, w, n) for n, (c, i, k, w, _) in enumerate(spec)]


def spec_of(text, kinds=None, widths=None):
    """Characters for a string: the types GetCharType gives, widths from `widths` (a function of the position) or 6."""
    out = []
    for n, ch in enumerate(text):
        kind = {0x0A: 4, 0x0D: 0xC, 0x09: 0x12, 0x20: 0x12}.get(ord(ch), 1) if kinds is None else kinds[n]
        out.append((ord(ch), ord(ch), kind, 6 if widths is None else widths(n), n))
    return out


RECTS = [(0, 0, 100, 40), (10, 20, 110, 80), (-50, -30, 51, 33), (5, 5, 5, 5), (0, 0, 0, 0), (100, 50, 0, 0), (-7, -9, -8, -10), (3, 4, 1000, 999)]
JUSTS = [-1, 0, 1, 2, 3, 4, 5, 6, 0x7FFFFFFF, -0x80000000]


# ======================================================================= the groups: one function each

def scripted(world_fn, build):
    """A case: a world and the script `build(builder)` adds to it."""
    def make(r, label=""):
        world = world_fn(r)
        world["label"] = label
        build(Builder(r, world))
        return world
    return make


def edge_cases(builders):
    return lambda: [scripted(lambda r: edge_world(), build)(random.Random(i), label) for i, (label, build) in enumerate(builders)]


def random_of(build):
    return lambda r: scripted(random_world, build)(r)


# ---- TbTextFormatter::TbTextFormatter

def g_ctor_edges():
    out = []
    for fill in (0x00, 0xA5, 0xFF, 0x5A, 0x01):
        out.append(("fill %#x" % fill, lambda b, fill=fill: b.p_ctor(0, fill).f_ctor(0, fill)))
    out.append(("twice", lambda b: (b.params().formatter().create(0, 3, 0), b.layout(0, RECTS[1]), b.f_ctor(0, 0xA5))))
    out.append(("second formatter", lambda b: (b.f_ctor(0), b.f_ctor(1, 0x33))))
    return out


def g_ctor_random(b):
    r = b.r
    b.params(0, r.randrange(NFONT), r.choice([0, 1, 2]))
    for _ in range(r.randrange(1, 4)):
        n = r.randrange(NFORM)
        if b.f[n] and r.random() < 0.7:
            b.formatter(n, r.choice([2, 4, 5]), r.choice([0, 1, 2]))
            b.create(n, r.randrange(4), 0)
            b.layout(n, r.choice(RECTS))
        b.f_ctor(n, r.choice([0, 0xA5, 0xFF, r.randrange(256)]))


# ---- ~TbTextFormatter

FLAGS = [-1, 0, 1, 2, 0x7FFF, -0x8000]


def g_dtor_edges():
    out = []
    for flag in FLAGS:
        out.append(("fresh, flag %d" % flag, lambda b, flag=flag: b.f_ctor(0).add("f_dtor", 0, flag)))
        out.append(("used, flag %d" % flag, lambda b, flag=flag: (b.params(0, 0, 1).formatter(0, 4, 0).create(0, 3, 0), b.layout(0, RECTS[1]),
                                                                  b.add("drawtext", 0, 0, 1, 1), b.add("f_dtor", 0, flag))))
    out.append(("blocks of one vector", lambda b: (b.params(0, 0, 1).formatter(0), b.create(0, 1, 0), b.add("f_dtor", 0, 1))))
    out.append(("after the render took the font", lambda b: (b.params(0, 1, 1).formatter(0, 5, 2), b.add("draw", 0, 0, 0, 90, 40, 3, 0, 1),
                                                             b.add("f_dtor", 0, 1))))
    return out


def g_dtor_random(b):
    r = b.r
    b.params(0, r.randrange(NFONT), r.choice([0, 1, 2]), r.randrange(2))
    b.formatter(0, r.choice([2, 4, 5]), r.choice([0, 1, 2]))
    for _ in range(r.randrange(0, 4)):
        what = r.randrange(5)
        if what == 0:
            b.create(0, r.randrange(4), 0)
        elif what == 1:
            b.layout(0, r.choice(RECTS))
        elif what == 2 and b.lines_ok[0]:
            b.add("drawtext", 0, 0, r.randrange(2), r.randrange(2))
        elif what == 3:
            b.add("draw", 0, *r.choice(RECTS), r.randrange(4), 0, r.randrange(2))
        else:
            b.add("measure", 0, *r.choice(RECTS), r.randrange(4), 0, r.randrange(2))
    b.add("f_dtor", 0, r.choice(FLAGS))
    if r.random() < 0.3:
        b.f_ctor(0).add("f_dtor", 0, r.choice(FLAGS))


# ---- the justification setters

def g_set_edges(op):
    out = []
    for v in JUSTS + [7, 8, -2]:
        out.append(("%s %d" % (op, v), lambda b, v=v: b.f_ctor(0).add(op, 0, v)))
    out.append(("valid then invalid", lambda b: b.f_ctor(0).add(op, 0, 2).add(op, 0, 3).add(op, 0, 5).add(op, 0, 9).add(op, 0, 1)))
    out.append(("over zero fill", lambda b: b.f_ctor(0, 0).add(op, 0, 4)))
    return out


def g_set_random(op):
    def build(b):
        b.f_ctor(0, b.r.choice([0, 0xA5, 0xFF]))
        for _ in range(b.r.randrange(2, 8)):
            b.add(op, 0, b.r.choice(JUSTS + [b.r.randrange(-3, 9)] * 4))
    return build


# ---- the accessors

def poke_vectors(b):
    r = b.r
    for vector in range(3):
        if r.random() < 0.5:
            b.add("poke", 2, 0, vector * 3, r.randrange(0, 100))
            b.add("poke", 2, 0, vector * 3 + 1, r.randrange(0, 100))


def g_arr_edges(op):
    return [("fresh", lambda b: b.f_ctor(0).add(op, 0)),
            ("after a draw", lambda b: (b.params(0, 0, 1).formatter(0), b.add("draw", 0, *RECTS[1], 3, 0, 1), b.add(op, 0))),
            ("after an empty string", lambda b: (b.params(0, 0, 1).formatter(0), b.add("draw", 0, *RECTS[1], 0, 0, 1), b.add(op, 0))),
            ("poked", lambda b: (b.f_ctor(0), b.add("poke", 2, 0, 6, 17), b.add("poke", 2, 0, 7, 17), b.add("poke", 2, 0, 8, 0x1234), b.add(op, 0))),
            ("poked to nothing", lambda b: (b.f_ctor(0), b.add("poke", 2, 0, 8, 0), b.add("poke", 2, 0, 7, 5), b.add(op, 0)))]


def g_arr_random(op):
    def build(b):
        r = b.r
        b.params(0, r.randrange(NFONT), r.choice([0, 1, 2]))
        b.formatter(0, r.choice([2, 4, 5]), r.choice([0, 1, 2]))
        if r.random() < 0.3:
            poke_vectors(b)   # on a formatter with no blocks yet: the pokes are not followed
        else:
            for _ in range(r.randrange(0, 3)):
                b.add("draw", 0, *r.choice(RECTS), r.randrange(4), 0, 1)
                b.add(op, 0)
        b.add(op, 0)
    return build


# ---- GetCharIdxFromPosition and GetCharType

POINTS = [(0, 0), (1, 1), (-1, -1), (0x7FFFFFFF, -0x80000000), (100, 200), (-0x80000000, 0x7FFFFFFF)]


def g_idx_edges():
    return [("point %s" % (p,), lambda b, p=p: b.f_ctor(0).add("idx", 0, *p)) for p in POINTS]


def g_idx_random(b):
    b.f_ctor(0)
    for _ in range(5):
        b.add("idx", 0, b.r.randrange(-1 << 31, 1 << 31), b.r.randrange(-1 << 31, 1 << 31))


CHAR_EDGES = list(range(0, 0x30)) + [0x7F, 0x80, 0xFF, 0x100, 0x109, 0x10A, 0x10D, 0x120, 0x5B0, 0x5EB, 0x7FFF, 0x8000, 0xFFF7, 0xFFFF]


def g_chartype_edges():
    out = []
    for i in range(0, len(CHAR_EDGES), 12):
        chunk = CHAR_EDGES[i:i + 12]
        out.append(("characters %s" % chunk[:3], lambda b, chunk=chunk: (b.f_ctor(0, 0xA5), [b.add("char_type", 0, c) for c in chunk])))
    return out


def g_chartype_random(b):
    b.f_ctor(0)
    for _ in range(40):
        c = b.r.choice([b.r.randrange(0x30), b.r.randrange(65536), b.r.choice([9, 10, 13, 32, 0x109, 0x10A, 0x10D, 0x120])])
        b.add("char_type", 0, c)


# ---- CreateInfoList

DOUBLE = [b"\x81\x01\x90\xff\xc0\x10", b"\x41\x90", b"\xff\xff\xfe\xfe", b"\x01\x80\x80\x7f", b"ab\xc0", b"\x81"]


def g_create_edges():
    out = []
    for text in range(8):
        out.append(("text %d" % text, lambda b, text=text: (b.params(0, 0, 1).formatter(0), b.create(0, text, 0))))
    out.append(("no font", lambda b: (b.p_ctor(0), b.formatter(0), b.create(0, 2, 0))))
    out.append(("a font without a converter", lambda b: (b.params(0, 2, 1).formatter(0), b.create(0, 2, 0))))
    out.append(("the same text twice", lambda b: (b.params(0, 0, 1).formatter(0), b.create(0, 3, 0), b.create(0, 3, 0))))
    out.append(("a shorter text after a longer", lambda b: (b.params(0, 0, 1).formatter(0), b.create(0, 7, 0), b.create(0, 1, 0), b.create(0, 0, 0))))
    out.append(("another font after the first", lambda b: (b.params(0, 0, 1).params(1, 1, 1).formatter(0), b.create(0, 2, 0), b.create(0, 2, 1))))

    def double(b, k):
        b.world["texts"] = b.world["texts"] + [DOUBLE[k]]
        b.params(0, 1, 1).formatter(0)
        b.create(0, len(b.world["texts"]) - 1, 0)

    for k in range(len(DOUBLE)):
        out.append(("double byte text %d" % k, lambda b, k=k: double(b, k)))

    def spacing(b, value):
        b.world["fonts"][0]["spacing"] = value
        b.params(0, 0, 1).formatter(0)
        b.create(0, 2, 0)

    for value in (0, 1, 255):
        out.append(("spacing %d" % value, lambda b, value=value: spacing(b, value)))

    def widths(b, wbase, wmod, wmul):
        b.world["fonts"][0].update(wbase=wbase, wmod=wmod, wmul=wmul)
        b.params(0, 0, 1).formatter(0)
        b.create(0, 2, 0)

    out.append(("zero widths", lambda b: widths(b, 0, 1, 1)))
    out.append(("wide characters", lambda b: widths(b, 65000, 500, 3)))

    def long_text(b, n, quiet):
        b.world["texts"] = b.world["texts"] + [(b"abc defghij\nklm " * (n // 16 + 1))[:n]]
        b.params(0, 0, 1).formatter(0)
        if quiet:
            b.add("mode", 1, 0)
        b.create(0, len(b.world["texts"]) - 1, 0)
        b.add("mode", 0, 0)

    out.append(("300 characters", lambda b: long_text(b, 300, False)))
    out.append(("1,100 characters", lambda b: long_text(b, 1100, True)))
    out.append(("65,537 characters (the number wraps)", lambda b: long_text(b, 65537, True)))
    out.append(("a counter near the wrap", lambda b: (b.params(0, 0, 1).formatter(0), b.add("poke", 0, 0, 9, 0xFFFE), b.create(0, 2, 0))))
    return out


def g_create_random(b):
    r = b.r
    for p in range(2):
        b.params(p, r.randrange(NFONT), r.choice([0, 1, 2]), r.randrange(2))
        if r.random() < 0.15:
            b.add("poke", 1, p, 0, -1)
    b.formatter(0, r.choice([2, 4, 5]), r.choice([0, 1, 2]), r.choice([0xA5, 0, 0xFF]))
    for _ in range(r.randrange(1, 4)):
        b.create(0, r.randrange(len(b.world["texts"])), r.randrange(2))


# ---- Layout and LayoutLines

SPECS = {
    "empty": [],
    "one": [char(0x41)],
    "abc": spec_of("abc"),
    "ab-nl-cd": spec_of("ab\ncd"),
    "ab-nl-nl-cd": spec_of("ab\n\ncd"),
    "a-nl-cr-b": spec_of("a\n\rb"),
    "a-cr-nl-b": spec_of("a\r\nb"),
    "a-cr-cr-b": spec_of("a\r\rb"),
    "nl": spec_of("\n"),
    "nl-nl-nl": spec_of("\n\n\n"),
    "ab-nl": spec_of("ab\n"),
    "nl-ab": spec_of("\nab"),
    "cr-cr-nl-x": spec_of("\r\r\nx"),
}
WORDS = {
    "words": spec_of("aaaa bbbb cccc dd eeeeee"),
    "long word": spec_of("xxxxxxxxxxxxxxxxxxxx"),
    "leading spaces": spec_of("   ab cd"),
    "double spaces": spec_of("ab  cd  ef gh"),
    "trailing spaces": spec_of("ab cd   "),
    "only spaces": spec_of("      "),
    "tabs": spec_of("a\tb\tc\td"),
    "breaks inside": spec_of("ab cd\nef gh ij kl\r\nmn"),
    "zero widths": spec_of("a b c d e f", widths=lambda n: 0),
    "mixed widths": spec_of("ab cd ef gh ij", widths=lambda n: (n * 7) % 11),
    "a wide first": [char(0x41, 1, 50)] + spec_of("bc de"),
    "a negative width": spec_of("ab cd ef", widths=lambda n: -3 if n == 4 else 6),
    "word break without delimiter": spec_of("abcdefghij", kinds=[1, 1, 0x10, 1, 1, 0x11, 1, 0x10, 1, 1]),
    "delimiter without word break": spec_of("ab cd ef gh", kinds=[1, 1, 2, 1, 1, 2, 1, 1, 2, 1, 1]),
    "already hidden": spec_of("ab cd ef gh", kinds=[1, 1, 0x52, 1, 1, 0x12, 1, 1, 0x42, 1, 1]),
    "one space": spec_of(" "),
    "space then word": spec_of(" abcdefgh"),
}
WRAPS = [0, 1, 5, 6, 11, 12, 13, 17, 24, 30, 100, 0xFFFFFFFF, 0x80000000]


def numbered_spec(spec):
    return numbered(spec)


def g_lines_edges():
    out = []
    for name, spec in list(SPECS.items()) + list(WORDS.items()):
        for wrap in WRAPS:
            if name in SPECS and wrap not in (0, 6, 13, 0xFFFFFFFF):
                continue
            out.append(("%s wrap %d" % (name, wrap), lambda b, spec=spec, wrap=wrap: (b.formatter(0), b.chars(0, numbered(spec)), b.add("poke", 0, 0, 3, wrap),
                                                                                       b.add("lines", 0, 0, len(spec)))))
    return out


def g_lines_random(b):
    r = b.r
    b.formatter(0, r.choice([2, 4, 5]), r.choice([0, 1, 2]))
    n = r.choice([0, 1, 2, 5, 9, 14])
    kinds = [1, 1, 1, 0x12, 0x12, 0x10, 0x11, 2, 4, 0xC, 0x40, 0x52, 0x13]
    spec = [(0x41 + i, 0x41 + i, r.choice(kinds), r.choice([0, 3, 6, 6, 7, 12, 40, -2]), i) for i in range(n)]
    b.chars(0, spec)
    b.add("poke", 0, 0, 3, r.choice(WRAPS + [r.randrange(0, 60)] * 6))
    for _ in range(r.randrange(1, 4)):
        i = r.randrange(n + 1)
        j = r.randrange(i, n + 1)
        b.add("lines", 0, i, j)


LAYOUT_SPECS = list(SPECS.items())


def g_layout_edges():
    out = []
    for name, spec in LAYOUT_SPECS:
        out.append(("%s default" % name, lambda b, spec=spec: (b.formatter(0), b.chars(0, numbered(spec)), b.add("poke", 0, 0, 3, 100), b.add("poke", 0, 0, 4, 10),
                                                                b.layout(0, RECTS[1]))))
    spec = numbered(spec_of("ab\ncd ef gh\nij"))
    for v in (-1, 0, 1, 2, 3, 4, 5, 6):
        for h in (-1, 0, 1, 2, 3):
            out.append(("justification %d %d" % (v, h), lambda b, v=v, h=h: (b.f_ctor(0), b.add("poke", 0, 0, 1, v), b.add("poke", 0, 0, 2, h),
                                                                             b.chars(0, spec), b.add("poke", 0, 0, 3, 100), b.add("poke", 0, 0, 4, 7),
                                                                             b.layout(0, RECTS[2]))))
    for k, rect in enumerate(RECTS):
        out.append(("rect %s" % (rect,), lambda b, rect=rect: (b.formatter(0, 2, 2), b.chars(0, spec), b.add("poke", 0, 0, 3, 100), b.add("poke", 0, 0, 4, 9),
                                                               b.layout(0, rect))))
    out.append(("layout twice", lambda b: (b.formatter(0), b.chars(0, spec), b.add("poke", 0, 0, 3, 9), b.add("poke", 0, 0, 4, 9), b.layout(0, RECTS[1]),
                                           b.layout(0, RECTS[0]))))
    out.append(("empty after a layout", lambda b: (b.formatter(0), b.chars(0, spec), b.add("poke", 0, 0, 3, 9), b.add("poke", 0, 0, 4, 9), b.layout(0, RECTS[1]),
                                                   b.chars(0, []), b.layout(0, RECTS[0]))))
    out.append(("a pile of line breaks", lambda b: (b.formatter(0), b.chars(0, numbered(spec_of("\n" * 40))), b.add("poke", 0, 0, 3, 9),
                                                    b.add("poke", 0, 0, 4, 3), b.layout(0, RECTS[1]))))
    return out


def g_layout_random(b):
    r = b.r
    b.f_ctor(0, r.choice([0xA5, 0, 0xFF]))
    b.add("poke", 0, 0, 1, r.choice([2, 4, 5, r.randrange(-2, 8)]))
    b.add("poke", 0, 0, 2, r.choice([0, 1, 2, r.randrange(-2, 5)]))
    n = r.choice([0, 1, 3, 6, 12, 25])
    kinds = [1, 1, 1, 1, 0x12, 0x12, 4, 0xC, 0x10, 2]
    spec = [(0x41 + i, 0x41 + i, r.choice(kinds), r.choice([0, 3, 6, 7, 12]), i) for i in range(n)]
    b.chars(0, spec)
    b.add("poke", 0, 0, 3, r.choice([0, 6, 13, 30, 100, 0xFFFFFFFF, r.randrange(0, 80)]))
    b.add("poke", 0, 0, 4, r.choice([0, 1, 7, 10, 13, 255]))
    for _ in range(r.randrange(1, 3)):
        b.layout(0, r.choice(RECTS + [(r.randrange(-100, 100), r.randrange(-100, 100), r.randrange(-100, 200), r.randrange(-100, 200))]))


# ---- DrawText

HEBREW = "אבג"


def hebrew_spec(text):
    """Characters of a unicode string (Hebrew and all) with the types GetCharType gives."""
    out = []
    for n, ch in enumerate(text):
        kind = {0x0A: 4, 0x0D: 0xC, 0x09: 0x12, 0x20: 0x12}.get(ord(ch), 1)
        out.append((ord(ch), ord(ch) & 0xFFFF, kind, 6 + n % 3, n))
    return out


DRAW_TEXTS = ["ab cd", "abc " + HEBREW + " def", HEBREW + " " + HEBREW, "ab\ncd ef\n" + HEBREW, "x", "a b c d e f g h i j", "ְֱת׫ a", "ab א"]


def draw_setup(b, text, wrap=40, v=4, h=0, style=1, blend=1, height=10, font=0, table=None):
    if table is not None:
        b.world["table"] = table
    b.params(0, font, style, blend)
    b.formatter(0, v, h)
    b.chars(0, hebrew_spec(text))
    b.add("poke", 0, 0, 3, wrap)
    b.add("poke", 0, 0, 4, height)
    b.layout(0, RECTS[1])


def g_drawtext_edges():
    out = []
    for k, text in enumerate(DRAW_TEXTS):
        for h in (0, 1, 2):
            for modes in ([0], [1], [2], [1, 2, 0]):
                out.append(("text %d h %d modes %s" % (k, h, modes), lambda b, text=text, h=h, modes=modes: (
                    b.world["queues"].update(ProcessLine=modes), draw_setup(b, text, 30, 4, h), b.add("drawtext", 0, 0, 1, 1))))
    for draw in (0, 1):
        for want in (0, 1):
            out.append(("draw %d bounds %d" % (draw, want), lambda b, draw=draw, want=want: (draw_setup(b, DRAW_TEXTS[0], 100), b.add("drawtext", 0, 0, want, draw))))
    for v in (2, 4, 5):
        for h in (0, 1, 2):
            out.append(("v %d h %d" % (v, h), lambda b, v=v, h=h: (draw_setup(b, DRAW_TEXTS[3], 100, v, h), b.add("drawtext", 0, 0, 1, 1))))
    for mode in (1, 2, 3):
        out.append(("method mode %d" % mode, lambda b, mode=mode: (draw_setup(b, DRAW_TEXTS[0], 100), b.add("mode", 0, mode), b.add("drawtext", 0, 0, 1, 1))))
    out.append(("the method fails", lambda b: (b.world["queues"].update(MethodDrawChar=[-1, 0, 5]), draw_setup(b, DRAW_TEXTS[0], 100), b.add("drawtext", 0, 0, 1, 1))))
    out.append(("no method in the table", lambda b: (draw_setup(b, DRAW_TEXTS[0], 100, table=TABLE_NONE), b.add("drawtext", 0, 0, 1, 1))))
    out.append(("style 3 refuses", lambda b: (draw_setup(b, DRAW_TEXTS[0], 100, style=3), b.add("drawtext", 0, 0, 1, 1))))
    out.append(("style 0 and blend 0", lambda b: (draw_setup(b, DRAW_TEXTS[0], 100, style=0, blend=0, table=TABLE_FULL), b.add("drawtext", 0, 0, 1, 1))))
    out.append(("no characters", lambda b: (b.params(0, 0, 1).formatter(0), b.add("drawtext", 0, 0, 1, 1))))
    out.append(("drawn twice", lambda b: (draw_setup(b, DRAW_TEXTS[3], 30), b.add("drawtext", 0, 0, 1, 1), b.add("drawtext", 0, 0, 1, 1))))
    out.append(("after a longer text", lambda b: (draw_setup(b, DRAW_TEXTS[5], 30), b.add("drawtext", 0, 0, 1, 1), b.chars(0, hebrew_spec("ab")), b.layout(0, RECTS[1]),
                                                  b.add("drawtext", 0, 0, 1, 1))))
    out.append(("the render's own parameters", lambda b: (draw_setup(b, DRAW_TEXTS[0], 100), b.add("drawtext", 0, 100, 1, 1))))
    for h in (1, 2):
        for right in (60, 41, 20, 0, -7):
            out.append(("block narrower than a line: h %d right %d" % (h, right), lambda b, h=h, right=right: (
                draw_setup(b, DRAW_TEXTS[3], 100, 4, h), b.add("poke", 0, 0, 7, right), b.add("drawtext", 0, 0, 1, 1))))
    for h in (-1, 3, 9):
        out.append(("horizontal %d poked" % h, lambda b, h=h: (draw_setup(b, DRAW_TEXTS[3], 100, 4, 0), b.add("poke", 0, 0, 2, h), b.add("drawtext", 0, 0, 1, 1))))
    out.append(("hidden characters", lambda b: (draw_setup(b, DRAW_TEXTS[5], 8), b.add("drawtext", 0, 0, 1, 1))))
    out.append(("a long text through the numbers", lambda b: (draw_setup(b, "ab cd " * 12, 60, 4, 1), b.add("drawtext", 0, 0, 1, 1))))
    return out


def g_drawtext_random(b):
    r = b.r
    text = "".join(r.choice(["a", "b", " ", " ", "\n", "\t", "א", "ת", "ְ", "ֱ", "׫", "c"]) for _ in range(r.choice([0, 1, 4, 9, 16])))
    draw_setup(b, text, r.choice([0, 12, 24, 40, 100]), r.choice([2, 4, 5, 3]), r.choice([0, 1, 2, 3]), r.choice([0, 1, 2, 2, 3]), r.randrange(2),
               r.choice([0, 7, 10, 13]), r.randrange(NFONT))
    b.world["table"] = r.choice([TABLE_FULL, TABLE_FULL, TABLE_SPARSE, TABLE_NONE])
    if r.random() < 0.4:
        b.add("mode", 0, r.choice([0, 1, 2, 3]))
    if r.random() < 0.3:   # the text block is not the width of its widest line
        b.add("poke", 0, 0, r.choice([5, 6, 7, 8]), r.randrange(-20, 120))
    for _ in range(r.randrange(1, 3)):
        b.add("drawtext", 0, r.choice([0, 0, 100]), r.randrange(2), r.randrange(2))
        if r.random() < 0.3:
            b.chars(0, hebrew_spec(text[::-1]))
            b.layout(0, r.choice(RECTS))


# ---- EmptyStringProcess

def g_empty_edges():
    out = []
    for v in (-1, 0, 1, 2, 3, 4, 5, 6):
        for h in (-1, 0, 1, 2, 3):
            out.append(("v %d h %d" % (v, h), lambda b, v=v, h=h: (b.f_ctor(0), b.add("poke", 0, 0, 1, v), b.add("poke", 0, 0, 2, h), b.add("poke", 0, 0, 4, 7),
                                                                  b.add("empty", 0, *RECTS[2]))))
    for k, rect in enumerate(RECTS):
        out.append(("rect %s" % (rect,), lambda b, rect=rect: (b.formatter(0, 2, 2), b.add("poke", 0, 0, 4, 12), b.add("empty", 0, *rect))))
    out.append(("line height 0 and 1", lambda b: (b.formatter(0, 2, 2), b.add("poke", 0, 0, 4, 0), b.add("empty", 0, *RECTS[0]), b.add("poke", 0, 0, 4, 1),
                                                  b.add("empty", 0, *RECTS[0]))))
    out.append(("after a draw", lambda b: (b.params(0, 0, 1).formatter(0, 5, 1), b.add("draw", 0, *RECTS[1], 3, 0, 1), b.add("empty", 0, *RECTS[0]))))
    out.append(("twice", lambda b: (b.formatter(0, 5, 1), b.add("poke", 0, 0, 4, 9), b.add("empty", 0, *RECTS[0]), b.add("empty", 0, *RECTS[1]))))
    return out


def g_empty_random(b):
    r = b.r
    b.f_ctor(0, r.choice([0xA5, 0, 0xFF]))
    b.add("poke", 0, 0, 1, r.choice([2, 4, 5, r.randrange(-3, 9)]))
    b.add("poke", 0, 0, 2, r.choice([0, 1, 2, r.randrange(-3, 6)]))
    b.add("poke", 0, 0, 4, r.choice([0, 1, 7, 10, 13, 255, r.randrange(0, 40)]))
    if r.random() < 0.3:
        b.params(0, 0, 1)
        b.add("draw", 0, *r.choice(RECTS), r.randrange(4), 0, 1)
    for _ in range(r.randrange(1, 3)):
        b.add("empty", 0, r.randrange(-100, 100), r.randrange(-100, 100), r.randrange(-100, 200), r.randrange(-100, 200))


# ---- Draw and Measure

def g_draw_edges(op):
    out = []
    for text in range(8):
        for want in (0, 1):
            out.append(("text %d out %d" % (text, want), lambda b, text=text, want=want: (b.params(0, 0, 1).formatter(0, 4, 0), b.add("poke", 0, 0, 4, 4),
                                                                                           b.add(op, 0, *RECTS[0], text, 0, want))))
    for v in (2, 4, 5):
        for h in (0, 1, 2):
            out.append(("v %d h %d" % (v, h), lambda b, v=v, h=h: (b.params(0, 0, 1).formatter(0, v, h), b.add(op, 0, *RECTS[1], 3, 0, 1))))
    for k, rect in enumerate(RECTS):
        out.append(("rect %s" % (rect,), lambda b, rect=rect: (b.params(0, 0, 1).formatter(0, 2, 2), b.add(op, 0, *rect, 3, 0, 1))))
    out.append(("no font", lambda b: (b.p_ctor(0), b.formatter(0), b.add(op, 0, *RECTS[1], 2, 0, 1))))
    out.append(("no converter", lambda b: (b.params(0, 2, 1).formatter(0), b.add(op, 0, *RECTS[1], 2, 0, 1))))
    out.append(("style 3 (the render refuses)", lambda b: (b.params(0, 0, 3).formatter(0), b.add(op, 0, *RECTS[1], 2, 0, 1))))
    out.append(("no method", lambda b: (b.world.update(table=TABLE_NONE), b.params(0, 0, 1).formatter(0), b.add(op, 0, *RECTS[1], 2, 0, 1))))
    out.append(("the measure flag", lambda b: (b.params(0, 0, 1).formatter(0, 5, 1), b.add("poke", 0, 0, 0, 1), b.add(op, 0, *RECTS[1], 3, 0, 1))))
    out.append(("the measure flag, no output", lambda b: (b.params(0, 0, 1).formatter(0, 5, 1), b.add("poke", 0, 0, 0, 1), b.add(op, 0, *RECTS[1], 3, 0, 0))))
    out.append(("the measure flag, empty text", lambda b: (b.params(0, 0, 1).formatter(0, 5, 1), b.add("poke", 0, 0, 0, 1), b.add(op, 0, *RECTS[1], 0, 0, 1))))
    out.append(("narrow", lambda b: (b.params(0, 0, 1).formatter(0, 4, 0), b.add(op, 0, 0, 0, 12, 100, 3, 0, 1))))
    out.append(("zero and negative widths", lambda b: (b.params(0, 0, 1).formatter(0, 4, 0), b.add(op, 0, 0, 0, 0, 100, 3, 0, 1), b.add(op, 0, 50, 0, 10, 100, 3, 0, 1))))
    out.append(("twice", lambda b: (b.params(0, 0, 1).formatter(0, 4, 1), b.add(op, 0, *RECTS[1], 3, 0, 1), b.add(op, 0, *RECTS[0], 2, 0, 1))))
    out.append(("with Hebrew", lambda b: (b.world.update(queues={"ProcessLine": [1, 2, 0]}), b.params(0, 0, 1).formatter(0, 4, 2), b.add(op, 0, *RECTS[1], 6, 0, 1))))
    out.append(("the render's own parameters", lambda b: (b.params(0, 0, 1).formatter(0), b.add(op, 0, *RECTS[1], 2, 100, 1))))
    out.append(("method fails", lambda b: (b.world["queues"].update(MethodDrawChar=[-1]), b.params(0, 0, 1).formatter(0), b.add(op, 0, *RECTS[1], 3, 0, 1))))
    return out


def g_draw_random(op):
    def build(b):
        r = b.r
        b.params(0, r.randrange(NFONT), r.choice([0, 1, 2, 2, 3]), r.randrange(2))
        if r.random() < 0.1:
            b.add("poke", 1, 0, 0, -1)
        b.formatter(0, r.choice([2, 4, 5, 3]), r.choice([0, 1, 2, 3]))
        if r.random() < 0.4:
            b.add("poke", 0, 0, 0, 1)
        if r.random() < 0.3:
            b.add("poke", 0, 0, 4, r.choice([0, 3, 9]))
        if r.random() < 0.3:
            b.add("mode", 0, r.choice([0, 1, 2, 3]))
        for _ in range(r.randrange(1, 4)):
            rect = r.choice(RECTS + [(r.randrange(-100, 100), r.randrange(-100, 100), r.randrange(-100, 200), r.randrange(-100, 200))])
            b.add(op, 0, *rect, r.randrange(len(b.world["texts"])), r.choice([0, 0, 0, 100]), r.randrange(2))
    return build


# ---- TbFontRender

def g_rctor_edges():
    out = [("fill %#x" % fill, lambda b, fill=fill: b.r_ctor(0, fill)) for fill in (0, 0xA5, 0xFF, 0x5A)]
    out.append(("over a used render", lambda b: (b.params(0, 0, 1), b.r_ctor(0), b.add("r_init", 0, 0), b.r_ctor(0, 0x33))))
    return out


def g_rctor_random(b):
    b.r_ctor(b.r.randrange(NREND), b.r.choice([0, 0xA5, 0xFF, b.r.randrange(256)]))
    if b.r.random() < 0.5:
        b.params(0, b.r.randrange(NFONT), b.r.choice([0, 1, 2]))
        b.r_ctor(0, b.r.randrange(256))
        b.add("r_init", 0, 0)
        b.r_ctor(0, b.r.randrange(256))


def g_rdtor_edges():
    out = []
    for flag in FLAGS:
        out.append(("fresh, flag %d" % flag, lambda b, flag=flag: (b.r_ctor(0), b.add("r_dtor", 0, flag))))
        out.append(("initialised, flag %d" % flag, lambda b, flag=flag: (b.params(0, 1, 1), b.r_ctor(0), b.add("r_init", 0, 0), b.add("r_dtor", 0, flag))))
    return out


def g_rdtor_random(b):
    r = b.r
    b.params(0, r.randrange(NFONT), r.choice([0, 1, 2]), r.randrange(2))
    b.r_ctor(0, r.choice([0xA5, 0, 0xFF]))
    if r.random() < 0.7:
        b.add("r_init", 0, 0)
    b.add("r_dtor", 0, r.choice(FLAGS))


def g_rinit_edges():
    out = []
    for style in (-1, 0, 1, 2, 3, 4, 9):
        for font in (0, 1, 2):
            for blend in (0, 1):
                if style < 0 and blend == 0 and b_type(font) == 0:
                    continue
                out.append(("style %d font %d blend %d" % (style, font, blend), lambda b, style=style, font=font, blend=blend: (
                    b.world.update(table=TABLE_FULL), b.params(0, font, style, blend), b.r_ctor(0), b.add("r_init", 0, 0))))
    for table in (TABLE_SPARSE, TABLE_NONE):
        out.append(("table %s" % ("sparse" if table is TABLE_SPARSE else "none"), lambda b, table=table: (
            b.world.update(table=table), b.params(0, 0, 1, 1), b.r_ctor(0), b.add("r_init", 0, 0), b.add("poke", 1, 0, 2, 2), b.add("r_init", 0, 0))))
    for typ in range(0, 9):
        out.append(("type %d" % typ, lambda b, typ=typ: (b.world["fonts"][0].update(type=typ), b.world.update(table=TABLE_FULL), b.params(0, 0, 1, 1),
                                                         b.r_ctor(0), b.add("r_init", 0, 0))))
    out.append(("no font", lambda b: (b.p_ctor(0), b.add("poke", 1, 0, 2, 1), b.r_ctor(0), b.add("r_init", 0, 0))))
    out.append(("its own parameters", lambda b: (b.r_ctor(0), b.add("r_init", 0, 200))))
    out.append(("a formatter's parameters", lambda b: (b.params(0, 0, 1), b.formatter(0), b.r_ctor(0), b.add("r_init", 0, 100))))
    out.append(("twice, two fonts", lambda b: (b.world.update(table=TABLE_FULL), b.params(0, 0, 1, 1), b.params(1, 1, 2, 0), b.r_ctor(0), b.add("r_init", 0, 0),
                                                b.add("r_init", 0, 1), b.add("r_init", 0, 0))))
    out.append(("a refused one after a good one", lambda b: (b.world.update(table=TABLE_FULL), b.params(0, 0, 1, 1), b.params(1, 0, 3, 1), b.r_ctor(0),
                                                             b.add("r_init", 0, 0), b.add("r_init", 0, 1))))
    return out


def b_type(font):
    return [4, 4, 2][font]


def g_rinit_random(b):
    r = b.r
    b.world["table"] = r.choice([TABLE_FULL, TABLE_FULL, TABLE_SPARSE, TABLE_NONE, [r.choice([0, r.randrange(1, 31)]) for _ in range(30)]])
    b.params(0, r.randrange(NFONT), r.choice([0, 1, 2, 2, 3, 4]), r.randrange(2))
    b.params(1, r.randrange(NFONT), r.choice([0, 1, 2]), r.randrange(2))
    b.r_ctor(0, r.choice([0xA5, 0, 0xFF]))
    for _ in range(r.randrange(1, 4)):
        b.add("r_init", 0, r.choice([0, 1]))


def g_rdraw_edges():
    out = []
    for glyph in list(range(0, 15)) + [0xFFFF, 0x8000]:
        out.append(("glyph %d" % glyph, lambda b, glyph=glyph: (b.params(0, 0, 1), b.r_ctor(0), b.add("r_init", 0, 0), b.add("r_drawchar", 0, 17, -4, glyph))))
    for x, y in ((0, 0), (-5, 7), (0x7FFFFFF0, 3), (3, -0x7FFFFFF0)):
        out.append(("position %d %d" % (x, y), lambda b, x=x, y=y: (b.params(0, 0, 1), b.r_ctor(0), b.add("r_init", 0, 0), b.add("r_drawchar", 0, x, y, 9))))
    out.append(("no method", lambda b: (b.r_ctor(0), b.add("r_drawchar", 0, 1, 2, 3))))
    out.append(("after a refused Initialise", lambda b: (b.params(0, 0, 3), b.r_ctor(0), b.add("r_init", 0, 0), b.add("r_drawchar", 0, 1, 2, 3))))
    for mode in (1, 2, 3):
        out.append(("method mode %d" % mode, lambda b, mode=mode: (b.params(0, 0, 1), b.r_ctor(0), b.add("r_init", 0, 0), b.add("mode", 0, mode),
                                                                    b.add("r_drawchar", 0, 1, 2, 3))))
    out.append(("the method fails", lambda b: (b.world["queues"].update(MethodDrawChar=[-5, 0]), b.params(0, 0, 1), b.r_ctor(0), b.add("r_init", 0, 0),
                                                b.add("r_drawchar", 0, 1, 2, 3), b.add("r_drawchar", 0, 1, 2, 3))))
    out.append(("every cell of the table", lambda b: (b.world.update(table=TABLE_FULL), b.r_ctor(0),
                                                      [(b.add("poke", 3, 0, 0, k), b.add("r_drawchar", 0, k, k, k)) for k in range(0, 31)])))
    return out


def g_rdraw_random(b):
    r = b.r
    b.world["table"] = r.choice([TABLE_FULL, TABLE_SPARSE])
    b.params(0, r.randrange(NFONT), r.choice([0, 1, 2]), r.randrange(2))
    b.r_ctor(0, r.choice([0xA5, 0]))
    if r.random() < 0.8:
        b.add("r_init", 0, 0)
    for _ in range(r.randrange(1, 4)):
        b.add("r_drawchar", 0, r.randrange(-1000, 1000), r.randrange(-1000, 1000), r.randrange(65536))
        if r.random() < 0.3:
            b.add("mode", 0, r.choice([0, 1, 2, 3]))


# ---- the three TbFontRenderParams members

COLOURS = [(0, 0, 0, 0, 0), (255, 255, 255, 255, 255), (1, 2, 3, 4, 5), (0x80, 0x7F, 0, 1, 0xFE)]


def g_recolour_edges():
    out = []
    for caps in range(16):
        out.append(("caps %d" % caps, lambda b, caps=caps: (b.world["fonts"][0].update(caps=caps), b.p_ctor(0), b.add("setfont", 0, 0), b.add("recolour", 0, 9, 8, 7, 6, 5))))
    out.append(("no font", lambda b: (b.p_ctor(0), b.add("recolour", 0, 9, 8, 7, 6, 5))))
    for colour in COLOURS:
        out.append(("colour %s" % (colour,), lambda b, colour=colour: (b.world["fonts"][0].update(caps=2), b.p_ctor(0, 0), b.add("setfont", 0, 0), b.add("recolour", 0, *colour))))
    out.append(("after the font was dropped", lambda b: (b.world["fonts"][0].update(caps=2), b.p_ctor(0), b.add("setfont", 0, 0), b.add("setfont", 0, -1),
                                                         b.add("recolour", 0, 1, 2, 3, 4, 5))))
    out.append(("twice", lambda b: (b.world["fonts"][0].update(caps=3), b.p_ctor(0), b.add("setfont", 0, 0), b.add("recolour", 0, 1, 2, 3, 4, 5), b.add("recolour", 0, 6, 7, 8, 9, 10))))
    return out


def g_recolour_random(b):
    r = b.r
    b.p_ctor(0, r.choice([0xA5, 0, 0xFF]))
    if r.random() < 0.85:
        b.add("setfont", 0, r.randrange(NFONT))
    for _ in range(r.randrange(1, 4)):
        b.add("recolour", 0, *[r.randrange(256) for _ in range(5)])
        if r.random() < 0.3:
            b.add("poke", 1, 0, 1, r.randrange(1 << 32))
        if r.random() < 0.2:
            b.add("poke", 1, 0, 2, r.randrange(5))


def g_precol_edges():
    out = []
    for caps in range(16):
        out.append(("caps %d" % caps, lambda b, caps=caps: (b.world["fonts"][0].update(caps=caps), b.p_ctor(0), b.add("setfont", 0, 0), b.add("precol", 0))))
    out.append(("no font", lambda b: (b.p_ctor(0), b.add("precol", 0))))
    out.append(("after a recolour", lambda b: (b.world["fonts"][0].update(caps=3), b.p_ctor(0), b.add("setfont", 0, 0), b.add("recolour", 0, 1, 2, 3, 4, 5), b.add("precol", 0))))
    out.append(("style was 7", lambda b: (b.world["fonts"][0].update(caps=1), b.p_ctor(0), b.add("setfont", 0, 0), b.add("poke", 1, 0, 2, 7), b.add("precol", 0))))
    return out


def g_precol_random(b):
    r = b.r
    b.p_ctor(0, r.choice([0xA5, 0, 0xFF]))
    if r.random() < 0.85:
        b.add("setfont", 0, r.randrange(NFONT))
    for _ in range(r.randrange(1, 4)):
        b.add("precol", 0)
        if r.random() < 0.3:
            b.add("poke", 1, 0, 1, r.randrange(1 << 32))
        if r.random() < 0.3:
            b.add("poke", 1, 0, 2, r.randrange(5))


def g_setfont_edges():
    out = []
    for first in (-1, 0, 1, 2):
        for second in (-1, 0, 1, 2):
            out.append(("font %d then %d" % (first, second), lambda b, first=first, second=second: (b.p_ctor(0), b.add("setfont", 0, first), b.add("setfont", 0, second))))
    out.append(("one font", lambda b: (b.p_ctor(0, 0), b.add("setfont", 0, 1))))
    out.append(("the same font twice", lambda b: (b.p_ctor(0), b.add("setfont", 0, 1), b.add("setfont", 0, 1))))
    out.append(("after a style", lambda b: (b.p_ctor(0), b.add("poke", 1, 0, 2, 1), b.add("setfont", 0, 0))))
    out.append(("none keeps the caps", lambda b: (b.p_ctor(0), b.add("setfont", 0, 0), b.add("setfont", 0, -1))))
    out.append(("none on a fresh object", lambda b: (b.p_ctor(0, 0xFF), b.add("setfont", 0, -1))))
    return out


def g_setfont_random(b):
    r = b.r
    b.p_ctor(0, r.choice([0xA5, 0, 0xFF]))
    for _ in range(r.randrange(1, 5)):
        b.add("setfont", 0, r.choice([-1, 0, 1, 2]))
        if r.random() < 0.3:
            b.add("poke", 1, 0, 2, r.randrange(5))


def g_getfont_edges():
    out = []
    out.append(("no font", lambda b: (b.p_ctor(0), b.add("getfont", 0))))
    out.append(("no font, filler 0xff", lambda b: (b.p_ctor(0, 0xFF), b.add("getfont", 0))))
    for font in range(NFONT):
        out.append(("font %d" % font, lambda b, font=font: (b.p_ctor(0), b.add("setfont", 0, font), b.add("getfont", 0))))
        out.append(("font %d twice" % font, lambda b, font=font: (b.p_ctor(0), b.add("setfont", 0, font), b.add("getfont", 0), b.add("getfont", 0))))
    out.append(("after the font was dropped", lambda b: (b.p_ctor(0), b.add("setfont", 0, 1), b.add("setfont", 0, -1), b.add("getfont", 0))))
    return out


def g_getfont_random(b):
    r = b.r
    b.p_ctor(0, r.choice([0xA5, 0, 0xFF]))
    for _ in range(r.randrange(1, 6)):
        if r.random() < 0.4:
            b.add("setfont", 0, r.choice([-1, 0, 1, 2]))
        else:
            b.add("getfont", 0)


# ---- the three vector tear-downs

def g_td_edges(kind):
    out = []
    for cap, size, nbytes in ((0, 0, 0), (0, 0, 16), (1, 1, 16), (4, 3, 64), (8, 8, 128), (1000, 1000, 16000), (5, 0, 64), (0, 3, 0), (7, 7, 0), (2, 5, 32)):
        out.append(("capacity %d size %d block %d" % (cap, size, nbytes), lambda b, cap=cap, size=size, nbytes=nbytes: b.add("teardown", kind, cap, size, nbytes)))
    out.append(("twice", lambda b: (b.add("teardown", kind, 4, 3, 64), b.add("teardown", kind, 4, 3, 64))))
    return out


def g_td_random(kind):
    def build(b):
        r = b.r
        for _ in range(r.randrange(1, 4)):
            # the sizes stay small: the original counts its (empty) destruction loop down once per element
            b.add("teardown", kind, r.choice([0, 1, 2, 7, 100, 0xFFFFFFFF, r.randrange(1 << 32)]), r.choice([0, 1, 5, 100, 2000, r.randrange(3000)]),
                  r.choice([0, 0, 1, 16, 400, 4096]))
    return build


# ---- random scripts of every operation

def random_script(r, nops=200):
    world = random_world(r, ntexts=6)
    b = Builder(r, world)
    b.params(0, r.randrange(NFONT), r.choice([0, 1, 2]), r.randrange(2))
    b.params(1, r.randrange(NFONT), r.choice([0, 1, 2]), r.randrange(2))
    b.formatter(0, r.choice([2, 4, 5]), r.choice([0, 1, 2]))
    b.r_ctor(0)
    ptext = lambda: r.randrange(len(world["texts"]))   # noqa: E731

    def formatters():
        return [n for n in range(NFORM) if b.f[n]]

    def params():
        return [n for n in range(NPARAM) if b.p[n]]

    def renders():
        return [n for n in range(NREND) if b.rn[n]]

    while len(b.ops) < nops:
        fs, ps, rs = formatters(), params(), renders()
        k = r.randrange(32)
        if k == 0 and len(fs) < NFORM:
            b.formatter(NFORM - 1 if not b.f[NFORM - 1] else 0, r.choice([2, 4, 5]), r.choice([0, 1, 2]), r.choice([0xA5, 0, 0xFF]))
        elif k == 1 and fs and r.random() < 0.3:
            n = r.choice(fs)
            b.add("f_dtor", n, r.choice(FLAGS))
            b.f[n] = False
        elif k == 2 and fs:
            b.add(r.choice(["set_v", "set_h"]), r.choice(fs), r.choice(JUSTS + [2, 4, 5, 0, 1] * 3))
        elif k == 3 and fs:
            b.add(r.choice(["get_arr", "get_arr_size"]), r.choice(fs))
        elif k == 4 and fs:
            b.add("idx", r.choice(fs), r.randrange(-50, 50), r.randrange(-50, 50))
        elif k == 5 and fs:
            b.add("char_type", r.choice(fs), r.choice([9, 10, 13, 32, 65, r.randrange(65536)]))
        elif k in (6, 7) and fs and ps:
            b.create(r.choice(fs), ptext(), r.choice(ps))
        elif k in (8, 9) and fs:
            n = r.choice(fs)
            b.layout(n, r.choice(RECTS))
        elif k in (10, 11) and fs:
            n = r.choice(fs)
            size = b.nchars[n]
            i = r.randrange(size + 1)
            j = r.randrange(i, size + 1)
            b.add("lines", n, i, j)
        elif k in (12, 13, 14) and fs and ps and any(b.lines_ok[n] for n in fs):
            n = r.choice([n for n in fs if b.lines_ok[n]])
            b.add("drawtext", n, r.choice(ps + [100 + n]), r.randrange(2), r.randrange(2))
        elif k == 15 and fs:
            b.add("empty", r.choice(fs), *r.choice(RECTS))
        elif k in (16, 17) and fs and ps:
            n = r.choice(fs)
            b.add("draw", n, *r.choice(RECTS), ptext(), r.choice(ps + [100 + n]), r.randrange(2))
            b.lines_ok[n] = False
        elif k in (18, 19) and fs and ps:
            n = r.choice(fs)
            b.add("measure", n, *r.choice(RECTS), ptext(), r.choice(ps), r.randrange(2))
            b.lines_ok[n] = False
        elif k == 20 and fs:
            n = r.choice(fs)
            spec = [(0x41 + i, 0x41 + i, r.choice([1, 1, 0x12, 4, 0xC]), r.choice([0, 5, 6, 9]), i) for i in range(r.choice([0, 1, 4, 8]))]
            b.chars(n, spec)
        elif k == 21 and fs:
            b.add("poke", 0, r.choice(fs), r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 10]), r.choice([0, 1, 7, 10, 30, 100, r.randrange(0, 1 << 32)]))
        elif k == 22 and rs:
            n = r.choice(rs)
            if r.random() < 0.3:
                b.add("r_dtor", n, r.choice(FLAGS))
                if b.ops[-1][2] > 0:
                    b.rn[n] = False
            else:
                b.add("r_init", n, r.choice(ps + [100 + f for f in fs] + [200 + n]))
        elif k == 23 and rs:
            b.add("r_drawchar", r.choice(rs), r.randrange(-50, 50), r.randrange(-50, 50), r.randrange(65536))
        elif k == 24 and len(rs) < NREND:
            b.r_ctor(r.choice([n for n in range(NREND) if not b.rn[n]]), r.choice([0xA5, 0]))
        elif k == 25 and ps:
            p = r.choice(ps)
            font = r.choice([-1, 0, 0, 0, 1, 1, 2])
            b.add("setfont", p, font)
            if font >= 0 and r.random() < 0.7:
                b.add("poke", 1, p, 2, r.choice([0, 1, 1, 2]))
        elif k == 26 and ps:
            b.add("recolour", r.choice(ps), *[r.randrange(256) for _ in range(5)])
        elif k == 27 and ps:
            b.add("precol", r.choice(ps))
        elif k == 28 and ps:
            b.add("poke", 1, r.choice(ps), 2, r.choice([0, 1, 2, 2, 3]))
        elif k == 29 and ps and len(ps) < NPARAM:
            b.p_ctor(r.choice([n for n in range(NPARAM) if not b.p[n]]), r.choice([0xA5, 0]))
        elif k == 30 and ps and r.random() < 0.3:
            p = r.choice(ps)
            b.add("p_dtor", p, r.choice(FLAGS))
            if b.ops[-1][2] > 0:
                b.p[p] = False
        elif k == 31:
            b.add("mode", 0, r.choice([0, 0, 1, 2, 3]))
            b.add("teardown", r.randrange(3), r.randrange(4), r.randrange(4), r.choice([0, 64]))
        # at least one formatter, parameters object and render must remain for the rest
        if not b.f[0] and not b.f[1]:
            b.formatter(0, r.choice([2, 4, 5]), r.choice([0, 1, 2]))
        if not any(b.p):
            b.params(0, r.randrange(NFONT), r.choice([0, 1, 2]))
        if not any(b.rn):
            b.r_ctor(0)
    return world


# ======================================================================= the game's own data

import pathlib   # noqa: E402

LANGUAGE = pathlib.Path(__file__).resolve().parents[2] / "extracted/disc-hfs/Theme Park Data/data/Language/American"


def real_data():
    """The American language's character converter (MBToUni.dat) and three of its bitmap fonts (widths by glyph, the line height of
    byte 5), read from ROOT/extracted at run time; None when they are not there."""
    try:
        raw = (LANGUAGE / "MBToUni.dat").read_bytes()
        lead_word, counts = struct.unpack("<HH", raw[4:8])
        first_lead = (0xFF - (lead_word & 0xFF)) & 0xFF
        last_lead, last_trail = counts & 0xFF, (counts >> 8) & 0xFF
        count = (((last_lead - 1) & 0xFF) * 0xFF + first_lead * -0xFE + last_trail - 1) & 0xFFFF
        table = list(struct.unpack("<%dH" % ((len(raw) - 8) // 2), raw[8:]))
        fonts = []
        for name in ("GAME10.bf4", "TITLEBIG.bf4", "GAME6AA.bf4"):
            data = (LANGUAGE / name).read_bytes()
            n = struct.unpack("<H", data[6:8])[0]
            offsets = struct.unpack("<%dI" % n, data[8:8 + 4 * n])
            fonts.append(dict(spacing=data[5], widths=[struct.unpack("<H", data[o + 0x16:o + 0x18])[0] for o in offsets]))
        return dict(lead=first_lead, count=count, table=table, fonts=fonts)
    except (OSError, struct.error):
        return None


REAL_STRINGS = ["Create new player", "Quit game", "Welcome to Theme Park World", "Click here to build a new ride in your park.",
                "Your park has 3 rides and 2 shops.\nThe guests are happy.", "A very very long word: Supercalifragilisticexpialidocious!",
                "Name:\n\tTim\r\nCash: $1,234.50", ""]


def real_world(name_of_font=0):
    """A world whose font 0/1/2 are the game's GAME10, TITLEBIG and GAME6AA, converter 0 its MBToUni.dat, and texts the strings above."""
    data = real_data()
    if data is None:
        return None
    inverse = {}
    for i, u in enumerate(data["table"]):
        inverse.setdefault(u, i + 1)
    texts = [bytes(inverse.get(ord(ch), inverse[0x20]) for ch in s) for s in REAL_STRINGS]
    world = edge_world()
    world["convs"][0] = dict(lead=data["lead"], count=data["count"], table=data["table"])
    world["fonts"] = [font_cfg(f["spacing"], 3, 4, 0, 0, 1, 1) for f in data["fonts"]]
    for cfg, f in zip(world["fonts"], data["fonts"]):
        cfg["widths"] = f["widths"]
    world["texts"] = texts
    return world


def real_cases(op, rects=((0, 0, 200, 80), (10, 10, 120, 100), (0, 0, 400, 30), (5, 5, 60, 200))):
    """Draw, Measure or a DrawText flow on the real font and strings (a case list; empty without the game's files)."""
    out = []
    if real_world() is None:
        return out
    for font in range(3):
        for text in range(len(REAL_STRINGS)):
            for v, h in ((4, 0), (5, 1), (2, 2)):
                rect = rects[(font + text) % len(rects)]

                def build(b, font=font, text=text, v=v, h=h, rect=rect):
                    b.params(0, font, 1, 1)
                    b.formatter(0, v, h)
                    if op == "drawtext":
                        b.add("draw", 0, *rect, text, 0, 1)
                        b.layout(0, rect)
                        b.add("drawtext", 0, 0, 1, 1)
                    else:
                        b.add(op, 0, *rect, text, 0, 1)
                world = real_world()
                world["label"] = "real font %d text %d v %d h %d" % (font, text, v, h)
                build(Builder(random.Random(0), world))
                out.append(world)
    return out


# ======================================================================= the tests

def E(edges):
    return lambda: edge_cases(edges())()


def script_edges():
    return [random_script(random.Random(900 + i), 200) for i in range(6)]


TESTS = [
    Group("TbTextFormatter::TbTextFormatter", E(g_ctor_edges), random_of(g_ctor_random)),
    Group("TbTextFormatter::~TbTextFormatter", E(g_dtor_edges), random_of(g_dtor_random)),
    Group("TbTextFormatter::SetVerticalJustification", E(lambda: g_set_edges("set_v")), random_of(g_set_random("set_v"))),
    Group("TbTextFormatter::SetHorizontalJustification", E(lambda: g_set_edges("set_h")), random_of(g_set_random("set_h"))),
    Group("TbTextFormatter::GetCharPositionArray", E(lambda: g_arr_edges("get_arr")), random_of(g_arr_random("get_arr"))),
    Group("TbTextFormatter::GetCharPositionArraySize", E(lambda: g_arr_edges("get_arr_size")), random_of(g_arr_random("get_arr_size"))),
    Group("TbTextFormatter::GetCharIdxFromPosition", E(g_idx_edges), random_of(g_idx_random)),
    Group("TbTextFormatter::GetCharType", E(g_chartype_edges), random_of(g_chartype_random)),
    Group("TbTextFormatter::CreateInfoList", E(g_create_edges), random_of(g_create_random)),
    Group("TbTextFormatter::Layout", E(g_layout_edges), random_of(g_layout_random)),
    Group("TbTextFormatter::LayoutLines", E(g_lines_edges), random_of(g_lines_random)),
    Group("TbTextFormatter::DrawText", lambda: E(g_drawtext_edges)() + real_cases("drawtext"), random_of(g_drawtext_random)),
    Group("TbTextFormatter::EmptyStringProcess", E(g_empty_edges), random_of(g_empty_random)),
    Group("TbTextFormatter::Draw", lambda: E(lambda: g_draw_edges("draw"))() + real_cases("draw"), random_of(g_draw_random("draw"))),
    Group("TbTextFormatter::Measure", lambda: E(lambda: g_draw_edges("measure"))() + real_cases("measure"), random_of(g_draw_random("measure"))),
    Group("TbFontRender::TbFontRender", E(g_rctor_edges), random_of(g_rctor_random)),
    Group("TbFontRender::~TbFontRender", E(g_rdtor_edges), random_of(g_rdtor_random)),
    Group("TbFontRender::Initialise", E(g_rinit_edges), random_of(g_rinit_random)),
    Group("TbFontRender::DrawChar", E(g_rdraw_edges), random_of(g_rdraw_random)),
    Group("TbFontRenderParams::UseReColour", E(g_recolour_edges), random_of(g_recolour_random)),
    Group("TbFontRenderParams::UsePreColoured", E(g_precol_edges), random_of(g_precol_random)),
    Group("TbFontRenderParams::SetFont", E(g_setfont_edges), random_of(g_setfont_random)),
    Group("TbFontRenderParams::GetFont", E(g_getfont_edges), random_of(g_getfont_random)),
    Group("std::vector<CCharacterInfo>::tear_down", E(lambda: g_td_edges(0)), random_of(g_td_random(0))),
    Group("std::vector<CLineInfo>::tear_down", E(lambda: g_td_edges(1)), random_of(g_td_random(1))),
    Group("std::vector<TbPoint>::tear_down", E(lambda: g_td_edges(2)), random_of(g_td_random(2))),
    Group("random scripts of 200 operations", script_edges, lambda r: random_script(r, 200)),
]
