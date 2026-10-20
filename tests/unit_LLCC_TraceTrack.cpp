// Test library for LLCC_TraceTrack (unit v5988; libtpw_unit_LLCC_TraceTrack.dylib, tests/test_LLCC_TraceTrack.py), main app:
//   src/LLCC_TraceTrack.cpp   LLCC_TraceTrack
//
// Every callee is a fake that records the arguments the real one reads (cells as their index in the grid) and answers from the test's queue.
// The globals of other files the unit reads (gGameWorld, gLLCCDB6C, gLLCMCurrentObject) are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "BMP_InsideArea.h"
#include "CellBase.h"
#include "LLCellSystem.h"
#include "ObjectParts.h"
#include "TrackCell.h"
#include "World.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gGameWorld;
int32_t gLLCCDB6C;
CObject* gLLCMCurrentObject;

namespace {

// The cell's index in the grid (id - 1).
int64_t Idx(const void* cell) {
    return (reinterpret_cast<const char*>(cell) - (reinterpret_cast<const char*>(gGameWorld) + 0x2d8 + 0x110000)) / 0x28;
}

}  // namespace

// ---- the fakes
int32_t CTrackCell::IsTrackX() const {
    ev("IsTrackX", {Idx(this)});
    return static_cast<int32_t>(ret("IsTrackX"));
}

int16_t CTrackCell::GetSegmentNumber() const {
    ev("GetSegmentNumber", {Idx(this)});
    return static_cast<int16_t>(ret("GetSegmentNumber"));
}

uint8_t CCellBase::GetDirection() const {
    ev("GetDirection", {Idx(this)});
    return static_cast<uint8_t>(ret("GetDirection"));
}

int32_t CTrackCell::IsTrack() const {
    ev("IsTrack", {Idx(this)});
    return static_cast<int32_t>(ret("IsTrack"));
}

uint8_t CTrackCell::IsCrossRoad() const {
    ev("IsCrossRoad", {Idx(this)});
    return static_cast<uint8_t>(ret("IsCrossRoad"));
}

uint8_t CTrackCell::IsCorner() const {
    ev("IsCorner", {Idx(this)});
    return static_cast<uint8_t>(ret("IsCorner"));
}

uint8_t CTrackCell::IsStraight() const {
    ev("IsStraight", {Idx(this)});
    return static_cast<uint8_t>(ret("IsStraight"));
}

uint8_t OPPOSITE(uint8_t direction) {
    ev("OPPOSITE", {direction});
    return static_cast<uint8_t>((direction << 4) | (direction >> 4));
}

uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {Idx(this)});
    return static_cast<uint8_t>(ret("GetNeighbours"));
}

void CCellBase::ModifyDirection(uint8_t direction) {
    ev("ModifyDirection", {Idx(this), direction});
}

int32_t CCellBase::NumberOfDirections() const {
    ev("NumberOfDirections", {Idx(this)});
    return static_cast<int32_t>(ret("NumberOfDirections"));
}

int32_t CCellBase::NumberOfNeighbours() const {
    ev("NumberOfNeighbours", {Idx(this)});
    return static_cast<int32_t>(ret("NumberOfNeighbours"));
}

void CTrackCell::Delete(int32_t a, int32_t b, bool c) {
    ev("Delete", {Idx(this), a, b, c ? 1 : 0});
}

void LLCC_DrawTrackLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    ev("DrawTrackLine", {static_cast<int32_t>(mode), x, y, *endX, *endY});
}

CTrackCell* LLCC_BreakTrack(int32_t x, int32_t y, uint8_t dir) {
    ev("BreakTrack", {x, y, dir});
    return nullptr;
}

void BMP_FreeTrack(int32_t ride) {
    ev("BMP_FreeTrack", {ride});
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_tt_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
        {"obj", &gLLCCDB6C},
        {"curobj", &gLLCMCurrentObject},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
// The blocks of the previous case are given back first (the world is big).
static void* gBlocks[8];
static int gBlockCount;

void tpw_tt_reset() {
    for (int i = 0; i < gBlockCount; i++) tpw::ArenaFree(gBlocks[i]);
    gBlockCount = 0;
    gGameWorld = nullptr;
    gLLCCDB6C = 0;
    gLLCMCurrentObject = nullptr;
}

uint64_t tpw_tt_alloc(uint32_t size) {
    void* block = tpw::ArenaAllocZeroed(size);
    if (gBlockCount < 8) gBlocks[gBlockCount++] = block;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_tt_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mCellId));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mType));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mParentID));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    return n;
}

int32_t tpw_tt_run(uint64_t from, uint64_t to, int32_t flag) {
    return LLCC_TraceTrackCount(reinterpret_cast<CTrackCell*>(static_cast<uintptr_t>(from)), reinterpret_cast<CTrackCell*>(static_cast<uintptr_t>(to)), flag);
}

}  // extern "C"
