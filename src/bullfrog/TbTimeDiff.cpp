// TbTimeDiff.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbTimeDiff code, named after it.

#include "ppc_math.h"
#include "TbTimeDiff.h"

// TbTimeDiff.cpp (decompiled from the bullfrog library of SimTheme Park 2.04b3,
// functions at 0x100157bc..0x1001603c; original file name unknown, see TbTimeDiff.h).
//
// Products and sums wrap as the PowerPC's 64-bit arithmetic does (the build uses
// -fwrapv); the float forms go through CodeWarrior's runtime conversions (ppc_math.h).

namespace {
const int64_t kTicksPerMillisecond = 10000;
}  // namespace

TbTimeDiff TbTimeDiff::operator+(const TbTimeDiff& other) const {
    TbTimeDiff result;
    result.mTicks = mTicks + other.mTicks;
    return result;
}

TbTimeStamp TbTimeDiff::operator+(const TbTimeStamp& other) const {
    TbTimeStamp result;
    result.mTicks = mTicks + other.mTicks;
    return result;
}

TbTimeDiff TbTimeDiff::operator-(const TbTimeDiff& other) const {
    TbTimeDiff result;
    result.mTicks = mTicks - other.mTicks;
    return result;
}

TbTimeDiff& TbTimeDiff::operator+=(const TbTimeDiff& other) {
    mTicks += other.mTicks;
    return *this;
}

TbTimeDiff& TbTimeDiff::operator-=(const TbTimeDiff& other) {
    mTicks -= other.mTicks;
    return *this;
}

TbTimeDiff& TbTimeDiff::operator*=(int32_t factor) {
    mTicks *= factor;
    return *this;
}

// The product is rounded to single precision (fmuls) before the conversion back.
TbTimeDiff& TbTimeDiff::operator*=(float factor) {
    float product = cw_cvt_sll_flt(mTicks) * factor;
    mTicks = cw_cvt_dbl_sll(product);
    return *this;
}

TbTimeDiff& TbTimeDiff::operator/=(int32_t divisor) {
    mTicks /= divisor;
    return *this;
}

TbTimeDiff& TbTimeDiff::operator/=(float divisor) {
    float quotient = cw_cvt_sll_flt(mTicks) / divisor;
    mTicks = cw_cvt_dbl_sll(quotient);
    return *this;
}

bool TbTimeDiff::operator<(const TbTimeDiff& other) const { return mTicks < other.mTicks; }
bool TbTimeDiff::operator<=(const TbTimeDiff& other) const { return mTicks <= other.mTicks; }
bool TbTimeDiff::operator>(const TbTimeDiff& other) const { return mTicks > other.mTicks; }
bool TbTimeDiff::operator>=(const TbTimeDiff& other) const { return mTicks >= other.mTicks; }
bool TbTimeDiff::operator!=(const TbTimeDiff& other) const { return mTicks != other.mTicks; }
bool TbTimeDiff::operator==(const TbTimeDiff& other) const { return mTicks == other.mTicks; }

// The range check keeps the tick count within int64_t; a 32-bit sum always passes it.
TbError TbTimeDiff::Set(int32_t days, int32_t hours, int32_t minutes, int32_t seconds,
                        int32_t milliseconds) {
    int64_t total = milliseconds + seconds * 1000 + minutes * 60000 + hours * 3600000 +
                    days * 86400000;
    const int64_t kLimit = INT64_MAX / kTicksPerMillisecond;
    if (total > kLimit || total < -kLimit) return TbError(-1);
    mTicks = total * kTicksPerMillisecond;
    return TbError(0);
}

// Each part takes the low word of the 64-bit quotient, then a 32-bit remainder.
TbError TbTimeDiff::Get(int32_t* days, int32_t* hours, int32_t* minutes, int32_t* seconds,
                        int32_t* milliseconds) const {
    if (days != nullptr) *days = static_cast<int32_t>(mTicks / 864000000000LL);
    if (hours != nullptr) *hours = static_cast<int32_t>(mTicks / 36000000000LL) % 24;
    if (minutes != nullptr) *minutes = static_cast<int32_t>(mTicks / 600000000LL) % 60;
    if (seconds != nullptr) *seconds = static_cast<int32_t>(mTicks / 10000000LL) % 60;
    if (milliseconds != nullptr)
        *milliseconds = static_cast<int32_t>(mTicks / kTicksPerMillisecond) % 1000;
    return TbError(0);
}
