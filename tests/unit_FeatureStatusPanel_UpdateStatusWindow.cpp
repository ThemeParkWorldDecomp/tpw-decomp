// Test library for the feature status panel (unit v5311; libtpw_unit_FeatureStatusPanel_UpdateStatusWindow.dylib,
// tests/test_FeatureStatusPanel_UpdateStatusWindow.py), main app:
//   src/FeatureStatusPanel_UpdateStatusWindow.cpp   UpdateStatusWindow, InitWindow, UpdateWindow, RideIt, UpdateViewWindow, Show
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues: CObject (GetDBInfo,
// GetCurrentResidualValue, GoFirstPerson), CWorld::GetParkAnalyser, CParkAnalyser::NumberOfObjectInPark, the InterfaceWindow members
// (GetWindow, Show, GetClientRect, SetValue, SetText: a window here is a real object whose constructor does nothing), CoolRect,
// UIMain_ShowRidePreview, UIMain_AdvisorMessage and the UIStatusPanel members. The panel's table of virtual members holds blocks whose
// first 8 bytes are fake functions that record too. The globals of other files are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
CThing* gStaffRoomPanelThing;
CWorld* gGameWorld;

namespace {

bool gQuiet = false;            // the window constructor makes CoolRects: not events
InterfaceWindow* gKids[16];     // GetWindow's answers by id - 0x157c
RideInfo* gInfo;                // GetDBInfo's answer

int64_t WinId(const InterfaceWindow* window) {
    uintptr_t p = reinterpret_cast<uintptr_t>(window);
    asm volatile("" : "+r"(p));
    return p == 0 ? -1 : static_cast<int64_t>(reinterpret_cast<const InterfaceWindow*>(p)->mId);
}

using PanelFn = uint64_t (*)(void*);
uint64_t Slot1c(void*) { ev("Slot1c", {0}); return static_cast<uint64_t>(ret("Slot1c")) & 0xffffffffu; }
uint64_t Slot20(void*) { ev("Slot20", {0}); return static_cast<uint64_t>(ret("Slot20")) & 0xffffffffu; }
uint64_t Slot34(void*) { ev("Slot34", {0}); return static_cast<uint64_t>(ret("Slot34")) & 0xffffffffu; }
uint64_t Slot3c(void*) { ev("Slot3c", {0}); return static_cast<uint64_t>(ret("Slot3c")) & 0xffffffffu; }

}  // namespace

// ---- the fakes
RideInfo* CObject::GetDBInfo() const {    ev("GetDBInfo", {0});
    return gInfo;
}
uint32_t CObject::GetCurrentResidualValue() const {
    ev("Residual", {0});
    return static_cast<uint32_t>(ret("Residual"));
}
void CObject::GoFirstPerson(int32_t on) const { ev("GoFirstPerson", {static_cast<int64_t>(on)}); }

CParkAnalyser* CWorld::GetParkAnalyser() {    ev("GetParkAnalyser", {static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)))});
    return reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(0x6000));
}
int32_t CParkAnalyser::NumberOfObjectInPark(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) const {    ev("NumberOfObjectInPark", {static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this))), static_cast<int64_t>(id.mIndex)});
    return static_cast<int32_t>(ret("NumberOfObjectInPark"));
}

CoolRegion::~CoolRegion() {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect() {
    if (!gQuiet) ev("CoolRect", {0});
}
CoolRect::~CoolRect() {
    if (!gQuiet) ev("~CoolRect", {0});
}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t id, int16_t, int16_t, int16_t, int16_t) { mId = id; }
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 0; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t value) {
    ev("SetValue", {WinId(this), static_cast<int64_t>(value)});
    return 0;
}
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t* text) {
    ev("SetText", {WinId(this), static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(text)))});
    return 0;
}
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {    ev("GetWindow", {WinId(this), static_cast<int64_t>(id)});
    return gKids[id - 0x157c];
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {WinId(this), static_cast<int64_t>(on)});
    return 1;
}
int32_t InterfaceWindow::GetClientRect(CoolRect* rect) {
    ev("GetClientRect", {WinId(this)});
    rect->mLeft = static_cast<int16_t>(ret("rl"));
    rect->mTop = static_cast<int16_t>(ret("rt"));
    rect->mRight = static_cast<int16_t>(ret("rr"));
    rect->mBottom = static_cast<int16_t>(ret("rb"));
    return 1;
}

void UIMain_ShowRidePreview(CoolRect& rect, uint32_t id, int32_t z) {
    ev("ShowRidePreview", {static_cast<int64_t>(rect.mLeft), static_cast<int64_t>(rect.mTop), static_cast<int64_t>(rect.mRight),
                           static_cast<int64_t>(rect.mBottom), static_cast<int64_t>(id), static_cast<int64_t>(z)});
}
void UIMain_AdvisorMessage(AdvisorMessage message) { ev("AdvisorMessage", {static_cast<int64_t>(message)}); }

int32_t UIStatusPanel::FillObjectList(CThing* thing) {
    ev("FillObjectList", {static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(thing)))});
    return static_cast<int32_t>(ret("FillObjectList"));
}
void UIStatusPanel::InitEditBox(int32_t id) { ev("InitEditBox", {static_cast<int64_t>(id)}); }
void UIStatusPanel::OpenWindow(int16_t* description, InterfaceWindowCallback callback, int32_t windowA, int32_t windowB, int32_t windowC) {
    ev("OpenWindow", {lab(description), lab(reinterpret_cast<const void*>(callback)), static_cast<int64_t>(windowA),
                      static_cast<int64_t>(windowB), static_cast<int64_t>(windowC)});
}
int32_t FeatureStatusMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

extern "C" {

uint64_t tpw_fsp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"thing", &gStaffRoomPanelThing},
        {"world", &gGameWorld},
        {"target", &gFeatureStatusThing},
        {"script", &gFeatureStatusScript[0]},
        {"cb", reinterpret_cast<const void*>(&FeatureStatusMsg)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fsp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_fsp_off(uint64_t p) { return p == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(p))); }

int32_t tpw_fsp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(RideInfo, mRideTypeNumber));
    out[n++] = static_cast<int64_t>(sizeof(UIStatusPanelFields));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mVtable));
    return n;
}

void tpw_fsp_reset() {
    std::memset(gKids, 0, sizeof(gKids));
    gInfo = nullptr;
    gQuiet = false;
}
void tpw_fsp_setkid(int32_t id, uint64_t window) { gKids[id - 0x157c] = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)); }
void tpw_fsp_setinfo(uint64_t info) { gInfo = reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(info)); }

// A window: a real object, id and z set.
uint64_t tpw_fsp_mkwin(int32_t id, int32_t z) {
    void* memory = tpw::ArenaAllocZeroed(sizeof(InterfaceWindow) + 16);
    gQuiet = true;
    InterfaceWindow* window = new (memory) InterfaceWindow(nullptr, 0, id, 0, 0, 0, 0);
    gQuiet = false;
    window->mZ = static_cast<int16_t>(z);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(window));
}

// A block whose first 8 bytes are the fake virtual member for that table offset.
uint64_t tpw_fsp_slot(int32_t slot) {
    PanelFn fn = slot == 0x1c ? Slot1c : slot == 0x20 ? Slot20 : slot == 0x34 ? Slot34 : Slot3c;
    void* block = tpw::ArenaAllocZeroed(16);
    *reinterpret_cast<PanelFn*>(block) = fn;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
}

void tpw_fsp_status(uint64_t p) { reinterpret_cast<FeatureStatusPanel*>(static_cast<uintptr_t>(p))->UpdateStatusWindow(); }
void tpw_fsp_init(uint64_t p) { reinterpret_cast<FeatureStatusPanel*>(static_cast<uintptr_t>(p))->InitWindow(); }
void tpw_fsp_update(uint64_t p) { reinterpret_cast<FeatureStatusPanel*>(static_cast<uintptr_t>(p))->UpdateWindow(); }
void tpw_fsp_ride(uint64_t p) { reinterpret_cast<FeatureStatusPanel*>(static_cast<uintptr_t>(p))->RideIt(); }
void tpw_fsp_view(uint64_t p) { reinterpret_cast<FeatureStatusPanel*>(static_cast<uintptr_t>(p))->UpdateViewWindow(); }
void tpw_fsp_show(uint64_t p, uint32_t thing) {
    reinterpret_cast<FeatureStatusPanel*>(static_cast<uintptr_t>(p))->Show(reinterpret_cast<CThing*>(static_cast<uintptr_t>(thing)));
}

}  // extern "C"
