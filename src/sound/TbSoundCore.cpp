// TbSoundCore.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbSoundCore code, named after it.

#include "CCodecFactory.h"
#include "CSprocketsModule.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "snd_commands.h"
#include "snd_handles.h"
#include "TbAudioBody.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundCore.h"
#include "TbSoundInfo.h"
#include "TbSoundManager.h"
#include "TbSoundPipe.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleStore.h"
#include "TbSoundSystemModule.h"
#include <cmath>
#include <cstring>
#include <new>

// TbSoundCore.cpp: the sound library's core (unit w79b, sound symbols shared). See include/TbSoundCore.h for the layouts, the
// vtables and the calling convention. The sample store (unit w79c), the device module (CSprocketsModule, unit p-sound) and the
// command bodies (unit w79d) are other units'.

// ---------------------------------------------------------------- TbSoundCore

TbSoundCore::TbSoundCore() : CAudioComponent(), mModule(nullptr), mStore(nullptr), mCopyInit() {}

CMemoryCopyInit::~CMemoryCopyInit() {
    ::operator delete(CMemoryCopy::_mpCopy);
}

// Close first, then the members and the base in the reverse order of the constructor.
TbSoundCore::~TbSoundCore() {
    Close();
}

void TbSoundCore::ModifyMaxBuffers(TbSoundSystemInfo& info) {
    info.SetMaxBuffers(info.mBuffersToUse);
}

TbError TbSoundCore::Init(const TbSoundSystemInfo& info) {
    TbSoundSystemInfo copy(info);
    ModifyMaxBuffers(copy);
    mStore = new TbSoundSampleStore(this, copy.mMaxBuffers);
    TbError result = mStore->Init(copy.mBuffersToUse);
    if (static_cast<int32_t>(result) >= 0) {
        CModuleFactory factory;
        TbError made = factory.GenerateModule(mModule, copy, mStore);
        if (static_cast<int32_t>(made) >= 0) {
            SystemCommand::SetSystemInfo command(copy);
            return SystemCommand(command);
        }
    }
    return TbError(-1);
}

// The system command constructors of the sound library (unit w79b, sound symbols shared): each stores its vptr (the Mac
// vtable addresses are in include/snd_commands.h) and at most two words. The Execute bodies are unit w79d's.

namespace SystemCommand {

SetSystemInfo::SetSystemInfo(TbSoundSystemInfo& info) : Command(), mInfo(&info) {}

Command::Command() {}

SetSampleInfo::SetSampleInfo(void* handle, TbSoundSampleInfo& info) : HandleCommand(handle), mInfo(&info) {}

HandleCommand::HandleCommand(void* handle) : Command(), mHandle(handle) {}

}  // namespace SystemCommand

// The sound library's value classes (unit w77, program "sound symbols shared", Code at 0x10000000): TbSoundPosition,
// TbSoundDistance, TbSoundReverbInfo, TbSoundVolumeControl, TbSoundSampleInfo and TbSoundSystemInfo. Declared in
// include/TbSoundPosition.h and include/TbSoundInfo.h. TbSoundSampleInfo::SetBank (0x1000df50) is in src/TbSoundBank.cpp.

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

void TbSoundCore::Close() {
    if (mModule != nullptr) mModule->StopAll();
    if (mStore != nullptr) {
        mStore->ReleaseAllPipes();
        delete mStore;
        mStore = nullptr;
    }
    if (mModule != nullptr) {
        mModule->Close();
        if (mModule != nullptr) delete mModule;
        mModule = nullptr;
    }
}

TbError TbSoundSystemModule::StopAll() {
    return TbError(-1);
}

// The request words are mField4 (0x100: environmental effects) and mField8 (0x80 max channels, 0x40 channels in use, 0x200
// channels playing); the answers set the same bits of the valid mask mMask.
TbError TbSoundCore::GetSystemInfo(TbSoundSystemInfo& info) {
    if ((info.mField4 & 0x100) != 0) {
        if (mModule != nullptr && !mModule->SupportsEnvironmentalEffects()) info.mField4 &= ~0x100u;
    }
    if ((info.mField8 & 0x80) != 0) {
        if (mStore != nullptr) {
            info.mMaxBuffers = mStore->GetMaxNumberOfChannels();
            info.mMask |= 0x80;
        }
    }
    if ((info.mField8 & 0x40) != 0) {
        if (mStore != nullptr) {
            info.mBuffersToUse = mStore->GetNumberOfUsedChannels();
            info.mMask |= 0x40;
        }
    }
    if ((info.mField8 & 0x200) != 0) {
        info.mSamplesPlaying = static_cast<uint32_t>(CChannelState::Instance()->GetNumberOfChannelsInUse());
        info.mMask |= 0x200;
    }
    return TbError(0);
}

int32_t CChannelState::GetNumberOfChannelsInUse() { return mCount; }

// The sound library's sample store (unit w79c, program "sound symbols shared"): TbSoundSampleStore with its two channel
// iterators (TbSoundSampleWAV.cpp and TbSoundSampleCallback.cpp, AudioPlaceHolder.cpp hold the out-of-line copies). The
// store's array of channels is the CAutoArrayPtr<TbSoundPipeChannel> instance (0x1000fb94, 0x1000faec, 0x1000fd90), which is
// include/snd_containers.h's template. See include/TbSoundSampleStore.h for the layout.

uint32_t TbSoundSampleStore::GetNumberOfUsedChannels() {
    return mChannelsToUse;
}

uint32_t TbSoundSampleStore::GetMaxNumberOfChannels() {
    return mMaxChannels;
}

bool TbSoundSystemModule::SupportsEnvironmentalEffects() {
    return false;
}

TbError TbSoundCore::StopAndReleasePipe(TbSoundPipeChannel* channel, bool release) {
    if (channel != nullptr && mModule != nullptr) {
        TbError result = mModule->StopChannel(channel->GetIndex());
        if (static_cast<int32_t>(result) >= 0) {
            channel->ReleasePipe(release);
            return TbError(0);
        }
    }
    return TbError(-1);
}

// The sample info's words +0x0c and +0x18 are requests (mField0C 0x100: existence only; mField18 0x02 volume, 0x400 priority).
TbError TbSoundCore::GetSampleInfo(void* handle, TbSoundSampleInfo& info) {
    if (handle != nullptr) {
        CUniqueHandle unique(handle);
        TbSoundPipeChannel* channel = (*mStore)[unique];
        if (channel != nullptr) {
            if ((info.mField0C & 0x100) != 0) return TbError(channel != nullptr ? 0 : -1);
            if ((info.mField18 & 2) != 0) info.SetVolume(channel->GetActualVolume());
            if ((info.mField18 & 0x400) != 0) info.SetPriority(channel->GetPriority());
            return TbError(0);
        }
    }
    return TbError(-1);
}

// The sound library's pipe channel (unit w79c, program "sound symbols shared"): TbSoundPipeChannel, the head of a sample's pipe
// chain (TbSoundCodecLoop.cpp, LbMpeg.cpp, TbSoundSampleCallback.cpp, TbSoundSampleWAV.cpp, TimerCodec.cpp and
// CSprocketsModule.cpp hold its out-of-line copies). See include/TbSoundPipeChannel.h for the layout and the slot order.

uint32_t TbSoundPipeChannel::GetPriority() {
    return static_cast<uint32_t>(mSampleHeader.mField8);
}

uint32_t TbSoundPipeChannel::GetActualVolume() const {
    return mVolume;
}

void* TbSoundCore::NewSample(void* handle, TbSoundSampleInfo& info) {
    uint32_t extra; // the store fills it in; not read (uninitialised on the Mac too)
    void* sample = mStore->CreateNewSample(handle, info, extra);
    if (sample != nullptr) {
        SystemCommand::SetSampleInfo command(sample, info);
        SystemCommand(command);
    }
    return sample;
}

void* TbSoundCore::NewControl(void* handle, TbSoundSampleInfo& info) {
    return mStore->CreateNewControl(handle, info);
}

TbError TbSoundCore::SystemCommand(const SystemCommand::Command& command) {
    return command.Execute(this);
}

TbSoundSystemModule* TbSoundCore::GetSystemModule() const { return mModule; }
TbSoundSampleStore* TbSoundCore::GetSampleStore() const { return mStore; }

// Unit v51113: the small sound-library functions at the edges of CSprocketsModule.cpp (sound 0x10023310 to 0x1002e8fc) and the
// float square roots / cleanup objects of the codec files.

using namespace SamsUtilities;

_CS__TbSoundCodecLoop* _CS__TbSoundCodecLoop::Destruct(_CS__TbSoundCodecLoop* self, int16_t deleteFlag) {   // 0x1000b590
    if (self != nullptr) {
        _TbSoundCodecLoop::ClearCache();
        if (deleteFlag > 0) USoundChannel_Free(self);
    }
    return self;
}

// ---- 0x1000c70c and 0x1000c6cc: the runtime's double sqrt (glue 0x1002ecec), rounded to float
namespace tbstd {
float sqrtf(float x) { return static_cast<float>(::sqrt(static_cast<double>(x))); }
float sqrtOfFloat(float x) { return sqrtf(x); }
}  // namespace tbstd
