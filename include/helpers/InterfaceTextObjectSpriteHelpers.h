// InterfaceTextObjectSpriteHelpers.h: file-local helpers that src/game/ui/InterfaceTextObjectSprite.cpp shares with
// ui_textmanager.cpp. Before the regroup, the functions using them shared a source file (1 file), which had one copy of
// each; that copy now lives here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "Text.h"
#include "ui_DrawObject.h"

// ---- arithmetic the compiler's way

// (units * screen width) / 2048 truncated toward zero: srawi 11 and addze, as the sprite object does for its width.
static inline int32_t ToPixelsX(int32_t units) {
    const int32_t product = units * gUIRealScreenWidth;
    int32_t quotient = product >> 11;
    if (product < 0 && (product & 0x7ff) != 0) quotient += 1;
    return quotient;
}

// (units * screen height) / 1536 as the compiler's multiply-high by 0x2aaaaaab, shift by 8 and added sign do it (truncated toward zero).
static inline int32_t ToPixelsY(int32_t units) {
    const int32_t product = units * gUIRealScreenHeight;
    const int32_t high = static_cast<int32_t>((static_cast<int64_t>(0x2aaaaaab) * product) >> 32);
    int32_t quotient = high >> 8;
    quotient += static_cast<int32_t>(static_cast<uint32_t>(quotient) >> 31);
    return quotient;
}

// divw: the signed quotient; the processor leaves the result undefined for a zero divisor and for the most negative number over -1
// (the translation answers 0, which no case of the game reaches: the screen's size is set before anything draws).
static inline int32_t DivideWord(int32_t dividend, int32_t divisor) {
    if (divisor == 0 || (divisor == -1 && dividend == INT32_MIN)) return 0;
    return dividend / divisor;
}

// The pixel position of a text box edge in the UI's units: the pixels shifted up to 2048 wide and divided by the real screen's width,
// or (pixels * 0x600) / the real screen's height.
static inline int32_t ToUnitsX(int32_t pixels) {
    return DivideWord(static_cast<int32_t>(static_cast<uint32_t>(pixels) << 11), gUIRealScreenWidth);
}

static inline int32_t ToUnitsY(int32_t pixels) { return DivideWord(pixels * 0x600, gUIRealScreenHeight); }

// ---- what the three functions that lay a text out share

// The box the text is laid out in, in the surface's pixels (left and top inside, right and bottom outside).
struct TextBox {
    int32_t mLeft;
    int32_t mTop;
    int32_t mRight;
    int32_t mBottom;
};

// The Mac's SetHorizontalJustification codes for the object's (0 left, 1 centre, 2 right).
constexpr TbTextFormatter::eJustification kHorizontal[3] = {TbTextFormatter::kLeft, TbTextFormatter::kCentre, TbTextFormatter::kRight};

// ... and the vertical ones (0 top, 1 centre, 2 bottom).
constexpr TbTextFormatter::eJustification kVertical[3] = {TbTextFormatter::kTop, TbTextFormatter::kCentre, TbTextFormatter::kBottom};

// Tells the formatter the object's justifications and works out the box: with the wrap byte clear, a box 0x4000 wide that the
// justification places against the object's left edge, its middle or its right edge; else the object's own width. The offsets, in
// the UI's units, move the box (the left and top for the left and centre justifications, pulling the right and bottom in for the right).
// A justification outside 0 to 2 is not told to the formatter and leaves the box empty (the registers the original leaves are the
// caller's); the vertical code is told on its own.
static inline TextBox Justify(InterfaceTextObjectSprite& object) {
    int32_t right = object.mPixelWidth;
    int32_t base = 0;
    const int32_t bottom = object.mPixelHeight;
    int32_t justify = object.mJustify;
    AdjustJustificationForHebrew(justify);
    TextBox box = {0, 0, 0, 0};
    if (justify == 1) {
        object.mFormatter.SetHorizontalJustification(kHorizontal[1]);
        if (object.mWrap == 0) {
            right = 0x4000;
            base = (object.mPixelWidth >> 1) - 0x2000;
        }
        box.mLeft = base + ToPixelsX(object.mOffsetX);
        box.mTop = ToPixelsY(object.mOffsetY);
        box.mRight = base + right;
        box.mBottom = bottom;
    } else if (justify == 0) {
        object.mFormatter.SetHorizontalJustification(kHorizontal[0]);
        if (object.mWrap == 0) right = 0x4000;
        box.mLeft = ToPixelsX(object.mOffsetX);
        box.mTop = ToPixelsY(object.mOffsetY);
        box.mRight = right;
        box.mBottom = bottom;
    } else if (justify == 2) {
        object.mFormatter.SetHorizontalJustification(kHorizontal[2]);
        if (object.mWrap == 0) {
            right = 0x4000;
            base = object.mPixelWidth - 0x4000;
        }
        box.mLeft = base;
        box.mTop = 0;
        box.mRight = (base + right) - ToPixelsX(object.mOffsetX);
        box.mBottom = bottom - ToPixelsY(object.mOffsetY);
    }
    const int32_t vertical = object.mVJustify;
    if (vertical >= 0 && vertical <= 2) object.mFormatter.SetVerticalJustification(kVertical[vertical]);
    return box;
}

// The font the object draws with: its id picks one of the language's 13 fonts in the current resolution group (the id is a signed byte
// compared as unsigned, so 0xff and every other id past 12 is no font).
static inline TbIFont* FontOf(const InterfaceTextObjectSprite& object) {
    const uint32_t id = static_cast<uint32_t>(static_cast<int32_t>(object.mFontId));
    if (id >= 13) return nullptr;
    return gText.mFonts[gText.mResolutionGroup * 13 + id].mFont;
}

// A colour from its five bytes. (The Mac's constructor is inline and zeroes them first; every field is set, so the out-of-line copy's
// work is not seen.)
static inline TbColour MakeColour(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue, uint8_t index) {
    TbColour colour;
    colour.mAlpha = alpha;
    colour.mRed = red;
    colour.mGreen = green;
    colour.mBlue = blue;
    colour.mIndex = index;
    return colour;
}

// Makes the render area over the object's surface, makes it the one the formatter draws into, switches the font to writing alpha, and
// clears the surface unless the pre-draw callback says it has drawn the background (a nonzero answer). The clearing is the rows of the
// area (its bytes per pixel, from its depth in bits) over the surface's size.
static inline void PrepareSurface(InterfaceTextObjectSprite& object, TbRenderArea& area) {
    uint8_t* pixels = static_cast<uint8_t*>(object.mEngineData.mWords.mPixels);
    MakeRenderArea(pixels, &area, object.mEngineData.mWords.mWidth, object.mEngineData.mWords.mHeight);
    LbDraw_SetCurrent(&area);
    gText.mRenderParams.UseAlphaWriteMode();
    if (pixels != nullptr && (object.mPreDraw == nullptr || object.mPreDraw(area) == 0)) {
        const uint32_t bytesPerPixel = (static_cast<uint32_t>(area.mDepth) + 7) >> 3;
        const uint32_t row = bytesPerPixel * object.mEngineData.mWords.mWidth;
        std::memset(pixels, 0, object.mEngineData.mWords.mHeight * row);
    }
}

// The rectangle the formatter reported, which is the text's, becomes the object's second rectangle (its place on the screen, in the UI's
// units); then the post-draw callback, and the surface goes to the texture.
static inline void FinishSurface(InterfaceTextObjectSprite& object, TbRenderArea& area, const TbRect& bounds) {
    const int32_t left = object.mRect.mLeft;
    const int32_t top = object.mRect.mTop;
    object.mRect2.Set(static_cast<int16_t>(left + ToUnitsX(bounds.left)), static_cast<int16_t>(top + ToUnitsY(bounds.top)),
                      static_cast<int16_t>(left + ToUnitsX(bounds.right)), static_cast<int16_t>(top + ToUnitsY(bounds.bottom)));
    if (object.mPostDraw != nullptr) object.mPostDraw(area);
    object.mEngineData.RefreshPartialTexture();
}
