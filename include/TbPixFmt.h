// TbPixFmt.h: bullfrog library pixel formats (original TbPixFmt.cpp, named by its static
// initialiser; functions at 0x10016264..0x10016534).
#pragma once
#include <cstdint>

// A pixel layout: channel masks and depth. The four predefined formats are exported
// globals the static initialiser fills in (BSS 0x10073e88..0x10073ee8).
class TbPixFormat {
public:
    TbPixFormat(uint32_t red, uint32_t green, uint32_t blue, uint32_t alpha,
                int32_t bitsPerPixel, int32_t palettised)
        : mRedMask(red), mGreenMask(green), mBlueMask(blue), mAlphaMask(alpha),
          mBitsPerPixel(bitsPerPixel), mPalettised(palettised) {}

    uint32_t mRedMask;       // 0x00
    uint32_t mGreenMask;     // 0x04
    uint32_t mBlueMask;      // 0x08
    uint32_t mAlphaMask;     // 0x0c
    int32_t mBitsPerPixel;   // 0x10
    int32_t mPalettised;     // 0x14  1 only for TbPixFormat8 (name invented)
};

extern TbPixFormat TbPixFormat8;    // 0x10073ed0
extern TbPixFormat TbPixFormat16;   // 0x10073eb8  x1r5g5b5
extern TbPixFormat TbPixFormat24;   // 0x10073ea0
extern TbPixFormat TbPixFormat32;   // 0x10073e88  a8r8g8b8

// How to move one channel so that its top bit lands on bit `topBit`: shift right by
// mShiftRight or left by mShiftLeft (one of them is 0). A zero mask counts as bit 31.
class TbMaskInfo {
public:
    void Set(uint32_t mask, uint32_t topBit);   // 0x10016264

    uint32_t mMask;          // 0x00
    int16_t mShiftRight;     // 0x04
    int16_t mShiftLeft;      // 0x06
};

// The four channels mapped onto a8r8g8b8 (top bits 23, 15, 7 and 31).
class TbPixelCaps {
public:
    void Set(const TbPixFormat& format);         // 0x100162e8

    TbMaskInfo mRed;         // 0x00
    TbMaskInfo mGreen;       // 0x08
    TbMaskInfo mBlue;        // 0x10
    TbMaskInfo mAlpha;       // 0x18
};
