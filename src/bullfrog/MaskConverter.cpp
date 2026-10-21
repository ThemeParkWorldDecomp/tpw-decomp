// MaskConverter.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of MaskConverter code, named after it.

#include "bfl_FileSystem.h"
#include "DrawRenderClasses.h"
#include "LbFile.h"
#include "platform/macdoze.h"
#include "TbRaConvert.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <new>

// Unit w241: Colour.cpp formatting and Draw.cpp palette converters.

namespace {

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

CPalToMaskConverter::CPalToMaskConverter() = default;
CPalToMaskConverter::~CPalToMaskConverter() = default;

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

void CPalToMaskConverter::GetShiftBits(uint32_t source, uint32_t destination,
                                       uint32_t& shiftRight, uint32_t& shiftLeft) {
    ShiftBits(source, destination, shiftRight, shiftLeft);
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

void CMaskToPalConverter::GetShiftBits(uint32_t source, uint32_t destination,
                                       uint32_t& shiftRight, uint32_t& shiftLeft) {
    ShiftBits(source, destination, shiftRight, shiftLeft);
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

// The bullfrog access chains and disc path.

namespace bfl::FileSystem {

TbError _TbBufferedFile::Open(CFile& file, uint32_t size) {
    CAccessItem* access = file.GetAccessClass();
    auto* buffer = new (std::nothrow) _TbBufferAccess();
    if (buffer == nullptr) return TbError(static_cast<int32_t>(0xcffe0100));
    if (!buffer->Open(size).Succeeded()) {
        delete buffer;
        return TbError(-1);
    }
    AttachAccessClass(access);
    AttachAccessClass(buffer);
    return TbError(0);
}
}
