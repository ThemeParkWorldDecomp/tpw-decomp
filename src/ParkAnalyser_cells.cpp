// The park analyser's cell and map queries (main app, analyser.cpp range; unit w156): CParkAnalyser::GetAnalyserCell 0x100e4c38 (64 bytes),
// GetNumWalkableCells 0x100e571c, GetNumLitteredCells 0x100e57d4 and GetNumDevelopedCells 0x100e588c (124 bytes each), IsNearbyThingOfType 0x100e6c90
// (44 bytes), GetNearbyLitteredCell 0x100e6d38 (516 bytes), GetPercentageLitteredCells 0x100ed738 (336 bytes), MarkAreaPatrolledBy 0x100ee694 (576 bytes),
// PercentageOfAreaPatrolledBy 0x100ee924 (836 bytes), PercentageOfParkCoveredByCameras 0x100eecd8 (272 bytes), ClearMapData 0x100eee30 (108 bytes),
// NumberOfCellsCoveredByObjects 0x100eeed0 (248 bytes), and CParkAnalyserCell::GetNumWalkableCells 0x100e659c (304 bytes), GetNumLitteredCells
// 0x100e670c (288 bytes) and GetNumDevelopedCells 0x100e686c (448 bytes).
//
// A map cell id is 1 based: x = (id - 1) & 0x7f and y = (id - 1) >> 7 (a 32-bit logical shift, so id 0 gives x 0x7f and y 0x1ffffff); the id of (x, y) is the
// halfword x + (y << 7) + 1 and the cell is the 0x44 byte entry (id - 1) of the map (id 0 reads the entry before it). The functions read different maps:
// the three counters ask the CELL as a CWorldSavvy for GetMap once per cell, GetNearbyLitteredCell reads gWorldMap, GetPercentageLitteredCells the map of
// GetWorld(), and the rest the map of gWorld (at +0x2d8). The full-map loops run x from 0x7f down to 0 (outermost) and y from 0x7f down to 0.
// The patrol mark is CMapCell::mF3C (0 for none, 1 for patrolled).
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "Debug.h"
#include "MapCell.h"
#include "RegionEffectCell.h"
#include "ThingReceivers.h"
#include "TimeSig.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

constexpr uint16_t kNoThing = 0;        // the end of the used list: the halfword at 0x102a0532 (initialised data, all 0 and never written)
constexpr uint16_t kNoCollector = 0;    // the halfword at 0x102a04c4, compared with a cell's litter collector (0, never written; not gLitterNoCollector)
constexpr uint32_t kNearbyCellsAt = 0x1f020;   // CParkAnalyser::mNearbyCells, addressed from `this` (a wide search writes past its 0x24 entries)
const char kEntryZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10240242, as World_turn.cpp
const char kRadiusText[] = "Litter search radius too large.  Reduce search radius or increase size of buffer in CParkAnalyser\n";   // 0x102402a3

// The id of (x, y): the halfword x + (y << 7) + 1 (the arithmetic is the 32-bit word's, cut to 16 bits).
uint16_t CellId(int32_t x, int32_t y) {
    return static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1u);
}

// The cell of `id` in a map: entry (id - 1), 0x44 bytes each (id 0 is the entry before the map).
CMapCell* CellOf(const void* map, uint16_t id) {
    const int32_t offset = (static_cast<int32_t>(id) - 1) * 0x44;
    return reinterpret_cast<CMapCell*>(reinterpret_cast<uintptr_t>(map) + static_cast<intptr_t>(offset));
}

// The sum of the 0x400 analyser cells' counts, from the last cell down.
template <typename Count>
int32_t SumOverCells(const CParkAnalyser* analyser, Count count) {
    uint32_t sum = 0;
    for (int32_t i = 0x3ff; i >= 0; i--) sum += static_cast<uint32_t>(count(analyser->mCells[i]));
    return static_cast<int32_t>(sum);
}

// A Val-cached count over the cell's rectangle of map cells: recounted when the Val has no value or maxAge is below the seconds since its stamp
// (unsigned), the cells' map asked of the analyser cell (as a CWorldSavvy) once per cell; the answer is always the Val's.
template <typename Counted>
int32_t CellCount(const CParkAnalyserCell* cell, const CParkAnalyserVal<int>& val, uint32_t maxAge, Counted counted) {
    if (val.mWorkedOut == 0 || maxAge < static_cast<uint32_t>(reinterpret_cast<const CTimeSig*>(&val)->SecondsSinceLastUpdate())) {
        const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(cell);
        uint32_t total = 0;
        for (int32_t x = cell->mStartX; x < static_cast<int32_t>(static_cast<uint32_t>(cell->mStartX) + static_cast<uint32_t>(cell->mSizeX)); x++) {
            for (int32_t y = cell->mStartY; y < static_cast<int32_t>(static_cast<uint32_t>(cell->mStartY) + static_cast<uint32_t>(cell->mSizeY)); y++) {
                const uint16_t id = CellId(x, y);
                const CMapCell* mapCell = CellOf(savvy->GetMap(), id);
                if (counted(mapCell)) total++;
            }
        }
        const_cast<CParkAnalyserVal<int>&>(val).Set(static_cast<int32_t>(total));
    }
    return val.Get();
}

// percent = hits * 100 / count (signed divide, the product wrapping through 32 bits), 0 when nothing was counted.
int32_t Percent(int32_t hits, int32_t count) {
    if (count == 0) return 0;
    return static_cast<int32_t>(static_cast<uint32_t>(hits) * 100u) / count;
}

// The full-map loop of gWorld's map shared by the percentage and count functions: x from 0x7f down outermost, y from 0x7f down.
template <typename Visit>
void ForEachCellDown(const void* map, Visit visit) {
    for (int32_t x = 0x7f; x >= 0; x--) {
        for (int32_t y = 0x7f; y >= 0; y--) visit(CellId(x, y), CellOf(map, CellId(x, y)));
    }
}

// PercentageOfAreaPatrolledBy's four loops: the cells for which `inArea` answers true are counted, the ones with a positive patrol mark are hits.
template <typename InArea>
int32_t PatrolledPercent(const void* map, InArea inArea) {
    int32_t count = 0;
    int32_t hits = 0;
    ForEachCellDown(map, [&](uint16_t, const CMapCell* cell) {
        if (!inArea(cell)) return;
        count++;
        if (static_cast<int32_t>(cell->mF3C) > 0) hits++;
    });
    return Percent(hits, count);
}

}  // namespace

// ---- the three counters of one analyser cell (0x100e659c, 0x100e670c, 0x100e686c)

// The map cells of the rectangle that are a path or a queue; mVal4C caches the count.
int32_t CParkAnalyserCell::GetNumWalkableCells(uint32_t maxAge) const {
    return CellCount(this, mVal4C, maxAge, [](const CMapCell* cell) { return cell->IsPath() != 0 || cell->IsQueue() != 0; });
}

// The map cells of the rectangle with litter (the litter word is not 0, no call); mVal58 caches the count.
int32_t CParkAnalyserCell::GetNumLitteredCells(uint32_t maxAge) const {
    return CellCount(this, mVal58, maxAge, [](const CMapCell* cell) { return static_cast<int32_t>(cell->mLitter) != 0; });
}

// The map cells of the rectangle that are a path, queue, entrance, exit, bridge or ride (each next predicate only when the last answered 0);
// mVal64 caches the count.
int32_t CParkAnalyserCell::GetNumDevelopedCells(uint32_t maxAge) const {
    return CellCount(this, mVal64, maxAge, [](const CMapCell* cell) {
        return cell->IsPath() != 0 || cell->IsQueue() != 0 || cell->IsEntrance() != 0 || cell->IsExit() != 0 || cell->IsBridge() != 0 ||
               cell->IsRide() != 0;
    });
}

// ---- the analyser

// The analyser cell that covers the map cell: mCells[x / squares + gridX * (y / squares)], two signed divides.
CParkAnalyserCell* CParkAnalyser::GetAnalyserCell(MapId id) const {
    const uint32_t index = static_cast<uint32_t>(id.mIndex) - 1u;
    const int32_t x = static_cast<int32_t>(index & 0x7fu);
    const int32_t y = static_cast<int32_t>(index >> 7);
    const int32_t row = y / mMapSquaresPerCell;
    const int32_t column = x / mMapSquaresPerCell;
    const int32_t cell = static_cast<int32_t>(static_cast<uint32_t>(column) + static_cast<uint32_t>(mGridSizeX) * static_cast<uint32_t>(row));
    const int32_t offset = static_cast<int32_t>(static_cast<uint32_t>(cell) * 0x7cu + 0x20u);
    return reinterpret_cast<CParkAnalyserCell*>(reinterpret_cast<uintptr_t>(this) + static_cast<intptr_t>(offset));
}

int32_t CParkAnalyser::GetNumWalkableCells(uint32_t maxAge) const {
    return SumOverCells(this, [maxAge](const CParkAnalyserCell& cell) { return cell.GetNumWalkableCells(maxAge); });
}

int32_t CParkAnalyser::GetNumLitteredCells(uint32_t maxAge) const {
    return SumOverCells(this, [maxAge](const CParkAnalyserCell& cell) { return cell.GetNumLitteredCells(maxAge); });
}

int32_t CParkAnalyser::GetNumDevelopedCells(uint32_t maxAge) const {
    return SumOverCells(this, [maxAge](const CParkAnalyserCell& cell) { return cell.GetNumDevelopedCells(maxAge); });
}

int32_t CParkAnalyser::IsNearbyThingOfType(CThing::ThingModel model, MapId id, int32_t a) {
    return CountNeighboursOfType(model, id, a, 1);
}

// The litter search: the cells of gWorldMap in the square of `radius` around `id` (clamped at 0 on the low side by a signed test of the 32-bit difference and
// at 0x7f on the high side; x outermost, y inner) that have litter and whose collector is `who` (or the halfword kNoCollector) are listed from this + 0x1f020;
// the answer is the first listed cell with the smallest |dx| + |dy| (an unsigned compare against 9999 to start with), 0 when none.
MapId CParkAnalyser::GetNearbyLitteredCell(ThingId who, MapId id, int32_t radius) {
    GAME_ASSERTMSG(radius <= 5, kRadiusText);
    const uint32_t index = static_cast<uint32_t>(id.mIndex) - 1u;
    const int32_t x = static_cast<int32_t>(index & 0x7fu);
    const int32_t y = static_cast<int32_t>(index >> 7);
    int32_t startX = static_cast<int32_t>(static_cast<uint32_t>(x) - static_cast<uint32_t>(radius));
    if (startX < 0) startX = 0;
    int32_t startY = static_cast<int32_t>(static_cast<uint32_t>(y) - static_cast<uint32_t>(radius));
    if (startY < 0) startY = 0;
    int32_t endX = static_cast<int32_t>(static_cast<uint32_t>(radius) + static_cast<uint32_t>(x));
    if (endX > 0x7f) endX = 0x7f;
    int32_t endY = static_cast<int32_t>(static_cast<uint32_t>(radius) + static_cast<uint32_t>(y));
    if (endY > 0x7f) endY = 0x7f;
    uint8_t* const base = reinterpret_cast<uint8_t*>(this);
    int32_t found = 0;
    for (int32_t cellX = startX; cellX <= endX; cellX++) {
        for (int32_t cellY = startY; cellY <= endY; cellY++) {
            const uint16_t cellId = CellId(cellX, cellY);
            const CMapCell* cell = CellOf(gWorldMap, cellId);
            if (static_cast<int32_t>(cell->mLitter) == 0) continue;
            if (cell->mLitterCollector == who.mIndex || cell->mLitterCollector == kNoCollector) {
                std::memcpy(base + kNearbyCellsAt + 2u * static_cast<uint32_t>(found), &cellId, sizeof(cellId));
                found++;
            }
        }
    }
    uint16_t nearest = 0;
    uint32_t best = 9999;
    for (int32_t i = 0; i < found; i++) {
        uint16_t entry;
        std::memcpy(&entry, base + kNearbyCellsAt + 2u * static_cast<uint32_t>(i), sizeof(entry));
        const int32_t entryY = static_cast<int32_t>((static_cast<uint32_t>(entry) - 1u) >> 7);
        const uint32_t distanceY = static_cast<uint32_t>(std::labs(static_cast<long>(static_cast<int32_t>(static_cast<uint32_t>(entryY) - static_cast<uint32_t>(y)))));
        const int32_t entryX = static_cast<int32_t>((static_cast<uint32_t>(entry) - 1u) & 0x7fu);
        const uint32_t distanceX = static_cast<uint32_t>(std::labs(static_cast<long>(static_cast<int32_t>(static_cast<uint32_t>(entryX) - static_cast<uint32_t>(x)))));
        const uint32_t distance = distanceX + distanceY;
        if (distance < best) {
            best = distance;
            nearest = entry;
        }
    }
    MapId result;
    result.mIndex = nearest;
    return result;
}

// Percent of the cells of GetWorld()'s map that are walkable (a path or a queue) or littered which are littered. The accessor is asked once, first.
int32_t CParkAnalyser::GetPercentageLitteredCells() const {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(this);
    const void* const map = savvy->GetWorld().GetMap();
    int32_t count = 0;
    int32_t hits = 0;
    ForEachCellDown(map, [&](uint16_t, const CMapCell* cell) {
        const bool walkable = cell->IsPath() != 0 || cell->IsQueue() != 0;
        const bool littered = static_cast<int32_t>(cell->mLitter) != 0;
        if (walkable || littered) count++;
        if (littered) hits++;
    });
    return Percent(hits, count);
}

// Clears the patrol mark of every cell of gWorld's map.
void CParkAnalyser::ClearMapData() {
    const void* const map = gWorld->GetMap();
    ForEachCellDown(map, [](uint16_t, CMapCell* cell) { cell->mF3C = 0; });
}

// Marks the patrol areas of the staff of `model`: ClearMapData first, then every used thing (the walk of gWorld, entry 0 of the thing table asserted free
// before each) whose kind is a staff kind (4 to 8) equal to `model` and that has a patrol area marks the cells of the rectangle from the area's start cell to
// its end cell (x outermost; both signed compares; the bounds are those of the cell ids' x and y, so a start right of the end marks nothing).
void CParkAnalyser::MarkAreaPatrolledBy(CThing::ThingModel model) {
    ClearMapData();
    uint16_t cur = gWorld->GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        const uint8_t kind = thing->mKind;
        const bool staff = kind == 4 || kind == 5 || kind == 6 || kind == 8 || kind == 7;
        if (staff && static_cast<int32_t>(model) == static_cast<int32_t>(kind) && reinterpret_cast<CStaff*>(thing)->HasPatrolArea() != 0) {
            const void* const map = gWorld->GetMap();
            MapId start;
            MapId end;
            reinterpret_cast<CStaff*>(thing)->GetPatrolArea(&start, &end);
            const int32_t firstX = static_cast<int32_t>((static_cast<uint32_t>(start.mIndex) - 1u) & 0x7fu);
            for (int32_t x = firstX; x <= static_cast<int32_t>((static_cast<uint32_t>(end.mIndex) - 1u) & 0x7fu); x++) {
                for (int32_t y = static_cast<int32_t>((static_cast<uint32_t>(start.mIndex) - 1u) >> 7);
                     y <= static_cast<int32_t>((static_cast<uint32_t>(end.mIndex) - 1u) >> 7); y++) {
                    CellOf(map, CellId(x, y))->mF3C = 1;
                }
            }
        }
        cur = gWorld->GetNextUsedThing(thing).mIndex;
    }
}

// MarkAreaPatrolledBy(model), then the percent of the cells of one kind of area that are marked (a positive mark): the land (not water, freeform or lake),
// the built cells (path, queue, entrance, exit, bridge, ride, track or coaster pylon), the entrances, or the paths and queues; any other area answers 0.
int32_t CParkAnalyser::PercentageOfAreaPatrolledBy(CThing::ThingModel model, AreaType area) {
    MarkAreaPatrolledBy(model);
    const void* const map = gWorld->GetMap();
    switch (static_cast<int32_t>(area)) {
        case kAreaLand:
            return PatrolledPercent(map, [](const CMapCell* cell) { return cell->IsWater() == 0 && cell->IsFreeform() == 0 && cell->IsLake() == 0; });
        case kAreaBuilt:
            return PatrolledPercent(map, [](const CMapCell* cell) {
                return cell->IsPath() != 0 || cell->IsQueue() != 0 || cell->IsEntrance() != 0 || cell->IsExit() != 0 || cell->IsBridge() != 0 ||
                       cell->IsRide() != 0 || cell->IsTrack() != 0 || cell->IsCoasterPylon() != 0;
            });
        case kAreaEntrance:
            return PatrolledPercent(map, [](const CMapCell* cell) { return cell->IsEntrance() != 0; });
        case kAreaPath:
            return PatrolledPercent(map, [](const CMapCell* cell) { return cell->IsPath() != 0 || cell->IsQueue() != 0; });
        default:
            return 0;
    }
}

// Percent of the land cells (not water, freeform or lake) of gWorld's map whose region effect cell has a positive effect [3].
int32_t CParkAnalyser::PercentageOfParkCoveredByCameras() {
    CMap* const map = gWorld->GetMap();
    int32_t count = 0;
    int32_t hits = 0;
    ForEachCellDown(map, [&](uint16_t id, const CMapCell* cell) {
        if (cell->IsWater() != 0 || cell->IsFreeform() != 0 || cell->IsLake() != 0) return;
        count++;
        MapId effectId;
        effectId.mIndex = id;
        if (map->GetRegionalEffectCell(effectId)->mpEffect[3] > 0) hits++;
    });
    return Percent(hits, count);
}

// The cells of gWorld's map that are an entrance, exit, ride, track or coaster pylon.
int32_t CParkAnalyser::NumberOfCellsCoveredByObjects() {
    const void* const map = gWorld->GetMap();
    int32_t count = 0;
    ForEachCellDown(map, [&](uint16_t, const CMapCell* cell) {
        if (cell->IsEntrance() != 0 || cell->IsExit() != 0 || cell->IsRide() != 0 || cell->IsTrack() != 0 || cell->IsCoasterPylon() != 0) count++;
    });
    return count;
}
