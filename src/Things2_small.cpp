// The things' second batch, the small functions (unit w130): CParkAnalyser::BuiltSomething 0x100e46a4 (120 bytes) and NumberOfStaffType 0x100e8144
// (404 bytes), CBankAccount::MakeWithdrawal 0x100f33b0 (172 bytes), CAdvisor::IsPlaying 0x1000c6e0 (84 bytes) and FindHighestScoringSlotInQueue
// 0x1000b900 (216 bytes), CWeather::SetPrecipitationType 0x10139388 (256 bytes).
#include <cstdint>
#include <cstring>

#include "AdvisorQueue.h"
#include "Clock.h"
#include "Debug.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "tb_rain.h"

namespace {
const char kEntryZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10240242
constexpr uint16_t kNoThing = 0;
constexpr int32_t kFirstStaffKind = 4;
constexpr int32_t kLastStaffKind = 8;
constexpr int32_t kAdvisorQueueSize = 8;
}  // namespace

// The park's funny time (the Mac's stack result is copied at once) is the time the last object was built.
void CParkAnalyser::BuiltSomething() {
    TbTimeStamp stamp = gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    static_assert(sizeof(stamp) == sizeof(mLastObjectBuilt), "a time stamp is two words");
    std::memcpy(mLastObjectBuilt, &stamp, sizeof(mLastObjectBuilt));
}

// The things of the used list whose kind is one of 4 to 8 (the staff kinds), is `model` itself and is not dead. The walk and its three calls
// of GetWorld (one before the loop, two a thing) are the readers' (src/ParkAnalyser_readers.cpp).
int32_t CParkAnalyser::NumberOfStaffType(CThing::ThingModel model) const {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(this);
    int32_t count = 0;
    uint16_t cur = savvy->GetWorld().GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        const int32_t kind = thing->mKind;
        if (kind >= kFirstStaffKind && kind <= kLastStaffKind && static_cast<int32_t>(model) == kind && thing->mDead == 0) count++;
        cur = savvy->GetWorld().GetNextUsedThing(thing).mIndex;
    }
    return count;
}

// The balance goes down; the turn it first goes below 0 is kept; the park's total of withdrawals (the analyser at the game world's, not the
// world global) and the year's profit take the amount off.
void CBankAccount::MakeWithdrawal(uint32_t amount) {
    if (mWithdrawalsEnabled == 0) return;
    mBalance = static_cast<int32_t>(static_cast<uint32_t>(mBalance) - amount);
    if (mBalance < 0 && mLastBalance >= 0) mTurnEnteredRed = gWorld->mTurn;
    mLastBalance = mBalance;
    CParkAnalyser* analyser = gGameWorld->GetParkAnalyser();
    analyser->mHistory1F100[2].mTemp = static_cast<int32_t>(static_cast<uint32_t>(analyser->mHistory1F100[2].mTemp) + amount);
    mProfitThisYear = static_cast<int32_t>(static_cast<uint32_t>(mProfitThisYear) - amount);
}

// He is saying a line while the clock (unsigned) is before the line's start plus its length.
bool CAdvisor::IsPlaying() {
    return static_cast<uint32_t>(gClock.ReadFixed()) < mPlayStartTime + mPlayDuration;
}

// The valid request with the highest score (a signed compare; the first valid one seeds it, later ones must beat it); -1 for none.
int32_t CAdvisor::FindHighestScoringSlotInQueue() {
    int32_t best = -1;
    bool seeded = false;
    int32_t bestScore = 0;   // the Mac's register is uninitialised until the first valid request seeds it
    for (int32_t i = 0; i < kAdvisorQueueSize; ++i) {
        if (!mQueue[i].Valid()) continue;
        const int32_t score = mQueue[i].mValid != 0 ? mQueue[i].mScore : -1;
        if (!seeded) {
            best = i;
            bestScore = score;
            seeded = true;
        }
        const int32_t again = mQueue[i].mValid != 0 ? mQueue[i].mScore : -1;
        if (again > bestScore) {
            best = i;
            bestScore = again;
        }
    }
    return best;
}

// 1 and 0 start the rain (type 2 and 1; the ambience sound is started for 0 only if none plays), 2 stops the rain; the sound is faded and
// forgotten for 1 and 2. Any other value does nothing.
void CWeather::SetPrecipitationType(PrecipitationType type) {
    const int32_t t = static_cast<int32_t>(type);
    if (t == 1) {
        gei_Rain->SetRainType(2);
        if (mF54 != 0) {
            SOUND_FadeSoundEffect(static_cast<int32_t>(mF54));
            mF54 = 0;
        }
    } else if (t < 1) {
        if (t >= 0) {
            gei_Rain->SetRainType(1);
            if (mF54 == 0) mF54 = static_cast<uint32_t>(SOUND_SendEvent(0, gSoundBanks[0], 0x21, 0, 0, 0));
        }
    } else if (t < 3) {
        CRain* rain = gei_Rain;
        mF30 = 0;
        rain->SetRaindrops(0);
        if (mF54 != 0) {
            SOUND_FadeSoundEffect(static_cast<int32_t>(mF54));
            mF54 = 0;
        }
    }
}
