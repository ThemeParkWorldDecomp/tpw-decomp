// ltms TbFontRender.cpp, the pixel writers of TbTextured4BitFontRenderMethod (unit w45c; ltms symbols shared 0x1000a1dc..0x1000b944):
// they draw a glyph through a 32-bit texture (a8r8g8b8, held by the render parameters, tiled over the glyph's rectangle from the
// texture origin), BLENDING each texel's channels into the destination pixel with the glyph's nybble as the weight n: every channel
// becomes dest + n * (texel - dest) / 15. The destination's alpha (or its low byte, in BGRA) is kept as the listings keep it, and every
// pixel of the rectangle is written back, also for the weight 0. A texture whose six format words differ from TbPixFormat32 (bullfrog
// 0x10073e88) makes every writer answer -1 before it draws; else they answer 0. The generic one answers -1.
// Quirks kept (see the comments): the 555 and 565 writers divide the SIGNED difference (truncating towards zero), the 4444, 8888 and
// BGRA ones divide it as an UNSIGNED number (mulhwu), so a channel that must get darker gets a huge quotient; the BGRA writer ORs the
// whole destination into its top byte and writes a zero low byte.
#include "TbFontRender.h"

// 0x1000a1dc. 5-5-5: the destination's bit 15 is kept; the channels are worked in place at 8, 5 and 5 bits of weight (red as the top
// five bits of the byte, green in its 0x3e0 field, blue in 0x1f). Signed quotients.
TbError TbTextured4BitFontRenderMethod::DrawCharTo555(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    if (!TextureIsA8R8G8B8(TextureOf(*parameters.mParams))) return TbError(-1);
    WalkTexturedGlyph<2, true>(area, parameters, [](uint8_t* pixel, uint32_t texel, uint8_t nybble) {
        const uint32_t dest = LoadPixel16(pixel);
        const uint32_t weight = nybble;
        const uint32_t destRed = (dest >> 7) & 0xf8;
        const uint32_t destBlue = dest & 0x1f;
        const uint32_t destGreen = dest & 0x3e0;
        const int32_t red = static_cast<int32_t>(weight * (((texel >> 16) & 0xf8) - destRed));
        const int32_t blue = static_cast<int32_t>(weight * (((texel >> 3) & 0x1f) - destBlue));
        const int32_t green = static_cast<int32_t>(weight * (((texel >> 6) & 0x3e0) - destGreen));
        const uint32_t newRed = destRed + static_cast<uint32_t>(QuotientBy15Signed(red));
        const uint32_t newBlue = destBlue + static_cast<uint32_t>(QuotientBy15Signed(blue));
        const uint32_t newGreen = destGreen + static_cast<uint32_t>(QuotientBy15Signed(green));
        StorePixel16(pixel, static_cast<uint16_t>((newBlue & 0x1f) | (dest & 0x8000) | (newGreen & 0x3e0) | ((newRed << 7) & 0x7c00)));
    });
    return TbError(0);
}

// 0x1000a6f4. 5-6-5, no alpha: red as the top five bits of the byte (dest >> 8 & 0xf8), green in the 0x7e0 field (texel >> 5), blue
// in 0x1f. Signed quotients.
TbError TbTextured4BitFontRenderMethod::DrawCharTo565(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    if (!TextureIsA8R8G8B8(TextureOf(*parameters.mParams))) return TbError(-1);
    WalkTexturedGlyph<2, true>(area, parameters, [](uint8_t* pixel, uint32_t texel, uint8_t nybble) {
        const uint32_t dest = LoadPixel16(pixel);
        const uint32_t weight = nybble;
        const uint32_t destRed = (dest >> 8) & 0xf8;
        const uint32_t destBlue = dest & 0x1f;
        const uint32_t destGreen = dest & 0x7e0;
        const int32_t red = static_cast<int32_t>(weight * (((texel >> 16) & 0xf8) - destRed));
        const int32_t blue = static_cast<int32_t>(weight * (((texel >> 3) & 0x1f) - destBlue));
        const int32_t green = static_cast<int32_t>(weight * (((texel >> 5) & 0x7e0) - destGreen));
        const uint32_t newRed = destRed + static_cast<uint32_t>(QuotientBy15Signed(red));
        const uint32_t newBlue = destBlue + static_cast<uint32_t>(QuotientBy15Signed(blue));
        const uint32_t newGreen = destGreen + static_cast<uint32_t>(QuotientBy15Signed(green));
        StorePixel16(pixel, static_cast<uint16_t>((newGreen & 0x7e0) | (newBlue & 0x1f) | ((newRed << 8) & 0xf800)));
    });
    return TbError(0);
}

// 0x1000aba0. 4-4-4 with the alpha nybble at the top (0xf000, kept from the destination): red in 0xf00, green in 0xf0, blue in 0xf.
// UNSIGNED quotients.
TbError TbTextured4BitFontRenderMethod::DrawCharTo4444(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    if (!TextureIsA8R8G8B8(TextureOf(*parameters.mParams))) return TbError(-1);
    WalkTexturedGlyph<2, true>(area, parameters, [](uint8_t* pixel, uint32_t texel, uint8_t nybble) {
        const uint32_t dest = LoadPixel16(pixel);
        const uint32_t weight = nybble;
        const uint32_t destBlue = dest & 0xf;
        const uint32_t destGreen = dest & 0xf0;
        const uint32_t destRed = dest & 0xf00;
        const uint32_t blue = weight * (((texel >> 4) & 0xf) - destBlue);
        const uint32_t green = weight * (((texel >> 8) & 0xf0) - destGreen);
        const uint32_t red = weight * (((texel >> 12) & 0xf00) - destRed);
        const uint32_t newBlue = destBlue + QuotientBy15Unsigned(blue);
        const uint32_t newGreen = destGreen + QuotientBy15Unsigned(green);
        const uint32_t newRed = destRed + QuotientBy15Unsigned(red);
        StorePixel16(pixel, static_cast<uint16_t>((newBlue & 0xf) | (dest & 0xf000) | (newGreen & 0xf0) | (newRed & 0xf00)));
    });
    return TbError(0);
}

// 0x1000b944. The generic format has no writer here.
TbError TbTextured4BitFontRenderMethod::DrawCharToAny(TbRenderArea&, const C4BitFontRenderMethodParameters&) {
    return TbError(-1);
}

// 0x1000b02c. a8r8g8b8: the destination's alpha byte is kept; blue, green and red are blended in place (0xff, 0xff00, 0xff0000), with
// UNSIGNED quotients.
TbError TbTextured4BitFontRenderMethod::DrawCharTo8888(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    if (!TextureIsA8R8G8B8(TextureOf(*parameters.mParams))) return TbError(-1);
    WalkTexturedGlyph<4, true>(area, parameters, [](uint8_t* pixel, uint32_t texel, uint8_t nybble) {
        const uint32_t dest = LoadPixel32(pixel);
        const uint32_t weight = nybble;
        const uint32_t destBlue = dest & 0xff;
        const uint32_t destGreen = dest & 0xff00;
        const uint32_t destRed = dest & 0xff0000;
        const uint32_t blue = weight * ((texel & 0xff) - destBlue);
        const uint32_t green = weight * ((texel & 0xff00) - destGreen);
        const uint32_t red = weight * ((texel & 0xff0000) - destRed);
        const uint32_t newBlue = destBlue + QuotientBy15Unsigned(blue);
        const uint32_t newGreen = destGreen + QuotientBy15Unsigned(green);
        const uint32_t newRed = destRed + QuotientBy15Unsigned(red);
        StorePixel32(pixel, (newBlue & 0xff) | (dest & 0xff000000u) | (newGreen & 0xff00) | (newRed & 0xff0000));
    });
    return TbError(0);
}

// 0x1000b4b8. The destination in BGRA order (the top byte is the first channel the writer blends, the byte below it the second, the
// byte at 0xff00 the third; the low byte is the alpha, which is left out). The texel's blue byte moves to the top, its red to 0xff0000
// and its green to 0xff00. UNSIGNED quotients. The top byte of the answer is the destination WHOLE ORed with the blended top byte
// (a quirk: the listing ORs r31 where the other writers insert the field), and the low byte of the answer is 0.
TbError TbTextured4BitFontRenderMethod::DrawCharToBGRA(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    if (!TextureIsA8R8G8B8(TextureOf(*parameters.mParams))) return TbError(-1);
    WalkTexturedGlyph<4, true>(area, parameters, [](uint8_t* pixel, uint32_t texel, uint8_t nybble) {
        const uint32_t dest = LoadPixel32(pixel);
        const uint32_t weight = nybble;
        const uint32_t destTop = dest & 0xff000000u;
        const uint32_t destMiddle = dest & 0xff0000;
        const uint32_t destLow = dest & 0xff00;
        const uint32_t top = weight * ((texel << 24) - destTop);
        const uint32_t middle = weight * (((texel << 8) & 0xff0000) - destMiddle);
        const uint32_t low = weight * (((texel >> 8) & 0xff00) - destLow);
        const uint32_t newTop = dest | (destTop + QuotientBy15Unsigned(top));
        const uint32_t newMiddle = destMiddle + QuotientBy15Unsigned(middle);
        const uint32_t newLow = destLow + QuotientBy15Unsigned(low);
        StorePixel32(pixel, (newMiddle & 0xff0000) | (newTop & 0xff000000u) | (newLow & 0xff00));
    });
    return TbError(0);
}
