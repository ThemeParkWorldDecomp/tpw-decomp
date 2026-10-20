// Request queue test library. Scoring construction and all ten queries are recording fakes.
#include <cstring>
#include "AdvisorQueue.h"
#include "TPSettings.h"
#include "calllog.h"

using namespace calllog;

CAdvisor::CAdvisor() {}   // declared for CWorld::SetupNewLevel (unit w117), which this module does not link
namespace {
CAdvisor gObject;
CAdvisorRequest gRequest;
// The world is raw storage: CWorld has a constructor (src/World_init.cpp, unit w113) that this module does not link.
alignas(CWorld) unsigned char gTestWorldBytes[sizeof(CWorld)];
CWorld& gTestWorld = *reinterpret_cast<CWorld*>(gTestWorldBytes);
CAdvisorScoring* gScoring;
bool gConstructed;
}

// The untranslated world accessor returns the world's turn input.
CWorld* gWorld = &gTestWorld;
CWorld& CWorldSavvy::GetWorld() const {
    ev("GetWorld", {lab(this)});
    return *gWorld;
}

// Settings construction is outside the unit and has no event.
CPlayerSettings gPlayerSettings;
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}

// The real constructor loads advisor.sam. This fake only records first use.
CAdvisorScoring::CAdvisorScoring() {
    gScoring = this;
    gConstructed = true;
    tpw_cl_label(3, this);
    ev("Scoring.Ctor", {lab(this)});
}
CAdvisorScoring::~CAdvisorScoring() {}
const BalanceDescription* CAdvisorBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CAdvisorBalance::GetBalanceValuesStart() { return mValues; }

// Every scoring argument is recorded; replies come from per-case queues.
int32_t CAdvisorScoring::GetScoreFor(AdvisorMessage message) {
    ev("score", {lab(this), message});
    return static_cast<int32_t>(ret("score"));
}
int32_t CAdvisorScoring::GetNumResponsesFor(AdvisorMessage message) const {
    ev("num", {lab(this), message});
    return static_cast<int32_t>(ret("num"));
}
uint32_t CAdvisorScoring::GetMaxDuplicatesInQueueFor(AdvisorMessage message) const {
    ev("maximum", {lab(this), message});
    return static_cast<uint32_t>(ret("maximum"));
}
int32_t CAdvisorScoring::GetMinTimeFor(AdvisorMessage message) const {
    ev("time", {lab(this), message});
    return static_cast<int32_t>(ret("time"));
}
bool CAdvisorScoring::SayOnlyOnce(AdvisorMessage message) const {
    ev("once", {lab(this), message});
    return ret("once") != 0;
}
int32_t CAdvisorScoring::GetCyclingFor(AdvisorMessage message) const {
    ev("cycling", {lab(this), message});
    return static_cast<int32_t>(ret("cycling"));
}
int32_t CAdvisorScoring::GetMessageCategory(AdvisorMessage message) const {
    ev("category", {lab(this), message});
    return static_cast<int32_t>(ret("category"));
}
bool CAdvisorScoring::HasThisBeenKilledTooManyTimes(AdvisorMessage message, int32_t killed) const {
    ev("killed", {lab(this), message, killed});
    return ret("killed") != 0;
}
bool CAdvisorScoring::IsThisValidInTheCurrentMode(AdvisorMessage message) const {
    ev("mode", {lab(this), message});
    return ret("mode") != 0;
}
bool CAdvisorScoring::IsThisATutorialMessage(AdvisorMessage message) const {
    ev("tutorial", {lab(this), message});
    return ret("tutorial") != 0;
}

extern "C" {
// Inputs use native byte order; Python converts every typed field independently.
void tpw_aq_reset(const uint8_t* object, const uint8_t* request, uint32_t turn, int32_t tutorial, int32_t message) {
    std::memcpy(static_cast<void*>(&gObject), object, sizeof gObject);
    std::memcpy(static_cast<void*>(&gRequest), request, sizeof gRequest);
    gTestWorld.mTurn = turn;
    gPlayerSettings.mCurrent.mTutorialOn = tutorial != 0;
    tpw_cl_label(2, &gObject);
    tpw_cl_label(4, &gObject.mMessageState[message].mLastSaid);
    if (gScoring) tpw_cl_label(3, gScoring);
}

int32_t tpw_aq_call(int32_t function, int32_t message, int32_t force) {
    switch (function) {
    case 0: return gObject.QueueRequest(gRequest);
    case 1: return gObject.CanIQueueThis(static_cast<AdvisorMessage>(message), force != 0);
    case 2: return gObject.FindFirstInvalidSlotInQueue();
    case 3: return gObject.FindLowestScoringSlotInQueue();
    }
    return -999;
}

// Includes every object byte, the unchanged input request, settings and world turn.
void tpw_aq_dump(uint8_t* object, uint8_t* request, uint32_t* globals) {
    std::memcpy(object, &gObject, sizeof gObject);
    std::memcpy(request, &gRequest, sizeof gRequest);
    globals[0] = gTestWorld.mTurn;
    globals[1] = gPlayerSettings.mCurrent.mTutorialOn;
    globals[2] = gConstructed;
}
}
