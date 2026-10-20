// Advisor.cpp (main app): the advisor request's constructors and SetScore (0x1000deb4, 0x1000de30, 0x1000e8cc; the default constructor
// 0x1000a0b4 and Valid 0x1000e894 are inline in AdvisorQueue.h) and CAdvisor::CancelMessage, PlayLastTutorialMessage and GetTypePlaying
// (0x1000b048, 0x1000b1c4, 0x1000c760).
#include "AdvisorQueue.h"

#include "Advisor.h"
#include "Clock.h"

namespace {
// The slot of the advisor balance table (src/AdvisorBalance_table.cpp, the walk of CBalanceTable::Init) the original reads as the word at
// CAdvisorScoring + 0x32c: 0x14 + 4 * 198, the row "ContextHelpScore" of the heading "HelpMessage".
constexpr int32_t kContextHelpScoreSlot = 198;
}  // namespace

// 0x1000deb4: the message, the thing id (the high half of the tag word; the low half is never read), the secondary; then the score: a
// negative one is looked up (and not marked as overridden), else it is taken and marked; the response number, the override flag, and
// the request becomes valid last.
CAdvisorRequest::CAdvisorRequest(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId, uint32_t secondary,
                                 int32_t responseNo, bool overrideOnlyOnce, int32_t score) {
    mMessage = message;
    mThingId = thingId.mIndex;
    mSecondary = secondary;
    if (score < 0) {
        mScore = TheScoring().GetScoreFor(message);
        mScoreOverridden = 0;
    } else {
        mScore = score;
        mScoreOverridden = 1;
    }
    mResponseNo = responseNo;
    mOverrideOnlyOnce = overrideOnlyOnce;
    mValid = 1;
}

// 0x1000de30: field by field in the original's order; the two padding regions keep what they held.
CAdvisorRequest::CAdvisorRequest(const CAdvisorRequest& other) {
    mThingId = other.mThingId;
    mMessage = other.mMessage;
    mScore = other.mScore;
    mValid = other.mValid;
    mSecondary = other.mSecondary;
    mResponseNo = other.mResponseNo;
    mOverrideOnlyOnce = other.mOverrideOnlyOnce;
    mScoreOverridden = other.mScoreOverridden;
}

// 0x1000e8cc
void CAdvisorRequest::SetScore(int32_t score) {
    mScore = score;
    mScoreOverridden = 1;
}

// 0x1000c760: the compare is unsigned, so a start plus duration that wraps past 2^32 ends the line at once (and a clock that wraps
// starts it again).
AdvisorMessage CAdvisor::GetTypePlaying() {
    const uint32_t now = static_cast<uint32_t>(gClock.ReadFixed());
    if (now < mPlayStartTime + mPlayDuration) return mTypePlaying;
    return kAdvisorNoMessage;
}

// 0x1000b048: the tutorial request is dropped only when the message is a tutorial one (category 1) and the same message (the category
// is looked up only while the request is valid); every valid queued request of the message is invalidated; and if he is saying the
// message he is slapped (ADV_Slap), the message may be said again, its kill count goes up and the line ends at once.
// Quirk: the message state is indexed without a range check, so kAdvisorNoMessage (what GetTypePlaying answers while he is silent) would
// reach state[0x160], that is mF16E8[0] and mTypePlaying.
int32_t CAdvisor::CancelMessage(AdvisorMessage message) {
    int32_t answer = 0;
    if (mLastTutorialRequest.Valid()) {
        if (TheScoring().GetMessageCategory(message) == 1 && message == mLastTutorialRequest.mMessage) {
            mLastTutorialRequest.mValid = 0;
        }
    }
    for (int32_t i = 0; i < 8; ++i) {
        if (mQueue[i].Valid() && message == mQueue[i].mMessage) {
            answer = 1;
            mQueue[i].mValid = 0;
        }
    }
    if (message == GetTypePlaying()) {
        answer = 1;
        ADV_Slap();
        mMessageState[message].mSaidOnce = 0;
        mMessageState[message].mKillCount = mMessageState[message].mKillCount + 1;
        mPlayDuration = 0;
    }
    return answer;
}

// 0x1000b1c4: F1. 0 when there is no tutorial request. Else the request is scored with the balance table's ContextHelpScore, forced past
// the "say only once" and kill-count gates, and put in the queue as QueueRequest would (a free slot, else over the lowest scoring slot
// when that scores strictly lower; invalid scores count as -1). The answer is 1 whatever happened to the request. The category lookup's
// answer is dropped (the original compares it with 1 and does nothing with the result).
int32_t CAdvisor::PlayLastTutorialMessage() {
    if (!mLastTutorialRequest.Valid()) return 0;
    mLastTutorialRequest.SetScore(TheScoring().mBalance.mValues[kContextHelpScoreSlot].mInteger);
    mLastTutorialRequest.mOverrideOnlyOnce = 1;
    TheScoring().GetMessageCategory(mLastTutorialRequest.mMessage);
    if (CanIQueueThis(mLastTutorialRequest.mMessage, mLastTutorialRequest.mOverrideOnlyOnce != 0)) {
        int32_t slot = FindFirstInvalidSlotInQueue();
        if (slot >= 0) {
            mQueue[slot] = mLastTutorialRequest;
        } else {
            slot = FindLowestScoringSlotInQueue();
            if (slot >= 0) {
                const int32_t newScore = mLastTutorialRequest.mValid ? mLastTutorialRequest.mScore : -1;
                const int32_t oldScore = mQueue[slot].mValid ? mQueue[slot].mScore : -1;
                if (oldScore < newScore) mQueue[slot] = mLastTutorialRequest;
            }
        }
    }
    return 1;
}
