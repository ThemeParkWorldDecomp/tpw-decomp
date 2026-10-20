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
#include <cstddef>
#include <cstdint>

#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "LbFile.h"
#include "MapCell.h"
#include "RegionEffectCell.h"
#include "Serialise.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"
#include "rsse.h"

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

constexpr uint32_t kTrackCellsOffset = 0x110000;
constexpr uint32_t kTimeStampsOffset = 0x1b0000;
constexpr uint32_t kEffectCellsOffset = 0x1b1104;

// The names the cell serialiser writes (the Mac's string pool at 0x102421ca + 0, 8, 0x19, 0x27, 0x33, 0x40 and 0x5f). The long at +0x38 is
// written under "mLitterScript" as well.
constexpr const char* kNameLitter = "mLitter";
constexpr const char* kNameLitterCollector = "mLitterCollector";
constexpr const char* kNameLitterScript = "mLitterScript";
constexpr const char* kNamePylonIndex = "mPylonIndex";
constexpr const char* kNameStatusFlags = "mStatusFlags";
constexpr const char* kNameTimeMarked = "mTimeMarkedForLitterCollection";
constexpr const char* kNameWho = "mWho";

// The Mac's strings (0x10241f0b and following; each ends with a newline except the status byte's name).
constexpr const char* kMapSaveWarning =
    "The save game takes advantage of the region effect grid being the same dimensions as the map grid - this has now changed, and the save system has "
    "broken, though it's easily fixed by changing \t\tthe save status byte\n";   // 0x10241f0b
constexpr const char* kStatusByteName = "save_status_byte";                      // 0x10241fe2
constexpr const char* kSavedMapCells = "\t\tSaved %d (%d %%) map cells as default\n";     // 0x10241ff3
constexpr const char* kSavedTrackCells = "\t\tSaved %d (%d %%) track cells as default\n";   // 0x1024201c
constexpr const char* kSavedEffectCells = "\t\tSaved %d (%d %%) RE cells as default\n";     // 0x10242047
constexpr const char* kMapSaveWrote = "Map save wrote %d bytes\n";                          // 0x1024206f
constexpr const char* kLittering = "Littering cell %d, %d\n";                               // 0x10242271
constexpr const char* kDodgyLitter = "Dodgy litter type\n";                                 // 0x10242288

// The data blocks ESP_SpawnScript takes as its first argument, one per litter kind (1 to 6, 7 and 8 in this order at 0x1029fa00, 0x1029fa24 ...
// 0x1029fafc, 0x24 bytes each, MapDelta.cpp's data: nine words). The words are pointers into the TOC area and small numbers, meaning unknown; the
// first word 0x10266a50 and the third to the eighth are the same in all of them, the second counts 0 to 7 and the last is the block's own Mac
// address. The host gives each an opaque block of the same words under an invented name; ESP_SpawnScript is untranslated.
uint32_t gLitterSpawnBlock1[9] = {0x10266a50, 0, 0x102669d8, 0x10, 0x80000, 0x10266a60, 0, 0x10266958, 0x1029fa00};
uint32_t gLitterSpawnBlock2[9] = {0x10266a50, 1, 0x102669d8, 0x10, 0x80000, 0x10266a60, 0, 0x10266958, 0x1029fa24};
uint32_t gLitterSpawnBlock3[9] = {0x10266a50, 2, 0x102669d8, 0x10, 0x80000, 0x10266a60, 0, 0x10266958, 0x1029fa48};
uint32_t gLitterSpawnBlock4[9] = {0x10266a50, 3, 0x102669d8, 0x10, 0x80000, 0x10266a60, 0, 0x10266958, 0x1029fa6c};
uint32_t gLitterSpawnBlock5[9] = {0x10266a50, 4, 0x102669d8, 0x10, 0x80000, 0x10266a60, 0, 0x10266958, 0x1029fa90};
uint32_t gLitterSpawnBlock6[9] = {0x10266a50, 5, 0x102669d8, 0x10, 0x80000, 0x10266a60, 0, 0x10266958, 0x1029fab4};
uint32_t gLitterSpawnBlock7[9] = {0x10266a50, 6, 0x102669d8, 0x10, 0x80000, 0x10266a60, 0, 0x10266958, 0x1029fad8};
uint32_t gLitterSpawnBlock8[9] = {0x10266a50, 7, 0x102669d8, 0x10, 0x80000, 0x10266a60, 0, 0x10266958, 0x1029fafc};

}  // namespace

// CMapCell::LitterType instances (the enum is four bytes: the same bodies as the int instances).
template<> int32_t LOADVAR<CMapCell::LitterType>(void* file, CMapCell::LitterType* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x10103af0, 236 bytes
}
template<> int32_t SAVEVAR<CMapCell::LitterType>(void* file, CMapCell::LitterType* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x10103c30, 260 bytes
}

// 0x10103010, 844 bytes. Litter lands on the cell: a sprite script of the kind's data block at a random spot of the cell (the world's generator is
// reseeded with the cell id first, so the spot is the same every time), for kind 8 also a particle emitter there. Kinds 7 and 8 add a region
// effector (2 and 5) first. Kind 0 and kinds above 8 assert "Dodgy litter type" (the release assert does nothing) and take the block of kind 6.
void CMapCell::PlaceLitter(LitterType type) {
    if (TheGameType().mType == 1) return;   // an online game
    if (mLitter != 0) return;
    const uint32_t index = static_cast<uint32_t>(mCellId) - 1u;
    CTRACE(2, kLittering, index & 0x7fu, index >> 7);
    uint32_t* block = nullptr;
    switch (static_cast<uint32_t>(type)) {
    case 1: block = gLitterSpawnBlock1; break;
    case 2: block = gLitterSpawnBlock2; break;
    case 3: block = gLitterSpawnBlock3; break;
    case 4: block = gLitterSpawnBlock4; break;
    case 5: block = gLitterSpawnBlock5; break;
    case 6: block = gLitterSpawnBlock6; break;
    case 7: {
        block = gLitterSpawnBlock7;
        MapId id;
        id.mIndex = mCellId;
        gWorldMap->AddRegionalEffector(kRegionEffectorLitter, id);
        break;
    }
    case 8: {
        block = gLitterSpawnBlock8;
        MapId id;
        id.mIndex = mCellId;
        gWorldMap->AddRegionalEffector(kRegionEffectorVomit, id);
        break;
    }
    default:
        GAME_ASSERTMSG(0, kDodgyLitter);
        block = gLitterSpawnBlock6;
        break;
    }
    mLitterScript = ESP_SpawnScript(block, static_cast<ESP_Sprites>(0xb), 0, static_cast<ACTS>(0), 0.0f, 0.0f, 0.0f);
    gWorld->SeedRand(mCellId);
    const uint32_t cellX = (static_cast<uint32_t>(mCellId) - 1u) & 0x7fu;
    const uint32_t first = gWorld->GetRand();
    // (the Mac fuses the multiply and the add; the values are small whole numbers, so one rounding or two give the same float)
    const float x = 10.0f * static_cast<float>(static_cast<int32_t>(cellX)) + (1.0f + static_cast<float>(first % 9u));
    const uint32_t cellY = (static_cast<uint32_t>(mCellId) - 1u) >> 7;
    const uint32_t second = gWorld->GetRand();
    const float z = 10.0f * static_cast<float>(static_cast<int32_t>(cellY)) + (1.0f + static_cast<float>(second % 9u));
    SCR_SetScriptPosition(mLitterScript, x, 0.0f, z, 0);
    mLitterCollector = 0;
    mLitter = type;
    if (static_cast<int32_t>(type) == 8) {
        mLitterEmitter = static_cast<int32_t>(PAR_InitEmitter(0x32, static_cast<int32_t>(1024.0f * x), static_cast<int32_t>(1024.0f * 0.0f),
                                                              static_cast<int32_t>(1024.0f * z)));
    }
}

// 0x10102a10, 756 bytes. The shared part first (its answer is ignored), then for a version of 0 or 1 (signed compares) the eight values in the
// same order whether loading or saving; each answers 1 at once on failure. A version out of range does nothing and answers 0.
int32_t CMapCell::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    CCellBase::Serialise(file, save, version, unused);
    if (version >= 2 || version < 0) return 0;
    ThingId* const collector = reinterpret_cast<ThingId*>(&mLitterCollector);
    ThingId* const who = reinterpret_cast<ThingId*>(&mWho);
    if (save == 0) {
        if (LOADVAR(file, &mLitter, kNameLitter, 0) != 0) return 1;
        if (LOADVARSTRUCT(file, collector, kNameLitterCollector, 0) != 0) return 1;
        // (the Mac's long: four bytes, so the host reads the int32 instance)
        if (LOADVAR(file, &mLitterScript, kNameLitterScript, 0) != 0) return 1;
        if (LOADVAR(file, &mLitterEmitter, kNameLitterScript, 0) != 0) return 1;   // (the same name for +0x38)
        if (LOADVAR(file, &mPylonIndex, kNamePylonIndex, 0) != 0) return 1;
        if (LOADVAR(file, &mStatusFlags, kNameStatusFlags, 0) != 0) return 1;
        if (LOADVAR(file, &mTimeMarkedForLitterCollection, kNameTimeMarked, 0) != 0) return 1;
        if (LOADVARSTRUCT(file, who, kNameWho, 0) != 0) return 1;
    } else {
        if (SAVEVAR(file, &mLitter, kNameLitter, 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, collector, kNameLitterCollector, 0) != 0) return 1;
        if (SAVEVAR(file, &mLitterScript, kNameLitterScript, 0) != 0) return 1;
        if (SAVEVAR(file, &mLitterEmitter, kNameLitterScript, 0) != 0) return 1;
        if (SAVEVAR(file, &mPylonIndex, kNamePylonIndex, 0) != 0) return 1;
        if (SAVEVAR(file, &mStatusFlags, kNameStatusFlags, 0) != 0) return 1;
        if (SAVEVAR(file, &mTimeMarkedForLitterCollection, kNameTimeMarked, 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, who, kNameWho, 0) != 0) return 1;
    }
    return 0;
}

// 0x10100890, 172 bytes.
int32_t CMacroAI::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    CWorld* const world = reinterpret_cast<CWorld*>(this);
    if (world->mStaffDB.Serialise(file, save, version, unused) != 0) return 1;
    if (world->mParkTime.Serialise(file, save, version, unused) != 0) return 1;
    if (world->mArrival.Serialise(file, save, version, unused) != 0) return 1;
    return 0;
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
