// Test library for CTrackCell::CalculateDirections and GetTile (unit v5696; libtpw_unit_CTrackCell_CalculateDirections.dylib,
// tests/test_CTrackCell_CalculateDirections.py), main app:
//   src/TrackCell_flags_CTrackCell_CalculateDirections.cpp
//
// The callees outside the unit are recording fakes; the globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TrackCell.h"
#include "MapCell.h"
#include "World.h"
#include "LLCellSystem.h"
#include "ObjectParts.h"

using namespace calllog;

extern CMapCellTileEntry gTrackTileTableA[16];
extern int32_t gTrackTileCountA;
extern CMapCellTileEntry gTrackTileTableB[16];
extern int32_t gTrackTileCountB;
extern int32_t gTrackTileLast;
extern int32_t gTrackTileRand;

CMapCellTileEntry gTileTableB[49];
int32_t gTileCountB;
int32_t gHoardingRight, gHoardingBottom, gHoardingLeft, gHoardingTop;
uint8_t gLLCC4818;
CWorld* gGameWorld;

namespace {
uint64_t gEqBlock;   // the "map cell equivalent": a block whose +8 is its mode
}

// ---- the fakes
uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {});
    return static_cast<uint8_t>(ret("GetNeighbours"));
}
uint8_t CCellBase::GetDirection() const {
    ev("GetDirection", {});
    return static_cast<uint8_t>(ret("GetDirection"));
}
const CMapCell* CCellBase::GetMapCellEquiv() const {
    ev("GetMapCellEquivC", {});
    return reinterpret_cast<const CMapCell*>(static_cast<uintptr_t>(gEqBlock));
}
CMapCell* CCellBase::GetMapCellEquiv() {
    ev("GetMapCellEquiv", {});
    return reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(gEqBlock));
}
int32_t CCellBase::NumberOfHVNeighbours() const {
    ev("NumberOfHVNeighbours", {});
    return static_cast<int32_t>(ret("NumberOfHVNeighbours"));
}
void CCellBase::ModifyDirection(uint8_t direction) { ev("ModifyDirection", {direction}); }
uint8_t CTrackCell::IsStraight() const {
    ev("IsStraight", {});
    return static_cast<uint8_t>(ret("IsStraight"));
}
uint8_t CTrackCell::IsCorner() const {
    ev("IsCorner", {});
    return static_cast<uint8_t>(ret("IsCorner"));
}
uint8_t CTrackCell::IsCrossRoad() const {
    ev("IsCrossRoad", {});
    return static_cast<uint8_t>(ret("IsCrossRoad"));
}
int32_t CTrackCell::IsBridge() const {
    ev("IsBridge", {});
    return static_cast<int32_t>(ret("IsBridge"));
}
uint8_t OPPOSITE(uint8_t direction) {
    ev("OPPOSITE", {direction});
    return static_cast<uint8_t>(ret("OPPOSITE"));
}
uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("GetRand"));
}

extern "C" {

uint64_t tpw_tcd_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"tableA", gTrackTileTableA},
        {"countA", &gTrackTileCountA},
        {"tableB", gTrackTileTableB},
        {"countB", &gTrackTileCountB},
        {"last", &gTrackTileLast},
        {"rand", &gTrackTileRand},
        {"tableC", gTileTableB},
        {"countC", &gTileCountB},
        {"right", &gHoardingRight},
        {"bottom", &gHoardingBottom},
        {"left", &gHoardingLeft},
        {"top", &gHoardingTop},
        {"flag", &gLLCC4818},
        {"world", &gGameWorld},
        {"eq", &gEqBlock},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_tcd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_tcd_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mType));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mCellId));
    out[n++] = static_cast<int64_t>(sizeof(CMapCellTileEntry));
    out[n++] = static_cast<int64_t>(offsetof(CMapCellTileEntry, mMask));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    return n;
}

void tpw_tcd_calcdir(uint64_t cell, int32_t a, int32_t b) {
    reinterpret_cast<CTrackCell*>(static_cast<uintptr_t>(cell))->CalculateDirections(a, b);
}
uint32_t tpw_tcd_gettile(uint64_t cell, uint64_t rot) {
    return reinterpret_cast<const CTrackCell*>(static_cast<uintptr_t>(cell))->GetTile(reinterpret_cast<int32_t*>(static_cast<uintptr_t>(rot)));
}

}  // extern "C"
