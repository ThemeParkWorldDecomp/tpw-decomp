// host_new_rides_research.cpp: the research projects a park gets on loading for the rides it did not have when it began (the
// port's fix newrides, tpw::AddResearchForNewRides in src/game/save/SaveLoad.cpp; contributed by samueleagle91, tpw-port PR
// #129). A park's projects are set up once, when it starts; a ride switched on later (the Gold Edition's, or a mod's) had
// none, so it could never be researched. On a made-up lab that began with two rides, in a database that now has six:
//   - the two rides it began with keep their projects, one of them finished, and get no second one;
//   - a new ride whose research costs something gets its project (its category, group and cost), the best project follows
//     the higher cost, and a category with no current project takes it (NextProject);
//   - a new ride whose research costs nothing is made available at once and gets its first upgrade level that costs, as a
//     statistics upgrade;
//   - a ride that failed to load and a ride of user interface type 4 get nothing;
//   - a second and a third load change nothing at all (the lab and the controls, byte for byte): the pass is idempotent;
//   - in one of the TPI Worlds mod's worlds the pass does nothing (its research is RepairTpiResearch's);
//   - a lab whose 0x100 project slots are full gets nothing more, and nothing is written past them.
// The program runs the real AddResearchForNewRides, CResearchProject::SetupProject, SetupStatUpgrade and IsProjectCompleted,
// CResearchLab::NextProject and the ride database's walk and GetRideInfo; the world, its research lab and its object
// controls are the test's stand-ins. Made only with the port's fixes; exits nonzero on a failure.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "ObjectControl.h"
#include "ObjectParts.h"
#include "RideDB.h"
#include "SaveLoad.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "TpiGateFlow.h"
#include "TpiWorlds.h"
#include "World.h"

// ---- globals of other files, defined here (zero at start)
CRideDatabase gRideDatabase;
CWorld* gGameWorld = nullptr;
CWorld* gWorld = nullptr;

namespace {

int gFailures = 0;
int gChecks = 0;

void Check(bool ok, const char* what, long value = -1) {
    ++gChecks;
    if (ok) return;
    ++gFailures;
    std::printf("FAIL %s (%ld)\n", what, value);
}

constexpr uint16_t kOldCoaster = 1201;   // the park began with these two
constexpr uint16_t kOldSwing = 1202;
constexpr uint16_t kGoldRide = 1100;     // switched on later: research costs 900, category 1, group 2
constexpr uint16_t kFreeRide = 1101;     // switched on later: research costs nothing, its upgrade level 1 costs 300
constexpr uint16_t kFailed = 1102;       // failed to load
constexpr uint16_t kUiFour = 1103;       // user interface type 4
constexpr int kRides = 6;

alignas(RideInfo) unsigned char gRecords[kRides][sizeof(RideInfo)];
alignas(CResearchLab) unsigned char gLabBlock[sizeof(CResearchLab) + 0x40];   // the lab, then a guard
alignas(ObjectControl) unsigned char gControlBlock[kRides][sizeof(ObjectControl)];
alignas(16) unsigned char gWorldBlock[64];   // the world's pointer only: the lab and the controls are the stand-ins'

CResearchLab* Lab() { return reinterpret_cast<CResearchLab*>(gLabBlock); }

RideInfo* Record(int i, uint16_t id, int32_t cost, int32_t category, int32_t group) {
    RideInfo* ride = reinterpret_cast<RideInfo*>(gRecords[i]);
    std::memset(gRecords[i], 0, sizeof(RideInfo));
    ride->mId = id;
    ride->Value(kRideSlot_Upgrades_CostOfResearch).mInteger = cost;
    ride->Value(kRideSlot_Category).mInteger = category;
    ride->Value(kRideSlot_Group).mInteger = group;
    ride->Value(kRideSlot_UpgradesId).mInteger = id;   // NextProject's open test of a statistics upgrade reads the type's own record
    return ride;
}

ObjectControl* ControlOf(uint16_t id) {
    for (int i = 0; i < kRides; ++i) {
        if (reinterpret_cast<RideInfo*>(gRecords[i])->mId == id) return reinterpret_cast<ObjectControl*>(gControlBlock[i]);
    }
    return nullptr;
}

const CResearchProject* ProjectOf(uint16_t id, int* count) {
    const CResearchProject* found = nullptr;
    *count = 0;
    for (int32_t i = 0; i < Lab()->mProjectCount; ++i) {
        if (Lab()->mProjects[i].mObjectId != id) continue;
        if (found == nullptr) found = &Lab()->mProjects[i];
        ++*count;
    }
    return found;
}

// The lab as a park that began with the two old rides has it: their projects (the coaster's finished), the swing current
// in category 0, the best project the coaster's.
void BeginPark() {
    std::memset(gLabBlock, 0xa5, sizeof gLabBlock);
    std::memset(gLabBlock, 0, sizeof(CResearchLab));
    std::memset(gControlBlock, 0, sizeof gControlBlock);
    CResearchLab* lab = Lab();
    for (CResearchCategory& category : lab->mCategories) {
        category.mF08 = -1;
        category.mF0C = 9;
    }
    CResearchProject& coaster = lab->mProjects[0];
    coaster.mObjectId = kOldCoaster;
    coaster.mCategory = 0;
    coaster.mScore = 500.0f;
    coaster.mProgress = 500.0f;   // finished
    CResearchProject& swing = lab->mProjects[1];
    swing.mObjectId = kOldSwing;
    swing.mCategory = 0;
    swing.mScore = 200.0f;
    lab->mProjectCount = 2;
    lab->mBestProject = 0;
    lab->mCategories[0].mF08 = 1;
}

void Database() {
    gRideDatabase.mRides[0] = Record(0, kOldCoaster, 500, 0, 1);
    gRideDatabase.mRides[1] = Record(1, kOldSwing, 200, 0, 1);
    gRideDatabase.mRides[2] = Record(2, kGoldRide, 900, 1, 2);
    RideInfo* free = Record(3, kFreeRide, 0, 2, 1);
    free->Value(static_cast<RideBalanceSlot>(kRideSlot_Upgrades_CostOfResearch + 16)).mInteger = 300;
    gRideDatabase.mRides[3] = free;
    gRideDatabase.mRides[4] = Record(4, kFailed, 700, 1, 1);
    gRideDatabase.mRides[4]->mLoadFailed = 1;
    gRideDatabase.mRides[5] = Record(5, kUiFour, 700, 1, 1);
    gRideDatabase.mRides[5]->mUIType = 4;
    gRideDatabase.mRideCount = kRides;
    gRideDatabase.mCachedId = 0;
    gRideDatabase.mCachedRide = nullptr;
}

bool GuardIntact() {
    for (size_t i = sizeof(CResearchLab); i < sizeof gLabBlock; ++i) {
        if (gLabBlock[i] != 0xa5) return false;
    }
    return true;
}

}  // namespace

// ---- stand-ins for the callees outside the test
CResearchLab* CWorld::GetResearchLab() { return Lab(); }
ObjectControl* CControlManager::GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t) {
    return ControlOf(type.mIndex);
}
bool CGoldenTicketControl::HasBeenBought(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>) const { return false; }

int main() {
    gGameWorld = reinterpret_cast<CWorld*>(gWorldBlock);
    gWorld = gGameWorld;
    Database();
    BeginPark();

    {   // the first load with the new rides switched on
        tpw::AddResearchForNewRides();
        const CResearchLab* lab = Lab();
        int count = 0;
        Check(lab->mProjectCount == 4, "first load: two projects added", lab->mProjectCount);
        ProjectOf(kOldCoaster, &count);
        Check(count == 1, "first load: the finished old project not set up again", count);
        ProjectOf(kOldSwing, &count);
        Check(count == 1, "first load: the open old project not set up again", count);
        const CResearchProject* gold = ProjectOf(kGoldRide, &count);
        Check(count == 1 && gold != nullptr, "first load: the new ride's project", count);
        if (gold != nullptr) {
            Check(gold->mCategory == 1 && gold->mF08 == 2 && gold->mScore == 900.0f && gold->mProgress == 0.0f && gold->mUpgradeLevel == 0,
                  "first load: the new ride's category, group and cost");
            Check(lab->mBestProject == gold - lab->mProjects, "first load: the best project follows the higher cost", lab->mBestProject);
            Check(lab->mCategories[1].mF08 == gold - lab->mProjects, "first load: its empty category takes it", lab->mCategories[1].mF08);
        }
        const CResearchProject* free = ProjectOf(kFreeRide, &count);
        Check(count == 1 && free != nullptr, "first load: the free ride's upgrade project", count);
        if (free != nullptr) {
            Check(free->mCategory == 4 && free->mUpgradeLevel == 1 && free->mScore == 300.0f, "first load: a statistics upgrade, level 1, cost 300");
            Check(lab->mCategories[4].mF08 == free - lab->mProjects, "first load: the upgrades category takes it", lab->mCategories[4].mF08);
        }
        Check(ControlOf(kFreeRide)->mAvailable == 1, "first load: the free ride made available at once");
        Check(ControlOf(kGoldRide)->mAvailable == 0, "first load: the costed ride not available yet");
        ProjectOf(kFailed, &count);
        Check(count == 0, "first load: no project for a ride that failed to load", count);
        ProjectOf(kUiFour, &count);
        Check(count == 0, "first load: no project for a ride of user interface type 4", count);
        Check(lab->mCategories[0].mF08 == 1, "first load: a category with a current project keeps it", lab->mCategories[0].mF08);
        Check(GuardIntact(), "first load: nothing written past the lab");
        std::printf("%-52s %d projects\n", "first load", lab->mProjectCount);
    }
    {   // a second and a third load: nothing changes
        unsigned char lab[sizeof(CResearchLab)];
        unsigned char controls[sizeof gControlBlock];
        std::memcpy(lab, gLabBlock, sizeof lab);
        std::memcpy(controls, gControlBlock, sizeof controls);
        tpw::AddResearchForNewRides();
        Check(std::memcmp(lab, gLabBlock, sizeof lab) == 0, "second load: the lab unchanged");
        Check(std::memcmp(controls, gControlBlock, sizeof controls) == 0, "second load: the controls unchanged");
        tpw::AddResearchForNewRides();
        Check(std::memcmp(lab, gLabBlock, sizeof lab) == 0, "third load: the lab unchanged");
        Check(std::memcmp(controls, gControlBlock, sizeof controls) == 0, "third load: the controls unchanged");
        std::printf("%-52s %d projects\n", "second and third loads", Lab()->mProjectCount);
    }
    {   // a new ride's project finished, then a load: it is not set up again
        int count = 0;
        CResearchProject* gold = const_cast<CResearchProject*>(ProjectOf(kGoldRide, &count));
        if (gold != nullptr) gold->mProgress = gold->mScore;
        const int32_t before = Lab()->mProjectCount;
        tpw::AddResearchForNewRides();
        ProjectOf(kGoldRide, &count);
        Check(Lab()->mProjectCount == before && count == 1, "a finished new project: not set up again", Lab()->mProjectCount);
        std::printf("%-52s %d projects\n", "a finished new project", Lab()->mProjectCount);
    }
    {   // in one of the TPI Worlds mod's worlds: nothing
        BeginPark();
        gModTpiWorlds = true;
        tpw::gTpiThemeIsMod = true;
        tpw::AddResearchForNewRides();
        Check(Lab()->mProjectCount == 2, "a TPI world: no project added", Lab()->mProjectCount);
        Check(ControlOf(kFreeRide)->mAvailable == 0, "a TPI world: no control changed");
        gModTpiWorlds = false;
        tpw::gTpiThemeIsMod = false;
        std::printf("%-52s %d projects\n", "a TPI world", Lab()->mProjectCount);
    }
    {   // a full lab: nothing more, nothing past the slots
        BeginPark();
        CResearchLab* lab = Lab();
        const int32_t slots = static_cast<int32_t>(sizeof lab->mProjects / sizeof lab->mProjects[0]);
        for (int32_t i = 2; i < slots; ++i) {
            lab->mProjects[i].mObjectId = static_cast<uint16_t>(2000 + i);
            lab->mProjects[i].mCategory = 3;
            lab->mProjects[i].mScore = 1.0f;
        }
        lab->mProjectCount = slots;
        tpw::AddResearchForNewRides();
        int count = 0;
        ProjectOf(kGoldRide, &count);
        Check(lab->mProjectCount == slots && count == 0, "a full lab: no project added", lab->mProjectCount);
        Check(GuardIntact(), "a full lab: nothing written past the lab");
        std::printf("%-52s %d projects\n", "a full lab", lab->mProjectCount);
    }

    std::printf("%d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
