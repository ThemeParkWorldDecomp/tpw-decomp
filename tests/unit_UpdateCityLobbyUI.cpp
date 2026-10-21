// Test library for the city lobby controller (unit v5189; libtpw_unit_UpdateCityLobbyUI.dylib, tests/test_UpdateCityLobbyUI.py), main app:
//   src/game/ui/ui_cityLobbyController.cpp   UpdateCityLobbyUI, CreateCityLobbyController,
//                                                                                    CityLobbyControllerMsg, gCityLobbyWindow, gCityLobbyScript
//
// The callees outside the unit are fakes that record the arguments the real ones read (labels for objects) and answer from the case's queues:
// the window members, the title object and master save constructors, the font table, the swear filter, the advisor, the park list panel and
// the string members. The virtual calls the Mac makes through __ptr_glue (window SetText, the screens' Show and Hide and the online islands'
// own slots) go to hand-built vtables here; every one of them records "vcall" with the Mac's slot offset (SetText records its text).
// The globals of other files (gLobby, gLobbyScreens, gMasterSave, gUIMainConvertBuffer, the string pool) are defined here.
#include <cstdio>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"

#include "AdvisorScoring.h"
#include "FS_MasterSave.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "online.h"
#include "std_basic_filebuf.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "ui_publishParkPanel.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CLBLobby* gLobby;                       // 0x102e4888
CLBScreenSystem* gLobbyScreens;         // 0x102e49b8
CFSGlobalMasterSave* gMasterSave;       // 0x10380a8c
char16_t gUIMainConvertBuffer[0x4000];  // 0x10380c48
// 0x1024edc2: the pool's first string, the number format "%d" (the test checks it against the Mac's bytes).
alignas(2) const char gCityLobbyStringPool[0x60] = {'%', 0, 'd', 0, 0, 0};

namespace {

InterfaceWindow* gW;    // what Build answers
InterfaceWindow* gW9;   // GetWindow(0x49d9)
InterfaceWindow* gWa;   // GetWindow(0x49da)
char gFonts[16][16];    // the font table's entries (GetFontTableEntry answers one by index)

int64_t L_(const void* p) { return static_cast<int64_t>(lab(p)); }

int64_t Hash(const char16_t* t, size_t n) {
    uint32_t h = 0;
    for (size_t i = 0; i < n; i++) h = h * 31u + t[i];
    return static_cast<int64_t>(h);
}

size_t Len16(const char16_t* t) {
    size_t n = 0;
    while (t[n] != 0) n++;
    return n;
}

template <class PM>
size_t SlotIndex(PM pm) {
    static_assert(sizeof(PM) == 16, "arm64 member pointer");
    uint64_t raw[2];
    std::memcpy(raw, &pm, 16);
    return static_cast<size_t>(raw[0] / 8);
}

// ---- the hand-built vtables (slot functions take `this` first, like the members they stand for)
int32_t SlotSetText(void* self, char16_t* text) {
    int64_t kind = text == nullptr ? 0 : (text == gUIMainConvertBuffer ? 1 : 2);
    size_t n = text == nullptr ? 0 : Len16(text);
    ev("SetText", {L_(self), kind, static_cast<int64_t>(n), text == nullptr ? 0 : Hash(text, n)});
    return 0;
}
void SlotShow(void* self) { ev("vcall", {L_(self), 0x24}); }
void SlotHide(void* self) { ev("vcall", {L_(self), 0x28}); }

void* gWindowVt[64];
void* gScreenVt[64];
bool gVtReady = false;

void MakeVtables() {
    if (gVtReady) return;
    gVtReady = true;
    gWindowVt[SlotIndex(&InterfaceWindow::SetText)] = reinterpret_cast<void*>(&SlotSetText);
    gScreenVt[SlotIndex(&CLBScreenComponent::Show)] = reinterpret_cast<void*>(&SlotShow);
    gScreenVt[SlotIndex(&CLBScreenComponent::Hide)] = reinterpret_cast<void*>(&SlotHide);
}

void* MakeObject(void** vtable) {
    MakeVtables();
    void* block = tpw::ArenaAllocZeroed(0x400);
    *reinterpret_cast<void***>(block) = vtable;
    return block;
}

}  // namespace

// ---- constructors faked by symbol (the classes' own members and vtables are not in this library)
extern "C" void FakeTitleObjectCtor(void* self, void* data) __asm__("__ZN24InterfaceTitleTextObjectC1EPv");
extern "C" void FakeTitleObjectCtor(void* self, void* data) {
    ev("TitleCtor", {static_cast<int64_t>(newlab(self)), static_cast<int64_t>(reinterpret_cast<uintptr_t>(data))});
}
extern "C" void FakeMasterSaveCtor(void* self) __asm__("__ZN19CFSGlobalMasterSaveC1Ev");
extern "C" void FakeMasterSaveCtor(void* self) { ev("MasterCtor", {static_cast<int64_t>(newlab(self))}); }

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {L_(this), id});
    return id == 0x49d9 ? gW9 : (id == 0x49da ? gWa : nullptr);
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {L_(this), on});
    return 0;
}
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {L_(this), description == gCityLobbyScript ? 1 : 0, callback == &CityLobbyControllerMsg ? 1 : 0});
    return ret("Build") != 0 ? gW : nullptr;
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("Default", {L_(this), message, a, b});
    return static_cast<int32_t>(ret("Default"));
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {L_(this), L_(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) {
    ev("SetFont", {L_(this), static_cast<int64_t>((reinterpret_cast<char*>(font) - &gFonts[0][0]) / 16)});
}
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {L_(this), horizontal, vertical}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {L_(this), red, green, blue, alpha});
}
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(gFonts[index & 15]);
}
void UI_TITLE_COLOUR(InterfaceWindow* window) { ev("UI_TITLE_COLOUR", {L_(window)}); }
// Translated in another unit (v51067); only its address is passed on here.
void ListParkOnlineParkInfoPanelCancelCallback() {}
void CreateOnlineParkListPanel(void (*onClose)()) {
    ev("CreateOnlineParkListPanel", {onClose == &ListParkOnlineParkInfoPanelCancelCallback ? 1 : 0});
}
void CLBScreenComponentAdvisor::Add(AdvisorResponse response, int32_t clearFirst) {
    ev("Add", {L_(this), response.mId, clearFirst});
}
void CLBScreenComponentIslands::GotoNextIsland() { ev("vcall", {L_(this), 0x40}); }
// v5255 declared the online screen's own GotoNextIsland (slot 0x40), which hides the base's: the controller's call binds to it
void CLBScreenComponentOnlineIslands::GotoNextIsland() { ev("vcall", {L_(this), 0x40}); }
void CLBScreenComponentIslands::GotoPreviousIsland() { ev("vcall", {L_(this), 0x44}); }
// v51106 declared the online screen's own GotoPreviousIsland (slot 0x44), which hides the base's, as v5255 did for GotoNextIsland
void CLBScreenComponentOnlineIslands::GotoPreviousIsland() { ev("vcall", {L_(this), 0x44}); }
void OnlineIslandsSlot34(CLBScreenComponentOnlineIslands* screen) { ev("vcall", {L_(screen), 0x34}); }
void OnlineIslandsSlot48(CLBScreenComponentOnlineIslands* screen) { ev("vcall", {L_(screen), 0x48}); }
bool CFSGlobalMasterSave::IsSwearFilterOn() const {
    ev("IsSwearFilterOn", {L_(this)});
    return (ret("IsSwearFilterOn") & 0xff) != 0;
}
bool SwearFilter::Filter(std::u16string& text, bool replace) {
    ev("Filter", {L_(this), static_cast<int64_t>(text.size()), Hash(text.data(), text.size()), replace ? 1 : 0});
    bool answer = ret("Filter") != 0;
    if (answer) {   // the fake's own edit: the first character becomes '*', the last '#'
        if (!text.empty()) text[0] = u'*';
        if (text.size() > 1) text[text.size() - 1] = u'#';
    }
    return answer;
}

// The C library glue the module's PowerPC stubs stand in for: swprintf (only "%d" and plain text, the formats this unit passes) and wcslen.
int swprintf(char16_t* buffer, uint32_t size, const char16_t* format, ...) {
    va_list args;
    va_start(args, format);
    int value = va_arg(args, int);
    va_end(args);
    size_t formatLength = Len16(format);
    ev("swprintf", {static_cast<int64_t>(size), Hash(format, formatLength), value});
    std::u16string out;
    for (size_t i = 0; i < formatLength; i++) {
        if (format[i] == u'%' && i + 1 < formatLength && format[i + 1] == u'd') {
            for (char c : std::to_string(value)) out.push_back(static_cast<char16_t>(c));
            i++;
        } else {
            out.push_back(format[i]);
        }
    }
    if (size > 0) {
        if (out.size() > size - 1) out.resize(size - 1);
        std::memcpy(buffer, out.data(), out.size() * 2);
        buffer[out.size()] = 0;
    }
    return static_cast<int>(out.size());
}
size_t wcslen(const char16_t* text) { return Len16(text); }

// The string members (a Mac std::wstring is one pointer to a representation: len +0, capacity +4, count +8, the data pointer +0xc).
namespace online_callee {
void WStringCtor(void* self) {
    MslStrRep* rep = static_cast<MslStrRep*>(tpw::ArenaAllocZeroed(0x10));
    char16_t* data = static_cast<char16_t*>(tpw::ArenaAllocZeroed(0x10));
    rep->len = 0;
    rep->cap = 7;
    rep->refs = 1;
    rep->data = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(data));
    *static_cast<MslStrRep**>(self) = rep;
    ev("WStringCtor", {1});
}
void WStringDtor(void* self, int16_t flag) {
    MslStrRep* rep = *static_cast<MslStrRep**>(self);
    ev("WStringDtor", {flag, static_cast<int64_t>(rep->len)});
}
void WStringAssign(void* self, const char16_t* text, uint32_t n) {
    ev("WStringAssign", {static_cast<int64_t>(n), Hash(text, n)});
    MslStrRep* rep = *static_cast<MslStrRep**>(self);
    char16_t* data = static_cast<char16_t*>(tpw::ArenaAllocZeroed((n + 1) * 2));
    std::memcpy(data, text, n * 2);
    rep->len = n;
    rep->cap = n;
    rep->data = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(data));
}
}  // namespace online_callee

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_cl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"buf", gUIMainConvertBuffer},
        {"pool", gCityLobbyStringPool},
        {"fonts", gFonts},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The offsets and sizes the module relies on: none (every block is built through members here).
int32_t tpw_cl_layout(int64_t* out) {
    (void)out;
    return 0;
}

// ---- the world's objects (zeroed arena blocks with a hand-built vtable where the Mac calls through one)
uint64_t tpw_cl_mk_win() { return reinterpret_cast<uintptr_t>(MakeObject(gWindowVt)); }
uint64_t tpw_cl_mk_screen() { return reinterpret_cast<uintptr_t>(MakeObject(gScreenVt)); }
uint64_t tpw_cl_mk_plain() { return reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(0x400)); }
uint64_t tpw_cl_mk_lobby(uint64_t window) {
    CLBLobby* lobby = static_cast<CLBLobby*>(tpw::ArenaAllocZeroed(sizeof(CLBLobby) + 0x40));
    lobby->mWindow = reinterpret_cast<InterfaceWindow*>(window);
    return reinterpret_cast<uintptr_t>(lobby);
}
uint64_t tpw_cl_mk_screens(uint64_t theme, uint64_t online, uint64_t globe, uint64_t advisor) {
    CLBScreenSystem* screens = static_cast<CLBScreenSystem*>(tpw::ArenaAllocZeroed(sizeof(CLBScreenSystem) + 0x40));
    screens->mThemeIslands = reinterpret_cast<CLBScreenComponentThemeIslands*>(theme);
    screens->mOnlineIslands = reinterpret_cast<CLBScreenComponentOnlineIslands*>(online);
    screens->mDownloadGlobe = reinterpret_cast<CLBScreenComponentDownloadGlobe*>(globe);
    screens->mAdvisor = reinterpret_cast<CLBScreenComponentAdvisor*>(advisor);
    return reinterpret_cast<uintptr_t>(screens);
}
uint64_t tpw_cl_mk_park(const uint16_t* text, int32_t n) {
    CFSOnlinePark* park = static_cast<CFSOnlinePark*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlinePark) + 0x40));
    char16_t* data = static_cast<char16_t*>(tpw::ArenaAllocZeroed((static_cast<size_t>(n) + 1) * 2));
    for (int32_t i = 0; i < n; i++) data[i] = text[i];
    park->mText0C.mData = data;
    return reinterpret_cast<uintptr_t>(park);
}
uint64_t tpw_cl_mk_island(int32_t order, uint64_t park) {
    CLBIsland* island = static_cast<CLBIsland*>(tpw::ArenaAllocZeroed(sizeof(CLBIsland) + 0x40));
    island->mOrder = order;
    island->mPark = reinterpret_cast<CFSOnlinePark*>(park);
    return reinterpret_cast<uintptr_t>(island);
}
void tpw_cl_set_ctx(uint64_t w, uint64_t w9, uint64_t wa) {
    gW = reinterpret_cast<InterfaceWindow*>(w);
    gW9 = reinterpret_cast<InterfaceWindow*>(w9);
    gWa = reinterpret_cast<InterfaceWindow*>(wa);
}
void tpw_cl_set_globals(uint64_t window, uint64_t master, uint64_t lobby, uint64_t screens) {
    gCityLobbyWindow = reinterpret_cast<InterfaceWindow*>(window);
    gMasterSave = reinterpret_cast<CFSGlobalMasterSave*>(master);
    gLobby = reinterpret_cast<CLBLobby*>(lobby);
    gLobbyScreens = reinterpret_cast<CLBScreenSystem*>(screens);
}
uint64_t tpw_cl_get_window() { return reinterpret_cast<uintptr_t>(gCityLobbyWindow); }
uint64_t tpw_cl_get_master() { return reinterpret_cast<uintptr_t>(gMasterSave); }
int32_t tpw_cl_labp(uint64_t p) { return static_cast<int32_t>(lab(reinterpret_cast<const void*>(p))); }

// ---- the functions, one wrapper each with C types only
void tpw_cl_update(uint64_t island) { UpdateCityLobbyUI(reinterpret_cast<CLBIsland*>(island)); }
void tpw_cl_create() { CreateCityLobbyController(); }
int32_t tpw_cl_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return CityLobbyControllerMsg(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}

}  // extern "C"
