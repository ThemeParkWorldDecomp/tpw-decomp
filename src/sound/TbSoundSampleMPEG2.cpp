// TbSoundSampleMPEG2.cpp: name proven by __sinit_TbSoundSampleMPEG2_cpp in the sound library.

#include "AudioPlaceHolder.h"
#include "bfl_FileSystem.h"
#include "CMpegBase.h"
#include "helpers/StreamableMpeg2Helpers.h"
#include "LbTime.h"
#include "snd_containers.h"
#include "snd_handles.h"
#include "TbMpegBase.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundSampleMPEG2.h"
#include <cmath>
#include <new>

// The sound library's MPEG sample codecs (unit w80b, program "sound symbols shared"): _TbSoundSampleMPEG2
// (TbSoundSampleMPEG2.cpp) and _TbSoundSampleMPEG2File (StreamableMpeg2.cpp). See include/TbSoundSampleMPEG2.h for the layouts
// and the slot map.

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

// The Mac's LB_MIN<int,int> (0x1000e7d8, 24 bytes): `a < b ? a : b` on references, signed.
const int32_t& LbMinInt_2(const int32_t& a, const int32_t& b) {
    return a < b ? a : b;
}

}  // namespace

void _TbSoundSampleMPEG2::ClearCache() { PoolClearCache<MPEG2Pool>(); }

// ---------------------------------------------------------------- _TbSoundSampleMPEG2

void* _TbSoundSampleMPEG2::operator new(std::size_t size) { return PoolNew<MPEG2Pool>(size); }
void _TbSoundSampleMPEG2::operator delete(void* object, std::size_t size) { PoolDelete<MPEG2Pool>(object, size); }
void _TbSoundSampleMPEG2::TopUp() { PoolTopUp<MPEG2Pool>(); }
void _TbSoundSampleMPEG2::SetLevel(int32_t level) { mLevel_TbSoundSampleMPEG2 = level; }
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
    done = LbMinInt_2(size, left);
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

void _TbSoundSampleMPEG2::SetNewDataPosition() {}

void _TbSoundSampleMPEG2::SetDataPosition(uint32_t) {}

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

// Unit v51112: small functions of the sound program's AudioPlaceHolder, TbSoundSample*, CSprocketsModule ranges (see the header block).

extern "C" void __register_global_object(void* object, void* destructor, void* node);

uint32_t gCS_TbSoundSampleMPEG2Object;      // 0x1003f260
uint32_t gCS_TbSoundSampleMPEG2Node;        // 0x1003f264
void* DestructStandInMPEG2(void* self, int16_t);

void __sinit_TbSoundSampleMPEG2_cpp() {
    __register_global_object(&gCS_TbSoundSampleMPEG2Object, reinterpret_cast<void*>(&DestructStandInMPEG2),
                             &gCS_TbSoundSampleMPEG2Node);
}
