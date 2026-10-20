// Test library for the small predicates and accessors of CMapCell, CTrackCell, CLBLobby and the cell system (unit v51060;
// libtpw_unit_CMapCell_IsQueue.dylib, tests/test_CMapCell_IsQueue.py), main app:
//   src/MapDelta_map_CMapCell_IsQueue.cpp   thirty small functions
//
// The callees outside the unit are fakes that record the arguments the real one reads and answer from the test's queue; the globals of
// other files the unit reads or writes are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "MapCell.h"
#include "TrackCell.h"
#include "lobby.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
uint8_t gLLCCE090;
uint16_t gLLCCE092;
int32_t gLLCCHoardMaxY;
int32_t gLLCCHoardMaxX;
int32_t gLLCCHoardMinY;
int32_t gLLCCHoardMinX;

// ---- the fakes: the same event name and arguments as the module's PowerPC stub
uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {lab(this)});
    return static_cast<uint8_t>(ret("GetNeighbours"));
}

int32_t CCellBase::NumberOfHVNeighbours() const {
    ev("NumberOfHVNeighbours", {lab(this)});
    return static_cast<int32_t>(ret("NumberOfHVNeighbours"));
}

const CTrackCell* CCellBase::GetTrackCellEquiv() const {
    ev("GetTrackCellEquiv", {lab(this)});
    return reinterpret_cast<const CTrackCell*>(static_cast<uintptr_t>(ret("GetTrackCellEquiv")));
}

int32_t CTrackCell::IsTrackX() const {
    ev("TrackIsTrackX", {lab(this)});
    return static_cast<int32_t>(ret("TrackIsTrackX"));
}

void LLCCM_DestroyQMesh(uint32_t mesh) {
    ev("DestroyQMesh", {static_cast<int64_t>(mesh)});
}

void LLCCM_DestroyKTrackMesh(uint32_t mesh) {
    ev("DestroyKTrackMesh", {static_cast<int64_t>(mesh)});
}

extern "C" {

uint64_t tpw_cq_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gLLCCE090", &gLLCCE090},       {"gLLCCE092", &gLLCCE092},       {"gLLCCHoardMaxY", &gLLCCHoardMaxY},
        {"gLLCCHoardMaxX", &gLLCCHoardMaxX}, {"gLLCCHoardMinY", &gLLCCHoardMinY}, {"gLLCCHoardMinX", &gLLCCHoardMinX},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets the module relies on (HOST_LAYOUT), measured on a zeroed block; the last is the lobby's flag word on the host.
int32_t tpw_cq_layout(int64_t* out) {
    char* block = static_cast<char*>(tpw::ArenaAllocZeroed(0x400));
    CMapCell* m = reinterpret_cast<CMapCell*>(block);
    CTrackCell* t = reinterpret_cast<CTrackCell*>(block);
    CLBLobby* l = reinterpret_cast<CLBLobby*>(block);
    int32_t n = 0;
    out[n++] = reinterpret_cast<char*>(&m->mMeshInstance) - block;
    out[n++] = reinterpret_cast<char*>(&m->mType) - block;
    out[n++] = reinterpret_cast<char*>(&m->mFlags) - block;
    out[n++] = reinterpret_cast<char*>(&m->mParentID) - block;
    out[n++] = reinterpret_cast<char*>(&m->mPylonIndex) - block;
    out[n++] = reinterpret_cast<char*>(&t->mSegmentNumber) - block;
    out[n++] = reinterpret_cast<char*>(&l->mF1C) - block;
    return n;
}

// One entry for every function: obj is the cell, the lobby or (for the extents) four int32 slots.
uint32_t tpw_cq_call(int32_t op, uint64_t obj, uint32_t arg) {
    CMapCell* m = reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(obj));
    CTrackCell* t = reinterpret_cast<CTrackCell*>(static_cast<uintptr_t>(obj));
    CLBLobby* l = reinterpret_cast<CLBLobby*>(static_cast<uintptr_t>(obj));
    int32_t* slots = reinterpret_cast<int32_t*>(static_cast<uintptr_t>(obj));
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id{};
    id.mIndex = static_cast<uint16_t>(arg >> 16);
    switch (op) {
        case 0: return static_cast<uint32_t>(m->IsQueue());
        case 1: return static_cast<uint32_t>(m->IsPath());
        case 2: return static_cast<uint32_t>(m->IsEntrance());
        case 3: return static_cast<uint32_t>(m->IsExit());
        case 4: return static_cast<uint32_t>(m->IsBridge());
        case 5: return static_cast<uint32_t>(m->IsLake());
        case 6: return static_cast<uint32_t>(m->IsRide());
        case 7: return static_cast<uint32_t>(m->IsGrass());
        case 8: return static_cast<uint32_t>(m->IsWalkable());
        case 9: return static_cast<uint32_t>(m->IsWalkableInFirstPerson());
        case 10: return static_cast<uint32_t>(m->IsNoGo());
        case 11: return static_cast<uint32_t>(m->IsCoasterPylon());
        case 12: return m->IsStraight();
        case 13: return static_cast<uint32_t>(m->IsTrackX());
        case 14: return m->GetFlag();
        case 15: m->SetFlag(static_cast<uint16_t>(arg)); return 0;
        case 16: m->RemoveInstanceMesh(); return 0;
        case 17: return static_cast<uint32_t>(static_cast<int32_t>(m->GetPylonIndex()));
        case 18: m->SetPylonIndex(static_cast<int16_t>(arg)); return 0;
        case 19: m->ModifyParentID(id); return 0;
        case 20: t->ModifyParentID(id); return 0;
        case 21: return static_cast<uint32_t>(t->IsTrack());
        case 22: return static_cast<uint32_t>(t->IsJustTrack());
        case 23: t->SetSegmentNumber(static_cast<int16_t>(arg)); return 0;
        case 24: t->RemoveInstanceMesh(); return 0;
        case 25: t->DeleteMesh(); return 0;
        case 26: return static_cast<uint32_t>(l->IsKeyDown(arg));
        case 27: __sinit_LLCellRide_cpp(); return 0;
        case 28: LLCR_GetHoardingExtents(slots, slots + 1, slots + 2, slots + 3); return 0;
        case 29: LLCC_ResetPylonMove(); return 0;
    }
    return 0;
}

}  // extern "C"
