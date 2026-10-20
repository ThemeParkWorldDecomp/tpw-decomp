// CStaffHQ::ConsiderStriking (unit v5681), 0x1012b084.
#include <new>

#include "CMessageCentre.h"
#include "Debug.h"
#include "ParkMessages.h"
#include "ParkTime.h"
#include "ThingReceivers.h"
#include "TbTimeDiff.h"
#include "World.h"

// For the program; the test library defines its own (recording) versions, which win over these weak ones.
__attribute__((weak)) void* staffhq_callee::OperatorNew(uint32_t size) { return ::operator new(size); }
__attribute__((weak)) void staffhq_callee::ConstructSubscriptions(void* mem) { new (mem) CMessageCentre; }

namespace {

// The five copies of the original's "create the message centre if there is none, then post".
void PostStaffInfo(StaffType type, StaffSituation situation) {
    CMsgStaffInfo message(type, situation);
    if (gMessageCentre == nullptr) {
        void* mem = staffhq_callee::OperatorNew(0x1d0);
        if (mem != nullptr) staffhq_callee::ConstructSubscriptions(mem);
        gMessageCentre = static_cast<CMessageCentre*>(mem);
    }
    gMessageCentre->PostMessage(message);
}

}  // namespace

void CStaffHQ::ConsiderStriking(StaffType type) {
    CTRACE(2, "Considering striking on turn %d\n", gWorld->mTurn);
    TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    // 0x10237e1c: signed 64 bit division by 30 days of 100 ns ticks; only the low word of the quotient is compared.
    int64_t months = since.mTicks / 0x1792F8648000LL;
    if (static_cast<int32_t>(months) < 0x18) return;

    StrikeType& strike = mStrikes[type];
    if (StaffTypeHasAGrievance(type) == 0) {
        if (strike.mF00 == 1) PostStaffInfo(type, static_cast<StaffSituation>(3));
        strike.mF00 = 0;
        StopStrikeInProgress(type);
        return;
    }
    switch (static_cast<int32_t>(strike.mF00)) {
    case 0:
        CTRACE(0xc, "** Your staff have issued a strike warning!!! Clean your park up!! **\n");
        strike.mF00 = 1;
        PostStaffInfo(type, static_cast<StaffSituation>(0));
        break;
    case 1:
        CTRACE(0xc, "** Your staff are going on strike for one week!! **\n");
        strike.mF00 = 2;
        strike.mF04 = 1;
        PostStaffInfo(type, static_cast<StaffSituation>(2));
        break;
    case 2:
        CTRACE(0xc, "** Your staff are going on strike for two weeks!! **\n");
        strike.mF00 = 3;
        strike.mF04 = 1;
        PostStaffInfo(type, static_cast<StaffSituation>(2));
        break;
    case 3:
        CTRACE(0xc, "** Your staff are going on strike for a month!! **\n");
        strike.mF00 = 4;
        strike.mF04 = 1;
        PostStaffInfo(type, static_cast<StaffSituation>(2));
        break;
    case 4:
        strike.mF00 = 4;
        strike.mF04 = 1;
        PostStaffInfo(type, static_cast<StaffSituation>(2));
        break;
    default:
        break;
    }
}
