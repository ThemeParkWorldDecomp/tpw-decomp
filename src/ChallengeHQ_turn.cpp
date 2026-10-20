// CChallengeHQ::UpdateTurn, main app 0x100f6b98 (1048 bytes), unit w124: offers the next challenge, reports its progress every fourth turn, ends it
// when its time is up, and warns when little time is left. Reads *gWorld; the first clock read after the turn word uses the world pointer read once
// after the GetCurrentChallenge test, the others read the global again.
#include <cstdint>
#include <new>

#include "CMessageCentre.h"
#include "MainBalance.h"
#include "TbTimeDiff.h"
#include "ThingMessages.h"
#include "ThingReceivers.h"
#include "World.h"

namespace {
CMessageCentre* MessageCentreForPost() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

TbTimeStamp FunnyTimeIn(CWorld* world) {
    return world->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}

// 0x10237e1c: the signed 64 bit division of the span in ticks by 864,000,000,000 (a day); the Mac keeps the low word.
int32_t DaysOf(const TbTimeDiff& span) { return static_cast<int32_t>(span.mTicks / 864000000000LL); }
}

void CChallengeHQ::UpdateTurn() {
    if (!IsSystemOn()) return;
    if (!mChallengeOn) {
        if (mWaitingForReply) return;
        TbTimeStamp now = FunnyTimeIn(gWorld);
        if (now > *NextEventTime()) OfferNextChallenge(false);
        return;
    }
    if (GetCurrentChallenge() == nullptr) return;
    CWorld* world = gWorld;
    if ((world->mTurn & 3) == 0) {
        TbTimeStamp now = FunnyTimeIn(world);
        TbTimeDiff left = *NextEventTime() - now;
        int32_t days = DaysOf(left);
        int32_t value = mValue;
        CChallenge* challenge = GetCurrentChallenge();
        int32_t staff = challenge->mTargetStaffType;
        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj{challenge->mTargetObj};
        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj2{challenge->mTargetObj2};
        int32_t targetVal = challenge->mTargetVal;
        int32_t targetTime = challenge->mTargetTime;
        int32_t prize = challenge->mPrize;
        int32_t id = challenge->GetActiveId();
        CMsgChallenge message(id, kAdvisorNoMessage, false, prize, targetTime, targetVal, days, value, obj, obj2, static_cast<StaffType>(staff));
        MessageCentreForPost()->PostMessage(message);
        return;
    }
    {
        TbTimeStamp now = FunnyTimeIn(world);
        if (now > *NextEventTime()) {
            EndCurrentChallenge();
            return;
        }
    }
    if (GetCurrentChallenge()->DidThisWinIt(mValue, false)) {
        WonCurrentChallenge();
        return;
    }
    if (GetCurrentChallenge()->mTargetTime == 0) return;
    TbTimeStamp now = FunnyTimeIn(gWorld);
    TbTimeDiff left = *NextEventTime() - now;
    int32_t days = DaysOf(left);
    int32_t targetTime = GetCurrentChallenge()->mTargetTime;
    int32_t percent = static_cast<int32_t>(static_cast<uint32_t>(days) * 100u) / targetTime;
    if (percent < gMainBalance.mValues[kMainBalanceShortTimeLeftWarningAt].mInteger) {
        CMsgChallenge message(0x17f, static_cast<AdvisorMessage>(0x10f), false, 0, 0, 0, 0, 0, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{0},
                              tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{0}, static_cast<StaffType>(0));
        MessageCentreForPost()->PostMessage(message);
    }
}
