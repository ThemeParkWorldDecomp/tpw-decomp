// Clock.cpp (SimTheme Park 2.04b3), 0x101472f4..0x10147754.
#include "Clock.h"

CClock gClock;   // __sinit_Clock_cpp at 0x101476dc

int32_t CClock::Read() { return mTime.ReadSaveableTime(); }             // 0x101472f4
int32_t CClock::ReadFixed() { return mFixedTime.ReadSaveableFixedTime(); } // 0x1014733c

void CClock::Pause() {   // 0x1014738c
    mTime.mPausableTime.Pause();
    mFixedTime.mPausableTime.Pause();
}

void CClock::UnPause() {   // 0x101473ec
    mTime.mPausableTime.UnPause();
    mFixedTime.mPausableTime.UnPause();
}

void CClock::TogglePause() {   // 0x10147450
    mTime.mPausableTime.TogglePause();
    mFixedTime.mPausableTime.TogglePause();
}

int32_t CClock::GetPauseFlag() { return mTime.mPausableTime.GetPauseFlag(); } // 0x101474b8

int32_t CClock::Save(void* file) {   // 0x10147508
    if (mTime.Save(file) == 0) return 0;
    return mFixedTime.Save(file);
}

int32_t CClock::Load(void* file) {   // 0x10147588
    if (mTime.Load(file) == 0) return 0;
    return mFixedTime.Load(file);
}

void CClock::PreSave() {   // 0x10147608
    mTime.mSavedTime = static_cast<uint32_t>(mTime.ReadSaveableTime());
    mFixedTime.mSavedTime = static_cast<uint32_t>(mFixedTime.ReadSaveableFixedTime());
}

void CClock::PostLoad() {   // 0x10147678
    mTime.AdjustTime();
    mFixedTime.AdjustTime();
}
