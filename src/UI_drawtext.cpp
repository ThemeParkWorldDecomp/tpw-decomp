// The interface's text drawing (main app of SimTheme Park 2.04b3): UI_DrawText 0x1018b578, MakeRenderArea 0x1018b360 and
// AdjustJustificationForHebrew 0x1018b2c8 (ui_utils.cpp), UIMain_ConvertString 0x10181a7c (ui_main.cpp) and the three
// InterfaceFont width members 0x1016915c, 0x10169264, 0x101692d4 (the inventory files them under FS_MasterSave.cpp).
// UI_DrawText hands the text to the ltms text formatter (unit w35a), which draws through the Bullfrog library's current
// area, viewport and clip rectangle (src/bfl_lbdraw_leaves.cpp). The declarations are in include/UI_Base.h and
// include/InterfaceFont.h.
#include <cstring>

#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "LbDraw.h"
#include "TbColour.h"
#include "TbFontRenderParams.h"
#include "TbGraphicFile.h"
#include "TbResource.h"
#include "TbTextFormatter.h"
#include "Text.h"
#include "UI_Base.h"
#include "platform/msl.h"
#include "tb_resource_texture.h"

// ---- the file's globals (__sinit_ui_utils_cpp makes the area with the default constructor, which zeroes what this does)
TbRenderArea gUIRenderArea{TbRenderArea::kCleared};
int32_t gUIRenderAreaInUse = 0;

// ---- ui_utils.cpp

// 0x1018b2c8
void AdjustJustificationForHebrew(int32_t& justification) {
    if (gText.IsHebrew()) {
        if (justification == 0) {
            justification = 2;
        } else if (justification == 2) {
            justification = 0;
        }
    }
}

// 0x1018b360
void MakeRenderArea(uint8_t* pixels, TbRenderArea* area, int32_t width, int32_t height) {
    if (area == nullptr) return;
    const DDPIXELFORMAT& format = gei_TextureSettings.mPixelFormat;
    area->mData = pixels;
    area->mStride = static_cast<int32_t>((format.dwRGBBitCount >> 3) * static_cast<uint32_t>(width));
    area->mWidth = width;
    area->mPictureHeight = height;
    std::memcpy(area->mF00 + 0, &format.dwRBitMask, 4);
    std::memcpy(area->mF00 + 4, &format.dwGBitMask, 4);
    std::memcpy(area->mF00 + 8, &format.dwBBitMask, 4);
    std::memcpy(area->mF00 + 12, &format.dwRGBAlphaBitMask, 4);
    area->mDepth = static_cast<int32_t>(format.dwRGBBitCount);
    area->mPalettised = static_cast<int32_t>(format.dwFlags & DDPF_PALETTEINDEXED8);
}

// 0x1018b578. The justification arguments are signed bytes; a value outside 0..2 leaves the formatter as it is. Not
// wrapping (wrap 0) makes the box 0x4000 wide, and for the centred and the right-hand text moves its left edge to
// half the width minus 0x2000 and to the width minus 0x4000: the rectangle's own left edge is not added (BUG kept; it is
// only right for a rectangle that starts at 0). A font number outside 0..12 makes the original read the word at address
// 0x14 (a null table entry); the translation passes NULL, which is what a zero word there gives.
void UI_DrawText(const char16_t* text, const TbRect& rect, const TbColour& colour, int8_t font, int8_t horizontal,
                 int8_t vertical, int32_t wrap, TbRect* extent, int32_t onCurrent) {
    TbTextFormatter formatter;
    TbRect measured = {0, 0, 0, 0};
    if (gUIRenderAreaInUse == 0 && onCurrent == 0) return;
    if (text == nullptr) return;
    int32_t left = rect.left;
    const int32_t top = rect.top;
    int32_t width = rect.right - left;
    const int32_t height = rect.bottom - top;
    int32_t across = horizontal;
    // The same swap as AdjustJustificationForHebrew, written out in the original (one call of IsHebrew, not two).
    if (gText.IsHebrew()) {
        if (across == 0) {
            across = 2;
        } else if (across == 2) {
            across = 0;
        }
    }
    if (across == 1) {
        formatter.SetHorizontalJustification(TbTextFormatter::kJustifyCentre);
        if (wrap == 0) {
            left = (width >> 1) - 0x2000;
            width = 0x4000;
        }
    } else if (across == 0) {
        formatter.SetHorizontalJustification(TbTextFormatter::kJustifyLeft);
        if (wrap == 0) width = 0x4000;
    } else if (across == 2) {
        formatter.SetHorizontalJustification(TbTextFormatter::kJustifyRight);
        if (wrap == 0) {
            left = width - 0x4000;
            width = 0x4000;
        }
    }
    if (vertical == 1) {
        formatter.SetVerticalJustification(TbTextFormatter::kJustifyCentre);
    } else if (vertical == 0) {
        formatter.SetVerticalJustification(TbTextFormatter::kJustifyTop);
    } else if (vertical == 2) {
        formatter.SetVerticalJustification(TbTextFormatter::kJustifyBottom);
    }
    const uint32_t index = static_cast<uint32_t>(static_cast<int32_t>(font));
    TbIFont* fontObject = index < 13 ? gText.mFonts[gText.mResolutionGroup * 13 + index].mFont : nullptr;
    gText.mRenderParams.SetFont(fontObject);
    gText.mRenderParams.UseReColour(colour);
    if (onCurrent == 0) LbDraw_SetCurrent(&gUIRenderArea);
    const TbRect box = {left, top, left + width, top + height};
    LbDraw_SetClipRect(&box);
    const TbRect drawBox = box;
    const uint8_t* multibyte = UIMain_ConvertString(text);
    formatter.Draw(drawBox, multibyte, gText.mRenderParams, &measured);
    LbDraw_SetClipRect(nullptr);
    if (extent != nullptr) *extent = measured;
}

// ---- ui_main.cpp

// The original has one UIMain_ConvertString, taking a char16_t* (src/ui_main.cpp, 0x10181a7c). The const overload is only this
// translation's way for callers holding a const text to reach it.
uint8_t* UIMain_ConvertString(const char16_t* text) { return UIMain_ConvertString(const_cast<char16_t*>(text)); }

// ---- InterfaceFont's widths

// 0x1016915c
int32_t InterfaceFont::GetWidth(const char16_t* text, int16_t first, int16_t last) {
    // (the Mac's call is virtual: a wrapper answers with its own; see src/InterfaceFont.cpp)
    if (auto* const wrapper = dynamic_cast<InterfaceFontWrapper*>(this)) return wrapper->GetWidth(text, first, last);
    int32_t width = 0;
    if (text == nullptr || mBitmap == nullptr) return 0;
    const int32_t length = static_cast<int32_t>(wcslen(text));
    if (last < first) last = static_cast<int16_t>(length - 1);
    if (length == 0 || first < 0 || first > length) return 0;
    if (last <= first || last > length) return 0;
    for (int32_t i = first; i <= last; ++i) {
        width += static_cast<int8_t>(mBitmap[static_cast<uint32_t>(text[i]) * 8]);
        width += mF02;
    }
    return width;
}

// 0x10169264
int32_t InterfaceFont::GetCharWidth(char16_t character) {
    if (auto* const wrapper = dynamic_cast<InterfaceFontWrapper*>(this)) return wrapper->GetCharWidth(character);
    if (mBitmap != nullptr && character < mF00) return mF02 + static_cast<int8_t>(mBitmap[static_cast<uint32_t>(character) * 8]);
    return 0;
}

// 0x101692d4
int32_t InterfaceFont::LastCharIn(const char16_t* text, int16_t first, int16_t last) {
    if (auto* const wrapper = dynamic_cast<InterfaceFontWrapper*>(this)) return wrapper->LastCharIn(text, first, last);
    return 0;
}
