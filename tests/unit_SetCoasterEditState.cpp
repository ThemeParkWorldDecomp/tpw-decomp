// Test library for SetCoasterEditState, RejoinTrack, SetPlaceLoop (unit v5257; libtpw_unit_SetCoasterEditState.dylib, tests/test_SetCoasterEditState.py), main app:
//   src/DisableCurrentPylonEditState_SetCoasterEditState.cpp
//
// The callees outside the unit are recording fakes (DisableCurrentPylonEditState, SetCoasterButtonState, GetCoasterEditAvailableButtons,
// the two UIInterface button calls, SetCoasterState, FlagGenerateTrack). The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "UIInterface.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
tag_sCoasterInstance* gCoasterEditInstance;
uint32_t gCoasterWord1790;
uint32_t gCoasterInputFlags;
uint32_t gCoasterEditEnabled;
uint32_t gCoasterEditDown;
UIInterface gUIInterface;

// ---- the fakes
void DisableCurrentPylonEditState() { ev("DisableCurrentPylonEditState", {}); }

void SetCoasterButtonState(uint32_t state, uint32_t arg) { ev("SetCoasterButtonState", {static_cast<int64_t>(state), static_cast<int64_t>(arg)}); }

void GetCoasterEditAvailableButtons(uint32_t* out) {
    ev("GetCoasterEditAvailableButtons", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(out))});
    gCoasterEditEnabled = static_cast<uint32_t>(ret("get_en"));
    gCoasterEditDown = static_cast<uint32_t>(ret("get_dn"));
}

void UIInterface::ChangeCoasterButtonsEnabled(uint32_t mask) { ev("ChangeCoasterButtonsEnabled", {lab(this), static_cast<int64_t>(mask)}); }

void UIInterface::ChangeCoasterButtonsDown(uint32_t mask) { ev("ChangeCoasterButtonsDown", {lab(this), static_cast<int64_t>(mask)}); }

void SetCoasterState(tag_sCoasterInstance* instance, uint32_t state) { ev("SetCoasterState", {lab(instance), static_cast<int64_t>(state)}); }

void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) { ev("FlagGenerateTrack", {lab(instance), flag}); }

extern "C" {

uint64_t tpw_sce_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"edit", &gCoasterEditInstance}, {"flags", &gCoasterInputFlags}, {"w1790", &gCoasterWord1790},
        {"enabled", &gCoasterEditEnabled}, {"down", &gCoasterEditDown}, {"ui", &gUIInterface},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sce_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sce_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mType));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBraces));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBlock3C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mPreviewBrace));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBrace64));
    return n;
}

void tpw_sce_set(uint32_t state) { SetCoasterEditState(state); }
void tpw_sce_rejoin(uint64_t instance) { RejoinTrack(reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance))); }
int32_t tpw_sce_loop() { return SetPlaceLoop(); }

}  // extern "C"
