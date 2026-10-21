// Test library for the status panel's button helpers (unit v5240; libtpw_unit_SetButton.dylib, tests/test_SetButton.py), main app:
//   src/game/ui/ui_tags.cpp   SetButton, DoButton
//
// The callees outside the unit (the draw object library, the panels, the zooms, the maintenance request, the challenge answers, the assert)
// are fakes that record what the real ones read. InterfaceWindow's constructor and virtuals are defined here (Enable records its call),
// as are gGameWorld and gThingArray.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

using namespace calllog;

CWorld* gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];

namespace {
int64_t Raw(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) & 0xffffffffu); }
}  // namespace

CoolRect::CoolRect() {}   // the window's rectangles are members
CoolRect::~CoolRect() {}
CoolRegion::~CoolRegion() {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

// ---- InterfaceWindow: the constructor and the virtuals
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) { mHelpText = -1; }
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
int32_t InterfaceWindow::SetValue(int32_t) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return mHelpText; }

void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SetDrawObject", {lab(this), Raw(object)}); }

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {}, {name});
    return reinterpret_cast<InterfaceDrawObject*>(static_cast<uintptr_t>(ret("FindDrawObject")));
}

// ---- the panels and the rest
void CreateRideListPanel(UIObjType type, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> selected) {
    ev("CreateRideListPanel", {static_cast<int32_t>(type), selected.mIndex});
}
void CreateHireListPanel(StaffType type) { ev("CreateHireListPanel", {static_cast<int32_t>(type)}); }
void CreateLoansPanel() { ev("CreateLoansPanel", {}); }
void CreateResearchPanel() { ev("CreateResearchPanel", {}); }
void CreateParkStatusPanel() { ev("CreateParkStatusPanel", {}); }
void CreateStaffCostsPanel() { ev("CreateStaffCostsPanel", {}); }
void CreateYearEndPanel() { ev("CreateYearEndPanel", {}); }
void CreateEntryPricePanel() { ev("CreateEntryPricePanel", {}); }
void UIMain_ZoomToThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { ev("UIMain_ZoomToThing", {id.mIndex}); }
void UIMain_ZoomToMapId(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>& id) { ev("UIMain_ZoomToMapId", {id.mIndex}); }

CChallengeHQ* CWorld::GetChallengeHQ() {
    ev("GetChallengeHQ", {Raw(this)});
    return reinterpret_cast<CChallengeHQ*>(static_cast<uintptr_t>(ret("GetChallengeHQ")));
}
void CChallengeHQ::ChallengeAccepted() { ev("ChallengeAccepted", {Raw(this)}); }
void CChallengeHQ::ChallengeDeclined() { ev("ChallengeDeclined", {Raw(this)}); }
int32_t CObject::RequestMaintenance(int32_t on) {
    ev("RequestMaintenance", {Raw(this), on});
    return 1;
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

extern "C" {

uint64_t tpw_sb_addr(const char* name) {
    if (std::strcmp(name, "world") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gGameWorld));
    return 0;
}

// The host address of gThingArray[index].mThing.
uint64_t tpw_sb_thing(uint32_t index) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gThingArray[index].mThing)); }

uint64_t tpw_sb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_sb_newwindow() {
    void* memory = tpw::ArenaAllocZeroed(sizeof(InterfaceWindow));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new (memory) InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0)));
}

int32_t tpw_sb_helptext(uint64_t window) { return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window))->mHelpText; }

int32_t tpw_sb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(kThingArraySize);
    return n;
}

void tpw_sb_set(uint64_t window, int32_t action) {
    SetButton(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), static_cast<TagButtonAction>(action));
}

// `idword` is the register the Mac passes: the id in the HIGH half.
int32_t tpw_sb_do(int32_t action, uint32_t idword, int32_t extra) {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(idword >> 16);
    return DoButton(static_cast<TagButtonAction>(action), id, extra);
}

}  // extern "C"
