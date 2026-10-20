// Test library for the hire panel's list functions (unit v5554; libtpw_unit_HirePanelLCHandler.dylib, tests/test_HirePanelLCHandler.py), main app:
//   src/HirePanelLCHandler.cpp   HirePanelLCPostFormat (0x101a18a4), HirePanelLCHandler (0x101a19f8) and the four globals they use
//
// The callees outside the unit are recording fakes (the same event names and arguments as the PowerPC stubs): the allocation, the two
// constructors, the window's members, GetFontTableEntry, FillHireList, UIMain_GetString, UIMain_SetTitleText, SCR_KillScript.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "LoadPanelMsg.h"
#include "analyser.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
InterfaceWindow* gHirePanelWindow;  // 0x10394924

static InterfaceWindow* gFakeChild;

static int64_t Lo32(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

// ---- the fakes
namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {newlab(mem)}); }
void CashCtor(void* mem) { ev("CashCtor", {newlab(mem)}); }
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("WindowSetValue", {lab(window), value}); }
}  // namespace lender_callee

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("GetFontTableEntry"))));
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), Lo32(font)}); }
InterfaceWindow* InterfaceWindow::GetChild(int32_t unused) {
    ev("GetChild", {lab(this), unused});
    return gFakeChild;
}
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}

void FillHireList(StaffType type, int32_t flag) { ev("FillHireList", {static_cast<int32_t>(type), flag}); }
const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("UIMain_GetString"))));
}
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) { ev("UIMain_SetTitleText", {lab(window), id, Lo32(text)}); }
void SCR_KillScript(int32_t handle) { ev("SCR_KillScript", {handle}); }

extern "C" {

uint64_t tpw_hire_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"font", &gHireLCFontIndex},   {"script", &gHireListScript}, {"sela", &gHireListSelA},
        {"selb", &gHireListSelB},      {"hirewin", &gHirePanelWindow},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_hire_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_hire_layout(int64_t* out) {
    (void)out;
    return 0;
}

void tpw_hire_setchild(uint64_t child) { gFakeChild = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(child)); }

void tpw_hire_post(uint64_t window) { HirePanelLCPostFormat(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window))); }

int32_t tpw_hire_handler(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return HirePanelLCHandler(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
