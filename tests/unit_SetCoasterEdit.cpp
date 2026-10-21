// Test library for SetCoasterEdit, GetPylonFromBraceMesh, GetNthBraceMesh, HandleCoasterButtonClick (unit v5314;
// libtpw_unit_SetCoasterEdit.dylib, tests/test_SetCoasterEdit.py), main app:
//   src/game/rides/CoasterEdit.cpp
//
// The callees outside the unit are fakes that record the arguments and answer from the test's queue; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "UIInterface.h"
#include "ui_main.h"

using namespace calllog;

CActionRecorder gActionRecorder;
UIInterface gUIInterface;
uint32_t gCoasterEditEnabled;
uint32_t gCoasterEditDown;
tag_sRideAnimSet* gRideAnimSets[8];

void SetCoasterEdit(tag_sCoasterInstance* instance, int32_t flag);

namespace {
uint8_t gDummyInstance[16];
}

// ---- the fakes
tag_sCoasterInstance* FindCoasterInstanceFromEntranceID(uint32_t entrance) {
    ev("Find", {entrance});
    return ret("Find") != 0 ? reinterpret_cast<tag_sCoasterInstance*>(gDummyInstance) : nullptr;
}

void SetCoasterEdit(tag_sCoasterInstance* instance, int32_t flag) { ev("SetCoasterEditInst", {lab(instance), flag}); }

void DisableCurrentPylonEditState() { ev("Disable", {}); }

void HandleCoasterButtonDown(uint32_t buttons) { ev("ButtonDown", {buttons}); }

void GetCoasterEditAvailableButtons(uint32_t* out) { ev("Available", {lab(out)}); }

void UIInterface::ChangeCoasterButtonsEnabled(uint32_t mask) { ev("ChangeEnabled", {lab(this), mask}); }

extern "C" {

uint64_t tpw_sce_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"enabled", &gCoasterEditEnabled},
        {"down", &gCoasterEditDown},
        {"recording", &gActionRecorder.mRecording},
        {"slots", gRideAnimSets},
        {"ui", &gUIInterface},
        {"dummy", gDummyInstance},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sce_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets of the ride set's members the module writes by Mac offset (0x04 flags, 0xd4 the block), its size.
int32_t tpw_sce_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mBraceBlock));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(sizeof(gRideAnimSets[0]));
    out[n++] = static_cast<int64_t>(offsetof(sBraceMeshBlock, mList));
    out[n++] = static_cast<int64_t>(sizeof(sBraceMeshBlock));
    out[n++] = static_cast<int64_t>(offsetof(sBraceMeshList, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(sBraceMeshList, mCount));
    out[n++] = static_cast<int64_t>(offsetof(sBraceMeshList, mEntries));
    out[n++] = static_cast<int64_t>(sizeof(sBraceMeshList) + 8 * sizeof(void*));
    out[n++] = static_cast<int64_t>(offsetof(sBraceMeshEntry, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(sBraceMeshEntry, mMesh));
    out[n++] = static_cast<int64_t>(sizeof(sBraceMeshEntry));
    return n;
}

void tpw_sce_set(uint64_t coaster, int32_t flag) { SetCoasterEdit(reinterpret_cast<void*>(static_cast<uintptr_t>(coaster)), flag); }

uint64_t tpw_sce_pylon(uint32_t id, uint64_t out) {
    return static_cast<uint64_t>(
        reinterpret_cast<uintptr_t>(GetPylonFromBraceMesh(id, reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(out)))));
}

uint64_t tpw_sce_nth(uint64_t list, uint32_t n) {
    return static_cast<uint64_t>(
        reinterpret_cast<uintptr_t>(GetNthBraceMesh(reinterpret_cast<sBraceMeshList*>(static_cast<uintptr_t>(list)), n)));
}

void tpw_sce_click(uint32_t buttons, uint32_t event) { HandleCoasterButtonClick(buttons, event); }

}  // extern "C"
