// TbSoundPipe.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbSoundPipe code, named after it.

#include "AudioPlaceHolder.h"
#include "CCodecFactory.h"
#include "LbTime.h"
#include "ppc_math.h"
#include "snd_handles.h"
#include "TbMpegBase.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundInfo.h"
#include "TbSoundPipe.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleCallback.h"
#include "TbSoundSampleMPEG2.h"
#include <cmath>
#include <cstring>
#include <new>

// The sound library's codec factory and the bank file wrapper (unit w80b, program "sound symbols shared"): CCodecFactory
// (LbMpeg.cpp) and _CFileAccess (LbMpeg.cpp). See include/CCodecFactory.h for the layouts.

// The controller is the completion callback pipe; the flag is cleared before it is made. A failed allocation answers NULL.
TbSoundSamplePipe* CCodecFactory::CreateController(TbSoundSampleInfo& info) {
    if ((info.mFlags & 0x2000) == 0) return nullptr;
    info.mFlags &= ~0x2000u;
    void* block = _TbSoundSampleCompletedCallback::operator new(sizeof(_TbSoundSampleCompletedCallback));
    _TbSoundSampleCompletedCallback* controller = nullptr;
    if (block != nullptr) {
        controller = ::new (block) _TbSoundSampleCompletedCallback(info.mCompletedCallback, info.mUserData);
    }
    return controller;
}

// The sample callback pipes of the sound library (unit w79i, program "sound symbols shared"): TbSoundUserCallback (UserCallback.cpp),
// _TbSoundSampleCallback and CAutoDetach (TbSoundSampleCallback.cpp), _TbSoundSampleCompletedCallback (TbSoundSamplePipe.cpp, its
// constructor and GetDataChunk in LbMpeg.cpp) and the three cleanup objects. See include/TbSoundSampleCallback.h for the layouts
// and the vtables.

// The user callback's constructor with the user data, then the callback. The pool links keep what the block held.
_TbSoundSampleCompletedCallback::_TbSoundSampleCompletedCallback(TbSndCallback* callback, void* userData)
    : TbSoundUserCallback(userData) {
    mCallback = callback;
}

// (The Mac's `this != NULL` test and its flag are the compiler's delete: see the header.)
TbSoundUserCallback::~TbSoundUserCallback() {}

// The sound library's sample pipe base (unit w78, program "sound symbols shared"): TbSoundSamplePipe (LbMpeg.cpp and
// TbSoundSamplePipe.cpp), TbSoundSPCodec (LbMpeg.cpp, StreamableMpeg2.cpp, TbSoundCodecLoop.cpp, TimerCodec.cpp) and
// TbSoundSampleCodec (StreamableMpeg2.cpp, TbSoundSampleCallback.cpp, TbSoundSampleMPEG2.cpp, TimerCodec.cpp). See
// include/TbSoundPipe.h for the layouts and the slot order.

// (The Mac's `this != NULL` test and its flag are the compiler's delete: see the header.)
TbSoundSamplePipe::~TbSoundSamplePipe() {}

// The pipe's constructor (the chain links and the handle), then the user data. The pool links keep what the block held.
TbSoundUserCallback::TbSoundUserCallback() {
    mUserData = nullptr;
}

TbSoundUserCallback::TbSoundUserCallback(void* userData) {
    mUserData = userData;
}

// ---------------------------------------------------------------- the pipe

// Only the three chain fields: the pool link and the vptr's other neighbours keep what the block held.
TbSoundSamplePipe::TbSoundSamplePipe() {
    mNext = nullptr;
    mPrevious = nullptr;
    mHandle = nullptr;
}

// The sample's header from the bank, when the info has a sample number (+0) and a bank (+4).
int32_t CCodecFactory::GetNextSampleFromBank(TbSampleHeader** header, TbSoundSampleInfo& info) {
    if (info.mNumber == 0 || info.mBank == nullptr) return 0;
    return info.mBank->RetrieveSample(header, info.mNumber);
}

// A loop in front of the codec when the loop flag is set and the count is above 1 (unsigned), made under the factory's lock.
TbSoundSamplePipe* CCodecFactory::CreateCodecModifier(TbSoundSampleInfo& info, TbSampleHeader*, TbSoundSamplePipe* pipe) {
    if ((info.mFlags & 0x40) != 0) {
        if (info.mLoopCount > 1) {
            TbSoundCriticalSection lock(static_cast<void**>(mLock));
            void* block = _TbSoundCodecLoop::operator new(sizeof(_TbSoundCodecLoop));
            _TbSoundCodecLoop* loop = nullptr;
            if (block != nullptr) loop = ::new (block) _TbSoundCodecLoop();
            pipe = pipe->AttachReplace(loop, nullptr);
        }
    }
    return pipe;
}

// The sound library's four small codecs (unit w80, program "sound symbols shared"): _TbSoundSampleWAV and
// _TbSoundSampleWAVFile (TbSoundSampleWAV.cpp, LbMpeg.cpp), _TbSoundCodecLoop (TbSoundCodecLoop.cpp) and TbTimerCodec
// (TimerCodec.cpp), with CMemoryCopy::Copy. See include/TbSoundCodecs.h for the layouts and the slot order.

_TbSoundCodecLoop::_TbSoundCodecLoop() {}

TbSoundSPCodec::~TbSoundSPCodec() {}

// ---------------------------------------------------------------- the SP codec

TbSoundSPCodec::TbSoundSPCodec() {}

TbSoundCriticalSection::~TbSoundCriticalSection() {
    if (mWord != nullptr) --*mWord;
}
TbSoundCriticalSection::TbSoundCriticalSection(void* word) : mWord(static_cast<uint32_t*>(word)) {
    if (mWord != nullptr) ++*mWord;
}
CAutoCriticalSection::operator void**() { return reinterpret_cast<void**>(&mValue); }

// The order of the original: the header, the channel's header and buffer size, the codec (MPEG when header flag 0x20 is set; WAV
// otherwise; the streaming kinds read the bank's file from the sample's position + 0x28, the others the memory), each made from
// its pool under the lock, then the modifiers (loop, callback), the handle and the info. Any failure releases the pipe that
// was made (the virtual Release, slot +0x10) and the channel's (ReleasePipe(false)) and answers NULL. On success the channel
// remembers whether the bank streams. A channel is not tested for NULL after the first test (GetNextHandle).
TbSoundSamplePipe* CCodecFactory::CreateCodec(TbSoundPipeChannel* channel, TbSoundSampleInfo& info) {
    TbSoundSamplePipe* pipe = nullptr;
    TbSampleHeader scratch = {};
    TbSampleHeader* header = &scratch;
    if ((GetNextSampleFromBank(&header, info) & 0xff) != 0 && header->mSize != 0) {
        if (channel != nullptr) {
            channel->SetSampleHeader(*header);
            if ((info.mFlags & 0x8000) != 0) channel->SetBufferSize(info.mStreamingBufferSize);
        }
        TbSoundSamplePipe* codec;
        if ((header->mFlags & 0x20) == 0) {
            TbSoundCriticalSection lock(static_cast<void**>(mLock));
            if (!info.mBank->IsStreaming()) {
                void* block = _TbSoundSampleWAV::operator new(sizeof(_TbSoundSampleWAV));
                _TbSoundSampleWAV* wav = nullptr;
                if (block != nullptr) {
                    TbRefRelease* owner = info.mBank->GetBankAsRefRelease();
                    wav = ::new (block) _TbSoundSampleWAV(header, owner);
                }
                codec = wav;
            } else {
                uint32_t position = info.mBank->GetSamplePosition(info.mNumber);
                void* block = _TbSoundSampleWAVFile::operator new(sizeof(_TbSoundSampleWAVFile));
                _TbSoundSampleWAVFile* wav = nullptr;
                if (block != nullptr) {
                    TbRefRelease* owner = info.mBank->GetBankAsRefRelease();
                    bfl::FileSystem::CFile* file = *info.mBank;
                    wav = ::new (block) _TbSoundSampleWAVFile(file, header, position + 0x28, owner);
                }
                codec = wav;
            }
        } else {
            TbSoundCriticalSection lock(static_cast<void**>(mLock));
            if (!info.mBank->IsStreaming()) {
                void* block = _TbSoundSampleMPEG2::operator new(sizeof(_TbSoundSampleMPEG2));
                _TbSoundSampleMPEG2* mpeg = nullptr;
                if (block != nullptr) {
                    TbRefRelease* owner = info.mBank->GetBankAsRefRelease();
                    mpeg = ::new (block) _TbSoundSampleMPEG2(header, owner);
                }
                codec = mpeg;
            } else {
                uint32_t position = info.mBank->GetSamplePosition(info.mNumber);
                void* block = _TbSoundSampleMPEG2File::operator new(sizeof(_TbSoundSampleMPEG2File));
                _TbSoundSampleMPEG2File* mpeg = nullptr;
                if (block != nullptr) {
                    TbRefRelease* owner = info.mBank->GetBankAsRefRelease();
                    bfl::FileSystem::CFile* file = *info.mBank;
                    mpeg = ::new (block) _TbSoundSampleMPEG2File(file, header, position + 0x28, owner);
                }
                codec = mpeg;
            }
        }
        if (codec != nullptr) {
            pipe = CreateCodecModifier(info, header, codec);
            if (pipe != nullptr) {
                if ((info.mFlags & 0x800) != 0) {
                    TbSoundCriticalSection lock(static_cast<void**>(mLock));
                    void* block = _TbSoundSampleCallback::operator new(sizeof(_TbSoundSampleCallback));
                    _TbSoundSampleCallback* callback = nullptr;
                    if (block != nullptr) callback = ::new (block) _TbSoundSampleCallback(info.mCallback);
                    pipe = pipe->AttachReplace(callback, nullptr);
                    info.mFlags &= ~0x800u;
                }
                uint32_t handle = channel->GetNextHandle();
                pipe->SetHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(handle)));
                TbError result = pipe->SetInfo(info);
                if (static_cast<int32_t>(result) >= 0) {
                    channel->mStreaming = info.mBank->IsStreaming();
                    return pipe;
                }
            }
        }
    }
    if (pipe != nullptr) pipe->Release();
    if (channel != nullptr) channel->ReleasePipe(false);
    return nullptr;
}

void* TbSoundSamplePipe::SetHandle(void* handle) {
    mHandle = handle;
    return handle;
}

// The sound library's pipe channel (unit w79c, program "sound symbols shared"): TbSoundPipeChannel, the head of a sample's pipe
// chain (TbSoundCodecLoop.cpp, LbMpeg.cpp, TbSoundSampleCallback.cpp, TbSoundSampleWAV.cpp, TimerCodec.cpp and
// CSprocketsModule.cpp hold its out-of-line copies). See include/TbSoundPipeChannel.h for the layout and the slot order.

// The handle's new value is in r3 (the listing says void).
uint32_t TbSoundPipeChannel::GetNextHandle() {
    return ++mHandle;
}

uint32_t CUniqueHandle::operator++() { return ++mValue; }

// The user callback's no-argument constructor (user data 0), then the callback. The pool links keep what the block held.
_TbSoundSampleCallback::_TbSoundSampleCallback(TbSndCallback* callback) : TbSoundUserCallback() {
    mCallback = callback;
}

_TbSoundSampleWAV::_TbSoundSampleWAV(TbSampleHeader* header, TbRefRelease* owner) : TbSoundSampleCodec(header, owner) {}

TbSoundSampleCodec::~TbSoundSampleCodec() {}

// Copies the header's five words but keeps the priority word (+0x48 = header +8): the priority survives a header change.
void TbSoundPipeChannel::SetSampleHeader(const TbSampleHeader& header) {
    const uint32_t priority = static_cast<uint32_t>(mSampleHeader.mField8);
    mSampleHeader = header;
    mSampleHeader.mField8 = static_cast<int32_t>(priority);
}

_TbSoundSampleCompletedCallback::~_TbSoundSampleCompletedCallback() {}

int32_t _TbSoundSampleCompletedCallback::GetDataChunk(void* buffer, int32_t size, int32_t& done, bool flag) {
    if (mNext == nullptr) return 0;
    return mNext->GetDataChunk(buffer, size, done, flag);
}

int32_t TbSoundSamplePipe::GetDataChunk(void*, int32_t, int32_t&, bool) {
    return -1;
}

uint32_t TbSoundSamplePipe::GetSampleRate() {
    if (mNext == nullptr) return 0;
    return mNext->GetSampleRate();
}

void TbSoundSamplePipe::Restart() {}

void TbSoundSamplePipe::SetDataStart(void* start) {
    if (mNext != nullptr) mNext->SetDataStart(start);
}

// The sound library's value classes (unit w77, program "sound symbols shared", Code at 0x10000000): TbSoundPosition,
// TbSoundDistance, TbSoundReverbInfo, TbSoundVolumeControl, TbSoundSampleInfo and TbSoundSystemInfo. Declared in
// include/TbSoundPosition.h and include/TbSoundInfo.h. TbSoundSampleInfo::SetBank (0x1000df50) is in src/TbSoundBank.cpp.

// ---- TbSoundReverbInfo -------------------------------------------------------------------------------------------------

// 0x100012ac (72 bytes): constants 0.0f (0x1003316c), 0.25f (0x10033168), 0.5f (0x10033164), 1.0f (0x10033160).
TbSoundReverbInfo::TbSoundReverbInfo() {
    mField0 = 0;
    mField4 = 0;
    mField8 = 0;
    mFieldC = 0;
    mField10 = 0.0f;
    mField14 = 0;
    mField18 = 0.0f;
    mField1C = 0;
    mField20 = 0.25f;
    mField24 = 0.5f;
    mField28 = 0;
    mField2C = 1.0f;
}
// ---- TbSoundDistance ---------------------------------------------------------------------------------------------------

// 0x10015950 (4 bytes, empty): the three floats stay as they were.
TbSoundDistance::TbSoundDistance() {}

// 0x10001324 (16 bytes)
TbSoundDistance::TbSoundDistance(float a, float b, float c) : mA(a), mB(b), mC(c) {}

// 0x10002280: operator tWAVEFORMATEX*(): fills the record, answers the address of its wave format part (this + 4).
uint8_t* CFormat_operator_tWAVEFORMATEX(CFormat* self) {
    self->FillWaveFormatEx();
    return reinterpret_cast<uint8_t*>(self) + 4;
}

// 0x10001f38: CFile::Read; impl is the access object at this+4. No access object: -1.
int32_t CFile_Read(FileAccessRead* impl, void* dest, uint32_t size) {
    if (impl == nullptr) return -1;
    return impl->Read(dest, size);
}

// 0x100015b8: TbError::TbError(long): stores the value, answers this.
int32_t* TbError_ctor(int32_t* self, int32_t value) {
    *self = value;
    return self;
}

// 0x10000b88: TbError::operator long() const.
int32_t TbError_operator_long(const int32_t* self) { return *self; }

// The runtime's operator new[] (0x1002ebfc), delete[] (0x1002ebcc) and delete (0x1002ea94). Weak so that a test library can
// replace them with recording fakes (the arena replaces the global operators for the whole program).
__attribute__((weak)) void* VoidArrayNew(size_t n) { return ::operator new[](n); }
__attribute__((weak)) void VoidArrayDelete(void* p) { ::operator delete[](p); }
__attribute__((weak)) void VoidObjectDelete(void* p) { ::operator delete(p); }

// 0x10007340: ~CAutoArrayPtr<void*>(short freeFlag): frees the array, and the object when the flag is positive.
void*** CAutoArrayPtrVoid_dtor(void*** self, int16_t freeFlag) {
    if (self != nullptr) {
        if (*self != nullptr) VoidArrayDelete(*self);
        if (freeFlag > 0) VoidObjectDelete(self);
    }
    return self;
}

// 0x100073d4: operator void**() const.
void** CAutoArrayPtrVoid_operator_ptr(void*** self) { return *self; }

// 0x100074a8: CAutoArrayPtr<void*>(unsigned count): new void*[count] (4 bytes an element on the Mac).
void*** CAutoArrayPtrVoid_ctor(void*** self, uint32_t count) {
    *self = static_cast<void**>(VoidArrayNew(static_cast<size_t>(count << 2)));
    return self;
}
