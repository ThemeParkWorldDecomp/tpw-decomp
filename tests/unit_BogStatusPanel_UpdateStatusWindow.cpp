// Test library for BogStatusPanel::InitStatusWindow and UpdateStatusWindow (unit v5519; libtpw_unit_BogStatusPanel_UpdateStatusWindow.dylib,
// tests/test_BogStatusPanel_UpdateStatusWindow.py), main app:
//   src/BogStatusPanel_InitViewWindow_BogStatusPanel_UpdateStatusWindow.cpp
//
// The callees outside the unit are recording fakes; the windows are real InterfaceWindow objects whose virtuals are defined here
// (Enable and SetValue record, the others do nothing). gStaffRoomPanelThing is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_main.h"

using namespace calllog;

CThing* gStaffRoomPanelThing;

int32_t FloatToCell(double value);
namespace bog_callee {
void HGradientCtor(void* memory, const char* mesh);
}

namespace {

struct Child {
    const void* parent;
    int32_t id;
    void* window;
};
Child gChildren[32];
int32_t gChildCount;

uint64_t FakePanelSlot(void* self) {
    ev("PanelSlot2c", {lab(self)});
    return 0;
}

}  // namespace

// ---- the rectangle classes the window class holds (the window constructor below builds them; nothing here is used by the unit)
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect() {}
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

// ---- the window class: its virtuals, defined here because the library does not compile the program's files
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t on) {
    ev("Enable", {lab(this), on});
    return 0;
}
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t value) {
    ev("SetValue", {lab(this), value});
    return 0;
}
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }

// ---- the fakes
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    for (int32_t i = 0; i < gChildCount; ++i)
        if (gChildren[i].parent == this && gChildren[i].id == id) return static_cast<InterfaceWindow*>(gChildren[i].window);
    return nullptr;
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(font))}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) {
    ev("SetClientDrawText", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(text))});
}
int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {lab(this), static_cast<uint8_t>(state), static_cast<uint8_t>(redraw)});
    return 0;
}
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(static_cast<uintptr_t>(0x7000 + index));
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(0x9000 + id));
}
void UIMain_IndentClient(InterfaceWindow* window, int16_t a, int16_t b) { ev("UIMain_IndentClient", {lab(window), a, b}); }
int32_t UIMain_GetObjectState(CObject* object, int32_t flag) {
    ev("UIMain_GetObjectState", {lab(object), flag});
    return static_cast<int32_t>(ret("UIMain_GetObjectState"));
}
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this)});
    return nullptr;
}
uint32_t CObject::GetCurrentResidualValue() const {
    ev("GetCurrentResidualValue", {lab(this)});
    return static_cast<uint32_t>(ret("GetCurrentResidualValue"));
}
int32_t CObject::IsOpenable() {
    ev("IsOpenable", {lab(this)});
    return static_cast<int32_t>(ret("IsOpenable"));
}
bool CObject::IsClosed() const {
    ev("IsClosed", {lab(this)});
    return ret("IsClosed") != 0;
}
template <>
int32_t CHistory<uint32_t, 30>::GetSumOfLast(double& sum, int32_t count) const {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    ev("GetSumOfLast", {static_cast<int64_t>(reinterpret_cast<const uint8_t*>(this) - reinterpret_cast<const uint8_t*>(object)), count});
    sum = static_cast<double>(static_cast<int32_t>(ret("sum"))) / 4.0;
    return 0;
}
int32_t FloatToCell(double value) {
    int64_t bits;
    std::memcpy(&bits, &value, 8);
    ev("FloatToCell", {bits});
    return static_cast<int32_t>(ret("cell"));
}
namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {size});
    return tpw::ArenaAllocZeroed(size);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {newlab(mem)}); }
void CashCtor(void* mem) { ev("CashCtor", {newlab(mem)}); }
void NumberCtor(void* mem) { ev("NumberCtor", {newlab(mem)}); }
void PercentCtor(void* mem) { ev("PercentCtor", {newlab(mem)}); }
void WindowCtor(void*, InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
void WindowSetValue(InterfaceWindow*, int32_t) {}
}  // namespace lender_callee
// The symbol the program's weak HGradientCtor calls: never reached here (the fake below replaces HGradientCtor), defined so the library links.
extern "C++" void HGradientRawFake(void*, void*) __asm__("__ZN28InterfaceDrawEngineHGradientC1EPv");
void HGradientRawFake(void*, void*) {}
namespace bog_callee {
void HGradientCtor(void* memory, const char* mesh) { ev("HGradientCtor", {newlab(memory)}, {mesh}); }
}  // namespace bog_callee

extern "C" {

uint64_t tpw_bsp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"thing", &gStaffRoomPanelThing},
        {"panelslot", reinterpret_cast<const void*>(&FakePanelSlot)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bsp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_bsp_off(uint64_t address) { return address == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

uint64_t tpw_bsp_mkwin(uint64_t memory) {
    new (reinterpret_cast<void*>(static_cast<uintptr_t>(memory))) InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0);
    return memory;
}

void tpw_bsp_link(uint64_t parent, int32_t id, uint64_t child) {
    if (gChildCount < 32)
        gChildren[gChildCount++] = {reinterpret_cast<const void*>(static_cast<uintptr_t>(parent)), id,
                                    reinterpret_cast<void*>(static_cast<uintptr_t>(child))};
}

void tpw_bsp_unlink_all() { gChildCount = 0; }

int32_t tpw_bsp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF04));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF10));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mVtable));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF40));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory1A4));
    out[n++] = static_cast<int64_t>(sizeof(CObject));
    return n;
}

void tpw_bsp_init(uint64_t panel) { reinterpret_cast<BogStatusPanel*>(static_cast<uintptr_t>(panel))->InitStatusWindow(); }
void tpw_bsp_update(uint64_t panel) { reinterpret_cast<BogStatusPanel*>(static_cast<uintptr_t>(panel))->UpdateStatusWindow(); }

}  // extern "C"
