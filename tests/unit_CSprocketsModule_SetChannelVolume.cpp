// Test library for the CSprocketsModule channel commands (unit v5250; libtpw_unit_CSprocketsModule_SetChannelVolume.dylib,
// tests/test_CSprocketsModule_SetChannelVolume.py), sound symbols shared:
//   src/sound/CSprocketsModule.cpp   StopChannel, StopAll, PauseChannel, PauseAll, GetPosition,
//   SetListenerPosition, SetChannelVolume, SetSpeakerConfig
//
// The commands' constructors, SetImmediate, SetLocation, IfOk, the channel send (SendToChannel, the Mac's glue 0x1002f154), the speaker
// type of a configuration and TbSoundPosition::Get are fakes that record what the real ones read and answer from the case's queues.
// The module's channel array (Channels(), the Mac's +0x14) is the global gChannels the test sets.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CSprocketsModule.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

CSprocketsChannel** gChannels = nullptr;

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

uint32_t TagOf(const void* object) {
    uint32_t tag;
    std::memcpy(&tag, static_cast<const char*>(object) + 4, 4);
    return tag;
}

float BitsFloat(int64_t bits) {
    uint32_t b = static_cast<uint32_t>(bits);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

}  // namespace

CSprocketsChannel** CSprocketsModule::Channels() const { return gChannels; }

namespace SamsUtilities {
UCommandStop::UCommandStop() { ev("UCommandStop", {newlab(this)}); }
void UCommand::SetImmediate(uint8_t immediate) { ev("SetImmediate", {lab(this), immediate}); }
UCommandPause::UCommandPause(uint32_t on) { ev("UCommandPause", {newlab(this), on}); }
UCommandVolume::UCommandVolume(double volume) { ev("UCommandVolume", {newlab(this), CanonBits(static_cast<float>(volume))}); }
UCommandSpeakerType::UCommandSpeakerType(SpeakerType type, int32_t flags) {
    ev("UCommandSpeakerType", {newlab(this), static_cast<int32_t>(type), flags});
}
void UCommand3dPosition::SetLocation(double x, double y, double z) {
    ev("SetLocation", {lab(this), CanonBits(static_cast<float>(x)), CanonBits(static_cast<float>(y)), CanonBits(static_cast<float>(z))});
}
UCommandListenerPosition::UCommandListenerPosition(uint32_t on) { ev("UCommandListenerPosition", {newlab(this), on}); }

// UStatus's default constructor and assignment are translated in unit v51053; plain copies here.
UStatus::UStatus() { mValue = 0; }
UStatus& UStatus::operator=(const UStatus& other) {
    mValue = other.mValue;
    return *this;
}

int32_t UStatus::IfOk(TbLegacyError ok, TbLegacyError failed) const {
    ev("IfOk", {static_cast<uint32_t>(mValue), ok.mValue, failed.mValue});
    return static_cast<int32_t>(ret("IfOk"));
}

UStatus SendToChannel(CSprocketsChannel* channel, UCommand& command) {
    ev("Send", {TagOf(channel), lab(&command)});
    UStatus status;
    status.mValue = static_cast<int32_t>(ret("Send"));
    return status;
}

SpeakerType GetConfigSpeakerType(const TbSpeaker::Config& config) {
    ev("ConfigType", {TagOf(&config)});
    return static_cast<SpeakerType>(ret("ConfigType"));
}
}  // namespace SamsUtilities

void TbSoundPosition::Get(float& x, float& y, float& z) const {
    uint32_t words[3];
    std::memcpy(words, this, 12);
    ev("Get", {words[0], words[1], words[2]});
    x = BitsFloat(ret("GetX"));
    y = BitsFloat(ret("GetY"));
    z = BitsFloat(ret("GetZ"));
}

extern "C" {

uint64_t tpw_csm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"channels", &gChannels},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_csm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_csm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(TbSoundSystemModule, mBuffers));
    out[n++] = static_cast<int64_t>(offsetof(TbSoundSampleInfo, mVolume));
    out[n++] = static_cast<int64_t>(sizeof(SamsUtilities::UStatus));
    return n;
}

static CSprocketsModule* M(uint64_t m) { return reinterpret_cast<CSprocketsModule*>(static_cast<uintptr_t>(m)); }

int32_t tpw_csm_StopChannel(uint64_t m, uint32_t channel) { return static_cast<int32_t>(M(m)->CSprocketsModule::StopChannel(channel)); }
int32_t tpw_csm_StopAll(uint64_t m) { return static_cast<int32_t>(M(m)->CSprocketsModule::StopAll()); }
int32_t tpw_csm_PauseChannel(uint64_t m, uint32_t channel) { return static_cast<int32_t>(M(m)->CSprocketsModule::PauseChannel(channel)); }
int32_t tpw_csm_PauseAll(uint64_t m) { return static_cast<int32_t>(M(m)->CSprocketsModule::PauseAll()); }
int32_t tpw_csm_GetPosition(uint64_t m, uint32_t channel, uint64_t position) {
    return static_cast<int32_t>(
        M(m)->CSprocketsModule::GetPosition(channel, *reinterpret_cast<TbSoundPosition*>(static_cast<uintptr_t>(position))));
}
int32_t tpw_csm_SetListenerPosition(uint64_t m, uint64_t position) {
    return static_cast<int32_t>(
        M(m)->CSprocketsModule::SetListenerPosition(*reinterpret_cast<const TbSoundPosition*>(static_cast<uintptr_t>(position))));
}
int32_t tpw_csm_SetChannelVolume(uint64_t m, uint32_t channel, uint64_t info) {
    return static_cast<int32_t>(
        M(m)->CSprocketsModule::SetChannelVolume(channel, *reinterpret_cast<TbSoundSampleInfo*>(static_cast<uintptr_t>(info))));
}
int32_t tpw_csm_SetSpeakerConfig(uint64_t m, uint64_t config) {
    return static_cast<int32_t>(
        M(m)->CSprocketsModule::SetSpeakerConfig(*reinterpret_cast<const TbSpeaker::Config*>(static_cast<uintptr_t>(config))));
}

}  // extern "C"
