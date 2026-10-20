// CSprocketsModule: the channel commands the Mac sent through Sound Sprockets (unit v5250). Each function reads the channel from the
// module's channel array (mChannels, +0x14, no bounds test), builds a SamsUtilities command, hands it to the channel's virtual
// send (the glue 0x1002f154 through slot +0x2c) and answers the status as a TbError.
#include "CSprocketsModule.h"

using namespace SamsUtilities;

// 0x1002c068 (164)
TbError CSprocketsModule::StopChannel(uint32_t channel) {
    CSprocketsChannel* ch = Channels()[channel];
    if (ch == nullptr) return TbError(-1);
    UCommandStop command;
    command.SetImmediate(1);
    UStatus status = SendToChannel(ch, command);
    return TbError(status.IfOk(TbLegacyError{0}, TbLegacyError{-1}));
}

// 0x1002c238 (172)
TbError CSprocketsModule::StopAll() {
    UCommandStop command;
    command.SetImmediate(1);
    for (uint32_t i = 0; i < mBuffers; i++) {
        CSprocketsChannel* ch = Channels()[i];
        if (ch != nullptr) SendToChannel(ch, command);
    }
    return TbError(0);
}

// 0x1002c314 (168): the command is built before the channel is tested
TbError CSprocketsModule::PauseChannel(uint32_t channel) {
    UCommandPause command(1);
    CSprocketsChannel* ch = Channels()[channel];
    if (ch == nullptr) return TbError(-1);
    UStatus status = SendToChannel(ch, command);
    return TbError(status.IfOk(TbLegacyError{0}, TbLegacyError{-1}));
}

// 0x1002c670 (164)
TbError CSprocketsModule::PauseAll() {
    UCommandPause command(1);
    for (uint32_t i = 0; i < mBuffers; i++) {
        CSprocketsChannel* ch = Channels()[i];
        if (ch != nullptr) SendToChannel(ch, command);
    }
    return TbError(0);
}

// 0x1002cfa8 (68): only tests that the channel exists
TbError CSprocketsModule::GetPosition(uint32_t channel, TbSoundPosition& position) {
    (void)position;
    if (Channels()[channel] == nullptr) return TbError(-1);
    return TbError(0);
}

// 0x1002d090 (192): sent to channel 0
TbError CSprocketsModule::SetListenerPosition(const TbSoundPosition& position) {
    CSprocketsChannel* ch = Channels()[0];
    if (ch == nullptr) return TbError(-1);
    float x, y, z;
    position.Get(x, y, z);
    UCommandListenerPosition command(1);
    command.SetLocation(x, y, z);
    UStatus status = SendToChannel(ch, command);
    return TbError(status.IfOk(TbLegacyError{0}, TbLegacyError{-1}));
}

// 0x1002d5b4 (200): the volume (0..127) as a single-precision fraction
TbError CSprocketsModule::SetChannelVolume(uint32_t channel, TbSoundSampleInfo& info) {
    CSprocketsChannel* ch = Channels()[channel];
    if (ch == nullptr) return TbError(-1);
    float volume = static_cast<float>(static_cast<double>(info.mVolume)) / 127.0f;
    UCommandVolume command(volume);
    command.SetImmediate(1);
    UStatus status = SendToChannel(ch, command);
    return TbError(status.IfOk(TbLegacyError{0}, TbLegacyError{-1}));
}

// 0x1002d6cc (176): sent to channel 0
TbError CSprocketsModule::SetSpeakerConfig(const TbSpeaker::Config& config) {
    CSprocketsChannel* ch = Channels()[0];
    if (ch == nullptr) return TbError(-1);
    UCommandSpeakerType command(GetConfigSpeakerType(config), 0);
    UStatus status = SendToChannel(ch, command);
    return TbError(status.IfOk(TbLegacyError{0}, TbLegacyError{-1}));
}
