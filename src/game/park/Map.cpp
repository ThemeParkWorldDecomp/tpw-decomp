// Map.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of Map code, named after it.

#include "analyser.h"
#include "CellBase.h"
#include "ChunkFile.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "ExitErr.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "LbFile.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "RegionEffectCell.h"
#include "RideDB.h"
#include "rsse.h"
#include "Serialise.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TrackCell.h"
#include "World.h"
#include "WorldSavvy.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

// The save and litter half of MapDelta.cpp (main app, SimTheme Park 2.04b3; unit w146): CMapCell::PlaceLitter 0x10103010, the serialisers of the
// map (CMap::Serialise 0x10100e80), the map cell (CMapCell::Serialise 0x10102a10) and the macro layer (CMacroAI::Serialise 0x10100890), and the
// LOADVAR / SAVEVAR instances for CMapCell::LitterType (0x10103af0, 0x10103c30). `this` of CMap::Serialise is the map, CWorld + 0x2d8; of
// CMacroAI::Serialise the world. Raw offsets as in src/Map_init.cpp: map cells 0x44 bytes at +0, track cells 0x28 bytes at +0x110000, the time stamp
// words at +0x1b0000 (0x441 of them), the region effect cells 10 bytes at +0x1b1104.
//
// Quirks kept: CMapCell::Serialise throws away CCellBase::Serialise's answer; the long values at +0x28 and +0x38 are written under the same name
// "mLitterScript" (+0x38 is the litter emitter handle); CMap::Serialise's load path hands the cells' Serialise the literal 0 as `save` while the
// save path hands it `save`; the map's save traces run for a version out of range too (with three zero counts); the load of a version out of range
// answers 0 without clearing the time stamps. The Mac's long is four bytes, the host's eight: the int32 instances serve (same file bytes).

namespace {

constexpr uint32_t kTrackCellsOffset = 0x110000;
constexpr uint32_t kTimeStampsOffset = 0x1b0000;
constexpr uint32_t kEffectCellsOffset = 0x1b1104;

// The Mac's strings (0x10241f0b and following; each ends with a newline except the status byte's name).
constexpr const char* kMapSaveWarning =
    "The save game takes advantage of the region effect grid being the same dimensions as the map grid - this has now changed, and the save system has "
    "broken, though it's easily fixed by changing \t\tthe save status byte\n";   // 0x10241f0b
constexpr const char* kStatusByteName = "save_status_byte";                      // 0x10241fe2
constexpr const char* kSavedMapCells = "\t\tSaved %d (%d %%) map cells as default\n";     // 0x10241ff3
constexpr const char* kSavedTrackCells = "\t\tSaved %d (%d %%) track cells as default\n";   // 0x1024201c
constexpr const char* kSavedEffectCells = "\t\tSaved %d (%d %%) RE cells as default\n";     // 0x10242047
constexpr const char* kMapSaveWrote = "Map save wrote %d bytes\n";                          // 0x1024206f
}  // namespace

// CMapCell::LitterType instances (the enum is four bytes: the same bodies as the int instances).
template<> int32_t LOADVAR<CMapCell::LitterType>(void* file, CMapCell::LitterType* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x10103af0, 236 bytes
}
template<> int32_t SAVEVAR<CMapCell::LitterType>(void* file, CMapCell::LitterType* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x10103c30, 260 bytes
}

// 0x10100890, 172 bytes.
int32_t CMacroAI::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    CWorld* const world = reinterpret_cast<CWorld*>(this);
    if (world->mStaffDB.Serialise(file, save, version, unused) != 0) return 1;
    if (world->mParkTime.Serialise(file, save, version, unused) != 0) return 1;
    if (world->mArrival.Serialise(file, save, version, unused) != 0) return 1;
    return 0;
}

// CMacroAI (main app, unit w129): the world's own macro layer. The world is the CMacroAI (the staff database at +0, the park time at +0x2a0, the golden
// ticket control at +0x2c0 and the arrivals at +0x2c4 are its first 0x2d8 bytes), so `this` is viewed as the world.

// 0x1010096c (76 bytes): the arrivals, the staff database and the park clock are set up, in that order.
void CMacroAI::Init() {
    CWorld* world = reinterpret_cast<CWorld*>(this);
    world->mArrival.Init();
    world->mStaffDB.Init();
    world->mParkTime.Init();
}

// 0x101009dc (136 bytes): the arrivals, the staff database and the park clock are updated, in that order. Then, when the turn is a multiple of 100 (an
// UNSIGNED remainder), the golden tickets are checked. The turn is read from *gGameWorldRef (the pointer at 0x102a460c names gGameWorld), not from `this`
// and not from gWorld, which the staff database and the park clock read through CWorldSavvy::GetWorld.
void CMacroAI::Update() {
    CWorld* world = reinterpret_cast<CWorld*>(this);
    world->mArrival.Update();
    world->mStaffDB.Update();
    world->mParkTime.Update();
    const uint32_t turn = gGameWorldRef->mTurn;
    if (turn % 100 == 0) world->mParkTime.GoldenTickets()->CheckForTickets();
}

// CMap::Init (main app, SimTheme Park 2.04b3, Map.cpp; unit w139), 0x10100a8c, 980 bytes. `this` is the map, CWorld + 0x2d8.
//
// It clears the map cells (0x110000 bytes), the track cells (0xa0000 bytes at +0x110000) and the 0x1104 bytes after them (at +0x1b0000),
// then numbers every cell of the 128 x 128 level: id = x + (y << 7) + 1, kind 1 for the map cell and kind 2 for the track cell with that
// index (the id less one). Then it fills the eight region effect records (0xc bytes each, at +0x1d9104 = CWorld + 0x1d93dc) from the
// balance table: the radius byte first (low byte of the word at Mac offset 0x5d0 + 0x18 i), then the five effect halfwords (the next five
// words, each cut to 16 bits) copied as ten bytes. The pad byte of each record (+0xb) is not written. The Mac also copies an 80 byte zero
// image from 0x102a0aa0 to the stack first; that image is overwritten before use (dead), and is not reproduced.

namespace {

constexpr uintptr_t kTrackCellsOffset_2 = 0x110000;   // CWorld + 0x1102d8 less the map's 0x2d8
constexpr uintptr_t kRunOffset = 0x1b0000;
constexpr uintptr_t kRecordsOffset = 0x1d9104;
constexpr int kBalanceFirstSlot = 370;               // slot = (Mac offset - 8) / 4; 0x5d0 is slot 370

}  // namespace

void CMap::Init() {
    uint8_t* const base = reinterpret_cast<uint8_t*>(this);
    memset(base, 0, 0x110000);
    memset(base + kTrackCellsOffset_2, 0, 0xa0000);
    memset(base + kRunOffset, 0, 0x1104);

    for (int32_t x = 0; x < 128; ++x) {
        for (int32_t y = 0; y < 128; ++y) {
            const uint16_t id = static_cast<uint16_t>(x + (y << 7) + 1);
            CCellBase* const mapCell = reinterpret_cast<CCellBase*>(base + 0x44 * static_cast<uint32_t>(id - 1));
            mapCell->mCellId = id;
            mapCell->mKind = 1;
            CCellBase* const trackCell = reinterpret_cast<CCellBase*>(base + kTrackCellsOffset_2 + 0x28 * static_cast<uint32_t>(id - 1));
            trackCell->mCellId = id;
            trackCell->mKind = 2;
        }
    }

    RegionEffectInfo* const records = reinterpret_cast<RegionEffectInfo*>(base + kRecordsOffset);
    for (int32_t i = 0; i < 8; ++i) {
        records[i].mRadius = static_cast<uint8_t>(gMainBalance.mValues[kBalanceFirstSlot + 6 * i].mInteger);
    }
    for (int32_t i = 0; i < 8; ++i) {
        uint16_t image[5];
        for (int32_t k = 0; k < 5; ++k) {
            image[k] = static_cast<uint16_t>(gMainBalance.mValues[kBalanceFirstSlot + 1 + 6 * i + k].mInteger);
        }
        memcpy(&records[i], image, 10);
    }
}

// 0x10100e80, 1,096 bytes. For each of the 0x4000 cells one status byte says which of the map cell (bit 1), the track cell (bit 2) and the region
// effect cell (bit 4) differ from a fresh cell and follow. Saving makes the fresh cells from three locals: the id (and, for the map cell, the
// attribute byte of the real cell) goes in, Reset puts the rest in order, and the real cell is compared with it.
int32_t CMap::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    uint8_t* const base = reinterpret_cast<uint8_t*>(this);
    CMapCell freshMapCell;
    CTrackCell freshTrackCell;
    CRegionEffectCell freshEffectCell;
    uint32_t mapDefaults = 0;
    uint32_t trackDefaults = 0;
    uint32_t effectDefaults = 0;
    const int32_t start = LbFile_GetPosition(file);
    uint8_t status = 0;
    GAME_ASSERTMSG(1, kMapSaveWarning);
    const bool inRange = version < 2 && version >= 0;
    if (save == 0) {
        if (!inRange) return 0;
        for (int32_t i = 0; i < 0x4000; ++i) {
            CMapCell* const mapCell = reinterpret_cast<CMapCell*>(base + 0x44u * static_cast<uint32_t>(i));
            CTrackCell* const trackCell = reinterpret_cast<CTrackCell*>(base + kTrackCellsOffset + 0x28u * static_cast<uint32_t>(i));
            CRegionEffectCell* const effectCell = reinterpret_cast<CRegionEffectCell*>(base + kEffectCellsOffset + 10u * static_cast<uint32_t>(i));
            if (LOADVAR(file, &status, kStatusByteName, 0) != 0) return 1;
            // (the load path passes the literal 0 as `save`)
            if ((status & 1) != 0 && mapCell->Serialise(file, 0, version, unused) != 0) return 1;
            if ((status & 2) != 0 && trackCell->Serialise(file, 0, version, unused) != 0) return 1;
            if ((status & 4) != 0 && effectCell->Serialise(file, 0, version, unused) != 0) return 1;
        }
        uint32_t* const stamps = reinterpret_cast<uint32_t*>(base + kTimeStampsOffset);
        for (uint32_t i = 0; i < 0x441; ++i) stamps[i] = 0;
        return 0;
    }
    if (inRange) {
        for (int32_t i = 0; i < 0x4000; ++i) {
            status = 0;
            const uint16_t id = static_cast<uint16_t>(i + 1);
            CMapCell* const mapCell = reinterpret_cast<CMapCell*>(base + 0x44u * static_cast<uint32_t>(i));
            freshMapCell.mCellId = id;
            freshMapCell.mStatusFlags = mapCell->mStatusFlags;
            freshMapCell.Reset(false);
            if (mapCell->HasBeenModifiedFrom(freshMapCell) != 0) status = static_cast<uint8_t>(status | 1);
            CTrackCell* const trackCell = reinterpret_cast<CTrackCell*>(base + kTrackCellsOffset + 0x28u * static_cast<uint32_t>(i));
            freshTrackCell.mCellId = id;
            freshTrackCell.Reset();
            if (trackCell->HasBeenModifiedFrom(freshTrackCell) != 0) status = static_cast<uint8_t>(status | 2);
            CRegionEffectCell* const effectCell = reinterpret_cast<CRegionEffectCell*>(base + kEffectCellsOffset + 10u * static_cast<uint32_t>(i));
            if (!(*effectCell == freshEffectCell)) status = static_cast<uint8_t>(status | 4);
            if (SAVEVAR(file, &status, kStatusByteName, 0) != 0) return 1;
            if ((status & 1) != 0) {
                if (mapCell->Serialise(file, save, version, unused) != 0) return 1;
            } else {
                ++mapDefaults;
            }
            if ((status & 2) != 0) {
                if (trackCell->Serialise(file, save, version, unused) != 0) return 1;
            } else {
                ++trackDefaults;
            }
            if ((status & 4) != 0) {
                if (effectCell->Serialise(file, save, version, unused) != 0) return 1;
            } else {
                ++effectDefaults;
            }
        }
    }
    // The counts are halfwords in the traces; the first two percentages are signed divisions by 0x4000 (srawi, addze), the third unsigned (srwi).
    const int32_t mapCount = static_cast<int32_t>(mapDefaults & 0xffffu);
    const int32_t trackCount = static_cast<int32_t>(trackDefaults & 0xffffu);
    const uint32_t effectCount = effectDefaults & 0xffffu;
    CTRACE(2, kSavedMapCells, mapCount, mapCount * 100 / 0x4000);
    CTRACE(2, kSavedTrackCells, trackCount, trackCount * 100 / 0x4000);
    CTRACE(2, kSavedEffectCells, static_cast<int32_t>(effectCount), static_cast<int32_t>(effectCount * 100u >> 14));
    CTRACE(2, kMapSaveWrote, LbFile_GetPosition(file) - start);
    return 0;
}

// The map half of MapDelta.cpp (main app, SimTheme Park 2.04b3; unit w145): CMap's validity test, region effectors, park position getters, time
// stamps, engine refresh walks and the walking distance; the direction helpers; the litter and the thing list of CMapCell. `this` of the CMap
// functions is the map, CWorld + 0x2d8; the cell functions' `this` is a CMapCell.
//
// Cell ids are 1 + x + (y << 7). Layout of the map (offsets from the map): map cells 0x44 bytes each at +0, track cells 0x28 bytes at +0x110000,
// the time stamp grid (33 words wide, one word per 16 by 16 block) at +0x1b0000, the region effect cells (10 bytes each) at +0x1b1104, the
// eight region effect records (0xc bytes each) at +0x1d9104.
//
// Notes on the original's habits, kept: the crossing getters read A's Y for B (CrossingParkSide and CrossingBSSide); GetNeighbouringCell's cell
// uses the UNSCALED delta (the validity test the scaled one); the time stamp and effect arithmetic is done in 32 bits; the six halfword
// sentinels the original compares with are globals (all 0, never written).

uint32_t gMapTimeStamp = 0;   // Mac 0x1034c21c

uint16_t gMapEmptyCellId = 0;       // Mac 0x102a0a9e

// The five compass offsets of this unit are defined in src/MapDelta.cpp with the other three (__sinit_MapDelta_cpp 0x10103ec0, unit w146).

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
constexpr uint32_t kRecordsOffset_2 = 0x1d9104;

// The gMainBalance value slots of the position getters: slot = (Mac offset - 8) / 4.
constexpr int kTicketBoothAX = 341, kTicketBoothAY = 342, kTicketBoothBX = 343, kTicketBoothBY = 344;
constexpr int kBusStopAX = 345, kBusStopAY = 346, kBusStopBX = 347, kBusStopBY = 348;
constexpr int kEntranceAX = 349, kEntranceAY = 350, kEntranceBX = 351, kEntranceBY = 352;
constexpr int kCrossingParkAX = 353, kCrossingParkAY = 354, kCrossingParkBX = 355;
constexpr int kCrossingBSAX = 357, kCrossingBSAY = 358, kCrossingBSBX = 359;

MapId CellIdOf(int32_t x, int32_t y) {
    MapId id;
    id.mIndex = static_cast<uint16_t>(x + (y << 7) + 1);
    return id;
}

}  // namespace

// 0x101012f4, 80 bytes.
int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    return x >= 0 && y >= 0 && x < 0x80 && y < 0x80;
}

// 0x1010136c, 196 bytes.
void CMap::DeleteMapInEngine() {
    uint8_t* const base = reinterpret_cast<uint8_t*>(this);
    for (int32_t y = 0; y < 0x80; ++y) {
        for (int32_t x = 0; x < 0x80; ++x) {
            const int32_t index = static_cast<int32_t>(CellIdOf(x, y).mIndex) - 1;
            reinterpret_cast<CMapCell*>(base + index * 0x44)->RemoveInstanceMesh();
            reinterpret_cast<CTrackCell*>(base + kTrackCellsOffset + index * 0x28)->RemoveInstanceMesh();
        }
    }
}

// 0x10101460, 196 bytes.
void CMap::UpdateMapInEngine() {
    uint8_t* const base = reinterpret_cast<uint8_t*>(this);
    for (int32_t y = 0; y < 0x80; ++y) {
        for (int32_t x = 0; x < 0x80; ++x) {
            const int32_t index = static_cast<int32_t>(CellIdOf(x, y).mIndex) - 1;
            reinterpret_cast<CMapCell*>(base + index * 0x44)->RefreshHFieldTile();
            reinterpret_cast<CTrackCell*>(base + kTrackCellsOffset + index * 0x28)->RefreshHFieldTile();
        }
    }
}

// 0x10101554, 196 bytes.
void CMap::CleanUpPark() {
    uint8_t* const base = reinterpret_cast<uint8_t*>(this);
    for (int32_t y = 0; y < 0x80; ++y) {
        for (int32_t x = 0; x < 0x80; ++x) {
            const int32_t index = static_cast<int32_t>(CellIdOf(x, y).mIndex) - 1;
            CMapCell* const cell = reinterpret_cast<CMapCell*>(base + index * 0x44);
            if (cell->mLitter != 0 && !cell->MarkedForLitterCollection()) cell->RemoveLitter();
        }
    }
}
// 0x10101640, 48 bytes.
CRegionEffectCell* CMap::GetRegionalEffectCell(MapId cell) {
    uint8_t* const base = reinterpret_cast<uint8_t*>(this);
    const uint32_t i = static_cast<uint32_t>(cell.mIndex) - 1u;
    const uint32_t offset = ((i & 0x7fu) + (i & ~0x7fu)) * 10u;
    return reinterpret_cast<CRegionEffectCell*>(base + kEffectCellsOffset + static_cast<int32_t>(offset));
}

// 0x101016cc, 52 bytes.
void CMap::AddRegionalEffector(RegionEffector effector, MapId cell) {
    ModifyRegionalEffector(1, effector, cell);
}

// 0x1010176c, 52 bytes.
void CMap::RemoveRegionalEffector(RegionEffector effector, MapId cell) {
    ModifyRegionalEffector(0, effector, cell);
}

// 0x1010180c, 320 bytes. The bounds and the loops compare signed; the quotient is a signed divide.
void CMap::ModifyRegionalEffector(int32_t add, RegionEffector effector, MapId cell) {
    uint8_t* const base = reinterpret_cast<uint8_t*>(this);
    const uint32_t i = static_cast<uint32_t>(cell.mIndex) - 1u;
    const int32_t x = static_cast<int32_t>(i & 0x7fu);
    const int32_t y = static_cast<int32_t>(i >> 7);
    const int32_t sign = add != 0 ? 1 : -1;
    const RegionEffectInfo* const record =
        reinterpret_cast<const RegionEffectInfo*>(base + kRecordsOffset_2 + static_cast<uint32_t>(effector) * 12u);
    const int32_t radius = record->mRadius;
    int32_t x0 = x - radius;
    if (x0 < 0) x0 = 0;
    int32_t y0 = y - radius;
    if (y0 < 0) y0 = 0;
    int32_t x1 = x + radius;
    if (x1 > 0x7f) x1 = 0x7f;
    int32_t y1 = y + radius;
    if (y1 > 0x7f) y1 = 0x7f;
    for (int32_t cx = x0; cx <= x1; ++cx) {
        for (int32_t cy = y0; cy <= y1; ++cy) {
            MapId id = CellIdOf(cx, cy);
            CRegionEffectCell* const effectCell = GetRegionalEffectCell(id);
            for (int32_t k = 0; k < 5; ++k) {
                const int32_t dx = x - cx;
                const int32_t dy = y - cy;
                const int32_t distance = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) + 1;
                const int16_t value = reinterpret_cast<const int16_t*>(record)[k];
                const int32_t quotient = (sign * static_cast<int32_t>(value)) / distance;
                effectCell->mpEffect[k] = static_cast<int16_t>(effectCell->mpEffect[k] + quotient);
            }
        }
    }
}

// 0x101019bc, 100 bytes.
MapId CMap::GetTicketBoothPos(int32_t which) const {
    if (which == 0) return CellIdOf(gMainBalance.mValues[kTicketBoothAX].mInteger, gMainBalance.mValues[kTicketBoothAY].mInteger);
    return CellIdOf(gMainBalance.mValues[kTicketBoothBX].mInteger, gMainBalance.mValues[kTicketBoothBY].mInteger);
}

// 0x10101a50, 100 bytes.
MapId CMap::GetBusStopPos(int32_t which) const {
    if (which == 0) return CellIdOf(gMainBalance.mValues[kBusStopAX].mInteger, gMainBalance.mValues[kBusStopAY].mInteger);
    return CellIdOf(gMainBalance.mValues[kBusStopBX].mInteger, gMainBalance.mValues[kBusStopBY].mInteger);
}

// 0x10101ae0, 100 bytes.
MapId CMap::GetParkEntrancePos(int32_t which) const {
    if (which == 0) return CellIdOf(gMainBalance.mValues[kEntranceAX].mInteger, gMainBalance.mValues[kEntranceAY].mInteger);
    return CellIdOf(gMainBalance.mValues[kEntranceBX].mInteger, gMainBalance.mValues[kEntranceBY].mInteger);
}

// 0x10101b74, 100 bytes. B reads A's Y.
MapId CMap::GetCrossingParkSidePos(int32_t which) const {
    if (which == 0) return CellIdOf(gMainBalance.mValues[kCrossingParkAX].mInteger, gMainBalance.mValues[kCrossingParkAY].mInteger);
    return CellIdOf(gMainBalance.mValues[kCrossingParkBX].mInteger, gMainBalance.mValues[kCrossingParkAY].mInteger);
}

// 0x10101c0c, 100 bytes. B reads A's Y.
MapId CMap::GetCrossingBSSidePos(int32_t which) const {
    if (which == 0) return CellIdOf(gMainBalance.mValues[kCrossingBSAX].mInteger, gMainBalance.mValues[kCrossingBSAY].mInteger);
    return CellIdOf(gMainBalance.mValues[kCrossingBSBX].mInteger, gMainBalance.mValues[kCrossingBSAY].mInteger);
}

// MapDelta.cpp (main app of SimTheme Park 2.04b3), unit w141: MAP_wall_in_way (0x10101ca4, 1,724 bytes).
//
// Whether a wall (or anything that stops the walk) stands between the square (x, z) and its neighbour in direction dir (0 and 2 along z,
// 1 and 3 along x). The cells are read by raw offsets from the world pointer as src/MapDelta_map.cpp does: map cells 0x44 bytes at world
// + 0x2d8, track cells 0x28 bytes at +0x110000 of that; ids are 1 + x + (z << 7). The predicates are the untranslated cell members, asked
// in the listing's order (each call counts). The edges give 1 first. dir 4 or more leaves the neighbour's id uninitialised on the Mac
// (the stack halfword): 0 here.

uint16_t gEntranceWalkEnd;   // 0x102a0af0, BSS 0

namespace {
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

const CMapCell* MapCellAt(uint16_t id) {
    unsigned char* const base = reinterpret_cast<unsigned char*>(gGameWorldRef) + 0x2d8;
    return reinterpret_cast<const CMapCell*>(base + (static_cast<int32_t>(id) - 1) * 0x44);
}

// The mask of GetNeighbours' byte that blocks a step in direction dir (the else arm is dir 3).
int32_t NeighbourBlocked(uint8_t neighbours, uint8_t dir) {
    const int32_t mask = dir == 0 ? 0x10 : dir == 2 ? 1 : dir == 1 ? 0x40 : 4;
    return (neighbours & mask) == 0;
}
}  // namespace

int32_t MAP_wall_in_way(uint8_t x, uint8_t z, uint8_t dir, NavMode mode) {
    if (x == 0 && dir == 3) return 1;
    if (z == 0 && dir == 0) return 1;
    if (x == 0x7f && dir == 1) return 1;
    if (z == 0x7f && dir == 2) return 1;
    const int32_t m = static_cast<int32_t>(mode);

    const uint16_t thisId = static_cast<uint16_t>(x + z * 128 + 1);
    uint16_t thatId = 0;
    if (dir == 0) thatId = static_cast<uint16_t>((z - 1) * 128 + 1 + x);
    else if (dir == 1) thatId = static_cast<uint16_t>(x + z * 128 + 2);
    else if (dir == 2) thatId = static_cast<uint16_t>((z + 1) * 128 + 1 + x);
    else if (dir == 3) thatId = static_cast<uint16_t>(x + z * 128);

    const CMapCell* const here = MapCellAt(thisId);
    const CMapCell* const that = MapCellAt(thatId);

    if (that->IsPath() != 0) {
        if (here->IsQueue() == 0) return 0;
    }
    if (here->IsRide() != 0) {
        if (that->IsRide() != 0) return 0;
    }
    if (that->IsRide() != 0) return 1;
    if (that->IsWalkable() != 0 && m != 2) return 0;
    if (m == 2) {
        if (that->IsWalkableInFirstPerson() != 0) return 0;
        if (that->IsEntrance() != 0) {
            const CMapCell* const entrance = MapCellAt(that->mParentID);
            if (entrance != nullptr) {
                uint16_t id = entrance->mWho;
                while (id != gEntranceWalkEnd) {
                    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
                    CThing* const thing = static_cast<CThing*>(gThingArray[id].mThing);
                    if (thing->mKind == 3) {
                        CObject* const object = reinterpret_cast<CObject*>(thing);
                        if (IsRideRideable(object, object->mF1C) != 0) {
                            if (object->GetDBInfo()->Value(kRideSlot_CannotRide).mInteger == 0) return 0;
                        }
                        return 1;
                    }
                    id = thing->mCellNext;
                }
            }
        }
    }
    if (that->IsNoGo() != 0) return 1;
    if (that->IsWater() != 0) return 1;
    if (that->IsQueue() != 0) {
        if (here->IsPath() != 0 || here->IsQueue() != 0) return NeighbourBlocked(that->GetNeighbours(), dir);
        return 1;
    }
    if (here->IsQueue() != 0 && that->IsPath() != 0) return NeighbourBlocked(that->GetNeighbours(), dir);
    if (that->IsTrack() != 0) {
        uint16_t trackId = 0;
        if (that->IsTrackX() != 0) trackId = that->GetTrackCellEquiv()->mParentID;
        else trackId = that->mCellId;
        unsigned char* const base = reinterpret_cast<unsigned char*>(gGameWorldRef) + 0x2d8;
        const CTrackCell* const track = reinterpret_cast<const CTrackCell*>(base + 0x110000 + (static_cast<int32_t>(trackId) - 1) * 0x28);
        if (track->IsBridge() == 0) return 1;
    }
    if (here->IsPath() == 0 && here->IsQueue() == 0) return 0;
    if (m == 1 && here->IsPath() != 0 && that->IsPath() == 0 && that->IsQueue() == 0 && that->IsRide() == 0) return 0;
    if (m == 2 && here->IsPath() != 0 && that->IsGrass() != 0) return 0;
    if ((here->IsExit() != 0 || here->IsEntrance() != 0) && that->IsGrass() != 0) {
        if (dir == 3 && here->GetDirection() == 4) return 0;
        if (dir == 1 && here->GetDirection() == 0x40) return 0;
        if (dir == 0 && here->GetDirection() == 0x10) return 0;
        if (dir == 2 && here->GetDirection() == 1) return 0;
    }
    return 1;
}

// 0x10102394, 220 bytes.
int32_t MAP_count_walking_distance(MapId from, MapId to, NavMode mode) {
    wallhug_path path;
    const int32_t f = static_cast<int32_t>(from.mIndex) - 1;
    const int32_t t = static_cast<int32_t>(to.mIndex) - 1;
    path.mStartX = static_cast<uint8_t>(f & 0x7f);
    path.mStartY = static_cast<uint8_t>((f >> 7) & 0xff);
    path.mEndX = static_cast<uint8_t>(t & 0x7f);
    path.mEndY = static_cast<uint8_t>((t >> 7) & 0xff);
    if (wallhug_trivial(&path, mode) == 0x70000000) return -1;
    int32_t sum = 0;
    const int32_t count = path.mCount;
    for (int32_t i = 0; i < count - 1; ++i) {
        const int32_t dy = static_cast<int32_t>(path.mPoints[i].mY) - static_cast<int32_t>(path.mPoints[i + 1].mY);
        const int32_t dx = static_cast<int32_t>(path.mPoints[i].mX) - static_cast<int32_t>(path.mPoints[i + 1].mX);
        sum += (dy < 0 ? -dy : dy) + (dx < 0 ? -dx : dx);
    }
    return sum;
}

// 0x10102500, 44 bytes.
uint8_t OPPOSITE(uint8_t d) {
    if (d == 0) return 0;
    if (d < 0x10) return static_cast<uint8_t>((d << 4) & 0xf0);
    return static_cast<uint8_t>((d >> 4) & 0xff);
}

// 0x1010254c, 80 bytes.
uint8_t ADD_DIRECTION(uint8_t a, uint8_t b) {
    int32_t n = 0;
    int32_t first = a;
    while (static_cast<uint8_t>(first) > 1) {
        first = first >> 1;
        ++n;
    }
    const int32_t v = static_cast<int32_t>(static_cast<uint32_t>(b) << n);
    if (v > 0x80) return static_cast<uint8_t>((v >> 8) & 0xff);
    return static_cast<uint8_t>(v & 0xff);
}

// 0x101025c4, 20 bytes.
uint32_t CMap::MakeNewTimeStamp() {
    ++gMapTimeStamp;
    return gMapTimeStamp;
}

// 0x10102604, 72 bytes.
void CMap::SetTimeStamp(uint32_t stamp, MapId cell) {
    if (cell.mIndex == gMapEmptyCellId) return;
    uint8_t* const base = reinterpret_cast<uint8_t*>(this);
    const uint32_t i = static_cast<uint32_t>(cell.mIndex) - 1u;
    const int32_t index = static_cast<int32_t>((i >> 4) & 7u) + (static_cast<int32_t>(i >> 7) >> 4) * 0x21;
    *reinterpret_cast<uint32_t*>(base + kTimeStampsOffset + static_cast<uint32_t>(index) * 4u) = stamp;
}

// 0x101026a0, 48 bytes.
uint32_t CMap::GetTimeStamp(MapId cell) {
    uint8_t* const base = reinterpret_cast<uint8_t*>(this);
    const uint32_t i = static_cast<uint32_t>(cell.mIndex) - 1u;
    const int32_t index = static_cast<int32_t>((i >> 4) & 7u) + (static_cast<int32_t>(i >> 7) >> 4) * 0x21;
    return *reinterpret_cast<const uint32_t*>(base + kTimeStampsOffset + static_cast<uint32_t>(index) * 4u);
}

// 0x10102724, 24 bytes.
uint32_t CMap::GetTimeStampCell(uint32_t x, uint32_t y) {
    uint8_t* const base = reinterpret_cast<uint8_t*>(this);
    return *reinterpret_cast<const uint32_t*>(base + kTimeStampsOffset + (x + y * 0x21u) * 4u);
}

// The chunk file reader and the attribute map load (main app of SimTheme Park 2.04b3; the inventory files these under
// Entertainer.cpp, lobby.cpp, MapDelta.cpp and World.cpp, unreliable; unit w112c): CChunkFileReader, its handler classes, the list
// members it uses and CMap::LoadAttribMap, which reads the level's attribute map ("Base.map") into the cells' attribute bytes.

namespace {
// The main balance's value slots of the height field's start (the Mac's byte offsets 0x544 and 0x548 less the table's 8 bytes of
// header, over four, equal Init's slot numbers 335 and 336 for the rows HeightfieldXStart and HeightfieldYStart).
constexpr int kBalanceHeightfieldXStart = 335;
constexpr int kBalanceHeightfieldYStart = 336;

}  // namespace

// 0x1010276c (420 bytes). The data is the chunk's: two extents at words 0 and 1 (file order, little endian), words 2 to 6 unused,
// the bytes from +0x1c. The outer loop runs to the SECOND word and subtracts the x start from its counter, the inner loop runs to
// the first and subtracts the y start; a byte is stored only for a cell inside 0 to 0x7f in both, and the source moves on for every
// byte. All compares are signed, the subtractions wrap.
void CMapChunkHandler::ParseChunk(void* data, uint32_t size) {
    const uint8_t* source = static_cast<const uint8_t*>(data) + 0x1c;
    uint32_t extent0;
    uint32_t extent1;
    std::memcpy(&extent0, data, 4);
    std::memcpy(&extent1, static_cast<const uint8_t*>(data) + 4, 4);
    const int32_t xStart = gMainBalance.mValues[kBalanceHeightfieldXStart].mInteger;
    const int32_t yStart = gMainBalance.mValues[kBalanceHeightfieldYStart].mInteger;
    CMapCell* const cells = reinterpret_cast<CMapCell*>(mMap);
    for (int32_t outer = 0; outer < static_cast<int32_t>(extent1); ++outer) {
        for (int32_t inner = 0; inner < static_cast<int32_t>(extent0); ++inner) {
            const int32_t cellX = static_cast<int32_t>(static_cast<uint32_t>(outer) - static_cast<uint32_t>(xStart));
            const int32_t cellY = static_cast<int32_t>(static_cast<uint32_t>(inner) - static_cast<uint32_t>(yStart));
            if (cellX >= 0 && cellX < 0x80 && cellY >= 0 && cellY < 0x80) {
                const uint16_t place = static_cast<uint16_t>(static_cast<uint32_t>(cellX) + (static_cast<uint32_t>(cellY) << 7) + 1u);
                cells[place - 1].mStatusFlags = *source;
            }
            ++source;
        }
    }
}
