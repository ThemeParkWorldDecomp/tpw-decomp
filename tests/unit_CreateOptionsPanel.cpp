// Test library for the options panel's CreateOptionsPanel (unit v5634; libtpw_unit_CreateOptionsPanel.dylib,
// tests/test_CreateOptionsPanel.py), main app:
//   src/game/ui/ui_optionsPanel.cpp   CreateOptionsPanel 0x101a9fb8 and the panel's globals (0x10394a08, 0x10394a0c, 0x10394a10, 0x102ab668)
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queue: the
// settings' StoreSettings and four volume getters, CGame::Pause, UI_PauseTimerSystem, ADV_Kill, the map's render options, the texture
// cache's Flush, UI_SendMessage, the window members (Build, GetWindow, setZ, SetClientDrawObject, SetFontColour, SetFont,
// SetClientDrawText), the scroll bar's SetMinMax, the button's SetState, UIMain_GetString (which writes its string to
// gUIMainConvertBuffer as the real one does), UIMain_SetTitleText, GetFontTableEntry, the allocation and the text sprite's constructor
// (lender_callee), the panel's DoScroll and DoButton, SetOnOffText, and the three virtual members the function calls (Enable, SetValue,
// SetText) through a fake vtable the test's windows carry. wcsncpy and wcscat are platform/msl.cpp's. The globals of other files the
// unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "Advisor.h"
#include "FillLenderList.h"
#include "Game.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "MapWho.h"
#include "TPSettings.h"
#include "UI_Base.h"
#include "calllog.h"
#include "lobby.h"
#include "platform/arena.h"
#include "tb_ddraw.h"
#include "tb_texturecache.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_mainmenu.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes. gGame and gPlayerSettings are raw storage under the objects' symbols (the
// classes' constructors live in files this library does not compile); the test writes their fields.
alignas(16) unsigned char gCopGameStorage[sizeof(CGame)] __asm__("_gGame");
alignas(16) unsigned char gCopSettingsStorage[sizeof(CPlayerSettings)] __asm__("_gPlayerSettings");
CLBLobby* gLobby;
InterfaceWindow* gGameWindow;
InterfaceWindow* gUIRootWindow;
InterfaceWindow* gSetOnOffRoot;
char16_t gUIMainConvertBuffer[0x4000];
CMapWho* gei_MapWho;
CTextureCache* gei_TCache;
CDirectDraw* gei_DDraw;
int16_t gOptionsPanelScript[8] = {1};

namespace {

CGame* Game() { return reinterpret_cast<CGame*>(gCopGameStorage); }
CPlayerSettings* Settings() { return reinterpret_cast<CPlayerSettings*>(gCopSettingsStorage); }

// The objects the fakes answer, by the queue's index (the test fills it; a negative index is NULL).
uint64_t gObjects[16];
InterfaceWindow* Obj(int64_t index) {
    return index < 0 ? nullptr : reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(gObjects[index]));
}

// The case's string stem: UIMain_GetString(id) is the stem followed by id in hex.
char gTag[80];

// The three virtual members, recorded as the PowerPC stub of __ptr_glue records them.
int16_t FakeEnable(InterfaceWindow* window, int16_t on) {
    ev("Enable", {lab(window), on});
    return 0;
}
int32_t FakeSetValue(InterfaceWindow* window, int32_t value) {
    ev("SetValue", {lab(window), value});
    return 0;
}
// A UTF-16 string as "text" events of eight characters each, the last one holding the terminator (zeros after it).
void RecText(const char16_t* text) {
    for (size_t at = 0;; at += 8) {
        int64_t c[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        bool end = false;
        for (size_t k = 0; k < 8 && !end; ++k) {
            c[k] = text[at + k];
            if (c[k] == 0) end = true;
        }
        ev("text", {c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]});
        if (end) return;
    }
}

int32_t FakeSetText(InterfaceWindow* window, char16_t* text) {
    ev("SetText", {lab(window)});
    RecText(text);
    return 0;
}
void FakeTrap() { std::abort(); }

// The vtable offset of a virtual member function (both encodings of the Itanium member pointer: ARM's and the generic one).
template <typename M>
size_t SlotOf(M member) {
    static_assert(sizeof(M) == 2 * sizeof(uintptr_t), "member pointer");
    uintptr_t words[2];
    std::memcpy(words, &member, sizeof words);
    uintptr_t offset = (words[1] & 1) ? words[0] : words[0] - 1;
    return offset / sizeof(void*);
}

void* gFakeVtable[64];
void InitFakeVtable() {
    for (void*& slot : gFakeVtable) slot = reinterpret_cast<void*>(&FakeTrap);
    gFakeVtable[SlotOf(&InterfaceWindow::Enable)] = reinterpret_cast<void*>(&FakeEnable);
    gFakeVtable[SlotOf(&InterfaceWindow::SetValue)] = reinterpret_cast<void*>(&FakeSetValue);
    gFakeVtable[SlotOf(&InterfaceWindow::SetText)] = reinterpret_cast<void*>(&FakeSetText);
}

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub (pointers as lab(p))
void CPlayerSettings::StoreSettings() { ev("StoreSettings", {lab(this)}); }
int32_t CPlayerSettings::GetSFXVolume() const {
    ev("GetSFXVolume", {lab(this)});
    return static_cast<int32_t>(ret("GetSFXVolume"));
}
int32_t CPlayerSettings::GetMusicVolume() const {
    ev("GetMusicVolume", {lab(this)});
    return static_cast<int32_t>(ret("GetMusicVolume"));
}
int32_t CPlayerSettings::GetSpeechVolume() const {
    ev("GetSpeechVolume", {lab(this)});
    return static_cast<int32_t>(ret("GetSpeechVolume"));
}
int32_t CPlayerSettings::GetMovieVolume() const {
    ev("GetMovieVolume", {lab(this)});
    return static_cast<int32_t>(ret("GetMovieVolume"));
}
void CGame::Pause(ePauseFlag flag, bool allSounds) {
    ev("Pause", {lab(this), static_cast<int32_t>(flag), allSounds ? 1 : 0});
}
void UI_PauseTimerSystem() { ev("UI_PauseTimerSystem", {}); }
void ADV_Kill(int32_t mode) { ev("ADV_Kill", {mode}); }
int32_t CMapWho::GetRenderOptions() {
    ev("GetRenderOptions", {lab(this)});
    return static_cast<int32_t>(ret("GetRenderOptions"));
}
void CMapWho::SetRenderOptions(int32_t options) { ev("SetRenderOptions", {lab(this), options}); }
void CTextureCache::Flush(int32_t mode) { ev("Flush", {lab(this), mode}); }
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("UI_SendMessage"));
}
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), lab(description), lab(reinterpret_cast<void*>(callback))});
    return Obj(ret("Build"));
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    static const char kHex[] = "0123456789abcdef";
    char digits[16];
    int32_t n = 0;
    uint32_t v = static_cast<uint32_t>(id);
    do {
        digits[n++] = kHex[v & 15];
        v >>= 4;
    } while (v != 0);
    int32_t at = 0;
    for (const char* p = gTag; *p != 0; ++p) gUIMainConvertBuffer[at++] = static_cast<char16_t>(static_cast<uint8_t>(*p));
    while (n > 0) gUIMainConvertBuffer[at++] = static_cast<char16_t>(digits[--n]);
    gUIMainConvertBuffer[at] = 0;
    return gUIMainConvertBuffer;
}
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) {
    ev("SetTitleText", {lab(window), id});
    RecText(text);
}
// GetWindow's answers in call order (the test writes them; more than a queue holds), and the next one's index.
int64_t gCopGetWindowAnswers[256];
int32_t gCopGetWindowNext;
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return Obj(gCopGetWindowAnswers[gCopGetWindowNext++ & 255]);
}
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }
void* lender_callee::OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    if (ret("new") == 0) return nullptr;
    return tpw::ArenaAllocZeroed(size);
}
void lender_callee::SpriteCtor(void* memory) { ev("SpriteCtor", {newlab(memory), lab(nullptr)}); }
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(Obj(ret("Font")));
}
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), lab(text)}); }
void InterfaceScrollBar::SetMinMax(int32_t min, int32_t max) { ev("SetMinMax", {lab(this), min, max}); }
int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {lab(this), state, redraw});
    return 0;
}
void options_panel::DoScroll(int32_t id, int32_t value) { ev("DoScroll", {id, value}); }
void options_panel::DoButton(int32_t id, int32_t on) { ev("DoButton", {id, on}); }
void SetOnOffText(int32_t window, int32_t text, bool first, bool second, bool button, int32_t buttonId) {
    ev("SetOnOffText", {window, text, first ? 1 : 0, second ? 1 : 0, button ? 1 : 0, buttonId});
}
// Never called by the function (its address is Build's argument).
int32_t OptionsPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

extern "C" {

// The host address of a global (or a field the test writes) by the module's name for it.
uint64_t tpw_cop_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gGame", gCopGameStorage},
        {"game_f3c", &Game()->mF3C},
        {"settings", gCopSettingsStorage},
        {"gLobby", &gLobby},
        {"gGameWindow", &gGameWindow},
        {"gUIRootWindow", &gUIRootWindow},
        {"gSetOnOffRoot", &gSetOnOffRoot},
        {"convbuf", gUIMainConvertBuffer},
        {"gei_MapWho", &gei_MapWho},
        {"gei_TCache", &gei_TCache},
        {"gei_DDraw", &gei_DDraw},
        {"running", &gOptionsPanelGameRunning},
        {"f0c", &gOptionsPanelF0C},
        {"render", &gOptionsPanelRenderOptions},
        {"font_index", &gOptionsPanelFontIndex},
        {"script", gOptionsPanelScript},
        {"callback", reinterpret_cast<const void*>(&OptionsPanelMsg)},
        {"objects", gObjects},
        {"gw_answers", gCopGetWindowAnswers},
        {"gw_next", &gCopGetWindowNext},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_cop_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A window with the fake vtable and the depth z.
uint64_t tpw_cop_window(int32_t z) {
    if (gFakeVtable[0] == nullptr) InitFakeVtable();
    void* memory = tpw::ArenaAllocZeroed(sizeof(InterfaceWindow));
    *reinterpret_cast<void**>(memory) = gFakeVtable;
    reinterpret_cast<InterfaceWindow*>(memory)->mZ = static_cast<int16_t>(z);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(memory));
}

// A lobby whose window is `window`.
uint64_t tpw_cop_lobby(uint64_t window) {
    void* memory = tpw::ArenaAllocZeroed(sizeof(CLBLobby));
    reinterpret_cast<CLBLobby*>(memory)->mWindow = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(memory));
}

// A display with `count` devices.
uint64_t tpw_cop_ddraw(int32_t count) {
    void* memory = tpw::ArenaAllocZeroed(sizeof(CDirectDraw));
    reinterpret_cast<CDirectDraw*>(memory)->mDeviceCount = count;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(memory));
}

void tpw_cop_set_tag(const char* tag) {
    std::strncpy(gTag, tag, sizeof gTag - 1);
    gTag[sizeof gTag - 1] = 0;
}

// The offsets the module writes by Mac offset (the settings block is the Mac's layout on the host too).
int32_t tpw_cop_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings, mCurrent));
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings::Settings, mResolution));
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings::Settings, mDetail));
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings::Settings, mVideoCard));
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings::Settings, mSfx));
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings::Settings, mMusic));
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings::Settings, mSpeech));
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings::Settings, mMovie));
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings::Settings, mSoundQuality));
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings::Settings, mAdvisorOn));
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings::Settings, mIsometricOn));
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings::Volume, mVolume));
    out[n++] = static_cast<int64_t>(sizeof(int32_t));
    return n;
}

void tpw_cop_run() { CreateOptionsPanel(); }

}  // extern "C"
