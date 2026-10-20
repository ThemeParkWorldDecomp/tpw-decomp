// Wavelet.cpp, part 2 (engine library of SimTheme Park 2.04b3): the version 0x13 wavelet
// texture decoder and what it uses. LzssDecompress 0x10039210, read_code 0x10039360,
// read_reset 0x10039424, the three dispatching CWaveletImp::Decompress overloads 0x10039694,
// 0x10039718 and 0x100397dc, ReverseTransformVert 0x1003d054, Decompress_v13 0x1003d230, and
// CIgArray<float, float>'s members (GetData 0x1003d1f8, constructor 0x1003ee44, destructor
// 0x1003ee88, SetSize 0x1003ef48). Kept apart from src/Wavelet.cpp (the callbacks and the
// decompressor's lifetime) so a test library can link one without the other.
//
// A version 0x13 file holds two LZSS-packed blocks of wavelet coefficients: the colour
// (brightness at full size, then the two colour differences at half size) and, when the
// header says so, the alpha. Each image is one level of the Daubechies D4 transform: every
// row of the stream is (smooth, detail) pairs; the horizontal pass rebuilds the rows into
// gWaveletRows and the vertical pass the columns into the image's plane. The colour is then
// converted to 16-bit RGBA 4444 pixels (two by two, one colour difference per square).
#include "Wavelet.h"

#include <cmath>
#include <cstring>
#include <new>
#include <type_traits>

#include "IgArray.h"
#include "ppc_math.h"
#include "tb_resource_texture.h"

// ---- The decoder's statics ------------------------------------------------------------------
float gWaveletPlanes[5][kWaveletPlaneFloats];

// __sinit_Wavelet_cpp (0x1003f1f0): each one in double precision, rounded once.
float gWaveletH0 = static_cast<float>((1.0 + std::sqrt(3.0)) * 0.25 / std::sqrt(2.0));
float gWaveletH1 = static_cast<float>((3.0 + std::sqrt(3.0)) * 0.25 / std::sqrt(2.0));
float gWaveletH2 = static_cast<float>((3.0 - std::sqrt(3.0)) * 0.25 / std::sqrt(2.0));
float gWaveletH3 = static_cast<float>((1.0 - std::sqrt(3.0)) * 0.25 / std::sqrt(2.0));

uint32_t gLzssBitCount;
uint32_t gLzssBitBuffer;
uint32_t gLzssUnused;
uint32_t gLzssSourcePos;
uint8_t* gLzssDest;
const uint8_t* gLzssSource;

// ---- CIgArray -----------------------------------------------------------------------------
namespace {

// The byte count of `count` elements as the Mac computes it (a 32-bit shift).
template <class TYPE>
uint32_t ByteCount(int32_t count) {
    return static_cast<uint32_t>(count) * static_cast<uint32_t>(sizeof(TYPE));
}

}  // namespace

template <class TYPE, class ARG_TYPE>
CIgArray<TYPE, ARG_TYPE>::CIgArray() : mData(nullptr), mSize(0), mCapacity(0), mGrowBy(0) {}

template <class TYPE, class ARG_TYPE>
CIgArray<TYPE, ARG_TYPE>::~CIgArray() {
    if (mData != nullptr) ::operator delete[](mData);
}

template <class TYPE, class ARG_TYPE>
void CIgArray<TYPE, ARG_TYPE>::SetSize(int32_t size, int32_t growBy) {
    static_assert(std::is_trivially_copyable<TYPE>::value, "the elements are set with memset and memcpy");
    if (growBy != -1) mGrowBy = growBy;
    if (size == 0) {
        if (mData != nullptr) {
            ::operator delete[](mData);
            mData = nullptr;
        }
        mCapacity = 0;
        mSize = 0;
    } else if (mData == nullptr) {
        mData = static_cast<TYPE*>(::operator new[](ByteCount<TYPE>(size)));
        std::memset(mData, 0, ByteCount<TYPE>(size));
        mCapacity = size;
        mSize = size;
    } else if (size <= mCapacity) {
        if (size > mSize) std::memset(mData + mSize, 0, ByteCount<TYPE>(size - mSize));
        mSize = size;
    } else {
        int32_t step = mGrowBy;
        if (step == 0) {
            step = mSize / 8;
            if (step < 4) {
                step = 4;
            } else if (step > 0x400) {
                step = 0x400;
            }
        }
        const int32_t capacity = size < mCapacity + step ? mCapacity + step : size;
        TYPE* data = static_cast<TYPE*>(::operator new[](ByteCount<TYPE>(capacity)));
        std::memcpy(data, mData, ByteCount<TYPE>(mSize));
        std::memset(data + mSize, 0, ByteCount<TYPE>(size - mSize));
        ::operator delete[](mData);
        mData = data;
        mSize = size;
        mCapacity = capacity;
    }
}

template <class TYPE, class ARG_TYPE>
TYPE* CIgArray<TYPE, ARG_TYPE>::GetData() {
    return mData;
}

template class CIgArray<float, float>;

// ---- LZSS ---------------------------------------------------------------------------------
void read_reset() {
    gLzssBitBuffer = 0;
    gLzssBitCount = 0;
}

uint32_t read_code(uint32_t bits) {
    if (gLzssBitCount < bits) {
        // Two more bytes, low byte first, above the bits that are left.
        const uint8_t* source = gLzssSource;
        gLzssBitBuffer |= ppc_slw(source[gLzssSourcePos++], static_cast<int32_t>(gLzssBitCount));
        gLzssBitBuffer |= ppc_slw(source[gLzssSourcePos++], static_cast<int32_t>(gLzssBitCount + 8));
        gLzssBitCount += 16;
    }
    const uint32_t buffer = gLzssBitBuffer;
    gLzssBitBuffer = ppc_srw(buffer, static_cast<int32_t>(bits));
    const uint32_t code = buffer & (ppc_slw(1, static_cast<int32_t>(bits)) - 1);
    gLzssBitCount -= bits;
    return code;
}

void LzssDecompress(void* source, void* dest) {
    gLzssSource = static_cast<const uint8_t*>(source);
    gLzssDest = static_cast<uint8_t*>(dest);
    gLzssSourcePos = 0;
    gLzssUnused = 0;
    read_reset();
    uint8_t* out = gLzssDest;
    for (;;) {
        if (read_code(1) == 0) {
            *out++ = static_cast<uint8_t>(read_code(8));
            continue;
        }
        const uint32_t offset = read_code(12);
        const uint32_t length = read_code(7) + 1;
        if (offset == 0) return;
        // A byte at a time, so a copy may overlap its own output.
        const uint8_t* from = out - offset;
        for (uint32_t i = 0; i < length; ++i) *out++ = *from++;
    }
}

// ---- The inverse transform ----------------------------------------------------------------
void CWaveletImp::ReverseTransformVert(const float* source, float* dest, int32_t count, int32_t stride) {
    const int32_t half = count >> 1;
    const int32_t detail = half * stride;
    const int32_t last = (half - 1) * stride;
    float* out = dest + 2;
    int32_t k = 0;
    for (; k < last; k += stride) {
        float even = gWaveletH0 * source[k + stride];
        even = ppc_fmadds(gWaveletH2, source[k], even);
        even = ppc_fmadds(gWaveletH1, source[k + detail], even);
        even = ppc_fmadds(gWaveletH3, source[detail + k + stride], even);
        out[0] = even;
        float odd = gWaveletH1 * source[k + stride];
        odd = ppc_fmadds(gWaveletH3, source[k], odd);
        odd = ppc_fnmsubs(gWaveletH0, source[k + detail], odd);
        odd = ppc_fnmsubs(gWaveletH2, source[detail + k + stride], odd);
        out[1] = odd;
        out += 2;
    }
    // The first pair wraps round to the column's last coefficients.
    float even = gWaveletH0 * source[0];
    even = ppc_fmadds(gWaveletH2, source[k], even);
    even = ppc_fmadds(gWaveletH3, source[detail], even);
    even = ppc_fmadds(gWaveletH1, source[k + detail], even);
    dest[0] = even;
    float odd = gWaveletH1 * source[0];
    odd = ppc_fmadds(gWaveletH3, source[k], odd);
    odd = ppc_fnmsubs(gWaveletH0, source[k + detail], odd);
    odd = ppc_fnmsubs(gWaveletH2, source[detail], odd);
    dest[1] = odd;
}

namespace {

// One coefficient of the unpacked stream: a signed byte, or the escape byte -128 followed by
// a little-endian 16-bit value.
inline float ReadCoefficient(const uint8_t*& stream) {
    int32_t value = static_cast<int8_t>(*stream++);
    if (value == -0x80) {
        value = stream[0] | stream[1] << 8;
        stream += 2;
    }
    return static_cast<float>(static_cast<int16_t>(value));
}

// The horizontal pass: `rows` rows of `pairs` (smooth, detail) pairs each from the stream into
// gWaveletRows, `stride` floats apart, through the D4 coefficients times `scale`. Returns where
// the stream stops.
const uint8_t* ReverseRows(const uint8_t* stream, uint32_t rows, uint32_t pairs, uint32_t stride, float scale) {
    const float h1 = gWaveletH1 * scale;
    const float h0 = gWaveletH0 * scale;
    const float h2 = gWaveletH2 * scale;
    const float h3 = gWaveletH3 * scale;
    const int32_t inner = static_cast<int32_t>(pairs - 1);
    float* row = gWaveletPlanes[kWaveletRows];
    for (uint32_t y = 0; y < rows; ++y) {
        const float firstSmooth = ReadCoefficient(stream);
        const float firstDetail = ReadCoefficient(stream);
        float smooth = firstSmooth;
        float detail = firstDetail;
        float* out = row + 2;
        for (int32_t k = inner; k > 0; --k) {
            const float previous = smooth;
            smooth = ReadCoefficient(stream);
            const float next = ReadCoefficient(stream);
            float even = h0 * smooth;
            float odd = h1 * smooth;
            even = ppc_fmadds(h2, previous, even);
            odd = ppc_fmadds(h3, previous, odd);
            even = ppc_fmadds(h1, detail, even);
            odd = ppc_fnmsubs(h0, detail, odd);
            detail = next;
            even = ppc_fmadds(h3, detail, even);
            odd = ppc_fnmsubs(h2, detail, odd);
            out[0] = even;
            out[1] = odd;
            out += 2;
        }
        // The first pair wraps round to the row's last pair (with the terms in another order
        // than the column's).
        float even = h0 * firstSmooth;
        float odd = h1 * firstSmooth;
        even = ppc_fmadds(h2, smooth, even);
        odd = ppc_fmadds(h3, smooth, odd);
        even = ppc_fmadds(h3, firstDetail, even);
        odd = ppc_fnmsubs(h0, detail, odd);
        even = ppc_fmadds(h1, detail, even);
        odd = ppc_fnmsubs(h2, firstDetail, odd);
        row[0] = even;
        row[1] = odd;
        row += stride;
    }
    return stream;
}

// The vertical pass over an n x n block of gWaveletRows into `out`: eight columns at a time
// into a work array (a column to a row of it), copied back transposed. With n = 4 (an 8 x 8
// image's colour differences) the eight columns run into the next rows and past the block,
// as in the original.
void ReverseColumns(CWaveletImp* wavelet, float* out, uint32_t n) {
    CIgArray<float, float> work;
    work.SetSize(static_cast<int32_t>(n << 3), -1);
    const float* rows = gWaveletPlanes[kWaveletRows];
    for (uint32_t column = 0; column < n; column += 8) {
        for (int32_t k = 0; k < 8; ++k) wavelet->ReverseTransformVert(rows + column + k, work.GetData() + k * n, n, n);
        const float* data = work.GetData();
        float* to = out + column;
        for (uint32_t i = 0; i < n; ++i) {
            for (uint32_t j = 0; j < 8; ++j) to[j] = data[j * n + i];
            to += n;
        }
    }
}

// The side of the square the transform works on for one dimension: the next power of two,
// at least 8. A zero dimension gives 2^32, which the shift turns into 0.
uint32_t TransformSize(int32_t length) {
    uint32_t bits = 1;
    for (uint32_t rest = static_cast<uint32_t>(length - 1) >> 1; rest != 0; rest >>= 1) ++bits;
    if (bits < 3) bits = 3;
    return ppc_slw(1, static_cast<int32_t>(bits));
}

// A truncated colour component as the listing limits it: its low 16 bits as a signed short,
// clamped to 0..255.
inline int32_t Clamp(int32_t value) {
    const int16_t low = static_cast<int16_t>(value);
    if (low < 0) return 0;
    if (low > 0xff) return 0xff;
    return low;
}

// The colour part of a 4444 pixel: red in the top four bits, then green and blue.
inline uint32_t PackColour(int32_t red, int32_t green, int32_t blue) {
    return static_cast<uint32_t>(Clamp(red) & 0xf0) << 8 | static_cast<uint32_t>(Clamp(green) & 0xf0) << 4 |
           static_cast<uint32_t>(Clamp(blue) & 0xf0);
}

// YCbCr to RGB (engine constants 0x10050090..0x1005009c).
constexpr float kRedCr = 1.402f;
constexpr float kGreenCb = 0.344136f;
constexpr float kGreenCr = 0.714136f;
constexpr float kBlueCb = 1.772f;

}  // namespace

// The arguments are the image's size, the destination's bytes per pixel, the two packed
// blocks, the destination (width x height pixels, no padding) and the four scales of the
// brightness, the blue and red differences and the alpha. Nothing is written for 4 bytes per
// pixel; anything else gets 16-bit RGBA 4444. The pixels are written two by two, so an odd
// width or height writes one column or row past the image. The original returns whatever is
// left in r3 (nobody reads it); this returns 0.
int32_t CWaveletImp::Decompress_v13(int32_t width, int32_t height, int32_t bytesPerPixel, void* data, void* alpha,
                                    void* dest, float lumaScale, float cbScale, float crScale, float alphaScale) {
    const uint32_t sizeX = TransformSize(width);
    const uint32_t sizeY = TransformSize(height);
    const uint32_t size = sizeX > sizeY ? sizeX : sizeY;
    const uint32_t half = size >> 1;

    LzssDecompress(data, gWaveletWorkspace);
    const uint8_t* stream = gWaveletWorkspace;
    stream = ReverseRows(stream, size, half, size, lumaScale);
    ReverseColumns(this, gWaveletPlanes[kWaveletLuma], size);
    stream = ReverseRows(stream, half, half >> 1, half, cbScale);
    ReverseColumns(this, gWaveletPlanes[kWaveletCb], half);
    ReverseRows(stream, half, half >> 1, half, crScale);
    ReverseColumns(this, gWaveletPlanes[kWaveletCr], half);
    if (alpha != nullptr) {
        LzssDecompress(alpha, gWaveletWorkspace);
        ReverseRows(gWaveletWorkspace, size, half, size, alphaScale);
        ReverseColumns(this, gWaveletPlanes[kWaveletAlpha], size);
    }
    if (bytesPerPixel == 4) return 0;

    const float* luma = gWaveletPlanes[kWaveletLuma];
    const float* alphas = gWaveletPlanes[kWaveletAlpha];
    uint16_t* pixels = static_cast<uint16_t*>(dest);
    if (alpha != nullptr) {
        gei_TextureSettings.GetAlphaMask();   // called, and the result not used
        int32_t chromaRow = 0;
        int32_t first = 0;
        for (int32_t y = 0; y < height; y += 2) {
            uint32_t at = y * size;
            int32_t index = first;
            const float* cb = gWaveletPlanes[kWaveletCb] + chromaRow * half;
            const float* cr = gWaveletPlanes[kWaveletCr] + chromaRow * half;
            if (width > 0) {
                for (uint32_t pairs = (static_cast<uint32_t>(width) + 1) >> 1; pairs != 0; --pairs) {
                    const float u = *cb;
                    const float v = *cr;
                    const float blue = kBlueCb * u;
                    const float red = kRedCr * v;
                    const float green = ppc_fnmsubs(kGreenCr, v, ppc_fneg(kGreenCb * u));
                    // The square's four pixels: this row, the next, then the row below.
                    const uint32_t square[4] = {at, at + 1, at + size, at + size + 1};
                    const int32_t target[4] = {index, index + 1, index + width, index + width + 1};
                    for (int32_t p = 0; p < 4; ++p) {
                        const float y0 = luma[square[p]];
                        const int32_t b = ppc_fctiwz(y0 + blue);
                        const int32_t a = ppc_fctiwz(alphas[square[p]]);
                        const int32_t g = ppc_fctiwz(y0 + green);
                        const int32_t r = ppc_fctiwz(y0 + red);
                        pixels[target[p]] = static_cast<uint16_t>(PackColour(r, g, b) | (Clamp(a) >> 4 & 0xf));
                    }
                    at += 2;
                    index += 2;
                    ++cb;
                    ++cr;
                }
            }
            first += width * 2;
            ++chromaRow;
        }
    } else {
        const uint32_t mask = gei_TextureSettings.GetAlphaMask();
        int32_t chromaRow = 0;
        int32_t first = 0;
        for (int32_t y = 0; y < height; y += 2) {
            uint32_t at = y * size;
            int32_t index = first;
            const float* cb = gWaveletPlanes[kWaveletCb] + chromaRow * half;
            const float* cr = gWaveletPlanes[kWaveletCr] + chromaRow * half;
            if (width > 0) {
                for (uint32_t pairs = (static_cast<uint32_t>(width) + 1) >> 1; pairs != 0; --pairs) {
                    const float u = *cb;
                    const float v = *cr;
                    const float blue = kBlueCb * u;
                    const float red = kRedCr * v;
                    const float green = ppc_fnmsubs(kGreenCr, v, ppc_fneg(kGreenCb * u));
                    const uint32_t square[4] = {at, at + 1, at + size, at + size + 1};
                    const int32_t target[4] = {index, index + 1, index + width, index + width + 1};
                    for (int32_t p = 0; p < 4; ++p) {
                        const float y0 = luma[square[p]];
                        const int32_t b = ppc_fctiwz(y0 + blue);
                        const int32_t r = ppc_fctiwz(y0 + red);
                        const int32_t g = ppc_fctiwz(y0 + green);
                        pixels[target[p]] = static_cast<uint16_t>(mask | PackColour(r, g, b));
                    }
                    at += 2;
                    index += 2;
                    ++cb;
                    ++cr;
                }
            }
            first += width * 2;
            ++chromaRow;
        }
    }
    return 0;
}

// ---- The fast path's dispatch ---------------------------------------------------------------
// A colour image goes to DecompressColorFast (a, b, c) and a mono one to DecompressMonoFast (a
// only). The original is void and leaves the callee's r3 in place; this answers 0.
int32_t CWaveletImp::Decompress(uint32_t width, uint32_t height, bool colour, const uint8_t* source, uint8_t* dest,
                                float a, float b, float c) {
    if (colour) {
        DecompressColorFast(width, height, source, dest, a, b, c);
    } else {
        DecompressMonoFast(width, height, source, dest, a);
    }
    return 0;
}

// ---- The version dispatch -------------------------------------------------------------------
// Version 0x13 goes to Decompress_v13 and 0x12 to Decompress_v12 (vtable slots 0x28 and 0x24);
// any other version does nothing. The original returns the decoder's r3, or `this` when it
// calls none; this returns the decoder's result, or 0.
int32_t CWaveletImp::Decompress(int32_t version, int32_t width, int32_t height, int32_t bytesPerPixel, void* data,
                                void* alpha, void* dest, float lumaScale, float cbScale, float crScale,
                                float alphaScale) {
    if (version == 0x13) {
        return Decompress_v13(width, height, bytesPerPixel, data, alpha, dest, lumaScale, cbScale, crScale,
                              alphaScale);
    }
    if (version > 0x13) return 0;
    if (version >= 0x12) {
        return Decompress_v12(width, height, bytesPerPixel, data, alpha, dest, lumaScale, cbScale, crScale,
                              alphaScale);
    }
    return 0;
}

// The Blit entry: decodes only when the version is above 0x12 and the destination is the
// image's own size with a row length (CWaveLoad::Blit passes it in bytes) equal to its width
// in pixels, so a 2-byte destination is never decoded here; the row length itself is not
// used.
int32_t CWaveletImp::Decompress(int32_t version, int32_t width, int32_t height, int32_t destWidth, int32_t destHeight,
                                int32_t pitch, int32_t bytesPerPixel, void* data, void* alpha, void* dest,
                                float lumaScale, float cbScale, float crScale, float alphaScale) {
    if (version > 0x12 && width == destWidth && height == destHeight && destWidth == pitch) {
        return Decompress_v13(width, height, bytesPerPixel, data, alpha, dest, lumaScale, cbScale, crScale,
                              alphaScale);
    }
    return 0;
}
