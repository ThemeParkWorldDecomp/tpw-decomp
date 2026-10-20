// ltms TbFontRender.cpp, the pixel writers of TbTexturedNoBlend4BitFontRenderMethod (unit w45c; ltms symbols shared
// 0x1000baf4..0x1000cf98): they draw a glyph through a 32-bit texture (a8r8g8b8, held by the render parameters, tiled over the glyph's
// rectangle from the texture origin) WITHOUT reading the destination pixel. A pixel is written where the glyph's nybble is not zero
// (the 555 writer: where its bit 0 is set), the texel converted to the destination format; the 565 writer is the exception, it reads
// no nybbles and fills the whole source rectangle. A texture whose six format words differ from TbPixFormat32 (bullfrog 0x10073e88)
// makes every writer answer -1 before it draws; else they answer 0. The generic one answers -1.
// Quirks kept (see the comments): the 555 and 565 writers pack 5-5-5 (the 565 one is not 5-6-5); the 555 writer's alpha bit is bit 12
// of the pixel (0x1000), not bit 15; the 555 writer tests the nybble's bit 0 where the 4444, 8888 and BGRA writers test for non-zero.
#include "TbFontRender.h"

// 0x1000baf4. A pixel is written where the nybble's lowest bit is set: the texel packed 5-5-5 (red >> 9 & 0x7c00, green >> 6 & 0x3e0,
// blue >> 3 & 0x1f) with that bit moved to bit 12 (0x1000) of the pixel.
TbError TbTexturedNoBlend4BitFontRenderMethod::DrawCharTo555(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    if (!TextureIsA8R8G8B8(TextureOf(*parameters.mParams))) return TbError(-1);
    WalkTexturedGlyph<2, true>(area, parameters, [](uint8_t* pixel, uint32_t texel, uint8_t nybble) {
        if ((nybble & 1) != 0) {
            StorePixel16(pixel, static_cast<uint16_t>(((nybble & 1u) << 12) | ((texel >> 3) & 0x1f) | ((texel >> 6) & 0x3e0) | ((texel >> 9) & 0x7c00)));
        }
    });
    return TbError(0);
}

// 0x1000bf28. Every pixel of the source rectangle is set to the texel packed 5-5-5 (not 5-6-5: green >> 6 & 0x3e0, the sixth bit is
// not used); the glyph's nybbles are not read, the pixel has no alpha bit. The original unrolls the loop by four; the result is the
// same as a plain one.
TbError TbTexturedNoBlend4BitFontRenderMethod::DrawCharTo565(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    if (!TextureIsA8R8G8B8(TextureOf(*parameters.mParams))) return TbError(-1);
    WalkTexturedGlyph<2, false>(area, parameters, [](uint8_t* pixel, uint32_t texel, uint8_t) {
        StorePixel16(pixel, static_cast<uint16_t>(((texel >> 9) & 0x7c00) | ((texel >> 3) & 0x1f) | ((texel >> 6) & 0x3e0)));
    });
    return TbError(0);
}

// 0x1000c2e0. A pixel is written where the nybble is not zero: the texel packed 4-4-4 (red >> 12 & 0xf00, green >> 8 & 0xf0,
// blue >> 4 & 0xf) with the nybble itself as the top nybble (alpha); the texel's own alpha is dropped.
TbError TbTexturedNoBlend4BitFontRenderMethod::DrawCharTo4444(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    if (!TextureIsA8R8G8B8(TextureOf(*parameters.mParams))) return TbError(-1);
    WalkTexturedGlyph<2, true>(area, parameters, [](uint8_t* pixel, uint32_t texel, uint8_t nybble) {
        if ((nybble & 0xf) != 0) {
            StorePixel16(pixel, static_cast<uint16_t>(((texel >> 12) & 0xf00) | ((texel >> 8) & 0xf0) | (static_cast<uint32_t>(nybble) << 12) | ((texel >> 4) & 0xf)));
        }
    });
    return TbError(0);
}

// 0x1000cf98. The generic format has no writer here.
TbError TbTexturedNoBlend4BitFontRenderMethod::DrawCharToAny(TbRenderArea&, const C4BitFontRenderMethodParameters&) {
    return TbError(-1);
}

// 0x1000c718. A pixel is written where the nybble is not zero: the texel's colour bytes unchanged (texel & 0xffffff) with the alpha
// byte nybble * 17 (the nybble in both halves of the byte); the texel's own alpha is dropped.
TbError TbTexturedNoBlend4BitFontRenderMethod::DrawCharTo8888(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    if (!TextureIsA8R8G8B8(TextureOf(*parameters.mParams))) return TbError(-1);
    WalkTexturedGlyph<4, true>(area, parameters, [](uint8_t* pixel, uint32_t texel, uint8_t nybble) {
        const uint32_t alpha = static_cast<uint32_t>(nybble) << 24;
        if (alpha != 0) {
            StorePixel32(pixel, alpha | (alpha << 4) | (texel & 0xff) | (texel & 0xff00) | (texel & 0xff0000));
        }
    });
    return TbError(0);
}

// 0x1000cb58. A pixel is written where the nybble is not zero: the texel's bytes reversed to blue, red, green order (blue in the top
// byte, red below it, green below that) with the alpha byte nybble * 17 in the low byte; the texel's own alpha is dropped.
TbError TbTexturedNoBlend4BitFontRenderMethod::DrawCharToBGRA(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    if (!TextureIsA8R8G8B8(TextureOf(*parameters.mParams))) return TbError(-1);
    WalkTexturedGlyph<4, true>(area, parameters, [](uint8_t* pixel, uint32_t texel, uint8_t nybble) {
        const uint32_t alpha = nybble;
        if (alpha != 0) {
            StorePixel32(pixel, (texel << 24) | alpha | (alpha << 4) | ((texel << 8) & 0xff0000) | ((texel >> 8) & 0xff00));
        }
    });
    return TbError(0);
}
