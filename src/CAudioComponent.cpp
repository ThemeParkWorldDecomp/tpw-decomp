// CAudioComponent.cpp: the component chain of the sound library (unit w79b, sound symbols shared): the pure interface's
// constructor and the chain link CAudioComponent. Its own file so that units that only hold a component by pointer (the sample
// banks, unit w72) can link it without the core's dependencies. See include/TbSoundCore.h.
#include "TbSoundCore.h"

// ---------------------------------------------------------------- CAudioComponent

CAudioComponentAccess::CAudioComponentAccess() {}

CAudioComponent::CAudioComponent() : CAudioComponentAccess(), TbRefRelease(), mNext(nullptr), mPrevious(nullptr), mId(0) {}

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
