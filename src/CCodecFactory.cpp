// The sound library's codec factory and the bank file wrapper (unit w80b, program "sound symbols shared"): CCodecFactory
// (LbMpeg.cpp) and _CFileAccess (LbMpeg.cpp). See include/CCodecFactory.h for the layouts.
#include "CCodecFactory.h"

#include <new>

#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleCallback.h"
#include "TbSoundSampleMPEG2.h"

// ---------------------------------------------------------------- CCodecFactory

CCodecFactory::CCodecFactory() {}

CCodecFactory::~CCodecFactory() {}

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

// ---------------------------------------------------------------- _CFileAccess

// The position and the start first, then the file member is copy constructed from the file given (no test for NULL: the
// original dereferences it), then the semaphore.
_CFileAccess::_CFileAccess(bfl::FileSystem::CFile* file) : mPosition(0), mStart(0), mFile(*file) {}

_CFileAccess::_CFileAccess(bfl::FileSystem::CFile* file, uint32_t start) : mPosition(0), mStart(start), mFile(*file) {}

_CFileAccess::~_CFileAccess() {}

// Under the semaphore: when the file is not where the position says (start + position) the position is set again through the
// virtual Seek (slot +0x10); then the file reads and the position moves by what it answered (-1 included).
int32_t _CFileAccess::Read(void* buffer, uint32_t count) {
    _LbSemaLock lock(mSemaphore);
    lock.Lock(1);
    if (static_cast<int32_t>(mStart) + mPosition != mFile.GetPos()) Seek(mPosition);
    int32_t got = mFile.Read(buffer, count);
    mPosition = mPosition + got;
    return mPosition;
}

void _CFileAccess::Seek(int32_t offset) {
    int32_t position = mFile.Seek(mStart + static_cast<uint32_t>(offset), 0);
    mPosition = position - static_cast<int32_t>(mStart);
}

void _CFileAccess::SeekToDataStart() {
    mPosition = 0;
    mFile.Seek(mStart, 0);
}
