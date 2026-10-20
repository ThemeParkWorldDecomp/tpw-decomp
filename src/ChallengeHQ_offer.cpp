// The challenge hall's list and offers (main app 0x100f680c range; unit w130): ClearInvalidFollowups 0x100f680c (188 bytes), AddChallenge 0x100f7498
// (164 bytes), IsSystemOn 0x100f7c98 (272 bytes) and OfferNextChallenge 0x100f7dd8 (412 bytes). Reads *gWorld; the funny time is asked of it again
// for the span.
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "FS_Online.h"
#include "MainBalance.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

namespace {
int32_t Slot(int slot) { return gMainBalance.mValues[slot].mInteger; }
constexpr int kDaysBeforeSystemSlot = 480;   // the balance word at 0x788: the day the system switches on
constexpr int kDeclinedLimitSlot = 481;      // the balance word at 0x78c
constexpr int32_t kChallengeSlots = 0x14;

// 0x10237e1c: the signed 64 bit division of the span in ticks by 864,000,000,000 (a day); the Mac keeps the low word.
int32_t DaysOf(const TbTimeDiff& span) { return static_cast<int32_t>(span.mTicks / 864000000000LL); }
}  // namespace

// A follow-up that no other challenge in the list has the type of is dropped: the owning challenge's type is cleared (at once, so later
// rows see it).
void CChallengeHQ::ClearInvalidFollowups() {
    for (int32_t i = 0; i < kChallengeSlots; ++i) {
        if (mChallenges[i].mType != 0 && mChallenges[i].mDoubleOrNothingFollowup != 0) {
            const int32_t followup = mChallenges[i].mDoubleOrNothingFollowup;
            bool found = false;
            for (int32_t j = 0; j < kChallengeSlots; ++j) {
                if (j != i && mChallenges[j].mType != 0 && mChallenges[j].mType == followup) {
                    found = true;
                    break;
                }
            }
            if (!found) mChallenges[i].mType = 0;
        }
    }
}

// The challenge goes into the first free row (type 0), all 0x30 bytes copied (the padding too); a full list drops it.
void CChallengeHQ::AddChallenge(CChallenge& challenge) {
    for (int32_t i = 0; i < kChallengeSlots; ++i) {
        if (mChallenges[i].mType == 0) {
            std::memcpy(&mChallenges[i], &challenge, sizeof(CChallenge));
            return;
        }
    }
}

// Not in the online or easy modes (the game type must be 0). Once the gates are open and the days since the session started reach the
// balance word, the system switches on for good and the first challenge is offered at once.
bool CChallengeHQ::IsSystemOn() {
    if (TheGameType().mType != 0) return false;
    if (mSystemSwitchedOn) return true;
    if (gWorld->GetParkAnalyser()->GatesOpen()) {
        TbTimeDiff span = gWorld->mParkTime.GetFunnyTimeSinceSessionStart();
        if (!(DaysOf(span) < Slot(kDaysBeforeSystemSlot))) {
            mSystemSwitchedOn = true;
            OfferNextChallenge(true);
            return true;
        }
    }
    return false;
}

// Looks from the row after the current one round the ring (twenty more rows than the current index) for a row that is used, not won,
// has a declined count below the balance word and is independent; it becomes the current one and the player is told.
void CChallengeHQ::OfferNextChallenge(bool force) {
    if (mChallengeOn || mWaitingForReply) return;
    mValue = 0;
    const int32_t current = mCurrentChallenge;
    int32_t first = 0;
    if (current >= 0) first = current + 1;
    const int32_t last = static_cast<int32_t>(static_cast<uint32_t>(current) + kChallengeSlots);
    for (int32_t i = first; i <= last; ++i) {
        const int32_t index = i - (i / kChallengeSlots) * kChallengeSlots;
        bool eligible = mChallenges[index].mType != 0;
        if (eligible) {
            if (mChallenges[index].mHaveWon) {
                eligible = false;
            } else {
                eligible = mChallenges[index].mChallengeDeclined < Slot(kDeclinedLimitSlot);
            }
        }
        if (eligible && mChallenges[index].mIndependent) {
            CTRACE(3, "Challenge %d is being offered!\n", index);
            mCurrentChallenge = index;
            CChallenge* challenge = nullptr;
            if (mCurrentChallenge >= 0 && mCurrentChallenge < kChallengeSlots) challenge = &mChallenges[mCurrentChallenge];
            if (TellPlayerChallengeOffered(challenge, force)) mWaitingForReply = true;
            return;
        }
    }
}
