// Test library for the first-person ride helpers (unit v5764; libtpw_unit_IsRideRideable.dylib, tests/test_IsRideRideable.py), main app:
//   src/game/TPCamera.cpp   StopRidingRideIn1stPerson, GetThingAtPosnIfEntrance(float, float), IsRideRideable
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals
// of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files
uint32_t gFirstPersonFlags;
uint32_t gFirstPersonRideInstance;
int32_t gHeightfieldWidth;
int32_t gHeightfieldHeight;
tag_sRideAnimSet* gRideAnimSets[64];
static char gScriptDummy[8];

// ---- the fakes
int32_t BMP_GetCameraMesh(int32_t ride) {
    ev("BMP_GetCameraMesh", {ride});
    return static_cast<int32_t>(ret("BMP_GetCameraMesh"));
}
RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t script) {
    ev("RSSE_GetScriptPointer", {script});
    return ret("RSSE_GetScriptPointer") != 0 ? reinterpret_cast<RSSE_SCRIPT*>(gScriptDummy) : nullptr;
}
int32_t RSSE_GetTourBaseID(RSSE_SCRIPT* script) {
    ev("RSSE_GetTourBaseID", {script != nullptr ? 1 : 0});
    return static_cast<int32_t>(ret("RSSE_GetTourBaseID"));
}
int32_t FLY_GetCameraMesh(int32_t tour) {
    ev("FLY_GetCameraMesh", {tour});
    return static_cast<int32_t>(ret("FLY_GetCameraMesh"));
}
int32_t IsCoaster(uint32_t instance) {
    ev("IsCoaster", {static_cast<int64_t>(instance)});
    return static_cast<int32_t>(ret("IsCoaster"));
}
uint32_t GetCoaster1stPersonCar(uint32_t entrance) {
    ev("GetCoaster1stPersonCar", {static_cast<int64_t>(entrance)});
    return static_cast<uint32_t>(ret("GetCoaster1stPersonCar"));
}
uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    ev("FindDataNodeIndexByID", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(set) & 0xffffffffu), static_cast<int64_t>(mask), static_cast<int64_t>(id)});
    return static_cast<uint32_t>(ret("FindDataNodeIndexByID"));
}
CObject* GetThingAtPosnIfEntrance(int32_t gridX, int32_t gridZ) {
    ev("GetThingAtPosnIfEntrance", {gridX, gridZ});
    return reinterpret_cast<CObject*>(static_cast<uintptr_t>(ret("GetThingAtPosnIfEntrance")));
}
void ToggleRideRideIn1stPerson(int32_t ride, int32_t a, int32_t b, int32_t* out, int32_t node, int32_t c) {
    ev("ToggleRideRideIn1stPerson", {ride, a, b, node, c});
    *out = static_cast<int32_t>(ret("ToggleRideRideIn1stPerson"));
}
void MoveCameraOverRideExit() { ev("MoveCameraOverRideExit", {}); }

extern "C" {

uint64_t tpw_ir_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"flags", &gFirstPersonFlags},
        {"instance", &gFirstPersonRideInstance},
        {"width", &gHeightfieldWidth},
        {"height", &gHeightfieldHeight},
        {"sets", &gRideAnimSets[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ir_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ir_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gRideAnimSets[0]));
    return n;
}

void tpw_ir_stop() { StopRidingRideIn1stPerson(); }
uint32_t tpw_ir_get(float x, float z) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(GetThingAtPosnIfEntrance(x, z))); }
uint32_t tpw_ir_ride(uint64_t object, uint32_t animSet) {
    return static_cast<uint32_t>(IsRideRideable(reinterpret_cast<void*>(static_cast<uintptr_t>(object)), animSet));
}

}  // extern "C"
