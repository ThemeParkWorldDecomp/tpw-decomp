// Test library for four CTrackCell methods (unit v5268; libtpw_unit_CTrackCell_IsTrackStartBlock.dylib), main app:
//   src/TrackCell_flags_CTrackCell_IsTrackStartBlock.cpp
//
// CCellBase::GetMapCellEquiv and CTrackCell::IsFlag are recording fakes; gGameWorld is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CellBase.h"
#include "TrackCell.h"
#include "World.h"

using namespace calllog;

CWorld* gGameWorld;

CMapCell* CCellBase::GetMapCellEquiv() {
    ev("GetMapCellEquiv", {lab(this)});
    return nullptr;
}

int32_t CTrackCell::IsFlag(uint16_t flag) const {
    ev("IsFlag", {lab(this), flag});
    return static_cast<int32_t>(ret("IsFlag"));
}

extern "C" {

uint64_t tpw_tsb_addr(const char* name) {
    if (std::strcmp(name, "gGameWorld") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gGameWorld));
    return 0;
}

uint64_t tpw_tsb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_tsb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mType));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mParentID));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mSegmentNumber));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    return n;
}

uint32_t tpw_tsb_start(uint64_t cell) { return static_cast<uint32_t>(reinterpret_cast<CTrackCell*>(cell)->IsTrackStartBlock()); }
uint32_t tpw_tsb_bridge(uint64_t cell) { return static_cast<uint32_t>(reinterpret_cast<CTrackCell*>(cell)->IsBridge()); }
uint32_t tpw_tsb_seg(uint64_t cell) {
    return static_cast<uint32_t>(static_cast<int32_t>(reinterpret_cast<CTrackCell*>(cell)->GetSegmentNumber()));
}
uint32_t tpw_tsb_valid(uint64_t cell, int32_t mode) {
    return static_cast<uint32_t>(reinterpret_cast<CTrackCell*>(cell)->ValidTypeChange(static_cast<CELL_MODES>(mode)));
}

}  // extern "C"
