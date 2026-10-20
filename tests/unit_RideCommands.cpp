// Test library for a ride script's ride-specific commands (unit w172; libtpw_unit_RideCommands.dylib, tests/test_RideCommands.py), main app:
//   src/rsse_commands.cpp  ExecuteTourSpecificCommand, ExecuteBumperSpecificCommand, ExecuteCoasterSpecificCommand, __sinit_rsse_cpp
// src/rsse_objects.cpp is linked for ReadScript and ExtractData (the dispatchers call them); its other callees are fakes copied from
// tests/unit_RideObjects.cpp, unused here. Every ride callee (FLY_, BMP_, Coaster, LLCC_, GetCoasterInstanceID, FindDataNodeIndexByID) is a fake that
// records its arguments (the same event names and arguments as the module's PowerPC stubs) and answers from the case's queue of its name.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "Debug.h"
#include "HLSprite.h"
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


// ---- the ride callees of the three dispatchers: each records its name and arguments and answers from the case's queue of its own name
// (a void callee answers nothing). FLY_RemoveBase is declared in HLSprite.h (its translation, src/hlsprite_flybase.cpp, is not linked).
void FLY_RemoveBase(int32_t fly) { ev("FLY_RemoveBase", {fly}); }
void FLY_SetRunTime(int32_t fly, int32_t time) { ev("FLY_SetRunTime", {fly, time}); }
void CoasterSetSpeed(uint32_t coaster, uint32_t speed) { ev("CoasterSetSpeed", {coaster, speed}); }

int32_t FLY_CreateBase(uint32_t thingWord, int32_t handle, int32_t x, int32_t y, int32_t z, int32_t angle, int32_t a) {
    ev("FLY_CreateBase", {thingWord, handle, x, y, z, angle, a});
    return static_cast<int32_t>(ret("FLY_CreateBase"));
}
int32_t FLY_AddPeep(int32_t fly, int32_t peep) {
    ev("FLY_AddPeep", {fly, peep});
    return static_cast<int32_t>(ret("FLY_AddPeep"));
}
int32_t FLY_ExtractPeep(int32_t fly) {
    ev("FLY_ExtractPeep", {fly});
    return static_cast<int32_t>(ret("FLY_ExtractPeep"));
}
int32_t FLY_GetBrokenStat(int32_t fly) {
    ev("FLY_GetBrokenStat", {fly});
    return static_cast<int32_t>(ret("FLY_GetBrokenStat"));
}
int32_t FLY_GetCarInBase(int32_t fly) {
    ev("FLY_GetCarInBase", {fly});
    return static_cast<int32_t>(ret("FLY_GetCarInBase"));
}
int32_t FLY_LaunchCar(int32_t fly) {
    ev("FLY_LaunchCar", {fly});
    return static_cast<int32_t>(ret("FLY_LaunchCar"));
}
int32_t FLY_RideRunning(int32_t fly) {
    ev("FLY_RideRunning", {fly});
    return static_cast<int32_t>(ret("FLY_RideRunning"));
}
int32_t FLY_SetCarSize(int32_t fly, int32_t a) {
    ev("FLY_SetCarSize", {fly, a});
    return static_cast<int32_t>(ret("FLY_SetCarSize"));
}
int32_t FLY_SetMotionScale(int32_t fly, int32_t a) {
    ev("FLY_SetMotionScale", {fly, a});
    return static_cast<int32_t>(ret("FLY_SetMotionScale"));
}
int32_t FLY_SetNumberOfCars(int32_t fly, int32_t a) {
    ev("FLY_SetNumberOfCars", {fly, a});
    return static_cast<int32_t>(ret("FLY_SetNumberOfCars"));
}
int32_t FLY_SetOpenClosed(int32_t fly, int32_t a) {
    ev("FLY_SetOpenClosed", {fly, a});
    return static_cast<int32_t>(ret("FLY_SetOpenClosed"));
}
int32_t FLY_SetRepairState(int32_t fly, int32_t a) {
    ev("FLY_SetRepairState", {fly, a});
    return static_cast<int32_t>(ret("FLY_SetRepairState"));
}
int32_t FLY_SetRunway(int32_t fly, int32_t a) {
    ev("FLY_SetRunway", {fly, a});
    return static_cast<int32_t>(ret("FLY_SetRunway"));
}

int32_t BMP_AddJoiningListToBump(int32_t ride) {
    ev("BMP_AddJoiningListToBump", {ride});
    return static_cast<int32_t>(ret("BMP_AddJoiningListToBump"));
}
int32_t BMP_AddPeepToRideJoiningList(int32_t ride, uint32_t peep) {
    ev("BMP_AddPeepToRideJoiningList", {ride, peep});
    return static_cast<int32_t>(ret("BMP_AddPeepToRideJoiningList"));
}
int32_t BMP_BeginRide(int32_t ride) {
    ev("BMP_BeginRide", {ride});
    return static_cast<int32_t>(ret("BMP_BeginRide"));
}
int32_t BMP_BreakRide(int32_t ride) {
    ev("BMP_BreakRide", {ride});
    return static_cast<int32_t>(ret("BMP_BreakRide"));
}
int32_t BMP_CloseRide(int32_t ride) {
    ev("BMP_CloseRide", {ride});
    return static_cast<int32_t>(ret("BMP_CloseRide"));
}
int32_t BMP_FixRide(int32_t ride) {
    ev("BMP_FixRide", {ride});
    return static_cast<int32_t>(ret("BMP_FixRide"));
}
int32_t BMP_GetCarsOnRide(int32_t ride) {
    ev("BMP_GetCarsOnRide", {ride});
    return static_cast<int32_t>(ret("BMP_GetCarsOnRide"));
}
int32_t BMP_HaltRide(int32_t ride) {
    ev("BMP_HaltRide", {ride});
    return static_cast<int32_t>(ret("BMP_HaltRide"));
}
int32_t BMP_LAUNCH(int32_t ride, int32_t a) {
    ev("BMP_LAUNCH", {ride, a});
    return static_cast<int32_t>(ret("BMP_LAUNCH"));
}
int32_t BMP_OpenRide(int32_t ride) {
    ev("BMP_OpenRide", {ride});
    return static_cast<int32_t>(ret("BMP_OpenRide"));
}
int32_t BMP_PollForPeeps(int32_t ride) {
    ev("BMP_PollForPeeps", {ride});
    return static_cast<int32_t>(ret("BMP_PollForPeeps"));
}
int32_t BMP_REMOVEBUMP(int32_t ride, int32_t a) {
    ev("BMP_REMOVEBUMP", {ride, a});
    return static_cast<int32_t>(ret("BMP_REMOVEBUMP"));
}
int32_t BMP_RideWorn(int32_t ride) {
    ev("BMP_RideWorn", {ride});
    return static_cast<int32_t>(ret("BMP_RideWorn"));
}
int32_t BMP_SetRideDuration(int32_t ride, int32_t duration) {
    ev("BMP_SetRideDuration", {ride, duration});
    return static_cast<int32_t>(ret("BMP_SetRideDuration"));
}

uint32_t CoasterAddPeep(uint32_t coaster, uint32_t peep) {
    ev("CoasterAddPeep", {coaster, peep});
    return static_cast<uint32_t>(ret("CoasterAddPeep"));
}
uint32_t CoasterGetPeep(uint32_t coaster) {
    ev("CoasterGetPeep", {coaster});
    return static_cast<uint32_t>(ret("CoasterGetPeep"));
}
uint32_t CoasterGetQueueSpace(uint32_t coaster) {
    ev("CoasterGetQueueSpace", {coaster});
    return static_cast<uint32_t>(ret("CoasterGetQueueSpace"));
}
uint32_t CoasterSetCapacity(uint32_t coaster, int32_t capacity) {
    ev("CoasterSetCapacity", {coaster, capacity});
    return static_cast<uint32_t>(ret("CoasterSetCapacity"));
}
uint32_t CoasterSetOpenClosedState(uint32_t coaster, int32_t state) {
    ev("CoasterSetOpenClosedState", {coaster, state});
    return static_cast<uint32_t>(ret("CoasterSetOpenClosedState"));
}
uint32_t CoasterSetRepairState(uint32_t coaster, uint32_t state) {
    ev("CoasterSetRepairState", {coaster, state});
    return static_cast<uint32_t>(ret("CoasterSetRepairState"));
}
int32_t GetCoasterInstanceID(int32_t handle) {
    ev("GetCoasterInstanceID", {handle});
    return static_cast<int32_t>(ret("GetCoasterInstanceID"));
}
void LLCC_SetTrackAnim(CObject* object, bool flag) { ev("LLCC_SetTrackAnim", {lab(object), flag}); }
void LLCC_SetTrackAnimReverse(CObject* object, bool flag) { ev("LLCC_SetTrackAnimReverse", {lab(object), flag}); }

namespace {

// The thing objects the module places in the thing table: raw storage for a CObject (nothing constructs it; only mF24 and mF28 are read).
constexpr int kRows = 8;
alignas(16) unsigned char gObjectStorage[kRows][sizeof(CObject) + 16];

}  // namespace

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_rcm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"W00", &gRsseWord102F7C00},
        {"W04", &gRsseWord102F7C04},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold), and the arena's base.
uint64_t tpw_rcm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
uint64_t tpw_rcm_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase())); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_rcm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(RSSE_SCRIPT));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mCode));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mVars));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mCommandAnswer));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mFE6));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mThingId));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mFAE));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mSpeed));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mCoaster));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mFlyHandle));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF24));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF28));
    return n;
}

// Thing row `row` of the thing table: no thing, or an object whose mF24 and mF28 are the given numbers. Answers the object's address.
uint64_t tpw_rcm_thing(uint32_t row, int32_t present, int32_t f24, int32_t f28) {
    if (present == 0) {
        gThingArray[row].mThing = nullptr;
        return 0;
    }
    CObject* object = reinterpret_cast<CObject*>(gObjectStorage[row]);
    std::memset(gObjectStorage[row], 0, sizeof(gObjectStorage[row]));
    object->mF24 = f24;
    object->mF28 = f28;
    gThingArray[row].mThing = object;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(object));
}

// Builds ride set `slot` into gRideAnimSets[slot]: the mesh's word mF10, then per embedded record (kRecs of them) 1 when it has a block and the 16
// words of the block (d). Answers the set's address.
uint64_t tpw_rcm_ride(int32_t slot, int32_t f10, const int64_t* d) {
    const int64_t* p = d;
    for (int i = 0; i < kRecs; ++i) {
        const bool hasBlock = *p++ != 0;
        for (uint32_t& w : gBlocks[slot][i]) w = static_cast<uint32_t>(*p++);
        gRecords[slot][i].mFlags = 0;
        gRecords[slot][i].mBlock = hasBlock ? reinterpret_cast<uint8_t*>(gBlocks[slot][i]) : nullptr;
        gRecords[slot][i].mF08 = 0;
        gRecords[slot][i].mF0C = 0;
        gRecords[slot][i].mF10 = 0;
    }
    std::memset(static_cast<void*>(&gEmbedded[slot]), 0, sizeof(gEmbedded[slot]));
    gEmbedded[slot].mRecords = gRecords[slot];
    std::memset(static_cast<void*>(&gMeshes[slot]), 0, sizeof(gMeshes[slot]));
    gMeshes[slot].mF28 = &gEmbedded[slot];
    gMeshes[slot].mF10 = f10;
    std::memset(static_cast<void*>(&gSets[slot]), 0, sizeof(gSets[slot]));
    gSets[slot].mSelf = &gSets[slot];
    gSets[slot].mHeads = &gMeshes[slot];
    gRideAnimSets[slot] = &gSets[slot];
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gSets[slot]));
}

// ---- the functions
uint64_t tpw_rcm_Tour(uint64_t script) {
    ExecuteTourSpecificCommand(reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(script)));
    return 0;
}
uint64_t tpw_rcm_Bumper(uint64_t script) {
    ExecuteBumperSpecificCommand(reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(script)));
    return 0;
}
uint64_t tpw_rcm_Coaster(uint64_t script) {
    ExecuteCoasterSpecificCommand(reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(script)));
    return 0;
}

}  // extern "C"
