// TbFontRender.cpp: name proven by __sinit_TbFontRender_cpp in the ltms library.

#include "LbDraw.h"
#include "TbFontBits.h"
#include "TbFontRender.h"
#include "TbIRLE4bitFont.h"
#include "TbResource.h"
#include <cstring>
#include <new>
#include <vector>

// ltms TbFontRender.cpp, the 4-bit font render method's core (unit w45a; ltms symbols shared 0x10000bf0..0x10001f64): the
// method's constructor and destructors, DrawChar (decodes one glyph and hands it to the writer for the render area's
// pixel format), CalculateSourceRect, GetRotate and the decode buffer FourBit::FontData. The nybble and bit iterators
// (include/TbFontBits.h) are inline. The writers (DrawCharTo*) and the four method objects are unit w45b's.

// ---- the decode buffer

bool FourBit::FontData::sMade = false;

// 0x10001d30
FourBit::FontData::~FontData() { delete[] mBlock; }

// ---- the glyph rectangle

// 0x10001dbc. The rectangle moved to `point` is cut to the clip rectangle (a copy of it is taken first), and the source
// starts where the glyph does plus the amount the point lies left of or above the clip. The area (third argument) is
// not read.
TbRect Tb4BitFontRenderMethod::CalculateSourceRect(const TbRect& rect, const TbPoint& point, const TbRenderArea& area) {
    const int32_t width = rect.right - rect.left;
    const int32_t height = rect.bottom - rect.top;
    const TbRect moved = {point.x, point.y, point.x + width, point.y + height};
    const TbRect* const clipRect = LbDraw_GetClipRect();
    const TbRect clip = {clipRect->left, clipRect->top, clipRect->right, clipRect->bottom};
    const TbRect inside = moved.Intersection(clip);
    int32_t left = rect.left;
    int32_t top = rect.top;
    if (point.x < clip.left) left += clip.left - point.x;
    if (point.y < clip.top) top += clip.top - point.y;
    return {left, top, left + (inside.right - inside.left), top + (inside.bottom - inside.top)};
}

// 0x10001f64: the count starts at 32 and the mask 0x80000000; both step down until the mask has a bit in common with the
// argument. For 0 the count also steps once more past 0 (to 0xffffffff) before the loop gives up.
int32_t Tb4BitFontRenderMethod::GetRotate(uint32_t mask) {
    uint32_t count = 32;
    uint32_t bit = 0x80000000u;
    while ((mask & bit) == 0) {
        const bool atZero = count == 0;
        --count;
        if (atZero) break;
        bit >>= 1;
    }
    if (count >= 8) return static_cast<int32_t>(count - 8);
    return static_cast<int32_t>(count + 0x18);
}

// ltms TbFontRender: the constructor, destructor, Initialise and DrawChar (0x10001fdc, 0x10002050, 0x100020e4, 0x100021fc),
// and three TbFontRenderParams members (UseReColour 0x10002800, UsePreColoured 0x10002888, SetFont 0x100029d0). The class's
// other members are the render methods' (untranslated) or in src/tb_fontrenderparams.cpp.

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
