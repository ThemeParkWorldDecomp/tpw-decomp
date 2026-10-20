// Bullfrog TbRaConvert.cpp, unit w219. Mac addresses and extents are in TbRaConvert.h.
#include "TbRaConvert.h"
#include "ppc_math.h"
#include <algorithm>
#include <cstring>
#include <new>

CConverter gRaConverter;
int32_t gRedloopRxx;
int32_t gGreenloopHere, gGreenloopMin, gGreenloopMax, gGreenloopGinc;
int32_t gGreenloopGxx, gGreenloopGcdist;
uint32_t* gGreenloopGcdp;
uint8_t* gGreenloopGcrgbp;
int32_t gBlueloopHere, gBlueloopMin, gBlueloopMax, gBlueloopBinc;

namespace {
inline uint32_t Rotl(uint32_t value, uint32_t amount) {
    amount &= 31;
    return (value << amount) | (value >> ((32 - amount) & 31));
}
// A pair of register shifts is not a rotate when bit 5 of the amount is set.
inline uint32_t ShiftPair(uint32_t value, uint32_t amount) {
    return ppc_srw(value, amount) | ppc_slw(value, 32 - amount);
}
inline uint32_t ReadPixel(const uint8_t* s) {
    return uint32_t(s[0]) | (uint32_t(s[1]) << 8) | (uint32_t(s[2]) << 16) | (uint32_t(s[3]) << 24);
}
inline void Store16(uint8_t* d, uint32_t value) {
    uint16_t word = static_cast<uint16_t>(value);
    std::memcpy(d, &word, 2); // Engine 16-bit texels are host-order words, even at odd addresses.
}
inline void Store24(uint8_t* d, uint32_t value) {
    d[0] = value >> 24;
    d[1] = value >> 16;
    d[2] = value >> 8;
}
inline void Store32(uint8_t* d, uint32_t value) {
    d[0] = value >> 24;
    d[1] = value >> 16;
    d[2] = value >> 8;
    d[3] = value;
}
}

CConverter::CConverter() : mConverter(nullptr), mKind(4) {}
CConverterBase::~CConverterBase() = default;
CMaskConverter::~CMaskConverter() = default;
CInverseMap::CInverseMap() {}
CInverseMap::~CInverseMap() = default;
TbRAConv::~TbRAConv() = default;

void CConverter::Init(const TbPixFormat* destination, const TbPixFormat* source,
                      const TbPalette* palette, uint32_t unused) {
    (void)unused;
    mSourceBytes = (uint32_t(source->mBitsPerPixel) + 7) >> 3;
    mDestBytes = (uint32_t(destination->mBitsPerPixel) + 7) >> 3;
    if (source->mPalettised == 0 && destination->mPalettised == 0) {
        if (mKind == 1) {
            static_cast<CMaskConverter*>(mConverter)->Init(*source, *destination);
        } else {
            delete mConverter;
            auto* converter = new CMaskConverter;
            if (converter) converter->Init(*source, *destination);
            mConverter = converter;
            mKind = 1;
        }
    } else if (source->mPalettised != 0 && destination->mPalettised == 0) {
        if (palette) {
            if (mKind == 2) {
                static_cast<CPalToMaskConverter*>(mConverter)->Init(*palette, *destination);
            } else {
                delete mConverter;
                // The Mac inlines this constructor, whose only effect is its vptr.
                auto* converter = new CPalToMaskConverter;
                if (converter) converter->Init(*palette, *destination);
                mConverter = converter;
                mKind = 2;
            }
        }
    } else if (source->mPalettised == 0 && destination->mPalettised != 0 && palette) {
        if (mKind == 3) {
            static_cast<CMaskToPalConverter*>(mConverter)->Init(*source, *palette);
        } else {
            delete mConverter;
            auto* converter = new CMaskToPalConverter(*source, *palette);
            mConverter = converter;
            mKind = 3;
        }
    }
}
void CConverter::ConvBuf(uint8_t* dest, uint8_t* source, int32_t count) {
    if (mSourceBytes < mDestBytes) mConverter->ConvertLineDown(dest, source, count);
    else mConverter->ConvertLineUp(dest, source, count);
}
void CConverter::ConvertArea(uint8_t* dest, uint8_t* source, uint32_t destPitch,
                             uint32_t sourcePitch, uint32_t rows, uint32_t width) {
    if (!dest) dest = source;
    // This is the opposite size comparison to ConvBuf, as in the Mac listing.
    bool down = dest == source && mDestBytes < mSourceBytes;
    while (rows-- != 0) {
        if (down) mConverter->ConvertLineDown(dest, source, width);
        else mConverter->ConvertLineUp(dest, source, width);
        dest += destPitch;
        source += sourcePitch;
    }
}
void CConverter::Release() {
    if (mConverter) {
        delete mConverter;
        mConverter = nullptr;
        mKind = 4;
    }
}
void CMaskConverter::Init(const TbPixFormat& source, const TbPixFormat& destination) {
    mDestBytes = (uint32_t(destination.mBitsPerPixel) + 7) >> 3;
    mSourceBytes = (uint32_t(source.mBitsPerPixel) + 7) >> 3;
    mSourceMask[0] = source.mRedMask;
    mSourceMask[1] = source.mGreenMask;
    mSourceMask[2] = source.mBlueMask;
    mSourceMask[3] = source.mAlphaMask;
    mDestMask[0] = destination.mRedMask;
    mDestMask[1] = destination.mGreenMask;
    mDestMask[2] = destination.mBlueMask;
    mDestMask[3] = destination.mAlphaMask;
    mWiden = 0;
    GetShiftBits(source.mRedMask, destination.mRedMask, &mShift[0], &mWidth[0]);
    GetShiftBits(source.mGreenMask, destination.mGreenMask, &mShift[1], &mWidth[1]);
    GetShiftBits(source.mBlueMask, destination.mBlueMask, &mShift[2], &mWidth[2]);
    GetShiftBits(source.mAlphaMask, destination.mAlphaMask, &mShift[3], &mWidth[3]);
    mAlphaFill = 0;
    if (source.mAlphaMask == 0) mAlphaFill = destination.mAlphaMask;
}
void CMaskConverter::GetShiftBits(uint32_t source, uint32_t dest, uint32_t* shift, uint32_t* width) {
    if (source == 0 || dest == 0) {
        *shift = 0;
        return; // The width retains its old value.
    }
    uint32_t bit = 0x80000000;
    int32_t low = 31;
    while (!(source & bit)) { bit >>= 1; --low; }
    int32_t sourceTop = low;
    while (source & bit) { bit >>= 1; --low; }
    int32_t sourceWidth = sourceTop - low;
    bit = 0x80000000;
    low = 31;
    while (!(dest & bit)) { bit >>= 1; --low; }
    int32_t destTop = low;
    while (dest & bit) { bit >>= 1; --low; }
    if (sourceWidth < destTop - low && mWiden == 0) mWiden = 1;
    int32_t amount = sourceTop - destTop;
    *width = sourceWidth;
    if (amount < 0) amount += 32;
    *shift = amount;
}

void CMaskConverter::ConvertLineUp(uint8_t* dest, uint8_t* source, int32_t count) {
    if (mDestBytes == 1) {
        if (mWiden == 0) {
            for (; count != 0; --count) {
                uint32_t pixel = ReadPixel(source);
                uint32_t c0 = pixel & mSourceMask[0];
                uint32_t c1 = pixel & mSourceMask[1];
                uint32_t c2 = pixel & mSourceMask[2];
                uint32_t c3 = pixel & mSourceMask[3];
                uint32_t value = mAlphaFill
                    | (mDestMask[0] & ShiftPair(c0, mShift[0]))
                    | (mDestMask[1] & ShiftPair(c1, mShift[1]))
                    | (mDestMask[2] & Rotl(c2, mDestMask[2]))
                    | (mDestMask[3] & ShiftPair(c3, mShift[3]));
                *dest = static_cast<uint8_t>(value);
                dest += 1;
                source += mSourceBytes;
            }
        } else {
            for (; count != 0; --count) {
                uint32_t pixel = ReadPixel(source);
                uint32_t c0 = pixel & mSourceMask[0];
                uint32_t c1 = pixel & mSourceMask[1];
                uint32_t c2 = pixel & mSourceMask[2];
                uint32_t c3 = pixel & mSourceMask[3];
                uint32_t firstGreen = Rotl(mDestMask[0], 32 - mShift[1]);
                uint32_t secondAlpha = ppc_srw(c3, mShift[3] + mWidth[3]);
                uint32_t value = mAlphaFill
                    | (mDestMask[0] & (ShiftPair(c0, mShift[0]) | ShiftPair(c0, mShift[0] + mWidth[0])))
                    | (mDestMask[1] & (firstGreen | ShiftPair(c1, mShift[1] + mWidth[1])))
                    | (mDestMask[2] & (ShiftPair(c2, mShift[2]) | Rotl(c2, firstGreen)))
                    | (mDestMask[3] & (Rotl(c3, secondAlpha) | secondAlpha | ppc_slw(c3, 32 - (mShift[3] + mWidth[3]))));
                *dest = static_cast<uint8_t>(value);
                dest += 1;
                source += mSourceBytes;
            }
        }
    }
    else if (mDestBytes == 2) {
        if (mWiden == 0) {
            for (; count != 0; --count) {
                uint32_t pixel = ReadPixel(source);
                uint32_t c0 = pixel & mSourceMask[0];
                uint32_t c1 = pixel & mSourceMask[1];
                uint32_t c2 = pixel & mSourceMask[2];
                uint32_t c3 = pixel & mSourceMask[3];
                uint32_t value = mAlphaFill
                    | (mDestMask[0] & ShiftPair(c0, mShift[0]))
                    | (mDestMask[1] & ShiftPair(c1, mShift[1]))
                    | (mDestMask[2] & Rotl(c2, mDestMask[2]))
                    | (mDestMask[3] & ShiftPair(c3, mShift[3]));
                Store16(dest, value);
                dest += 2;
                source += mSourceBytes;
            }
        } else {
            for (; count != 0; --count) {
                uint32_t pixel = ReadPixel(source);
                uint32_t c0 = pixel & mSourceMask[0];
                uint32_t c1 = pixel & mSourceMask[1];
                uint32_t c2 = pixel & mSourceMask[2];
                uint32_t c3 = pixel & mSourceMask[3];
                uint32_t firstGreen = Rotl(mDestMask[0], 32 - mShift[1]);
                uint32_t secondAlpha = ppc_srw(c3, mShift[3] + mWidth[3]);
                uint32_t value = mAlphaFill
                    | (mDestMask[0] & (ShiftPair(c0, mShift[0]) | ShiftPair(c0, mShift[0] + mWidth[0])))
                    | (mDestMask[1] & (firstGreen | ShiftPair(c1, mShift[1] + mWidth[1])))
                    | (mDestMask[2] & (ShiftPair(c2, mShift[2]) | Rotl(c2, firstGreen)))
                    | (mDestMask[3] & (Rotl(c3, secondAlpha) | secondAlpha | ppc_slw(c3, 32 - (mShift[3] + mWidth[3]))));
                Store16(dest, value);
                dest += 2;
                source += mSourceBytes;
            }
        }
    }
    else if (mDestBytes == 3) {
        if (mWiden == 0) {
            for (; count != 0; --count) {
                uint32_t pixel = ReadPixel(source);
                uint32_t c0 = pixel & mSourceMask[0];
                uint32_t c1 = pixel & mSourceMask[1];
                uint32_t c2 = pixel & mSourceMask[2];
                uint32_t c3 = pixel & mSourceMask[3];
                uint32_t green = mDestMask[1] & ShiftPair(c1, mShift[1]);
                uint32_t red = mAlphaFill | (mDestMask[0] & ShiftPair(c0, mShift[0]));
                uint32_t value = red | green
                    | (mDestMask[2] & ShiftPair(c2, mShift[2]))
                    | (mDestMask[3] & Rotl(red, green));
                Store24(dest, value);
                dest += 3;
                source += mSourceBytes;
            }
        } else {
            for (; count != 0; --count) {
                uint32_t pixel = ReadPixel(source);
                uint32_t c0 = pixel & mSourceMask[0];
                uint32_t c1 = pixel & mSourceMask[1];
                uint32_t c2 = pixel & mSourceMask[2];
                uint32_t c3 = pixel & mSourceMask[3];
                uint32_t secondRed = ppc_srw(c0, mShift[0] + mWidth[0]);
                uint32_t value = mAlphaFill
                    | (mDestMask[0] & (Rotl(c0, secondRed) | secondRed | ppc_slw(c0, 32 - (mShift[0] + mWidth[0]))))
                    | (mDestMask[1] & (ShiftPair(c1, mShift[1]) | Rotl(c1, 32 - (mShift[1] + mWidth[1]))))
                    | (mDestMask[2] & (Rotl(mAlphaFill, 32 - mShift[2]) | Rotl(mAlphaFill, mDestMask[1])))
                    | (mDestMask[3] & (Rotl(c3, 32 - mShift[3]) | Rotl(c3, mDestMask[2])));
                Store24(dest, value);
                dest += 3;
                source += mSourceBytes;
            }
        }
    }
    else if (mDestBytes == 4) {
        if (mWiden == 0) {
            for (; count != 0; --count) {
                uint32_t pixel = ReadPixel(source);
                uint32_t c0 = pixel & mSourceMask[0];
                uint32_t c1 = pixel & mSourceMask[1];
                uint32_t c2 = pixel & mSourceMask[2];
                uint32_t c3 = pixel & mSourceMask[3];
                uint32_t value = mAlphaFill
                    | (mDestMask[0] & ShiftPair(c0, mShift[0]))
                    | (mDestMask[1] & ShiftPair(c1, mShift[1]))
                    | (mDestMask[2] & Rotl(c2, mDestMask[2]))
                    | (mDestMask[3] & ShiftPair(c3, mShift[3]));
                Store32(dest, value);
                dest += 4;
                source += mSourceBytes;
            }
        } else {
            for (; count != 0; --count) {
                uint32_t pixel = ReadPixel(source);
                uint32_t c0 = pixel & mSourceMask[0];
                uint32_t c1 = pixel & mSourceMask[1];
                uint32_t c2 = pixel & mSourceMask[2];
                uint32_t c3 = pixel & mSourceMask[3];
                uint32_t firstGreen = Rotl(mDestMask[0], 32 - mShift[1]);
                uint32_t secondAlpha = ppc_srw(c3, mShift[3] + mWidth[3]);
                uint32_t value = mAlphaFill
                    | (mDestMask[0] & (ShiftPair(c0, mShift[0]) | ShiftPair(c0, mShift[0] + mWidth[0])))
                    | (mDestMask[1] & (firstGreen | ShiftPair(c1, mShift[1] + mWidth[1])))
                    | (mDestMask[2] & (ShiftPair(c2, mShift[2]) | Rotl(c2, firstGreen)))
                    | (mDestMask[3] & (Rotl(c3, secondAlpha) | secondAlpha | ppc_slw(c3, 32 - (mShift[3] + mWidth[3]))));
                Store32(dest, value);
                dest += 4;
                source += mSourceBytes;
            }
        }
    }
}

void CMaskConverter::ConvertLineDown(uint8_t* dest, uint8_t* source, int32_t count) {
    dest += int32_t((uint32_t(count) - 1) * mDestBytes);
    source += int32_t((uint32_t(count) - 1) * mSourceBytes);
    if (mDestBytes == 1) {
        if (mWiden == 0) {
            for (; count != 0; --count) {
                uint32_t pixel = ReadPixel(source);
                uint32_t c0 = pixel & mSourceMask[0];
                uint32_t c1 = pixel & mSourceMask[1];
                uint32_t c2 = pixel & mSourceMask[2];
                uint32_t c3 = pixel & mSourceMask[3];
                uint32_t value = mAlphaFill
                    | (mDestMask[0] & ShiftPair(c0, mShift[0]))
                    | (mDestMask[1] & ShiftPair(c1, mShift[1]))
                    | (mDestMask[2] & Rotl(c2, mDestMask[2]))
                    | (mDestMask[3] & ShiftPair(c3, mShift[3]));
                *dest = static_cast<uint8_t>(value);
                dest -= 1;
                source -= mSourceBytes;
            }
        } else {
            for (; count != 0; --count) {
                uint32_t pixel = ReadPixel(source);
                uint32_t c0 = pixel & mSourceMask[0];
                uint32_t c1 = pixel & mSourceMask[1];
                uint32_t c2 = pixel & mSourceMask[2];
                uint32_t c3 = pixel & mSourceMask[3];
                uint32_t red = Rotl(c0, 32 - mShift[0]) | Rotl(c0, 32 - (mShift[0] + mWidth[0]));
                uint32_t firstGreen = Rotl(c1, 32 - mShift[1]);
                uint32_t green = firstGreen | Rotl(c1, red);
                uint32_t value = mAlphaFill
                    | (mDestMask[0] & red)
                    | (mDestMask[1] & green)
                    | (mDestMask[2] & (Rotl(c2, firstGreen) | Rotl(c2, green)))
                    | (mDestMask[3] & (ShiftPair(c3, mShift[3]) | ShiftPair(c3, mShift[3] + mWidth[3])));
                *dest = static_cast<uint8_t>(value);
                dest -= 1;
                source -= mSourceBytes;
            }
        }
    }
    else if (mDestBytes == 2) {
        if (mWiden == 0) {
            for (; count != 0; --count) {
                uint32_t pixel = ReadPixel(source);
                uint32_t c0 = pixel & mSourceMask[0];
                uint32_t c1 = pixel & mSourceMask[1];
                uint32_t c2 = pixel & mSourceMask[2];
                uint32_t c3 = pixel & mSourceMask[3];
                uint32_t value = mAlphaFill
                    | (mDestMask[0] & ShiftPair(c0, mShift[0]))
                    | (mDestMask[1] & ShiftPair(c1, mShift[1]))
                    | (mDestMask[2] & Rotl(c2, mDestMask[2]))
                    | (mDestMask[3] & ShiftPair(c3, mShift[3]));
                Store16(dest, value);
                dest -= 2;
                source -= mSourceBytes;
            }
        } else {
            for (; count != 0; --count) {
                uint32_t pixel = ReadPixel(source);
                uint32_t c0 = pixel & mSourceMask[0];
                uint32_t c1 = pixel & mSourceMask[1];
                uint32_t c2 = pixel & mSourceMask[2];
                uint32_t c3 = pixel & mSourceMask[3];
                uint32_t red = Rotl(c0, 32 - mShift[0]) | Rotl(c0, 32 - (mShift[0] + mWidth[0]));
                uint32_t firstGreen = Rotl(c1, 32 - mShift[1]);
                uint32_t green = firstGreen | Rotl(c1, red);
                uint32_t value = mAlphaFill
                    | (mDestMask[0] & red)
                    | (mDestMask[1] & green)
                    | (mDestMask[2] & (Rotl(c2, firstGreen) | Rotl(c2, green)))
                    | (mDestMask[3] & (ShiftPair(c3, mShift[3]) | ShiftPair(c3, mShift[3] + mWidth[3])));
                Store16(dest, value);
                dest -= 2;
                source -= mSourceBytes;
            }
        }
    }
    else if (mDestBytes == 3) {
        if (mWiden == 0) {
            for (; count != 0; --count) {
                uint32_t pixel = ReadPixel(source);
                uint32_t c0 = pixel & mSourceMask[0];
                uint32_t c1 = pixel & mSourceMask[1];
                uint32_t c2 = pixel & mSourceMask[2];
                uint32_t c3 = pixel & mSourceMask[3];
                uint32_t value = mAlphaFill
                    | (mDestMask[0] & ShiftPair(c0, mShift[0]))
                    | (mDestMask[1] & ShiftPair(c1, mShift[1]))
                    | (mDestMask[2] & Rotl(c2, mDestMask[2]))
                    | (mDestMask[3] & ShiftPair(c3, mShift[3]));
                Store24(dest, value);
                dest -= 3;
                source -= mSourceBytes;
            }
        } else {
            for (; count != 0; --count) {
                uint32_t pixel = ReadPixel(source);
                uint32_t c0 = pixel & mSourceMask[0];
                uint32_t c1 = pixel & mSourceMask[1];
                uint32_t c2 = pixel & mSourceMask[2];
                uint32_t c3 = pixel & mSourceMask[3];
                uint32_t secondRed = ppc_srw(c0, mShift[0] + mWidth[0]);
                uint32_t secondGreen = ppc_srw(c1, mShift[1] + mWidth[1]);
                uint32_t greenRotation = Rotl(c1, secondGreen);
                uint32_t value = mAlphaFill
                    | (mDestMask[0] & (Rotl(c0, secondRed) | secondRed | ppc_slw(c0, 32 - (mShift[0] + mWidth[0]))))
                    | (mDestMask[1] & (greenRotation | secondGreen | ppc_slw(c1, 32 - (mShift[1] + mWidth[1]))))
                    | (mDestMask[2] & (ShiftPair(c2, mShift[2]) | Rotl(c2, greenRotation)))
                    | (mDestMask[3] & (Rotl(c3, 32 - mShift[3]) | ShiftPair(c3, mShift[3] + mWidth[3])));
                Store24(dest, value);
                dest -= 3;
                source -= mSourceBytes;
            }
        }
    }
    else if (mDestBytes == 4) {
        if (mWiden == 0) {
            for (; count != 0; --count) {
                uint32_t pixel = ReadPixel(source);
                uint32_t c0 = pixel & mSourceMask[0];
                uint32_t c1 = pixel & mSourceMask[1];
                uint32_t c2 = pixel & mSourceMask[2];
                uint32_t c3 = pixel & mSourceMask[3];
                uint32_t value = mAlphaFill
                    | (mDestMask[0] & ShiftPair(c0, mShift[0]))
                    | (mDestMask[1] & ShiftPair(c1, mShift[1]))
                    | (mDestMask[2] & Rotl(c2, mDestMask[2]))
                    | (mDestMask[3] & ShiftPair(c3, mShift[3]));
                Store32(dest, value);
                dest -= 4;
                source -= mSourceBytes;
            }
        } else {
            for (; count != 0; --count) {
                uint32_t pixel = ReadPixel(source);
                uint32_t c0 = pixel & mSourceMask[0];
                uint32_t c1 = pixel & mSourceMask[1];
                uint32_t c2 = pixel & mSourceMask[2];
                uint32_t c3 = pixel & mSourceMask[3];
                uint32_t red = ShiftPair(c0, mShift[0]) | Rotl(c0, 32 - (mShift[0] + mWidth[0]));
                uint32_t firstGreen = Rotl(red, 32 - mShift[1]);
                uint32_t green = firstGreen | Rotl(red, 32 - (mShift[1] + mWidth[1]));
                uint32_t value = mAlphaFill
                    | (mDestMask[0] & red)
                    | (mDestMask[1] & green)
                    | (mDestMask[2] & (Rotl(c2, firstGreen) | Rotl(c2, green)))
                    | (mDestMask[3] & (ShiftPair(c3, mShift[3]) | ShiftPair(c3, mShift[3] + mWidth[3])));
                Store32(dest, value);
                dest -= 4;
                source -= mSourceBytes;
            }
        }
    }
}

void CInverseMap::maxfill(uint32_t* buffer, int32_t side) {
    int32_t n = side * side * side;
    if (n < 1) return;
    while (n >= 8) {
        buffer[0] = buffer[1] = buffer[2] = buffer[3] = 0xffffffff;
        buffer[4] = buffer[5] = buffer[6] = buffer[7] = 0xffffffff;
        buffer += 8;
        n -= 8;
    }
    while (n-- != 0) *buffer++ = 0xffffffff;
}
void CInverseMap::InvCMap(int32_t first, int32_t count, const TbPalette& palette, int32_t bits,
                         uint32_t* distBuf, uint8_t* rgbMap) {
    int32_t nbits = 8 - bits;
    colormax = ppc_slw(1, bits);
    x = ppc_slw(1, nbits);
    xsqr = ppc_slw(1, nbits * 2);
    gstride = colormax;
    rstride = colormax * colormax;
    maxfill(distBuf, colormax);
    cindex = first;
    const auto* colors = reinterpret_cast<const uint8_t*>(&palette);
    while (cindex < first + count) {
        rcenter = ppc_sraw(colors[cindex * 4], nbits);
        gcenter = ppc_sraw(colors[cindex * 4 + 1], nbits);
        bcenter = ppc_sraw(colors[cindex * 4 + 2], nbits);
        rdist = colors[cindex * 4] - (x / 2 + rcenter * x);
        gdist = colors[cindex * 4 + 1] - (x / 2 + gcenter * x);
        cdist = colors[cindex * 4 + 2] - (x / 2 + bcenter * x);
        cdist = rdist * rdist + (gdist * gdist + cdist * cdist);
        crinc = ((rcenter + 1) * xsqr - x * colors[cindex * 4]) * 2;
        cginc = ((gcenter + 1) * xsqr - x * colors[cindex * 4 + 1]) * 2;
        cbinc = ((bcenter + 1) * xsqr - x * colors[cindex * 4 + 2]) * 2;
        cdp = distBuf + bcenter + gcenter * gstride + rcenter * rstride;
        crgbp = rgbMap + rcenter * rstride + gcenter * gstride + bcenter;
        redloop();
        ++cindex;
    }
}
int32_t CInverseMap::redloop() {
    int32_t detect = 0, restart = 1;
    int32_t square = xsqr;
    int32_t red = rcenter;
    rdist = cdist;
    gRedloopRxx = crinc;
    rdp = cdp;
    rrgbp = crgbp;
    for (; red < colormax; ++red) {
        restart = greenloop(restart);
        if (restart == 0) { if (detect != 0) break; }
        else detect = 1;
        restart = 0;
        rdp += rstride;
        rrgbp += rstride;
        rdist += gRedloopRxx;
        gRedloopRxx += square * 2;
    }
    restart = 1;
    red = rcenter;
    gRedloopRxx = crinc - square * 2;
    rdist = cdist - gRedloopRxx;
    rdp = cdp - rstride;
    rrgbp = crgbp - rstride;
    while (--red >= 0) {
        restart = greenloop(restart);
        if (restart == 0) { if (detect != 0) return detect; }
        else detect = 1;
        restart = 0;
        rdp -= rstride;
        rrgbp -= rstride;
        gRedloopRxx -= square * 2;
        rdist -= gRedloopRxx;
    }
    return detect;
}
int32_t CInverseMap::greenloop(int32_t restart) {
    int32_t square = xsqr;
    if (restart != 0) {
        gGreenloopHere = gcenter;
        gGreenloopMin = 0;
        gGreenloopMax = colormax - 1;
        gGreenloopGinc = cginc;
    }
    int32_t green = gGreenloopHere, detect = 0;
    gGreenloopGcdist = rdist;
    restart = 1;
    gdist = gGreenloopGcdist;
    gGreenloopGxx = gGreenloopGinc;
    gGreenloopGcdp = rdp;
    gdp = gGreenloopGcdp;
    gGreenloopGcrgbp = rrgbp;
    grgbp = gGreenloopGcrgbp;
    for (; green <= gGreenloopMax; ++green) {
        restart = blueloop(restart);
        if (restart == 0) { if (detect != 0) break; }
        else if (detect == 0) {
            if (gGreenloopHere < green) {
                gGreenloopHere = green;
                rdp = gGreenloopGcdp;
                rrgbp = gGreenloopGcrgbp;
                rdist = gGreenloopGcdist;
                gGreenloopGinc = gGreenloopGxx;
            }
            detect = 1;
        }
        restart = 0;
        gdp += gstride;
        gGreenloopGcdp += gstride;
        grgbp += gstride;
        gGreenloopGcrgbp += gstride;
        gdist += gGreenloopGxx;
        gGreenloopGcdist += gGreenloopGxx;
        gGreenloopGxx += square * 2;
    }
    green = gGreenloopHere;
    restart = 1;
    gGreenloopGxx = gGreenloopGinc - square * 2;
    gGreenloopGcdist = rdist - gGreenloopGxx;
    gdist = gGreenloopGcdist;
    gGreenloopGcdp = rdp - gstride;
    gdp = gGreenloopGcdp;
    gGreenloopGcrgbp = rrgbp - gstride;
    grgbp = gGreenloopGcrgbp;
    while (--green >= gGreenloopMin) {
        restart = blueloop(restart);
        if (restart == 0) { if (detect != 0) return detect; }
        else if (detect == 0) {
            detect = 1;
            gGreenloopHere = green;
            rdp = gGreenloopGcdp;
            rrgbp = gGreenloopGcrgbp;
            rdist = gGreenloopGcdist;
            gGreenloopGinc = gGreenloopGxx;
        }
        restart = 0;
        gdp -= gstride;
        gGreenloopGcdp -= gstride;
        grgbp -= gstride;
        gGreenloopGcrgbp -= gstride;
        gGreenloopGxx -= square * 2;
        gdist -= gGreenloopGxx;
        gGreenloopGcdist -= gGreenloopGxx;
    }
    return detect;
}
int32_t CInverseMap::blueloop(int32_t restart) {
    int32_t square = xsqr, index = cindex;
    if (restart != 0) {
        gBlueloopHere = bcenter;
        gBlueloopMin = 0;
        gBlueloopMax = colormax - 1;
        gBlueloopBinc = cbinc;
    }
    int32_t max = gBlueloopMax, detect = 0;
    uint32_t distance = gdist;
    uint32_t* dp = gdp;
    uint8_t* rgbp = grgbp;
    int32_t blue = gBlueloopHere, inc = gBlueloopBinc;
    for (; blue <= max; ++blue) {
        if (distance < *dp) {
            if (gBlueloopHere < blue) {
                gBlueloopHere = blue;
                gdp = dp;
                grgbp = rgbp;
                gdist = distance;
                gBlueloopBinc = inc;
            }
            detect = 1;
            break;
        }
        distance += inc;
        inc += square * 2;
        ++dp;
        ++rgbp;
    }
    int32_t n = max + 1 - blue;
    if (blue <= max) {
        do {
            if (*dp <= distance) break;
            *dp++ = distance;
            distance += inc;
            inc += square * 2;
            *rgbp++ = static_cast<uint8_t>(index);
        } while (--n != 0);
    }
    int32_t min = gBlueloopMin;
    inc = gBlueloopBinc - square * 2;
    blue = gBlueloopHere - 1;
    dp = gdp - 1;
    distance = uint32_t(gdist) - inc;
    rgbp = grgbp - 1;
    if (detect == 0) {
        for (; blue >= min; --blue) {
            if (distance < *dp) {
                detect = 1;
                gBlueloopHere = blue;
                gdp = dp;
                grgbp = rgbp;
                gdist = distance;
                gBlueloopBinc = inc;
                break;
            }
            inc -= square * 2;
            distance -= inc;
            --dp;
            --rgbp;
        }
    }
    n = blue + 1 - min;
    if (min <= blue) {
        do {
            if (*dp <= distance) return detect;
            *dp-- = distance;
            inc -= square * 2;
            distance -= inc;
            *rgbp-- = static_cast<uint8_t>(index);
        } while (--n != 0);
    }
    return detect;
}
TbResult TbRAConv::Move(TbRenderArea& destination, TbRenderArea& source, TbPalette* palette) {
    if (!destination.mData || !source.mData) return TbResult{-1};
    bool same = std::memcmp(source.mF00, destination.mF00, 16) == 0
        && source.mDepth == destination.mDepth
        && (source.mPalettised == 0) == (destination.mPalettised == 0);
    if (same) {
        if (destination.mData != source.mData) {
            uint32_t width = std::min(uint32_t(destination.mWidth), uint32_t(source.mWidth));
            uint32_t bytes = width * ((uint32_t(destination.mDepth) + 7) >> 3);
            uint32_t height = std::min(uint32_t(destination.mPictureHeight), uint32_t(source.mPictureHeight));
            auto* dest = destination.mData;
            auto* src = source.mData;
            while (height-- != 0) {
                std::memcpy(dest, src, bytes);
                dest += destination.mStride;
                src += source.mStride;
            }
        }
    } else {
        uint32_t sourceBytes = (uint32_t(source.mDepth) + 7) >> 3;
        uint32_t destBytes = (uint32_t(destination.mDepth) + 7) >> 3;
        if (destination.mData == source.mData) {
            uint32_t n = uint32_t(source.mWidth) * sourceBytes;
            destination.mWidth = std::min(uint32_t(destination.mStride), n) / destBytes;
        }
        uint32_t width = std::min(uint32_t(destination.mWidth), uint32_t(source.mWidth));
        uint32_t height = std::min(uint32_t(destination.mPictureHeight), uint32_t(source.mPictureHeight));
        gRaConverter.Init(reinterpret_cast<const TbPixFormat*>(&destination), reinterpret_cast<const TbPixFormat*>(&source), palette, width);
        gRaConverter.ConvertArea(destination.mData, source.mData, destination.mStride, source.mStride, height, width);
    }
    return TbResult{0};
}
