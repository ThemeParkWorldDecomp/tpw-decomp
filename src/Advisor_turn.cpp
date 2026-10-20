// CAdvisor::UpdateTurn, main app 0x1000a8c8 (1212 bytes), unit w124: queues the best message the scorers like, and says the best queued one when
// it scores above the minimum and he is not already speaking. Reads *gWorld for the random response. The scoring object is a function-local static
// on the Mac (the guard byte at 0x102b4100 is tested before each use), so every use below calls TheScoring() again.
#include <cstdint>

#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "World.h"

void CAdvisor::UpdateTurn() {
    // 1: queue the best message of the next record when it scores above the minimum.
    AdvisorMessage best = TheScoring().GetHighestScoringFromNext(1);
    int32_t score = TheScoring().GetScoreFor(best);
    int32_t threshold = TheScoring().mBalance.mValues[kAdvisorBalanceMinScore].mInteger;
    if (score > threshold) {
        QueueRequest(CAdvisorRequest(best, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0}, 0, -1, false, -1));
    }
    // 2: nothing while he speaks; one turn of silence after the byte at +0x16e8 is 1.
    if (IsPlaying()) return;
    if (mF16E8[0] == 1) {
        mF16E8[0] = 0;
        return;
    }
    // 3: the best queued request.
    int32_t slot = FindHighestScoringSlotInQueue();
    if (slot >= 0) {
        int32_t queuedScore = mQueue[slot].mValid != 0 ? mQueue[slot].mScore : -1;
        int32_t minimum = TheScoring().mBalance.mValues[kAdvisorBalanceMinScore].mInteger;
        if (queuedScore > minimum) {
            CAdvisorRequest* request = &mQueue[slot];
            int32_t index = request->mResponseNo;
            if (index < 0) {
                // The response is the scorer's choice. The original leaves an unassigned index in a caller-saved register (r25); 0 is used here.
                int32_t count = TheScoring().GetNumResponsesFor(request->mMessage);
                int32_t cycling = TheScoring().GetCyclingFor(request->mMessage);
                int32_t current = mMessageState[request->mMessage].mResponseIndex;
                index = 0;
                switch (cycling) {
                case 3:
                    index = static_cast<int32_t>(gWorld->GetRand() % static_cast<uint32_t>(count));
                    break;
                case 1:
                    index = current < count - 1 ? current + 1 : current;
                    break;
                case 2:
                    index = current + 1;
                    break;
                case 0:
                    if (current != count - 1) index = current + 1;
                    break;
                default:
                    break;
                }
                if (index >= 0) {
                    int32_t count2 = TheScoring().GetNumResponsesFor(request->mMessage);
                    if (!(index < count2)) index = 0;
                } else {
                    index = 0;
                }
            }
            // 5: say it.
            uint32_t length;
            int32_t played;
            if (request->mScoreOverridden == 0) {
                played = request->Play(&length, index);
            } else {
                played = request->PlayWithoutScore(&length, index);
            }
            if (played == 0) return;
            SetPlayingFor(length + 1000u);
            mMessageState[request->mMessage].mLastSaid.SetTimeSig();
            mMessageState[request->mMessage].mResponseIndex = index;
            mMessageState[request->mMessage].mSaidOnce = 1;
            mTypePlaying = request->mMessage;
            return;
        }
    }
    // 6: nothing to say; the clock is read and the answer dropped.
    TheScoring();
    Clock().SecondsSinceLastUpdate();
}
