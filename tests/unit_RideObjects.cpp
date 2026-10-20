// Test library for the script objects of the ride scripts (unit w150; libtpw_unit_RideObjects.dylib, tests/test_RideObjects.py), main app:
//   src/rsse_objects.cpp   ExtractData, CreateObject, KillObject, KillLinkedObject, SetObjParam, StartEvent, InitObject, ProcessObjects,
//                          ReadScript, Push, Pop, RSSE_Setvar, RSSE_SetRideSpeed, RSSE_GetRideSpeed, RSSE_GetTourBaseID, GetRidePosition,
//                          GetRideCentrePosition
//
// The callees outside the unit are fakes that record the arguments the real ones read (the same event names and arguments as the
// module's PowerPC stubs) and answer from the test's queues. The globals of other files are defined here. The script record, its arrays
// and the objects come from the arena (the module fills them by Mac offset); the ride sets are built here (their pointers are native).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "Debug.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here (the program's definitions are in files this library does not compile)
RSSEState gRSSEState;
int32_t gRSSEF7BE0;
tag_sRideAnimSet* gRideAnimSets[16];
CRideDatabase gRideDatabase;
CThingArrayEntry gThingArray[kThingArraySize];
int32_t gSoundBanks[8];

namespace {

constexpr int64_t kKeep = 0x5a5a5a5a;   // a queue value that means "leave it alone"
alignas(16) unsigned char gInfoBlock[sizeof(RideInfo) + 0x40];
float gMatrix[16];

// A float written from its bits.
void PutBits(float* where, int64_t bits) {
    const uint32_t word = static_cast<uint32_t>(bits);
    std::memcpy(where, &word, sizeof(word));
}

// The ride sets the module builds: a set, its mesh, the mesh's embedded set with kRecs records and their 0x40-byte blocks, and the
// header with the node mF78 (in the arena, since the header's link is a Ptr32).
constexpr int kSlots = 4;
constexpr int kRecs = 8;
tag_sRideAnimSet gSets[kSlots];
tag_sMesh gMeshes[kSlots];
sEmbeddedSet gEmbedded[kSlots];
sEmbeddedRecord gRecords[kSlots][kRecs];
uint32_t gBlocks[kSlots][kRecs][16];
tag_sM3D2Header* gHeaders[kSlots];
void* gNodes[kSlots];

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub (pointers as lab(p))
void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int32_t value = va_arg(args, int32_t);
    va_end(args);
    ev("CTRACE", {static_cast<int32_t>(level), value}, {format});
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

void PAR_SetEmitterLife(int32_t handle, int32_t life) { ev("PAR_SetEmitterLife", {handle, life}); }
void PAR_SetEmitterScale(int32_t handle, int32_t scale) { ev("PAR_SetEmitterScale", {handle, scale}); }
void PAR_SetEmitterPos(int32_t handle, int32_t x, int32_t y, int32_t z) { ev("PAR_SetEmitterPos", {handle, x, y, z}); }
void PAR_SetEmitterLaunchVector(int32_t handle, int32_t x, int32_t y, int32_t z) { ev("PAR_SetEmitterLaunchVector", {handle, x, y, z}); }
uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {type, a, b, c});
    return static_cast<uint32_t>(ret("PAR_InitEmitter"));
}
uint32_t PAR_InitEmitterLaunch(int32_t type, int32_t x, int32_t y, int32_t z, int32_t vx, int32_t vy, int32_t vz) {
    ev("PAR_InitEmitterLaunch", {type, x, y, z, vx, vy, vz});
    return static_cast<uint32_t>(ret("PAR_InitEmitterLaunch"));
}

int32_t SOUND_SetParamValue(int32_t handle, uint32_t param, uint32_t value) {
    ev("SOUND_SetParamValue", {handle, param, value});
    return static_cast<int32_t>(ret("SOUND_SetParamValue"));
}
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return static_cast<int32_t>(ret("SOUND_SendEvent"));
}
int32_t SOUND_MoveSoundEffect(int32_t handle, int32_t x, int32_t y, int32_t z) {
    ev("SOUND_MoveSoundEffect", {handle, x, y, z});
    return static_cast<int32_t>(ret("SOUND_MoveSoundEffect"));
}
void SOUND_StopSoundEffect(int32_t handle) { ev("SOUND_StopSoundEffect", {handle}); }
void SOUND_FadeSoundEffect(int32_t handle) { ev("SOUND_FadeSoundEffect", {handle}); }

uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    ev("FindDataNodeIndexByID", {lab(set), mask, id});
    return static_cast<uint32_t>(ret("FindDataNodeIndexByID"));
}

// The node position: answers from the queue and, when it answers nonzero, writes the position (and the direction) from the queues.
int32_t GetNodePos(RSSE_SCRIPT* script, JCPOINT3D* pos, JCPOINT3D* dir, int32_t node, int32_t mask) {
    ev("GetNodePos", {lab(script), pos != nullptr, dir != nullptr, node, mask});
    const int32_t answer = static_cast<int32_t>(ret("GetNodePos", 1));
    if (answer != 0) {
        PutBits(&pos->x, ret("gp0"));
        PutBits(&pos->y, ret("gp1"));
        PutBits(&pos->z, ret("gp2"));
        if (dir != nullptr) {
            PutBits(&dir->x, ret("gd0"));
            PutBits(&dir->y, ret("gd1"));
            PutBits(&dir->z, ret("gd2"));
        }
    }
    return answer;
}

// The ride database: answers the info block (label 60 on both sides) or NULL.
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return ret("GetRideInfo", 1) != 0 ? reinterpret_cast<RideInfo*>(gInfoBlock) : nullptr;
}
uint32_t RideInfo::GetCustomParticleEffect(int32_t index) const {
    ev("RideInfo::GetCustomParticleEffect", {lab(this), index});
    return static_cast<uint32_t>(ret("RideInfo::GetCustomParticleEffect"));
}
int32_t RideInfo::GetCustomSoundBank() const {
    ev("RideInfo::GetCustomSoundBank", {lab(this)});
    return static_cast<int32_t>(ret("RideInfo::GetCustomSoundBank"));
}

// The matrix helpers of ProcessObjects. NormaliseVector writes a scripted answer into the vector (the queues nv0..nv2, unless they hold kKeep).
void NormaliseVector(s3DPoint* vector) {
    ev("NormaliseVector", {fbits(vector->x), fbits(vector->y), fbits(vector->z)});
    const int64_t x = ret("nv0", kKeep);
    const int64_t y = ret("nv1", kKeep);
    const int64_t z = ret("nv2", kKeep);
    if (x != kKeep) PutBits(&vector->x, x);
    if (y != kKeep) PutBits(&vector->y, y);
    if (z != kKeep) PutBits(&vector->z, z);
}
// The queue gdnwm_flag may change the node's record flags (to show that ProcessObjects reads them after the call); gdnwm picks the matrix or NULL.
float* GetDataNodeWorldMatrix(tag_sMesh*& mesh, uint32_t& index) {
    ev("GetDataNodeWorldMatrix", {lab(mesh), index});
    const int64_t flags = ret("gdnwm_flag", kKeep);
    if (flags != kKeep) static_cast<sEmbeddedSet*>(mesh->mF28)->mRecords[index].mFlags = static_cast<uint32_t>(flags);
    return ret("gdnwm", -1) < 0 ? nullptr : gMatrix;
}
tag_sRideAnimSet** GetRideFromID(uint32_t& id) {
    ev("GetRideFromID", {id});
    return &gRideAnimSets[ret("grfi", 1)];
}
int32_t GetRideMasterBoundArea(uint32_t ride, sAlignedBox* box) {
    ev("GetRideMasterBoundArea", {ride});
    const int32_t answer = static_cast<int32_t>(ret("grmba", 0));
    if (answer != 0) {
        for (int i = 0; i < 3; ++i) {
            char key[8] = {'b', 'x', static_cast<char>('0' + i), 0};
            PutBits(&box->mMin[i], ret(key));
            key[1] = 'y';
            PutBits(&box->mMax[i], ret(key));
        }
    }
    return answer;
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_ro_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"F7BE0", &gRSSEF7BE0},
        {"STATE", &gRSSEState},
        {"BANKS", gSoundBanks},
        {"RIDEDB", &gRideDatabase},
        {"INFO", gInfoBlock},
        {"MATRIX", gMatrix},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold), and the arena's base.
uint64_t tpw_ro_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
uint64_t tpw_ro_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase())); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_ro_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(RSSE_SCRIPT));
    out[n++] = static_cast<int64_t>(sizeof(RSSE_OBJECT));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mCode));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mObjects));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mStack));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mSpeed));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_OBJECT, mF18));
    out[n++] = static_cast<int64_t>(offsetof(RSSEState, mF04));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mObjectType));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    return n;
}

// The thing table's row and the matrix ProcessObjects is handed.
void tpw_ro_thing(uint32_t row, uint64_t thing) { gThingArray[row].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(thing)); }
void tpw_ro_matrix(const uint32_t* words) { std::memcpy(gMatrix, words, sizeof(gMatrix)); }

// Builds ride set `slot` into gRideAnimSets[slot] from the numbers d: 20 words of the node (the header's mF78), the record count, then per
// record its flags, 1 when it has a block, and the 16 words of the block. Answers the addresses of the set, the mesh, the records.
void tpw_ro_ride(int32_t slot, const int64_t* d, uint64_t* out) {
    tpw::ArenaFree(gNodes[slot]);
    tpw::ArenaFree(gHeaders[slot]);
    gNodes[slot] = tpw::ArenaAllocZeroed(0x50);
    gHeaders[slot] = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    uint32_t words[20];
    for (int i = 0; i < 20; ++i) words[i] = static_cast<uint32_t>(d[i]);
    std::memcpy(gNodes[slot], words, sizeof(words));
    gHeaders[slot]->mF78.set(reinterpret_cast<tag_sNode*>(gNodes[slot]));
    const int count = static_cast<int>(d[20]);
    const int64_t* p = d + 21;
    for (int i = 0; i < count; ++i) {
        gRecords[slot][i].mFlags = static_cast<uint32_t>(*p++);
        const bool hasBlock = *p++ != 0;
        for (uint32_t& w : gBlocks[slot][i]) w = static_cast<uint32_t>(*p++);
        gRecords[slot][i].mBlock = hasBlock ? reinterpret_cast<uint8_t*>(gBlocks[slot][i]) : nullptr;
        gRecords[slot][i].mF08 = 0;
        gRecords[slot][i].mF0C = 0;
        gRecords[slot][i].mF10 = 0;
    }
    std::memset(static_cast<void*>(&gEmbedded[slot]), 0, sizeof(gEmbedded[slot]));
    gEmbedded[slot].mRecords = gRecords[slot];
    std::memset(static_cast<void*>(&gMeshes[slot]), 0, sizeof(gMeshes[slot]));
    gMeshes[slot].mHeader = gHeaders[slot];
    gMeshes[slot].mF28 = &gEmbedded[slot];
    std::memset(static_cast<void*>(&gSets[slot]), 0, sizeof(gSets[slot]));
    gSets[slot].mSelf = &gSets[slot];
    gSets[slot].mHeads = &gMeshes[slot];
    gRideAnimSets[slot] = &gSets[slot];
    out[0] = reinterpret_cast<uintptr_t>(&gSets[slot]);
    out[1] = reinterpret_cast<uintptr_t>(&gMeshes[slot]);
    out[2] = reinterpret_cast<uintptr_t>(gRecords[slot]);
}
void tpw_ro_clear_rides() { std::memset(static_cast<void*>(gRideAnimSets), 0, sizeof(gRideAnimSets)); }

// ---- the functions, one wrapper each with the same five integer arguments (pointers as uint64_t); void functions answer 0
#define ARG32(x) static_cast<int32_t>(x)
#define PTR(T, x) reinterpret_cast<T*>(static_cast<uintptr_t>(x))
#define W(x) static_cast<uint64_t>(static_cast<uint32_t>(x))
#define P(x) static_cast<uint64_t>(reinterpret_cast<uintptr_t>(x))

uint64_t tpw_ro_ExtractData(uint64_t a, uint64_t b, uint64_t, uint64_t, uint64_t) { return W(ExtractData(PTR(RSSE_SCRIPT, a), ARG32(b))); }
uint64_t tpw_ro_CreateObject(uint64_t a, uint64_t, uint64_t, uint64_t, uint64_t) { return P(CreateObject(PTR(RSSE_SCRIPT, a))); }
uint64_t tpw_ro_KillObject(uint64_t a, uint64_t b, uint64_t, uint64_t, uint64_t) { return P(KillObject(PTR(RSSE_SCRIPT, a), PTR(RSSE_OBJECT, b))); }
uint64_t tpw_ro_KillLinkedObject(uint64_t a, uint64_t b, uint64_t c, uint64_t, uint64_t) {
    KillLinkedObject(PTR(RSSE_SCRIPT, a), PTR(RSSE_OBJECT, b), ARG32(c));
    return 0;
}
uint64_t tpw_ro_SetObjParam(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t) {
    SetObjParam(PTR(RSSE_SCRIPT, a), PTR(RSSE_OBJECT, b), ARG32(c), ARG32(d));
    return 0;
}
uint64_t tpw_ro_StartEvent(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e) {
    return W(StartEvent(PTR(RSSE_SCRIPT, a), ARG32(b), ARG32(c), ARG32(d), ARG32(e)));
}
uint64_t tpw_ro_InitObject(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e) {
    InitObject(PTR(RSSE_SCRIPT, a), PTR(RSSE_OBJECT, b), ARG32(c), ARG32(d), ARG32(e));
    return 0;
}
uint64_t tpw_ro_ProcessObjects(uint64_t a, uint64_t, uint64_t, uint64_t, uint64_t) {
    ProcessObjects(PTR(RSSE_SCRIPT, a));
    return 0;
}
uint64_t tpw_ro_ReadScript(uint64_t a, uint64_t, uint64_t, uint64_t, uint64_t) { return W(ReadScript(PTR(RSSE_SCRIPT, a))); }
uint64_t tpw_ro_Push(uint64_t a, uint64_t b, uint64_t, uint64_t, uint64_t) {
    Push(PTR(RSSE_SCRIPT, a), ARG32(b));
    return 0;
}
uint64_t tpw_ro_Pop(uint64_t a, uint64_t, uint64_t, uint64_t, uint64_t) { return W(Pop(PTR(RSSE_SCRIPT, a))); }
uint64_t tpw_ro_RSSE_Setvar(uint64_t a, uint64_t b, uint64_t c, uint64_t, uint64_t) {
    RSSE_Setvar(PTR(RSSE_SCRIPT, a), ARG32(b), ARG32(c));
    return 0;
}
uint64_t tpw_ro_RSSE_SetRideSpeed(uint64_t a, uint64_t b, uint64_t, uint64_t, uint64_t) {
    RSSE_SetRideSpeed(PTR(RSSE_SCRIPT, a), ARG32(b));
    return 0;
}
uint64_t tpw_ro_RSSE_GetRideSpeed(uint64_t a, uint64_t, uint64_t, uint64_t, uint64_t) { return W(RSSE_GetRideSpeed(PTR(RSSE_SCRIPT, a))); }
uint64_t tpw_ro_RSSE_GetTourBaseID(uint64_t a, uint64_t, uint64_t, uint64_t, uint64_t) { return W(RSSE_GetTourBaseID(PTR(RSSE_SCRIPT, a))); }
uint64_t tpw_ro_GetRidePosition(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t) {
    GetRidePosition(PTR(tag_sRideAnimSet, a), *PTR(float, b), *PTR(float, c), *PTR(float, d));
    return 0;
}
uint64_t tpw_ro_GetRideCentrePosition(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t) {
    return W(GetRideCentrePosition(*PTR(uint32_t, a), *PTR(float, b), *PTR(float, c), *PTR(float, d)));
}

}  // extern "C"
