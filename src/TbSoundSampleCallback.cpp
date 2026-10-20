// The sample callback pipes of the sound library (unit w79i, program "sound symbols shared"): TbSoundUserCallback (UserCallback.cpp),
// _TbSoundSampleCallback and CAutoDetach (TbSoundSampleCallback.cpp), _TbSoundSampleCompletedCallback (TbSoundSamplePipe.cpp, its
// constructor and GetDataChunk in LbMpeg.cpp) and the three cleanup objects. See include/TbSoundSampleCallback.h for the layouts
// and the vtables.
#include "TbSoundSampleCallback.h"

#include <new>

#include "AudioPlaceHolder.h"
#include "TbSoundCodecs.h"

TbSoundUserCallback* TbSoundUserCallback::mFirstTbSoundUserCallback = nullptr;
int32_t TbSoundUserCallback::mCurrentTbSoundUserCallback = 0;
int32_t TbSoundUserCallback::mLowTbSoundUserCallback = 0;
int32_t TbSoundUserCallback::mHighTbSoundUserCallback = 0;
int32_t TbSoundUserCallback::mTotalTbSoundUserCallback = 0;
int32_t TbSoundUserCallback::mLevelTbSoundUserCallback = 10;

_TbSoundSampleCallback* _TbSoundSampleCallback::mFirst_TbSoundSampleCallback = nullptr;
int32_t _TbSoundSampleCallback::mCurrent_TbSoundSampleCallback = 0;
int32_t _TbSoundSampleCallback::mLow_TbSoundSampleCallback = 0;
int32_t _TbSoundSampleCallback::mHigh_TbSoundSampleCallback = 0;
int32_t _TbSoundSampleCallback::mTotal_TbSoundSampleCallback = 0;
int32_t _TbSoundSampleCallback::mLevel_TbSoundSampleCallback = 10;

_TbSoundSampleCompletedCallback* _TbSoundSampleCompletedCallback::mFirst_TbSoundSampleCompletedCallback = nullptr;
int32_t _TbSoundSampleCompletedCallback::mCurrent_TbSoundSampleCompletedCallback = 0;
int32_t _TbSoundSampleCompletedCallback::mLow_TbSoundSampleCompletedCallback = 0;
int32_t _TbSoundSampleCompletedCallback::mHigh_TbSoundSampleCompletedCallback = 0;
int32_t _TbSoundSampleCompletedCallback::mTotal_TbSoundSampleCompletedCallback = 0;
int32_t _TbSoundSampleCompletedCallback::mLevel_TbSoundSampleCompletedCallback = 10;

namespace {

// The three pools have the allocation code of the other pooled classes (see src/TbSoundSampleMPEG2.cpp, which keeps its own copy of
// these templates): a separate listing per class that differs only in the statics, the size and the offset of the link.

struct UserCallbackPool {
    using Object = TbSoundUserCallback;
    static Object*& first() { return Object::mFirstTbSoundUserCallback; }
    static int32_t& current() { return Object::mCurrentTbSoundUserCallback; }
    static int32_t& low() { return Object::mLowTbSoundUserCallback; }
    static int32_t& high() { return Object::mHighTbSoundUserCallback; }
    static int32_t& total() { return Object::mTotalTbSoundUserCallback; }
    static int32_t& level() { return Object::mLevelTbSoundUserCallback; }
    static Object*& link(Object* object) { return object->mUserCallbackPoolLink; }
};

struct SampleCallbackPool {
    using Object = _TbSoundSampleCallback;
    static Object*& first() { return Object::mFirst_TbSoundSampleCallback; }
    static int32_t& current() { return Object::mCurrent_TbSoundSampleCallback; }
    static int32_t& low() { return Object::mLow_TbSoundSampleCallback; }
    static int32_t& high() { return Object::mHigh_TbSoundSampleCallback; }
    static int32_t& total() { return Object::mTotal_TbSoundSampleCallback; }
    static int32_t& level() { return Object::mLevel_TbSoundSampleCallback; }
    static Object*& link(Object* object) { return object->mSampleCallbackPoolLink; }
};

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

// A size of 0 counts as 1; a block of the class's size comes from the list of idle objects when there is one, anything else
// from the runtime's operator new (import 0x1002eac4, __nw__FUl).
template <class P>
void* PoolNew(std::size_t size) {
    if (size == 0) size = 1;
    if (size != sizeof(typename P::Object)) return ::operator new(size);
    if (P::first() == nullptr) return ::operator new(size);
    --P::current();
    if (P::current() < P::low()) P::low() = P::current();
    ++P::total();
    typename P::Object* object = P::first();
    P::first() = P::link(object);
    return object;
}

// NULL is ignored; a block of the class's size goes on the list, anything else back to the runtime (import 0x1002ea94).
template <class P>
void PoolDelete(void* object, std::size_t size) {
    if (object == nullptr) return;
    if (size != sizeof(typename P::Object)) {
        ::operator delete(object);
        return;
    }
    typename P::Object* idle = static_cast<typename P::Object*>(object);
    P::link(idle) = P::first();
    P::first() = idle;
    ++P::current();
    if (P::high() < P::current()) P::high() = P::current();
}

// Counts the idle objects, allocates blocks up to the level (the runtime's operator new directly: a NULL answer would be
// written through), then raises the high mark and, when the low mark is 0, sets it to the count.
template <class P>
void PoolTopUp() {
    int32_t count = 0;
    for (typename P::Object* object = P::first(); object != nullptr; object = P::link(object)) ++count;
    for (; count < P::level(); ++count) {
        typename P::Object* object = static_cast<typename P::Object*>(::operator new(sizeof(typename P::Object)));
        P::link(object) = P::first();
        ++P::current();
        P::first() = object;
    }
    if (P::high() < P::current()) P::high() = P::current();
    if (P::low() == 0) P::low() = P::current();
}

// Frees the idle objects (a plain delete, not through the list) and zeroes the current and total counts only.
template <class P>
void PoolClearCache() {
    while (P::first() != nullptr) {
        typename P::Object* object = P::first();
        P::first() = P::link(object);
        ::operator delete(object);
    }
    P::current() = 0;
    P::total() = 0;
}

// The Mac's 32-bit signed arithmetic on the byte counts (wraps instead of overflowing).
int32_t Add32(int32_t a, int32_t b) {
    return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

int32_t Sub32(int32_t a, int32_t b) {
    return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

}  // namespace

// ---------------------------------------------------------------- TbSoundUserCallback

void* TbSoundUserCallback::operator new(std::size_t size) { return PoolNew<UserCallbackPool>(size); }
void TbSoundUserCallback::operator delete(void* object, std::size_t size) { PoolDelete<UserCallbackPool>(object, size); }
void TbSoundUserCallback::TopUp() { PoolTopUp<UserCallbackPool>(); }
void TbSoundUserCallback::SetLevel(int32_t level) { mLevelTbSoundUserCallback = level; }
void TbSoundUserCallback::ClearCache() { PoolClearCache<UserCallbackPool>(); }

// The pipe's constructor (the chain links and the handle), then the user data. The pool links keep what the block held.
TbSoundUserCallback::TbSoundUserCallback() {
    mUserData = nullptr;
}

TbSoundUserCallback::TbSoundUserCallback(void* userData) {
    mUserData = userData;
}

// (The Mac's `this != NULL` test and its flag are the compiler's delete: see the header.)
TbSoundUserCallback::~TbSoundUserCallback() {}

// The chain's answer first (the pipe's own SetInfo, not a virtual call); a negative one is -1, else the user data is taken from
// the info when it carries it.
TbError TbSoundUserCallback::SetInfo(const TbSoundSampleInfo& info) {
    TbError result = TbSoundSamplePipe::SetInfo(info);
    if (static_cast<int32_t>(result) < 0) return TbError(-1);
    if ((info.mFlags & 0x1000) != 0) mUserData = info.mUserData;
    return TbError(0);
}

// The info's request word (+0x18, which include/TbSoundInfo.h calls mField18) asks for the user data with 0x1000.
void TbSoundUserCallback::GetInfo(TbSoundSampleInfo& info) {
    if ((info.mField18 & 0x1000) != 0) info.SetUserData(mUserData);
}

// ---------------------------------------------------------------- _TbSoundSampleCallback

void* _TbSoundSampleCallback::operator new(std::size_t size) { return PoolNew<SampleCallbackPool>(size); }
void _TbSoundSampleCallback::operator delete(void* object, std::size_t size) { PoolDelete<SampleCallbackPool>(object, size); }
void _TbSoundSampleCallback::TopUp() { PoolTopUp<SampleCallbackPool>(); }
void _TbSoundSampleCallback::SetLevel(int32_t level) { mLevel_TbSoundSampleCallback = level; }
void _TbSoundSampleCallback::ClearCache() { PoolClearCache<SampleCallbackPool>(); }

// The user callback's no-argument constructor (user data 0), then the callback. The pool links keep what the block held.
_TbSoundSampleCallback::_TbSoundSampleCallback(TbSndCallback* callback) : TbSoundUserCallback() {
    mCallback = callback;
}

_TbSoundSampleCallback::~_TbSoundSampleCallback() {}

// The user callback's SetInfo (not the pipe's), then the callback of the info when it carries one (flag 0x800).
TbError _TbSoundSampleCallback::SetInfo(const TbSoundSampleInfo& info) {
    TbError result = TbSoundUserCallback::SetInfo(info);
    if (static_cast<int32_t>(result) < 0) return TbError(-1);
    if ((info.mFlags & 0x800) != 0) mCallback = info.mCallback;
    return TbError(0);
}

void _TbSoundSampleCallback::GetInfo(TbSoundSampleInfo& info) {
    TbSoundUserCallback::GetInfo(info);
    if ((info.mField18 & 0x800) != 0) info.SetCallback(mCallback);
}

// The guard detaches this pipe at the end of the call once a sample has finished (the destructor of the guard runs on both exits;
// the "nothing behind" exit never set it). The chunk is zeroed before every call, so a pipe that answers 1 without writing it
// delivers nothing; done starts at 0 and the loop test is a signed compare. mNext is read again for every round (GetNextSample
// replaces it).
int32_t _TbSoundSampleCallback::GetDataChunk(void* buffer, int32_t size, int32_t& done, bool) {
    CAutoDetach guard(this);
    int32_t chunk = 0;
    if (mNext == nullptr) return 1;
    done = 0;
    while (done < size) {
        chunk = 0;
        int32_t result = mNext->GetDataChunk(static_cast<uint8_t*>(buffer) + done, Sub32(size, done), chunk, true);
        if (result == 1) {
            guard = true;
            GetNextSample(static_cast<uint32_t>(Add32(chunk, done)));
        }
        done = Add32(done, chunk);
    }
    return 0;
}

// The format byte comes from the old pipe (not tested for NULL: GetDataChunk only comes here with one behind). mNext is cleared
// BEFORE the callback runs, so that an Attatch by the callback does not release the old pipe; the old pipe is released after.
void _TbSoundSampleCallback::GetNextSample(uint32_t position) {
    uint8_t format = static_cast<TbSoundSampleCodec*>(mNext)->GetSampleFormat();
    TbSoundSamplePipe* old = mNext;
    mNext = nullptr;
    mCallback->SoundCallback(mHandle, mUserData);
    if (old != nullptr) old->Release();
    if (mNext == nullptr) {
        void* block = TbTimerCodec::operator new(sizeof(TbTimerCodec));
        TbTimerCodec* timer = nullptr;
        if (block != nullptr) timer = ::new (block) TbTimerCodec(format, position);
        mNext = timer;
    }
}

// ---------------------------------------------------------------- _TbSoundSampleCompletedCallback

void* _TbSoundSampleCompletedCallback::operator new(std::size_t size) { return PoolNew<CompletedCallbackPool>(size); }
void _TbSoundSampleCompletedCallback::operator delete(void* object, std::size_t size) {
    PoolDelete<CompletedCallbackPool>(object, size);
}
void _TbSoundSampleCompletedCallback::TopUp() { PoolTopUp<CompletedCallbackPool>(); }
void _TbSoundSampleCompletedCallback::SetLevel(int32_t level) { mLevel_TbSoundSampleCompletedCallback = level; }
void _TbSoundSampleCompletedCallback::ClearCache() { PoolClearCache<CompletedCallbackPool>(); }

// The user callback's constructor with the user data, then the callback. The pool links keep what the block held.
_TbSoundSampleCompletedCallback::_TbSoundSampleCompletedCallback(TbSndCallback* callback, void* userData)
    : TbSoundUserCallback(userData) {
    mCallback = callback;
}

_TbSoundSampleCompletedCallback::~_TbSoundSampleCompletedCallback() {}

int32_t _TbSoundSampleCompletedCallback::GetDataChunk(void* buffer, int32_t size, int32_t& done, bool flag) {
    if (mNext == nullptr) return 0;
    return mNext->GetDataChunk(buffer, size, done, flag);
}

// ---------------------------------------------------------------- CAutoDetach

CAutoDetach::CAutoDetach(TbSoundSamplePipe* pipe) : mPipe(pipe), mDetach(false) {}

CAutoDetach& CAutoDetach::operator=(bool detach) {
    mDetach = detach;
    return *this;
}

// The pipe detaches itself through the virtual slot +0x14 (the pipe's Detatch: it unlinks and deletes the pipe).
CAutoDetach::~CAutoDetach() {
    if (mDetach) mPipe->Detatch();
}

// ---------------------------------------------------------------- the static cleanup objects

_CS_TbSoundUserCallback::~_CS_TbSoundUserCallback() { TbSoundUserCallback::ClearCache(); }

_CS__TbSoundSampleCallback::~_CS__TbSoundSampleCallback() { _TbSoundSampleCallback::ClearCache(); }

_CS__TbSoundSampleCompletedCallback::~_CS__TbSoundSampleCompletedCallback() { _TbSoundSampleCompletedCallback::ClearCache(); }

namespace {
// The static objects the Mac's initialisers and finalisers run.
_CS_TbSoundUserCallback sCleanupUserCallback;
_CS__TbSoundSampleCallback sCleanupSampleCallback;
_CS__TbSoundSampleCompletedCallback sCleanupCompletedCallback;
}  // namespace
