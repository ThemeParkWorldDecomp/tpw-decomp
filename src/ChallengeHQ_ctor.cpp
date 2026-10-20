// CChallengeHQ::CChallengeHQ, main app 0x100f6640 (416 bytes), unit w122b.
#include <new>

#include "CMessageCentre.h"
#include "ThingReceivers.h"

namespace {
// Lazy centre block at 0x100f66c0, repeated before each subscription, including after allocation failure.
CMessageCentre* MessageCentreForSubscribe() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}
}

CChallengeHQ::CChallengeHQ()
    : mBase(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{1}, static_cast<CThing::ThingModel>(0x13)) {
    mCurrentChallenge = -1;
    mNextChallengeEventTime[1] = 0;
    mNextChallengeEventTime[0] = 0;
    mSystemSwitchedOn = false;
    mChallengeOn = false;
    mWaitingForReply = false;
    mValue = 0;
    mActualThing = 0;
    MessageCentreForSubscribe()->Subscribe(mBase, static_cast<MsgType>(0x1a));
    MessageCentreForSubscribe()->Subscribe(mBase, static_cast<MsgType>(0xb));
    MessageCentreForSubscribe()->Subscribe(mBase, static_cast<MsgType>(0x11));
    Init();
}
