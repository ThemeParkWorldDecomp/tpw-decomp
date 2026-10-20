// Test library for CreateFinancePanel (unit v51026; libtpw_unit_CreateFinancePanel_2.dylib, tests/test_CreateFinancePanel_2.py), main app:
//   src/CreateFinancePanel_2.cpp   CreateFinancePanel, gFinanceScript
//
// Every callee outside the unit is a fake that records the arguments the real one reads. Window pointers come from a table the test
// fills (GetWindow answers the table's entry for the id), so no window is ever dereferenced on the host.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
uint32_t gFinanceGraphFlags;
int32_t gFinanceGraphMode;
InterfaceWindow* gFinanceGraphPanel;
InterfaceWindow* gGameWindow;
unsigned char gLoansGameTypeStorage[4];
int8_t gLoansGameTypeGuard;

// ---- the test's tables
static uint64_t gWinTable[28];   // 14 pairs: id, address (0 for a window that does not exist)
static uint64_t gBuildRes;
static uint32_t gNewCount;

int32_t FinanceGraphPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t FinancePanelPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

static int64_t P(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)); }

bool UIMain_CheckPanel() {
    ev("CheckPanel", {});
    return false;
}
void SetLastPanel(int32_t type, int32_t panel) { ev("SetLastPanel", {type, panel}); }
void UIMain_SetPanel(InterfaceWindow* window) { ev("SetPanel", {lab(window)}); }
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(static_cast<uintptr_t>(0x64000000u + index));
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(0x63000000u + static_cast<uint32_t>(id)));
}
void UIMain_AdvisorMessage(AdvisorMessage message) { ev("Advisor", {static_cast<int32_t>(message)}); }

InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), lab(description), lab(reinterpret_cast<const void*>(callback))});
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(gBuildRes));
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    for (int i = 0; i < 14; i++)
        if (gWinTable[2 * i] == static_cast<uint64_t>(id)) return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(gWinTable[2 * i + 1]));
    return nullptr;
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), P(object)}); }
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), lab(reinterpret_cast<const void*>(callback))});
}
void InterfaceWindow::SetFontJust(int32_t h, int32_t v) { ev("SetFontJust", {lab(this), h, v}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), P(font)}); }
void InterfaceWindow::SetFontColour(int32_t r, int32_t g, int32_t b, int32_t a) { ev("SetFontColour", {lab(this), r, g, b, a}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), P(text)}); }
int32_t InterfaceCheckButtons::SetCurrent(int32_t id) {
    ev("SetCurrent", {lab(this), id});
    return 0;
}
int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {lab(this), state, redraw});
    return 0;
}
CGameType::CGameType() {
    ev("CGameType", {lab(this)});
    mType = static_cast<GameType>(ret("gtype"));
}

namespace parkstatus_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {size});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0x62000000u + 0x1000u * gNewCount++));
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {P(mem)}); }
void DrawSpriteCtor(void* mem) { ev("DrawSpriteCtor", {P(mem)}); }
void WindowSetText(InterfaceWindow* window, const char16_t* text) { ev("virt", {lab(window), 0x5C5C002C, P(text)}); }
}  // namespace parkstatus_callee

namespace entry_callee {
void WindowEnable(InterfaceWindow* window, int16_t on) { ev("virt", {lab(window), 0x5C5C001C, on}); }
}  // namespace entry_callee

extern "C" {

uint64_t tpw_fin_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"flags", &gFinanceGraphFlags},
        {"mode", &gFinanceGraphMode},
        {"panel", &gFinanceGraphPanel},
        {"gamewin", &gGameWindow},
        {"guard", &gLoansGameTypeGuard},
        {"gametype", gLoansGameTypeStorage},
        {"wintab", gWinTable},
        {"buildres", &gBuildRes},
        {"newcnt", &gNewCount},
        {"script", gFinanceScript},
        {"cbpanel", reinterpret_cast<const void*>(&FinancePanelPanelMsg)},
        {"cbgraph", reinterpret_cast<const void*>(&FinanceGraphPanelMsg)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fin_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_fin_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CGameType));
    return n;
}

uint32_t tpw_fin_call() {
    CreateFinancePanel();
    return 0;
}

}  // extern "C"
