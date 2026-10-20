// Test library for the online park info panel's messages and the park find panel (unit v5550; libtpw_unit_OnlineParkInfoPanelMsg.dylib,
// tests/test_OnlineParkInfoPanelMsg.py), main app:
//   src/OnlineParkInfoPanelMsg.cpp   OnlineParkInfoPanelMsg, GeneralParkDownloadCompleteCallback, VisitParkPostDownloadCallbackFn,
//                                    __sinit_ui_onlineParkFindPanel_cpp, UpdateParkFindPanelProgress, RemoveOnlineParkFindPanel
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals of
// other files the unit reads (gLobby, gLobbyScreens, gOnlineSystem, gUIRootWindow, gUIMainConvertBuffer) are defined here.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "InterfaceWindow.h"
#include "TbDynamicStringTemplate.h"
#include "UI_Base.h"
#include "lobby.h"
#include "platform/msl.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_mainmenu.h"

using namespace calllog;

// A label that tolerates NULL (0, like the PowerPC side's).
static int64_t lab0(const void* p) { return p == nullptr ? 0 : calllog::lab(p); }
#define lab(p) lab0(p)

// ---- globals of other files the unit reads or writes, defined here
CLBLobby* gLobby = nullptr;
CLBScreenSystem* gLobbyScreens = nullptr;
CFSOnlineSystem* gOnlineSystem = nullptr;
InterfaceWindow* gUIRootWindow = nullptr;
char16_t gUIMainConvertBuffer[0x4000];

void UIMain_DestroyWString(void*) {}   // the wide string destructor glue (only its address matters)

namespace {

InterfaceWindow* gParentAnswer = nullptr;   // what GetParent answers
InterfaceWindow* gListAnswer = nullptr;     // what GetWindow answers

// A text's hash over its 16-bit units (the same code in the test module).
int64_t Hash16(const char16_t* text) {
    if (text == nullptr) return 0;
    uint32_t h = 0x811c9dc5u;
    for (size_t i = 0; i < 0x4000 && text[i] != 0; i++) {
        h ^= static_cast<uint16_t>(text[i]);
        h *= 16777619u;
    }
    return h;
}

void FakeClose() { ev("InfoClose", {}); }
void FakePost(int32_t error, const void* context) {
    ev("PostDownload", {error, static_cast<int64_t>(reinterpret_cast<uintptr_t>(context) & 0xffffffffu)});
}

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs (pointers as lab(p))
InterfaceWindow* InterfaceWindow::GetParent() {
    ev("GetParent", {lab(this)});
    return gParentAnswer;
}

void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return gListAnswer;
}

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}

void UIMain_DecideKbCapture() { ev("UIMain_DecideKbCapture", {}); }

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}

void RemoveOnlineParkInfoPanel() { ev("RemoveOnlineParkInfoPanel", {}); }

CFSOnlineSystem* mainmenu_callee::NewOnlineSystem() {
    ev("operator_new", {0x404});
    void* p = tpw::ArenaAllocZeroed(0x404);
    int64_t label = newlab(p);
    ev("CFSOnlineSystem", {label});
    return static_cast<CFSOnlineSystem*>(p);
}

void CFSOnlineSystem::RequestFTPDownload(const CFSOnlinePark& park, void (*callback)(int32_t, const void*), const void* context) {
    ev("RequestFTPDownload", {lab(this), static_cast<int64_t>(reinterpret_cast<const void*>(&park) == reinterpret_cast<const void*>(gOnlineParkInfoParkStorage)),
                              static_cast<int64_t>(callback == &VisitParkPostDownloadCallbackFn),
                              static_cast<int64_t>(reinterpret_cast<uintptr_t>(context) & 0xffffffffu)});
}

void CFSOnlineSystem::UnpublishPark(int32_t park) { ev("UnpublishPark", {lab(this), park}); }

int32_t mainmenu_callee::ScreenIsVisible(void* screen) {
    ev("ScreenIsVisible", {lab(screen)});
    return static_cast<int32_t>(ret("ScreenIsVisible"));
}

void parkinfo_callee::OnlineIslandsSlot30(void* screen) { ev("OnlineIslandsSlot30", {lab(screen)}); }
void parkinfo_callee::DownloadGlobeSlot4c(void* screen) { ev("DownloadGlobeSlot4c", {lab(screen)}); }

void CGameType::SetGameType(GameType type) { ev("SetGameType", {lab(this), static_cast<int32_t>(type)}); }
CGameType::CGameType() { ev("CGameType", {lab(this)}); }

void CLBScreenComponentAdvisor::ClearAll() { ev("ClearAll", {lab(this)}); }

int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {lab(this), position});
    *data = static_cast<int32_t>(ret("GetData"));
    return 0;
}

void InterfaceListControl2::ChangeItem(int32_t position, UI_LCAdd* item, int32_t columns, int32_t update, int32_t unused) {
    // The first column of the item is not set by the original (its stack word): only the second is compared.
    ev("ChangeItem", {lab(this), position, columns, update, unused,
                      static_cast<int64_t>(item[1].mText == gUIMainConvertBuffer), Hash16(item[1].mText)});
}

int32_t swprintf(char16_t* buffer, uint32_t size, const char16_t* format, ...) {
    va_list args;
    va_start(args, format);
    int32_t value = va_arg(args, int32_t);
    va_end(args);
    ev("swprintf", {static_cast<int64_t>(buffer == gUIMainConvertBuffer), size, Hash16(format), value});
    // The fake writes the number in decimal.
    char text[16];
    int n = 0;
    int64_t v = value;
    bool negative = v < 0;
    if (negative) v = -v;
    do {
        text[n++] = static_cast<char>('0' + v % 10);
        v /= 10;
    } while (v != 0);
    if (negative) text[n++] = '-';
    for (int i = 0; i < n; i++) buffer[i] = static_cast<char16_t>(text[n - 1 - i]);
    buffer[n] = 0;
    return n;
}

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("register", {lab(object), static_cast<int64_t>(destructor == reinterpret_cast<void*>(&UIMain_DestroyWString)), lab(node)});
}

// TbDynamicStringTemplate<wchar_t>::TbDynamicStringTemplate(const wchar_t*), the complete object constructor (a fake under its own name:
// the class's vtable is not built in this library).
// (the Apple arm64 ABI constructors return this, and the caller relies on it)
extern "C" void* tpw_opi_string_ctor(void* self, const char16_t* text) __asm__("__ZN23TbDynamicStringTemplateIDsEC1EPKDs");
extern "C" void* tpw_opi_string_ctor(void* self, const char16_t* text) { ev("TbDynamicString", {lab(self), Hash16(text)}); return self; }

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_opi_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gLobby", &gLobby},
        {"gLobbyScreens", &gLobbyScreens},
        {"gOnlineSystem", &gOnlineSystem},
        {"gUIRootWindow", &gUIRootWindow},
        {"find", &gOnlineParkFindPanel},
        {"info", &gOnlineParkInfoPanel},
        {"ctx", &gVisitParkContext},
        {"park", gOnlineParkInfoParkStorage},
        {"strA", gOnlineParkFindStringA},
        {"nodeA", gOnlineParkFindNodeA},
        {"strB", gOnlineParkFindStringB},
        {"nodeB", gOnlineParkFindNodeB},
        {"buf", gUIMainConvertBuffer},
        {"gametype", &TheGameType()},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_opi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// What GetParent (which 0) and GetWindow (which 1) answer, and which of the two function pointer globals are set.
void tpw_opi_set(int32_t which, uint64_t value) {
    if (which == 0) gParentAnswer = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(value));
    if (which == 1) gListAnswer = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(value));
}
void tpw_opi_setfn(int32_t closeSet, int32_t postSet) {
    gOnlineParkInfoClose = closeSet != 0 ? &FakeClose : nullptr;
    gVisitParkPostDownload = postSet != 0 ? &FakePost : nullptr;
}

// The offsets the module writes by (HOST_LAYOUT): the lobby's window and state, the screen system's three screens, the park's id.
int32_t tpw_opi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CLBLobby, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(CLBLobby, mState));
    out[n++] = static_cast<int64_t>(offsetof(CLBScreenSystem, mOnlineIslands));
    out[n++] = static_cast<int64_t>(offsetof(CLBScreenSystem, mDownloadGlobe));
    out[n++] = static_cast<int64_t>(offsetof(CLBScreenSystem, mAdvisor));
    out[n++] = static_cast<int64_t>(offsetof(CFSOnlinePark, mParkID));
    out[n++] = static_cast<int64_t>(sizeof(CFSOnlinePark));
    return n;
}

// ---- the functions
uint32_t tpw_opi_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return static_cast<uint32_t>(OnlineParkInfoPanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b));
}
void tpw_opi_general(int32_t error, uint64_t context) {
    GeneralParkDownloadCompleteCallback(error, reinterpret_cast<const void*>(static_cast<uintptr_t>(context)));
}
void tpw_opi_visit(int32_t error, uint64_t context) {
    VisitParkPostDownloadCallbackFn(error, reinterpret_cast<const void*>(static_cast<uintptr_t>(context)));
}
void tpw_opi_sinit() { __sinit_ui_onlineParkFindPanel_cpp(); }
void tpw_opi_update(int32_t percent) { UpdateParkFindPanelProgress(percent); }
void tpw_opi_removefind() { RemoveOnlineParkFindPanel(); }

}  // extern "C"
