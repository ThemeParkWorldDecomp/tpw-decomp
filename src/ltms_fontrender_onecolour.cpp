// ltms TbFontRender.cpp, the pixel writers of TbOneColour4BitFontRenderMethod (unit w45b; ltms symbols shared
// 0x1000e10c..0x1000f558): they draw a glyph in the colour of the render parameters by BLENDING it over the destination pixel. The
// colour is the first four of the parameters' five flag bytes (include/TbColour.h): alpha at +0x44, red, green, blue. For each pixel
// the weight is the glyph's nybble (0..15), scaled by the colour's alpha as nybble * alpha / 255 when the alpha is below 255; each
// channel then moves from the destination towards the colour by weight / 15 of the difference, the quotient truncated. Every pixel
// of the source rectangle is written (a weight of 0 writes the destination back); a zero alpha draws nothing; every writer answers
// 0, the generic one -1. Quirks kept: the 555, 565 and 4444 writers divide a SIGNED difference (truncating towards zero) but the
// 8888 and BGRA writers divide the difference as an UNSIGNED 32-bit number, so a channel that has to get darker takes a large
// wrong quotient; the 555 and 565 writers compute green in place (without moving it down to the low bits) and the 4444 writer
// does not drop the low nybble of the colour's channels before it takes the difference.
#include "TbFontRender.h"

namespace {

// The colour's bytes in the parameters: alpha, red, green, blue.
const uint8_t* ColourOf(const C4BitFontRenderMethodParameters& parameters) {
    return reinterpret_cast<const uint8_t*>(&parameters.mParams->mState[16]);
}

// The weight of a nybble: itself for an alpha of 255, else scaled by the alpha.
uint32_t WeightOf(uint32_t alpha, uint8_t nybble) { return alpha == 0xff ? nybble : (alpha * nybble) / 0xff; }

// One channel of the 16-bit formats: destination + the difference to the colour times the weight, over 15, signed.
int32_t Blend(int32_t destination, int32_t colour, uint32_t weight) {
    const int32_t delta = static_cast<int32_t>(weight * static_cast<uint32_t>(colour - destination));
    return destination + delta / 15;
}

// The same for the 32-bit formats, where the product and the quotient are unsigned.
uint32_t BlendUnsigned(uint32_t destination, uint32_t colour, uint32_t weight) {
    return destination + (weight * (colour - destination)) / 15;
}

}  // namespace

// 0x1000e10c. 5-5-5 with the alpha bit of the destination kept. Green is blended in place (bits 5..9).
TbError TbOneColour4BitFontRenderMethod::DrawCharTo555(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    const uint8_t* const colour = ColourOf(parameters);
    const uint32_t alpha = colour[0];
    if (alpha == 0) return TbError(0);
    const int32_t red = colour[1] & 0xf8;
    const int32_t green = (colour[2] << 2) & 0x3e0;
    const int32_t blue = (colour[3] >> 3) & 0x1f;
    WalkOneColourGlyph<2>(area, parameters, [&](uint8_t* pixel, uint8_t nybble) {
        const uint32_t weight = WeightOf(alpha, nybble);
        const uint32_t old = LoadPixel16(pixel);
        const int32_t newRed = Blend((old >> 7) & 0xf8, red, weight);
        const int32_t newGreen = Blend(old & 0x3e0, green, weight);
        const int32_t newBlue = Blend(old & 0x1f, blue, weight);
        StorePixel16(pixel, static_cast<uint16_t>((static_cast<uint32_t>(newBlue) & 0x1f) | (old & 0x8000) |
                                                  (static_cast<uint32_t>(newGreen) & 0x3e0) | ((static_cast<uint32_t>(newRed) << 7) & 0x7c00)));
    });
    return TbError(0);
}

// 0x1000e528. 5-6-5. Green is blended in place (bits 5..10).
TbError TbOneColour4BitFontRenderMethod::DrawCharTo565(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    const uint8_t* const colour = ColourOf(parameters);
    const uint32_t alpha = colour[0];
    if (alpha == 0) return TbError(0);
    const int32_t red = colour[1] & 0xf8;
    const int32_t green = (colour[2] & 0xfc) * 8;
    const int32_t blue = colour[3] >> 3;
    WalkOneColourGlyph<2>(area, parameters, [&](uint8_t* pixel, uint8_t nybble) {
        const uint32_t weight = WeightOf(alpha, nybble);
        const uint32_t old = LoadPixel16(pixel);
        const int32_t newRed = Blend((old >> 8) & 0xf8, red, weight);
        const int32_t newGreen = Blend(old & 0x7e0, green, weight);
        const int32_t newBlue = Blend(old & 0x1f, blue, weight);
        StorePixel16(pixel, static_cast<uint16_t>(((static_cast<uint32_t>(newRed) << 8) & 0xf800) | (static_cast<uint32_t>(newBlue) & 0x1f) |
                                                  (static_cast<uint32_t>(newGreen) & 0x7e0)));
    });
    return TbError(0);
}

// 0x1000e93c. 4-4-4 with the destination's alpha nybble kept. The colour's channels are used whole (not masked to their top
// nybble) in the difference, while the destination's are the top nybbles moved to bits 4..7.
TbError TbOneColour4BitFontRenderMethod::DrawCharTo4444(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    const uint8_t* const colour = ColourOf(parameters);
    const uint32_t alpha = colour[0];
    if (alpha == 0) return TbError(0);
    const int32_t red = colour[1];
    const int32_t green = colour[2];
    const int32_t blue = colour[3];
    WalkOneColourGlyph<2>(area, parameters, [&](uint8_t* pixel, uint8_t nybble) {
        const uint32_t weight = WeightOf(alpha, nybble) & 0xff;
        const uint32_t old = LoadPixel16(pixel);
        const int32_t newRed = Blend((old >> 8) & 0xf0, red, weight);
        const int32_t newGreen = Blend((old >> 4) & 0xf0, green, weight);
        const int32_t newBlue = Blend(old & 0xf0, blue, weight);
        StorePixel16(pixel, static_cast<uint16_t>((old & 0xf) | (static_cast<uint32_t>(newBlue) & 0xf0) |
                                                  ((static_cast<uint32_t>(newRed) << 8) & 0xf000) | ((static_cast<uint32_t>(newGreen) << 4) & 0xf00)));
    });
    return TbError(0);
}

// 0x1000edb4. 8-8-8 with the destination's alpha byte kept; unsigned arithmetic on the channels in place.
TbError TbOneColour4BitFontRenderMethod::DrawCharTo8888(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    const uint8_t* const colour = ColourOf(parameters);
    const uint32_t alpha = colour[0];
    if (alpha == 0) return TbError(0);
    const uint32_t red = static_cast<uint32_t>(colour[1]) << 16;
    const uint32_t green = static_cast<uint32_t>(colour[2]) << 8;
    const uint32_t blue = colour[3];
    WalkOneColourGlyph<4>(area, parameters, [&](uint8_t* pixel, uint8_t nybble) {
        const uint32_t weight = WeightOf(alpha, nybble) & 0xff;
        const uint32_t old = LoadPixel32(pixel);
        StorePixel32(pixel, (BlendUnsigned(old & 0xff0000, red, weight) & 0xff0000) | (BlendUnsigned(old & 0xff00, green, weight) & 0xff00) |
                                (old & 0xff000000) | (BlendUnsigned(old & 0xff, blue, weight) & 0xff));
    });
    return TbError(0);
}

// 0x1000f184. As the 8888 writer with the channels in the BGRA order: red at bits 8..15, green at 16..23, blue at 24..31 and the
// destination's alpha in the low byte.
TbError TbOneColour4BitFontRenderMethod::DrawCharToBGRA(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    const uint8_t* const colour = ColourOf(parameters);
    const uint32_t alpha = colour[0];
    if (alpha == 0) return TbError(0);
    const uint32_t red = static_cast<uint32_t>(colour[1]) << 8;
    const uint32_t green = static_cast<uint32_t>(colour[2]) << 16;
    const uint32_t blue = static_cast<uint32_t>(colour[3]) << 24;
    WalkOneColourGlyph<4>(area, parameters, [&](uint8_t* pixel, uint8_t nybble) {
        const uint32_t weight = WeightOf(alpha, nybble) & 0xff;
        const uint32_t old = LoadPixel32(pixel);
        StorePixel32(pixel, (BlendUnsigned(old & 0xff00, red, weight) & 0xff00) | (BlendUnsigned(old & 0xff0000, green, weight) & 0xff0000) |
                                (old & 0xff) | (BlendUnsigned(old & 0xff000000, blue, weight) & 0xff000000));
    });
    return TbError(0);
}

// 0x1000f558: the generic format is refused.
TbError TbOneColour4BitFontRenderMethod::DrawCharToAny(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    return TbError(-1);
}
