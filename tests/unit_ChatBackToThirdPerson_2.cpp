// Test library for the online chat panel's first-person switches (unit v5919; libtpw_unit_ChatBackToThirdPerson_2.dylib, tests/test_ChatBackToThirdPerson_2.py), main app:
//   src/game/ui/ui_onlinePanel.cpp   ChatBackToThirdPerson, ChatGoFirstPerson, UpdateAvatarPos, ChatGoFullScreen
//   and the globals gOnlineFirstPersonCountdown, gOnlineSavedFlags, gOnlineMapZ, gOnlineMapX
//
// The callees outside the unit are fakes that record what the real ones read: UI_SendMessage, InterfaceWindow::{setCursor, GetPosition,
// GetWindow, CaptureKb}, CoolRect (constructor, destructor and the virtuals the vtable needs), COnlineControl (constructor, HideAvatar and the
// four first-person calls), operator new, CMouse::AssignMouseMode, CMMDefault (constructor and virtuals). The globals of other files are
// defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "Mouse.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "lobby.h"
#include "online.h"
#include "ui_Postcard.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
uint32_t gFirstPersonFlags;
s3DPoint gCameraViewPoint;
float gCameraSavedPosition[3];
float gCameraPosition[3];
s3DPoint gPointerWorldPosn;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouseMode* gMouseMode;
CMouse* gMouse;
uint8_t gOnlineByte78;
uint8_t gOnlineBlockToggle;
uint8_t gOnlinePanelShown;
InterfaceWindow* gOnlineWord80;
InterfaceWindow* gOnlineWindow;
InterfaceWindow* gOnlinePanel;
COnlineControl* gOnlineControl;

namespace {

InterfaceWindow* gWins[3];
int gWinNext = 0;

// The mode the test hangs on gMouseMode: GetMode answers from the case's queue.
class ProbeMode : public CMouseMode {
public:
    int32_t GetMode() override {
        ev("GetMode", {});
        return static_cast<int32_t>(ret("GetMode"));
    }
};
ProbeMode gProbe;

}  // namespace

// ---- the fakes
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}

void InterfaceWindow::setCursor(int16_t cursor) { ev("setCursor", {lab(this), cursor}); }

int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), lab(rect), subtract});
    return 0;
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    InterfaceWindow* answer = gWinNext < 3 ? gWins[gWinNext] : nullptr;
    gWinNext++;
    return answer;
}

void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }

CoolRegion::~CoolRegion() {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }

CoolRect::CoolRect() { ev("CoolRect", {newlab(this)}); }
CoolRect::~CoolRect() { ev("~CoolRect", {lab(this)}); }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

namespace online_callee {
void* OperatorNew(uint32_t size) {
    ev("OperatorNew", {size});
    return tpw::ArenaAllocZeroed(size);
}
}  // namespace online_callee

COnlineControl::COnlineControl() { ev("COnlineControl", {newlab(this)}); }
void COnlineControl::HideAvatar() { ev("HideAvatar", {lab(this)}); }
void COnlineControl::EnteringFirstPerson(MapPos x, MapPos y) {
    ev("EnteringFirstPerson", {lab(this), (x.mCell << 8) | x.mOffset, (y.mCell << 8) | y.mOffset});
}
void COnlineControl::LeavingFirstPerson(MapPos x, MapPos y) {
    ev("LeavingFirstPerson", {lab(this), (x.mCell << 8) | x.mOffset, (y.mCell << 8) | y.mOffset});
}
void COnlineControl::UpdateFirstPersonPos(MapPos x, MapPos y) {
    ev("UpdateFirstPersonPos", {lab(this), (x.mCell << 8) | x.mOffset, (y.mCell << 8) | y.mOffset});
}
void COnlineControl::UnHideAvatar(MapPos x, MapPos y) {
    ev("UnHideAvatar", {lab(this), (x.mCell << 8) | x.mOffset, (y.mCell << 8) | y.mOffset});
}

void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }

CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

extern "C" {

uint64_t tpw_ccp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"flags", &gFirstPersonFlags},     {"view", &gCameraViewPoint},       {"saved", gCameraSavedPosition},
        {"campos", gCameraPosition},       {"pointer", &gPointerWorldPosn},   {"m1e0", &gMouseWord1E0},
        {"m1e4", &gMouseWord1E4},          {"mode", &gMouseMode},             {"mouse", &gMouse},
        {"b78", &gOnlineByte78},           {"b79", &gOnlineBlockToggle},      {"shown", &gOnlinePanelShown},
        {"word80", &gOnlineWord80},        {"window", &gOnlineWindow},        {"panel", &gOnlinePanel},
        {"control", &gOnlineControl},      {"cnt", &gOnlineFirstPersonCountdown}, {"sflags", &gOnlineSavedFlags},
        {"mz", gOnlineMapZ},               {"mx", gOnlineMapX},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ccp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The three windows GetWindow answers in turn (the counter starts again).
void tpw_ccp_wins(uint64_t a, uint64_t b, uint64_t c) {
    gWins[0] = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(a));
    gWins[1] = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(b));
    gWins[2] = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(c));
    gWinNext = 0;
}

uint64_t tpw_ccp_probe() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(static_cast<CMouseMode*>(&gProbe))); }

int32_t tpw_ccp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(MapPos));
    out[n++] = static_cast<int64_t>(sizeof(gOnlineMapX));
    out[n++] = static_cast<int64_t>(offsetof(s3DPoint, z));
    out[n++] = static_cast<int64_t>(sizeof(s3DPoint));
    return n;
}

void tpw_ccp_back() { ChatBackToThirdPerson(); }
void tpw_ccp_first() { ChatGoFirstPerson(); }
void tpw_ccp_update() { UpdateAvatarPos(); }
void tpw_ccp_full() { ChatGoFullScreen(); }

}  // extern "C"
