// Test library for the online panels' font and window virtuals (libtpw_unit_ContactsSlots.dylib; see
// tests/test_ContactsSlots.py): contacts_callee's definitions in src/game/GameShimGlue.cpp, as libtpw links them, and the real
// bodies they reach in src/game/ui/InterfaceFont.cpp, src/game/ui/InterfaceWindow.cpp and src/game/ui/InterfaceButton.cpp. Objects carry their real vtables and type information, so the
// dispatch is the program's. Outside that the fakes record through calllog.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "InterfaceFontWrapper.h"
#include "Text.h"
#include "fake_arena_on_heap.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

namespace {
int64_t I(int64_t value) { return value; }
}  // namespace

uint8_t gTextRaw[0x800] __asm__("_gText");

namespace fontwrapper_callee {
uint16_t GetMBValue(void* converter, char16_t character) {
    ev("GetMBValue", {lab(converter), I(character)});
    return static_cast<uint16_t>(ret("GetMBValue"));
}
uint32_t GetCharIndex(TbIFont* font, const unsigned char*& text) {
    uint16_t v;
    std::memcpy(&v, text, 2);
    ev("GetCharIndex", {lab(font), I(v)});
    return static_cast<uint32_t>(ret("GetCharIndex"));
}
uint32_t GetCharWidthOf(TbIFont* font, uint16_t index) {
    ev("GetCharSize", {lab(font), I(index)});
    return static_cast<uint32_t>(ret("width"));
}
}  // namespace fontwrapper_callee

// ---- the fonts' members outside the calls under test
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) { mFont = font; }
InterfaceFontWrapper::~InterfaceFontWrapper() {}

// ---- the windows' members outside the calls under test
int32_t UI_UpdatePointerIn() {
    ev("UpdatePointerIn");
    return 1;
}
CoolRect::CoolRect() {}
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 0; }
void InterfaceWindow::ClientDraw() {}
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }
InterfaceButton::~InterfaceButton() {}
int32_t InterfaceButton::WhatAmI() { return 0; }
int32_t InterfaceButton::SetText(char16_t*) { return 0; }
char16_t* InterfaceButton::GetText() { return nullptr; }
int32_t InterfaceButton::GetHelpText() { return 0; }
InterfaceDrawObject::InterfaceDrawObject(void*) {}
InterfaceDrawObject::~InterfaceDrawObject() {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

extern void* _ZTV15InterfaceWindow[] __asm__("__ZTV15InterfaceWindow");
extern void* _ZTV15InterfaceButton[] __asm__("__ZTV15InterfaceButton");

namespace {
alignas(16) uint8_t gObject[0x400];

class FakeDraw : public InterfaceDrawObject {
public:
    FakeDraw() : InterfaceDrawObject(nullptr) {}
    using InterfaceDrawObject::Draw;
    void Change(uint32_t flags, DOChange* change) override { ev("Change", {lab(this), I(flags), I(change->mFrame)}); }
};
FakeDraw gDraws[2];

int32_t RecordCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("Callback", {lab(window), I(message), I(a), I(b)});
    return 0;
}
}  // namespace

extern "C" {

void tpw_cs_offsets(int64_t* out) { out[0] = static_cast<int64_t>(offsetof(CText, mResourcePointers)); }

void* tpw_cs_gtext() { return gTextRaw; }

int32_t tpw_cs_plain_font(int32_t count, int32_t spacing, uint8_t* table, int32_t character) {
    auto* font = new (gObject) InterfaceFont();
    font->mF00 = static_cast<uint16_t>(count);
    font->mF02 = static_cast<uint16_t>(spacing);
    font->mBitmap = table;
    return contacts_callee::FontCharWidth(font, static_cast<char16_t>(character));
}

int32_t tpw_cs_wrapper_font(TbIFont* inner, int32_t character) {
    auto* font = new (gObject) InterfaceFontWrapper(inner);
    return contacts_callee::FontCharWidth(font, static_cast<char16_t>(character));
}

// Makes the window (a button with `button`) with its real vtable and calls the program's definition. Answers the attributes after.
uint32_t tpw_cs_window(int32_t button, uint32_t attributes, int32_t state, int32_t hilight, int32_t callback, int32_t draw, int32_t client,
                       int32_t value) {
    std::memset(gObject, 0, sizeof gObject);
    void* vptr = button != 0 ? &_ZTV15InterfaceButton[2] : &_ZTV15InterfaceWindow[2];
    std::memcpy(gObject, &vptr, sizeof vptr);
    auto* window = reinterpret_cast<InterfaceWindow*>(gObject);
    tpw_cl_label(20, window);
    tpw_cl_label(24, &gDraws[0]);
    tpw_cl_label(25, &gDraws[1]);
    window->mAttributes = attributes;
    window->mCallback = callback != 0 ? RecordCallback : nullptr;
    window->mDrawObject = draw != 0 ? &gDraws[0] : nullptr;
    window->mClientDrawObject = client != 0 ? &gDraws[1] : nullptr;
    if (button != 0) {
        auto* control = static_cast<InterfaceButton*>(window);
        control->mState = static_cast<int8_t>(state);
        control->mHilight = static_cast<int8_t>(hilight);
    }
    contacts_callee::WindowSlot1c(window, value);
    return window->mAttributes;
}

}  // extern "C"
