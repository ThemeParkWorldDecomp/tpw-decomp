// TbPixFmt.cpp (decompiled from the bullfrog library of SimTheme Park 2.04b3, functions
// at 0x10016264..0x10016534).
#include "TbPixFmt.h"

// __sinit_TbPixFmt_cpp at 0x10016484 (inline constructors).
TbPixFormat TbPixFormat8(0, 0, 0, 0, 8, 1);
TbPixFormat TbPixFormat16(0x7c00, 0x3e0, 0x1f, 0, 16, 0);
TbPixFormat TbPixFormat24(0xff0000, 0xff00, 0xff, 0, 24, 0);
TbPixFormat TbPixFormat32(0xff0000, 0xff00, 0xff, 0xff000000u, 32, 0);

void TbMaskInfo::Set(uint32_t mask, uint32_t topBit) {
    mMask = mask;
    uint32_t top = 31;
    if (mask != 0) {
        for (uint32_t bit = 0x80000000u; (mask & bit) == 0; bit >>= 1) --top;
    }
    if (top >= topBit) {
        mShiftRight = static_cast<int16_t>(top - topBit);
        mShiftLeft = 0;
    } else {
        mShiftRight = 0;
        mShiftLeft = static_cast<int16_t>(topBit - top);
    }
}

// The original inlines TbMaskInfo::Set and does alpha first.
void TbPixelCaps::Set(const TbPixFormat& format) {
    mAlpha.Set(format.mAlphaMask, 31);
    mRed.Set(format.mRedMask, 23);
    mGreen.Set(format.mGreenMask, 15);
    mBlue.Set(format.mBlueMask, 7);
}
