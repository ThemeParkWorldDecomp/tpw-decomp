// Person.cpp: name proven by __sinit_Person_cpp in the main application.

#include "analyser.h"
#include "CMessageCentre.h"
#include "CMsg.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "helpers/PeepHelpers.h"
#include "helpers/PersonHelpers.h"
#include "HLSprite.h"
#include "m3d2.h"
#include "MainBalance.h"
#include "ObjectControl.h"
#include "ParkMessages.h"
#include "ParkTime.h"
#include "platform/macdoze.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "rsse.h"
#include "Serialise.h"
#include "TbTimeDiff.h"
#include "ThingMessages2.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "VehicleSteer.h"
#include "World.h"
#include "WorldSavvy.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

// CPerson::CPerson (0x10116634, 1488 bytes) and CPerson::ReceiveMessage (0x10116e24, 604 bytes). The person's fields are reached by Mac
// offset (see the comment in ThingReceivers.h); every field is a 32-bit scalar, so the host layout of that memory is the Mac's.

// The tables the person reads (initialised data; names invented in ThingReceivers.h).
uint16_t gPersonTable[2] = {0, 25};   // 0x102a0d54: halfword 1 is the sprite id stored at +0xc2
uint16_t gPersonCellA = 0;            // 0x102a0f14
uint16_t gPersonCellB = 0;            // 0x102a0f16

namespace {

using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

template <class T>
T& At(CPerson* p, size_t offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(p) + offset);
}

// The Mac slot of gMainBalance at a Mac offset: (offset - 8) / 4.
int32_t Bal(int32_t offset) { return gMainBalance.mValues[(offset - 8) / 4].mInteger; }

}  // namespace

// 0x10116634
CPerson::CPerson(MapId cell) {
    new (this) CPeep(cell, static_cast<CThing::ThingModel>(1));
    const float zero = 0.0f;
    At<float>(this, 0x19c) = zero;
    At<int32_t>(this, 0x1a0) = 0;
    At<float>(this, 0x1a4) = zero;
    At<float>(this, 0x1a8) = zero;
    At<float>(this, 0x1ac) = zero;
    At<float>(this, 0x1b0) = zero;
    At<float>(this, 0x1b4) = zero;
    At<float>(this, 0x1b8) = zero;
    At<int32_t>(this, 0x1bc) = 0;
    At<uint8_t>(this, 0x1c0) = 0;
    At<int32_t>(this, 0x1c4) = 0;
    At<int32_t>(this, 0x1c8) = 0;
    At<int32_t>(this, 0x1cc) = 0;
    At<int32_t>(this, 0x1d0) = 0;
    At<int32_t>(this, 0x1d4) = 0;
    At<uint16_t>(this, 0x1dc) = 0;
    At<uint16_t>(this, 0x1de) = 0;
    // __construct_array of the two arrays of four thing ids (+0x1e0 and +0x1e8): the element constructor writes nothing the loop below
    // does not write again.
    At<uint8_t>(this, 0x1f0) = 0;
    At<uint8_t>(this, 0x1f1) = 0;
    At<int32_t>(this, 0x1f4) = 0;
    At<int32_t>(this, 0x1f8) = 0;
    At<int32_t>(this, 0x1fc) = 0;
    At<int32_t>(this, 0x200) = 0;
    At<int32_t>(this, 0x204) = 0;
    At<int32_t>(this, 0x208) = 0;
    At<float>(this, 0x20c) = zero;
    At<int32_t>(this, 0x210) = 0;
    At<int32_t>(this, 0x214) = 0;
    At<float>(this, 0x218) = zero;
    At<float>(this, 0x21c) = zero;
    At<int32_t>(this, 0x220) = 6;
    At<int32_t>(this, 0x224) = 6;
    At<uint16_t>(this, 0x228) = 0;
    At<uint16_t>(this, 0x22a) = 0;

    At<uint32_t>(this, 0x1d4) = gWorld->mTurn;
    At<int32_t>(this, 0x1d8) = 0;
    At<int32_t>(this, 0x1bc) = Bal(0xc);
    int32_t spread = Bal(0x10);
    int32_t a = static_cast<int32_t>(gWorld->GetRand());
    int32_t d = spread * 2;
    int32_t rem = a - (a / d) * d;
    At<int32_t>(this, 0x1bc) = At<int32_t>(this, 0x1bc) + (rem - spread);

    int32_t count = Bal(0x194);
    int32_t b = static_cast<int32_t>(gWorld->GetRand());
    int32_t which = b - (b / count) * count;
    At<uint8_t>(this, 0x1f0) = static_cast<uint8_t>(which);
    int32_t base = Bal(0xa8 + which * 12);
    int32_t range = Bal(0x14);
    int32_t d2 = range * 2 + 1;
    int32_t c = static_cast<int32_t>(gWorld->GetRand());
    int32_t rem2 = c - (c / d2) * d2;
    int32_t scaled = static_cast<int32_t>(static_cast<uint32_t>(base) * static_cast<uint32_t>(rem2 - range + 100));
    int32_t hi = static_cast<int32_t>((static_cast<int64_t>(0x51eb851f) * static_cast<int64_t>(scaled)) >> 32);
    int32_t shifted = hi >> 5;
    int32_t result = shifted + static_cast<int32_t>(static_cast<uint32_t>(shifted) >> 31);
    if (result < 0) result = 0;
    At<int32_t>(this, 0x1a0) = result;
    At<float>(this, 0x19c) = 50.0f;

    At<float>(this, 0x1a4) = static_cast<float>(gWorld->GetRand() % 50u);
    At<float>(this, 0x1a8) = static_cast<float>(gWorld->GetRand() % 50u);
    At<float>(this, 0x1ac) = static_cast<float>(gWorld->GetRand() % 30u);
    At<float>(this, 0x1b4) = zero;
    At<float>(this, 0x1b0) = zero;
    At<float>(this, 0x1b8) = zero;
    gWorld->GetRand();
    uint32_t pick = gWorld->GetRand() % 100u;
    At<uint8_t>(this, 0x1c0) = pick < static_cast<uint32_t>(Bal(0x9c)) ? 0x64 : 0;
    if (At<uint8_t>(this, 0x1c0) == 0x64) {
        uint16_t id = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
        At<uint8_t>(this, 0x1c0) = static_cast<uint8_t>(At<uint8_t>(this, 0x1c0) + id % 3u);
    }

    At<uint16_t>(this, 0x1dc) = 0;
    At<uint16_t>(this, 0x1de) = 0;
    At<uint16_t>(this, 0x228) = 0;
    At<uint16_t>(this, 0x22a) = 0;
    At<uint8_t>(this, 0x1f1) = 0;
    At<int32_t>(this, 0x1f8) = 0;
    At<int32_t>(this, 0x208) = 0;
    gWorld->SeedRand(reinterpret_cast<CThing*>(this)->GetThingId().mIndex);
    At<int32_t>(this, 0x24) = 0;
    At<int32_t>(this, 0x20) = ESP_GetRandomSpritesFrom(static_cast<ESP_Sprites>(At<int32_t>(this, 0x24)));
    At<CDelayedScript>(this, 0xc).SpawnSpriteScript(static_cast<CDelayedScript::ScriptIdx>(1), At<int32_t>(this, 0x24), At<int32_t>(this, 0x20));
    At<uint16_t>(this, 0xc2) = gPersonTable[1];

    if (reinterpret_cast<CPeep*>(this)->IsInsidePark() == 0) {
        MapId side = gWorldMap->GetCrossingBSSidePos(static_cast<int32_t>(gWorld->GetRand() & 1u));
        uint8_t offset = static_cast<uint8_t>(gWorld->GetRand());
        uint32_t cellMinus1 = static_cast<uint32_t>(side.mIndex) - 1u;
        MapPos x;
        MapPos y;
        x.mCell = static_cast<uint8_t>(cellMinus1 & 0x7f);
        x.mOffset = offset;
        y.mCell = static_cast<uint8_t>(cellMinus1 >> 7);
        y.mOffset = 200;
        if (reinterpret_cast<CPeep*>(this)->SetDest(x, y) != 0) {
            SetState(static_cast<PersonState>(0));
        } else {
            SetState(static_cast<PersonState>(6));
        }
        reinterpret_cast<CPeep*>(this)->UpdatePositionInEngine(zero);
        reinterpret_cast<CPeep*>(this)->SpawnParticleEffect(0x13);
    } else {
        At<int32_t>(this, 0x1d8) = gWorld->GetNewPersonIndex();
        SetState(static_cast<PersonState>(6));
    }

    At<int32_t>(this, 0x204) = 0;
    for (int32_t i = 0; i < 4; i++) {
        At<uint16_t>(this, 0x1e0 + i * 2) = 0;
        At<uint16_t>(this, 0x1e8 + i * 2) = 0;
    }

    CMsgPeepArrived arrived(reinterpret_cast<CThing*>(this)->GetThingId());
    if (gMessageCentre == nullptr) gMessageCentre = new CMessageCentre;   // 0x1d0 bytes: 29 subscription headers
    gMessageCentre->PostMessage(arrived);
}

// CPeep (Peep.cpp, Person.cpp): the destructor, the sprite members and FinalDestroy. The peep is reached by Mac offset.

// 0x10116d18: destroys the vehicle at +0xd4. The Mac's delete-storage flag (r4) is not modelled.
CPeep::~CPeep() {
    reinterpret_cast<Vehicle*>(PBytes(this) + 0xd4)->~Vehicle();
}

// Unit v51034: CPerson::GetHeadSpriteID, SetSpotAnim, ScoreOnCloserInspection, FinalDestroy, LOADVAR<ESP_Sprites>, Pilot::~Pilot,
// CParkTime::GetFunnyTimeSinceGameTurn. The person is reached by Mac offset.

namespace {
template<class T> T& Fld_3(void* object, size_t offset) { return *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset); }
}  // namespace

// 0x10116da4
void CPerson::FinalDestroy() {
    if (Fld_3<int32_t>(this, 0x210) != 0) {
        SCR_KillScript(Fld_3<int32_t>(this, 0x210));
        Fld_3<int32_t>(this, 0x210) = 0;
    }
    reinterpret_cast<CPeep*>(this)->FinalDestroy();
}

// 0x10116e24
void CPerson::ReceiveMessage(const CMsg& message) {
    if (message.GetType() != 10) return;
    const uint16_t gone = static_cast<const CMsgRideDeleted&>(message).mThingId;
    if (At<uint16_t>(this, 0x1dc) == gone) {
        if (At<int32_t>(this, 0x220) == 0x10) {
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
            if (At<int32_t>(this, 0xc) != 0) {
                At<CDelayedScript>(this, 0xc).SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(1));
            } else {
                At<CDelayedScript>(this, 0xc).SpawnSpriteScript(static_cast<CDelayedScript::ScriptIdx>(3), At<int32_t>(this, 0x24), At<int32_t>(this, 0x20));
            }
            At<int32_t>(this, 0x28) = 0;
            At<CThoughts>(this, 0x30).RecordEvent(static_cast<CThoughts::EventType>(0xd), ThingId{At<uint16_t>(this, 0x1dc)});
            reinterpret_cast<CPeep*>(this)->PlaySound(gSoundBanks[1], 0x80);
        } else {
            if (InQueueingState() != 0) DismissFromQueue();
        }
        LowerHappiness(static_cast<Amount>(0));
        At<uint16_t>(this, 0x1dc) = 0;
        uint16_t c22a = At<uint16_t>(this, 0x22a);
        uint16_t c228 = At<uint16_t>(this, 0x228);
        uint16_t c1dc = At<uint16_t>(this, 0x1dc);
        uint16_t myId = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
        int32_t ok = 0;
        if (gPersonCellA == At<uint16_t>(this, 0x228) && gPersonCellB == At<uint16_t>(this, 0x22a)) ok = 1;
        GAME_ASSERTMSG(ok, "Person %d is in queue for object %d (next %d, prev %d) but doesn't think he is\n", myId, c1dc, c228, c22a);
        SetState(static_cast<PersonState>(6));
    }
    if (At<uint16_t>(this, 0x1de) == gone) At<uint16_t>(this, 0x1de) = 0;
    for (int32_t i = 0; i < 4; i++) {
        if (At<uint16_t>(this, 0x1e0 + i * 2) == gone) {
            At<uint16_t>(this, 0x1e0 + i * 2) = 0;
            At<uint16_t>(this, 0x1e8 + i * 2) = 0;
        }
    }
}

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

// 0x10117e64
void CPerson::SetSpotAnim(CDelayedScript::ScriptIdx script) {
    float x, y, z;
    int32_t mode;
    SCR_GetScriptPosition(Fld_3<int32_t>(this, 0xc), &x, &y, &z, &mode);
    if (static_cast<int32_t>(script) == 4) {
        auto id = reinterpret_cast<CThing*>(this)->GetThingId();
        if ((id.mIndex & 0xf) == 0) reinterpret_cast<CPeep*>(this)->PlaySound(gSoundBanks[1], 0x7e);
    }
    reinterpret_cast<CDelayedScript*>(reinterpret_cast<uint8_t*>(this) + 0xc)->SetSpriteScript(script);
    Fld_3<uint32_t>(this, 0x208) = gWorld->mTurn;
    Fld_3<uint32_t>(this, 0x224) = Fld_3<uint32_t>(this, 0x220);
    SetState(static_cast<PersonState>(8));
}

// Unit v51064: twenty-four small functions of person.cpp, personqueue.cpp, mechanichq.cpp, guard.cpp and vehicle.cpp (main app).

// The initialised words of the five static-initialiser functions (BSS on the Mac, set by the function).
int32_t gPersonSinitA;       // 0x1034c4a0
int32_t gPersonSinitB;       // 0x1034c4a4
// 0x10116c54: the vtable store is not modelled; the deleting form (flag above 0 as a signed halfword) deletes the object.
CMsgPeepArrived* CMsgPeepArrived_Dtor(CMsgPeepArrived* self, int16_t deleteFlag) {
    if (self != nullptr && deleteFlag > 0) FixedSizeArrayOperatorDelete(self);
    return self;
}

// 0x10117f80: back to the state saved at +0x224.
void CPerson::EndSpotAnim() { SetState(static_cast<PersonState>(Fld_2<int32_t>(Raw(this), 0x224))); }

namespace {

// fctiwz, then the low byte (clrlwi 24)
uint32_t Low8(float value) { return static_cast<uint32_t>(ppc_fctiwz(value)) & 0xffu; }

}  // namespace

// 0x10117fd4
void CPerson::DoSomeThinking() {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    uint32_t thought;
    if (Low8(Fld_2<float>(self, 0x1a8)) > 0x5a && Low8(Fld_2<float>(self, 0x1a4)) > 0x5a) {
        thought = 3;
    } else if (static_cast<float>(Low8(Fld_2<float>(self, 0x1a8))) > 99.0f) {
        thought = 1;
    } else if (static_cast<float>(Low8(Fld_2<float>(self, 0x1a4))) > 99.0f) {
        thought = 2;
    } else if (static_cast<float>(Low8(Fld_2<float>(self, 0x1ac))) > 99.0f) {
        thought = 4;
    } else if (static_cast<float>(Low8(Fld_2<float>(self, 0x1b0))) > 99.0f) {
        thought = 0xe;
    } else {
        uint16_t cellId = static_cast<uint16_t>(Fld_2<uint8_t>(self, 4) + (static_cast<uint32_t>(Fld_2<uint8_t>(self, 6)) << 7) + 1);
        CParkAnalyser* analyser = gWorld->GetParkAnalyser();
        CParkAnalyserCell* cell = analyser->GetAnalyserCell(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{cellId});
        float walkable = static_cast<float>(cell->GetNumWalkableCells(4));
        float littered = static_cast<float>(cell->GetNumLitteredCells(4));
        float ratio = littered / walkable;
        if (ratio > 0.2f) {
            thought = 7;
        } else {
            uint32_t happy = Low8(Fld_2<float>(self, 0x19c));
            if (static_cast<float>(happy) > 99.0f) {
                thought = 9;
            } else if (static_cast<float>(happy) < 10.0f) {
                thought = 0xb;
            } else {
                return;
            }
        }
    }
    uint16_t id = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
    if ((id & 0xf) == 0) {
        CPeep* peep = reinterpret_cast<CPeep*>(this);
        if (thought == 0xd) peep->PlaySound(gSoundBanks[1], 0x7f);
        else if (thought < 0xd) {
            if (thought == 4) peep->PlaySound(gSoundBanks[1], 0x85);
            else if (thought >= 0xc) peep->PlaySound(gSoundBanks[1], 0x7e);
        } else if (thought == 0xf) peep->PlaySound(gSoundBanks[1], 0x85);
    }
    reinterpret_cast<CThoughts*>(self + 0x30)->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(thought), false);
}

// 0x1011836c: the costume words +0x20 / +0x24 when the sprite set 3 has sprites (bit 31 for kind 2), else a random head from set 1
// seeded by the thing id.
uint32_t CPerson::GetHeadSpriteID() {
    if (ESP_GetNumberOfSprites(static_cast<ESP_Sprites>(3)) != 0) {
        uint32_t word = Fld_3<uint32_t>(this, 0x20);
        if (Fld_3<int32_t>(this, 0x24) == 2) return word | 0x80000000u;
        return word;
    }
    auto id = reinterpret_cast<CThing*>(this)->GetThingId();
    gWorld->SeedRand(id.mIndex);
    return static_cast<uint32_t>(ESP_GetRandomSpritesFrom(static_cast<ESP_Sprites>(1)));
}

// Unit v5962: the embedded-entry counters (0x101fff0c, 0x101ffe74), EnableRideHead (0x10200230), bthread_cond (0x10200e34, 0x10200ea8,
// 0x10200f30) and CalculateOrientFromTrackMorph (0x101fec54).

// CPerson::ModelState 0x1011fdb4, OnAdmittance 0x1011f114, GetDebagged 0x1011b594, MakeMajorDecision 0x10118430 (unit v5543).
// The person is reached by Mac offset: +0x1c angle word, +0x20 and +0x24 script words, +0x188 word, +0x1d8 person index, +0x1dc ride halfword,
// +0x1f8 admit flag, +0x208 turn stamp, +0x220 state, +0x228 queue link.

// The "none" ids these functions compare halfwords with (names invented; zero at start like the Mac's bss).
uint16_t gMajorListEnd;    // 0x102a0f18: ends the object list MakeMajorDecision walks
uint16_t gMajorNoQueue;    // 0x102a0f1a: the "nobody at the back of the queue" id
uint16_t gMajorNoRide;     // 0x102a0f1c: MakeMajorDecision's "no ride chosen"
namespace {

const char kRowZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x102442be and 0x10244aa8
const char kHumphText[] = "<Humph>\n";                                                              // 0x10244981

}  // namespace

// 0x10118430
void CPerson::MakeMajorDecision() {
    uint8_t* self = Raw(this);
    CPeep* peep = reinterpret_cast<CPeep*>(this);
    int32_t best = 0;
    Fld<uint16_t>(self, 0x1dc) = 0;
    uint16_t id = gWorld->mF1DA746;
    while (id != gMajorListEnd) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
        CObject* object = static_cast<CObject*>(gThingArray[id].mThing);
        uint8_t* o = Raw(object);
        if ((Fld<uint16_t>(o, 0x2e) & 4u) != 0) {
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> back = object->GetBackOfQueue();
            if (back.mIndex != gMajorNoQueue) {
                uint32_t scoreU = CalculateOptionScore(object);
                if (scoreU >= 10u) {
                    int32_t score = static_cast<int32_t>(scoreU);
                    if (score > best || (score == best && (gWorld->mTurn & 1u) != 0)) {
                        if (object->IsAvailableToChoose() != 0) {
                            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> where = object->GetBackOfQueue();
                            if (peep->SetDest(where) != 0) {
                                Fld<uint16_t>(self, 0x1dc) = id;
                                best = score;
                            }
                        }
                    }
                }
            }
        }
        id = Fld<uint16_t>(o, 0xc);
    }
    if (Fld<uint16_t>(self, 0x1dc) != gMajorNoRide) GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
}

// CPerson::CalculateOptionScore 0x101185f8 (2,612 bytes): how much the person wants to go on a ride, 0 to a few hundred.

// Names invented, zero at start (declared in ThingReceivers.h).
uint16_t gNoBackOfQueue = 0;                 // 0x102a0fac
uint16_t gNoThingRide = 0;                   // 0x102a0fae

// The image's initialised data. gNeedMatchTable is 11 x 11: the row is the option's tenth of its need effect (thirst or
// hunger), the column the person's tenth of the need. gAmountMatchTable is indexed by (amount + 4) / 5 (toilet, illness).
uint8_t gNeedMatchTable[0x79] = {            // 0x102a0f1e
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0
      0,   0,   1,   2,   5,   7,  11,  15,  20,  25,  31,   // 1
      0,   0,   1,   4,   7,  11,  16,  21,  28,  36,  44,   // 2
      0,   0,   2,   4,   8,  13,  19,  26,  35,  44,  54,   // 3
      0,   0,   2,   5,  10,  15,  22,  30,  40,  51,  63,   // 4
      0,   0,   2,   6,  11,  17,  25,  34,  45,  57,  70,   // 5
      0,   0,   3,   6,  12,  19,  27,  37,  49,  62,  77,   // 6
      0,   0,   3,   7,  13,  20,  30,  40,  53,  67,  83,   // 7
      0,   0,   3,   8,  14,  22,  32,  43,  57,  72,  89,   // 8
      0,   0,   3,   8,  15,  23,  34,  46,  60,  76,  94,   // 9
      0,   1,   4,   9,  16,  25,  36,  49,  64,  81, 100,   // 10
};
uint8_t gAmountMatchTable[0x15] = {          // 0x102a0f97
      0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   2,
      5,  10,  20,  30,  40,  50,  60,  75,  90, 100,
};

namespace {

const char kNoBoqText[] = "Calculating option score for object with no BOQ\n";                       // +0x228
const char kHeadText[] = "Person %d: COS on %ls %d:\n";                                               // +0x259
const char kDistFireText[] = "\t- dist (inc. nearby fireworks): %d\n";                                // +0x274
const char kDistText[] = "\t- dist: %d\n";                                                            // +0x299
const char kCloseQueueText[] = "\t- close to queue, q_match=%d\n";                                    // +0x2a6
const char kFarQueueText[] = "\t- far from queue, q_match=0\n";                                       // +0x2c5
const char kExText[] = "\t- base ex_diff=%d, base ex_match=%d, es=%d\n";                              // +0x2e3
const char kThirstText[] = "\t- thirst=%d, effect=%d, thirst_match=%d\n";                             // +0x310
const char kHungerText[] = "\t- hunger=%d, effect=%d, hunger_match=%d\n";                             // +0x33a
const char kToiletText[] = "\t- toilet=%d, effect=%d, toilet_match=%d\n";                             // +0x364
const char kIllnessText[] = "\t- illness=%d, effect=%d, illness_match=%d\n";                          // +0x38e
const char kBaseText[] = "\t- Base score = %d";                                                       // +0x3ba
const char kNewRideText[] = "\t- new ride, increasing score to %d\n";                                 // +0x3cd
const char kRainText[] = "\t- indoor ride in the rain, increasing score to %d\n";                     // +0x3f2
const char kGtText[] = "\t- GT-only ride, increasing score to %d\n";                                  // +0x426
const char kPriceText[] = "\t- expensive ride, increasing score to %d\n";                             // +0x44f
const char kUsedText[] = "\t- score after examining used-ride history=%d\n";                          // +0x47a
const char kTempText[] = "\t- score after examining temp history=%d\n";                               // +0x4a9
const char kFinalText[] = "\t- FINAL SCORE for ride %d = %d\n";                                       // +0x4d3

inline uint8_t* Bytes(void* p) { return static_cast<uint8_t*>(p); }
inline uint16_t Half(const uint8_t* base, uint32_t off) { return *reinterpret_cast<const uint16_t*>(base + off); }
inline uint32_t Word(const uint8_t* base, uint32_t off) { return *reinterpret_cast<const uint32_t*>(base + off); }
inline float Flt(const uint8_t* base, uint32_t off) { return *reinterpret_cast<const float*>(base + off); }

// gMainBalance's word at the Mac offset 8 + 4 * slot.
inline int32_t Bal_2(int32_t slot) { return gMainBalance.mValues[slot].mInteger; }

// The ride record's value at the Mac offset 0x0c + 4 * slot.
inline int32_t DbWord(const RideInfo* db, int32_t slot) { return db->Value(static_cast<RideBalanceSlot>(slot)).mInteger; }
constexpr int32_t kSlot144 = 78, kSlot148 = 79, kSlot114 = 66, kSlotC4 = 46, kSlot1b8 = 107;

// mulhwu-and-shift division by ten, as the compiler wrote it: (x * 0xcccccccd >> 32) >> 3, kept to a byte.
inline uint32_t TenthByte(uint32_t x) { return static_cast<uint32_t>((static_cast<uint64_t>(0xcccccccdu) * x) >> 32 >> 3) & 0xffu; }

}  // namespace

// 0x101185f8
uint32_t CPerson::CalculateOptionScore(CObject* object) {
    uint8_t* self = Bytes(this);
    uint8_t* obj = Bytes(object);

    uint16_t queueA = object->GetBackOfQueue().mIndex;
    GAME_ASSERTMSG(static_cast<int32_t>((static_cast<uint32_t>(gNoBackOfQueue) - queueA) != 0 ? 1 : 0), kNoBoqText);
    uint16_t queueB = object->GetBackOfQueue().mIndex;
    int32_t rideX = static_cast<int32_t>((static_cast<uint32_t>(queueB) - 1u) & 0x7fu);
    uint16_t queueC = object->GetBackOfQueue().mIndex;
    uint16_t myCell = static_cast<uint16_t>(self[4] + (static_cast<uint32_t>(self[6]) << 7) + 1);
    int32_t rideY = static_cast<int16_t>((static_cast<uint32_t>(queueC) - 1u) >> 7);
    uint32_t myX = (static_cast<uint32_t>(myCell) - 1u) & 0x7fu;
    uint32_t myY = (static_cast<uint32_t>(myCell) - 1u) >> 7;
    uint32_t wDist = Bal_2(26) & 0xffff;
    uint32_t wToilet0 = static_cast<uint32_t>(Bal_2(29)) & 0xffff;   // thirst weight
    uint32_t wHunger = static_cast<uint32_t>(Bal_2(30)) & 0xffff;
    uint32_t wToilet = static_cast<uint32_t>(Bal_2(31)) & 0xffff;
    uint32_t wIllness = static_cast<uint32_t>(Bal_2(32)) & 0xffff;

    uint16_t objectId = reinterpret_cast<CThing*>(object)->GetThingId().mIndex;
    uint16_t personId = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
    const uint8_t* db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
    CTRACE(7, kHeadText, static_cast<uint32_t>(personId), reinterpret_cast<const RideInfo*>(db)->mItemName, static_cast<uint32_t>(objectId));

    if (Half(self, 0x1e0) != gNoThingRide) {
        uint16_t index = Half(self, 0x1e0);
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kRowZeroText);
        const uint8_t* thing = static_cast<const uint8_t*>(gThingArray[index].mThing);
        if (thing != nullptr) {
            uint16_t type = Half(thing, 0xe);
            if (type == Half(obj, 0xe)) return 0;
        }
    }

    int32_t dx = static_cast<int32_t>(myX) - rideX;
    int32_t dy = static_cast<int32_t>(myY) - rideY;
    int32_t distA = dx * dx;
    int32_t distB = dy * dy;
    int32_t scaled = (distA + distB) * 100;
    uint32_t dist = static_cast<uint32_t>(scaled / 450);   // the compiler's multiply-high form of a signed divide by 450
    if (dist > 100u) dist = 100u;
    uint32_t distScore = 100u - dist;
    uint16_t cellId = static_cast<uint16_t>(rideX + (rideY << 7) + 1);
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cellTag;
    cellTag.mIndex = cellId;
    CRegionEffectCell* effectCell = gWorldMap->GetRegionalEffectCell(cellTag);
    int32_t effect = effectCell->mpEffect[4];
    if (effect != 0) {
        distScore = distScore / static_cast<uint32_t>(effect);
        CTRACE(7, kDistFireText, distScore);
    } else {
        CTRACE(7, kDistText, distScore);
    }

    uint32_t queueWeight;
    uint32_t queueMatch;
    if (distA + distB <= 8) {
        uint32_t capacity = Word(obj, 0x3c);
        if (capacity == 0) capacity = 1;
        uint32_t divisor = capacity << 2;
        int32_t waiting = object->GetNumPeopleInQueue();
        queueWeight = static_cast<uint32_t>(Bal_2(27));
        uint32_t share = divisor == 0 ? 0u : (static_cast<uint32_t>(waiting) * 100u) / divisor;   // divwu by 0 answers 0 on the emulator
        queueMatch = 100u - share;
        CTRACE(7, kCloseQueueText, queueMatch);
    } else {
        queueMatch = 0;
        queueWeight = 0;
        CTRACE(7, kFarQueueText);
    }

    uint32_t preferred = this->GetPreferredExcitement();
    uint32_t level = object->GetExcitementLevel(false);
    int32_t exDiff = static_cast<int32_t>((preferred & 0xffu) - (level & 0xffu));
    if (exDiff < 0) exDiff = -exDiff;
    if (exDiff < 0) exDiff = 0;
    else if (exDiff > 0x32) exDiff = 0x32;
    int32_t exMatch = (0x32 - exDiff) * 2;
    uint32_t exWeight;
    if ((object->GetExcitementLevel(true) & 0xffu) == 0) exWeight = 0;
    else exWeight = static_cast<uint32_t>(Bal_2(28)) & 0xffff;
    CTRACE(700, kExText, exDiff, exMatch, exWeight);

    // thirst
    db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
    int32_t thirstAmount = ppc_fctiwz(static_cast<double>(Flt(self, 0x1a4)));
    uint32_t needWord = static_cast<uint32_t>(DbWord(reinterpret_cast<const RideInfo*>(db), kSlot144));
    uint32_t thirstMatch = gNeedMatchTable[TenthByte(needWord & 0xffu) * 11u + TenthByte(static_cast<uint32_t>(thirstAmount))];
    db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
    CTRACE(7, kThirstText, static_cast<uint32_t>(thirstAmount) & 0xffu, DbWord(reinterpret_cast<const RideInfo*>(db), kSlot144), thirstMatch);

    // hunger
    db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
    int32_t hungerAmount = ppc_fctiwz(static_cast<double>(Flt(self, 0x1a8)));
    uint32_t hungerWord = static_cast<uint32_t>(DbWord(reinterpret_cast<const RideInfo*>(db), kSlot148));
    uint32_t hungerMatch = gNeedMatchTable[TenthByte(hungerWord & 0xffu) * 11u + TenthByte(static_cast<uint32_t>(hungerAmount))];
    db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
    CTRACE(7, kHungerText, static_cast<uint32_t>(hungerAmount) & 0xffu, DbWord(reinterpret_cast<const RideInfo*>(db), kSlot148), hungerMatch);

    // toilet
    bool indoor = (Half(obj, 0x2e) & 1u) != 0;
    int32_t toiletAmount = ppc_fctiwz(static_cast<double>(Flt(self, 0x1ac)));
    uint32_t toiletMatch = indoor ? gAmountMatchTable[((static_cast<uint32_t>(toiletAmount) & 0xffu) + 4u) / 5u] : 0u;
    CTRACE(7, kToiletText, static_cast<uint32_t>(toiletAmount) & 0xffu, indoor ? 100 : 0, toiletMatch);

    // illness
    indoor = (Half(obj, 0x2e) & 1u) != 0;
    int32_t illnessAmount = ppc_fctiwz(static_cast<double>(Flt(self, 0x1b0)));
    uint32_t illnessMatch = indoor ? gAmountMatchTable[((static_cast<uint32_t>(illnessAmount) & 0xffu) + 4u) / 5u] : 0u;
    CTRACE(7, kIllnessText, static_cast<uint32_t>(illnessAmount) & 0xffu, indoor ? 100 : 0, illnessMatch);

    uint32_t sumWeights = wDist + queueWeight + exWeight + wToilet0 + wHunger + wToilet + wIllness;
    uint32_t sumScores = wDist * distScore + queueWeight * queueMatch + exWeight * static_cast<uint32_t>(exMatch) + wToilet0 * thirstMatch
                         + wHunger * hungerMatch + wToilet * toiletMatch + wIllness * illnessMatch;
    // (the sum is formed in the original's order; the wrap-around is the same either way)
    uint32_t scoreU = sumWeights == 0 ? 0u : sumScores / sumWeights;
    int32_t score = static_cast<int32_t>(scoreU);
    CTRACE(7, kBaseText, scoreU);

    uint32_t age = static_cast<uint32_t>(object->GetObjectAge());
    if (age <= static_cast<uint32_t>(Bal_2(18))) {
        score = score * Bal_2(19);
        CTRACE(7, kNewRideText, static_cast<uint32_t>(score));
    }

    CWeather* weather = gWorld->GetWeather();
    if (static_cast<int32_t>(weather->mF30) > 0) {
        db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
        if (DbWord(reinterpret_cast<const RideInfo*>(db), kSlot114) != 0) {
            score = score * Bal_2(20);
            CTRACE(7, kRainText, static_cast<uint32_t>(score));
        }
    }

    db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
    if (DbWord(reinterpret_cast<const RideInfo*>(db), kSlotC4) > 0) {
        db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
        int32_t extra = DbWord(reinterpret_cast<const RideInfo*>(db), kSlotC4);
        double scoreD = static_cast<double>(score);
        double extraD = static_cast<double>(static_cast<int32_t>(static_cast<uint32_t>(extra) + 1u));
        score = ppc_fctiwz(scoreD * (1.0 + extraD / 10.0));
        CTRACE(7, kGtText, static_cast<uint32_t>(score));
    } else {
        db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
        if (DbWord(reinterpret_cast<const RideInfo*>(db), kSlot1b8) != 0) {
            db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
            int32_t price = DbWord(reinterpret_cast<const RideInfo*>(db), kSlot1b8);
            float ratio = static_cast<float>(static_cast<double>(price)) / 3000.0f;
            if (static_cast<double>(ratio) > 1.0) {
                float factor = 1.0f + ratio / 10.0f;
                float scoreF = static_cast<float>(static_cast<double>(score));
                score = ppc_fctiwz(static_cast<double>(scoreF * factor));
                CTRACE(7, kPriceText, static_cast<uint32_t>(score));
            }
        }
    }

    // the rides the person already used, then the recent ones: each match divides the score
    if (Half(self, 0x1e0) == reinterpret_cast<CThing*>(object)->GetThingId().mIndex) {
        score = score / 5;
    } else if (Half(self, 0x1e2) == reinterpret_cast<CThing*>(object)->GetThingId().mIndex) {
        score = score / 4;
    } else if (Half(self, 0x1e4) == reinterpret_cast<CThing*>(object)->GetThingId().mIndex) {
        score = score / 3;
    } else if (Half(self, 0x1e6) == reinterpret_cast<CThing*>(object)->GetThingId().mIndex) {
        score = score / 2;
    }
    CTRACE(7, kUsedText, static_cast<uint32_t>(score));

    if (Half(self, 0x1e8) == reinterpret_cast<CThing*>(object)->GetThingId().mIndex) score = score / 5;
    if (Half(self, 0x1ea) == reinterpret_cast<CThing*>(object)->GetThingId().mIndex) score = score / 4;
    if (Half(self, 0x1ec) == reinterpret_cast<CThing*>(object)->GetThingId().mIndex) score = score / 3;
    if (Half(self, 0x1ee) == reinterpret_cast<CThing*>(object)->GetThingId().mIndex) score = score / 2;
    CTRACE(7, kTempText, static_cast<uint32_t>(score));

    uint16_t finalId = reinterpret_cast<CThing*>(object)->GetThingId().mIndex;
    CTRACE(7, kFinalText, static_cast<uint32_t>(score), static_cast<uint32_t>(finalId));
    return static_cast<uint32_t>(score);
}

// 0x10119068
int32_t CPerson::ScoreOnCloserInspection(CObject* object) {
    if (object->IsDirty() != 0) return 100;
    int32_t preferred = GetPreferredExcitement();
    int32_t level = object->GetExcitementLevel(false);
    int32_t diff = (preferred & 0xff) - (level & 0xff);
    int32_t sign = diff < 0 ? 1 : -1;
    if (diff < 0) diff = -diff;
    if (diff < 0) diff = 0;
    else if (diff > 0x32) diff = 0x32;
    return sign * diff;
}

// CPerson::EveryTimeMods 0x1011a364, UpdateTemporaryHistory 0x10119b68, UseObject 0x101196a0, GetTimeInPark 0x1011962c, LookForMinorDest 0x1011915c
// (unit v51016). The person and the objects are reached by Mac offset.

// Names invented, zero at start (the Mac's bss).
uint16_t gMinorNoRide;    // 0x102a0fb0
uint16_t gMinorNoQueue;   // 0x102a0fb2
uint16_t gMinorListEnd;   // 0x102a0fb4
uint16_t gMinorNoDest;    // 0x102a0fb6
uint16_t gMinorNoDest2;   // 0x102a0fb8

namespace {

using MapIdTag = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

const char kMinorText[] = "Minor Decision: OID=%d, @=(%d, %d), sc=%d\n";                              // +0x4f4
const char kHappyText[] = "Person %d: Happiness changed by %d since using object %d\n";               // +0x51f

// The rider counts and the opinion buckets switch on the ride record's +0x7a8 (the UI type).
// The ride record's words are read by name (the Mac's +0x7a8, +0xe8, +0x158, +0x15c): the host's record is laid out wider.
inline int32_t UIType(CObject* object) { return object->GetDBInfo()->mUIType; }

}  // namespace

// 0x1011915c: looks around the cells beside the person for a ride closer to the one he is queueing for.
void CPerson::LookForMinorDest() {
    uint8_t* self = Raw(this);
    if (Fld_2<uint16_t>(self, 0x1dc) == gMinorNoRide) return;
    uint16_t bestId = 0;
    int32_t bestScore = 0;
    uint16_t here = static_cast<uint16_t>(Fld_2<uint8_t>(self, 4) + (Fld_2<uint8_t>(self, 6) * 128 + 1));
    int32_t x0 = static_cast<int32_t>((static_cast<uint32_t>(here) - 1u) & 0x7fu);
    int32_t y0 = static_cast<int32_t>((static_cast<uint32_t>(here) - 1u) >> 7);
    uint16_t current = Fld_2<uint16_t>(self, 0x1dc);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
    MapIdTag back = static_cast<CObject*>(gThingArray[current].mThing)->GetBackOfQueue();
    if (back.mIndex == gMinorNoQueue) return;
    int32_t u = static_cast<int32_t>((static_cast<uint32_t>(back.mIndex) - 1u) & 0x7fu);
    int32_t v = static_cast<int32_t>((static_cast<uint32_t>(back.mIndex) - 1u) >> 7);
    int32_t limit = (x0 - u) * (x0 - u) + (y0 - v) * (y0 - v);
    uint8_t* map = Raw(gWorldMap);
    for (int32_t dx = -2; dx < 2; dx++) {
        for (int32_t dy = -2; dy < 2; dy++) {
            int32_t cx = x0 + dx;
            int32_t cy = y0 + dy;
            if (cx < 0 || cx >= 0x80 || cy < 0 || cy >= 0x80) continue;
            if ((cx - u) * (cx - u) + (cy - v) * (cy - v) >= limit) continue;
            uint16_t cell = static_cast<uint16_t>(cx + (cy * 128 + 1));
            uint16_t who = Fld_2<uint16_t>(map + static_cast<int32_t>(cell - 1) * 0x44, 0x24);
            while (who != gMinorListEnd) {
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
                CObject* object = static_cast<CObject*>(gThingArray[who].mThing);
                uint8_t* o = Raw(object);
                if (Fld_2<uint8_t>(o, 2) == 3 && who != Fld_2<uint16_t>(self, 0x1dc) && object->IsAvailableToChoose() != 0) {
                    int32_t score = static_cast<int32_t>(CalculateOptionScore(object));
                    if (score > bestScore || (score == bestScore && (gWorld->mTurn & 1u) != 0)) {
                        bestId = who;
                        bestScore = score;
                    }
                }
                who = Fld_2<uint16_t>(o, 0xa);
            }
        }
    }
    if (bestId == gMinorNoDest) return;
    uint16_t ride = Fld_2<uint16_t>(self, 0x1dc);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
    MapIdTag rideCell{Fld_2<uint16_t>(Raw(gThingArray[ride].mThing), 0x32)};
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
    MapIdTag destCell{Fld_2<uint16_t>(Raw(gThingArray[bestId].mThing), 0x32)};
    MapIdTag myCell{static_cast<uint16_t>(Fld_2<uint8_t>(self, 4) + (Fld_2<uint8_t>(self, 6) * 128 + 1))};
    NavMode mode = static_cast<NavMode>(Fld_2<int32_t>(self, 0x188));
    int32_t d1 = MAP_count_walking_distance(rideCell, myCell, mode);
    int32_t d2 = MAP_count_walking_distance(rideCell, destCell, mode);
    if (d1 == -1 || d2 == -1 || !(d1 > d2) || bestId == gMinorNoDest2) return;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
    uint8_t* target = Raw(gThingArray[bestId].mThing);
    MapIdTag targetCell{Fld_2<uint16_t>(target, 0x32)};
    uint16_t pos = static_cast<uint16_t>(Fld_2<uint8_t>(target, 4) + (Fld_2<uint8_t>(target, 6) * 128 + 1));
    uint16_t pos2 = static_cast<uint16_t>(Fld_2<uint8_t>(target, 4) + (Fld_2<uint8_t>(target, 6) * 128 + 1));
    CTRACE(2, kMinorText, static_cast<uint32_t>(Fld_2<uint16_t>(target, 0xe)), static_cast<uint32_t>((static_cast<uint32_t>(pos2) - 1u) & 0x7fu),
           static_cast<uint32_t>((static_cast<uint32_t>(pos) - 1u) >> 7), bestScore);
    reinterpret_cast<CThoughts*>(self + 0x30)->RecordEvent(static_cast<CThoughts::EventType>(0x17), reinterpret_cast<CThing*>(target)->GetThingId());
    Fld_2<uint16_t>(self, 0x1de) = Fld_2<uint16_t>(self, 0x1dc);
    Fld_2<uint16_t>(self, 0x1dc) = bestId;
    reinterpret_cast<CPeep*>(this)->SetDest(targetCell);
}
// 0x1011962c: the game turns the person has been in the park, in units of four.
uint32_t CPerson::GetTimeInPark() const {
    const CWorld& world = reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
    uint32_t turn = world.mTurn;
    return (turn - Fld_2<uint32_t>(Raw(this), 0x1d4)) >> 2;
}

// 0x101196a0
void CPerson::UseObject(CObject* object) {
    uint8_t* self = Raw(this);
    CThing* thingObject = reinterpret_cast<CThing*>(object);
    CThoughts* thoughts = reinterpret_cast<CThoughts*>(self + 0x30);
    AddRideToRecentHistory(thingObject->GetThingId());
    EveryTimeMods(object);
    if (Fld_2<uint8_t>(self, 0x1f1) == 0) {
        LosingMods(object);
        if (gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{Fld_2<uint16_t>(Raw(object), 0xe)})->mUIType == 2) {
            thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(6), false);
            thoughts->RecordEvent(static_cast<CThoughts::EventType>(0x19), thingObject->GetThingId());
        }
        return;
    }
    WinningMods(object);
    int32_t afterValue = ppc_fctiwz(static_cast<double>(Fld_2<float>(self, 0x20c)));
    int32_t beforeValue = ppc_fctiwz(static_cast<double>(Fld_2<float>(self, 0x19c)));
    int32_t change = (static_cast<int32_t>(static_cast<uint32_t>(beforeValue) & 0xffu) - static_cast<int32_t>(static_cast<uint32_t>(afterValue) & 0xffu)) * 3;
    uint16_t objectId = thingObject->GetThingId().mIndex;
    uint16_t personId = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
    CTRACE(2, kHappyText, static_cast<uint32_t>(personId), change, static_cast<uint32_t>(objectId));
    object->ExpressSatisfaction(change);
    uint8_t value = static_cast<uint8_t>(change + 0x32);
    int32_t kind = UIType(object);
    if (kind == 2) {
        gWorld->GetParkAnalyser()->OfferOpinionOn(0x12, value);
    } else if (kind < 2 && kind > 0) {
        int32_t a = object->GetDBInfo()->Value(kRideSlot_SpecialIngredient).mInteger;
        if (a == 2) {
            gWorld->GetParkAnalyser()->OfferOpinionOn(0xb, value);
        } else if (a < 2) {
            if (a != 0 && a >= 0) gWorld->GetParkAnalyser()->OfferOpinionOn(0xa, value);
        } else if (a == 4) {
            gWorld->GetParkAnalyser()->OfferOpinionOn(0xd, value);
        } else if (a < 4) {
            gWorld->GetParkAnalyser()->OfferOpinionOn(0xc, value);
        }
        int32_t b = object->GetDBInfo()->Value(kRideSlot_AppearanceEffect).mInteger;
        if (b == 1) {
            gWorld->GetParkAnalyser()->OfferOpinionOn(0xf, value);
        } else if (b > 0 && b < 3) {
            gWorld->GetParkAnalyser()->OfferOpinionOn(0x10, value);
        }
    }
    object->HadAWinner();
    if (gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{Fld_2<uint16_t>(Raw(object), 0xe)})->mUIType == 2) {
        thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(5), false);
        thoughts->RecordEvent(static_cast<CThoughts::EventType>(0x18), thingObject->GetThingId());
    }
}

// 0x10119a30
void CPerson::AddRideToRecentHistory(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ride) {
    uint8_t* self = Raw(this);
    for (int32_t i = 3; i > 0; i--) Fld_2<uint16_t>(self, 0x1e0 + i * 2) = Fld_2<uint16_t>(self, 0x1de + i * 2);
    Fld_2<uint16_t>(self, 0x1e0) = ride.mIndex;
}

// 0x10119ac8
void CPerson::AddRideToRecentTemporaryHistory(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ride) {
    uint8_t* self = Raw(this);
    for (int32_t i = 3; i > 0; i--) Fld_2<uint16_t>(self, 0x1e8 + i * 2) = Fld_2<uint16_t>(self, 0x1e6 + i * 2);
    Fld_2<uint16_t>(self, 0x1e8) = ride.mIndex;
}

// 0x10119b68
void CPerson::UpdateTemporaryHistory() {
    uint8_t* self = Raw(this);
    uint32_t turn = gWorld->mTurn;
    if (turn != (turn / 20u) * 20u) return;
    for (int32_t i = 3; i > 0; i--) Fld_2<uint16_t>(self, 0x1e8 + i * 2) = Fld_2<uint16_t>(self, 0x1e6 + i * 2);
    Fld_2<uint16_t>(self, 0x1e8) = 0;
}

// CPerson::TooPricey and CPerson::ExtraExcitementEffects (unit v5238). The person and the ride record are reached by Mac offset.

namespace {

template <class T> T& At_2(void* base, size_t offset) { return *reinterpret_cast<T*>(static_cast<char*>(base) + offset); }
template <class T> T At_2(const void* base, size_t offset) { return *reinterpret_cast<const T*>(static_cast<const char*>(base) + offset); }

uint32_t Low8_2(float value) { return static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(value))) & 0xffu; }

// mulhw by 0x51eb851f, srawi 5, plus the sign bit: a signed divide by 100
int32_t Div100(int32_t x) {
    int32_t q = static_cast<int32_t>((static_cast<int64_t>(static_cast<int32_t>(0x51eb851f)) * x) >> 32) >> 5;
    return q + static_cast<int32_t>(static_cast<uint32_t>(q) >> 31);
}

// mulhwu by 0x51eb851f, srwi 5: an unsigned divide by 100
uint32_t UDiv100(uint32_t x) { return static_cast<uint32_t>((static_cast<uint64_t>(0x51eb851fu) * x) >> 32) >> 5; }

// min(100, max(0, field + amount)) with the compares of the listing (fcmpo: a NaN is neither above nor below)
void AddClamped(float& field, int32_t amount) {
    float old = field;
    float sum = static_cast<float>(amount) + old;
    if (sum > 100.0f) {
        field = 100.0f;
    } else if (sum < 0.0f) {
        field = 0.0f;
    } else {
        field = sum;
    }
}

void Offer(int32_t opinion, uint32_t value) {
    if (value >= 0x64) value = 0x64;
    gGameWorld->GetParkAnalyser()->OfferOpinionOn(opinion, static_cast<uint8_t>(value));
}

}  // namespace

// 0x10119c0c
void CPerson::ExtraExcitementEffects(const CObject* object) {
    char* balance = reinterpret_cast<char*>(&gMainBalance);
    if ((object->GetExcitementLevel(false) & 0xff) == 0) return;
    int32_t preferred = GetPreferredExcitement() & 0xff;
    int32_t diff = preferred - (object->GetExcitementLevel(false) & 0xff);
    if (diff < 0) diff = -diff;
    float& happy = At_2<float>(this, 0x19c);
    if (diff < 5) {
        AddClamped(happy, At_2<int32_t>(static_cast<const void*>(balance), 0x24));
    } else if (diff < 0xf) {
        AddClamped(happy, At_2<int32_t>(static_cast<const void*>(balance), 0x28));
    } else if (diff < 0x28) {
        AddClamped(happy, At_2<int32_t>(static_cast<const void*>(balance), 0x2c));
    }
    int32_t divisor = At_2<int32_t>(static_cast<const void*>(balance), 0x30);
    int32_t level = object->GetExcitementLevel(false) & 0xff;
    int32_t quotient = level / divisor;
    int32_t p = 100 - static_cast<int32_t>(Low8_2(At_2<float>(this, 0x1a8)));
    // mulhw by 0x66666667, srawi 3, plus the sign bit: a signed divide by 20
    int32_t q20 = static_cast<int32_t>((static_cast<int64_t>(0x66666667) * p) >> 32) >> 3;
    q20 += static_cast<int32_t>(static_cast<uint32_t>(q20) >> 31);
    AddClamped(At_2<float>(this, 0x1b0), quotient * q20);
}

// 0x10119eec
bool CPerson::TooPricey(CObject* object) const {
    uint32_t price = object->GetPricePerUse();
    if (price == 0) return false;
    // The ride record's words by name (the Mac's +0x140 to +0x15c and +0x7a8): the host's record is laid out wider.
    const RideInfo* db = object->GetDBInfo();
    int32_t base = Div100(db->Value(kRideSlot_InitCostOfGoods).mInteger * 0x73);
    uint32_t a = Low8_2(At_2<float>(this, 0x19c));
    uint32_t b = Low8_2(At_2<float>(this, 0x1b0));
    uint32_t c = Low8_2(At_2<float>(this, 0x1a4));
    uint32_t d = Low8_2(At_2<float>(this, 0x1a8));
    int32_t termA = Div100(db->Value(kRideSlot_HappinessEffect).mInteger * static_cast<int32_t>(100 - a));
    int32_t termB = Div100(db->Value(kRideSlot_VomitEffect).mInteger * static_cast<int32_t>(b));
    int32_t termC = Div100(db->Value(kRideSlot_ThirstEffect).mInteger * static_cast<int32_t>(c));
    int32_t termD = Div100(db->Value(kRideSlot_HungerEffect).mInteger * static_cast<int32_t>(d));
    int32_t factor = termA + (termC + termD + 100 - termB);
    uint32_t value = UDiv100(static_cast<uint32_t>(base * factor));
    (void)object->GetPricePerUse();
    uint32_t chance = object->GetChanceOfWinning() & 0xff;
    uint32_t prize = 0;
    if (object->GetDBInfo()->mUIType == 2) prize = object->GetPrizeValue();
    ObjectControl* control = object->GetControl();
    int32_t winnings = Div100(static_cast<int32_t>(chance * prize));
    uint32_t controlFactor = static_cast<uint32_t>(control->mRipOffOK + 100);
    uint32_t happyFactor = Low8_2(At_2<float>(this, 0x19c)) + 100;
    value = (value + static_cast<uint32_t>(winnings)) * controlFactor;
    value = UDiv100(value);
    value = UDiv100(value * happyFactor);

    uint32_t opinion = (value - price) * 10 + 0x32;
    int32_t type = object->GetDBInfo()->mUIType;
    if (type == 2) {
        Offer(9, opinion);
    } else if (type == 1) {
        switch (db->Value(kRideSlot_SpecialIngredient).mInteger) {
            case 1: Offer(1, opinion); break;
            case 2: Offer(2, opinion); break;
            case 3: Offer(3, opinion); break;
            case 4: Offer(4, opinion); break;
            default: break;
        }
        switch (db->Value(kRideSlot_AppearanceEffect).mInteger) {
            case 1: Offer(6, opinion); break;
            case 2: Offer(7, opinion); break;
            default: break;
        }
    }
    if (price > value) return true;
    return At_2<uint32_t>(this, 0x1a0) < price;
}

// 0x1011a364
void CPerson::EveryTimeMods(CObject* object) {
    uint8_t* self = Raw(this);
    int32_t kind = UIType(object);
    if (kind == 2) {
        Fld_2<int32_t>(self, 0x1cc) = Fld_2<int32_t>(self, 0x1cc) + 1;
    } else if (kind < 2) {
        if (kind == 0) {
            Fld_2<int32_t>(self, 0x1c4) = Fld_2<int32_t>(self, 0x1c4) + 1;
        } else if (kind > 0) {
            Fld_2<int32_t>(self, 0x1c8) = Fld_2<int32_t>(self, 0x1c8) + 1;
        }
    }
    PayPricePerUseForObject(object);
    object->HadCustomer();
    float cost = static_cast<float>(object->GetDBInfo()->Value(kRideSlot_FatigueEffect).mInteger);
    float happy = Fld_2<float>(self, 0x1b8);
    float after = -cost + happy;
    if (after > 100.0f) {
        Fld_2<float>(self, 0x1b8) = 100.0f;
    } else if (!(after < 0.0f)) {   // a NaN lands here, as the listing's bge does
        Fld_2<float>(self, 0x1b8) = happy - cost;
    } else {
        Fld_2<float>(self, 0x1b8) = 0.0f;
    }
}

// Unit v51002: the save-file instances for CStaffHQ::StrikeType (SAVEVARSTRUCT 0x1012c3d4, LOADVARSTRUCT 0x1012c27c) and CPerson::PersonState
// (SAVEVAR 0x1011b90c), and CPerson::PayPricePerUseForObject (0x1011a4ac). LOADVAR<ResearchCat> (0x10124554) is in
// src/ResearchLab_projects_CResearchProject_Serialise.cpp.

template<> int32_t SAVEVAR<CPerson::PersonState>(void* file, CPerson::PersonState* value, const char* name, int32_t noLog) {   // 0x1011b90c
    return serialise::SaveVar(file, value, name, noLog);
}

void CPerson::PayPricePerUseForObject(CObject* object) {   // 0x1011a4ac
    uint32_t price = object->GetPricePerUse();
    if (price != 0) {
        object->RecordNewTakings(price);
        reinterpret_cast<CPeep*>(this)->PlaySound(gSoundBanks[1], 0xd0);
        uint32_t* money = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(this) + 0x1a0);
        *money = *money - price;
    }
}

// The balloon script's start block (0x1029fd48, initial word 0x10266a60; name invented) and the two limits of a balloon's life
// (0x102a0f0c = 25 and 0x102a0f10 = 255; names invented).
uint32_t gWinningBalloonBlock = 0x10266a60;
uint32_t gBalloonLifeMin = 25;
uint32_t gBalloonLifeMax = 255;

int32_t ESP_GetRandomActionFrom(ESP_Sprites set, int32_t sprites);   // 0x1007c9f8

namespace {

constexpr char kLitter[] = "Litter gone up by %d, is now %d\n";                                               // 0x10244817
constexpr char kBought[] = "Customer bought a balloon.  Aaah.\n";                                            // 0x10244838
constexpr char kHasOne[] = "Trying to give a balloon to a person who already has one!\n";                    // 0x1024485b
constexpr char kCostume[] = "Customer returning a costume.\n";                                               // 0x10244896
constexpr char kBalance[] = "Balance file error!  Shop has unknown appearance effect (can safely be ignored for now)\n";   // 0x102448b5
constexpr char kSideshow[] = "Sideshow won - happiness up %d points to %d\n";                                // 0x1024490e

inline float& Fl(uint8_t* s, uint32_t off) { return *reinterpret_cast<float*>(s + off); }
inline uint32_t& Wd(uint8_t* s, uint32_t off) { return *reinterpret_cast<uint32_t*>(s + off); }

// The original's compare chain: above 100 -> 100, below 0 -> 0, else the sum (a NaN falls through to the store).
inline float Clamp(float sum, float stored) {
    if (sum > 100.0f) return 100.0f;
    if (sum < 0.0f) return 0.0f;
    return stored;
}

}  // namespace

// 0x1011a570
void CPerson::WinningMods(CObject* object) {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    CThoughts* thoughts = reinterpret_cast<CThoughts*>(self + 0x30);
    const CThing* objThing = reinterpret_cast<const CThing*>(object);
    const CThing* selfThing = reinterpret_cast<const CThing*>(this);

    thoughts->RecordEvent(static_cast<CThoughts::EventType>(8), objThing->GetThingId());
    // The ride record's words are read by name (the Mac's offsets: the UI type +0x7a8, the effect slots +0x144 to +0x15c): the host's
    // record is laid out wider (a vtable and 8-byte balance slots).
    const RideInfo* info = object->GetDBInfo();
    if (info->mUIType == 2) {
        object->RecordACost(object->GetPrizeValue());
        Wd(self, 0x1a0) = Wd(self, 0x1a0) + object->GetPrizeValue();
    } else {
        info = object->GetDBInfo();
        if (info->mUIType == 1) object->RecordACost(static_cast<uint32_t>(object->GetCostOfGoods()));
    }
    ExtraExcitementEffects(object);
    info = object->GetDBInfo();

    {
        float v = static_cast<float>(info->Value(kRideSlot_ThirstEffect).mInteger);
        float f = Fl(self, 0x1a4);
        float sum = -v + f;
        Fl(self, 0x1a4) = Clamp(sum, f - v);
    }
    {
        float v = static_cast<float>(info->Value(kRideSlot_HungerEffect).mInteger);
        float f = Fl(self, 0x1a8);
        float sum = -v + f;
        Fl(self, 0x1a8) = Clamp(sum, f - v);
    }
    if (info->Value(kRideSlot_ThirstEffect).mInteger > info->Value(kRideSlot_HungerEffect).mInteger) {
        reinterpret_cast<CPeep*>(this)->PlaySound(gSoundBanks[1], 0x83);
    } else if (info->Value(kRideSlot_ThirstEffect).mInteger < info->Value(kRideSlot_HungerEffect).mInteger) {
        reinterpret_cast<CPeep*>(this)->PlaySound(gSoundBanks[1], 0x84);
    }
    {
        float v = static_cast<float>(info->Value(kRideSlot_VomitEffect).mInteger);
        float f = Fl(self, 0x1b0);
        float sum = v + f;
        Fl(self, 0x1b0) = Clamp(sum, sum);
    }
    {
        float v = static_cast<float>(info->Value(kRideSlot_HappinessEffect).mInteger);
        float f = Fl(self, 0x19c);
        float sum = v + f;
        Fl(self, 0x19c) = Clamp(sum, sum);
    }
    {
        float v = static_cast<float>(info->Value(kRideSlot_LitterEffect).mInteger);
        float f = Fl(self, 0x1b4);
        float sum = v + f;
        Fl(self, 0x1b4) = Clamp(sum, sum);
    }
    if (info->Value(kRideSlot_LitterEffect).mInteger > 0) {
        CTRACE(2, kLitter, info->Value(kRideSlot_LitterEffect).mInteger, static_cast<uint32_t>(ppc_fctiwz(Fl(self, 0x1b4))) & 0xffu);
    }
    uint32_t ingredient = object->GetAmountOfSpecialIngredient() & 0xffu;
    if (info->Value(kRideSlot_HungerEffect).mInteger != 0) {
        uint32_t r = gWorld->GetRand();
        uint32_t total = static_cast<uint32_t>(info->Value(kRideSlot_HungerEffect).mInteger) + (ingredient + (r & 7u));
        if (total < 0x1eu) LowerHappiness(static_cast<Amount>(0));
    }
    if (info->Value(kRideSlot_ThirstEffect).mInteger != 0) {
        uint32_t r = gWorld->GetRand();
        uint32_t total = static_cast<uint32_t>(info->Value(kRideSlot_ThirstEffect).mInteger) + (ingredient + (r & 7u));
        if (total < 0x1eu) LowerHappiness(static_cast<Amount>(0));
    }
    {
        int32_t product = static_cast<int32_t>(static_cast<uint32_t>(info->Value(kRideSlot_HappinessEffect).mInteger) * ingredient);
        float v = static_cast<float>(product / 100);
        float f = Fl(self, 0x19c);
        float sum = v + f;
        Fl(self, 0x19c) = Clamp(sum, sum);
    }
    switch (info->Value(kRideSlot_SpecialIngredient).mInteger) {
    case 2: {
        float v = static_cast<float>(static_cast<int32_t>(ingredient));
        float f = Fl(self, 0x1a4);
        float sum = v + f;
        Fl(self, 0x1a4) = Clamp(sum, f + v);
        break;
    }
    case 1: {
        float v = static_cast<float>(static_cast<int32_t>(ingredient));
        float f = Fl(self, 0x1ac);
        float sum = v + f;
        Fl(self, 0x1ac) = Clamp(sum, f + v);
        break;
    }
    case 3: {
        int32_t product = static_cast<int32_t>(static_cast<uint32_t>(info->Value(kRideSlot_ThirstEffect).mInteger) * ingredient);
        float v = static_cast<float>(product / 100);
        float f = Fl(self, 0x1a4);
        float sum = v + f;
        Fl(self, 0x1a4) = Clamp(sum, sum);
        break;
    }
    case 4: {
        uint16_t* h = reinterpret_cast<uint16_t*>(self + 0xc4);
        *h = static_cast<uint16_t>(*h + static_cast<int16_t>(static_cast<int32_t>(ingredient * 6u) / 100));
        break;
    }
    default: break;
    }

    int32_t kind = info->Value(kRideSlot_AppearanceEffect).mInteger;
    if (kind == 1) {
        CTRACE(2, kBought);
        GAME_ASSERTMSG(Wd(self, 0x210) == 0 ? 1 : 0, kHasOne);
        ThingId id = selfThing->GetThingId();
        gWorld->SeedRand(id.mIndex);
        int32_t sprites = ESP_GetRandomSpritesFrom(static_cast<ESP_Sprites>(10));
        int32_t action = ESP_GetRandomActionFrom(static_cast<ESP_Sprites>(10), sprites);
        Wd(self, 0x210) = static_cast<uint32_t>(ESP_SpawnScript(&gWinningBalloonBlock, static_cast<ESP_Sprites>(10), sprites,
                                                                static_cast<ACTS>(action), 0.0f, 0.0f, 0.0f));
        uint32_t quality = object->GetProductQuality() & 0xffu;
        uint32_t life = (gBalloonLifeMax * quality) / 100u;
        Wd(self, 0x214) = life;
        uint32_t result = life;
        if (life < gBalloonLifeMin) result = gBalloonLifeMin;
        else if (life > gBalloonLifeMax) result = gBalloonLifeMax;
        Wd(self, 0x214) = result;
        thoughts->RecordEvent(static_cast<CThoughts::EventType>(0xc), objThing->GetThingId());
    } else if (kind == 2) {
        if (Wd(self, 0x24) == 2) {
            CTRACE(2, kCostume);
            Wd(self, 0x24) = 0;
            ThingId id = selfThing->GetThingId();
            gWorld->SeedRand(id.mIndex);
            Wd(self, 0x20) = static_cast<uint32_t>(ESP_GetRandomSpritesFrom(static_cast<ESP_Sprites>(Wd(self, 0x24))));
        } else {
            Wd(self, 0x24) = 2;
            Wd(self, 0x20) = static_cast<uint32_t>(ESP_GetRandomSpritesFrom(static_cast<ESP_Sprites>(Wd(self, 0x24))));
            thoughts->RecordEvent(static_cast<CThoughts::EventType>(0xb), objThing->GetThingId());
        }
    } else if (kind != 0) {
        GAME_ASSERTMSG(0, kBalance);
    }

    if ((*reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(object) + 0x2e) & 1u) != 0) {
        object->PersonUsedToilet(static_cast<uint32_t>(ppc_fctiwz(Fl(self, 0x1ac))) & 0xffu);
        Fl(self, 0x1ac) = 0.0f;
        thoughts->RecordEvent(static_cast<CThoughts::EventType>(0x11), objThing->GetThingId());
        if ((static_cast<uint32_t>(ppc_fctiwz(Fl(self, 0x1b0))) & 0xffu) > 0x5au) {
            ThingId none;
            none.mIndex = 0;
            thoughts->RecordEvent(static_cast<CThoughts::EventType>(0x12), none);
            Fl(self, 0x1b0) = 0.0f;
        }
        *reinterpret_cast<uint16_t*>(self + 0xc2) = gPersonTable[1];
    }

    info = object->GetDBInfo();
    if (info->mUIType == 2) {
        Wd(self, 0x1d0) = Wd(self, 0x1d0) + 1;
        uint32_t prize = object->GetPrizeValue();
        uint32_t price = object->GetPricePerUse();
        double ln2 = std::log(2.0);
        double ratio = static_cast<double>(static_cast<int32_t>(prize)) / static_cast<double>(static_cast<int32_t>(price));
        double scale = std::log(ratio) / ln2;
        uint32_t happy = GetHappinessAmount(static_cast<Amount>(1)) & 0xffu;
        int32_t shown = ppc_fctiwz(Fl(self, 0x19c));
        int32_t gained = ppc_fctiwz(static_cast<double>(happy) * scale);
        CTRACE(2, kSideshow, gained, static_cast<uint32_t>(shown) & 0xffu);
        float v = static_cast<float>(gained);
        float f = Fl(self, 0x19c);
        float sum = v + f;
        Fl(self, 0x19c) = Clamp(sum, f + v);
        gWorld->GetRand();
    }
}

// CPerson balloon, happiness and catch helpers (unit v5293).

uint32_t gBalloonPopTarget = 0x102669d8;   // 0x1029fd84

namespace {
template <typename T>
T* FieldAt(void* base, uint32_t offset) {
    return reinterpret_cast<T*>(reinterpret_cast<char*>(base) + offset);
}

// The balance rows at +0x18, +0x1c, +0x20 of gMainBalance, low byte only.
uint32_t BalanceByte(uint32_t offset) {
    return *FieldAt<uint32_t>(&gMainBalance, offset) & 0xff;
}

uint32_t AmountOf(int32_t amount) {
    switch (amount) {
        case 0: return BalanceByte(0x18);
        case 1: return BalanceByte(0x1c);
        case 2: return BalanceByte(0x20);
        default: break;
    }
    GAME_ASSERTMSG(0, "get_happiness_amount - unknown default");
    return 0;
}
}  // namespace

// 0x1011b004
void CPerson::LosingMods(CObject*) {
    CTRACE(2, "Person lost this sideshow :( \n");
    LowerHappiness(static_cast<Amount>(1));
}

// 0x1011b084
void CPerson::DrawBalloon(float x, float y, float z, int32_t mode) {
    int32_t* script = FieldAt<int32_t>(this, 0x210);
    if (*script != 0) {
        SCR_SetScriptPosition(*script, *FieldAt<float>(this, 0x218), y, *FieldAt<float>(this, 0x21c), mode);
        *FieldAt<float>(this, 0x218) = x;
        *FieldAt<float>(this, 0x21c) = z;
    }
}

// 0x1011b11c
void CPerson::MakeBalloonPop() {
    int32_t* script = FieldAt<int32_t>(this, 0x210);
    if (*script != 0) {
        ESP_ForceGoto(*script, &gBalloonPopTarget);
        *script = 0;
    }
}

// 0x1011b198
uint32_t CPerson::GetHappinessAmount(Amount amount) const {
    return AmountOf(amount);
}

// 0x1011b258
void CPerson::RaiseHappiness(Amount amount) {
    uint32_t value = AmountOf(amount) & 0xff;
    float* happiness = FieldAt<float>(this, 0x19c);
    float h = *happiness;
    float sum = static_cast<float>(value) + h;
    if (!(sum > 100.0f)) {   // ble / bge are taken for NaN
        if (!(sum < 0.0f))
            *happiness = h + static_cast<float>(value);
        else
            *happiness = 0.0f;
    } else {
        *happiness = 100.0f;
    }
}

// 0x1011b384
void CPerson::LowerHappiness(Amount amount) {
    uint32_t value = AmountOf(amount) & 0xff;
    float* happiness = FieldAt<float>(this, 0x19c);
    float h = *happiness;
    float diff = -static_cast<float>(value) + h;
    if (!(diff > 100.0f)) {
        if (!(diff < 0.0f))
            *happiness = h - static_cast<float>(value);
        else
            *happiness = 0.0f;
    } else {
        *happiness = 100.0f;
    }
}

// 0x1011b4b4: a byte of the balance row (12 bytes a row from Mac offset 0xa4) of the person's +0x1f0 byte. The index is not checked.
uint8_t CPerson::GetPreferredExcitement() const {
    uint32_t row = Fld_2<uint8_t>(Raw(this), 0x1f0);
    return static_cast<uint8_t>(gMainBalance.mValues[(0xa4 - 8) / 4 + 3 * row].mInteger);
}

// 0x1011b508
void CPerson::GetBagged() {
    int32_t* script = FieldAt<int32_t>(this, 0x210);
    if (*script != 0) {
        SCR_KillScript(*script);
        *script = 0;
    }
    FieldAt<CDelayedScript>(this, 0xc)->KillSpriteScript();
    SetState(static_cast<PersonState>(0x11));
}

// 0x1011b594
void CPerson::GetDebagged(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    uint8_t* self = Raw(this);
    CPeep* peep = reinterpret_cast<CPeep*>(this);
    MapPos x, y;
    CellPair(cell.mIndex, 0x80, 0x80, x, y);
    peep->Teleport(x, y);
    reinterpret_cast<CDelayedScript*>(self + 0xc)->SpawnSpriteScript(static_cast<CDelayedScript::ScriptIdx>(1), Fld<int32_t>(self, 0x24),
                                                                    Fld<int32_t>(self, 0x20));
    peep->PlaySound(gSoundBanks[1], 0x80);
    Fld<int32_t>(self, 0x188) = 1;
    CMap* map = gWorldMap;
    uint32_t rand = gWorld->GetRand();
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> dest = map->GetCrossingParkSidePos(static_cast<int32_t>(rand & 1u));
    if (peep->SetDest(dest) != 0) {
        CTRACE(2, kHumphText);
        SetState(static_cast<PersonState>(0x12));
    } else {
        reinterpret_cast<CThing*>(this)->KillMe();
    }
}

// 0x1011b700
int32_t CPerson::IsCatchable() const {
    int32_t state = *FieldAt<int32_t>(const_cast<CPerson*>(this), 0x220);
    if (state == 6 || state == 7 || state == 8 || state == 10 || state == 9) return 1;
    CTRACE(2, "Not catchable because of state %d\n", state);
    return 0;
}

void __sinit_Person_cpp() {
    gPersonSinitA = 0x10000;
    gPersonSinitB = 0;
}

// 0x1011b7cc, 236 bytes
template<> int32_t LOADVAR<CPerson::PersonState>(void* file, CPerson::PersonState* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);
}
