// The text draw objects (main app of SimTheme Park 2.04b3, 0x1018d11c..0x101902a8; the inventory files the range under
// ui_textmanager.cpp): InterfaceTextObjectSprite (constructor, destructor, UpdateTextures, Change), InterfaceCursorPosTextObjectSprite
// (constructor, destructor, Query) and InterfaceTitleTextObject (constructor, destructor, UpdateTextures). See include/ui_DrawObject.h for
// the classes. The other text objects of the game (cash, number, rank, percent...) are further subclasses made elsewhere.
#include <cstring>

#include "LbPalette.h"
#include "Text.h"
#include "ui_DrawObject.h"

CoolRect gUIQueryRect;

namespace {

// ---- arithmetic the compiler's way

// (units * screen width) / 2048 truncated toward zero: srawi 11 and addze, as the sprite object does for its width.
int32_t ToPixelsX(int32_t units) {
    const int32_t product = units * gUIRealScreenWidth;
    int32_t quotient = product >> 11;
    if (product < 0 && (product & 0x7ff) != 0) quotient += 1;
    return quotient;
}

// (units * screen height) / 1536 as the compiler's multiply-high by 0x2aaaaaab, shift by 8 and added sign do it (truncated toward zero).
int32_t ToPixelsY(int32_t units) {
    const int32_t product = units * gUIRealScreenHeight;
    const int32_t high = static_cast<int32_t>((static_cast<int64_t>(0x2aaaaaab) * product) >> 32);
    int32_t quotient = high >> 8;
    quotient += static_cast<int32_t>(static_cast<uint32_t>(quotient) >> 31);
    return quotient;
}

// divw: the signed quotient; the processor leaves the result undefined for a zero divisor and for the most negative number over -1
// (the translation answers 0, which no case of the game reaches: the screen's size is set before anything draws).
int32_t DivideWord(int32_t dividend, int32_t divisor) {
    if (divisor == 0 || (divisor == -1 && dividend == INT32_MIN)) return 0;
    return dividend / divisor;
}

// The pixel position of a text box edge in the UI's units: the pixels shifted up to 2048 wide and divided by the real screen's width,
// or (pixels * 0x600) / the real screen's height.
int32_t ToUnitsX(int32_t pixels) {
    return DivideWord(static_cast<int32_t>(static_cast<uint32_t>(pixels) << 11), gUIRealScreenWidth);
}
int32_t ToUnitsY(int32_t pixels) { return DivideWord(pixels * 0x600, gUIRealScreenHeight); }

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
TextBox Justify(InterfaceTextObjectSprite& object) {
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
TbIFont* FontOf(const InterfaceTextObjectSprite& object) {
    const uint32_t id = static_cast<uint32_t>(static_cast<int32_t>(object.mFontId));
    if (id >= 13) return nullptr;
    return gText.mFonts[gText.mResolutionGroup * 13 + id].mFont;
}

// A colour from its five bytes. (The Mac's constructor is inline and zeroes them first; every field is set, so the out-of-line copy's
// work is not seen.)
TbColour MakeColour(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue, uint8_t index) {
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
void PrepareSurface(InterfaceTextObjectSprite& object, TbRenderArea& area) {
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
void FinishSurface(InterfaceTextObjectSprite& object, TbRenderArea& area, const TbRect& bounds) {
    const int32_t left = object.mRect.mLeft;
    const int32_t top = object.mRect.mTop;
    object.mRect2.Set(static_cast<int16_t>(left + ToUnitsX(bounds.left)), static_cast<int16_t>(top + ToUnitsY(bounds.top)),
                      static_cast<int16_t>(left + ToUnitsX(bounds.right)), static_cast<int16_t>(top + ToUnitsY(bounds.bottom)));
    if (object.mPostDraw != nullptr) object.mPostDraw(area);
    object.mEngineData.RefreshPartialTexture();
}

}  // namespace

// ---- InterfaceTextObjectSprite

// Left justified, vertically centred, no wrap, no font, the one colour, no offsets, the text colour white.

// (The formatter, the list and the surface are destroyed by the members' destructors, in that order, before the sprite object's base.)
InterfaceTextObjectSprite::~InterfaceTextObjectSprite() {}

// Draws `text` in white on a cleared surface (the sprite's colour word tints it); nothing without a text. The font is the object's; the
// colour is the one white of the palette (found each time) when the colour mode is 0, else whatever the font parameters have.
void InterfaceTextObjectSprite::UpdateTextures(char16_t* text) {
    TbRenderArea area;
    TbRect bounds = {0, 0, 0, 0};
    if (text == nullptr) return;
    const TextBox box = Justify(*this);
    gText.mRenderParams.SetFont(FontOf(*this));
    if (mColourMode == 0) {
        const uint8_t index = LbPalette_FindColour(&_lbGlobalPalette, 0xff, 0xff, 0xff);
        const TbColour white = MakeColour(0xff, 0xff, 0xff, 0xff, index);
        gText.mRenderParams.UseReColour(white);
    }
    PrepareSurface(*this, area);
    const TbRect rect = {box.mLeft, box.mTop, box.mRight, box.mBottom};
    mFormatter.Draw(rect, UIMain_ConvertString(text), gText.mRenderParams, &bounds);
    FinishSurface(*this, area, bounds);
}

void InterfaceTextObjectSprite::Change(uint32_t flags, DOChange* change) {
    if (change == nullptr || flags == 0) return;
    InterfaceDrawObjectSprite::Change(flags, change);
    if ((flags & 0x7fd00) == 0) return;
    const DOTextStyle* style = change->mText;
    if (style == nullptr) return;
    if ((flags & 0x400) != 0) mJustify = static_cast<int8_t>(style->mF04);
    if ((flags & 0x800) != 0) mVJustify = static_cast<int8_t>(style->mF05);
    if ((flags & 0x1000) != 0) mWrap = style->mF06[0];
    if ((flags & 0x2000) != 0) mF0DF = style->mF06[1];
    if ((flags & 0x4000) != 0) std::memcpy(&mOffsetX, &style->mF0A[0], sizeof mOffsetX);
    if ((flags & 0x8000) != 0) std::memcpy(&mOffsetY, &style->mF0A[2], sizeof mOffsetY);
    if ((flags & 0x10000) != 0) mFontId = static_cast<int8_t>(style->mF06[2]);
    if ((flags & 0x20000) != 0) mColourMode = style->mF09;
    if ((flags & 0x40000) != 0) {
        uint8_t* colour = TextColour();
        colour[0] = style->mColour[0];
        colour[1] = style->mColour[1];
        colour[2] = style->mColour[2];
        colour[3] = style->mF13;
        mSprite.mF24 = static_cast<int32_t>((static_cast<uint32_t>(colour[0]) << 24) | (static_cast<uint32_t>(colour[1]) << 16) |
                                            (static_cast<uint32_t>(colour[2]) << 8) | colour[3]);
    }
    if ((flags & 0x100) != 0 && style->mText != nullptr) UpdateTextures(style->mText);
}

// ---- InterfaceTitleTextObject

// The text object's constructor, inline (the vtable pointer goes to the text object's, then to this class's).
InterfaceTitleTextObject::InterfaceTitleTextObject(void* data) : InterfaceTextObjectSprite(data) {}

InterfaceTitleTextObject::~InterfaceTitleTextObject() {}

// The text object's drawing with a shadow: the text in mid grey (0x80 all over) in the box moved two pixels left and down (the right and
// bottom edges too), then in white in the box itself; the font colour of the object is not used. The formatter's rectangle out is
// the same variable for both: the second draw is given what the first reported.
void InterfaceTitleTextObject::UpdateTextures(char16_t* text) {
    TbRenderArea area;
    TbRect bounds = {0, 0, 0, 0};
    if (text == nullptr) return;
    const TextBox box = Justify(*this);
    gText.mRenderParams.SetFont(FontOf(*this));
    PrepareSurface(*this, area);
    gText.mRenderParams.UseReColour(MakeColour(0x80, 0x80, 0x80, 0x80, 0));
    const TbRect shadow = {box.mLeft - 2, box.mTop + 2, box.mRight - 2, box.mBottom + 2};
    mFormatter.Draw(shadow, UIMain_ConvertString(text), gText.mRenderParams, &bounds);
    gText.mRenderParams.UseReColour(MakeColour(0xff, 0xff, 0xff, 0xff, 0));
    const TbRect rect = {box.mLeft, box.mTop, box.mRight, box.mBottom};
    mFormatter.Draw(rect, UIMain_ConvertString(text), gText.mRenderParams, &bounds);
    FinishSurface(*this, area, bounds);
}

// ---- InterfaceCursorPosTextObjectSprite

InterfaceCursorPosTextObjectSprite::InterfaceCursorPosTextObjectSprite(void* data) : InterfaceTextObjectSprite(data) {
    mFormatter.mMeasurePositions = 1;
}

InterfaceCursorPosTextObjectSprite::~InterfaceCursorPosTextObjectSprite() {}

// what 1: where character `value` is, in the UI's units, as the left and top of the rectangle (the right and bottom are 0); the end of the
// text when the index is negative or past the last character laid out (or nothing has been). what 2: the rectangle the text at
// `value` (an address; 0 answers 0) would take, measured in the object's box with its justification and font, less nothing drawn; its
// bottom is cut to the object's height. The answer is the address of gUIQueryRect.
intptr_t InterfaceCursorPosTextObjectSprite::Query(uint32_t what, intptr_t value) {
    if (what == 1) {
        const int32_t index = static_cast<int32_t>(value);
        if (index >= 0) {
            const TbPoint* positions = mFormatter.GetCharPositionArray();
            const int32_t count = mFormatter.GetCharPositionArraySize();
            if (count != 0 && index < count) {
                gUIQueryRect.Set(static_cast<int16_t>(ToUnitsX(positions[index].x)), static_cast<int16_t>(ToUnitsY(positions[index].y)), 0, 0);
                return reinterpret_cast<intptr_t>(&gUIQueryRect);
            }
        }
        gUIQueryRect.Set(static_cast<int16_t>(ToUnitsX(mFormatter.mCaret.x)), static_cast<int16_t>(ToUnitsY(mFormatter.mCaret.y)), 0, 0);
        return reinterpret_cast<intptr_t>(&gUIQueryRect);
    }
    if (what != 2) return 0;
    if (value == 0) return 0;
    const TextBox box = Justify(*this);
    gText.mRenderParams.SetFont(FontOf(*this));
    const int8_t mode = static_cast<int8_t>(mColourMode);
    if (mode == 1) {
        gText.mRenderParams.UsePreColoured();
    } else if (mode == 0) {
        const uint8_t* colour = TextColour();
        gText.mRenderParams.UseReColour(MakeColour(0xff, colour[0], colour[1], colour[2], 0));
    }
    TbRect bounds = {0, 0, 0, 0};
    const TbRect rect = {box.mLeft, box.mTop, box.mRight, box.mBottom};
    mFormatter.Measure(rect, UIMain_ConvertString(reinterpret_cast<char16_t*>(value)), gText.mRenderParams, &bounds);
    gUIQueryRect.Set(static_cast<int16_t>(ToUnitsX(bounds.left)), static_cast<int16_t>(ToUnitsY(bounds.top)),
                     static_cast<int16_t>(ToUnitsX(bounds.right)), static_cast<int16_t>(ToUnitsY(bounds.bottom)));
    const int32_t height = static_cast<int32_t>(mRect.mBottom) - static_cast<int32_t>(mRect.mTop);
    if (gUIQueryRect.mBottom > height) gUIQueryRect.mBottom = static_cast<int16_t>(height);
    return reinterpret_cast<intptr_t>(&gUIQueryRect);
}
