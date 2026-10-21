// FileAccess.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of FileAccess code, named after it.

#include "CCodecFactory.h"
#include "helpers/LbMpegHelpers.h"
#include "ppc_math.h"
#include "TbMpegBase.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleCallback.h"
#include "TbSoundSampleMPEG2.h"
#include <cmath>
#include <cstring>
#include <new>

// The sound library's codec factory and the bank file wrapper (unit w80b, program "sound symbols shared"): CCodecFactory
// (LbMpeg.cpp) and _CFileAccess (LbMpeg.cpp). See include/CCodecFactory.h for the layouts.

// ---------------------------------------------------------------- _CFileAccess

// The position and the start first, then the file member is copy constructed from the file given (no test for NULL: the
// original dereferences it), then the semaphore.
_CFileAccess::_CFileAccess(bfl::FileSystem::CFile* file) : mPosition(0), mStart(0), mFile(*file) {}

_CFileAccess::_CFileAccess(bfl::FileSystem::CFile* file, uint32_t start) : mPosition(0), mStart(start), mFile(*file) {}

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
_CFileAccess::~_CFileAccess() {}

// CFormat::FillWaveFormatEx (0x100021b0), sound library, LbMpeg.cpp.

void CFormat::FillWaveFormatEx() {
    if (mFilled != 0) return;
    if (mInfo != nullptr) {
        mChannels = (mInfo->mFlags & 1) ? 2 : 1;
        mSamplesPerSec = mInfo->mRate;
        mBitsPerSample = mInfo->mBits;
    } else {
        mChannels = mChannelsByte;
        mSamplesPerSec = mRateWord;
        mBitsPerSample = mBitsByte;
    }
    mFilled = 1;
    mBlockAlign = static_cast<uint16_t>((static_cast<int32_t>(static_cast<uint32_t>(mChannels) * mBitsPerSample)) >> 3);
    mAvgBytesPerSec = mSamplesPerSec * mBlockAlign;
    mExtraSize = 0;
}

// The sound library's MPEG audio decoder object, part 1 (unit w71a): CMpegBase's pool, bit reader, header parser, frame
// reader and layer I decoder (program "sound symbols shared", LbMpeg.cpp and StreamableMpeg2.cpp). See include/TbMpegBase.h
// for the layout. PolySynth, dct64, Decode_Layer2 and II_step_two (unit w71b) are declared there and defined elsewhere.

int32_t muldiv(int32_t a, int32_t b, int32_t c) { return Div(Mul(a, b), c); }
