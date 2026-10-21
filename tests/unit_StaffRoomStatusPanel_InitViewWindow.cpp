// Test library for the staff, show and upgrade status panels' view-window functions (unit v51051;
// libtpw_unit_StaffRoomStatusPanel_InitViewWindow.dylib, tests/test_StaffRoomStatusPanel_InitViewWindow.py), main app:
//   src/game/ui/ui_showStatus.cpp, src/game/ui/ui_staffPanel.cpp, src/game/ui/ui_staffRoomStatus.cpp, src/game/ui/ui_staffStatus.cpp, src/game/ui/ui_upgradeStatus.cpp
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues; the globals of other
// files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
CThing* gStaffRoomPanelThing;   // 0x103947a4
int32_t gStaffPanelState;       // 0x10395a38
ShowStatusPanel gShowStatusPanel;   // 0x10395a58

namespace {
uint64_t gTestRide;   // what CObject::GetDBInfo answers
uint32_t U32(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

// ---- the fakes
void UIStatusPanel::CloseWindow() { ev("CloseWindow", {lab(this)}); }
void ShowStatusPanel::applytoallcallback(CThing* thing) { ev("applytoallcallback", {lab(this), lab(thing)}); }
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this)});
    return reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(gTestRide));
}

CoolRegion::~CoolRegion() {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect() { ev("CoolRectCtor", {lab(this)}); }
CoolRect::~CoolRect() { ev("CoolRectDtor", {lab(this)}); }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

int32_t InterfaceWindow::GetClientRect(CoolRect* rect) {
    ev("GetClientRect", {lab(this), lab(rect)});
    return 0;
}
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFontJust(int32_t h, int32_t v) { ev("SetFontJust", {lab(this), h, v}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), U32(font)}); }
void InterfaceWindow::SetFontColour(int32_t r, int32_t g, int32_t b, int32_t a) { ev("SetFontColour", {lab(this), r, g, b, a}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), U32(text)}); }

void UIMain_ShowRidePreview(CoolRect& rect, uint32_t id, int32_t z) { ev("ShowRidePreview", {lab(&rect), id, z}); }
void UIMain_DestroyRidePreview() { ev("DestroyRidePreview", {}); }
void UIMain_DestroySpritePreview() { ev("DestroySpritePreview", {}); }
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(static_cast<uintptr_t>(ret("GetFontTableEntry")));
}
namespace statusinit_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {size});
    if (ret("new") == 0) return nullptr;
    return tpw::ArenaAllocZeroed(size);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {lab(mem)}); }
}  // namespace statusinit_callee
void RemoveSlideoutPanel(int16_t which) { ev("RemoveSlideoutPanel", {which}); }
void UI_SetSelectedPeep(CPeep* peep) { ev("SetSelectedPeep", {lab(peep)}); }
void CStaff::SackMe() { ev("SackMe", {lab(this)}); }
void CStaff::SetName(const char16_t* name) { ev("SetName", {lab(this)}, {narrow(name).c_str()}); }
const char16_t* CStaff::GetName() const {
    ev("GetName", {lab(this)});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(ret("GetName")));
}

#pragma clang diagnostic ignored "-Winvalid-offsetof"
extern "C" {

uint64_t tpw_sv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"thing", &gStaffRoomPanelThing}, {"state", &gStaffPanelState}, {"show", &gShowStatusPanel},
        {"w4c", &gStaffPanelWord4c},      {"w50", &gStaffPanelWord50},  {"ride", &gTestRide},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The 32-bit word a Ptr32 holds for the block (its offset in the arena).
uint32_t tpw_sv_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_sv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(StaffRoomStatusPanel, mScript));
    out[n++] = static_cast<int64_t>(offsetof(StaffRoomStatusPanel, mF28));
    out[n++] = static_cast<int64_t>(offsetof(StaffRoomStatusPanel, mF2c));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mZ));
    out[n++] = static_cast<int64_t>(offsetof(RideInfo, mRideTypeNumber));
    return n;
}

// One entry for every function: op is the index in the module's table; a is the object (or thing or context), b the name.
uint64_t tpw_sv_call(int32_t op, uint64_t a, uint64_t b) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(a));
    auto* room = static_cast<StaffRoomStatusPanel*>(p);
    auto* show = static_cast<ShowStatusPanel*>(p);
    auto* staff = static_cast<StaffStatusPanel*>(p);
    auto* upgrade = static_cast<UpgradeStatusPanel*>(p);
    switch (op) {
        case 0: room->InitViewWindow(); break;
        case 1: room->UpdateViewWindow(); break;
        case 2: room->DeInitStatusWindow(); break;
        case 3: room->DeInitWindow(); break;
        case 4: room->Remove(); break;
        case 5: show->Remove(); break;
        case 6: return static_cast<uint32_t>(_applytoall(static_cast<CThing*>(p)));
        case 7: show->DeInitWindow(); break;
        case 8: staff->InitViewWindow(); break;
        case 9: staff->DeInitViewWindow(); break;
        case 10: show->DeInitStatusWindow(); break;
        case 11: staff->DeInitStatusWindow(); break;
        case 12: show->UpdateViewWindow(); break;
        case 13: show->DeInitViewWindow(); break;
        case 14: staff->DeInitWindow(); break;
        case 15: show->InitViewWindow(); break;
        case 16: __sinit_ui_staffPanel_cpp(); break;
        case 17: return static_cast<uint32_t>(StaffPanelUp());
        case 18: RemoveStaffPanel(); break;
        case 19: staff->SetSelected(); break;
        case 20: return static_cast<uint32_t>(fireme(p));
        case 21: staff->ChangeName(reinterpret_cast<char16_t*>(static_cast<uintptr_t>(b))); break;
        case 22: return U32(staff->GetName());
        case 23: staff->Remove(); break;
        case 24: upgrade->InitViewWindow(); break;
        case 25: upgrade->DeInitViewWindow(); break;
        case 26: upgrade->UpdateViewWindow(); break;
    }
    return 0;
}

}  // extern "C"
