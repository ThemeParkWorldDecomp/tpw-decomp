// Test library for the publish park panel and the online park list panel (unit v5064; libtpw_unit_PublishPanelMsg.dylib,
// tests/test_PublishPanelMsg.py), main app:
//   src/game/ui/ui_onlineParkListPanel.cpp, src/game/ui/ui_publishParkPanel.cpp   PublishPanelMsg, CreatePublishParkPanel, CreateOnlineParkListPanel and the panels' globals
//
// Every callee outside the unit is a fake that records what the real one reads (the same event name and arguments as the module's PowerPC
// stub). The world's objects are blocks the module makes through the tpw_pp_* helpers: a window is a block with a fake vtable (the one
// virtual the code calls is SetText); the table `gObjs` (index = the log label) is what the fakes answer from. The globals of other
// files the unit reads are defined here, zero at start like the Mac's bss. The constructors of classes whose vtables would pull in
// their whole hierarchy are given as plain functions under the constructors' symbol names.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_publishParkPanel.h"

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "lobby.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CFSOnlineSystem* gOnlineSystem = nullptr;
CLBLobby* gLobby = nullptr;
CLBScreenSystem* gLobbyScreens = nullptr;
char16_t gUIMainConvertBuffer[0x4000];
// gGame and gText are big classes with constructors of their own: raw storage under their symbols.
alignas(16) char gGameStorage[sizeof(CGame)] __asm("_gGame");
alignas(16) char gTextStorage[sizeof(CText)] __asm("_gText");

namespace {

void* gObjs[128];   // the world's objects by label

void* Obj(int label) { return gObjs[label]; }

int WindowLabel(int32_t id) {
    switch (id) {
        case 0x11e3: return 3;
        case 0x11e4: return 4;
        case 0x14beb: return 5;
        case 0x10: return 6;
        case 0x11: return 7;
        case 0x12: return 8;
        case 0x13: return 9;
        case 0x14: return 10;
        default: return 0;
    }
}

int StringLabel(int32_t id) {
    switch (id) {
        case 0xdc: return 30;
        case 0xda: return 31;
        case 0x126: return 32;
        case 0x127: return 33;
        case 0x128: return 34;
        case 0x129: return 35;
        case 0x12a: return 36;
        default: return 0;
    }
}

const void* Fn(void* f) { return f; }

void FakeClose0() {}
void FakeClose1() {}
void* gFns[128];   // function addresses by label

// ---- the fake window: a vtable whose SetText slot records
int32_t FakeSetText(InterfaceWindow* self, char16_t* text) {
    ev("SetText", {lab(self), lab(text)});
    return 1;
}
void FakeUnexpected() { __builtin_trap(); }
void* gVtable[64];
bool gVtableMade = false;

size_t SetTextSlot() {
    int32_t (InterfaceWindow::*member)(char16_t*) = &InterfaceWindow::SetText;
    uintptr_t raw[2];
    std::memcpy(raw, &member, sizeof(raw));
    return raw[0] / sizeof(void*);   // the arm64 ABI keeps the virtual flag in the adjustment, so ptr is the byte offset
}

void MakeVtable() {
    if (gVtableMade) return;
    for (void*& entry : gVtable) entry = reinterpret_cast<void*>(&FakeUnexpected);
    gVtable[SetTextSlot()] = reinterpret_cast<void*>(&FakeSetText);
    gVtableMade = true;
}

void* Block(int label, size_t size) {
    void* p = tpw::ArenaAllocZeroed(size);
    gObjs[label] = p;
    return p;
}

}  // namespace

// ---- the constructors under their symbol names (no vtables needed here)
void FakeCursorSpriteCtor(void* self, void* data) __asm("__ZN34InterfaceCursorPosTextObjectSpriteC1EPv");
void FakeCursorSpriteCtor(void* self, void* data) { ev("CursorSprite", {newlab(self), reinterpret_cast<intptr_t>(data)}); }

void FakeStringCtor(TbDynamicStringTemplate<char16_t>* self) __asm("__ZN23TbDynamicStringTemplateIDsEC1Ev");
void FakeStringCtor(TbDynamicStringTemplate<char16_t>* self) {
    ev("TbStrCtor", {newlab(self)});
    self->mData = static_cast<char16_t*>(Obj(42));
}
void FakeStringCtor2(TbDynamicStringTemplate<char16_t>* self) __asm("__ZN23TbDynamicStringTemplateIDsEC2Ev");
void FakeStringCtor2(TbDynamicStringTemplate<char16_t>* self) { FakeStringCtor(self); }
void FakeStringDtor(TbDynamicStringTemplate<char16_t>* self) __asm("__ZN23TbDynamicStringTemplateIDsED1Ev");
void FakeStringDtor(TbDynamicStringTemplate<char16_t>* self) { ev("TbStrDtor", {lab(self)}); }

// ---- the fakes
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {lab(window), message, a, b});
    return 0;
}
void UIMain_DisableAllKeyTables() { ev("UIMain_DisableAllKeyTables", {}); }
void UIMain_EnableAllKeyTables() { ev("UIMain_EnableAllKeyTables", {}); }
void UIMain_DecideKbCapture() { ev("UIMain_DecideKbCapture", {}); }
bool UIMain_CheckPanel() {
    ev("UIMain_CheckPanel", {});
    return false;
}
int32_t DefaultDialogCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultDialogCallback", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("DefaultDialogCallback"));
}
InterfaceWindow* Dialog(int16_t* script, int32_t (*handler)(InterfaceWindow*, int32_t, int32_t, int32_t), int32_t modal) {
    ev("Dialog", {lab(script), lab(reinterpret_cast<const void*>(handler)), modal});
    return static_cast<InterfaceWindow*>(Obj(11));
}
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int32_t>(index)});
    return static_cast<InterfaceFont*>(Obj(41));
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return static_cast<const char16_t*>(Obj(StringLabel(id)));
}
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) {
    ev("UIMain_SetTitleText", {lab(window), id, lab(text)});
}
void MAKECOLUMNBUTTON(InterfaceWindow* window, char16_t* text, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    ev("MAKECOLUMNBUTTON", {lab(window), lab(text), a, b, c, d, e});
}
void RemovePublishParkPanel() { ev("RemovePublishParkPanel", {}); }
// Never called here; their addresses are what the panels pass on.
void PostRequestUploadCallbackFunction(int32_t, const void*) {}
int32_t OnlineParkListPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
void OnlineParkListPostGetAllParksInCityCallbackFn(int32_t, const void*, TbPtrList<CFSOnlinePark>*) {}
InterfaceWindow* OnlineParkListLCFormat() { return nullptr; }
void OnlineParkListLCPostFormat(InterfaceWindow*) {}
int32_t TbUniStringVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

CFSOnlineSystem::CFSOnlineSystem() {
    ev("CFSOnlineSystem", {newlab(this)});
    mParksInCity = static_cast<CFSOnlineSystemParkList_InCity*>(Obj(13));
}
CGameType::CGameType() {
    ev("CGameType", {});
    mType = static_cast<GameType>(ret("CGameType"));
}
void CFSOnlineSystem::RequestUpload(const char* file, int32_t f344, const char16_t* name, const char16_t* description, int32_t f370,
                                    void (*callback)(int32_t, const void*), const void* context) {
    ev("RequestUpload", {lab(this), lab(file), f344, lab(name), lab(description), f370, lab(reinterpret_cast<const void*>(callback)),
                         reinterpret_cast<intptr_t>(context)});
}
void CFSOnlineSystemParkList_InCity::GetList(int32_t cityID, Callback callback, const void* context) {
    ev("GetList", {lab(this), cityID, lab(reinterpret_cast<const void*>(callback)), reinterpret_cast<intptr_t>(context)});
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return static_cast<InterfaceWindow*>(Obj(WindowLabel(id)));
}
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontWrap(int32_t horizontal, int32_t vertical) { ev("SetFontWrap", {lab(this), horizontal, vertical}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), lab(description), lab(reinterpret_cast<const void*>(callback))});
    return static_cast<InterfaceWindow*>(Obj(11));
}
int32_t InterfaceEditBox::SetStaticTextArea(char16_t* text, int32_t capacity) {
    ev("SetStaticTextArea", {lab(this), lab(text), capacity});
    return 0;
}
void InterfaceEditBox::SetCursorDrawObject(InterfaceDrawObject* object) { ev("SetCursorDrawObject", {lab(this), lab(object)}); }
void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    ev("SetColumnFormatsAll", {lab(this), lab(reinterpret_cast<const void*>(makeRow)), lab(reinterpret_cast<const void*>(attach))});
}
void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) { ev("SetColumnType", {lab(this), column, static_cast<int32_t>(type)}); }
void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) { ev("SetSelectionObject", {lab(this), lab(object)}); }
void CLBScreenComponentAdvisor::Add(AdvisorResponse response, int32_t clearFirst) { ev("Add", {lab(this), response.mId, clearFirst}); }
const TbDynamicStringTemplate<char>& CGame::GetCurrentThemeName() {
    ev("GetCurrentThemeName", {lab(this)});
    return *static_cast<const TbDynamicStringTemplate<char>*>(Obj(43));
}
void CGame::GetLocalisedThemeName(const TbDynamicStringTemplate<char>& theme, TbDynamicStringTemplate<char16_t>& out) const {
    ev("GetLocalisedThemeName", {lab(this), lab(&theme), lab(&out)});
}
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {});
    return static_cast<InterfaceDrawObject*>(Obj(40));
}
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list args;
    va_start(args, id);
    TbUniStringVTag* tag = va_arg(args, TbUniStringVTag*);
    va_end(args);
    ev("FormatString", {lab(this), lab(out), size, id, tag->mMagic, tag->mId, lab(tag->mText)});
    return 0;
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_pp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gLobby", &gLobby},
        {"gLobbyScreens", &gLobbyScreens},
        {"gOnlineSystem", &gOnlineSystem},
        {"selection", &gOnlineParkSelection},
        {"uploadfile", &gOnlineUploadFile},
        {"panel", &gPublishParkPanel},
        {"desc", gPublishParkDescription},
        {"pname", gPublishParkName},
        {"pscript", gPublishParkPanelScript},
        {"lscript", gOnlineParkListScript},
        {"listpanel", &gOnlineParkListPanel},
        {"listclose", &gOnlineParkListClose},
        {"gGame", gGameStorage},
        {"convbuf", gUIMainConvertBuffer},
        {"gText", gTextStorage},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_pp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host function with this label (the panels' callbacks and the two the case gives CreateOnlineParkListPanel).
uint64_t tpw_pp_fn(int32_t label) {
    switch (label) {
        case 60: return reinterpret_cast<uintptr_t>(&PublishPanelMsg);
        case 61: return reinterpret_cast<uintptr_t>(&PostRequestUploadCallbackFunction);
        case 62: return reinterpret_cast<uintptr_t>(&OnlineParkListPanelMsg);
        case 63: return reinterpret_cast<uintptr_t>(&OnlineParkListLCFormat);
        case 64: return reinterpret_cast<uintptr_t>(&OnlineParkListLCPostFormat);
        case 65: return reinterpret_cast<uintptr_t>(&OnlineParkListPostGetAllParksInCityCallbackFn);
        case 66: return reinterpret_cast<uintptr_t>(&FakeClose0);
        case 67: return reinterpret_cast<uintptr_t>(&FakeClose1);
        default: return 0;
    }
}

uint64_t tpw_pp_blk(int32_t label, uint32_t size) { return reinterpret_cast<uintptr_t>(Block(label, size)); }

uint64_t tpw_pp_window(int32_t label, int32_t z) {
    MakeVtable();
    InterfaceWindow* window = static_cast<InterfaceWindow*>(Block(label, sizeof(InterfaceWindow)));
    *reinterpret_cast<void***>(window) = gVtable;
    window->mZ = static_cast<int16_t>(z);
    return reinterpret_cast<uintptr_t>(window);
}

uint64_t tpw_pp_lobby(int32_t label, uint64_t window) {
    CLBLobby* lobby = static_cast<CLBLobby*>(Block(label, sizeof(CLBLobby)));
    lobby->mWindow = reinterpret_cast<InterfaceWindow*>(window);
    return reinterpret_cast<uintptr_t>(lobby);
}

uint64_t tpw_pp_screens(int32_t label, uint64_t advisor) {
    CLBScreenSystem* screens = static_cast<CLBScreenSystem*>(Block(label, sizeof(CLBScreenSystem)));
    screens->mAdvisor = reinterpret_cast<CLBScreenComponentAdvisor*>(advisor);
    return reinterpret_cast<uintptr_t>(screens);
}

uint64_t tpw_pp_system(int32_t label, uint64_t parkList) {
    CFSOnlineSystem* system = static_cast<CFSOnlineSystem*>(Block(label, sizeof(CFSOnlineSystem)));
    system->mParksInCity = reinterpret_cast<CFSOnlineSystemParkList_InCity*>(parkList);
    return reinterpret_cast<uintptr_t>(system);
}

uint64_t tpw_pp_city(int32_t label, int32_t id, uint64_t name) {
    OnlineParkSelection* city = static_cast<OnlineParkSelection*>(Block(label, sizeof(OnlineParkSelection)));
    city->mID = id;
    city->mName = reinterpret_cast<const char16_t*>(name);
    return reinterpret_cast<uintptr_t>(city);
}

// gText's table of interface strings (resource 6).
void tpw_pp_settable(uint64_t table) { reinterpret_cast<CText*>(gTextStorage)->mResourcePointers[6] = reinterpret_cast<void*>(table); }

int32_t tpw_pp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(OnlineParkSelection, mName));
    out[n++] = static_cast<int64_t>(offsetof(OnlineUploadFile, mPath));
    return n;
}

// ---- the functions
int32_t tpw_pp_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return PublishPanelMsg(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}
void tpw_pp_publish() { CreatePublishParkPanel(); }
void tpw_pp_list(uint64_t onClose) { CreateOnlineParkListPanel(reinterpret_cast<void (*)()>(onClose)); }

}  // extern "C"
