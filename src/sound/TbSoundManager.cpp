// TbSoundManager.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbSoundManager code, named after it.

#include "bfl_FileSystem.h"
#include "ppc_math.h"
#include "snd_commands.h"
#include "TbDynamicStringTemplate.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundInfo.h"
#include "TbSoundManager.h"
#include <cmath>
#include <cstddef>
#include <cstring>
#include <new>

// Sound map registration and in-place readers, sound library 0x1001f6ec..0x100237a4.

CEventData* TbEventMapper::MapEvent(CCatagoryData* category, const sound::TbAudioEvent& event) {
    const uint32_t type = event.GetEvent();
    const uint32_t index = event.GetCategory();
    if (index != 0 && type != 0 && category != nullptr) {
        for (uint32_t i = 0; i < category->mEventCount; ++i) {
            CEventData& data = category->mEvents[i];
            if (data.mID == type) return data.mSoundCount == 0 ? nullptr : &data;
        }
    }
    return nullptr;
}

// TbSoundManager.cpp: the sound manager the game holds (unit w79e, sound symbols shared, file CSprocketsModule.cpp and
// AudioPlaceHolder.cpp in the library). See include/TbSoundManager.h for the layout, the vtable and the calling convention. The
// commands' Execute bodies (unit w79d), the sample store, the device module (p-sound) and TbMapStreamer::SetDroppable
// (0x100231fc) are other units'.

TbSoundManager::TbSoundManager()
    : TbSoundCore(),
      mOptions(8),
      mDistanceMultiplier(1.0f),
      mSoundPath(),
      mListenerPosition(nullptr),
      mAudioSystem(nullptr),
      mVolumeControl(),
      mDroppable(0) {
    mVolumes[0] = 1.0f;
    mVolumes[1] = 1.0f;
    mVolumes[2] = 1.0f;
    mVolumes[3] = 1.0f;
    mControlOn[0] = 1;
    mControlOn[1] = 1;
    mControlOn[2] = 1;
    mControlOn[3] = 1;
    mVolumeControl.Init(4);
    mVolumeControl.SetVolumes(mVolumes);
}

// The sound library's value classes (unit w77, program "sound symbols shared", Code at 0x10000000): TbSoundPosition,
// TbSoundDistance, TbSoundReverbInfo, TbSoundVolumeControl, TbSoundSampleInfo and TbSoundSystemInfo. Declared in
// include/TbSoundPosition.h and include/TbSoundInfo.h. TbSoundSampleInfo::SetBank (0x1000df50) is in src/TbSoundBank.cpp.

// 0x10026f28 (8 bytes)
void TbSoundVolumeControl::SetVolumes(float* volumes) {
    mVolumes = volumes;
}

// 0x10026f68 (8 bytes)
void TbSoundVolumeControl::Init(uint32_t count) {
    mCount = count;
}

TbError TbSoundManager::Init(const char* path) {
    mSoundPath = path;
    return TbError(0);
}

TbError TbSoundManager::GetMaxNumberOfChannels(uint32_t& out) {
    TbSoundSystemInfo info;
    info.mField8 |= 0x80;
    TbError error = GetSystemInfo(info);
    if (static_cast<int32_t>(error) >= 0 && (*info.GetMask() & 0x80) != 0) {
        out = info.GetMaxBuffers();
        return TbError(0);
    }
    return TbError(-1);
}

// 0x10027120 (8 bytes)
uint32_t TbSoundSystemInfo::GetMaxBuffers() const {
    return mMaxBuffers;
}

TbError TbSoundManager::SetQSoundInfo(uint32_t enable, uint32_t extended, uint32_t third) {
    if (enable != 0) {
        mOptions |= 8;
    } else {
        mOptions &= ~8u;
    }
    TbSoundSystemInfo info;
    if (extended != 0) {
        mOptions |= 0x100;
        info.mField4 |= 4;
    }
    if (third != 0) {
        info.mField4 |= 8;
    }
    SystemCommand::SetSystemInfo command(info);
    return SystemCommand(command);
}

void TbSoundManager::SetDistanceMapping(float multiplier) {
    mDistanceMultiplier = multiplier;
}

bool TbSoundManager::IsBankDroppable(uint32_t index) {
    CBankContainer* data = GetBankData(index);
    if (data == nullptr) return false;
    return (data->mFlags & 0x40) != 0;
}

CBankContainer* TbSoundManager::GetBankData(uint32_t index) {
    CResize<CBankContainer*>& banks = TbMapStreamer::Instance()->GetBankDeque();
    if (index < banks.Size()) return banks[index];
    return nullptr;
}

void TbSoundManager::SetDroppable(bool on) {
    if (mDroppable != on) {
        mDroppable = on;
        TbMapStreamer::Instance()->SetDroppable(mDroppable);
    }
}

// The three branches of the listing differ only in the open mode, the extra argument and the bank type. A container with none
// of the flags 1, 2 or 4 opens nothing and answers its bank as it was (NULL here, since a cached bank returns at once). The
// original stores the factory's Bank::CBank* in the container's bank word, which w70's header types as a TbBank*.
TbBank* TbSoundManager::GetBank(uint32_t index) {
    CBankContainer* data = GetBankData(index);
    if (data == nullptr) return nullptr;
    if (data->mBank != nullptr) return data->mBank;
    TbDynamicStringTemplate<char> path;
    TbDynamicStringTemplate<char> name;
    {
        TbDynamicStringTemplate<char> empty("");
        data->mStorage.FullPath(name, empty);
    }
    path = data->mName;
    // The original is a member of an empty CFileOperation (this = a stack word in r3, then the storage, then the string that
    // gets "HD.sdt" appended, then the other string, then 0); w72's translation is static and appends to its third argument.
    if (CFileOperation::GetBankPath(data->mStorage, name, path, 0) == 0) return nullptr;
    bfl::FileSystem::CFile file;
    CBankFactory factory;
    const uint32_t flags = data->mFlags;
    uint32_t mode = 0;
    uint32_t extra = 0;
    uint32_t type = 0;
    bool open = true;
    if ((flags & 1) != 0) {
        mode = 0x81000001;
        type = 2;
    } else if ((flags & 2) != 0) {
        mode = 0x80008001;
        extra = 2;
        type = 1;
    } else if ((flags & 4) != 0) {
        mode = 0x81000001;
        type = 2;
    } else {
        open = false;
    }
    if (open) {
        const char* text = path.mData;
        TbError error = data->mStorage.OpenFile(file, text, mode, reinterpret_cast<void*>(static_cast<uintptr_t>(extra)));
        if (static_cast<int32_t>(error) >= 0) {
            TbSoundBankInfo info(type, path.mData, &file);
            data->mBank = reinterpret_cast<TbBank*>(factory.GenerateBank(info, this));
        }
    }
    return data->mBank;
}

// The sound library's sample banks (file TbMMFileBank.cpp of the Mac's "sound symbols shared" library, with the factory and
// the bank handle from ChannelState.cpp). See include/TbSoundBank.h for the layouts, the vtables and the file format.
//
// Every allocation goes through the nothrow forms of operator new, new[] and delete[] (the library's own allocator pair on
// the Mac: __nw__FUl, __nwa__FUl and __dla__FPv), so that a failed allocation is a NULL result as it is there.

// 0x100277c4 (16 bytes)
TbSoundBankInfo::TbSoundBankInfo(uint32_t type, const char* path, bfl::FileSystem::CFile* file)
    : mType(type), mPath(path), mFile(file) {}

// ---- CBankFactory ------------------------------------------------------------------------------------------------------

// 0x10027820 (56 bytes): the base (CAudioFileSystem, 0x10027884) and the factory's vtable.
CBankFactory::CBankFactory() {}

TbError TbSoundManager::GetDirectSoundObject(uint32_t& out) {
    out = 0;
    TbSoundSystemInfo info;
    info.mField8 |= 1;
    TbError error = GetSystemInfo(info);
    if (static_cast<int32_t>(error) >= 0 && (*info.GetMask() & 1) != 0) {
        out = info.mField0;
        return TbError(0);
    }
    return TbError(-1);
}

TbError TbSoundManager::UpdateVolumeControl(uint8_t index, float volume) {
    mVolumes[index] = volume;   // no bounds check
    SystemCommand::UpdateAllVolumes command;
    return SystemCommand(command);
}
