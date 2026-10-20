// The research lab's projects, efforts and outputs (main app, unit w211): 21 CResearchLab functions (0x10121b7c FinalDestroy to 0x10123a5c
// GetResearchOutputFromCategory) and 5 CResearchProject ones (0x10123d7c SetupProject to 0x1012420c GetRemainingPoints). They choose a
// category's next or previous project, share the researchers' points over the categories, raise a category's level, answer the research
// panel, run the cheats, keep the lab's active flag, and set up, advance and measure a project.
//
// The lab keeps 0x100 projects of 0x14 bytes at +0xc and five categories of 0x10 bytes at +0x140c (an effort, an output, the index of the
// current project and the highest group open). A project is done when its progress (a float) has reached its score. Every project needs a
// ride record (CRideDatabase) and every finished one makes the ride's object control available (CControlManager).
#include <cstdint>
#include <new>

#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "MainBalance.h"
#include "ObjectParts.h"
#include "ParkMessages.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "ppc_math.h"

namespace {
using ObjectId = tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

// The halfword at 0x102a10f8, which CResearchLab::UpdateActiveStatus compares the researcher list's head with: the original's 0 (the name is mine).
constexpr uint16_t kNoThing = 0;
// The balance words of the category levels: Mac offset 0x500 + 4 * level is slot 318 + level (the percentage of the level's projects
// that must be done before the next level opens).
constexpr int kLevelSlot = 318;
constexpr int kCategories = 5;
constexpr int kStatUpgradeCategory = 4;   // a project's mCategory of a statistics upgrade; the category argument 5 means all of them

int32_t Slot(int slot) { return gMainBalance.mValues[slot].mInteger; }

// The ride record's value slot `slot` as the Mac lays it out: 0xc + 4 * slot (the host's own record has 8 byte slots).
int32_t RideValue(const RideInfo* ride, int slot) { return ride->Value(static_cast<RideBalanceSlot>(slot)).mInteger; }

ObjectId ObjectIdOf(uint16_t id) {
    ObjectId tag;
    tag.mIndex = id;
    return tag;
}

// The object controls and the golden tickets are reached through the global world (read again at every use, as the Mac does).
CControlManager* Controls() { return gWorld->GetControlManager(); }
CGoldenTicketControl* GoldenTickets() {
    return reinterpret_cast<CGoldenTicketControl*>(reinterpret_cast<uintptr_t>(gWorld) + 0x2c0);
}

// The const manager the lab's getters use: GetWorld() of the lab (a CWorldSavvy in the original) at +0x1d943c.
const CControlManager* ControlsOf(const CResearchLab* lab) {
    return reinterpret_cast<const CControlManager*>(
        reinterpret_cast<uintptr_t>(&reinterpret_cast<const CWorldSavvy*>(lab)->GetWorld()) + 0x1d943c);
}

// The park analyser of the world the lab's GetWorld answers (the const getter, 0x1013eb38; the lab is a CWorldSavvy to the Mac).
const CParkAnalyser* AnalyserOf(const CResearchLab* lab) {
    const CWorld& world = reinterpret_cast<const CWorldSavvy*>(lab)->GetWorld();
    return world.GetParkAnalyser();
}

CMessageCentre* MessageCentre() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

// The Mac tests the flag byte against 0 (lbz, cmplwi): any nonzero byte is set, though only 0 and 1 are ever stored.
bool IsSet(const bool& flag) { return *reinterpret_cast<const unsigned char*>(&flag) != 0; }

// One of the advisor's research lines (0x85 to 0x88: a category was raised and still has projects left), about no thing in particular. The
// world is read before the request is built, as the Mac does.
void QueueAdvisorLine(int32_t line) {
    CWorld* const world = gWorld;
    ThingId none;
    none.mIndex = 0;
    CAdvisorRequest request(static_cast<AdvisorMessage>(line), none, 0, -1, false, -1);
    world->GetAdvisor()->QueueRequest(request);
}

// A statistics upgrade can be picked when the ride type it upgrades (the ride's UpgradesId, or the project's own id when that is 0) has been
// researched, and, when that type costs a golden ticket, the ticket has been bought. The Mac passes the upgraded type's id in the high half.
bool StatUpgradeIsOpen(const CResearchProject& project) {
    const RideInfo* info = gRideDatabase.GetRideInfo(ObjectIdOf(project.mObjectId));
    uint16_t typeId = static_cast<uint16_t>(RideValue(info, kRideSlot_UpgradesId));
    if (typeId == 0) typeId = project.mObjectId;
    bool open = Controls()->GetObjectControl(ObjectIdOf(typeId), 0)->mAvailable != 0;
    info = gRideDatabase.GetRideInfo(ObjectIdOf(typeId));
    if (RideValue(info, kRideSlot_GoldenTicketCost) > 0 && !GoldenTickets()->HasBeenBought(ObjectIdOf(typeId))) open = false;
    return open;
}
}  // namespace

// 36 bytes: the lab's own part is the thing at its start.
void CResearchLab::FinalDestroy() { mBase.FinalDestroy(); }

// 260 bytes. The points are shared over the categories that have a project not yet done, by their efforts, and scaled by the work load:
// points * (effort / total) * workLoad / 100, each step a single float operation (the unsigned words convert exactly, then round to float).
void CResearchLab::ContributeResearch(uint32_t points) {
    const uint32_t total = GetTotalEffortFromActiveCategories();
    if (total == 0) return;
    for (int32_t i = 0; i < kCategories; ++i) {
        if (GetEffort(i) != 0 && mCategories[i].mF08 >= 0) {
            const uint32_t effort = GetEffort(i);
            const float share = static_cast<float>(effort) / static_cast<float>(total);
            float amount = static_cast<float>(points) * share;
            amount = amount * static_cast<float>(static_cast<uint32_t>(mF1460));
            amount = amount / 100.0f;
            mProjects[mCategories[i].mF08].ContributeResearch(amount);
        }
    }
}

// 460 bytes. The category's next project: the first open one after the current index, or else the first open one of the list (the wrap);
// an open project is of the category, within the category's level, not done and, for a statistics upgrade, StatUpgradeIsOpen. The search stops
// at the first open one past the current. With none found the current one is dropped when it is done.
void CResearchLab::NextProject(ResearchCat category) {
    int32_t found = -1;
    for (int32_t i = 0; i < mProjectCount; ++i) {
        CResearchProject* project = &mProjects[i];
        if (category != project->mCategory) continue;
        if (project->mF08 > mCategories[category].mF0C) continue;
        if (project->IsProjectCompleted()) continue;
        bool open = true;
        if (category == kStatUpgradeCategory) open = StatUpgradeIsOpen(*project);
        if (!open) continue;
        const int32_t current = mCategories[category].mF08;
        if (i < current) {
            if (found < 0) found = i;
        } else if (i > current) {
            found = i;
            break;
        }
    }
    if (found >= 0) {
        mCategories[category].mF08 = found;
    } else if (mCategories[category].mF08 >= 0 && mProjects[mCategories[category].mF08].IsProjectCompleted()) {
        mCategories[category].mF08 = -1;
    }
}

// 460 bytes: NextProject walking the list backwards (from the last project; the count is read once), wrapping to the last open one.
void CResearchLab::PreviousProject(ResearchCat category) {
    int32_t found = -1;
    for (int32_t i = mProjectCount - 1; i >= 0; --i) {
        CResearchProject* project = &mProjects[i];
        if (category != project->mCategory) continue;
        if (project->mF08 > mCategories[category].mF0C) continue;
        if (project->IsProjectCompleted()) continue;
        bool open = true;
        if (category == kStatUpgradeCategory) open = StatUpgradeIsOpen(*project);
        if (!open) continue;
        const int32_t current = mCategories[category].mF08;
        if (i > current) {
            if (found < 0) found = i;
        } else if (i < current) {
            found = i;
            break;
        }
    }
    if (found >= 0) {
        mCategories[category].mF08 = found;
    } else if (mCategories[category].mF08 >= 0 && mProjects[mCategories[category].mF08].IsProjectCompleted()) {
        mCategories[category].mF08 = -1;
    }
}

// 16 bytes
uint32_t CResearchLab::GetEffort(uint32_t group) const { return mCategories[group].mEffort; }

// 148 bytes: the efforts of the categories whose current project exists and is not done.
uint32_t CResearchLab::GetTotalEffortFromActiveCategories() const {
    uint32_t sum = 0;
    for (int32_t i = 0; i < kCategories; ++i) {
        const int32_t index = mCategories[i].mF08;
        if (index >= 0 && !mProjects[index].IsProjectCompleted()) sum += mCategories[i].mEffort;
    }
    return sum;
}

// 76 bytes: the effort is at most 100; the outputs follow; the answer is the stored effort read again after CalculateOutputs.
uint32_t CResearchLab::SetEffort(int32_t group, uint32_t effort) {
    if (effort >= 100) effort = 100;
    mCategories[group].mEffort = effort;
    CalculateOutputs();
    return mCategories[group].mEffort;
}

// 56 bytes: the work load is at most 100; the outputs follow.
void CResearchLab::SetWorkLoad(uint32_t load) {
    if (load >= 100) load = 100;
    mF1460 = static_cast<int32_t>(load);
    CalculateOutputs();
}

// 172 bytes: the category's output scaled by how able the researchers are (their current ability as a percentage of the most they could
// have); no possible ability answers 0. GetWorld is called with the lab itself, each time, as on the Mac.
uint32_t CResearchLab::GetResearchSpeed(ResearchCat category) const {
    const uint32_t maxAbility = static_cast<uint32_t>(AnalyserOf(this)->MaxResearcherAbility());
    if (maxAbility == 0) return 0;
    const uint32_t currentAbility = static_cast<uint32_t>(AnalyserOf(this)->CurrentResearcherAbility());
    const uint32_t speed = (currentAbility * 100) / maxAbility;
    const uint32_t output = GetResearchOutputFromCategory(category);
    return speed * output / 100;
}

// 68 bytes: the object id of the category's current project, 0 for none (one halfword; the Mac writes only that through its result pointer).
tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> CResearchLab::GetCurrentProject(ResearchCat category) const {
    ObjectId result;
    const int32_t index = mCategories[category].mF08;
    if (index < 0) {
        result.mIndex = 0;
    } else {
        result.mIndex = mProjects[index].mObjectId;
    }
    return result;
}

// 196 bytes: what the category's current project costs. A statistics upgrade costs its ride's CostOfUpgrade of the project's level (slot 107
// + 16 * level); the other kinds cost the object control's upgrade cost (the field's name is narrower than its use). 0 for no project.
int32_t CResearchLab::GetEstimatedCostForCurrentProject(ResearchCat category) const {
    const int32_t index = mCategories[category].mF08;
    if (index < 0) return 0;
    if (category == kStatUpgradeCategory) {
        const uint8_t level = mProjects[index].mUpgradeLevel;
        const RideInfo* info = gRideDatabase.GetRideInfo(ObjectIdOf(mProjects[index].mObjectId));
        return RideValue(info, kRideSlot_Upgrades_CostOfUpgrade + 16 * level);
    }
    const uint16_t id = mProjects[index].mObjectId;
    return ControlsOf(this)->GetObjectControl(ObjectIdOf(id), 0)->mUpgradeCost;
}

// 100 bytes: the percentage of the best project's score still to do on the category's current project (0 for none). The best project's score
// is read before the call.
uint32_t CResearchLab::GetRemainingPoints(ResearchCat category) const {
    const int32_t index = mCategories[category].mF08;
    if (index < 0) return 0;
    const float best = mProjects[mBestProject].mScore;
    return mProjects[index].GetRemainingPoints(best);
}

// 160 bytes: the projects of the category (5: all) not yet done, counted from the last.
int32_t CResearchLab::GetNumberLeftToResearch(ResearchCat category) const {
    int32_t left = 0;
    for (int32_t i = mProjectCount - 1; i >= 0; --i) {
        if (category == 5 || category == mProjects[i].mCategory) {
            if (!mProjects[i].IsProjectCompleted()) left++;
        }
    }
    return left;
}

// 728 bytes. While the share of the projects of the group within the category's current level (group byte up to the level) that are done
// reaches the level's balance threshold (unsigned compare) and the level is below 7, the level goes up by one. When it went up and projects
// of the group are left, the advisor says so: lines 0x85 to 0x88 for groups 0 to 3.
void CResearchLab::UpdateHighestGroup(int32_t group) {
    bool raised = false;
    for (;;) {
        const int32_t visible = mCategories[group].mF0C + 1;
        uint32_t total = 0;
        uint32_t done = 0;
        for (int32_t i = mProjectCount - 1; i >= 0; --i) {
            CResearchProject* project = &mProjects[i];
            if (group == project->mCategory && project->mF08 < visible) {
                total++;
                if (project->IsProjectCompleted()) done++;
            }
        }
        const uint32_t percent = total == 0 ? 0 : done * 100 / total;
        const uint8_t level = mCategories[group].mF0C;
        if (percent < static_cast<uint32_t>(Slot(kLevelSlot + level))) break;
        if (!(level < 7)) break;
        raised = true;
        mCategories[group].mF0C = mCategories[group].mF0C + 1;
    }
    if (!raised) return;
    if (GetNumberLeftToResearch(static_cast<ResearchCat>(group)) == 0) return;
    switch (group) {
        case 0: QueueAdvisorLine(0x85); break;
        case 1: QueueAdvisorLine(0x86); break;
        case 2: QueueAdvisorLine(0x87); break;
        case 3: QueueAdvisorLine(0x88); break;
        default: break;
    }
}

// 112 bytes: every project is done (the last one is looked at first).
bool CResearchLab::IsAllResearchDone() const {
    for (int32_t i = mProjectCount - 1; i >= 0; --i) {
        if (!mProjects[i].IsProjectCompleted()) return false;
    }
    return true;
}

// 148 bytes: every project's object becomes available (the projects themselves are not touched).
void CResearchLab::CheatCompleteAll() {
    for (int32_t i = mProjectCount - 1; i >= 0; --i) {
        Controls()->GetObjectControl(ObjectIdOf(mProjects[i].mObjectId), 0)->mAvailable = 1;
    }
}

// 228 bytes: a project's object is available exactly when the project is done (an unfinished one's is cleared, even if it was available).
void CResearchLab::CheatCompleteOnlyPaidFor() {
    for (int32_t i = mProjectCount - 1; i >= 0; --i) {
        if (mProjects[i].IsProjectCompleted()) {
            Controls()->GetObjectControl(ObjectIdOf(mProjects[i].mObjectId), 0)->mAvailable = 1;
        } else {
            Controls()->GetObjectControl(ObjectIdOf(mProjects[i].mObjectId), 0)->mAvailable = 0;
        }
    }
}

// 100 bytes: the category has a current project and it is not done.
bool CResearchLab::IsCategoryActive(ResearchCat category) const {
    const int32_t index = mCategories[category].mF08;
    if (index < 0) return false;
    return !mProjects[index].IsProjectCompleted();
}

// 336 bytes. The lab is active while the world's researcher list is not empty (the head halfword at +0x1da742 differs from kNoThing); each
// change of the flag is told to the park as an event: 5 when it turns on, 6 when it turns off. The flag is written before the event is built.
void CResearchLab::UpdateActiveStatus() {
    const uint16_t head = gWorld->mF1DA742;
    if (head != kNoThing) {
        if (!IsSet(mHaveMechanic)) {
            mHaveMechanic = true;
            CMsgEvent event(static_cast<EventType>(5));
            MessageCentre()->PostMessage(event);
        }
    } else if (IsSet(mHaveMechanic)) {
        mHaveMechanic = false;
        CMsgEvent event(static_cast<EventType>(6));
        MessageCentre()->PostMessage(event);
    }
}

// 140 bytes: each category's output is its share of the efforts (a percentage, rounded down), scaled by the work load (rounded down); no
// effort at all gives 0. All words are unsigned and wrap.
void CResearchLab::CalculateOutputs() {
    uint32_t total = 0;
    for (int32_t i = 0; i < kCategories; ++i) total += mCategories[i].mEffort;
    for (int32_t i = 0; i < kCategories; ++i) {
        if (total == 0) {
            mCategories[i].mOutput = 0;
        } else {
            const uint32_t share = (mCategories[i].mEffort * 100) / total;
            mCategories[i].mOutput = share * static_cast<uint32_t>(mF1460) / 100;
        }
    }
}

// 16 bytes
uint32_t CResearchLab::GetResearchOutputFromCategory(ResearchCat category) const { return mCategories[category].mOutput; }

// ---- CResearchProject

// 176 bytes. A ride whose research costs nothing gives no project: its object is made available at once and the answer is 0. Else the
// project is the ride: its id, group, cost (the score, as a float), category and no progress; the answer is 1.
char CResearchProject::SetupProject(const RideInfo* ride) {
    if (RideValue(ride, kRideSlot_Upgrades_CostOfResearch) == 0) {
        Controls()->GetObjectControl(ObjectIdOf(ride->mId), 0)->mAvailable = 1;
        return 0;
    }
    mObjectId = ride->mId;
    mF08 = static_cast<uint8_t>(RideValue(ride, kRideSlot_Group));
    mScore = static_cast<float>(RideValue(ride, kRideSlot_Upgrades_CostOfResearch));
    mCategory = RideValue(ride, kRideSlot_Category);
    mUpgradeLevel = 0;
    mProgress = 0.0f;
    return 1;
}

// 280 bytes. A ride of category 4 has no upgrades to research (0). Else from the level after the ones the ride's object control has
// available: the first of the three levels with a research cost (slot 93 + 16 * level) becomes the project (category 4, group 0) and the
// answer is 1; a level without a cost is counted as available at once (the control's mUpgradesAvailable is set to it). 0 when none is left.
char CResearchProject::SetupStatUpgrade(const RideInfo* ride) {
    if (RideValue(ride, kRideSlot_Category) == kStatUpgradeCategory) return 0;
    const int32_t available = Controls()->GetObjectControl(ObjectIdOf(ride->mId), 0)->mUpgradesAvailable;
    for (int32_t level = available + 1; level < 3; ++level) {
        const int32_t cost = RideValue(ride, kRideSlot_Upgrades_CostOfResearch + 16 * level);
        if (cost > 0) {
            mUpgradeLevel = static_cast<uint8_t>(level);
            mF08 = 0;
            mCategory = kStatUpgradeCategory;
            mScore = static_cast<float>(cost);
            mObjectId = ride->mId;
            mProgress = 0.0f;
            return 1;
        }
        Controls()->GetObjectControl(ObjectIdOf(ride->mId), 0)->mUpgradesAvailable = level;
    }
    return 0;
}

// 296 bytes. A project not yet done takes the points; the one that this finishes makes its object available, sets the object's upgrades
// available to its level and tells the park (CMsgResearchCompleted). The level is read before the second control is looked up.
void CResearchProject::ContributeResearch(float points) {
    if (IsProjectCompleted()) return;
    mProgress = mProgress + points;
    if (!IsProjectCompleted()) return;
    Controls()->GetObjectControl(ObjectIdOf(mObjectId), 0)->mAvailable = 1;
    const uint8_t level = mUpgradeLevel;
    Controls()->GetObjectControl(ObjectIdOf(mObjectId), 0)->mUpgradesAvailable = level;
    CMsgResearchCompleted message(ObjectIdOf(mObjectId), static_cast<ResearchCat>(mCategory));
    MessageCentre()->PostMessage(message);
}

// 36 bytes: the progress has reached the score (a NaN on either side: not done).
bool CResearchProject::IsProjectCompleted() const { return mProgress >= mScore; }

// 88 bytes: the percentage of the best score still to do, 100 * (score - progress) / best, as an unsigned word (CodeWarrior's conversion:
// below 0 gives 0, 2^32 and above or NaN gives 0xFFFFFFFF). A score of 0.1 or less, or a best of 1 or less (or a NaN of either), gives 0.
uint32_t CResearchProject::GetRemainingPoints(float best) const {
    if (!(mScore > 0.1f)) return 0;
    if (!(best > 1.0f)) return 0;
    float left = mScore - mProgress;
    left = 100.0f * left;
    left = left / best;
    return cw_cvt_fp2unsigned(left);
}
