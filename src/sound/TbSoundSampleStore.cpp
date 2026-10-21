// TbSoundSampleStore.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbSoundSampleStore code, named after it.

#include "AudioPlaceHolder.h"
#include "CCodecFactory.h"
#include "CSprocketsModule.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "snd_handles.h"
#include "TbAudioBody.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundCore.h"
#include "TbSoundInfo.h"
#include "TbSoundManager.h"
#include "TbSoundPipe.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleCallback.h"
#include "TbSoundSampleMPEG2.h"
#include "TbSoundSampleStore.h"
#include <cmath>
#include <cstring>
#include <new>

// The sample callback pipes of the sound library (unit w79i, program "sound symbols shared"): TbSoundUserCallback (UserCallback.cpp),
// _TbSoundSampleCallback and CAutoDetach (TbSoundSampleCallback.cpp), _TbSoundSampleCompletedCallback (TbSoundSamplePipe.cpp, its
// constructor and GetDataChunk in LbMpeg.cpp) and the three cleanup objects. See include/TbSoundSampleCallback.h for the layouts
// and the vtables.

_CS__TbSoundSampleCompletedCallback::~_CS__TbSoundSampleCompletedCallback() { _TbSoundSampleCompletedCallback::ClearCache(); }

namespace {
// The static objects the Mac's initialisers and finalisers run.
_CS_TbSoundUserCallback sCleanupUserCallback;
_CS__TbSoundSampleCallback sCleanupSampleCallback;
_CS__TbSoundSampleCompletedCallback sCleanupCompletedCallback;
}  // namespace

// The sound library's sample store (unit w79c, program "sound symbols shared"): TbSoundSampleStore with its two channel
// iterators (TbSoundSampleWAV.cpp and TbSoundSampleCallback.cpp, AudioPlaceHolder.cpp hold the out-of-line copies). The
// store's array of channels is the CAutoArrayPtr<TbSoundPipeChannel> instance (0x1000fb94, 0x1000faec, 0x1000fd90), which is
// include/snd_containers.h's template. See include/TbSoundSampleStore.h for the layout.

// ---- the store

TbSoundSampleStore::TbSoundSampleStore(TbSoundCore* core, uint32_t maxChannels)
    : mCore(core), mMaxChannels(maxChannels), mChannelsToUse(0), mChannels(maxChannels) {}

// The used count is the smaller of the request and the maximum (LB_MIN answers the address of the chosen one), set before the
// channels get their index (a halfword loop counter) in their handles.
TbError TbSoundSampleStore::Init(uint32_t channelsToUse) {
    mChannelsToUse = LB_MIN(channelsToUse, mMaxChannels);
    for (uint16_t i = 0; i < mMaxChannels; ++i) {
        static_cast<TbSoundPipeChannel*>(mChannels)[i].SetIndex(i);
    }
    return TbError(0);
}

// The walk starts at the first channel even when the used count is 0 (see the iterator).
int32_t TbSoundSampleStore::ReleaseAllPipes() {
    CChannelIterator it(this);
    int32_t count = 0;
    for (TbSoundPipeChannel* channel = it; channel != nullptr; channel = ++it) {
        count += static_cast<uint8_t>(channel->ReleasePipe(false));
    }
    return count;
}

TbSoundPipeChannel* TbSoundSampleStore::CChannelIterator::operator++() {
    ++mCurrent;
    if (mCurrent < mEnd) return mCurrent;
    return nullptr;
}

TbSoundSampleStore::CChannelIterator::operator TbSoundPipeChannel*() {
    return mCurrent;
}

// ---- the iterators

TbSoundSampleStore::CChannelIterator::CChannelIterator(TbSoundSampleStore* store)
    : mCurrent(static_cast<TbSoundPipeChannel*>(store->mChannels)), mEnd(nullptr) {
    mEnd = mCurrent + store->mChannelsToUse;
}

// Releases every channel's pipe, then the array goes (each channel's destructor, last to first).
TbSoundSampleStore::~TbSoundSampleStore() {
    ReleaseAllPipes();
}

// No bounds check.
TbSoundPipeChannel* TbSoundSampleStore::operator[](uint32_t index) const {
    return static_cast<TbSoundPipeChannel*>(mChannels) + index;
}

// The handle's index picks the channel (no bounds check); the channel answers only when its chain holds the handle.
TbSoundPipeChannel* TbSoundSampleStore::operator[](const CUniqueHandle& handle) const {
    const uint32_t index = handle.GetIndex();
    TbSoundPipeChannel* channel = static_cast<TbSoundPipeChannel*>(mChannels) + index;
    if (channel->FindPipe(handle) == nullptr) return nullptr;
    return channel;
}

TbSoundPipeChannel* TbSoundSampleStore::FindChannelAndPipe(const CUniqueHandle& handle, TbSoundSamplePipe*& pipe) {
    const uint32_t index = handle.GetIndex();
    TbSoundPipeChannel* channel = static_cast<TbSoundPipeChannel*>(mChannels) + index;
    pipe = channel->FindPipe(handle);
    if (pipe == nullptr) return nullptr;
    return channel;
}

// The index is written whether or not the channel holds the handle.
bool TbSoundSampleStore::GetValidChannelIndexFromHandle(const CUniqueHandle& handle, uint32_t& index) {
    index = handle.GetIndex();
    TbSoundPipeChannel* channel = static_cast<TbSoundPipeChannel*>(mChannels) + index;
    return channel->FindPipe(handle) != nullptr;
}

void TbSoundSampleStore::StopAllChannels(bool flag) {
    CChannelIterator it(this);
    for (TbSoundPipeChannel* channel = it; channel != nullptr; channel = ++it) {
        mCore->StopAndReleasePipe(channel, flag);
    }
}

void TbSoundSampleStore::StopAllChannelsWithVolumeControl(uint32_t mask) {
    CChannelIterator it(this);
    for (TbSoundPipeChannel* channel = it; channel != nullptr; channel = ++it) {
        if (channel->ControlledBy(mask) != 0) mCore->StopAndReleasePipe(channel, false);
    }
}

// The sound library's pipe channel (unit w79c, program "sound symbols shared"): TbSoundPipeChannel, the head of a sample's pipe
// chain (TbSoundCodecLoop.cpp, LbMpeg.cpp, TbSoundSampleCallback.cpp, TbSoundSampleWAV.cpp, TimerCodec.cpp and
// CSprocketsModule.cpp hold its out-of-line copies). See include/TbSoundPipeChannel.h for the layout and the slot order.

uint32_t TbSoundPipeChannel::ControlledBy(uint32_t mask) const {
    return mask & mControlMask;
}

// Every channel in use is stopped with the flag true (the count is still the old one), then the new count is set.
void TbSoundSampleStore::SetNumberOfChannelsToUse(const TbSoundSystemInfo& info) {
    const uint32_t wanted = info.GetBuffersToUse();
    const uint32_t count = LB_MIN(wanted, mMaxChannels);
    StopAllChannels(true);
    mChannelsToUse = count;
}

// The sound library's value classes (unit w77, program "sound symbols shared", Code at 0x10000000): TbSoundPosition,
// TbSoundDistance, TbSoundReverbInfo, TbSoundVolumeControl, TbSoundSampleInfo and TbSoundSystemInfo. Declared in
// include/TbSoundPosition.h and include/TbSoundInfo.h. TbSoundSampleInfo::SetBank (0x1000df50) is in src/TbSoundBank.cpp.

// 0x1001061c (8 bytes)
uint32_t TbSoundSystemInfo::GetBuffersToUse() const {
    return mBuffersToUse;
}

// The first channel that is free is claimed and answered. When none is, the one with the lowest priority (the first of equal
// ones) is stolen if its priority is strictly lower than the request's: it is stopped (flag true), the request's word +0x10
// gets its handle and it is claimed again. Otherwise NULL.
TbSoundPipeChannel* TbSoundSampleStore::GetFreeChannel(TbSoundSampleInfo& info) {
    CLowest lowest;
    CUsedChannelIterator it(this);
    for (TbSoundPipeChannel* channel = it; channel != nullptr; channel = ++it) {
        uint32_t used;
        uint32_t priority;
        channel->GetChannelInfo(used, priority);
        if (used == 0) {
            channel->mInUse = 1;
            return channel;
        }
        lowest(priority, channel);
    }
    if (lowest.mPriority < info.mPriority) {
        mCore->StopAndReleasePipe(lowest.mChannel, true);
        info.mField10 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lowest.mChannel->GetHandle()));
        lowest.mChannel->mInUse = 1;
        return lowest.mChannel;
    }
    return nullptr;
}

void* TbSoundPipeChannel::GetHandle() {
    return static_cast<void*>(mHandle);
}

// The base has set the end already; it is set to the same sum again.
TbSoundSampleStore::CUsedChannelIterator::CUsedChannelIterator(TbSoundSampleStore* store) : CChannelIterator(store) {
    mEnd = mCurrent + store->mChannelsToUse;
}

CLowest::CLowest() : mChannel(nullptr), mPriority(0xffffffffu) {}

// Needs the number (0x80) and the bank (8) in the request. The factory (a stack temporary) makes the codec for the pipe the
// handle names, or for a free channel; the codec is attached behind that pipe and the channel is initialised from the request.
// The factory's destructor runs on every exit.
void* TbSoundSampleStore::CreateNewSample(void* handle, TbSoundSampleInfo& info, uint32_t& index) {
    if ((info.mFlags & 0x80) == 0 || (info.mFlags & 8) == 0) return nullptr;
    CCodecFactory factory;
    TbSoundPipeChannel* channel;
    TbSoundSamplePipe* pipe = GetPipesFromHandle(handle, &channel, info);
    if (pipe != nullptr) {
        TbSoundSamplePipe* codec = factory.CreateCodec(channel, info);
        if (pipe->Attatch(codec) != nullptr) return channel->InitChannelInfo(info, index);
    }
    return nullptr;
}

// The sound library's codec factory and the bank file wrapper (unit w80b, program "sound symbols shared"): CCodecFactory
// (LbMpeg.cpp) and _CFileAccess (LbMpeg.cpp). See include/CCodecFactory.h for the layouts.

CCodecFactory::~CCodecFactory() {}
// ---------------------------------------------------------------- CCodecFactory

CCodecFactory::CCodecFactory() {}

// Needs the completed callback flag (0x2000). The pipe is found before the factory is made; the controller is attached behind
// it and the pipe the attach answered gets the channel's next handle.
void* TbSoundSampleStore::CreateNewControl(void* handle, TbSoundSampleInfo& info) {
    if ((info.mFlags & 0x2000) == 0) return nullptr;
    TbSoundPipeChannel* channel;
    TbSoundSamplePipe* pipe = GetPipesFromHandle(handle, &channel, info);
    if (pipe == nullptr) return nullptr;
    CCodecFactory factory;
    TbSoundSamplePipe* controller = factory.CreateController(info);
    TbSoundSamplePipe* attached = pipe->Attatch(controller);
    if (attached == nullptr) return nullptr;
    return attached->SetHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(channel->GetNextHandle())));
}
// No handle: a free channel (counted in the channel state, with the request's priority) is the pipe. A handle: the channel
// and pipe it names, or NULL (and the channel NULL) when nothing answers to it.
TbSoundSamplePipe* TbSoundSampleStore::GetPipesFromHandle(void* handle, TbSoundPipeChannel** channel, TbSoundSampleInfo& info) {
    if (handle == nullptr) {
        *channel = GetFreeChannel(info);
        if (*channel != nullptr) {
            (*channel)->SetPriority(info.mPriority);
            CChannelState::Instance()->AddChannel();
        }
        return *channel;
    }
    TbSoundSamplePipe* pipe = nullptr;
    *channel = FindChannelAndPipe(CUniqueHandle(handle), pipe);
    if (*channel != nullptr) return pipe;
    return *channel;
}

void CChannelState::AddChannel() { ++mCount; }

// Unit v51113: the small sound-library functions at the edges of CSprocketsModule.cpp (sound 0x10023310 to 0x1002e8fc) and the
// float square roots / cleanup objects of the codec files.

using namespace SamsUtilities;

// ---- the cleanup objects
_CS_TbSoundSamplePipe* _CS_TbSoundSamplePipe::Destruct(_CS_TbSoundSamplePipe* self, int16_t deleteFlag) {   // 0x1000f9dc
    if (self != nullptr) {
        TbSoundSamplePipe::ClearCache();
        if (deleteFlag > 0) USoundChannel_Free(self);
    }
    return self;
}
