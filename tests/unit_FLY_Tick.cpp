// Test library for FLY_Tick (unit v5880; libtpw_unit_FLY_Tick.dylib, tests/test_FLY_Tick.py), main app:
//   src/hlsprite_zone_FLY_Tick.cpp   FLY_Tick 0x1007567c and the three lowest-height globals (0x1029d2c4, 0x1029d2c8, 0x1029d2cc)
//
// The callees outside the unit (EmployEric, SetCurRideAnim, GAME_ASSERTMSG, RSSE_GetSoundEventCode, SOUND_SetParamValue,
// GetNextHoldingWaypoint, GetNextWaypoint, CWorld::GetRand, GetWorldGSquareHeight, PAR_KillEmitter, DestroyRideInstance,
// SOUND_StopSoundEffect, SOUND_MoveSoundEffect) are fakes that record the arguments the real ones read and answer from the test's
// queues; the globals of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Advisor.h"
#include "Debug.h"
#include "HLSprite.h"
#include "MapCell.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "World.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
void* gFlyBases[100];
void* gFlySinTable;
int32_t gFlyZoneB;
int32_t gFlyZoneA;
int32_t gFlyZoneD;
int32_t gFlyZoneC;
int32_t gHeightfieldWidth;
int32_t gHeightfieldHeight;
tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances];
CWorld* gGameWorld;

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs (pointers as lab(p))
int32_t EmployEric(int32_t index) {
    ev("EmployEric", {index});
    return static_cast<int32_t>(ret("EmployEric"));
}

uint32_t SetCurRideAnimDuration(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {lab(set), static_cast<int32_t>(a), static_cast<int32_t>(anim), static_cast<int32_t>(loop),
                          static_cast<int64_t>(fbits(speed)), static_cast<int32_t>(b)});
    return static_cast<uint32_t>(ret("SetCurRideAnim"));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    const int32_t n = va_arg(ap, int32_t);
    va_end(ap);
    ev("GAME_ASSERTMSG", {condition, n}, {format});
}

int32_t RSSE_GetSoundEventCode(int32_t a, int32_t b) {
    ev("RSSE_GetSoundEventCode", {a, b});
    return static_cast<int32_t>(ret("RSSE_GetSoundEventCode"));
}

int32_t SOUND_SetParamValue(int32_t handle, uint32_t param, uint32_t value) {
    ev("SOUND_SetParamValue", {handle, static_cast<int32_t>(param), static_cast<int32_t>(value)});
    return static_cast<int32_t>(ret("SOUND_SetParamValue"));
}

void GetNextHoldingWaypoint(FLYBOY* fly) { ev("GetNextHoldingWaypoint", {lab(fly)}); }

void GetNextWaypoint(FLYBOY* fly) { ev("GetNextWaypoint", {lab(fly)}); }

uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("GetRand"));
}

float GetWorldGSquareHeight(int32_t x, int32_t y, uint32_t* extra) {
    ev("GetWorldGSquareHeight", {x, y, lab(extra)});
    const uint32_t bits = static_cast<uint32_t>(ret("GetWorldGSquareHeight"));
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

void PAR_KillEmitter(int32_t handle) { ev("PAR_KillEmitter", {handle}); }

void DestroyRideInstance(uint32_t instance) { ev("DestroyRideInstance", {static_cast<int32_t>(instance)}); }

void SOUND_StopSoundEffect(int32_t handle) { ev("SOUND_StopSoundEffect", {handle}); }

int32_t SOUND_MoveSoundEffect(int32_t handle, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_MoveSoundEffect", {handle, a, b, c});
    return static_cast<int32_t>(ret("SOUND_MoveSoundEffect"));
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_fly_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gFlyBases", &gFlyBases},
        {"gFlySinTable", &gFlySinTable},
        {"gFlyZoneA", &gFlyZoneA},
        {"gFlyZoneB", &gFlyZoneB},
        {"gFlyZoneC", &gFlyZoneC},
        {"gFlyZoneD", &gFlyZoneD},
        {"gFlyApproachMinHeight", &gFlyApproachMinHeight},
        {"gFlyTurnMinHeight", &gFlyTurnMinHeight},
        {"gFlyTakeOffMinHeight", &gFlyTakeOffMinHeight},
        {"gHeightfieldWidth", &gHeightfieldWidth},
        {"gHeightfieldHeight", &gHeightfieldHeight},
        {"gRideAnimSets", &gRideAnimSets},
        {"gGameWorld", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable).
uint64_t tpw_fly_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_fly_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(sFlyBase));
    out[n++] = static_cast<int64_t>(offsetof(sFlyBase, mSlotArea));
    out[n++] = static_cast<int64_t>(sizeof(void*));
    out[n++] = static_cast<int64_t>(sizeof(gRideAnimSets[0]));
    return n;
}

void tpw_fly_tick() { FLY_Tick(); }

}  // extern "C"
