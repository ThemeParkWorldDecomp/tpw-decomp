// ltms TbFontRender: the constructor, destructor, Initialise and DrawChar (0x10001fdc, 0x10002050, 0x100020e4, 0x100021fc),
// and three TbFontRenderParams members (UseReColour 0x10002800, UsePreColoured 0x10002888, SetFont 0x100029d0). The class's
// other members are the render methods' (untranslated) or in src/tb_fontrenderparams.cpp.
#include "TbFontRender.h"

#include "TbResource.h"

namespace {

// The five flag bytes at +0x44..+0x48 (the first five bytes of slot 16 on the host): a colour and its palette index.
uint8_t* Flags(TbFontRenderParams& params) { return reinterpret_cast<uint8_t*>(&params.mState[16]); }

}  // namespace

// 0x10001fdc: the parameters' own constructor, then no method.
TbFontRender::TbFontRender() : mMethod(nullptr) {}

// 0x10002050: the parameters' destructor (member destruction); the deleting form is the compiler's `delete`.
TbFontRender::~TbFontRender() {}

// 0x100020e4. The checks come first and are signed: a style below 3 and a type below 5, so a NEGATIVE style or type passes
// and indexes the table backwards (the table is read as a flat run of 30 words, as the original does). The parameters are
// copied (operator=, which takes a reference on the font and never lets go of the old one), and the style and type are read
// again from the copy. The blend word is the copy's slot at +0x10, read before them.
TbError TbFontRender::Initialise(const TbFontRenderParams& params) {
    mMethod = nullptr;
    if (params.GetStyle() >= 3) return TbError(-1);
    if (params.GetType() >= 5) return TbError(-1);
    mParams = params;
    const int32_t blend = static_cast<int32_t>(static_cast<uint32_t>(mParams.mState[3]));
    const int32_t style = mParams.GetStyle();
    const int32_t type = mParams.GetType();
    TbFontRenderMethod* const* flat = &gFontRenderMethods[0][0][0];
    mMethod = flat[blend * 15 + type * 3 + style];
    if (mMethod == nullptr) return TbError(-1);
    return TbError(0);
}

// 0x100021fc
TbError TbFontRender::DrawChar(const TbPoint& at, uint16_t glyph, TbRect& drawn) {
    if (mMethod == nullptr) return TbError(-1);
    return mMethod->DrawChar(at, glyph, drawn, mParams);
}

// 0x10002800: with a font whose caps include 2, the colour (four bytes and the index byte) and style 1 (one colour).
TbError TbFontRenderParams::UseReColour(const TbColour& colour) {
    if (mState[0] == 0 || (static_cast<uint32_t>(mState[1]) & 2) == 0) return TbError(-1);
    const uint8_t* source = reinterpret_cast<const uint8_t*>(&colour);
    uint8_t* flags = Flags(*this);
    flags[0] = source[0];
    flags[1] = source[1];
    flags[2] = source[2];
    flags[3] = source[3];
    flags[4] = source[4];
    mState[2] = 1;
    return TbError(0);
}

// 0x10002888: with a font whose caps include 1, style 0 (the font's own colours).
TbError TbFontRenderParams::UsePreColoured() {
    if (mState[0] == 0 || (static_cast<uint32_t>(mState[1]) & 1) == 0) return TbError(-1);
    mState[2] = 0;
    return TbError(0);
}

// 0x100029d0: style 3 first, then the old font is released (before the new one is held, so the same font twice is let go
// first) and forgotten, then the new one is held and its caps taken. A NULL font leaves the caps as they were.
TbError TbFontRenderParams::SetFont(TbIFont* font) {
    mState[2] = 3;
    if (mState[0] != 0) {
        reinterpret_cast<TbIFont*>(mState[0])->Release();
        mState[0] = 0;
    }
    mState[0] = reinterpret_cast<uintptr_t>(font);
    if (mState[0] != 0) {
        font->AddRef();
        mState[1] = font->GetCaps();
    }
    return TbError(0);
}

// 0x10002af4: the font (slot 0, +4), after a reference has been taken on it (the font's virtual AddRef, slot 3) when there is one.
// Nobody lets the reference go (CreateInfoList leaks one per call).
TbIFont* TbFontRenderParams::GetFont() const {
    TbIFont* font = reinterpret_cast<TbIFont*>(mState[0]);
    if (font != nullptr) font->AddRef();
    return reinterpret_cast<TbIFont*>(mState[0]);
}

// ---- unit w45a: the rest of the parameters' members (ltms 0x100025e8, 0x10002718, 0x100028f8, 0x10002ab8, 0x10002b70)

// 0x100025e8: nothing for a copy of itself. Otherwise: the blend word (+0x10), the five flag bytes (+0x44 as a word, +0x48 as
// a byte), the font (+4) with a reference taken on it WITHOUT a NULL test and without letting go of the old one, then the
// caps (+8), the style (+0xc), the render area and the point after it (+0x14..+0x40) and the word at +0x4c.
TbFontRenderParams& TbFontRenderParams::operator=(const TbFontRenderParams& other) {
    if (&other == this) return *this;
    mState[3] = other.mState[3];
    const uint8_t* source = reinterpret_cast<const uint8_t*>(&other.mState[16]);
    uint8_t* flags = Flags(*this);
    for (int32_t i = 0; i < 5; ++i) flags[i] = source[i];
    mState[0] = other.mState[0];
    reinterpret_cast<TbIFont*>(mState[0])->AddRef();
    mState[1] = other.mState[1];
    mState[2] = other.mState[2];
    for (int32_t i = 4; i < 16; ++i) mState[i] = other.mState[i];
    mState[18] = other.mState[18];
    return *this;
}

// 0x10002718: with a font whose caps include 4, the render area and the point become the parameters' (the area's ten words
// to +0x14..+0x38, the point's two to +0x3c and +0x40) and the style is 2 (textured); else -1 and nothing changes.
TbError TbFontRenderParams::UseTexture(const TbRenderArea& area, const TbPoint& point) {
    if (mState[0] == 0 || (static_cast<uint32_t>(mState[1]) & 4) == 0) return TbError(-1);
    *reinterpret_cast<TbRenderArea*>(&mState[4]) = area;
    mState[14] = static_cast<uint32_t>(point.x);
    mState[15] = static_cast<uint32_t>(point.y);
    mState[2] = 2;
    return TbError(0);
}

// 0x100028f8: with a font whose caps include 8, the blend word is 1 (blended writing); else -1.
TbError TbFontRenderParams::UseBlendedMode() {
    if (mState[0] == 0 || (static_cast<uint32_t>(mState[1]) & 8) == 0) return TbError(-1);
    mState[3] = 1;
    return TbError(0);
}

// 0x10002ab8
int32_t TbFontRenderParams::GetStyle() const { return static_cast<int32_t>(static_cast<uint32_t>(mState[2])); }

// 0x10002b70: the font's virtual GetType (slot 0x30), 5 without a font.
int32_t TbFontRenderParams::GetType() const {
    TbIFont* font = reinterpret_cast<TbIFont*>(mState[0]);
    if (font == nullptr) return 5;
    return static_cast<int32_t>(font->GetType());
}
