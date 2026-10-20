"""w82 SignRenderer shell, all five functions in the main app.

Host Bitmap storage and strings are real. PPC engine imports model the already
tested storage methods because the engine image overlaps the main image. Other
callees are logged fakes on both sides. Files contain identical little-endian
bytes; font/shader load fakes consume none. Shader destruction models mandatory
C++ Bitmap member cleanup. Full object fields, bitmap bytes, textures and call
order compare. NULL-this and impossible member-address guards are PPC-only.
"""
import ctypes as C
import itertools
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RideSignRenderer"
PROGRAM = "SimTheme Park"
CT, DT, LOAD, INSTALL, RENDER = 0x100CB594, 0x100CB82C, 0x100CB934, 0x100CBD68, 0x100CBEE4
BITMAPS = [0x18, 0x28, 0x384, 0x394, 0x3A4, 0x3B4, 0x3F0, 0x400, 0x410, 0x420]
MASK = 0xFFFFFFFF


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def rs(e, a, n):
    return list(struct.unpack(">" + "I" * n, e.read(a, 4 * n)))


def label_bm(e, a):
    if e.cl.lab(a) == -1:
        e.cl.newlab(a)
    return e.cl.lab(a)


def record_bm(e, name, a):
    w, h, bpp, data = rs(e, a, 4)
    e.cl.rec(name, [label_bm(e, a), w, h, bpp, e.cl.lab(data)])


def alloc(e, size):
    a = e.alloc(max(size, 1))
    e.cl.rec("new[]", [size, e.cl.newlab(a)])
    return a


def delete(e, a):
    if a and e.cl.lab(a) > 0:
        e.cl.rec("delete", [e.cl.lab(a)])
        e.cl.forget(a)


def read(e, dest, size, count, file):
    pos = e.srr_pos
    e.cl.rec("Read", [size, count, e.cl.lab(file), pos])
    data = e.srr_bytes[pos:pos + size * count]
    if data:
        e.write(dest, data)
    e.srr_pos += len(data)
    return len(data) // size if size else 0


def build_stubs():
    s = CL.Stubs()
    s.real = {CT, DT, LOAD, INSTALL, RENDER, 0x1023BAA8}
    r = CL.recorder

    def font_ctor(e):
        e.cl.rec("Font.ctor", [e.cl.lab(e.r(3))])
        record_bm(e, "Ctor.background", e.srr_obj + 0x18)
        record_bm(e, "Ctor.output", e.srr_obj + 0x28)
        e.write(e.r(3), b"\x7b" * 0x190)

    def shader_dtor(e):
        a = e.r(3)
        e.cl.rec("Shader.dtor", [e.cl.lab(a), int(T.s16(e.r(4)) <= 0)])
        for offset in (0x5C, 0x4C, 0x3C, 0x2C):
            delete(e, e.u32(a + offset + 12))
            ws(e, a + offset, [0, 0, 0, 0])

    def font(e):
        e.cl.rec("Font.render", [e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), label_bm(e, e.r(6))],
                 [CL.wide_text(e, e.r(5))])
        text = e.r(5)
        i = 0
        while True:
            word = struct.unpack(">H", e.read(text + i * 2, 2))[0]
            if not word:
                break
            e.cl.rec("Font.text", [word])
            i += 1
        record_bm(e, "Font.bitmap", e.r(6))
        e.set_r(3, e.cl.ret("Font.render", 0))

    def glow(e):
        e.cl.rec("Glow", [e.cl.lab(e.r(3)), label_bm(e, e.r(4)), label_bm(e, e.r(5))])
        record_bm(e, "Glow.mask", e.r(4))
        record_bm(e, "Glow.output", e.r(5))

    def bevel(e):
        e.cl.rec("Bevel", [e.cl.lab(e.r(3)), label_bm(e, e.r(4)), e.r(5), label_bm(e, e.r(6))])
        record_bm(e, "Bevel.mask", e.r(4))
        record_bm(e, "Bevel.output", e.r(6))

    def blur(e):
        e.cl.rec("Blur", [e.cl.lab(e.r(3)), label_bm(e, e.r(4)), label_bm(e, e.r(5)), CL.fbits(e.f(1))])
        record_bm(e, "Blur.bevel", e.r(4))
        record_bm(e, "Blur.mask", e.r(5))

    def shade(e):
        e.cl.rec("Shade", [e.cl.lab(e.r(3)), label_bm(e, e.r(4))])
        record_bm(e, "Shade.bitmap", e.r(4))

    def fill(e):
        a = e.r(3)
        e.cl.rec("Fill", [label_bm(e, a)] + [e.r(i) & 255 for i in range(4, 8)])
        record_bm(e, "Fill.bitmap", a)
        if a == e.srr_obj + 0x28:
            w, h, bpp, data = rs(e, a, 4)
            seed = e.cl.ret("PixelSeed", 0)
            e.write(data, bytes((i * 37 + seed) & 255 for i in range(w * h * bpp)))

    def pair(name):
        def fake(e):
            a, b = e.r(3), e.r(4)
            ints = [label_bm(e, a), label_bm(e, b)]
            if name == "Alpha":
                ints += [e.r(5), e.r(6)]
            if name == "Colour":
                ints += [e.r(i) & 255 for i in range(5, 9)]
            e.cl.rec(name, ints)
            record_bm(e, name + ".source", a)
            record_bm(e, name + ".dest", b)
        return fake

    def swizzle(e):
        e.cl.rec("Swizzle", [label_bm(e, e.r(3))])
        record_bm(e, "Swizzle.bitmap", e.r(3))

    def clear(e):
        a = e.r(3)
        delete(e, e.u32(a + 12))
        ws(e, a, [0, 0, 0, 0])

    def realloc(e):
        a, w, h, bpp = [e.r(i) for i in range(3, 7)]
        oldw, oldh, oldbpp, data = rs(e, a, 4)
        size = (w * h * bpp) & MASK
        if (w, h, bpp) != (oldw, oldh, oldbpp) or not data:
            delete(e, data)
            data = alloc(e, size)
            ws(e, a, [w, h, bpp, data])
        e.write(data, bytes(size))
        e.set_r(3, 1)

    def bitmap_load(e):
        a, file = e.r(3), e.r(4)
        temp = e.alloc(12)
        for i in range(3):
            read(e, temp + i * 4, 4, 1, file)
        w, h, bpp = struct.unpack("<III", e.read(temp, 12))
        oldw, oldh, oldbpp, data = rs(e, a, 4)
        size = w * h * bpp
        if (w, h, bpp) != (oldw, oldh, oldbpp) or not data:
            delete(e, data)
            data = alloc(e, size)
            ws(e, a, [w, h, bpp, data])
        e.write(data, bytes(size))
        read(e, data, size, 1, file)
        e.set_r(3, 1)

    def wavelet_load(e):
        e.cl.rec("Wavelet.load", [label_bm(e, e.r(3)), e.cl.lab(e.r(4)), e.cl.lab(e.r(5))])
        record_bm(e, "Wavelet.bitmap", e.r(3))
        e.set_r(3, e.cl.ret("Wavelet.load", 0))

    def open_file(e):
        e.cl.rec("Open", [], [e.cstr(e.r(3)), e.cstr(e.r(4))])
        e.set_r(3, e.srr_file if e.srr_d[26] else 0)

    def swap(e):
        for i in range(e.r(4)):
            a = e.r(3) + i * 4
            e.write(a, e.read(a, 4)[::-1])

    def instance(e):
        e.cl.rec("Instance")
        e.set_r(3, e.srr_wavelet)

    s.internal(0x100C9C90, font_ctor)
    s.internal(0x100CA558, shader_dtor)
    s.internal(0x100CA340, r("Font.load", "LL", ret="i"))
    s.internal(0x100CA724, r("Shader.load", "LL", ret="i"))
    s.internal(0x100C9DC0, font)
    s.internal(0x100CB468, glow)
    s.internal(0x100CA95C, bevel)
    s.internal(0x100CAB6C, blur)
    s.internal(0x100CAF40, shade)
    s.internal(0x10003FD4, open_file)
    s.internal(0x10004260, lambda e: e.set_r(3, read(e, e.r(3), e.r(4), e.r(5), e.r(6))))
    s.imp("Instance__8CWaveletFv", instance)
    s.imp("clear__6BitmapFv", clear)
    s.imp("realloc__6BitmapFUiUiUi", realloc)
    s.imp("load__6BitmapFPv", bitmap_load)
    s.imp("load_wavelet__6BitmapFPvP8CWavelet", wavelet_load)
    s.imp("colour_fill__6BitmapFUcUcUcUc", fill)
    s.imp("copy_to__6BitmapFR6Bitmap", pair("Copy"))
    s.imp("shadowblend__6BitmapFR6Bitmap", pair("Blend"))
    s.imp("alphablt__6BitmapFR6BitmapUlUl", pair("Alpha"))
    s.imp("colourblt__6BitmapFR6BitmapUcUcUcUc", pair("Colour"))
    s.imp("swizzle_for_gimex__6BitmapFv", swizzle)
    s.imp("_USwapBlock32__13SamsUtilitiesFPUll", swap)
    s.imp("__nwa__FUl", lambda e: e.set_r(3, alloc(e, e.r(3))))
    for name in ("__dla__FPv", "__dl__FPv"):
        s.imp(name, lambda e: delete(e, e.r(3)))
    s.imp("AddFontResource__10NS_MacDozeFPCc", r("AddFont", "s", ret="i"))
    s.imp("strcmp", lambda e: e.set_r(3, int(e.cstr(e.r(3)) != e.cstr(e.r(4)))))
    for name, fake in CL.string_stubs().items():
        s.imp(name, fake)
    s.imp("__as__26TbDynamicStringTemplate<c>FPCc",
          lambda e: CL.tds_set(e, e.r(3), e.cstr(e.r(4))))
    s.imp("__apl__26TbDynamicStringTemplate<c>FPCc",
          lambda e: CL.tds_set(e, e.r(3), CL.tds_text(e, e.r(3)) + e.cstr(e.r(4))))
    return s


STUBS = build_stubs()


def setup(runner):
    T.install_std_stubs(runner.emu)
    e = runner.emu
    e.srr_release = T.fake_function(e, CL.recorder("Release", "L"))
    e.srr_vtable = T.persist_alloc(e, 0x40)
    ws(e, e.srr_vtable + 0x1C, [e.srr_release])


def dump(e, a):
    result = rs(e, a, 2) + list(e.read(a + 8, 4)) + rs(e, a + 12, 2) + [e.cl.lab(e.u32(a + 20))]
    for offset in (0x38, 0x1C8):
        result += rs(e, a + offset, 2) + list(e.read(a + offset + 8, 0x188))
    for offset in (0x358, 0x3C4):
        result += rs(e, a + offset, 11)
    for offset in (0x430, 0x444):
        result += list(e.read(a + offset, 4)) + rs(e, a + offset + 4, 4)
    for offset in BITMAPS:
        w, h, bpp, data = rs(e, a + offset, 4)
        result += [w, h, bpp, e.cl.lab(data)]
        if data:
            result += list(e.read(data, w * h * bpp))
    return result


def ppc(e, c):
    d, op = c["d"], c["op"]
    a = e.alloc(0x458, bytes([d[0]]) * 0x458)
    wavelet = e.alloc(4)
    ws(e, wavelet, [e.srr_vtable])
    file = e.alloc(4)
    dc = e.alloc(4)
    labels = {a: 1, a + 0x38: 2, a + 0x1C8: 3, a + 0x358: 4, a + 0x3C4: 5,
              a + 0x430: 6, a + 0x444: 7, wavelet: 8, file: 9, dc: 10}
    labels.update({a + offset: 20 + i for i, offset in enumerate(BITMAPS)})
    e.cl = CL.PpcLog(labels, c["q"])
    e.srr_obj, e.srr_wavelet, e.srr_file = a, wavelet, file
    e.srr_d, e.srr_pos, e.srr_bytes = d, 0, c["bytes"]
    if op != 0:
        for i, offset in enumerate(BITMAPS):
            ws(e, a + offset, [0, 0, 0, 0])
            if (i == 3 and d[9]) or (i == 7 and d[10]) or ((d[28] >> i) & 1):
                w, h, bpp = d[5] * 2, d[6] * 2, 4 if i in (1, 5, 9) else 1
                data = e.alloc(max(w * h * bpp, 1), b"\x5a" * (w * h * bpp))
                ws(e, a + offset, [w, h, bpp, data])
                e.cl.labels[data] = 50 + i
        ws(e, a, [d[27], d[1]])
        e.write(a + 8, bytes([d[2]]))
        ws(e, a + 12, [d[3], d[4], wavelet])
        ws(e, a + 0x360, [d[11]])
        ws(e, a + 0x3CC, [d[12]])
        ws(e, a + 0x38, d[13:15])
        ws(e, a + 0x1C8, d[15:17])
        e.write(a + 0x430, bytes(d[17:21]))
        e.write(a + 0x444, bytes(d[21:25]))
        if op == 3:
            e.write(a + 0xC4, c["name1"].encode() + b"\0")
            e.write(a + 0x254, c["name2"].encode() + b"\0")
    texture1 = e.alloc(1024, b"\xa5" * 1024)
    texture2 = e.alloc(1024, b"\xa5" * 1024)
    text1, text2 = CL.new_wide(e, "First \u00e9"), CL.new_wide(e, "Second \u03b1")
    pointers = [text1, text2, texture1, texture2]
    pointers = [0 if d[8] & (1 << i) else ptr for i, ptr in enumerate(pointers)]
    options = e.alloc(28)
    ws(e, options, pointers + d[5:8])
    path = CL.new_text(e, c["path"])
    # The original uses gTPGlobals' real separator object at +0x404c.
    CL.tds_set(e, 0x103B74C8 + 0x404C, c["separator"])
    saved = STUBS.activate(e)
    try:
        if op == 0:
            result = int(T.call_ppc(e, CT, [a])[0] == a)
        elif op == 1:
            result = int(T.call_ppc(e, DT, [a, d[25] & MASK])[0] == a)
        elif op in (2, 3):
            result = T.call_ppc(e, LOAD if op == 2 else INSTALL, [a, path])[0]
        else:
            result = T.call_ppc(e, RENDER, [a, dc, options])[0]
        textures = list(e.read(texture1, 1024)) + list(e.read(texture2, 1024))
        if op == 4 and result and d[7] == 2:
            # Compare the native 16-bit words as bytes in host order.
            count = d[5] * d[6] * 2
            for start in (0, 1024):
                for i in range(start, start + count, 2):
                    textures[i], textures[i + 1] = textures[i + 1], textures[i]
        return dict(state=[result, e.srr_pos] + dump(e, a) + textures, log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, c):
    CL.host_begin(lib, {}, c["q"])
    fn = lib.tpw_srr_run
    fn.restype = C.c_int32
    fn.argtypes = [C.c_int32, C.POINTER(C.c_int64)] + [C.c_char_p] * 4 + [C.c_char_p, C.c_uint32, C.POINTER(C.c_int64)]
    d = (C.c_int64 * len(c["d"]))(*c["d"])
    out = (C.c_int64 * 16384)()
    n = fn(c["op"], d, c["path"].encode(), c["name1"].encode(), c["name2"].encode(), c["separator"].encode(),
           c["bytes"], len(c["bytes"]), out)
    return dict(state=list(out[:n]), log=CL.host_end(lib))


def case(op, **kw):
    d = [0xA5, 0, 0, 0, 0, 2, 3, 1, 0, 1, 1, 0x3F800000, 0x3F800000,
         4, 17, -3, 13, 11, 22, 33, 44, 55, 66, 77, 88, 0, 1, 101, 0]
    indexes = dict(fill=0, order=1, background=2, mode1=3, mode2=4, w=5, h=6, quality=7, nulls=8,
                   file1=9, file2=10, blur1=11, blur2=12, flag=25, opened=26, version=27, reuse=28)
    for name, i in indexes.items():
        if name in kw:
            d[i] = kw.pop(name)
    c = dict(op=op, d=d, path="Fonts:Signs", name1="font.ttf", name2="font.ttf", separator=":",
             q={"AddFont": [1], "PixelSeed": [0], "Font.load": [0], "Shader.load": [0]})
    c.update(kw)
    if "bytes" not in c:
        blob = struct.pack("<iiBii", d[27], d[1], d[2], d[3], d[4])
        for mode in d[3:5]:
            if mode:
                blob += bytes([1, 128, 255, 13]) + struct.pack("<IIII", 0x12345678, 0xBF800000, 0x80000000, 0xFFFFFFFF)
        for seed in (19, 83, 111):
            blob += struct.pack("<III", 2, 3, 1) + bytes((seed + i * 31) & 255 for i in range(6))
        c["bytes"] = blob
    return c


def edges(op):
    if op == 0:
        for fill in (0, 1, 0x55, 0x80, 0xA5, 0xFF):
            yield case(op, fill=fill)
    elif op == 1:
        for flag, reuse in itertools.product((0, 1, -1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001),
                                             (0, 3, 0x3FF, 0x155, 0x2AA)):
            yield case(op, flag=flag, reuse=reuse)
    elif op == 2:
        for version, background, modes in itertools.product((-1, 0, 99, 100, 101, 0x7FFFFFFF, -0x80000000),
                                                            (0, 1, 128, 255), ((0, 0), (1, 0), (0, 2), (2, 2), (-1, 3))):
            yield case(op, version=version, background=background, mode1=modes[0], mode2=modes[1])
        yield case(op, opened=0)
        # A truncated pixel tail is harmless: load still answers 1 and the buffer's remaining bytes stay zero.
        c = case(op, version=100, background=1)
        c["bytes"] = c["bytes"][:-3]
        yield c
        for reuse in (0, 1 << 3, 1 << 7, (1 << 3) | (1 << 7)):
            yield case(op, reuse=reuse, w=1, h=1, file1=0, file2=0)
        # The stream is the header, the glow records and zeros, so a read that is dropped, widened or taken
        # for the wrong mode shifts every later Read event but does not turn the bitmap sizes into huge
        # allocations (the bitmaps after the records read as empty ones). The second record starts with
        # four zero bytes, which a skipped read would take for an empty bitmap's width.
        glow1 = bytes([0x11, 0x22, 0x33, 0x44]) + struct.pack("<IIII", 0x12345678, 0xBF800000, 0x80000000, 7)
        glow2 = bytes([0, 0, 0, 0]) + struct.pack("<IIII", 1, 2, 0, 0)
        for version, background, modes, tail in itertools.product((100, 101), (0, 1), ((1, 2), (0, 2), (2, 0), (1, 0), (0, 1)),
                                                                  (bytes(40),)):
            blob = struct.pack("<iiBii", version, 1, background, modes[0], modes[1])
            blob += glow1 if modes[0] else b""
            blob += glow2 if modes[1] else b""
            yield case(op, version=version, background=background, mode1=modes[0], mode2=modes[1], bytes=blob + tail)
        for version in (100, 101):
            c = case(op, version=version, background=1, w=2, h=3, reuse=0x3FF)
            c["bytes"] = c["bytes"][:17]
            for bpp in (1, 1, 1):
                c["bytes"] += struct.pack("<III", 4, 6, bpp) + bytes(range(24 * bpp))
            yield c
    elif op == 3:
        for names, answers, separator in itertools.product((("", ""), ("A", "A"), ("A", "a"), ("A", "B")),
                                                          ((0,), (1,), (1, 0), (-1, 1)), (":", "/", "::")):
            yield case(op, name1=names[0], name2=names[1], separator=separator, q={"AddFont": list(answers)})
    else:
        for nulls in range(16):
            yield case(op, nulls=nulls)
        for modes, order, background, files, quality in itertools.product(itertools.product((0, 1, 2, 3, -1), repeat=2),
                (0, 1), (0, 1), ((0, 0), (1, 0), (0, 1), (1, 1)), (0, 1, 2, 3)):
            yield case(op, mode1=modes[0], mode2=modes[1], order=order, background=background,
                       file1=files[0], file2=files[1], quality=quality)
        for bits in (0, 0x80000000, 1, 0x80000001, 0xBF800000, 0x7F800000, 0xFF800000, 0x7FC00000,
                     0x7F800001, 0xFFBFFFFF, 0xFFC00000, 0x3F800000):
            yield case(op, blur1=bits, blur2=bits)
        for quality, w, h in itertools.product((1, 2, -1), (0, 1, 3), (0, 1, 4)):
            yield case(op, quality=quality, w=w, h=h)
        for seed in range(256):
            yield case(op, quality=2, q={"PixelSeed": [seed]})
        for reuse in (0, 0x3FF, 0x155, 0x2AA):
            yield case(op, reuse=reuse)


def random_case(r, op):
    c = case(op, fill=r.randrange(256), order=r.choice((0, 1, -1, 0x7FFFFFFF)), background=r.randrange(256),
             mode1=r.choice((0, 1, 2, 3, -1)), mode2=r.choice((0, 1, 2, 3, -1)), w=r.randrange(1, 9), h=r.randrange(1, 9),
             quality=r.choice((0, 1, 2, 3)), nulls=r.choice((0, 0, 0, 1, 2, 4, 8)), file1=r.randrange(2), file2=r.randrange(2),
             blur1=r.choice((0, 0x3F800000, 0xBF800000, 0x7FC00000)), blur2=r.choice((0, 0x3F800000, 0x80000000)),
             flag=r.getrandbits(32), opened=r.choice((0, 1, 1, 1)), version=r.choice((99, 100, 101, -1, 0x7FFFFFFF)),
             reuse=r.getrandbits(10), name1="font" + str(r.randrange(10)), name2="font" + str(r.randrange(10)),
             path="Dir" * r.randrange(30), separator=r.choice((":", "/", "::")),
             q={"AddFont": [r.choice((0, 1, -1)), r.choice((0, 1, -1))], "PixelSeed": [r.randrange(256)],
                "Font.load": [r.getrandbits(32)], "Shader.load": [r.getrandbits(32)], "Wavelet.load": [r.getrandbits(32)]})
    c["d"][13:17] = [T.s32(r.getrandbits(32)) for _ in range(4)]
    c["d"][17:25] = [r.randrange(256) for _ in range(8)]
    return c


TESTS = [FnTest(name, ppc, host, edge=lambda op=op: edges(op), rnd=lambda r, op=op: random_case(r, op))
         for op, name in enumerate(("SignRenderer::SignRenderer", "SignRenderer::~SignRenderer",
                                    "SignRenderer::sign_load_settings", "SignRenderer::sign_install_fonts", "SignRenderer::sign_render"))]
