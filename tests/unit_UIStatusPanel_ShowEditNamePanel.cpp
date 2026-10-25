// Test library for UIStatusPanel::ShowEditNamePanel (unit v5340; libtpw_unit_UIStatusPanel_ShowEditNamePanel.dylib,
// tests/test_UIStatusPanel_ShowEditNamePanel.py), main app:
//   src/game/ui/ui_StatusPanel.cpp
//
// Every callee outside the unit is a fake that records the arguments the real one reads. The windows are blocks the test builds, whose
// first word points to a fake vtable (SetText is the only virtual the function calls). The two constructors are fakes under the
// constructors' own symbols (an asm label), so no vtable is needed.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

extern int16_t gEditNamePanelScript[];   // defined by the translation

// ---- globals of other files, defined here
CThing* gStaffRoomPanelThing;
char16_t gEditSignBufA[15];
char16_t gEditSignBufB[15];
char16_t gUIMainConvertBuffer[0x4000];

int32_t TwoEditsMsgStatus(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

namespace {

// W, T, A, B, str, font5, font7, draw (set by the test for every case)
uint64_t gWorld[8];

void* Ptr(int i) { return reinterpret_cast<void*>(static_cast<uintptr_t>(gWorld[i])); }

void FakeTrap() {}
int32_t FakeSetText(InterfaceWindow* self, char16_t* text) {
    ev("SetText", {lab(self)}, {std::string(narrow(text)).c_str()});
    return 1;
}

void* gVtable[300];

void* VtableAddress() {
    auto member = &InterfaceWindow::SetText;
    struct Raw {
        intptr_t ptr;
        intptr_t adj;
    } raw;
    std::memcpy(&raw, &member, sizeof raw);
    // arm64: the virtual flag is the low bit of adj, the first word is the byte offset into the vtable
    size_t slot = static_cast<size_t>(raw.ptr) / 8;
    for (size_t i = 0; i < 300; i++) gVtable[i] = reinterpret_cast<void*>(&FakeTrap);
    gVtable[2 + slot] = reinterpret_cast<void*>(&FakeSetText);
    return &gVtable[2];
}

void PutText(char16_t* buffer, const char16_t* text) {
    size_t n = 0;
    while (text[n] != 0) {
        buffer[n] = text[n];
        n++;
    }
    buffer[n] = 0;
}

}  // namespace

// ---- the fakes of the constructors, under the real symbols
extern "C" void FakeTitleCtor(void* self, void* data) __asm__("__ZN24InterfaceTitleTextObjectC1EPv");
extern "C" void FakeTitleCtor(void* self, void* data) { ev("TitleCtor", {newlab(self), reinterpret_cast<intptr_t>(data)}); }
extern "C" void FakeTitleCtor2(void* self, void* data) __asm__("__ZN24InterfaceTitleTextObjectC2EPv");
extern "C" void FakeTitleCtor2(void* self, void* data) { ev("TitleCtor", {newlab(self), reinterpret_cast<intptr_t>(data)}); }
extern "C" void FakeCursorCtor2(void* self, void* data) __asm__("__ZN34InterfaceCursorPosTextObjectSpriteC2EPv");
extern "C" void FakeCursorCtor2(void* self, void* data) { ev("CursorCtor", {newlab(self), reinterpret_cast<intptr_t>(data)}); }
extern "C" void FakeCursorCtor(void* self, void* data) __asm__("__ZN34InterfaceCursorPosTextObjectSpriteC1EPv");
extern "C" void FakeCursorCtor(void* self, void* data) { ev("CursorCtor", {newlab(self), reinterpret_cast<intptr_t>(data)}); }

// ---- the fakes
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), lab(description), lab(reinterpret_cast<const void*>(callback))});
    return static_cast<InterfaceWindow*>(Ptr(0));
}
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }
int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {lab(this), index, lab(value)});
    return 0;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    if (id == 0x3943e) return static_cast<InterfaceWindow*>(Ptr(1));
    if (id == 0x3943c) return static_cast<InterfaceWindow*>(Ptr(2));
    return static_cast<InterfaceWindow*>(Ptr(3));
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }

int32_t InterfaceEditBox::SetStaticTextArea(char16_t* text, int32_t capacity) {
    ev("SetStaticTextArea", {lab(this), lab(text), capacity});
    return 0;
}
void InterfaceEditBox::SetCursorDrawObject(InterfaceDrawObject* object) { ev("SetCursorDrawObject", {lab(this), lab(object)}); }
void InterfaceEditBox::SelectAll() { ev("SelectAll", {lab(this)}); }

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {}, {static_cast<const char*>(name)});
    return static_cast<InterfaceDrawObject*>(Ptr(7));
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return static_cast<InterfaceFont*>(index == 5 ? Ptr(5) : Ptr(6));
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return static_cast<const char16_t*>(Ptr(4));
}
void UI_TITLE_COLOUR(InterfaceWindow* window) { ev("UI_TITLE_COLOUR", {lab(window)}); }
void UIMain_DisableAllKeyTables() { ev("DisableAllKeyTables", {}); }

void CObject::GetSignNameA(char16_t* buffer, uint32_t size) {
    ev("GetSignNameA", {lab(this), lab(buffer), size});
    PutText(buffer, u"NameA");
}
void CObject::GetSignNameB(char16_t* buffer, uint32_t size) {
    ev("GetSignNameB", {lab(this), lab(buffer), size});
    PutText(buffer, u"NameB");
}

extern "C" {

uint64_t tpw_ses_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"script", gEditNamePanelScript},
        {"callback", reinterpret_cast<const void*>(&TwoEditsMsgStatus)},
        {"buf", gUIMainConvertBuffer},
        {"bufA", gEditSignBufA},
        {"bufB", gEditSignBufB},
        {"thing", &gStaffRoomPanelThing},
    };
    if (std::strcmp(name, "base") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase));
    if (std::strcmp(name, "vtable") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(VtableAddress()));
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ses_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ses_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mZ));
    out[n++] = static_cast<int64_t>(sizeof(InterfaceWindow));
    return n;
}

void tpw_ses_setworld(const uint64_t* world) { std::memcpy(gWorld, world, sizeof gWorld); }

void tpw_ses_show(uint64_t panel) {
    reinterpret_cast<UIStatusPanel*>(static_cast<uintptr_t>(panel))->ShowEditNamePanel();
}

}  // extern "C"
