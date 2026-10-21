// Test library for CMapCell::ModifyType and CMapCell::GetTile (unit v5535; libtpw_unit_CMapCell_ModifyType.dylib, tests/test_CMapCell_ModifyType.py), main app:
//   src/MapDelta_map_CMapCell_ModifyType.cpp   both functions and the globals nobody else defines (the two tile tables, their counts, the random flag,
//                                               the no-object thing id)
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The globals of other files
// the unit reads (gGameWorld, gThingArray, gActionRecorder, gTrackCostFree, gLLCC47E4) are defined here. The world is a static block (the map cells
// at +0x2d8, the track cells at +0x1102d8); cells and objects are named in the log by their ids or their offsets from the world.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ObjectParts.h"
#include "MapCell.h"
#include "TrackCell.h"
#include "World.h"
#include "Debug.h"
#include "analyser.h"
#include "LLCellSystem.h"
#include "ActionRecorder.h"
#include "ThingReceivers.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CWorld* gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];
CActionRecorder gActionRecorder;
char gTrackCostFree;
int32_t gLLCC47E4;

namespace {

alignas(16) uint8_t gTestWorld[0x112000];
uint32_t gMarkers[8];

// The tile tables and counts as the library was loaded (before a case writes them): the "image" test compares them with the Mac's data (unit v5L35).
struct InitialTiles {
    CMapCellTileEntry a[11], b[49];
    int32_t countA, countB;
    InitialTiles() : countA(gTileCountA), countB(gTileCountB) {
        std::memcpy(a, gTileTableA, sizeof(a));
        std::memcpy(b, gTileTableB, sizeof(b));
    }
};
const InitialTiles gInitialTiles;

int32_t Rel(const void* p) { return static_cast<int32_t>(reinterpret_cast<const uint8_t*>(p) - reinterpret_cast<const uint8_t*>(gGameWorld)); }
int64_t CellId(const void* cell) { return *reinterpret_cast<const uint16_t*>(cell); }
CMapCell* CellAt(int index) { return reinterpret_cast<CMapCell*>(gTestWorld + 0x2d8 + index * 0x44); }
CTrackCell* TrackAt(int index) { return reinterpret_cast<CTrackCell*>(gTestWorld + 0x1102d8 + index * 0x28); }
uint8_t Rotl8(uint8_t v, int n) { return static_cast<uint8_t>((v << n) | (v >> (8 - n))); }

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
int32_t CMapCell::ValidTypeChange(CELL_MODES mode) {
    ev("ValidTypeChange", {CellId(this), static_cast<int32_t>(mode)});
    return static_cast<int32_t>(ret("ValidTypeChange"));
}
int32_t LLCC_SystemGetPathCost() {
    ev("PathCost", {});
    return static_cast<int32_t>(ret("PathCost"));
}
int32_t LLCC_SystemGetQueueCost() {
    ev("QueueCost", {});
    return static_cast<int32_t>(ret("QueueCost"));
}
void CMapCell::Delete(int32_t a, int32_t b) {
    ev("Delete", {CellId(this), a, b, static_cast<int64_t>(static_cast<uint32_t>(gLLCC47E4))});
}
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {Rel(this)});
    return reinterpret_cast<CBankAccount*>(static_cast<uintptr_t>(0xab1000));
}
int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this))});
    return static_cast<int32_t>(ret("GetBalance"));
}
void CBankAccount::MakeWithdrawal(uint32_t amount) {
    ev("MakeWithdrawal", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this)), amount});
}
uint32_t CMap::MakeNewTimeStamp() {
    ev("MakeNewTimeStamp", {Rel(this)});
    return static_cast<uint32_t>(ret("MakeNewTimeStamp"));
}
void CMap::SetTimeStamp(uint32_t stamp, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("SetTimeStamp", {Rel(this), stamp, cell.mIndex});
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CMapCell::GetObjectThingId() const {
    ev("GetObjectThingId", {CellId(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("GetObjectThingId"));
    return id;
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    int64_t sum = 0;
    for (const char* p = format; *p; ++p) sum += static_cast<unsigned char>(*p);
    ev("GAME_ASSERTMSG", {condition, static_cast<int64_t>(std::strlen(format)), sum});
}
void CObject::RecalculateBackOfQueue() {
    ev("RecalculateBackOfQueue", {this ? static_cast<int64_t>(*reinterpret_cast<uint32_t*>(this)) : -1});
}

uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {CellId(this)});
    return static_cast<uint8_t>(ret("GetNeighbours"));
}
uint8_t CCellBase::GetDirection() const {
    ev("GetDirection", {CellId(this)});
    return static_cast<uint8_t>(ret("GetDirection"));
}
uint8_t CMapCell::IsStraight() const {
    ev("IsStraight", {CellId(this)});
    return static_cast<uint8_t>(ret("IsStraight"));
}
uint8_t CMapCell::IsCorner() const {
    ev("IsCorner", {CellId(this)});
    return static_cast<uint8_t>(ret("IsCorner"));
}
CMapCell* CMapCell::GetNeighbouringCell(uint8_t direction) {
    ev("GetNeighbouringCell", {CellId(this), direction});
    const int64_t k = ret("GetNeighbouringCell");
    return k == 0 ? nullptr : CellAt(static_cast<int>(k - 1));
}
int32_t CMapCell::IsPath() const {
    ev("IsPath", {CellId(this)});
    return static_cast<int32_t>(ret("IsPath"));
}
int32_t CMapCell::IsTrackX() const {
    ev("IsTrackX", {CellId(this)});
    return static_cast<int32_t>(ret("IsTrackX"));
}
CTrackCell* CCellBase::GetTrackCellEquiv() {
    ev("GetTrackCellEquiv", {CellId(this)});
    return TrackAt(static_cast<int>(ret("GetTrackCellEquiv")));
}
int32_t CTrackCell::IsBridge() const {
    ev("IsBridge", {Rel(this)});
    return static_cast<int32_t>(ret("IsBridge"));
}
uint8_t OPPOSITE(uint8_t direction) {
    ev("OPPOSITE", {direction});
    return Rotl8(direction, 4);
}
uint8_t ADD_DIRECTION(uint8_t first, uint8_t second) {
    ev("ADD_DIRECTION", {first, second});
    return Rotl8(first, 2);
}
extern "C" int rand() {
    ev("rand", {});
    return static_cast<int>(ret("rand"));
}

extern "C" {

uint64_t tpw_cm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"free", &gTrackCostFree},
        {"g47e4", &gLLCC47E4},
        {"rec", &gActionRecorder.mRecording},
        {"nothing", &gNoObjectThingId},
        {"flag", &gTileRandomFlag},
        {"tabA", gTileTableA},
        {"cntA", &gTileCountA},
        {"tabB", gTileTableB},
        {"cntB", &gTileCountB},
        {"initA", gInitialTiles.a},
        {"initCntA", &gInitialTiles.countA},
        {"initB", gInitialTiles.b},
        {"initCntB", &gInitialTiles.countB},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The static world block, made the global world.
uint64_t tpw_cm_world() {
    gGameWorld = reinterpret_cast<CWorld*>(gTestWorld);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gTestWorld));
}

// Row i of gThingArray: no thing (present 0) or a marker object whose first word is `marker`.
void tpw_cm_setthing(int32_t i, int32_t present, uint32_t marker) {
    gMarkers[i] = marker;
    gThingArray[i].mThing = present ? static_cast<void*>(&gMarkers[i]) : nullptr;
}

int32_t tpw_cm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mCellId));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mType));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mNeighbours));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mDirection));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mParentID));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mOverlapCounter));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mParentID));
    out[n++] = static_cast<int64_t>(sizeof(CMapCellTileEntry));
    out[n++] = static_cast<int64_t>(offsetof(CMapCellTileEntry, mRotation));
    out[n++] = static_cast<int64_t>(offsetof(CMapCellTileEntry, mMask));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mRecording));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMapCells));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTrackCells));
    return n;
}

uint32_t tpw_cm_modify(uint64_t cell, int32_t mode) {
    return static_cast<uint32_t>(reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(cell))->ModifyTypeBody(static_cast<CELL_MODES>(mode)));
}
uint32_t tpw_cm_gettile(uint64_t cell, uint64_t out) {
    return reinterpret_cast<CMapCell*>(static_cast<uintptr_t>(cell))->GetTile(reinterpret_cast<int32_t*>(static_cast<uintptr_t>(out)));
}

}  // extern "C"
