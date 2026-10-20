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
#include "CFixedTime.h"
#include "CFixableTime.h"
#include "CPausableFixedTime.h"
#include "CPausableTime.h"
#include "CSaveableFixedTime.h"
#include "CSaveableTime.h"
#include "CVariableTime.h"
#include "LbFile.h"
#include "LbTime.h"
#include "UI_Base.h"
#include "platform/samsutils.h"
#include "ppc_math.h"

// ---- CFixedTime

// 0x10147c18: the unsigned change of the clock since the last call, in milliseconds, added
// to the running total. The reading and the total are stored before the conversion.
uint32_t CFixedTime::ReadFixedTime() {
    const uint32_t now = LbTime_GetClock();
    mMilliseconds = mMilliseconds + static_cast<double>(now - mLastClock);
    mLastClock = now;
    return cw_cvt_fp2unsigned(mMilliseconds);
}

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

// ---- CSaveableTime and CSaveableFixedTime

// A saved clock is one word, mSavedTime, in a little-endian file. The Mac swaps the word
// in place around the call (stwbrx) and back again afterwards; this host is little-endian,
// so the field already is in file order and goes to the file as it is. Both ways stop
// with a text when the count of bytes moved is not four to seven (count >> 2 == 1), and
// nothing else is looked at: not the library's own result.

// 0x1015778c
int32_t CSaveableTime::ReadSaveableTime() { return mAdjustment + ReadFixableTime(); }

// 0x1015763c
int32_t CSaveableTime::Save(void* file) {
    uint32_t transferred;
    LbFile_Write(file, &mSavedTime, 4, &transferred);
    if ((transferred >> 2) != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    return 1;
}

// 0x101576ec: a failed read leaves whatever the file call put in the field.
int32_t CSaveableTime::Load(void* file) {
    uint32_t transferred;
    LbFile_Read(file, &mSavedTime, 4, &transferred);
    if ((transferred >> 2) != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    return 1;
}

// 0x101577fc: after a load, makes the clock read the saved time now.
void CSaveableTime::AdjustTime() { mAdjustment = static_cast<int32_t>(mSavedTime) - ReadFixableTime(); }

// 0x10157550
int32_t CSaveableFixedTime::ReadSaveableFixedTime() { return mAdjustment + mPausableTime.ReadPausableFixedTime(); }

// 0x101573f8
int32_t CSaveableFixedTime::Save(void* file) {
    uint32_t transferred;
    LbFile_Write(file, &mSavedTime, 4, &transferred);
    if ((transferred >> 2) != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    return 1;
}

// 0x101574ac
int32_t CSaveableFixedTime::Load(void* file) {
    uint32_t transferred;
    LbFile_Read(file, &mSavedTime, 4, &transferred);
    if ((transferred >> 2) != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    return 1;
}

// 0x101575c8
void CSaveableFixedTime::AdjustTime() {
    mAdjustment = static_cast<int32_t>(mSavedTime) - mPausableTime.ReadPausableFixedTime();
}

// ---- the user interface's clock

// 0x101cab58: the platform timer's microseconds as milliseconds, the low word of an
// unsigned 64-bit divide by 1000 (the same as LbTime_GetClock).
uint32_t timeGetTimeClockTickFunction::tickyticky() {
    return static_cast<uint32_t>(SamsUtilities::UTimer::GetAbsolute() / 1000);
}
