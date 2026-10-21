// Test library for the bump car's move (unit v5627; libtpw_unit_BMP_Move.dylib, tests/test_BMP_Move.py), main app:
//   src/BMP_Move.cpp   BMP_Move; it defines gBumpTurnMax and gBumpTurnMin
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues (PAR_InitEmitter,
// PossitionSound1, TrackRideIdToThingId, GAME_ASSERTMSG, RSSE_GetSoundEventCode, SOUND_SetParamValue, GetRideFromID,
// GetDataNodeWorldMatrix, NormaliseVector, PAR_InitEmitterLaunch, PAR_SetEmitterPos, FlushDeferredAnim, SetCurRideAnim, BMP_MaskCar,
// GetCurrentAnim, GetDeferredAnim). The globals of other files the unit reads are defined here, zero at start like the Mac's bss.
// The list at 0x102b56c0 is longer on the Mac than tabCOLLIST (BMP_Move reads its word 0x38), so the header's declaration is renamed
// while it is read and gColList is defined here as 0x40 words.
#define gColList gColList_header_declaration
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"
#include "Debug.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "World.h"
#include "m3d2.h"
#include "rsse.h"
#undef gColList

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
alignas(16) uint32_t gColList[0x40];
uint16_t gAdvisorThingId;
CThingArrayEntry gThingArray[kThingArraySize];
tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances];

namespace {
float* gMatrixAnswer;   // what GetDataNodeWorldMatrix answers when its queue says 1 (set by the test)
}

// ---- the fakes
uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {type, a, b, c});
    return static_cast<uint32_t>(ret("PAR_InitEmitter"));
}

void PossitionSound1(tagBUMP* bump) { ev("PossitionSound1", {lab(bump)}); }

void TrackRideIdToThingId(uint16_t* out, int32_t rideId) {
    ev("TrackRideIdToThingId", {rideId});
    *out = static_cast<uint16_t>(ret("Thing"));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

int32_t RSSE_GetSoundEventCode(int32_t a, int32_t b) {
    ev("RSSE_GetSoundEventCode", {a, b});
    return static_cast<int32_t>(ret("Code"));
}

int32_t SOUND_SetParamValue(int32_t handle, uint32_t param, uint32_t value) {
    ev("SOUND_SetParamValue", {handle, param, value});
    return static_cast<int32_t>(ret("Snd"));
}

tag_sRideAnimSet** GetRideFromID(uint32_t& id) {
    ev("GetRideFromID", {id});
    return &gRideAnimSets[ret("Ride")];
}

float* GetDataNodeWorldMatrix(tag_sMesh*& mesh, uint32_t& index) {
    ev("GetDataNodeWorldMatrix", {lab(mesh), index});
    return ret("Mat") != 0 ? gMatrixAnswer : nullptr;
}

void NormaliseVector(s3DPoint* vector) {
    ev("NormaliseVector", {fbits(vector->x), fbits(vector->y), fbits(vector->z)});
    uint32_t x = static_cast<uint32_t>(ret("NormX")), y = static_cast<uint32_t>(ret("NormY")), z = static_cast<uint32_t>(ret("NormZ"));
    std::memcpy(&vector->x, &x, 4);
    std::memcpy(&vector->y, &y, 4);
    std::memcpy(&vector->z, &z, 4);
}

uint32_t PAR_InitEmitterLaunch(int32_t type, int32_t x, int32_t y, int32_t z, int32_t vx, int32_t vy, int32_t vz) {
    ev("PAR_InitEmitterLaunch", {type, x, y, z, vx, vy, vz});
    return static_cast<uint32_t>(ret("Launch"));
}

void PAR_SetEmitterPos(int32_t handle, int32_t x, int32_t y, int32_t z) { ev("PAR_SetEmitterPos", {handle, x, y, z}); }

void FlushDeferredAnim(tag_sRideAnimSet* set, uint32_t flag) { ev("FlushDeferredAnim", {lab(set), flag}); }

void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {lab(set), a, anim, loop, fbits(speed), b});
}

void BMP_MaskCar(tagBUMP* bump, int32_t mask) { ev("BMP_MaskCar", {lab(bump), mask}); }

uint32_t GetCurrentAnim(tag_sRideAnimSet* set, uint32_t* kind, uint32_t* index, uint32_t control) {
    ev("GetCurrentAnim", {lab(set), control});
    *kind = static_cast<uint32_t>(ret("CurKind"));
    *index = static_cast<uint32_t>(ret("CurIdx"));
    return 0;   // BMP_Move does not read the answer
}

void GetDeferredAnim(tag_sRideAnimSet* set, uint32_t* kind, uint32_t* index, uint32_t control) {
    ev("GetDeferredAnim", {lab(set), control});
    *kind = static_cast<uint32_t>(ret("DefKind"));
    *index = static_cast<uint32_t>(ret("DefIdx"));
}

extern "C" {

uint64_t tpw_bm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gColList", &gColList},           {"gAdvisorThingId", &gAdvisorThingId}, {"gThingArray", &gThingArray},
        {"gRideAnimSets", &gRideAnimSets}, {"gBumpTurnMax", &gBumpTurnMax},       {"gBumpTurnMin", &gBumpTurnMin},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_bm_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase())); }

void tpw_bm_set_matrix(uint64_t matrix) { gMatrixAnswer = reinterpret_cast<float*>(static_cast<uintptr_t>(matrix)); }

int32_t tpw_bm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF28));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    out[n++] = static_cast<int64_t>(offsetof(tagBumpHeadTable, entries));
    out[n++] = static_cast<int64_t>(sizeof(tagBumpHeadEntry));
    out[n++] = static_cast<int64_t>(offsetof(tagBumpHeadEntry, node));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet*));
    out[n++] = static_cast<int64_t>(offsetof(tagBumpHeadNode, x));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mMatrix));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMP));
    return n;
}

void tpw_bm_move(uint64_t bump) { BMP_Move(reinterpret_cast<tagBUMP*>(static_cast<uintptr_t>(bump))); }

}  // extern "C"
