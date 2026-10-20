// The sound library's value classes (unit w77, program "sound symbols shared", Code at 0x10000000): TbSoundPosition,
// TbSoundDistance, TbSoundReverbInfo, TbSoundVolumeControl, TbSoundSampleInfo and TbSoundSystemInfo. Declared in
// include/TbSoundPosition.h and include/TbSoundInfo.h. TbSoundSampleInfo::SetBank (0x1000df50) is in src/TbSoundBank.cpp.
#include "TbSoundInfo.h"

#include <cmath>
#include <cstring>
#include <new>

#include "ppc_math.h"

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

// 0x10015f3c (28 bytes)
void TbSoundPosition::Get(float& x, float& y, float& z) const {
    x = mX;
    y = mY;
    z = mZ;
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

// 0x1000cba4 (132 bytes)
TbSoundPosition TbSoundPosition::operator+(const TbSoundPosition& other) {
    TbSoundPosition local;
    local.mX = mX + other.mX;
    local.mY = mY + other.mY;
    local.mZ = mZ + other.mZ;
    return TbSoundPosition(local);
}

// ---- TbSoundDistance ---------------------------------------------------------------------------------------------------

// 0x10015950 (4 bytes, empty): the three floats stay as they were.
TbSoundDistance::TbSoundDistance() {}

// 0x10001324 (16 bytes)
TbSoundDistance::TbSoundDistance(float a, float b, float c) : mA(a), mB(b), mC(c) {}

// ---- TbSoundReverbInfo -------------------------------------------------------------------------------------------------

// 0x100012ac (72 bytes): constants 0.0f (0x1003316c), 0.25f (0x10033168), 0.5f (0x10033164), 1.0f (0x10033160).
TbSoundReverbInfo::TbSoundReverbInfo() {
    mField0 = 0;
    mField4 = 0;
    mField8 = 0;
    mFieldC = 0;
    mField10 = 0.0f;
    mField14 = 0;
    mField18 = 0.0f;
    mField1C = 0;
    mField20 = 0.25f;
    mField24 = 0.5f;
    mField28 = 0;
    mField2C = 1.0f;
}

// ---- TbSoundVolumeControl ----------------------------------------------------------------------------------------------

// 0x10009d9c (16 bytes)
TbSoundVolumeControl::TbSoundVolumeControl() {
    mVolumes = nullptr;
    mCount = 0;
}

// 0x1000aac8 (16 bytes): no bounds check.
double TbSoundVolumeControl::GetVolume(uint32_t index) {
    return static_cast<double>(mVolumes[index]);
}

// 0x10026f28 (8 bytes)
void TbSoundVolumeControl::SetVolumes(float* volumes) {
    mVolumes = volumes;
}

// 0x10026f68 (8 bytes)
void TbSoundVolumeControl::Init(uint32_t count) {
    mCount = count;
}

// ---- TbSoundSampleInfo -------------------------------------------------------------------------------------------------

// 0x100158bc (100 bytes): zeroes +0, +4, +0xc, +0x10, +0x14, +0x28, +0x2c, +0x30, then the distance's empty constructor
// and the reverb constructor; everything else is left as it was.
TbSoundSampleInfo::TbSoundSampleInfo() {
    mNumber = 0;
    mBank = nullptr;
    mField0C = 0;
    mField10 = 0;
    mFlags = 0;
    mPriority = 0;
    mPosition = nullptr;
    mVelocity = nullptr;
    new (&mDistance) TbSoundDistance();
    new (&mReverb) TbSoundReverbInfo();
}

// 0x1000dc60 (20 bytes)
void TbSoundSampleInfo::SetUserData(void* data) {
    mUserData = data;
    mFlags |= 0x1000;
}

// 0x1000dcac (20 bytes)
void TbSoundSampleInfo::SetUserId(uint32_t id) {
    mUserId = id;
    mFlags |= 4;
}

// 0x1000dcf4 (20 bytes)
void TbSoundSampleInfo::SetCallback(TbSndCallback* callback) {
    mCallback = callback;
    mFlags |= 0x800;
}

// 0x1000dd4c (20 bytes)
void TbSoundSampleInfo::SetLoopCount(uint32_t count) {
    mLoopCount = count;
    mFlags |= 0x40;
}

// 0x1000dd98 (20 bytes)
void TbSoundSampleInfo::SetVolume(uint32_t volume) {
    mVolume = volume;
    mFlags |= 2;
}

// 0x1000dde0 (20 bytes)
void TbSoundSampleInfo::SetPriority(uint32_t priority) {
    mPriority = priority;
    mFlags |= 0x400;
}

// 0x1000de2c (20 bytes)
void TbSoundSampleInfo::SetPitch(int32_t pitch) {
    mPitch = pitch;
    mFlags |= 0x20;
}

// 0x1000de74 (20 bytes)
void TbSoundSampleInfo::SetFrequency(float frequency) {
    mFrequency = frequency;
    mFlags |= 0x400000;
}

// 0x1000dec0 (20 bytes)
void TbSoundSampleInfo::SetPan(uint32_t pan) {
    mPan = pan;
    mFlags |= 0x10;
}

// 0x1000df08 (20 bytes): the flag first, then the number.
void TbSoundSampleInfo::SetNumber(uint32_t number) {
    mFlags |= 0x80;
    mNumber = number;
}

// 0x1000dfa4 (20 bytes): the flag first, then the size.
void TbSoundSampleInfo::SetStreamingBufferSize(uint32_t size) {
    mFlags |= 0x8000;
    mStreamingBufferSize = size;
}

// 0x1000dffc (24 bytes): stores the address of the caller's control and the index.
void TbSoundSampleInfo::SetVolumeControl(const TbSoundVolumeControl& control, uint32_t index) {
    mVolumeControl = &control;
    mVolumeControlIndex = index;
    mFlags |= 0x40000;
}

// 0x1000e068 (112 bytes): twelve words copied into +0x50, then the flag.
void TbSoundSampleInfo::SetReverb(const TbSoundReverbInfo& reverb) {
    mReverb.mField0 = reverb.mField0;
    mReverb.mField4 = reverb.mField4;
    mReverb.mField8 = reverb.mField8;
    mReverb.mFieldC = reverb.mFieldC;
    mReverb.mField10 = reverb.mField10;
    mReverb.mField14 = reverb.mField14;
    mReverb.mField18 = reverb.mField18;
    mReverb.mField1C = reverb.mField1C;
    mReverb.mField20 = reverb.mField20;
    mReverb.mField24 = reverb.mField24;
    mReverb.mField28 = reverb.mField28;
    mReverb.mField2C = reverb.mField2C;
    mFlags |= 0x100000;
}

// 0x1001847c (88 bytes): stores the address of the caller's velocity, only when it is valid.
void TbSoundSampleInfo::SetVelocity(const TbSoundPosition& velocity) {
    if (velocity.Valid()) {
        mVelocity = &velocity;
        mFlags |= 0x800000;
    }
}

// 0x1001851c (88 bytes): stores the address of the caller's position, only when it is valid.
void TbSoundSampleInfo::SetPosition(const TbSoundPosition& position) {
    if (position.Valid()) {
        mPosition = &position;
        mFlags |= 1;
    }
}

// 0x1001959c (40 bytes): copies the three floats (not the address).
void TbSoundSampleInfo::SetDistance(const TbSoundDistance& distance) {
    mDistance.mA = distance.mA;
    mDistance.mB = distance.mB;
    mDistance.mC = distance.mC;
    mFlags |= 0x200;
}

// 0x100196f4 (20 bytes)
void TbSoundSampleInfo::SetCompletedCallback(TbSndCallback* callback) {
    mCompletedCallback = callback;
    mFlags |= 0x2000;
}

// ---- TbSoundSystemInfo -------------------------------------------------------------------------------------------------

// 0x10011d7c (40 bytes): sets only +0, +4, +8, +0x18 (0), +0x1c (10 buffers), +0x20 and +0x40.
TbSoundSystemInfo::TbSoundSystemInfo() {
    mField0 = 0;
    mField4 = 0;
    mField8 = 0;
    mMask = 0;
    mBuffersToUse = 10;
    mHWnd = nullptr;
    mField40 = 0;
}

// 0x10011dd4 (28 bytes): sets only +4, +8, +0x18 (the speaker mask 0x20) and +0x3c (the address of the config).
TbSoundSystemInfo::TbSoundSystemInfo(const TbSpeaker::Config& speakers) {
    mField4 = 0;
    mField8 = 0;
    mMask = 0x20;
    mSpeakerConfig = &speakers;
}

// 0x1000ba78 (148 bytes): every member, the eight bytes at +0xc as one unit.
TbSoundSystemInfo::TbSoundSystemInfo(const TbSoundSystemInfo& other) {
    mField0 = other.mField0;
    mField4 = other.mField4;
    mField8 = other.mField8;
    mFieldC = other.mFieldC;
    mField10 = other.mField10;
    mMaxBuffers = other.mMaxBuffers;
    mMask = other.mMask;
    mBuffersToUse = other.mBuffersToUse;
    mHWnd = other.mHWnd;
    mGuid = other.mGuid;
    mClassId = other.mClassId;
    mListenerPosition = other.mListenerPosition;
    mFrontDir = other.mFrontDir;
    mTopDir = other.mTopDir;
    mListenerVelocity = other.mListenerVelocity;
    mSpeakerConfig = other.mSpeakerConfig;
    mField40 = other.mField40;
    mSamplesPlaying = other.mSamplesPlaying;
    mHardwareDevice = other.mHardwareDevice;
}

// 0x10011e34 (20 bytes)
void TbSoundSystemInfo::SetHWnd(void* window) {
    mHWnd = window;
    mMask |= 2;
}

// 0x10011e7c (20 bytes)
void TbSoundSystemInfo::SetGuid(NS_MacDoze::_GUID* guid) {
    mGuid = guid;
    mMask |= 0x400;
}

// 0x10011ed8 (20 bytes)
void TbSoundSystemInfo::SetClassId(NS_MacDoze::_GUID* classId) {
    mClassId = classId;
    mMask |= 0x2000;
}

// 0x10011f34 (88 bytes): the caller's position by address, only when valid.
void TbSoundSystemInfo::SetListenerPosition(const TbSoundPosition& position) {
    if (position.Valid()) {
        mListenerPosition = &position;
        mMask |= 4;
    }
}

// 0x10011fdc (124 bytes): both addresses, only when the front is valid and then the top is valid (the top is not tested when
// the front is not).
void TbSoundSystemInfo::SetListenerDirection(const TbSoundPosition& front, const TbSoundPosition& top) {
    if (front.Valid()) {
        if (top.Valid()) {
            mFrontDir = &front;
            mTopDir = &top;
            mMask |= 8;
        }
    }
}

// 0x100120bc (88 bytes)
void TbSoundSystemInfo::SetListenerVelocity(const TbSoundPosition& velocity) {
    if (velocity.Valid()) {
        mListenerVelocity = &velocity;
        mMask |= 0x10;
    }
}

// 0x10012164 (20 bytes)
void TbSoundSystemInfo::SetSpeakerConfig(const TbSpeaker::Config& speakers) {
    mSpeakerConfig = &speakers;
    mMask |= 0x20;
}

// 0x100121c8 (20 bytes)
void TbSoundSystemInfo::SetBuffersToUse(uint32_t count) {
    mBuffersToUse = count;
    mMask |= 0x40;
}

// 0x10012218 (20 bytes)
void TbSoundSystemInfo::SetMaxBuffers(uint32_t count) {
    mMaxBuffers = count;
    mMask |= 0x80;
}

// 0x10012264 (4 bytes): does nothing.
void TbSoundSystemInfo::SetHalfRate(uint32_t) {}

// 0x100122a0 (20 bytes)
void TbSoundSystemInfo::SetHardwareDevice(void* device) {
    mHardwareDevice = device;
    mMask |= 0x1000;
}

// 0x1001061c (8 bytes)
uint32_t TbSoundSystemInfo::GetBuffersToUse() const {
    return mBuffersToUse;
}

// 0x10015e74 (8 bytes)
const TbSpeaker::Config* TbSoundSystemInfo::GetSpeakerConfig() const {
    return mSpeakerConfig;
}

// 0x10015eb8 (8 bytes)
const TbSoundPosition* TbSoundSystemInfo::GetListenerVelocity() const {
    return mListenerVelocity;
}

// 0x10015f00 (8 bytes)
const TbSoundPosition* TbSoundSystemInfo::GetTopDir() const {
    return mTopDir;
}

// 0x10015f8c (8 bytes)
const TbSoundPosition* TbSoundSystemInfo::GetFrontDir() const {
    return mFrontDir;
}

// 0x10015fcc (8 bytes)
const TbSoundPosition* TbSoundSystemInfo::GetListenerPosition() const {
    return mListenerPosition;
}

// 0x10016014 (8 bytes): the address of the mask word (this + 0x18), not its value.
uint32_t* TbSoundSystemInfo::GetMask() {
    return &mMask;
}

// 0x10027120 (8 bytes)
uint32_t TbSoundSystemInfo::GetMaxBuffers() const {
    return mMaxBuffers;
}

// 0x10028c08 (8 bytes)
uint32_t TbSoundSystemInfo::GetSamplesPlaying() const {
    return mSamplesPlaying;
}
