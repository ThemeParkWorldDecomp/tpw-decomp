// CWeather::UpdateTurn, main app 0x10138670 (1156 bytes), unit w124: the forecast, the warning, the rain, the lightning and the thunder, once a turn.
// Reads gGameWorld (not gWorld) for the clock and the random numbers; the original re-reads the global before every call.
#include <cstdint>
#include <cstring>
#include <new>

#include "CMessageCentre.h"
#include "Debug.h"
#include "MainBalance.h"
#include "TPSound.h"
#include "TbTimeDiff.h"
#include "ThingMessages.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ppc_math.h"
#include "tb_rain.h"

namespace {
// Lazy centre block (0x1d0 bytes, 29 headers), repeated before each post; the global is read again at the call.
CMessageCentre* MessageCentreForPost() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

int32_t Slot(int slot) { return gMainBalance.mValues[slot].mInteger; }

TbTimeStamp FunnyTimeNow() {
    return gGameWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}
}

void CWeather::UpdateTurn() {
    // A: the forecast has come true.
    if (mForecast.mF0C != 0) {
        TbTimeStamp now = FunnyTimeNow();
        if (now > *ForecastTime()) {
            CMsgWeather message(static_cast<int32_t>(mForecast.mF08));
            MessageCentreForPost()->PostMessage(message);
            mForecast.Invalidate();
        }
    }
    // B: no forecast; warn when the time of the next change less the days of warning has come.
    if (mForecast.mF0C == 0) {
        int32_t days = static_cast<int32_t>(static_cast<uint32_t>(Slot(kMainBalanceDaysBetweenChanges)) -
                                            static_cast<uint32_t>(Slot(kMainBalanceDaysOfWarning)));
        if (days < 0) days = 0;
        TbTimeDiff diff;
        diff.Set(days, 0, 0, 0, 0);
        TbTimeStamp warning = *StampF10() + diff;
        TbTimeStamp now = FunnyTimeNow();
        if (now > warning) {
            uint32_t quality = FindNewQuality();
            CMsgWeatherWarning message(Slot(kMainBalanceDaysOfWarning), static_cast<int32_t>(quality));
            MessageCentreForPost()->PostMessage(message);
        }
    }
    // C: the rain moves towards its target.
    if (static_cast<int32_t>(mF34) >= 0) {
        int32_t signedDifference = static_cast<int32_t>(mF30 - mF34);
        int32_t distance = signedDifference < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(signedDifference)) : signedDifference;   // abs
        int32_t step = Slot(kMainBalanceSpeedOfChange);
        if (distance < step) {
            if (mF34 == 0) {
                SetPrecipitationType(static_cast<PrecipitationType>(2));
                SND_SetRainLevel(0);
            }
        } else {
            if (static_cast<int32_t>(mF30) < static_cast<int32_t>(mF34)) {
                mF30 = mF30 + static_cast<uint32_t>(step);
            } else {
                mF30 = mF30 - static_cast<uint32_t>(step);
            }
            int32_t current = static_cast<int32_t>(mF30);
            int32_t clamped = current < 0 ? 0 : current;
            int32_t maximum = Slot(kMainBalanceMaxRaindrops);
            int32_t drops = clamped < maximum ? clamped : maximum;
            gei_Rain->SetRaindrops(drops);
            int32_t scaled = static_cast<int32_t>(mF30 * 100u);
            SND_SetRainLevel(scaled / Slot(kMainBalanceMaxRaindrops));
        }
    }
    // D: the lightning countdown.
    if (mF38 != 0) {
        mF3C = mF3C - static_cast<uint32_t>(Slot(kMainBalanceLightningCountdownSpeed));
        if (static_cast<int32_t>(mF3C) < 0) {
            D3DVECTOR start;
            D3DVECTOR end;
            FindLightningCoords(start, end, static_cast<int32_t>(mF44));
            gei_Rain->AddLightning(start, end, Slot(kMainBalanceLightningTime), 1);
            if (static_cast<int32_t>(mF44) <= 0) {
                int32_t base = static_cast<int32_t>(static_cast<uint32_t>(Slot(kMainBalanceThunderStartsThisFarAway)) - mF40);
                if (base < 0) base = 0;
                uint32_t random = gGameWorld->GetRand();
                mF44 = random % static_cast<uint32_t>(Slot(kMainBalanceThunderRandomiser)) + static_cast<uint32_t>(base);
                std::memcpy(&mF48, &end.x, sizeof(float));   // raw words, no float round trip
                std::memcpy(&mF4C, &end.y, sizeof(float));
                std::memcpy(&mF50, &end.z, sizeof(float));
            }
            mF40 = mF40 + 1;
            CTRACE(3, "LIGHTNING! BZZZZT!\n");
            uint32_t random = gGameWorld->GetRand();
            mF3C = random % static_cast<uint32_t>(Slot(kMainBalanceLightningCountdownFrom));
        }
    } else {
        mF40 = 0;
    }
    // E: the thunder countdown runs every turn.
    mF44 = mF44 - 1;
    if (mF44 == 0) {
        SOUND_SendEvent(0, gSoundBanks[0], 0x20, ppc_fctiwz(static_cast<double>(mF48)), ppc_fctiwz(static_cast<double>(mF4C)),
                        ppc_fctiwz(static_cast<double>(mF50)));
        CTRACE(3, "THUNDER! RUMBLE RUMBLE!\n");
    }
}
