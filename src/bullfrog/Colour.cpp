// Colour.cpp: name proven by __sinit_Colour_cpp in the Bullfrog library.

#include "DrawRenderClasses.h"
#include "LbSCBack.h"
#include "LbSemaphore.h"
#include "lobby.h"
#include "platform/samsutils.h"
#include "TbGraphicFile.h"
#include "TbPixFmt.h"
#include "TbRaConvert.h"
#include "WinScrn.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <new>

// Unit w241: Colour.cpp formatting and Draw.cpp palette converters.

namespace {

uint32_t RotateLeft(uint32_t value, uint32_t amount) {
    amount &= 31;
    return (value << amount) | (value >> ((32 - amount) & 31));
}

uint32_t RotateRight(uint32_t value, uint32_t amount) {
    return RotateLeft(value, 32 - (amount & 31));
}

bool SameFormat(const TbPixFormat& f, uint32_t red, uint32_t green,
                uint32_t blue, uint32_t alpha, int32_t bits) {
    return f.mRedMask == red && f.mGreenMask == green && f.mBlueMask == blue &&
           f.mAlphaMask == alpha && f.mBitsPerPixel == bits && f.mPalettised == 0;
}

} // namespace

namespace bfl {
namespace draw {

uint32_t TbMaskRotates::GetRightRotate2(uint32_t topBit, uint32_t mask) const {
    if (mask == 0) return 0;
    uint32_t maskTop = 31;
    for (uint32_t bit = 0x80000000u; (mask & bit) == 0; bit >>= 1) --maskTop;
    return topBit < maskTop ? topBit + 32 - maskTop : topBit - maskTop;
}

void TbMaskRotates::Init(const TbPixFormat& source, const TbPixFormat& destination) {
    const uint32_t sourceMasks[4] = {source.mRedMask, source.mGreenMask,
                                     source.mBlueMask, source.mAlphaMask};
    const uint32_t destinationMasks[4] = {destination.mRedMask, destination.mGreenMask,
                                          destination.mBlueMask, destination.mAlphaMask};
    for (unsigned i = 0; i < 4; ++i) {
        if (sourceMasks[i] == 0 || destinationMasks[i] == 0) mRotate[i] = 0;
        else {
            uint32_t sourceTop = 31, destinationTop = 31;
            for (uint32_t bit = 0x80000000u; (sourceMasks[i] & bit) == 0; bit >>= 1)
                --sourceTop;
            for (uint32_t bit = 0x80000000u; (destinationMasks[i] & bit) == 0; bit >>= 1)
                --destinationTop;
            mRotate[i] = sourceTop < destinationTop
                ? sourceTop + 32 - destinationTop : sourceTop - destinationTop;
        }
    }
    for (unsigned i = 0; i < 4; ++i) {
        // The original's blue entry reuses the red source mask and rotation.
        uint32_t sourceMask = sourceMasks[i == 2 ? 0 : i];
        if (sourceMask == 0) {
            mMask[i] = 0;
            continue;
        }
        uint32_t lowBit = 1;
        while ((sourceMask & lowBit) == 0) lowBit <<= 1;
        const unsigned destinationIndex = i == 2 ? 2 : i;
        const unsigned rotateIndex = i == 2 ? 0 : i;
        const uint32_t rotated = RotateLeft(destinationMasks[destinationIndex],
                                             mRotate[rotateIndex]);
        mMask[i] = (rotated & lowBit) != 0 ? sourceMask : rotated;
    }
}

uint32_t CColour::GetFormattedValue(const TbPixFormat& format) const {
    if (SameFormat(format, 0xf000, 0x0f00, 0x00f0, 0x000f, 16))
        return uint32_t(mAlpha >> 4) | (mRed & 0xf0u) |
               uint32_t(mBlue & 0x80u) << 8 | uint32_t(mGreen & 0xf0u) << 4;
    if (SameFormat(format, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff, 32))
        return uint32_t(mAlpha) | uint32_t(mRed) << 8 |
               uint32_t(mGreen) << 16 | uint32_t(mBlue) << 24;
    if (SameFormat(format, 0x7c00, 0x03e0, 0x001f, 0x8000, 16))
        return uint32_t(mAlpha & 0x80u) << 8 | uint32_t(mRed >> 3) |
               uint32_t(mGreen & 0xf8u) << 2 | uint32_t(mBlue & 0xf8u) << 7;
    if (SameFormat(format, 0x7c00, 0x03e0, 0x001f, 0, 16))
        return uint32_t(mRed >> 3) | uint32_t(mGreen & 0xf8u) << 2 |
               uint32_t(mBlue & 0xf8u) << 7;
    if (SameFormat(format, 0xf800, 0x07e0, 0x001f, 0, 16))
        return uint32_t(mRed >> 3) | uint32_t(mGreen & 0xfcu) << 3 |
               uint32_t(mBlue & 0xf8u) << 8;
    if (SameFormat(format, 0x0f00, 0x00f0, 0x000f, 0xf000, 16))
        return uint32_t(mRed >> 4) | (mGreen & 0xf0u) |
               uint32_t(mBlue & 0xf0u) << 4 | uint32_t(mAlpha & 0x80u) << 8;
    if (SameFormat(format, 0xff0000, 0xff00, 0xff, 0xff000000, 32))
        return uint32_t(mRed) | uint32_t(mGreen) << 8 |
               uint32_t(mBlue) << 16 | uint32_t(mAlpha) << 24;
    if (SameFormat(format, 0xff, 0xff00, 0xff0000, 0xff000000, 32))
        return uint32_t(mBlue) | uint32_t(mGreen) << 8 |
               uint32_t(mRed) << 16 | uint32_t(mAlpha) << 24;
    if (SameFormat(format, 0xff0000, 0xff00, 0xff, 0, 32) ||
        SameFormat(format, 0xff0000, 0xff00, 0xff, 0, 24))
        return uint32_t(mRed) | uint32_t(mGreen) << 8 | uint32_t(mBlue) << 16;
    if (SameFormat(format, 0xff, 0xff00, 0xff0000, 0, 32) ||
        SameFormat(format, 0xff, 0xff00, 0xff0000, 0, 24))
        return uint32_t(mBlue) | uint32_t(mGreen) << 8 | uint32_t(mRed) << 16;

    auto& rotates = *reinterpret_cast<TbMaskRotates*>(mFormatCache);
    rotates.mRotate[0] = rotates.GetRightRotate2(23, format.mRedMask);
    rotates.mRotate[1] = rotates.GetRightRotate2(15, format.mGreenMask);
    rotates.mRotate[2] = rotates.GetRightRotate2(7, format.mBlueMask);
    rotates.mRotate[3] = rotates.GetRightRotate2(31, format.mAlphaMask);
    rotates.mMask[0] = RotateLeft(format.mRedMask, rotates.mRotate[0]);
    rotates.mMask[1] = RotateLeft(format.mGreenMask, rotates.mRotate[1]);
    rotates.mMask[2] = RotateLeft(format.mBlueMask, rotates.mRotate[2]);
    rotates.mMask[3] = RotateLeft(format.mAlphaMask, rotates.mRotate[3]);
    return GetFormattedValue(rotates);
}

uint32_t CColour::GetFormattedValue(const TbMaskRotates& rotates) const {
    const uint32_t rgba = uint32_t(mRed) | uint32_t(mGreen) << 8 |
                          uint32_t(mBlue) << 16 | uint32_t(mAlpha) << 24;
    uint32_t result = 0;
    for (unsigned i = 0; i < 4; ++i)
        result |= RotateRight(rgba & rotates.mMask[i], rotates.mRotate[i]);
    return result;
}

} // namespace draw
} // namespace bfl

// Unit v51127: small bullfrog functions (LbSCBack.cpp application globals, the _LbSEFunction list instances, CConverter and TbJPEGLoader
// destructors, SamsUtilities swap helpers, TbPoint, _LbSemaLock stand-ins). Mac addresses are in the comments.

// CConverter::~CConverter 0x10019358.
CConverter* CConverter_DtorBody(CConverter* self, int16_t flag) {
    if (self != nullptr) {
        self->Release();
        if (flag > 0) LbSE_OperatorDelete(self);
    }
    return self;
}

// The pixel format records of the bullfrog library's Colour.cpp (unit v5L21). __sinit_Colour_cpp (0x1001a104, 460 bytes) stores
// the six words of each (four channel masks, the depth, palettised 0); the host initialises the same values as constants.
// The Mac has no names for them: the names say the layout (masks red, green, blue, alpha from the top). Addresses in the comments.

TbPixFormat gColourFormatRGBA8888(0xff000000u, 0xff0000, 0xff00, 0xff, 32, 0);        // 0x10073f38
TbPixFormat gColourFormatRGBA4444(0xf000, 0xf00, 0xf0, 0xf, 16, 0);                    // 0x10073f50
TbPixFormat gColourFormatXBGR8888(0xff, 0xff00, 0xff0000, 0, 32, 0);                   // 0x10073f68
TbPixFormat gColourFormatXRGB8888(0xff0000, 0xff00, 0xff, 0, 32, 0);                   // 0x10073f80
TbPixFormat gColourFormatABGR8888(0xff, 0xff00, 0xff0000, 0xff000000u, 32, 0);         // 0x10073f98
TbPixFormat gColourFormatARGB8888(0xff0000, 0xff00, 0xff, 0xff000000u, 32, 0);         // 0x10073fb0
TbPixFormat gColourFormatBGR888(0xff, 0xff00, 0xff0000, 0, 24, 0);                     // 0x10073fc8
TbPixFormat gColourFormatRGB888(0xff0000, 0xff00, 0xff, 0, 24, 0);                     // 0x10073fe0
TbPixFormat gColourFormatARGB4444(0xf00, 0xf0, 0xf, 0xf000, 16, 0);                    // 0x10073ff8
TbPixFormat gColourFormatRGB565(0xf800, 0x7e0, 0x1f, 0, 16, 0);                        // 0x10074010
TbPixFormat gColourFormatXRGB1555(0x7c00, 0x3e0, 0x1f, 0, 16, 0);                      // 0x10074028
TbPixFormat gColourFormatARGB1555(0x7c00, 0x3e0, 0x1f, 0x8000, 16, 0);                 // 0x10074040
// The only record with a TOC slot of its own (TOC+0x374, the others are at TOC+0x610..0x63c): the same layout as 0x10073fb0.
TbPixFormat gColourFormatARGB8888Default(0xff0000, 0xff00, 0xff, 0xff000000u, 32, 0);  // 0x10074058
