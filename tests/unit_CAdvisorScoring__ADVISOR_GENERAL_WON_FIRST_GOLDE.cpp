// Test library for the scorers of unit v51073. The wrapper fills the balance slots (Mac offsets 0x14 on) from a word array and calls one.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"

typedef int32_t (CAdvisorScoring::*Fn)();

static const Fn kFns[] = {
    &CAdvisorScoring::_ADVISOR_GENERAL_WON_FIRST_GOLDEN_TICKET, &CAdvisorScoring::_ADVISOR_WELCOME_NAMEENTRY,
    &CAdvisorScoring::_ADVISOR_GENERAL_BLOWUP_ADVISOR_1, &CAdvisorScoring::_ADVISOR_WEATHER_FUNNY_STORM,
    &CAdvisorScoring::_ADVISOR_GENERAL_BLOWUP_ADVISOR_2, &CAdvisorScoring::_ADVISOR_WEATHER_FUNNY_SNOW,
    &CAdvisorScoring::_ADVISOR_WEATHER_FUNNY_RAIN, &CAdvisorScoring::_ADVISOR_RIDE_EXIT_BLOCKED,
    &CAdvisorScoring::_ADVISOR_GOLD_TICKET_CLUES_FOR_REMAINDER, &CAdvisorScoring::_ADVISOR_OBJECTIVE_MET_OFFERDOUBLE,
    &CAdvisorScoring::_ADVISOR_OBJECTIVE_MET_OFFERDOUBLE_2, &CAdvisorScoring::_ADVISOR_CONGRAT_GOLDEN_KEY_ANYPARK,
    &CAdvisorScoring::_ADVISOR_RIDE_CONDEMNED, &CAdvisorScoring::_ADVISOR_CASH_LOW,
    &CAdvisorScoring::_ADVISOR_CASH_LOW_TAKE_LOAN, &CAdvisorScoring::_ADVISOR_PEEPS_STUCK_BREAKDOWN,
    &CAdvisorScoring::_ADVISOR_PEEPS_STUCK_CONDEMNED, &CAdvisorScoring::_ADVISOR_PREBUILT_PARK,
    &CAdvisorScoring::_ADVISOR_CHALLENGE, &CAdvisorScoring::_ADVISOR_WELCOME_LOST_WORLD,
    &CAdvisorScoring::_ADVISOR_WELCOME_SPACE, &CAdvisorScoring::_ADVISOR_WELCOME_HALLOWEEN,
    &CAdvisorScoring::_ADVISOR_WELCOME_FANTASY, &CAdvisorScoring::_ADVISOR_WELCOME_VETERAN,
    &CAdvisorScoring::_ADVISOR_WELCOME_ENTER_NAME, &CAdvisorScoring::_ADVISOR_WELCOME_NO_NAME,
    &CAdvisorScoring::_ADVISOR_WELCOME_NEWBIE, &CAdvisorScoring::_ADVISOR_BUY_MORE_LAND,
    &CAdvisorScoring::_ADVISOR_BANKRUPTED, &CAdvisorScoring::_ADVISOR_TUTORIAL_MESSAGE,
};

extern "C" {

// words: the Mac words from offset 0x14 on, count n.
uint32_t tpw_v51073_call(int32_t which, const uint32_t* words, int32_t n) {
    void* mem = tpw::ArenaAllocZeroed(sizeof(CAdvisorScoring) + 0x1000);
    CAdvisorScoring* object = static_cast<CAdvisorScoring*>(mem);
    for (int32_t i = 0; i < n; i++) object->mBalance.mValues[i].mInteger = static_cast<int32_t>(words[i]);
    return static_cast<uint32_t>((object->*kFns[which])());
}

}  // extern "C"
