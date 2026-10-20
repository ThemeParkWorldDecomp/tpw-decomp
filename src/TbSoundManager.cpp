// TbSoundManager.cpp: the sound manager the game holds (unit w79e, sound symbols shared, file CSprocketsModule.cpp and
// AudioPlaceHolder.cpp in the library). See include/TbSoundManager.h for the layout, the vtable and the calling convention. The
// commands' Execute bodies (unit w79d), the sample store, the device module (p-sound) and TbMapStreamer::SetDroppable
// (0x100231fc) are other units'.
#include "TbSoundManager.h"

#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "bfl_FileSystem.h"
#include "snd_commands.h"

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

// The string goes first, then ~TbSoundCore (the members and bases of C++ run in that order).
TbSoundManager::~TbSoundManager() {}

TbError TbSoundManager::Init(const char* path) {
    mSoundPath = path;
    return TbError(0);
}

float TbSoundManager::GetRolloffFactorToMinMaxDistance(float min, float max) {
    const float span = max - min;
    const float scaled = min / 0.015625f;   // the constant at 0x10033284 is 1/64
    return (scaled - min) / span;
}

TbDynamicStringTemplate<char>& TbSoundManager::GetSoundPath() {
    return mSoundPath;
}

void TbSoundManager::SetListenerPosition(const TbSoundPosition& position) {
    mListenerPosition = &position;
}

const TbSoundPosition* TbSoundManager::GetListenerPosition() {
    return mListenerPosition;
}

void TbSoundManager::SetAudioSystem(CAudioComponent* component) {
    mAudioSystem = component;
}

void TbSoundManager::SetDistanceMapping(float multiplier) {
    mDistanceMultiplier = multiplier;
}

float TbSoundManager::GetDistanceMultiplier() {
    return mDistanceMultiplier;
}

uint32_t TbSoundManager::IsQSoundOn() {
    if ((mOptions & 8) != 0) return 1;
    return 0;
}

TbSoundVolumeControl* TbSoundManager::GetVolumeControl() {
    return &mVolumeControl;
}

float TbSoundManager::GetVolume(uint8_t index) {
    return mVolumes[index];
}

// The listing nests four tests (bit, then the control byte being 0) and answers "none of the selected controls is off".
bool TbSoundManager::VolumeControlIsOn(uint8_t mask) {
    if ((mask & 1) != 0 && mControlOn[0] == 0) return false;
    if ((mask & 2) != 0 && mControlOn[1] == 0) return false;
    if ((mask & 4) != 0 && mControlOn[2] == 0) return false;
    if ((mask & 8) != 0 && mControlOn[3] == 0) return false;
    return true;
}

TbError TbSoundManager::UpdateVolumeControl(uint8_t index, float volume) {
    mVolumes[index] = volume;   // no bounds check
    SystemCommand::UpdateAllVolumes command;
    return SystemCommand(command);
}

TbError TbSoundManager::ToggleVolumeControl(uint8_t index, uint32_t on) {
    if (index < 4) {
        mControlOn[index] = static_cast<uint8_t>(on);
        return TbError(0);
    }
    return TbError(-1);
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

TbError TbSoundManager::SetListenerOrientation(const TbSoundPosition& position, const TbSoundPosition& front,
                                               const TbSoundPosition& top, const TbSoundPosition& velocity) {
    TbSoundSystemInfo info;
    mListenerPosition = &position;
    info.SetListenerPosition(position);
    info.SetListenerDirection(front, top);
    info.SetListenerVelocity(velocity);
    SystemCommand::SetSystemInfo command(info);
    return SystemCommand(command);
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

CBankContainer* TbSoundManager::GetBankData(uint32_t index) {
    CResize<CBankContainer*>& banks = TbMapStreamer::Instance()->GetBankDeque();
    if (index < banks.Size()) return banks[index];
    return nullptr;
}

bool TbSoundManager::IsBankDroppable(uint32_t index) {
    CBankContainer* data = GetBankData(index);
    if (data == nullptr) return false;
    return (data->mFlags & 0x40) != 0;
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

void* TbSoundManager::ConstructSample(void* handle, void*& control, TbSoundSampleInfo& info) {
    control = mAudioSystem->NewControl(handle, info);
    if ((info.mFlags & 0x2000) != 0 && control == nullptr) return nullptr;
    if (control != nullptr) handle = control;
    return mAudioSystem->NewSample(handle, info);
}

void* TbSoundManager::PlaySample(TbSoundSampleInfo& info, void*& control) {
    if ((mOptions & 8) != 0 && (mOptions & 0x100) != 0) info.mField0C |= 2;
    info.mField0C |= 0x2000;
    void* sample = ConstructSample(nullptr, control, info);
    if (sample != nullptr) {
        if (control == nullptr) control = sample;
        SystemCommand::Start command(control);
        TbError error = SystemCommand(command);
        if (static_cast<int32_t>(error) >= 0) return sample;
    }
    return nullptr;
}
