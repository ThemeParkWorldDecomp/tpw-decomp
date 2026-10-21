// SystemCommand.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of SystemCommand code, named after it.

#include "platform/samsutils.h"
#include "ppc_math.h"
#include "snd_commands.h"
#include "snd_handles.h"
#include "TbSoundCore.h"
#include "TbSoundInfo.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleStore.h"
#include "TbSoundSystemModule.h"
#include <cmath>
#include <cstring>
#include <new>

CAtomicValue::CAtomicValue() : mValue(0) {}

// snd_commands_execute.cpp: the Execute bodies of the sound library's SystemCommand classes (unit w79d, "sound symbols shared",
// Code at 0x10000000, TOC r2 = 0x1003abf0; file AudioPlaceHolder.cpp in the original). TbSoundCore::SystemCommand runs a command
// with the core as the CAudioComponentAccess; each command fetches the device module (slot +0x1c) and the sample store (slot
// +0x20) from it, in the order the listing does, and drives them. The constructors are unit w79b's (src/snd_commands.cpp).
//
// Result conventions (a TbError comes back through the hidden r3 buffer on the Mac): Pause, Restart, PauseAll, RestartAll,
// Process (Process answers 0 itself), StopLoop and Stop hand the callee's TbError out unchanged; a command whose handle does
// not name a channel answers -1, except Stop, which answers 0. A device-module call whose result the listing never reads
// (SetOptions, Set3DDSPFunctionality, SetChannelInfo, the listener setters, SetChannelVolume) has the result dropped.

namespace SystemCommand {

// Start::Execute's statics, 0x1003f354 and 0x1003f355 (see snd_commands.h).
bool sStartInitialised = false;
bool sStartInProgress = false;

// 0x10014e00 (184 bytes)
TbError Pause::Execute(CAudioComponentAccess* access) const {
    CUniqueHandle handle(mHandle);
    uint32_t index;
    if (!access->GetSampleStore()->GetValidChannelIndexFromHandle(handle, index)) return TbError(-1);
    return access->GetSystemModule()->PauseChannel(index);
}

// 0x10014f08 (84 bytes)
TbError PauseAll::Execute(CAudioComponentAccess* access) const {
    return access->GetSystemModule()->PauseAll();
}

// 0x10014fac (184 bytes)
TbError Restart::Execute(CAudioComponentAccess* access) const {
    CUniqueHandle handle(mHandle);
    uint32_t index;
    if (!access->GetSampleStore()->GetValidChannelIndexFromHandle(handle, index)) return TbError(-1);
    return access->GetSystemModule()->RestartChannel(index);
}

// 0x100150b4 (84 bytes)
TbError RestartAll::Execute(CAudioComponentAccess* access) const {
    return access->GetSystemModule()->RestartAll();
}

// 0x1001515c (180 bytes): a NULL handle answers -1 like an invalid one. The answer is SetChannelInfo's (0).
TbError SetSampleInfo::Execute(CAudioComponentAccess* access) const {
    if (mHandle != nullptr) {
        CUniqueHandle handle(mHandle);
        uint32_t index;
        if (access->GetSampleStore()->GetValidChannelIndexFromHandle(handle, index)) return SetChannelInfo(access, index);
    }
    return TbError(-1);
}

// 0x10015268 (432 bytes). Every test of the info re-reads it through mInfo, as the listing does; the module's answers are
// dropped.
TbError SetSampleInfo::SetChannelInfo(CAudioComponentAccess* access, uint32_t index) const {
    uint32_t bits = mInfo->mField0C;
    if (bits != 0) {
        if ((bits & 2) != 0 || (bits & 8) != 0 || (bits & 0x10) != 0) {
            access->GetSystemModule()->Set3DDSPFunctionality(index, bits);
        }
    }
    TbSoundPipeChannel* channel = (*access->GetSampleStore())[index];
    if ((mInfo->mField0C & 0x4000) != 0 && channel != nullptr) channel->SetInfo(*mInfo);
    uint32_t flags = mInfo->mFlags;
    if (flags != 0) {
        if ((flags & 0x400) != 0) {
            if (channel != nullptr) channel->SetPriority(mInfo->mPriority);
            mInfo->mFlags = mInfo->mFlags & ~0x400u;
        }
        if ((mInfo->mFlags & 2) != 0 && channel != nullptr) {
            channel->SetVolume(mInfo->mVolume);
            mInfo->mVolume = channel->GetVolume();
        }
        TbSoundSampleInfo* info = mInfo;
        if (info->mFlags != 0) {
            access->GetSystemModule()->SetChannelInfo(index, channel, *info);   // read through operator long, then dropped
        }
    }
    return TbError(0);
}

// 0x100154e4 (336 bytes): not re-entrant. A second Start while one runs logs and answers -1 (and leaves the guard set). The
// guard is cleared on every other way out. A device refusal (a negative answer) releases the channel's pipe, flag false.
TbError Start::Execute(CAudioComponentAccess* access) const {
    if (!sStartInitialised) {
        sStartInProgress = false;
        sStartInitialised = true;
    }
    if (sStartInProgress) {
        SamsUtilities::UDebug("trying to renter start!");   // the string at 0x1002f862
        return TbError(-1);
    }
    sStartInProgress = true;
    if (mHandle != nullptr) {
        CUniqueHandle handle(mHandle);
        TbSoundPipeChannel* channel = (*access->GetSampleStore())[handle];
        if (channel != nullptr) {
            TbError result = access->GetSystemModule()->StreamData(channel);
            if (static_cast<int32_t>(result) >= 0) {
                sStartInProgress = false;
                return TbError(0);
            }
            channel->ReleasePipe(false);
        }
    }
    sStartInProgress = false;
    return TbError(-1);
}

// 0x10015684 (188 bytes): an invalid handle answers 0, not -1. The core is the access object (the CAudioComponentAccess base
// sits at offset 0 of TbSoundCore), so the downcast is a static one; the bool is the constant false.
TbError Stop::Execute(CAudioComponentAccess* access) const {
    CUniqueHandle handle(mHandle);
    uint32_t index;
    if (!access->GetSampleStore()->GetValidChannelIndexFromHandle(handle, index)) return TbError(0);
    TbSoundPipeChannel* channel = (*access->GetSampleStore())[index];
    return static_cast<TbSoundCore*>(access)->StopAndReleasePipe(channel, false);
}

// 0x1001578c (224 bytes): the info that goes to the channel has only the loop count (1, flag 0x40) set.
TbError StopLoop::Execute(CAudioComponentAccess* access) const {
    CUniqueHandle handle(mHandle);
    uint32_t index;
    if (access->GetSampleStore()->GetValidChannelIndexFromHandle(handle, index)) {
        TbSoundPipeChannel* channel = (*access->GetSampleStore())[index];
        if (channel != nullptr) {
            TbSoundSampleInfo info;
            info.SetLoopCount(1);
            return channel->SetInfo(info);
        }
    }
    return TbError(-1);
}

// 0x10015980 (84 bytes)
TbError StopAll::Execute(CAudioComponentAccess* access) const {
    access->GetSampleStore()->StopAllChannels(false);
    return TbError(0);
}

// 0x10015a24 (96 bytes): the word at +4 is a mask of volume controls (named mIndex in snd_commands.h).
TbError StopAllVolumeControl::Execute(CAudioComponentAccess* access) const {
    access->GetSampleStore()->StopAllChannelsWithVolumeControl(mIndex);
    return TbError(0);
}

// 0x10015ae4 (88 bytes)
TbError Process::Execute(CAudioComponentAccess* access) const {
    access->GetSystemModule()->Process();
    return TbError(0);
}

// 0x10015b8c (656 bytes). The mask is fetched through GetMask (the address of the word) before every test, as the listing
// does. The front direction is normalised in single precision: z*z + (x*x + y*y), std::sqrt(float), fdivs 1.0f / length, and
// each component multiplied by that (a zero vector divides by zero: infinity, then NaN components). Each module answer is
// dropped; the answer is 0.
TbError SetSystemInfo::Execute(CAudioComponentAccess* access) const {
    TbSoundSystemInfo* info = mInfo;
    uint32_t options = info->mField4;
    if (options != 0) access->GetSystemModule()->SetOptions(options);
    if (*info->GetMask() != 0) {
        if ((*info->GetMask() & 0x40) != 0) access->GetSampleStore()->SetNumberOfChannelsToUse(*info);
        if ((*info->GetMask() & 4) != 0) {
            const TbSoundPosition* position = info->GetListenerPosition();
            access->GetSystemModule()->SetListenerPosition(*position);
        }
        if ((*info->GetMask() & 8) != 0) {
            float x;
            float y;
            float z;
            info->GetFrontDir()->Get(x, y, z);
            const float xx = x * x;
            const float yy = y * y;
            const float zz = z * z;
            const float xy = xx + yy;
            const float sum = zz + xy;
            const float length = static_cast<float>(std::sqrt(static_cast<double>(sum)));
            const float inverse = 1.0f / length;
            TbSoundPosition direction(*info->GetFrontDir());
            direction *= inverse;
            const TbSoundPosition* top = info->GetTopDir();
            access->GetSystemModule()->SetListenerDirection(direction, *top);
        }
        if ((*info->GetMask() & 0x10) != 0) {
            const TbSoundPosition* velocity = info->GetListenerVelocity();
            access->GetSystemModule()->SetListenerVelocity(*velocity);
        }
        if ((*info->GetMask() & 0x20) != 0) {
            const TbSpeaker::Config* speakers = info->GetSpeakerConfig();
            access->GetSystemModule()->SetSpeakerConfig(*speakers);
        }
    }
    return TbError(0);
}

// 0x10016050 (244 bytes): every channel up to the used count gets its controlled volume pushed to the device. One info is
// built for the whole walk, so its volume flag stays set; the module is fetched anew in each pass.
TbError UpdateAllVolumes::Execute(CAudioComponentAccess* access) const {
    TbSoundSampleStore::CChannelIterator iterator(access->GetSampleStore());
    TbSoundPipeChannel* channel = iterator;
    TbSoundSampleInfo info;
    while (channel != nullptr) {
        info.SetVolume(channel->GetVolume());
        uint32_t index = channel->GetIndex();
        access->GetSystemModule()->SetChannelVolume(index, info);
        channel = ++iterator;
    }
    return TbError(0);
}

}  // namespace SystemCommand

// The sound library's value classes (unit w77, program "sound symbols shared", Code at 0x10000000): TbSoundPosition,
// TbSoundDistance, TbSoundReverbInfo, TbSoundVolumeControl, TbSoundSampleInfo and TbSoundSystemInfo. Declared in
// include/TbSoundPosition.h and include/TbSoundInfo.h. TbSoundSampleInfo::SetBank (0x1000df50) is in src/TbSoundBank.cpp.

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

// 0x10015f3c (28 bytes)
void TbSoundPosition::Get(float& x, float& y, float& z) const {
    x = mX;
    y = mY;
    z = mZ;
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
