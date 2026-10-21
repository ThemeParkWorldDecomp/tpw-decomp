// Test library for the all-staff panel (unit v5019; libtpw_unit_CreateAllStaffPanel.dylib, tests/test_CreateAllStaffPanel.py), main app:
//   src/game/ui/ui_allStaffPanel.cpp   CreateAllStaffPanel and the globals gStaffColumnFont, gStaffSort, gStaffPanel, gStaffType
//
// Every callee is a fake that records the arguments the real one reads and answers from the test's queue. The windows are raw blocks
// whose first word is a hand made vtable (SetValue and SetText are the only virtual calls). The objects the function makes (the
// sprite and the gradient) have fake constructors (the symbols are defined under their mangled names).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "MainWindowCallback.h"
#include "InterfaceFont.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"

using namespace calllog;

// ---- globals of other files, defined here
InterfaceWindow* gGameWindow = nullptr;
CWorld* gGameWorld = nullptr;
int16_t gAllStaffScript[4] = {0, 1, 0, 0};

namespace {

void* gPanelObj = nullptr;
void* gAnalyserObj = nullptr;
void* gFontObj = nullptr;
void* gDrawObj = nullptr;
void* gWin[12] = {};
const int32_t kWinIds[12] = {0x321, 0x10, 0x11, 0x12, 0x13, 0x14, 0x322, 0x329, 0x32a, 0x32b, 0x32c, 0x32d};
void* gVtbl[32] = {};

int64_t P(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))); }

void FakeSetValue(void* self, int32_t value) { ev("SetValue", {lab(self), value}); }
int32_t FakeSetText(void* self, char16_t* text) {
    ev("SetText", {lab(self), P(text)});
    return 0;
}

}  // namespace

// ---- the fakes
bool UIMain_CheckPanel() {
    ev("CheckPanel", {});
    return false;
}
void SetLastPanel(int32_t type, int32_t panel) { ev("SetLastPanel", {type, panel}); }
void UIMain_SetPanel(InterfaceWindow* window) { ev("SetPanel", {lab(window)}); }
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(0x70000000u + static_cast<uint32_t>(id)));
}
void MAKECOLUMNBUTTON(InterfaceWindow* window, char16_t* text, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    ev("MAKECOLUMNBUTTON", {lab(window), P(text), a, b, c, d, e});
}
void FillAllStaffList(StaffType type, int32_t flag) { ev("FillAllStaffList", {static_cast<int32_t>(type), flag}); }
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {P(reinterpret_cast<void*>(static_cast<uintptr_t>(index)))});
    return static_cast<InterfaceFont*>(gFontObj);
}
void UIMain_IndentClient(InterfaceWindow* window, int16_t a, int16_t b) { ev("IndentClient", {lab(window), a, b}); }
void UIMain_AdvisorMessage(AdvisorMessage message) { ev("AdvisorMessage", {static_cast<int32_t>(message)}); }
InterfaceWindow* AllStaffLCFormat() { return nullptr; }
void AllStaffLCPostFormat(InterfaceWindow*) {}
int32_t AllStaffPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t AllStaffPanelLCHandler(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), description == gAllStaffScript ? 0x102a8d0e : 0,
                 callback == AllStaffPanelMsg ? 0x10267a88 : 0});
    return ret("build") != 0 ? static_cast<InterfaceWindow*>(gPanelObj) : nullptr;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    char key[16];
    std::snprintf(key, sizeof key, "w%x", static_cast<unsigned>(id));
    for (int i = 0; i < 12; ++i)
        if (kWinIds[i] == id) return ret(key) != 0 ? static_cast<InterfaceWindow*>(gWin[i]) : nullptr;
    return nullptr;
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), callback == AllStaffPanelLCHandler ? 0x10267a80 : 0});
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), P(text)}); }
int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {lab(this), id, ms});
    return static_cast<int16_t>(ret("setAnimTimer"));
}
int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("startAnimTimer", {lab(this), which});
    return 0;
}

void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    ev("SetColumnFormatsAll", {lab(this), makeRow == AllStaffLCFormat ? 0x10267a78 : 0, attach == AllStaffLCPostFormat ? 0x10267a70 : 0});
}
void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) {
    ev("SetColumnType", {lab(this), column, static_cast<int32_t>(type)});
}
void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) { ev("SetSelectionObject", {lab(this), lab(object)}); }
void InterfaceListControl2::SetSort(int32_t column, bool descending) { ev("SetSort", {lab(this), column, descending ? 1 : 0}); }
int32_t InterfaceListControl2::FindData(int32_t data) {
    ev("FindData", {lab(this), data});
    return static_cast<int32_t>(ret("FindData"));
}
void InterfaceListControl2::SetCurrent(int32_t position) { ev("ListSetCurrent", {lab(this), position}); }
int32_t InterfaceCheckButtons::SetCurrent(int32_t id) {
    ev("CheckSetCurrent", {lab(this), id});
    return 0;
}

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {}, {name});
    return static_cast<InterfaceDrawObject*>(gDrawObj);
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return static_cast<CParkAnalyser*>(gAnalyserObj);
}
float CParkAnalyser::GetStaffHappinessLevel(uint32_t unused) const {
    ev("GetStaffHappinessLevel", {lab(this), static_cast<int64_t>(unused)});
    uint32_t bits = static_cast<uint32_t>(ret("level"));
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}
int32_t CParkAnalyser::AverageHappinessOfStaffType(CThing::ThingModel model) const {
    ev("Average", {lab(this), static_cast<int32_t>(model)});
    return static_cast<int32_t>(ret("Average"));
}

// The constructors under their mangled names (the classes' vtables are not part of this library).
void FakeSpriteCtor(void* self, void* data) asm("__ZN25InterfaceTextObjectSpriteC1EPv");
void FakeSpriteCtor(void* self, void* data) { ev("SpriteCtor", {newlab(self), P(data)}); }
void FakeSpriteCtor2(void* self, void* data) asm("__ZN25InterfaceTextObjectSpriteC2EPv");
void FakeSpriteCtor2(void* self, void* data) { ev("SpriteCtor", {newlab(self), P(data)}); }
void FakeGradientCtor2(void* self, void* data) asm("__ZN28InterfaceDrawEngineHGradientC2EPv");
void FakeGradientCtor2(void* self, void* data) { ev("GradientCtor", {newlab(self)}, {static_cast<const char*>(data)}); }
void FakeGradientCtor(void* self, void* data) asm("__ZN28InterfaceDrawEngineHGradientC1EPv");
void FakeGradientCtor(void* self, void* data) { ev("GradientCtor", {newlab(self)}, {static_cast<const char*>(data)}); }

extern "C" {

uint64_t tpw_staff_addr(const char* name) {
    if (gVtbl[8] == nullptr) {
        gVtbl[7] = gVtbl[8] = reinterpret_cast<void*>(&FakeSetValue);
        gVtbl[9] = gVtbl[10] = reinterpret_cast<void*>(&FakeSetText);
    }
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gamewin", &gGameWindow}, {"world", &gGameWorld},       {"panelp", &gPanelObj}, {"analyserp", &gAnalyserObj},
        {"fontp", &gFontObj},      {"drawp", &gDrawObj},         {"wins", gWin},         {"vtbl", gVtbl},
        {"stafftype", &gStaffType}, {"staffsort", &gStaffSort},  {"stafffont", &gStaffColumnFont}, {"staffpanel", &gStaffPanel},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_staff_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_staff_layout(int64_t*) { return 0; }

void tpw_staff_create(int32_t type, uint32_t thing) {
    CreateAllStaffPanel(static_cast<StaffType>(type), reinterpret_cast<CThing*>(static_cast<uintptr_t>(thing)));
}

}  // extern "C"
