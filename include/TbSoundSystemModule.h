// Sound device base, sound symbols shared. Mac size 0x14, vtable 0x1003c5c4.
// Native pointers enlarge host objects. Slots below follow the Mac order from +8.
// TbError returns use a hidden r3 result buffer on the Mac; this moves to r4.
#pragma once
#include <cstdint>
#include "TbError.h"
#include "TbSoundInfo.h"
#include "snd_handles.h"
class TbSoundPipeChannel;
class IDirectSound;
class IDirectSoundBuffer;

class TbSoundSystemModule {
public:
    TbSoundSystemModule(); // 0x100122f0 (32), leaves +0x10 untouched
    virtual ~TbSoundSystemModule(); // +0x08, 0x100071d4 (104)
    virtual void Process(); // +0x0c, 0x100125c8
    virtual uint32_t GetDirectSoundObject(IDirectSound*& object); // +0x10, 0x10012600
    virtual void GetDirectSoundBuffer(uint32_t channel, IDirectSoundBuffer*& buffer); // +0x14, 0x10012660
    virtual void* GetSystemObject(uint32_t kind); // +0x18, 0x100126c4
    virtual TbError SetOptions(uint32_t options); // +0x1c, 0x10012708
    virtual TbError Init(const TbSoundSystemInfo& info); // +0x20, 0x10012340
    virtual void Close() = 0; // +0x24, pure
    virtual TbError StreamData(TbSoundPipeChannel* channel) = 0; // +0x28, pure
    virtual TbError StopChannel(uint32_t channel) = 0; // +0x2c, pure
    virtual TbError StopAll(); // +0x30, 0x1000bc34
    virtual TbError PauseChannel(uint32_t channel); // +0x34, 0x10012764
    virtual TbError RestartChannel(uint32_t channel); // +0x38, 0x100127c4
    virtual TbError PauseAll(); // +0x3c, 0x10012824
    virtual TbError RestartAll(); // +0x40, 0x1001287c
    virtual bool IsChannelPlaying(uint32_t channel); // +0x44, 0x100128d8
    virtual bool SupportsEnvironmentalEffects(); // +0x48, 0x1000bf00
    virtual TbError SetLoPass(uint32_t channel, float amount); // +0x4c, 0x10012920
    virtual TbError SetDistanceFactor(uint32_t channel, const TbSoundDistance& distance); // +0x50, 0x1001297c
    virtual TbError SetSpeakerConfig(const TbSpeaker::Config& config); // +0x54, 0x100129f4
    virtual TbError Set3DDSPFunctionality(uint32_t channel, uint32_t options); // +0x58, 0x10012a68
    virtual TbError SetSpeedOfSound(float speed); // +0x5c, 0x10012ad0
    virtual TbError SetPanRate(int32_t rate); // +0x60, 0x10012b30
    virtual TbError SetPosition(uint32_t channel, const TbSoundPosition& position, int32_t mode); // +0x64, 0x10012b8c
    virtual TbError GetPosition(uint32_t channel, TbSoundPosition& position); // +0x68, 0x10012bfc
    virtual TbError SetVelocity(uint32_t channel, const TbSoundPosition& velocity); // +0x6c, 0x10012c6c
    virtual TbError GetVelocity(uint32_t channel, TbSoundPosition& velocity); // +0x70, 0x10012cdc
    virtual TbError SetListenerVelocity(const TbSoundPosition& velocity); // +0x74, 0x10012d4c
    virtual TbError SetListenerPosition(const TbSoundPosition& position); // +0x78, 0x10012dc4
    virtual TbError SetListenerDirection(const TbSoundPosition& front, const TbSoundPosition& top); // +0x7c, 0x10012e3c
    virtual TbError SetPitch(uint32_t channel, uint32_t rate, int32_t pitch); // +0x80, 0x10012ec8
    virtual TbError SetFrequency(uint32_t channel, uint32_t rate, float frequency); // +0x84, 0x10012f24
    virtual TbError SetChannelInfo(uint32_t channel, TbSoundPipeChannel* pipe, TbSoundSampleInfo& info); // +0x88, 0x10012f84
    virtual TbError SetChannelVolume(uint32_t channel, TbSoundSampleInfo& info) = 0; // +0x8c, pure
    virtual TbError SetReverbEnvironment(uint32_t environment, float a, float b, float c); // +0x90, 0x1001300c
    virtual TbError SetReverbPosition(const TbSoundPosition& position); // +0x94, 0x10013074

    uint32_t GetBufferSize(TbSoundPipeChannel* channel); // 0x100123c4 (72), nonvirtual
    static double ConvertPitchIndexToFrequency(int32_t index); // 0x1001249c (156), index in r3

    uint32_t mBuffers; // Mac +0x04
    uint32_t mF008;    // Mac +0x08
    uint32_t mRate;    // Mac +0x0c
    TbSpeaker::AutoDelete mAutoDelete; // Mac +0x10, empty class
    uint8_t mPadding[3]; // reserves the rest of the Mac's four-byte AutoDelete slot
};
