// TbOneColourNoBlend4BitFontRenderMethod.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "TbFontRender.h"

// ltms TbFontRender.cpp, the four 4-bit font render method objects (unit w45b; ltms symbols shared 0x1000a0c0..0x1000e068 for the
// constructors and destructors, 0x1000d020 for SetWriteZero and 0x10002298 for the static initialiser): the objects the table
// gFontRenderMethods holds and the table itself. The two one-colour classes' pixel writers are in
// src/ltms_fontrender_onecolour.cpp and src/ltms_fontrender_onecolour_noblend.cpp; the two textured classes' writers
// (0x1000a1dc..0x1000cf98) are untranslated.

// 0x1000d050
TbOneColourNoBlend4BitFontRenderMethod::TbOneColourNoBlend4BitFontRenderMethod() {}
// 0x1000d0d0
TbOneColourNoBlend4BitFontRenderMethod::~TbOneColourNoBlend4BitFontRenderMethod() {}
// ---- SetWriteZero

// The byte at 0x10011768. Nothing in the ltms library reads it (the symbols name it `writeZero`).
bool gWriteZero = false;

// 0x1000d020
void SetWriteZero(bool writeZero) { gWriteZero = writeZero; }

// ---- the objects and the table

// __sinit_TbFontRender_cpp (0x10002298) constructs the four objects, in this order, and registers each destructor to run at exit
// (__register_global_object); here the compiler does both for the global definitions, which follow the order of the original.
TbOneColour4BitFontRenderMethod gOneColour4BitFontRenderMethod;                 // 0x10011744
TbOneColourNoBlend4BitFontRenderMethod gOneColourNoBlend4BitFontRenderMethod;   // 0x10011734
TbTextured4BitFontRenderMethod gTextured4BitFontRenderMethod;                   // 0x10011724
TbTexturedNoBlend4BitFontRenderMethod gTexturedNoBlend4BitFontRenderMethod;     // 0x10011714

// The table is initialised data of the Mac (0x1001103c, relocated pointers; the static initialiser does not touch it): 2 blends x
// 5 types x 3 styles, all null but four cells at type 4. [blend][type][style], blend 0 = after UseAlphaWriteMode, style 1 = one
// colour, style 2 = textured.
TbFontRenderMethod* gFontRenderMethods[2][5][3] = {
    {{}, {}, {}, {}, {nullptr, &gOneColourNoBlend4BitFontRenderMethod, &gTexturedNoBlend4BitFontRenderMethod}},
    {{}, {}, {}, {}, {nullptr, &gOneColour4BitFontRenderMethod, &gTextured4BitFontRenderMethod}},
};

// ltms TbFontRender.cpp, the pixel writers of TbOneColourNoBlend4BitFontRenderMethod (unit w45b; ltms symbols shared
// 0x1000d17c..0x1000df64): they draw a glyph in the colour of the render parameters, WITHOUT reading the destination pixel. The
// colour is the first four of the parameters' five flag bytes (include/TbColour.h): alpha at +0x44, red, green, blue. A pixel is
// written where the glyph's nybble (scaled by the colour's alpha) is not zero, so the destination keeps its other pixels. Every
// writer answers 0; the generic one answers -1. Quirks kept (see the comments): the 565 writer fills the whole source rectangle
// and ignores the nybbles and the alpha; the 4444 writer's scaled alpha is always 0 (it shifts a value of at most 15 right by
// four) when the colour's alpha is below 255; the BGRA writer leaves the blue byte out of the colour.

namespace {

// The colour's bytes in the parameters: alpha, red, green, blue.
const uint8_t* ColourOf(const C4BitFontRenderMethodParameters& parameters) {
    return reinterpret_cast<const uint8_t*>(&parameters.mParams->mState[16]);
}

}  // namespace

// 0x1000d17c. With a zero alpha nothing is drawn. The colour is 5-5-5 (the low bits of each channel dropped). With alpha 255 a
// pixel is written where the nybble has its top bit (8) set, and that bit becomes the pixel's alpha bit; with a lower alpha the
// nybble is first scaled to nybble * alpha / 255 and the same bit of the result decides.
TbError TbOneColourNoBlend4BitFontRenderMethod::DrawCharTo555(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    const uint8_t* const colour = ColourOf(parameters);
    const uint32_t alpha = colour[0];
    if (alpha == 0) return TbError(0);
    const uint32_t packed = static_cast<uint32_t>(colour[3] >> 3) | ((colour[1] & 0xf8u) << 7) | ((colour[2] & 0xf8u) << 2);
    if (alpha == 0xff) {
        WalkOneColourGlyph<2>(area, parameters, [&](uint8_t* pixel, uint8_t nybble) {
            if ((nybble & 8) != 0) StorePixel16(pixel, static_cast<uint16_t>(((nybble & 8u) << 12) | packed));
        });
    } else {
        WalkOneColourGlyph<2>(area, parameters, [&](uint8_t* pixel, uint8_t nybble) {
            const uint32_t bit = (((alpha * nybble) / 0xff) * 0x1000) & 0x8000;
            if (bit != 0) StorePixel16(pixel, static_cast<uint16_t>(bit | packed));
        });
    }
    return TbError(0);
}

// 0x1000d4a4. The colour is 5-6-5. Every pixel of the source rectangle is set to it: the glyph's data and the colour's alpha are
// not read. The row and column counts are bytes here (the low eight bits of the rectangle's height and width are counted down).
TbError TbOneColourNoBlend4BitFontRenderMethod::DrawCharTo565(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    const uint8_t* const colour = ColourOf(parameters);
    const TbRect& source = *parameters.mSource;
    const TbPoint& point = *parameters.mPoint;
    const uint32_t packed = static_cast<uint32_t>(colour[3] >> 3) | ((colour[2] & 0xfcu) << 3) | ((colour[1] & 0xf8u) << 8);
    const uint32_t offset = static_cast<uint32_t>(point.y) * static_cast<uint32_t>(area.mStride) + static_cast<uint32_t>(point.x) * 2;
    uint8_t* row = area.mData + static_cast<int32_t>(offset);
    for (uint32_t rows = static_cast<uint32_t>(source.bottom - source.top) & 0xff; rows != 0; --rows) {
        uint8_t* pixel = row;
        for (uint32_t columns = static_cast<uint32_t>(source.right - source.left) & 0xff; columns != 0; --columns) {
            StorePixel16(pixel, static_cast<uint16_t>(packed));
            pixel += 2;
        }
        row += area.mStride;
    }
    return TbError(0);
}

// 0x1000d5cc. The colour is 4-4-4 with the pixel's alpha in the low nybble. With alpha 255 a pixel is written where the nybble is
// not zero, with the nybble as its alpha. With a lower alpha the scaled value nybble * alpha / 255 decides whether a pixel is
// written, but the alpha written is that value shifted right by four, which is always 0.
TbError TbOneColourNoBlend4BitFontRenderMethod::DrawCharTo4444(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    const uint8_t* const colour = ColourOf(parameters);
    const uint32_t alpha = colour[0];
    if (alpha == 0) return TbError(0);
    const uint32_t packed = (colour[3] & 0xf0u) | ((colour[1] & 0xf0u) << 8) | ((colour[2] & 0xf0u) << 4);
    if (alpha == 0xff) {
        WalkOneColourGlyph<2>(area, parameters, [&](uint8_t* pixel, uint8_t nybble) {
            if (nybble != 0) StorePixel16(pixel, static_cast<uint16_t>((nybble & 0xfu) | packed));
        });
    } else {
        WalkOneColourGlyph<2>(area, parameters, [&](uint8_t* pixel, uint8_t nybble) {
            const uint32_t scaled = (alpha * nybble) / 0xff;
            if (scaled != 0) StorePixel16(pixel, static_cast<uint16_t>(((scaled >> 4) & 0xfu) | packed));
        });
    }
    return TbError(0);
}

// 0x1000d908. The colour is 8-8-8 and the alpha byte is the nybble repeated in both halves (n * 17), or the scaled value
// (nybble * alpha / 255) repeated, for an alpha below 255.
TbError TbOneColourNoBlend4BitFontRenderMethod::DrawCharTo8888(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    const uint8_t* const colour = ColourOf(parameters);
    const uint32_t alpha = colour[0];
    if (alpha == 0) return TbError(0);
    const uint32_t packed = static_cast<uint32_t>(colour[3]) | (static_cast<uint32_t>(colour[1]) << 16) | (static_cast<uint32_t>(colour[2]) << 8);
    if (alpha == 0xff) {
        WalkOneColourGlyph<4>(area, parameters, [&](uint8_t* pixel, uint8_t nybble) {
            if (nybble != 0) StorePixel32(pixel, packed | (static_cast<uint32_t>(nybble) << 24) | (static_cast<uint32_t>(nybble) << 28));
        });
    } else {
        WalkOneColourGlyph<4>(area, parameters, [&](uint8_t* pixel, uint8_t nybble) {
            const uint32_t scaled = (alpha * nybble) / 0xff;
            if (scaled != 0) StorePixel32(pixel, packed | (scaled << 24) | (scaled << 28));
        });
    }
    return TbError(0);
}

// 0x1000dc3c. As the 8888 writer with the channels in the BGRA order (red at bits 8, green at 16, alpha in the low byte), but the
// colour's blue byte is never read, so the blue channel of every pixel written is 0.
TbError TbOneColourNoBlend4BitFontRenderMethod::DrawCharToBGRA(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    const uint8_t* const colour = ColourOf(parameters);
    const uint32_t alpha = colour[0];
    if (alpha == 0) return TbError(0);
    const uint32_t packed = (static_cast<uint32_t>(colour[1]) << 8) | (static_cast<uint32_t>(colour[2]) << 16);
    if (alpha == 0xff) {
        WalkOneColourGlyph<4>(area, parameters, [&](uint8_t* pixel, uint8_t nybble) {
            if (nybble != 0) StorePixel32(pixel, packed | nybble | (static_cast<uint32_t>(nybble) << 4));
        });
    } else {
        WalkOneColourGlyph<4>(area, parameters, [&](uint8_t* pixel, uint8_t nybble) {
            const uint32_t scaled = (alpha * nybble) / 0xff;
            if (scaled != 0) StorePixel32(pixel, packed | scaled | (scaled << 4));
        });
    }
    return TbError(0);
}

// 0x1000df64: the generic format is refused.
TbError TbOneColourNoBlend4BitFontRenderMethod::DrawCharToAny(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) {
    return TbError(-1);
}
