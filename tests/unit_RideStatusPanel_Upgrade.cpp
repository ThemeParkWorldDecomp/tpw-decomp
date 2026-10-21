// Test library for the ride status panel's knobs and buttons (unit v5914; libtpw_unit_RideStatusPanel_Upgrade.dylib,
// tests/test_RideStatusPanel_Upgrade.py), main app:
//   src/game/ui/ui_RideStatus.cpp   Upgrade, ApplyChanges, DoDial, Edit, Show,
//   UpdateKnobsWindow, and the globals gRideStatusShowThing and gRideStatusScript.
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queues (pointers the
// callees answer go through the queues too, as 32-bit arena addresses). The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "InterfaceScrollBar.h"
#include "LLCellSystem.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "ObjectControl.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_main.h"
#include "fake_status_panel_table.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CThing* gStaffRoomPanelThing;
CWorld* gGameWorld;
CMouse* gMouse;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CRideDatabase gRideDatabase;

namespace {

// The panel's table (unit v5L31): the slots at +0x1c and +0x34 are logged with the panel, as the Mac's __ptr_glue fakes log them.
uint64_t Slot(void* panel, uint32_t offset, const void*) {
    if (offset == 0x1c) ev("Slot5", {lab(panel)});
    if (offset == 0x34) ev("Slot11", {lab(panel)});
    return 0;
}

template <class T>
T* PointerOf(int64_t value) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(value));
}

}  // namespace

// The callback the status window script names (the Mac's TVector at 0x10267d98); another unit defines it.
int32_t RideStatusMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

// ---- the fakes
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return PointerOf<CBankAccount>(ret("GetBankAccount"));
}

int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("GetBalance"));
}

int32_t CObject::RequestUpgrade(int32_t on) {
    ev("RequestUpgrade", {lab(this), on});
    return 0;
}

RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this)});
    return PointerOf<RideInfo>(ret("GetDBInfo"));
}

void CObject::SetOperatingSpeed(uint32_t speed) { ev("SetOperatingSpeed", {lab(this), static_cast<int64_t>(speed)}); }
void CObject::SetOperatingCapacity(uint8_t capacity) { ev("SetOperatingCapacity", {lab(this), capacity}); }
void CObject::SetOperatingDuration(uint8_t duration) { ev("SetOperatingDuration", {lab(this), duration}); }

ObjectControl* CControlManager::GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t flag) {
    ev("GetObjectControl", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this)) - static_cast<int64_t>(reinterpret_cast<uintptr_t>(gGameWorld)),
                            type.mIndex, flag});
    return PointerOf<ObjectControl>(ret("GetObjectControl"));
}

// The constructors store the vtables, so the key functions (the destructors) and the other virtuals are defined here, empty (nothing calls them).
CMMPathDraw::CMMPathDraw(CELL_MODES mode) { ev("CMMPathDraw1", {newlab(this), static_cast<int32_t>(mode)}); }
CMMPathDraw::CMMPathDraw(CELL_MODES mode, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("CMMPathDraw2", {newlab(this), static_cast<int32_t>(mode), id.mIndex});
}
CMMPathDraw::~CMMPathDraw() {}
void CMMPathDraw::OnLeftClick(int32_t, int32_t) {}
void CMMPathDraw::OnLeftRelease(int32_t, int32_t) {}
void CMMPathDraw::OnRightClick(int32_t, int32_t) {}
void CMMPathDraw::OnMouseMove(int32_t, int32_t, int32_t, int32_t) {}
void CMMPathDraw::WhileLeftDown(int32_t, int32_t) {}
void CMMPathDraw::WhileRightDown(int32_t, int32_t) {}
int32_t CMMPathDraw::GetMode() { return 3; }
void CMMPathDraw::OnAssign() {}
CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }

void UI_SetSelectedObject(void* object) { ev("UI_SetSelectedObject", {lab(object)}); }
void LLCM_SetCurrentObject(bool flag) { ev("LLCM_SetCurrentObject", {flag}); }
void UIMain_NotEnoughDough() { ev("UIMain_NotEnoughDough", {}); }

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {id.mIndex});
    return PointerOf<RideInfo>(ret("GetRideInfo"));
}

void CoasterEditMode(void* object) { ev("CoasterEditMode", {lab(object)}); }
void LLCC_SystemSetRideTrack(int32_t track) { ev("LLCC_SystemSetRideTrack", {track}); }
void LLCM_LeftMouseButtonUp(int32_t x, int32_t y, uint16_t shift, int32_t flag) { ev("LLCM_LeftMouseButtonUp", {x, y, shift, flag}); }

void UIStatusPanel::OpenWindow(int16_t* description, InterfaceWindowCallback callback, int32_t windowA, int32_t windowB, int32_t windowC) {
    ev("OpenWindow", {lab(this), description == gRideStatusScript, callback == RideStatusMsg, windowA, windowB, windowC});
}

void UIMain_AdvisorMessage(AdvisorMessage message) { ev("UIMain_AdvisorMessage", {static_cast<int32_t>(message)}); }

// A window block's children: the words at +0x10 + 4 * (id - 0x3e2d) are the children's addresses (32 bits), zero for none.
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    if (id < 0x3e2d || id > 0x3e31) return nullptr;
    uint64_t word;
    std::memcpy(&word, reinterpret_cast<uint8_t*>(this) + 0x10 + 8 * (id - 0x3e2d), 8);
    return PointerOf<InterfaceWindow>(word);
}

void InterfaceScrollBar::SetMinMax(int32_t min, int32_t max) { ev("SetMinMax", {lab(this), min, max}); }
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}

void RideStatusPanel::UpdateKnobsTitles(int32_t which) { ev("UpdateKnobsTitles", {lab(this), which}); }

namespace allitems_post_callee {
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {lab(window), value}); }
}  // namespace allitems_post_callee

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_rsp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"thing", &gStaffRoomPanelThing}, {"world", &gGameWorld}, {"mouse", &gMouse},        {"w1e0", &gMouseWord1E0},
        {"w1e4", &gMouseWord1E4},         {"show", &gRideStatusShowThing}, {"ops", FakeStatusPanelTable<Slot>()},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_rsp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The Ptr32 value of a block (its offset from the arena base; the panel's windows and table are Ptr32, unit v5L31).
uint32_t tpw_rsp_off(uint64_t address) {
    return address == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address)));
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_rsp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = offsetof(RideStatusPanel, mWindow);
    out[n++] = offsetof(RideStatusPanel, mVtable);
    out[n++] = offsetof(RideStatusPanel, mUpgradeWindow);
    out[n++] = offsetof(RideStatusPanel, mF28);
    out[n++] = offsetof(RideStatusPanel, mSpeed);
    out[n++] = offsetof(RideStatusPanel, mCapacity);
    out[n++] = offsetof(RideStatusPanel, mDuration);
    out[n++] = sizeof(RideStatusPanel);
    out[n++] = offsetof(CObject, mObjectType);
    out[n++] = offsetof(CObject, mF40);
    out[n++] = offsetof(CObject, mF4C);
    out[n++] = offsetof(CObject, mF54);
    out[n++] = offsetof(CObject, mF58);
    out[n++] = offsetof(CObject, mF59);
    out[n++] = sizeof(CObject);
    RideInfo* info = static_cast<RideInfo*>(tpw::ArenaAllocZeroed(sizeof(RideInfo)));
    out[n++] = sizeof(RideInfo);
    out[n++] = reinterpret_cast<uint8_t*>(&info->Value(static_cast<RideBalanceSlot>(0))) - reinterpret_cast<uint8_t*>(info);
    out[n++] = reinterpret_cast<uint8_t*>(&info->Value(static_cast<RideBalanceSlot>(1))) - reinterpret_cast<uint8_t*>(&info->Value(static_cast<RideBalanceSlot>(0)));
    out[n++] = offsetof(ObjectControl, mUpgradeCost);
    out[n++] = sizeof(ObjectControl);
    return n;
}

// ---- the functions, one wrapper each with C types only
uint32_t tpw_rsp_upgrade(uint64_t panel, int32_t what, int32_t flag) {
    return static_cast<uint32_t>(PointerOf<RideStatusPanel>(panel)->Upgrade(what, flag != 0));
}
void tpw_rsp_apply(uint64_t panel) {
    PointerOf<RideStatusPanel>(panel)->ApplyChanges();
}
void tpw_rsp_dodial(uint64_t panel, int32_t a, int32_t b) {
    PointerOf<RideStatusPanel>(panel)->DoDial(a, b);
}
void tpw_rsp_edit(uint64_t panel, int32_t on) {
    PointerOf<RideStatusPanel>(panel)->Edit(on);
}
void tpw_rsp_show(uint64_t panel, uint64_t thing) {
    PointerOf<RideStatusPanel>(panel)->Show(PointerOf<CThing>(thing));
}
void tpw_rsp_knobs(uint64_t panel) {
    PointerOf<RideStatusPanel>(panel)->UpdateKnobsWindow();
}

}  // extern "C"
