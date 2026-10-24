// Test library for DeactivateCoasterTrain, GetTrainFrontAndRearSamples, GetCarWantedSpeedAtHeight (unit v5763;
// libtpw_unit_DeactivateCoasterTrain.dylib, tests/test_DeactivateCoasterTrain.py), main app:
//   src/game/rides/CoasterTrain.cpp
//
// DisableMeshDraw (both forms) and StopTrainSounds are recording fakes; gRideAnimSets is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEngine.h"

using namespace calllog;

tag_sRideAnimSet* gRideAnimSets[64];

void DisableMeshDraw(tag_sMesh* mesh) { ev("DisableMeshDraw", {static_cast<int64_t>(mesh->mF00)}); }

void DisableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) {
    ev("DisableMeshDraw3", {static_cast<int64_t>(mesh->mF00),
                            static_cast<int64_t>((static_cast<uint32_t>(from.mF00) << 16) | from.mF02),
                            static_cast<int64_t>((static_cast<uint32_t>(to.mF00) << 16) | to.mF02)});
}

void StopTrainSounds(tag_sCoasterInstance* instance, sTrainInstance* train) { ev("StopTrainSounds", {lab(instance), lab(train)}); }

extern "C" {

uint64_t tpw_dct_addr(const char* name) {
    if (std::strcmp(name, "gRideAnimSets") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gRideAnimSets[0]));
    return 0;
}

uint64_t tpw_dct_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_dct_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(sCarInstance));
    out[n++] = static_cast<int64_t>(offsetof(sCarInstance, mType));
    out[n++] = static_cast<int64_t>(offsetof(sCarInstance, mRide));
    out[n++] = static_cast<int64_t>(offsetof(sCarInstance, mF38));
    out[n++] = static_cast<int64_t>(sizeof(sTrainInstance));
    out[n++] = static_cast<int64_t>(offsetof(sTrainInstance, mF08));
    out[n++] = static_cast<int64_t>(offsetof(sTrainInstance, mCarsInUse));
    out[n++] = static_cast<int64_t>(offsetof(sTrainInstance, mFrontSample));
    out[n++] = static_cast<int64_t>(offsetof(sTrainInstance, mRearSample));
    out[n++] = static_cast<int64_t>(offsetof(sTrainInstance, mCars));
    out[n++] = static_cast<int64_t>(offsetof(sTrainInstance, mPosition));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mFAC));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = static_cast<int64_t>(offsetof(sCarType, mF24));
    out[n++] = static_cast<int64_t>(offsetof(sCarType, mF28));
    out[n++] = static_cast<int64_t>(sizeof(sCarType));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFC0));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFC4));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));
    out[n++] = 0x24;   // the type's words, read at the Mac offsets
    out[n++] = 0x28;
    out[n++] = 0x2c;
    out[n++] = 0x100;
    return n;
}

void tpw_dct_deactivate(uint64_t instance, uint64_t train) {
    DeactivateCoasterTrain(reinterpret_cast<tag_sCoasterInstance*>(instance), reinterpret_cast<sTrainInstance*>(train));
}

void tpw_dct_samples(uint64_t instance, uint64_t train) {
    GetTrainFrontAndRearSamples(reinterpret_cast<tag_sCoasterInstance*>(instance), reinterpret_cast<sTrainInstance*>(train));
}

float tpw_dct_speed(uint64_t type, float a, float b, float c, float d) {
    return GetCarWantedSpeedAtHeight(reinterpret_cast<sCoasterType*>(type), a, b, c, d);
}

}  // extern "C"
