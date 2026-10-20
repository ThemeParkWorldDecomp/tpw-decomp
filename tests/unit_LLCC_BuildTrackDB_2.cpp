// Test library for LLCC_BuildTrackDB (unit v5982; libtpw_unit_LLCC_BuildTrackDB_2.dylib, tests/test_LLCC_BuildTrackDB_2.py), main app:
//   src/LLCellRide_offsets_LLCC_SetTrackAnimReverse_LLCC_BuildTrackDB_2.cpp   LLCC_BuildTrackDB and the words gLLCCE0B0 / gLLCCE0B2
//
// Every callee outside the unit is a recording fake with the same event name and arguments as the module's PowerPC stub; the cells
// are named by their index in the world's track grid. The globals of other files the unit reads are defined here (zero at start).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LLCellSystem.h"
#include "TrackCell.h"
#include "CellBase.h"
#include "MapCell.h"
#include "World.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "MainBalance.h"
#include "ThingReceivers.h"
#include "Debug.h"
#include "analyser.h"
#include "BMP_InsideArea.h"
#include "ObjectParts.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

// ---- globals of other files, defined here
int32_t gLLCC9F20 = 0;
int32_t gLLCCFirstBlockX = 0;
int32_t gLLCCFirstBlockY = 0;
uint8_t gLLCCE0CC = 0;
CWorld* gGameWorld = nullptr;
CThingArrayEntry gThingArray[kThingArraySize];
tag_sRideAnimSet* gRideAnimSets[64];
// The balance table: raw storage under the symbol of gMainBalance (its constructor and vtable are not compiled here).
extern "C" {
alignas(16) unsigned char gMainBalanceStorage[0x2000] __asm__("_gMainBalance");
}

namespace {

int64_t I(int64_t v) { return v; }

// The index of a cell in the track grid (a NULL answers -1000, like the PowerPC stubs).
int64_t CellIdx(const void* p) {
    if (p == nullptr) return -1000;
    return (static_cast<int64_t>(reinterpret_cast<intptr_t>(p)) - static_cast<int64_t>(reinterpret_cast<intptr_t>(gGameWorld)) - 0x1102d8) / 0x28;
}

uint32_t Word(const void* base, size_t offset) { return *reinterpret_cast<const uint32_t*>(reinterpret_cast<const char*>(base) + offset); }

}  // namespace

// ---- the fakes
int32_t CTrackCell::IsTrackX() const {
    ev("IsTrackX", {CellIdx(this)});
    return static_cast<int32_t>(ret("IsTrackX"));
}
int32_t CTrackCell::IsTrack() const {
    ev("IsTrack", {CellIdx(this)});
    return static_cast<int32_t>(ret("IsTrack"));
}
uint8_t CTrackCell::IsCorner() const {
    ev("IsCorner", {CellIdx(this)});
    return static_cast<uint8_t>(ret("IsCorner"));
}
uint8_t CTrackCell::IsCrossRoad() const {
    ev("IsCrossRoad", {CellIdx(this)});
    return static_cast<uint8_t>(ret("IsCrossRoad"));
}
uint8_t CTrackCell::IsStraight() const {
    ev("IsStraight", {CellIdx(this)});
    return static_cast<uint8_t>(ret("IsStraight"));
}
int32_t CTrackCell::IsFlag(uint16_t flag) const {
    ev("IsFlag", {CellIdx(this), I(flag)});
    return static_cast<int32_t>(ret("IsFlag"));
}
int32_t CTrackCell::IsJustTrack() const {
    ev("IsJustTrack", {CellIdx(this)});
    return static_cast<int32_t>(ret("IsJustTrack"));
}
void CTrackCell::SetHFieldTile() { ev("SetHFieldTile", {CellIdx(this)}); }
void CTrackCell::SetSegmentNumber(int16_t number) { ev("SetSegmentNumber", {CellIdx(this), I(number)}); }
CTrackCell* CTrackCell::GetNeighbouringCell(uint8_t direction, int32_t offset) {
    ev("GetNeighbouringCell", {CellIdx(this), I(direction), I(offset)});
    int64_t id = ret("GetNeighbouringCell");
    if (id == 0) return nullptr;
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x1102d8 + (id - 1) * 0x28);
}
CMapCell* CCellBase::GetMapCellEquiv() {
    ev("GetMapCellEquiv", {CellIdx(this)});
    return reinterpret_cast<CMapCell*>(this);
}
uint8_t CCellBase::GetDirection() const {
    ev("GetDirection", {CellIdx(this)});
    return static_cast<uint8_t>(ret("GetDirection"));
}
uint8_t CCellBase::GetNeighbours() const {
    ev("GetNeighbours", {CellIdx(this)});
    return static_cast<uint8_t>(ret("GetNeighbours"));
}
void CCellBase::ModifyDirection(uint8_t direction) { ev("ModifyDirection", {CellIdx(this), I(direction)}); }
int32_t CCellBase::NumberOfNeighbours() const {
    ev("NumberOfNeighbours", {CellIdx(this)});
    return static_cast<int32_t>(ret("NumberOfNeighbours"));
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CMapCell::GetObjectThingId() const {
    ev("GetObjectThingId", {CellIdx(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("GetObjectThingId"));
    return id;
}
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {I(Word(this, 0x30))});
    return reinterpret_cast<RideInfo*>(const_cast<char*>(reinterpret_cast<const char*>(this)) + 0x34 - 0x10);
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    (void)format;
    ev("GAME_ASSERTMSG", {I(condition & 0xff)});
}
void CTRACE(uint32_t level, const char* format, ...) {
    (void)format;
    ev("CTRACE", {I(level)});
}
void BMP_FreeTrack(int32_t ride) { ev("BMP_FreeTrack", {I(ride)}); }
int32_t BMP_TrackSectionPlaceSection(int32_t ride, int32_t x, int32_t z, int32_t kind, int32_t count, uint32_t thing, int32_t info) {
    ev("BMP_TrackSectionPlaceSection", {I(ride), I(x), I(z), I(kind), I(count), I(static_cast<int32_t>(thing)), I(info)});
    return 1;   // the answer LLCC_BuildTrackDB ignores
}
int32_t FindDataNodeIndexByDisableCode(tag_sRideAnimSet* set, uint32_t a, uint32_t code) {
    ev("FindDataNodeIndexByDisableCode", {I(set->mFF8), I(static_cast<int32_t>(a)), I(static_cast<int32_t>(code))});
    return static_cast<int32_t>(ret("FindDataNodeIndexByDisableCode"));
}
void LLCR_OverRide(uint16_t* out, int32_t* x, int32_t* y) {
    ev("LLCR_OverRide", {I(*x), I(*y)});
    *out = static_cast<uint16_t>(ret("LLCR_OverRide"));
}
uint8_t OPPOSITE(uint8_t direction) {
    ev("OPPOSITE", {I(direction)});
    return static_cast<uint8_t>((direction << 4) | (direction >> 4));
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_bt_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"rot", &gLLCC9F20}, {"fx", &gLLCCFirstBlockX}, {"fy", &gLLCCFirstBlockY}, {"dir", &gLLCCE0CC}, {"b0", &gLLCCE0B0}, {"b2", &gLLCCE0B2},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The world block (allocated once; the test writes every byte the call reads) and its pointer global.
uint64_t tpw_bt_world() {
    static void* block = nullptr;
    if (block == nullptr) block = tpw::ArenaAllocZeroed(0x1e0000);
    gGameWorld = reinterpret_cast<CWorld*>(block);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
}

uint32_t tpw_bt_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

// index 0 / 1 / 2: the words the original reads at +0x57c (low x), +0x580 (row), +0x584 (high x) of the balance table.
void tpw_bt_setbalance(int32_t which, int32_t value) {
    static const int32_t offsets[] = {0x57c, 0x580, 0x584};
    reinterpret_cast<CMainBalance*>(gMainBalanceStorage)->mValues[(offsets[which] - 8) / 4].mInteger = value;
}

void tpw_bt_setthing(int32_t index, uint64_t object) { gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(object)); }
void tpw_bt_setanim(int32_t index, uint64_t set) { gRideAnimSets[index] = reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(set)); }

// Joins a ride anim set to its mesh and header (native pointers) and gives the set the number the fakes log.
void tpw_bt_link(uint64_t set, uint64_t mesh, uint64_t header, uint32_t id) {
    tag_sRideAnimSet* s = reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(set));
    tag_sMesh* m = reinterpret_cast<tag_sMesh*>(static_cast<uintptr_t>(mesh));
    s->mHeads = m;
    m->mHeader = reinterpret_cast<tag_sM3D2Header*>(static_cast<uintptr_t>(header));
    s->mFF8 = id;
}

int32_t tpw_bt_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodeCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF46));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodes));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mMiniNodes));
    out[n++] = static_cast<int64_t>(sizeof(tag_sNode));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMiniNode));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMiniNode, mFlags));
    out[n++] = sizeof(tag_sRideAnimSet) <= 0x200 ? 1 : 0;
    out[n++] = sizeof(tag_sMesh) <= 0x200 ? 1 : 0;
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mCellId));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mMeshInstance));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mType));
    out[n++] = static_cast<int64_t>(offsetof(CTrackCell, mParentID));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    return n;
}

int32_t tpw_bt_run(int32_t flag) { return LLCC_BuildTrackDB(flag != 0); }

}  // extern "C"
