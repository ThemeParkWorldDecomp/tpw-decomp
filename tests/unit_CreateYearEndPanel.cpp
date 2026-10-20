// Test library for CreateYearEndPanel (unit v5913; libtpw_unit_CreateYearEndPanel.dylib, tests/test_CreateYearEndPanel.py), main app:
//   src/UpgradeStatusMsg_UpgradeStatusPanel_InitStatusWindow_2_CreateYearEndPanel.cpp   CreateYearEndPanel, gYearEndPanel, gYearEndScript
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The windows,
// strings and fonts are arena blocks the test names by labels (tpw_yep_setlabel); the new draw objects get the constructor labels.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
InterfaceWindow* gGameWindow;
CWorld* gGameWorld;

namespace {

void* gLabelPtr[64];
template <class T>
int64_t I(T value) {
    return static_cast<int64_t>(value);
}
void* LabelPtr(int64_t label) { return label > 0 && label < 64 ? gLabelPtr[label] : nullptr; }
const int64_t kAnalyserLabel = 4;

}  // namespace

// ---- the fakes (the same event names and arguments as the module's PowerPC stubs)
bool UIMain_CheckPanel() {
    ev("CheckPanel", {});
    return ret("CheckPanel") != 0;
}
void UIMain_SetPanel(InterfaceWindow* window) { ev("SetPanel", {lab(window)}); }
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {I(id)});
    return id >= 0xba && id <= 0xc1 ? static_cast<const char16_t*>(LabelPtr(30 + (id - 0xba))) : nullptr;
}
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) { ev("SetTitleText", {lab(window), I(id), lab(text)}); }
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("Font", {I(index)});
    return static_cast<InterfaceFont*>(index == 6 ? LabelPtr(40) : index == 8 ? LabelPtr(41) : nullptr);
}

InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), I(description == gYearEndScript), I(callback == YearEndPanelMsg)});
    return static_cast<InterfaceWindow*>(LabelPtr(ret("Build")));
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), I(id)});
    return static_cast<InterfaceWindow*>(LabelPtr(ret("GW", id, 0)));
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), lab(text)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), I(horizontal), I(vertical)}); }

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return static_cast<CParkAnalyser*>(LabelPtr(kAnalyserLabel));
}

template <>
bool CHistory<int, 144>::GetValue(int32_t& out, int32_t back) const {
    ev("GetValue", {I(reinterpret_cast<const char*>(this) - static_cast<const char*>(LabelPtr(kAnalyserLabel))), I(back)});
    out = static_cast<int32_t>(ret("GV"));
    return true;
}

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {I(size)});
    return tpw::ArenaAllocZeroed(size);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {I(newlab(mem))}); }
void CashCtor(void* mem) { ev("CashCtor", {I(newlab(mem))}); }
void NumberCtor(void* mem) { ev("NumberCtor", {I(newlab(mem))}); }
void PercentCtor(void* mem) { ev("PercentCtor", {I(newlab(mem))}); }
void WindowCtor(void* mem, InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) { (void)mem; }
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {lab(window), I(value)}); }
}  // namespace lender_callee

extern "C" {

uint64_t tpw_yep_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gwin", &gGameWindow},
        {"world", &gGameWorld},
        {"panel", &gYearEndPanel},
        {"script", &gYearEndScript},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_yep_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_yep_setlabel(int32_t label, uint64_t address) {
    if (label > 0 && label < 64) gLabelPtr[label] = reinterpret_cast<void*>(static_cast<uintptr_t>(address));
}

int32_t tpw_yep_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CHistory<int, 144>));
    return n;
}

void tpw_yep_run() { CreateYearEndPanel(); }

}  // extern "C"
