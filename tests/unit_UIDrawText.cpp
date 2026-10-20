// Fakes and test entry points for the interface's text drawing (src/UI_drawtext.cpp; libtpw_unit_UIDrawText.dylib, driven by
// test_UIDrawText.py; see calllog.h).
//
// UI_DrawText calls the ltms text formatter and the font render parameters (unit w35a), the Bullfrog library's LbDraw_SetCurrent
// and LbDraw_SetClipRect (this unit's sibling, unit LbDrawLeaves), CText::IsHebrew and the Unicode to multibyte converter. Each is a
// fake that records the same event as the PowerPC side's: the name, the labels of the objects (see tpw_dt_world for the fixed ones;
// the formatter on the stack takes the next free label when it is constructed) and the values and texts it is given. The
// converter's fake writes the low bytes of the characters, so that what flows into the formatter shows in the log. The formatter's
// Draw writes the extent it is given from the case's queue.
#include <cstdint>
#include <cstring>
#include <new>

#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "LbDraw.h"
#include "TbColour.h"
#include "TbFontRenderParams.h"
#include "TbResource.h"
#include "TbTextFormatter.h"
#include "TbTextResourceManager.h"
#include "Text.h"
#include "UI_Base.h"
#include "calllog.h"
#include "platform/msl.h"
#include "tb_resource_texture.h"

using calllog::ev;
using calllog::lab;
using calllog::newlab;
using calllog::ret;

// ---- the game text object (the font table and the converter), with the constructors of its members faked as elsewhere
InterfaceFont::InterfaceFont() {}   // the fake leaves the object as it finds it
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
// Fakes of the member TbFontRender that w35a's TbTextFormatter owns (include/TbTextFormatter.h):
// this unit's fake formatter constructs and destroys it, and the real ones live in
// src/ltms_fontrender.cpp, which is not part of this library (added at the merge, 2026-10-12).
TbFontRender::TbFontRender() : mMethod(nullptr) {}
TbFontRender::~TbFontRender() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
CText gText;
CTextureSettings::CTextureSettings() {}
CTextureSettings gei_TextureSettings;

namespace {

// The converter object CText holds a pointer to: never dereferenced (the fake ConvertString ignores `this`).
alignas(16) unsigned char gConverterStore[64];
// What the PowerPC side's fonts are: arbitrary addresses, never dereferenced, labelled 10 + number.
uintptr_t FontToken(int index) { return 0x20000000u + 0x40u * static_cast<uintptr_t>(index); }

// A UTF-16 text as the log keeps it (ASCII kept, anything else '?', at most 159 characters), in a buffer of the caller's: the heap
// traffic the test compares must not include the fakes' own strings.
void Narrow(const char16_t* text, char (&out)[160]) {
    uint32_t n = 0;
    for (; text[n] != 0 && n < 159; ++n) out[n] = text[n] < 0x80 ? static_cast<char>(text[n]) : '?';
    out[n] = '\0';
}

}  // namespace

// ---- CText::IsHebrew
bool CText::IsHebrew() const {
    ev("IsHebrew", {lab(this)});
    return ret("IsHebrew") != 0;
}

// ---- the font render parameters
TbError TbFontRenderParams::SetFont(TbIFont* font) {
    ev("SetFont", {lab(this), lab(font)});
    return TbError(0);
}
TbError TbFontRenderParams::UseReColour(const TbColour& colour) {
    ev("UseReColour", {lab(this), lab(&colour), colour.mAlpha, colour.mRed, colour.mGreen, colour.mBlue, colour.mIndex});
    return TbError(0);
}

// ---- the text formatter
TbTextFormatter::TbTextFormatter() { ev("TbTextFormatter::TbTextFormatter", {newlab(this)}); }
TbTextFormatter::~TbTextFormatter() { ev("TbTextFormatter::~TbTextFormatter", {lab(this)}); }
TbError TbTextFormatter::SetHorizontalJustification(eJustification justification) {
    ev("SetHorizontalJustification", {lab(this), justification});
    return TbError(0);
}
TbError TbTextFormatter::SetVerticalJustification(eJustification justification) {
    ev("SetVerticalJustification", {lab(this), justification});
    return TbError(0);
}
TbError TbTextFormatter::Draw(const TbRect& rect, const unsigned char* text, TbFontRenderParams& params, TbRect* extent) {
    char shown[160] = "(null)";
    if (text != nullptr) {
        uint32_t n = 0;
        for (; text[n] != 0 && n < 159; ++n) shown[n] = text[n] < 0x80 ? static_cast<char>(text[n]) : '?';
        shown[n] = '\0';
    }
    ev("Draw", {lab(this), lab(&rect), rect.left, rect.top, rect.right, rect.bottom, lab(&params), lab(text), lab(extent)}, {shown});
    if (extent != nullptr) {
        extent->left = static_cast<int32_t>(ret("Draw"));
        extent->top = static_cast<int32_t>(ret("Draw"));
        extent->right = static_cast<int32_t>(ret("Draw"));
        extent->bottom = static_cast<int32_t>(ret("Draw"));
    }
    return TbError(0);
}

// ---- the Bullfrog library's drawing calls
void LbDraw_SetCurrent(TbRenderArea* area) { ev("LbDraw_SetCurrent", {lab(area)}); }
void LbDraw_SetClipRect(const TbRect* rect) {
    if (rect == nullptr) {
        ev("LbDraw_SetClipRect", {0, 0, 0, 0, 0});
    } else {
        ev("LbDraw_SetClipRect", {1, rect->left, rect->top, rect->right, rect->bottom});
    }
}

// ---- the converter: the low byte of each character, at most size - 2 of them, and a terminator
TbError TbIUnicodeToMB::ConvertString(unsigned char* out, const char16_t* source, uint16_t size) {
    uint32_t length = 0;
    while (source[length] != 0) ++length;
    char shown[160];
    Narrow(source, shown);
    ev("ConvertString", {lab(this), lab(out), size, length}, {shown});
    const uint32_t room = static_cast<uint16_t>(size - 2);
    uint32_t i = 0;
    for (; i < length && i < room; ++i) out[i] = static_cast<unsigned char>(source[i]);
    out[i] = 0;
    return TbError(0);
}

// UIMain_ConvertMulti (src/ui_main.cpp, linked for UIMain_ConvertString since unit w44) calls this one; this unit never does.
TbError TbIMBToUnicode::ConvertString(char16_t*, const unsigned char*, uint16_t) { return TbError(0); }

// ---- MSL
uint32_t wcslen(const char16_t* text) {
    uint32_t length = 0;
    while (text[length] != 0) ++length;
    char shown[160];
    Narrow(text, shown);
    ev("wcslen", {}, {shown});
    return length;
}

extern "C" {

// The world UI_DrawText and UIMain_ConvertString read: the resolution group, the 52 font slots, whether the language has a converter,
// the in-use word and the first bytes of the multibyte buffer (the rest is zeroed). fonts: 0 for a NULL font, otherwise the font's number
// 1..52 (the font object is FontToken(number - 1), labelled 10 + number - 1). The other labels: 1 the render parameters inside gText,
// 2 gUIRenderArea, 3 the multibyte buffer, 4 the converter, 5 gText.
void tpw_dt_world(int32_t group, const int32_t* fonts, int32_t converter, int32_t inUse, const uint8_t* mbFill, int32_t mbFillLength) {
    gText.mResolutionGroup = static_cast<uint32_t>(group);
    for (int i = 0; i < 52; ++i) {
        gText.mFonts[i].mFont = fonts[i] == 0 ? nullptr : reinterpret_cast<TbIFont*>(FontToken(fonts[i] - 1));
        if (fonts[i] != 0) tpw_cl_label(10 + fonts[i] - 1, gText.mFonts[i].mFont);
    }
    gText.mResourcePointers[1] = converter != 0 ? static_cast<void*>(gConverterStore) : nullptr;
    gUIRenderAreaInUse = inUse;
    std::memset(gUIMainMBBuffer, 0, sizeof gUIMainMBBuffer);
    std::memcpy(gUIMainMBBuffer, mbFill, static_cast<size_t>(mbFillLength));
    tpw_cl_label(1, &gText.mRenderParams);
    tpw_cl_label(2, &gUIRenderArea);
    tpw_cl_label(3, gUIMainMBBuffer);
    tpw_cl_label(4, gConverterStore);
    tpw_cl_label(5, &gText);
}

// The multibyte buffer's first `n` bytes.
void tpw_dt_mb(uint8_t* out, int32_t n) { std::memcpy(out, gUIMainMBBuffer, static_cast<size_t>(n)); }

// UI_DrawText. text may be NULL, extent may be NULL. The colour object the call is given is label 6.
void tpw_dt_draw(const char16_t* text, const int32_t* rect, const uint8_t* colour, int32_t font, int32_t horizontal, int32_t vertical,
                 int32_t wrap, int32_t* extent, int32_t onCurrent) {
    // TbColour's constructor is not part of this unit: the colour is five bytes in raw storage.
    alignas(TbColour) unsigned char value[sizeof(TbColour)];
    std::memcpy(value, colour, sizeof value);
    tpw_cl_label(6, value);
    const TbRect box = {rect[0], rect[1], rect[2], rect[3]};
    UI_DrawText(text, box, *reinterpret_cast<const TbColour*>(value), static_cast<int8_t>(font), static_cast<int8_t>(horizontal), static_cast<int8_t>(vertical), wrap,
                reinterpret_cast<TbRect*>(extent), onCurrent);
}

// UIMain_ConvertString: 1 when the answer is the multibyte buffer.
int32_t tpw_dt_convert(const char16_t* text) { return UIMain_ConvertString(text) == gUIMainMBBuffer ? 1 : 0; }

// MakeRenderArea over an area whose every byte starts as 0xa5, with the engine's pixel format set to the eight words `format`
// (dwSize, dwFlags, dwFourCC, dwRGBBitCount and the four masks). out: the area's words (masks 4, depth, palettised, pixels,
// width, height, stride). With hasArea 0 the call gets NULL (and out is not written).
void tpw_dt_make(int32_t hasArea, uint32_t pixels, int32_t width, int32_t height, const uint32_t* format, uint32_t* out) {
    std::memcpy(&gei_TextureSettings.mPixelFormat, format, 32);
    alignas(TbRenderArea) unsigned char store[sizeof(TbRenderArea)];
    std::memset(store, 0xa5, sizeof store);
    TbRenderArea* area = reinterpret_cast<TbRenderArea*>(store);
    MakeRenderArea(reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(pixels)), hasArea != 0 ? area : nullptr, width, height);
    if (hasArea == 0) return;
    std::memcpy(out, area->mF00, 16);
    out[4] = static_cast<uint32_t>(area->mDepth);
    out[5] = static_cast<uint32_t>(area->mPalettised);
    out[6] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(area->mData));
    out[7] = static_cast<uint32_t>(area->mWidth);
    out[8] = static_cast<uint32_t>(area->mPictureHeight);
    out[9] = static_cast<uint32_t>(area->mStride);
}

// AdjustJustificationForHebrew: the justification after the call.
int32_t tpw_dt_adjust(int32_t value) {
    int32_t justification = value;
    AdjustJustificationForHebrew(justification);
    return justification;
}

// The three InterfaceFont members on a font with the glyph count `count`, the spacing `spacing` and, when hasTable, the table (8 bytes
// a glyph; the caller makes it big enough for every character). op 0 GetWidth(text, first, last), 1 GetCharWidth(text[0]), 2
// LastCharIn. after gets the font's count, its spacing and whether its table pointer is still the table (1) or NULL (2).
int32_t tpw_dt_font(int32_t op, int32_t count, int32_t spacing, int32_t hasTable, const uint8_t* table, const char16_t* text,
                    int32_t first, int32_t last, int32_t* after) {
    alignas(InterfaceFont) unsigned char store[sizeof(InterfaceFont)];
    std::memset(store, 0xa5, sizeof store);
    InterfaceFont* font = new (store) InterfaceFont();
    font->mF00 = static_cast<uint16_t>(count);
    font->mF02 = static_cast<uint16_t>(spacing);
    font->mBitmap = hasTable != 0 ? const_cast<uint8_t*>(table) : nullptr;
    int32_t answer;
    switch (op) {
    case 0: answer = font->GetWidth(text, static_cast<int16_t>(first), static_cast<int16_t>(last)); break;
    case 1: answer = font->GetCharWidth(text != nullptr ? text[0] : 0); break;
    default: answer = font->LastCharIn(text, static_cast<int16_t>(first), static_cast<int16_t>(last)); break;
    }
    after[0] = font->mF00;
    after[1] = font->mF02;
    after[2] = font->mBitmap == nullptr ? 2 : font->mBitmap == table ? 1 : 0;
    return answer;
}

}  // extern "C"
