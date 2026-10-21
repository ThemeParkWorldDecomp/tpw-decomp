// ResearchLab.cpp: name proven by __sinit_ResearchLab_cpp in the main application.

#include "AdvisorQueue.h"
#include "analyser.h"
#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "helpers/ResearchLabHelpers.h"
#include "helpers/ThingHelpers.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "ParkMessages.h"
#include "ppc_math.h"
#include "RegionEffectCell.h"
#include "RideDB.h"
#include "Serialise.h"
#include "tb_rain.h"
#include "TbDynamicStringTemplate.h"
#include "ThingParts.h"
#include "ThingReceivers.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "World.h"
#include "WorldSavvy.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

// The constructors of the things a new level makes (unit w122a, main app): CThing::CThing (Thing.cpp) and the constructors of CStaffHQ,
// CMechanicHQ, CAdvisor, CTagSystem, CResearchLab, CWeather and CUIMsgReceiver. The six leaf constructors they build their parts with
// (CTrainingCentre, CWeatherForecast, CStaffHQ::StrikeType, AdvisorHistory (host: AdvisorMessageState, written in CAdvisor's loop),
// CResearchProject, CResearchCategory) are inline in include/ThingParts.h and include/ThingReceivers.h.
//
// Every derived constructor builds its CThing first (the host holds it as the first member, which is constructed before the others) with
// map cell id 1 and the model of its class, then its own fields, then subscribes to the message centre. The Mac's constructors leave most
// bytes of the object alone (a new object keeps whatever the allocator gave); so do these, and no member has an initialiser.

// 0x10121ad0 (128 bytes, ResearchLab.cpp), model 0xe: the 256 projects (0x10123abc, empty) and the five categories (0x101242a0).
CResearchLab::CResearchLab() : mBase(FirstCell(), static_cast<CThing::ThingModel>(0xe)) {}

// The research lab's start-up (main app 0x10121bd4 range; unit w130): CResearchLab::Init 0x10121bd4 (232 bytes), SetupProjects 0x10122004 (368 bytes)
// and SetupCategories 0x101221a8 (208 bytes). The projects and the effort helpers are other units' (fakes in the test).

namespace {
int32_t Slot(int slot) { return gMainBalance.mValues[slot].mInteger; }
// The balance words: Mac offset 0x528 + 4 * group (the research effort of each group) is slot 328 + group; 0x540 is slot 334.
constexpr int kEffortSlot = 328;
constexpr int kF1460Slot = 334;
constexpr int kProjectGroups = 5;

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

// CResearchLab::ReceiveMessage (0x10123144, 716 bytes) and CResearchLab::Serialise (0x10121ce8, 744 bytes), unit v5024.

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

typedef tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short> ObjIdTag;
template<> int32_t SAVEVARSTRUCT<ObjIdTag>(void*, ObjIdTag*, const char*, int32_t);
template<> int32_t LOADVARSTRUCT<ObjIdTag>(void*, ObjIdTag*, const char*, int32_t);
template<> int32_t SAVEVAR<ResearchCat>(void*, ResearchCat*, const char*, int32_t);
template<> int32_t LOADVAR<ResearchCat>(void*, ResearchCat*, const char*, int32_t);
template<> int32_t SAVEVAR<unsigned char>(void*, unsigned char*, const char*, int32_t);
template<> int32_t LOADVAR<unsigned char>(void*, unsigned char*, const char*, int32_t);
template<> int32_t SAVEVAR<float>(void*, float*, const char*, int32_t);
template<> int32_t LOADVAR<float>(void*, float*, const char*, int32_t);

// 596 bytes. Saves (save != 0) or loads the six fields in order, stopping with 1 at the first failing step; versions 0 and 1 only
// (any other version does nothing and answers 0). The unused argument is ignored.
int32_t CResearchProject::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (save != 0) {
        if (version < 2 && version >= 0) {
            if (SAVEVARSTRUCT(file, reinterpret_cast<ObjIdTag*>(&mObjectId), "mObjId", 0) != 0) return 1;
            if (SAVEVAR(file, reinterpret_cast<ResearchCat*>(&mCategory), "mCat", 0) != 0) return 1;
            if (SAVEVAR(file, &mF08, "mResearchGroup", 0) != 0) return 1;
            if (SAVEVAR(file, &mUpgradeLevel, "mUpgradeLevel", 0) != 0) return 1;
            if (SAVEVAR(file, &mScore, "mCost", 0) != 0) return 1;
            if (SAVEVAR(file, &mProgress, "mResearchDone", 0) != 0) return 1;
        }
    } else {
        if (version < 2 && version >= 0) {
            if (LOADVARSTRUCT(file, reinterpret_cast<ObjIdTag*>(&mObjectId), "mObjId", 0) != 0) return 1;
            if (LOADVAR(file, reinterpret_cast<ResearchCat*>(&mCategory), "mCat", 0) != 0) return 1;
            if (LOADVAR(file, &mF08, "mResearchGroup", 0) != 0) return 1;
            if (LOADVAR(file, &mUpgradeLevel, "mUpgradeLevel", 0) != 0) return 1;
            if (LOADVAR(file, &mScore, "mCost", 0) != 0) return 1;
            if (LOADVAR(file, &mProgress, "mResearchDone", 0) != 0) return 1;
        }
    }
    return 0;
}

// The park analyser's three messages (main app, unit w161): the constructors 0x1015210c (CMsgAddToChallenge), 0x10151cfc (CMsgResearchCompleted) and
// 0x10151f14 (CMsgStaffInfo), the GetType functions 0x100ef1f4, 0x101244d0 and 0x1012c244, the destructors 0x100e4584, 0x1012412c and 0x1012b4f4, and
// CBankAccount::GetBalance 0x100f32b4. They live together because the native vtable needs a key function (GetType); the pattern is src/ThingMessages.cpp's.

CMsgResearchCompleted::~CMsgResearchCompleted() {}

// 436 bytes: saves (save != 0) or loads the four fields, stopping at the first callee that answers non-zero.
// Versions 0 and 1 are handled; any other version does nothing and answers 0.
int32_t CResearchCategory::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (save != 0) {
        if (version < 2 && version >= 0) {
            if (SAVEVAR(file, reinterpret_cast<MacULong*>(&mEffort), "effort", 0) != 0) return 1;
            if (SAVEVAR(file, reinterpret_cast<MacULong*>(&mOutput), "output", 0) != 0) return 1;
            if (SAVEVAR(file, &mF08, "currentProject", 0) != 0) return 1;
            if (SAVEVAR(file, &mF0C, "highestGroupAvailable", 0) != 0) return 1;
        }
    } else {
        if (version < 2 && version >= 0) {
            if (LOADVAR(file, reinterpret_cast<MacULong*>(&mEffort), "effort", 0) != 0) return 1;
            if (LOADVAR(file, reinterpret_cast<MacULong*>(&mOutput), "output", 0) != 0) return 1;
            if (LOADVAR(file, &mF08, "currentProject", 0) != 0) return 1;
            if (LOADVAR(file, &mF0C, "highestGroupAvailable", 0) != 0) return 1;
        }
    }
    return 0;
}

int32_t CMsgResearchCompleted::GetType() const { return 0x11; }

// Unit v51052: small functions of the staff HQ, tag system, thoughts, UI receiver, research lab message, Vector2d and the steering behaviours.
// The destructors are the Mac's deleting forms (the flag rides in r4): free functions here, because the host classes have no matching
// destructors. The vtable stores of the originals have no host counterpart and are not written.

alignas(16) uint8_t gResearchLabString[64];      // 0x1034c4b8
alignas(16) uint8_t gResearchLabStringNode[64];  // 0x1034c4cc

namespace {
const char16_t kResearchLabText[] = u"gms.dat";   // 0x102457e0
}

void __sinit_ResearchLab_cpp() {
    void* str = ::new (gResearchLabString) TbDynamicStringTemplate<char16_t>(kResearchLabText);
    __register_global_object(str, reinterpret_cast<void*>(&UIMain_DestroyWString), gResearchLabStringNode);
}

int32_t CMsgResearchCategoryUpdated_GetType(const CMsgResearchCategoryUpdated*) { return 0x12; }

CMsgResearchCategoryUpdated* CMsgResearchCategoryUpdated_Dtor(CMsgResearchCategoryUpdated* self, int16_t flag) {
    if (self != nullptr && flag > 0) v51052_callee::OperatorDelete(self);
    return self;
}

Follow_Path* Follow_Path_Dtor(Follow_Path* self, int16_t flag) {
    if (self != nullptr && flag > 0) v51052_callee::OperatorDelete(self);
    return self;
}

Avoid_Walls* Avoid_Walls_Dtor(Avoid_Walls* self, int16_t flag) {
    if (self != nullptr && flag > 0) v51052_callee::OperatorDelete(self);
    return self;
}

Repel_Obstacle* Repel_Obstacle_Dtor(Repel_Obstacle* self, int16_t flag) {
    if (self != nullptr && flag > 0) v51052_callee::OperatorDelete(self);
    return self;
}

Avoid_Obstacle* Avoid_Obstacle_Dtor(Avoid_Obstacle* self, int16_t flag) {
    if (self != nullptr && flag > 0) v51052_callee::OperatorDelete(self);
    return self;
}

Separation* Separation_Dtor(Separation* self, int16_t flag) {
    if (self != nullptr && flag > 0) v51052_callee::OperatorDelete(self);
    return self;
}

// The research lab's projects, efforts and outputs (main app, unit w211): 21 CResearchLab functions (0x10121b7c FinalDestroy to 0x10123a5c
// GetResearchOutputFromCategory) and 5 CResearchProject ones (0x10123d7c SetupProject to 0x1012420c GetRemainingPoints). They choose a
// category's next or previous project, share the researchers' points over the categories, raise a category's level, answer the research
// panel, run the cheats, keep the lab's active flag, and set up, advance and measure a project.
//
// The lab keeps 0x100 projects of 0x14 bytes at +0xc and five categories of 0x10 bytes at +0x140c (an effort, an output, the index of the
// current project and the highest group open). A project is done when its progress (a float) has reached its score. Every project needs a
// ride record (CRideDatabase) and every finished one makes the ride's object control available (CControlManager).

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

// SAVEVAR<ResearchCat> (0x10124680, 260 B): saves the research category, a 32-bit enum, through the shared serialise::SaveVar body.

template<> int32_t SAVEVAR<ResearchCat>(void* file, ResearchCat* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);
}

// Unit v51002: the save-file instances for CStaffHQ::StrikeType (SAVEVARSTRUCT 0x1012c3d4, LOADVARSTRUCT 0x1012c27c) and CPerson::PersonState
// (SAVEVAR 0x1011b90c), and CPerson::PayPricePerUseForObject (0x1011a4ac). LOADVAR<ResearchCat> (0x10124554) is in
// src/ResearchLab_projects_CResearchProject_Serialise.cpp.

template<> int32_t LOADVAR<ResearchCat>(void* file, ResearchCat* value, const char* name, int32_t noLog) {   // 0x10124554
    return serialise::LoadVar(file, value, name, noLog);
}
