// TbSoundSystemInfo.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbSoundSystemInfo code, named after it.

#include "LbTime.h"
#include "ppc_math.h"
#include "TbSoundInfo.h"
#include "TbSoundPipe.h"
#include <cmath>
#include <cstring>
#include <new>

// The sound library's sample pipe base (unit w78, program "sound symbols shared"): TbSoundSamplePipe (LbMpeg.cpp and
// TbSoundSamplePipe.cpp), TbSoundSPCodec (LbMpeg.cpp, StreamableMpeg2.cpp, TbSoundCodecLoop.cpp, TimerCodec.cpp) and
// TbSoundSampleCodec (StreamableMpeg2.cpp, TbSoundSampleCallback.cpp, TbSoundSampleMPEG2.cpp, TimerCodec.cpp). See
// include/TbSoundPipe.h for the layouts and the slot order.

TbError TbSoundSPCodec::SetInfo(const TbSoundSampleInfo& info) {
    return GetCodec()->SetInfo(info);
}

// The sound library's value classes (unit w77, program "sound symbols shared", Code at 0x10000000): TbSoundPosition,
// TbSoundDistance, TbSoundReverbInfo, TbSoundVolumeControl, TbSoundSampleInfo and TbSoundSystemInfo. Declared in
// include/TbSoundPosition.h and include/TbSoundInfo.h. TbSoundSampleInfo::SetBank (0x1000df50) is in src/TbSoundBank.cpp.

// 0x10011dd4 (28 bytes): sets only +4, +8, +0x18 (the speaker mask 0x20) and +0x3c (the address of the config).
TbSoundSystemInfo::TbSoundSystemInfo(const TbSpeaker::Config& speakers) {
    mField4 = 0;
    mField8 = 0;
    mMask = 0x20;
    mSpeakerConfig = &speakers;
}

// 0x10011e34 (20 bytes)
void TbSoundSystemInfo::SetHWnd(void* window) {
    mHWnd = window;
    mMask |= 2;
}

// 0x10011e7c (20 bytes)
void TbSoundSystemInfo::SetGuid(NS_MacDoze::_GUID* guid) {
    mGuid = guid;
    mMask |= 0x400;
}

// 0x10011ed8 (20 bytes)
void TbSoundSystemInfo::SetClassId(NS_MacDoze::_GUID* classId) {
    mClassId = classId;
    mMask |= 0x2000;
}

// 0x10011f34 (88 bytes): the caller's position by address, only when valid.
void TbSoundSystemInfo::SetListenerPosition(const TbSoundPosition& position) {
    if (position.Valid()) {
        mListenerPosition = &position;
        mMask |= 4;
    }
}

// 0x10011fdc (124 bytes): both addresses, only when the front is valid and then the top is valid (the top is not tested when
// the front is not).
void TbSoundSystemInfo::SetListenerDirection(const TbSoundPosition& front, const TbSoundPosition& top) {
    if (front.Valid()) {
        if (top.Valid()) {
            mFrontDir = &front;
            mTopDir = &top;
            mMask |= 8;
        }
    }
}

// 0x100120bc (88 bytes)
void TbSoundSystemInfo::SetListenerVelocity(const TbSoundPosition& velocity) {
    if (velocity.Valid()) {
        mListenerVelocity = &velocity;
        mMask |= 0x10;
    }
}

// 0x10012164 (20 bytes)
void TbSoundSystemInfo::SetSpeakerConfig(const TbSpeaker::Config& speakers) {
    mSpeakerConfig = &speakers;
    mMask |= 0x20;
}

// 0x100121c8 (20 bytes)
void TbSoundSystemInfo::SetBuffersToUse(uint32_t count) {
    mBuffersToUse = count;
    mMask |= 0x40;
}

// 0x10012218 (20 bytes)
void TbSoundSystemInfo::SetMaxBuffers(uint32_t count) {
    mMaxBuffers = count;
    mMask |= 0x80;
}

// 0x10012264 (4 bytes): does nothing.
void TbSoundSystemInfo::SetHalfRate(uint32_t) {}

// 0x100122a0 (20 bytes)
void TbSoundSystemInfo::SetHardwareDevice(void* device) {
    mHardwareDevice = device;
    mMask |= 0x1000;
}
