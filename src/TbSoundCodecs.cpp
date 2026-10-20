// The sound library's four small codecs (unit w80, program "sound symbols shared"): _TbSoundSampleWAV and
// _TbSoundSampleWAVFile (TbSoundSampleWAV.cpp, LbMpeg.cpp), _TbSoundCodecLoop (TbSoundCodecLoop.cpp) and TbTimerCodec
// (TimerCodec.cpp), with CMemoryCopy::Copy. See include/TbSoundCodecs.h for the layouts and the slot order.
#include "TbSoundCodecs.h"

#include <cstring>
#include <new>

_TbSoundSampleWAV* _TbSoundSampleWAV::mFirst_TbSoundSampleWAV = nullptr;
int32_t _TbSoundSampleWAV::mCurrent_TbSoundSampleWAV = 0;
int32_t _TbSoundSampleWAV::mLow_TbSoundSampleWAV = 0;
int32_t _TbSoundSampleWAV::mHigh_TbSoundSampleWAV = 0;
int32_t _TbSoundSampleWAV::mTotal_TbSoundSampleWAV = 0;
int32_t _TbSoundSampleWAV::mLevel_TbSoundSampleWAV = 10;

_TbSoundSampleWAVFile* _TbSoundSampleWAVFile::mFirst_TbSoundSampleWAVFile = nullptr;
int32_t _TbSoundSampleWAVFile::mCurrent_TbSoundSampleWAVFile = 0;
int32_t _TbSoundSampleWAVFile::mLow_TbSoundSampleWAVFile = 0;
int32_t _TbSoundSampleWAVFile::mHigh_TbSoundSampleWAVFile = 0;
int32_t _TbSoundSampleWAVFile::mTotal_TbSoundSampleWAVFile = 0;
int32_t _TbSoundSampleWAVFile::mLevel_TbSoundSampleWAVFile = 10;

_TbSoundCodecLoop* _TbSoundCodecLoop::mFirst_TbSoundCodecLoop = nullptr;
int32_t _TbSoundCodecLoop::mCurrent_TbSoundCodecLoop = 0;
int32_t _TbSoundCodecLoop::mLow_TbSoundCodecLoop = 0;
int32_t _TbSoundCodecLoop::mHigh_TbSoundCodecLoop = 0;
int32_t _TbSoundCodecLoop::mTotal_TbSoundCodecLoop = 0;
int32_t _TbSoundCodecLoop::mLevel_TbSoundCodecLoop = 10;

TbTimerCodec* TbTimerCodec::mFirstTbTimerCodec = nullptr;
int32_t TbTimerCodec::mCurrentTbTimerCodec = 0;
int32_t TbTimerCodec::mLowTbTimerCodec = 0;
int32_t TbTimerCodec::mHighTbTimerCodec = 0;
int32_t TbTimerCodec::mTotalTbTimerCodec = 0;
int32_t TbTimerCodec::mLevelTbTimerCodec = 10;

namespace {

// The Mac's LB_MIN<int,int> (0x1000e7d8, 0x100112f8 and 0x10011a98, 24 bytes each): `a < b ? a : b` on references, signed.
// (Unit w79a adds LB_MIN<unsigned long> to a header of its own; this one stays private to this file.)
template <class T>
const T& LB_MIN(const T& a, const T& b) {
    return a < b ? a : b;
}

// ---------------------------------------------------------------- the four pools
// The four classes have the same allocation code (the POOLED pattern) with their own statics, size and link word. Each trait
// names the statics of one class; the template functions below are the Mac's operator new, operator delete, TopUp and
// ClearCache of that class. (The Mac code is a separate copy per class: the listings differ only in the addresses, the size
// and the offset of the link.)

struct WAVPool {
    using Object = _TbSoundSampleWAV;
    static Object*& first() { return Object::mFirst_TbSoundSampleWAV; }
    static int32_t& current() { return Object::mCurrent_TbSoundSampleWAV; }
    static int32_t& low() { return Object::mLow_TbSoundSampleWAV; }
    static int32_t& high() { return Object::mHigh_TbSoundSampleWAV; }
    static int32_t& total() { return Object::mTotal_TbSoundSampleWAV; }
    static int32_t& level() { return Object::mLevel_TbSoundSampleWAV; }
    static Object*& link(Object* object) { return object->mWAVPoolLink; }
};

struct WAVFilePool {
    using Object = _TbSoundSampleWAVFile;
    static Object*& first() { return Object::mFirst_TbSoundSampleWAVFile; }
    static int32_t& current() { return Object::mCurrent_TbSoundSampleWAVFile; }
    static int32_t& low() { return Object::mLow_TbSoundSampleWAVFile; }
    static int32_t& high() { return Object::mHigh_TbSoundSampleWAVFile; }
    static int32_t& total() { return Object::mTotal_TbSoundSampleWAVFile; }
    static int32_t& level() { return Object::mLevel_TbSoundSampleWAVFile; }
    static Object*& link(Object* object) { return object->mWAVFilePoolLink; }
};

struct LoopPool {
    using Object = _TbSoundCodecLoop;
    static Object*& first() { return Object::mFirst_TbSoundCodecLoop; }
    static int32_t& current() { return Object::mCurrent_TbSoundCodecLoop; }
    static int32_t& low() { return Object::mLow_TbSoundCodecLoop; }
    static int32_t& high() { return Object::mHigh_TbSoundCodecLoop; }
    static int32_t& total() { return Object::mTotal_TbSoundCodecLoop; }
    static int32_t& level() { return Object::mLevel_TbSoundCodecLoop; }
    static Object*& link(Object* object) { return object->mLoopPoolLink; }
};

struct TimerPool {
    using Object = TbTimerCodec;
    static Object*& first() { return Object::mFirstTbTimerCodec; }
    static int32_t& current() { return Object::mCurrentTbTimerCodec; }
    static int32_t& low() { return Object::mLowTbTimerCodec; }
    static int32_t& high() { return Object::mHighTbTimerCodec; }
    static int32_t& total() { return Object::mTotalTbTimerCodec; }
    static int32_t& level() { return Object::mLevelTbTimerCodec; }
    static Object*& link(Object* object) { return object->mTimerPoolLink; }
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

// Frees the idle objects (a plain delete, not through the list) and zeroes the current and total counts only: the low and
// high marks stay.
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

}  // namespace

// ---------------------------------------------------------------- CMemoryCopy

// Slot +8 of the global copier (CStandardCopy::Copy 0x10014cac) is memcpy(dest, source, count).
void CMemoryCopy::Copy(void* dest, const void* source, uint32_t count) {
    std::memcpy(dest, source, count);
}

// ---------------------------------------------------------------- _TbSoundSampleWAV

void* _TbSoundSampleWAV::operator new(std::size_t size) { return PoolNew<WAVPool>(size); }
void _TbSoundSampleWAV::operator delete(void* object, std::size_t size) { PoolDelete<WAVPool>(object, size); }
void _TbSoundSampleWAV::TopUp() { PoolTopUp<WAVPool>(); }
void _TbSoundSampleWAV::SetLevel(int32_t level) { mLevel_TbSoundSampleWAV = level; }
void _TbSoundSampleWAV::ClearCache() { PoolClearCache<WAVPool>(); }

_TbSoundSampleWAV::_TbSoundSampleWAV(TbSampleHeader* header, TbRefRelease* owner) : TbSoundSampleCodec(header, owner) {}

_TbSoundSampleWAV::~_TbSoundSampleWAV() {}

// Not timed: copies min(size, bytes left) from the data at the position and advances it. At the end of the data the timer
// is started for the bytes just copied plus 0x1200 (SetTime through the virtual slot +0x5c; a stopped codec starts none), and
// the answer is 1 unless the timer was started; the next call is then timed. Timed: nothing is copied, `done` is 0 and the
// answer is whether the time is over. The compare of the position with the size is unsigned, the minimum is signed (a
// position past the size gives a negative count that the copy takes as a huge unsigned one).
int32_t _TbSoundSampleWAV::GetDataChunk(void* buffer, int32_t size, int32_t& done, bool stopped) {
    if (Timed() != 0) {
        done = 0;
        return IsTimeComplete();
    }
    int32_t left = static_cast<int32_t>(mDataSize - mDataPosition);
    done = LB_MIN<int32_t>(size, left);
    CMemoryCopy copier;
    copier.Copy(buffer, mDataStart + mDataPosition, static_cast<uint32_t>(done));
    mDataPosition += static_cast<uint32_t>(done);
    if (mDataPosition >= mDataSize) {
        if (!SetTime(stopped, static_cast<uint32_t>(done + 0x1200))) return 1;
    }
    return 0;
}

void _TbSoundSampleWAV::Restart() {
    mDataPosition = 0;
}

void _TbSoundSampleWAV::SetDataStart(void* start) {
    mDataStart = static_cast<const uint8_t*>(start);
}

// ---------------------------------------------------------------- _TbSoundSampleWAVFile

void* _TbSoundSampleWAVFile::operator new(std::size_t size) { return PoolNew<WAVFilePool>(size); }
void _TbSoundSampleWAVFile::operator delete(void* object, std::size_t size) { PoolDelete<WAVFilePool>(object, size); }
void _TbSoundSampleWAVFile::TopUp() { PoolTopUp<WAVFilePool>(); }
void _TbSoundSampleWAVFile::SetLevel(int32_t level) { mLevel_TbSoundSampleWAVFile = level; }
void _TbSoundSampleWAVFile::ClearCache() { PoolClearCache<WAVFilePool>(); }

// The file, then the start (the arguments of the Mac constructor are file, header, start, owner).
_TbSoundSampleWAVFile::_TbSoundSampleWAVFile(bfl::FileSystem::CFile* file, TbSampleHeader* header, uint32_t start,
                                             TbRefRelease* owner)
    : _TbSoundSampleWAV(header, owner) {
    mFile = file;
    mStart = start;
}

_TbSoundSampleWAVFile::~_TbSoundSampleWAVFile() {}

// Like the memory codec, but the position is the file's: the bytes left are mDataSize - (file position - mStart), read with
// the file's Read (slot +0x10, after GetPos, slot +0x20), and the end of the data is tested by asking GetPos again (a signed
// compare, unlike the memory codec's). The timer gets the bytes just read, without the memory codec's + 0x1200.
int32_t _TbSoundSampleWAVFile::GetDataChunk(void* buffer, int32_t size, int32_t& done, bool stopped) {
    if (Timed() != 0) {
        done = 0;
        return IsTimeComplete();
    }
    int32_t position = mFile->GetPos();
    int32_t left = static_cast<int32_t>(mDataSize - static_cast<uint32_t>(position - static_cast<int32_t>(mStart)));
    done = LB_MIN<int32_t>(size, left);
    mFile->Read(buffer, static_cast<uint32_t>(done));
    int32_t after = static_cast<int32_t>(static_cast<uint32_t>(mFile->GetPos()) - mStart);
    if (after >= static_cast<int32_t>(mDataSize)) {
        if (!SetTime(stopped, static_cast<uint32_t>(done))) return 1;
    }
    return 0;
}

void _TbSoundSampleWAVFile::Restart() {
    mFile->Seek(mStart, 0);
}

// ---------------------------------------------------------------- _TbSoundCodecLoop

void* _TbSoundCodecLoop::operator new(std::size_t size) { return PoolNew<LoopPool>(size); }
void _TbSoundCodecLoop::operator delete(void* object, std::size_t size) { PoolDelete<LoopPool>(object, size); }
void _TbSoundCodecLoop::TopUp() { PoolTopUp<LoopPool>(); }
void _TbSoundCodecLoop::SetLevel(int32_t level) { mLevel_TbSoundCodecLoop = level; }
void _TbSoundCodecLoop::ClearCache() { PoolClearCache<LoopPool>(); }

_TbSoundCodecLoop::_TbSoundCodecLoop() {}

_TbSoundCodecLoop::~_TbSoundCodecLoop() {}

// Asks the codec behind (mNext, never tested for NULL) for the rest of the buffer, always with stopped = true, and when it
// answers exactly 1 (finished) restarts it through GetCodec() (slot +0x58) and its Restart (slot +0x28). `done` is stored
// back after every round, so it is the bytes delivered so far; the loop ends when it reaches the size (signed compare) and the
// answer is 0. The argument `stopped` is not used. A codec that delivers nothing and never answers anything else would loop
// for ever (it is not tested).
int32_t _TbSoundCodecLoop::GetDataChunk(void* buffer, int32_t size, int32_t& done, bool) {
    done = 0;
    while (done < size) {
        int32_t got = 0;
        int32_t result = mNext->GetDataChunk(static_cast<uint8_t*>(buffer) + done, size - done, got, true);
        if (result == 1) GetCodec()->Restart();
        done += got;
    }
    return 0;
}

// ---------------------------------------------------------------- TbTimerCodec

void* TbTimerCodec::operator new(std::size_t size) { return PoolNew<TimerPool>(size); }
void TbTimerCodec::operator delete(void* object, std::size_t size) { PoolDelete<TimerPool>(object, size); }
void TbTimerCodec::TopUp() { PoolTopUp<TimerPool>(); }
void TbTimerCodec::SetLevel(int32_t level) { mLevelTbTimerCodec = level; }
void TbTimerCodec::ClearCache() { PoolClearCache<TimerPool>(); }

// The default sample codec constructor (no owner), the format byte, then SetTime through the virtual slot with the
// milliseconds + 0x1200 as the "byte" count (the codec's SetTime halves it for stereo and 16 bits, as for a real sample).
TbTimerCodec::TbTimerCodec(uint8_t format, uint32_t milliseconds) {
    mFormat = format;
    SetTime(false, milliseconds + 0x1200);
}

TbTimerCodec::~TbTimerCodec() {}

// Silence: the whole buffer is zeroed and counted as delivered; the answer is whether the time is over.
int32_t TbTimerCodec::GetDataChunk(void* buffer, int32_t size, int32_t& done, bool) {
    done = size;
    std::memset(buffer, 0, static_cast<std::size_t>(done));
    return IsTimeComplete();
}
