// AudioPlaceHolder.cpp: name proven by __sinit_AudioPlaceHolder_cpp in the sound library.

#include "AudioPlaceHolder.h"
#include "bfl_FileSystem.h"
#include "LbFile.h"
#include "LbTime.h"
#include "PlaceHolderHelpers.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "snd_commands.h"
#include "snd_containers.h"
#include "TbMapStreamer.h"
#include "TbMpegBase.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundInfo.h"
#include "TbSoundManager.h"
#include <cmath>
#include <cstring>
#include <new>

// AudioPlaceHolder.cpp: CAudioPlaceHolder, the base of every sound the game is playing (unit w79g2, "sound symbols shared", file
// AudioPlaceHolder.cpp of the library). See include/AudioPlaceHolder.h for the layout, the vtable and the flag bits.
//
// Conventions of this file. The sound manager is reached through the class static mpSoundManager (0x1003f360), never tested for
// NULL except where the listing tests it. Every command to the manager is built on the stack and handed to its virtual
// SystemCommand (slot +0x18); the answer is dropped unless the function says otherwise. The file words of Save and Load are the
// Mac's SamsUtilities swaps: the file is little-endian, so the host (little-endian) writes and reads the native words (the helpers of
// include/TbSoundBank.h do nothing here). The random numbers come from bfl::maths::Random and RangedRandomSlow (stand-ins), always
// given a COPY of mRandomSeed. All float arithmetic is single precision in the listing's order; the build has -ffp-contract=off.

TbSoundManager* CAudioPlaceHolder::mpSoundManager = nullptr;
int32_t CAudioPlaceHolder::mRandomSeed = 0;

namespace {

// The bits CAudioPlaceHolder ORs into the word at +0xc of a TbSoundSampleInfo (include/TbSoundInfo.h calls it mField0C; its
// names are unknown): the requests the sample is made with.
constexpr uint32_t kInfoNonPositional = 0x1;   // FillSampleInfo of a non-positional placeholder
constexpr uint32_t kInfoEventMode = 0x4;       // the placeholder's flag 0x40000 (the event's flag 0x4000)
constexpr uint32_t kInfoPositional = 0x20;     // a positional sample (Update, FillSampleInfo, ModifyPositionForBoundingRectangle)
constexpr uint32_t kInfoNotEventMode = 0xc0;   // the placeholder's flag 0x40000 clear
constexpr uint32_t kInfoUserId = 0x200;        // the user id was given
constexpr uint32_t kInfoQSound = 0x1000;       // the manager has QSound on

// The roles of the bytes and words of the map data that include/TbMapStreamer.h names by offset (read off the listings).
uint8_t VolumeMin(const CSoundData& s) { return s.mF0C[0]; }      // +0xc
uint8_t VolumeMax(const CSoundData& s) { return s.mF0C[1]; }      // +0xd
int8_t PitchMin(const CSoundData& s) { return static_cast<int8_t>(s.mF0C[2]); }   // +0xe
int8_t PitchMax(const CSoundData& s) { return static_cast<int8_t>(s.mF0C[3]); }   // +0xf
constexpr uint32_t kParamVolume = 1;   // GetParameterValue's masks: the volume, the pitch and the pseudo time
constexpr uint32_t kParamPitch = 2;
constexpr uint32_t kParamPseudoTime = 4;

// One word, 4 bytes of the file: the answers of LbFile_Read and LbFile_Write are not looked at.
void WriteBytes(void* file, const void* data, uint32_t size) {
    uint32_t transferred;
    LbFile_Write(file, data, size, &transferred);
}

void ReadBytes(void* file, void* data, uint32_t size) {
    uint32_t transferred;
    LbFile_Read(file, data, size, &transferred);
}

template <class T>
void WriteWord(void* file, T value) {
    const T swapped = ::SamsUtilities::USwap32<T>(value);
    WriteBytes(file, &swapped, 4);
}

template <class T>
void ReadWord(void* file, T& value) {
    ReadBytes(file, &value, 4);
    value = ::SamsUtilities::USwap32<T>(value);
}

constexpr uint32_t kMagic = 0x12d591;

}  // namespace

// ---- TbSndCallback ------------------------------------------------------------------------------------------------------

TbSndCallback::TbSndCallback() {}

void CAudioPlaceHolder::CDistPriority::SetDistance(uint16_t distance) {
    mDistance = distance;
}

// TbSoundManager.cpp: the sound manager the game holds (unit w79e, sound symbols shared, file CSprocketsModule.cpp and
// AudioPlaceHolder.cpp in the library). See include/TbSoundManager.h for the layout, the vtable and the calling convention. The
// commands' Execute bodies (unit w79d), the sample store, the device module (p-sound) and TbMapStreamer::SetDroppable
// (0x100231fc) are other units'.

const TbSoundPosition* TbSoundManager::GetListenerPosition() {
    return mListenerPosition;
}

void CAudioPlaceHolder::CDistPriority::SetBasePriority(uint16_t priority) {
    mBasePriority = priority;
}

// The sound library's value classes (unit w77, program "sound symbols shared", Code at 0x10000000): TbSoundPosition,
// TbSoundDistance, TbSoundReverbInfo, TbSoundVolumeControl, TbSoundSampleInfo and TbSoundSystemInfo. Declared in
// include/TbSoundPosition.h and include/TbSoundInfo.h. TbSoundSampleInfo::SetBank (0x1000df50) is in src/TbSoundBank.cpp.

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

// Unit v51112: small functions of the sound program's AudioPlaceHolder, TbSoundSample*, CSprocketsModule ranges (see the header block).

// ---- random ------------------------------------------------------------------------------------------------------------------

// LbMaths_RangedRandomSlow__FRlUl 0x10018c88 (56): divwu by zero is undefined on the hardware; the emulator answers 0, so the
// remainder is the random value itself.
uint32_t LbMaths_RangedRandomSlow(int32_t& seed, uint32_t range) {
    const uint32_t value = static_cast<uint32_t>(LbMaths_Random(seed));
    const uint32_t quotient = range != 0 ? value / range : 0;
    return value - quotient * range;
}

namespace bfl::maths {
// 0x10018c30 (32)
uint32_t RangedRandomSlow(int32_t& seed, uint32_t range) { return LbMaths_RangedRandomSlow(seed, range); }
// 0x10019ad0 (32)
uint32_t Random(int32_t& seed) { return static_cast<uint32_t>(LbMaths_Random(seed)); }
}  // namespace bfl::maths

// Declared before its first use: the definition is further down.
namespace SamsUtilities::UBinary {
template <>
int32_t RotateRight<int32_t>(int32_t value, int32_t amount);
}

// LbMaths_Random__FRl 0x10018cf4 (80): linear congruential step; the RotateRight result is dropped by the original.
int32_t LbMaths_Random(int32_t& seed) {
    const int32_t next = static_cast<int32_t>(static_cast<uint32_t>(seed) * 0x19660du + 0x3c6ef35fu);
    seed = next;
    SamsUtilities::UBinary::RotateRight<int32_t>(seed, 13);
    return next;
}

uint32_t TbSoundManager::IsQSoundOn() {
    if ((mOptions & 8) != 0) return 1;
    return 0;
}

// 0x1001959c (40 bytes): copies the three floats (not the address).
void TbSoundSampleInfo::SetDistance(const TbSoundDistance& distance) {
    mDistance.mA = distance.mA;
    mDistance.mB = distance.mB;
    mDistance.mC = distance.mC;
    mFlags |= 0x200;
}

float TbSoundManager::GetRolloffFactorToMinMaxDistance(float min, float max) {
    const float span = max - min;
    const float scaled = min / 0.015625f;   // the constant at 0x10033284 is 1/64
    return (scaled - min) / span;
}

float TbSoundManager::GetDistanceMultiplier() {
    return mDistanceMultiplier;
}

// 0x100196f4 (20 bytes)
void TbSoundSampleInfo::SetCompletedCallback(TbSndCallback* callback) {
    mCompletedCallback = callback;
    mFlags |= 0x2000;
}

TbSoundVolumeControl* TbSoundManager::GetVolumeControl() {
    return &mVolumeControl;
}

// The system command constructors of the sound library (unit w79b, sound symbols shared): each stores its vptr (the Mac
// vtable addresses are in include/snd_commands.h) and at most two words. The Execute bodies are unit w79d's.

namespace SystemCommand {

Stop::Stop(void* handle) : HandleCommand(handle) {}

Pause::Pause(void* handle) : HandleCommand(handle) {}

Restart::Restart(void* handle) : HandleCommand(handle) {}

}  // namespace SystemCommand

// PlaceHolderHelpers.cpp: CMapReference and the one-shot sentence children of the sound library (unit w79g4, "sound symbols shared",
// the library's CSprocketsModule.cpp). See include/PlaceHolderHelpers.h for the classes, their vtables and the overrides.
//
// Conventions of this file. The parent of an Element is the sentence placeholder kept in the extra data's mParent; the Mac code
// reads it through the extra data on every use. Virtual calls the Mac makes through the object's vtable are virtual calls here
// (Init calls +0x3c, +0x40 and +0x50, FillSampleInfo +0x4c); the base's non-virtual members are called directly. The sound manager
// is the class static CAudioPlaceHolder::mpSoundManager (0x1003f360). The pointers of the map data that the Mac compares as 32-bit
// addresses are compared as arena offsets (the value a Ptr32 holds).

// ---- CMapReference ---------------------------------------------------------------------------------------------------------

CMapReference::CMapReference() {}

void TbSndCallback::CompletedCallback(void*, void*, int32_t) {}

CAudioPlaceHolder::CDistPriority::operator uint32_t() const {
    return (static_cast<uint32_t>(mDistance) << 16) | mBasePriority;
}

CAudioPlaceHolder::~CAudioPlaceHolder() {
    mListHandle = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(mListHandle) & 0xffff);
    if (mExtra != nullptr) delete mExtra;
    if (mCatagory != nullptr) mCatagory->Release();
}

// ---- priority and the manager's commands ---------------------------------------------------------------------------------

float CAudioPlaceHolder::ChangeDistPriority() {
    mPriority.SetBasePriority(static_cast<uint16_t>(mEvent->mF0C));
    float result = 0.0f;
    const TbSoundPosition* listener = mpSoundManager->GetListenerPosition();
    if ((mFlags & kNonPositional) != 0) {
        mPriority.SetDistance(0xffff);
    } else if (listener != nullptr) {
        float listenerX, listenerY, listenerZ;
        float x, y, z;
        listener->Get(listenerX, listenerY, listenerZ);
        mPosition.Get(x, y, z);
        const float dx = std::fabs(x - listenerX);
        const float dz = std::fabs(z - listenerZ);
        const float squared = dx * dx + dz * dz;
        result = squared;
        const uint32_t scaled = CvtDblUl(static_cast<double>(0.01f * squared));
        mPriority.SetDistance(static_cast<uint16_t>(0xffff - static_cast<uint16_t>(scaled)));
    } else {
        mPriority.SetDistance(0);
    }
    return result;
}

void CAudioPlaceHolder::SetPriorityAndVolume(uint32_t priority) {
    mPriority = priority;
    if (mHandle == nullptr) return;
    TbSoundSampleInfo info;
    info.SetPriority(mPriority);
    if ((mFlags & kFading) != 0) {
        const float scaled = static_cast<float>(mVolume) * mFade;
        info.SetVolume(CvtDblUl(static_cast<double>(scaled)));
    }
    SystemCommand::SetSampleInfo command(mHandle, info);
    mpSoundManager->SystemCommand(command);
}

CAudioPlaceHolder::CDistPriority& CAudioPlaceHolder::CDistPriority::operator=(uint32_t word) {
    mDistance = static_cast<uint16_t>(word >> 16);
    mBasePriority = static_cast<uint16_t>(word);
    return *this;
}

// ---- parameters and the random pickers -----------------------------------------------------------------------------------

void CAudioPlaceHolder::UpdateParameter(uint8_t id, uint32_t value) {
    if (mExtra == nullptr) AllocateExtraData();
    bool dependants = false;
    for (uint8_t i = 0; i < 4; ++i) {
        if (id == mExtra->mParamId[i]) {
            mExtra->mParamValue[i] = static_cast<uint8_t>(value);
            if (i != 0) dependants = true;
        }
    }
    if (dependants && (mFlags & kVolumeControlOff) == 0) UpdateParameterDepandants();
}

// ---- extra data and flags ------------------------------------------------------------------------------------------------

void CAudioPlaceHolder::AllocateExtraData() {
    if (mExtra == nullptr) mExtra = new CAudioPlaceHolderExtraData();
}

// ---- CAudioPlaceHolderExtraData ------------------------------------------------------------------------------------------

// The id bytes after the first and the parent word stay as the allocator gave them.
CAudioPlaceHolderExtraData::CAudioPlaceHolderExtraData() {
    mBounds.left = 0;
    mBounds.top = 0;
    mBounds.right = 0;
    mBounds.bottom = 0;
    mChild = nullptr;
    mParamId[0] = 0;
    std::memset(mParamValue, 0, 4);
}

void CAudioPlaceHolder::UpdateParameterDepandants() {
    if (mHandle == nullptr || mpSoundManager == nullptr) return;
    TbSoundSampleInfo info;
    if ((mFlags & kFixedVolume) == 0) {
        const int32_t volume = GetRandomVolume();
        mVolume = static_cast<uint16_t>(volume);
        info.SetVolume(static_cast<uint16_t>(volume));
    }
    const int32_t pitch = GetRandomPitch();
    mPitch = static_cast<int16_t>(pitch);
    info.SetPitch(static_cast<int16_t>(pitch));
    SystemCommand::SetSampleInfo command(mHandle, info);
    mpSoundManager->SystemCommand(command);
}

// A valid new position or velocity that differs from the stored one replaces both; a playing sample is told (a non-positional
// one gets the stored (0, 1, 0) and (0, 0, 0) set with the integer Set, whose words are 0 and 1). A refusal by the manager
// forgets the sample. The answer is 0 whatever happened.
TbError CAudioPlaceHolder::Update(const TbSoundPosition& position, const TbSoundPosition& velocity) {
    if (position.Valid()) {
        if (mPosition != position || mVelocity != velocity) {
            mPosition = position;
            mVelocity = velocity;
            if (mHandle != nullptr) {
                TbSoundSampleInfo info;
                if ((mFlags & kNonPositional) != 0) {
                    mPosition.Set(0, 1, 0);
                    mVelocity.Set(0, 0, 0);
                } else {
                    info.mField0C |= kInfoPositional;
                }
                info.SetPosition(mPosition);
                info.SetVelocity(mVelocity);
                SystemCommand::SetSampleInfo command(mHandle, info);
                const TbError error = mpSoundManager->SystemCommand(command);
                if (static_cast<int32_t>(error) < 0) {
                    mHandle = nullptr;
                    mFlags &= ~kPlaying;
                }
            }
        }
    }
    return TbError(0);
}

// ---- the callback --------------------------------------------------------------------------------------------------------

void CAudioPlaceHolder::CompletedCallback(void*, void* userData, int32_t reason) {
    if (userData != mListHandle) return;
    mFlags &= ~kPlaying;
    mHandle = nullptr;
    if (reason != 0) return;
    if ((mFlags & kSubclass40) != 0) return;
    if ((mFlags & kVolumeControlOff) != 0) return;
    if ((mFlags & kOutOfRange) != 0) return;
    mFlags |= kFinished;
}

void CAudioPlaceHolder::Release() {
    this->~CAudioPlaceHolder();
}

// The fresh bit is set whatever the event says; a nonzero maximum distance (a NaN counts) is kept.
void CAudioPlaceHolder::SetFlagsFromEvent(float maxDistance) {
    mFlags = kFresh;
    if ((mEvent->mFlags & 8) != 0) {
        mFlags |= kLoopForever;
        mFlags |= kLooping;
    }
    if ((mEvent->mFlags & 4) != 0) mFlags |= kLooping;
    if ((mEvent->mFlags & 0x200) != 0) mFlags |= kNonPositional;
    if ((mEvent->mFlags & 0x4000) != 0) mFlags |= kEventMode;
    if (maxDistance == 0.0f) return;
    mMaxDistance = maxDistance;
    mFlags |= kMaxDistanceFromEvent;
}

// The sound's two parameter ids go to the extra data's ids 1 and 2 (their low bytes); without ids an existing extra data has
// its ids 1 to 3 cleared and none is made.
void CAudioPlaceHolder::InitParamatisedControls() {
    if (mSound->mF16 != 0 || mSound->mF1A != 0) {
        AllocateExtraData();
        mExtra->mParamId[1] = static_cast<uint8_t>(mSound->mF16);
        mExtra->mParamId[2] = static_cast<uint8_t>(mSound->mF1A);
        mExtra->mParamId[3] = 0;
    } else if (mExtra != nullptr) {
        mExtra->mParamId[1] = 0;
        mExtra->mParamId[2] = 0;
        mExtra->mParamId[3] = 0;
    }
}

// ---- Init ----------------------------------------------------------------------------------------------------------------

TbError CAudioPlaceHolder::Init(CCatagoryContainer* container, void* event, const TbSoundPosition& position, float maxDistance) {
    mCatagory = container;
    mCatagory->AddRef();
    mEvent = static_cast<CEventData*>(event);
    if (position.Valid()) mPosition = position;
    mVolumeControlIndex = static_cast<uint8_t>(container->mData->mF08);
    if (mEvent->mF12 != 0) {
        AllocateExtraData();
        mExtra->mParamId[0] = static_cast<uint8_t>(mEvent->mF12);
    }
    SetFlagsFromEvent(maxDistance);
    bool ok = ChooseRandomSound(false) || mSound != nullptr;
    if (ok) {
        InitParamatisedControls();
        if (mSample == nullptr) {
            mSample = ChooseRandomSample(false);
            if (mSample == nullptr) ok = false;
        }
    }
    if (!ok) {
        mCatagory->Release();
        mCatagory = nullptr;
        return TbError(-1);
    }
    mEndTime = mSample->mF08 + LbTime_GetClock() + 250;
    return TbError(0);
}

// A parameter 1 or 2 of the sound (its mask against the sound's two masks) answers the byte value stored for it.
bool CAudioPlaceHolder::GetParameterValue(uint32_t mask, uint32_t& value) {
    if ((mSound->mF18 & mask) != 0) {
        value = mExtra->mParamValue[1];
        return true;
    }
    if ((mSound->mF1C & mask) != 0) {
        value = mExtra->mParamValue[2];
        return true;
    }
    return false;
}

// The minimum and maximum bytes are swapped in place when inverted. With the extra data and a parameter the range is scaled by
// the parameter's percentage (an unsigned division of the 32-bit product); otherwise the random remainder of the range.
int32_t CAudioPlaceHolder::GetRandomVolume() {
    if (mSound == nullptr) return 100;
    if (VolumeMax(*mSound) < VolumeMin(*mSound)) {
        const uint8_t minimum = mSound->mF0C[0];
        mSound->mF0C[0] = mSound->mF0C[1];
        mSound->mF0C[1] = minimum;
    }
    const uint32_t range = static_cast<uint32_t>(VolumeMax(*mSound)) - VolumeMin(*mSound);
    uint32_t percent;
    if (mExtra != nullptr && GetParameterValue(kParamVolume, percent)) {
        return static_cast<int32_t>(VolumeMin(*mSound) + (range * percent) / 100);
    }
    int32_t seed = mRandomSeed;
    const uint32_t offset = range != 0 ? bfl::maths::RangedRandomSlow(seed, range) : 0;
    return static_cast<int32_t>(offset + VolumeMin(*mSound));
}

// The same with the signed pitch bytes.
int32_t CAudioPlaceHolder::GetRandomPitch() {
    if (mSound == nullptr) return 0;
    if (PitchMax(*mSound) < PitchMin(*mSound)) {
        const uint8_t minimum = mSound->mF0C[2];
        mSound->mF0C[2] = mSound->mF0C[3];
        mSound->mF0C[3] = minimum;
    }
    const int32_t range = static_cast<int32_t>(PitchMax(*mSound)) - PitchMin(*mSound);
    uint32_t percent;
    if (mExtra != nullptr && GetParameterValue(kParamPitch, percent)) {
        return PitchMin(*mSound) + static_cast<int32_t>((static_cast<uint32_t>(range) * percent) / 100);
    }
    int32_t seed = mRandomSeed;
    const int32_t offset = range != 0 ? static_cast<int32_t>(bfl::maths::RangedRandomSlow(seed, static_cast<uint32_t>(range))) : 0;
    return offset + PitchMin(*mSound);
}

// ---- the sample info -----------------------------------------------------------------------------------------------------

bool CAudioPlaceHolder::FillBasicSampleInfo(TbSoundSampleInfo& info) {
    TbBank* bank = mpSoundManager->GetBank(mSample->mBankIndex);
    if (bank == nullptr) return false;
    info.SetNumber(mSample->mF00);
    info.SetBank(reinterpret_cast<Bank::CBank*>(bank));   // GetBank answers what the factory made, a Bank::CBank
    if ((mFlags & kUserId) != 0) {
        info.SetUserId(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mEvent)));
        info.mField0C |= kInfoUserId;
    }
    if ((mFlags & kEventMode) != 0) {
        info.mField0C |= kInfoEventMode;
    } else {
        info.mField0C |= kInfoNotEventMode;
    }
    return true;
}

// 0 when both limits are 0. With the parameter (no swap in this case, and the difference is signed): min + (difference * percent)
// / 100 with a signed division of the 32-bit product. Otherwise the limits are swapped in place when inverted (unsigned) and the
// result is min + the random remainder of the range.
int32_t CAudioPlaceHolder::GetPsuedoTime() {
    if (mSound->mF10 == 0 && mSound->mF12 == 0) return 0;
    uint32_t percent;
    int32_t offset;
    if (mExtra != nullptr && GetParameterValue(kParamPseudoTime, percent)) {
        const int32_t difference = static_cast<int32_t>(mSound->mF12) - static_cast<int32_t>(mSound->mF10);
        const int32_t product = static_cast<int32_t>(static_cast<uint32_t>(difference) * percent);
        offset = product / 100;
    } else {
        if (mSound->mF12 < mSound->mF10) {
            const uint16_t minimum = mSound->mF10;
            mSound->mF10 = mSound->mF12;
            mSound->mF12 = minimum;
        }
        const int32_t range = static_cast<int32_t>(mSound->mF12) - static_cast<int32_t>(mSound->mF10);
        int32_t seed = mRandomSeed;
        offset = range != 0 ? static_cast<int32_t>(bfl::maths::RangedRandomSlow(seed, static_cast<uint32_t>(range))) : 0;
    }
    return static_cast<int32_t>(mSound->mF10) + offset;
}

// Fills `info` for starting the sample; false when it should not start (no sound or sample, no manager, the basic info failed, the
// sample is over, or a non-looping long sample is within a second of the end of its cycle).
bool CAudioPlaceHolder::FillSampleInfo(uint32_t now, TbSoundSampleInfo& info) {
    if (mSample == nullptr || mSound == nullptr || mpSoundManager == nullptr) return false;
    if (!FillBasicSampleInfo(info)) return false;
    info.SetVolume(GetVolume());
    info.SetVolumeControl(*mpSoundManager->GetVolumeControl(), mVolumeControlIndex);
    if ((mFlags & kPitchSet) != 0) {
        info.SetPitch(mPitch);
    } else {
        mPitch = static_cast<int16_t>(GetRandomPitch());
        info.SetPitch(mPitch);
    }
    info.SetPriority(mPriority);
    info.SetCompletedCallback(this);
    info.SetUserData(mListHandle);
    if (mSample->mF08 == 0) mSample = mSound->mSamples.get();
    mPauseTime = static_cast<uint32_t>(GetPsuedoTime());
    if (FinishedPlaying(now)) return false;
    if ((mFlags & kNonPositional) != 0) {
        mPosition.Set(0, 1, 0);
        info.mField0C |= kInfoNonPositional;
    } else {
        const uint32_t length = mSample->mF08;
        const uint32_t elapsed = now - (mEndTime - (length + 0xfa));
        // divwu: an undefined result for a zero length on the PowerPC; the emulator's quotient is taken to be zero
        const uint32_t remainder = length != 0 ? elapsed - (elapsed / length) * length : elapsed;
        if (remainder > 1000 && (mFlags & kLooping) == 0 && length > 1000 && length - remainder < 1000) return false;
        float distance = mpSoundManager->GetDistanceMultiplier();
        float maxDistance = GetMaxDistance();
        maxDistance = maxDistance * distance;
        const float minDistance = 2.0f;
        if (maxDistance <= minDistance) maxDistance = 2.1f;   // 0x1003328c
        const float rolloff = mpSoundManager->GetRolloffFactorToMinMaxDistance(minDistance, maxDistance);
        const TbSoundDistance distanceRecord(minDistance, maxDistance, rolloff);
        info.SetDistance(distanceRecord);
        distance = mpSoundManager->GetDistanceMultiplier();
        if (1.0f != distance) mPosition *= distance;
        info.mField0C |= kInfoPositional;
        if (mpSoundManager->IsQSoundOn()) info.mField0C |= kInfoQSound;
    }
    info.SetPosition(mPosition);
    info.SetVelocity(mVelocity);
    if ((mFlags & kLoopForever) != 0) info.SetLoopCount(9999);
    return true;
}

// ---- volume, pitch, distance ---------------------------------------------------------------------------------------------

// The volume as given, or a random one that is kept; while fading, scaled by the fade (single precision, to unsigned).
uint32_t CAudioPlaceHolder::GetVolume() {
    uint32_t volume;
    if ((mFlags & kVolumeSet) != 0) {
        volume = mVolume;
    } else {
        volume = static_cast<uint16_t>(GetRandomVolume());
        mVolume = static_cast<uint16_t>(volume);
    }
    if ((mFlags & kFading) != 0) {
        const float scaled = static_cast<float>(volume) * mFade;
        volume = CvtDblUl(static_cast<double>(scaled));
    }
    return volume;
}

float CAudioPlaceHolder::GetMaxDistance() {
    if ((mFlags & kMaxDistanceFromEvent) != 0) {
        return mMaxDistance * mpSoundManager->GetDistanceMultiplier();
    }
    if (mSound == nullptr) return 0.0f;
    return mSound->mF22;
}

CSampleData* CAudioPlaceHolder::ChooseRandomSample(bool rotate) {
    if (mSound == nullptr) return nullptr;
    const uint32_t count = mSound->mSampleCount & 0xffff;
    if (count == 1) return mSound->mSamples.get();
    if (count == 0) return nullptr;
    int32_t seed = mRandomSeed;
    const uint32_t pick = bfl::maths::Random(seed) >> 16;
    CSampleData* sample = mSound->mSamples.get();
    for (uint32_t left = count; left != 0; --left) {
        if (sample->mF04 >= pick) {
            const int8_t index = static_cast<int8_t>(sample - mSound->mSamples.get());
            GetNextSample(rotate, index, sample);
            return sample;
        }
        ++sample;
    }
    return nullptr;
}

void CAudioPlaceHolder::GetNextSample(bool rotate, int8_t index, CSampleData*& sample) {
    if (rotate) {
        const uint32_t count = mSound->mSampleCount & 0xffff;
        if (count > 2 && index == mLastSample) {
            index = static_cast<int8_t>(index + 1);
            const uint32_t next = static_cast<uint32_t>(static_cast<int32_t>(index));
            index = static_cast<int8_t>(next - (next / count) * count);
            sample = mSound->mSamples.get() + static_cast<int32_t>(index);
        }
    }
    mLastSample = index;
}

// Stops a sample whose volume control is off (a looping or one-shot-looping event is remembered as waiting) and answers whether
// the control is on.
bool CAudioPlaceHolder::VolumeControlIsOn() {
    if (mpSoundManager->VolumeControlIsOn(mVolumeControlIndex)) {
        mFlags &= ~kVolumeControlOff;
        return true;
    }
    if ((mFlags & kVolumeControlOff) == 0) {
        if (mEvent != nullptr) {
            const uint16_t eventFlags = mEvent->mFlags;
            if ((eventFlags & 4) != 0 || (eventFlags & 8) != 0) mFlags |= kVolumeControlOff;
        }
        if (mHandle != nullptr) {
            SystemCommand::Stop command(mHandle);
            mpSoundManager->SystemCommand(command);
            mHandle = nullptr;
        }
        mFlags &= ~kPlaying;
    }
    return false;
}

// The weighted pick: the first sound whose running total of weights reaches Random() >> 16. A single sound is taken at once;
// without a pick the first sound is taken (the paths that pick call GetNextSound and InitParamatisedControls, the others do not).
bool CAudioPlaceHolder::ChooseRandomSound(bool rotate) {
    if (mEvent == nullptr) return false;
    if (mEvent->mSoundCount == 1) {
        mSound = mEvent->mSounds.get();
        return true;
    }
    int32_t seed = mRandomSeed;
    const uint32_t pick = bfl::maths::Random(seed) >> 16;
    uint32_t total = 0;
    CSoundData* sounds = mEvent->mSounds.get();
    CSoundData* sound = sounds;
    for (uint32_t left = mEvent->mSoundCount; left != 0; --left) {
        total += sound->mF1E;
        if (total >= pick) {
            mSound = sound;
            const int8_t index = static_cast<int8_t>(sound - mEvent->mSounds.get());
            GetNextSound(rotate, index, mSound);
            InitParamatisedControls();
            return true;
        }
        ++sound;
    }
    mSound = sounds;
    return true;
}

void CAudioPlaceHolder::GetNextSound(bool rotate, int8_t index, CSoundData*& sound) {
    if (rotate) {
        const uint32_t count = mEvent->mSoundCount;
        if (count > 2 && index == mLastSound) {
            index = static_cast<int8_t>(index + 1);
            const uint32_t next = static_cast<uint32_t>(static_cast<int32_t>(index));   // extsb, then an UNSIGNED division
            index = static_cast<int8_t>(next - (next / count) * count);
            sound = mEvent->mSounds.get() + static_cast<int32_t>(index);
        }
    }
    mLastSound = index;
}

void CAudioPlaceHolder::SetVolume(uint32_t volume) {
    mVolume = static_cast<uint16_t>(volume);
    mFlags |= kVolumeSet;
}

// The volume is scaled by min(127, the sound's minimum volume) / 127; a frequency of exactly 1.0 means pitch 0.
void CAudioPlaceHolder::SetVolumeAndFrequency(uint32_t volume, float frequency) {
    const float minimum = static_cast<float>(VolumeMin(*mSound));
    const float limit = 127.0f;
    const float& smaller = limit < minimum ? limit : minimum;
    const float scaled = (static_cast<float>(volume) * smaller) / 127.0f;
    mFlags |= kVolumeSet;
    mVolume = static_cast<uint16_t>(CvtDblUl(static_cast<double>(scaled)));
    mFlags |= kPitchSet;
    if (frequency == 1.0f) {
        mPitch = 0;
    } else {
        mPitch = static_cast<int16_t>(ConvertFrequencyToPitchIndex(frequency));
    }
}

// 96 pitch steps per octave, rounded half away from zero: the sound library's own local static invln2 (1/ln 2, set at the first
// call that gets past the test for 1.0) times the natural log (std::logf is the double log rounded to single).
int32_t CAudioPlaceHolder::ConvertFrequencyToPitchIndex(float frequency) {
    static float invln2;
    static bool invln2Set = false;
    if (frequency == 1.0f) return 0;
    if (!invln2Set) {
        invln2 = 1.4426950216293335f;   // 0x1003327c
        invln2Set = true;
    }
    if (frequency < 1.0f) {   // bge: a NaN takes the other branch
        const float inverse = 1.0f / frequency;
        const float logarithm = static_cast<float>(std::log(static_cast<double>(inverse)));
        float steps = invln2 * logarithm;
        steps = steps * 96.0f;
        steps = steps - 1.0f;
        steps = ppc_fneg(steps);
        steps = steps - 0.5f;
        return ppc_fctiwz(static_cast<double>(steps));
    }
    const float logarithm = static_cast<float>(std::log(static_cast<double>(frequency)));
    float steps = invln2 * logarithm;
    steps = steps * 96.0f;
    steps = steps - 1.0f;
    steps = 0.5f + steps;
    return ppc_fctiwz(static_cast<double>(steps));
}

void CAudioPlaceHolder::SetVolumeAndPitch(uint32_t volume, int32_t pitch) {
    mPitch = static_cast<int16_t>(pitch);
    mVolume = static_cast<uint16_t>(volume);
    mFlags |= kVolumeSet | kPitchSet;
}

// In range when the squared distance (ChangeDistPriority, virtual) does not exceed the squared maximum distance, or the placeholder
// is non-positional. In range, a fading placeholder's fade goes up by 0.1 (to at most 1.0, a NaN stays), the priority is sent
// and a complete fade is switched off. Out of range, SetOutOfRange.
bool CAudioPlaceHolder::ProcessPositionAndPriority() {
    if ((mFlags & kBoundingRectangle) != 0) ModifyPositionForBoundingRectangle();
    const float maxDistance = GetMaxDistance();
    const float squared = ChangeDistPriority();
    const float limit = maxDistance * maxDistance;
    if (static_cast<double>(squared) > static_cast<double>(limit) && (mFlags & kNonPositional) == 0) {
        SetOutOfRange();
        return false;
    }
    mFlags &= ~kOutOfRange;
    if ((mFlags & kFading) != 0) {
        mFade = mFade + 0.1f;
        if (mFade >= 1.0f) mFade = 1.0f;
    }
    SetPriorityAndVolume(mPriority);
    if ((mFlags & kFading) != 0 && mFade == 1.0f) mFlags &= ~kFading;
    return true;
}

void CAudioPlaceHolder::InitFade() {
    if ((mFlags & kFresh) == 0) {
        mFlags |= kFading;
        mFade = 0.0f;
    }
    mFlags &= ~kFresh;
}

void CAudioPlaceHolder::SetOutOfRange() {
    mFlags |= kOutOfRange;
    if (mHandle != nullptr) {
        SystemCommand::Stop command(mHandle);
        mpSoundManager->SystemCommand(command);
        mHandle = nullptr;
    }
    mFlags &= ~kPlaying;
}

// ---- stop, pause, restart ------------------------------------------------------------------------------------------------

TbError CAudioPlaceHolder::Stop() {
    if (mHandle != nullptr) {
        SystemCommand::Stop command(mHandle);
        mpSoundManager->SystemCommand(command);
    }
    mFlags |= kFinished;
    mFlags &= ~kPlaying;
    mHandle = nullptr;
    return TbError(0);
}

TbError CAudioPlaceHolder::Pause() {
    if (mHandle != nullptr) {
        SystemCommand::Pause command(mHandle);
        mpSoundManager->SystemCommand(command);
    }
    if (mPauseTime == 0) {
        mFlags |= kPaused;
        mPauseTime = LbTime_GetClock();
    }
    return TbError(0);
}

// The end time moves by the time spent paused. The paused flag and the pause time stay (the pause time is also the pseudo time
// of FillSampleInfo, which FinishedPlaying adds to the end time).
TbError CAudioPlaceHolder::Restart() {
    if (mHandle != nullptr) {
        SystemCommand::Restart command(mHandle);
        mpSoundManager->SystemCommand(command);
    }
    if ((mFlags & kPaused) != 0) {
        mEndTime = mEndTime + (LbTime_GetClock() - mPauseTime);
    }
    return TbError(0);
}

// A non-looping placeholder whose end time (plus the pause/pseudo time) is past: playing cleared, finished answered.
bool CAudioPlaceHolder::FinishedPlaying(uint32_t now) {
    if ((mFlags & kLooping) == 0 && now > mEndTime + mPauseTime) {
        mFlags &= ~kPlaying;
        return true;
    }
    return false;
}

TbError CAudioPlaceHolder::Save(void* file, uint32_t reserved) {
    const uint32_t classId = GetClassID();
    if (classId == 0) return TbError(-1);
    WriteWord<uint32_t>(file, classId);
    float x, y, z;
    mPosition.Get(x, y, z);
    CMapReference reference;
    int32_t category, event, sound, sample;
    reference.GetSoundIds(category, event, sound, sample, this);
    if (category == 0) return TbError(-1);
    WriteMagicHeader(file);
    WriteWord<float>(file, x);
    WriteWord<float>(file, y);
    WriteWord<float>(file, z);
    const float velocityWord = 0.0f;
    WriteBytes(file, &velocityWord, 4);   // unswapped
    WriteWord<float>(file, mMaxDistance);
    WriteWord<int32_t>(file, category);
    WriteWord<int32_t>(file, event);
    WriteWord<int32_t>(file, sound);
    WriteWord<int32_t>(file, sample);
    WriteMagicHeader(file);
    if (mExtra == nullptr) {
        const uint32_t none = 0;
        WriteBytes(file, &none, 4);
    } else {
        WriteWord<uint32_t>(file, 1);
        WriteBytes(file, mExtra->mParamId, 4);
        WriteBytes(file, mExtra->mParamValue, 4);
        WriteWord<int32_t>(file, mExtra->mBounds.top);
        WriteWord<int32_t>(file, mExtra->mBounds.left);
        WriteWord<int32_t>(file, mExtra->mBounds.right);
        WriteWord<int32_t>(file, mExtra->mBounds.bottom);
    }
    WriteMagicHeader(file);
    const uint8_t bounded = (mFlags & kBoundingRectangle) != 0 ? 1 : 0;
    WriteBytes(file, &bounded, 1);
    WriteWord<uint32_t>(file, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mListHandle)));
    if (mExtra != nullptr && mExtra->mChild != nullptr) {
        WriteWord<uint32_t>(file, mExtra->mChild->GetClassID());
        mExtra->mChild->Save(file, reserved);
    } else {
        const uint32_t none = 0;
        WriteBytes(file, &none, 4);
    }
    return TbError(0);
}

// ---- save and load -------------------------------------------------------------------------------------------------------

void CAudioPlaceHolder::WriteMagicHeader(void* file) {
    WriteWord<uint32_t>(file, kMagic);
}

uint32_t CAudioPlaceHolder::GetClassID() {
    return 0;
}

TbError CAudioPlaceHolder::Load(void* file, uint32_t reserved) {
    bool unresolved = false;
    if (!ReadMagicHeader(file)) return TbError(-1);
    float x, y, z;
    ReadBytes(file, &x, 4);
    ReadBytes(file, &y, 4);
    ReadBytes(file, &z, 4);
    x = ::SamsUtilities::USwap32<float>(x);
    y = ::SamsUtilities::USwap32<float>(y);
    z = ::SamsUtilities::USwap32<float>(z);
    mPosition.Set(x, y, z);
    uint32_t velocityWord;   // the saved velocity (always 0.0f) is read and dropped
    ReadBytes(file, &velocityWord, 4);
    ReadWord(file, mMaxDistance);
    int32_t category, event, sound, sample;
    ReadWord(file, category);
    ReadWord(file, event);
    ReadWord(file, sound);
    ReadWord(file, sample);
    if (!ReadMagicHeader(file)) return TbError(-1);
    uint32_t hasExtra;
    ReadWord(file, hasExtra);
    if (hasExtra != 0) {
        AllocateExtraData();
        ReadBytes(file, mExtra->mParamId, 4);
        ReadBytes(file, mExtra->mParamValue, 4);
        ReadWord(file, mExtra->mBounds.top);
        ReadWord(file, mExtra->mBounds.left);
        ReadWord(file, mExtra->mBounds.right);
        ReadWord(file, mExtra->mBounds.bottom);
    }
    if (!ReadMagicHeader(file)) return TbError(-1);
    uint8_t bounded;
    ReadBytes(file, &bounded, 1);
    int32_t savedHandle;   // read, swapped and dropped: the list gives the placeholder its handle
    ReadWord(file, savedHandle);
    uint32_t childClass;
    ReadWord(file, childClass);
    if (childClass != 0) {
        AllocateExtraData();
        // the class id waits in the child slot until the child is made, as in the original
        mExtra->mChild = reinterpret_cast<CAudioPlaceHolder*>(static_cast<uintptr_t>(childClass));
    }
    mHandle = nullptr;
    const uint32_t fixedCategory = TbMapStreamer::Instance()->FixUpCatagoryIndex(static_cast<uint32_t>(category));
    CCatagoryContainer* container = nullptr;
    if (fixedCategory == 0) {
        unresolved = true;
    } else {
        CMapReference reference;
        container = reference.GetSoundPointer(fixedCategory, this);
        if (container == nullptr) return TbError(-1);
        if (container->mData == nullptr) return TbError(-1);
        mEvent = container->mData->mEvents.get() + event;
        mSound = mEvent->mSounds.get() + sound;
        mSample = mSound->mSamples.get() + sample;
    }
    if (mExtra != nullptr && mExtra->mChild != nullptr) {
        const uint32_t savedClass = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mExtra->mChild));
        mExtra->mChild = nullptr;
        CAudioPlaceHolder* parent = this;
        if (GetClassID() != 6 && GetClassID() != 4) parent = mExtra->mParent;
        if (savedClass == 7) {
            mExtra->mChild = new CPlaceHolderOneShotSentenceElement(parent);
        } else if (savedClass == 9) {
            mExtra->mChild = new CPlaceHolderOneShotBranchingSentenceElement(parent);
        }
        uint32_t childOwnClass;   // the child's own class id, read again and dropped (its Save writes it first)
        ReadWord(file, childOwnClass);
        mExtra->mChild->Load(file, reserved);
    }
    if (unresolved) {
        mFlags |= kFinished;
    } else {
        Init(container, mEvent, mPosition, mMaxDistance);
        if (bounded != 0) mFlags |= kBoundingRectangle;
    }
    return TbError(0);
}
// The word is read into an uninitialised local on the Mac (a short or failed read compares stale stack); zero here.
bool CAudioPlaceHolder::ReadMagicHeader(void* file) {
    uint32_t word = 0;
    ReadWord(file, word);
    return word == kMagic;
}

// The position follows the listener, clamped into the rectangle: x between left and right, z between bottom and top (the top is
// the high z). The branches are bge and ble on an ordered compare, so a NaN passes through unchanged. A playing sample is told.
void CAudioPlaceHolder::ModifyPositionForBoundingRectangle() {
    const TbSoundPosition* listener = mpSoundManager->GetListenerPosition();
    float x, y, z;
    listener->Get(x, y, z);
    const float left = static_cast<float>(mExtra->mBounds.left);
    if (x < left) {
        x = left;
    } else {
        const float right = static_cast<float>(mExtra->mBounds.right);
        if (x > right) x = right;
    }
    const float top = static_cast<float>(mExtra->mBounds.top);
    if (z > top) {
        z = top;
    } else {
        const float bottom = static_cast<float>(mExtra->mBounds.bottom);
        if (z < bottom) z = bottom;
    }
    mPosition.Set(x, y, z);
    if (mHandle != nullptr) {
        TbSoundSampleInfo info;
        info.SetPosition(mPosition);
        info.SetVelocity(mVelocity);
        info.mField0C |= kInfoPositional;
        SystemCommand::SetSampleInfo command(mHandle, info);
        mpSoundManager->SystemCommand(command);
    }
}

TbError CAudioPlaceHolder::PlaySample(uint32_t) {
    return TbError(-1);
}

namespace SamsUtilities::UBinary {
// RotateRight<long> 0x10018d6c (20): sraw | slw (shift counts use six bits: from 32 up, sraw fills with the sign and slw gives 0).
template <>
int32_t RotateRight<int32_t>(int32_t value, int32_t amount) {
    const uint32_t right = static_cast<uint32_t>(amount) & 0x3f;
    const uint32_t left = (0x20u - static_cast<uint32_t>(amount)) & 0x3f;
    const int32_t a = right > 31 ? (value < 0 ? -1 : 0) : (value >> right);
    const uint32_t b = left > 31 ? 0u : (static_cast<uint32_t>(value) << left);
    return static_cast<int32_t>(static_cast<uint32_t>(a) | b);
}
}  // namespace SamsUtilities::UBinary

extern "C" __attribute__((weak)) int32_t MslSeedGlue1002eaac() { return static_cast<int32_t>(LbTime_GetClock()); }

void __sinit_AudioPlaceHolder_cpp() { CAudioPlaceHolder::mRandomSeed = MslSeedGlue1002eaac(); }
