// Test library for LLCC_GetRideProfileInfo and LLCC_StartPylonMove (unit v5691; libtpw_unit_LLCC_GetRideProfileInfo.dylib,
// tests/test_LLCC_GetRideProfileInfo.py), main app:
//   src/LLCC_Kill_Them_KILL_THEM_ALL_LLCC_AbortPylonMove_LLCC_GetRideProfileInfo.cpp
//
// CCellBase::GetTrackCellEquiv and CMapCell::Delete are fakes that record the cell's offset from the world. The globals of other files
// the unit touches are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CellBase.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "TrackCell.h"
#include "World.h"
#include "RideDB.h"
#include "ui_main.h"

using namespace calllog;

int32_t gLLCCE0B4 = -1;
int32_t gLLCCE0B8 = -1;
uint32_t gLLCCE08C;
uint8_t gLLCCE090;
CWorld* gGameWorld;

namespace {
uint64_t gTrk;  // the track cell the fake answers (an arena block)

int32_t Off(const void* cell) { return static_cast<int32_t>(reinterpret_cast<const char*>(cell) - reinterpret_cast<const char*>(gGameWorld)); }
}  // namespace

CTrackCell* CCellBase::GetTrackCellEquiv() {
    ev("GetTrackCellEquiv", {Off(this)});
    return reinterpret_cast<CTrackCell*>(static_cast<uintptr_t>(gTrk));
}

void CMapCell::Delete(int32_t a, int32_t b) { ev("Delete", {Off(this), a, b}); }

extern "C" {

uint64_t tpw_llp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"tbl", gLLCCDB7C}, {"e0b4", &gLLCCE0B4}, {"e0b8", &gLLCCE0B8}, {"e08c", &gLLCCE08C},
        {"e090", &gLLCCE090}, {"world", &gGameWorld}, {"trk", &gTrk},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_llp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_llp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gLLCCDB7C));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mMeshInstance));
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<RideInfo*>(0x1000)->Value(static_cast<RideBalanceSlot>(3))) - 0x1000);
    return n;
}

uint64_t tpw_llp_profile(uint64_t ride) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(LLCC_GetRideProfileInfo(reinterpret_cast<void*>(static_cast<uintptr_t>(ride)))));
}

void tpw_llp_start(int32_t x, int32_t z, int32_t flag) { LLCC_StartPylonMove(x, z, flag); }

}  // extern "C"
