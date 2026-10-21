// Test library for the advisor scorers of unit v5971 (libtpw_unit_CAdvisorScoring__ADVISOR_STAFF_HIRE_RESEARCHERS_.dylib,
// tests/test_CAdvisorScoring__ADVISOR_STAFF_HIRE_RESEARCHERS_.py), main app:
//   src/game/advisor/AdvisorScoring.cpp
//
// The callees outside the unit (the world's getters, the park analyser's queries, the park time, the advisor's CanIQueueThis, the
// game type's constructor, CTRACE and the scorer every record points at) are recording fakes answering from the test's queues.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "Debug.h"
#include "FS_Online.h"
#include "ParkTime.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files, defined here
AdvisorMessageRecord gAdvisorMessageBlock[kAdvisorMessageCount + 2];
CWorld* gGameWorld = nullptr;
CWorld*& gGameWorldRef = gGameWorld;

// The guard byte of TheGameType()'s function-local static (the test sets it to try both first-use paths).
extern uint64_t gTheGameTypeGuard asm("__ZGVZ11TheGameTypevE9sGameType");

namespace {
CParkAnalyser* sAnalyser = nullptr;
CStaffHQ* sHq = nullptr;
CAdvisor* sAdvisor = nullptr;
}  // namespace

// ---- the fakes (the same event names and arguments as the module's PowerPC stubs)
int32_t CWorld::IsParkClosed() const {
    ev("IsParkClosed", {lab(this)});
    return static_cast<int32_t>(ret("IsParkClosed"));
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return sAnalyser;
}
CStaffHQ* CWorld::GetStaffHQ() {
    ev("GetStaffHQ", {lab(this)});
    return sHq;
}
CAdvisor* CWorld::GetAdvisor() {
    ev("GetAdvisor", {lab(this)});
    return sAdvisor;
}

int32_t CParkAnalyser::MaxResearcherAbility() const {
    ev("MaxAbility", {lab(this)});
    return static_cast<int32_t>(ret("MaxAbility"));
}
int32_t CParkAnalyser::CurrentNumberOfStaffTypeForHire(StaffType type) const {
    ev("ForHire", {lab(this), static_cast<int32_t>(type)});
    return static_cast<int32_t>(ret("ForHire"));
}
int32_t CParkAnalyser::CurrentResearcherAbility() const {
    ev("ResearcherAbility", {lab(this)});
    return static_cast<int32_t>(ret("ResearcherAbility"));
}
int32_t CParkAnalyser::NumberOfStaffType(CThing::ThingModel model) const {
    ev("NumStaff", {lab(this), static_cast<int32_t>(model)});
    return static_cast<int32_t>(ret("NumStaff"));
}
int32_t CParkAnalyser::NumberOfStaffTypeWithPatrolArea(CThing::ThingModel model) const {
    ev("PatrolArea", {lab(this), static_cast<int32_t>(model)});
    return static_cast<int32_t>(ret("PatrolArea"));
}
int32_t CParkAnalyser::PercentageOfAreaPatrolledBy(CThing::ThingModel model, AreaType area) {
    ev("Percent", {lab(this), static_cast<int32_t>(model), static_cast<int32_t>(area)});
    return static_cast<int32_t>(ret("Percent"));
}
bool CParkAnalyser::GatesOpen() const {
    ev("GatesOpen", {lab(this)});
    return ret("GatesOpen") != 0;
}
int32_t CParkAnalyser::NumberOfObjectTypeInPark(UIObjType type, int32_t level) const {
    ev("ObjInPark", {lab(this), static_cast<int32_t>(type), level});
    return static_cast<int32_t>(ret("ObjInPark"));
}
int32_t CParkAnalyser::NumberOfVisitorsOutsidePark() const {
    ev("Outside", {lab(this)});
    return static_cast<int32_t>(ret("Outside"));
}
int32_t CParkAnalyser::TypesOfRidesLeftToResearch() const {
    ev("LeftToResearch", {lab(this)});
    return static_cast<int32_t>(ret("LeftToResearch"));
}
int32_t CParkAnalyser::TypesOfRidesAvailable(bool golden) const {
    ev("Available", {lab(this), golden ? 1 : 0});
    return static_cast<int32_t>(ret("Available"));
}
int32_t CParkAnalyser::TypesOfParticularObjectTypeInPark(UIObjType type, bool golden) const {
    ev("ParticularObj", {lab(this), static_cast<int32_t>(type), golden ? 1 : 0});
    return static_cast<int32_t>(ret("ParticularObj"));
}
int32_t CParkAnalyser::AverageQualityOfStaffType(CThing::ThingModel model) const {
    ev("AvgQuality", {lab(this), static_cast<int32_t>(model)});
    return static_cast<int32_t>(ret("AvgQuality"));
}

TbTimeDiff CParkTime::GetFunnyTimeSinceSessionStart() const {
    ev("FunnyTime", {static_cast<int64_t>(reinterpret_cast<const char*>(this) - reinterpret_cast<const char*>(gGameWorld))});
    uint64_t hi = static_cast<uint32_t>(ret("TickHi"));
    uint64_t lo = static_cast<uint32_t>(ret("TickLo"));
    TbTimeDiff t;
    t.mTicks = static_cast<int64_t>((hi << 32) | lo);
    return t;
}

bool CAdvisor::CanIQueueThis(AdvisorMessage message, bool force) const {
    ev("CanQueue", {lab(this), static_cast<int32_t>(message), force ? 1 : 0});
    return ret("CanQueue") != 0;
}

CGameType::CGameType() {
    ev("CGameType", {0});
    mType = static_cast<GameType>(static_cast<int32_t>(ret("CGameType")));
}

void CTRACE(uint32_t level, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int v = va_arg(ap, int);
    va_end(ap);
    ev("CTRACE", {static_cast<int64_t>(level), v}, {format});
}

// The scorer every table record points at: the event carries the cursor (the PowerPC stub computes the record's index).
int32_t CAdvisorScoring::_ADVISOR_NO_SCORE() {
    ev("Scorer", {mCursor});
    return static_cast<int32_t>(ret("Scorer"));
}

extern "C" {

// A zeroed block, 32-bit addressable.
uint64_t tpw_fx_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_fx_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CAdvisorScoring, mCursor));
    out[n++] = static_cast<int64_t>(sizeof(CGameType));
    return n;
}

// The world, its analyser, staff headquarters and advisor (the getters answer these); gGameWorld points at the world.
uint64_t tpw_fx_world(int32_t trainingWord) {
    // Allocated once and cleared for every case (the arena is not freed).
    static CWorld* world = static_cast<CWorld*>(tpw::ArenaAllocZeroed(sizeof(CWorld)));
    static void* analyser = tpw::ArenaAllocZeroed(256);
    static void* hq = tpw::ArenaAllocZeroed(sizeof(CStaffHQ));
    static void* advisor = tpw::ArenaAllocZeroed(256);
    std::memset(static_cast<void*>(world), 0, sizeof(CWorld));
    std::memset(analyser, 0, 256);
    std::memset(hq, 0, sizeof(CStaffHQ));
    std::memset(advisor, 0, 256);
    sAnalyser = static_cast<CParkAnalyser*>(analyser);
    sHq = static_cast<CStaffHQ*>(hq);
    sAdvisor = static_cast<CAdvisor*>(advisor);
    sHq->mTraining.mF00[4] = static_cast<uint32_t>(trainingWord);
    gGameWorld = world;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(world));
}
uint64_t tpw_fx_obj(int32_t which) {
    const void* p = which == 1 ? static_cast<const void*>(sAnalyser) : which == 2 ? static_cast<const void*>(sHq) : static_cast<const void*>(sAdvisor);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
}

uint64_t tpw_fx_newscoring() {
    static void* block = tpw::ArenaAllocZeroed(sizeof(CAdvisorScoring));
    std::memset(block, 0, sizeof(CAdvisorScoring));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
}
void tpw_fx_setslot(uint64_t scoring, int32_t macOffset, int32_t value) {
    reinterpret_cast<CAdvisorScoring*>(scoring)->mBalance.mValues[(macOffset - 0x14) / 4].mInteger = value;
}
void tpw_fx_setcursor(uint64_t scoring, int32_t value) { reinterpret_cast<CAdvisorScoring*>(scoring)->mCursor = value; }
int32_t tpw_fx_getcursor(uint64_t scoring) { return reinterpret_cast<CAdvisorScoring*>(scoring)->mCursor; }

// The table: three words per record (mSkipped, mMessage, mMode); every record's scorer is the fake above.
void tpw_fx_settable(const int32_t* d) {
    for (int32_t i = 0; i < kAdvisorMessageCount; ++i) {
        AdvisorMessageRecord& r = gAdvisorMessageBlock[i + 1];
        std::memset(&r, 0, sizeof r);
        r.mSkipped = d[3 * i];
        r.mMessage = d[3 * i + 1];
        r.mMode = d[3 * i + 2];
        r.mScorer = &CAdvisorScoring::_ADVISOR_NO_SCORE;
    }
}

// The game type object and its guard.
void tpw_fx_setgame(int32_t type, int32_t guard) {
    gTheGameTypeGuard = 1;
    TheGameType().mType = static_cast<GameType>(type);
    gTheGameTypeGuard = guard ? 1 : 0;
}
void tpw_fx_getgame(int64_t* out) {
    uint64_t g = gTheGameTypeGuard;
    gTheGameTypeGuard = 1;
    out[0] = static_cast<int32_t>(TheGameType().mType);
    out[1] = static_cast<int64_t>(g & 0xff);
    gTheGameTypeGuard = g;
}

uint32_t tpw_fx_call(int32_t which, uint64_t scoring, int32_t count) {
    CAdvisorScoring* s = reinterpret_cast<CAdvisorScoring*>(scoring);
    switch (which) {
        case 0: return static_cast<uint32_t>(s->_ADVISOR_STAFF_HIRE_RESEARCHERS_3());
        case 1: return static_cast<uint32_t>(s->_ADVISOR_STAFF_HIRE_RESEARCHERS_1());
        case 2: return static_cast<uint32_t>(s->_ADVISOR_STAFF_MECHANIC_COVERAGE());
        case 3: return static_cast<uint32_t>(s->_ADVISOR_STAFF_HANDYMAN_COVERAGE());
        case 4: return static_cast<uint32_t>(s->_ADVISOR_CLOSE_PARK());
        case 5: return static_cast<uint32_t>(s->_ADVISOR_STAFF_TRAIN_RESEARCHERS());
        case 6: return static_cast<uint32_t>(s->_ADVISOR_OPEN_PARK());
        default: return static_cast<uint32_t>(s->GetHighestScoringFromNext(count));
    }
}

}  // extern "C"
