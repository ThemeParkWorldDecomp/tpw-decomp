// Unit v5971: the open / close park, researcher and coverage scorers of CAdvisorScoring and GetHighestScoringFromNext.
// Balance values are read by their Mac offset in CAdvisorScoring (the values start at 0x14, four bytes each).
#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "Debug.h"
#include "FS_Online.h"
#include "History.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"

namespace {

int32_t Slot(CAdvisorScoring* self, int32_t macOffset) {
    return self->mBalance.mValues[(macOffset - 0x14) / 4].mInteger;
}

CThing::ThingModel Model(int32_t n) { return static_cast<CThing::ThingModel>(n); }

// The park time in 0x1792F8648000-tick units, as the 64 bit divide helper 0x10237e1c leaves it (the low word is used).
int32_t SessionUnits() {
    TbTimeDiff t = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    return static_cast<int32_t>(t.mTicks / 0x1792F8648000LL);
}

// The original's inlined record lookup (the same as src/AdvisorScoring.cpp's), including its bug of answering the record at the
// message's own index when the number was found elsewhere in the table.
const AdvisorMessageRecord* Lookup(int32_t message) {
    if (static_cast<uint32_t>(message) - static_cast<uint32_t>(kAdvisorMessageCount) < 2u) return nullptr;
    const bool inBlock = message >= -1 && message <= kAdvisorMessageCount;
    if (inBlock && message == gAdvisorMessages[message].mMessage) return &gAdvisorMessages[message];
    for (int32_t i = 0; i < kAdvisorMessageCount; ++i) {
        if (message == gAdvisorMessages[i].mMessage) return gAdvisorMessages + message;
    }
    CTRACE(3, "Not found: %d\n", message);
    return nullptr;
}

}  // namespace

// 0x10010f88: scores the next `count` considered records from the cursor (wrapping at 0x15f); the best one the advisor can queue wins.
AdvisorMessage CAdvisorScoring::GetHighestScoringFromNext(int32_t count) {
    int32_t result = kAdvisorNoMessage;
    int32_t best = 0;
    for (int32_t done = 0; done < count;) {
        if (mCursor >= kAdvisorMessageCount) mCursor = 0;
        if (gAdvisorMessages[mCursor].mSkipped == 0) {
            const AdvisorMessageRecord* record = Lookup(gAdvisorMessages[mCursor].mMessage);
            bool valid;
            if (record == nullptr) {
                valid = false;
            } else if (record->mMode == 2) {
                valid = true;
            } else {
                valid = false;
                if (record->mMode == 0) {
                    if (TheGameType().mType == 0) valid = true;
                }
                if (!valid && record->mMode == 1) {
                    if (TheGameType().mType == 2) valid = true;
                }
            }
            int32_t score = valid ? (this->*gAdvisorMessages[mCursor].mScorer)() : -1;
            if (best < score) {
                if (gGameWorldRef->GetAdvisor()->CanIQueueThis(static_cast<AdvisorMessage>(gAdvisorMessages[mCursor].mMessage), false)) {
                    result = gAdvisorMessages[mCursor].mMessage;
                    best = score;
                }
            }
            ++done;
        }
        ++mCursor;
    }
    return static_cast<AdvisorMessage>(result);
}

// 0x100113dc
int32_t CAdvisorScoring::_ADVISOR_OPEN_PARK() {
    if (gGameWorldRef->GetParkAnalyser()->GatesOpen()) return 0;
    bool rides = gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0) > 0;
    bool sideshows = gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjSideshow, 0) > 0;
    if (rides || sideshows) {
        int32_t outside = gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsOutsidePark();
        return Slot(this, 0xa4) * outside;
    }
    return 0;
}

// 0x1001151c
int32_t CAdvisorScoring::_ADVISOR_CLOSE_PARK() {
    if (!gGameWorldRef->GetParkAnalyser()->GatesOpen()) return 0;
    bool rides = gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0) > 0;
    bool sideshows = gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjSideshow, 0) > 0;
    if (rides || sideshows) return 0;
    return Slot(this, 0xa8);
}

// 0x100121b0
int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_RESEARCHERS_1() {
    if (gGameWorldRef->IsParkClosed()) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(8)) > 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->TypesOfRidesLeftToResearch() == 0) return 0;
    int32_t available = gGameWorldRef->GetParkAnalyser()->TypesOfRidesAvailable(false);
    if (available == 0) return 0;
    int32_t inPark = gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjRide, false);
    if (inPark == available) return 0;
    int32_t percent = (inPark * 100) / available;
    if (percent > Slot(this, 0x114)) return Slot(this, 0x118) * (percent - Slot(this, 0x114));
    return 0;
}

// 0x10012468
int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_RESEARCHERS_3() {
    if (gGameWorldRef->IsParkClosed()) return 0;
    uint32_t maxAbility = static_cast<uint32_t>(gGameWorldRef->GetParkAnalyser()->MaxResearcherAbility());
    if (maxAbility == 0) return 0;
    if (SessionUnits() < Slot(this, 0x120)) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(8)) == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->CurrentNumberOfStaffTypeForHire(static_cast<StaffType>(4)) == 0) return 0;
    uint32_t ability = static_cast<uint32_t>(gGameWorldRef->GetParkAnalyser()->CurrentResearcherAbility() * 100);
    if (ability / maxAbility < static_cast<uint32_t>(Slot(this, 0x124))) return Slot(this, 0x128);
    return 0;
}

// 0x1001274c (reads the world through gGameWorld for the last query)
int32_t CAdvisorScoring::_ADVISOR_STAFF_MECHANIC_COVERAGE() {
    if (SessionUnits() < Slot(this, 0x138)) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(4)) < Slot(this, 0x12c)) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffTypeWithPatrolArea(Model(4)) == 0) return 0;
    int32_t percent = gGameWorld->GetParkAnalyser()->PercentageOfAreaPatrolledBy(Model(4), static_cast<CParkAnalyser::AreaType>(2));
    int32_t result = Slot(this, 0x140) * (100 - percent);
    if (result < Slot(this, 0x13c)) return 0;
    return result;
}

// 0x100129e4
int32_t CAdvisorScoring::_ADVISOR_STAFF_HANDYMAN_COVERAGE() {
    if (SessionUnits() < Slot(this, 0x150)) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(5)) == 0) return 0;
    int32_t percent = gGameWorld->GetParkAnalyser()->PercentageOfAreaPatrolledBy(Model(5), static_cast<CParkAnalyser::AreaType>(3));
    int32_t result = Slot(this, 0x158) * (100 - percent);
    if (result < Slot(this, 0x154)) return 0;
    return result;
}

// 0x10013444
int32_t CAdvisorScoring::_ADVISOR_STAFF_TRAIN_RESEARCHERS() {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(8)) == 0) return 0;
    if (static_cast<int32_t>(gGameWorldRef->GetStaffHQ()->mTraining.mF00[4]) > 0) return 0;   // 0x1c: the training centre's fifth word
    if (gGameWorldRef->GetParkAnalyser()->AverageQualityOfStaffType(Model(8)) < Slot(this, 0x1b4)) return Slot(this, 0x1b8);
    return 0;
}
