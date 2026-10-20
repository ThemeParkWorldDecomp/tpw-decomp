// Test library for a coaster car's ride along the track (unit w215; libtpw_unit_CarRideTick.dylib, tests/test_CarRideTick.py), main app:
//   src/TPConfig_ridetick.cpp   CoasterCarRideTrackTick, SetCarOrientation, SetTrainSoundState, HandleCollisions,
//                               InterpolateCoasterSpline (the instance overload), PlayCollisionSound, GetLinearSamplePosn,
//                               DistanceBetweenPoints (the reference overload)
//
// Every callee outside the unit is a fake that records its arguments (floats as canonical bits, objects as labels) and answers from the
// case's queues: the frame's clock (GetRideAnimTime), the sound handles (SOUND_SetParamValue, SOUND_SendEvent). The sample overload of
// InterpolateCoasterSpline writes a point and an orientation made of the controls it is given, so the tick's walk along the two chains
// shows in the results. The matrix helpers the car's orientation uses are the project's real ones. The module builds the world in the
// arena (the order list holds Ptr32 offsets) and calls the functions through the tpw_crt_ entry points; tpw_crt_layout answers the
// host structs' offsets and sizes, tpw_crt_addr the globals' addresses.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

// ---- the strings of gTPGlobals are never read
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// ---- globals of other files, defined here
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
CActionRecorder gActionRecorder;
tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances + 1];
int32_t gSoundBanks[8];

namespace {

// A float's bits, every NaN made one (the two processors make different NaNs).
int64_t CanonBits(float value) {
    return value != value ? 0x7fc00000 : static_cast<int64_t>(fbits(value));
}

float FloatOf(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

uint32_t BitsOf(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

template<typename T>
T* At(uint64_t address) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(address));
}

}  // namespace

// ---- the callees outside the unit
uint32_t GetRideAnimTime(int32_t which) {
    ev("GetRideAnimTime", {which});
    return static_cast<uint32_t>(ret("time"));
}

void UpdateRideAnimSpeedImmediate(tag_sRideAnimSet* set, float speed, int32_t flag) {
    ev("UpdateRideAnimSpeedImmediate", {lab(set), CanonBits(speed), flag});
}

void BendCarToTrack(tag_sCoasterInstance* instance, sCarInstance* car) {
    ev("BendCarToTrack", {lab(instance), lab(car), CanonBits(car->mF40)});
}

// The sample overload: the point is (a.x, t, b.z) of the controls' points, the orientation (t, c.mF0C, d.mF0C, a.mF0C, b.mF0C, c.mF08, d.mF08).
void InterpolateCoasterSpline(sTrackSample* samples, sCoasterTrackParams* params, tag_sBezierControl* a, tag_sBezierControl* b,
                              tag_sBezierControl* c, tag_sBezierControl* d, float t, s3DPoint* point, sCoasterOrientation* orient) {
    ev("InterpolateCoasterSpline", {lab(samples), lab(params), lab(a), lab(b), lab(c), lab(d), CanonBits(t), lab(point), lab(orient)});
    point->x = a->mF40.x;
    point->y = t;
    point->z = b->mF40.z;
    if (orient) {
        orient->mF00 = t;
        orient->mF04 = c->mF0C;
        orient->mF08 = d->mF0C;
        orient->mF0C = a->mF0C;
        orient->mF10 = b->mF0C;
        orient->mF14 = c->mF08;
        orient->mF18 = d->mF08;
    }
}

int32_t SOUND_SetParamValue(int32_t handle, uint32_t param, uint32_t value) {
    ev("SOUND_SetParamValue", {handle, param, value});
    return static_cast<int32_t>(ret("handle"));
}

int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return static_cast<int32_t>(ret("send"));
}

// ---- the entry points of the module
#define F(T, m) out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<T*>(0x1000)->m) - 0x1000);
#define S(T) out[n++] = static_cast<int64_t>(sizeof(T));

extern "C" {

struct Entry {
    const char* name;
    const void* address;
};

uint64_t tpw_crt_addr(const char* name) {
    const Entry table[] = {{"sin_mask", &gTPGlobals.mSinTableMask},
                           {"sin_scale", &gTPGlobals.mSinTableScale},
                           {"sin_table", gTPGlobals.mSinTable},
                           {"recording", &gActionRecorder.mRecording},
                           {"rideanim", gRideAnimSets},
                           {"bank2", &gSoundBanks[2]},
                           {"init", &gCarRideTickInit},
                           {"max", &gCarRideTickMax}};
    for (const Entry& entry : table) {
        if (std::strcmp(entry.name, name) == 0) {
            return reinterpret_cast<uintptr_t>(entry.address);
        }
    }
    return 0;
}

uint64_t tpw_crt_alloc(uint32_t size) {
    return reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size));
}

uint64_t tpw_crt_arena_base() {
    return reinterpret_cast<uintptr_t>(tpw::ArenaBase());
}

// For each struct of the module's ORDER: the offset of each field of its table, then the struct's size.
int32_t tpw_crt_layout(int64_t* out) {
    int32_t n = 0;
    F(sCarInstance, mFlags) F(sCarInstance, mType) F(sCarInstance, mIndex) F(sCarInstance, mRide) F(sCarInstance, mF10[0])
    F(sCarInstance, mF10[1]) F(sCarInstance, mF10[2]) F(sCarInstance, mF1C) F(sCarInstance, mHeadsA) F(sCarInstance, mHeadsB)
    F(sCarInstance, mCurrentA) F(sCarInstance, mCurrentB) F(sCarInstance, mCountA) F(sCarInstance, mCountB)
    F(sCarInstance, mF38) F(sCarInstance, mF3C) F(sCarInstance, mF40) F(sCarInstance, mF44) F(sCarInstance, mF48[0])
    F(sCarInstance, mF48[1]) F(sCarInstance, mF48[2]) F(sCarInstance, mF54[0]) F(sCarInstance, mF54[1]) F(sCarInstance, mF54[2])
    S(sCarInstance)
    F(sTrainInstance, mFlags) F(sTrainInstance, mType) F(sTrainInstance, mF08) F(sTrainInstance, mCarsInUse)
    F(sTrainInstance, mF10) F(sTrainInstance, mF14) F(sTrainInstance, mF18) F(sTrainInstance, mF1C)
    F(sTrainInstance, mFrontSample) F(sTrainInstance, mRearSample) F(sTrainInstance, mCars) F(sTrainInstance, mCentreCar)
    F(sTrainInstance, mF30) F(sTrainInstance, mFirstPersonCar) F(sTrainInstance, mF38) F(sTrainInstance, mF3C[0])
    F(sTrainInstance, mF3C[1]) F(sTrainInstance, mF3C[2]) F(sTrainInstance, mF48[0]) F(sTrainInstance, mF48[1])
    F(sTrainInstance, mF48[2]) F(sTrainInstance, mF54[0]) F(sTrainInstance, mF54[1]) F(sTrainInstance, mF54[2])
    F(sTrainInstance, mF54[3]) F(sTrainInstance, mF64) F(sTrainInstance, mF68) F(sTrainInstance, mF6C) F(sTrainInstance, mF70)
    F(sTrainInstance, mPosition) F(sTrainInstance, mF78) F(sTrainInstance, mF7C) S(sTrainInstance)
    F(tag_sCoasterInstance, mFlags) F(tag_sCoasterInstance, mType) F(tag_sCoasterInstance, mF2C)
    F(tag_sCoasterInstance, mSpeedLimit) F(tag_sCoasterInstance, mBlock3C) F(tag_sCoasterInstance, mF78)
    F(tag_sCoasterInstance, mF7C) F(tag_sCoasterInstance, mF80) F(tag_sCoasterInstance, mF84) F(tag_sCoasterInstance, mF88)
    F(tag_sCoasterInstance, mF8C) F(tag_sCoasterInstance, mF90) F(tag_sCoasterInstance, mSamples)
    F(tag_sCoasterInstance, mSampleCount) F(tag_sCoasterInstance, mLength) F(tag_sCoasterInstance, mFAC)
    F(tag_sCoasterInstance, mTrainCount) F(tag_sCoasterInstance, mFFC) F(tag_sCoasterInstance, mF120)
    F(tag_sCoasterInstance, mLeadTrain) F(tag_sCoasterInstance, mTrains) F(tag_sCoasterInstance, mTrainWords)
    F(tag_sCoasterInstance, mCars) F(tag_sCoasterInstance, mBankFadeStart) S(tag_sCoasterInstance)
    F(tag_sCoasterType, mFlags) F(tag_sCoasterType, mF24) F(tag_sCoasterType, mF28) F(tag_sCoasterType, mF2C)
    F(tag_sCoasterType, mF44) F(tag_sCoasterType, mF48) F(tag_sCoasterType, mF54) F(tag_sCoasterType, mF2EC)
    F(tag_sCoasterType, mF2F4) S(tag_sCoasterType)
    F(sCarType, mFlags) F(sCarType, mF18) F(sCarType, mF1C) F(sCarType, mF24) F(sCarType, mF28) S(sCarType)
    F(tag_sRideAnimSet, mHeads) S(tag_sRideAnimSet)
    F(tag_sMesh, mHeader) S(tag_sMesh)
    F(tag_sM3D2Header, mF78) S(tag_sM3D2Header)
    F(tag_sNode, mMatrix.m[0][0]) F(tag_sNode, mMatrix.m[0][1]) F(tag_sNode, mMatrix.m[0][2]) F(tag_sNode, mMatrix.m[0][3])
    F(tag_sNode, mMatrix.m[1][0]) F(tag_sNode, mMatrix.m[1][1]) F(tag_sNode, mMatrix.m[1][2]) F(tag_sNode, mMatrix.m[1][3])
    F(tag_sNode, mMatrix.m[2][0]) F(tag_sNode, mMatrix.m[2][1]) F(tag_sNode, mMatrix.m[2][2]) F(tag_sNode, mMatrix.m[2][3])
    F(tag_sNode, mMatrix.m[3][0]) F(tag_sNode, mMatrix.m[3][1]) F(tag_sNode, mMatrix.m[3][2]) F(tag_sNode, mMatrix.m[3][3])
    S(tag_sNode)
    F(tag_sTrackBrace, mFlags) S(tag_sTrackBrace)
    F(tag_sBezierControl, mF08) F(tag_sBezierControl, mF0C) F(tag_sBezierControl, mBrace) F(tag_sBezierControl, mNext)
    F(tag_sBezierControl, mF40.x) F(tag_sBezierControl, mF40.y) F(tag_sBezierControl, mF40.z) S(tag_sBezierControl)
    F(sTrackSample, mFlags) F(sTrackSample, mPos.x) F(sTrackSample, mPos.y) F(sTrackSample, mPos.z) F(sTrackSample, mSpeed)
    F(sTrackSample, mPiece) F(sTrackSample, mPieceB) S(sTrackSample)
    return n;
}

uint32_t tpw_crt_Distance(uint64_t a, uint64_t b) {
    return BitsOf(DistanceBetweenPoints(*At<s3DPoint>(a), *At<s3DPoint>(b)));
}

void tpw_crt_Linear(uint64_t instance, int32_t sample, uint32_t t, uint64_t out) {
    GetLinearSamplePosn(At<tag_sCoasterInstance>(instance), sample, FloatOf(t), At<s3DPoint>(out));
}

void tpw_crt_Collide(uint64_t instance, uint64_t train) {
    HandleCollisions(At<tag_sCoasterInstance>(instance), At<sTrainInstance>(train));
}

void tpw_crt_Play(uint64_t instance, uint64_t train) {
    PlayCollisionSound(At<tag_sCoasterInstance>(instance), At<sTrainInstance>(train));
}

void tpw_crt_SoundState(uint64_t train, uint32_t state, uint32_t value) {
    SetTrainSoundState(At<sTrainInstance>(train), state, value);
}

void tpw_crt_Interp(uint64_t instance, uint32_t position, uint64_t point, uint64_t orient) {
    InterpolateCoasterSpline(At<tag_sCoasterInstance>(instance), FloatOf(position), At<s3DPoint>(point), At<sCoasterOrientation>(orient));
}

void tpw_crt_Orient(uint64_t car, uint64_t orient, uint64_t point) {
    SetCarOrientation(At<sCarInstance>(car), At<sCoasterOrientation>(orient), At<s3DPoint>(point));
}

void tpw_crt_Tick(uint64_t instance) {
    CoasterCarRideTrackTick(At<tag_sCoasterInstance>(instance));
}

}  // extern "C"
