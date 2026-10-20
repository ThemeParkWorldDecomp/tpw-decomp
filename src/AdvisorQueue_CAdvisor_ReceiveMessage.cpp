// CAdvisor::ReceiveMessage (main 0x1000c874, 5,440 bytes; unit v5631). Advisor.cpp on the Mac.
#include "AdvisorQueue.h"
#include "TPSettings.h"

#include "CMsgEvent.h"
#include "Debug.h"
#include "MsgAdvisor.h"
#include "ParkMessages.h"
#include "ThingMessages.h"
#include "ThingMessages2.h"
#include "World.h"

namespace {

using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

// The tail the original inlines after every request it builds (QueueRequest's body): a tutorial line is remembered for F1, then the request
// goes to the first free slot of the queue or, with the queue full, over the lowest scoring slot when it scores higher (an invalid request
// scores -1).
void Offer(CAdvisor& advisor, const CAdvisorRequest& request) {
    if (TheScoring().GetMessageCategory(request.mMessage) == 1) advisor.mLastTutorialRequest = request;
    if (!advisor.CanIQueueThis(request.mMessage, request.mOverrideOnlyOnce != 0)) return;
    int32_t slot;
    for (slot = 0; slot < 8; slot++) {
        if (advisor.mQueue[slot].Valid() == 0) break;
    }
    if (slot >= 8) slot = -1;
    if (slot >= 0) {
        advisor.mQueue[slot] = request;
        return;
    }
    const int32_t lowest = advisor.FindLowestScoringSlotInQueue();
    if (lowest < 0) return;
    const int32_t newScore = request.mValid != 0 ? request.mScore : -1;
    const int32_t oldScore = advisor.mQueue[lowest].mValid != 0 ? advisor.mQueue[lowest].mScore : -1;
    if (oldScore < newScore) advisor.mQueue[lowest] = request;
}

// A request for `message` with the original's defaults: response -1 (the scorer picks), no override, score -1 (looked up).
void Build(CAdvisor& advisor, int32_t message, uint16_t thing, uint32_t secondary) {
    ThingId thingId;
    thingId.mIndex = thing;
    CAdvisorRequest request(static_cast<AdvisorMessage>(message), thingId, secondary, -1, false, -1);
    Offer(advisor, request);
}

}  // namespace

// Types 5 (an advisor request), 8 (a ride condemned), 0xe (a thing's notice), 0x11 (research completed), 0x13 (an event), 0x17 (staff info)
// and 0x19 (a challenge's line) are handled; every other type is traced. The original's jump table (0x1027de10) covers types 5 to 0x19.
void CAdvisor::ReceiveMessage(const CMsg& message) {
    switch (message.GetType()) {
    case 5: {
        const CAdvisorRequest& request = static_cast<const CMsgAdvisor&>(message).mRequest;
        if (request.Valid() == 0) return;
        Offer(*this, request);
        return;
    }
    case 0x11: {
        const CMsgResearchCompleted& research = static_cast<const CMsgResearchCompleted&>(message);
        int32_t line;
        switch (static_cast<int32_t>(research.mCategory)) {
        case 0: line = 0x5d; break;
        case 1: line = 0x5f; break;
        case 2: line = 0x60; break;
        case 3: line = 0x61; break;
        case 4: line = 0x5e; break;
        default:
            CTRACE(3, "unknown research category in CAdvisor::ReceiveMessage.\n");
            return;
        }
        Build(*this, line, 0, research.mObj);
        return;
    }
    case 0x13:
        HandleEvent(static_cast<EventType>(static_cast<const CMsgEvent&>(message).mEvent));
        return;
    case 0x17:
        HandleStaffInfo(static_cast<const CMsgStaffInfo&>(message));
        return;
    case 0xe: {
        const CMsgThingAdvice& advice = static_cast<const CMsgThingAdvice&>(message);
        const uint16_t row = advice.mThingId;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
        // The thing's map cell id: 1 + x + (y << 7), kept to 16 bits.
        const CThing* thing = static_cast<const CThing*>(gThingArray[row].mThing);
        const uint16_t cell = static_cast<uint16_t>(thing->mCellX + ((static_cast<uint32_t>(thing->mCellY) << 7) + 1));
        int32_t line;
        switch (advice.mKind) {
        case 0: line = 0x7d; break;
        case 1: line = 0x7e; break;
        case 2: line = 0x7f; break;
        default: return;
        }
        Build(*this, line, row, cell);
        return;
    }
    case 8:
        Build(*this, 0x102, static_cast<const CMsgRideCondemned&>(message).mThingId, 0);
        return;
    case 0x19: {
        const AdvisorMessage line = static_cast<const CMsgChallenge&>(message).mMessage;
        if (line == kAdvisorNoMessage) return;
        Build(*this, line, 0, 0);
        return;
    }
    default:
        TRACE("CAdvisor::ReceiveMessage has encountered an unknown (to it) message type (%d).\n", message.GetType());
        return;
    }
}
