// Test library for LLCR_StartQueue and LLCR_StartTrack (unit v5998; libtpw_unit_LLCR_StartQueue.dylib, tests/test_LLCR_StartQueue.py), main app:
//   src/LLCR_OverRide_LLCR_OverRide_3_LLCR_StartQueue.cpp
//
// The callees are fakes that record the arguments the real ones read (cell pointers as the offset from the world) and answer from the queue.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LLCellSystem.h"
#include "World.h"
#include "MapCell.h"
#include "ObjectParts.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
CWorld* gGameWorld;
int32_t gLLCCFirstBlockX;
int32_t gLLCCFirstBlockY;
int32_t gLLCCDB44;
uint8_t gLLCCE0CC;
int32_t gLLCRPlaceRotation;

namespace {
int64_t Off(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(gGameWorld)); }
}  // namespace

int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("IsXYValid", {x, y});
    return static_cast<int32_t>(ret("IsXYValid"));
}

uint8_t OPPOSITE(uint8_t direction) {
    ev("OPPOSITE", {direction});
    return static_cast<uint8_t>(ret("OPPOSITE"));
}

void CCellBase::ModifyNeighbours(uint8_t mask) { ev("ModifyNeighbours", {Off(this), mask}); }

int32_t CMapCell::ValidTypeChange(CELL_MODES mode) {
    ev("ValidTypeChange", {Off(this), static_cast<int32_t>(mode)});
    return static_cast<int32_t>(ret("ValidTypeChange"));
}

void CCellBase::ModifyDirection(uint8_t direction) { ev("ModifyDirection", {Off(this), direction}); }

void LLCC_DrawTrackLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    ev("DrawTrackLine", {static_cast<int32_t>(mode), x, y, *endX, *endY});
    *endX += 1;
    *endY += 1;
}

extern "C" {

uint64_t tpw_llq_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
        {"firstx", &gLLCCFirstBlockX},
        {"firsty", &gLLCCFirstBlockY},
        {"db44", &gLLCCDB44},
        {"e0cc", &gLLCCE0CC},
        {"rot", &gLLCRPlaceRotation},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_llq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_llq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMapCells));
    out[n++] = static_cast<int64_t>(0x4000 * sizeof(CMapCell) - 0x4000 * sizeof(CMapCell) + 0x110000);
    return n;
}

uint64_t tpw_llq_q(uint64_t prr, uint64_t x, uint64_t y) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        LLCR_StartQueue(reinterpret_cast<LLCR_PRR*>(prr), reinterpret_cast<int32_t*>(x), reinterpret_cast<int32_t*>(y))));
}

uint64_t tpw_llq_t(uint64_t prr, uint64_t x, uint64_t y, int32_t track) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        LLCR_StartTrack(reinterpret_cast<LLCR_PRR*>(prr), reinterpret_cast<int32_t*>(x), reinterpret_cast<int32_t*>(y), track)));
}

}  // extern "C"
