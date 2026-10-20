// Test library for the bumhole window's message function, ToggleGameWindow and SetOnOffText (unit v5143; libtpw_unit_bumhole.dylib,
// tests/test_bumhole.py), main app:
//   src/ui_mainpanel2_window_bumhole.cpp   bumhole, ToggleGameWindow, SetOnOffText and the globals at 0x10394984 / 0x10394a14
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues. The fake windows are real
// InterfaceWindow objects of this library (the constructor, the virtual members and the destructor are fakes: the destructor records
// "dtor", the SetText fake records the text's length and a checksum). The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "InterfaceWindow.h"
#include "TPCamera.h"
#include "UI_Base.h"
#include "World.h"
#include "keyboard.h"
#include "lobby.h"
#include "ui_Postcard.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
uint32_t gFirstPersonFlags;
CWorld* gGameWorld;
CKeyTable* gKeyTableShortcuts;
CKeyTable* gKeyTableCamera;
CKeyTable* gKeyTableGame;
InterfaceWindow* gBumholeWindow;
InterfaceWindow* gGameWindow;
InterfaceWindow* gUIRootWindow;
char16_t gUIMainConvertBuffer[0x4000];

namespace {
InterfaceWindow* gFakeObjs[4];

// The fake string of an id: n characters, never 0.
char16_t GsChar(int32_t id, int32_t i) { return static_cast<char16_t>(((id * 7 + i * 13) % 255) + 1); }

int64_t Checksum(const char16_t* text, int64_t* length) {
    uint32_t h = 0;
    int64_t n = 0;
    for (; text[n] != 0; ++n) h = (h * 31 + text[n]) & 0x7fffffffu;
    *length = n;
    return h;
}
}  // namespace

// ---- the fakes
// The fake windows are blocks with a hand-made vtable (Itanium order of the declaration: DrawFunction, CalcClientArea, ~ (D1, D0), WhatAmI,
// ClientDraw, Enable, operator-, SetValue, GetValue, SetText, GetText, GetHelpText): only the deleting destructor (slot 3) and SetText
// (slot 10) are called.
namespace {
void FakeDelete(void* self) { ev("dtor", {lab(self)}); }
int32_t FakeSetText(void* self, char16_t* text) {
    int64_t length;
    int64_t sum = Checksum(text, &length);
    ev("SetText", {lab(self), length, sum});
    return 1;
}
void* gFakeVtable[16];
}  // namespace

int32_t InterfaceWindow::CaptureDevice() {
    ev("CaptureDevice", {lab(this)});
    return 0;
}
int32_t InterfaceWindow::UnCaptureDevice() {
    ev("UnCaptureDevice", {lab(this)});
    return 0;
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefault", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefault"));
}
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return gFakeObjs[ret("GetWindow") & 3];
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallback", {lab(this), callback == bumhole ? 1 : 0});
}
InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    return gFakeObjs[ret("CreateObject") & 3];
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}
int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {lab(this), state, redraw});
    return 0;
}

int32_t CKeyTable::KeyDown(int16_t key, int16_t modifiers) {
    ev("KeyDown", {lab(this), key, modifiers});
    return 0;
}
int32_t CKeyTable::KeyUp(int16_t key, int16_t modifiers) {
    ev("KeyUp", {lab(this), key, modifiers});
    return 0;
}
int32_t CKeyTable::Search(int16_t key, int16_t modifiers) {
    ev("Search", {lab(this), key, modifiers});
    return static_cast<int32_t>(ret("Search"));
}

void Will_HandleCameraMessages(int32_t message, int32_t a, int32_t b) { ev("Camera", {message, a, b}); }
int16_t CheckShifts() {
    ev("CheckShifts", {});
    return static_cast<int16_t>(ret("CheckShifts"));
}
void CreateMainMenu(int32_t a) { ev("CreateMainMenu", {a}); }
void UI_DoPostcard() { ev("UI_DoPostcard", {}); }
void UIMain_DecideKbCapture() { ev("DecideKb", {}); }
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    int32_t n = static_cast<int32_t>(ret("GetString"));
    if (n < 0) return nullptr;
    for (int32_t i = 0; i < n; ++i) gUIMainConvertBuffer[i] = GsChar(id, i);
    gUIMainConvertBuffer[n] = 0;
    return gUIMainConvertBuffer;
}

extern "C" {

uint64_t tpw_bh_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"camera", &gKeyTableCamera},   {"game", &gKeyTableGame},       {"shortcuts", &gKeyTableShortcuts},
        {"world", &gGameWorld},         {"bumhole", &gBumholeWindow},   {"gamewin", &gGameWindow},
        {"uiroot", &gUIRootWindow},     {"flags", &gFirstPersonFlags},  {"toggled", &gGameWindowToggled},
        {"onoffroot", &gSetOnOffRoot},  {"buffer", gUIMainConvertBuffer},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bh_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A fresh fake window (the real constructor is a fake too).
uint64_t tpw_bh_newwin() {
    gFakeVtable[3] = reinterpret_cast<void*>(&FakeDelete);
    gFakeVtable[10] = reinterpret_cast<void*>(&FakeSetText);
    void** block = static_cast<void**>(tpw::ArenaAllocZeroed(0x400));
    block[0] = gFakeVtable;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
}

// The objects the fakes answer with: index 1 to 3 (0 is NULL).
void tpw_bh_setobj(int32_t index, uint64_t address) { gFakeObjs[index & 3] = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(address)); }

int32_t tpw_bh_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA738));
    return n;
}

int32_t tpw_bh_bumhole(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return bumhole(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}
void tpw_bh_toggle() { ToggleGameWindow(); }
void tpw_bh_onoff(int32_t window, int32_t text, int32_t first, int32_t second, int32_t button, int32_t buttonId) {
    SetOnOffText(window, text, first != 0, second != 0, button != 0, buttonId);
}

}  // extern "C"
