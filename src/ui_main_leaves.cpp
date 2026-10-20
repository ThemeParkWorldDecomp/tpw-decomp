// Two small functions of the interface's main file (ui_main.cpp, main app; unit w40): GetFontTableEntry 0x10180270 (48 bytes), which
// every panel builder uses to pick a font, and UI_CreateVersionNumber 0x101874d0 (528 bytes), which CLBLobby::Init calls to put the
// game's version under the lobby.
#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "InterfaceWindow.h"
#include "TbGraphicFile.h"
#include "Text.h"
#include "UI_Base.h"
#include "lobby.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

namespace {

// The version label's format at 0x1024da40 (the wide string behind the table slot 0x1026392c's target 0x1024da3e, plus 2): a "v", a
// space and the version resource's short text, a plain (narrow) string.
const char16_t kVersionFormat[] = u"v %s";

}  // namespace

// 0x10180270. The font table is 13 fonts to a resolution group (0x138 bytes a group on the Mac, 0x18 a font, from +0x1c of the text
// object); 13 and up is no font. The group is not checked (SetRez keeps it in 0..3).

// 0x101874d0. The label is "v " and the version text, at most 63 characters. It is measured (font 6, left and top justified, not
// wrapped, in a box of 32000 by 32000), and the window is the measured size scaled from the screen's real pixels to the interface's
// 2048 x 1536 units (the widths and heights are signed 32-bit products, divided with the screen's size as a signed halfword; a screen
// size of 0 would divide by zero, as on the Mac), ending at (0x7c0, 0x5c0) whatever the size. The window's coordinates are 16 bits.
// It has a text draw object as its client draw object, the text font 6 of the current group, and the label as its text.
void UI_CreateVersionNumber(bool show) {
    InterfaceWindow* parent = show ? gUIRootWindow : gLobby->mWindow;
    SamsUtilities::UVersion version(2, 0, 0, 0);
    SamsUtilities::UString text;
    version.LoadFromResource(SamsUtilities::UVersion::kDefaultVersionResource, 0);
    text = version.mText;
    char16_t label[64];
    swprintf(label, 0x40, kVersionFormat, text.mText);
    const TbRect limit = {0, 0, 32000, 32000};
    TbRect size = {0, 0, 0, 0};
    UI_MeasureText(label, limit, 6, 0, 0, 0, &size);
    const int32_t width = ((size.right - size.left) * 0x800) / static_cast<int32_t>(gUIRealScreenWidth);
    const int32_t left = 0x7c0 - width;
    const int32_t height = ((size.bottom - size.top) * 0x600) / static_cast<int32_t>(gUIRealScreenHeight);
    const int32_t top = 0x5c0 - height;
    InterfaceWindow* window = parent->CreateObject(1, 3, 0x8cfd55, static_cast<int16_t>(left), static_cast<int16_t>(top),
                                                   static_cast<int16_t>(left + width), static_cast<int16_t>(top + height));
    window->SetClientDrawObject(new InterfaceTextObjectSprite(nullptr));
    window->SetFont(&gText.mFonts[gText.mResolutionGroup * 13 + 6]);
    window->SetClientDrawText(label);
}
