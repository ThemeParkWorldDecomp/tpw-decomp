// Test library for the coaster train loading (unit v5900; libtpw_unit_UnloadTrain_2.dylib, tests/test_UnloadTrain_2.py), main app:
//   src/game/rides/CoasterQueue.cpp   UnloadTrain, KickPeepsFromEntranceQueue, LoadTrain, CoasterSetCapacity, SetEntranceLinearSamples,
//                                         GetCarsInStation, HandleTrainLoading (and the two bss globals it defines)
//
// The callees outside the unit (EnableRideHead, DisableRideHead, SetTrainCapacity) are fakes that record their arguments (a mesh as the id in
// its first word). The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "LevelInit.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
void* gObjectTable[0x100];
tag_sRideAnimSet* gRideAnimSets[64];
int32_t gLevelRandom;

// ---- the fakes
void DisableRideHead(tag_sMesh* mesh, uint32_t index) { ev("DisableRideHead", {static_cast<int64_t>(mesh->mF00), static_cast<int64_t>(index)}); }

void EnableRideHead(tag_sMesh* mesh, uint32_t index, uint32_t peep) {
    ev("EnableRideHead", {static_cast<int64_t>(mesh->mF00), static_cast<int64_t>(index), static_cast<int64_t>(peep)});
}

void SetTrainCapacity(tag_sCoasterInstance* instance, uint32_t a, uint32_t b, uint32_t c) {
    ev("SetTrainCapacity", {lab(instance), static_cast<int64_t>(a), static_cast<int64_t>(b), static_cast<int64_t>(c)});
}

extern "C" {

uint64_t tpw_u_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"table", gObjectTable},
        {"sets", gRideAnimSets},
        {"capfloor", &gCoasterCapacityFloor},
        {"kickfloor", &gKickPeepsFloor},
        {"seed", &gLevelRandom},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_u_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets, in the order of the module's MAC table.
int32_t tpw_u_layout(int64_t* out) {
    int32_t n = 0;
#define OFF(S, M) out[n++] = static_cast<int64_t>(offsetof(S, M))
#define SIZE(S) out[n++] = static_cast<int64_t>(sizeof(S))
    OFF(tag_sCoasterInstance, mType);
    OFF(tag_sCoasterInstance, mState);
    OFF(tag_sCoasterInstance, mFAC);
    OFF(tag_sCoasterInstance, mQueueB0);
    OFF(tag_sCoasterInstance, mQueueD0);
    OFF(tag_sCoasterInstance, mFF0);
    OFF(tag_sCoasterInstance, mFF8);
    OFF(tag_sCoasterInstance, mFFC);
    OFF(tag_sCoasterInstance, mTrains);
    OFF(tag_sCoasterInstance, mLastBrace);
    SIZE(tag_sCoasterInstance);
    OFF(sWraparoundQueue, mCapacity);
    OFF(sWraparoundQueue, mBlock);
    OFF(sWraparoundQueue, mF08);
    OFF(sTrainInstance, mFlags);
    OFF(sTrainInstance, mType);
    OFF(sTrainInstance, mF08);
    OFF(sTrainInstance, mCarsInUse);
    OFF(sTrainInstance, mF10);
    OFF(sTrainInstance, mF14);
    OFF(sTrainInstance, mF18);
    OFF(sTrainInstance, mF1C);
    OFF(sTrainInstance, mFrontSample);
    OFF(sTrainInstance, mRearSample);
    OFF(sTrainInstance, mCars);
    SIZE(sTrainInstance);
    OFF(sCarInstance, mType);
    OFF(sCarInstance, mRide);
    OFF(sCarInstance, mCurrentA);
    OFF(sCarInstance, mCurrentB);
    OFF(sCarInstance, mCountA);
    OFF(sCarInstance, mCountB);
    OFF(sCarInstance, mF40);
    OFF(sCarInstance, mF44);
    SIZE(sCarInstance);
    OFF(sCarType, mF24);
    OFF(sCarType, mF28);
    OFF(sCarType, mBlock);
    SIZE(sCarType);
    OFF(tag_sCoasterType, mF2F8);
    OFF(tag_sCoasterType, mF318);
    OFF(tag_sCoasterType, mF31C);
    out[n++] = 0;   // the train's type pointer is the type itself on the host
    SIZE(tag_sCoasterType);
    OFF(tag_sRideAnimSet, mHeads);
    SIZE(tag_sRideAnimSet);
    OFF(tag_sTrackBrace, mPrev);
    OFF(tag_sTrackBrace, mF90);
    SIZE(tag_sTrackBrace);
    OFF(sBraceSpline, mControlsA);
    SIZE(sBraceSpline);
    OFF(tag_sBezierControl, mF08);
    SIZE(tag_sBezierControl);
#undef OFF
#undef SIZE
    return n;
}

// ---- the functions (pointers as uint64_t, everything else widened)
void tpw_u_kick(uint64_t inst) { KickPeepsFromEntranceQueue(reinterpret_cast<tag_sCoasterInstance*>(inst)); }

void tpw_u_unload(uint64_t inst, uint64_t train, uint64_t flag) {
    UnloadTrain(reinterpret_cast<tag_sCoasterInstance*>(inst), reinterpret_cast<sTrainInstance*>(train), static_cast<int32_t>(flag));
}

void tpw_u_load(uint64_t inst, uint64_t train, uint64_t first, uint64_t count) {
    LoadTrain(reinterpret_cast<tag_sCoasterInstance*>(inst), reinterpret_cast<sTrainInstance*>(train), static_cast<uint32_t>(first),
              static_cast<uint32_t>(count));
}

uint32_t tpw_u_setcap(uint64_t coaster, uint64_t capacity) {
    return CoasterSetCapacity(static_cast<uint32_t>(coaster), static_cast<int32_t>(capacity));
}

void tpw_u_entr(uint64_t inst) { SetEntranceLinearSamples(reinterpret_cast<tag_sCoasterInstance*>(inst)); }

void tpw_u_cars(uint64_t inst, uint64_t train, uint64_t first, uint64_t last) {
    GetCarsInStation(reinterpret_cast<tag_sCoasterInstance*>(inst), reinterpret_cast<sTrainInstance*>(train),
                     reinterpret_cast<uint32_t*>(first), reinterpret_cast<uint32_t*>(last));
}

void tpw_u_handle(uint64_t inst, uint64_t train) {
    HandleTrainLoading(reinterpret_cast<tag_sCoasterInstance*>(inst), reinterpret_cast<sTrainInstance*>(train));
}

}  // extern "C"
