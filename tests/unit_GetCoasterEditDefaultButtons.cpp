// Test library for the coaster editor's button functions (unit v5920; libtpw_unit_GetCoasterEditDefaultButtons.dylib,
// tests/test_GetCoasterEditDefaultButtons.py), main app:
//   src/LayoutSetRotatePylon_DeleteLastTrackBrace_SetCoasterButtonState_GetCoasterEditDefaultButtons.cpp
//   HandleCoasterButtonDown, GetCoasterEditDefaultButtons, GetCoasterEditAvailableButtons
//
// DisableCurrentPylonEditState, FlashEndPylons and SetPylonType are recording fakes; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "ui_main.h"

using namespace calllog;

tag_sCoasterInstance* gCoasterEditInstance;
uint32_t gCoasterEditEnabled;
uint32_t gCoasterEditDown;
uint32_t gCoasterWord16E0;
uint32_t gCoasterWord1790;
uint32_t gCoasterInputFlags;

void DisableCurrentPylonEditState() { ev("DisableCurrentPylonEditState", {}); }

void FlashEndPylons(tag_sCoasterInstance* instance, uint32_t mode) { ev("FlashEndPylons", {lab(instance), static_cast<int64_t>(mode)}); }

void SetPylonType(uint32_t type) { ev("SetPylonType", {static_cast<int64_t>(type)}); }

extern "C" {

uint64_t tpw_cebtn_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"edit", &gCoasterEditInstance}, {"enabled", &gCoasterEditEnabled}, {"down", &gCoasterEditDown},
        {"w16E0", &gCoasterWord16E0},    {"w1790", &gCoasterWord1790},      {"input", &gCoasterInputFlags},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cebtn_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cebtn_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mType));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBlock3C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBraces));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mPreviewBrace));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFlags));
    return n;
}

void tpw_cebtn_handle(uint32_t buttons) { HandleCoasterButtonDown(buttons); }
void tpw_cebtn_default(uint64_t out) { GetCoasterEditDefaultButtons(reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(out))); }
void tpw_cebtn_available(uint64_t out) { GetCoasterEditAvailableButtons(reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(out))); }

}  // extern "C"
