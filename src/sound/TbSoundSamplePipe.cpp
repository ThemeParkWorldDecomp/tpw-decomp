// TbSoundSamplePipe.cpp: name proven by __sinit_TbSoundSamplePipe_cpp in the sound library.

#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "bfl_FileSystem.h"
#include "CMpegBase.h"
#include "CSprocketsModule.h"
#include "helpers/TbSoundSampleCallbackHelpers.h"
#include "LbTime.h"
#include "platform/samsutils.h"
#include "snd_commands.h"
#include "snd_containers.h"
#include "snd_handles.h"
#include "TbAudioBody.h"
#include "TbAudioSystem.h"
#include "TbMapStreamer.h"
#include "TbMpegBase.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundInfo.h"
#include "TbSoundManager.h"
#include "TbSoundPipe.h"
#include "TbSoundSampleCallback.h"
#include "TbSoundSampleMPEG2.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>

// The sound library's MPEG sample codecs (unit w80b, program "sound symbols shared"): _TbSoundSampleMPEG2
// (TbSoundSampleMPEG2.cpp) and _TbSoundSampleMPEG2File (StreamableMpeg2.cpp). See include/TbSoundSampleMPEG2.h for the layouts
// and the slot map.

// ---------------------------------------------------------------- the static cleanup objects

_CS__TbSoundSampleMPEG2::~_CS__TbSoundSampleMPEG2() { _TbSoundSampleMPEG2::ClearCache(); }

// The sound library's sample pipe base (unit w78, program "sound symbols shared"): TbSoundSamplePipe (LbMpeg.cpp and
// TbSoundSamplePipe.cpp), TbSoundSPCodec (LbMpeg.cpp, StreamableMpeg2.cpp, TbSoundCodecLoop.cpp, TimerCodec.cpp) and
// TbSoundSampleCodec (StreamableMpeg2.cpp, TbSoundSampleCallback.cpp, TbSoundSampleMPEG2.cpp, TimerCodec.cpp). See
// include/TbSoundPipe.h for the layouts and the slot order.

TbSoundSamplePipe* TbSoundSamplePipe::mFirstTbSoundSamplePipe = nullptr;
int32_t TbSoundSamplePipe::mCurrentTbSoundSamplePipe = 0;
int32_t TbSoundSamplePipe::mLowTbSoundSamplePipe = 0;
int32_t TbSoundSamplePipe::mHighTbSoundSamplePipe = 0;
int32_t TbSoundSamplePipe::mTotalTbSoundSamplePipe = 0;
int32_t TbSoundSamplePipe::mLevelTbSoundSamplePipe = 10;

// Frees the idle objects (a plain delete, not through the list) and zeroes the current and total counts only: the low and
// high marks stay.
void TbSoundSamplePipe::ClearCache() {
    while (mFirstTbSoundSamplePipe != nullptr) {
        TbSoundSamplePipe* object = mFirstTbSoundSamplePipe;
        mFirstTbSoundSamplePipe = object->mPoolLink;
        ::operator delete(object);
    }
    mCurrentTbSoundSamplePipe = 0;
    mTotalTbSoundSamplePipe = 0;
}

// ---------------------------------------------------------------- the pool

// A size of 0 counts as 1; a block of the pipe's size comes from the list of idle objects when there is one, anything else
// from the runtime's operator new (import 0x1002eac4, __nw__FUl).
void* TbSoundSamplePipe::operator new(std::size_t size) {
    if (size == 0) size = 1;
    if (size != sizeof(TbSoundSamplePipe)) return ::operator new(size);
    if (mFirstTbSoundSamplePipe == nullptr) return ::operator new(size);
    --mCurrentTbSoundSamplePipe;
    if (mCurrentTbSoundSamplePipe < mLowTbSoundSamplePipe) mLowTbSoundSamplePipe = mCurrentTbSoundSamplePipe;
    ++mTotalTbSoundSamplePipe;
    TbSoundSamplePipe* object = mFirstTbSoundSamplePipe;
    mFirstTbSoundSamplePipe = object->mPoolLink;
    return object;
}

// NULL is ignored; a block of the pipe's size goes on the list, anything else back to the runtime (import 0x1002ea94).
void TbSoundSamplePipe::operator delete(void* object, std::size_t size) {
    if (object == nullptr) return;
    if (size != sizeof(TbSoundSamplePipe)) {
        ::operator delete(object);
        return;
    }
    TbSoundSamplePipe* idle = static_cast<TbSoundSamplePipe*>(object);
    idle->mPoolLink = mFirstTbSoundSamplePipe;
    mFirstTbSoundSamplePipe = idle;
    ++mCurrentTbSoundSamplePipe;
    if (mHighTbSoundSamplePipe < mCurrentTbSoundSamplePipe) mHighTbSoundSamplePipe = mCurrentTbSoundSamplePipe;
}

// Counts the idle objects, allocates blocks up to the level (the runtime's operator new: a NULL answer would be written
// through), then raises the high mark and, when the low mark is 0, sets it to the count.
void TbSoundSamplePipe::TopUp() {
    int32_t count = 0;
    for (TbSoundSamplePipe* object = mFirstTbSoundSamplePipe; object != nullptr; object = object->mPoolLink) ++count;
    for (; count < mLevelTbSoundSamplePipe; ++count) {
        TbSoundSamplePipe* object = static_cast<TbSoundSamplePipe*>(::operator new(sizeof(TbSoundSamplePipe)));
        object->mPoolLink = mFirstTbSoundSamplePipe;
        ++mCurrentTbSoundSamplePipe;
        mFirstTbSoundSamplePipe = object;
    }
    if (mHighTbSoundSamplePipe < mCurrentTbSoundSamplePipe) mHighTbSoundSamplePipe = mCurrentTbSoundSamplePipe;
    if (mLowTbSoundSamplePipe == 0) mLowTbSoundSamplePipe = mCurrentTbSoundSamplePipe;
}

void TbSoundSamplePipe::SetLevel(int32_t level) {
    mLevelTbSoundSamplePipe = level;
}

// The sample callback pipes of the sound library (unit w79i, program "sound symbols shared"): TbSoundUserCallback (UserCallback.cpp),
// _TbSoundSampleCallback and CAutoDetach (TbSoundSampleCallback.cpp), _TbSoundSampleCompletedCallback (TbSoundSamplePipe.cpp, its
// constructor and GetDataChunk in LbMpeg.cpp) and the three cleanup objects. See include/TbSoundSampleCallback.h for the layouts
// and the vtables.

namespace {

struct CompletedCallbackPool {
    using Object = _TbSoundSampleCompletedCallback;
    static Object*& first() { return Object::mFirst_TbSoundSampleCompletedCallback; }
    static int32_t& current() { return Object::mCurrent_TbSoundSampleCompletedCallback; }
    static int32_t& low() { return Object::mLow_TbSoundSampleCompletedCallback; }
    static int32_t& high() { return Object::mHigh_TbSoundSampleCompletedCallback; }
    static int32_t& total() { return Object::mTotal_TbSoundSampleCompletedCallback; }
    static int32_t& level() { return Object::mLevel_TbSoundSampleCompletedCallback; }
    static Object*& link(Object* object) { return object->mCompletedCallbackPoolLink; }
};

}  // namespace

void _TbSoundSampleCompletedCallback::ClearCache() { PoolClearCache<CompletedCallbackPool>(); }
// ---------------------------------------------------------------- _TbSoundSampleCompletedCallback

void* _TbSoundSampleCompletedCallback::operator new(std::size_t size) { return PoolNew<CompletedCallbackPool>(size); }
void _TbSoundSampleCompletedCallback::operator delete(void* object, std::size_t size) {
    PoolDelete<CompletedCallbackPool>(object, size);
}
void _TbSoundSampleCompletedCallback::TopUp() { PoolTopUp<CompletedCallbackPool>(); }
void _TbSoundSampleCompletedCallback::SetLevel(int32_t level) { mLevel_TbSoundSampleCompletedCallback = level; }

// Releases the pipe that was attached behind this one (the whole chain behind it goes), then links the new one.
TbSoundSamplePipe* TbSoundSamplePipe::Attatch(TbSoundSamplePipe* pipe) {
    if (pipe != nullptr) {
        if (mNext != nullptr) mNext->Release();
        mNext = pipe;
        pipe->mPrevious = this;
    }
    return pipe;
}

// Puts this pipe behind `replacement` (replacing what was attached there) and answers `replacement`.
TbSoundSamplePipe* TbSoundSamplePipe::AttachReplace(TbSoundSamplePipe* replacement, TbSoundSamplePipe*) {
    replacement->Attatch(this);
    return replacement;
}

// Releases the chain behind this pipe, then deletes this one (the Mac: the destructor with flag 1 after a this != NULL test).
void TbSoundSamplePipe::Release() {
    if (mNext != nullptr) {
        mNext->Release();
        mNext = nullptr;
    }
    delete this;
}

void TbSoundSamplePipe::Completed(bool done) {
    if (mNext != nullptr) mNext->Completed(done);
}

TbError TbSoundSamplePipe::SetInfo(const TbSoundSampleInfo& info) {
    if (mNext == nullptr) return TbError(0);
    return mNext->SetInfo(info);
}

void TbSoundSamplePipe::GetInfo(TbSoundSampleInfo& info) {
    if (mNext != nullptr) mNext->GetInfo(info);
}

TbSoundSamplePipe* TbSoundSamplePipe::FindPipe(void* handle) {
    if (mHandle == handle) return this;
    if (mNext == nullptr) return nullptr;
    return mNext->FindPipe(handle);
}

// Unlinks this pipe when it has a previous one (a first pipe leaves the one behind it pointing back at it), then deletes it.
// The Mac reloads both fields after the first store, as written here.
void TbSoundSamplePipe::Detatch() {
    TbSoundSamplePipe* previous = mPrevious;
    if (previous != nullptr) {
        if (mNext == nullptr) {
            previous->mNext = nullptr;
        } else {
            mNext->mPrevious = previous;
            TbSoundSamplePipe* next = mNext;
            previous = mPrevious;
            previous->mNext = next;
        }
    }
    delete this;
}

// Unit v51045: TbStaticFileBank::RegisterBank and three static initialisers of the sound library.

// 0x100130e8 (252)
// RegisterBank is defined inline in include/TbSoundBank.h.

// Mac data the initialisers register (their addresses are in the names).
uint32_t gSinit_1003f2a8[1], gSinit_1003f2ac[3], gSinit_1003f284[1], gSinit_1003f288[3];
uint32_t gSinitDtor_10033d3c[2], gSinitDtor_10033d34[2], gSinitDtor_1003352c[2], gSinitDtor_100336dc[2];
// 0x1000f8d4 (68)
void __sinit_TbSoundSamplePipe_cpp() {
    __register_global_object(gSinit_1003f2a8, gSinitDtor_10033d3c, gSinit_1003f2ac);
    __register_global_object(gSinit_1003f284, gSinitDtor_10033d34, gSinit_1003f288);
}

// Unit v51112: small functions of the sound program's AudioPlaceHolder, TbSoundSample*, CSprocketsModule ranges (see the header block).

// Stand-ins: the real destructors (0x1000ed30, 0x1000d608, 0x1000b590) are not translated; only their addresses are registered.
void* DestructStandInMPEG2(void* self, int16_t) { return self; }
