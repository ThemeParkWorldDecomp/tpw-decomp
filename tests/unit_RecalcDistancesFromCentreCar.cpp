// Test library for the trains' capacity and activation (unit v5369; libtpw_unit_RecalcDistancesFromCentreCar.dylib, tests/test_RecalcDistancesFromCentreCar.py), main app:
//   src/game/rides/CoasterQueue.cpp, src/game/rides/CoasterTrain.cpp   RecalcDistancesFromCentreCar, SetTrainCapacity, ActivateCoasterTrain
//
// The callees outside the unit are fakes that record the arguments the real ones read: GetTrainFrontAndRearSamples, GetRideAnimTime, both
// EnableMeshDraw, SetCurRideAnim, StartTrainSounds. gRideAnimSets (RideTypes.cpp) is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "rsse.h"

using namespace calllog;

tag_sRideAnimSet* gRideAnimSets[8];

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t Word(HFieldXZ f) { return (static_cast<int64_t>(f.mF00) << 16) | f.mF02; }

}  // namespace

void GetTrainFrontAndRearSamples(tag_sCoasterInstance* instance, sTrainInstance* train) {
    ev("GetTrainFrontAndRearSamples", {lab(instance), lab(train)});
}

uint32_t GetRideAnimTime(int32_t which) {
    ev("GetRideAnimTime", {which});
    return 0;
}

void EnableMeshDraw(tag_sMesh* mesh) { ev("EnableMeshDraw", {lab(mesh)}); }

void EnableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) { ev("EnableMeshDrawRange", {lab(mesh), Word(from), Word(to)}); }

void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {lab(set), a, anim, loop, CanonBits(speed), b});
}

void StartTrainSounds(tag_sCoasterInstance* instance, sTrainInstance* train, int32_t noState) {
    ev("StartTrainSounds", {lab(instance), lab(train), noState});
}

extern "C" {

uint64_t tpw_rc_addr(const char* name) {
    if (std::strcmp(name, "rides") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gRideAnimSets[0]));
    return 0;
}

uint64_t tpw_rc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Order = KEYS of the test module.
int32_t tpw_rc_layout(int64_t* out) {
    int32_t n = 0;
#define O(T, m) out[n++] = static_cast<int64_t>(offsetof(T, m))
    O(tag_sCoasterInstance, mFlags);
    O(tag_sCoasterInstance, mType);
    O(tag_sCoasterInstance, mBlock3C);
    O(tag_sCoasterInstance, mFAC);
    O(tag_sCoasterInstance, mFF0);
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mQueueB0) + offsetof(sWraparoundQueue, mF08));
    O(tag_sCoasterInstance, mTrains);
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    O(sTrainInstance, mFlags);
    O(sTrainInstance, mType);
    O(sTrainInstance, mF08);
    O(sTrainInstance, mCarsInUse);
    O(sTrainInstance, mCars);
    O(sTrainInstance, mCentreCar);
    out[n++] = static_cast<int64_t>(sizeof(sTrainInstance));
    O(sCarInstance, mFlags);
    O(sCarInstance, mType);
    O(sCarInstance, mIndex);
    O(sCarInstance, mRide);
    O(sCarInstance, mF1C);
    O(sCarInstance, mHeadsA);
    O(sCarInstance, mHeadsB);
    O(sCarInstance, mCurrentA);
    O(sCarInstance, mCurrentB);
    O(sCarInstance, mCountA);
    O(sCarInstance, mCountB);
    O(sCarInstance, mF38);
    O(sCarInstance, mF3C);
    O(sCarInstance, mF44);
    out[n++] = static_cast<int64_t>(sizeof(sCarInstance));
    O(tag_sCoasterType, mCarTypeA);
    O(tag_sCoasterType, mCarTypeB);
    O(tag_sCoasterType, mCarTypeC);
    O(tag_sCoasterType, mF2F8);
    O(tag_sCoasterType, mF2E8);
    O(tag_sCoasterType, mF30C);
    O(tag_sCoasterType, mF310);
    O(tag_sCoasterType, mF314);
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterType));
    O(sCarType, mFlags);
    out[n++] = static_cast<int64_t>(sizeof(sCarType));
    O(tag_sRideAnimSet, mFlags);
    O(tag_sRideAnimSet, mHeads);
    O(tag_sRideAnimSet, mFC0);
    O(tag_sRideAnimSet, mFC4);
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
#undef O
    return n;
}

void tpw_rc_recalc(uint64_t instance, uint64_t train) {
    RecalcDistancesFromCentreCar(reinterpret_cast<tag_sCoasterInstance*>(instance), reinterpret_cast<sTrainInstance*>(train));
}

void tpw_rc_setcap(uint64_t instance, uint32_t a, uint32_t b, uint32_t c) {
    SetTrainCapacity(reinterpret_cast<tag_sCoasterInstance*>(instance), a, b, c);
}

void tpw_rc_activate(uint64_t instance, uint64_t train) {
    ActivateCoasterTrain(reinterpret_cast<tag_sCoasterInstance*>(instance), reinterpret_cast<sTrainInstance*>(train));
}

}  // extern "C"
