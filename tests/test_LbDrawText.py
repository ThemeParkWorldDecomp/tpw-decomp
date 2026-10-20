"""Unit w224: all 35 Bullfrog text entry points, templates, sprite renderers and rectangle leaves.

Renderer callbacks and the two external sprite draws record all arguments. Measured widths,
heights and draw advances vary independently. Both sides compare globals, guarded text,
size/rectangle/font blocks, drawing area and every pixel. Real sprite tables are also used.
No floating point operations occur. Excluded: unchecked NULL renderers, cheap-font indices
outside its table, nonterminating width/whitespace cases, and the Mac's unset layout registers.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbDrawText"
MASK = 0xFFFFFFFF
# In manifest order; each template instantiation runs separately.
FUNCTIONS = [
    (0x1001B6A8, "LbDraw_Text", 48),
    (0x1001B708, "LbDraw_UnicodeText", 48),
    (0x1001B770, "LbDraw_FormattedText", 48),
    (0x1001B7E4, "LbDraw_UnicodeFormattedText", 48),
    (0x1001B85C, "LbDraw_GetTextSize", 32),
    (0x1001B8B0, "LbDraw_GetUnicodeTextSize", 32),
    (0x1001B90C, "LbDraw_GetFormattedTextHeight", 32),
    (0x1001B964, "LbDraw_GetUnicodeFormattedTextHeight", 32),
    (0x1001B9C4, "LbDraw_CheapText", 48),
    (0x1001BA28, "LbDraw_UnicodeCheapText", 48),
    (0x1001BA94, "SpriteRender_OnMeasure", 32),
    (0x1001BAEC, "SpriteRender_OnDrawChar", 72),
    (0x1001BB70, "OneColourSpriteRender_OnMeasure", 32),
    (0x1001BBD4, "OneColourSpriteRender_OnDrawChar", 88),
    (0x1001BC74, "LbDraw_SetTextFont", 220),
    (0x1001BD8C, "LbDraw_GetTextCharacterWidth", 80),
    (0x1001BE10, "LbDraw_GetTextCharacterHeight", 80),
    (0x1001C0AC, "Draw_CheapText<char>", 476),
    (0x1001BE98, "Draw_CheapText<wchar_t>", 476),
    (0x1001C48C, "Text_MeasureFormattedHeight<char>", 408),
    (0x1001C2C0, "Text_MeasureFormattedHeight<wchar_t>", 400),
    (0x1001CAC4, "Draw_FormattedText<char>", 1060),
    (0x1001C660, "Draw_FormattedText<wchar_t>", 1056),
    (0x1001D028, "Draw_Text<char>", 204),
    (0x1001CF2C, "Draw_Text<wchar_t>", 204),
    (0x1001D124, "draw_text_chars<char>", 136),
    (0x1001D1E4, "draw_text_chars<wchar_t>", 132),
    (0x1001D2A0, "draw_text_line<char>", 792),
    (0x1001D5F8, "draw_text_line<wchar_t>", 788),
    (0x1001D94C, "word_width<char>", 168),
    (0x1001DA20, "word_width<wchar_t>", 164),
    (0x1001DAF0, "Text_Measure<char>", 380),
    (0x1001DC9C, "Text_Measure<wchar_t>", 376),
    (0x1002998C, "LbRect_Width", 16),
    (0x100299C8, "LbRect_Height", 16),
]
ADDR = dict(gLbTextFont=0x10074488, gLbTextRender=0x1007448C, gLbTextSelectSense=0x10072A7C,
            gLbCheapTextColour=0x10074470, gLbCheapTextGuard=0x10074474, gLbCheapTextIndex=0x10074478,
            gLbCheapUnicodeTextColour=0x1007447C, gLbCheapUnicodeTextGuard=0x10074480,
            gLbCheapUnicodeTextIndex=0x10074484, _lbpCurrentRA=0x10074880,
            _lbCurrentClipRect=0x10074860, _lbCurrentOriginPtr=0x1007485C,
            _lbSpriteRender=0x10072A5C, _lbOneColourSpriteRender=0x10072A6C)
STUBS = CL.Stubs()
STUBS.real.update(a for a, _, _ in FUNCTIONS)


def colour(e, reg):
    return list(struct.pack(">II", e.r(reg), e.r(reg + 1))[:5])


def sprite(e):
    e.cl.rec("Sprite", (T.s32(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5) - e.ldt_font) // 8))
    width = e.cl.ret("SpriteWidth", -1)
    if width >= 0:
        e.write(e.r(5) + 4, struct.pack(">H", width & 0xFFFF))


def one_colour(e):
    e.cl.rec("OneColour", (T.s32(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5) - e.ldt_font) // 8, *colour(e, 6)))
    width = e.cl.ret("SpriteWidth", -1)
    if width >= 0:
        e.write(e.r(5) + 4, struct.pack(">H", width & 0xFFFF))


STUBS.internal(0x1001B630, sprite)
STUBS.internal(0x1001AA30, one_colour)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    # Recheck a mask word directly in the loaded image. rd.py cannot start in this sandbox.
    assert e.u32(0x10072D68) == 0xFF

    def measure(e):
        font, size, ch = e.r(3), e.r(4), e.r(5)
        e.cl.rec("Measure", (e.cl.lab(font), ch, e.s32(size), e.s32(size + 4)))
        if e.ldt_case["mask"] & 1:
            e.write(size, struct.pack(">I", e.ldt_case["widths"][ch & 255] & MASK))
        if e.ldt_case["mask"] & 2:
            e.write(size + 4, struct.pack(">I", e.ldt_case["heights"][ch & 255] & MASK))

    def draw(e):
        ch = e.r(6)
        e.cl.rec("Draw", (e.cl.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), ch, *colour(e, 7)))
        e.set_r(3, e.ldt_case["advances"][ch & 255] & MASK)

    def select(e):
        result, slot, font = e.r(3), e.r(4), e.r(5)
        e.cl.rec("Select", (e.cl.lab(slot), e.cl.lab(font), e.cl.lab(e.u32(slot)), e.cl.lab(e.u32(ADDR["gLbTextRender"]))))
        if e.cl.ret("WriteFont", 0):
            e.write(slot, struct.pack(">I", font))
        e.write(result, struct.pack(">I", e.cl.ret("Select", 0) & MASK))

    def release(e):
        e.cl.rec("Release", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), e.cl.lab(e.r(5)), e.cl.lab(e.u32(ADDR["gLbTextRender"]))))

    callbacks = [T.fake_function(e, f) for f in (select, release, measure, draw)]
    e.ldt_tables = []
    for i in range(4):
        table = T.persist_alloc(e, 16)
        e.write(table, struct.pack(">IIII", callbacks[0] if i & 1 else 0,
                                   callbacks[1] if i & 2 else 0, callbacks[2], callbacks[3]))
        e.ldt_tables.append(table)
    ctype = bytearray(256)
    for ch in b" \t\n\v\f\r":
        ctype[ch] = 4
    e.write(e.data_import("__ctype_map"), bytes(ctype))


def bind(lib):
    if getattr(lib, "ldt_bound", False):
        return
    lib.tpw_ldt_addr.argtypes, lib.tpw_ldt_addr.restype = [C.c_char_p], C.c_uint64
    lib.tpw_ldt_alloc.argtypes, lib.tpw_ldt_alloc.restype = [C.c_uint32], C.c_uint64
    lib.tpw_ldt_table.argtypes, lib.tpw_ldt_table.restype = [C.c_uint32], C.c_uint64
    lib.tpw_ldt_config.argtypes = [C.POINTER(C.c_uint32)] * 3 + [C.c_uint32, C.c_uint64]
    lib.tpw_ldt_call.argtypes = [C.c_uint32, C.POINTER(C.c_uint64), C.c_void_p]
    lib.tpw_ldt_call.restype = C.c_uint32
    lib.tpw_ldt_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 6)()
    lib.tpw_ldt_layout(out)
    assert list(out) == [16, 8, 10, 48, 40, 32], list(out)
    lib.ldt_bound = True


class Side:
    def word(self, addr, value):
        self.write(addr, struct.pack(self.endian + "I", value & MASK))

    def words(self, addr, values):
        self.write(addr, struct.pack(self.endian + "I" * len(values), *(v & MASK for v in values)))

    def rd(self, addr, count=1):
        return list(struct.unpack(self.endian + "I" * count, self.read(addr, count * 4)))

    def ptr(self, addr, value):
        self.write(addr, struct.pack(self.endian + ("I" if self.ps == 4 else "Q"), value))

    def rp(self, addr):
        return int.from_bytes(self.read(addr, self.ps), "big" if self.ps == 4 else "little")

    def lab(self, addr):
        return self.labels.get(addr, 0 if addr == 0 else -1)


class Ppc(Side):
    endian, ps, sprite_size, area_size, stride_offset = ">", 4, 8, 40, 36

    def __init__(self, e, c):
        self.e = e
        e.ldt_case = c
        self.saved = STUBS.activate(e)
        self.tables = e.ldt_tables

    def alloc(self, n):
        return self.e.alloc(n)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def begin(self, c):
        self.e.ldt_font = self.font
        self.e.cl = CL.PpcLog(self.labels, c["queues"])

    def log(self):
        return self.e.cl.log

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def call(self, c, a):
        op = c["op"]
        x, y, text, size, rect, flags, count, nl, width, lw, ch, render, font = a
        cw = list(struct.unpack(">II", bytes(c["colour"]) + bytes(3)))
        if op in (0, 1, 8, 9, 17, 18, 23, 24):
            args = [x, y, text, *cw]
        elif op in (2, 3, 21, 22):
            args = [rect, text, flags, *cw]
        elif op in (4, 5, 31, 32, 29, 30):
            args = [text, size]
        elif op in (6, 7, 19, 20):
            args = [width, text]
        elif op in (10, 12):
            args = [font, size, ch]
        elif op in (11, 13):
            args = [font, x, y, ch, *cw]
        elif op == 14:
            args = [size, render, font]
        elif op in (15, 16):
            args = [ch]
        elif op in (25, 26):
            args = [x, y, text, count, *cw]
        elif op in (27, 28):
            args = [x, y, text, count, *cw, flags, nl, width, lw]
        else:
            args = [rect]
        r3 = T.call_ppc(self.e, FUNCTIONS[op][0], [v & MASK for v in args])[0] & MASK
        if op == 14:
            return self.rd(size)[0]
        return r3 if op in (4, 5, 6, 7, 11, 13, 15, 16, 19, 20, 29, 30, 31, 32, 33, 34) else 0


class Host(Side):
    endian, ps, sprite_size, area_size, stride_offset = "<", 8, 16, 48, 40

    def __init__(self, lib, c):
        self.lib = lib
        bind(lib)
        self.tables = [lib.tpw_ldt_table(i) for i in range(4)]

    def alloc(self, n):
        return self.lib.tpw_ldt_alloc(n)

    def g(self, name):
        return self.lib.tpw_ldt_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def begin(self, c):
        arrays = [(C.c_uint32 * 256)(*(v & MASK for v in c[n])) for n in ("widths", "heights", "advances")]
        self.lib.tpw_ldt_config(*arrays, c["mask"], self.font)
        CL.host_begin(self.lib, {label: addr for addr, label in self.labels.items()}, c["queues"])

    def log(self):
        return CL.host_end(self.lib)

    def done(self):
        pass

    def call(self, c, a):
        result = self.lib.tpw_ldt_call(c["op"], (C.c_uint64 * len(a))(*(v & 0xFFFFFFFFFFFFFFFF for v in a)), bytes(c["colour"]))
        if c["op"] == 14:
            self.word(a[3], result)
        return result


def build(s, c):
    op = c["op"]
    wide = "Unicode" in FUNCTIONS[op][1] or "wchar_t" in FUNCTIONS[op][1]
    s.text_units = list(c["text"]) + [0, 0x55, 0x66]
    s.text_block = s.alloc(8 + len(s.text_units) * (2 if wide else 1))
    s.write(s.text_block, bytes([0xA5]) * (8 + len(s.text_units) * (2 if wide else 1)))
    s.text = s.text_block + 4
    s.wide = wide
    s.write(s.text, struct.pack(s.endian + ("H" if wide else "B") * len(s.text_units), *s.text_units))
    s.size_block, s.rect_block = s.alloc(16), s.alloc(24)
    s.words(s.size_block, [0xA5A5A5A5, *c["size"], 0xA5A5A5A5])
    s.words(s.rect_block, [0xA5A5A5A5, *c["rect"], 0xA5A5A5A5])
    s.font_block = s.alloc(512 * s.sprite_size + 16)
    s.write(s.font_block, bytes([0xA5]) * (512 * s.sprite_size + 16))
    s.font = s.font_block + 8 + 160 * s.sprite_size
    font_image = bytearray([0xA5]) * (512 * s.sprite_size)
    for i, (w, h) in enumerate(c["sprites"]):
        a = i * s.sprite_size
        font_image[a:a + s.ps] = bytes(s.ps)
        font_image[a + s.ps:a + s.ps + 4] = struct.pack(s.endian + "HH", w, h)
    s.write(s.font_block + 8, bytes(font_image))
    s.newfont = s.alloc(8)
    s.write(s.newfont, b"newfont!")
    s.pixels = s.alloc(2048)
    s.write(s.pixels, c["pixels"])
    s.area = s.alloc(s.area_size)
    s.write(s.area, bytes([0xA5]) * s.area_size)
    s.word(s.area + s.stride_offset, c["pitch"])
    s.ptr(s.g("_lbpCurrentRA"), s.area)
    s.ptr(s.g("_lbCurrentOriginPtr"), s.pixels + 256)
    s.words(s.g("_lbCurrentClipRect"), c["clip"])
    for name, value in zip(("gLbCheapTextColour", "gLbCheapTextIndex", "gLbCheapUnicodeTextColour", "gLbCheapUnicodeTextIndex"), c["cache"]):
        s.word(s.g(name), value)
    s.write(s.g("gLbCheapTextGuard"), bytes([c["guards"][0]]))
    s.write(s.g("gLbCheapUnicodeTextGuard"), bytes([c["guards"][1]]))
    s.word(s.g("gLbTextSelectSense"), c["sense"])
    real_tables = [s.g("_lbSpriteRender"), s.g("_lbOneColourSpriteRender")]
    all_tables = [*s.tables, *real_tables]
    oldrender = 0 if c["old"] < 0 else all_tables[c["old"]]
    current = all_tables[c["render"]]
    s.ptr(s.g("gLbTextRender"), oldrender if op == 14 else current)
    s.ptr(s.g("gLbTextFont"), s.font)
    s.labels = {0: 0, s.font: 1, s.newfont: 2, s.g("gLbTextFont"): 10,
                s.area: 40, s.pixels + 256: 41, **{a: 20 + i for i, a in enumerate(all_tables)}}
    size = 0 if c["null"] and op in (4, 5, 29, 30, 31, 32) else s.size_block + 4
    font = s.newfont if op == 14 and c["newfont"] else s.font
    return [c["x"], c["y"], s.text, size, s.rect_block + 4, c["flags"], c["count"], c["newline"], c["width"], c["lineWidth"], c["ch"], current, font]


def observe(s, c, answer):
    font = []
    font_image = s.read(s.font_block + 8, 512 * s.sprite_size)
    for i in range(512):
        a = i * s.sprite_size
        pointer = int.from_bytes(font_image[a:a + s.ps], "big" if s.ps == 4 else "little")
        font.append((pointer, *struct.unpack_from(s.endian + "HH", font_image, a + s.ps)))
    text = list(struct.unpack(s.endian + ("H" if s.wide else "B") * len(s.text_units), s.read(s.text, len(s.text_units) * (2 if s.wide else 1))))
    state = {n: s.rd(s.g(n))[0] for n in ("gLbTextSelectSense", "gLbCheapTextColour", "gLbCheapTextIndex", "gLbCheapUnicodeTextColour", "gLbCheapUnicodeTextIndex")}
    state.update({n: s.read(s.g(n), 1) for n in ("gLbCheapTextGuard", "gLbCheapUnicodeTextGuard")})
    state.update({n: s.lab(s.rp(s.g(n))) for n in ("gLbTextFont", "gLbTextRender", "_lbpCurrentRA", "_lbCurrentOriginPtr")})
    return dict(answer=answer, log=s.log(), state=state, text=text, size=s.rd(s.size_block, 4), rect=s.rd(s.rect_block, 6),
                clip=s.rd(s.g("_lbCurrentClipRect"), 4), pixels=s.read(s.pixels, 2048), font=font,
                guards=[s.read(s.text_block, 4), s.read(s.font_block, 8), s.read(s.font_block + 8 + 512 * s.sprite_size, 8)],
                area=[s.read(s.area, 24), s.lab(s.rp(s.area + 24)), s.rd(s.area + 24 + s.ps, 3)], newfont=s.read(s.newfont, 8))


def run(s, c):
    try:
        a = build(s, c)
        s.begin(c)
        return observe(s, c, s.call(c, a))
    finally:
        s.done()


def ppc(e, c):
    return run(Ppc(e, c), c)


def host(lib, c):
    return run(Host(lib, c), c)


def case(r, op):
    wide = "Unicode" in FUNCTIONS[op][1] or "wchar_t" in FUNCTIONS[op][1]
    widths = [r.randrange(1, 10) for _ in range(256)]
    heights = [r.randrange(1, 17) for _ in range(256)]
    advances = [r.choice([-13, -1, 0, 2, 15, 0x7FFFFFFF, 0x80000000]) for _ in range(256)]
    sprites = [(r.randrange(1, 10), r.randrange(1, 17)) for _ in range(512)]
    text = [r.choice([32, 65, 66, 67, 97, 98, 128, 255] + ([0x141, 0x17F] if wide else [])) for _ in range(r.randrange(20))]
    if op in (6, 7, 19, 20):
        text = [r.choice([32, 10, 65, 66, 67, 97, 98, 128, 255]) for _ in range(r.randrange(25))]
    elif op in (2, 3, 21, 22):
        text = [r.choice([32, 9, 10, 11, 12, 13, 65, 66, 97, 128, 255] + ([0x120, 0x10A, 0x141] if wide else [])) for _ in range(r.randrange(25))]
    elif op in (8, 9, 17, 18):
        text = [r.randrange(32, 155 if wide else 128) for _ in range(r.randrange(15))]
    elif op in (0, 1, 4, 5, 23, 24, 31, 32):
        text += [10] if r.random() < 0.4 else []
    left, top = r.randrange(-40, 41), r.randrange(-40, 41)
    width = r.choice([9, 10, 17, 25, 50, 200])
    c = dict(op=op, widths=widths, heights=heights, advances=advances, sprites=sprites, text=text,
             colour=[r.randrange(256) for _ in range(5)], mask=3, x=r.randrange(-8, 49), y=r.randrange(-3, 13),
             size=[r.getrandbits(32), r.getrandbits(32)], rect=[left, top, left + width, top + r.randrange(-20, 81)],
             clip=[r.randrange(-4, 9), r.randrange(-2, 4), r.randrange(12, 64), r.randrange(6, 19)],
             cache=[r.getrandbits(32) for _ in range(4)], guards=[r.choice([0, 1, 128, 255]) for _ in range(2)],
             sense=r.choice([0, 1, -1, -0x80000000]), old=r.randrange(-1, 6), render=r.randrange(6), newfont=r.randrange(2),
             queues={"Select": [r.choice([-1, 0, 1, 0x7FFFFFFF, -0x80000000])], "WriteFont": [r.randrange(2)]},
             flags=r.choice([1, 2, 4, 8, 3, 15]) | r.choice([0x10, 0x20, 0x40, 0x80, 0xE0]),
             count=r.randrange(len(text) + 1), newline=r.choice([0, 1, -1]), width=width, lineWidth=r.getrandbits(32),
             ch=r.choice(list(range(0x180)) + list(range(0xFFFFFF80, 0x100000000))), null=r.randrange(2), pitch=r.choice([65, 67, 71]),
             pixels=bytes(r.randrange(256) for _ in range(2048)))
    if op in (10, 11, 12, 13):
        c["sprites"] = [(r.randrange(65536), r.randrange(65536)) for _ in range(512)]
    if op in (11, 13):
        c["queues"]["SpriteWidth"] = [r.choice([-1, 0, 1, 0xFFFF, r.randrange(65536)])]
    if op in (15, 16, 4, 5, 29, 30, 31, 32):
        c["mask"] = r.choice([0, 1, 2, 3])
        if c["render"] < 4:
            c["widths"] = [r.getrandbits(32) for _ in range(256)]
            c["heights"] = [r.getrandbits(32) for _ in range(256)]
    if op in (27, 28):
        c["count"] = r.randrange(6)
    return c


def edges(op):
    r = random.Random(224 + op)
    for text in ([], [32], [32, 32], [10], [10, 10], [65], [65, 32, 66], [65] * 15, [32, 65, 32, 32, 66, 32], [128, 255]):
        c = case(r, op)
        if op in (8, 9, 17, 18):
            text = [32 if ch < 32 or ch >= 128 else ch for ch in text]
        c["text"] = text
        c["count"] = min(c["count"], len(text)) if op in (25, 26) else c["count"]
        yield c
    if op == 14:
        for old in (-1, 0, 2, 3, 4, 5):
            for render in range(6):
                for result in (-0x80000000, -1, 0, 1, 0x7FFFFFFF):
                    for sense in (0, 1, -1):
                        c = case(r, op)
                        c.update(old=old, render=render, sense=sense, queues={"Select": [result], "WriteFont": [1]})
                        yield c
    elif op in (8, 9, 17, 18):
        for x in (3, 4, 5, 16, 17):
            for y in (1, 2, 3, 4, 5):
                for guard, index in ((0, 0), (1, 0), (1, 0x100), (255, 9)):
                    c = case(r, op)
                    c.update(x=x, y=y, clip=[4, 2, 20, 10], guards=[guard, guard], cache=[0x12345678, index, 0x87654321, index], text=[65, 66, 67], colour=[1, 2, 3, 4, 0])
                    yield c
    elif op in (2, 3, 21, 22, 27, 28):
        for flags in (1, 2, 4, 8, 15):
            for vertical in (0x10, 0x20, 0x40, 0x80):
                for text in ([], [65], [65, 32, 66, 10, 67], [65] * 12):
                    c = case(r, op)
                    c.update(flags=flags | vertical, text=text, count=3, newline=0)
                    yield c
    elif op in (10, 11, 12, 13, 15, 16):
        for ch in (0, 0x1F, 0x20, 0x7F, 0x80, 0xFF, 0x100, 0x17F, 0xFFFFFF80, 0xFFFFFFFF):
            c = case(r, op)
            c["ch"] = ch
            yield c
    elif op in (33, 34):
        for rect in ([0, 0, 0, 0], [1, 2, -1, -2], [-0x80000000, -0x80000000, 0x7FFFFFFF, 0x7FFFFFFF]):
            c = case(r, op)
            c["rect"] = rect
            yield c
    elif op in (25, 26):
        c = case(r, op)
        c.update(text=[65, 0, 66], count=3)
        yield c


TESTS = [FnTest(name, ppc, host, edge=lambda op=op: edges(op), rnd=lambda r, op=op: case(r, op))
         for op, (_, name, _) in enumerate(FUNCTIONS)]
