// Unit w241: Colour.cpp formatting and Draw.cpp palette converters.
#include "DrawRenderClasses.h"
#include "TbRaConvert.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

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

uint32_t LoadPixel(const uint8_t* source) {
    uint32_t value;
    std::memcpy(&value, source, sizeof(value));
    return value;
}

void StorePixel(uint8_t* destination, uint32_t bytes, uint32_t value) {
    if (bytes == 1) {
        destination[0] = static_cast<uint8_t>(value);
    } else if (bytes == 2) {
        const uint16_t word = static_cast<uint16_t>(value);
        std::memcpy(destination, &word, sizeof(word));
    } else if (bytes == 3) {
        destination[0] = static_cast<uint8_t>(value >> 24);
        destination[1] = static_cast<uint8_t>(value >> 16);
        destination[2] = static_cast<uint8_t>(value >> 8);
    } else if (bytes == 4) {
        destination[0] = static_cast<uint8_t>(value >> 24);
        destination[1] = static_cast<uint8_t>(value >> 16);
        destination[2] = static_cast<uint8_t>(value >> 8);
        destination[3] = static_cast<uint8_t>(value);
    }
}

void ShiftBits(uint32_t source, uint32_t destination,
               uint32_t& shiftRight, uint32_t& shiftLeft) {
    if (source == 0) {
        shiftRight = 0;
        shiftLeft = 0;
        return;
    }
    int32_t sourceTop = 31;
    uint32_t bit = 0x80000000u;
    while ((source & bit) == 0) {
        bit >>= 1;
        --sourceTop;
    }
    int32_t sourceLow = sourceTop;
    while ((source & bit) != 0) {
        bit >>= 1;
        --sourceLow;
    }
    const uint32_t sourceWidth = static_cast<uint32_t>(sourceTop - sourceLow);
    if (destination != 0) {
        int32_t destinationTop = 31;
        bit = 0x80000000u;
        while ((destination & bit) == 0) {
            bit >>= 1;
            --destinationTop;
        }
        int32_t destinationLow = destinationTop;
        while ((destination & bit) != 0) {
            bit >>= 1;
            --destinationLow;
        }
        const uint32_t destinationWidth =
            static_cast<uint32_t>(destinationTop - destinationLow);
        shiftRight = static_cast<uint32_t>(sourceTop) -
                     std::min(sourceWidth, destinationWidth) + 1;
        shiftLeft = static_cast<uint32_t>(destinationLow + 1);
        return;
    }
    shiftLeft = static_cast<uint32_t>(31 - sourceTop) + sourceWidth;
    shiftRight = 0;
    if (shiftLeft > 31) {
        shiftLeft = 0;
        shiftRight = 31;
    }
}

uint32_t PaletteWord(const uint8_t* palette, uint8_t index) {
    const uint8_t* p = palette + uint32_t(index) * 4;
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 |
           uint32_t(p[2]) << 8 | uint32_t(p[3]);
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

uint32_t CColour::GetFormattedValue(const TbMaskRotates& rotates) const {
    const uint32_t rgba = uint32_t(mRed) | uint32_t(mGreen) << 8 |
                          uint32_t(mBlue) << 16 | uint32_t(mAlpha) << 24;
    uint32_t result = 0;
    for (unsigned i = 0; i < 4; ++i)
        result |= RotateRight(rgba & rotates.mMask[i], rotates.mRotate[i]);
    return result;
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

} // namespace draw
} // namespace bfl

CPalToMaskConverter::CPalToMaskConverter() = default;
CPalToMaskConverter::~CPalToMaskConverter() = default;

void CPalToMaskConverter::GetShiftBits(uint32_t source, uint32_t destination,
                                       uint32_t& shiftRight, uint32_t& shiftLeft) {
    ShiftBits(source, destination, shiftRight, shiftLeft);
}

void CPalToMaskConverter::Init(const TbPalette& palette, const TbPixFormat& destination) {
    std::memcpy(mPalette, &palette, sizeof(mPalette));
    mDestinationBytes = (uint32_t(destination.mBitsPerPixel) + 7) >> 3;
    mSourceMask[0] = 0xff;
    mSourceMask[1] = 0xff00;
    mSourceMask[2] = 0xff0000;
    GetShiftBits(mSourceMask[0], destination.mRedMask, mShiftRight[0], mShiftLeft[0]);
    GetShiftBits(mSourceMask[1], destination.mGreenMask, mShiftRight[1], mShiftLeft[1]);
    GetShiftBits(mSourceMask[2], destination.mBlueMask, mShiftRight[2], mShiftLeft[2]);
    mAlphaMask = destination.mAlphaMask;
}

namespace {
uint32_t PalConverted(const CPalToMaskConverter& converter,
                      uint8_t blueIndex, uint8_t redGreenIndex) {
    const uint32_t blue = PaletteWord(converter.mPalette, blueIndex);
    const uint32_t redGreen = PaletteWord(converter.mPalette, redGreenIndex);
    return converter.mAlphaMask |
           (((redGreen & converter.mSourceMask[0]) >> converter.mShiftRight[0])
             << converter.mShiftLeft[0]) |
           (((redGreen & converter.mSourceMask[1]) >> converter.mShiftRight[1])
             << converter.mShiftLeft[1]) |
           (((blue & converter.mSourceMask[2]) >> converter.mShiftRight[2])
             << converter.mShiftLeft[2]);
}
}

void CPalToMaskConverter::ConvertLineUp(uint8_t* destination, uint8_t* source,
                                        int32_t count) {
    for (int32_t i = 0; i < count; ++i) {
        const uint32_t value = PalConverted(*this, source[i], source[i + 1]);
        StorePixel(destination + ptrdiff_t(i) * mDestinationBytes,
                   mDestinationBytes, value);
    }
}

void CPalToMaskConverter::ConvertLineDown(uint8_t* destination, uint8_t* source,
                                          int32_t count) {
    for (int32_t i = count - 1; i >= 0; --i) {
        const uint32_t value = PalConverted(*this, source[i], source[i - 1]);
        StorePixel(destination + ptrdiff_t(i) * mDestinationBytes,
                   mDestinationBytes, value);
    }
}

CMaskToPalConverter::CMaskToPalConverter(const TbPixFormat& format,
                                         const TbPalette& palette)
    : mPaletteSource(nullptr), mInverseMap(nullptr) {
    Init(format, palette);
}

CMaskToPalConverter::~CMaskToPalConverter() {
    Release();
}

void CMaskToPalConverter::GetShiftBits(uint32_t source, uint32_t destination,
                                       uint32_t& shiftRight, uint32_t& shiftLeft) {
    ShiftBits(source, destination, shiftRight, shiftLeft);
}

void CMaskToPalConverter::Init(const TbPixFormat& format, const TbPalette& palette) {
    std::memcpy(mPalette, &palette, sizeof(mPalette));
    mSourceBytes = (uint32_t(format.mBitsPerPixel) + 7) >> 3;
    mBits = 5;
    mSide = 1u << mBits;
    if (!mInverseMap) mInverseMap = static_cast<uint8_t*>(std::malloc(mSide * mSide * mSide));
    if (mInverseMap && &palette != mPaletteSource) {
        CInverseMap inverse;
        auto* distance = static_cast<uint32_t*>(
            std::malloc((1u << (mBits * 3)) * sizeof(uint32_t)));
        inverse.InvCMap(0, 256, palette, mBits, distance, mInverseMap);
        std::free(distance);
    }
    mPaletteSource = &palette;
    mSourceMask[0] = format.mRedMask;
    mSourceMask[1] = format.mGreenMask;
    mSourceMask[2] = format.mBlueMask;
    const uint32_t low = mInverseMap ? mSide - 1 : 0xff;
    const uint32_t middle = mInverseMap ? low << mBits : 0xff;
    const uint32_t high = mInverseMap ? middle << mBits : 0xff;
    GetShiftBits(mSourceMask[0], high, mShiftRight[0], mShiftLeft[0]);
    GetShiftBits(mSourceMask[1], middle, mShiftRight[1], mShiftLeft[1]);
    GetShiftBits(mSourceMask[2], low, mShiftRight[2], mShiftLeft[2]);
}

void CMaskToPalConverter::Release() {
    if (mInverseMap) std::free(mInverseMap);
}

uint8_t CMaskToPalConverter::BruteFindColour(uint8_t red, uint8_t green, uint8_t blue) {
    uint32_t bestDistance = 0x7fffffffu;
    uint8_t best = 0;
    for (uint32_t i = 0; i <= 0xff && bestDistance != 0; ++i) {
        const int32_t dr = int32_t(red) - mPalette[i * 4];
        const int32_t dg = int32_t(green) - mPalette[i * 4 + 1];
        const int32_t db = int32_t(blue) - mPalette[i * 4 + 2];
        const uint32_t distance = uint32_t(dr * dr + dg * dg + db * db);
        if (distance < bestDistance) {
            best = static_cast<uint8_t>(i);
            bestDistance = distance;
        }
    }
    return best;
}

namespace {
uint8_t MaskConverted(CMaskToPalConverter& converter, const uint8_t* source) {
    const uint32_t value = LoadPixel(source);
    const uint32_t red =
        ((value & converter.mSourceMask[0]) >> converter.mShiftRight[0])
        << converter.mShiftLeft[0];
    const uint32_t green =
        ((value & converter.mSourceMask[1]) >> converter.mShiftRight[1])
        << converter.mShiftLeft[1];
    const uint32_t blue =
        ((value & converter.mSourceMask[2]) >> converter.mShiftRight[2])
        << converter.mShiftLeft[2];
    if (!converter.mInverseMap)
        return converter.BruteFindColour(static_cast<uint8_t>(red),
                                         static_cast<uint8_t>(green),
                                         static_cast<uint8_t>(blue));
    const uint32_t index = red | green | blue;
    return converter.mInverseMap[index];
}
}

void CMaskToPalConverter::ConvertLineUp(uint8_t* destination, uint8_t* source,
                                        int32_t count) {
    for (int32_t i = 0; i < count; ++i)
        destination[i] = MaskConverted(*this, source + ptrdiff_t(i) * mSourceBytes);
}

void CMaskToPalConverter::ConvertLineDown(uint8_t* destination, uint8_t* source,
                                          int32_t count) {
    for (int32_t i = count - 1; i >= 0; --i)
        destination[i] = MaskConverted(*this, source + ptrdiff_t(i) * mSourceBytes);
}
