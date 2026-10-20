// Test library for the all-staff panel's row functions and button handler (unit v5153; libtpw_unit_LCFormat_2.dylib, tests/test_LCFormat_2.py), main app:
//   src/CreateAllStaffPanel_LCFormat.cpp   AllStaffLCFormat (LCFormat), AllStaffLCPostFormat (LCPostFormat), AllStaffPanelLCHandler;
//                                          gStaffListFormat and gStaffListFont
//
// The callees outside the unit are recording fakes (window members, draw object constructors, the font, the analyser, the world, strings);
// the objects they hand out are blocks the test makes (tpw_lc_setobj). The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
InterfaceWindow* gStaffPanel;
CWorld* gGameWorld;
int16_t gUIRealScreenHeight;

namespace {
// 0..4 the window pool, 5 the world, 6 the analyser, 7 the font, 8..12 the strings 0x6b..0x6f
uintptr_t gObj[13];
bool gMute = false;

int PoolIndex(const void* p) {
    for (int i = 0; i < 5; ++i)
        if (gObj[i] == reinterpret_cast<uintptr_t>(p)) return i;
    return -1;
}
}  // namespace

// ---- InterfaceWindow: the constructor, the virtuals and the members the unit calls
InterfaceWindow::InterfaceWindow(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                 int16_t bottom) {
    if (gMute) return;
    ev("IWCtor", {newlab(this), lab(parent), attributes, id, left, top, right, bottom});
}
InterfaceWindow::~InterfaceWindow() {}
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t value) {
    ev("SetValue", {lab(this), value});
    return 0;
}
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t* text) {
    ev("SetText", {lab(this), lab(text)});
    return 0;
}
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }

InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    return nullptr;
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), lab(text)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }
InterfaceWindow* InterfaceWindow::GetChild(int32_t unused) {
    ev("GetChild", {lab(this), unused});
    return reinterpret_cast<InterfaceWindow*>(gObj[1]);
}
InterfaceWindow* InterfaceWindow::GetNext() {
    ev("GetNext", {lab(this)});
    int i = PoolIndex(this);
    return (i >= 0 && i < 4) ? reinterpret_cast<InterfaceWindow*>(gObj[i + 1]) : nullptr;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    int slot = id == 0x329 ? 1 : id == 0x32a ? 2 : id == 0x32d ? 3 : -1;
    return slot < 0 ? nullptr : reinterpret_cast<InterfaceWindow*>(gObj[slot]);
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}

// ---- the draw objects' constructors
// These classes carry long chains of bases and members this unit does not compile, so their constructors (and CoolRect's constructor and
// destructor) are plain C functions that take the symbol names of the real ones (complete-object constructor C1, destructor D1).
extern "C" {
void FakeTextSpriteCtor(void* self, void* data) __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
void FakeCashTextCtor(void* self, void* data) __asm__("__ZN23InterfaceCashTextObjectC1EPv");
void FakeGradientCtor(void* self, void* mesh) __asm__("__ZN28InterfaceDrawEngineHGradientC1EPv");
void FakeCoolRectCtor(void* self) __asm__("__ZN8CoolRectC1Ev");
void FakeCoolRectDtor(void* self) __asm__("__ZN8CoolRectD1Ev");

void FakeTextSpriteCtor(void* self, void* data) { ev("TextSpriteCtor", {newlab(self), lab(data)}); }
void FakeCashTextCtor(void* self, void* data) { ev("CashTextCtor", {newlab(self), lab(data)}); }
void FakeGradientCtor(void* self, void* mesh) { ev("GradientCtor", {newlab(self)}, {static_cast<const char*>(mesh)}); }
// The CoolRects inside the windows and draw objects are not the unit's: only the local one of LCPostFormat is logged.
bool gTrackRects = false;
void FakeCoolRectCtor(void* self) {
    if (gTrackRects) ev("CoolRectCtor", {newlab(self)});
}
void FakeCoolRectDtor(void* self) {
    if (gTrackRects) ev("CoolRectDtor", {lab(self), -1});
}
}

// ---- the others
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(gObj[7]);
}
int32_t InterfaceFont::GetHeight() {
    ev("GetHeight", {lab(this)});
    return static_cast<int32_t>(ret("GetHeight"));
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return (id >= 0x6b && id <= 0x6f) ? reinterpret_cast<const char16_t*>(gObj[8 + (id - 0x6b)]) : nullptr;
}
void UIMain_IndentClient(InterfaceWindow* window, int16_t a, int16_t b) { ev("IndentClient", {lab(window), a, b}); }
void FillAllStaffList(StaffType type, int32_t flag) { ev("FillAllStaffList", {static_cast<int64_t>(type), flag}); }
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(gObj[6]);
}
int32_t CParkAnalyser::AverageHappinessOfStaffType(CThing::ThingModel model) const {
    ev("AverageHappiness", {lab(this), static_cast<int64_t>(model)});
    return static_cast<int32_t>(ret("AverageHappiness"));
}

extern "C" {

uint64_t tpw_lc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gStaffListFormat", &gStaffListFormat}, {"gStaffListFont", &gStaffListFont}, {"gUIRealScreenHeight", &gUIRealScreenHeight},
        {"gStaffPanel", &gStaffPanel},           {"gGameWorld", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Registers a block the fakes hand out; the pool slots become (silently) constructed windows.
void tpw_lc_setobj(int32_t slot, uint64_t address) {
    gObj[slot] = static_cast<uintptr_t>(address);
    if (slot < 5) {
        gMute = true;
        new (reinterpret_cast<void*>(address)) InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0);
        gMute = false;
    }
}

int32_t tpw_lc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(InterfaceWindow) <= 0x400);
    return n;
}

int64_t tpw_lc_format() { return lab(AllStaffLCFormat()); }
void tpw_lc_post(uint64_t window) {
    gTrackRects = true;
    AllStaffLCPostFormat(reinterpret_cast<InterfaceWindow*>(window));
    gTrackRects = false;
}
int32_t tpw_lc_handler(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return AllStaffPanelLCHandler(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}

}  // extern "C"
