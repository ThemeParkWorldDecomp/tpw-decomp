// CWeather and CWeatherForecast (main app, SimTheme Park 2.04b3), unit w181: CWeather::FinalDestroy 0x10137f3c, Serialise 0x10137f9c,
// ReceiveMessage 0x1013855c, FindLightningCoords 0x10138c10, SetQuality 0x10139000, SetWindDirection 0x101394d0, SetWindForce 0x101395a8,
// UpdateWindInfoToEngine 0x10139604; CWeatherForecast::Serialise 0x101396b8, Invalidate 0x10139854, Set 0x10139894; and the four save
// instances LOADVARSTRUCT / SAVEVARSTRUCT<D3DVECTOR_SWAPABLE> (0x10139b90, 0x10139a04) and LOADVAR / SAVEVAR<PrecipitationType> (0x10139ce8,
// 0x10139e20). The member names in the comments are the save names CWeather::Serialise gives.
//
// Quirks kept: Serialise's order is the same loading and saving (fifteen values, noLog 0) and it answers 0 for a version outside 0 and 1 after
// the base call; the forecast's Serialise answers 0 without touching the file for a version outside 0 and 1; SetQuality picks rain for an odd
// random number and snow for an even one when both ranges hold, divides with divw (32-bit wrapping products) and reads mWindDir BEFORE the
// random number; SetWindDirection's loops run through the member and the assert's double is the ORIGINAL argument; FindLightningCoords
// reads the clamp bounds from the stack copies, the start and end vectors through memory (distinct vectors only are tested).
//
// The four TOC words 0x102a45fc, 0x102a4600, 0x102a4604 and 0x102a4608 the instances read are each instance's copy of the file layer's
// success value (initial 0, never written): the shared bodies' .Succeeded() covers them, so nothing is declared for them.
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "Serialise.h"
#include "TPEngine.h"
#include "TbTimeDiff.h"
#include "ThingMessages.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ppc_math.h"
#include "tb_rain.h"

namespace {

// WeatherEffects slots, (Mac offset - 8) / 4 (names from the description rows).
constexpr int kSlotQualityForSnowLow = 303;         // +0x4c4
constexpr int kSlotQualityForSnowHigh = 304;        // +0x4c8
constexpr int kSlotQualityForRainLow = 305;         // +0x4cc
constexpr int kSlotQualityForRainHigh = 306;        // +0x4d0
constexpr int kSlotQualityForLightningLow = 307;    // +0x4d4
constexpr int kSlotQualityForLightningHigh = 308;   // +0x4d8

int32_t Slot(int slot) { return gMainBalance.mValues[slot].mInteger; }

TbTimeStamp FunnyTimeNow() {
    return gGameWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}

// The stamp's eight bytes into two words of the object (the host's int64 is the pair as the headers hold it).
void StoreStamp(uint32_t* words, const TbTimeStamp& stamp) { std::memcpy(words, &stamp, sizeof(stamp)); }

// divw: the quotient truncated toward zero; the overflowing INT_MIN / -1 gives the sign of the dividend smeared (what the emulator answers).
int32_t Divw(int32_t a, int32_t b) {
    if (a == INT32_MIN && b == -1) return a >> 31;
    return a / b;
}

// An unsigned 32-bit value or a 30-bit random through the 2^52 trick: the double is exact, fsubs rounds it to single.
float UnsignedToSingle(uint32_t value) { return static_cast<float>(static_cast<double>(value)); }

// The percentage of the quality inside [low, high], then the drops left: min((100 - pct) * MaxRaindrops / 100, MaxRaindrops).
int32_t PercentInRange(int32_t quality, int32_t low, int32_t high) {
    const int32_t span = static_cast<int32_t>(static_cast<uint32_t>(high) - static_cast<uint32_t>(low));
    if (span == 0) return 100;
    const int32_t scaled = static_cast<int32_t>(static_cast<uint32_t>(quality - low) * 100u);
    return Divw(scaled, span);
}

int32_t DropsFor(int32_t pct) {
    const int32_t maximum = Slot(kMainBalanceMaxRaindrops);
    const int32_t product = static_cast<int32_t>(static_cast<uint32_t>(100 - pct) * static_cast<uint32_t>(maximum));
    const int32_t drops = product / 100;
    return drops < maximum ? drops : maximum;
}

}  // namespace

// ---- D3DVECTOR_SWAPABLE: three floats, each word reversed on the way to and from the file (the listing: the rlwinm / rlwimi quartets).
static void SwapForFile(D3DVECTOR_SWAPABLE& vector) {
    vector.x = serialise::SwapWord(vector.x);
    vector.y = serialise::SwapWord(vector.y);
    vector.z = serialise::SwapWord(vector.z);
}
static void UnswapForFile(D3DVECTOR_SWAPABLE& vector) {
    vector.x = serialise::UnswapWord(vector.x);
    vector.y = serialise::UnswapWord(vector.y);
    vector.z = serialise::UnswapWord(vector.z);
}

template<> int32_t LOADVARSTRUCT<D3DVECTOR_SWAPABLE>(void* file, D3DVECTOR_SWAPABLE* value, const char* name, int32_t noLog) {
    return serialise::LoadStruct(file, value, name, noLog);   // 0x10139b90, 260 bytes
}
template<> int32_t SAVEVARSTRUCT<D3DVECTOR_SWAPABLE>(void* file, D3DVECTOR_SWAPABLE* value, const char* name, int32_t noLog) {
    return serialise::SaveStruct(file, value, name, noLog);   // 0x10139a04, 312 bytes
}
template<> int32_t LOADVAR<PrecipitationType>(void* file, PrecipitationType* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x10139ce8, 236 bytes
}
template<> int32_t SAVEVAR<PrecipitationType>(void* file, PrecipitationType* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x10139e20, 260 bytes
}

// 0x10137f3c, 52 bytes.
void CWeather::FinalDestroy() {
    mBase.FinalDestroy();
    gei_Rain->Close();
}

// 0x10137f9c, 1424 bytes.
int32_t CWeather::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (mBase.Serialise(file, save, version, unused) != 0) return 1;
    if (version < 0 || version >= 2) return 0;
    int32_t* quality = reinterpret_cast<int32_t*>(&mF0C);
    int32_t* currentDrops = reinterpret_cast<int32_t*>(&mF30);
    int32_t* targetDrops = reinterpret_cast<int32_t*>(&mF34);
    bool* lightning = reinterpret_cast<bool*>(&mF38);
    int32_t* lightningCountdown = reinterpret_cast<int32_t*>(&mF3C);
    int32_t* lightningNum = reinterpret_cast<int32_t*>(&mF40);
    int32_t* thunderCountdown = reinterpret_cast<int32_t*>(&mF44);
    MacLong* rainSound = reinterpret_cast<MacLong*>(&mF54);
    PrecipitationType* ptype = reinterpret_cast<PrecipitationType*>(&mF58);
    bool* overridden = reinterpret_cast<bool*>(&mF64);
    if (save == 0) {
        if (LOADVAR<int32_t>(file, quality, "mQuality", 0) != 0) return 1;
        if (LOADVARSTRUCT<TbTimeStamp>(file, StampF10(), "mLastChanged", 0) != 0) return 1;
        if (LOADVARSTRUCT<TbTimeStamp>(file, StampF18(), "mLastForecasted", 0) != 0) return 1;
        if (mForecast.Serialise(file, save, version, unused) != 0) return 1;
        if (LOADVAR<int32_t>(file, currentDrops, "mCurrentDrops", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, targetDrops, "mTargetDrops", 0) != 0) return 1;
        if (LOADVAR<bool>(file, lightning, "mLightning", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, lightningCountdown, "mLightningCountdown", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, lightningNum, "mLightningNumThisPeriod", 0) != 0) return 1;
        if (LOADVAR<int32_t>(file, thunderCountdown, "mThunderCountdown", 0) != 0) return 1;
        if (LOADVARSTRUCT<D3DVECTOR_SWAPABLE>(file, LastLightningLocation(), "mLastLightningLocation", 0) != 0) return 1;
        if (LOADVAR<MacLong>(file, rainSound, "mRainSoundHandle", 0) != 0) return 1;
        if (LOADVAR<PrecipitationType>(file, ptype, "mPType", 0) != 0) return 1;
        if (LOADVAR<float>(file, &mWindDirection, "mWindDir", 0) != 0) return 1;
        if (LOADVAR<float>(file, &mWindForce, "mWindForce", 0) != 0) return 1;
        if (LOADVAR<bool>(file, overridden, "mOverridden", 0) != 0) return 1;
        return 0;
    }
    if (SAVEVAR<int32_t>(file, quality, "mQuality", 0) != 0) return 1;
    if (SAVEVARSTRUCT<TbTimeStamp>(file, StampF10(), "mLastChanged", 0) != 0) return 1;
    if (SAVEVARSTRUCT<TbTimeStamp>(file, StampF18(), "mLastForecasted", 0) != 0) return 1;
    if (mForecast.Serialise(file, save, version, unused) != 0) return 1;
    if (SAVEVAR<int32_t>(file, currentDrops, "mCurrentDrops", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, targetDrops, "mTargetDrops", 0) != 0) return 1;
    if (SAVEVAR<bool>(file, lightning, "mLightning", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, lightningCountdown, "mLightningCountdown", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, lightningNum, "mLightningNumThisPeriod", 0) != 0) return 1;
    if (SAVEVAR<int32_t>(file, thunderCountdown, "mThunderCountdown", 0) != 0) return 1;
    if (SAVEVARSTRUCT<D3DVECTOR_SWAPABLE>(file, LastLightningLocation(), "mLastLightningLocation", 0) != 0) return 1;
    if (SAVEVAR<MacLong>(file, rainSound, "mRainSoundHandle", 0) != 0) return 1;
    if (SAVEVAR<PrecipitationType>(file, ptype, "mPType", 0) != 0) return 1;
    if (SAVEVAR<float>(file, &mWindDirection, "mWindDir", 0) != 0) return 1;
    if (SAVEVAR<float>(file, &mWindForce, "mWindForce", 0) != 0) return 1;
    if (SAVEVAR<bool>(file, overridden, "mOverridden", 0) != 0) return 1;
    return 0;
}

// 0x1013855c, 224 bytes. Type 2 is the warning (the forecast is set, the last-forecasted stamp is now); type 1 the new quality, unless overridden.
void CWeather::ReceiveMessage(const CMsg& message) {
    const int32_t type = message.GetType();
    if (type == 2) {
        const CMsgWeatherWarning& warning = static_cast<const CMsgWeatherWarning&>(message);
        mForecast.Set(warning.mDays, warning.mQuality);
        StoreStamp(&mF18, FunnyTimeNow());
    } else if (type == 1) {
        if (mF64 == 0) SetQuality(static_cast<const CMsgWeather&>(message).mQuality);
    }
}

// 0x10138c10, 528 bytes.
void CWeather::FindLightningCoords(D3DVECTOR& start, D3DVECTOR& end, int32_t strikesLeft) {
    (void)strikesLeft;
    const float zero = 0.0f;
    const float width = ppc_fmadds(10.0f, UnsignedToSingle(static_cast<uint32_t>(gHeightfieldWidth)), zero);
    const float spanX = width - zero;
    const float height = ppc_fmadds(10.0f, UnsignedToSingle(static_cast<uint32_t>(gHeightfieldHeight)), zero);
    const float spanZ = height - zero;
    const float k = 1073741824.0f;
    float f = UnsignedToSingle(gGameWorld->GetRand() & 0x3fffffffu) / k;
    start.x = ppc_fmadds(spanX, f, zero);
    start.y = zero;
    f = UnsignedToSingle(gGameWorld->GetRand() & 0x3fffffffu) / k;
    start.z = ppc_fmadds(spanZ, f, zero);
    f = UnsignedToSingle(gGameWorld->GetRand() & 0x3fffffffu) / k;
    float t = ppc_fmadds(100.0f, f, start.x);
    end.x = t - 50.0f;
    end.y = 300.0f;
    f = UnsignedToSingle(gGameWorld->GetRand() & 0x3fffffffu) / k;
    t = ppc_fmadds(100.0f, f, start.z);
    end.z = t - 50.0f;
    if (start.x > width) start.x = width;
    if (start.x < 1.0f) start.x = 1.0f;
    if (start.z > height) start.z = height;
    if (start.z < 1.0f) start.z = 1.0f;
}

// 0x10139000, 860 bytes.
void CWeather::SetQuality(int32_t quality) {
    const int32_t floor = quality < 1 ? 1 : quality;
    quality = floor >= 100 ? 100 : floor;
    mF0C = static_cast<uint32_t>(quality);
    const TbTimeStamp now = FunnyTimeNow();
    StoreStamp(&mF10, now);
    const int32_t q = static_cast<int32_t>(mF0C);
    bool snow = q >= Slot(kSlotQualityForSnowLow) && q <= Slot(kSlotQualityForSnowHigh);
    bool rain = q >= Slot(kSlotQualityForRainLow) && q <= Slot(kSlotQualityForRainHigh);
    mF38 = (q >= Slot(kSlotQualityForLightningLow) && q <= Slot(kSlotQualityForLightningHigh)) ? 1 : 0;
    if (snow && rain) {
        if ((gGameWorld->GetRand() & 1u) != 0) {
            snow = false;
        } else {
            rain = false;
        }
    }
    if (snow) {
        const int32_t pct = PercentInRange(static_cast<int32_t>(mF0C), Slot(kSlotQualityForSnowLow), Slot(kSlotQualityForSnowHigh));
        SetPrecipitationType(static_cast<PrecipitationType>(1));
        mF34 = static_cast<uint32_t>(DropsFor(pct));
        gei_Rain->SetVelocity(0.8f);
    } else if (rain) {
        const int32_t pct = PercentInRange(static_cast<int32_t>(mF0C), Slot(kSlotQualityForRainLow), Slot(kSlotQualityForRainHigh));
        SetPrecipitationType(static_cast<PrecipitationType>(0));
        mF34 = static_cast<uint32_t>(DropsFor(pct));
        gei_Rain->SetVelocity(2.5f);
    } else {
        mF34 = 0;
    }
    const int32_t left = static_cast<int32_t>(100u - mF0C);
    SetWindForce(static_cast<float>(left) / 100.0f);
    const float direction = mWindDirection;
    const float base = direction - 45.0f;
    const uint32_t random = gGameWorld->GetRand();
    const float turn = UnsignedToSingle(random % 90u);
    SetWindDirection(base + turn);
    UpdateWindInfoToEngine();
}

// 0x101394d0, 168 bytes.
void CWeather::SetWindDirection(float direction) {
    mWindDirection = direction;
    while (mWindDirection < 0.0f) mWindDirection = mWindDirection + 360.0f;
    while (mWindDirection > 360.0f) mWindDirection = mWindDirection - 360.0f;
    const float now = mWindDirection;
    GAME_ASSERTMSG(now >= 0.0f && now <= 360.0f, "Something loopy is going on with the wind direction. It is %f\n", static_cast<double>(direction));
}

// 0x101395a8, 48 bytes.
void CWeather::SetWindForce(float force) {
    if (force < 0.0f) {
        mWindForce = 0.0f;
    } else if (force > 1.0f) {
        mWindForce = 1.0f;
    } else {
        mWindForce = force;
    }
}

// 0x10139604, 56 bytes.
void CWeather::UpdateWindInfoToEngine() { gei_Rain->SetWindParameters(mWindDirection, mWindForce); }

// 0x101396b8, 356 bytes.
int32_t CWeatherForecast::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    (void)unused;
    TbTimeStamp* time = reinterpret_cast<TbTimeStamp*>(&mF00);
    int32_t* quality = reinterpret_cast<int32_t*>(&mF08);
    bool* valid = reinterpret_cast<bool*>(&mF0C);
    if (version < 0 || version >= 2) return 0;
    if (save != 0) {
        if (SAVEVARSTRUCT<TbTimeStamp>(file, time, "mTime", 0) != 0) return 1;
        if (SAVEVAR<int32_t>(file, quality, "mQuality", 0) != 0) return 1;
        if (SAVEVAR<bool>(file, valid, "mValid", 0) != 0) return 1;
        return 0;
    }
    if (LOADVARSTRUCT<TbTimeStamp>(file, time, "mTime", 0) != 0) return 1;
    if (LOADVAR<int32_t>(file, quality, "mQuality", 0) != 0) return 1;
    if (LOADVAR<bool>(file, valid, "mValid", 0) != 0) return 1;
    return 0;
}

// 0x10139854, 12 bytes.
void CWeatherForecast::Invalidate() { mF0C = 0; }

// 0x10139894, 208 bytes.
void CWeatherForecast::Set(int32_t days, int32_t quality) {
    TbTimeDiff diff;
    diff.Set(days, 0, 0, 0, 0);
    const TbTimeStamp now = FunnyTimeNow();
    const TbTimeStamp due = now + diff;
    std::memcpy(&mF00, &due, sizeof(due));
    mF08 = static_cast<uint32_t>(quality);
    mF0C = 1;
}

