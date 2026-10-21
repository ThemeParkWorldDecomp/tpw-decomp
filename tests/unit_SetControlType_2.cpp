// Test library for SetControlType (unit v5922; libtpw_unit_SetControlType_2.dylib, tests/test_SetControlType_2.py), main app:
//   src/game/rides/BumperRide.cpp   SetControlType
//
// The callees outside the unit (GetRand, FindDataNodeIndexByID, GetRideInfo, InstanceRide, TrackRideIdToThingId, GAME_ASSERTMSG,
// SND_SendIndirectedEvent, SOUND_MoveSoundEffect, FindTargetBump) are recording fakes that answer from the test's queues. The globals
// of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"
#include "analyser.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "Advisor.h"
#include "World.h"
#include "Debug.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
// The list (read up to +0x24) and the ride database (never touched by a fake) are storage with the program's symbol names.
extern "C" {
alignas(16) uint8_t gColListStorage[0x80] __asm__("_gColList");
alignas(16) uint8_t gRideDatabaseStorage[0x1000] __asm__("_gRideDatabase");
}
uint16_t gAdvisorThingId;
CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gGameWorld;
int32_t gSoundBanks[8];
tag_sRideAnimSet* gRideAnimSets[8];
int32_t gHostSineTable[512];

namespace {
alignas(16) uint8_t gRideInfoStorage[0x1000];
}

// ---- the fakes: the same event name and arguments as the module's PowerPC stub
uint32_t CWorld::GetRand() {
    ev("GetRand", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this))});
    return static_cast<uint32_t>(ret("GetRand"));
}

uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    ev("FindDataNodeIndexByID", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(set)), mask, id});
    return static_cast<uint32_t>(ret("FindDataNodeIndexByID"));
}

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {id.mIndex});
    RideInfo* info = reinterpret_cast<RideInfo*>(gRideInfoStorage);
    info->mSubRideTypes[0] = static_cast<uint32_t>(ret("GetRideInfo"));
    return info;
}

uint32_t InstanceRide(uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, int32_t b, void* c, int32_t d, int32_t e,
                      uint32_t f, int32_t g) {
    ev("InstanceRide", {type, flags, static_cast<int64_t>(reinterpret_cast<uintptr_t>(params)), a, static_cast<uint32_t>(b),
                        static_cast<int64_t>(reinterpret_cast<uintptr_t>(c)), static_cast<uint32_t>(d), static_cast<uint32_t>(e), f,
                        static_cast<uint32_t>(g)});
    return static_cast<uint32_t>(ret("InstanceRide"));
}

void TrackRideIdToThingId(uint16_t* out, int32_t rideId) {
    ev("TrackRideIdToThingId", {static_cast<uint32_t>(rideId)});
    *out = static_cast<uint16_t>(ret("TrackRideIdToThingId"));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {static_cast<uint32_t>(condition) & 0xff}, {format});
}

int32_t SND_SendIndirectedEvent(int32_t a, int32_t entity, int32_t category, int32_t b, int32_t p5, int32_t p6, int32_t p7) {
    ev("SND_SendIndirectedEvent", {static_cast<uint32_t>(a), static_cast<uint32_t>(entity), static_cast<uint32_t>(category),
                                   static_cast<uint32_t>(b), static_cast<uint32_t>(p5), static_cast<uint32_t>(p6),
                                   static_cast<uint32_t>(p7)});
    return static_cast<int32_t>(ret("SND_SendIndirectedEvent"));
}

int32_t SOUND_MoveSoundEffect(int32_t handle, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_MoveSoundEffect", {static_cast<uint32_t>(handle), static_cast<uint32_t>(a), static_cast<uint32_t>(b),
                                 static_cast<uint32_t>(c)});
    return static_cast<int32_t>(ret("SOUND_MoveSoundEffect"));
}

tagBUMP* FindTargetBump(tagBUMP* bump, tagBUMPRIDE* ride, int32_t* index) {
    uint8_t* list = gColListStorage;
    const uint32_t bumps = *reinterpret_cast<uint32_t*>(list + 0x10);
    const uint32_t rides = *reinterpret_cast<uint32_t*>(list + 8);
    ev("FindTargetBump", {static_cast<uint32_t>(tpw::ArenaOffset(bump) - bumps), static_cast<uint32_t>(tpw::ArenaOffset(ride) - rides)});
    if (ret("FindTargetBump") == 0) return nullptr;
    *index = static_cast<int32_t>(ret("FindTargetBumpIndex"));
    return bump;
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_sct_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"list", gColListStorage},
        {"world", &gGameWorld},
        {"sine", gHostSineTable},
        {"banks", gSoundBanks},
        {"sets", gRideAnimSets},
        {"things", gThingArray},
        {"advisor", &gAdvisorThingId},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sct_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_sct_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_sct_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet*));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMP));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPRIDE));
    return n;
}

void tpw_sct_run(uint64_t ride, uint64_t bump) {
    SetControlType(reinterpret_cast<tagBUMPRIDE*>(ride), reinterpret_cast<tagBUMP*>(bump));
}

}  // extern "C"
