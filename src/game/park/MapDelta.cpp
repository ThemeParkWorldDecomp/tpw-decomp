// MapDelta.cpp: name proven by __sinit_MapDelta_cpp in the main application.

#include "AdvisorQueue.h"
#include "analyser.h"
#include "CellBase.h"
#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "LbFile.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "RegionEffectCell.h"
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
#include <initializer_list>
#include <new>

// The world's cells (main app of SimTheme Park 2.04b3; the inventory's file names for these are unreliable; unit w113): the three
// cell constructors and the two grid registrations. The six grid globals are defined by the unit's test file and, in the launcher,
// by the generated zero definitions (nothing else reads them).

// 0x10102948 (92 bytes). The word at +0x3c is cleared first (the memset makes that redundant), then all 0x44 bytes are zeroed, then the
// type byte and the halfword 0x423f are set. The halfword is big-endian on the Mac (bytes 0x42 0x3f); here it is the value.
CMapCell::CMapCell() {
    mF3C = 0;
    std::memset(static_cast<void*>(this), 0, sizeof(CMapCell));
    mKind = 1;
    mCellId = 0x423f;
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

uint16_t gRemoveThingNoPrev = 0;    // Mac 0x102a0b26
uint16_t gRemoveThingNoWho = 0;     // Mac 0x102a0b28
uint16_t gRemoveThingNoNext = 0;    // Mac 0x102a0b2a
uint16_t gLitterNoCollector = 0;    // Mac 0x102a0b50
uint16_t gGetObjectThingIdEnd = 0;  // Mac 0x102a0b52
// The five compass offsets of this unit are defined in src/MapDelta.cpp with the other three (__sinit_MapDelta_cpp 0x10103ec0, unit w146).

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

// The Mac's strings, in the string pool at 0x102421ca + offset (each ends with one newline).
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x64
constexpr const char* kLitterMarkRemoved = "**** Removing litter collection mark from (%d, %d); handyman %d has taken too long!\n";   // +0xd1
constexpr const char* kRemovingLitter = "Removing litter from cell %d, %d\n";   // +0x126
constexpr const char* kLitterScriptNull = "**** mLitterScript is NULL!  This should ASSERT! ****\n";   // +0x148
constexpr const char* kNoParticle = "Trying to kill a non-existent particle effect\n(Non-fatal, can safely ignore)";   // +0x17f (no final newline)
constexpr const char* kBadDirection = "Incorrect use of function CMapCell::GetNeighbouringCell( Direction )";   // +0x1cc (no final newline)

CThing* ThingOf(uint16_t id) { return static_cast<CThing*>(gThingArray[id].mThing); }

}  // namespace

// 0x101029c8, 8 bytes.
int32_t CMapCell::HasBeenModifiedFrom(const CMapCell&) const {
    return 1;
}

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

// The names the cell serialiser writes (the Mac's string pool at 0x102421ca + 0, 8, 0x19, 0x27, 0x33, 0x40 and 0x5f). The long at +0x38 is
// written under "mLitterScript" as well.
constexpr const char* kNameLitter = "mLitter";
constexpr const char* kNameLitterCollector = "mLitterCollector";
constexpr const char* kNameLitterScript = "mLitterScript";
constexpr const char* kNamePylonIndex = "mPylonIndex";
constexpr const char* kNameStatusFlags = "mStatusFlags";
constexpr const char* kNameTimeMarked = "mTimeMarkedForLitterCollection";
constexpr const char* kNameWho = "mWho";

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

template<> int32_t LOADVAR<CMapCell::LitterType>(void* file, CMapCell::LitterType* value, const char* name, int32_t noLog);

template<> int32_t SAVEVAR<CMapCell::LitterType>(void* file, CMapCell::LitterType* value, const char* name, int32_t noLog);

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

// The seven thing-loop entry points (unit w123a, main app). External dispatch,
// death-message support, random/season providers and tree insertion remain callees.

// 0x10102d34, 200 bytes. The old head is cached across the second assertion,
// while the next link uses a later head read. Self-insertion preserves that order.
void CMapCell::InsertThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    CThing* const thing = static_cast<CThing*>(gThingArray[id.mIndex].mThing);
    if (mWho != gThingLoopEmptyCellId) {
        const uint16_t oldHead = mWho;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        static_cast<CThing*>(gThingArray[oldHead].mThing)->mCellPrev = id.mIndex;
    }
    thing->mCellNext = mWho;
    thing->mCellPrev = 0;
    mWho = id.mIndex;
}

// 0x10102e54, 356 bytes. The thing's links are read after the row 0 assertion that precedes each access.
void CMapCell::RemoveThing(ThingId id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    CThing* const thing = ThingOf(id.mIndex);
    if (thing->mCellPrev == gRemoveThingNoPrev) {
        mWho = thing->mCellNext;
        if (mWho != gRemoveThingNoWho) {
            const uint16_t who = mWho;
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
            ThingOf(who)->mCellPrev = 0;
        }
    } else {
        const uint16_t prevId = thing->mCellPrev;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        ThingOf(prevId)->mCellNext = thing->mCellNext;
        if (thing->mCellNext != gRemoveThingNoNext) {
            const uint16_t nextId = thing->mCellNext;
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
            ThingOf(nextId)->mCellPrev = thing->mCellPrev;
        }
    }
    thing->mCellPrev = 0;
    thing->mCellNext = 0;
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

// 0x101033a0, 36 bytes.
void CMapCell::MarkForLitterCollection(ThingId collector) {
    mLitterCollector = collector.mIndex;
    mTimeMarkedForLitterCollection = gWorld->mTurn;
}

// 0x10103428, 220 bytes. The third TRACE argument is the word the original leaves in a stack slot: the collector over the cell id.
int32_t CMapCell::MarkedForLitterCollection() {
    if (mLitterCollector == gLitterNoCollector) return 0;
    if (gWorld->mTurn - mTimeMarkedForLitterCollection < 100u) return 1;
    const uint32_t word = (static_cast<uint32_t>(mLitterCollector) << 16) | mCellId;
    const uint32_t i = static_cast<uint32_t>(mCellId) - 1u;
    TRACE(kLitterMarkRemoved, i & 0x7fu, i >> 7, word);
    mLitterCollector = 0;
    mTimeMarkedForLitterCollection = 0;
    return 0;
}

// 0x10103540, 324 bytes.
void CMapCell::RemoveLitter() {
    const uint32_t i = static_cast<uint32_t>(mCellId) - 1u;
    TRACE(kRemovingLitter, i & 0x7fu, i >> 7);
    GAME_ASSERTMSG(mLitterScript != 0, kLitterScriptNull);
    MapId id;
    id.mIndex = mCellId;
    if (mLitter == 7) {
        gWorldMap->RemoveRegionalEffector(kRegionEffectorLitter, id);
    } else if (mLitter == 8) {
        gWorldMap->RemoveRegionalEffector(kRegionEffectorVomit, id);
        if (mLitterEmitter == 0) {
            GAME_ASSERTMSG(0, kNoParticle);
        } else {
            PAR_KillEmitter(mLitterEmitter);
        }
    }
    SCR_KillScript(mLitterScript);
    mLitterScript = 0;
    mLitterCollector = 0;
    mLitter = static_cast<LitterType>(0);
}

// 0x101036b0, 228 bytes. The validity test uses the scaled offset, the cell the unscaled one.
CMapCell* CMapCell::GetNeighbouringCell(const MapDelta* delta, int32_t flag) {
    const MapDelta scaled = (*delta) * flag;
    const uint32_t i = static_cast<uint32_t>(mCellId) - 1u;
    if (!CMap::IsXYValid(static_cast<int32_t>(i & 0x7fu) + scaled.mX, static_cast<int32_t>(i >> 7) + scaled.mY)) return nullptr;
    const uint16_t id = static_cast<uint16_t>(delta->mX + (delta->mY << 7) + mCellId);
    return reinterpret_cast<CMapCell*>(reinterpret_cast<uint8_t*>(gWorld) + 0x2d8 + (static_cast<int32_t>(id) - 1) * 0x44);
}

// 0x10103814, 444 bytes.
CMapCell* CMapCell::GetNeighbouringCell(uint8_t direction) {
    const MapDelta* source;
    switch (direction) {
        case 1: source = &gMapDeltaMinusY; break;
        case 2: source = &gMapDeltaPlusXMinusY; break;
        case 4: source = &gMapDeltaPlusX; break;
        case 8: source = &gMapDeltaPlusXPlusY; break;
        case 0x10: source = &gMapDeltaPlusY; break;
        case 0x20: source = &gMapDeltaMinusXPlusY; break;
        case 0x40: source = &gMapDeltaMinusX; break;
        case 0x80: source = &gMapDeltaMinusXMinusY; break;
        default:
            GAME_ASSERTMSG(0, kBadDirection);
            return nullptr;
    }
    MapDelta local = *source;
    return GetNeighbouringCell(&local, 1);
}

// 0x101037d0, 28 bytes. The original never writes the flag byte; the host writes 0.
MapDelta MapDelta::operator*(int32_t k) const {
    MapDelta result;
    result.mX = mX * k;
    result.mY = mY * k;
    result.mFlag = 0;
    return result;
}

// 0x10103a04, 184 bytes. GetWorld is called on the cell (the original's inline copy ignores `this`); its answer is not used.
ThingId CMapCell::GetObjectThingId() const {
    uint16_t current = mWho;
    while (current != gGetObjectThingIdEnd) {
        (void)reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        const CThing* const thing = ThingOf(current);
        if (thing->mKind == 3) {
            ThingId result;
            result.mIndex = current;
            return result;
        }
        current = thing->mCellNext;
    }
    ThingId none;
    none.mIndex = 0;
    return none;
}

// MapDelta::Rotate (main app, SimTheme Park 2.04b3, MapDelta.cpp; unit w139), 0x10103d88, 272 bytes, and the three compass offsets.
//
// The offset turned for a ride rotated by `rotation` degrees: r = rotation % 360 (C remainder: the sign of the dividend), then 0 gives
// (x, y); 180 or -180 gives (-x, -y); 90 or -270 gives (y, -x); 270 or -90 gives (-y, x); any other value asserts
// "MapDelta::Rotate - illegal angle" (the release assert does nothing) and answers (x, y). The Mac never writes the flag byte of the result
// (the result's third word is whatever was there); this version writes 0.

// __sinit_MapDelta_cpp 0x10103ec0 (436 bytes; the REPORT anchor of unit w146): the static construction of the eight compass offsets, in this order.
// The Mac writes the two words of each (x then y; the flag byte stays 0 in the BSS) and registers a destructor (0x102663d0) for it with
// __register_global_object, which links an 8-byte node after each object (so the objects lie 0x14 bytes apart). The host's constant aggregates
// need no construction and no destructor. Units w139 and w145 held these definitions before; they were moved here to stand in the __sinit order.
const MapDelta gMapDeltaMinusY = {0, -1, 0};        // Mac 0x1034c2ac
const MapDelta gMapDeltaPlusXMinusY = {1, -1, 0};   // Mac 0x1034c298
const MapDelta gMapDeltaPlusX = {1, 0, 0};          // Mac 0x1034c284
const MapDelta gMapDeltaPlusXPlusY = {1, 1, 0};     // Mac 0x1034c270
const MapDelta gMapDeltaPlusY = {0, 1, 0};          // Mac 0x1034c25c
const MapDelta gMapDeltaMinusXPlusY = {-1, 1, 0};   // Mac 0x1034c248
const MapDelta gMapDeltaMinusX = {-1, 0, 0};        // Mac 0x1034c234
const MapDelta gMapDeltaMinusXMinusY = {-1, -1, 0}; // Mac 0x1034c220

namespace {
// neg: wraps for INT_MIN
int32_t Negate(int32_t v) { return static_cast<int32_t>(0u - static_cast<uint32_t>(v)); }
}  // namespace

MapDelta MapDelta::Rotate(int32_t rotation) const {
    const int32_t r = rotation % 360;
    MapDelta result = {0, 0, 0};
    if (r == 0) {
        result.mX = mX;
        result.mY = mY;
    } else if (r == 180 || r == -180) {
        result.mX = Negate(mX);
        result.mY = Negate(mY);
    } else if (r == 90 || r == -270) {
        result.mX = mY;
        result.mY = Negate(mX);
    } else if (r == 270 || r == -90) {
        result.mX = Negate(mY);
        result.mY = mX;
    } else {
        GAME_ASSERTMSG(0, "MapDelta::Rotate - illegal angle");
        result.mX = mX;
        result.mY = mY;
    }
    return result;
}
