// Test library for LLCM_SetCurrentObject and LLCM_StartTrackEditMode (unit v5182; libtpw_unit_LLCM_SetCurrentObject.dylib, tests/test_LLCM_SetCurrentObject.py), main app:
//   src/LLCellMouse_LLCM_SetCurrentObject.cpp   the two functions and the nine new words they define
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs); cells are logged by their
// index from the (fake) world, objects by label. The globals of other files the unit reads or writes are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "LLCellSystem.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "UIInterface.h"
#include "World.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
int32_t gHoardingRight;
int32_t gHoardingBottom;
uint8_t gLLCCE0CC;
int32_t gLLCCFirstBlockX;
int32_t gLLCCFirstBlockY;
uint16_t gLLCCDB68;
int32_t gLLCCDB6C;
uint16_t gLLCC9F24;
uint16_t gLLCC9F26;
uint16_t gLLCCDB58;
uint16_t gLLCCDB5A;
int32_t gLLCCDB50;
int32_t gLLCCDB54;
int32_t gLLCCC210;
int32_t gLLCCC214;
int32_t gLLCMIllegalPlacement;
int32_t gLLCC4800;
int32_t gLLCC4804;
float gDynamicCellF9BC0;
float gDynamicCellF9BC4;
CWorld* gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];
CRideDatabase gRideDatabase;
UIInterface gUIInterface;
CActionRecorder gActionRecorder;

namespace {

CObject* gObjects[8];
uint8_t* gInfo;

int64_t CellIndex(const void* cell) {
    return (static_cast<int64_t>(reinterpret_cast<intptr_t>(cell)) - static_cast<int64_t>(reinterpret_cast<intptr_t>(gGameWorld)) - 0x1102d8) / 0x28;
}

CObject* ObjectOf(int64_t k) { return k > 0 ? gObjects[k - 1] : nullptr; }

}  // namespace

// ---- the fakes
int32_t LLCC_SystemIsMode(int32_t mode) {
    ev("IsMode", {mode});
    return static_cast<int32_t>(ret("IsMode"));
}

uint8_t LLCR_OffsetEndOfTrack(int32_t* x, int32_t* y, int32_t rotation) {
    ev("OffsetEnd", {*x, *y, rotation});
    int32_t nx = static_cast<int32_t>(static_cast<uint32_t>(*x) ^ 0x15u);
    int32_t ny = static_cast<int32_t>(static_cast<uint32_t>(*y) + static_cast<uint32_t>(rotation));
    *x = nx;
    *y = ny;
    return static_cast<uint8_t>(ret("OffsetEnd"));
}

uint8_t LLCR_OffsetStartOfTrack(int32_t* x, int32_t* y, int32_t rotation) {
    ev("OffsetStart", {*x, *y, rotation});
    int32_t nx = static_cast<int32_t>(static_cast<uint32_t>(*x) + static_cast<uint32_t>(rotation) + 1u);
    int32_t ny = static_cast<int32_t>(static_cast<uint32_t>(*y) - 3u);
    *x = nx;
    *y = ny;
    return static_cast<uint8_t>(ret("OffsetStart"));
}

CObject* UIInterface::GetSelectedObject() {
    ev("GetSelectedObject", {});
    return ObjectOf(ret("GetSelectedObject"));
}

void UIInterface::SetSelectedObject(CObject* object) { ev("SetSelectedObject", {lab(object)}); }

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CObject::GetUpgradeParent() const {
    ev("GetUpgradeParent", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("GetUpgradeParent"));
    return id;
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    int64_t sum = 0;
    for (int64_t i = 0; format[i] != 0; ++i) sum += static_cast<int64_t>(static_cast<uint8_t>(format[i])) * (i + 1);
    ev("GAME_ASSERTMSG", {condition, sum});
}

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {id.mIndex});
    return reinterpret_cast<RideInfo*>(gInfo);
}

void CActionRecorder::RecordAction(int32_t action, uint16_t value) { ev("RecordAction", {action, value}); }

RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this)});
    return ret("GetDBInfo") != 0 ? reinterpret_cast<RideInfo*>(gInfo) : nullptr;
}

void LLCC_SystemSetRideQueue(int32_t queue) { ev("SetRideQueue", {queue}); }
void LLCC_SystemSetRideTrack(int32_t track) { ev("SetRideTrack", {track}); }
int32_t LLCC_SystemGetRideTrack() {
    ev("GetRideTrack", {});
    return static_cast<int32_t>(ret("GetRideTrack"));
}
void LLCC_SystemSetModeNR(int32_t mode, bool flag) { ev("SetModeNR", {mode, flag ? 1 : 0}); }

void LLCM_CoverUpgrade(int32_t* x, int32_t* y) {
    ev("CoverUpgrade", {*x, *y});
    *x = static_cast<int32_t>(static_cast<uint32_t>(*x) + 1u);
    *y = static_cast<int32_t>(static_cast<uint32_t>(*y) - 2u);
}

int32_t LLCC_SystemGetMode() {
    ev("GetMode", {});
    return static_cast<int32_t>(ret("GetMode"));
}

void LLCC_DrawTrackLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    ev("DrawTrackLine", {static_cast<int32_t>(mode), x, y, *endX, *endY});
}

void LLCC_SystemConstructTrackClickStack(CObject* obj) { ev("ConstructTrackClickStack", {lab(obj)}); }

int16_t CTrackCell::GetSegmentNumber() const {
    ev("GetSegmentNumber", {CellIndex(this)});
    return static_cast<int16_t>(ret("GetSegmentNumber"));
}

int32_t CTrackCell::ConnectionType(CELL_MODES mode, int32_t a, uint8_t b) {
    ev("ConnectionType", {CellIndex(this), static_cast<int32_t>(mode), a, b});
    return static_cast<int32_t>(ret("ConnectionType"));
}

void LLCC_TraceTrack(CTrackCell* from, CTrackCell* to, int32_t flag) { ev("TraceTrack", {CellIndex(from), CellIndex(to), flag}); }

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_slc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"df18", &gLLCC9F18},   {"df1c", &gLLCC9F1C},   {"df20", &gLLCC9F20},   {"df24", &gLLCC9F24},   {"df26", &gLLCC9F26},
        {"e028", &gLLCCE028},   {"e02a", &gLLCCE02A},   {"e02c", &gLLCCFirstBlockX}, {"e030", &gLLCCFirstBlockY}, {"e034", &gLLCCE034},
        {"e038", &gLLCCE038},   {"hoardR", &gHoardingRight}, {"hoardB", &gHoardingBottom}, {"dc210", &gLLCCC210}, {"dc214", &gLLCCC214},
        {"dc218", &gLLCMIllegalPlacement}, {"db48", &gLLCCDB48}, {"db50", &gLLCCDB50}, {"db54", &gLLCCDB54}, {"db58", &gLLCCDB58},
        {"db5a", &gLLCCDB5A},   {"db5c", &gLLCCDB5C},   {"db68", &gLLCCDB68},   {"db6c", &gLLCCDB6C},   {"e0cc", &gLLCCE0CC},
        {"g4800", &gLLCC4800},  {"g4804", &gLLCC4804},  {"f9bc0", &gDynamicCellF9BC0}, {"f9bc4", &gDynamicCellF9BC4}, {"world", &gGameWorld},
        {"things", gThingArray}, {"db6cp", &gLLCMCurrentObject},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block (32-bit addressable).
uint64_t tpw_slc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The persistent objects (8 at most) and the ride record the fakes answer.
void tpw_slc_setup(const uint64_t* objects, uint32_t count, uint64_t info) {
    for (uint32_t i = 0; i < count && i < 8; ++i) gObjects[i] = reinterpret_cast<CObject*>(static_cast<uintptr_t>(objects[i]));
    gInfo = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(info));
}

int32_t tpw_slc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mBase) + offsetof(CThing, mCellX));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mBase) + offsetof(CThing, mCellY));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mObjectType));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mRotation));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF28));
    return n;
}

void tpw_slc_set_current(int32_t flag) { LLCM_SetCurrentObject((flag & 0xff) != 0); }   // a bool is its low byte
void tpw_slc_start(int32_t x, int32_t y, uint32_t unused) { LLCM_StartTrackEditMode(x, y, static_cast<uint16_t>(unused)); }

}  // extern "C"
