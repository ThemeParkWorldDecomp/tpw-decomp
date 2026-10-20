// CResearchLab::ReceiveMessage (0x10123144, 716 bytes) and CResearchLab::Serialise (0x10121ce8, 744 bytes), unit v5024.
#include "ThingReceivers.h"

#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "Serialise.h"
#include "TPEngine.h"
#include "World.h"

namespace {

// The lab is told the game is won: the advisor says line 0xa7, then the winning sequence starts. The world is read once before the
// request is built (the first copy of the code in the original) and again for the sequence.
void WinningAdvice() {
    CWorld* world = gWorld;
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> noThing{0};
    CAdvisorRequest request(static_cast<AdvisorMessage>(0xa7), noThing, 0, -1, false, -1);
    world->GetAdvisor()->QueueRequest(request);
    gWorld->StartWinningSequence();
}

CFSGlobalMasterSave* MasterSave() {
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave();
    return gMasterSave;
}

}  // namespace

// Kind 0x11 (a ride became available): the next project slot is set up as a statistics upgrade (kept when it gives one), the category's
// level is raised, the category update is posted, and when all the research is done the master save records it and the game is won (in the
// easy game at once, in the normal game only the first time).
void CResearchLab::ReceiveMessage(const CMsg& message) {
    if (message.GetType() != 0x11) return;
    const CMsgRideResearched& ride = static_cast<const CMsgRideResearched&>(message);
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id{ride.mObjectId};
    const int32_t category = ride.mCategory;
    RideInfo* info = gRideDatabase.GetRideInfo(id);
    if (mProjects[mProjectCount].SetupStatUpgrade(info)) mProjectCount = mProjectCount + 1;
    UpdateHighestGroup(category);
    CMsgResearchCategoryUpdated updated(static_cast<ResearchCat>(category));
    if (gMessageCentre == nullptr) gMessageCentre = new CMessageCentre();
    gMessageCentre->PostMessage(updated);
    if (!IsAllResearchDone()) return;
    MasterSave()->SetAllResearchCompletedInCurrentTheme();
    if (TheGameType().mType == kGameTypeEasy) {
        WinningAdvice();
    } else if (TheGameType().mType == kGameTypeNormal) {
        if (MasterSave()->HasAllResearchBeenCompleted()) WinningAdvice();
    }
}

int32_t CResearchLab::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (mBase.Serialise(file, save, version, unused) != 0) return 1;
    if (save != 0) {
        if (version < 2 && version >= 0) {
            for (int32_t i = 0; i < 0x100; ++i)
                if (mProjects[i].Serialise(file, save, version, unused) != 0) return 1;
            for (int32_t i = 0; i < 5; ++i)
                if (mCategories[i].Serialise(file, save, version, unused) != 0) return 1;
            if (SAVEVAR(file, &mProjectCount, "mNumProjects", 0) != 0) return 1;
            if (SAVEVAR(file, reinterpret_cast<MacULong*>(&mF1460), "mWorkLoad", 0) != 0) return 1;
            if (SAVEVAR(file, &mBestProject, "mMostExpensiveProject", 0) != 0) return 1;
            if (SAVEVAR(file, &mHaveMechanic, "mLabActive", 0) != 0) return 1;
        }
    } else {
        if (version < 2 && version >= 0) {
            for (int32_t i = 0; i < 0x100; ++i)
                if (mProjects[i].Serialise(file, save, version, unused) != 0) return 1;
            for (int32_t i = 0; i < 5; ++i)
                if (mCategories[i].Serialise(file, save, version, unused) != 0) return 1;
            if (LOADVAR(file, &mProjectCount, "mNumProjects", 0) != 0) return 1;
            if (LOADVAR(file, reinterpret_cast<MacULong*>(&mF1460), "mWorkLoad", 0) != 0) return 1;
            if (LOADVAR(file, &mBestProject, "mMostExpensiveProject", 0) != 0) return 1;
            if (LOADVAR(file, &mHaveMechanic, "mLabActive", 0) != 0) return 1;
        }
    }
    return 0;
}
