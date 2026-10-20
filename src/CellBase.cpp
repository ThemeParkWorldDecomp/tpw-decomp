// CCellBase (main app, SimTheme Park 2.04b3; the inventory files its code under Entertainer.cpp, as CellBase.h line 1 says: unreliable; unit w179):
// the neighbour, direction and overlap bookkeeping of a cell, the four equivalents (the track and map cell a cell stands for), the 0x24 bytes both
// cell classes serialise (CCellBase::Serialise) and the instances it needs: LOADVAR / SAVEVAR of CELL_MODES (0x100f52a0, 0x100f577c) and
// LOADVARSTRUCT / SAVEVARSTRUCT of CCellBase::CELL_IndexType (0x100f4f0c, 0x100f4d70).
//
// Quirks kept: the neighbour byte is the hoarding mask (+0x22) when the cell system's flag byte gLLCC4818 is nonzero AND mKind is 2, else mNeighbours
// (+0xc) (as RemoveNeighbour in src/MapReset.cpp); the const equivalents ask CWorldSavvy::GetMap ON THE CELL (r3 is still `this`: the answer is
// the world's map whatever the cell is), the non-const ones read gWorldMap; an id of 0 lands one cell before the grid (the Mac's 32-bit add);
// SetOverlapCounter stores only the low halfword and DecOverlapCounter wraps (lha / sth); CCellBase::Serialise's order is the same loading and
// saving (nine values, noLog 0), and it answers 0 for a version outside 0 and 1 without touching the file.
//
// The four TOC words 0x102a05b8, 0x102a05c0, 0x102a05cc and 0x102a05d4 the instances read are each instance's copy of the file layer's success
// value (initial 0, never written): the shared bodies' .Succeeded() covers them, so nothing is declared for them.
#include <cstddef>
#include <cstdint>

#include "CellBase.h"
#include "Debug.h"
#include "LLCellSystem.h"
#include "Serialise.h"
#include "WorldSavvy.h"
#include "World.h"
#include "analyser.h"

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

constexpr int32_t kMapCellSize = 0x44;
constexpr int32_t kTrackCellSize = 0x28;
constexpr int32_t kTrackCellsOffset = 0x110000;   // the track grid lies this many bytes after the map's first cell

// The byte the neighbour functions work on: the hoarding mask for a track cell (kind 2) while the cell system's flag is set, else mNeighbours.
inline uint8_t SelectedNeighbours(const CCellBase* cell) {
    if (gLLCC4818 != 0 && cell->mKind == 2) return cell->mHoardingNeighbours;
    return cell->mNeighbours;
}

// The names CCellBase::Serialise writes (the Mac's string pool at 0x10240b16 + offset).
constexpr const char* kNameDirection = "mDirection";                 // +0
constexpr const char* kNameFlags = "mFlags";                         // +0xb
constexpr const char* kNameMeshInstance = "mMeshInstance";           // +0x12
constexpr const char* kNameNeighbours = "mNeighbours";               // +0x20
constexpr const char* kNameOverlapCounter = "mOverlapCounter";       // +0x2c
constexpr const char* kNameParentID = "mParentID";                   // +0x3c
constexpr const char* kNameTileData = "mTileData";                   // +0x46
constexpr const char* kNameType = "mType";                           // +0x50
constexpr const char* kNameHoardingNeighbours = "mHoardingNeighbours";   // +0x56

}  // namespace

// 0x10081608, 60 bytes: ORs the mask into the selected neighbour byte.
void CCellBase::ModifyNeighbours(uint8_t mask) {
    if (gLLCC4818 != 0 && mKind == 2) {
        mHoardingNeighbours = static_cast<uint8_t>(mHoardingNeighbours | mask);
        return;
    }
    mNeighbours = static_cast<uint8_t>(mNeighbours | mask);
}

// 0x100816e8, 44 bytes.
uint8_t CCellBase::GetNeighbours() const { return SelectedNeighbours(this); }

// 0x10081744, 152 bytes: the eight bits of the selected byte, one test each.
int32_t CCellBase::NumberOfNeighbours() const {
    const uint8_t bits = SelectedNeighbours(this);
    int32_t count = 0;
    if ((bits & 0x01) != 0) count++;
    if ((bits & 0x02) != 0) count++;
    if ((bits & 0x04) != 0) count++;
    if ((bits & 0x08) != 0) count++;
    if ((bits & 0x10) != 0) count++;
    if ((bits & 0x20) != 0) count++;
    if ((bits & 0x40) != 0) count++;
    if ((bits & 0x80) != 0) count++;
    return count;
}

// 0x1008184c, 16 bytes (andc).
void CCellBase::RemoveDirection(uint8_t direction) { mDirection = static_cast<uint8_t>(mDirection & ~direction); }

// 0x10081890, 104 bytes: only the horizontal and vertical neighbours, the bits 0x01 (up), 0x04 (right), 0x10 (down) and 0x40 (left).
int32_t CCellBase::NumberOfHVNeighbours() const {
    const uint8_t bits = SelectedNeighbours(this);
    int32_t count = 0;
    if ((bits & 0x01) != 0) count++;
    if ((bits & 0x04) != 0) count++;
    if ((bits & 0x10) != 0) count++;
    if ((bits & 0x40) != 0) count++;
    return count;
}

// 0x10081930, 8 bytes.
uint8_t CCellBase::GetDirection() const { return mDirection; }

// 0x10081968, 60 bytes: the bits 0x01, 0x04, 0x10 and 0x40 of the direction byte.
int32_t CCellBase::NumberOfDirections() const {
    int32_t count = 0;
    if ((mDirection & 0x01) != 0) count++;
    if ((mDirection & 0x04) != 0) count++;
    if ((mDirection & 0x10) != 0) count++;
    if ((mDirection & 0x40) != 0) count++;
    return count;
}

// 0x100819d8, 16 bytes (lha, addi -1, sth: 0x8000 wraps to 0x7fff).
void CCellBase::DecOverlapCounter() { mOverlapCounter = static_cast<int16_t>(mOverlapCounter - 1); }

// 0x10081a1c, 12 bytes (extsh, sth): the low halfword.
void CCellBase::SetOverlapCounter(int32_t count) { mOverlapCounter = static_cast<int16_t>(count); }

// 0x10081a5c, 8 bytes (lha).
int16_t CCellBase::GetOverlapCounter() const { return mOverlapCounter; }

// The equivalents. The cell id is 1 based; the offset is computed as a signed 32-bit number (id 0 gives one cell before the grid).
// 0x100f4844, 76 bytes: the CONST form calls CWorldSavvy::GetMap on the cell itself.
const CTrackCell* CCellBase::GetTrackCellEquiv() const {
    const uint16_t id = mCellId;
    const CMap* const map = reinterpret_cast<const CWorldSavvy*>(this)->GetMap();
    const int32_t offset = (static_cast<int32_t>(id) - 1) * kTrackCellSize + kTrackCellsOffset;
    return reinterpret_cast<const CTrackCell*>(reinterpret_cast<const uint8_t*>(map) + offset);
}

// 0x100f48c4, 48 bytes: the non-const form reads gWorldMap.
CTrackCell* CCellBase::GetTrackCellEquiv() {
    const uint16_t id = mCellId;
    const int32_t offset = (static_cast<int32_t>(id) - 1) * kTrackCellSize + kTrackCellsOffset;
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<uint8_t*>(gWorldMap) + offset);
}

// 0x100f4928, 68 bytes: the CONST form.
const CMapCell* CCellBase::GetMapCellEquiv() const {
    const uint16_t id = mCellId;
    const CMap* const map = reinterpret_cast<const CWorldSavvy*>(this)->GetMap();
    const int32_t offset = (static_cast<int32_t>(id) - 1) * kMapCellSize;
    return reinterpret_cast<const CMapCell*>(reinterpret_cast<const uint8_t*>(map) + offset);
}

// 0x100f49a0, 44 bytes.
CMapCell* CCellBase::GetMapCellEquiv() {
    const uint16_t id = mCellId;
    const int32_t offset = (static_cast<int32_t>(id) - 1) * kMapCellSize;
    return reinterpret_cast<CMapCell*>(reinterpret_cast<uint8_t*>(gWorldMap) + offset);
}

// The tile record of a cell: three words, each reversed on the way to and from the file (the listings: stwbrx, and the rlwinm / rlwimi quartets).
static void SwapForFile(CCellBase::CELL_IndexType& tile) {
    tile.mKind = serialise::SwapWord(tile.mKind);
    tile.mIndex = serialise::SwapWord(tile.mIndex);
    tile.mRotation = serialise::SwapWord(tile.mRotation);
}
static void UnswapForFile(CCellBase::CELL_IndexType& tile) {
    tile.mKind = serialise::UnswapWord(tile.mKind);
    tile.mIndex = serialise::UnswapWord(tile.mIndex);
    tile.mRotation = serialise::UnswapWord(tile.mRotation);
}

template<> int32_t LOADVARSTRUCT<CCellBase::CELL_IndexType>(void* file, CCellBase::CELL_IndexType* value, const char* name, int32_t noLog) {
    return serialise::LoadStruct(file, value, name, noLog);   // 0x100f4f0c, 260 bytes
}
template<> int32_t SAVEVARSTRUCT<CCellBase::CELL_IndexType>(void* file, CCellBase::CELL_IndexType* value, const char* name, int32_t noLog) {
    return serialise::SaveStruct(file, value, name, noLog);   // 0x100f4d70, 312 bytes
}
template<> int32_t LOADVAR<CELL_MODES>(void* file, CELL_MODES* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x100f52a0, 236 bytes
}
template<> int32_t SAVEVAR<CELL_MODES>(void* file, CELL_MODES* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x100f577c, 260 bytes
}

// 0x100f49fc, 836 bytes. Only for 0 <= version < 2 (signed compares): nine values in one order, loading (save == 0) or saving; each answers 1 at
// once when its instance answers nonzero. Anything else answers 0 without touching the file.
int32_t CCellBase::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    (void)unused;
    if (version >= 2 || version < 0) return 0;
    MacULong* const meshInstance = reinterpret_cast<MacULong*>(&mMeshInstance);
    MapId* const parentId = reinterpret_cast<MapId*>(&mParentID);
    CELL_IndexType* const tileData = reinterpret_cast<CELL_IndexType*>(mTileData);
    if (save == 0) {
        if (LOADVAR(file, &mDirection, kNameDirection, 0) != 0) return 1;
        if (LOADVAR(file, &mFlags, kNameFlags, 0) != 0) return 1;
        if (LOADVAR(file, meshInstance, kNameMeshInstance, 0) != 0) return 1;
        if (LOADVAR(file, &mNeighbours, kNameNeighbours, 0) != 0) return 1;
        if (LOADVAR(file, &mOverlapCounter, kNameOverlapCounter, 0) != 0) return 1;
        if (LOADVARSTRUCT(file, parentId, kNameParentID, 0) != 0) return 1;
        if (LOADVARSTRUCT(file, tileData, kNameTileData, 0) != 0) return 1;
        if (LOADVAR(file, &mType, kNameType, 0) != 0) return 1;
        if (LOADVAR(file, &mHoardingNeighbours, kNameHoardingNeighbours, 0) != 0) return 1;
    } else {
        if (SAVEVAR(file, &mDirection, kNameDirection, 0) != 0) return 1;
        if (SAVEVAR(file, &mFlags, kNameFlags, 0) != 0) return 1;
        if (SAVEVAR(file, meshInstance, kNameMeshInstance, 0) != 0) return 1;
        if (SAVEVAR(file, &mNeighbours, kNameNeighbours, 0) != 0) return 1;
        if (SAVEVAR(file, &mOverlapCounter, kNameOverlapCounter, 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, parentId, kNameParentID, 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, tileData, kNameTileData, 0) != 0) return 1;
        if (SAVEVAR(file, &mType, kNameType, 0) != 0) return 1;
        if (SAVEVAR(file, &mHoardingNeighbours, kNameHoardingNeighbours, 0) != 0) return 1;
    }
    return 0;
}
