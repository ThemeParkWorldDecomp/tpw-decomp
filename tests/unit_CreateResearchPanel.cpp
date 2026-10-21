// Test library for CreateResearchPanel (unit v5021; libtpw_unit_CreateResearchPanel.dylib, tests/test_CreateResearchPanel.py), main app:
//   src/game/ui/ui_researchPanel.cpp   CreateResearchPanel and the panel's globals
//
// Every callee is a fake that records what the real one reads. The windows are fake objects (labels 1 to 0x3f) whose virtual SetValue is a
// fake too; GetWindow answers the window whose label is the id's low byte (or NULL, as the case's mask says). The case's numbers come in
// through tpw_crp_config. The three draw object constructors are replaced by their symbols (asm labels), so no vtable is needed.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <sys/mman.h>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"
#include "InterfaceWindow.h"
#include "InterfaceScrollBar.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "FS_Online.h"
#include "World.h"
#include "analyser.h"
#include "ThingReceivers.h"
#include "AdvisorScoring.h"

using namespace calllog;

// ---- globals of other files, defined here
InterfaceWindow* gGameWindow = nullptr;
CWorld* gGameWorld = nullptr;

namespace {

// the case: [0] null mask (bit n: GetWindow of the id with low byte n answers NULL; for 0x10 only its first call), [1] MainMenuUp,
// [2] Build answers the panel, [3] max ability, [4] current ability, [5] and [6] the two UIMain_UISound answers
int64_t gCfg[8];
int gCalls10 = 0;

constexpr int kObjects = 0x43;
void* gObjects[kObjects];
void* gVTable[128];
bool gReady = false;

int64_t I(int64_t v) { return v; }

int32_t FakeSetValue(InterfaceWindow* self, int32_t value) {
    ev("VCall", {lab(self), I(value), 1});
    return 0;
}

void Ready() {
    if (gReady) return;
    gReady = true;
    using PM = int32_t (InterfaceWindow::*)(int32_t);
    PM pm = &InterfaceWindow::SetValue;
    static_assert(sizeof(pm) == 16, "pointer to member");
    uintptr_t raw[2];
    std::memcpy(raw, &pm, sizeof raw);
    // arm64: the offset in the vtable is the first word and the virtual flag is bit 0 of the second
    gVTable[raw[0] / sizeof(void*)] = reinterpret_cast<void*>(&FakeSetValue);
    for (int k = 1; k < kObjects; k++) {
        size_t size = 0x200;
        if (k == 0x40) size = sizeof(CWorld) > 0x1e0000 ? sizeof(CWorld) : 0x1e0000;
        if (k == 0x41) size = 0x100;
        if (k == 0x42) size = sizeof(CResearchLab) > 0x1480 ? sizeof(CResearchLab) : 0x1480;
        gObjects[k] = tpw::ArenaAllocZeroed(static_cast<uint32_t>(size));        if (k < 0x40) *reinterpret_cast<void***>(gObjects[k]) = gVTable;
    }
}

int64_t CbId(InterfaceWindowCallback cb) {
    if (cb == ResearchPanelMsg) return 1;
    if (cb == StoreTankMsg) return 2;
    if (cb == RideMsg) return 3;
    if (cb == UpgMsg) return 4;
    if (cb == ShopMsg) return 5;
    if (cb == ShowMsg) return 6;
    if (cb == FeatureMsg) return 7;
    return cb ? 99 : 0;
}

int64_t MeshId(const void* mesh) {
    if (!mesh) return 0;
    const char* text = static_cast<const char*>(mesh);
    if (std::strcmp(text, "effortguage.wct") == 0) return 1;
    if (std::strcmp(text, "bluebar.wct") == 0) return 2;
    return 9;
}

}  // namespace

// ---- the message functions (untranslated; only their addresses are compared)
int32_t ResearchPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t StoreTankMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t RideMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UpgMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t ShopMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t ShowMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t FeatureMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

// ---- the draw object constructors, by symbol
void FakeVCtor(void* self, void* mesh) __asm__("__ZN28InterfaceDrawEngineVGradientC1EPv");
void FakeHCtor(void* self, void* mesh) __asm__("__ZN28InterfaceDrawEngineHGradientC1EPv");
void FakeTCtor(void* self, void* data) __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
// (operator new itself is not replaced: the constructor's fake logs the allocation first, with the Mac's size, as the PowerPC side's fake
// of operator new does just before it)
void FakeVCtor(void* self, void* mesh) {
    ev("new", {0x124});
    ev("VGradientCtor", {newlab(self)});
    ev("VGradientMesh", {MeshId(mesh)});
}
void FakeHCtor(void* self, void* mesh) {
    ev("new", {0x124});
    ev("HGradientCtor", {newlab(self)});
    ev("HGradientMesh", {MeshId(mesh)});
}
void FakeTCtor(void* self, void* data) {
    ev("new", {0x19c});
    ev("TextSpriteCtor", {newlab(self)});
    ev("TextSpriteData", {I(reinterpret_cast<uintptr_t>(data))});
}
void FakeTCtor2(void* self, void* data) __asm__("__ZN25InterfaceTextObjectSpriteC2EPv");
void FakeTCtor2(void* self, void* data) { FakeTCtor(self, data); }

// UIMain_UISound, as the int the panel stores
int32_t UIMain_UISoundHandle(int32_t code) __asm__("__Z14UIMain_UISoundi");
int32_t UIMain_UISoundHandle(int32_t code) {
    ev("UIMain_UISound", {I(code)});
    return static_cast<int32_t>(code == 0x9a ? gCfg[5] : gCfg[6]);
}

// ---- the free callees
int32_t MainMenuUp() {
    ev("MainMenuUp", {});
    return static_cast<int32_t>(gCfg[1]);
}
bool UIMain_CheckPanel() {
    ev("UIMain_CheckPanel", {});
    return false;
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {I(id)});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(0x70000000u + static_cast<uint32_t>(id)));
}
InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton button1, dlgcallback callback1, dlgbutton button2, dlgcallback callback2,
                              dlgbutton button3, dlgcallback callback3, bool flag) {
    ev("UI_DialogBox", {I(reinterpret_cast<uintptr_t>(params.mText)),
                        I((params.mRed << 16) | (params.mGreen << 8) | params.mBlue), I(params.mValue8), I(params.mValueC),
                        I(button1), callback1 ? 1 : 0, I(button2), callback2 ? 1 : 0, I(button3), callback3 ? 1 : 0, flag ? 1 : 0});
    return nullptr;
}
void UIMain_SetPanel(InterfaceWindow* window) { ev("UIMain_SetPanel", {lab(window)}); }
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {I(index)});
    return reinterpret_cast<InterfaceFont*>(static_cast<uintptr_t>(0x60000000u + index));
}
void InitAll() { ev("InitAll", {}); }
void UIMain_AdvisorMessage(AdvisorMessage message) { ev("UIMain_AdvisorMessage", {I(message)}); }

CGameType::CGameType() { mType = kGameTypeNormal; }

// ---- the world's callees
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return static_cast<CParkAnalyser*>(gObjects[0x41]);
}
CResearchLab* CWorld::GetResearchLab() {
    ev("GetResearchLab", {lab(this)});
    return static_cast<CResearchLab*>(gObjects[0x42]);
}
int32_t CParkAnalyser::MaxResearcherAbility() const {
    ev("MaxResearcherAbility", {lab(this)});
    return static_cast<int32_t>(gCfg[3]);
}
int32_t CParkAnalyser::CurrentResearcherAbility() const {
    ev("CurrentResearcherAbility", {lab(this)});
    return static_cast<int32_t>(gCfg[4]);
}

// ---- the window's callees
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), description == gResearchPanelScript ? 1 : 0, CbId(callback)});
    return gCfg[2] ? static_cast<InterfaceWindow*>(gObjects[0x30]) : nullptr;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), I(id)});
    int low = id & 0xff;
    bool isNull = ((gCfg[0] >> (low & 0x1f)) & 1) != 0;
    if (low == 0x10 && gCalls10++ != 0) isNull = false;
    return isNull ? nullptr : static_cast<InterfaceWindow*>(gObjects[low]);
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) { ev("SetCallbackFunction", {lab(this), CbId(callback)}); }
void InterfaceScrollBar::SetMinMax(int32_t min, int32_t max) { ev("SetMinMax", {lab(this), I(min), I(max)}); }
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SetDrawObject", {lab(this), lab(object)}); }
int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {lab(this), I(index), I(reinterpret_cast<uintptr_t>(value))});
    return 0;
}
void InterfaceWindow::SetFrame(int32_t frame) { ev("SetFrame", {lab(this), I(frame)}); }
void InterfaceWindow::SetFontJust(int32_t h, int32_t v) { ev("SetFontJust", {lab(this), I(h), I(v)}); }
void InterfaceWindow::SetFontWrap(int32_t h, int32_t v) { ev("SetFontWrap", {lab(this), I(h), I(v)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), I(reinterpret_cast<uintptr_t>(font))}); }
void InterfaceWindow::SetFontColour(int32_t r, int32_t g, int32_t b, int32_t a) { ev("SetFontColour", {lab(this), I(r), I(g), I(b), I(a)}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), I(reinterpret_cast<uintptr_t>(text))}); }

extern "C" {

uint64_t tpw_crp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    Ready();
    const Entry entries[] = {
        {"gGameWindow", &gGameWindow},
        {"gGameWorld", &gGameWorld},
        {"gPanel", &gResearchPanel},
        {"gBar", &gResearchEffortBar},
        {"gMax", &gResearchMaxAbility},
        {"gCur", &gResearchCurrentAbility},
        {"gGauge", &gResearchGaugeValue},
        {"gMade", &gResearchPanelMade},
        {"gSndA", &gResearchSoundA},
        {"gSndB", &gResearchSoundB},
        {"gFont", &gResearchFontIndex},
        {"gType", &TheGameType().mType},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The host address of the fake object with that label (windows 1 to 0x3f, the world 0x40, the analyser 0x41, the research lab 0x42).
uint64_t tpw_crp_obj(uint32_t label) {
    Ready();
    return label < kObjects ? static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gObjects[label])) : 0;
}

// The case's numbers (see gCfg); resets the call counter.
void tpw_crp_config(const int64_t* values) {
    Ready();
    for (int i = 0; i < 8; i++) gCfg[i] = values[i];
    gCalls10 = 0;
}

// The host offsets of the world's head halfword (Mac 0x1da742) and the lab's work load (Mac 0x1460).
int32_t tpw_crp_layout(int64_t* out) {
    Ready();
    int32_t n = 0;
    out[n++] = reinterpret_cast<char*>(&static_cast<CWorld*>(gObjects[0x40])->mF1DA742) - static_cast<char*>(gObjects[0x40]);
    out[n++] = reinterpret_cast<char*>(&static_cast<CResearchLab*>(gObjects[0x42])->mF1460) - static_cast<char*>(gObjects[0x42]);
    return n;
}

void tpw_crp_run() {
    CreateResearchPanel();
}

}  // extern "C"
