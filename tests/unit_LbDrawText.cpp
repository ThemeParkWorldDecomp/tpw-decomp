// Unit w224: recording font and sprite callees; all unit-to-unit calls are real.
#include "LbDrawText.h"
#include "LbDraw.h"
#include "LbRect.h"
#include "calllog.h"
#include "platform/arena.h"
#include <cstring>
#include <cstddef>
using namespace calllog;
// Drawing state belongs to bfl_lbdraw_leaves.cpp in the complete library.
TbRenderArea* _lbpCurrentRA;
TbRect _lbCurrentClipRect;
uint8_t* _lbCurrentOriginPtr;
namespace {
uint32_t widths[256], heights[256], advances[256];
uint32_t writeMask;
const TbSprite* fontBase;
TbResult Select(void** slot, void* font) {
    ev("Select", {lab(slot), lab(font), lab(*slot), lab(gLbTextRender)});
    if (ret("WriteFont")) *slot = font;
    return {static_cast<int32_t>(ret("Select"))};
}
void Release(void* oldFont, const TbTextRender* render, void* font) {
    ev("Release", {lab(oldFont), lab(render), lab(font), lab(gLbTextRender)});
}
void Measure(void* font, TbSize* size, uint32_t ch) {
    ev("Measure", {lab(font), ch, size->width, size->height});
    if (writeMask & 1) size->width = widths[ch & 255];
    if (writeMask & 2) size->height = heights[ch & 255];
}
uint32_t Draw(void* font, int32_t x, int32_t y, uint32_t ch, TbColour c) {
    ev("Draw", {lab(font), x, y, ch, c.mAlpha, c.mRed, c.mGreen, c.mBlue, c.mIndex});
    return advances[ch & 255];
}
TbTextRender tables[4];
}
void LbDraw_Sprite(int32_t x, int32_t y, const TbSprite* p) {
    ev("Sprite", {x, y, p - fontBase});
    int64_t width = ret("SpriteWidth", -1);
    if (width >= 0) const_cast<TbSprite*>(p)->mWidth = static_cast<uint16_t>(width);
}
void LbDraw_OneColourSprite(int32_t x, int32_t y, const TbSprite* p, TbColour c) {
    ev("OneColour", {x, y, p - fontBase, c.mAlpha, c.mRed, c.mGreen, c.mBlue, c.mIndex});
    int64_t width = ret("SpriteWidth", -1);
    if (width >= 0) const_cast<TbSprite*>(p)->mWidth = static_cast<uint16_t>(width);
}
extern "C" {
uint64_t tpw_ldt_alloc(uint32_t n) { return reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(n)); }
uint64_t tpw_ldt_addr(const char* name) {
#define ENTRY(n) if (!std::strcmp(name, #n)) return reinterpret_cast<uintptr_t>(&n);
    ENTRY(gLbTextFont)
    ENTRY(gLbTextRender)
    ENTRY(gLbTextSelectSense)
    ENTRY(gLbCheapTextColour)
    ENTRY(gLbCheapTextGuard)
    ENTRY(gLbCheapTextIndex)
    ENTRY(gLbCheapUnicodeTextColour)
    ENTRY(gLbCheapUnicodeTextGuard)
    ENTRY(gLbCheapUnicodeTextIndex)
    ENTRY(_lbpCurrentRA)
    ENTRY(_lbCurrentClipRect)
    ENTRY(_lbCurrentOriginPtr)
    ENTRY(_lbSpriteRender)
    ENTRY(_lbOneColourSpriteRender)
#undef ENTRY
    return 0;
}
uint64_t tpw_ldt_table(uint32_t i) { return reinterpret_cast<uintptr_t>(&tables[i]); }
void tpw_ldt_config(const uint32_t* w, const uint32_t* h, const uint32_t* a, uint32_t mask, uint64_t font) {
    std::memcpy(widths, w, sizeof widths);
    std::memcpy(heights, h, sizeof heights);
    std::memcpy(advances, a, sizeof advances);
    writeMask = mask;
    fontBase = reinterpret_cast<const TbSprite*>(font);
    for (int i = 0; i < 4; ++i) tables[i] = {(i & 1) ? Select : nullptr, (i & 2) ? Release : nullptr, Measure, Draw};
}
int32_t tpw_ldt_layout(int64_t* o) {
    int64_t v[] = {sizeof(TbSprite), offsetof(TbSprite, mWidth), offsetof(TbSprite, mHeight), sizeof(TbRenderArea), offsetof(TbRenderArea, mStride), sizeof(TbTextRender)};
    std::memcpy(o, v, sizeof v);
    return 6;
}
uint32_t tpw_ldt_call(uint32_t op, const uint64_t* a, const uint8_t* cb) {
    TbColour c;
    std::memcpy(&c, cb, 5);
    int32_t x = a[0], y = a[1];
    auto* z = reinterpret_cast<TbSize*>(a[3]);
    auto* r = reinterpret_cast<const TbRect*>(a[4]);
    uint32_t f = a[5], count = a[6], nl = a[7], width = a[8], lw = a[9], ch = a[10];
    void* font = reinterpret_cast<void*>(a[12]);
    switch (op) {
    case 0: LbDraw_Text(x, y, reinterpret_cast<const char*>(a[2]), c); return 0;
    case 1: LbDraw_UnicodeText(x, y, reinterpret_cast<const char16_t*>(a[2]), c); return 0;
    case 2: LbDraw_FormattedText(r, reinterpret_cast<const char*>(a[2]), f, c); return 0;
    case 3: LbDraw_UnicodeFormattedText(r, reinterpret_cast<const char16_t*>(a[2]), f, c); return 0;
    case 4: return LbDraw_GetTextSize(reinterpret_cast<const char*>(a[2]), z);
    case 5: return LbDraw_GetUnicodeTextSize(reinterpret_cast<const char16_t*>(a[2]), z);
    case 6: return LbDraw_GetFormattedTextHeight(width, reinterpret_cast<const char*>(a[2]));
    case 7: return LbDraw_GetUnicodeFormattedTextHeight(width, reinterpret_cast<const char16_t*>(a[2]));
    case 8: LbDraw_CheapText(x, y, reinterpret_cast<const char*>(a[2]), c); return 0;
    case 9: LbDraw_UnicodeCheapText(x, y, reinterpret_cast<const char16_t*>(a[2]), c); return 0;
    case 10: SpriteRender_OnMeasure(font, z, ch); return 0;
    case 11: return SpriteRender_OnDrawChar(font, x, y, ch, c);
    case 12: OneColourSpriteRender_OnMeasure(font, z, ch); return 0;
    case 13: return OneColourSpriteRender_OnDrawChar(font, x, y, ch, c);
    case 14: return LbDraw_SetTextFont(reinterpret_cast<const TbTextRender*>(a[11]), font).mCode;
    case 15: return LbDraw_GetTextCharacterWidth(ch);
    case 16: return LbDraw_GetTextCharacterHeight(ch);
    case 17: Draw_CheapText<char>(x, y, reinterpret_cast<const char*>(a[2]), c); return 0;
    case 18: Draw_CheapText<char16_t>(x, y, reinterpret_cast<const char16_t*>(a[2]), c); return 0;
    case 19: return Text_MeasureFormattedHeight<char>(width, reinterpret_cast<const char*>(a[2]));
    case 20: return Text_MeasureFormattedHeight<char16_t>(width, reinterpret_cast<const char16_t*>(a[2]));
    case 21: Draw_FormattedText<char>(r, reinterpret_cast<const char*>(a[2]), f, c); return 0;
    case 22: Draw_FormattedText<char16_t>(r, reinterpret_cast<const char16_t*>(a[2]), f, c); return 0;
    case 23: Draw_Text<char>(x, y, reinterpret_cast<const char*>(a[2]), c); return 0;
    case 24: Draw_Text<char16_t>(x, y, reinterpret_cast<const char16_t*>(a[2]), c); return 0;
    case 25: draw_text_chars<char>(x, y, reinterpret_cast<const char*>(a[2]), count, c); return 0;
    case 26: draw_text_chars<char16_t>(x, y, reinterpret_cast<const char16_t*>(a[2]), count, c); return 0;
    case 27: draw_text_line<char>(x, y, reinterpret_cast<const char*>(a[2]), count, c, f, nl, width, lw); return 0;
    case 28: draw_text_line<char16_t>(x, y, reinterpret_cast<const char16_t*>(a[2]), count, c, f, nl, width, lw); return 0;
    case 29: return word_width<char>(reinterpret_cast<const char*>(a[2]), reinterpret_cast<uint32_t*>(a[3]));
    case 30: return word_width<char16_t>(reinterpret_cast<const char16_t*>(a[2]), reinterpret_cast<uint32_t*>(a[3]));
    case 31: return Text_Measure<char>(reinterpret_cast<const char*>(a[2]), z);
    case 32: return Text_Measure<char16_t>(reinterpret_cast<const char16_t*>(a[2]), z);
    case 33: return LbRect_Width(r);
    case 34: return LbRect_Height(r);
    }
    return 0;
}
}
