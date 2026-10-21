// ui_utils.cpp: name proven by __sinit_ui_utils_cpp in the main application.

#include "FS_UserInfo.h"
#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "InterfaceWindow.h"
#include "keyboard.h"
#include "LbDraw.h"
#include "LbPalette.h"
#include "lobby.h"
#include "platform/msl.h"
#include "tb_resource_texture.h"
#include "TbColour.h"
#include "TbFontRenderParams.h"
#include "TbGraphicFile.h"
#include "TbIRLE4bitFont.h"
#include "TbITable.h"
#include "TbResource.h"
#include "TbTextFormatter.h"
#include "Text.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "ui_Postcard.h"
#include <cstring>
#include <new>

// The interface's text drawing (main app of SimTheme Park 2.04b3): UI_DrawText 0x1018b578, MakeRenderArea 0x1018b360 and
// AdjustJustificationForHebrew 0x1018b2c8 (ui_utils.cpp), UIMain_ConvertString 0x10181a7c (ui_main.cpp) and the three
// InterfaceFont width members 0x1016915c, 0x10169264, 0x101692d4 (the inventory files them under FS_MasterSave.cpp).
// UI_DrawText hands the text to the ltms text formatter (unit w35a), which draws through the Bullfrog library's current
// area, viewport and clip rectangle (src/bfl_lbdraw_leaves.cpp). The declarations are in include/UI_Base.h and
// include/InterfaceFont.h.

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

// Unit v51012: the rest of the interface's ui_utils.cpp helpers (main app of SimTheme Park 2.04b3):
//   UI_DrawPostcardText 0x1018bbc4, __sinit_ui_utils_cpp 0x1018bda8, MakeRenderAreaFromRAW 0x1018b434,
//   InterfaceDrawEngineHGradient::CalcUVs 0x10189d70, InterfaceVisitorTimeTextObject::Change 0x1018efcc.

// 0x1038cd2c: the colour the module's static initialiser makes (opaque black and its palette index); the original name is unknown.
uint8_t gUIUtilsColour[5];

// 0x1018b434: describes `width` x `height` pixels at `pixels` as raw data of the given type. NULL area: nothing. Type 0: 24-bit
// (stride 3 x width, no alpha mask); 2: 32-bit ARGB; 3: 32-bit with the channels in the other order; any other type leaves the
// masks, depth and stride alone (the width, height and pixels are still stored).
void MakeRenderAreaFromRAW(void* pixels, int32_t width, int32_t height, RAWSurfaceType type, TbRenderArea* area) {
    if (area == nullptr) return;
    area->mData = static_cast<uint8_t*>(pixels);
    area->mWidth = width;
    area->mPictureHeight = height;
    uint32_t masks[4];
    if (type == 2) {
        area->mStride = static_cast<int32_t>(static_cast<uint32_t>(width) << 2);
        masks[0] = 0xff0000;
        masks[1] = 0xff00;
        masks[2] = 0xff;
        masks[3] = 0xff000000u;
        area->mDepth = 0x20;
        area->mPalettised = 0;
    } else if (type > 1) {
        if (type > 3) return;
        area->mStride = static_cast<int32_t>(static_cast<uint32_t>(width) << 2);
        masks[0] = 0xff00;
        masks[1] = 0xff0000;
        masks[2] = 0xff000000u;
        masks[3] = 0xff;
        area->mDepth = 0x20;
        area->mPalettised = 0;
    } else {
        if (type != 0) return;
        area->mStride = static_cast<int32_t>(static_cast<uint32_t>(width) * 3u);
        masks[0] = 0xff0000;
        masks[1] = 0xff00;
        masks[2] = 0xff;
        masks[3] = 0;
        area->mDepth = 0x18;
        area->mPalettised = 0;
    }
    std::memcpy(area->mF00, masks, sizeof(masks));
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

uint8_t* UIMain_ConvertString(const char16_t* text);

// UI_MeasureText (main app 0x1018b8e4, 676 bytes; the inventory files it under ui_utils.cpp; unit w40): UI_DrawText's measuring twin,
// for texts whose size the interface needs before it makes the window that shows them (the version number's). The set-up of the
// formatter is UI_DrawText's (src/UI_drawtext.cpp) without the drawing: no colour, no render area, no clip rectangle.

// 0x1018b8e4. The justifications are signed bytes: horizontal 0, 1, 2 become the formatter's left, centre and right (the Hebrew
// languages exchange 0 and 2 first), vertical 0, 1, 2 its top, centre and bottom; any other value sets nothing. With `wrap` 0 the box
// is made 0x4000 wide, and for centred text its left becomes half the width minus 0x2000 and for right-hand text the width minus
// 0x4000: the rectangle's own left edge is not added (BUG kept, as in UI_DrawText: it is only right for a rectangle that starts at
// 0, and the one caller, UI_CreateVersionNumber, passes such a rectangle). The font is gText's font `font` of the current resolution
// group; a number outside 0..12 makes the original read the word at address 0x14 (a null table entry), here a NULL font. NULL text
// ends the call before anything but the formatter's construction and destruction, and `extent` is not written then.
void UI_MeasureText(const char16_t* text, const TbRect& rect, int8_t font, int8_t horizontal, int8_t vertical, int32_t wrap,
                    TbRect* extent) {
    TbTextFormatter formatter;
    TbRect measured = {0, 0, 0, 0};
    if (text == nullptr) return;
    int32_t left = rect.left;
    const int32_t top = rect.top;
    int32_t width = rect.right - left;
    const int32_t height = rect.bottom - top;
    int32_t across = horizontal;
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
    const TbRect box = {left, top, left + width, top + height};
    const uint8_t* multibyte = UIMain_ConvertString(text);
    formatter.Measure(box, multibyte, gText.mRenderParams, &measured);
    if (extent != nullptr) *extent = measured;
}

// 0x1018bbc4: draws the postcard's message (font 12, left and top justified, wrapped) into the postcard's pixels, through the UI's
// render area. `colour` has the red in the top byte, then green and blue (the lowest byte is not read); the last argument is unused.
void UI_DrawPostcardText(char16_t* text, void* pixels, int32_t width, int32_t height, SamsUtilities::URectangle::LongRect rect,
                         uint32_t colour, int32_t unused) {
    (void)unused;
    // TbColour's constructor is not translated (and the original does not run it for this local): build it over raw storage.
    alignas(TbColour) uint8_t storage[sizeof(TbColour)];
    TbColour& textColour = *reinterpret_cast<TbColour*>(storage);
    uint8_t red = static_cast<uint8_t>(colour >> 24);
    uint8_t green = static_cast<uint8_t>(colour >> 16);
    uint8_t blue = static_cast<uint8_t>(colour >> 8);
    textColour.mAlpha = 0xff;
    textColour.mRed = red;
    textColour.mGreen = green;
    textColour.mBlue = blue;
    textColour.mIndex = LbPalette_FindColour(&_lbGlobalPalette, red, green, blue);
    MakeRenderAreaFromRAW(pixels, width, height, static_cast<RAWSurfaceType>(3), &gUIRenderArea);
    gUIRenderAreaInUse = 1;
    gText.mRenderParams.UseBlendedMode();
    TbRect box;
    box.left = rect.left;
    box.top = rect.top;
    box.right = rect.right;
    box.bottom = rect.bottom;
    UI_DrawText(text, box, textColour, 12, 0, 0, 1, nullptr, 0);
    gText.mRenderParams.UseAlphaWriteMode();
    gUIRenderAreaInUse = 0;
}

// The player login panel (main app, ui_playerLogin.cpp, 0x101ae39c and 0x101ae9e0) and two small helpers of the interface (UIMain_GetString,
// ui_main.cpp, and UI_TITLE_COLOUR, ui_utils.cpp). Unit w39.
//
// The panel is the window script gLoginPanelWindowData (src/ui_loginpanel_data.cpp) built under the interface's root window. It has four
// rows, one for each saved player (the row windows 0x7a14, 0x7a19, 0x7a1a and 0x7a1b), each with a name area (0x7a15), a button (0x7a16)
// and a picture (0x7a17, shown for a player in the easy game); a fifth row (0x7a1c) carries the text of the quit button. CreateLoginPanel
// dresses the name areas with title text objects and FillLoginNames puts the players' names in them.
//
// Quirks kept: FillLoginNames and CreateLoginPanel call GetWindow on the answer of the one before without a test (a panel without the
// rows crashes them); the easy-mode answer is cut to a short before it is passed to Show; UI_TITLE_COLOUR takes no theme number (the
// colour is fixed).

void UI_TITLE_COLOUR(InterfaceWindow* window) { window->SetFontColour(0xea, 0xef, 0x66, 0xff); }

// 0x1018bda8: the static initialiser of ui_utils.cpp.
void __sinit_ui_utils_cpp() {
    gUIUtilsColour[1] = 0;
    gUIUtilsColour[2] = 0;
    gUIUtilsColour[3] = 0;
    gUIUtilsColour[0] = 0xff;
    gUIUtilsColour[4] = LbPalette_FindColour(&_lbGlobalPalette, 0, 0, 0);
    new (&gUIRenderArea) TbRenderArea();
}
