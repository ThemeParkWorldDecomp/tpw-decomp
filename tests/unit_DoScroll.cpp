// Test library for the research panel's DoScroll, InitAll and ResearchPanelMsg (unit v5097; libtpw_unit_DoScroll.dylib, tests/test_DoScroll.py), main app:
//   src/game/ui/ui_researchPanel.cpp   DoScroll, InitAll, ResearchPanelMsg, gResearchScrollSound
//
// The callees outside the unit are recording fakes; the windows are arena blocks whose vtable (built here) answers SetValue with a fake.
// The globals of other files the unit reads, defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "InterfaceWindow.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CWorld* gGameWorld;
InterfaceWindow* gResearchPanel;
int32_t gResearchMaxAbility;
int32_t gResearchCurrentAbility;
int32_t gResearchGaugeValue;
uint8_t gResearchPanelMade;
int32_t gResearchSoundA;
int32_t gResearchSoundB;

// The Mac's UIMain_UISound answers the sound handle (the header says void): the fake carries the answer under the same symbol.
int32_t UIMain_UISoundFake(int32_t code) __asm__("__Z13UIMain_UISoundi");

namespace {

InterfaceWindow* gWin[9];   // ids 0xd2d0b to 0xd2d12, then any other id
void* gVt[64];

int32_t FakeSetValue(InterfaceWindow* self, int32_t value) {
    ev("SetValue", {lab(self), value});
    return 0;
}

}  // namespace

// ---- the fakes: the same event name and arguments as the module's PowerPC stubs
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    char key[24];
    std::snprintf(key, sizeof key, "GW%x", static_cast<unsigned>(id));
    if (ret(key) == 0) return nullptr;
    int index = (id >= 0xd2d0b && id <= 0xd2d12) ? id - 0xd2d0b : 8;
    return gWin[index];
}

void InterfaceWindow::SetFrame(int32_t frame) { ev("SetFrame", {lab(this), frame}); }

int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {lab(this), id, ms});
    return static_cast<int16_t>(ret("setAnimTimer"));
}

int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("startAnimTimer", {lab(this), which});
    return 0;
}

uint32_t CResearchLab::SetEffort(int32_t group, uint32_t effort) {
    ev("SetEffort", {lab(this), group, static_cast<int64_t>(effort)});
    return 0;
}

void CResearchLab::SetWorkLoad(uint32_t load) { ev("SetWorkLoad", {lab(this), static_cast<int64_t>(load)}); }

uint32_t CResearchLab::GetEffort(uint32_t group) const {
    ev("GetEffort", {lab(this), static_cast<int64_t>(group)});
    return static_cast<uint32_t>(ret("GetEffort"));
}

namespace {
CParkAnalyser* gAnalyser;
CResearchLab* gLab;
}  // namespace

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return gAnalyser;
}

CResearchLab* CWorld::GetResearchLab() {
    ev("GetResearchLab", {lab(this)});
    return gLab;
}

int32_t CParkAnalyser::CurrentResearcherAbility() const {
    ev("CurrentResearcherAbility", {lab(this)});
    return static_cast<int32_t>(ret("CurrentResearcherAbility"));
}

void NameWindow(int32_t group) { ev("NameWindow", {group}); }
void PercentComplete(int32_t group) { ev("PercentComplete", {group}); }
void ResearchSpeed(int32_t group) { ev("ResearchSpeed", {group}); }
void RemoveResearchPanel() { ev("RemoveResearchPanel", {}); }

int32_t SOUND_SetParamValue(int32_t handle, uint32_t param, uint32_t value) {
    ev("SOUND_SetParamValue", {handle, static_cast<int64_t>(param), static_cast<int64_t>(value)});
    return 0;
}
void SOUND_StopSoundEffect(int32_t handle) { ev("SOUND_StopSoundEffect", {handle}); }
void SOUND_FadeSoundEffect(int32_t handle) { ev("SOUND_FadeSoundEffect", {handle}); }
void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("UIMain_CancelAdvisorMessage", {static_cast<int64_t>(message)}); }

int32_t UIMain_DefaultPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UIMain_DefaultPanelMsg", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("UIMain_DefaultPanelMsg"));
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("UIMain_GetString"))));
}

void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) {
    ev("UIMain_SetTitleText", {lab(window), id, static_cast<int64_t>(reinterpret_cast<uintptr_t>(text))});
}

int32_t UIMain_UISoundFake(int32_t code) {
    ev("UIMain_UISound", {code});
    return static_cast<int32_t>(ret("UIMain_UISound"));
}

extern "C" {

uint64_t tpw_rp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},         {"panel", &gResearchPanel},      {"scroll", &gResearchScrollSound},
        {"soundB", &gResearchSoundB},   {"soundA", &gResearchSoundA},    {"gauge", &gResearchGaugeValue},
        {"current", &gResearchCurrentAbility}, {"max", &gResearchMaxAbility}, {"made", &gResearchPanelMade},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_rp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Makes the case's objects: out = panel, world, analyser, lab, then the nine windows (their vtable answers SetValue with the fake).
void tpw_rp_objects(uint64_t* out) {
    auto pm = &InterfaceWindow::SetValue;
    uintptr_t raw = 0;
    std::memcpy(&raw, &pm, sizeof raw);   // arm64: the first word of a virtual member pointer is the vtable offset itself
    size_t index = raw / sizeof(void*);
    gVt[index] = reinterpret_cast<void*>(&FakeSetValue);
    out[0] = tpw_rp_alloc(64);
    out[1] = tpw_rp_alloc(64);
    out[2] = tpw_rp_alloc(64);
    out[3] = tpw_rp_alloc(0x4000);
    gAnalyser = reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(out[2]));
    gLab = reinterpret_cast<CResearchLab*>(static_cast<uintptr_t>(out[3]));
    for (int i = 0; i < 9; i++) {
        uint64_t block = tpw_rp_alloc(64);
        *reinterpret_cast<void***>(static_cast<uintptr_t>(block)) = gVt;
        gWin[i] = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(block));
        out[4 + i] = block;
    }
}

int32_t tpw_rp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CResearchLab, mF1460));
    return n;
}

void tpw_rp_scroll(int32_t id, int32_t value) { DoScroll(id, value); }
void tpw_rp_init() { InitAll(); }
int32_t tpw_rp_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return ResearchPanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
