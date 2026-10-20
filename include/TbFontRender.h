// ltms TbFontRender (TbFontRender.cpp range 0x10001fdc..0x10002298): draws one glyph with the render method its parameters
// select. The 4-bit method's core (DrawChar and its helpers, 0x10000bf0..0x10001f64) is unit w45a (src/ltms_fontrender_method.cpp);
// the four subclasses' constructors and destructors, the two one-colour classes' pixel writers, the objects and the table are unit w45b
// (src/ltms_fontrender_methods.cpp, src/ltms_fontrender_onecolour.cpp, src/ltms_fontrender_onecolour_noblend.cpp); the two textured
// classes' writers (0x1000a1dc..0x1000cf98) are untranslated.
#pragma once
#include <cstdint>
#include <cstring>

#include "TbError.h"
#include "TbFontBits.h"
#include "TbFontRenderParams.h"
#include "TbGraphicFile.h"

// What draws a glyph. Mac vtable: the destructor at +8, DrawChar at +0xc (the only other virtual member). The four method
// objects are globals of the Mac (0x10011714, 0x10011724, 0x10011734, 0x10011744, built by __sinit_TbFontRender_cpp
// 0x10002298): TbOneColour4BitFontRenderMethod, TbOneColourNoBlend4BitFontRenderMethod, TbTextured4BitFontRenderMethod and
// TbTexturedNoBlend4BitFontRenderMethod.
class TbFontRenderMethod {
public:
    virtual ~TbFontRenderMethod();   // 0x10000c38 (src/ltms_fontrender_method.cpp)
    // Draws glyph `glyph` with its top left at `at` into the render area of `params` and answers, in `drawn`, the rectangle
    // it changed (left == right and top == bottom for none). Mac mangling: Tb4BitFontRenderMethod::DrawChar(const TbPoint&,
    // unsigned short, TbRect&, TbFontRenderParams&) at 0x10000d40.
    virtual TbError DrawChar(const TbPoint& at, uint16_t glyph, TbRect& drawn, TbFontRenderParams& params) = 0;
};

// The record DrawChar hands to a pixel-format writer (unit w45a; Mac 0x18 bytes on the stack of DrawChar, the writers
// receive its address). The pointers are the Mac's 32-bit words on the Mac and native pointers here.
struct C4BitFontRenderMethodParameters {
    const TbPoint* mPoint;          // +0x00  where the glyph's top left goes (already moved by the glyph's offsets and clamped to the clip)
    const uint8_t* mData;           // +0x04  the glyph's nybbles, high nybble first: the decode buffer (run-length and one-bit
                                    //        glyphs) or the font's own data (raw ones)
    int32_t mBytesPerRow;           // +0x08  (width + 1) >> 1
    const TbRect* mSource;          // +0x0c  the part of the glyph to draw (left and top are the pixels cut off by the clip)
    TbFontRenderParams* mParams;    // +0x10  the render parameters DrawChar was called with
    uint8_t mWidth;                 // +0x14  the glyph's width, low byte
};

// The 4-bit font render method: decodes one glyph and hands it to the writer for the render area's pixel format. Mac
// size 4 (the vtable pointer), vtable 0x10011004: +8 destructor, +0xc DrawChar, then the six writers in the order
// 555 (+0x10), 565 (+0x14), 4444 (+0x18), Any (+0x1c), 8888 (+0x20), BGRA (+0x24). The four subclasses (the pixel
// writers; unit w45b) are declared after it.
class Tb4BitFontRenderMethod : public TbFontRenderMethod {
public:
    Tb4BitFontRenderMethod() {}                                     // 0x10000bf0
    ~Tb4BitFontRenderMethod() override;                             // 0x10000cb4
    // 0x10000d40. Answers -1 without a font in the parameters (the original then releases through a NULL vtable), with
    // a palettised render area, without data for the glyph or with nothing of the glyph inside the clip rectangle; 0 for
    // a space (header halfword 0x20), with an empty rectangle at the unadjusted point; else the writer's answer. The
    // font's Release is called on every path after GetFont took a reference.
    TbError DrawChar(const TbPoint& at, uint16_t glyph, TbRect& drawn, TbFontRenderParams& params) override;
    virtual TbError DrawCharTo555(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) = 0;    // slot +0x10
    virtual TbError DrawCharTo565(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) = 0;    // slot +0x14
    virtual TbError DrawCharTo4444(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) = 0;    // slot +0x18
    virtual TbError DrawCharToAny(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) = 0;    // slot +0x1c
    virtual TbError DrawCharTo8888(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) = 0;    // slot +0x20
    virtual TbError DrawCharToBGRA(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) = 0;    // slot +0x24
    // 0x10001dbc: the part of the glyph rectangle `rect` that lies inside the clip rectangle when its top left is at
    // `point`: the clip is LbDraw_GetClipRect, the rectangle moved to `point` is cut with TbRect::Intersection, and the
    // answer starts at rect's left and top plus the amount the point is outside the clip. The render area is not read.
    TbRect CalculateSourceRect(const TbRect& rect, const TbPoint& point, const TbRenderArea& area);
    // 0x10001f64: 32 minus the position of the highest set bit counted from the top (bit 31 gives 32), minus 8 when
    // that is 8 or more (unsigned), else plus 0x18; 0 gives 0xfffffff7. The pixel writers turn a masked channel by this.
    int32_t GetRotate(uint32_t mask);
};

// The four methods the table holds (unit w45b). The textured classes' twelve writers are not translated: no definitions.
class TbOneColour4BitFontRenderMethod : public Tb4BitFontRenderMethod {
public:
    TbOneColour4BitFontRenderMethod();                             // 0x1000dff0
    ~TbOneColour4BitFontRenderMethod() override;
    TbError DrawCharTo555(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharTo565(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharTo4444(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharToAny(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharTo8888(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharToBGRA(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
};
class TbOneColourNoBlend4BitFontRenderMethod : public Tb4BitFontRenderMethod {
public:
    TbOneColourNoBlend4BitFontRenderMethod();                             // 0x1000d050
    ~TbOneColourNoBlend4BitFontRenderMethod() override;
    TbError DrawCharTo555(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharTo565(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharTo4444(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharToAny(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharTo8888(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharToBGRA(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
};
class TbTextured4BitFontRenderMethod : public Tb4BitFontRenderMethod {
public:
    TbTextured4BitFontRenderMethod();                             // 0x1000a0c0
    ~TbTextured4BitFontRenderMethod() override;
    TbError DrawCharTo555(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharTo565(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharTo4444(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharToAny(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharTo8888(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharToBGRA(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
};
class TbTexturedNoBlend4BitFontRenderMethod : public Tb4BitFontRenderMethod {
public:
    TbTexturedNoBlend4BitFontRenderMethod();                             // 0x1000b9c8
    ~TbTexturedNoBlend4BitFontRenderMethod() override;
    TbError DrawCharTo555(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharTo565(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharTo4444(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharToAny(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharTo8888(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
    TbError DrawCharToBGRA(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters) override;
};

// The method for each (blend, type, style), as the Mac's data holds it at 0x1001103c: 2 x 5 x 3 pointers of which four are
// set, all at type 4: blend 0 style 1 the one-colour method without blending (0x10011734), style 2 the textured one without
// blending (0x10011714); blend 1 style 1 the one-colour method (0x10011744), style 2 the textured one (0x10011724). The
// table is the render methods' data, defined in src/ltms_fontrender_methods.cpp.
extern TbFontRenderMethod* gFontRenderMethods[2][5][3];

// Mac 0x58 bytes, vtable 0x100110bc (only the destructor, slot 8).
class TbFontRender {
public:
    TbFontRender();                                                           // 0x10001fdc
    virtual ~TbFontRender();                                                  // 0x10002050
    // 0x100020e4: copies `params` into this render's own and picks the method. -1 (and no method) when the style is 3 or more
    // or the type 5 or more (signed tests), or when the table has no method for the combination.
    TbError Initialise(const TbFontRenderParams& params);
    // 0x100021fc: the method's DrawChar, with this render's own parameters; -1 without a method.
    TbError DrawChar(const TbPoint& at, uint16_t glyph, TbRect& drawn);

    TbFontRenderParams mParams;     // +0x04, 0x50 bytes
    TbFontRenderMethod* mMethod;    // +0x54  chosen by Initialise, NULL until then
};

// unit w45b
// The four method objects, global instances of the Mac (their addresses are in the comment on the classes above). They are
// built by __sinit_TbFontRender_cpp (0x10002298) in the order one-colour, one-colour without blending, textured, textured
// without blending, which the host reproduces by defining them in that order in src/ltms_fontrender_methods.cpp.
extern TbOneColour4BitFontRenderMethod gOneColour4BitFontRenderMethod;                 // 0x10011744
extern TbOneColourNoBlend4BitFontRenderMethod gOneColourNoBlend4BitFontRenderMethod;   // 0x10011734
extern TbTextured4BitFontRenderMethod gTextured4BitFontRenderMethod;                   // 0x10011724
extern TbTexturedNoBlend4BitFontRenderMethod gTexturedNoBlend4BitFontRenderMethod;     // 0x10011714

// 0x1000d020 (ltms): stores its argument in the byte at 0x10011768 (`writeZero` in the symbols). Nothing in the ltms library
// reads that byte; the host keeps it as gWriteZero.
void SetWriteZero(bool writeZero);
extern bool gWriteZero;

// Destination pixels as the writers touch them (the original loads and stores them where they lie, aligned or not).
inline uint16_t LoadPixel16(const uint8_t* at) {
    uint16_t value;
    std::memcpy(&value, at, sizeof(value));
    return value;
}
inline uint32_t LoadPixel32(const uint8_t* at) {
    uint32_t value;
    std::memcpy(&value, at, sizeof(value));
    return value;
}
inline void StorePixel16(uint8_t* at, uint16_t value) { std::memcpy(at, &value, sizeof(value)); }
inline void StorePixel32(uint8_t* at, uint32_t value) { std::memcpy(at, &value, sizeof(value)); }

// The loop eleven of the twelve one-colour writers share (the original inlines it into each; the 565 writer without blending has
// its own, which counts bytes and reads no nybbles): walks the part of the glyph that parameters.mSource names, row by row, over the
// destination pixels of `area` from parameters.mPoint, and calls operation(uint8_t* destination, uint8_t nybble) for each pixel
// (PixelBytes wide: the destination is not necessarily aligned). The glyph is a plain run of nybbles whose row length is the width
// BYTE (parameters.mWidth), not the bytes in a row; the first nybble is source.left + source.top * width. The offset of the first
// pixel is computed in 32 bits as the original does. After each row the reader is stepped over the rest of the glyph's row
// (SkipNybbles, an unsigned count).
template <unsigned PixelBytes, class Operation>
inline void WalkOneColourGlyph(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters, Operation operation) {
    const TbRect& source = *parameters.mSource;
    const TbPoint& point = *parameters.mPoint;
    const uint32_t first = static_cast<uint32_t>(source.left) + static_cast<uint32_t>(source.top) * parameters.mWidth;
    FourBit::NybbleIterator in(const_cast<uint8_t*>(parameters.mData) + (first >> 1), (first & 1) != 0);
    const uint32_t offset = static_cast<uint32_t>(point.y) * static_cast<uint32_t>(area.mStride) + static_cast<uint32_t>(point.x) * PixelBytes;
    uint8_t* row = area.mData + static_cast<int32_t>(offset);
    const uint32_t skip = parameters.mWidth - static_cast<uint32_t>(source.right - source.left);
    for (int32_t rows = source.bottom - source.top; rows != 0; --rows) {
        uint8_t* const next = row + area.mStride;
        uint8_t* pixel = row;
        for (int32_t columns = source.right - source.left; columns != 0; --columns) {
            const uint8_t nybble = (in++).current();
            operation(pixel, nybble);
            pixel += PixelBytes;
        }
        row = next;
        FourBit::SkipNybbles(in, skip);
    }
}

// unit w45c
// The twelve writers of the two textured classes (src/ltms_fontrender_textured.cpp, src/ltms_fontrender_textured_noblend.cpp) draw the glyph
// through a 32-bit texture that the render parameters hold: the render area at +0x14..+0x3b (mState[4..13], a TbRenderArea by value: the
// six format words, the pixels, the width, the height and the bytes in a row) and the texture origin at +0x3c and +0x40 (mState[14] and
// mState[15], written by TbFontRenderParams::UseTexture).
#include "TbPixFmt.h"

inline const TbRenderArea& TextureOf(const TbFontRenderParams& params) { return *reinterpret_cast<const TbRenderArea*>(&params.mState[4]); }
inline int32_t TextureOriginX(const TbFontRenderParams& params) { return static_cast<int32_t>(static_cast<uint32_t>(params.mState[14])); }
inline int32_t TextureOriginY(const TbFontRenderParams& params) { return static_cast<int32_t>(static_cast<uint32_t>(params.mState[15])); }

// The test every textured writer starts with: the texture's six format words against TbPixFormat32 (the four masks and the depth as
// words, the palettised flag as "both zero or both not zero"). The original compares them in this order, stopping at the first
// difference; the writers answer -1 and draw nothing when any differs.
inline bool TextureIsA8R8G8B8(const TbRenderArea& texture) {
    uint32_t masks[4];
    std::memcpy(masks, texture.mF00, sizeof(masks));
    return masks[0] == TbPixFormat32.mRedMask && masks[1] == TbPixFormat32.mGreenMask && masks[2] == TbPixFormat32.mBlueMask &&
           masks[3] == TbPixFormat32.mAlphaMask && static_cast<uint32_t>(texture.mDepth) == static_cast<uint32_t>(TbPixFormat32.mBitsPerPixel) &&
           ((texture.mPalettised == 0) == (TbPixFormat32.mPalettised == 0));
}

// The quotient by 15 the blended writers take of a weighted channel difference: the multiply-high by the constant 0x88888889 of
// the original, signed (555 and 565; mulhw, the sign bit added back, truncating towards zero) or unsigned (4444, 8888
// and BGRA; mulhwu, so a difference that is negative as a 32-bit number gives a huge quotient).
inline int32_t QuotientBy15Signed(int32_t value) {
    int32_t high = static_cast<int32_t>((static_cast<int64_t>(static_cast<int32_t>(0x88888889u)) * value) >> 32);
    high += value;
    high >>= 3;
    return high + static_cast<int32_t>(static_cast<uint32_t>(high) >> 31);
}
inline uint32_t QuotientBy15Unsigned(uint32_t value) {
    return static_cast<uint32_t>((static_cast<uint64_t>(0x88888889u) * value) >> 32) >> 3;
}

// The loop the twelve textured writers share (the original inlines it into each; the 565 writer without blending reads no nybbles and
// has its own copy that is unrolled by four): walks the part of the glyph that parameters.mSource names, row by row, over the
// destination pixels of `area` from parameters.mPoint, tiling the texture over it, and calls operation(destination, texel, nybble)
// for each pixel (PixelBytes wide; the nybble is 0 when ReadNybbles is false). The first texel row is (point.y - origin.y) modulo the
// texture's height, the first texel column (point.x - origin.x) modulo its width (unsigned divisions: a negative difference wraps as
// a 32-bit number first); the walk goes in runs up to the texture's edge and wraps to 0 (a row starts at the column of the point
// again). The glyph is read as WalkOneColourGlyph reads it, the row skip after each row included.
template <unsigned PixelBytes, bool ReadNybbles, class Operation>
inline void WalkTexturedGlyph(TbRenderArea& area, const C4BitFontRenderMethodParameters& parameters, Operation operation) {
    const TbRect& source = *parameters.mSource;
    const TbPoint& point = *parameters.mPoint;
    const TbRenderArea& texture = TextureOf(*parameters.mParams);
    const uint32_t textureWidth = static_cast<uint32_t>(texture.mWidth);
    const uint32_t textureHeight = static_cast<uint32_t>(texture.mPictureHeight);
    const uint32_t first = static_cast<uint32_t>(source.left) + static_cast<uint32_t>(source.top) * parameters.mWidth;
    FourBit::NybbleIterator in(const_cast<uint8_t*>(parameters.mData) + (first >> 1), (first & 1) != 0);
    const uint32_t offset = static_cast<uint32_t>(point.y) * static_cast<uint32_t>(area.mStride) + static_cast<uint32_t>(point.x) * PixelBytes;
    uint8_t* row = area.mData + static_cast<int32_t>(offset);
    const uint32_t skip = parameters.mWidth - static_cast<uint32_t>(source.right - source.left);
    uint32_t rows = static_cast<uint32_t>(source.bottom - source.top);
    uint32_t textureRow = (static_cast<uint32_t>(point.y) - static_cast<uint32_t>(TextureOriginY(*parameters.mParams))) % textureHeight;
    while (rows != 0) {
        uint32_t rowRun = textureHeight - textureRow;
        if (rows < rowRun) rowRun = rows;
        rows -= rowRun;
        for (; rowRun != 0; --rowRun) {
            const uint8_t* const textureLine = texture.mData + static_cast<int32_t>(textureRow * static_cast<uint32_t>(texture.mStride));
            ++textureRow;
            uint32_t columns = static_cast<uint32_t>(source.right - source.left);
            uint32_t textureColumn = (static_cast<uint32_t>(point.x) - static_cast<uint32_t>(TextureOriginX(*parameters.mParams))) % textureWidth;
            uint8_t* pixel = row;
            while (columns != 0) {
                const uint8_t* texel = textureLine + textureColumn * 4;
                uint32_t columnRun = textureWidth - textureColumn;
                if (columns < columnRun) columnRun = columns;
                columns -= columnRun;
                for (; columnRun != 0; --columnRun) {
                    uint8_t nybble = 0;
                    if (ReadNybbles) nybble = (in++).current();
                    operation(pixel, LoadPixel32(texel), nybble);
                    texel += 4;
                    pixel += PixelBytes;
                }
                textureColumn = 0;
            }
            row += area.mStride;
            if (ReadNybbles) FourBit::SkipNybbles(in, skip);
        }
        textureRow = 0;
    }
}
