// PausableTime.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of PausableTime code, named after it.

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

// ---- CPausableFixedTime

// 0x1015400c, 0x10154088, 0x10154118 and 0x101541d0: the same four on the fixed time.
void CPausableFixedTime::Pause() {
    if (mPauseFlag != 0) return;
    mPausedAt = static_cast<int32_t>(mFixedTime.ReadFixedTime());
    mPauseFlag = 1;
}

void CPausableFixedTime::UnPause() {
    if (mPauseFlag == 0) return;
    const int32_t now = static_cast<int32_t>(mFixedTime.ReadFixedTime());
    mPausedDuration = mPausedDuration + (now - mPausedAt);
    mPauseFlag = 0;
}

void CPausableFixedTime::TogglePause() {
    if (mPauseFlag != 0) {
        UnPause();
    } else {
        Pause();
    }
}

int32_t CPausableFixedTime::ReadPausableFixedTime() {
    if (mPauseFlag == 0) return static_cast<int32_t>(mFixedTime.ReadFixedTime()) - mPausedDuration;
    return mPausedAt - mPausedDuration;
}
// ---- CPausableTime

// 0x10154268: reads once, to keep the reading the pause began at.
void CPausableTime::Pause() {
    if (mPauseFlag != 0) return;
    mPausedAt = static_cast<int32_t>(mVariableTime.ReadVariableTime());
    mPauseFlag = 1;
}

// 0x101542e0: the time spent paused grows by the time since Pause.
void CPausableTime::UnPause() {
    if (mPauseFlag == 0) return;
    const int32_t now = static_cast<int32_t>(mVariableTime.ReadVariableTime());
    mPausedDuration = mPausedDuration + (now - mPausedAt);
    mPauseFlag = 0;
}

// 0x1015436c: the original has both bodies inline, with no call to Pause or UnPause.
void CPausableTime::TogglePause() {
    if (mPauseFlag != 0) {
        UnPause();
    } else {
        Pause();
    }
}

// 0x10154420
int32_t CPausableTime::GetPauseFlag() { return mPauseFlag; }

// 0x1015445c: a paused clock does not read the time.
int32_t CPausableTime::ReadPausableTime() {
    if (mPauseFlag == 0) return static_cast<int32_t>(mVariableTime.ReadVariableTime()) - mPausedDuration;
    return mPausedAt - mPausedDuration;
}
