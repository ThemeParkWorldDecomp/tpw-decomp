// Test library for the game and lobby menus' handlers (unit v5522; libtpw_unit_MainGameMenuConfirmPublish_OK.dylib,
// tests/test_MainGameMenuConfirmPublish_OK.py), main app:
//   src/game/ui/ui_mainmenu.cpp   MainGameMenuConfirmPublish_OK, MainGameMenuCB, MainLobbyMenuConfirmLogout_OK,
//                                                        MainLobbyMenuCB
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals of
// other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"

#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "GameSys.h"
#include "SaveLoad.h"
#include "TPString.h"
#include "World.h"
#include "keyboard.h"
#include "lobby.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"
#include "ui_publishParkPanel.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CFSOnlineSystem* gOnlineSystem;
CFSUserInfo* gUserInfo;
CKeyTable* gKeyTableShortcuts;
CLBScreenSystem* gLobbyScreens;
int32_t gLobbyReturnFlag;
CWorld* gGameWorld;
OnlineUploadFile gOnlineUploadFile;
// The two objects the unit only passes as `this` (their constructors are other files'): raw bytes under their symbols.
alignas(64) unsigned char gSaveLoadBytes[0x10000] __asm__("_gSaveLoad");
alignas(64) unsigned char gGameBytes[0x10000] __asm__("_gGame");
static char gThemeText[64];

namespace {

int64_t Hash(const char* text) {
    uint32_t h = 0;
    for (uint32_t i = 0; text[i] != 0; ++i) h += static_cast<uint32_t>(static_cast<unsigned char>(text[i])) * (i + 1);
    return h;
}

int64_t HashWords(const uint16_t* words) {
    uint32_t h = 0;
    for (uint32_t i = 0; i < 0x100; ++i) h += static_cast<uint32_t>(words[i]) * (i + 1);
    return h;
}

int32_t Tag(const void* object) { return *reinterpret_cast<const int32_t*>(reinterpret_cast<const char*>(object) + 0x20); }

int64_t CbId(const void* f) {
    if (f == nullptr) return 0;
    if (f == reinterpret_cast<const void*>(&MainGameMenuConfirmRestart_OK)) return 1;
    if (f == reinterpret_cast<const void*>(&BankruptGameMenuConfirmRestart_OK)) return 2;
    if (f == reinterpret_cast<const void*>(&MainGameMenuConfirmPublish_OK)) return 3;
    if (f == reinterpret_cast<const void*>(&MainGameMenuConfirmQuit_OK)) return 4;
    if (f == reinterpret_cast<const void*>(&MainLobbyMenuConfirmQuit_OK)) return 5;
    if (f == reinterpret_cast<const void*>(&MainLobbyMenuConfirmLogout_OK)) return 6;
    if (f == reinterpret_cast<const void*>(&MainLobbyMenuPostOnlineLoginCallbackFn)) return 7;
    return -9;
}

}  // namespace

// ---- the callbacks the dialogs and the login hold (identities only)
int32_t MainGameMenuConfirmRestart_OK(void*) { return 11; }
int32_t BankruptGameMenuConfirmRestart_OK(void*) { return 12; }
int32_t MainGameMenuConfirmQuit_OK(void*) { return 13; }
int32_t MainLobbyMenuConfirmQuit_OK(void*) { return 14; }
void MainLobbyMenuPostOnlineLoginCallbackFn(int32_t, const void*) { gLobbyReturnFlag += 0; }

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub
void HideMainMenu() { ev("HideMainMenu", {}); }
void CreateLoadPanel(int32_t mode) { ev("CreateLoadPanel", {mode}); }
void CreateSavePanel(int32_t mode) { ev("CreateSavePanel", {mode}); }
void CreateOptionsPanel() { ev("CreateOptionsPanel", {}); }
void UI_ByeByeBankrupt() { ev("UI_ByeByeBankrupt", {}); }
void CreateLoginPanel() { ev("CreateLoginPanel", {}); }
void GS_SetCurrentGameStatus(GS_GAME_STATES status) { ev("GS_SetCurrentGameStatus", {static_cast<int32_t>(status)}); }

int32_t UIMenu_DefaultCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UIMenu_DefaultCallback", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("UIMenu_DefaultCallback"));
}

const char16_t* UIMain_GetString(int32_t id) {
    static char16_t table[0x200][4];
    ev("UIMain_GetString", {id});
    char16_t* text = table[id & 0x1ff];
    text[0] = 0x53;
    text[1] = static_cast<char16_t>(id);
    text[2] = 0;
    return text;
}

InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton button1, dlgcallback callback1, dlgbutton button2, dlgcallback callback2,
                              dlgbutton button3, dlgcallback callback3, bool flag) {
    const int64_t text = (static_cast<int64_t>(params.mText[0]) << 16) | params.mText[1];
    const int64_t rgb = (params.mRed << 16) | (params.mGreen << 8) | params.mBlue;
    const int64_t buttons = ((static_cast<int64_t>(button1) & 0xff) << 16) | ((static_cast<int64_t>(button2) & 0xff) << 8) | (static_cast<int64_t>(button3) & 0xff);
    const int64_t cbs = ((CbId(reinterpret_cast<const void*>(callback1)) & 0xff) << 16) | ((CbId(reinterpret_cast<const void*>(callback2)) & 0xff) << 8) |
                        (CbId(reinterpret_cast<const void*>(callback3)) & 0xff);
    ev("UI_DialogBox", {text, rgb, params.mValue8, params.mValueC, buttons, cbs, flag ? 1 : 0});
    return nullptr;
}

int32_t CFSOnlineSystem::AreWeProcessing() {
    ev("AreWeProcessing", {lab(this)});
    return static_cast<int32_t>(ret("AreWeProcessing"));
}
void CFSOnlineSystem::Logout() { ev("Logout", {lab(this)}); }
void CFSOnlineSystem::StopProcessing() { ev("StopProcessing", {lab(this)}); }
void CFSOnlineSystem::Login(void (*callback)(int32_t, const void*), const void* context) {
    ev("Login", {lab(this), CbId(reinterpret_cast<const void*>(callback)), static_cast<int64_t>(reinterpret_cast<uintptr_t>(context))});
}
void CFSOnlineSystem::SetPublishing(int32_t publishing, const char* text, const uint16_t* words) {
    ev("SetPublishing", {lab(this), publishing, Hash(text), HashWords(words)});
}
void CFSUserInfo::GameLogout() { ev("GameLogout", {lab(this)}); }

int32_t CKeyTable::Search(int16_t key, int16_t modifiers) {
    ev("Search", {lab(this), key, modifiers});
    return static_cast<int32_t>(ret("Search"));
}

void CLBScreenComponentAdvisor::Add(AdvisorResponse response, int32_t clearFirst) { ev("Add", {Tag(this), response.mId, clearFirst}); }

int32_t CSaveLoad::Save(const char* path, CSaveLoadHeader*, LoadType type) {
    ev("Save", {lab(this), Hash(path), static_cast<int32_t>(type)});
    return 1;
}

int32_t CWorld::ConvertDLRidesUsedToIndices(uint16_t* out) {
    const uint32_t seed = static_cast<uint32_t>(ret("ConvertDL"));
    for (uint32_t i = 0; i < 0x100; ++i) out[i] = static_cast<uint16_t>(seed * (i + 1) + i);
    ev("ConvertDL", {lab(this)});
    return 0;
}

const TbDynamicStringTemplate<char>& CGame::GetCurrentThemeName() {
    alignas(16) static unsigned char object[256];
    ev("GetCurrentThemeName", {lab(this)});
    TbDynamicStringTemplate<char>& name = *reinterpret_cast<TbDynamicStringTemplate<char>*>(object);
    name.mData = gThemeText;
    return name;
}

// The weak program definitions of the callees below call this; the fakes below win, so it is never reached.
int32_t UniStringToString(TbDynamicStringTemplate<char>&, const TbDynamicStringTemplate<char16_t>&) { return 0; }

// The weak program definitions below call the constructors; the fakes below win, so these are never reached. They are defined under the
// constructors' symbols (a real definition would drag in the members' constructors).
extern "C" void tpw_fake_online_ctor() __asm__("__ZN15CFSOnlineSystemC1Ev");
extern "C" void tpw_fake_online_ctor() {}
extern "C" void tpw_fake_userinfo_ctor() __asm__("__ZN11CFSUserInfoC1Ev");
extern "C" void tpw_fake_userinfo_ctor() {}
extern "C" void tpw_fake_return_upload() __asm__("__ZN15CFSOnlineSystem20ReturnUploadFilenameEv");
extern "C" void tpw_fake_return_upload() {}
// The strings the weak program definitions touch (constructors, destructors, copy).
extern "C" void tpw_fake_str_c_ctor() __asm__("__ZN26TbDynamicStringTemplateIcEC1Ev");
extern "C" void tpw_fake_str_c_ctor() {}
extern "C" void tpw_fake_str_c_dtor() __asm__("__ZN26TbDynamicStringTemplateIcED1Ev");
extern "C" void tpw_fake_str_c_dtor() {}
extern "C" void tpw_fake_str_w_copy() __asm__("__ZN26TbDynamicStringTemplateIDsEC1ERKS0_");
extern "C" void tpw_fake_str_w_copy() {}
extern "C" void tpw_fake_str_w_dtor() __asm__("__ZN26TbDynamicStringTemplateIDsED1Ev");
extern "C" void tpw_fake_str_w_dtor() {}
extern "C" void tpw_fake_str_c_ctor2() __asm__("__ZN26TbDynamicStringTemplateIcEC2Ev");
extern "C" void tpw_fake_str_c_ctor2() {}
extern "C" void tpw_fake_str_c_dtor2() __asm__("__ZN26TbDynamicStringTemplateIcED2Ev");
extern "C" void tpw_fake_str_c_dtor2() {}
extern "C" void tpw_fake_str_w_copy2() __asm__("__ZN26TbDynamicStringTemplateIDsEC2ERKS0_");
extern "C" void tpw_fake_str_w_copy2() {}
extern "C" void tpw_fake_str_w_dtor2() __asm__("__ZN26TbDynamicStringTemplateIDsED2Ev");
extern "C" void tpw_fake_str_w_dtor2() {}

namespace mainmenu_callee {
CFSOnlineSystem* NewOnlineSystem() {
    void* p = tpw::ArenaAllocZeroed(0x404);
    ev("CFSOnlineSystem::ctor", {newlab(p)});
    return static_cast<CFSOnlineSystem*>(p);
}
CFSUserInfo* NewUserInfo() {
    void* p = tpw::ArenaAllocZeroed(0xd4);
    ev("CFSUserInfo::ctor", {newlab(p)});
    return static_cast<CFSUserInfo*>(p);
}
void ReturnUploadFilename(void* result, CFSOnlineSystem* system) {
    *static_cast<uint32_t*>(result) = static_cast<uint32_t>(ret("UploadSeed")) + 1;
    ev("ReturnUploadFilename", {lab(system)});
}
void UniStringToUploadPath(OnlineUploadFile* out, const void* wide) {
    static char path[64];
    std::snprintf(path, sizeof path, "park%u.dat", static_cast<uint32_t>(ret("UploadSeed")));
    out->mPath = path;
    ev("UniStringToString", {*static_cast<const uint32_t*>(wide)});
}
void WideStringDtor(void* string, int16_t flag) {
 ev("WideStringDtor", {*static_cast<uint32_t*>(string), flag}); }
void ScreenShow(void* screen) { ev("Show", {Tag(screen)}); }
void ScreenHide(void* screen) { ev("Hide", {Tag(screen)}); }
int32_t ScreenIsVisible(void* screen) {    const int32_t tag = Tag(screen);
    ev("IsVisible", {tag});
    return static_cast<int32_t>(ret("IsVisible", tag, 0));
}
void PublishGlobeSlot34(void* screen) { ev("Slot34", {Tag(screen)}); }
}  // namespace mainmenu_callee

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_mm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"sys", &gOnlineSystem},          {"userinfo", &gUserInfo},      {"retflag", &gLobbyReturnFlag},   {"screens", &gLobbyScreens},
        {"keytable", &gKeyTableShortcuts}, {"world", &gGameWorld},        {"upload", &gOnlineUploadFile},   {"gsave", gSaveLoadBytes},
        {"ggame", gGameBytes},            {"themetext", gThemeText},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_mm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets of the screen system's members (HOST_LAYOUT).
int32_t tpw_mm_layout(int64_t* out) {
    int32_t n = 0;
    CLBScreenSystem* base = reinterpret_cast<CLBScreenSystem*>(0x10000);
    const uintptr_t origin = reinterpret_cast<uintptr_t>(base);
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&base->mThemeIslands) - origin);
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&base->mOnlineIslands) - origin);
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&base->mDownloadGlobe) - origin);
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&base->mPublishGlobe) - origin);
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&base->mAdvisor) - origin);
    out[n++] = static_cast<int64_t>(offsetof(OnlineUploadFile, mPath));
    return n;
}

uint32_t tpw_mm_publish() { return static_cast<uint32_t>(MainGameMenuConfirmPublish_OK(nullptr)); }
uint32_t tpw_mm_logout_ok() { return static_cast<uint32_t>(MainLobbyMenuConfirmLogout_OK(nullptr)); }
uint32_t tpw_mm_game_cb(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return static_cast<uint32_t>(MainGameMenuCB(reinterpret_cast<InterfaceWindow*>(window), message, a, b));
}
uint32_t tpw_mm_lobby_cb(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return static_cast<uint32_t>(MainLobbyMenuCB(reinterpret_cast<InterfaceWindow*>(window), message, a, b));
}

}  // extern "C"
