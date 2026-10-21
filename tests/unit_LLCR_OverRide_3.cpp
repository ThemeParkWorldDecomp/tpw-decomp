// Test library for LLCR_OverRide (pointer overload), LLCM_TrackCellBoundary, LLCR_SetRide, LLCR_AddRideToDB (unit v5932; libtpw_unit_LLCR_OverRide_3.dylib, tests/test_LLCR_OverRide_3.py), main app:
//   src/LLCR_OverRide_LLCR_OverRide_3.cpp   the four functions and the words 0x1029e04c, 0x102de092, 0x102de094, 0x102de098, 0x102de09c
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals of
// other files the unit reads (the world, the thing array, the ride database, the UI, the track flag) are defined here, zero at start.
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LLCellSystem.h"
#include "World.h"
#include "MapCell.h"
#include "TPEngine.h"
#include "RideDB.h"
#include "UIInterface.h"
#include "ThingReceivers.h"
#include "BMP_InsideArea.h"
#include "ObjectParts.h"
#include "analyser.h"
#include "Debug.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CWorld* gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];
CRideDatabase gRideDatabase;
UIInterface gUIInterface;
int32_t gLLCCE0DC;

static uint64_t gTestRide;      // the ride record GetRideInfo answers (host address)
static uint64_t gTestCurrent;   // the object LLCM_GetCurrentObject answers

// ---- the fakes: the same event name and arguments as the module's PowerPC stubs
int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("IsXYValid", {x, y});
    return static_cast<int32_t>(ret("IsXYValid"));
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CMapCell::GetObjectThingId() const {
    int64_t offset = static_cast<int64_t>(reinterpret_cast<uintptr_t>(this)) - static_cast<int64_t>(reinterpret_cast<uintptr_t>(gGameWorld));
    ev("GetObjectThingId", {offset});
    return tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{static_cast<uint16_t>(ret("GetObjectThingId"))};
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition}, {format});
}

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(gTestRide));
}

uint8_t LLCR_OffsetStartOfTrack(int32_t* x, int32_t* y, int32_t rotation) {
    ev("OffsetStartOfTrack", {*x, *y, rotation});
    *x += static_cast<int32_t>(ret("OffX"));
    *y += static_cast<int32_t>(ret("OffY"));
    return 0;
}

int32_t BMP_CreateRide(int32_t kind, int32_t x, int32_t y, int32_t rotation, int32_t id, int32_t f10) {
    ev("BMP_CreateRide", {kind, x, y, rotation, id, f10});
    return static_cast<int32_t>(ret("BMP_CreateRide"));
}

MapDelta MapDelta::Rotate(int32_t rotation) const {
    int64_t offset = static_cast<int64_t>(reinterpret_cast<uintptr_t>(this)) - static_cast<int64_t>(gTestRide);
    // the record's map offset is logged at the Mac's offset (0x7ac); any other host offset is logged apart from every Mac offset
    offset = offset == static_cast<int64_t>(offsetof(RideInfo, mMapOffsetX)) ? 0x7ac : -1 - offset;
    ev("Rotate", {offset, rotation});
    MapDelta d;
    d.mX = static_cast<int32_t>(ret("RotX"));
    d.mY = static_cast<int32_t>(ret("RotY"));
    d.mFlag = 0;
    return d;
}

CObject::CObject(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> map, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t a, int32_t b) {
    ev("CObject", {newlab(this), map.mIndex, type.mIndex, a, b});
}

void CObject::SetUpgradeParent(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    ev("SetUpgradeParent", {lab(this), id.mIndex});
}

CObject* LLCM_GetCurrentObject() {
    ev("LLCM_GetCurrentObject", {});
    return reinterpret_cast<CObject*>(static_cast<uintptr_t>(gTestCurrent));
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    uint32_t marker = *reinterpret_cast<const uint32_t*>(this);
    ev("GetThingId", {marker});
    return tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{static_cast<uint16_t>(ret("GetThingId"))};
}

void UIInterface::SetSelectedObject(CObject* object) {
    ev("SetSelectedObject", {lab(this), lab(object)});
}

extern "C" {

uint64_t tpw_lcro3_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gworld", &gGameWorld},   {"tarr", &gThingArray[0]}, {"g04C", &gLLCCE04C}, {"g092", &gLLCCE092},
        {"g094", &gLLCCE094},      {"g098", &gLLCCE098},      {"shape", &gLLCRRideShape},      {"g09C", &gLLCCE09C}, {"g0DC", &gLLCCE0DC},
        {"rdb", &gRideDatabase},   {"ui", &gUIInterface},     {"ride", &gTestRide}, {"cur", &gTestCurrent},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lcro3_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lcro3_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMapCells));
    out[n++] = static_cast<int64_t>(offsetof(MapDelta, mY));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(sizeof(RideInfo));
    return n;
}

// The host offset of a ride record word at the Mac's offset: a balance value (0x00c + 4 * slot on the Mac) or the map offset (0x7ac).
int64_t tpw_lcro3_rideoff(uint32_t macOffset) {
    alignas(16) static unsigned char probe[sizeof(RideInfo)];
    RideInfo* ride = reinterpret_cast<RideInfo*>(probe);
    const char* base = reinterpret_cast<const char*>(ride);
    if (macOffset == 0x7ac) return static_cast<int64_t>(offsetof(RideInfo, mMapOffsetX));
    const RideBalanceSlot slot = static_cast<RideBalanceSlot>((macOffset - 0x00c) / 4);
    return reinterpret_cast<const char*>(&ride->Value(slot)) - base;
}

void tpw_lcro3_over(uint64_t out, uint64_t x, uint64_t y) {
    LLCR_OverRide(reinterpret_cast<uint16_t*>(out), reinterpret_cast<int32_t*>(x), reinterpret_cast<int32_t*>(y));
}

void tpw_lcro3_boundary(int32_t x, int32_t y, uint64_t cx, uint64_t cy, uint64_t ox, uint64_t oy) {
    LLCM_TrackCellBoundary(x, y, reinterpret_cast<int32_t*>(cx), reinterpret_cast<int32_t*>(cy), reinterpret_cast<int32_t*>(ox),
                           reinterpret_cast<int32_t*>(oy));
}

void tpw_lcro3_setride(uint32_t id) { LLCR_SetRide(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(id)}); }

uint64_t tpw_lcro3_addride(int32_t rotation) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(LLCR_AddRideToDB(rotation))); }

}  // extern "C"
