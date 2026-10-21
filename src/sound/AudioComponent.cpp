// AudioComponent.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of AudioComponent code, named after it.

#include "AudioPlaceHolder.h"
#include "snd_handles.h"
#include "TbAudioBody.h"
#include "TbMapStreamer.h"
#include "TbSoundCore.h"
#include "TbSoundInfo.h"
#include <cstring>
#include <new>

// CAudioComponent.cpp: the component chain of the sound library (unit w79b, sound symbols shared): the pure interface's
// constructor and the chain link CAudioComponent. Its own file so that units that only hold a component by pointer (the sample
// banks, unit w72) can link it without the core's dependencies. See include/TbSoundCore.h.

CAudioComponent::CAudioComponent() : CAudioComponentAccess(), TbRefRelease(), mNext(nullptr), mPrevious(nullptr), mId(0) {}

// ---------------------------------------------------------------- CAudioComponent

CAudioComponentAccess::CAudioComponentAccess() {}

CAudioComponent::~CAudioComponent() {
    if (mNext != nullptr) mNext->Release();
}

TbError CAudioComponent::Init(const TbSoundSystemInfo& info) {
    if (mNext == nullptr) return TbError(-1);
    TbError result = mNext->Init(info);
    if (static_cast<int32_t>(result) < 0) mNext->UnchainAndRelease();
    return TbError(0);
}

TbError CAudioComponent::Chain(CAudioComponent* other) {
    if (other == nullptr) return TbError(-1);
    other->AddRef();
    // The old next is chained after `other` (its answer is not looked at).
    other->Chain(mNext);
    if (mNext != nullptr) mNext->Release();
    mNext = other;
    other->mPrevious = this;
    return TbError(0);
}

int32_t CAudioComponent::UnchainAndRelease() {
    if (mNext != nullptr) mNext->mPrevious = mPrevious;
    if (mPrevious != nullptr) mPrevious->mNext = mNext;
    mNext = nullptr;
    delete this;
    return 0;
}

CAudioComponent* CAudioComponent::GetComponent(uint32_t id) {
    if (id == mId) return this;
    return mNext != nullptr ? mNext->GetComponent(id) : nullptr;
}

// The forwarders call through the next component with no NULL test (a NULL next faults on the Mac).
TbError CAudioComponent::GetSystemInfo(TbSoundSystemInfo& info) { return mNext->GetSystemInfo(info); }
TbError CAudioComponent::GetSampleInfo(void* handle, TbSoundSampleInfo& info) { return mNext->GetSampleInfo(handle, info); }
void* CAudioComponent::NewSample(void* handle, TbSoundSampleInfo& info) { return mNext->NewSample(handle, info); }
void* CAudioComponent::NewControl(void* handle, TbSoundSampleInfo& info) { return mNext->NewControl(handle, info); }
TbSoundSystemModule* CAudioComponent::GetSystemModule() const { return mNext->GetSystemModule(); }
TbSoundSampleStore* CAudioComponent::GetSampleStore() const { return mNext->GetSampleStore(); }
TbError CAudioComponent::SystemCommand(const SystemCommand::Command& command) { return mNext->SystemCommand(command); }

// TbAudioBody.cpp: the audio object the game holds, decompiled from the sound library of SimTheme Park 2.04b3 ("sound symbols
// shared", Code at 0x10000000, TOC r2 = 0x1003abf0; file AudioPlaceHolder.cpp, with GetSoundManager, GetPlaceHolderList and
// CreateAudioObject in CSprocketsModule.cpp and the factory in LbMpeg.cpp). See include/TbAudioBody.h for the layout and the
// slots.

// ---- CAudioComponentFactory -----------------------------------------------------------------------------------------------

// 0x10001b54 (72)
TbSoundCore* CAudioComponentFactory::CreateCore() {
    return new TbSoundCore();
}

// 0x10001bd8 (72)
CAudioComponent* CAudioComponentFactory::CreateComponent() {
    return new CAudioComponent();
}

namespace {
uint8_t gHandleMaskInitialized; // sound 0x1003e320
uint32_t gHandleIndexMask; // sound 0x1003e324, cached on the first SetIndex

uint32_t ShiftLeft(uint32_t value, uint32_t count) {
    count &= 63u;
    return count < 32 ? value << count : 0;
}
}

void CUniqueHandle::SetIndex(uint16_t index) {
    if (gHandleMaskInitialized == 0) {
        gHandleIndexMask = ShiftLeft(1u, _mBitsPerCount + 1u) - 1u;
        gHandleMaskInitialized = 1;
    }
    // The original preserves one more bit than the count's 26-bit width.
    mValue = (mValue & gHandleIndexMask) + ShiftLeft(index, _mBitsPerCount);
}
