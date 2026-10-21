// VariableTime.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of VariableTime code, named after it.

#include "CFixableTime.h"
#include "CFixedTime.h"
#include "CPausableFixedTime.h"
#include "CPausableTime.h"
#include "CSaveableFixedTime.h"
#include "CSaveableTime.h"
#include "CVariableTime.h"
#include "LbFile.h"
#include "LbTime.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "UI_Base.h"
#include <new>

// CVariableTime (main app of SimTheme Park 2.04b3): the constructor, 0x1016886c (the
// inventory's FS_MasterSave.cpp range). The rate members and ReadVariableTime
// (0x101688d4..0x10168a3c) are in GameClock.cpp.

// The CFixedTime it starts with, then the rate at +0x18: 1.0. The reading and the
// milliseconds at +0x0c and +0x10 are not written.
CVariableTime::CVariableTime() {
    new (mFixedTimeStorage) CFixedTime;
    mRate = 1.0;
}

// The game clock (SimTheme Park 2.04b3): the time classes CClock (src/Clock.cpp) is built
// from, 28 functions in the inventory's Game.cpp, RideDB.cpp, SaveLoad.cpp and
// FS_MasterSave.cpp ranges, and the user interface's timeGetTime clock.
//
//   CFixedTime -> CPausableFixedTime -> CSaveableFixedTime      (ReadFixed)
//   CVariableTime -> CPausableTime -> CFixableTime -> CSaveableTime      (Read)
//
// Both chains count milliseconds of LbTime_GetClock, which is a 32-bit number that wraps.
// A reading adds the unsigned difference to the previous reading to a double and hands out
// that double as an unsigned long (CodeWarrior's saturating conversion), so a clock that
// goes backwards adds almost 2^32 milliseconds. Pausing keeps the reading it paused at and
// the time spent paused; the fixed-step mode holds the reading until NextFrame moves it.

// ---- CVariableTime

// 0x101688d4: any rate, none checked.
void CVariableTime::SetTimeRate(double rate) { mRate = rate; }

// 0x10168910 and 0x10168988: a step of a quarter of the rate, then the rate kept within
// 0.25 .. 2.0. The tests are written as the listing branches: a rate that is NaN passes
// both (it is neither below nor above), so it stays NaN.
static double ClampRate(double rate) {
    if (rate < 0.25) return 0.25;
    if (rate > 2.0) return 2.0;
    return rate;
}

void CVariableTime::IncreaseTimeRate() { mRate = ClampRate(mRate * 1.25); }

void CVariableTime::DecreaseTimeRate() { mRate = ClampRate(mRate / 1.25); }

double CVariableTime::GetTimeRate() { return mRate; }

// 0x10168a3c: like ReadFixedTime on its own reading and total (+0x0c and +0x10), but the
// change is scaled by the rate and added with one fused multiply-add.
uint32_t CVariableTime::ReadVariableTime() {
    const uint32_t now = LbTime_GetClock();
    mMilliseconds = ppc_fmadd(static_cast<double>(now - mLastClock), mRate, mMilliseconds);
    mLastClock = now;
    return cw_cvt_fp2unsigned(mMilliseconds);
}
