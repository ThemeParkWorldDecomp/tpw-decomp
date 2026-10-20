// Test library for the coaster's track statistics (unit w212; libtpw_unit_CoasterStats.dylib, tests/test_CoasterStats.py), main app:
//   src/TPConfig_stats.cpp   ModF, SetCoasterCapacity, UpdateCoasterTrainsActive, UpdateCoasterTrainPosns, SpreadTrainsOnTrack,
//                            CalculateTrackGForcesSpeedAndExcitement, SetInitialExcitementValues, UpdateExcitementAtSample and
//                            CollateCoasterStats (and the 22 words of TPConfig.cpp they work on)
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues: UnloadTrain,
// ActivateCoasterTrain and StopTrainSounds (each may change the instance or the train, because the unit reads them again after the call),
// the two DisableMeshDraw and GetVectorRotationAroundZAxisFromY (the angle comes from the queue "angle"). The ride animation sets' table and
// the engine's sine table (gTPGlobals) are defined here, zero at start like the Mac's bss.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "calllog.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

// ---- the strings of gTPGlobals are never read: Format (the only member src/TbString.cpp leaves to the test) does nothing
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// ---- globals of other files, defined here
tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances + 1];
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
static_assert(sizeof(TPGlobals::mSinTable) == 0x4000, "the table's size");

namespace {

// A float's bits with every NaN made one value (the two processors make different NaNs).
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

// The two coaster types the test's world holds: ActivateCoasterTrain's queue "atype" switches the instance to one of them.
const uint32_t* gTypes[2];

// What the queues of the fakes answer for "leave it as it is" (no value the test queues is this large).
constexpr int64_t kNone = 0x7fffffff;

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub (pointers as lab(p))

// 0x100499e4: the queue "utc" (a train count, or kNone) sets the instance's count and "utb" moves its train base on by that many trains.
void UnloadTrain(tag_sCoasterInstance* instance, sTrainInstance* train, int32_t flag) {
    ev("UnloadTrain", {lab(instance), lab(train), flag});
    int64_t count = ret("utc", kNone);
    if (count != kNone) instance->mTrainCount = static_cast<int32_t>(count);
    int64_t advance = ret("utb", 0);
    instance->mTrains += advance;
}

// 0x1004a8a0: the queues "atype" (a type 0 or 1, or kNone), "atc" (a train count, or kNone) and "aflags" (the instance's flags, or kNone) change the instance.
void ActivateCoasterTrain(tag_sCoasterInstance* instance, sTrainInstance* train) {
    ev("ActivateCoasterTrain", {lab(instance), lab(train)});
    int64_t type = ret("atype", kNone);
    if (type == 0 || type == 1) instance->mType = gTypes[type];
    int64_t count = ret("atc", kNone);
    if (count != kNone) instance->mTrainCount = static_cast<int32_t>(count);
    int64_t flags = ret("aflags", kNone);
    if (flags != kNone) instance->mFlags = static_cast<uint32_t>(flags);
}

// 0x100471bc: the queue "sflags" (the train's flags, or kNone for no change).
void StopTrainSounds(tag_sCoasterInstance* instance, sTrainInstance* train) {
    ev("StopTrainSounds", {lab(instance), lab(train)});
    int64_t flags = ret("sflags", kNone);
    if (flags != kNone) train->mFlags = static_cast<uint32_t>(flags);
}

// 0x100653a4 and 0x1006548c (the two halfwords of each HFieldXZ as the one word the Mac passes it in).
void DisableMeshDraw(tag_sMesh* mesh) { ev("DisableMeshDraw1", {lab(mesh)}); }
void DisableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) {
    ev("DisableMeshDraw3", {lab(mesh), static_cast<int64_t>((static_cast<uint32_t>(from.mF00) << 16) | from.mF02),
                            static_cast<int64_t>((static_cast<uint32_t>(to.mF00) << 16) | to.mF02)});
}

// 0x100c1290: the vector's three floats are recorded; the answer is the next float of the queue "angle" (its bits).
float GetVectorRotationAroundZAxisFromY(s3DPoint* point) {
    ev("GetVectorRotationAroundZAxisFromY", {CanonBits(point->x), CanonBits(point->y), CanonBits(point->z)});
    uint32_t bits = static_cast<uint32_t>(ret("angle", 0));
    float angle;
    std::memcpy(&angle, &bits, sizeof(angle));
    return angle;
}

extern "C" {

// The host address of a global by the module's name for it (GLOBALS' names, plus the table of the ride animation sets and gTPGlobals).
uint64_t tpw_cst_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gTrackGravity", &gTrackGravity},
        {"gCoasterMinTrains", &gCoasterMinTrains},
        {"gBankFadeStartMin", &gBankFadeStartMin},
        {"gBankFadeMax", &gBankFadeMax},
        {"gBankFadeMin", &gBankFadeMin},
        {"gExciteBankFloorA", &gExciteBankFloorA},
        {"gExciteBankFloorB", &gExciteBankFloorB},
        {"gExciteFloor", &gExciteFloor},
        {"gRatingBMax", &gRatingBMax},
        {"gRatingBMin", &gRatingBMin},
        {"gRatingAMax", &gRatingAMax},
        {"gRatingAMin", &gRatingAMin},
        {"gExciteRiseA", &gExciteRiseA},
        {"gExciteRiseB", &gExciteRiseB},
        {"gExcitePrevB", &gExcitePrevB},
        {"gExciteMaxB", &gExciteMaxB},
        {"gExciteMaxA", &gExciteMaxA},
        {"gExcitePrevBank", &gExcitePrevBank},
        {"gExcitePrevG", &gExcitePrevG[0]},
        {"gExciteWord", &gExciteWord},
        {"gExciteB", &gExciteB},
        {"gExciteA", &gExciteA},
        {"gTPGlobals", &gTPGlobals},
        {"gRideAnimSets", &gRideAnimSets[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_cst_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

#define OFF(T, m) static_cast<int64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<T*>(0x1000)->m) - 0x1000)

// The offsets and sizes the module relies on (HOST_LAYOUT): each struct's fields in the module's FIELDS order, then its size; at the end
// the sine table's mask, scale and table in gTPGlobals.
int32_t tpw_cst_layout(int64_t* out) {
    int32_t n = 0;
    // tag_sCoasterInstance
    out[n++] = OFF(tag_sCoasterInstance, mFlags);
    out[n++] = OFF(tag_sCoasterInstance, mType);
    out[n++] = OFF(tag_sCoasterInstance, mState);
    out[n++] = OFF(tag_sCoasterInstance, mSpeedLimit);
    out[n++] = OFF(tag_sCoasterInstance, mLastBrace);
    out[n++] = OFF(tag_sCoasterInstance, mBraces);
    out[n++] = OFF(tag_sCoasterInstance, mSamples);
    out[n++] = OFF(tag_sCoasterInstance, mSampleCount);
    out[n++] = OFF(tag_sCoasterInstance, mLength);
    out[n++] = OFF(tag_sCoasterInstance, mFAC);
    out[n++] = OFF(tag_sCoasterInstance, mTrainCount);
    out[n++] = OFF(tag_sCoasterInstance, mRatingA);
    out[n++] = OFF(tag_sCoasterInstance, mRatingB);
    out[n++] = OFF(tag_sCoasterInstance, mMarkedBraces);
    out[n++] = OFF(tag_sCoasterInstance, mTrackLength);
    out[n++] = OFF(tag_sCoasterInstance, mTopHeight);
    out[n++] = OFF(tag_sCoasterInstance, mTopSpeed);
    out[n++] = OFF(tag_sCoasterInstance, mLeadTrain);
    out[n++] = OFF(tag_sCoasterInstance, mTrains);
    out[n++] = OFF(tag_sCoasterInstance, mBankFadeStart);
    out[n++] = sizeof(tag_sCoasterInstance);
    // sTrainInstance
    out[n++] = OFF(sTrainInstance, mFlags);
    out[n++] = OFF(sTrainInstance, mF08);
    out[n++] = OFF(sTrainInstance, mCarsInUse);
    out[n++] = OFF(sTrainInstance, mFrontSample);
    out[n++] = OFF(sTrainInstance, mRearSample);
    out[n++] = OFF(sTrainInstance, mCars);
    out[n++] = OFF(sTrainInstance, mCentreCar);
    out[n++] = OFF(sTrainInstance, mF30);
    out[n++] = OFF(sTrainInstance, mPosition);
    out[n++] = OFF(sTrainInstance, mF78);
    out[n++] = sizeof(sTrainInstance);
    // sCarInstance
    out[n++] = OFF(sCarInstance, mFlags);
    out[n++] = OFF(sCarInstance, mType);
    out[n++] = OFF(sCarInstance, mRide);
    out[n++] = OFF(sCarInstance, mF38);
    out[n++] = OFF(sCarInstance, mF40);
    out[n++] = sizeof(sCarInstance);
    // sCarType
    out[n++] = OFF(sCarType, mF24);
    out[n++] = OFF(sCarType, mF28);
    out[n++] = sizeof(sCarType);
    // tag_sCoasterType
    out[n++] = OFF(tag_sCoasterType, mFlags);
    out[n++] = OFF(tag_sCoasterType, mF24);
    out[n++] = OFF(tag_sCoasterType, mF28);
    out[n++] = OFF(tag_sCoasterType, mF2C);
    out[n++] = OFF(tag_sCoasterType, mF44);
    out[n++] = OFF(tag_sCoasterType, mF54);
    out[n++] = OFF(tag_sCoasterType, mF60);
    out[n++] = OFF(tag_sCoasterType, mF64);
    out[n++] = OFF(tag_sCoasterType, mF68);
    out[n++] = OFF(tag_sCoasterType, mF6C);
    out[n++] = OFF(tag_sCoasterType, mF2E8);
    out[n++] = OFF(tag_sCoasterType, mF2EC);
    out[n++] = OFF(tag_sCoasterType, mF320);
    out[n++] = OFF(tag_sCoasterType, mF324);
    out[n++] = OFF(tag_sCoasterType, mF32C);
    out[n++] = OFF(tag_sCoasterType, mF330);
    out[n++] = OFF(tag_sCoasterType, mF334);
    out[n++] = OFF(tag_sCoasterType, mF338);
    out[n++] = OFF(tag_sCoasterType, mF340);
    out[n++] = OFF(tag_sCoasterType, mF344);
    out[n++] = OFF(tag_sCoasterType, mF348);
    out[n++] = OFF(tag_sCoasterType, mF34C);
    out[n++] = OFF(tag_sCoasterType, mF350);
    out[n++] = OFF(tag_sCoasterType, mF354);
    out[n++] = OFF(tag_sCoasterType, mF358);
    out[n++] = sizeof(tag_sCoasterType);
    // sTrackSample
    out[n++] = OFF(sTrackSample, mFlags);
    out[n++] = OFF(sTrackSample, mPos.x);
    out[n++] = OFF(sTrackSample, mPos.y);
    out[n++] = OFF(sTrackSample, mPos.z);
    out[n++] = OFF(sTrackSample, mYaw);
    out[n++] = OFF(sTrackSample, mPitch);
    out[n++] = OFF(sTrackSample, mBank);
    out[n++] = OFF(sTrackSample, mSpeed);
    out[n++] = OFF(sTrackSample, mG.x);
    out[n++] = OFF(sTrackSample, mG.y);
    out[n++] = OFF(sTrackSample, mG.z);
    out[n++] = OFF(sTrackSample, mPiece);
    out[n++] = sizeof(sTrackSample);
    // sTrackSamplePiece
    out[n++] = OFF(sTrackSamplePiece, mBrace);
    out[n++] = sizeof(sTrackSamplePiece);
    // tag_sTrackBrace
    out[n++] = OFF(tag_sTrackBrace, mFlags);
    out[n++] = OFF(tag_sTrackBrace, mPrev);
    out[n++] = OFF(tag_sTrackBrace, mSample);
    out[n++] = sizeof(tag_sTrackBrace);
    // tag_sRideAnimSet
    out[n++] = OFF(tag_sRideAnimSet, mFlags);
    out[n++] = OFF(tag_sRideAnimSet, mHeads);
    out[n++] = OFF(tag_sRideAnimSet, mFC0);
    out[n++] = OFF(tag_sRideAnimSet, mFC4);
    out[n++] = sizeof(tag_sRideAnimSet);
    // TPGlobals
    out[n++] = OFF(TPGlobals, mSinTableMask);
    out[n++] = OFF(TPGlobals, mSinTableScale);
    out[n++] = OFF(TPGlobals, mSinTable);
    return n;
}

// The two coaster types ActivateCoasterTrain's queue "atype" can switch the instance to.
void tpw_cst_set_world(uint64_t type0, uint64_t type1) {
    gTypes[0] = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(type0));
    gTypes[1] = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(type1));
}

// ---- the functions, one wrapper each with C types only (floats as float, pointers as uint64_t)
#define INSTANCE(p) reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(p))

void tpw_cst_calc(uint64_t instance) { CalculateTrackGForcesSpeedAndExcitement(INSTANCE(instance)); }
void tpw_cst_excite(uint64_t instance, uint32_t sample, uint32_t word, float speed, float bank, uint64_t g, float time) {
    UpdateExcitementAtSample(INSTANCE(instance), sample, word, speed, bank, reinterpret_cast<s3DPoint*>(static_cast<uintptr_t>(g)), time);
}
void tpw_cst_spread(uint64_t instance, float time) { SpreadTrainsOnTrack(INSTANCE(instance), time); }
void tpw_cst_posns(uint64_t instance) { UpdateCoasterTrainPosns(INSTANCE(instance)); }
void tpw_cst_active(uint64_t instance) { UpdateCoasterTrainsActive(INSTANCE(instance)); }
void tpw_cst_collate(uint64_t instance, float height, float speed, float time) { CollateCoasterStats(INSTANCE(instance), height, speed, time); }
void tpw_cst_initial(uint64_t instance) { SetInitialExcitementValues(INSTANCE(instance)); }
void tpw_cst_capacity(uint64_t instance, uint32_t trains, uint32_t unused) { SetCoasterCapacity(INSTANCE(instance), trains, unused); }
float tpw_cst_modf(float value, uint64_t whole) { return ModF(value, reinterpret_cast<int32_t*>(static_cast<uintptr_t>(whole))); }

}  // extern "C"
