// Test library for the map half of MapDelta.cpp (unit w145; libtpw_unit_MapDelta.dylib, tests/test_MapDelta.py), main app:
//   src/game/park/Map.cpp, src/game/park/MapDelta.cpp   the 29 functions of the unit, the six halfword sentinels, the time stamp counter and five compass offsets
//   src/game/park/MapDelta.cpp       (unit w139) the other three compass offsets
//
// Every callee outside them is a fake that records the same event as the PowerPC side's stub: GAME_ASSERTMSG, TRACE (the format and one
// unsigned argument per % of the format), wallhug_trivial (the mode and the path's bytes 2 to 5; answers from the case's queues), the four
// instance mesh and height field tile members of the map and track cells (the cell's index), PAR_KillEmitter, SCR_KillScript and
// CWorldSavvy::GetWorld (the cell's index). The map is one static block (tpw_md_block), the world pointer is shifted back so that the map
// starts at the block, as it does at CWorld + 0x2d8.
#define TPW_CALLLOG_EVENTS 52000   // the engine refresh walks: two fake calls for each of 16,384 places
#define TPW_CALLLOG_LABELS 512
#define TPW_CALLLOG_QUEUE_VALUES 600   // the walking path queue: up to 255 points of two bytes
#include "calllog.cpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "Debug.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "TPCamera.h"
#include "TrackCell.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "rsse.h"

using namespace calllog;

// ---- the main balance (raw storage under the symbol's name)
#ifdef __APPLE__
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");
#else
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("gMainBalance");
#endif
alignas(16) unsigned char gBalanceStorage[sizeof(CMainBalance)] = {};

// ---- globals of other files the unit reads, defined here
CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gWorld;
CMap* gWorldMap;

namespace {

constexpr uint32_t kBlockSize = 0x1db000;
alignas(16) unsigned char gBlock[kBlockSize];

int64_t CellIndex(const void* cell) {
    return (static_cast<const unsigned char*>(cell) - reinterpret_cast<const unsigned char*>(gWorldMap)) / 0x44;
}
int64_t TrackIndex(const void* cell) {
    return ((static_cast<const unsigned char*>(cell) - reinterpret_cast<const unsigned char*>(gWorldMap)) - 0x110000) / 0x28;
}

}  // namespace

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

void TRACE(const char* format, ...) {
    int64_t values[3] = {0, 0, 0};
    int n = 0;
    for (const char* p = format; *p; ++p)
        if (*p == '%') ++n;
    va_list ap;
    va_start(ap, format);
    for (int i = 0; i < n && i < 3; ++i) values[i] = static_cast<int64_t>(va_arg(ap, uint32_t));
    va_end(ap);
    if (n == 0) ev("TRACE", {}, {format});
    else if (n == 1) ev("TRACE", {values[0]}, {format});
    else if (n == 2) ev("TRACE", {values[0], values[1]}, {format});
    else ev("TRACE", {values[0], values[1], values[2]}, {format});
}

int32_t wallhug_trivial(wallhug_path* path, NavMode mode) {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(path);
    ev("wallhug_trivial", {static_cast<int32_t>(mode), bytes[2], bytes[3], bytes[4], bytes[5]});
    const int64_t count = ret("wh_count", 0) & 0xff;
    path->mCount = static_cast<uint8_t>(count);
    for (int64_t i = 0; i < count; ++i) {
        path->mPoints[i].mX = static_cast<uint8_t>(ret("wh_pt", 0));
        path->mPoints[i].mY = static_cast<uint8_t>(ret("wh_pt", 0));
    }
    return static_cast<int32_t>(ret("wh_ret", 0));
}

void CMapCell::RemoveInstanceMesh() { ev("CMapCell::RemoveInstanceMesh", {CellIndex(this)}); }
void CMapCell::RefreshHFieldTile() { ev("CMapCell::RefreshHFieldTile", {CellIndex(this)}); }
void CTrackCell::RemoveInstanceMesh() { ev("CTrackCell::RemoveInstanceMesh", {TrackIndex(this)}); }
void CTrackCell::RefreshHFieldTile() { ev("CTrackCell::RefreshHFieldTile", {TrackIndex(this)}); }
void PAR_KillEmitter(int32_t handle) { ev("PAR_KillEmitter", {handle}); }
void SCR_KillScript(int32_t handle) { ev("SCR_KillScript", {handle}); }
CWorld& CWorldSavvy::GetWorld() const {
    ev("GetWorld", {CellIndex(this)});
    return *gWorld;
}

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_md_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gMapEmptyCellId", &gMapEmptyCellId},
        {"gRemoveThingNoPrev", &gRemoveThingNoPrev},
        {"gRemoveThingNoWho", &gRemoveThingNoWho},
        {"gRemoveThingNoNext", &gRemoveThingNoNext},
        {"gLitterNoCollector", &gLitterNoCollector},
        {"gGetObjectThingIdEnd", &gGetObjectThingIdEnd},
        {"gMapTimeStamp", &gMapTimeStamp},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the size of the map (0x1db000 bytes); the world pointer is set by tpw_md_world.
uint64_t tpw_md_block() {
    std::memset(gBlock, 0, sizeof gBlock);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gBlock));
}
uint64_t tpw_md_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
void tpw_md_world(uint64_t block) {
    gWorldMap = reinterpret_cast<CMap*>(static_cast<uintptr_t>(block));
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(block) - 0x2d8);
}
void tpw_md_row(int32_t row, uint64_t address) { gThingArray[row].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(address)); }
void tpw_md_set_balance(int32_t slot, int32_t value) { gMainBalance.mValues[slot].mBits = 0xa5a5a5a500000000ull | static_cast<uint32_t>(value); }

int32_t tpw_md_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mWho));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mLitterScript));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mLitter));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mLitterCollector));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mTimeMarkedForLitterCollection));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mLitterEmitter));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mKind));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellPrev));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellNext));
    out[n++] = static_cast<int64_t>(sizeof(RegionEffectInfo));
    out[n++] = static_cast<int64_t>(sizeof(wallhug_path));
    return n;
}

// ---- the functions, one wrapper each with C types only (the map and the cells as addresses, the tagged ids as uint32_t)
namespace {
CMap* M(uint64_t p) { return reinterpret_cast<CMap*>(static_cast<uintptr_t>(p)); }
CMapCell* Cell(uint64_t p) { return reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(p)); }
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> MapId(uint32_t v) {
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(v);
    return id;
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingId(uint32_t v) {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(v);
    return id;
}
}  // namespace

int32_t tpw_md_isxy(int32_t x, int32_t y) { return CMap::IsXYValid(x, y); }
uint64_t tpw_md_effcell(uint64_t m, uint32_t id) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(M(m)->GetRegionalEffectCell(MapId(id)))); }
void tpw_md_add(uint64_t m, int32_t effector, uint32_t id) { M(m)->AddRegionalEffector(static_cast<RegionEffector>(effector), MapId(id)); }
void tpw_md_remove(uint64_t m, int32_t effector, uint32_t id) { M(m)->RemoveRegionalEffector(static_cast<RegionEffector>(effector), MapId(id)); }
void tpw_md_modify(uint64_t m, int32_t add, int32_t effector, uint32_t id) { M(m)->ModifyRegionalEffector(add, static_cast<RegionEffector>(effector), MapId(id)); }
uint32_t tpw_md_ticket(uint64_t m, int32_t which) { return M(m)->GetTicketBoothPos(which).mIndex; }
uint32_t tpw_md_bus(uint64_t m, int32_t which) { return M(m)->GetBusStopPos(which).mIndex; }
uint32_t tpw_md_entrance(uint64_t m, int32_t which) { return M(m)->GetParkEntrancePos(which).mIndex; }
uint32_t tpw_md_crossing_park(uint64_t m, int32_t which) { return M(m)->GetCrossingParkSidePos(which).mIndex; }
uint32_t tpw_md_crossing_bs(uint64_t m, int32_t which) { return M(m)->GetCrossingBSSidePos(which).mIndex; }
uint32_t tpw_md_newstamp(uint64_t m) { return M(m)->MakeNewTimeStamp(); }
void tpw_md_setstamp(uint64_t m, uint32_t stamp, uint32_t id) { M(m)->SetTimeStamp(stamp, MapId(id)); }
uint32_t tpw_md_getstamp(uint64_t m, uint32_t id) { return M(m)->GetTimeStamp(MapId(id)); }
uint32_t tpw_md_stampcell(uint64_t m, uint32_t x, uint32_t y) { return M(m)->GetTimeStampCell(x, y); }
uint32_t tpw_md_opposite(uint32_t d) { return OPPOSITE(static_cast<uint8_t>(d)); }
uint32_t tpw_md_adddir(uint32_t a, uint32_t b) { return ADD_DIRECTION(static_cast<uint8_t>(a), static_cast<uint8_t>(b)); }
int32_t tpw_md_walk(uint32_t from, uint32_t to, int32_t mode) { return MAP_count_walking_distance(MapId(from), MapId(to), static_cast<NavMode>(mode)); }
void tpw_md_delete(uint64_t m) { M(m)->DeleteMapInEngine(); }
void tpw_md_update(uint64_t m) { M(m)->UpdateMapInEngine(); }
void tpw_md_cleanup(uint64_t m) { M(m)->CleanUpPark(); }
int32_t tpw_md_hasmod(uint64_t cell, uint64_t other) { return Cell(cell)->HasBeenModifiedFrom(*Cell(other)); }
void tpw_md_removething(uint64_t cell, uint32_t id) { Cell(cell)->RemoveThing(ThingId(id)); }
void tpw_md_markfor(uint64_t cell, uint32_t id) { Cell(cell)->MarkForLitterCollection(ThingId(id)); }
int32_t tpw_md_marked(uint64_t cell) { return Cell(cell)->MarkedForLitterCollection(); }
void tpw_md_removelitter(uint64_t cell) { Cell(cell)->RemoveLitter(); }
uint64_t tpw_md_neighbour_delta(uint64_t cell, int32_t dx, int32_t dy, int32_t flag) {
    const MapDelta delta = {dx, dy, 0};
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Cell(cell)->GetNeighbouringCell(&delta, flag)));
}
uint64_t tpw_md_neighbour_dir(uint64_t cell, uint32_t dir) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Cell(cell)->GetNeighbouringCell(static_cast<uint8_t>(dir))));
}
void tpw_md_mul(int32_t dx, int32_t dy, int32_t k, int32_t* out) {
    const MapDelta delta = {dx, dy, 0};
    const MapDelta result = delta * k;
    out[0] = result.mX;
    out[1] = result.mY;
}
uint32_t tpw_md_objthing(uint64_t cell) { return Cell(cell)->GetObjectThingId().mIndex; }

}  // extern "C"
