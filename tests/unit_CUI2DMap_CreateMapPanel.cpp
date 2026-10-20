// Test library for CUI2DMap::CreateMapPanel (unit v5986; libtpw_unit_CUI2DMap_CreateMapPanel.dylib, tests/test_CUI2DMap_CreateMapPanel.py), main app:
//   src/CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_UnRegister_CUI2DMap_CreateMapPanel.cpp
//   CUI2DMap::CreateMapPanel, gMapPanelScript and the weak stand-ins UI_2DMap_CallbackFn and mappanel_callee::NewTextSprite
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queue. The globals of
// other files the unit reads are defined here (the big engine objects as raw storage under the program's symbol names, so that no
// constructor is needed).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CellBase.h"
#include "Debug.h"
#include "DrawRenderClasses.h"
#include "DrawState.h"
#include "Light.h"
#include "TPCoaster.h"
#include "TPMain.h"
#include "TbGraphicFile.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "platform/d3d_types.h"
#include "ppc_math.h"
#include "ui_main.h"
#include "Main.h"
#include "TbRaConvert.h"
#include "TbTList.h"
#include "tb_ezsprite.h"
#include "tb_mapzoom.h"
#include "tb_resource_texture.h"
#include "Advisor.h"
#include "AdvisorScoring.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "MapWho.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "UI_Base.h"
#include "tb_resource_display.h"
#include "tb_texturecache.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_tooltip.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
alignas(16) char gGameStore[sizeof(CGame)] __asm__("_gGame");
alignas(16) char gDisplayStore[sizeof(CDisplay) + 0x10] __asm__("_gei_Display");
alignas(16) char gMapZoomStore[sizeof(CMapZoom)] __asm__("_gei_MapZoom");
CUI2DMap gUI2DMap;
InterfaceWindow* gGameWindow;
InterfaceWindow* gUIRootWindow;
CMapWho* gei_MapWho;
CTextureCache* gei_TCache;
int32_t gHeightfieldWidth;
int32_t gHeightfieldHeight;
float gCameraTarget[3];

namespace {

const int32_t kIds[16] = {0x990, 0x991, 0x995, 0x982, 0x987, 0x989, 0x993, 0x994, 0x983, 0x98b, 0x996, 0x98a, 0x984, 0x981, 0x988, 0x992};
void* gWin[16];
void* gPanel;
void* gFont;

int64_t Off(const void* p) { return reinterpret_cast<const char*>(p) - reinterpret_cast<const char*>(&gUI2DMap); }

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub (pointers as lab(p))
int32_t MainMenuUp() {
    ev("MainMenuUp", {});
    return static_cast<int32_t>(ret("MainMenuUp"));
}
void CGame::Pause(ePauseFlag flag, bool allSounds) { ev("Pause", {lab(this), static_cast<int32_t>(flag), allSounds ? 1 : 0}); }
void UI_PauseTimerSystem() { ev("UI_PauseTimerSystem", {}); }
void ADV_Kill(int32_t mode) { ev("ADV_Kill", {mode}); }
void CDisplay::SetFogColor(uint32_t argb) { ev("SetFogColor", {lab(this), argb}); }
int32_t CMapWho::GetRenderOptions() {
    ev("GetRenderOptions", {lab(this)});
    return static_cast<int32_t>(ret("GetRenderOptions"));
}
void CMapWho::SetRenderOptions(int32_t options) { ev("SetRenderOptions", {lab(this), options}); }
void CTextureCache::Flush(int32_t mode) { ev("Flush", {lab(this), mode}); }
bool UIMain_CheckPanel() {
    ev("UIMain_CheckPanel", {});
    return false;
}
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), lab(description), lab(reinterpret_cast<const void*>(callback))});
    return static_cast<InterfaceWindow*>(gPanel);
}
void UIMain_SetPanel(InterfaceWindow* window) { ev("UIMain_SetPanel", {lab(window)}); }
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    for (int i = 0; i < 16; i++)
        if (kIds[i] == id) return static_cast<InterfaceWindow*>(gWin[i]);
    return nullptr;
}
namespace mappanel_callee {
InterfaceDrawObject* NewTextSprite() {
    ev("new", {0x19c});
    void* p = tpw::ArenaAllocZeroed(0x19c);
    ev("TextSpriteCtor", {newlab(p), 0});
    return static_cast<InterfaceDrawObject*>(p);
}
}  // namespace mappanel_callee
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFontJust(int32_t h, int32_t v) { ev("SetFontJust", {lab(this), h, v}); }
void InterfaceWindow::SetFontWrap(int32_t h, int32_t v) { ev("SetFontWrap", {lab(this), h, v}); }
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return static_cast<InterfaceFont*>(gFont);
}
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontColour(int32_t r, int32_t g, int32_t b, int32_t a) { ev("SetFontColour", {lab(this), r, g, b, a}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), lab(text)}); }
namespace entry_callee {
void WindowEnable(InterfaceWindow* window, int16_t on) { ev("Enable", {lab(window), on, 1}); }
}  // namespace entry_callee
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }
int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {lab(this), state, redraw});
    return 0;
}
int32_t InterfaceCheckButtons::SetCurrent(int32_t id) {
    ev("SetCurrent", {lab(this), id});
    return -1;
}
void InterfaceButton::SetAutoRepeat(int32_t delay, int32_t interval) { ev("SetAutoRepeat", {lab(this), delay, interval}); }
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), lab(reinterpret_cast<const void*>(callback))});
}
void GetHFieldMaxVertexIndices(uint32_t* width, uint32_t* height) {
    ev("GetHFieldMaxVertexIndices", {Off(width), Off(height)});
    *width = static_cast<uint32_t>(ret("HFieldW"));
    *height = static_cast<uint32_t>(ret("HFieldH"));
}
void CUI2DMap::StoreMapGridDetails() { ev("StoreMapGridDetails", {lab(this)}); }
void CUI2DMap::InitSprites() { ev("InitSprites", {lab(this)}); }
void CUI2DMap::LoadMap(int32_t reload) { ev("LoadMap", {lab(this), reload}); }
void CUI2DMap::UpdateMap() { ev("UpdateMap", {lab(this)}); }
int32_t CMapZoom::Init(void* data, int32_t width, int32_t height, int32_t pitch) {
    ev("MapZoomInit", {lab(this), static_cast<uint32_t>(reinterpret_cast<uintptr_t>(data)), width, height, pitch});
    return 1;
}
void CMapZoom::DisableFiltering() { ev("DisableFiltering", {lab(this)}); }
void UIMain_AdvisorMessage(AdvisorMessage message) { ev("UIMain_AdvisorMessage", {static_cast<int32_t>(message)}); }
void MoveHelpText(int16_t x, int16_t y) { ev("MoveHelpText", {x, y}); }
void UI_2DMap_MapAreaCallback(InterfaceWindow*, int32_t, int32_t, int32_t) {}

// The weak NewTextSprite of the src file names the constructor, which this library does not link (it is faked): give the symbol a body.
void FakeTextSpriteCtor(void*) __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
void FakeTextSpriteCtor(void*) {}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_cmp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"map", &gUI2DMap},
        {"game", gGameStore},
        {"display", gDisplayStore},
        {"mapzoom", gMapZoomStore},
        {"script", &gMapPanelScript},
        {"cbfn", reinterpret_cast<const void*>(&UI_2DMap_CallbackFn)},
        {"areacb", reinterpret_cast<const void*>(&UI_2DMap_MapAreaCallback)},
        {"v_gamew", &gGameWindow},
        {"v_root", &gUIRootWindow},
        {"v_mapwho", &gei_MapWho},
        {"v_tcache", &gei_TCache},
        {"camera", gCameraTarget},
        {"hfw", &gHeightfieldWidth},
        {"hfh", &gHeightfieldHeight},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_cmp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// slot 0 the panel Build answers, 1 the font GetFontTableEntry answers, 2 + i the window GetWindow answers for kIds[i].
void tpw_cmp_set(int32_t slot, uint64_t pointer) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(pointer));
    if (slot == 0) gPanel = p;
    else if (slot == 1) gFont = p;
    else gWin[slot - 2] = p;
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_cmp_layout(int64_t* out) {
    int32_t n = 0;
    InterfaceWindow* window = static_cast<InterfaceWindow*>(tpw::ArenaAllocZeroed(sizeof(InterfaceWindow) + 16));
    out[n++] = reinterpret_cast<char*>(&window->mZ) - reinterpret_cast<char*>(window);
    out[n++] = reinterpret_cast<char*>(&reinterpret_cast<CGame*>(gGameStore)->mF3C) - gGameStore;
    out[n++] = reinterpret_cast<char*>(&reinterpret_cast<CDisplay*>(gDisplayStore)->mFogColor) - gDisplayStore;
    out[n++] = static_cast<int64_t>(sizeof(CUI2DMap));
    return n;
}

// ---- the function
void tpw_cmp_create() { gUI2DMap.CreateMapPanel(); }

}  // extern "C"
