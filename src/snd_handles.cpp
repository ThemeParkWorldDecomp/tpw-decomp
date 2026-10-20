#include "snd_handles.h"
#include <new>

void* TbSpeaker::AutoDelete::_mpSpeaker = nullptr;
uint32_t CUniqueHandle::_mBitsPerCount = 26;
CChannelState CChannelState::mChannelState;

namespace {
uint8_t gHandleMaskInitialized; // sound 0x1003e320
uint32_t gHandleIndexMask; // sound 0x1003e324, cached on the first SetIndex

uint32_t ShiftLeft(uint32_t value, uint32_t count) {
    count &= 63u;
    return count < 32 ? value << count : 0;
}
}

TbSpeaker::AutoDelete::~AutoDelete() {
    if (_mpSpeaker != nullptr) {
        ::operator delete(_mpSpeaker);
        _mpSpeaker = nullptr;
    }
}

CUniqueHandle::CUniqueHandle() : mValue(0) {}
CUniqueHandle::CUniqueHandle(void* value) : mValue(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value))) {}
CUniqueHandle::operator void*() const { return reinterpret_cast<void*>(static_cast<uintptr_t>(mValue)); }
uint32_t CUniqueHandle::GetIndex() const {
    uint32_t shift = _mBitsPerCount & 63u;
    return shift < 32 ? mValue >> shift : 0;
}
void CUniqueHandle::SetIndex(uint16_t index) {
    if (gHandleMaskInitialized == 0) {
        gHandleIndexMask = ShiftLeft(1u, _mBitsPerCount + 1u) - 1u;
        gHandleMaskInitialized = 1;
    }
    // The original preserves one more bit than the count's 26-bit width.
    mValue = (mValue & gHandleIndexMask) + ShiftLeft(index, _mBitsPerCount);
}
uint32_t CUniqueHandle::operator++() { return ++mValue; }

CAtomicValue::CAtomicValue() : mValue(0) {}
int32_t CAtomicValue::operator++() {
    mValue = static_cast<int32_t>(static_cast<uint32_t>(mValue) + 1u);
    return mValue;
}
int32_t CAtomicValue::operator--() {
    mValue = static_cast<int32_t>(static_cast<uint32_t>(mValue) - 1u);
    return mValue;
}
CAtomicValue::operator int32_t() const { return mValue; }
CChannelState::CChannelState() : mCount() {}
CChannelState* CChannelState::Instance() { return &mChannelState; }
void CChannelState::AddChannel() { ++mCount; }
void CChannelState::RemoveChannel() { --mCount; }
int32_t CChannelState::GetNumberOfChannelsInUse() { return mCount; }

CLowest::CLowest() : mChannel(nullptr), mPriority(0xffffffffu) {}
void CLowest::operator()(uint32_t priority, TbSoundPipeChannel* channel) {
    if (priority < mPriority) {
        mPriority = priority;
        mChannel = channel;
    }
}

CAutoCriticalSection::CAutoCriticalSection() : mValue(0) {}
CAutoCriticalSection::~CAutoCriticalSection() = default;
CAutoCriticalSection::operator void**() { return reinterpret_cast<void**>(&mValue); }
TbSoundCriticalSection::TbSoundCriticalSection(void* word) : mWord(static_cast<uint32_t*>(word)) {
    if (mWord != nullptr) ++*mWord;
}
TbSoundCriticalSection::~TbSoundCriticalSection() {
    if (mWord != nullptr) --*mWord;
}
