// Differential library for unit w156, the park analyser's cell and map queries (src/ParkAnalyser_cells.cpp): the three counters of CParkAnalyserCell, the
// park sums, GetAnalyserCell, IsNearbyThingOfType, GetNearbyLitteredCell, GetPercentageLitteredCells, ClearMapData, MarkAreaPatrolledBy,
// PercentageOfAreaPatrolledBy, PercentageOfParkCoveredByCameras and NumberOfCellsCoveredByObjects.
//
// ONE static block (gRegion, 8 MB) holds everything the functions touch, at the offsets the PowerPC side uses in its own mapped region (RB + offset): the
// trace, the region effect table, the analyser, eight thing blocks and four world-sized slots (W1 the world GetWorld answers, W2 gWorld, W3 the block gWorldMap
// points into, W4 the map GetMap answers; each map at slot + 0x2d8). Every fake appends (tag, value) to a trace; a pointer is recorded as its offset in the
// block, so the two sides' traces compare word for word. The CMapCell predicates answer from bits of the cell's halfword at +0x42 (tag 1 to 11, bit tag - 1):
// the value is the case's `truthy` word when the bit is set. The analyser value's Get answers from the Val's offset and the case's seed. The walk of the used
// things, the staff patrol areas and CountNeighboursOfType's answer come from the case.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "MapCell.h"
#include "RegionEffectCell.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "TimeSig.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"

namespace {

constexpr uint32_t kTrace = 0x100;
constexpr uint32_t kTraceCap = 0x30000;
constexpr uint32_t kEffect = 0x1A0000;
constexpr uint32_t kAnalyser = 0x1B0000;
constexpr uint32_t kThings = 0x1F0000;
constexpr uint32_t kW1 = 0x200000;
constexpr uint32_t kW2 = 0x3E0000;
constexpr uint32_t kW3 = 0x500000;
constexpr uint32_t kW4 = 0x620000;
constexpr uint32_t kRegionSize = 0x800000;

alignas(4096) unsigned char gRegion[kRegionSize];
uint32_t gTraceTag[kTraceCap];
uint32_t gTraceVal[kTraceCap];
uint32_t gTraceN;

int32_t gTruthy = 1;
uint32_t gValSeed;
int32_t gCountAnswer;
uint16_t gWalk[64];
int32_t gWalkN;
int32_t gWalkAt;
int32_t gHas[8];
uint16_t gStart[8];
uint16_t gEnd[8];

uint32_t Off(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(gRegion)); }

void Tr(uint32_t tag, uint32_t value) {
    if (gTraceN < kTraceCap) {
        gTraceTag[gTraceN] = tag;
        gTraceVal[gTraceN] = value;
    }
    gTraceN++;
}

uint32_t TextHash(const char* text) {
    uint32_t h = 0;
    for (; *text; text++) h = h * 31u + static_cast<uint8_t>(*text);
    return h;
}

// The predicate of tag 1..11: records the cell, answers the case's truthy word when bit (tag - 1) of the cell's halfword at +0x42 is set.
int32_t Pred(uint32_t tag, const CMapCell* cell) {
    Tr(tag, Off(cell));
    return (cell->mF42 & (1u << (tag - 1))) != 0 ? gTruthy : 0;
}

int32_t BlockOf(const void* thing) {
    const uint32_t off = Off(thing);
    if (off < kThings || off >= kThings + 8 * 0x100) return -1;
    return static_cast<int32_t>((off - kThings) / 0x100);
}

CParkAnalyser* Analyser() { return reinterpret_cast<CParkAnalyser*>(gRegion + kAnalyser); }
using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

}  // namespace

// ---- the unit's globals (the launcher gets zero definitions)
CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gWorld;
CMap* gWorldMap;

// ---- the fakes (the same tags and values as the PowerPC side's stubs)
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    Tr(20, static_cast<uint32_t>(condition));
    Tr(21, TextHash(format));
}

CWorld& CWorldSavvy::GetWorld() const {
    Tr(22, Off(this));
    return *reinterpret_cast<CWorld*>(gRegion + kW1);
}

CMap* CWorldSavvy::GetMap() const {
    Tr(23, Off(this));
    return reinterpret_cast<CMap*>(gRegion + kW4 + 0x2d8);
}

ThingId CWorld::GetUsedThingHead() const {
    Tr(27, Off(this));
    gWalkAt = 0;
    ThingId result;
    result.mIndex = gWalkN > 0 ? gWalk[0] : 0;
    return result;
}

ThingId CWorld::GetNextUsedThing(const CThing* thing) const {
    Tr(28, Off(this));
    Tr(29, Off(thing));
    gWalkAt++;
    ThingId result;
    result.mIndex = gWalkAt < gWalkN ? gWalk[gWalkAt] : 0;
    return result;
}

template <typename T>
T CParkAnalyserVal<T>::Get() const {
    const uint32_t off = Off(this);
    Tr(24, off);
    return static_cast<T>((off + 1u) * 0x9E3779B1u ^ gValSeed);
}
template <typename T>
void CParkAnalyserVal<T>::Set(T value) {
    Tr(25, Off(this));
    Tr(26, static_cast<uint32_t>(value));
}
template int CParkAnalyserVal<int>::Get() const;
template void CParkAnalyserVal<int>::Set(int);

int32_t CParkAnalyser::CountNeighboursOfType(CThing::ThingModel model, MapId id, int32_t a, int32_t b) {
    Tr(32, Off(this));
    Tr(33, static_cast<uint32_t>(model));
    Tr(34, id.mIndex);
    Tr(35, static_cast<uint32_t>(a));
    Tr(36, static_cast<uint32_t>(b));
    return gCountAnswer;
}

int32_t CMapCell::IsWater() const { return Pred(1, this); }
int32_t CMapCell::IsPath() const { return Pred(2, this); }
int32_t CMapCell::IsQueue() const { return Pred(3, this); }
int32_t CMapCell::IsEntrance() const { return Pred(4, this); }
int32_t CMapCell::IsExit() const { return Pred(5, this); }
int32_t CMapCell::IsBridge() const { return Pred(6, this); }
int32_t CMapCell::IsFreeform() const { return Pred(7, this); }
int32_t CMapCell::IsLake() const { return Pred(8, this); }
int32_t CMapCell::IsRide() const { return Pred(9, this); }
int32_t CMapCell::IsCoasterPylon() const { return Pred(10, this); }
int32_t CMapCell::IsTrack() const { return Pred(11, this); }

int32_t CStaff::HasPatrolArea() const {
    Tr(30, Off(this));
    const int32_t block = BlockOf(this);
    return block >= 0 ? gHas[block] : 0;
}

void CStaff::GetPatrolArea(MapId* start, MapId* end) const {
    Tr(31, Off(this));
    const int32_t block = BlockOf(this);
    start->mIndex = block >= 0 ? gStart[block] : 0;
    end->mIndex = block >= 0 ? gEnd[block] : 0;
}

CRegionEffectCell* CMap::GetRegionalEffectCell(MapId cell) {
    Tr(40, Off(this));
    Tr(41, cell.mIndex);
    return reinterpret_cast<CRegionEffectCell*>(gRegion + kEffect + 10u * (cell.mIndex & 15u));
}

extern "C" {

uint64_t tpw_pl_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gRegion)); }

// The unit's pointers and the case's answers; the rows of the thing table (row 0 to 15) point at the eight thing blocks (slot 1 to 8; 0 for none).
void tpw_pl_init() {
    gWorld = reinterpret_cast<CWorld*>(gRegion + kW2);
    gWorldMap = reinterpret_cast<CMap*>(gRegion + kW3 + 0x2d8);
}
void tpw_pl_begin(int32_t truthy, uint32_t valSeed, int32_t countAnswer) {
    gTruthy = truthy;
    gValSeed = valSeed;
    gCountAnswer = countAnswer;
    gTraceN = 0;
    gWalkN = 0;
    gWalkAt = 0;
    std::memset(gHas, 0, sizeof gHas);
    std::memset(gStart, 0, sizeof gStart);
    std::memset(gEnd, 0, sizeof gEnd);
}
void tpw_pl_row(int32_t row, int32_t slot) {
    gThingArray[row].mThing = slot != 0 ? static_cast<void*>(gRegion + kThings + 0x100u * static_cast<uint32_t>(slot - 1)) : nullptr;
}
void tpw_pl_walk(const uint16_t* ids, int32_t n) {
    gWalkN = n;
    for (int32_t i = 0; i < n; i++) gWalk[i] = ids[i];
}
void tpw_pl_patrol(int32_t block, int32_t has, uint32_t start, uint32_t end) {
    gHas[block] = has;
    gStart[block] = static_cast<uint16_t>(start);
    gEnd[block] = static_cast<uint16_t>(end);
}
uint32_t tpw_pl_trace_n() { return gTraceN; }
const uint32_t* tpw_pl_trace_tags() { return gTraceTag; }
const uint32_t* tpw_pl_trace_vals() { return gTraceVal; }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_pl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mMapSquaresPerCell));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mGridSizeX));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mCells));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mNearbyCells));
    out[n++] = static_cast<int64_t>(sizeof(CParkAnalyser));
    out[n++] = static_cast<int64_t>(sizeof(CParkAnalyserCell));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyserCell, mVal4C));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyserCell, mVal58));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyserCell, mVal64));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMapCells));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(sizeof(CWorld));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mLitter));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mLitterCollector));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mF3C));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mF42));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mKind));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    return n;
}

// ---- the functions, one wrapper each with C types only (the cell functions take the analyser cell's index)
int32_t tpw_pl_cell_walk(int32_t index, uint32_t maxAge) { return Analyser()->mCells[index].GetNumWalkableCells(maxAge); }
int32_t tpw_pl_cell_littered(int32_t index, uint32_t maxAge) { return Analyser()->mCells[index].GetNumLitteredCells(maxAge); }
int32_t tpw_pl_cell_developed(int32_t index, uint32_t maxAge) { return Analyser()->mCells[index].GetNumDevelopedCells(maxAge); }
int32_t tpw_pl_sum_walk(uint32_t maxAge) { return Analyser()->GetNumWalkableCells(maxAge); }
int32_t tpw_pl_sum_littered(uint32_t maxAge) { return Analyser()->GetNumLitteredCells(maxAge); }
int32_t tpw_pl_sum_developed(uint32_t maxAge) { return Analyser()->GetNumDevelopedCells(maxAge); }
uint32_t tpw_pl_analyser_cell(uint32_t id) {
    MapId map;
    map.mIndex = static_cast<uint16_t>(id);
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(Analyser()->GetAnalyserCell(map)) - reinterpret_cast<uintptr_t>(Analyser()));
}
int32_t tpw_pl_nearby_type(int32_t model, uint32_t id, int32_t a) {
    MapId map;
    map.mIndex = static_cast<uint16_t>(id);
    return Analyser()->IsNearbyThingOfType(static_cast<CThing::ThingModel>(model), map, a);
}
uint32_t tpw_pl_nearby_litter(uint32_t who, uint32_t id, int32_t radius) {
    MapId map;
    map.mIndex = static_cast<uint16_t>(id);
    ThingId person;
    person.mIndex = static_cast<uint16_t>(who);
    return Analyser()->GetNearbyLitteredCell(person, map, radius).mIndex;
}
int32_t tpw_pl_pct_littered() { return Analyser()->GetPercentageLitteredCells(); }
void tpw_pl_clear() { Analyser()->ClearMapData(); }
void tpw_pl_mark(int32_t model) { Analyser()->MarkAreaPatrolledBy(static_cast<CThing::ThingModel>(model)); }
int32_t tpw_pl_pct_patrolled(int32_t model, int32_t area) {
    return Analyser()->PercentageOfAreaPatrolledBy(static_cast<CThing::ThingModel>(model), static_cast<CParkAnalyser::AreaType>(area));
}
int32_t tpw_pl_pct_cameras() { return Analyser()->PercentageOfParkCoveredByCameras(); }
int32_t tpw_pl_objects() { return Analyser()->NumberOfCellsCoveredByObjects(); }

}  // extern "C"
