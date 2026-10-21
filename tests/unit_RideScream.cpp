// Test library for a ride script's screams and DrawPeep (unit w171; libtpw_unit_RideScream.dylib, tests/test_RideScream.py), main app:
//   src/rsse_scream.cpp   SCREAM_Start, SCREAM_SetLevel, SCREAM_Move, SCREAM_Stop, SCREAM_Single, SCREAM_SingleSad,
//                         TriggerRepairParticleEffect, DrawPeep, RSSE_GetSoundEventCode, RSSE_GetCoasterHandle
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues: the sound events,
// the emitters, GetNodePos (it fills the point from the queues "pos:<n>"), GetRideHeightAtPoint, GetRideMasterBoundArea, the ride database,
// the peep's sprite, the clock and the assertion. The globals of other files the unit reads or writes are defined here, zero at start like
// the Mac's bss. The ride set's chain to the node height carries native pointers on the host, so the library builds it (tpw_rsc_ride); the
// script records and the things are arena blocks the test fills with Mac-offset words.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "calllog.h"
#include "Clock.h"
#include "Debug.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
tag_sRideAnimSet* gRideAnimSets[16];
CThingArrayEntry gThingArray[kThingArraySize];
int32_t gSoundBanks[8];
RSSEState gRSSEState;
// gTPGlobals, gClock and gRideDatabase as raw storage under the symbols' names (nothing constructs them).
#ifdef __APPLE__
extern unsigned char gGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");
extern unsigned char gClockStorage[sizeof(CClock)] __asm__("_gClock");
extern unsigned char gDatabaseStorage[sizeof(CRideDatabase)] __asm__("_gRideDatabase");
#else
extern unsigned char gGlobalsStorage[sizeof(TPGlobals)] __asm__("gTPGlobals");
extern unsigned char gClockStorage[sizeof(CClock)] __asm__("gClock");
extern unsigned char gDatabaseStorage[sizeof(CRideDatabase)] __asm__("gRideDatabase");
#endif
alignas(16) unsigned char gGlobalsStorage[sizeof(TPGlobals)] = {};
alignas(16) unsigned char gClockStorage[sizeof(CClock)] = {};
alignas(16) unsigned char gDatabaseStorage[sizeof(CRideDatabase)] = {};

namespace {

// A float's bits with every NaN made one value (the two processors make different NaNs).
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float BitsFloat(int64_t bits) {
    const uint32_t b = static_cast<uint32_t>(bits);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

// What the fake ride database answers: a record whose repair effect slot the test sets.
alignas(16) unsigned char gInfoStorage[sizeof(RideInfo)];

}  // namespace

// ---- the fakes: the same event name and arguments as the module's PowerPC stubs (pointers as lab(p))
int32_t CClock::Read() {
    ev("Clock");
    return static_cast<int32_t>(ret("clock", 0));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return static_cast<int32_t>(ret("Event", 0));
}

int32_t SOUND_SetParamValue(int32_t handle, uint32_t param, uint32_t value) {
    ev("SOUND_SetParamValue", {handle, static_cast<int32_t>(param), static_cast<int32_t>(value)});
    return static_cast<int32_t>(ret("Param", 0));
}

int32_t SOUND_MoveSoundEffect(int32_t handle, int32_t x, int32_t y, int32_t z) {
    ev("SOUND_MoveSoundEffect", {handle, x, y, z});
    return static_cast<int32_t>(ret("Move", 0));
}

void SOUND_FadeSoundEffect(int32_t handle) { ev("SOUND_FadeSoundEffect", {handle}); }

uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {type, a, b, c});
    return static_cast<uint32_t>(ret("emit", 0));
}

void PAR_SetEmitterLife(int32_t handle, int32_t life) { ev("PAR_SetEmitterLife", {handle, life}); }

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return reinterpret_cast<RideInfo*>(gInfoStorage);
}

int32_t GetRideMasterBoundArea(uint32_t ride, sAlignedBox* box) {
    ev("GetRideMasterBoundArea", {static_cast<int32_t>(ride)});
    const int32_t answer = static_cast<int32_t>(ret("bound", 0));
    if (answer != 0) {
        const char* keys[6] = {"box0", "box1", "box2", "box3", "box4", "box5"};
        for (int i = 0; i < 3; ++i) {
            box->mMin[i] = BitsFloat(ret(keys[i], 0));
            box->mMax[i] = BitsFloat(ret(keys[3 + i], 0));
        }
    }
    return answer;
}

// The point of a node: three floats from the queues "pos:<k>", k picked by the node's low two bits, the mask and the axis.
int32_t GetNodePos(RSSE_SCRIPT* script, JCPOINT3D* pos, JCPOINT3D* dir, int32_t node, int32_t mask) {
    ev("GetNodePos", {lab(script), node, mask, dir != nullptr ? 1 : 0});
    const int64_t k = (node & 3) * 6 + (mask == 0x80 ? 3 : 0);
    pos->x = BitsFloat(ret("pos", k, 0));
    pos->y = BitsFloat(ret("pos", k + 1, 0));
    pos->z = BitsFloat(ret("pos", k + 2, 0));
    return 1;
}

void GetRideHeightAtPoint(float x, float z, float& y) {
    ev("GetRideHeightAtPoint", {CanonBits(x), CanonBits(z)});
    y = BitsFloat(ret("height", 0));
}

void CPeep::DrawMySpriteAt(float x, float y, float z, int32_t frame, CDelayedScript::ScriptIdx script) {
    ev("DrawMySpriteAt", {lab(this), CanonBits(x), CanonBits(y), CanonBits(z), frame, static_cast<int32_t>(script)});
}

void CPeep::HideMySprite() { ev("HideMySprite", {lab(this)}); }

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_rsc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gRideAnimSets", &gRideAnimSets},
        {"gThingArray", &gThingArray},
        {"gTPGlobals", &gTPGlobals},
        {"gSoundBanks", &gSoundBanks},
        {"gRSSEState", &gRSSEState},
        {"gRepairEffectGuard", &gRepairEffectGuard},
        {"gRepairEffectWord", &gRepairEffectWord},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_rsc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset of an arena block: what a Ptr32 field holds.
uint32_t tpw_rsc_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_rsc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(RSSE_SCRIPT));
    out[n++] = static_cast<int64_t>(sizeof(RSSE_WALK));
    out[n++] = static_cast<int64_t>(sizeof(JCPOINT3D));
    out[n++] = static_cast<int64_t>(offsetof(TPGlobals, mSinTableMask));
    out[n++] = static_cast<int64_t>(offsetof(TPGlobals, mSinTableScale));
    out[n++] = static_cast<int64_t>(offsetof(TPGlobals, mSinTable));
    out[n++] = static_cast<int64_t>(sizeof(TPGlobals::mSinTable));
    out[n++] = static_cast<int64_t>(sizeof(sAlignedBox));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mObjectType));
    out[n++] = static_cast<int64_t>(offsetof(RSSEState, mScripts));
    return n;
}

// The effect word the fake ride record answers in the slot of the repair particle effect.
void tpw_rsc_setinfo(int32_t effect) {
    reinterpret_cast<RideInfo*>(gInfoStorage)->Value(kRideSlot_RepairParticleEffect).mInteger = effect;
}

// Builds ride set `k` (in gRideAnimSets) whose root node height (the float +0x44 of the header's node mF78) is rootY.
uint64_t tpw_rsc_ride(int32_t k, float rootY) {
    auto* set = static_cast<tag_sRideAnimSet*>(std::calloc(1, sizeof(tag_sRideAnimSet)));
    auto* mesh = static_cast<tag_sMesh*>(std::calloc(1, sizeof(tag_sMesh)));
    auto* header = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    auto* node = static_cast<tag_sNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sNode)));
    node->mMatrix.m[3][1] = rootY;
    header->mF78 = node;
    mesh->mHeader = header;
    set->mHeads = mesh;
    gRideAnimSets[k] = set;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(set));
}
void tpw_rsc_ride_clear(int32_t k) { gRideAnimSets[k] = nullptr; }

// Puts a block in row `index` of gThingArray (0 clears it).
void tpw_rsc_thing(int32_t index, uint64_t object) { gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(object)); }

// The script system's state: initialised flag and the list head (a native pointer on the host).
void tpw_rsc_state(int32_t initialised, uint64_t head) {
    gRSSEState.mInitialised = initialised;
    gRSSEState.mScripts = reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(head));
}

// ---- the functions, one wrapper each with C types only (pointers as uint64_t)
static RSSE_SCRIPT* S(uint64_t p) { return reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(p)); }
int32_t tpw_rsc_start(int32_t h, int32_t count, int32_t a, int32_t b, int32_t p, int32_t q, int32_t r) { return SCREAM_Start(h, count, a, b, p, q, r); }
int32_t tpw_rsc_setlevel(int32_t h, int32_t a, int32_t b) { return SCREAM_SetLevel(h, a, b); }
int32_t tpw_rsc_move(int32_t h, int32_t x, int32_t y, int32_t z) { return SCREAM_Move(h, x, y, z); }
void tpw_rsc_stop(int32_t h) { SCREAM_Stop(h); }
int32_t tpw_rsc_single(int32_t count, int32_t a, int32_t b, int32_t p, int32_t q, int32_t r) { return SCREAM_Single(count, a, b, p, q, r); }
int32_t tpw_rsc_singlesad(int32_t count, int32_t p, int32_t q, int32_t r) { return SCREAM_SingleSad(count, p, q, r); }
void tpw_rsc_repair(uint64_t script, int32_t on) { TriggerRepairParticleEffect(S(script), on); }
void tpw_rsc_drawpeep(uint64_t script, uint64_t walk, int32_t from, int32_t to, int32_t mode, int32_t fraction) {
    DrawPeep(S(script), reinterpret_cast<RSSE_WALK*>(static_cast<uintptr_t>(walk)), from, to, mode, fraction);
}
int32_t tpw_rsc_eventcode(int32_t handle, int32_t index) { return RSSE_GetSoundEventCode(handle, index); }
uint32_t tpw_rsc_coaster(int32_t handle) { return RSSE_GetCoasterHandle(handle); }

}  // extern "C"
