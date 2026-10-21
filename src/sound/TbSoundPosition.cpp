// TbSoundPosition.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbSoundPosition code, named after it.

#include "ppc_math.h"
#include "TbSoundInfo.h"
#include <cmath>
#include <cstring>
#include <new>

// The sound library's value classes (unit w77, program "sound symbols shared", Code at 0x10000000): TbSoundPosition,
// TbSoundDistance, TbSoundReverbInfo, TbSoundVolumeControl, TbSoundSampleInfo and TbSoundSystemInfo. Declared in
// include/TbSoundPosition.h and include/TbSoundInfo.h. TbSoundSampleInfo::SetBank (0x1000df50) is in src/TbSoundBank.cpp.

namespace {

// The MathLib calls TbSoundPosition::Valid makes, as the library's own fp.h numbers them (an assumption from the header,
// not something the program shows): FP_SNAN 0, FP_QNAN 1, FP_INFINITE 2, FP_ZERO 3, FP_NORMAL 4, FP_SUBNORMAL 5. Valid only
// compares the class with 1 and the finiteness with 0.
int MathLibClass(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    switch (std::fpclassify(value)) {
        case FP_NAN:
            return (bits & 0x00400000u) != 0 ? 1 : 0;
        case FP_INFINITE:
            return 2;
        case FP_ZERO:
            return 3;
        case FP_SUBNORMAL:
            return 5;
        default:
            return 4;
    }
}

int MathLibIsFinite(float value) {
    return std::isfinite(value) ? 1 : 0;
}

}  // namespace

// 0x1000c580 (16 bytes)
void TbSoundPosition::Set(float x, float y, float z) {
    mX = x;
    mY = y;
    mZ = z;
}

// 0x1000c8e0 (16 bytes): raw words, as the int constructor.
void TbSoundPosition::Set(int32_t x, int32_t y, int32_t z) {
    std::memcpy(&mX, &x, sizeof x);
    std::memcpy(&mY, &y, sizeof y);
    std::memcpy(&mZ, &z, sizeof z);
}

// 0x1000c5c0 (8 bytes)
void TbSoundPosition::SetX(float x) {
    mX = x;
}

// 0x1000c5f4 (8 bytes)
void TbSoundPosition::SetY(float y) {
    mY = y;
}

// 0x1000c628 (8 bytes)
void TbSoundPosition::SetZ(float z) {
    mZ = z;
}

// 0x1000c65c (64 bytes): the squares in single precision, summed z*z + (x*x + y*y) (fadds f0 = x2 + y2, then f1 = z2 + f0),
// then std::sqrt(float), which is the double sqrt rounded to float (0x1000c6cc, 0x1000c70c).
float TbSoundPosition::length() const {
    const float x2 = mX * mX;
    const float y2 = mY * mY;
    const float z2 = mZ * mZ;
    const float xy = x2 + y2;
    const float sum = z2 + xy;
    return static_cast<float>(std::sqrt(static_cast<double>(sum)));
}

// 0x1000c754 (40 bytes): other.y * y, then two fused multiply-adds (x, then z); the single result as a double.
double TbSoundPosition::DotProduct(const TbSoundPosition& other) const {
    const float yy = other.mY * mY;
    const float xy = ppc_fmadds(other.mX, mX, yy);
    const float xyz = ppc_fmadds(other.mZ, mZ, xy);
    return static_cast<double>(xyz);
}

// 0x1000c7c4 (236 bytes): the six MathLib calls in this order, each reached only when the one before did not settle it:
// __fpclassifyf of x, y, z (a quiet NaN, class 1, fails), then __isfinitef of x, y, z (0 fails).
bool TbSoundPosition::Valid() const {
    if (MathLibClass(mX) == 1) return false;
    if (MathLibClass(mY) == 1) return false;
    if (MathLibClass(mZ) == 1) return false;
    if (MathLibIsFinite(mX) == 0) return false;
    if (MathLibIsFinite(mY) == 0) return false;
    if (MathLibIsFinite(mZ) == 0) return false;
    return true;
}

// 0x1000c920 (52 bytes): memcmp of the twelve bytes, so -0.0 differs from 0.0 and equal NaN patterns are equal.
bool TbSoundPosition::operator!=(const TbSoundPosition& other) const {
    return std::memcmp(this, &other, 12) != 0;
}

// 0x1000c994 (40 bytes)
TbSoundPosition& TbSoundPosition::operator*=(float factor) {
    mX = mX * factor;
    mY = mY * factor;
    mZ = mZ * factor;
    return *this;
}

// 0x1000c9ec (40 bytes)
TbSoundPosition& TbSoundPosition::operator/=(float divisor) {
    mX = mX / divisor;
    mY = mY / divisor;
    mZ = mZ / divisor;
    return *this;
}

// 0x1000ca44 (132 bytes): a zeroed local, the three differences, then the copy constructor into the result.
TbSoundPosition TbSoundPosition::operator-(const TbSoundPosition& other) {
    TbSoundPosition local;
    local.mX = mX - other.mX;
    local.mY = mY - other.mY;
    local.mZ = mZ - other.mZ;
    return TbSoundPosition(local);
}

// ---- TbSoundPosition ---------------------------------------------------------------------------------------------------

// 0x1000cb64 (20 bytes): three 0.0f (the constant at 0x1003323c).
TbSoundPosition::TbSoundPosition() {
    const float zero = 0.0f;
    mX = zero;
    mY = zero;
    mZ = zero;
}

// 0x100246e4 (16 bytes): the three ints are stored as raw words, not converted.
TbSoundPosition::TbSoundPosition(int32_t x, int32_t y, int32_t z) {
    std::memcpy(&mX, &x, sizeof x);
    std::memcpy(&mY, &y, sizeof y);
    std::memcpy(&mZ, &z, sizeof z);
}

// 0x1000cba4 (132 bytes)
TbSoundPosition TbSoundPosition::operator+(const TbSoundPosition& other) {
    TbSoundPosition local;
    local.mX = mX + other.mX;
    local.mY = mY + other.mY;
    local.mZ = mZ + other.mZ;
    return TbSoundPosition(local);
}
