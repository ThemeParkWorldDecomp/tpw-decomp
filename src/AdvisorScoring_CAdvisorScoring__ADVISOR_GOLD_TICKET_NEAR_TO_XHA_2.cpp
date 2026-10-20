// Unit v5892: twelve advisor scorers (0x10017c40 .. 0x1001b2b8). The balance values are read at Mac offsets from this
// (slot = (offset - 0x14) / 4, the table's values start at 0x14).
#include <new>

#include "AdvisorScoring.h"
#include "Debug.h"
#include "FS_Online.h"
#include "FS_MasterSave.h"
#include "MainBalance.h"
#include "ThingParts.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"

namespace {

inline CThing::ThingModel Model(int32_t n) { return static_cast<CThing::ThingModel>(n); }

// The shared shape of the five "trained" scorers: staff exist (the HQ word), more than a count, quality above a limit.
inline int32_t Trained(CAdvisorScoring* self, uint32_t hqWord, int32_t model, const BalanceValue& count,
                       const BalanceValue& quality, const BalanceValue& answer) {
    CStaffHQ* hq = gGameWorldRef->GetStaffHQ();
    if (!(static_cast<int32_t>(hq->mTraining.mF00[hqWord]) > 0)) return 0;
    if (!(gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(model)) > count.mInteger)) return 0;
    if (!(gGameWorldRef->GetParkAnalyser()->AverageQualityOfStaffType(Model(model)) > quality.mInteger)) return 0;
    (void)self;
    return answer.mInteger;
}

}  // namespace

#define MAC(off) (mBalance.mValues[((off) - 0x14) / 4])

// 0x1001b18c: happiness above (main balance 0x758 as float) * (float at 0x5cc), more than the visitors of 0x75c, no ticket 2 yet.
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_NEAR_TO_XHAPPINESS() {
    const float happiness = gGameWorldRef->GetParkAnalyser()->GetHappinessLevel(0);
    const float product = static_cast<float>(gMainBalance.mValues[467].mInteger) * MAC(0x5cc).mFloat;
    const float limit = static_cast<float>(ppc_fctiwz(product));
    if (!(happiness > limit)) return 0;
    const int32_t visitors = gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsInPark();
    if (!(visitors > gMainBalance.mValues[468].mInteger)) return 0;
    if (gMasterSave == nullptr) {
        CFSGlobalMasterSave* made = static_cast<CFSGlobalMasterSave*>(::operator new(0x4c));
        if (made != nullptr) new (made) CFSGlobalMasterSave();
        gMasterSave = made;
    }
    if (!gMasterSave->HaveTicket(static_cast<GoldenTicketTypeLocal>(2))) return MAC(0x5d0).mInteger;
    return 0;
}

// 0x10018854
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_BALLOONS() {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsWithBalloons() > MAC(0x4a0).mInteger) return MAC(0x4a4).mInteger;
    return 0;
}

// 0x1001873c
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_SEND_POSTCARD() {
    if (!(gGameWorldRef->GetParkAnalyser()->GetParkValue(10) > MAC(0x490).mInteger)) return 0;
    if (!(gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsInPark() > MAC(0x498).mInteger)) return 0;
    const float happiness = gGameWorldRef->GetParkAnalyser()->GetHappinessLevel(10);
    if (static_cast<float>(MAC(0x494).mInteger) < happiness) return MAC(0x49c).mInteger;
    return 0;
}

// 0x10018624
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PUBLISH_PARK() {
    if (!(gGameWorldRef->GetParkAnalyser()->GetParkValue(10) > MAC(0x480).mInteger)) return 0;
    if (!(gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsInPark() > MAC(0x488).mInteger)) return 0;
    const float happiness = gGameWorldRef->GetParkAnalyser()->GetHappinessLevel(10);
    if (static_cast<float>(MAC(0x484).mInteger) < happiness) return MAC(0x48c).mInteger;
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_CONGRAT_MECHANICS_TRAINED() {   // 0x100183ac
    return Trained(this, 1, 4, MAC(0x474), MAC(0x478), MAC(0x47c));
}

int32_t CAdvisorScoring::_ADVISOR_CONGRAT_ENTERTAINERS_TRAINED() {   // 0x100182b0
    return Trained(this, 2, 6, MAC(0x468), MAC(0x46c), MAC(0x470));
}

int32_t CAdvisorScoring::_ADVISOR_CONGRAT_HANDYMEN_TRAINED() {   // 0x100181b8
    return Trained(this, 0, 5, MAC(0x45c), MAC(0x460), MAC(0x464));
}

int32_t CAdvisorScoring::_ADVISOR_CONGRAT_RESEARCHERS_TRAINED() {   // 0x100180c0
    return Trained(this, 4, 8, MAC(0x450), MAC(0x454), MAC(0x458));
}

int32_t CAdvisorScoring::_ADVISOR_CONGRAT_SECURITY_TRAINED() {   // 0x10017fc8
    return Trained(this, 3, 7, MAC(0x444), MAC(0x448), MAC(0x44c));
}

// 0x10017edc: these two read the world through gGameWorld itself (TOC 0x1037ebfc), not through the reference.
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_VISITORS_HAPPY() {
    if (gGameWorld->GetParkAnalyser()->NumberOfVisitorsInPark() < MAC(0x438).mInteger) return 0;
    const float happiness = gGameWorld->GetParkAnalyser()->GetHappinessLevel(0);
    if (happiness >= static_cast<float>(MAC(0x43c).mInteger)) return MAC(0x440).mInteger;
    return 0;
}

// 0x10017df4
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_STAFF_HAPPY() {
    if (gGameWorld->GetParkAnalyser()->NumberOfStaff() < MAC(0x42c).mInteger) return 0;
    const float happiness = gGameWorld->GetParkAnalyser()->GetStaffHappinessLevel(0);
    if (happiness >= static_cast<float>(MAC(0x430).mInteger)) return MAC(0x434).mInteger;
    return 0;
}

// 0x10017c40: gates open, enough rides (count at 0x41c), average excitement above 0x418; the scariest ride is stored at +4.
int32_t CAdvisorScoring::_ADVISOR_VISITORS_TOO_SCARED() {
    if (!gGameWorldRef->GetParkAnalyser()->GatesOpen()) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0) < MAC(0x41c).mInteger) return 0;
    if (!(gGameWorldRef->GetParkAnalyser()->GetAverageExcitementLevelOfAllRides(10) > MAC(0x418).mInteger)) return 0;
    gGameWorldRef->GetParkAnalyser()->GetScariestRide(
        reinterpret_cast<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>&>(mWord4));
    return MAC(0x420).mInteger;
}
