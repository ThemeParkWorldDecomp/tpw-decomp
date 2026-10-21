// Test library for the world lobby controller (unit v5948; libtpw_unit_CreateWorldLobbyController.dylib, tests/test_CreateWorldLobbyController.py), main app:
//   src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_worldLobbyController.cpp   CreateWorldLobbyController, SatelliteMsg; defines gWorldLobbyScript
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues. The windows are plain
// blocks (no vtable): the two virtual calls (Enable, SetValue) go through WindowEnable and WindowSetValue, which are fakes too.
// The globals of other files are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
int32_t gChatSatelliteWanted;       // 0x10396fe8
InterfaceWindow* gUISatelliteParent;   // 0x10396fec
CFSOnlineSystem* gOnlineSystem;     // 0x10380ab0
CLBLobby* gLobby;                   // 0x102e4888

namespace {

constexpr int kWindows = 15;
alignas(16) char gWin[kWindows][0x200];
alignas(16) char gLobbyWin[0x200];
alignas(16) char gFont[16];
alignas(16) char gLobbyMem[0x400];
int32_t gNewVals[4];                 // what a system made by the constructor holds in the four lists' word 0x28
CFSOnlineSystem* gPreSystem;         // the system the world started with (0: none)

InterfaceWindow* Win(int i) { return reinterpret_cast<InterfaceWindow*>(gWin[i]); }

CFSOnlineSystem* MakeSystem() {
    auto* sys = static_cast<CFSOnlineSystem*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystem)));
    sys->mCityList = static_cast<CFSOnlineSystemCityList*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystemCityList)));
    sys->mThemeList = static_cast<CFSOnlineSystemThemeList*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystemThemeList)));
    sys->mServerList = static_cast<CFSOnlineSystemServerList*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystemServerList)));
    sys->mParksInCity2 = static_cast<CFSOnlineSystemParkList_InCity*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystemParkList_InCity)));
    sys->mCityList->mF28 = gNewVals[0];
    sys->mThemeList->mF28 = gNewVals[1];
    sys->mServerList->mF28 = gNewVals[2];
    sys->mParksInCity2->mF28 = gNewVals[3];
    return sys;
}

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), description == gWorldLobbyScript ? 1 : 0, callback == WorldLobbyControllerMsg ? 1 : 0});
    return Win(0);
}

int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {lab(this), index, static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value)))});
    return 0;
}

void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("getExtraParam", {lab(this), index});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("getExtraParam"))));
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    int i = id - 0x16000 + 1;
    return Win(i >= 1 && i <= 13 ? i : 0);
}

int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }

void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }

void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }

void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), callback == SatelliteMsg ? 1 : 0});
}

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}

void InterfaceButton::SetAutoRepeat(int32_t delay, int32_t interval) { ev("SetAutoRepeat", {lab(this), delay, interval}); }

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(gFont);
}

namespace entry_callee {
void WindowEnable(InterfaceWindow* window, int16_t on) { ev("Enable", {lab(window), on}); }
}  // namespace entry_callee

namespace lender_callee {
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {lab(window), value}); }
}  // namespace lender_callee

void CFSOnlineSystem::EnterChatSatellite(int32_t index) { ev("EnterChatSatellite", {lab(this), index}); }

void ProcessGetChatSatellitePlayers() { ev("ProcessGetChatSatellitePlayers", {}); }

int32_t WorldLobbyControllerMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

namespace lender_callee {
void NumberCtor(void* mem) { ev("NumberText", {newlab(mem), 0}); }
}  // namespace lender_callee

namespace worldlobby_callee {
InterfaceDrawObject* NewNumberText() {
    ev("new", {0x19c});
    void* p = tpw::ArenaAllocZeroed(0x200);
    lender_callee::NumberCtor(p);
    return reinterpret_cast<InterfaceDrawObject*>(p);
}
}  // namespace worldlobby_callee

namespace findpanel_callee {
CFSOnlineSystem* NewOnlineSystem() {
    ev("new", {0x404});
    CFSOnlineSystem* sys = MakeSystem();
    ev("OnlineSystem", {newlab(sys)});
    return sys;
}
}  // namespace findpanel_callee

extern "C" {

uint64_t tpw_cwl_addr(const char* name) {
    if (std::strcmp(name, "lobbywin") == 0) return reinterpret_cast<uintptr_t>(gLobbyWin);
    if (std::strcmp(name, "font") == 0) return reinterpret_cast<uintptr_t>(gFont);
    if (std::strncmp(name, "win", 3) == 0) {
        int i = std::atoi(name + 3);
        if (i >= 0 && i < kWindows) return reinterpret_cast<uintptr_t>(gWin[i]);
    }
    return 0;
}

uint64_t tpw_cwl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cwl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(int32_t));
    return n;
}

// The world: the satellite parent (0 none, 1 the first window), the lobby (0: its window is NULL, 1: the lobby window), the wanted word,
// the system (has_system: made now with the four values; else the constructor makes one with them). Answers the system's address.
uint64_t tpw_cwl_world(int32_t parent, int32_t lobby, int32_t wanted, int32_t has_system, const int32_t* vals) {
    std::memset(gLobbyMem, 0, sizeof gLobbyMem);
    gLobby = reinterpret_cast<CLBLobby*>(gLobbyMem);
    gLobby->mWindow = lobby ? reinterpret_cast<InterfaceWindow*>(gLobbyWin) : nullptr;
    gUISatelliteParent = parent ? Win(0) : nullptr;
    gChatSatelliteWanted = wanted;
    for (int i = 0; i < 4; ++i) gNewVals[i] = vals[i];
    gOnlineSystem = has_system ? MakeSystem() : nullptr;
    gPreSystem = gOnlineSystem;
    return reinterpret_cast<uintptr_t>(gOnlineSystem);
}

// wanted, the parent (0 none, 1 the first window, 2 other), the system (0 none, 1 the one the world had, 2 another)
void tpw_cwl_state(int64_t* out) {
    out[0] = gChatSatelliteWanted;
    out[1] = gUISatelliteParent == nullptr ? 0 : (gUISatelliteParent == Win(0) ? 1 : 2);
    out[2] = gOnlineSystem == nullptr ? 0 : (gOnlineSystem == gPreSystem ? 1 : 2);
}

void tpw_cwl_create(uint32_t publish) { CreateWorldLobbyController(publish != 0); }

uint32_t tpw_cwl_satellite(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return static_cast<uint32_t>(SatelliteMsg(reinterpret_cast<InterfaceWindow*>(window), message, a, b));
}

}  // extern "C"
