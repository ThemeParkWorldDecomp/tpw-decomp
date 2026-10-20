// TbSoundCore.cpp: the sound library's core (unit w79b, sound symbols shared). See include/TbSoundCore.h for the layouts, the
// vtables and the calling convention. The sample store (unit w79c), the device module (CSprocketsModule, unit p-sound) and the
// command bodies (unit w79d) are other units'.
#include "TbSoundCore.h"
#include "TbSoundCodecs.h"

#include <cstring>
#include <new>

#include "CSprocketsModule.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleStore.h"
#include "TbSoundSystemModule.h"

// ---------------------------------------------------------------- the copier

CMemoryCopy::CCopy* CMemoryCopy::_mpCopy = nullptr; // 0x1003f34c

CMemoryCopy::CCopy::CCopy() {}

CMemoryCopy::CStandardCopy::CStandardCopy() : CCopy() {}

void CMemoryCopy::CStandardCopy::Copy(void* dest, const void* source, uint32_t count) {
    std::memcpy(dest, source, count);
}

CMemoryCopyInit::CMemoryCopyInit() {
    CMemoryCopy::_mpCopy = new CMemoryCopy::CStandardCopy();
}

CMemoryCopyInit::~CMemoryCopyInit() {
    ::operator delete(CMemoryCopy::_mpCopy);
}

// ---------------------------------------------------------------- CModuleFactory

TbError CModuleFactory::GenerateModule(TbSoundSystemModule*& module, const TbSoundSystemInfo& info, TbSoundSampleStore*) {
    module = new CSprocketsModule();
    TbError result = module->Init(info);
    if (static_cast<int32_t>(result) >= 0) return TbError(0);
    CloseModule(module);
    return TbError(-1);
}

void CModuleFactory::CloseModule(TbSoundSystemModule*& module) {
    if (module != nullptr) {
        module->Close();
        if (module != nullptr) delete module;
        module = nullptr;
    }
}

// ---------------------------------------------------------------- TbSoundCore

TbSoundCore::TbSoundCore() : CAudioComponent(), mModule(nullptr), mStore(nullptr), mCopyInit() {}

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
