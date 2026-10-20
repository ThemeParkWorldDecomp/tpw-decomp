// The challenge hall's progress (main app 0x100f6b60 range; unit w175): FinalDestroy 0x100f6b60 (4 bytes), Serialise 0x100f6fe0 (784), AddToCounter
// 0x100f7324 (216), GetCurrentChallenge 0x100f757c (48), ReceiveMessage 0x100f75e4 (1,656), OfferFollowup 0x100f7fac (436), TellPlayerChallengeOffered
// 0x100f8194 (492), ChallengeAccepted 0x100f83cc (312), ChallengeDeclined 0x100f853c (356), EndCurrentChallenge 0x100f86d8 (196), WonCurrentChallenge
// 0x100f87d4 (788) and LostCurrentChallenge 0x100f8b20 (544). The flags are read as bytes (the Mac tests a byte against 0; a saved game can hold 2).
#include <cstdint>
#include <new>

#include "CMessageCentre.h"
#include "DateMessages.h"
#include "Debug.h"
#include "MainBalance.h"
#include "ParkMessages.h"
#include "Serialise.h"
#include "TbTimeDiff.h"
#include "ThingMessages.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

namespace {
// The names the serialiser writes (the Mac's string pool at 0x10240dfd and following).
constexpr const char* kNameCurrentChallenge = "mCurrentChallenge";
constexpr const char* kNameNextEventTime = "mNextChallengeEventTime";
constexpr const char* kNameSystemSwitchedOn = "mSystemSwitchedOn";
constexpr const char* kNameChallengeOn = "mChallengeOn";
constexpr const char* kNameWaitingForReply = "mWaitingForReply";
constexpr const char* kNameValue = "mValue";
constexpr const char* kNameActualThing = "mActualThing";

constexpr const char* kStaffText = "There really ought to be a staff type here!!!!!\n";   // 0x10240d6f
constexpr const char* kUnknownText = "Unknown message\n";                                  // 0x10240e6b
constexpr const char* kFollowupText = "Followup challenge %d is being offered\n";          // 0x10240e9c
constexpr const char* kNullText = "GetCurrentChallenge() returned NULL in EndCurrentChallenge()\n";   // 0x10240ec4
constexpr const char* kWonText = "Won the challenge!!!\n";                                 // 0x10240f02
constexpr const char* kLostText = "Lost the challenge!!!\n";                               // 0x10240f18

int32_t Slot(int slot) { return gMainBalance.mValues[slot].mInteger; }
constexpr int kDaysAfterCompletedSlot = 478;   // the balance word at 0x780
constexpr int kDaysAfterDeclinedSlot = 479;    // the balance word at 0x784
constexpr int kDeclinesToForfeitSlot = 481;    // the balance word at 0x78c
constexpr int32_t kChallengeSlots = 0x14;

// The halfword at 0x102a0668, which AddToCounter and ReceiveMessage read and nothing writes: its initial value, 0.
constexpr uint16_t kNoThing = 0;

constexpr int32_t kMsgEndOfDay = 0xb;
constexpr int32_t kMsgResearchCompleted = 0x11;
constexpr int32_t kMsgAddToChallenge = 0x1a;
constexpr int32_t kTypeResearch = 0x10;
constexpr int32_t kTypeToilets = 0x18;
constexpr int32_t kTypeStaffHappiness = 0x19;
constexpr int32_t kTypeStaffTypeHappiness = 0x1b;
// The staff model ReceiveMessage hands the analyser when the challenge's staff type is none of 0 to 4: the Mac passes r29, which nothing sets (the
// caller's value; the test makes it 0).
constexpr int32_t kModelLeftOver = 0;

// A flag byte tested against 0 (a bool with the byte 2 would be read as true by the Mac).
bool Flag(const bool& flag) { return *reinterpret_cast<const uint8_t*>(&flag) != 0; }

CMessageCentre* MessageCentreForPost() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

TbTimeStamp FunnyTimeIn(CWorld* world) {
    return world->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}

// The span of a balance slot's days: the Mac takes the low 64 bits of the sign-extended count times 864,000,000,000 (mulhwu, mullw), 0xc9 2a69c000.
TbTimeDiff DaysSpan(int32_t days) {
    TbTimeDiff span;
    span.mTicks = static_cast<int64_t>(static_cast<uint64_t>(static_cast<int64_t>(days)) * 864000000000ULL);
    return span;
}

// The funny time now plus the span, stored as the hall's next event.
void SetNextEvent(TbTimeStamp* next, int32_t days) {
    const TbTimeDiff span = DaysSpan(days);
    const TbTimeStamp when = FunnyTimeIn(gWorld) + span;
    *next = when;
}

template <class V>
int32_t Var(int32_t save, void* file, V* value, const char* name) {
    return save != 0 ? SAVEVAR<V>(file, value, name, 0) : LOADVAR<V>(file, value, name, 0);
}

template <class V>
int32_t VarStruct(int32_t save, void* file, V* value, const char* name) {
    return save != 0 ? SAVEVARSTRUCT<V>(file, value, name, 0) : LOADVARSTRUCT<V>(file, value, name, 0);
}

using ObjectId = tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
}  // namespace

void CChallengeHQ::FinalDestroy() {}

int32_t CChallengeHQ::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    mBase.Serialise(file, save, version, unused);
    if (version < 0 || version >= 2) return 0;
    for (int32_t i = 0; i < kChallengeSlots; ++i) mChallenges[i].Serialise(file, save, version, unused);
    if (Var<int32_t>(save, file, &mCurrentChallenge, kNameCurrentChallenge) != 0) return 1;
    if (VarStruct<TbTimeStamp>(save, file, NextEventTime(), kNameNextEventTime) != 0) return 1;
    if (Var<bool>(save, file, &mSystemSwitchedOn, kNameSystemSwitchedOn) != 0) return 1;
    if (Var<bool>(save, file, &mChallengeOn, kNameChallengeOn) != 0) return 1;
    if (Var<bool>(save, file, &mWaitingForReply, kNameWaitingForReply) != 0) return 1;
    if (Var<int32_t>(save, file, &mValue, kNameValue) != 0) return 1;
    if (VarStruct<ThingId>(save, file, reinterpret_cast<ThingId*>(&mActualThing), kNameActualThing) != 0) return 1;
    return 0;
}

// Adds to the counter when the current challenge is on, is of this type and target object, and is for this thing (or any, when the hall's thing is
// the none value).
void CChallengeHQ::AddToCounter(ChallengeType type, int32_t amount, ObjectId obj, ThingId thing) {
    if (GetCurrentChallenge() == nullptr) return;
    if (!Flag(mChallengeOn)) return;
    if (GetCurrentChallenge()->mType != static_cast<int32_t>(type)) return;
    if (obj.mIndex != GetCurrentChallenge()->mTargetObj) return;
    if (mActualThing != kNoThing && thing.mIndex != mActualThing) return;
    mValue = static_cast<int32_t>(static_cast<uint32_t>(mValue) + static_cast<uint32_t>(amount));
}

CChallenge* CChallengeHQ::GetCurrentChallenge() {
    if (mCurrentChallenge < 0 || mCurrentChallenge >= kChallengeSlots) return nullptr;
    return &mChallenges[mCurrentChallenge];
}

void CChallengeHQ::ReceiveMessage(const CMsg& message) {
    const ObjectId noObject{0};
    const ThingId noThing{0};
    switch (message.GetType()) {
        case kMsgResearchCompleted: {
            const CMsgResearchCompleted& done = static_cast<const CMsgResearchCompleted&>(message);
            if (static_cast<int32_t>(done.mCategory) != 0) return;
            AddToCounter(static_cast<ChallengeType>(kTypeResearch), 1, noObject, noThing);
            return;
        }
        case kMsgAddToChallenge: {
            // The message's thing id (+0x12) is not read: the thing is passed as none.
            const CMsgAddToChallenge& add = static_cast<const CMsgAddToChallenge&>(message);
            AddToCounter(add.mType, add.mValue, ObjectId{add.mObj}, noThing);
            return;
        }
        case kMsgEndOfDay: {
            if (GetCurrentChallenge() == nullptr) return;
            const int32_t type = GetCurrentChallenge()->mType;
            if (type == kTypeToilets) {
                CParkAnalyser* analyser = gGameWorldRef->GetParkAnalyser();
                const int32_t filth = analyser->AvgFilthLevelOfToilets();
                AddToCounter(static_cast<ChallengeType>(kTypeToilets), static_cast<int32_t>(100u - static_cast<uint32_t>(filth)), noObject, noThing);
            } else if (type == kTypeStaffHappiness) {
                CParkAnalyser* analyser = gGameWorldRef->GetParkAnalyser();
                AddToCounter(static_cast<ChallengeType>(kTypeStaffHappiness), analyser->AverageHappinessOfStaff(), noObject, noThing);
            } else if (type == kTypeStaffTypeHappiness) {
                int32_t model = kModelLeftOver;
                switch (GetCurrentChallenge()->mTargetStaffType) {
                    case 0: model = 5; break;
                    case 1: model = 4; break;
                    case 2: model = 6; break;
                    case 3: model = 7; break;
                    case 4: model = 8; break;
                    default: CTRACE(3, kStaffText); break;
                }
                CParkAnalyser* analyser = gGameWorldRef->GetParkAnalyser();
                const int32_t happiness = analyser->AverageHappinessOfStaffType(static_cast<CThing::ThingModel>(model));
                AddToCounter(static_cast<ChallengeType>(kTypeStaffTypeHappiness), happiness, noObject, noThing);
            }
            return;
        }
        default:
            GAME_ASSERTMSG(0, kUnknownText);
            return;
    }
}

// Offers the challenge the current one names as its follow-up: the first candidate row of that type that the player may be offered. A row the
// player refuses to be told about (the message answer false) lets the loop go on with mCurrentChallenge already changed.
bool CChallengeHQ::OfferFollowup() {
    if (Flag(mChallengeOn)) return false;
    if (GetCurrentChallenge() == nullptr) return false;
    const int32_t followup = GetCurrentChallenge()->mDoubleOrNothingFollowup;
    if (followup == 0) return false;
    for (int32_t i = 0; i < kChallengeSlots; ++i) {
        const CChallenge& row = mChallenges[i];
        const bool candidate = row.mType != 0 && !Flag(row.mHaveWon) && row.mChallengeDeclined < Slot(kDeclinesToForfeitSlot);
        if (candidate && followup == row.mType) {
            mCurrentChallenge = i;
            CTRACE(3, kFollowupText, i);
            if (TellPlayerChallengeOffered(GetCurrentChallenge(), false)) {
                mWaitingForReply = true;
                mValue = 0;
                return true;
            }
        }
    }
    return false;
}

bool CChallengeHQ::TellPlayerChallengeOffered(CChallenge* challenge, bool force) {
    if (challenge == nullptr) return false;
    const int32_t prize = challenge->mPrize;
    const int32_t targetTime = challenge->mTargetTime;
    const int32_t targetVal = challenge->mTargetVal;
    const ObjectId obj{challenge->mTargetObj};
    const ObjectId obj2{challenge->mTargetObj2};
    const int32_t staff = challenge->mTargetStaffType;
    // The listing builds the message twice (one copy per force value); one call with the chosen message number is the same.
    CMsgChallenge message(challenge->GetOfferId(), static_cast<AdvisorMessage>(force ? 0x158 : 0x10e), true, prize, targetTime, targetVal, 0, 0, obj, obj2,
                          static_cast<StaffType>(staff));
    MessageCentreForPost()->PostMessage(message);
    return true;
}

void CChallengeHQ::ChallengeAccepted() {
    if (!Flag(mWaitingForReply)) return;
    if (GetCurrentChallenge() == nullptr) return;
    mWaitingForReply = false;
    mChallengeOn = true;
    SetNextEvent(NextEventTime(), GetCurrentChallenge()->mTargetTime);
}

// The current challenge is read without a NULL test (the Mac reads c->mIndependent at address 0x1d with a bad index).
void CChallengeHQ::ChallengeDeclined() {
    if (!Flag(mWaitingForReply)) return;
    mWaitingForReply = false;
    mChallengeOn = false;
    if (!Flag(GetCurrentChallenge()->mIndependent)) {
        const int32_t prize = GetCurrentChallenge()->mPrize;
        CBankAccount* bank = gWorld->GetBankAccount();
        bank->MakeDeposit(static_cast<uint32_t>(prize / 2));
        mActualThing = 0;
    }
    SetNextEvent(NextEventTime(), Slot(kDaysAfterDeclinedSlot));
}

void CChallengeHQ::EndCurrentChallenge() {
    if (GetCurrentChallenge() == nullptr) {
        CTRACE(3, kNullText);
        return;
    }
    if (GetCurrentChallenge()->DidThisWinIt(mValue, true)) {
        WonCurrentChallenge();
    } else {
        LostCurrentChallenge();
    }
}

void CChallengeHQ::WonCurrentChallenge() {
    CTRACE(3, kWonText);
    mChallengeOn = false;
    SetNextEvent(NextEventTime(), Slot(kDaysAfterCompletedSlot));
    if (GetCurrentChallenge() == nullptr) return;
    if (GetCurrentChallenge()->mDoubleOrNothingFollowup != 0) {
        mActualThing = GetCurrentChallenge()->mThingIdForFollowup;
        if (OfferFollowup()) GetCurrentChallenge()->mThingIdForFollowup = mActualThing;
    } else {
        mActualThing = 0;
        const int32_t prize = GetCurrentChallenge()->mPrize;
        CBankAccount* bank = gWorld->GetBankAccount();
        bank->MakeDeposit(static_cast<uint32_t>(prize));
    }
    // The follow-up, when one was offered: the index is in range (OfferFollowup stores 0 to 19 only), so the Mac has no NULL test here.
    CChallenge* won = GetCurrentChallenge();
    won->mHaveWon = true;
    if (won->mDoubleOrNothingFollowup == 0) {
        const int32_t prize = won->mPrize;
        CMsgChallenge message(won->GetWinId(), static_cast<AdvisorMessage>(0x110), false, prize, 0, 0, 0, 0, ObjectId{0}, ObjectId{0},
                              static_cast<StaffType>(0));
        MessageCentreForPost()->PostMessage(message);
    }
}

void CChallengeHQ::LostCurrentChallenge() {
    CTRACE(3, kLostText);
    mChallengeOn = false;
    SetNextEvent(NextEventTime(), Slot(kDaysAfterCompletedSlot));
    if (GetCurrentChallenge() != nullptr) {
        const int32_t value = mValue;
        CChallenge* lost = GetCurrentChallenge();
        lost->mChallengeLost = lost->mChallengeLost + 1;
        const int32_t targetVal = lost->mTargetVal;
        const int32_t targetTime = lost->mTargetTime;
        const int32_t prize = lost->mPrize;
        CMsgChallenge message(lost->GetLoseId(), static_cast<AdvisorMessage>(0x111), false, prize, targetTime, targetVal, 0, value, ObjectId{0},
                              ObjectId{0}, static_cast<StaffType>(0));
        MessageCentreForPost()->PostMessage(message);
    }
    mActualThing = 0;
}
