// Unit v5252: eight advisor scorers about the bank account and the park analyser (main app, 0x10015f68 .. 0x10016a3c).
#include <cstdint>

#include "AdvisorScoring.h"
#include "ThingReceivers.h"
#include "TbTimeDiff.h"
#include "World.h"
#include "analyser.h"

namespace {

// The balance value at the Mac offset from the scoring object (values start at 0x18, four bytes each on the Mac).
inline int32_t Bal(CAdvisorScoring* self, int32_t macOffset) {
    return self->mBalance.mValues[(macOffset - 0x18) / 4].mInteger;
}

// BeenInTheRedFor() / 0x1792f8648000 (the runtime's 64-bit divide, 0x10237e1c); the answer's low word is tested.
inline int32_t MonthsInTheRed() {
    TbTimeDiff d = gGameWorld->GetBankAccount()->BeenInTheRedFor();
    return static_cast<int32_t>(d.mTicks / 0x1792F8648000LL);
}

// The shared head of the in-the-red scorers: in the red with no loan available.
inline bool RedWithoutLoan() {
    if (gGameWorld->GetBankAccount()->GetBalance() >= 0) return false;
    return gGameWorld->GetBankAccount()->AnyLoanAvailable() == 0;
}

}  // namespace

int32_t CAdvisorScoring::_ADVISOR_IN_THE_RED_SIX_MONTHS() {   // 0x10016420
    if (gGameWorld->GetBankAccount()->GetBalance() < 0 && !gGameWorld->GetBankAccount()->AnyLoanAvailable()) {
        if (MonthsInTheRed() < 3) return Bal(this, 0x344);
    }
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_TAKE_LOAN() {   // 0x1001636c
    if (gGameWorld->GetBankAccount()->GetBalance() < 0 && gGameWorld->GetBankAccount()->AnyLoanAvailable())
        return Bal(this, 0x340);
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_IN_THE_RED_THREE_MONTHS() {   // 0x10016524
    if (gGameWorld->GetBankAccount()->GetBalance() < 0 && !gGameWorld->GetBankAccount()->AnyLoanAvailable()) {
        int32_t months = MonthsInTheRed();
        if (months >= 3 && months < 5) return Bal(this, 0x348);
    }
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_IN_THE_RED_MONTH_LEFT() {   // 0x10016630
    if (gGameWorld->GetBankAccount()->GetBalance() < 0 && !gGameWorld->GetBankAccount()->AnyLoanAvailable()) {
        if (MonthsInTheRed() == 5) return Bal(this, 0x34c);
    }
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_WEALTH_HIGH() {   // 0x10016160
    if (gGameWorld->GetBankAccount()->GetBalance() > Bal(this, 0x330)) {
        if (gGameWorld->GetBankAccount()->GetNumberOfLoansBought() == 0) return Bal(this, 0x334);
    }
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_GENERAL_LAX_SECURITY() {   // 0x10015f68
    if (gGameWorld->GetParkAnalyser()->MisbehavingVisitorsLastMonth(true) > Bal(this, 0x314))
        return Bal(this, 0x318) * gGameWorld->GetParkAnalyser()->MisbehavingVisitorsLastMonth(true);
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_LOTS_HANDYMAN() {   // 0x100168e8
    int32_t limit = Bal(this, 0x360) + gGameWorld->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0);
    if (gGameWorld->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(5)) > limit)
        return Bal(this, 0x364);
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_LOTS_ENTERTAINERS() {   // 0x100169b4
    int32_t limit = Bal(this, 0x368) + gGameWorld->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0);
    if (gGameWorld->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(6)) > limit)
        return Bal(this, 0x36c);
    return 0;
}
