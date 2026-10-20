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
#include <cstddef>
#include <cstdint>

#include "ObjectParts.h"
#include "MapCell.h"
#include "TrackCell.h"
#include "TPCamera.h"
#include "World.h"
#include "MainBalance.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "Debug.h"
#include "rsse.h"
#include "RegionEffectCell.h"

uint32_t gMapTimeStamp = 0;   // Mac 0x1034c21c

uint16_t gRemoveThingNoPrev = 0;    // Mac 0x102a0b26
uint16_t gRemoveThingNoWho = 0;     // Mac 0x102a0b28
uint16_t gRemoveThingNoNext = 0;    // Mac 0x102a0b2a
uint16_t gLitterNoCollector = 0;    // Mac 0x102a0b50
uint16_t gGetObjectThingIdEnd = 0;  // Mac 0x102a0b52
uint16_t gMapEmptyCellId = 0;       // Mac 0x102a0a9e

// The five compass offsets of this unit are defined in src/MapDelta.cpp with the other three (__sinit_MapDelta_cpp 0x10103ec0, unit w146).

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

constexpr uint32_t kTrackCellsOffset = 0x110000;
constexpr uint32_t kTimeStampsOffset = 0x1b0000;
constexpr uint32_t kEffectCellsOffset = 0x1b1104;
constexpr uint32_t kRecordsOffset = 0x1d9104;

// The Mac's strings, in the string pool at 0x102421ca + offset (each ends with one newline).
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x64
constexpr const char* kLitterMarkRemoved = "**** Removing litter collection mark from (%d, %d); handyman %d has taken too long!\n";   // +0xd1
constexpr const char* kRemovingLitter = "Removing litter from cell %d, %d\n";   // +0x126
constexpr const char* kLitterScriptNull = "**** mLitterScript is NULL!  This should ASSERT! ****\n";   // +0x148
constexpr const char* kNoParticle = "Trying to kill a non-existent particle effect\n(Non-fatal, can safely ignore)";   // +0x17f (no final newline)
constexpr const char* kBadDirection = "Incorrect use of function CMapCell::GetNeighbouringCell( Direction )";   // +0x1cc (no final newline)

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

CThing* ThingOf(uint16_t id) { return static_cast<CThing*>(gThingArray[id].mThing); }

}  // namespace

// 0x101012f4, 80 bytes.
int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    return x >= 0 && y >= 0 && x < 0x80 && y < 0x80;
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
        reinterpret_cast<const RegionEffectInfo*>(base + kRecordsOffset + static_cast<uint32_t>(effector) * 12u);
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

// 0x101029c8, 8 bytes.
int32_t CMapCell::HasBeenModifiedFrom(const CMapCell&) const {
    return 1;
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

// 0x101037d0, 28 bytes. The original never writes the flag byte; the host writes 0.
MapDelta MapDelta::operator*(int32_t k) const {
    MapDelta result;
    result.mX = mX * k;
    result.mY = mY * k;
    result.mFlag = 0;
    return result;
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
