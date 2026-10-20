// The sound library's sample pipe base (unit w78, program "sound symbols shared"): TbSoundSamplePipe (LbMpeg.cpp and
// TbSoundSamplePipe.cpp), TbSoundSPCodec (LbMpeg.cpp, StreamableMpeg2.cpp, TbSoundCodecLoop.cpp, TimerCodec.cpp) and
// TbSoundSampleCodec (StreamableMpeg2.cpp, TbSoundSampleCallback.cpp, TbSoundSampleMPEG2.cpp, TimerCodec.cpp). See
// include/TbSoundPipe.h for the layouts and the slot order.
#include "TbSoundPipe.h"

#include <new>

#include "LbTime.h"

TbSoundSamplePipe* TbSoundSamplePipe::mFirstTbSoundSamplePipe = nullptr;
int32_t TbSoundSamplePipe::mCurrentTbSoundSamplePipe = 0;
int32_t TbSoundSamplePipe::mLowTbSoundSamplePipe = 0;
int32_t TbSoundSamplePipe::mHighTbSoundSamplePipe = 0;
int32_t TbSoundSamplePipe::mTotalTbSoundSamplePipe = 0;
int32_t TbSoundSamplePipe::mLevelTbSoundSamplePipe = 10;

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

// ---------------------------------------------------------------- the pipe

// Only the three chain fields: the pool link and the vptr's other neighbours keep what the block held.
TbSoundSamplePipe::TbSoundSamplePipe() {
    mNext = nullptr;
    mPrevious = nullptr;
    mHandle = nullptr;
}

// (The Mac's `this != NULL` test and its flag are the compiler's delete: see the header.)
TbSoundSamplePipe::~TbSoundSamplePipe() {}

void* TbSoundSamplePipe::SetHandle(void* handle) {
    mHandle = handle;
    return handle;
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

// ---------------------------------------------------------------- the SP codec

TbSoundSPCodec::TbSoundSPCodec() {}

TbSoundSPCodec::~TbSoundSPCodec() {}

TbSoundSPCodec* TbSoundSPCodec::GetCodec() {
    return static_cast<TbSoundSPCodec*>(mNext);
}

void TbSoundSPCodec::Restart() {
    GetCodec()->Restart();
}

const uint8_t* TbSoundSPCodec::GetDataStart() {
    return GetCodec()->GetDataStart();
}

void TbSoundSPCodec::SetDataPosition(uint32_t position) {
    GetCodec()->SetDataPosition(position);
}

uint32_t TbSoundSPCodec::GetDataPosition() {
    return GetCodec()->GetDataPosition();
}

uint32_t TbSoundSPCodec::GetDataSize() {
    return GetCodec()->GetDataSize();
}

void TbSoundSPCodec::SetSampleRate(uint16_t rate) {
    GetCodec()->SetSampleRate(rate);
}

uint32_t TbSoundSPCodec::GetSampleRate() {
    return GetCodec()->GetSampleRate();
}

uint32_t TbSoundSPCodec::GetBitRate() {
    return GetCodec()->GetBitRate();
}

uint32_t TbSoundSPCodec::GetChannels() {
    return GetCodec()->GetChannels();
}

void TbSoundSPCodec::GetInfo(TbSoundSampleInfo& info) {
    GetCodec()->GetInfo(info);
}

TbError TbSoundSPCodec::SetInfo(const TbSoundSampleInfo& info) {
    return GetCodec()->SetInfo(info);
}

// ---------------------------------------------------------------- the sample codec

// The data start is the header + 0x28 (the file's own 0x28-byte header, converted in place, is followed by the sample's data;
// the sample store hands the codec of a non-streaming bank that record). For a streaming bank (TbMMFileBank) the record is
// one of the bank's 0x14-byte headers and +0x28 is a bogus address two records on: the streaming codecs (unit w80) read the
// data through the file instead, and this base class never dereferences it.
TbSoundSampleCodec::TbSoundSampleCodec(TbSampleHeader* header, TbRefRelease* owner) : mRefManager(owner) {
    mDataSize = static_cast<uint32_t>(header->mSize);
    mDataStart = reinterpret_cast<const uint8_t*>(header) + 0x28;
    mTime = 0;
    mDataPosition = 0;
    SetSampleFormatFromHeader(header);
}

// The Mac's CRefManager::CRefManager() (0x10013824, 12 bytes: stores 0 at +0) is another unit's function; the host uses the
// existing constructor with no owner, which does the same and makes no AddRef call.
TbSoundSampleCodec::TbSoundSampleCodec() : mRefManager(nullptr) {
    mDataPosition = 0;
}

TbSoundSampleCodec::~TbSoundSampleCodec() {}

// The format byte: 0x40 stereo or 0x20, | 2 for 16 bits (header +6 == 0x10) or 1; then the virtual SetSampleRate with the
// header's rate (+4) adds the rate bit.
void TbSoundSampleCodec::SetSampleFormatFromHeader(TbSampleHeader* header) {
    if ((header->mFlags & 1) != 0) {
        mFormat = 0x40;
    } else {
        mFormat = 0x20;
    }
    if (header->mBits == 0x10) {
        mFormat = static_cast<uint8_t>(mFormat | 2);
    } else {
        mFormat = static_cast<uint8_t>(mFormat | 1);
    }
    SetSampleRate(header->mRate);
}

// Clears bits 4, 8 and 0x10, then sets 8 for 22050, 0x10 for 44100 and 4 for any other rate.
void TbSoundSampleCodec::SetSampleRate(uint16_t rate) {
    mFormat = static_cast<uint8_t>(mFormat & 0xfb);
    mFormat = static_cast<uint8_t>(mFormat & 0xf7);
    mFormat = static_cast<uint8_t>(mFormat & 0xef);
    if (rate == 0x5622) {
        mFormat = static_cast<uint8_t>(mFormat | 8);
        return;
    }
    if (rate == 0xac44) {
        mFormat = static_cast<uint8_t>(mFormat | 0x10);
        return;
    }
    mFormat = static_cast<uint8_t>(mFormat | 4);
}

uint32_t TbSoundSampleCodec::GetSampleRate() {
    if ((mFormat & 8) != 0) return 0x5622;
    if ((mFormat & 0x10) != 0) return 0xac44;
    return 0x2b11;
}

uint32_t TbSoundSampleCodec::GetBitRate() {
    return (mFormat & 2) != 0 ? 16 : 8;
}

uint32_t TbSoundSampleCodec::GetChannels() {
    return (mFormat & 0x40) != 0 ? 2 : 1;
}

uint8_t TbSoundSampleCodec::GetSampleFormat() const {
    return mFormat;
}

// The pipe's own SetInfo goes down the chain; a negative result is -1, otherwise the codec jumps to the end of the sample
// when the second flags word of the info (+0xc) has 0x4000.
TbError TbSoundSampleCodec::SetInfo(const TbSoundSampleInfo& info) {
    TbError result = TbSoundSamplePipe::SetInfo(info);
    if (static_cast<int32_t>(result) < 0) return TbError(-1);
    if ((info.mField0C & 0x4000) != 0) mDataPosition = mDataSize;
    return TbError(0);
}

void TbSoundSampleCodec::GetInfo(TbSoundSampleInfo& info) {
    TbSoundSamplePipe::GetInfo(info);
}

// Not stopped: the bytes become samples (halved for stereo, halved again for 16 bits), the time is the sample's length in
// milliseconds at the virtual GetSampleRate (an unsigned division: the Mac's divwu leaves a zero divisor undefined, the
// emulator answers the dividend and so does this) plus 200 and the clock.
bool TbSoundSampleCodec::SetTime(bool stopped, uint32_t bytes) {
    if (stopped) return false;
    if ((mFormat & 0x40) != 0) bytes >>= 1;
    if ((mFormat & 2) != 0) bytes >>= 1;
    uint32_t rate = GetSampleRate();
    uint32_t product = bytes * 1000u;
    uint32_t span = rate == 0 ? product : product / rate;
    uint32_t time = span + 200;
    mTime = time + LbTime_GetClock();
    return true;
}

bool TbSoundSampleCodec::IsTimeComplete() {
    uint32_t now = LbTime_GetClock();
    return now >= mTime;
}

uint32_t TbSoundSampleCodec::Timed() {
    return mTime;
}

void TbSoundSampleCodec::SetDataPosition(uint32_t position) {
    mDataPosition = position;
}

uint32_t TbSoundSampleCodec::GetDataPosition() {
    return mDataPosition;
}

const uint8_t* TbSoundSampleCodec::GetDataStart() {
    return mDataStart;
}

uint32_t TbSoundSampleCodec::GetDataSize() {
    return mDataSize;
}
