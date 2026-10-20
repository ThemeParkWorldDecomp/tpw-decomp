"""Sam's Utilities' pixel converter (platform/upixels.cpp) against the original
(sams utils shared): whole conversions between the five formats the image loader uses,
with the buffers compared byte for byte."""
import ctypes as C
import random
import struct
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sams utils symbols shared"
SHIM_LIBRARY = "unit_UPixels"

CT = "__ct__Q213SamsUtilities7UPixelsFQ213SamsUtilities11PixelFormatPvUlUlUlPQ213SamsUtilities6UPixel"
DT = "__dt__Q213SamsUtilities7UPixelsFv"
CONVERT = "Convert__Q213SamsUtilities7UPixelsFRCQ213SamsUtilities7UPixelsUc"
SET_DEFAULT = "SetAlphaDefault__Q213SamsUtilities7UPixelsFUc"
SET_THRESHOLD = "SetAlphaThreshold__Q213SamsUtilities7UPixelsFUc"
SET_FLIPPED = "SetFlipped__Q213SamsUtilities7UPixelsFUc"
SET_TRANSPARENT = "SetTransparentColour__Q213SamsUtilities7UPixelsFQ213SamsUtilities6UPixel"

# format number -> bytes per pixel, for the formats translated so far
BYTES = {4: 2, 5: 2, 8: 3, 10: 4, 11: 4}
GUARD = 0xA5


def setup(runner):
    T.install_std_stubs(runner.emu)


def layout(c):
    """Buffer sizes for a case: rows of `pitch` bytes (the last one too) plus a guard."""
    sp = c["spitch"] if c["spitch"] >= 0 else c["w"] * BYTES[c["src"]]
    dp = c["dpitch"] if c["dpitch"] >= 0 else c["dw"] * BYTES[c["dst"]]
    return sp, dp, sp * c["h"], dp * c["dh"]


def source_bytes(c):
    return bytes(c["pixels"][:layout(c)[2]])


def swap_words(buf, pitch, rows, width):
    """The buffer with the 16-bit words of each row's first `width` pixels swapped in pairs.

    Format 4 (16-bit RGBA) is the game's texel format, which the translation keeps as
    host-order words (the wavelet decoders and the OpenGL download read it so) where the
    original wrote big-endian bytes; the original's buffers are swapped to compare."""
    out = bytearray(buf)
    for row in range(rows):
        start = row * pitch
        for i in range(start, min(start + width * 2, len(out) - 1), 2):
            out[i], out[i + 1] = out[i + 1], out[i]
    return bytes(out)


def ppc(e, c):
    sp, dp, ssize, dsize = layout(c)
    source = e.alloc(ssize + 16, source_bytes(c) + bytes([GUARD]) * 16)
    dest = e.alloc(dsize + 16, bytes([GUARD]) * (dsize + 16))
    src = e.alloc(12, b"\0" * 12)
    dst = e.alloc(12, b"\0" * 12)
    T.call_ppc(e, e.func(CT), [src, c["src"], source, c["w"], c["h"], T.u32(c["spitch"]), 0])
    T.call_ppc(e, e.func(CT), [dst, c["dst"], dest, c["dw"], c["dh"], T.u32(c["dpitch"]), 0])
    T.call_ppc(e, e.func(SET_DEFAULT), [src, c["default"]])
    T.call_ppc(e, e.func(SET_THRESHOLD), [dst, c["threshold"]])
    if c["flags"] & 1:
        T.call_ppc(e, e.func(SET_FLIPPED), [src, 1])
    if c["flags"] & 2:
        T.call_ppc(e, e.func(SET_TRANSPARENT), [src, c["transparent"]])
    T.call_ppc(e, e.func(CONVERT), [dst, src, 1])
    alpha, red, green, blue = e.read(dst + 8, 4)
    dest_bytes = e.read(dest, dsize + 16)
    if c["dst"] == 4:
        dest_bytes = swap_words(dest_bytes, dp, c["h"], c["w"])
    out = dict(dest=dest_bytes, source=e.read(source, ssize + 16), count=T.s32(e.u32(dst + 4)),
               average=(red, green, blue, alpha))
    T.call_ppc(e, e.func(DT), [dst, 0])
    T.call_ppc(e, e.func(DT), [src, 0])
    return out


def host(lib, c):
    sp, dp, ssize, dsize = layout(c)
    source_in = source_bytes(c)
    if c["src"] == 4:   # the host reads host words: give it the original's words in its own order
        source_in = swap_words(source_in, sp, c["h"], c["w"])
    source = C.create_string_buffer(source_in + bytes([GUARD]) * 16, ssize + 16)
    dest = C.create_string_buffer(bytes([GUARD]) * (dsize + 16), dsize + 16)
    out = (C.c_int32 * 6)()
    f = lib["tpw_upixels_convert"]
    f.restype = None
    f.argtypes = [C.c_int32, C.c_int32, C.c_void_p, C.c_void_p] + [C.c_int32] * 6 + [C.c_uint32] * 4 + [C.c_void_p]
    f(c["src"], c["dst"], source, dest, c["w"], c["h"], c["dw"], c["dh"], c["spitch"], c["dpitch"],
      c["default"], c["threshold"], c["flags"] | 4, c["transparent"], out)
    assert out[5] == 1, "a format of this case is not translated"
    source_out = source.raw
    if c["src"] == 4:
        source_out = swap_words(source_out, sp, c["h"], c["w"])
    return dict(dest=dest.raw, source=source_out, count=out[0], average=(out[1], out[2], out[3], out[4]))


def case(r, src=None, dst=None):
    src = src if src is not None else r.choice(list(BYTES))
    dst = dst if dst is not None else r.choice(list(BYTES))
    w, h = r.randint(1, 9), r.randint(1, 7)
    # The destination may be larger than the source (only the source's area is written).
    dw, dh = w + r.choice([0, 0, 0, 1, 3]), h + r.choice([0, 0, 0, 1, 2])
    spitch = r.choice([-1, -1, w * BYTES[src] + r.randint(1, 5)])
    dpitch = r.choice([-1, -1, dw * BYTES[dst] + r.randint(1, 5)])
    pixels = list(r.randbytes(16 * 16 * 4))
    flags = r.choice([0, 0, 0, 1, 2, 3])
    # A transparent colour that some pixel has (24-bit sources compare it with alpha 0).
    transparent = pixels[0] | pixels[1] << 8 | pixels[2] << 16 | r.choice([0, 0, 0, 0xff]) << 24
    if r.random() < 0.3:   # runs of the transparent colour and of extreme alphas
        for i in range(0, len(pixels) - 4, r.choice([3, 4, 8])):
            pixels[i:i + 4] = r.choice([pixels[0:4], [0, 0, 0, 0], [255, 255, 255, 255]])
    return dict(src=src, dst=dst, w=w, h=h, dw=dw, dh=dh, spitch=spitch, dpitch=dpitch, pixels=pixels,
                default=r.choice([255, 255, 0, 128, r.randrange(256)]),
                threshold=r.choice([240, 240, 0, 255, 127, r.randrange(256)]), flags=flags,
                transparent=transparent)


def edges():
    r = random.Random(4)
    cases = []
    # Every pair of formats, the game's own settings (default alpha 255, threshold 240).
    for src in BYTES:
        for dst in BYTES:
            c = case(r, src, dst)
            c.update(default=255, threshold=240, flags=0, spitch=-1, dpitch=-1, dw=c["w"], dh=c["h"])
            cases.append(c)
    # One pixel; a flipped source with an explicit pitch; alphas exactly at the threshold.
    for src, dst in ((11, 10), (8, 4), (5, 10), (11, 4)):
        c = case(r, src, dst)
        c.update(w=1, h=1, dw=1, dh=1, spitch=-1, dpitch=-1)
        cases.append(c)
        c = case(r, src, dst)
        c.update(flags=1, spitch=c["w"] * BYTES[src] + 3)
        cases.append(c)
        c = case(r, src, dst)
        c.update(threshold=c["pixels"][3], default=c["pixels"][3])
        cases.append(c)
        # A destination narrower or shorter than the source: nothing is converted.
        c = case(r, src, dst)
        c.update(w=4, h=3, dw=3, dh=3, spitch=-1, dpitch=-1)
        cases.append(c)
        c = case(r, src, dst)
        c.update(w=4, h=3, dw=4, dh=2, spitch=-1, dpitch=-1)
        cases.append(c)
    return cases


TESTS = [FnTest("UPixels::Convert (" + name + ")", ppc, host, edge=edges if i == 0 else None,
                rnd=lambda r, pair=pair: case(r, *pair))
         for i, (name, pair) in enumerate((("any formats", (None, None)), ("32-bit TGA to RGBA", (11, 10)),
                                           ("24-bit TGA to RGBA", (8, 10)), ("32-bit TGA to RGBA4444", (11, 4)),
                                           ("24-bit TGA to RGBA4444", (8, 4)), ("16-bit TGA to RGBA", (5, 10))))]
