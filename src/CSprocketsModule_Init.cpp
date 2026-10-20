// CSprocketsModule::Init (unit v5L24), sound symbols shared 0x1002b408 (416). The Mac answers through a hidden r3 TbError
// buffer; the host returns the TbError.
#include <cstring>
#include <new>

#include "CMpegBase.h"
#include "CSprocketsModule.h"
#include "TbSoundInfo.h"

using namespace SamsUtilities;

// Init is weak: platform/sound_module.cpp holds the host version (the SDL device) the launcher links, as v51053 did for
// SetSpeedOfSound and SetListenerVelocity; the unit test library takes this one.
// 0x1002b408 (416): the base's Init (its answer dropped), then the channel array (mChannels, +0x14), -1 when it cannot be made.
// Three commands are built once: UCommandSoundDistance(400.0) and UCommandListenerScale(0.01f, 0, 0), both immediate, and
// UCommandCPULoad with the info's double at +0xc (not immediate). Every channel is allocated (0x4c bytes) and constructed;
// one that cannot be allocated leaves its slot as the allocation left it and gets nothing. The first channel that was made
// (not necessarily channel 0) gets the CPU load and the listener scale; every one gets the sound distance. The speaker
// configuration is a TbSpeaker::Stereo, sent through the virtual SetSpeakerConfig (answer dropped); the MPEG file player pool is
// set to be kept full and Tick runs. The commands have no destructor calls.
__attribute__((weak)) TbError CSprocketsModule::Init(const TbSoundSystemInfo& info) {
    TbSoundSystemModule::Init(info);
    // Host: the array holds native pointers (the Mac's was mBuffers << 2 bytes), and mChannels is the host record's.
    SetChannels(static_cast<CSprocketsChannel**>(AllocBuffer_(mBuffers * static_cast<uint32_t>(sizeof(CSprocketsChannel*)))));
    if (Channels() == nullptr) return TbError(-1);

    UCommandSoundDistance distance(400.0);
    UCommandListenerScale scale(0.01f, 0.0, 0.0);   // the Mac's constant is the float 0.01 widened (0x3f847ae140000000)
    scale.SetImmediate(1);
    // The info's +0xc and +0x10 words are one double on the Mac (TbAudioBody::Init stores the quality there).
    const uint64_t loadBits = (static_cast<uint64_t>(info.mFieldC) << 32) | info.mField10;
    double load;
    std::memcpy(&load, &loadBits, sizeof load);
    UCommandCPULoad cpuLoad(load);
    distance.SetImmediate(1);

    bool first = true;
    for (uint32_t i = 0; i < mBuffers; i++) {
        // Host: the native object's size (the Mac's is 0x4c).
        CSprocketsChannelMac* channel = static_cast<CSprocketsChannelMac*>(UAllocatorMake(sizeof(CSprocketsChannelMac)));
        if (channel != nullptr) new (channel) CSprocketsChannelMac();
        if (channel == nullptr) continue;
        // Host: the array's element type is the platform's channel class (platform/sound_module.h owns the name).
        Channels()[i] = reinterpret_cast<CSprocketsChannel*>(channel);
        if (first) {
            SendToChannel(channel, cpuLoad);
            SendToChannel(channel, scale);
            first = false;
        }
        SendToChannel(channel, distance);
    }

    uint32_t stereo;   // the Mac's 4-byte TbSpeaker::Stereo: its vtable word
    TbSpeaker::StereoConstruct(&stereo);
    SetSpeakerConfig(*reinterpret_cast<const TbSpeaker::Config*>(&stereo));
    MpegFilePlayerPool::SetPoolKeepFull(1);
    Tick();
    return TbError(0);
}
