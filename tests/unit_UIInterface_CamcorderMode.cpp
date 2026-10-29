// Test library for UIInterface's mouse, research, kid and staff room events and the error dialog queue (unit v51038;
// libtpw_unit_UIInterface_CamcorderMode.dylib, tests/test_UIInterface_CamcorderMode.py), main app:
//   src/game/ui/UIInterface.cpp, src/game/ui/ui_ErrorDisplay.cpp
//
// The callees outside the unit are fakes that record what the real ones read; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <new>

#include "calllog.h"
#include "platform/arena.h"

#include "Debug.h"
#include "FS_Online.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "TbITable.h"
#include "Text.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "UI_pump.h"
#include "World.h"
#include "std_deque_errorthingy.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files
CMouse* gMouse = nullptr;
int32_t gMouseWord1E0 = 0;
int32_t gMouseWord1E4 = 0;
CThingArrayEntry gThingArray[kThingArraySize];
StaffRoomStatusPanel gStaffRoomStatusPanel;
std::deque<errorthingy> gUIErrorQueue;
bthread_mutex gUIErrorMutex;
InterfaceWindow* gUIErrorDialog = nullptr;
// gText is raw zeroed storage under the program variable's symbol (CText's constructor needs font and resource files this library does not compile).
alignas(16) unsigned char gTextStorage[0x4000] __asm__("_gText");

uint64_t gMslMacAddressHigh = 0;

static int64_t I(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)); }

// ---- the fakes
bool UIMain_CheckPanel() {
    ev("UIMain_CheckPanel", {});
    return false;
}

CMMFirstPersonTarget::CMMFirstPersonTarget() { ev("CMMFirstPersonTarget", {newlab(this)}); }
CMMFirstPersonTarget::~CMMFirstPersonTarget() {}
void CMMFirstPersonTarget::OnMouseMove(int32_t, int32_t, int32_t, int32_t) {}
void CMMFirstPersonTarget::OnLeftClick(int32_t, int32_t) {}
void CMMFirstPersonTarget::OnRightClick(int32_t, int32_t) {}
int32_t CMMFirstPersonTarget::GetMode() { return 9; }
void CMMFirstPersonTarget::OnAssign() {}

CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {static_cast<int64_t>(condition & 0xff)}, {format});
}

void AllKidsPanel_KidAdded(CThing* thing) { ev("AllKidsPanel_KidAdded", {lab(thing)}); }
void StaffRoomStatusPanel::StaffWentIntoStaffRoom(CThing* staff) { ev("StaffWentIntoStaffRoom", {lab(this), lab(staff)}); }
void StaffRoomStatusPanel::StaffLeftStaffRoom(CThing* staff) { ev("StaffLeftStaffRoom", {lab(this), lab(staff)}); }

void MainPanel_EnableResearchButton(bool on) { ev("MainPanel_EnableResearchButton", {on ? 1 : 0}); }

CGameType::CGameType() {
    ev("CGameType", {lab(this)});
    mType = static_cast<GameType>(ret("CGameType"));
}

void bthread_mutex::lock() { ev("lock", {lab(this)}); }
void bthread_mutex::unlock() { ev("unlock", {lab(this)}); }

const unsigned char* TbITable::GetConstString(uint32_t id) const {
    ev("GetConstString", {lab(this), static_cast<int64_t>(id)});
    return reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(ret("GetConstString")));
}

char16_t* UIMain_ConvertMulti(const unsigned char* text) {
    ev("UIMain_ConvertMulti", {I(text)});
    return reinterpret_cast<char16_t*>(static_cast<uintptr_t>(ret("UIMain_ConvertMulti")));
}

namespace findpanel_callee {
void WindowSetText(InterfaceWindow* window, char16_t* text) { ev("WindowSetText", {lab(window), I(text), 1}); }
}  // namespace findpanel_callee

namespace errdeque_callee {
void OperatorDelete(void* p) { ev("delete", {lab(p)}); }
}  // namespace errdeque_callee

extern "C" {

uint64_t tpw_cam_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"mutex", &gUIErrorMutex},
        {"panel", &gStaffRoomStatusPanel},
        {"storage", gLoansGameTypeStorage},
        {"guard", &gLoansGameTypeGuard},
        {"mouse", &gMouse},
        {"w1e0", &gMouseWord1E0},
        {"w1e4", &gMouseWord1E4},
        {"dialog", &gUIErrorDialog},
        {"gtext3", &gText.mResourcePointers[3]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cam_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cam_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(errorthingy::mId));
    using Cdeque = std::__cdeque<const void*, std::allocator<const void*>>;
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<Cdeque*>(0x1000)->mWordC) - 0x1000);
    return n;
}

// the world
void tpw_cam_set_high(uint64_t high) { gMslMacAddressHigh = high; }
void tpw_cam_set_thing(uint32_t index, uint64_t value) { gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(value)); }
void tpw_cam_clear() {
    std::memset(gThingArray, 0, sizeof(gThingArray));
    gUIErrorQueue.clear();
}
void tpw_cam_q_push(uint64_t table, uint32_t id) {
    errorthingy e;
    e.mTable = reinterpret_cast<TbITable*>(static_cast<uintptr_t>(table));
    e.mId = id;
    gUIErrorQueue.push_back(e);
}
uint32_t tpw_cam_q_size() { return static_cast<uint32_t>(gUIErrorQueue.size()); }
void tpw_cam_q_at(uint32_t i, uint64_t* out) {
    out[0] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gUIErrorQueue[i].mTable));
    out[1] = gUIErrorQueue[i].mId;
}

// ---- the functions
void tpw_cam_camcorder() {
    UIInterface u;
    u.CamcorderMode();
}
void tpw_cam_enable(uint32_t on) {
    UIInterface u;
    u.EnableResearchButton((on & 0xff) != 0);
}
void tpw_cam_staff(uint32_t index, int32_t arriving) {
    UIInterface u;
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(index);
    u.StaffRoomEvent(id, arriving);
}
void tpw_cam_kid(uint32_t index) {
    UIInterface u;
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(index);
    u.KidArrived(id);
}
uint32_t tpw_cam_cancel() { return static_cast<uint32_t>(ErrorDlgCancel(nullptr)); }
void tpw_cam_add(int32_t error, uint64_t table) { AddUIError(error, reinterpret_cast<TbITable*>(static_cast<uintptr_t>(table))); }
void tpw_cam_dtor(uint64_t self, int16_t flag) { errdeque_callee::QueueCdequeDtor(reinterpret_cast<void*>(static_cast<uintptr_t>(self)), flag); }

}  // extern "C"
