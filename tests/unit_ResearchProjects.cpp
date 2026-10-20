// Differential library for unit w211: the research lab's projects, efforts and outputs (src/ResearchLab_projects.cpp: CResearchLab::FinalDestroy to
// GetResearchOutputFromCategory, 21 functions, and CResearchProject::SetupProject to GetRemainingPoints, 5 functions).
//
// Every callee outside those is a fake that records the same event as the PowerPC side's stub (see test_ResearchProjects.py): the ride database, both
// object control getters, the golden tickets, GetWorld, the park analyser's getter and its two researcher abilities, the advisor, its request and its
// queue, the two messages and the message centre's PostMessage, the nothrow operator new and the thing's FinalDestroy. The worlds are not blocks of their
// Mac size: only the 0x80 bytes at world + 0x1da6f0 are real memory (gTail), and a world pointer is that block's address minus 0x1da6f0 on both sides, so
// that the researcher list's head at world + 0x1da742 lies inside it; the control manager (world + 0x1d943c) and the golden tickets (world + 0x2c0) are
// labels only. The lab lives in gLab over a guard of 0x40 bytes on each side, the eight object control records in gControls and the eight ride records
// in gRide, so a stray store shows. gMainBalance is the real storage. Objects are reported as call-log labels.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "CSubscriptionHeader.h"
#include "MainBalance.h"
#include "ObjectParts.h"
#include "ParkMessages.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingParts.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "calllog.h"

using namespace calllog;

// Only a place for the storage of the Mac's ride database singleton (its symbol; the unit never constructs it).
alignas(16) unsigned char gRideDbBlock[sizeof(CRideDatabase)] __asm__("_gRideDatabase");

namespace {

using ObjectId = tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>;

constexpr int32_t kTail = 0x1da6f0;
constexpr int32_t kTailSize = 0x80;
constexpr int32_t kLabAt = 0x40;
constexpr int32_t kLabBlock = kLabAt + 0x146c + 0x40;
constexpr int32_t kRecords = 8;
constexpr int32_t kControlAt = 0x40;
constexpr int32_t kControlSize = 0x20;
constexpr int32_t kControlBlock = kControlAt + kRecords * kControlSize + 0x40;
constexpr int32_t kRideBlock = 0x1000;
constexpr int32_t kProbe = 0x40;

alignas(16) unsigned char gTail[2][kTailSize];
alignas(16) unsigned char gLab[kLabBlock];
alignas(16) unsigned char gControls[kControlBlock];
alignas(16) unsigned char gRide[kRecords][kRideBlock];
alignas(16) unsigned char gCentre[0x1d0];
alignas(16) unsigned char gAnalyser[2][kProbe];
alignas(16) unsigned char gAdvisor[kProbe];
int32_t gIds[kRecords];
void* gNewCentre;
void* gAllocated[8];
int32_t gAllocatedCount;
const void* gLastRequest;
const void* gLastMessage;

static_assert(sizeof(RideInfo) <= kRideBlock, "the ride blocks are big enough");
static_assert(sizeof(ObjectControl) == kControlSize, "the control records are 0x20 bytes");

CWorld* WorldPtr(int32_t k) { return reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gTail[k]) - kTail); }
CResearchLab* Lab() { return reinterpret_cast<CResearchLab*>(gLab + kLabAt); }
ObjectControl* Control(int32_t record) { return reinterpret_cast<ObjectControl*>(gControls + kControlAt + kControlSize * record); }
RideInfo* Ride(int64_t record) { return reinterpret_cast<RideInfo*>(gRide[record]); }

// The record the id names (each record of a case has its own id); the first one, with an event, for an id that no record has.
int32_t RecordOf(uint16_t id) {
    for (int32_t i = 0; i < kRecords; ++i) {
        if (static_cast<uint16_t>(gIds[i]) == id) return i;
    }
    ev("NoRecord", {id});
    return 0;
}

float FloatOf(int64_t bits) {
    const uint32_t word = static_cast<uint32_t>(bits);
    float value;
    std::memcpy(&value, &word, 4);
    return value;
}

}  // namespace

// ---- The unit's globals (the program's definitions are in files this library does not compile)
CWorld* gWorld;
CWorld* gGameWorld;
CMessageCentre* gMessageCentre;

void CBalanceTable::Init() {}
void CBalanceTable::UnInit() {}

// ---- The world and its getters --------------------------------------------------------------------------------------------------------
CWorld& CWorldSavvy::GetWorld() const {
    ev("GetWorld", {lab(this)});
    return *gWorld;
}
const CParkAnalyser* CWorld::GetParkAnalyser() const {
    ev("GetParkAnalyser.const", {lab(this)});
    return reinterpret_cast<const CParkAnalyser*>(gAnalyser[this == WorldPtr(0) ? 0 : 1]);
}
int32_t CParkAnalyser::MaxResearcherAbility() const {
    ev("MaxResearcherAbility", {lab(this)});
    return static_cast<int32_t>(ret("maxab"));
}
int32_t CParkAnalyser::CurrentResearcherAbility() const {
    ev("CurrentResearcherAbility", {lab(this)});
    return static_cast<int32_t>(ret("curab"));
}
CAdvisor* CWorld::GetAdvisor() {
    ev("CWorld::GetAdvisor", {lab(this)});
    return reinterpret_cast<CAdvisor*>(gAdvisor);
}

// ---- The ride database, the object controls and the golden tickets -----------------------------------------------------------------------------
RideInfo* CRideDatabase::GetRideInfo(ObjectId id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return Ride(RecordOf(id.mIndex));
}
ObjectControl* CControlManager::GetObjectControl(ObjectId type, int32_t flag) {
    ev("GetObjectControl", {lab(this), type.mIndex, flag});
    return Control(RecordOf(type.mIndex));
}
const ObjectControl* CControlManager::GetObjectControl(ObjectId type, int32_t flag) const {
    ev("GetObjectControl.const", {lab(this), type.mIndex, flag});
    return Control(RecordOf(type.mIndex));
}
bool CGoldenTicketControl::HasBeenBought(ObjectId type) const {
    ev("HasBeenBought", {lab(this), type.mIndex});
    return (ret("bought") & 0xff) != 0;
}

// ---- The advisor ---------------------------------------------------------------------------------------------------------------------------
CAdvisorRequest::CAdvisorRequest(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId, uint32_t secondary,
                                 int32_t responseNo, bool overrideOnlyOnce, int32_t score) {
    gLastRequest = this;
    ev("CAdvisorRequest", {static_cast<int32_t>(message), thingId.mIndex, secondary, responseNo, overrideOnlyOnce, score});
}
int32_t CAdvisor::QueueRequest(const CAdvisorRequest& request) {
    ev("QueueRequest", {lab(this), &request == gLastRequest ? 1 : 0});
    return 0;
}

// ---- The two messages (their destructors and GetType are silent: nobody calls GetType, the destructors run at the end of the scope) ------------
CMsgEvent::CMsgEvent(EventType event) {
    gLastMessage = this;
    ev("CMsgEvent", {static_cast<int32_t>(event)});
}
CMsgEvent::~CMsgEvent() {}
int32_t CMsgEvent::GetType() const { return 0; }
CMsgResearchCompleted::CMsgResearchCompleted(ObjectId obj, ResearchCat category) {
    gLastMessage = this;
    ev("CMsgResearchCompleted", {obj.mIndex, static_cast<int32_t>(category)});
}
CMsgResearchCompleted::~CMsgResearchCompleted() {}
int32_t CMsgResearchCompleted::GetType() const { return 0x11; }

// ---- The message centre: PostMessage, the 29 headers (the first stands for __construct_array's call) and the nothrow operator new ---------------
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    ev("PostMessage", {lab(this), &message == gLastMessage ? 1 : 0});
    return 1;
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), fail});
    if (fail) return nullptr;
    gNewCentre = std::malloc(size);
    std::memset(gNewCentre, 0xa5, size);
    gAllocated[gAllocatedCount++] = gNewCentre;
    return gNewCentre;
}
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

// ---- The thing's FinalDestroy (the lab's first member is the thing) ---------------------------------------------------------------------
void CThing::FinalDestroy() { ev("CThing::FinalDestroy", {lab(this)}); }

// ---- The test entry points -----------------------------------------------------------------------------------------------------------------
extern "C" {

// 0, 1 the world tails; 2 the lab block (the lab at +0x40); 3 the control block (the records from +0x40); 4 + k a ride block; 12 the message centre
// block; 13, 14 the analyser blocks; 15 the advisor block; 16 the ride database.
void* tpw_rpj_addr(int32_t which) {
    if (which == 0 || which == 1) return gTail[which];
    if (which >= 4 && which < 4 + kRecords) return gRide[which - 4];
    switch (which) {
    case 2: return gLab;
    case 3: return gControls;
    case 12: return gCentre;
    case 13: return gAnalyser[0];
    case 14: return gAnalyser[1];
    case 15: return gAdvisor;
    case 16: return gRideDbBlock;
    default: return nullptr;
    }
}
void* tpw_rpj_world(int32_t k) { return WorldPtr(k); }

// The globals before a case: the worlds named by index, the message centre block (0 for none); the allocations of the last case are freed.
void tpw_rpj_prepare(int32_t world, int32_t game, int32_t centre) {
    for (int32_t i = 0; i < gAllocatedCount; ++i) std::free(gAllocated[i]);
    gAllocatedCount = 0;
    gNewCentre = nullptr;
    gLastRequest = nullptr;
    gLastMessage = nullptr;
    gWorld = WorldPtr(world);
    gGameWorld = WorldPtr(game);
    gMessageCentre = centre != 0 ? reinterpret_cast<CMessageCentre*>(gCentre) : nullptr;
}
// The object id of each of the eight control and ride records of the case.
void tpw_rpj_ids(const int32_t* ids) {
    for (int32_t i = 0; i < kRecords; ++i) gIds[i] = ids[i];
}
void tpw_rpj_balance(int32_t slot, int32_t value) { gMainBalance.mValues[slot].mInteger = value; }

// The host offset of the ride record's id (slot -1) and of a value slot's integer.
int32_t tpw_rpj_ride_offset(int32_t slot) {
    RideInfo* ride = Ride(0);
    const unsigned char* member = slot < 0 ? reinterpret_cast<const unsigned char*>(&ride->mId)
                                           : reinterpret_cast<const unsigned char*>(&ride->Value(static_cast<RideBalanceSlot>(slot)).mInteger);
    return static_cast<int32_t>(member - gRide[0]);
}

// The offsets and sizes the module relies on (HOST_LAYOUT): the lab, a project, a category, a control record, the world's researcher head and the
// message centre.
int32_t tpw_rpj_layout(int64_t* out) {
    CResearchLab* lab = Lab();
    CResearchProject* project = &lab->mProjects[0];
    CResearchCategory* category = &lab->mCategories[0];
    ObjectControl* control = Control(0);
    CWorld* world = WorldPtr(0);
    auto at = [](const void* base, const void* member) {
        return static_cast<int64_t>(reinterpret_cast<const unsigned char*>(member) - reinterpret_cast<const unsigned char*>(base));
    };
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CResearchLab));
    out[n++] = at(lab, &lab->mProjects[0]);
    out[n++] = at(lab, &lab->mCategories[0]);
    out[n++] = at(lab, &lab->mProjectCount);
    out[n++] = at(lab, &lab->mF1460);
    out[n++] = at(lab, &lab->mBestProject);
    out[n++] = at(lab, &lab->mHaveMechanic);
    out[n++] = static_cast<int64_t>(sizeof(CResearchProject));
    out[n++] = at(project, &project->mObjectId);
    out[n++] = at(project, &project->mCategory);
    out[n++] = at(project, &project->mF08);
    out[n++] = at(project, &project->mUpgradeLevel);
    out[n++] = at(project, &project->mScore);
    out[n++] = at(project, &project->mProgress);
    out[n++] = static_cast<int64_t>(sizeof(CResearchCategory));
    out[n++] = at(category, &category->mEffort);
    out[n++] = at(category, &category->mOutput);
    out[n++] = at(category, &category->mF08);
    out[n++] = at(category, &category->mF0C);
    out[n++] = static_cast<int64_t>(sizeof(ObjectControl));
    out[n++] = at(control, &control->mType);
    out[n++] = at(control, &control->mUpgradeCost);
    out[n++] = at(control, &control->mAvailable);
    out[n++] = at(control, &control->mUpgradesAvailable);
    out[n++] = at(world, &world->mF1DA742);
    out[n++] = static_cast<int64_t>(sizeof(CMessageCentre));
    return n;
}

// What is left after a call: the label of the message centre global and the number of allocations.
void tpw_rpj_globals(int64_t* out) {
    out[0] = lab(gMessageCentre);
    out[1] = gAllocatedCount;
}

// which: 0 FinalDestroy, 1 ContributeResearch(a), 2 NextProject(a), 3 PreviousProject(a), 4 GetEffort(a), 5 GetTotalEffortFromActiveCategories,
// 6 SetEffort(a, b), 7 SetWorkLoad(a), 8 GetResearchSpeed(a), 9 GetCurrentProject(a), 10 GetEstimatedCostForCurrentProject(a), 11 GetRemainingPoints(a),
// 12 GetNumberLeftToResearch(a), 13 UpdateHighestGroup(a), 14 IsAllResearchDone, 15 CheatCompleteAll, 16 CheatCompleteOnlyPaidFor,
// 17 IsCategoryActive(a), 18 UpdateActiveStatus, 19 CalculateOutputs, 20 GetResearchOutputFromCategory(a); on project k: 21 SetupProject(ride a),
// 22 SetupStatUpgrade(ride a), 23 ContributeResearch(float bits a), 24 IsProjectCompleted, 25 GetRemainingPoints(float bits a).
int64_t tpw_rpj_run(int32_t which, int64_t a, int64_t b, int32_t k) {
    CResearchLab* lab = Lab();
    CResearchProject* project = &lab->mProjects[k];
    const ResearchCat category = static_cast<ResearchCat>(static_cast<int32_t>(a));
    switch (which) {
    case 0: lab->FinalDestroy(); return 0;
    case 1: lab->ContributeResearch(static_cast<uint32_t>(a)); return 0;
    case 2: lab->NextProject(category); return 0;
    case 3: lab->PreviousProject(category); return 0;
    case 4: return lab->GetEffort(static_cast<uint32_t>(a));
    case 5: return lab->GetTotalEffortFromActiveCategories();
    case 6: return lab->SetEffort(static_cast<int32_t>(a), static_cast<uint32_t>(b));
    case 7: lab->SetWorkLoad(static_cast<uint32_t>(a)); return 0;
    case 8: return lab->GetResearchSpeed(category);
    case 9: return lab->GetCurrentProject(category).mIndex;
    case 10: return lab->GetEstimatedCostForCurrentProject(category);
    case 11: return lab->GetRemainingPoints(category);
    case 12: return lab->GetNumberLeftToResearch(category);
    case 13: lab->UpdateHighestGroup(static_cast<int32_t>(a)); return 0;
    case 14: return lab->IsAllResearchDone() ? 1 : 0;
    case 15: lab->CheatCompleteAll(); return 0;
    case 16: lab->CheatCompleteOnlyPaidFor(); return 0;
    case 17: return lab->IsCategoryActive(category) ? 1 : 0;
    case 18: lab->UpdateActiveStatus(); return 0;
    case 19: lab->CalculateOutputs(); return 0;
    case 20: return lab->GetResearchOutputFromCategory(category);
    case 21: return static_cast<uint8_t>(project->SetupProject(Ride(a)));
    case 22: return static_cast<uint8_t>(project->SetupStatUpgrade(Ride(a)));
    case 23: project->ContributeResearch(FloatOf(a)); return 0;
    case 24: return project->IsProjectCompleted() ? 1 : 0;
    default: return project->GetRemainingPoints(FloatOf(a));
    }
}

}  // extern "C"
