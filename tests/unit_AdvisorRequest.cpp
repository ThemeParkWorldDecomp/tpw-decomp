// Test library for src/game/advisor/Advisor.cpp and src/game/World.cpp (unit w103). The queue's own functions (src/game/advisor/Advisor.cpp,
// unit w100), Valid and the request constructors run for real. Scoring construction and queries, the clock's ReadFixed, ADV_Slap and
// GAME_ASSERTMSG are recording fakes at their real addresses.
#include <cstring>
#include <new>

#include "Advisor.h"
#include "AdvisorQueue.h"
#include "Clock.h"
#include "Debug.h"
#include "TPSettings.h"
#include "World.h"
#include "calllog.h"

using namespace calllog;

CAdvisor::CAdvisor() {}   // declared for CWorld::SetupNewLevel (unit w117), which this module does not link
namespace {
CAdvisor gObject;                 // label 2
CAdvisorRequest gRequest;         // label 6, the request under construction or test
CAdvisorRequest gOther;           // label 7, the copy constructor's source
// The world is raw storage: CWorld has a constructor (src/game/World.cpp, unit w113) that this module does not link.
alignas(CWorld) unsigned char gTestWorldBytes[sizeof(CWorld)];
CWorld& gTestWorld = *reinterpret_cast<CWorld*>(gTestWorldBytes);   // label 12
CAdvisorScoring* gScoring;        // label 3
bool gConstructed;
int32_t gTutorialScore;           // what the balance table's ContextHelpScore reads
char gThings[4][16];              // labels 8 to 11: stand-ins for the things the table points at
constexpr int32_t kContextHelpScoreSlot = 198;   // CAdvisorScoring + 0x32c on the Mac
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

// The thing table GetAdvisor reads (declared in World.h).
CThingArrayEntry gThingArray[kThingArraySize];

// The clock: the reading comes from the case's queue (the clock classes are tested in test_GameClock).
CVariableTime::CVariableTime() {}   // the clock's members are built but never read here
CFixedTime::CFixedTime() {}
CClock gClock;   // label 5
int32_t CClock::ReadFixed() {
    ev("ReadFixed", {lab(this)});
    return static_cast<int32_t>(ret("ReadFixed"));
}

// The advisor's speech: only the call is recorded.
void ADV_Slap() { ev("ADV_Slap"); }

// The assertion records its condition byte and text (the real one does nothing).
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition & 0xff}, {format});
}

// The real constructor loads advisor.sam. This fake records first use and plants the tutorial score the balance table would hold.
CAdvisorScoring::CAdvisorScoring() {
    gScoring = this;
    gConstructed = true;
    mBalance.mValues[kContextHelpScoreSlot].mInteger = gTutorialScore;
    tpw_cl_label(3, this);
    ev("Scoring.Ctor", {lab(this)});
}
CAdvisorScoring::~CAdvisorScoring() {}
const BalanceDescription* CAdvisorBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CAdvisorBalance::GetBalanceValuesStart() { return mValues; }

// GetScoreFor also records the request being built: the constructor must have stored the first fields and must not have made it valid yet.
int32_t CAdvisorScoring::GetScoreFor(AdvisorMessage message) {
    ev("score", {lab(this), message, gRequest.mValid, gRequest.mThingId, gRequest.mSecondary, gRequest.mMessage,
                 gRequest.mScoreOverridden});
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
// Inputs are in native byte order; Python converts every typed field independently. The two requests keep their padding bytes.
void tpw_ar_reset(const uint8_t* object, const uint8_t* request, const uint8_t* other, uint32_t turn, int32_t tutorial,
                  int32_t message, int32_t tutorialScore, int32_t advisorThing) {
    std::memcpy(static_cast<void*>(&gObject), object, sizeof gObject);
    std::memcpy(static_cast<void*>(&gRequest), request, sizeof gRequest);
    std::memcpy(static_cast<void*>(&gOther), other, sizeof gOther);
    gTestWorld.mTurn = turn;
    gTestWorld.mAdvisorThing = static_cast<uint16_t>(advisorThing);
    gPlayerSettings.mCurrent.mTutorialOn = tutorial != 0;
    gTutorialScore = tutorialScore;
    tpw_cl_label(2, &gObject);
    tpw_cl_label(4, &gObject.mMessageState[message].mLastSaid);
    tpw_cl_label(5, &gClock);
    tpw_cl_label(6, &gRequest);
    tpw_cl_label(7, &gOther);
    tpw_cl_label(12, &gTestWorld);
    for (int32_t i = 0; i < 4; ++i) tpw_cl_label(8 + i, gThings[i]);
    if (gScoring) {
        tpw_cl_label(3, gScoring);
        gScoring->mBalance.mValues[kContextHelpScoreSlot].mInteger = tutorialScore;
    }
}

// One row of the thing table: a stand-in thing (kind 1 to 4 = labels 8 to 11, 0 = NULL) and the four other words.
void tpw_ar_row(int32_t index, int32_t kind, uint32_t word1) {
    gThingArray[index].mThing = kind == 0 ? nullptr : gThings[kind - 1];
    gThingArray[index].mF04[0] = word1;
}
void tpw_ar_clear_rows() { std::memset(static_cast<void*>(gThingArray), 0, sizeof gThingArray); }

// The function number selects the call; the arguments mean what the Python side says for that function.
int32_t tpw_ar_call(int32_t function, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f) {
    switch (function) {
    case 0: return gObject.CancelMessage(static_cast<AdvisorMessage>(a));
    case 1: return gObject.PlayLastTutorialMessage();
    case 2: return gObject.GetTypePlaying();
    case 3: {
        tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing;
        thing.mIndex = static_cast<uint16_t>(b);
        new (&gRequest) CAdvisorRequest(static_cast<AdvisorMessage>(a), thing, static_cast<uint32_t>(c), d, e != 0, f);
        return 0;
    }
    case 4: new (&gRequest) CAdvisorRequest(); return 0;
    case 5: new (&gRequest) CAdvisorRequest(gOther); return 0;
    case 6: gRequest.SetScore(a); return 0;
    case 7: return gRequest.Valid();
    case 8: return lab(gTestWorld.GetAdvisor());
    }
    return -999;
}

// Includes every object byte, both requests, settings and world turn.
void tpw_ar_dump(uint8_t* object, uint8_t* request, uint8_t* other, uint32_t* globals) {
    std::memcpy(object, static_cast<void*>(&gObject), sizeof gObject);
    std::memcpy(request, static_cast<void*>(&gRequest), sizeof gRequest);
    std::memcpy(other, static_cast<void*>(&gOther), sizeof gOther);
    globals[0] = gTestWorld.mTurn;
    globals[1] = gPlayerSettings.mCurrent.mTutorialOn;
    globals[2] = gConstructed;
    globals[3] = gTestWorld.mAdvisorThing;
}
}
