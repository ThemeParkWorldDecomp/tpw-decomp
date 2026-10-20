// Bitmap.cpp (engine library of SimTheme Park 2.04b3; original file name unknown).
// Bitmap::clear (0x10046db4), realloc (0x10046fdc), load (0x100470f0), load_wavelet (0x10047294)
// and the pixel methods colour_fill (0x10046e2c), copy_to (0x10046ee4), swizzle_for_gimex
// (0x10046f60), alphablt (0x10047910), shadowblend (0x10047afc), colourblt (0x10047bb4) and
// descimate (0x10047d24).
//
// The NULL checks after new[] are the original's but dead: the runtime's operator new[]
// throws std::bad_alloc instead of returning NULL (as standard C++ new does).
#include "Bitmap.h"

#include <cstring>

#include "Wavelet.h"
#include "platform/samsutils.h"

void Bitmap::clear() {
    mWidth = 0;
    mHeight = 0;
    mBytesPerPixel = 0;
    if (mData != nullptr) delete[] mData;
    mData = nullptr;
}

int32_t Bitmap::realloc(uint32_t width, uint32_t height, uint32_t bytesPerPixel) {
    if (width != mWidth || height != mHeight || bytesPerPixel != mBytesPerPixel || mData == nullptr) {
        if (mData != nullptr) delete[] mData;
        mData = new uint8_t[bytesPerPixel * width * height];
        if (mData == nullptr) return 0;
        mWidth = width;
        mHeight = height;
        mBytesPerPixel = bytesPerPixel;
    }
    if (mData != nullptr) std::memset(mData, 0, bytesPerPixel * width * height);
    return 1;
}

int32_t Bitmap::load(void* file) {
    uint32_t width, height, bytesPerPixel;
    gFileRead(&width, 4, 1, file);
    SamsUtilities::_USwapBlock32(&width, 1);
    gFileRead(&height, 4, 1, file);
    SamsUtilities::_USwapBlock32(&height, 1);
    gFileRead(&bytesPerPixel, 4, 1, file);
    SamsUtilities::_USwapBlock32(&bytesPerPixel, 1);
    if (width != mWidth || height != mHeight || bytesPerPixel != mBytesPerPixel || mData == nullptr) {
        if (mData != nullptr) delete[] mData;
        mData = new uint8_t[bytesPerPixel * width * height];
        if (mData == nullptr) goto read_pixels;   // the original skips the clear, not the read
        mWidth = width;
        mHeight = height;
        mBytesPerPixel = bytesPerPixel;
    }
    if (mData != nullptr) std::memset(mData, 0, bytesPerPixel * width * height);
read_pixels:
    gFileRead(mData, bytesPerPixel * width * height, 1, file);
    return 1;
}

// One word of the file as a float: the bits are swapped like any other word and not converted.
static float readFloat(void* file) {
    uint32_t bits;
    gFileRead(&bits, 4, 1, file);
    SamsUtilities::_USwapBlock32(&bits, 1);
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

static uint32_t readWord(void* file) {
    uint32_t word;
    gFileRead(&word, 4, 1, file);
    SamsUtilities::_USwapBlock32(&word, 1);
    return word;
}

// The header is Bitmap::load's, but the old size and buffer are cleared first, so the size
// compare below is always true (mData is NULL by then) and the buffer is always reallocated.
// A failed allocation of the pixels skips the clear (not the payload): the dispatch below then
// runs with width and height 0 and a NULL destination.
//
// The wavelet decompressor's slot 0 is called as Decompress(width, height, colour, packed, dest,
// a, b, c). The mono planes pass the engine's float at TOC - 0x7308 (0.0f in the dump, 0x10050168)
// for both b and c. Depth 4 builds a 3-byte plane (colour) and a 1-byte plane (alpha) and
// interleaves them as [alpha, c0, c1, c2] (the three colour bytes in the order the decoder wrote
// them). Allocation failures of the payload buffers answer 0 and free nothing (the colour buffer
// leaks when the alpha buffer fails); the 3-byte and 1-byte planes are not tested.
// The products wrap at 32 bits and the interleave loop's bound is a signed compare.
int32_t Bitmap::load_wavelet(void* file, CWavelet* wavelet) {
    const float k = 0.0f;   // the float at 0x10050168
    uint32_t width, height, depth;
    mWidth = 0;
    mHeight = 0;
    mBytesPerPixel = 0;
    if (mData != nullptr) delete[] mData;
    mData = nullptr;
    width = readWord(file);
    height = readWord(file);
    depth = readWord(file);
    if (width != mWidth || height != mHeight || depth != mBytesPerPixel || mData == nullptr) {
        if (mData != nullptr) delete[] mData;
        mData = new uint8_t[depth * width * height];
        if (mData == nullptr) goto dispatch;
        mWidth = width;
        mHeight = height;
        mBytesPerPixel = depth;
    }
    if (mData != nullptr) std::memset(mData, 0, depth * width * height);
dispatch:
    if (depth == 1) {
        float a = readFloat(file);
        uint32_t size = readWord(file);
        uint8_t* packed = new uint8_t[size];
        if (packed == nullptr) return 0;
        gFileRead(packed, size, 1, file);
        wavelet->Decompress(mWidth, mHeight, false, packed, mData, a, k, k);
        delete[] packed;
    } else if (depth == 3) {
        float a = readFloat(file);
        float b = readFloat(file);
        float c = readFloat(file);
        uint32_t size = readWord(file);
        uint8_t* packed = new uint8_t[size];
        if (packed == nullptr) return 0;
        gFileRead(packed, size, 1, file);
        wavelet->Decompress(mWidth, mHeight, true, packed, mData, a, b, c);
        delete[] packed;
    } else if (depth == 4) {
        float a = readFloat(file);
        float b = readFloat(file);
        float c = readFloat(file);
        float d = readFloat(file);
        uint32_t colourSize = readWord(file);
        uint32_t alphaSize = readWord(file);
        uint8_t* colour = new uint8_t[colourSize];
        if (colour == nullptr) return 0;
        uint8_t* alpha = new uint8_t[alphaSize];
        if (alpha == nullptr) return 0;   // leaks colour
        gFileRead(colour, colourSize, 1, file);
        gFileRead(alpha, alphaSize, 1, file);
        uint8_t* rgb = new uint8_t[mWidth * mHeight * 3];   // not tested for NULL
        wavelet->Decompress(mWidth, mHeight, true, colour, rgb, a, b, c);
        uint8_t* mono = new uint8_t[mWidth * mHeight];      // not tested for NULL
        wavelet->Decompress(mWidth, mHeight, false, alpha, mono, d, k, k);
        uint8_t* out = mData;
        const uint8_t* rgbIn = rgb;
        const uint8_t* monoIn = mono;
        for (int32_t i = 0; i < static_cast<int32_t>(mWidth * mHeight); ++i) {
            out[0] = *monoIn++;
            out[1] = rgbIn[0];
            out[2] = rgbIn[1];
            out[3] = rgbIn[2];
            rgbIn += 3;
            out += 4;
        }
        delete[] mono;
        delete[] rgb;
        delete[] alpha;
        delete[] colour;
    }
    return 1;
}

// ---- the pixel methods ---------------------------------------------------------------------

// The loops compare signed ints (cmpw) and the products wrap at 32 bits (mullw), as here.
static inline int32_t pixelCount(const Bitmap& b) { return static_cast<int32_t>(b.mWidth * b.mHeight); }

void Bitmap::colour_fill(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
    uint8_t* p = mData;
    if (p == nullptr) return;
    if (static_cast<int32_t>(mBytesPerPixel) == 4) {
        for (int32_t i = 0; i < pixelCount(*this); ++i) {
            p[0] = alpha;
            p[1] = red;
            p[2] = green;
            p[3] = blue;
            p += 4;
        }
        return;
    }
    if (static_cast<int32_t>(mBytesPerPixel) != 1) return;
    for (int32_t i = 0; i < pixelCount(*this); ++i) {
        *p = alpha;
        p += 1;
    }
}

void Bitmap::copy_to(Bitmap& dest) {
    if (mData != nullptr) std::memcpy(dest.mData, mData, mBytesPerPixel * (mWidth * mHeight));
}

void Bitmap::swizzle_for_gimex() {
    uint8_t* p = mData;
    for (int32_t i = 0; i < pixelCount(*this); ++i) {
        uint8_t b0 = p[0];
        uint8_t b1 = p[1];
        uint8_t b2 = p[2];
        uint8_t b3 = p[3];
        p[0] = b3;
        p[1] = b2;
        p[2] = b1;
        p[3] = b0;
        p += 4;
    }
}

// d + ((weight * (target - d)) >> 8), the arithmetic shift of a product that is at most 255 *
// 255 in magnitude (srawi), stored as a byte.
static inline uint8_t blendByte(int32_t d, int32_t weight, int32_t target) {
    return static_cast<uint8_t>(d + ((weight * (target - d)) >> 8));
}

void Bitmap::alphablt(Bitmap& dest, uint32_t x, uint32_t y) {
    const uint8_t* src = mData;
    uint8_t* dst = dest.mData + static_cast<int32_t>(dest.mBytesPerPixel * (x + dest.mWidth * y));
    for (int32_t row = 0; row < static_cast<int32_t>(mHeight); ++row) {
        for (int32_t col = 0; col < static_cast<int32_t>(mWidth); ++col) {
            if (static_cast<int32_t>(mBytesPerPixel) == 1) {
                // The one source byte is the weight and the target of all four dest bytes.
                int32_t a = src[0];
                dst[0] = blendByte(dst[0], a, src[0]);
                dst[1] = blendByte(dst[1], a, src[0]);
                dst[2] = blendByte(dst[2], a, src[0]);
                dst[3] = blendByte(dst[3], a, src[0]);
                src += 1;
            } else if (static_cast<int32_t>(mBytesPerPixel) == 2) {
                dst[0] = 0xff;
                dst[1] = src[0];
                dst[2] = src[1];
                dst[3] = 0;
                src += 2;
            } else {
                // Any other depth reads four bytes: the alpha keeps the larger of the two,
                // the three channels blend towards the source by the source's alpha.
                int32_t a = src[0];
                if (dst[0] < a) dst[0] = static_cast<uint8_t>(a);
                dst[1] = blendByte(dst[1], a, src[1]);
                dst[2] = blendByte(dst[2], a, src[2]);
                dst[3] = blendByte(dst[3], a, src[3]);
                src += 4;
            }
            dst += 4;
        }
        // Reproduced as written (not simplified): the skip uses dest's depth, the advance above 4.
        dst += static_cast<int32_t>(dest.mBytesPerPixel * (dest.mWidth - mWidth));
    }
}

void Bitmap::shadowblend(Bitmap& dest) {
    const uint8_t* src = mData;
    uint8_t* dst = dest.mData;
    if (src == nullptr) return;
    for (int32_t row = 0; row < static_cast<int32_t>(mHeight); ++row) {
        for (int32_t col = 0; col < static_cast<int32_t>(mWidth); ++col) {
            if (*dst < *src) *dst = *src;
            src += 1;
            dst += 1;
        }
        dst += static_cast<int32_t>(dest.mBytesPerPixel * (dest.mWidth - mWidth));
    }
}

void Bitmap::colourblt(Bitmap& dest, uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue) {
    const uint8_t* src = mData;
    if (src == nullptr) return;
    uint8_t* dst = dest.mData;
    // The three targets are scaled by the alpha once, before the loops (srawi, then a byte).
    const int32_t a = alpha;
    const uint8_t targetR = static_cast<uint8_t>((a * red) >> 8);
    const uint8_t targetG = static_cast<uint8_t>((a * green) >> 8);
    const uint8_t targetB = static_cast<uint8_t>((a * blue) >> 8);
    for (int32_t row = 0; row < static_cast<int32_t>(mHeight); ++row) {
        for (int32_t col = 0; col < static_cast<int32_t>(mWidth); ++col) {
            const uint8_t weight = static_cast<uint8_t>((*src * a) >> 8);
            src += 1;
            if (dst[0] < weight) dst[0] = weight;
            dst[1] = blendByte(dst[1], weight, targetR);
            dst[2] = blendByte(dst[2], weight, targetG);
            dst[3] = blendByte(dst[3], weight, targetB);
            dst += 4;
        }
        dst += static_cast<int32_t>(dest.mBytesPerPixel * (dest.mWidth - mWidth));
    }
}

int32_t Bitmap::descimate(Bitmap& dest) {
    const uint8_t* src = mData;
    // 16.16 steps, rounded: the vertical one is the signed quotient (divw).
    const int32_t rowStep = static_cast<int32_t>(mHeight * 0x10000u + 0x8000u) / static_cast<int32_t>(dest.mHeight);
    const int32_t destW = static_cast<int32_t>(dest.mWidth);
    uint8_t* out = dest.mData;
    const int32_t colStep = static_cast<int32_t>(mWidth * 0x10000u + 0x8000u) / destW;
    uint8_t* work = new uint8_t[((static_cast<uint32_t>(rowStep) >> 16) + 1) * (dest.mWidth * 4)];
    if (work == nullptr) return 0;
    uint8_t* colCounts = new uint8_t[dest.mWidth];
    uint8_t* rowCounts = new uint8_t[dest.mHeight];
    // A failed table allocation leaves the work buffer (and the other table) leaked.
    if (colCounts == nullptr || rowCounts == nullptr) return 0;

    // How many source columns and rows each destination column and row takes: the difference
    // of the integer parts of the running 16.16 position.
    uint32_t position = 0;
    for (int32_t i = 0; i < static_cast<int32_t>(dest.mWidth); ++i) {
        uint32_t before = position;
        position += static_cast<uint32_t>(colStep);
        colCounts[i] = static_cast<uint8_t>((position >> 16) - (before >> 16));
    }
    position = 0;
    for (int32_t i = 0; i < static_cast<int32_t>(dest.mHeight); ++i) {
        uint32_t before = position;
        position += static_cast<uint32_t>(rowStep);
        rowCounts[i] = static_cast<uint8_t>((position >> 16) - (before >> 16));
    }

    const uint8_t* rowCountPtr = rowCounts;
    for (int32_t dy = 0; dy < static_cast<int32_t>(dest.mHeight); ++dy) {
        const uint32_t rowCount = *rowCountPtr;
        // First pass: each of the rowCount source rows is averaged across its columns into
        // the next work row (the work pointer restarts per destination row only).
        uint8_t* w = work;
        for (uint32_t j = 0; j < rowCount; ++j) {
            const uint8_t* cc = colCounts;
            for (int32_t dx = 0; dx < static_cast<int32_t>(dest.mWidth); ++dx) {
                uint32_t sum0 = 0;
                uint32_t sum1 = 0;
                uint32_t sum2 = 0;
                uint32_t sum3 = 0;
                uint32_t done = 0;
                const uint16_t count = *cc;
                if (count != 0) {
                    if (count > 8) {
                        // Eight pixels per turn, (count - 1) / 8 turns.
                        for (uint32_t turns = (count - 8u + 7u) >> 3; turns != 0; --turns) {
                            for (int k = 0; k < 8; ++k) {
                                sum0 += src[4 * k + 0];
                                sum1 += src[4 * k + 1];
                                sum2 += src[4 * k + 2];
                                sum3 += src[4 * k + 3];
                            }
                            done += 8;
                            src += 32;
                        }
                    }
                    if (done < count) {
                        for (uint32_t left = count - done; left != 0; --left) {
                            sum0 += src[0];
                            sum1 += src[1];
                            sum2 += src[2];
                            sum3 += src[3];
                            src += 4;
                        }
                    }
                }
                // count 0 divides by zero (divwu: undefined on the PowerPC).
                w[0] = static_cast<uint8_t>(sum0 / count);
                w[1] = static_cast<uint8_t>(sum1 / count);
                w[2] = static_cast<uint8_t>(sum2 / count);
                w[3] = static_cast<uint8_t>(sum3 / count);
                w += 4;
                ++cc;
            }
        }
        // Second pass: each destination column averages its rowCount work rows (sum masked to
        // 16 bits first, as the original does).
        int32_t offset = 0;
        for (int32_t dx = 0; dx < static_cast<int32_t>(dest.mWidth); ++dx) {
            uint32_t sum0 = 0;
            uint32_t sum1 = 0;
            uint32_t sum2 = 0;
            uint32_t sum3 = 0;
            uint32_t done = 0;
            const uint8_t* p = work + offset;
            const int32_t stride = static_cast<int32_t>(dest.mWidth) * 4;
            if (rowCount != 0) {
                if (rowCount > 8) {
                    for (uint32_t turns = (rowCount - 8u + 7u) >> 3; turns != 0; --turns) {
                        for (int k = 0; k < 8; ++k) {
                            sum0 += p[0];
                            sum1 += p[1];
                            sum2 += p[2];
                            sum3 += p[3];
                            p += stride;
                        }
                        done += 8;
                    }
                }
                if (done < rowCount) {
                    for (uint32_t left = rowCount - done; left != 0; --left) {
                        sum0 += p[0];
                        sum1 += p[1];
                        sum2 += p[2];
                        sum3 += p[3];
                        p += stride;
                    }
                }
            }
            offset += 4;
            // rowCount 0 divides by zero (divwu: undefined on the PowerPC).
            out[0] = static_cast<uint8_t>((sum0 & 0xffff) / rowCount);
            out[1] = static_cast<uint8_t>((sum1 & 0xffff) / rowCount);
            out[2] = static_cast<uint8_t>((sum2 & 0xffff) / rowCount);
            out[3] = static_cast<uint8_t>((sum3 & 0xffff) / rowCount);
            out += 4;
        }
        ++rowCountPtr;
    }
    delete[] rowCounts;
    delete[] colCounts;
    delete[] work;
    return 1;
}
