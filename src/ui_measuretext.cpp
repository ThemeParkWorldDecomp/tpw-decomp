// UI_MeasureText (main app 0x1018b8e4, 676 bytes; the inventory files it under ui_utils.cpp; unit w40): UI_DrawText's measuring twin,
// for texts whose size the interface needs before it makes the window that shows them (the version number's). The set-up of the
// formatter is UI_DrawText's (src/UI_drawtext.cpp) without the drawing: no colour, no render area, no clip rectangle.
#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "TbFontRenderParams.h"
#include "TbGraphicFile.h"
#include "TbTextFormatter.h"
#include "Text.h"
#include "UI_Base.h"
#include "ui_main.h"

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
