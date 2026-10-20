// Test library for the online lobby's interface set-up and the bankrupt window's message function (unit v5060;
// libtpw_unit_InitOnlineUI.dylib, tests/test_InitOnlineUI.py), main app:
//   src/lobby_exit_InitOnlineUI.cpp   InitOnlineUI, BankruptWindowMsg
//
// Every callee outside the unit is a recording fake; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "Light.h"
#include "MapWho.h"
#include "TPEngine.h"
#include "TPSettings.h"
#include "UI_Base.h"
#include "keyboard.h"
#include "tb_resource_system.h"
#include "ui_ErrorDisplay.h"
#include "ui_Postcard.h"
#include "ui_main.h"

using namespace calllog;

void CreateOnlinePanel();

// ---- globals of other files
CSystem gei_System;
CMapWho* gei_MapWho;
CLight gUILight;
CPlayerSettings gPlayerSettings;
InterfaceWindow* gUIRootWindow;
InterfaceWindow* gFirstPersonWindow;
InterfaceWindow* gBumholeWindow;
InterfaceWindow* gBankruptWindow;
uint8_t gUIMainKeyTablesDisabled;
int32_t gCongratsTicks;
int16_t gFirstPersonWindowScript[41];
int16_t gBankruptScript[8];
UI_RectDelta gBankruptAnim[4];

namespace {
uint64_t gWin[4];   // [0] CreateObject's answer, [1] Build's answer (set by the test)
}

// ---- fakes for classes the unit never builds for real
CSystem::CSystem() {}
CMapLinkBase::~CMapLinkBase() {}
CLight::CLight() {}
CLight::~CLight() {}
int32_t CLight::Prepare() { return 0; }
int32_t CLight::GetLinkType() { return 1; }
void* CLight::GetData() { return nullptr; }
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}

bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect() { ev("CoolRect", {}); }
CoolRect::~CoolRect() { ev("~CoolRect", {}); }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

int32_t Engine_GameFirstPersonMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

// ---- the fakes
void UI_FlushAllMessages() { ev("Flush", {}); }
void CSystem::UpdateLights() { ev("UpdateLights", {}); }
int32_t CMapWho::Link(CMapLinkBase* link, int32_t list) {
    ev("Link", {lab(this), lab(link), list});
    return static_cast<int32_t>(ret("Link"));
}
void CreateOnlinePanel() { ev("CreateOnlinePanel", {}); }
InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t l, int16_t t, int16_t r,
                                               int16_t b) {
    ev("CreateObject", {lab(this), kind, attributes, id, l, t, r, b});
    return ret("CreateObject") ? reinterpret_cast<InterfaceWindow*>(gWin[0]) : nullptr;
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallback", {lab(this), callback == Engine_GameFirstPersonMsg ? 1 : callback == BankruptWindowMsg ? 2 : callback ? 3 : 0});
}
void InterfaceWindow::setCursor(int16_t cursor) { ev("setCursor", {lab(this), cursor}); }
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    int script = description == gFirstPersonWindowScript ? 1 : description == gBankruptScript ? 2 : description ? 3 : 0;
    int cb = callback == Engine_GameFirstPersonMsg ? 1 : callback == BankruptWindowMsg ? 2 : callback ? 3 : 0;
    ev("Build", {lab(this), script, cb});
    return ret("Build") ? reinterpret_cast<InterfaceWindow*>(gWin[1]) : nullptr;
}
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), subtract});
    rect->mRight = static_cast<int16_t>(ret("GPRight"));
    rect->mBottom = static_cast<int16_t>(ret("GPBottom"));
    return static_cast<int32_t>(ret("GetPosition"));
}
void InterfaceWindow::setPivot(int16_t x, int16_t y) { ev("setPivot", {lab(this), x, y}); }
int32_t InterfaceWindow::startAnim(UI_RectDelta* anim, int32_t ticks) {
    ev("startAnim", {lab(this), anim == gBankruptAnim ? 1 : anim ? 2 : 0, ticks});
    return static_cast<int32_t>(ret("startAnim"));
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("Default", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("Default"));
}
void UI_EnableHelpText() { ev("EnableHelp", {}); }
void UI_DisableHelpText() { ev("DisableHelp", {}); }
uint32_t IveGotTicks() { return static_cast<uint32_t>(ret("Ticks")); }
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("SendMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("SendMessage"));
}
void StopRidingRideIn1stPerson() { ev("StopRiding", {}); }
void ToggleGameWindow() { ev("ToggleGameWindow", {}); }
void EnableAllKeytables() { ev("EnableAllKeytables", {}); }

extern "C" {

uint64_t tpw_ou_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    static CMapLinkBase* base = nullptr;
    base = static_cast<CMapLinkBase*>(&gUILight);
    const Entry entries[] = {
        {"light", &gUILight},       {"lightbase", base},          {"settings", &gPlayerSettings},
        {"mapwho", &gei_MapWho},    {"root", &gUIRootWindow},     {"fp", &gFirstPersonWindow},
        {"bump", &gBumholeWindow},  {"bank", &gBankruptWindow},   {"ticks", &gCongratsTicks},
        {"kt", &gUIMainKeyTablesDisabled},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ou_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_ou_setwin(int32_t index, uint64_t address) { gWin[index] = address; }

int32_t tpw_ou_layout(int64_t* out) {
    int32_t n = 0;
    CoolRect rect;
    char* l = reinterpret_cast<char*>(&gUILight);
    char* s = reinterpret_cast<char*>(&gPlayerSettings);
    out[n++] = reinterpret_cast<char*>(&gUILight.mColor[0]) - l;
    out[n++] = reinterpret_cast<char*>(&gUILight.mNormal) - l;
    out[n++] = reinterpret_cast<char*>(&gUILight.mColorWord) - l;
    out[n++] = reinterpret_cast<char*>(&gPlayerSettings.mCurrent.mTooltipsOn) - s;
    out[n++] = reinterpret_cast<char*>(&gPlayerSettings.mCurrent.mRMBCancelOn) - s;
    out[n++] = reinterpret_cast<char*>(&rect.mRight) - reinterpret_cast<char*>(&rect);
    out[n++] = reinterpret_cast<char*>(&rect.mBottom) - reinterpret_cast<char*>(&rect);
    return n;
}

void tpw_ou_InitOnlineUI() { InitOnlineUI(); }
int32_t tpw_ou_BankruptWindowMsg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return BankruptWindowMsg(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}

}  // extern "C"
