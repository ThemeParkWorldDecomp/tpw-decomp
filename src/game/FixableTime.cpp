// FixableTime.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of FixableTime code, named after it.

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

// ---- CFixableTime (its state is CSaveableTime's, see CFixableTime.h)

// 0x101479c0: holds the clock at the reading it has now, unless it is held already. The
// step is 1000 / frames-a-second milliseconds (31 for the 0x20 the game asks for).
void CFixableTime::UseFixedTime(uint32_t step) {
    CSaveableTime& time = static_cast<CSaveableTime&>(*this);
    if (time.mFixedFlag != 0) return;
    time.mFixedAt = ReadFixableTime();
    // divwu by zero is undefined on the PowerPC; the emulator answers the dividend, 1000.
    time.mFixedStep = step != 0 ? 1000u / step : 1000u;
    time.mFixedFlag = 1;
}

// 0x10147a58: lets the clock run again from the held reading.
void CFixableTime::CancelFixedTime() {
    CSaveableTime& time = static_cast<CSaveableTime&>(*this);
    if (time.mFixedFlag == 0) return;
    time.mFixedAdjustment = time.mFixedAt - time.mPausableTime.ReadPausableTime();
    time.mFixedFlag = 0;
}

// 0x10147ae0: a paused clock does not step. (Nothing translated calls it yet.)
void CFixableTime::NextFrame() {
    CSaveableTime& time = static_cast<CSaveableTime&>(*this);
    if (time.mPausableTime.GetPauseFlag() != 0) return;
    time.mFixedAt = time.mFixedAt + static_cast<int32_t>(time.mFixedStep);
}

// 0x10147b58: the held reading, or the running clock moved by the adjustment.
int32_t CFixableTime::ReadFixableTime() {
    CSaveableTime& time = static_cast<CSaveableTime&>(*this);
    if (time.mFixedFlag != 0) return time.mFixedAt;
    return time.mFixedAdjustment + time.mPausableTime.ReadPausableTime();
}

// CFixedTime (main app of SimTheme Park 2.04b3): the constructor, 0x10147bdc (the
// inventory's Game.cpp range). ReadFixedTime (0x10147c18) is in GameClock.cpp.

// A reading of 0 and, unaligned at +4, a double of 0.0.
CFixedTime::CFixedTime() : mLastClock(0), mMilliseconds(0.0) {}

// ---- CFixedTime

// 0x10147c18: the unsigned change of the clock since the last call, in milliseconds, added
// to the running total. The reading and the total are stored before the conversion.
uint32_t CFixedTime::ReadFixedTime() {
    const uint32_t now = LbTime_GetClock();
    mMilliseconds = mMilliseconds + static_cast<double>(now - mLastClock);
    mLastClock = now;
    return cw_cvt_fp2unsigned(mMilliseconds);
}
