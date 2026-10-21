// Test library for the FLY base accessors (unit v5277; libtpw_unit_FLY_SetNumberOfCars.dylib, tests/test_FLY_SetNumberOfCars.py), main app:
//   src/game/rides/FlyingRide.cpp   seven functions on gFlyBases; no callees.
// gFlyBases is defined here (the program's definition is in a file this library does not compile).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "HLSprite.h"
#include "rsse.h"

using namespace calllog;

void* gFlyBases[100];

extern "C" {

uint64_t tpw_fly_addr(const char* name) {
    if (std::strcmp(name, "bases") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gFlyBases[0]));
    return 0;
}

uint64_t tpw_fly_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_fly_setbase(int32_t index, uint64_t address) { gFlyBases[index] = reinterpret_cast<void*>(static_cast<uintptr_t>(address)); }

int32_t tpw_fly_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gFlyBases[0]));
    return n;
}

void tpw_fly_SetNumberOfCars(int32_t a, int32_t b) { FLY_SetNumberOfCars(a, b); }
uint32_t tpw_fly_GetBrokenStat(int32_t a) { return static_cast<uint32_t>(FLY_GetBrokenStat(a)); }
uint32_t tpw_fly_LaunchCar(int32_t a) { return static_cast<uint32_t>(FLY_LaunchCar(a)); }
uint32_t tpw_fly_RideRunning(int32_t a) { return static_cast<uint32_t>(FLY_RideRunning(a)); }
void tpw_fly_SetOpenClosed(int32_t a, int32_t b) { FLY_SetOpenClosed(a, b); }
void tpw_fly_SetRunSpeed(int32_t a, int32_t b) { FLY_SetRunSpeed(a, b); }
void tpw_fly_SetRunTime(int32_t a, int32_t b) { FLY_SetRunTime(a, b); }

}  // extern "C"
