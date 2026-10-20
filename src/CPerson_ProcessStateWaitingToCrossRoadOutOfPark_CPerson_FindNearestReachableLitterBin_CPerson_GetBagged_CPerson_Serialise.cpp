#include <cstdint>
#include "ThingReceivers.h"
#include "Debug.h"
#include "analyser.h"
#include "MainBalance.h"
#include "Serialise.h"
#include "ppc_math.h"

// CPerson::Serialise 0x101170b4 (3,456 bytes). The names sit in the pool at 0x102442be (offsets in the comments).
// The fields are reached by Mac offset. Version 0 and 1 write the same list; any other version writes nothing.
// The small floats (+0x19c, +0x1a4, +0x1a8, +0x1ac, +0x1b0, +0x1b4, +0x1b8) go out as float(fctiwz(v) & 0xff).

namespace {

typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingIdx;

template<class T>
T* P(CPerson* self, uint32_t offset) {
    return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(self) + offset);
}

// lfs, fctiwz, clrlwi 24, then the magic-number conversion back to a single
float ByteOf(float value) {
    return static_cast<float>(static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(value))) & 0xff);
}

const char kArrivalDate[] = "mArrivalDate";               // +0x93
const char kArrivalIndex[] = "mArrivalIndex";             // +0xa0
const char kBalloonScript[] = "mBalloonScript";           // +0xae
const char kBeenAdmitted[] = "mBeenAdmitted";             // +0xbd
const char kCash[] = "mCash";                             // +0xcb
const char kExitLevel[] = "mExitLevel";                   // +0xd1
const char kPv[] = "pv";                                  // +0xdc
const char kLastPosX[] = "mLastPosX";                     // +0xdf
const char kLastPosY[] = "mLastPosY";                     // +0xe9
const char kMajorDest[] = "mMajorDest";                   // +0xf3
const char kNumRides[] = "mNumRides";                     // +0xfe
const char kNumShops[] = "mNumShops";                     // +0x108
const char kNumSideshows[] = "mNumSideshows";             // +0x112
const char kNumSideshowsWon[] = "mNumSideshowsWon";       // +0x120
const char kPaidAdmission[] = "mPaidAdmission";           // +0x131
const char kParkOpeningWaitingTime[] = "mParkOpeningWaitingTime";   // +0x140
const char kPersonType[] = "mPersonType";                 // +0x158
const char kPrankeryIndex[] = "mPrankeryIndex";           // +0x164
const char kPreviousRides[] = "mPreviousRides[i]";        // +0x173
const char kPreviousTemporaryRides[] = "mPreviousTemporaryRides[i]";   // +0x185
const char kQNext[] = "mQNext";                           // +0x1a0
const char kQPrev[] = "mQPrev";                           // +0x1a7
const char kQueueMoveDelay[] = "mQueueMoveDelay";         // +0x1ae
const char kQueuePos[] = "mQueuePos";                     // +0x1be
const char kRemainingBalloonLife[] = "mRemainingBalloonLife";   // +0x1c8
const char kSavedMajorDest[] = "mSavedMajorDest";         // +0x1de
const char kSavedState[] = "mSavedState";                 // +0x1ee
const char kState[] = "mState";                           // +0x1fa
const char kTimeOfLastSpotAnim[] = "mTimeOfLastSpotAnim"; // +0x201
const char kTimeStartedIdling[] = "mTimeStartedIdling";   // +0x215

}  // namespace

#define CHK(call) do { if ((call) != 0) return 1; } while (0)

int32_t CPerson::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (reinterpret_cast<CPeep*>(this)->Serialise(file, save, version, unused) != 0) return 1;
    if (version >= 2 || version < 0) return 0;
    if (save != 0) {
        CHK(SAVEVAR(file, P<MacULong>(this, 0x1d4), kArrivalDate, 0));
        CHK(SAVEVAR(file, P<MacULong>(this, 0x1d8), kArrivalIndex, 0));
        CHK(SAVEVAR(file, P<MacLong>(this, 0x210), kBalloonScript, 0));
        CHK(SAVEVAR(file, P<int32_t>(this, 0x1f8), kBeenAdmitted, 0));
        CHK(SAVEVAR(file, P<MacULong>(this, 0x1a0), kCash, 0));
        CHK(SAVEVAR(file, P<MacLong>(this, 0x1bc), kExitLevel, 0));
        float out;
        out = ByteOf(*P<float>(this, 0x19c));
        CHK(SAVEVAR(file, &out, kPv, 0));
        out = ByteOf(*P<float>(this, 0x1a8));
        CHK(SAVEVAR(file, &out, kPv, 0));
        CHK(SAVEVAR(file, P<float>(this, 0x218), kLastPosX, 0));
        CHK(SAVEVAR(file, P<float>(this, 0x21c), kLastPosY, 0));
        out = ByteOf(*P<float>(this, 0x1b4));
        CHK(SAVEVAR(file, &out, kPv, 0));
        CHK(SAVEVARSTRUCT(file, P<ThingIdx>(this, 0x1dc), kMajorDest, 0));
        CHK(SAVEVAR(file, P<MacULong>(this, 0x1c4), kNumRides, 0));
        CHK(SAVEVAR(file, P<MacULong>(this, 0x1c8), kNumShops, 0));
        CHK(SAVEVAR(file, P<MacULong>(this, 0x1cc), kNumSideshows, 0));
        CHK(SAVEVAR(file, P<MacULong>(this, 0x1d0), kNumSideshowsWon, 0));
        CHK(SAVEVAR(file, P<int32_t>(this, 0x204), kPaidAdmission, 0));
        CHK(SAVEVAR(file, P<uint32_t>(this, 0x200), kParkOpeningWaitingTime, 0));
        CHK(SAVEVAR(file, P<uint8_t>(this, 0x1f0), kPersonType, 0));
        CHK(SAVEVAR(file, P<uint8_t>(this, 0x1c0), kPrankeryIndex, 0));
        for (int32_t i = 0; i < 4; i++) {
            CHK(SAVEVARSTRUCT(file, P<ThingIdx>(this, 0x1e0 + i * 2), kPreviousRides, 0));
            CHK(SAVEVARSTRUCT(file, P<ThingIdx>(this, 0x1e8 + i * 2), kPreviousTemporaryRides, 0));
        }
        CHK(SAVEVARSTRUCT(file, P<ThingIdx>(this, 0x228), kQNext, 0));
        CHK(SAVEVARSTRUCT(file, P<ThingIdx>(this, 0x22a), kQPrev, 0));
        CHK(SAVEVAR(file, P<uint32_t>(this, 0x1f4), kQueueMoveDelay, 0));
        CHK(SAVEVAR(file, P<uint8_t>(this, 0x1f1), kQueuePos, 0));
        CHK(SAVEVAR(file, P<uint32_t>(this, 0x214), kRemainingBalloonLife, 0));
        CHK(SAVEVARSTRUCT(file, P<ThingIdx>(this, 0x1de), kSavedMajorDest, 0));
        CHK(SAVEVAR(file, P<PersonState>(this, 0x224), kSavedState, 0));
        CHK(SAVEVAR(file, P<PersonState>(this, 0x220), kState, 0));
        out = ByteOf(*P<float>(this, 0x1a4));
        CHK(SAVEVAR(file, &out, kPv, 0));
        CHK(SAVEVAR(file, P<MacULong>(this, 0x208), kTimeOfLastSpotAnim, 0));
        CHK(SAVEVAR(file, P<MacULong>(this, 0x1fc), kTimeStartedIdling, 0));
        out = ByteOf(*P<float>(this, 0x1b8));
        CHK(SAVEVAR(file, &out, kPv, 0));
        out = ByteOf(*P<float>(this, 0x1ac));
        CHK(SAVEVAR(file, &out, kPv, 0));
        out = ByteOf(*P<float>(this, 0x1b0));
        CHK(SAVEVAR(file, &out, kPv, 0));
    } else {
        CHK(LOADVAR(file, P<MacULong>(this, 0x1d4), kArrivalDate, 0));
        CHK(LOADVAR(file, P<MacULong>(this, 0x1d8), kArrivalIndex, 0));
        CHK(LOADVAR(file, P<MacLong>(this, 0x210), kBalloonScript, 0));
        CHK(LOADVAR(file, P<int32_t>(this, 0x1f8), kBeenAdmitted, 0));
        CHK(LOADVAR(file, P<MacULong>(this, 0x1a0), kCash, 0));
        CHK(LOADVAR(file, P<MacLong>(this, 0x1bc), kExitLevel, 0));
        float in;
        CHK(LOADVAR(file, &in, kPv, 0));
        *P<float>(this, 0x19c) = in;
        CHK(LOADVAR(file, &in, kPv, 0));
        *P<float>(this, 0x1a8) = in;
        CHK(LOADVAR(file, P<float>(this, 0x218), kLastPosX, 0));
        CHK(LOADVAR(file, P<float>(this, 0x21c), kLastPosY, 0));
        CHK(LOADVAR(file, &in, kPv, 0));
        *P<float>(this, 0x1b4) = in;
        CHK(LOADVARSTRUCT(file, P<ThingIdx>(this, 0x1dc), kMajorDest, 0));
        CHK(LOADVAR(file, P<MacULong>(this, 0x1c4), kNumRides, 0));
        CHK(LOADVAR(file, P<MacULong>(this, 0x1c8), kNumShops, 0));
        CHK(LOADVAR(file, P<MacULong>(this, 0x1cc), kNumSideshows, 0));
        CHK(LOADVAR(file, P<MacULong>(this, 0x1d0), kNumSideshowsWon, 0));
        CHK(LOADVAR(file, P<int32_t>(this, 0x204), kPaidAdmission, 0));
        CHK(LOADVAR(file, P<uint32_t>(this, 0x200), kParkOpeningWaitingTime, 0));
        CHK(LOADVAR(file, P<uint8_t>(this, 0x1f0), kPersonType, 0));
        CHK(LOADVAR(file, P<uint8_t>(this, 0x1c0), kPrankeryIndex, 0));
        for (int32_t i = 0; i < 4; i++) {
            CHK(LOADVARSTRUCT(file, P<ThingIdx>(this, 0x1e0 + i * 2), kPreviousRides, 0));
            CHK(LOADVARSTRUCT(file, P<ThingIdx>(this, 0x1e8 + i * 2), kPreviousTemporaryRides, 0));
        }
        CHK(LOADVARSTRUCT(file, P<ThingIdx>(this, 0x228), kQNext, 0));
        CHK(LOADVARSTRUCT(file, P<ThingIdx>(this, 0x22a), kQPrev, 0));
        CHK(LOADVAR(file, P<uint32_t>(this, 0x1f4), kQueueMoveDelay, 0));
        CHK(LOADVAR(file, P<uint8_t>(this, 0x1f1), kQueuePos, 0));
        CHK(LOADVAR(file, P<uint32_t>(this, 0x214), kRemainingBalloonLife, 0));
        CHK(LOADVARSTRUCT(file, P<ThingIdx>(this, 0x1de), kSavedMajorDest, 0));
        CHK(LOADVAR(file, P<PersonState>(this, 0x224), kSavedState, 0));
        CHK(LOADVAR(file, P<PersonState>(this, 0x220), kState, 0));
        CHK(LOADVAR(file, &in, kPv, 0));
        *P<float>(this, 0x1a4) = in;
        CHK(LOADVAR(file, P<MacULong>(this, 0x208), kTimeOfLastSpotAnim, 0));
        CHK(LOADVAR(file, P<MacULong>(this, 0x1fc), kTimeStartedIdling, 0));
        CHK(LOADVAR(file, &in, kPv, 0));
        *P<float>(this, 0x1b8) = in;
        CHK(LOADVAR(file, &in, kPv, 0));
        *P<float>(this, 0x1ac) = in;
        CHK(LOADVAR(file, &in, kPv, 0));
        *P<float>(this, 0x1b0) = in;
    }
    return 0;
}
