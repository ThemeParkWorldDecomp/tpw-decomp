// Test library for the research panel's SpeedTimerFudge, PercentComplete and ResearchSpeed (unit v5184; libtpw_unit_SpeedTimerFudge.dylib,
// tests/test_SpeedTimerFudge.py), main app:
//   src/SpeedTimerFudge.cpp   the three functions and gResearchProjectNone
//
// The callees outside the unit are recording fakes (window members, the research lab, the world, the tick clock, the sound call); the
// virtual SetValue is a fake in a hand-made vtable of the fake windows. Globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
CWorld* gGameWorld = nullptr;                 // 0x1037ebfc
InterfaceWindow* gResearchPanel = nullptr;    // 0x10395980
int32_t gResearchSoundA = 0;                  // 0x10395968

namespace {

uint64_t gLabelPtr[32];   // label -> object, set by the test for every case

void* Obj(int64_t label) { return (label > 0 && label < 32) ? reinterpret_cast<void*>(gLabelPtr[label]) : nullptr; }

std::string Key(const char* name, int64_t arg) { return std::string(name) + ":" + std::to_string(static_cast<long long>(arg)); }

int32_t FakeSetValue(void* self, int32_t value) {
    ev("SetValue", {lab(self), value});
    return 0;
}

void* gVtable[64];

}  // namespace

// ---- the fakes
void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("getExtraParam", {lab(this), index});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret(Key("getExtraParam", lab(this)).c_str()))));
}

int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {lab(this), index, static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value)))});
    return 0;
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return static_cast<InterfaceWindow*>(Obj(ret(Key("GetWindow", id).c_str())));
}

void InterfaceWindow::SetFrame(int32_t frame) { ev("SetFrame", {lab(this), frame}); }

uint32_t IveGotTicks() {
    ev("IveGotTicks", {});
    return static_cast<uint32_t>(ret("IveGotTicks"));
}

CResearchLab* CWorld::GetResearchLab() {
    ev("GetResearchLab", {lab(this)});
    return static_cast<CResearchLab*>(Obj(3));
}

tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> CResearchLab::GetCurrentProject(ResearchCat category) const {
    ev("GetCurrentProject", {lab(this), static_cast<int32_t>(category)});
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret(Key("GetCurrentProject", static_cast<int32_t>(category)).c_str()));
    return id;
}

uint32_t CResearchLab::GetRemainingPoints(ResearchCat category) const {
    ev("GetRemainingPoints", {lab(this), static_cast<int32_t>(category)});
    return static_cast<uint32_t>(ret(Key("GetRemainingPoints", static_cast<int32_t>(category)).c_str()));
}

uint32_t CResearchLab::GetResearchSpeed(ResearchCat category) const {
    ev("GetResearchSpeed", {lab(this), static_cast<int32_t>(category)});
    return static_cast<uint32_t>(ret(Key("GetResearchSpeed", static_cast<int32_t>(category)).c_str()));
}

int32_t SOUND_SetParamValue(int32_t handle, uint32_t param, uint32_t value) {
    ev("SetParamValue", {handle, param, value});
    return 0;
}

extern "C" {

uint64_t tpw_stf_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", &gResearchPanel},
        {"world", &gGameWorld},
        {"sound", &gResearchSoundA},
        {"none", &gResearchProjectNone},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_stf_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A fake window: a zeroed block whose vtable pointer (the host's, at +0) has the fake SetValue in SetValue's slot.
uint64_t tpw_stf_win() {
    static bool made = false;
    if (!made) {
        made = true;
        // The unit only ever makes the one virtual call (SetValue), so every slot holds the fake.
        for (void*& slot : gVtable) slot = reinterpret_cast<void*>(&FakeSetValue);
    }
    void** block = static_cast<void**>(tpw::ArenaAllocZeroed(0x120));
    block[0] = gVtable;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
}

void tpw_stf_setlab(int32_t label, uint64_t address) { gLabelPtr[label & 31] = address; }

int32_t tpw_stf_layout(int64_t* out) {
    int32_t n = 0;
    using Id = tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>;
    out[n++] = static_cast<int64_t>(sizeof(Id));
    out[n++] = static_cast<int64_t>(offsetof(Id, mIndex));
    return n;
}

void tpw_stf_speedtimerfudge(uint64_t window, int32_t speed) { SpeedTimerFudge(reinterpret_cast<InterfaceWindow*>(window), speed); }
void tpw_stf_percentcomplete(int32_t group) { PercentComplete(group); }
void tpw_stf_researchspeed(int32_t group) { ResearchSpeed(group); }

}  // extern "C"
