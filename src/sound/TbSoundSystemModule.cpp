// TbSoundSystemModule.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbSoundSystemModule code, named after it.

#include "TbSoundPipeChannel.h"
#include "TbSoundSystemModule.h"
#include <cmath>

TbSoundSystemModule::TbSoundSystemModule() : mBuffers(0), mF008(0), mRate(22050) {}
TbError TbSoundSystemModule::Init(const TbSoundSystemInfo& info) {
    mBuffers = (info.mMask & 0x80u) != 0 ? info.mMaxBuffers : info.mBuffersToUse;
    return TbError(0);
}

uint32_t TbSoundSystemModule::GetBufferSize(TbSoundPipeChannel* channel) {
    return channel->GetBufferSize() == 0 ? 0x1200u : channel->GetBufferSize();
}

double TbSoundSystemModule::ConvertPitchIndexToFrequency(int32_t index) {
    if (index == 0) return 1.0;
    // addi/subfic wrap before the signed integer is converted and rounded to single.
    uint32_t word = index < 0 ? 1u - static_cast<uint32_t>(index) : static_cast<uint32_t>(index) + 1u;
    float exponent = static_cast<float>(static_cast<int32_t>(word)) / 96.0f;
    float frequency = ::powf(2.0f, exponent);
    return index < 0 ? 1.0f / frequency : frequency;
}

void TbSoundSystemModule::Process() {}

uint32_t TbSoundSystemModule::GetDirectSoundObject(IDirectSound*& object) {
    object = nullptr;
    return 0;
}

void TbSoundSystemModule::GetDirectSoundBuffer(uint32_t, IDirectSoundBuffer*& buffer) {
    buffer = nullptr;
}

void* TbSoundSystemModule::GetSystemObject(uint32_t) {
    return nullptr;
}

TbError TbSoundSystemModule::SetOptions(uint32_t) {
    return TbError(-1);
}

TbError TbSoundSystemModule::PauseChannel(uint32_t) {
    return TbError(-1);
}

TbError TbSoundSystemModule::RestartChannel(uint32_t) {
    return TbError(-1);
}

TbError TbSoundSystemModule::PauseAll() {
    return TbError(-1);
}

TbError TbSoundSystemModule::RestartAll() {
    return TbError(-1);
}

bool TbSoundSystemModule::IsChannelPlaying(uint32_t) {
    return false;
}

TbError TbSoundSystemModule::SetLoPass(uint32_t, float) {
    return TbError(-1);
}

TbError TbSoundSystemModule::SetDistanceFactor(uint32_t, const TbSoundDistance&) {
    return TbError(-1);
}

TbError TbSoundSystemModule::SetSpeakerConfig(const TbSpeaker::Config&) {
    return TbError(-1);
}

TbError TbSoundSystemModule::Set3DDSPFunctionality(uint32_t, uint32_t) {
    return TbError(-1);
}

TbError TbSoundSystemModule::SetSpeedOfSound(float) {
    return TbError(-1);
}

TbError TbSoundSystemModule::SetPanRate(int32_t) {
    return TbError(-1);
}

TbError TbSoundSystemModule::SetPosition(uint32_t, const TbSoundPosition&, int32_t) {
    return TbError(-1);
}

TbError TbSoundSystemModule::GetPosition(uint32_t, TbSoundPosition&) {
    return TbError(-1);
}

TbError TbSoundSystemModule::SetVelocity(uint32_t, const TbSoundPosition&) {
    return TbError(-1);
}

TbError TbSoundSystemModule::GetVelocity(uint32_t, TbSoundPosition&) {
    return TbError(-1);
}

TbError TbSoundSystemModule::SetListenerVelocity(const TbSoundPosition&) {
    return TbError(-1);
}

TbError TbSoundSystemModule::SetListenerPosition(const TbSoundPosition&) {
    return TbError(-1);
}

TbError TbSoundSystemModule::SetListenerDirection(const TbSoundPosition&, const TbSoundPosition&) {
    return TbError(-1);
}

TbError TbSoundSystemModule::SetPitch(uint32_t, uint32_t, int32_t) {
    return TbError(-1);
}

TbError TbSoundSystemModule::SetFrequency(uint32_t, uint32_t, float) {
    return TbError(-1);
}

TbError TbSoundSystemModule::SetChannelInfo(uint32_t, TbSoundPipeChannel*, TbSoundSampleInfo&) {
    return TbError(-1);
}

TbError TbSoundSystemModule::SetReverbEnvironment(uint32_t, float, float, float) {
    return TbError(-1);
}

TbError TbSoundSystemModule::SetReverbPosition(const TbSoundPosition&) {
    return TbError(-1);
}
