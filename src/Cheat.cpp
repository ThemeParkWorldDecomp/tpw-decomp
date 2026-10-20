// CCheat (SimTheme Park 2.04b3), 0x10146f24..0x10147294.
// File name unknown. Cheat strings and the Clock-only static initializer support
// keeping these functions outside Clock.cpp. Save data consists of two raw bytes.
#include "Cheat.h"
#include "CMessageCentre.h"
#include "LbFile.h"
#include "MsgCheatMode.h"
#include "platform/samsutils.h"

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

uint8_t CCheat::IsCheating() { return mCheating; }   // 0x101470c8
