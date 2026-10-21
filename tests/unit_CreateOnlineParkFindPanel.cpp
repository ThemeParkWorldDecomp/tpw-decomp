// Test library for the online park find panel (unit v5661; libtpw_unit_CreateOnlineParkFindPanel.dylib, tests/test_CreateOnlineParkFindPanel.py), main app:
//   src/game/ui/ui_onlineParkFindPanel.cpp   CreateOnlineParkFindPanel, ShowFindListSearching, gOnlineParkFindScript, gOnlineParkFindWide
//
// The callees outside the unit are fakes that record what the real ones read; objects are fixed fake addresses (never dereferenced here),
// the globals of other files are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "FS_Online.h"
#include "Advisor.h"
#include "MainWindowCallback.h"
#include "TbITable.h"
#include "Text.h"
#include "lobby.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CLBLobby* gLobby;
CLBScreenSystem* gLobbyScreens;
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRenderParams::TbFontRenderParams() {}   // gText holds one; the real constructor is in a file this library does not compile
CText gText;
CFSOnlineSystem* gOnlineSystem;
InterfaceWindow* gOnlineParkFindPanel;
char16_t gUIMainConvertBuffer[0x4000];
char16_t gFindPanelPlayerName[64];
void (*gFindPanelCloseCallback)();

namespace {

constexpr uint64_t kSprite = 0x7A000500, kSys = 0x7A000400, kList = 0x7A000700, kCol = 0x7A000800, kFont = 0x7A000900,
                   kDraw = 0x7A000A00, kName = 0x7A000B00, kStr = 0x7B000000;
uint64_t gEdit;
alignas(16) char gLobbyMem[sizeof(CLBLobby)];
alignas(16) char gScreensMem[sizeof(CLBScreenSystem)];

int64_t I(int64_t x) { return x; }
void* P(uint64_t a) { return reinterpret_cast<void*>(static_cast<uintptr_t>(a)); }
int64_t L(const void* p) { return lab(const_cast<void*>(p)); }
int64_t RawWord(const UI_LCAdd& w) { return static_cast<int64_t>(w.mRaw); }
int64_t LabWord(const UI_LCAdd& w) { return lab(reinterpret_cast<void*>(static_cast<uintptr_t>(w.mRaw))); }

}  // namespace

// ---- the fakes
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {L(this), L(description), L(reinterpret_cast<const void*>(callback))});
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(0x7A000600));
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {L(this), I(id)});
    uint64_t r = 0;
    if (id == 0xe05a) r = gEdit;
    else if (id == 0xe05b) r = kList;
    else if (id >= 0x10 && id <= 0x15) r = kCol + static_cast<uint64_t>(id - 0x10) * 0x10;
    return static_cast<InterfaceWindow*>(P(r));
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {L(this), L(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {L(this), L(font)}); }
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {L(this), I(on)}); }

int32_t InterfaceEditBox::SetStaticTextArea(char16_t* text, int32_t capacity) {
    ev("SetStaticTextArea", {L(this), L(text), I(capacity)});
    return 0;
}
void InterfaceEditBox::SetCursorDrawObject(InterfaceDrawObject* object) { ev("SetCursorDrawObject", {L(this), L(object)}); }

void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    ev("SetColumnFormatsAll", {L(this), L(reinterpret_cast<const void*>(makeRow)), L(reinterpret_cast<const void*>(attach))});
}
void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) {
    ev("SetColumnType", {L(this), I(column), I(static_cast<int32_t>(type))});
}
void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) { ev("SetSelectionObject", {L(this), L(object)}); }
int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    ev("AddItem", {L(this), RawWord(item[0]), LabWord(item[1]), LabWord(item[2]), RawWord(item[3]), RawWord(item[4]), RawWord(item[5]),
                   I(data), I(position), I(update)});
    return 0;
}
void InterfaceListControl2::RemoveAllItems() { ev("RemoveAllItems", {L(this)}); }

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {I(static_cast<uint8_t>(name[0])), I(static_cast<uint8_t>(name[1])), I(static_cast<uint8_t>(name[2])),
                          I(static_cast<uint8_t>(name[3])), I(static_cast<uint8_t>(name[4])), I(static_cast<uint8_t>(name[5])),
                          I(static_cast<uint8_t>(name[6])), I(static_cast<uint8_t>(name[7]))});
    return static_cast<InterfaceDrawObject*>(P(kDraw));
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {I(index)});
    return static_cast<InterfaceFont*>(P(kFont));
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {I(id)});
    return static_cast<const char16_t*>(P(kStr + static_cast<uint64_t>(id) * 4));
}

void MAKECOLUMNBUTTON(InterfaceWindow* window, char16_t* text, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    ev("MAKECOLUMNBUTTON", {L(window), I(reinterpret_cast<uintptr_t>(text)), I(a), I(b), I(c), I(d), I(e)});
}

void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) { ev("SetTitleText", {L(window), I(id), L(text)}); }

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    ev("FormatString", {L(this), L(out), I(size), I(id)});
    return 0;
}

void CLBScreenComponentAdvisor::Add(AdvisorResponse response, int32_t clearFirst) {
    ev("AdvisorAdd", {L(this), I(response.mId), I(clearFirst)});
}

int32_t OnlineParkFindPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
InterfaceWindow* OnlineParkFindLCFormat() { return nullptr; }
void OnlineParkFindLCPostFormat(InterfaceWindow*) {}

namespace findpanel_callee {
InterfaceDrawObject* NewCursorSprite() {
    ev("new", {I(0x19c)});
    ev("SpriteCtor", {L(P(kSprite)), I(0)});
    return static_cast<InterfaceDrawObject*>(P(kSprite));
}
CFSOnlineSystem* NewOnlineSystem() {
    ev("new", {I(0x404)});
    ev("SystemCtor", {L(P(kSys))});
    return static_cast<CFSOnlineSystem*>(P(kSys));
}
const char16_t* LoginCustomerName(CFSOnlineSystem* system) {
    ev("LoginName", {L(system)});
    return static_cast<const char16_t*>(P(kName));
}
void WindowSetText(InterfaceWindow* window, char16_t* text) { ev("SetText", {L(window), L(text)}); }
}  // namespace findpanel_callee

extern "C" {

uint64_t tpw_fp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"playername", gFindPanelPlayerName},
        {"script", gOnlineParkFindScript},
        {"msg", reinterpret_cast<const void*>(&OnlineParkFindPanelMsg)},
        {"lcformat", reinterpret_cast<const void*>(&OnlineParkFindLCFormat)},
        {"lcpost", reinterpret_cast<const void*>(&OnlineParkFindLCPostFormat)},
        {"buf", gUIMainConvertBuffer},
        {"pool", gOnlineParkFindWide},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_fp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(LCWord));
    return n;
}

// The world: the lobby window, the advisor, the text table, the online system, the find panel, the close callback, the edit box.
void tpw_fp_world(uint64_t lobbyWindow, uint64_t advisor, uint64_t table, uint64_t sys, uint64_t panel, uint64_t cb, uint64_t edit) {
    std::memset(gLobbyMem, 0, sizeof gLobbyMem);
    std::memset(gScreensMem, 0, sizeof gScreensMem);
    gLobby = reinterpret_cast<CLBLobby*>(gLobbyMem);
    gLobby->mWindow = static_cast<InterfaceWindow*>(P(lobbyWindow));
    gLobbyScreens = reinterpret_cast<CLBScreenSystem*>(gScreensMem);
    gLobbyScreens->mAdvisor = static_cast<CLBScreenComponentAdvisor*>(P(advisor));
    gText.mResourcePointers[6] = P(table);
    gOnlineSystem = static_cast<CFSOnlineSystem*>(P(sys));
    gOnlineParkFindPanel = static_cast<InterfaceWindow*>(P(panel));
    gFindPanelCloseCallback = reinterpret_cast<void (*)()>(static_cast<uintptr_t>(cb));
    gEdit = edit;
}

void tpw_fp_state(uint64_t* out) {
    out[0] = reinterpret_cast<uintptr_t>(gOnlineSystem);
    out[1] = reinterpret_cast<uintptr_t>(gOnlineParkFindPanel);
    out[2] = reinterpret_cast<uintptr_t>(gFindPanelCloseCallback);
}

void tpw_fp_create(uint64_t cb) { CreateOnlineParkFindPanel(reinterpret_cast<void (*)()>(static_cast<uintptr_t>(cb))); }
void tpw_fp_show() { ShowFindListSearching(); }

}  // extern "C"
