// The sound library's MPEG sample codecs (unit w80b, program "sound symbols shared"): _TbSoundSampleMPEG2
// (TbSoundSampleMPEG2.cpp) and _TbSoundSampleMPEG2File (StreamableMpeg2.cpp). See include/TbSoundSampleMPEG2.h for the layouts
// and the slot map.
#include "TbSoundSampleMPEG2.h"

#include <new>

#include "CMpegBase.h"
#include "TbSoundBank.h"
#include "snd_handles.h"

_TbSoundSampleMPEG2* _TbSoundSampleMPEG2::mFirst_TbSoundSampleMPEG2 = nullptr;
int32_t _TbSoundSampleMPEG2::mCurrent_TbSoundSampleMPEG2 = 0;
int32_t _TbSoundSampleMPEG2::mLow_TbSoundSampleMPEG2 = 0;
int32_t _TbSoundSampleMPEG2::mHigh_TbSoundSampleMPEG2 = 0;
int32_t _TbSoundSampleMPEG2::mTotal_TbSoundSampleMPEG2 = 0;
int32_t _TbSoundSampleMPEG2::mLevel_TbSoundSampleMPEG2 = 10;

_TbSoundSampleMPEG2File* _TbSoundSampleMPEG2File::mFirst_TbSoundSampleMPEG2File = nullptr;
int32_t _TbSoundSampleMPEG2File::mCurrent_TbSoundSampleMPEG2File = 0;
int32_t _TbSoundSampleMPEG2File::mLow_TbSoundSampleMPEG2File = 0;
int32_t _TbSoundSampleMPEG2File::mHigh_TbSoundSampleMPEG2File = 0;
int32_t _TbSoundSampleMPEG2File::mTotal_TbSoundSampleMPEG2File = 0;
int32_t _TbSoundSampleMPEG2File::mLevel_TbSoundSampleMPEG2File = 10;

uint8_t _TbSoundSampleMPEG2::sSetInfoScratch[0x300];
uint8_t _TbSoundSampleMPEG2::sRestartScratch[0x300];

namespace {

// The two pools have the allocation code of the other pooled classes (see src/TbSoundCodecs.cpp, which keeps its copy private):
// a separate listing per class that differs only in the statics, the size and the offset of the link.

struct MPEG2Pool {
    using Object = _TbSoundSampleMPEG2;
    static Object*& first() { return Object::mFirst_TbSoundSampleMPEG2; }
    static int32_t& current() { return Object::mCurrent_TbSoundSampleMPEG2; }
    static int32_t& low() { return Object::mLow_TbSoundSampleMPEG2; }
    static int32_t& high() { return Object::mHigh_TbSoundSampleMPEG2; }
    static int32_t& total() { return Object::mTotal_TbSoundSampleMPEG2; }
    static int32_t& level() { return Object::mLevel_TbSoundSampleMPEG2; }
    static Object*& link(Object* object) { return object->mMPEG2PoolLink; }
};

struct MPEG2FilePool {
    using Object = _TbSoundSampleMPEG2File;
    static Object*& first() { return Object::mFirst_TbSoundSampleMPEG2File; }
    static int32_t& current() { return Object::mCurrent_TbSoundSampleMPEG2File; }
    static int32_t& low() { return Object::mLow_TbSoundSampleMPEG2File; }
    static int32_t& high() { return Object::mHigh_TbSoundSampleMPEG2File; }
    static int32_t& total() { return Object::mTotal_TbSoundSampleMPEG2File; }
    static int32_t& level() { return Object::mLevel_TbSoundSampleMPEG2File; }
    static Object*& link(Object* object) { return object->mMPEG2FilePoolLink; }
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

// The Mac's LB_MIN<int,int> (0x1000e7d8, 24 bytes): `a < b ? a : b` on references, signed.
const int32_t& LbMinInt(const int32_t& a, const int32_t& b) {
    return a < b ? a : b;
}

}  // namespace

// ---------------------------------------------------------------- _TbSoundSampleMPEG2

void* _TbSoundSampleMPEG2::operator new(std::size_t size) { return PoolNew<MPEG2Pool>(size); }
void _TbSoundSampleMPEG2::operator delete(void* object, std::size_t size) { PoolDelete<MPEG2Pool>(object, size); }
void _TbSoundSampleMPEG2::TopUp() { PoolTopUp<MPEG2Pool>(); }
void _TbSoundSampleMPEG2::SetLevel(int32_t level) { mLevel_TbSoundSampleMPEG2 = level; }
void _TbSoundSampleMPEG2::ClearCache() { PoolClearCache<MPEG2Pool>(); }

// No decoder yet; when the header's flag 4 is set the length is the header's word at +0xc (the decoded length in bytes) and the
// codec remembers it (flag 1), else OpenMpegFile takes the length from the decoder. The length (+0x30), the word at +0x38 and the
// pool link (+0x3c) are NOT written otherwise: they keep what the block held.
_TbSoundSampleMPEG2::_TbSoundSampleMPEG2(TbSampleHeader* header, TbRefRelease* owner)
    : TbSoundSampleCodec(header, owner), mFlags(0), mMpeg(nullptr) {
    if ((header->mFlags & 4) != 0) {
        mTotalBytes = header->mFieldC;
        mFlags |= 1;
    }
}

_TbSoundSampleMPEG2::~_TbSoundSampleMPEG2() {}

// Opens the decoder through the virtual (slot +0x64): a failure answers -1. A layer I stream is then primed: the first 0x300
// bytes are decoded into a scratch buffer when the sample loops (flag 0x40) or has a callback (flag 0x800).
TbError _TbSoundSampleMPEG2::SetInfo(const TbSoundSampleInfo& info) {
    TbError opened = OpenMpegFile();
    if (static_cast<int32_t>(opened) < 0) return TbError(-1);
    if (mMpeg->mLayer == 1) {
        if ((info.mFlags & 0x40) != 0 || (info.mFlags & 0x800) != 0) GetMpegBlock(sSetInfoScratch, 0x300);
    }
    return TbError(0);
}

// The decoder goes first (slot +0x68), then the pipe's own Release frees the chain and this object.
void _TbSoundSampleMPEG2::Release() {
    CloseMpegFile();
    TbSoundSamplePipe::Release();
}

int32_t _TbSoundSampleMPEG2::GetDataChunk(void* buffer, int32_t size, int32_t& done, bool stopped) {
    if (Timed() != 0) {
        done = 0;
        return IsTimeComplete();
    }
    int32_t left = static_cast<int32_t>(mTotalBytes - mDataPosition);
    done = LbMinInt(size, left);
    GetMpegBlock(buffer, done);
    mDataPosition += static_cast<uint32_t>(done);
    if (mDataPosition >= mTotalBytes) {
        if (!SetTime(stopped, static_cast<uint32_t>(done + 0x1200))) return 1;
    }
    return 0;
}

// The decoder is reset (no test for NULL) and a layer I stream is primed again.
void _TbSoundSampleMPEG2::Restart() {
    mDataPosition = 0;
    mMpeg->Reset();
    if (mMpeg->mLayer == 1) GetMpegBlock(sRestartScratch, 0x300);
}

// The decoder's stream moves with the codec's (no test for a missing decoder).
void _TbSoundSampleMPEG2::SetDataStart(void* start) {
    mDataStart = static_cast<const uint8_t*>(start);
    mMpeg->mData = static_cast<const uint8_t*>(start);
}

void _TbSoundSampleMPEG2::SetDataPosition(uint32_t) {}

void _TbSoundSampleMPEG2::SetNewDataPosition() {}

// Opens the decoder over the sample in memory, once (an open decoder answers success). The decoder's Open answers -1 for a
// stream that is not MPEG; anything else is success. The allocation is not tested for failure by the original except that the
// constructor is skipped for NULL.
TbError _TbSoundSampleMPEG2::OpenMpegFile() {
    if (mMpeg != nullptr) return TbError(0);
    void* block = CMpegBase::operator new(sizeof(CMpegBase));
    CMpegBase* decoder = nullptr;
    if (block != nullptr) decoder = ::new (block) CMpegBase();
    mMpeg = decoder;
    TbError opened(mMpeg->Open(const_cast<uint8_t*>(mDataStart), static_cast<int32_t>(mDataSize)));
    if (static_cast<int32_t>(opened) != -1) return FinishOpen();
    return TbError(-1);
}

// After the decoder opened. The length comes from the decoder unless the header gave it. When the decoder's rate (the word at
// CMpegBase +0x248c) differs from the codec's (compared as single precision floats), the codec takes the decoder's rate
// (low 16 bits; zero fails) and the length is scaled by the ratio of the two: the quotient is converted to an unsigned integer
// (twice, as the listing does), the length divided by it and made even. The open succeeds when the position is below the length.
TbError _TbSoundSampleMPEG2::FinishOpen() {
    if ((mFlags & 1) == 0) mTotalBytes = static_cast<uint32_t>(mMpeg->mTotalBytes);
    uint32_t codecRate = GetSampleRate();
    int32_t decoderRate = mMpeg->mSampleRate2;
    float codecRateFloat = static_cast<float>(codecRate);
    if (static_cast<float>(decoderRate) != codecRateFloat) {
        uint32_t rate16 = static_cast<uint32_t>(decoderRate) & 0xffff;
        SetSampleRate(static_cast<uint16_t>(rate16));
        if (rate16 == 0) return TbError(-1);
        float quotient = codecRateFloat / static_cast<float>(rate16);
        uint32_t divisor = CvtDblUl(quotient);
        if (CvtDblUl(quotient) == 0) return TbError(-1);
        mTotalBytes = mTotalBytes / divisor;
        mTotalBytes = mTotalBytes & 0xfffffffeu;
    }
    if (mDataPosition < mTotalBytes) return TbError(0);
    return TbError(-1);
}

// Closes the decoder, deletes it through its virtual destructor (slot +8, flag 1) and forgets it.
void _TbSoundSampleMPEG2::CloseMpegFile() {
    if (mMpeg != nullptr) {
        mMpeg->Close();
        if (mMpeg != nullptr) delete mMpeg;
        mMpeg = nullptr;
    }
}

// No test for a missing decoder.
int32_t _TbSoundSampleMPEG2::GetMpegBlock(void* buffer, int32_t bytes) {
    return mMpeg->Read(buffer, bytes);
}

// ---------------------------------------------------------------- _TbSoundSampleMPEG2File

void* _TbSoundSampleMPEG2File::operator new(std::size_t size) { return PoolNew<MPEG2FilePool>(size); }
void _TbSoundSampleMPEG2File::operator delete(void* object, std::size_t size) { PoolDelete<MPEG2FilePool>(object, size); }
void _TbSoundSampleMPEG2File::TopUp() { PoolTopUp<MPEG2FilePool>(); }
void _TbSoundSampleMPEG2File::SetLevel(int32_t level) { mLevel_TbSoundSampleMPEG2File = level; }
void _TbSoundSampleMPEG2File::ClearCache() { PoolClearCache<MPEG2FilePool>(); }

// The memory codec's constructor, then the file, the start (the arguments are file, header, start, owner) and the header's data
// word (the cached first bytes of the sample). The pool link (+0x40) is not written.
_TbSoundSampleMPEG2File::_TbSoundSampleMPEG2File(bfl::FileSystem::CFile* file, TbSampleHeader* header, uint32_t start,
                                                 TbRefRelease* owner)
    : _TbSoundSampleMPEG2(header, owner), mFile(file), mStart(start), mData(header->mData) {}

_TbSoundSampleMPEG2File::~_TbSoundSampleMPEG2File() {}

// No priming of a layer I stream, unlike the memory codec's.
void _TbSoundSampleMPEG2File::Restart() {
    mDataPosition = 0;
    mMpeg->Reset();
}

void _TbSoundSampleMPEG2File::SetNewDataPosition() {}

// Opens the file player (a CMpegMacFilePlayerUnpooled in its pooled wrapper, the allocation 0x6d64 bytes on the Mac) over the
// bank's file: the file, the start, the length and the cached first bytes. The rest is the memory codec's.
TbError _TbSoundSampleMPEG2File::OpenMpegFile() {
    if (mMpeg != nullptr) return TbError(0);
    using Player = SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>;
    void* block = Player::operator new(sizeof(Player));
    Player* player = nullptr;
    if (block != nullptr) player = ::new (block) Player();
    mMpeg = player;
    int32_t opened = player->Open(mFile, static_cast<int32_t>(mStart), static_cast<int32_t>(mDataSize), static_cast<char*>(mData));
    if (opened != -1) return FinishOpen();
    return TbError(-1);
}

void _TbSoundSampleMPEG2File::CloseMpegFile() {
    if (mMpeg != nullptr) {
        mMpeg->Close();
        if (mMpeg != nullptr) delete mMpeg;
        mMpeg = nullptr;
    }
}

int32_t _TbSoundSampleMPEG2File::GetMpegBlock(void* buffer, int32_t bytes) {
    if (mMpeg == nullptr) return 0;
    return mMpeg->Read(buffer, bytes);
}

// The bytes of the sample left from the file's position (mDataSize - (position - mStart), unsigned) cap the count; the minimum
// is tested as a SIGNED number (a position outside the sample makes it huge or zero) and fails with -1.
int32_t _TbSoundSampleMPEG2File::Read(void* buffer, uint32_t count) {
    uint32_t position = static_cast<uint32_t>(mFile->GetPos());
    uint32_t left = mDataSize - (position - mStart);
    const uint32_t& wanted = LB_MIN<uint32_t, uint32_t>(count, left);   // 0x100081c4 (include/snd_handles.h)
    if (static_cast<int32_t>(wanted) <= 0) return -1;
    return mFile->Read(buffer, wanted);
}

int32_t _TbSoundSampleMPEG2File::Seek(int32_t offset) {
    int32_t position = mFile->Seek(mStart + static_cast<uint32_t>(offset), 0);
    return static_cast<int32_t>(static_cast<uint32_t>(position) - mStart);
}

// ---------------------------------------------------------------- the static cleanup objects

_CS__TbSoundSampleMPEG2::~_CS__TbSoundSampleMPEG2() { _TbSoundSampleMPEG2::ClearCache(); }

_CS__TbSoundSampleMPEG2File::~_CS__TbSoundSampleMPEG2File() { _TbSoundSampleMPEG2File::ClearCache(); }

namespace {
// The static objects the Mac's initialiser and finaliser run.
_CS__TbSoundSampleMPEG2 sCleanupMPEG2;
_CS__TbSoundSampleMPEG2File sCleanupMPEG2File;
}  // namespace
