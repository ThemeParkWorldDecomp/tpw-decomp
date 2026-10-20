// Test library for CUI2DMap::MapCallback (unit v5526; libtpw_unit_CUI2DMap_MapCallback.dylib, tests/test_CUI2DMap_MapCallback.py), main app:
//   src/CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_MapCallback.cpp   CUI2DMap::MapCallback
//
// Every callee is a fake that records the arguments the real one reads and answers from the test's queue; the globals of other files the
// unit reads (gGame, gPlayerSettings, gei_Display, gei_MapWho, gGameWindow) are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "MapWho.h"
#include "TPCamera.h"
#include "TPSettings.h"
#include "TbGraphicFile.h"
#include "UI_Base.h"
#include "tb_resource_display.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_tooltip.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
// gGame, gPlayerSettings and gei_Display are zeroed storage under their symbols (their constructors pull in half the program).
alignas(16) unsigned char gGameStorage[0x1000] __asm__("_gGame");
alignas(16) unsigned char gPlayerSettingsStorage[0x1000] __asm__("_gPlayerSettings");
alignas(16) unsigned char gDisplayStorage[0x1000] __asm__("_gei_Display");
CMapWho* gei_MapWho = nullptr;
InterfaceWindow* gGameWindow = nullptr;

// ---- the fakes: the same event name and arguments as the module's PowerPC stub (pointers as lab(p))
void CUI2DMap::UpdateMapZoom() { ev("UpdateMapZoom", {lab(this)}); }
void CUI2DMap::CloseSprites() { ev("CloseSprites", {lab(this)}); }
void CUI2DMap::UpdateMap() { ev("UpdateMap", {lab(this)}); }
void CUI2DMap::MoveCentre(EUI2DMapMoveDirection direction, float amount) {
    ev("MoveCentre", {lab(this), static_cast<int32_t>(direction), static_cast<int64_t>(fbits(amount))});
}
void CUI2DMap::Zoom(EUI2DMapZoom zoom, float amount) {
    ev("Zoom", {lab(this), static_cast<int32_t>(zoom), static_cast<int64_t>(fbits(amount))});
}
void TbRenderArea::FreeMemory() { ev("FreeMemory", {lab(this)}); }
void CDisplay::SetFogColor(uint32_t argb) { ev("SetFogColor", {lab(this), static_cast<int64_t>(argb)}); }
void CMapWho::SetRenderOptions(int32_t options) { ev("SetRenderOptions", {lab(this), options}); }
void CGame::Unpause() { ev("Unpause", {lab(this)}); }
void UI_UnPauseTimerSystem() { ev("UnPauseTimerSystem", {}); }
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("SendMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("SendMessage"));
}
void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("CancelAdvisorMessage", {static_cast<int32_t>(message)}); }
void UIMain_SetPanel(InterfaceWindow* window) { ev("SetPanel", {lab(window)}); }
void UIMain_DecideKbCapture() { ev("DecideKbCapture", {}); }
void MoveHelpText(int16_t x, int16_t y) { ev("MoveHelpText", {x, y}); }
void SetCameraPOI(float* x, float* z, float* zoom, float* yaw, float* value) {
    ev("SetCameraPOI", {static_cast<int64_t>(fbits(*x)), static_cast<int64_t>(fbits(*z)), static_cast<int64_t>(reinterpret_cast<uintptr_t>(zoom)),
                        static_cast<int64_t>(reinterpret_cast<uintptr_t>(yaw)), static_cast<int64_t>(reinterpret_cast<uintptr_t>(value))});
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("Default", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("Default"));
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_mc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gGame", &gGame},
        {"settings", &gPlayerSettings},
        {"gameWindow", &gGameWindow},
        {"mapWho", &gei_MapWho},
        {"display", &gei_Display},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint32_t tpw_mc_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

uint64_t tpw_mc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_mc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(reinterpret_cast<const char*>(&gGame.mF3C) - reinterpret_cast<const char*>(&gGame));
    out[n++] = static_cast<int64_t>(reinterpret_cast<const char*>(&gPlayerSettings.mCurrent.mRMBScrollOn) - reinterpret_cast<const char*>(&gPlayerSettings));
    out[n++] = static_cast<int64_t>(sizeof(CUI2DMap));
    return n;
}

uint32_t tpw_mc_call(uint64_t map, uint64_t window, int32_t message, int32_t a, int32_t b) {
    return static_cast<uint32_t>(reinterpret_cast<CUI2DMap*>(static_cast<uintptr_t>(map))
                                     ->MapCallback(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b));
}

}  // extern "C"
