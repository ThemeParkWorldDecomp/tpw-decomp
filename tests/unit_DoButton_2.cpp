// Test library for the options panel's DoButton and OptionsPanelMsg (unit v5978; libtpw_unit_DoButton_2.dylib, tests/test_DoButton_2.py), main app:
//   src/game/ui/ui_optionsPanel.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals of
// other files are defined here (zero at start like the Mac's bss). gGame and gPlayerSettings are raw storage under the program's symbols.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "Game.h"
#include "HLSprite.h"
#include "InterfaceWindow.h"
#include "MapWho.h"
#include "TPSettings.h"
#include "UI_Base.h"
#include "lobby.h"
#include "tb_ddraw.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_parkStatus.h"

using namespace calllog;

// ---- globals of other files
alignas(16) char gGameStorage[sizeof(CGame)] __asm__("_gGame");
alignas(16) char gPlayerSettingsStorage[sizeof(CPlayerSettings)] __asm__("_gPlayerSettings");
uint8_t gOptionsPanelGameRunning;
int32_t gOptionsPanelF0C;
int32_t gOptionsPanelRenderOptions;
InterfaceWindow* gSetOnOffRoot;
InterfaceWindow* gGameWindow;
CLBLobby* gLobby;
char16_t gUIMainConvertBuffer[0x4000];
CDirectDraw* gei_DDraw;
CMapWho* gei_MapWho;

namespace {

const uintptr_t kRoot = 0x32000000;
const uintptr_t kChildren = 0x31000000;   // the window objects are never dereferenced: 0x140 bytes apart, 0x40 ids per level

int64_t TextHash(const char16_t* text, int64_t* length) {
    uint32_t hash = 0;
    int64_t n = 0;
    while (text[n] != 0 && n < 0x100) {
        hash += static_cast<uint32_t>(n + 1) * text[n];
        n++;
    }
    *length = n;
    return hash;
}

}  // namespace

// ---- the fakes
int32_t CPlayerSettings::GetSFXVolume() const { ev("GetSFX", {}); return static_cast<int32_t>(ret("GetSFX")); }
int32_t CPlayerSettings::GetMusicVolume() const { ev("GetMusic", {}); return static_cast<int32_t>(ret("GetMusic")); }
int32_t CPlayerSettings::GetSpeechVolume() const { ev("GetSpeech", {}); return static_cast<int32_t>(ret("GetSpeech")); }
int32_t CPlayerSettings::GetMovieVolume() const { ev("GetMovie", {}); return static_cast<int32_t>(ret("GetMovie")); }
void CPlayerSettings::SetRendererType(eRENDERER type) { ev("SetRendererType", {static_cast<int32_t>(type)}); }
void CPlayerSettings::SetVideoCard(eVIDEOCARD card) { ev("SetVideoCard", {static_cast<int32_t>(card)}); }
void CPlayerSettings::UpdateSettings() { ev("UpdateSettings", {}); }
void CPlayerSettings::ResetSettings() { ev("ResetSettings", {}); }
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}
void CGame::Unpause() { ev("Unpause", {}); }
void CMapWho::SetRenderOptions(int32_t options) { ev("SetRenderOptions", {lab(this), options}); }
void UI_UnPauseTimerSystem() { ev("UnPauseTimer", {}); }
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("SendMessage", {lab(window), message, a, b});
    return 0;
}
void UIMain_DecideKbCapture() { ev("DecideKbCapture", {}); }
void UI_EnableHelpText() { ev("EnableHelp", {}); }
void UI_DisableHelpText() { ev("DisableHelp", {}); }
void BTN_HighLight_Kill() { ev("HighLightKill", {}); }
void RemoveOptionsPanel() { ev("RemoveOptionsPanel", {}); }
void SetOnOffText(int32_t window, int32_t text, bool first, bool second, bool button, int32_t buttonId) {
    ev("SetOnOffText", {window, text, first, second, button, buttonId});
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    char16_t text[16];
    int n = 0;
    text[n++] = 's';
    for (int shift = 12; shift >= 0; shift -= 4) text[n++] = u"0123456789abcdef"[(static_cast<uint32_t>(id) >> shift) & 15];
    text[n] = 0;
    std::memcpy(gUIMainConvertBuffer, text, sizeof(text));
    return gUIMainConvertBuffer;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    uintptr_t level = reinterpret_cast<uintptr_t>(this) == kRoot ? 0 : 1;
    uintptr_t index = level * 0x40 + static_cast<uintptr_t>(id - 0x1d4c0);
    return reinterpret_cast<InterfaceWindow*>(kChildren + index * 0x140);
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("Default", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("Default"));
}
int32_t options_callee::CallSetValue(InterfaceWindow* window, int32_t value) {
    ev("SetValue", {lab(window), value});
    return static_cast<int32_t>(ret("SetValue"));
}
void options_callee::CallSetText(InterfaceWindow* window, const char16_t* text) {
    int64_t length = 0;
    int64_t hash = TextHash(text, &length);
    ev("SetText", {lab(window), length, hash, text == gUIMainConvertBuffer ? 1 : 0});
}
namespace options_panel {
void DoScroll(int32_t id, int32_t value) { ev("DoScroll", {id, value}); }
void StartScroll(int32_t id, int32_t value) { ev("StartScroll", {id, value}); }
}  // namespace options_panel

extern "C" {

uint64_t tpw_db_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"settings", gPlayerSettingsStorage},
        {"root", &gSetOnOffRoot},
        {"buffer", gUIMainConvertBuffer},
        {"ddraw", &gei_DDraw},
        {"mapwho", &gei_MapWho},
        {"lobby", &gLobby},
        {"game", gGameStorage},
        {"gamewindow", &gGameWindow},
        {"f0c", &gOptionsPanelF0C},
        {"renderoptions", &gOptionsPanelRenderOptions},
        {"running", &gOptionsPanelGameRunning},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_db_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_db_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CGame, mF3C));
    out[n++] = static_cast<int64_t>(offsetof(CLBLobby, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(CDirectDraw, mDeviceCount));
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings, mCurrent));
    out[n++] = static_cast<int64_t>(sizeof(CPlayerSettings::Settings));
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings::Settings, mTooltipsOn));
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings::Settings, mIsometricOn));
    return n;
}

void tpw_db_button(int32_t id, int32_t on) { options_panel::DoButton(id, on); }

int32_t tpw_db_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return OptionsPanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
