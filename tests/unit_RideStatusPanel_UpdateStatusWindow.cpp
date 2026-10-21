// Test library for the ride status panel's UpdateStatusWindow, ShowUpgradePanel and UpdateWhichButtons (unit v5191;
// libtpw_unit_RideStatusPanel_UpdateStatusWindow.dylib, tests/test_RideStatusPanel_UpdateStatusWindow.py), main app:
//   src/LCPostFormat_RideStatusPanel_UpdateStatusWindow.cpp   the three functions and gUpgradePanelScript
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues or from the pointer
// table the test fills (tpw_rsp_setptr). The windows are real InterfaceWindow objects whose constructor and virtual members are defined here
// (the Enable, SetValue and SetText calls are recorded). gStaffRoomPanelThing and gText are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <new>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "ride_layout.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "MainWindowCallback.h"
#include "RideDB.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "fake_status_panel_table.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
CThing* gStaffRoomPanelThing;
// gText as raw zeroed storage under its own symbol (its constructor would pull in the font code).
alignas(16) char gTextStorage[sizeof(CText)] __asm__("_gText");

namespace {

// A float's bits with every NaN made one value (the two processors make different NaNs).
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

// What the fakes answer: key -> pointer (the test fills it per case). Keys: -1 the ride record, -2 a string, -3 a font, -4 a draw object,
// -5 the window Build makes, -6 the ride object; the others are window ids.
std::map<int32_t, void*> gPtr;

void* Ptr(int32_t key) {
    auto it = gPtr.find(key);
    return it == gPtr.end() ? nullptr : it->second;
}

// The panel's table (unit v5L31): the slot at +0x2c is told the state changed.
uint64_t Slot(void* panel, uint32_t offset, const void*) {
    if (offset == 0x2c) ev("Notify", {lab(panel)});
    return 0;
}

int32_t gCaptureNothing;

}  // namespace

// ---- the window's rectangle member: empty bodies, so the vtables exist (the window's constructor builds one)
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect() : mLeft(0), mTop(0), mRight(0), mBottom(0) {}
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

// ---- the windows: the constructor and the virtual members of InterfaceWindow (the key function is defined here, so the vtable is too)
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
InterfaceWindow::~InterfaceWindow() {}
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
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
int32_t InterfaceWindow::SetText(char16_t* text) {
    ev("SetText", {lab(this)}, {narrow(text).c_str()});
    return 1;
}
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return static_cast<InterfaceWindow*>(Ptr(id));
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 1;
}
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), description == gUpgradePanelScript ? 1 : 0, callback == UpgradePanelMsg ? 1 : 0});
    return static_cast<InterfaceWindow*>(Ptr(-5));
}
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }
int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {lab(this), index, lab(value)});
    return 1;
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this)}, {narrow(text).c_str()}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }

int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {lab(this), state, redraw});
    return 1;
}

void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    ev("SetColumnFormatsAll", {lab(this), makeRow == ResearchListLCFormat ? 1 : 0, attach == OnlineParkListPanelLCPostFormat ? 1 : 0});
}
void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) { ev("SetColumnType", {lab(this), column, static_cast<int64_t>(type)}); }
void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) { ev("SetSelectionObject", {lab(this), lab(object)}); }

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {}, {name});
    return static_cast<InterfaceDrawObject*>(Ptr(-4));
}

// The title text object's constructor (its vtable is not in this library, so the constructor is named by its mangled label).
extern "C" void FakeTitleTextCtor(void* self, void* data) __asm__("__ZN24InterfaceTitleTextObjectC1EPv");
extern "C" void FakeTitleTextCtor(void* self, void* data) {
    ev("TitleTextCtor", {newlab(self), static_cast<int64_t>(reinterpret_cast<uintptr_t>(data))});
}

// ---- the ride object's members
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this)});
    return static_cast<RideInfo*>(Ptr(-1));
}
int32_t CObject::GetObjectAge() const {
    ev("GetObjectAge", {lab(this)});
    return static_cast<int32_t>(ret("GetObjectAge"));
}
int32_t CObject::GetProspectiveExcitementLevel(int32_t speed, int32_t duration, int32_t capacity) const {
    ev("GetProspectiveExcitementLevel", {lab(this), speed, duration, capacity});
    return static_cast<int32_t>(ret("GetProspectiveExcitementLevel"));
}
int32_t CObject::GetProspectiveReliability(float speed, float capacity, float duration) const {
    ev("GetProspectiveReliability", {lab(this), CanonBits(speed), CanonBits(capacity), CanonBits(duration)});
    return static_cast<int32_t>(ret("GetProspectiveReliability"));
}
int32_t CObject::GetRemainingLife() const {
    ev("GetRemainingLife", {lab(this)});
    return static_cast<int32_t>(ret("GetRemainingLife"));
}
uint32_t CObject::GetCurrentResidualValue() const {
    ev("GetCurrentResidualValue", {lab(this)});
    return static_cast<uint32_t>(ret("GetCurrentResidualValue"));
}
bool CObject::IsClosed() const {
    ev("IsClosed", {lab(this)});
    return ret("IsClosed") != 0;
}
int32_t CObject::IsOpenable() {
    ev("IsOpenable", {lab(this)});
    return static_cast<int32_t>(ret("IsOpenable"));
}
int32_t CObject::IsCloseable() {
    ev("IsCloseable", {lab(this)});
    return static_cast<int32_t>(ret("IsCloseable"));
}

template <>
int32_t CHistory<uint32_t, 30>::GetSumOfLast(double& sum, int32_t count) const {
    ev("GetSumOfLast", {static_cast<int64_t>(reinterpret_cast<const char*>(this) - static_cast<const char*>(Ptr(-6))), count});
    sum = static_cast<double>(ret("GetSumOfLast"));
    return 0;
}

// The variable argument is the address of the tag (RideAgeTag); the text written is "value,flag".
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list args;
    va_start(args, id);
    const RideAgeTag* tag = va_arg(args, const RideAgeTag*);
    va_end(args);
    ev("FormatString", {lab(this), size, id, tag->mMagic, 1, tag->mId, tag->mValue, tag->mNegative});
    std::string text = std::to_string(tag->mValue) + "," + std::to_string(tag->mNegative);
    size_t n = 0;
    for (; n < text.size(); ++n) out[n] = static_cast<char16_t>(text[n]);
    out[n] = 0;
    return 0;
}

int32_t UIMain_GetObjectState(CObject* object, int32_t flag) {
    ev("UIMain_GetObjectState", {lab(object), flag});
    return static_cast<int32_t>(ret("UIMain_GetObjectState"));
}
void RideStatusPanel::UpdateKnobsWindow() { ev("UpdateKnobsWindow", {lab(this)}); }
void FillUpgradeBox(int32_t kind, InterfaceListControl2* list, int32_t clear, CObject* object) {
    ev("FillUpgradeBox", {kind, lab(list), clear, lab(object)});
}
void UIStatusPanel::EnableNameClick(int32_t enable) { ev("EnableNameClick", {lab(this), enable}); }
int32_t LLCC_IsRideMovable(CObject* ride) {
    ev("LLCC_IsRideMovable", {lab(ride)});
    return static_cast<int32_t>(ret("LLCC_IsRideMovable"));
}
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return static_cast<InterfaceFont*>(Ptr(-3));
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return static_cast<const char16_t*>(Ptr(-2));
}
void UI_TITLE_COLOUR(InterfaceWindow* window) { ev("UI_TITLE_COLOUR", {lab(window)}); }

// The functions whose addresses the unit passes on (SetColumnFormatsAll and Build record which they were).
InterfaceWindow* ResearchListLCFormat() { return nullptr; }
void OnlineParkListPanelLCPostFormat(InterfaceWindow*) {}
int32_t UpgradePanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

// TbIntegerVTag's only virtual (the key function), here so that RideAgeTag's vtable links.
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

#define OFFSET_OF(Type, member) \
    static_cast<int64_t>(reinterpret_cast<const char*>(&reinterpret_cast<const Type*>(0x10000)->member) - reinterpret_cast<const char*>(0x10000))

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_rsp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"thing", &gStaffRoomPanelThing},
        {"text", &gText},
        {"script", gUpgradePanelScript},
        {"ops", FakeStatusPanelTable<Slot>()},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_rsp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The Ptr32 value of a block and back (the panel's windows and table are Ptr32, unit v5L31).
uint32_t tpw_rsp_off(uint64_t address) {
    return address == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address)));
}
uint64_t tpw_rsp_at(uint32_t offset) { return offset == 0 ? 0 : static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase + offset)); }

// A window: a real InterfaceWindow (zero bytes, its virtual members recorded above) in the arena.
uint64_t tpw_rsp_newwindow() {
    void* block = tpw::ArenaAllocZeroed(sizeof(InterfaceWindow) + 64);
    InterfaceWindow* window = new (block) InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(window));
}

void tpw_rsp_clearptrs() { gPtr.clear(); }
void tpw_rsp_setptr(int32_t key, uint64_t pointer) { gPtr[key] = reinterpret_cast<void*>(static_cast<uintptr_t>(pointer)); }

// The offsets and sizes the module relies on (HOST_LAYOUT): the first three are the ride object's (the Mac's), then the panel's fields.
int32_t tpw_rsp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = OFFSET_OF(CObject, mObjectType);
    out[n++] = OFFSET_OF(CObject, mF40);
    out[n++] = OFFSET_OF(CObject, mHistory1A4);
    out[n++] = static_cast<int64_t>(sizeof(CObject));
    out[n++] = OFFSET_OF(RideStatusPanel, mState);
    out[n++] = OFFSET_OF(RideStatusPanel, mF08);
    out[n++] = OFFSET_OF(RideStatusPanel, mWindow);
    out[n++] = OFFSET_OF(RideStatusPanel, mStatusWindow);
    out[n++] = OFFSET_OF(RideStatusPanel, mVtable);
    out[n++] = OFFSET_OF(RideStatusPanel, mUpgradeWindow);
    out[n++] = OFFSET_OF(RideStatusPanel, mSpeed);
    out[n++] = OFFSET_OF(RideStatusPanel, mCapacity);
    out[n++] = OFFSET_OF(RideStatusPanel, mDuration);
    out[n++] = static_cast<int64_t>(sizeof(RideStatusPanel));
    out[n++] = OFFSET_OF(CText, mResourcePointers[6]);
    out[n++] = OFFSET_OF(InterfaceWindow, mZ);
    return n;
}

// ---- the functions, one wrapper each
void tpw_rsp_update_status(uint64_t panel) { reinterpret_cast<RideStatusPanel*>(static_cast<uintptr_t>(panel))->UpdateStatusWindow(); }
void tpw_rsp_show_upgrade(uint64_t panel) { reinterpret_cast<RideStatusPanel*>(static_cast<uintptr_t>(panel))->ShowUpgradePanel(); }
void tpw_rsp_update_buttons(uint64_t panel) { reinterpret_cast<RideStatusPanel*>(static_cast<uintptr_t>(panel))->UpdateWhichButtons(); }

}  // extern "C"
