// Test entry points for the four main-app leaves of src/game/advisor/Advisor.cpp, src/game/rides/BumperRide.cpp: RealTimePauseAdjusted, HOST_Sin,
// HOST_Cos and BMP_GenerateSinTab (library unit_HostLeaves). The advisor's state (the three words the clock keeps
// its pauses in) and the table of sines are the real globals; the game clock's two reads are fakes that record
// and answer from the case's queues.
#include <cstdint>
#include <cstring>

#include "Advisor.h"
#include "Clock.h"
#include "calllog.h"
#include "effect_manager.h"

using namespace calllog;

// ---- what the leaves read, owned by other files of the game
CVariableTime::CVariableTime() {}
CFixedTime::CFixedTime() {}
CClock gClock;
SAdvisorState gAdvisorState;

// The game clock: the time and the pause flag come from the case's queues.
int32_t CClock::ReadFixed() {
    ev("ReadFixed", {lab(this)});
    return static_cast<int32_t>(ret("ReadFixed"));
}
int32_t CClock::GetPauseFlag() {
    ev("GetPauseFlag", {lab(this)});
    return static_cast<int32_t>(ret("GetPauseFlag"));
}

extern "C" {

void* tpw_hl_clock() { return &gClock; }

// The table the case starts from (the leaves read it, BMP_GenerateSinTab replaces it) and the clock's three
// words, in the order of the advisor's state: paused, start, total.
void tpw_hl_reset(const int32_t* table, const int32_t* words) {
    std::memcpy(gHostSineTable, table, sizeof gHostSineTable);
    gAdvisorState.mClockPaused = words[0];
    gAdvisorState.mPauseStart = words[1];
    gAdvisorState.mPausedTotal = words[2];
}

// One operation: 1 RealTimePauseAdjusted, 2 HOST_Sin(arg), 3 HOST_Cos(arg), 4 BMP_GenerateSinTab.
int64_t tpw_hl_op(int32_t op, int32_t arg) {
    switch (op) {
    case 1: return static_cast<int64_t>(RealTimePauseAdjusted());
    case 2: return HOST_Sin(arg);
    case 3: return HOST_Cos(arg);
    case 4: BMP_GenerateSinTab(); return 0;
    }
    return -99;
}

// The three words, then the table: all of it, one number each.
int32_t tpw_hl_state(int64_t* out) {
    out[0] = gAdvisorState.mClockPaused;
    out[1] = gAdvisorState.mPauseStart;
    out[2] = gAdvisorState.mPausedTotal;
    for (int32_t i = 0; i < 512; ++i) out[3 + i] = gHostSineTable[i];
    return 515;
}

}  // extern "C"
