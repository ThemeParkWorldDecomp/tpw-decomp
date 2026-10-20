// The research lab's start-up (main app 0x10121bd4 range; unit w130): CResearchLab::Init 0x10121bd4 (232 bytes), SetupProjects 0x10122004 (368 bytes)
// and SetupCategories 0x101221a8 (208 bytes). The projects and the effort helpers are other units' (fakes in the test).
#include <cstdint>
#include <new>

#include "CMessageCentre.h"
#include "MainBalance.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

namespace {
int32_t Slot(int slot) { return gMainBalance.mValues[slot].mInteger; }
// The balance words: Mac offset 0x528 + 4 * group (the research effort of each group) is slot 328 + group; 0x540 is slot 334.
constexpr int kEffortSlot = 328;
constexpr int kF1460Slot = 334;
constexpr int kProjectGroups = 5;

CMessageCentre* MessageCentreForSubscribe() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}
}  // namespace

void CResearchLab::Init() {
    mProjectCount = 0;
    mBestProject = -1;
    mF1460 = Slot(kF1460Slot);
    mHaveMechanic = gWorld->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(8)) > 0;
    SetupProjects();
    SetupCategories();
    MessageCentreForSubscribe()->Subscribe(mBase, static_cast<MsgType>(0x11));
}

// Every ride that loaded and has a UI type below 4 gives a project, or else a statistics upgrade; the best is the one with the highest score.
// The replace test is fcmpo then ble: a new score replaces the best only when it is greater (a NaN does not).
void CResearchLab::SetupProjects() {
    RideInfo* ride = gRideDatabase.GetFirstRide();
    while (ride != nullptr) {
        if (ride->mLoadFailed == 0 && ride->mUIType < 4) {
            CResearchProject* project = &mProjects[mProjectCount];
            bool added = project->SetupProject(ride) != 0;
            if (!added) added = project->SetupStatUpgrade(ride) != 0;
            if (added) {
                if (mBestProject < 0 || mProjects[mProjectCount].mScore > mProjects[mBestProject].mScore) mBestProject = mProjectCount;
                mProjectCount++;
            }
        }
        ride = gRideDatabase.GetNextRide();
    }
    CalculateOutputs();
}

// For each group: its effort, then the first project of the group that is within the category's level and not completed becomes the
// category's current one.
void CResearchLab::SetupCategories() {
    for (int32_t group = 0; group < kProjectGroups; ++group) {
        SetEffort(group, static_cast<uint32_t>(Slot(kEffortSlot + group)));
        for (int32_t i = 0; i < mProjectCount; ++i) {
            if (group == mProjects[i].mCategory && mProjects[i].mF08 <= mCategories[group].mF0C && !mProjects[i].IsProjectCompleted()) {
                mCategories[group].mF08 = i;
                break;
            }
        }
        UpdateHighestGroup(group);
    }
}
