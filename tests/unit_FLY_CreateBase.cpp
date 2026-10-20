// Test library for the fly base creation (unit v5236; libtpw_unit_FLY_CreateBase.dylib, tests/test_FLY_CreateBase.py), main app:
//   src/hlsprite_flies_init_FLY_CreateBase.cpp   FLY_CreateBase, EmployEric
//
// The callees outside the unit are recording fakes (LbMem_Alloc, CWorld::GetRand, SND_SendIndirectedEvent, GAME_ASSERTMSG,
// CRideDatabase::GetRideInfo, InstanceRide, SetCurRideAnimDuration, FindDataNodeIndexByID); the globals of other files are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "analyser.h"
#include "HLSprite.h"
#include "LbFile.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "World.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files, defined here
void* gFlyBases[100];
int32_t gFlyZoneB = 0x11170;
int32_t gFlyZoneA = 0x222e0;
int32_t gFlyZoneD = 0x2e630;
int32_t gFlyZoneC = 0x7530;
CWorld* gGameWorld;
int32_t gSoundBanks[8];
CThingArrayEntry gThingArray[kThingArraySize];
tag_sRideAnimSet* gRideAnimSets[16];
CRideDatabase gRideDatabase;

namespace {
uint8_t gRideInfoBlock[0x800];
}

// ---- the fakes
void* LbMem_Alloc(uint32_t size) {
    ev("LbMem_Alloc", {static_cast<int64_t>(size)});
    void* block = tpw::ArenaAllocZeroed(size);
    std::memset(block, 0xa5, size);
    return block;
}

uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("GetRand"));
}

int32_t SND_SendIndirectedEvent(int32_t a, int32_t entity, int32_t category, int32_t b, int32_t p5, int32_t p6, int32_t p7) {
    ev("SND_SendIndirectedEvent", {a, entity, category, b, p5, p6, p7});
    return static_cast<int32_t>(ret("SND_SendIndirectedEvent"));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return reinterpret_cast<RideInfo*>(gRideInfoBlock);
}

uint32_t InstanceRide(uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, int32_t b, void* c, int32_t d, int32_t e,
                      uint32_t f, int32_t g) {
    ev("InstanceRide", {type, flags, lab(params), a, b, lab(c), d, e, f, g});
    return static_cast<uint32_t>(ret("InstanceRide"));
}

uint32_t SetCurRideAnimDuration(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {lab(set), a, anim, loop, static_cast<int64_t>(fbits(speed)), b});
    return static_cast<uint32_t>(ret("SetCurRideAnim"));
}

uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    ev("FindDataNodeIndexByID", {lab(set), mask, id});
    return static_cast<uint32_t>(ret("FindDataNodeIndexByID"));
}

extern "C" {

uint64_t tpw_fe_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"zb", &gFlyZoneB}, {"za", &gFlyZoneA}, {"zd", &gFlyZoneD}, {"zc", &gFlyZoneC},
        {"world", &gGameWorld}, {"banks", gSoundBanks}, {"ri", gRideInfoBlock}, {"db", &gRideDatabase},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fe_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_fe_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(sFlyBase));
    return n;
}

void tpw_fe_setbase(int32_t i, uint64_t p) { gFlyBases[i] = reinterpret_cast<void*>(static_cast<uintptr_t>(p)); }
uint64_t tpw_fe_getbase(int32_t i) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gFlyBases[i])); }
void tpw_fe_setthing(int32_t i, uint64_t p) { gThingArray[i].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(p)); }
void tpw_fe_setset(int32_t i, uint64_t p) { gRideAnimSets[i] = reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(p)); }

int32_t tpw_fe_create(uint32_t thing, int32_t handle, int32_t x, int32_t y, int32_t z, int32_t angle, int32_t a) {
    return FLY_CreateBase(thing, handle, x, y, z, angle, a);
}
int32_t tpw_fe_employ(int32_t index) { return EmployEric(index); }

}  // extern "C"
