// Test library for the entrance camera and collision object helpers (unit v51013; libtpw_unit_GetThingAtPosnIfEntrance.dylib,
// tests/test_GetThingAtPosnIfEntrance.py), main app:
//   src/game/TPCamera.cpp, src/game/rides/BumperRide.cpp   GetThingAtPosnIfEntrance(int, int), MoveCameraOverRideExit,
//   FreeTrackForRide, BMP_StartColObject, BMP_StartColObjectForRide
//
// The callees outside the unit are fakes recording the arguments the real ones read: GAME_ASSERTMSG, TRACE, CMapCell::IsEntrance,
// CObject::GetDBInfo, CCellBase::GetDirection and GetThingAtPosnIfEntrance(float, float). Globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "BMP_InsideArea.h"
#include "CellBase.h"
#include "Debug.h"
#include "MapCell.h"
#include "TPCamera.h"
#include "ThingReceivers.h"
#include "World.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ride_layout.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CWorld* gGameWorld = nullptr;
CWorld*& gGameWorldRef = gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];
float gCameraPosition[3];
float gCameraF5BF4;
uint16_t gThingListEnd;

// The collision world (the unit reads +0x2c), with room after it; one block under the symbol the header declares.
struct ColBlock {
    tabCOLLIST list;
    uint32_t tail[16];
};
ColBlock gColBlock __asm__("_gColList");

namespace {

void* gRet[4];   // 0, 1: the ride records GetDBInfo answers; 2: the thing GetThingAtPosnIfEntrance(float, float) answers

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return static_cast<int32_t>(bits);
}

int32_t CellIndex(const void* cell) {
    const char* map = reinterpret_cast<const char*>(gGameWorld) + 0x2d8;
    return static_cast<int32_t>((reinterpret_cast<const char*>(cell) - map) / 0x44);
}

}  // namespace

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

void TRACE(const char* format, ...) { ev("TRACE", {}, {format}); }

int32_t CMapCell::IsEntrance() const {
    ev("IsEntrance", {CellIndex(this)});
    return static_cast<int32_t>(ret("IsEntrance"));
}

RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {static_cast<int32_t>(*reinterpret_cast<const uint32_t*>(reinterpret_cast<const char*>(this) + 0x20))});
    return static_cast<RideInfo*>(gRet[ret("GetDBInfo") & 1]);
}

uint8_t CCellBase::GetDirection() const {
    ev("GetDirection", {CellIndex(this)});
    return static_cast<uint8_t>(ret("GetDirection"));
}

CObject* GetThingAtPosnIfEntrance(float x, float z) {
    ev("GetThingAtPosn", {CanonBits(x), CanonBits(z)});
    return static_cast<CObject*>(gRet[2]);
}

extern "C" {

uint64_t tpw_gtp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"end", &gThingListEnd},
        {"camera", gCameraPosition},
        {"angle", &gCameraF5BF4},
        {"collist", &gColBlock},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_gtp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_gtp_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

void tpw_gtp_set_world(uint64_t address) { gGameWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(address)); }

void tpw_gtp_set_ret(int32_t key, uint64_t address) { gRet[key] = reinterpret_cast<void*>(static_cast<uintptr_t>(address)); }

void tpw_gtp_set_thing(int32_t index, uint64_t address) { gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(address)); }

int32_t tpw_gtp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tabCOLOBJECT));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPRIDE));
    out[n++] = static_cast<int64_t>(offsetof(tabCOLLIST, first));
    out[n++] = static_cast<int64_t>(offsetof(tabCOLLIST, count));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    return n;
}

uint64_t tpw_gtp_get(int32_t x, int32_t z) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(GetThingAtPosnIfEntrance(x, z))); }

void tpw_gtp_move() { MoveCameraOverRideExit(); }

void tpw_gtp_free(uint64_t ride) { FreeTrackForRide(reinterpret_cast<tagBUMPRIDE*>(static_cast<uintptr_t>(ride))); }

uint64_t tpw_gtp_start(int32_t x, int32_t y, int32_t radius) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(BMP_StartColObject(x, y, radius)));
}

void tpw_gtp_startride(uint64_t ride, int32_t x, int32_t y, int32_t shape) {
    BMP_StartColObjectForRide(reinterpret_cast<tagBUMPRIDE*>(static_cast<uintptr_t>(ride)), x, y, shape);
}

}  // extern "C"
