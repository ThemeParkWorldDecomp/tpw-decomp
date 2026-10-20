// CFormat::FillWaveFormatEx (0x100021b0), sound library, LbMpeg.cpp.
#include "TbMpegBase.h"

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
