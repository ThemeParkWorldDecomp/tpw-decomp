// Test library for the park status panel's creation (unit v5572; libtpw_unit_CreateParkStatusPanel.dylib, tests/test_CreateParkStatusPanel.py), main app:
//   src/CreateParkStatusPanel.cpp   CreateParkStatusPanel
//
// Every callee outside the unit is a fake here that records the arguments the real one reads (the same event names as the module's PowerPC
// stubs) and answers from the test's queues. The windows are blocks the test fills; GetWindow answers from the table gWins.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FinanceGraphPanelMsg.h"
#include "MainWindowCallback.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
uint32_t gParkStatusFlags;
int32_t gParkStatusRange;
InterfaceWindow* gParkStatusPanel;
int32_t gParkStatusLastTotal;
int32_t gParkStatusLastInPark;
InterfaceWindow* gGameWindow;
CWorld* gGameWorld;

// The windows the fakes answer: 0 the panel Build makes, 1 to 26 the children by id 0x4a68bb + index - 1, 27 the game window, 29 the analyser.
static void* gWins[32];

namespace {

uint32_t Low(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

}  // namespace

// The panel's message function (src/ParkInfoLines.cpp is not in this library); only its address is used.
int32_t ParkStatusPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

// ---- the fakes
bool UIMain_CheckPanel() {
    ev("CheckPanel", {});
    return false;
}

void SetLastPanel(int32_t type, int32_t panel) { ev("SetLastPanel", {type, panel}); }

void UIMain_SetPanel(InterfaceWindow* window) { ev("SetPanel", {lab(window)}); }

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(static_cast<uintptr_t>(0x6000 + index));
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(0x5000 + id * 4));
}

void DoDominantThoughts() { ev("DoDominantThoughts", {}); }

void UIMain_AdvisorMessage(AdvisorMessage message) { ev("AdvisorMessage", {static_cast<int32_t>(message)}); }

InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    int64_t cb = callback == ParkStatusPanelMsg ? 0x10267ca0 : Low(reinterpret_cast<void*>(callback));
    ev("Build", {lab(this), description == gParkStatusScript ? 0x102ab734 : static_cast<int64_t>(Low(description)), cb});
    return static_cast<InterfaceWindow*>(gWins[0]);
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    int32_t index = id - 0x4a68bb + 1;
    return static_cast<InterfaceWindow*>(gWins[index]);
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }

void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SetDrawObject", {lab(this), lab(object)}); }

void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }

void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), Low(font)}); }

void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}

void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), Low(text)}); }

void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    int64_t cb = Low(reinterpret_cast<void*>(callback));
    if (callback == ThoughtMsg) cb = 0x10267cb0;
    if (callback == GraphPanelMsg) cb = 0x10267c98;
    ev("SetCallbackFunction", {lab(this), cb});
}

int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {lab(this), id, ms});
    return static_cast<int16_t>(ret("setAnimTimer"));
}

int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("startAnimTimer", {lab(this), which});
    return 0;
}

void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }

int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {lab(this), static_cast<uint8_t>(state), static_cast<uint8_t>(redraw)});
    return 0;
}

int32_t InterfaceCheckButtons::SetCurrent(int32_t id) {
    ev("SetCurrent", {lab(this), id});
    return 0;
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return static_cast<CParkAnalyser*>(gWins[29]);
}

int32_t CParkAnalyser::NumberOfVisitorsInPark() const {
    ev("InPark", {lab(this)});
    return static_cast<int32_t>(ret("InPark"));
}

int32_t CParkAnalyser::NumberOfVisitorsSinceBeginning() const {
    ev("Total", {lab(this)});
    return static_cast<int32_t>(ret("Total"));
}

int32_t CParkAnalyser::PercentageOfVisitorsWithHappinessUnder(int32_t level) const {
    ev("Under", {lab(this), level});
    return static_cast<int32_t>(ret("Under"));
}

int32_t CParkAnalyser::PercentageOfVisitorsWithHappinessOver(int32_t level) const {
    ev("Over", {lab(this), level});
    return static_cast<int32_t>(ret("Over"));
}

namespace parkstatus_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {size});
    return tpw::ArenaAllocZeroed(size < 4 ? 4 : size);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {newlab(mem)}); }
void DrawSpriteCtor(void* mem) { ev("DrawSpriteCtor", {newlab(mem)}); }
void NumberCtor(void* mem) { ev("NumberCtor", {newlab(mem)}); }
void GradientCtor(void* mem, const char* meshName) {
    ev("GradientCtor", {newlab(mem), static_cast<int64_t>(0x1024e180 + (meshName - gParkStatusMeshNames))});
}
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {lab(window), value}); }
void WindowSetText(InterfaceWindow* window, const char16_t* text) { ev("SetText", {lab(window), Low(text)}); }
}  // namespace parkstatus_callee

// The constructors the unit's weak callee wrappers name (src/CreateParkStatusPanel.cpp) but the library does not compile: empty bodies under
// their mangled names (never called, the fakes above replace the wrappers), so the library links without their classes' vtables.
extern "C" {
void tpw_ps_dummy_sprite1(void*) __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
void tpw_ps_dummy_sprite2(void*) __asm__("__ZN25InterfaceTextObjectSpriteC2EPv");
void tpw_ps_dummy_draw1(void*) __asm__("__ZN25InterfaceDrawObjectSpriteC1EPv");
void tpw_ps_dummy_draw2(void*) __asm__("__ZN25InterfaceDrawObjectSpriteC2EPv");
void tpw_ps_dummy_number1(void*) __asm__("__ZN25InterfaceNumberTextObjectC1EPv");
void tpw_ps_dummy_number2(void*) __asm__("__ZN25InterfaceNumberTextObjectC2EPv");
void tpw_ps_dummy_grad1(void*) __asm__("__ZN28InterfaceDrawEngineHGradientC1EPv");
void tpw_ps_dummy_grad2(void*) __asm__("__ZN28InterfaceDrawEngineHGradientC2EPv");
void tpw_ps_dummy_sprite1(void*) {}
void tpw_ps_dummy_sprite2(void*) {}
void tpw_ps_dummy_draw1(void*) {}
void tpw_ps_dummy_draw2(void*) {}
void tpw_ps_dummy_number1(void*) {}
void tpw_ps_dummy_number2(void*) {}
void tpw_ps_dummy_grad1(void*) {}
void tpw_ps_dummy_grad2(void*) {}
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_ps_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", &gParkStatusPanel},     {"flags", &gParkStatusFlags},       {"range", &gParkStatusRange},
        {"lastTotal", &gParkStatusLastTotal}, {"lastInPark", &gParkStatusLastInPark}, {"gameWindow", &gGameWindow},
        {"gameWorld", &gGameWorld},       {"wins", gWins},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block (32-bit addressable).
uint64_t tpw_ps_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host window's size and the offset of mZ (the Mac's is 0xdc).
int32_t tpw_ps_layout(int64_t* out) {
    int32_t n = 0;
    alignas(16) static char block[sizeof(InterfaceWindow)];
    InterfaceWindow* window = reinterpret_cast<InterfaceWindow*>(block);
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&window->mZ) - block);
    out[n++] = static_cast<int64_t>(sizeof(InterfaceWindow));
    return n;
}

void tpw_ps_create() { CreateParkStatusPanel(); }

}  // extern "C"
