// Test library for the bump sound and ride record functions (unit v5966; libtpw_unit_BMP_ControlSound.dylib, tests/test_BMP_ControlSound.py), main app:
//   src/game/rides/BumperRide.cpp   BMP_ControlSound, FreeBuoysForRide, BMP_GetCarsOnRide, BMP_SetRideModelSelection
//
// The callees outside the unit are fakes that record what the real ones read; the world (gColList, 0x3c bytes), the thing array, the
// advisor id and the sound banks are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdarg>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"
#include "TPSound.h"
#include "World.h"
#include "Debug.h"

using namespace calllog;

// ---- globals of other files, defined here. The world is bigger than tabCOLLIST: the symbol is the same.
struct BigWorld {
    uint32_t words[16];
};
BigWorld gWorldBig __asm__("_gColList");
uint16_t gAdvisorThingId;
int32_t gSoundBanks[8];
CThingArrayEntry gThingArray[kThingArraySize];

// ---- the fakes
void SingleSoundAtBump(tagBUMP* bump, int32_t event) { ev("SingleSoundAtBump", {lab(bump), event}); }

void TRACE(const char* format, ...) { ev("TRACE", {}, {format}); }

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

void MineToSystemCords(int32_t x, int32_t y, NS_MacDoze::tagPOINT* out) {
    ev("MineToSystemCords", {x, y});
    int32_t* p = reinterpret_cast<int32_t*>(out);
    p[0] = static_cast<int32_t>(ret("pt_x"));
    p[1] = static_cast<int32_t>(ret("pt_y"));
}

void TrackRideIdToThingId(uint16_t* out, int32_t rideId) {
    ev("TrackRideIdToThingId", {rideId});
    *out = static_cast<uint16_t>(ret("tid"));
}

int32_t SND_SendIndirectedEvent(int32_t a, int32_t entity, int32_t category, int32_t b, int32_t p5, int32_t p6, int32_t p7) {
    ev("SND_SendIndirectedEvent", {a, entity, category, b, p5, p6, p7});
    return 0;
}

extern "C" {

uint64_t tpw_cs_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gWorldBig},
        {"banks", &gSoundBanks[0]},
        {"thingarr", &gThingArray[0]},
        {"adv", &gAdvisorThingId},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_cs_off(uint64_t address) { return static_cast<uint32_t>(tpw::ArenaOffset(reinterpret_cast<const void*>(address))); }

void tpw_cs_setthing(uint32_t index, uint64_t thing) { gThingArray[index].mThing = reinterpret_cast<void*>(thing); }

int32_t tpw_cs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(BigWorld));
    out[n++] = static_cast<int64_t>(sizeof(gAdvisorThingId));
    return n;
}

void tpw_cs_sound(uint64_t bump) { BMP_ControlSound(reinterpret_cast<tagBUMP*>(bump)); }
void tpw_cs_free(uint64_t ride, int32_t n) { FreeBuoysForRide(reinterpret_cast<tagBUMPRIDE*>(ride), n); }
int32_t tpw_cs_cars(int32_t ride) { return BMP_GetCarsOnRide(ride); }
void tpw_cs_set(int32_t ride, int32_t a, int32_t b) { BMP_SetRideModelSelection(ride, a, b); }

}  // extern "C"
