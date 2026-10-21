// Test library for the fly bases' peep handling and repair state (unit v5980; libtpw_unit_FLY_ForceExtractPeep.dylib,
// tests/test_FLY_ForceExtractPeep.py), main app:
//   src/game/rides/FlyingRide.cpp   FLY_ForceExtractPeep, FLY_ExtractPeep, FLY_AddPeep, FLY_SetRepairState
//
// The callees outside the unit are recording fakes; the ride sets are named by their index in gRideAnimSets (1 to 3), their meshes by
// the index of the set that owns them (0: NULL, 0xffffffff: unknown). The globals of other files the unit reads are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "HLSprite.h"
#include "rsse.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "World.h"
#include "Debug.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
tag_sRideAnimSet* gRideAnimSets[8];
void* gFlyBases[100];
int32_t gSoundBanks[8];
CWorld* gGameWorld;

namespace {

int64_t SetIdx(const void* p) {
    if (p == nullptr) return 0;
    for (int i = 1; i < 4; ++i)
        if (gRideAnimSets[i] == p) return i;
    return 0xffffffffll;
}

int64_t MeshIdx(const void* p) {
    if (p == nullptr) return 0;
    for (int i = 1; i < 4; ++i)
        if (gRideAnimSets[i] != nullptr && gRideAnimSets[i]->mHeads == p) return i;
    return 0xffffffffll;
}

}  // namespace

// ---- the fakes
uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    ev("Find", {SetIdx(set), mask, id});
    return static_cast<uint32_t>(ret("Find"));
}

void DisableRideHead(tag_sMesh* mesh, uint32_t index) { ev("Disable", {MeshIdx(mesh), index}); }

void EnableRideHead(tag_sMesh* mesh, uint32_t index, uint32_t peep) { ev("Enable", {MeshIdx(mesh), index, peep}); }

uint32_t SetCurRideAnimDuration(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("Anim", {SetIdx(set), a, anim, loop, static_cast<int64_t>(fbits(speed)), b});
    return static_cast<uint32_t>(ret("Anim"));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int32_t a = va_arg(args, int32_t);
    va_end(args);
    ev("Assert", {static_cast<uint32_t>(condition), static_cast<uint32_t>(a)}, {format});
}

int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("Sound", {static_cast<uint32_t>(entity), static_cast<uint32_t>(bank), static_cast<uint32_t>(event), static_cast<uint32_t>(a),
                 static_cast<uint32_t>(b), static_cast<uint32_t>(c)});
    return 0;
}

uint32_t CWorld::GetRand() {
    ev("Rand", {});
    return static_cast<uint32_t>(ret("Rand"));
}

namespace processinstruction_callee {
int32_t Labs(int32_t value) {
    ev("Labs", {static_cast<uint32_t>(value)});
    return value < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(value)) : value;
}
}  // namespace processinstruction_callee

void PAR_KillEmitter(int32_t handle) { ev("Kill", {static_cast<uint32_t>(handle)}); }

uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("Init", {static_cast<uint32_t>(type), static_cast<uint32_t>(a), static_cast<uint32_t>(b), static_cast<uint32_t>(c)});
    return static_cast<uint32_t>(ret("Init"));
}

extern "C" {

uint64_t tpw_fly_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"rides", &gRideAnimSets[0]},
        {"bases", &gFlyBases[0]},
        {"banks", &gSoundBanks[0]},
        {"world", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fly_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Gives a ride set its mesh (the host's mHeads is not at the Mac's offset).
void tpw_fly_setmesh(uint64_t set, uint64_t mesh) {
    reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(set))->mHeads = reinterpret_cast<tag_sMesh*>(static_cast<uintptr_t>(mesh));
}

int32_t tpw_fly_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(sFlyBase));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet) <= 0x400 ? 1 : 0);
    out[n++] = static_cast<int64_t>(sizeof(gSoundBanks[0]));
    return n;
}

uint32_t tpw_fly_force(int32_t fly) { return static_cast<uint32_t>(FLY_ForceExtractPeep(fly)); }
uint32_t tpw_fly_extract(int32_t fly) { return static_cast<uint32_t>(FLY_ExtractPeep(fly)); }
uint32_t tpw_fly_add(int32_t fly, int32_t peep) { return static_cast<uint32_t>(FLY_AddPeep(fly, peep)); }
uint32_t tpw_fly_repair(int32_t fly, int32_t state) {
    FLY_SetRepairState(fly, state);
    return 0;
}

}  // extern "C"
