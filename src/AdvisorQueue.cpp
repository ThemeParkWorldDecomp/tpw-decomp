// Advisor.cpp (main app): the request queue, 0x1000ae48..0x1000b8c4.
#include "AdvisorQueue.h"
#include "TPSettings.h"

CAdvisorScoring& TheScoring() {
    static CAdvisorScoring scoring;
    return scoring;
}

int32_t CAdvisor::QueueRequest(const CAdvisorRequest& request) {
    if (TheScoring().GetMessageCategory(request.mMessage) == 1) mLastTutorialRequest = request;
    if (!CanIQueueThis(request.mMessage, request.mOverrideOnlyOnce != 0)) return 0;
    int32_t slot = FindFirstInvalidSlotInQueue();
    if (slot >= 0) {
        mQueue[slot] = request;
        return 1;
    }
    slot = FindLowestScoringSlotInQueue();
    if (slot < 0) return 0;
    const int32_t newScore = request.mValid ? request.mScore : -1;
    const int32_t oldScore = mQueue[slot].mValid ? mQueue[slot].mScore : -1;
    if (oldScore < newScore) mQueue[slot] = request;
    return 1; // A full queue reports success even when it drops the request.
}

bool CAdvisor::CanIQueueThis(AdvisorMessage message, bool force) const {
    if (!gPlayerSettings.mCurrent.mTutorialOn && TheScoring().IsThisATutorialMessage(message)) return false;
    const AdvisorMessageState& state = mMessageState[message]; // Original has no range check.
    if (state.mLastSaid.GetTimeSecond() != 0) {
        const uint32_t minTime = static_cast<uint32_t>(TheScoring().GetMinTimeFor(message));
        const uint32_t elapsed = static_cast<uint32_t>(state.mLastSaid.SecondsSinceLastUpdate());
        if (elapsed < minTime) return false;
    }
    if (!force && TheScoring().SayOnlyOnce(message) && state.mSaidOnce != 0) return false;
    if (!force && TheScoring().HasThisBeenKilledTooManyTimes(message, state.mKillCount)) return false;
    int32_t count = 0;
    for (int32_t i = 0; i < 8; ++i) {
        if (mQueue[i].Valid() && mQueue[i].mMessage == message) ++count;
    }
    const int32_t maximum = static_cast<int32_t>(TheScoring().GetMaxDuplicatesInQueueFor(message) & 0xff);
    if (count < maximum) {
        if (TheScoring().GetCyclingFor(message) == 0) {
            const int32_t responses = TheScoring().GetNumResponsesFor(message);
            if (state.mResponseIndex == static_cast<int32_t>(static_cast<uint32_t>(responses) - 1u)) return false;
        }
        return true;
    }
    return false;
}

int32_t CAdvisor::FindFirstInvalidSlotInQueue() {
    for (int32_t i = 0; i < 8; ++i) {
        if (!mQueue[i].Valid()) return i;
    }
    return -1;
}

int32_t CAdvisor::FindLowestScoringSlotInQueue() {
    int32_t slot = -1;
    int32_t lowest = 0;
    for (int32_t i = 0; i < 8; ++i) {
        if (!mQueue[i].Valid()) continue;
        const int32_t score = mQueue[i].mValid ? mQueue[i].mScore : -1;
        if (slot < 0 || score < lowest) {
            slot = i;
            lowest = score;
        }
    }
    return slot;
}
