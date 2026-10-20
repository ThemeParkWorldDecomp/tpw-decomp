// CSprocketsModule, second part (unit v5997): the constructor, Close, Tick, StreamData, SetChannelInfo and SetListenerDirection.
// The Mac answers through a hidden r3 TbError buffer; the host returns the TbError.
#include <new>

#include "CMpegBase.h"
#include "CSprocketsModule.h"
#include "TbMpegBase.h"
#include "TbSoundCodecs.h"
#include "TbSoundInfo.h"
#include "TbSoundPipe.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleCallback.h"
#include "TbSoundSampleMPEG2.h"
#include "../platform/sound_module.h"

using namespace SamsUtilities;

// 0x1002b234 (108): the base constructor, the vtable, then a CSprocketsSpeaker (4 bytes from the sound library's allocator,
// constructed only when the allocation worked) handed to the speaker configuration.
__attribute__((weak)) CSprocketsModule::CSprocketsModule() {
    CSprocketsSpeaker* speaker = static_cast<CSprocketsSpeaker*>(UAllocatorMake(4));
    if (speaker != nullptr) new (speaker) CSprocketsSpeaker();
    TbSpeaker::ConfigSetModuleSpeaker(reinterpret_cast<TbSpeaker::Module*>(speaker));
}

// 0x1002b374 (100) is not translated yet: the destructor (the vtable's key function) is defined by platform/sound_module.cpp
// in the launcher and by the test library's unit file.

// 0x1002b8c4 (112): every pool is topped up, then the import.
__attribute__((weak)) void CSprocketsModule::Tick() {
    _TbSoundSampleMPEG2::TopUp();
    _TbSoundSampleCallback::TopUp();
    CMpegBase::TopUp();
    _TbSoundSampleMPEG2File::TopUp();
    TbTimerCodec::TopUp();
    TbSoundUserCallback::TopUp();
    TbSoundSamplePipe::TopUp();
    _TbSoundCodecLoop::TopUp();
    _TbSoundSampleCompletedCallback::TopUp();
    MpegFilePlayerPool::MaintainPoolLevel();
    SoundTickImport_();
}

// 0x1002bde8 (160): the file player pool is emptied, then every channel is deleted (the deleting destructor, flag 1) and the
// array is freed. The array pointer is not cleared and is read again at every step, as the original did.
__attribute__((weak)) void CSprocketsModule::Close() {
    MpegFilePlayerPool::ClearPool(1);
    if (Channels() != nullptr) {
        for (uint32_t i = 0; i < mBuffers; i++) {
            CSprocketsChannel* channel = Channels()[i];
            if (channel != nullptr) delete channel;
        }
        DisposeBuffer_(Channels());
    }
}

// 0x1002bf20 (188): a NULL pipe is ok; otherwise the pipe's channel must exist and have a description. GetSampleFlags is
// called and its answer dropped.
__attribute__((weak)) TbError CSprocketsModule::StreamData(TbSoundPipeChannel* pipe) {
    if (pipe != nullptr) {
        uint32_t index = pipe->GetIndex();
        pipe->GetSampleFlags();
        CSprocketsChannel* channel = Channels()[index];
        if (channel == nullptr) return TbError(-1);
        if (!channel->GetDescription()) return TbError(-1);
        channel->Play();
    }
    return TbError(0);
}

// 0x1002c820 (760). The flags tested in the sample info: +0x0c (mField0C) 0x2000 attaches the pipe, 0x20 is the 3D test; the
// mask +0x14 (mFlags) says what to send. A handled bit is cleared; the answer is -1 while a bit is left or the status is set.
// Bug kept: the 3D test ANDs the channel's 3D byte (0 or 1 after Set3D) with 0x20, so it only passes when the byte has bit 5;
// and the velocity branch clears 0x800000 before the position branch tests it again, so a position always zeroes the velocity.
__attribute__((weak)) TbError CSprocketsModule::SetChannelInfo(uint32_t channelIndex, TbSoundPipeChannel* pipe, TbSoundSampleInfo& info) {
    UStatus status{};
    CSprocketsChannel* channel = Channels()[channelIndex];
    if (channel == nullptr) return TbError(-1);
    if ((info.mField0C & 0x2000) != 0) {
        uint32_t sampleFlags = pipe->GetSampleFlags();
        channel->SetPipe(pipe);
        channel->SetDescription(*pipe->GetSampleHeader());
        channel->Set3D((sampleFlags & 1) == 0);
        UCommandPreroll preroll(1);
        status = SendToChannel(channel, preroll);
    }
    if ((info.mFlags & 2) != 0) {
        float volume = static_cast<float>(static_cast<double>(info.mVolume)) / 127.0f;
        UCommandVolume command(volume);
        command.SetImmediate(1);
        status = SendToChannel(channel, command);
        info.mFlags &= ~2u;
    }
    if ((info.mFlags & 0x400000) != 0) info.mFlags &= ~0x400000u;
    if ((info.mFlags & 0x800000) != 0 || (info.mFlags & 1) != 0) {
        UCommandPosition command(1);
        float x, y, z;
        if ((info.mFlags & 0x800000) != 0) {
            if ((channel->Is3D() & (info.mField0C & 0x20)) != 0) {
                info.mVelocity->Get(x, y, z);
                command.SetVelocity(x, y, z);
            }
            info.mFlags &= ~0x800000u;
        }
        if ((info.mFlags & 1) != 0) {
            if ((channel->Is3D() & (info.mField0C & 0x20)) != 0) {
                info.mPosition->Get(x, y, z);
                if ((info.mFlags & 0x800000) == 0) command.SetVelocity(0.0, 0.0, 0.0);
                command.SetLocation(x, y, z);
            }
            info.mFlags &= ~1u;
        }
        status = SendToChannel(channel, command);
    }
    if ((info.mFlags & 0x200) != 0) info.mFlags &= ~0x200u;
    if ((info.mFlags & 0x20) != 0) info.mFlags &= ~0x20u;
    if (status.IsSet() || info.mFlags != 0) return TbError(-1);
    return TbError(0);
}

// 0x1002d220 (280): channel 0 gets a listener command with the orientation and the up vector.
__attribute__((weak)) TbError CSprocketsModule::SetListenerDirection(const TbSoundPosition& front, const TbSoundPosition& top) {
    UStatus status{};
    CSprocketsChannel* channel = Channels()[0];
    if (channel == nullptr) return TbError(-1);
    UCommandListenerPosition command(1);
    float x, y, z;
    front.Get(x, y, z);
    command.SetOrientation(x, y, z);
    top.Get(x, y, z);
    command.SetUpVector(x, y, z);
    status = SendToChannel(channel, command);
    return TbError(status.IfOk(TbLegacyError{0}, TbLegacyError{-1}));
}
