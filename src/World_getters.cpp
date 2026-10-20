// CWorld's thing getters, main app 0x1013eb38..0x1013ee88 (World.cpp), unit w123b. The same code as GetAdvisor (src/World_GetAdvisor.cpp, unit w103).
#include <cstddef>

#include "Debug.h"
#include "World.h"

static_assert(offsetof(CWorld, mParkAnalyserThing) == 0x1da720, "the analyser's row index sits at +0x1da720");
static_assert(offsetof(CWorld, mResearchLabThing) == 0x1da722, "the research lab's row index sits at +0x1da722");
static_assert(offsetof(CWorld, mWeatherThing) == 0x1da724, "the weather's row index sits at +0x1da724");
static_assert(offsetof(CWorld, mBankAccountThing) == 0x1da726, "the bank account's row index sits at +0x1da726");

namespace {
// The text of the assertion at World.cpp string base + 0x151 (0x102475e9).
constexpr const char* kRowZeroText = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

// The seven getters in one body: the halfword id is read BEFORE the assertion (the saved copy is used even if the assertion changes the world), the
// assertion is made on row 0 being free, and word 0 of the saved id's row is read AFTER it (so a change the assertion made to the row shows). The id is
// not checked against the table's size and a failed assertion does not change the answer.
void* RowThing(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
    return gThingArray[id].mThing;
}
}  // namespace

const CParkAnalyser* CWorld::GetParkAnalyser() const {
    const uint16_t id = mParkAnalyserThing;
    return static_cast<const CParkAnalyser*>(RowThing(id));
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    const uint16_t id = mParkAnalyserThing;
    return static_cast<CParkAnalyser*>(RowThing(id));
}

const CResearchLab* CWorld::GetResearchLab() const {
    const uint16_t id = mResearchLabThing;
    return static_cast<const CResearchLab*>(RowThing(id));
}

CResearchLab* CWorld::GetResearchLab() {
    const uint16_t id = mResearchLabThing;
    return static_cast<CResearchLab*>(RowThing(id));
}

CWeather* CWorld::GetWeather() {
    const uint16_t id = mWeatherThing;
    return static_cast<CWeather*>(RowThing(id));
}

CBankAccount* CWorld::GetBankAccount() {
    const uint16_t id = mBankAccountThing;
    return static_cast<CBankAccount*>(RowThing(id));
}

// The halfword at +0x1da736. The neighbours at +0x1da728 (the UI receiver) and +0x1da738 (the world state word) are not read.
CChallengeHQ* CWorld::GetChallengeHQ() {
    const uint16_t id = mChallengeHQThing;
    return static_cast<CChallengeHQ*>(RowThing(id));
}
