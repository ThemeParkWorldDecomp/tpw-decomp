// Test library for the ride upgrade effect (unit v5020; libtpw_unit_RSSE_UpgradeRideEffect.dylib, tests/test_RSSE_UpgradeRideEffect.py), main app:
//   src/game/rides/rsse.cpp   RSSE_UpgradeRideEffect and gRSSEUpgradeSkipThing
//
// The callees outside the unit (GAME_ASSERTMSG, CRideDatabase::GetRideInfo, GetRideMasterBoundArea, PAR_InitEmitter,
// PAR_SetEmitterRandomPos, SOUND_SendEvent) are recording fakes; the globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ride_layout.h"
#include "Debug.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "m3d2.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
RSSEState gRSSEState = {};
CThingArrayEntry gThingArray[kThingArraySize];
tag_sRideAnimSet* gRideAnimSets[64];
int32_t gSoundBanks[8];
CRideDatabase gRideDatabase;

namespace {
RideInfo* gInfoBlock = nullptr;
}

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("Assert", {condition}, {format});
}

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return gInfoBlock;
}

int32_t GetRideMasterBoundArea(uint32_t ride, sAlignedBox* box) {
    int32_t answer = static_cast<int32_t>(ret("Bound"));
    ev("Bound", {static_cast<int64_t>(ride), answer});
    if (answer != 0) {
        float values[6];
        for (float& v : values) {
            uint32_t bits = static_cast<uint32_t>(ret("BoundF"));
            std::memcpy(&v, &bits, 4);
        }
        std::memcpy(box->mMin, values, 12);
        std::memcpy(box->mMax, values + 3, 12);
    }
    return answer;
}

uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {type, a, b, c});
    return static_cast<uint32_t>(ret("PAR_InitEmitter"));
}

void PAR_SetEmitterRandomPos(int32_t handle, int32_t x, int32_t y, int32_t z) {
    ev("PAR_SetEmitterRandomPos", {handle, x, y, z});
}

int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return 0;
}

extern "C" {

uint64_t tpw_ur_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"state", &gRSSEState},
        {"skip", &gRSSEUpgradeSkipThing},
        {"bank2", &gSoundBanks[2]},
        {"db", &gRideDatabase},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ur_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ur_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mHandle));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mThingId));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mRideAnim));
    out[n++] = static_cast<int64_t>(sizeof(RSSE_SCRIPT));
    return n;
}

// World set-up that holds native pointers.
void tpw_ur_reset() {
    std::memset(gThingArray, 0, sizeof(gThingArray));
    std::memset(gRideAnimSets, 0, sizeof(gRideAnimSets));
    std::memset(gSoundBanks, 0, sizeof(gSoundBanks));
    gRSSEState = {};
    gInfoBlock = nullptr;
}
void tpw_ur_set_head(uint64_t script) { gRSSEState.mScripts = reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(script)); }
void tpw_ur_link(uint64_t a, uint64_t b) {
    reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(a))->mNext = reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(b));
}
void tpw_ur_set_thing(uint32_t id, uint32_t type) {
    CObject* object = reinterpret_cast<CObject*>(tpw::ArenaAllocZeroed(sizeof(CObject)));
    object->mObjectType = static_cast<uint16_t>(type);
    gThingArray[id].mThing = object;
}
void tpw_ur_set_info(uint64_t block) { gInfoBlock = reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(block)); }
void tpw_ur_make_anim(uint32_t index, uint32_t floatbits) {
    tag_sRideAnimSet* set = reinterpret_cast<tag_sRideAnimSet*>(tpw::ArenaAllocZeroed(sizeof(tag_sRideAnimSet)));
    tag_sMesh* mesh = reinterpret_cast<tag_sMesh*>(tpw::ArenaAllocZeroed(sizeof(tag_sMesh)));
    tag_sM3D2Header* header = reinterpret_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    tag_sNode* node = reinterpret_cast<tag_sNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sNode)));
    std::memcpy(reinterpret_cast<char*>(node) + 0x44, &floatbits, 4);
    header->mF78 = node;
    mesh->mHeader = header;
    set->mHeads = mesh;
    gRideAnimSets[index] = set;
}

void tpw_ur_call(int32_t handle) { RSSE_UpgradeRideEffect(handle); }

}  // extern "C"
