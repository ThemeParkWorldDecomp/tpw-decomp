// sound_module.h: CSprocketsChannel, the host sound channel (unit p-sound, "sound symbols shared", file
// CSprocketsModule.cpp, constructor 0x1002a29c). Private to platform/ and its test program. On the Mac it was a
// SamsUtilities::USoundChannel (0x10c bytes: Sound Sprockets' wrapper of a Sound Manager channel) plus +0x3c mPipe, +0x40
// mBuffer (a cleared double buffer of 2 x mChunk), +0x44 mChunk, +0x48 mIs3D, +0x49 a byte. Here the sound channel is one
// stream of the module's tpw::SoundDevice and the double buffer is the device's scratch chunk.
#pragma once
#include <cstdint>


class TbSoundPipeChannel;
struct TbSampleHeader;

namespace tpw {
class SoundDevice;
}

class CSprocketsChannel {
public:
    CSprocketsChannel(tpw::SoundDevice* device, uint32_t index);   // 0x1002a29c (112)
    virtual ~CSprocketsChannel();                                   // 0x1002a33c (132), the deleting form (flag 1)

    void Cleanup();                                  // 0x1002a3f0 (168): flush and quiet the channel, free the buffer
    void Play();                                     // 0x1002a8a0 (124): pull the first chunk, play it when it carries data
    void SetPipe(TbSoundPipeChannel* pipe) { mPipe = pipe; }   // 0x1002a94c (8)
    bool GetDescription() const;                     // 0x1002a99c (36): a description exists (the Mac answered its address)
    void SetDescription(const TbSampleHeader& header);          // 0x1002a9f8 (332)
    void Set3D(uint8_t is3D) { mIs3D = is3D; }       // 0x1002b0b8 (112): the sound type command is dropped on the host
    uint8_t Is3D() const { return mIs3D; }           // 0x1002b1fc (8)

    // The commands the module sends, as the host does them. Each answers true when the Mac's UStatus would be ok.
    bool Preroll() const;                            // UCommandPreroll(1): needs a description; nothing to prime on the host
    bool SetVolume(float gain);                      // UCommandVolume, immediate: the stream's gain
    void Stop();                                     // UCommandStop, immediate: unbind, clear, forget the pipe
    void Pause();                                    // UCommandPause(1)
    void Restart();                                  // UCommandPlay(1)
    void SetPosition(const float location[3], const float velocity[3]);   // UCommandPosition (kept, not applied)

    uint32_t Index() const { return mIndex; }
    TbSoundPipeChannel* Pipe() const { return mPipe; }
    uint32_t Chunk() const { return mChunk; }       // +0x44: 0x1200 for mono, 0x2400 for stereo, 0 without a description
    tpw::SoundDevice* Device() const { return mDevice; }
    const float* Location() const { return mLocation; }
    const float* Velocity() const { return mVelocity; }

private:
    tpw::SoundDevice* mDevice;
    uint32_t mIndex;
    TbSoundPipeChannel* mPipe;     // +0x3c
    uint32_t mChunk;               // +0x44
    uint8_t mIs3D;                 // +0x48
    float mLocation[3];
    float mVelocity[3];
};
