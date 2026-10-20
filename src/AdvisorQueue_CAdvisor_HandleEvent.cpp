#include "AdvisorQueue.h"
#include "TPSettings.h"
#include "FS_Online.h"
#include "CMsgEvent.h"
#include "platform/samsutils.h"

// CAdvisor::HandleEvent 0x1000ba18 (2,496 bytes): an event message (jump table at 0x1027dde4, events 0 to 10; anything above does
// nothing) queues a canned advisor message, or on the reset event clears the history of every message. The original repeats the same
// queueing code inline for each message; QueueEventMessage is that code once.

namespace {

// Builds the request, remembers it when it is a tutorial message, and puts it in the first free slot or over the lowest scoring one
// when that scores less.
void QueueEventMessage(CAdvisor* self, AdvisorMessage message) {
    CAdvisorRequest request(message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0}, 0, -1, false, -1);
    if (TheScoring().GetMessageCategory(request.mMessage) == 1) self->mLastTutorialRequest = request;
    if (!self->CanIQueueThis(request.mMessage, request.mOverrideOnlyOnce)) return;
    int32_t slot = 0;
    while (slot < 8) {
        if (!self->mQueue[slot].Valid()) break;
        slot++;
    }
    if (slot >= 8) slot = -1;
    if (slot >= 0) {
        self->mQueue[slot] = request;
        return;
    }
    slot = self->FindLowestScoringSlotInQueue();
    if (slot < 0) return;
    int32_t newScore = request.mValid != 0 ? request.mScore : -1;
    int32_t oldScore = self->mQueue[slot].mValid != 0 ? self->mQueue[slot].mScore : -1;
    if (oldScore < newScore) self->mQueue[slot] = request;
}

}  // namespace

void CAdvisor::HandleEvent(EventType event) {
    switch (static_cast<uint32_t>(event)) {
    case 0:
        QueueEventMessage(this, static_cast<AdvisorMessage>(0));
        if (TheGameType().mType == 2) QueueEventMessage(this, static_cast<AdvisorMessage>(0x143));
        break;
    case 2:
        QueueEventMessage(this, static_cast<AdvisorMessage>(0x6a));
        break;
    case 3:
        QueueEventMessage(this, static_cast<AdvisorMessage>(0x80));
        break;
    case 4:
        QueueEventMessage(this, static_cast<AdvisorMessage>(0x81));
        break;
    case 10:
        SamsUtilities::UDebug("Clearing advisor history afer receiving RESET_FOR_EASY_MODE event\n");
        for (int32_t i = 0; i < kAdvisorMessageCount; i++) {
            mMessageState[i].mKillCount = 0;
            mMessageState[i].mResponseIndex = -1;
            mMessageState[i].mSaidOnce = 0;
        }
        break;
    default:
        break;
    }
}
