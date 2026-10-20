// The sound library's pipe channel (unit w79c, program "sound symbols shared"): TbSoundPipeChannel, the head of a sample's pipe
// chain (TbSoundCodecLoop.cpp, LbMpeg.cpp, TbSoundSampleCallback.cpp, TbSoundSampleWAV.cpp, TimerCodec.cpp and
// CSprocketsModule.cpp hold its out-of-line copies). See include/TbSoundPipeChannel.h for the layout and the slot order.
#include "TbSoundPipeChannel.h"

#include <cstring>

#include "TbSoundBank.h"

// Sets only the members the Mac does: not mSampleFlags, mField2C, mServiceResult or the header words but the priority. The
// service buffer is 0x1220 bytes and mAlignedBuffer is its first 32-byte aligned address (up to 0x1f bytes in).
TbSoundPipeChannel::TbSoundPipeChannel()
    : mInUse(0),
      mVolume(0),
      mFlags(0),
      mBufferSize(0),
      mControlMask(0),
      mHandle(),
      mVolumeControl(),
      mServiceBuffer(0x1220u),
      mAlignedBuffer(nullptr),
      mLock() {
    const uintptr_t start = reinterpret_cast<uintptr_t>(static_cast<unsigned char*>(mServiceBuffer));
    mAlignedBuffer = reinterpret_cast<unsigned char*>((start + 0x1f) & ~static_cast<uintptr_t>(0x1f));
    SetPriority(0);
}

// The members go in the reverse order (the lock, the service buffer, the pipe); the deleting form frees the object through the
// pipe's pool delete with the channel's Mac size 0x60, which is not the pipe's size, so the runtime's delete takes it.
TbSoundPipeChannel::~TbSoundPipeChannel() {}

// The handle's new value is in r3 (the listing says void).
uint32_t TbSoundPipeChannel::GetNextHandle() {
    return ++mHandle;
}

// Copies the header's five words but keeps the priority word (+0x48 = header +8): the priority survives a header change.
void TbSoundPipeChannel::SetSampleHeader(const TbSampleHeader& header) {
    const uint32_t priority = static_cast<uint32_t>(mSampleHeader.mField8);
    mSampleHeader = header;
    mSampleHeader.mField8 = static_cast<int32_t>(priority);
}

void TbSoundPipeChannel::SetPriority(uint32_t priority) {
    mSampleHeader.mField8 = static_cast<int32_t>(priority);
}

uint32_t TbSoundPipeChannel::GetPriority() {
    return static_cast<uint32_t>(mSampleHeader.mField8);
}

void TbSoundPipeChannel::SetVolume(uint32_t volume) {
    mVolume = volume;
}

uint32_t TbSoundPipeChannel::GetActualVolume() const {
    return mVolume;
}

uint32_t TbSoundPipeChannel::GetSampleFlags() {
    return mSampleFlags;
}

TbSampleHeader* TbSoundPipeChannel::GetSampleHeader() {
    return &mSampleHeader;
}

uint32_t TbSoundPipeChannel::ControlledBy(uint32_t mask) const {
    return mask & mControlMask;
}

void* TbSoundPipeChannel::GetHandle() {
    return static_cast<void*>(mHandle);
}

uint32_t TbSoundPipeChannel::GetIndex() {
    return mHandle.GetIndex();
}

void TbSoundPipeChannel::SetIndex(uint16_t index) {
    mHandle.SetIndex(index);
}

// Finds the pipe behind this channel with the handle (through the new slot +0x3c) and asks it for its info (slot +0x2c); a
// handle nothing answers to gives -1.
TbError TbSoundPipeChannel::SetPipeInfo(void* handle, TbSoundSampleInfo& info) {
    TbSoundCriticalSection lock(static_cast<void**>(mLock));
    TbSoundSamplePipe* pipe = FindPipe(CUniqueHandle(handle));
    if (pipe == nullptr) return TbError(-1);
    return pipe->SetInfo(info);
}

// used is 0 only for a channel that is neither claimed (mInUse) nor holding a pipe.
void TbSoundPipeChannel::GetChannelInfo(uint32_t& used, uint32_t& priority) {
    TbSoundCriticalSection lock(static_cast<void**>(mLock));
    used = 1;
    if (mInUse == 0 && mNext == nullptr) used = 0;
    priority = GetPriority();
}

// A channel that does not carry a sample yet takes the info's word +0xc as its sample flags and no controls; the info's
// priority (its bit is cleared in the info afterwards), volume and volume control (two words copied, and the mask from +0x84)
// follow. The handle's value is the answer (r3 of the last call, CUniqueHandle::operator void*).
void* TbSoundPipeChannel::InitChannelInfo(TbSoundSampleInfo& info, uint32_t& index) {
    if ((mFlags & 2) == 0) {
        mControlMask = 0;
        mSampleFlags = info.mField0C;
        mFlags = 2;
    }
    if ((info.mFlags & 0x400) != 0) {
        SetPriority(info.mPriority);
        info.mFlags &= ~0x400u;
    }
    if ((info.mFlags & 2) != 0) SetVolume(info.mVolume);
    if ((info.mFlags & 0x40000) != 0) {
        mVolumeControl = *info.mVolumeControl;
        mControlMask = info.mVolumeControlIndex;
    }
    index = GetIndex();
    return static_cast<void*>(mHandle);
}

// The base's SetInfo (its answer dropped) down the chain, then the priority when the info carries one; always success.
TbError TbSoundPipeChannel::SetInfo(const TbSoundSampleInfo& info) {
    TbSoundCriticalSection lock(static_cast<void**>(mLock));
    TbSoundSamplePipe::SetInfo(info);
    if ((info.mFlags & 0x400) != 0) SetPriority(info.mPriority);
    return TbError(0);
}

// A static: the streamer calls it with the buffer, its size and the pipe (the listing's labels are wrong). The pipe's
// GetDataChunk (slot +0x24) is asked with a done word of 0 and the flag false; the answer is true unless it was 1.
bool TbSoundPipeChannel::StreamerCallback(void* buffer, uint32_t size, void* pipe) {
    int32_t done = 0;
    const int32_t result = static_cast<TbSoundSamplePipe*>(pipe)->GetDataChunk(buffer, static_cast<int32_t>(size), done, false);
    return result != 1;
}

// Fills the aligned service buffer through the channel's own GetDataChunk (slot +0x24); mServiceResult is 0 when the chunk
// answered 1 (the sample is over) and 1 otherwise.
uint32_t TbSoundPipeChannel::FillServiceBuffer() {
    int32_t done;
    const int32_t result = GetDataChunk(mAlignedBuffer, 0x1200, done, false);
    mServiceResult = result != 1 ? 1u : 0u;
    return mServiceResult;
}

// Under the lock: the codec behind the channel is asked for the chunk; when it answers 1 it is released (ReleasePipe) and
// the answer is 1. In both cases the rest of the buffer after what was written is zeroed (the length is size - done, which
// is a huge unsigned count when done is more than size).
int32_t TbSoundPipeChannel::GetDataChunk(void* buffer, int32_t size, int32_t& done, bool) {
    TbSoundCriticalSection lock(static_cast<void**>(mLock));
    done = 0;
    if (mNext != nullptr && mNext->GetDataChunk(buffer, size, done, false) == 1) {
        ReleasePipe(false);
        memset(static_cast<unsigned char*>(buffer) + done, 0, static_cast<uint32_t>(size) - static_cast<uint32_t>(done));
        return 1;
    }
    memset(static_cast<unsigned char*>(buffer) + done, 0, static_cast<uint32_t>(size) - static_cast<uint32_t>(done));
    return 0;
}

TbSoundSamplePipe* TbSoundPipeChannel::FindPipe(const CUniqueHandle& handle) {
    TbSoundCriticalSection lock(static_cast<void**>(mLock));
    return TbSoundSamplePipe::FindPipe(static_cast<void*>(handle));
}

TbSoundSamplePipe* TbSoundPipeChannel::Attatch(TbSoundSamplePipe* pipe) {
    TbSoundCriticalSection lock(static_cast<void**>(mLock));
    return TbSoundSamplePipe::Attatch(pipe);
}

TbSoundSamplePipe* TbSoundPipeChannel::GetChannelUsed() {
    TbSoundCriticalSection lock(static_cast<void**>(mLock));
    return mNext;
}

// Under the lock: a channel with a pipe tells it (this->Completed(flag), the channel's own slot +0x34 forwards down the chain)
// and releases it (mNext->Release(), slot +0x10), forgets it and counts the channel out. Then in every case the priority is 0,
// the "carries a sample" bit is cleared and the channel is not in use. Answers whether there was a pipe.
bool TbSoundPipeChannel::ReleasePipe(bool flag) {
    TbSoundCriticalSection lock(static_cast<void**>(mLock));
    const bool had = mNext != nullptr;
    if (had) {
        Completed(flag);
        mNext->Release();
        mNext = nullptr;
        CChannelState::Instance()->RemoveChannel();
    }
    SetPriority(0);
    mFlags &= ~2u;
    mInUse = 0;
    return had;
}

// Without controls the volume is the stored one. With any control bit (0 to 3) the volume is scaled in single precision:
// the product of 1.0f and the control's values, then the volume (a word, rounded to single by the Mac's fsubs) times the
// product (single again), converted by the runtime's double to unsigned long.
uint32_t TbSoundPipeChannel::GetVolume() {
    uint32_t volume = mVolume;
    if (mControlMask != 0) {
        float product = 1.0f;
        if ((mControlMask & 1) != 0) product = product * static_cast<float>(mVolumeControl.GetVolume(0));
        if ((mControlMask & 2) != 0) product = product * static_cast<float>(mVolumeControl.GetVolume(1));
        if ((mControlMask & 4) != 0) product = product * static_cast<float>(mVolumeControl.GetVolume(2));
        if ((mControlMask & 8) != 0) product = product * static_cast<float>(mVolumeControl.GetVolume(3));
        const float base = static_cast<float>(static_cast<double>(volume));
        volume = CvtDblUl(static_cast<double>(base * product));
    }
    return volume;
}
