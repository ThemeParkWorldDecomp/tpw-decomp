// TbSoundInfo.h: the records the sound runtime passes around (unit w77, "sound symbols shared", Code at 0x10000000):
// TbSoundReverbInfo, TbSoundVolumeControl, TbSoundSampleInfo (what one sample is played with) and TbSoundSystemInfo (what the
// audio device is opened with). Plain value classes without virtuals. The functions are in src/sound/TbSoundPosition.cpp, src/sound/TbSoundSampleInfo.cpp, src/sound/TbSoundSystemInfo.cpp, src/sound/SystemCommand.cpp, src/sound/AudioPlaceHolder.cpp, src/sound/TbSoundManager.cpp, src/sound/TbSoundPipe.cpp, src/sound/TbSoundCore.cpp, src/sound/TbSoundCodecLoop.cpp, src/sound/TbSoundPipeChannel.cpp, src/sound/TbSoundSampleStore.cpp, src/sound/TbSysCommand.cpp, except
// TbSoundSampleInfo::SetBank, which is in src/sound/TbSoundSampleInfo.cpp (unit w72).
//
// The Mac's layouts are given in comments as offsets. Pointer members are native pointers on the host (8 bytes), so the host
// classes are larger than the Mac's (0x90 bytes for the sample info, 0x4c for the system info); the offsets here are the Mac's.
// A setter ORs its flag into the mask word, and the constructors set only the members the comments say.
#pragma once
#include <cstdint>

#include "TbSoundPosition.h"

namespace Bank {
class CBank;
}
class TbSndCallback;
namespace TbSpeaker {
struct Config;
}
namespace NS_MacDoze {
struct _GUID;
}

// The reverb settings of a sample (0x30 bytes). Which words are floats and which are integers is known only from the
// constructor's stores (stfs for the floats named here, stw 0 for the others).
class TbSoundReverbInfo {
public:
    TbSoundReverbInfo();                // 0x100012ac (72)

    uint32_t mField0;                   // +0x00: 0
    uint32_t mField4;                   // +0x04: 0
    uint32_t mField8;                   // +0x08: 0
    uint32_t mFieldC;                   // +0x0c: 0
    float mField10;                     // +0x10: 0.0f
    uint32_t mField14;                  // +0x14: 0
    float mField18;                     // +0x18: 0.0f
    uint32_t mField1C;                  // +0x1c: 0
    float mField20;                     // +0x20: 0.25f
    float mField24;                     // +0x24: 0.5f
    uint32_t mField28;                  // +0x28: 0
    float mField2C;                     // +0x2c: 1.0f
};

// A table of per-channel volumes the caller owns (8 bytes on the Mac).
class TbSoundVolumeControl {
public:
    TbSoundVolumeControl();             // 0x10009d9c (16): both members zero
    double GetVolume(uint32_t index);   // 0x1000aac8 (16): mVolumes[index] as a double, no bounds check
    void SetVolumes(float* volumes);    // 0x10026f28 (8)
    void Init(uint32_t count);          // 0x10026f68 (8)

    float* mVolumes;                    // +0
    uint32_t mCount;                    // +4
};

// What a sample is played with. The mask mFlags says which members are set. mPosition, mVelocity, mVolumeControl hold the
// caller's objects BY POINTER (the setters take references and store their addresses).
class TbSoundSampleInfo {
public:
    TbSoundSampleInfo();                                                    // 0x100158bc (100)

    void SetNumber(uint32_t number);                                        // 0x1000df08 (20): flag 0x80
    void SetBank(Bank::CBank* bank);                                        // 0x1000df50 (20): flag 8; src/sound/TbSoundSampleInfo.cpp (w72)
    void SetUserId(uint32_t id);                                            // 0x1000dcac (20): flag 4
    void SetPan(uint32_t pan);                                              // 0x1000dec0 (20): flag 0x10
    void SetVolume(uint32_t volume);                                        // 0x1000dd98 (20): flag 2
    void SetPitch(int32_t pitch);                                           // 0x1000de2c (20): flag 0x20
    void SetFrequency(float frequency);                                     // 0x1000de74 (20): flag 0x400000
    void SetPriority(uint32_t priority);                                    // 0x1000dde0 (20): flag 0x400
    void SetPosition(const TbSoundPosition& position);                      // 0x1001851c (88): flag 1, only when Valid()
    void SetVelocity(const TbSoundPosition& velocity);                      // 0x1001847c (88): flag 0x800000, only when Valid()
    void SetDistance(const TbSoundDistance& distance);                      // 0x1001959c (40): flag 0x200, copies the three floats
    void SetLoopCount(uint32_t count);                                      // 0x1000dd4c (20): flag 0x40
    void SetCallback(TbSndCallback* callback);                              // 0x1000dcf4 (20): flag 0x800
    void SetCompletedCallback(TbSndCallback* callback);                     // 0x100196f4 (20): flag 0x2000
    void SetUserData(void* data);                                           // 0x1000dc60 (20): flag 0x1000
    void SetReverb(const TbSoundReverbInfo& reverb);                        // 0x1000e068 (112): flag 0x100000, copies the twelve words
    void SetVolumeControl(const TbSoundVolumeControl& control, uint32_t index);   // 0x1000dffc (24): flag 0x40000
    void SetStreamingBufferSize(uint32_t size);                             // 0x1000dfa4 (20): flag 0x8000

    uint32_t mNumber;                   // +0x00 the one-based sample index in the bank (the placeholder in TbSoundBank.h called it mSample)
    Bank::CBank* mBank;                 // +0x04
    uint32_t mUserId;                   // +0x08
    uint32_t mField0C;                  // +0x0c: zeroed by the constructor
    uint32_t mField10;                  // +0x10: zeroed by the constructor
    uint32_t mFlags;                    // +0x14
    uint32_t mField18;                  // +0x18: never touched here
    uint32_t mVolume;                   // +0x1c
    float mFrequency;                   // +0x20
    int32_t mPitch;                     // +0x24
    uint32_t mPriority;                 // +0x28
    const TbSoundPosition* mPosition;   // +0x2c: the caller's position (a pointer)
    const TbSoundPosition* mVelocity;   // +0x30: the caller's velocity (a pointer)
    TbSoundDistance mDistance;          // +0x34 (12 bytes)
    uint32_t mLoopCount;                // +0x40
    TbSndCallback* mCallback;           // +0x44
    TbSndCallback* mCompletedCallback;  // +0x48
    void* mUserData;                    // +0x4c
    TbSoundReverbInfo mReverb;          // +0x50 (0x30 bytes)
    const TbSoundVolumeControl* mVolumeControl;   // +0x80: the caller's control (a pointer)
    uint32_t mVolumeControlIndex;       // +0x84
    uint32_t mStreamingBufferSize;      // +0x88
    uint32_t mPan;                      // +0x8c
};

// What the audio device is opened with (0x4c bytes on the Mac). The three Set..Position/Direction/Velocity setters store the
// caller's objects by pointer, and only when they are valid.
class TbSoundSystemInfo {
public:
    TbSoundSystemInfo();                                                    // 0x10011d7c (40): sets only +0, +4, +8, +0x18, +0x1c, +0x20, +0x40
    explicit TbSoundSystemInfo(const TbSpeaker::Config& speakers);          // 0x10011dd4 (28): sets only +4, +8, +0x18 (0x20), +0x3c
    TbSoundSystemInfo(const TbSoundSystemInfo& other);                      // 0x1000ba78 (148): copies all 0x4c bytes

    void SetHWnd(void* window);                                             // 0x10011e34 (20): mask 2
    void SetGuid(NS_MacDoze::_GUID* guid);                                  // 0x10011e7c (20): mask 0x400
    void SetClassId(NS_MacDoze::_GUID* classId);                            // 0x10011ed8 (20): mask 0x2000
    void SetListenerPosition(const TbSoundPosition& position);              // 0x10011f34 (88): mask 4, only when Valid()
    void SetListenerDirection(const TbSoundPosition& front, const TbSoundPosition& top);   // 0x10011fdc (124): mask 8, only when both are Valid()
    void SetListenerVelocity(const TbSoundPosition& velocity);              // 0x100120bc (88): mask 0x10, only when Valid()
    void SetSpeakerConfig(const TbSpeaker::Config& speakers);               // 0x10012164 (20): mask 0x20
    void SetBuffersToUse(uint32_t count);                                   // 0x100121c8 (20): mask 0x40
    void SetMaxBuffers(uint32_t count);                                     // 0x10012218 (20): mask 0x80
    void SetHalfRate(uint32_t halfRate);                                    // 0x10012264 (4): empty
    void SetHardwareDevice(void* device);                                   // 0x100122a0 (20): mask 0x1000

    uint32_t GetBuffersToUse() const;                                       // 0x1001061c (8)
    const TbSpeaker::Config* GetSpeakerConfig() const;                      // 0x10015e74 (8)
    const TbSoundPosition* GetListenerVelocity() const;                     // 0x10015eb8 (8)
    const TbSoundPosition* GetTopDir() const;                               // 0x10015f00 (8)
    const TbSoundPosition* GetFrontDir() const;                             // 0x10015f8c (8)
    const TbSoundPosition* GetListenerPosition() const;                     // 0x10015fcc (8)
    uint32_t* GetMask();                                                    // 0x10016014 (8): the ADDRESS of the mask word (+0x18)
    uint32_t GetMaxBuffers() const;                                         // 0x10027120 (8)
    uint32_t GetSamplesPlaying() const;                                     // 0x10028c08 (8)

    uint32_t mField0;                   // +0x00
    uint32_t mField4;                   // +0x04
    uint32_t mField8;                   // +0x08
    uint32_t mFieldC;                   // +0x0c: with +0x10 one eight-byte unit for the copy constructor
    uint32_t mField10;                  // +0x10
    uint32_t mMaxBuffers;               // +0x14
    uint32_t mMask;                     // +0x18
    uint32_t mBuffersToUse;             // +0x1c: 10 by default
    void* mHWnd;                        // +0x20
    NS_MacDoze::_GUID* mGuid;           // +0x24
    NS_MacDoze::_GUID* mClassId;        // +0x28
    const TbSoundPosition* mListenerPosition;   // +0x2c
    const TbSoundPosition* mFrontDir;   // +0x30
    const TbSoundPosition* mTopDir;     // +0x34
    const TbSoundPosition* mListenerVelocity;   // +0x38
    const TbSpeaker::Config* mSpeakerConfig;    // +0x3c
    uint32_t mField40;                  // +0x40
    uint32_t mSamplesPlaying;           // +0x44
    void* mHardwareDevice;              // +0x48
};
