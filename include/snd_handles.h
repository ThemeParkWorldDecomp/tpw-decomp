// Sound channel values, sound symbols shared. Offsets are the Mac's; host pointers are native.
#pragma once
#include <cstdint>

class TbSoundPipeChannel;

namespace TbSpeaker {
class AutoDelete {
public:
    ~AutoDelete(); // 0x1001493c (120), raw delete of the shared pointer
    static void* _mpSpeaker; // 0x1003f348
};
}

class CUniqueHandle {
public:
    CUniqueHandle(); // 0x10009de0 (12)
    explicit CUniqueHandle(void* value); // 0x10009f34 (8)
    operator void*() const; // 0x1000a164 (8)
    uint32_t GetIndex() const; // 0x1000a1f4 (20)
    void SetIndex(uint16_t index); // 0x10001c60 (96)
    uint32_t operator++(); // 0x10000c54 (20)
    static uint32_t _mBitsPerCount; // 0x10035c88, initially 26
    uint32_t mValue; // +0, size 4
};

class CAtomicValue {
public:
    CAtomicValue(); // 0x10014dc8 (12), a plain word on the Mac
    int32_t operator++(); // 0x10010df4 (20)
    int32_t operator--(); // 0x1000a93c (20)
    operator int32_t() const; // 0x1000be34 (8), Mac long
    int32_t mValue; // +0
};

class CChannelState {
public:
    CChannelState(); // 0x10014d6c (48)
    static CChannelState* Instance(); // 0x1000a97c (8)
    void AddChannel(); // 0x10010da4 (32)
    void RemoveChannel(); // 0x1000a8e8 (32)
    int32_t GetNumberOfChannelsInUse(); // 0x1000bdd4 (32)
    static CChannelState mChannelState; // 0x1003f350
    CAtomicValue mCount; // +0, size 4
};

class CLowest {
public:
    CLowest(); // 0x10010904 (20)
    void operator()(uint32_t priority, TbSoundPipeChannel* channel); // 0x10010804 (24)
    TbSoundPipeChannel* mChannel; // +0
    uint32_t mPriority; // +4, size 8
};

class CAutoCriticalSection {
public:
    CAutoCriticalSection(); // 0x10009c20 (12)
    ~CAutoCriticalSection(); // 0x10009ba8 (68)
    operator void**(); // 0x1000078c (4), the address of the nesting word
    uint32_t mValue; // +0, size 4, never a host pointer
};

class TbSoundCriticalSection {
public:
    explicit TbSoundCriticalSection(void* word); // 0x10000738 (32)
    ~TbSoundCriticalSection(); // 0x100006a8 (92)
    uint32_t* mWord; // +0, size 4 on the Mac
};

// The two emitted unsigned-long copies are 0x1000fdf0 and 0x1001bc00 (24 each).
// LB_MIN<unsigned int,unsigned int> is 0x100081c4 (24, StreamableMpeg2.cpp; unit v5L21 test: test_TbSpeaker_Mono_operator_int).
template<class T, class U> const T& LB_MIN(const T& a, const U& b) {
    return a < b ? a : b;
}

// Unit v51075: __sinit_ChannelState_cpp 0x10014d1c (36): constructs CChannelState::mChannelState (0x1003f350).
void __sinit_ChannelState_cpp();
