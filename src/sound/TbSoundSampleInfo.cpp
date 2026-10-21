// TbSoundSampleInfo.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbSoundSampleInfo code, named after it.

#include "ppc_math.h"
#include "TbDynamicStringTemplate.h"
#include "TbSoundBank.h"
#include "TbSoundInfo.h"
#include <cmath>
#include <cstddef>
#include <cstring>
#include <new>

// The sound library's value classes (unit w77, program "sound symbols shared", Code at 0x10000000): TbSoundPosition,
// TbSoundDistance, TbSoundReverbInfo, TbSoundVolumeControl, TbSoundSampleInfo and TbSoundSystemInfo. Declared in
// include/TbSoundPosition.h and include/TbSoundInfo.h. TbSoundSampleInfo::SetBank (0x1000df50) is in src/TbSoundBank.cpp.

// 0x1000dc60 (20 bytes)
void TbSoundSampleInfo::SetUserData(void* data) {
    mUserData = data;
    mFlags |= 0x1000;
}

// 0x1000dcac (20 bytes)
void TbSoundSampleInfo::SetUserId(uint32_t id) {
    mUserId = id;
    mFlags |= 4;
}

// 0x1000dcf4 (20 bytes)
void TbSoundSampleInfo::SetCallback(TbSndCallback* callback) {
    mCallback = callback;
    mFlags |= 0x800;
}

// 0x1000dd4c (20 bytes)
void TbSoundSampleInfo::SetLoopCount(uint32_t count) {
    mLoopCount = count;
    mFlags |= 0x40;
}

// 0x1000dd98 (20 bytes)
void TbSoundSampleInfo::SetVolume(uint32_t volume) {
    mVolume = volume;
    mFlags |= 2;
}

// 0x1000dde0 (20 bytes)
void TbSoundSampleInfo::SetPriority(uint32_t priority) {
    mPriority = priority;
    mFlags |= 0x400;
}

// 0x1000de2c (20 bytes)
void TbSoundSampleInfo::SetPitch(int32_t pitch) {
    mPitch = pitch;
    mFlags |= 0x20;
}

// 0x1000de74 (20 bytes)
void TbSoundSampleInfo::SetFrequency(float frequency) {
    mFrequency = frequency;
    mFlags |= 0x400000;
}

// 0x1000dec0 (20 bytes)
void TbSoundSampleInfo::SetPan(uint32_t pan) {
    mPan = pan;
    mFlags |= 0x10;
}

// 0x1000df08 (20 bytes): the flag first, then the number.
void TbSoundSampleInfo::SetNumber(uint32_t number) {
    mFlags |= 0x80;
    mNumber = number;
}

// The sound library's sample banks (file TbMMFileBank.cpp of the Mac's "sound symbols shared" library, with the factory and
// the bank handle from ChannelState.cpp). See include/TbSoundBank.h for the layouts, the vtables and the file format.
//
// Every allocation goes through the nothrow forms of operator new, new[] and delete[] (the library's own allocator pair on
// the Mac: __nw__FUl, __nwa__FUl and __dla__FPv), so that a failed allocation is a NULL result as it is there.

// ---- TbSoundSampleInfo -------------------------------------------------------------------------------------------------

// 0x1000df50 (20 bytes)
void TbSoundSampleInfo::SetBank(Bank::CBank* bank) {
    mFlags |= 8;
    mBank = bank;
}

// 0x1000dfa4 (20 bytes): the flag first, then the size.
void TbSoundSampleInfo::SetStreamingBufferSize(uint32_t size) {
    mFlags |= 0x8000;
    mStreamingBufferSize = size;
}

// 0x1000dffc (24 bytes): stores the address of the caller's control and the index.
void TbSoundSampleInfo::SetVolumeControl(const TbSoundVolumeControl& control, uint32_t index) {
    mVolumeControl = &control;
    mVolumeControlIndex = index;
    mFlags |= 0x40000;
}

// 0x1000e068 (112 bytes): twelve words copied into +0x50, then the flag.
void TbSoundSampleInfo::SetReverb(const TbSoundReverbInfo& reverb) {
    mReverb.mField0 = reverb.mField0;
    mReverb.mField4 = reverb.mField4;
    mReverb.mField8 = reverb.mField8;
    mReverb.mFieldC = reverb.mFieldC;
    mReverb.mField10 = reverb.mField10;
    mReverb.mField14 = reverb.mField14;
    mReverb.mField18 = reverb.mField18;
    mReverb.mField1C = reverb.mField1C;
    mReverb.mField20 = reverb.mField20;
    mReverb.mField24 = reverb.mField24;
    mReverb.mField28 = reverb.mField28;
    mReverb.mField2C = reverb.mField2C;
    mFlags |= 0x100000;
}
