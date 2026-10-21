// Test library for LLCC_AbortPylonMove, LLCC_IsRideMovable and LLCC_GetRideBlendInfo (unit v5186; libtpw_unit_LLCC_AbortPylonMove.dylib,
// tests/test_LLCC_AbortPylonMove.py), main app:
//   src/game/park/LLCellRide.cpp   the three functions and the globals gLLCCE0B4, gLLCCE0B8, gLLCCE08C, gLLCCDF84
//
// The callees outside the unit are recording fakes: LLCC_DrawLine, CObject::GetDBInfo, LLCR_OffsetStartOfTrack, LLCR_OffsetEndOfTrack and
// CCellBase::NumberOfNeighbours. gLLCCE090 and gGameWorld are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CellBase.h"
#include "LLCellSystem.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
uint8_t gLLCCE090;
CWorld* gGameWorld;

namespace {
RideInfo* gFakeInfo;
}

// ---- the fakes (the same event names and arguments as the module's PowerPC stubs)
void LLCC_DrawLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    ev("DrawLine", {static_cast<int64_t>(mode), x, y, *endX, *endY});
    *endX = static_cast<int32_t>(ret("DLx"));
    *endY = static_cast<int32_t>(ret("DLy"));
}

RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {0});
    return gFakeInfo;
}

uint8_t LLCR_OffsetStartOfTrack(int32_t* x, int32_t* y, int32_t rotation) {
    ev("OffsetStart", {*x, *y, rotation});
    *x = static_cast<int32_t>(ret("OSx"));
    *y = static_cast<int32_t>(ret("OSy"));
    return static_cast<uint8_t>(ret("OS"));
}

uint8_t LLCR_OffsetEndOfTrack(int32_t* x, int32_t* y, int32_t rotation) {
    ev("OffsetEnd", {*x, *y, rotation});
    *x = static_cast<int32_t>(ret("OEx"));
    *y = static_cast<int32_t>(ret("OEy"));
    return static_cast<uint8_t>(ret("OE"));
}

int32_t CCellBase::NumberOfNeighbours() const {
    int64_t index = (reinterpret_cast<intptr_t>(this) - reinterpret_cast<intptr_t>(&gGameWorld->mTrackCells[0])) / 0x28;
    ev("NumberOfNeighbours", {index});
    return static_cast<int32_t>(ret("NN"));
}

extern "C" {

uint64_t tpw_apm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"e0b4", &gLLCCE0B4},
        {"e0b8", &gLLCCE0B8},
        {"e08c", &gLLCCE08C},
        {"e090", &gLLCCE090},
        {"df84", &gLLCCDF84},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_apm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_apm_setworld(uint64_t world) { gGameWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world)); }
void tpw_apm_setinfo(uint64_t info) { gFakeInfo = reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(info)); }

int32_t tpw_apm_layout(int64_t* out) {
    int32_t n = 0;
    RideInfo* info = nullptr;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTrackCells));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mMeshInstance));
    out[n++] = static_cast<int64_t>(sizeof(LLCCRideBlend));
    out[n++] = static_cast<int64_t>(offsetof(LLCCRideBlend, mWidth));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mRotation));
    out[n++] = static_cast<int64_t>(reinterpret_cast<intptr_t>(&info->Value(kRideSlot_WhichTrackType)));
    return n;
}

void tpw_apm_abort() { LLCC_AbortPylonMove(); }
uint32_t tpw_apm_movable(uint64_t ride) { return static_cast<uint32_t>(LLCC_IsRideMovable(reinterpret_cast<CObject*>(static_cast<uintptr_t>(ride)))); }
uint64_t tpw_apm_blend(uint64_t shape) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(LLCC_GetRideBlendInfo(reinterpret_cast<void*>(static_cast<uintptr_t>(shape)))));
}

}  // extern "C"
