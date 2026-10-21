// Clock.cpp: name proven by __sinit_Clock_cpp in the main application.

#include "Cheat.h"
#include "Clock.h"
#include "CMessageCentre.h"
#include "LbFile.h"
#include "MsgCheatMode.h"
#include "platform/samsutils.h"

// CCheat (SimTheme Park 2.04b3), 0x10146f24..0x10147294.
// File name unknown. Cheat strings and the Clock-only static initializer support
// keeping these functions outside Clock.cpp. Save data consists of two raw bytes.

CCheat::CCheat() : mFlag09(0), mCheating(0), mFlag0B(0) {}   // 0x10146f24

void CCheat::SetCheatStatus(bool cheating) {   // 0x10146f5c
    mCheating = cheating;
    if (mCheating != 0) {
        {
            CMsgCheatMode message(IsCheating());
            if (gMessageCentre == nullptr) gMessageCentre = new CMessageCentre;
            gMessageCentre->PostMessage(message);
        }
        SamsUtilities::UDebug("cheat mode on");
    } else {
        SamsUtilities::UDebug("cheat mode off");
    }
}

// CMsgCheatMode (SimTheme Park 2.04b3), 0x10147050 and 0x101472bc.
// File name unknown. These inline-looking members were emitted beside CCheat.
// PARTIAL: the constructor remains a callee owned by untranslated message code.

// Original stores the derived vtable and, for a positive ABI flag, frees storage.
// Native C++ supplies vtable transitions and delete's storage handling.
CMsgCheatMode::~CMsgCheatMode() {}   // 0x10147050

uint8_t CCheat::IsCheating() { return mCheating; }   // 0x101470c8
int32_t CCheat::Load(void*& file) {   // 0x101470f8
    uint32_t transferred;
    LbFile_Read(file, &mCheating, 1, &transferred);
    if (transferred != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    LbFile_Read(file, &mFlag0B, 1, &transferred);
    if (transferred != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    return 1;
}

int32_t CCheat::Save(void*& file) const {   // 0x101471d0
    uint32_t transferred;
    uint8_t value = mCheating;
    LbFile_Write(file, &value, 1, &transferred);
    if (transferred != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    value = mFlag0B;
    LbFile_Write(file, &value, 1, &transferred);
    if (transferred != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    return 1;
}

int32_t CMsgCheatMode::GetType() const { return 0x15; }   // 0x101472bc

// Clock.cpp (SimTheme Park 2.04b3), 0x101472f4..0x10147754.

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
