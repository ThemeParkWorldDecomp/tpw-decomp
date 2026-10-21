// Test library for the bump removal (unit v5087; libtpw_unit_BMP_REMOVEBUMP.dylib, tests/test_BMP_REMOVEBUMP.py), main app:
//   src/game/rides/BumperRide.cpp   BMP_REMOVEBUMP, BMP_DOCK, RemovePeepsFromBump, FindTargetBump (and gAdvisorThingId)
//
// The callees outside the unit are recording fakes (same event names and arguments as the PowerPC stubs, answers from the
// test's queues); the globals of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "BMP_InsideArea.h"
#include "Debug.h"
#include "Ptr32.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "World.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
tabCOLLIST gColList;
CWorld* gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];
tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances];
int32_t gSoundBanks[8];

// ---- the fakes
uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("GetRand"));
}

int32_t DoAreasIntersect(tabCOLOBJECT* a, tabCOLOBJECT* b) {
    ev("DoAreasIntersect", {lab(a), lab(b)});
    return static_cast<int32_t>(ret("DoAreasIntersect"));
}

void TrackRideIdToThingId(uint16_t* out, int32_t rideId) {
    ev("TrackRideIdToThingId", {rideId});
    *out = static_cast<uint16_t>(ret("TrackRideIdToThingId"));
}

void DisableRideHead(tag_sMesh* mesh, uint32_t index) { ev("DisableRideHead", {lab(mesh), static_cast<int64_t>(index)}); }

uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {type, a, b, c});
    return 0;
}

void PAR_KillEmitter(int32_t handle) { ev("PAR_KillEmitter", {handle}); }

void DestroyRideInstance(uint32_t instance) { ev("DestroyRideInstance", {static_cast<int64_t>(instance)}); }

int32_t SND_SendIndirectedEvent(int32_t a, int32_t entity, int32_t category, int32_t b, int32_t p5, int32_t p6, int32_t p7) {
    ev("SND_SendIndirectedEvent", {a, entity, category, b, p5, p6, p7});
    return static_cast<int32_t>(ret("SND_SendIndirectedEvent"));
}

int32_t SOUND_MoveSoundEffect(int32_t handle, int32_t x, int32_t y, int32_t z) {
    ev("SOUND_MoveSoundEffect", {handle, x, y, z});
    return static_cast<int32_t>(ret("SOUND_MoveSoundEffect"));
}

void SOUND_FadeSoundEffect(int32_t handle) { ev("SOUND_FadeSoundEffect", {handle}); }

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition, format[0] == 'T' ? 1 : 2});
}

void TRACE(const char* format, ...) {
    if (std::strncmp(format, "Peep", 4) == 0) {
        va_list ap;
        va_start(ap, format);
        const int32_t a = va_arg(ap, int32_t);
        const int32_t b = va_arg(ap, int32_t);
        va_end(ap);
        ev("TRACE", {1, a, b});
    } else {
        ev("TRACE", {2, 0, 0});
    }
}

extern "C" {

uint64_t tpw_bmp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"list", &gColList},         {"thingarray", gThingArray}, {"ridesets", gRideAnimSets},
        {"banks", gSoundBanks},      {"advisor", &gAdvisorThingId}, {"gameworld", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bmp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_bmp_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_bmp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    out[n++] = static_cast<int64_t>(sizeof(tag_sM3D2Header));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(sizeof(tabCOLLIST));
    return n;
}

int32_t tpw_bmp_remove(int32_t ride, int32_t a) { return BMP_REMOVEBUMP(ride, a); }
int32_t tpw_bmp_dock(uint64_t bump) { return BMP_DOCK(reinterpret_cast<tagBUMP*>(bump)); }
int32_t tpw_bmp_peeps(uint64_t bump, int32_t emit) { return RemovePeepsFromBump(reinterpret_cast<tagBUMP*>(bump), emit); }
uint64_t tpw_bmp_find(uint64_t bump, uint64_t ride, uint64_t out) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        FindTargetBump(reinterpret_cast<tagBUMP*>(bump), reinterpret_cast<tagBUMPRIDE*>(ride), reinterpret_cast<int32_t*>(out))));
}

}  // extern "C"
