// CSprocketsChannel (unit v5263): the Sound Sprockets channel object, 0x1002a29c .. 0x1002b0b8 of the sound library.
// The host class is CSprocketsChannelMac: platform/sound_module.h owns the name CSprocketsChannel.
#include "CSprocketsModule.h"
#include "TbSoundPipeChannel.h"

using namespace SamsUtilities;

// 0x1002a29c (112)
CSprocketsChannelMac::CSprocketsChannelMac() {
    USoundChannel_Construct(this, 0x10c);
    mVtable10 = 0x1003e0dc;
    mVtable4 = 0x1003e0e4;
    mVtable8 = 0x1003e0f4;
    mPipe = nullptr;
    mBuffer = nullptr;
    mBufferSize = 0;
    m3D = 0;
    mFlag49 = 0;
}

// 0x1002a33c (132): the deleting form (flag > 0) goes through operator delete.
CSprocketsChannelMac::~CSprocketsChannelMac() {
    mVtable10 = 0x1003e0dc;
    mVtable4 = 0x1003e0e4;
    mVtable8 = 0x1003e0f4;
    Cleanup();
    USoundChannel_Destruct(this, 0);
}

void CSprocketsChannelMac::operator delete(void* object) { USoundChannel_Free(object); }

// 0x1002a3f0 (168)
void CSprocketsChannelMac::Cleanup() {
    if (mSndChannel != nullptr) {
        SndCommand command;
        command.cmd = 0x2e;
        command.param1 = 0;
        command.param2 = 0;
        SndDoImmediate_(mSndChannel, &command);
        command.cmd = 4;
        SndDoImmediate_(mSndChannel, &command);
        command.cmd = 3;
        SndDoImmediate_(mSndChannel, &command);
    }
    if (mBuffer != nullptr) {
        DisposeBuffer_(mBuffer);
        mBuffer = nullptr;
        mBufferSize = 0;
    }
}

// 0x1002a4cc (184)
void CSprocketsChannelMac::Callback(SndCommand* command) {
    UCommandPlay play;
    if (mPipe != nullptr && mBuffer != nullptr) {
        if (static_cast<USoundDescription*>(mDescription) != nullptr) {
            if (TbSoundPipeChannel::StreamerCallback(mBuffer, mBufferSize, mPipe)) {
                play.SetImmediate(1);
                SendToChannel(this, play);
            }
        }
    }
    USoundChannel_Callback(this, command);
}

// 0x1002a8a0 (124)
void CSprocketsChannelMac::Play() {
    UCommandPlay play;
    if (mPipe != nullptr) {
        if (TbSoundPipeChannel::StreamerCallback(mBuffer, mBufferSize, mPipe)) {
            play.SetImmediate(1);
            SendToChannel(this, play);
        }
    }
}

// 0x1002b0b8 (112)
void CSprocketsChannelMac::Set3D(uint8_t on) {
    m3D = on;
    UCommandSoundType command(static_cast<SoundType>(on != 0 ? 1 : 0));
    command.SetImmediate(1);
    SendToChannel(this, command);
}
