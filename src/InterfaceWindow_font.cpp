// InterfaceWindow's font and text setters (main app, UI toolkit): SetFont 0x101da118, SetFontJust 0x101da204, SetFontColour 0x101da3ac,
// setExtraParam 0x101dcfd0, SetClientDrawText 0x101de130 and the virtual SetText 0x101de248. Unit w39.
//
// A window has a draw object (+0x94) and a client draw object (+0x98); the setters tell them what changed through Change, with a flag that
// names the field of the change they should read (the original builds the change on its stack and sets only that field, so the other
// fields are never read). The draw object comes first, and the whole call is skipped for a draw object the window does not have.
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "ui_DrawObject.h"

namespace {

constexpr uint32_t kChangeText = 0x100;          // the style's text
constexpr uint32_t kChangeJustify = 0xc00;       // the style's two justification bytes
constexpr uint32_t kChangeFont = 0x10000;        // the style's font id
constexpr uint32_t kChangeColour = 0x40000;      // the style's colour

// Tells the window's draw objects (the main one, then the client one) the style, with the flag that says which part is new.
void ChangeBoth(InterfaceDrawObject* draw, InterfaceDrawObject* client, uint32_t flags, DOTextStyle* style) {
    DOChange change{};
    change.mText = style;
    if (draw != nullptr) draw->Change(flags, &change);
    if (client != nullptr) client->Change(flags, &change);
}

}  // namespace

// The font is kept even when the window has no draw object. A window that has the font already does nothing. The id the draw objects get
// is the low byte of the font's id (0xff for no font).
void InterfaceWindow::SetFont(InterfaceFont* font) {
    if (font == mFont) return;
    mFont = font;
    DOTextStyle style{};
    style.mF06[2] = font != nullptr ? static_cast<uint8_t>(font->mID) : 0xff;
    ChangeBoth(mDrawObject, mClientDrawObject, kChangeFont, &style);
}

void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) {
    if (mDrawObject == nullptr && mClientDrawObject == nullptr) return;
    DOTextStyle style{};
    style.mF04 = static_cast<uint8_t>(horizontal);
    style.mF05 = static_cast<uint8_t>(vertical);
    ChangeBoth(mDrawObject, mClientDrawObject, kChangeJustify, &style);
}

void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    if (mDrawObject == nullptr && mClientDrawObject == nullptr) return;
    DOTextStyle style{};
    style.mColour[0] = static_cast<uint8_t>(red);
    style.mColour[1] = static_cast<uint8_t>(green);
    style.mColour[2] = static_cast<uint8_t>(blue);
    style.mF13 = static_cast<uint8_t>(alpha);
    ChangeBoth(mDrawObject, mClientDrawObject, kChangeColour, &style);
}

// QUIRK: a negative index passes the test (it is compared as a signed number) and writes before the array.
int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    if (index >= 16) return 0;
    mExtraParams[index] = value;
    return 1;
}

void InterfaceWindow::SetClientDrawText(char16_t* text) {
    if (mClientDrawObject == nullptr) return;
    DOTextStyle style{};
    style.mText = text;
    ChangeBoth(nullptr, mClientDrawObject, kChangeText, &style);
}

// The client draw object is told first, then the main one.
int32_t InterfaceWindow::SetText(char16_t* text) {
    DOChange change{};
    DOTextStyle style{};
    style.mText = text;
    change.mText = &style;
    if (mClientDrawObject != nullptr) mClientDrawObject->Change(kChangeText, &change);
    if (mDrawObject != nullptr) mDrawObject->Change(kChangeText, &change);
    return 1;
}

// unit w43: main 0x101da2d8, 160 bytes. The wrapping codes occupy style bytes 6 and 7.
void InterfaceWindow::SetFontWrap(int32_t horizontal, int32_t vertical) {
    if (mDrawObject == nullptr && mClientDrawObject == nullptr) return;
    DOTextStyle style{};
    style.mF06[0] = static_cast<uint8_t>(horizontal);
    style.mF06[1] = static_cast<uint8_t>(vertical);
    ChangeBoth(mDrawObject, mClientDrawObject, 0x3000, &style);
}

// unit w43: main 0x101dd030, 36 bytes. Negative indices address the preceding words, as in setExtraParam.
void* InterfaceWindow::getExtraParam(int16_t index) {
    if (index >= 16) return nullptr;
    return mExtraParams[index];
}
