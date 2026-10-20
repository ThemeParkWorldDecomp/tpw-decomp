// Test library for CPerson::Serialise (unit v5972; libtpw_unit_CPerson_Serialise.dylib, tests/test_CPerson_Serialise.py), main app:
//   src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_CPerson_Serialise.cpp
//
// The callees outside the unit (CPeep::Serialise and every SAVEVAR / LOADVAR instance it calls) are recording fakes. A save fake records
// the file, the name (as an index into the name table), noLog and the value it is given; a load fake records the same but the value, and
// writes the next value of the "LOAD" queue into the target. Every fake answers from the "RET" queue (CPeep::Serialise from "CPEEP").
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "Serialise.h"

using namespace calllog;

namespace {

const char* const kNames[] = {
    "mArrivalDate", "mArrivalIndex", "mBalloonScript", "mBeenAdmitted", "mCash", "mExitLevel", "pv", "mLastPosX", "mLastPosY",
    "mMajorDest", "mNumRides", "mNumShops", "mNumSideshows", "mNumSideshowsWon", "mPaidAdmission", "mParkOpeningWaitingTime",
    "mPersonType", "mPrankeryIndex", "mPreviousRides[i]", "mPreviousTemporaryRides[i]", "mQNext", "mQPrev", "mQueueMoveDelay",
    "mQueuePos", "mRemainingBalloonLife", "mSavedMajorDest", "mSavedState", "mState", "mTimeOfLastSpotAnim", "mTimeStartedIdling",
};

int64_t NameIndex(const char* name) {
    for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++)
        if (std::strcmp(kNames[i], name) == 0) return static_cast<int64_t>(i);
    return -1;
}

template<class V> uint32_t Bits(const V& v) {
    uint32_t out = 0;
    std::memcpy(&out, &v, sizeof(V) < 4 ? sizeof(V) : 4);
    return out;
}
uint32_t Bits(const float& v) {
    uint32_t b = fbits(v);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu) != 0) b = 0x7fc00000u;
    return b;
}
uint32_t Bits(const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& v) { return v.mIndex; }

template<class V> void Put(V* target, uint32_t bits) {
    std::memcpy(target, &bits, sizeof(V) < 4 ? sizeof(V) : 4);
}
void Put(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>* target, uint32_t bits) { target->mIndex = static_cast<uint16_t>(bits); }

template<class V> int32_t Save(void* file, V* value, const char* name, int32_t noLog) {
    ev("SAVE", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file) & 0xffffffffu), NameIndex(name), noLog, static_cast<int64_t>(Bits(*value))});
    return static_cast<int32_t>(ret("RET"));
}

template<class V> int32_t Load(void* file, V* value, const char* name, int32_t noLog) {
    ev("LOAD", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file) & 0xffffffffu), NameIndex(name), noLog});
    Put(value, static_cast<uint32_t>(ret("LOAD")));
    return static_cast<int32_t>(ret("RET"));
}

typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingIdx;

}  // namespace

template<> int32_t SAVEVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t l) { return Save(f, v, n, l); }
template<> int32_t SAVEVAR<MacLong>(void* f, MacLong* v, const char* n, int32_t l) { return Save(f, v, n, l); }
template<> int32_t SAVEVAR<int32_t>(void* f, int32_t* v, const char* n, int32_t l) { return Save(f, v, n, l); }
template<> int32_t SAVEVAR<uint32_t>(void* f, uint32_t* v, const char* n, int32_t l) { return Save(f, v, n, l); }
template<> int32_t SAVEVAR<uint8_t>(void* f, uint8_t* v, const char* n, int32_t l) { return Save(f, v, n, l); }
template<> int32_t SAVEVAR<float>(void* f, float* v, const char* n, int32_t l) { return Save(f, v, n, l); }
template<> int32_t SAVEVARSTRUCT<ThingIdx>(void* f, ThingIdx* v, const char* n, int32_t l) { return Save(f, v, n, l); }
template<> int32_t SAVEVAR<CPerson::PersonState>(void* f, CPerson::PersonState* v, const char* n, int32_t l) { return Save(f, v, n, l); }
template<> int32_t LOADVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t l) { return Load(f, v, n, l); }
template<> int32_t LOADVAR<MacLong>(void* f, MacLong* v, const char* n, int32_t l) { return Load(f, v, n, l); }
template<> int32_t LOADVAR<int32_t>(void* f, int32_t* v, const char* n, int32_t l) { return Load(f, v, n, l); }
template<> int32_t LOADVAR<uint32_t>(void* f, uint32_t* v, const char* n, int32_t l) { return Load(f, v, n, l); }
template<> int32_t LOADVAR<uint8_t>(void* f, uint8_t* v, const char* n, int32_t l) { return Load(f, v, n, l); }
template<> int32_t LOADVAR<float>(void* f, float* v, const char* n, int32_t l) { return Load(f, v, n, l); }
template<> int32_t LOADVARSTRUCT<ThingIdx>(void* f, ThingIdx* v, const char* n, int32_t l) { return Load(f, v, n, l); }
template<> int32_t LOADVAR<CPerson::PersonState>(void* f, CPerson::PersonState* v, const char* n, int32_t l) { return Load(f, v, n, l); }

int32_t CPeep::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CPEEP", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file) & 0xffffffffu), save, version, static_cast<int64_t>(unused)});
    return static_cast<int32_t>(ret("CPEEP"));
}

extern "C" {

uint64_t tpw_ser_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ser_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(ThingIdx));
    out[n++] = static_cast<int64_t>(sizeof(CPerson::PersonState));
    return n;
}

uint32_t tpw_ser_call(uint64_t object, uint32_t file, int32_t save, int32_t version, uint32_t unused) {
    CPerson* self = reinterpret_cast<CPerson*>(static_cast<uintptr_t>(object));
    return static_cast<uint32_t>(self->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), save, version, unused));
}

}  // extern "C"
