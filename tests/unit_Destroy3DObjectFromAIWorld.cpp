// Test library for the AI-world cell helpers (unit v5901; libtpw_unit_Destroy3DObjectFromAIWorld.dylib, tests/test_Destroy3DObjectFromAIWorld.py), main app:
//   src/game/park/TPHField.cpp   Destroy3DObjectFromAIWorld, Assign3DObjectToAIWorld, GetWorldGSquareHeight,
//   DetermineInnerBaseInsets, GetRideIDAtCell (and the global gLevelNoThingId)
//
// The callees outside the unit are recording fakes; the globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "CellBase.h"
#include "LevelSystems.h"
#include "TPCoaster.h"
#include "Debug.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "TPEngine.h"
#include "TrackCell.h"
#include "World.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
int32_t gHeightfieldHeight;
int32_t gHeightfieldWidth;
CWorld* gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];
tag_sRideAnimSet* gRideAnimSets[64];

extern uint16_t gLevelNoThingId;

namespace {

alignas(16) uint8_t gTrackBlk[64];

int64_t CellIndex(const void* p) {
    return (static_cast<int64_t>(reinterpret_cast<intptr_t>(p)) - static_cast<int64_t>(reinterpret_cast<intptr_t>(gGameWorld)) - 0x2d8) / 0x44;
}

}  // namespace

// ---- the fakes
int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("IsXYValid", {x, y});
    return static_cast<int32_t>(ret("valid"));
}

CTrackCell* CCellBase::GetTrackCellEquiv() {
    ev("Equiv", {CellIndex(this)});
    return reinterpret_cast<CTrackCell*>(gTrackBlk);
}

void CMapCell::Delete(int32_t a, int32_t b) { ev("CellDelete", {CellIndex(this), a, b}); }

int32_t CMapCell::Modify(CELL_MODES mode, int32_t a, int32_t b) {
    ev("Modify", {CellIndex(this), static_cast<int32_t>(mode), a, b});
    return 0;
}

void CMapCell::ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id) { ev("ParentID", {CellIndex(this), id.mIndex}); }

uint16_t CMapCell::GetFlag() {
    ev("GetFlag", {CellIndex(this)});
    return static_cast<uint16_t>(ret("flag"));
}

void CMapCell::SetFlag(uint16_t flag) { ev("SetFlag", {CellIndex(this), flag}); }

void CTrackCell::Delete(int32_t a, int32_t b, bool c) {
    ev("TrackDelete", {static_cast<int64_t>(reinterpret_cast<uint8_t*>(this) == gTrackBlk), a, b, c ? 1 : 0});
}

uint32_t LLCC_GetTrackMeshAt(int32_t x, int32_t y) {
    ev("Mesh", {x, y});
    return static_cast<uint32_t>(ret("mesh"));
}

void LLCR_OverRide(uint16_t* out, int32_t x, int32_t y) {
    ev("Override", {x, y});
    *out = static_cast<uint16_t>(ret("ovr"));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("Assert", {condition != 0 ? 1 : 0}, {format}); }

float DetermineHFieldHeightQuick(float x, float z) {
    ev("Quick", {fbits(x), fbits(z)});
    uint32_t bits = static_cast<uint32_t>(ret("quick"));
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

extern "C" {

uint64_t tpw_lv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},         {"hw", &gHeightfieldWidth}, {"hh", &gHeightfieldHeight}, {"noid", &gLevelNoThingId},
        {"things", &gThingArray[0]},    {"sets", &gRideAnimSets[0]}, {"trk", gTrackBlk},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// heads, heightmap, mesh header, mesh rotation, thing array entry size
int32_t tpw_lv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeightMap));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF10));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    return n;
}

uint32_t tpw_lv_offset(uint64_t p) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(p))); }

void tpw_lv_destroy(uint32_t a, uint32_t b) { Destroy3DObjectFromAIWorld(a, b); }
void tpw_lv_assign(uint32_t a, uint32_t b, uint32_t c, int32_t d) { Assign3DObjectToAIWorld(nullptr, a, b, c, d); }
uint32_t tpw_lv_height(int32_t x, int32_t y, uint64_t extra) {
    float f = GetWorldGSquareHeight(x, y, reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(extra)));
    uint32_t bits;
    std::memcpy(&bits, &f, 4);
    return bits;
}
void tpw_lv_insets(uint64_t corners, uint64_t points, int32_t n, uint64_t out) {
    DetermineInnerBaseInsets(reinterpret_cast<s2DPoint*>(static_cast<uintptr_t>(corners)), reinterpret_cast<s3DPoint*>(static_cast<uintptr_t>(points)), n,
                             reinterpret_cast<s2DPoint*>(static_cast<uintptr_t>(out)));
}
int32_t tpw_lv_ride(int32_t x, int32_t y) { return GetRideIDAtCell(x, y); }

}  // extern "C"
