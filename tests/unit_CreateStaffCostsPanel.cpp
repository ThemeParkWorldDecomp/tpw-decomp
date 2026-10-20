// Test library for CreateStaffCostsPanel (unit v5088; libtpw_unit_CreateStaffCostsPanel.dylib, tests/test_CreateStaffCostsPanel.py), main app:
//   src/CreateStaffCostsPanel.cpp   CreateStaffCostsPanel and the globals gStaffCostsPanel, the four history words and the script.
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queues. The windows
// the panel finds are HostWin objects: they have the vtable layout of InterfaceWindow (so SetValue and Enable reach the recording
// overrides), no data. The constructors of the draw objects are defined under their mangled names (they only record).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "History.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CWorld* gGameWorld = nullptr;
InterfaceWindow* gGameWindow = nullptr;

int32_t StaffCostsPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

namespace {

struct HostWin {
    virtual void DrawFunction() {}
    virtual void CalcClientArea() {}
    virtual ~HostWin() {}
    virtual int32_t WhatAmI() { return 0; }
    virtual void ClientDraw() {}
    virtual int16_t Enable(int16_t on) {
        ev("Enable", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(on)});
        return 0;
    }
    virtual int32_t operator-(HostWin&) { return 0; }
    virtual int32_t SetValue(int32_t value) {
        ev("SetValue", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(value)});
        return 0;
    }
};

void* gPool[40];
void* gPanelBlock = nullptr;
void* gFont = nullptr;
char* gAnalyser = nullptr;
void* gHeadquarters = nullptr;
int32_t gCount = 0;
char16_t gTexts[8][8];

const char16_t* TextFor(int32_t id) {
    int32_t slot = -1;
    switch (id) {
        case 0x165: slot = 0; break;
        case 0x166: slot = 1; break;
        case 0x167: slot = 2; break;
        case 0x169: slot = 3; break;
        case 0x93: slot = 4; break;
        default: return nullptr;
    }
    char text[8];
    std::snprintf(text, sizeof text, "T%X", static_cast<unsigned>(id));
    for (int32_t i = 0; i < 8; ++i) gTexts[slot][i] = static_cast<char16_t>(static_cast<unsigned char>(text[i]));
    return gTexts[slot];
}

int64_t Tag(const int32_t* out) {
    if (out == &gStaffCostsWord10) return 1;
    if (out == &gStaffCostsWord14) return 2;
    if (out == &gStaffCostsWithdrawals) return 3;
    if (out == &gStaffCostsDeposits) return 4;
    return 0;
}

}  // namespace

// ---- the constructors of the draw objects, under the mangled names of the real ones
extern "C" __attribute__((used)) void tpw_fake_cash_ctor(void* self, void*) {
    ev("CashTextObject", {static_cast<int64_t>(newlab(self))});
}
__asm__(".text\n.p2align 2\n.globl __ZN23InterfaceCashTextObjectC1EPv\n.globl __ZN23InterfaceCashTextObjectC2EPv\n"
        "__ZN23InterfaceCashTextObjectC1EPv:\n__ZN23InterfaceCashTextObjectC2EPv:\n    b _tpw_fake_cash_ctor\n");
extern "C" __attribute__((used)) void tpw_fake_sprite_ctor(void* self, void*) {
    ev("SpriteTextObject", {static_cast<int64_t>(newlab(self))});
}
__asm__(".text\n.p2align 2\n.globl __ZN25InterfaceTextObjectSpriteC1EPv\n.globl __ZN25InterfaceTextObjectSpriteC2EPv\n"
        "__ZN25InterfaceTextObjectSpriteC1EPv:\n__ZN25InterfaceTextObjectSpriteC2EPv:\n    b _tpw_fake_sprite_ctor\n");
extern "C" __attribute__((used)) void tpw_fake_gradient_ctor(void* self, void* data) {
    ev("GradientObject", {static_cast<int64_t>(newlab(self))}, {static_cast<const char*>(data)});
}
__asm__(".text\n.p2align 2\n.globl __ZN28InterfaceDrawEngineHGradientC1EPv\n.globl __ZN28InterfaceDrawEngineHGradientC2EPv\n"
        "__ZN28InterfaceDrawEngineHGradientC1EPv:\n__ZN28InterfaceDrawEngineHGradientC2EPv:\n    b _tpw_fake_gradient_ctor\n");

// ---- the fakes
CGameType::CGameType() { mType = kGameTypeNormal; }

bool UIMain_CheckPanel() {
    ev("CheckPanel", {});
    return false;
}
void SetLastPanel(int32_t type, int32_t panel) { ev("SetLastPanel", {type, panel}); }
void UIMain_SetPanel(InterfaceWindow* window) { ev("SetPanel", {static_cast<int64_t>(lab(window))}); }
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return TextFor(id);
}
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) {
    std::string shown = narrow(text);
    ev("SetTitleText", {static_cast<int64_t>(lab(window)), id}, {shown.c_str()});
}
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(gFont);
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {static_cast<int64_t>(lab(this))});
    return reinterpret_cast<CParkAnalyser*>(gAnalyser);
}
CStaffHQ* CWorld::GetStaffHQ() {
    ev("GetStaffHQ", {static_cast<int64_t>(lab(this))});
    return reinterpret_cast<CStaffHQ*>(gHeadquarters);
}
int32_t CParkAnalyser::AverageQualityOfStaffType(CThing::ThingModel model) const {
    ev("AverageQuality", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(model)});
    char key[16];
    std::snprintf(key, sizeof key, "aq%d", static_cast<int>(model));
    return static_cast<int32_t>(ret(key));
}

template <>
bool CHistory<int32_t, 144>::GetValue(int32_t& out, int32_t back) const {
    const CParkAnalyser* analyser = reinterpret_cast<const CParkAnalyser*>(gAnalyser);
    ptrdiff_t index = (reinterpret_cast<const char*>(this) - reinterpret_cast<const char*>(&analyser->mHistory1F100[0])) /
                      static_cast<ptrdiff_t>(sizeof(CHistory<int32_t, 144>));
    ev("GetValue", {static_cast<int64_t>(index), static_cast<int64_t>(back), Tag(&out)});
    char key[16];
    std::snprintf(key, sizeof key, "gvw%d", static_cast<int>(index));
    if (ret(key) == 0) return false;
    std::snprintf(key, sizeof key, "gv%d", static_cast<int>(index));
    out = static_cast<int32_t>(ret(key));
    return true;
}

InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {static_cast<int64_t>(lab(this)), description == gStaffCostsScript ? 1 : 0, callback == StaffCostsPanelMsg ? 1 : 0});
    return ret("Build") ? reinterpret_cast<InterfaceWindow*>(gPanelBlock) : nullptr;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {static_cast<int64_t>(lab(this)), id});
    int32_t n = gCount++;
    if (id == 0x2de && ret("gw2de") == 0) return nullptr;
    return reinterpret_cast<InterfaceWindow*>(gPool[n]);
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) {
    ev("SetClientDrawObject", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(lab(object))});
}
void InterfaceWindow::SetFont(InterfaceFont* font) {
    ev("SetFont", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(lab(font))});
}
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) {
    ev("SetFontJust", {static_cast<int64_t>(lab(this)), horizontal, vertical});
}
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {static_cast<int64_t>(lab(this)), red, green, blue, alpha});
}
void InterfaceWindow::SetClientDrawText(char16_t* text) {
    std::string shown = narrow(text);
    ev("SetClientDrawText", {static_cast<int64_t>(lab(this))}, {shown.c_str()});
}
void InterfaceVaryBox::SetMinMaxValues(int32_t* minimum, int32_t* maximum) {
    ev("SetMinMax", {static_cast<int64_t>(lab(this)), *minimum, *maximum});
}
void InterfaceVaryBox::SetInc(int32_t* increment) { ev("SetInc", {static_cast<int64_t>(lab(this)), *increment}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_scs_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", &gStaffCostsPanel},       {"word10", &gStaffCostsWord10},   {"word14", &gStaffCostsWord14},
        {"withdrawals", &gStaffCostsWithdrawals}, {"deposits", &gStaffCostsDeposits}, {"world", &gGameWorld},
        {"window", &gGameWindow},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_scs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A window with the vtable of InterfaceWindow and recording SetValue / Enable.
uint64_t tpw_scs_window() {
    void* block = tpw::ArenaAllocZeroed(sizeof(HostWin));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new (block) HostWin()));
}

// The analyser's size on the host, and where the headquarters' five words are.
uint64_t tpw_scs_analyser_size() { return sizeof(CParkAnalyser); }
uint64_t tpw_scs_hq_size() { return sizeof(CStaffHQ); }
uint64_t tpw_scs_hq_offset() {
    CStaffHQ* none = reinterpret_cast<CStaffHQ*>(static_cast<uintptr_t>(0x1000));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&none->mTraining.mF00[0]) - 0x1000);
}

// 0 font, 1 analyser, 2 headquarters, 3 the panel Build answers, 4 the window count back to 0, 100 + i pool window i.
void tpw_scs_set(int32_t which, uint64_t value) {
    void* pointer = reinterpret_cast<void*>(static_cast<uintptr_t>(value));
    if (which == 0) gFont = pointer;
    else if (which == 1) gAnalyser = static_cast<char*>(pointer);
    else if (which == 2) gHeadquarters = pointer;
    else if (which == 3) gPanelBlock = pointer;
    else if (which == 4) gCount = 0;
    else if (which >= 100 && which < 140) gPool[which - 100] = pointer;
}

void tpw_scs_type(int32_t type) { TheGameType().mType = static_cast<GameType>(type); }

void tpw_scs_call() { CreateStaffCostsPanel(); }

}  // extern "C"

// The static game type is built once, before any case (its constructor fake records nothing then).
namespace {
const int32_t kBuilt = (TheGameType(), 0);
}
