// Test library for CTrackCell::SetHFieldTile, RefreshHFieldTile and QuickDelete (unit v5005; libtpw_unit_CTrackCell_SetHFieldTile.dylib), main app:
//   src/TrackCell_flags_CTrackCell_SetHFieldTile.cpp
//
// The callees outside the unit are fakes that record what the real ones read; pointers are recorded relative to the case's world block gW.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LLCellSystem.h"
#include "ObjectParts.h"
#include "TPEngine.h"
#include "TrackCell.h"
#include "rsse.h"

using namespace calllog;

uint8_t gLLCC4818;
tag_sRideAnimSet* gRideAnimSets[16];
const MapDelta gMapDeltaPlusX = {1, 0, 0};
const MapDelta gMapDeltaPlusXPlusY = {1, 1, 0};
const MapDelta gMapDeltaPlusY = {0, 1, 0};

namespace {
uintptr_t gW = 0;
int64_t Rel(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) - gW); }
}  // namespace

CMapCell* CCellBase::GetMapCellEquiv() {
    ev("GetMapCellEquiv", {Rel(this)});
    return reinterpret_cast<CMapCell*>(gW + 0x100);
}

uint32_t CTrackCell::GetTile(int32_t* rotation) const {
    ev("GetTile", {Rel(this)});
    *rotation = static_cast<int32_t>(ret("Rot"));
    return static_cast<uint32_t>(ret("GetTile"));
}

void LLCCM_DestroyKTrackMesh(uint32_t mesh) { ev("DestroyK", {static_cast<int64_t>(mesh)}); }
void LLCCM_DestroyWTrackMesh(uint32_t mesh) { ev("DestroyW", {static_cast<int64_t>(mesh)}); }
void LLCCM_DestroyHoardingMesh(uint32_t mesh) { ev("DestroyH", {static_cast<int64_t>(mesh)}); }

uint32_t LLCCM_CreateKTrackMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType i, int32_t restore) {
    ev("CreateK", {x, y, static_cast<int32_t>(i.mKind), static_cast<int32_t>(i.mIndex), static_cast<int32_t>(i.mRotation), restore});
    return static_cast<uint32_t>(ret("CreateK"));
}
uint32_t LLCCM_CreateWTrackMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType i, int32_t restore) {
    ev("CreateW", {x, y, static_cast<int32_t>(i.mKind), static_cast<int32_t>(i.mIndex), static_cast<int32_t>(i.mRotation), restore});
    return static_cast<uint32_t>(ret("CreateW"));
}
uint32_t LLCCM_CreateHoardingMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType i, int32_t restore) {
    ev("CreateH", {x, y, static_cast<int32_t>(i.mKind), static_cast<int32_t>(i.mIndex), static_cast<int32_t>(i.mRotation), restore});
    return static_cast<uint32_t>(ret("CreateH"));
}

void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {Rel(set), static_cast<int32_t>(a), static_cast<int32_t>(anim), static_cast<int32_t>(loop),
                          static_cast<int64_t>(fbits(speed)), static_cast<int32_t>(b)});
}

void CCellBase::RemoveNeighbour(uint8_t mask) { ev("RemoveNeighbour", {Rel(this), mask}); }
void CCellBase::ModifyDirection(uint8_t direction) { ev("ModifyDirection", {Rel(this), direction}); }
void CTrackCell::ModifyType(CELL_MODES mode) { ev("ModifyType", {Rel(this), static_cast<int32_t>(mode)}); }

CTrackCell* CTrackCell::GetNeighbouringCell(MapDelta delta, int32_t offset) {
    ev("GetNeighbouringCell", {Rel(this), delta.mX, delta.mY, offset});
    const int32_t v = static_cast<int32_t>(ret("GNC", delta.mX * 2 + delta.mY, 0));
    if (v == 0) return nullptr;
    return reinterpret_cast<CTrackCell*>(gW + (v - 1) * 0x28);
}

extern "C" {

uint64_t tpw_hft_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"flag", &gLLCC4818},
        {"tmp", gLLCC4840},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_hft_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_hft_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mType));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mTileData));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mMeshInstance));
    return n;
}

static void Prepare(uint64_t world) {
    gW = static_cast<uintptr_t>(world);
    for (int i = 0; i < 16; ++i) gRideAnimSets[i] = reinterpret_cast<tag_sRideAnimSet*>(gW + 0x200 + 16 * i);
}

void tpw_hft_set(uint64_t world) {
    Prepare(world);
    reinterpret_cast<CTrackCell*>(gW)->SetHFieldTile();
}
void tpw_hft_refresh(uint64_t world) {
    Prepare(world);
    reinterpret_cast<CTrackCell*>(gW)->RefreshHFieldTile();
}
void tpw_hft_quick(uint64_t world) {
    Prepare(world);
    reinterpret_cast<CTrackCell*>(gW)->QuickDelete();
}

}  // extern "C"
