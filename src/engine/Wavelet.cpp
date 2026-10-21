// Wavelet.cpp: name proven by __sinit_Wavelet_cpp in the engine library.

#include "IgArray.h"
#include "platform/interfacelib.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "tb_rendermemory.h"
#include "tb_resource_texture.h"
#include "Wavelet.h"
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <new>
#include <type_traits>

// Wavelet.cpp (engine library of SimTheme Park 2.04b3).
// PARTIAL: the callbacks so far: SetFileCallbacks (0x10038964), translated with
// landscape.cpp, whose loaders read through them; DefaultMalloc (0x100388c0), DefaultFree
// (0x10038914) and SetEngineCallbacks (0x100389f8), translated with tb_capture.cpp;
// BitsSet, ShiftFactor and CreateSineTable (0x10038a78..0x10038bf8), translated with
// engine start-up. The rest of the file is still to do.

// ---- The wavelet decompressor's lifetime --------------------------------------------------
CWavelet* gWaveletInstance = nullptr;
uint8_t* gWaveletWorkspace = nullptr;

CWavelet::~CWavelet() {}

CWaveletImp::CWaveletImp() {
    mReferences = 1;
    mBuffer = nullptr;
    if (gWaveletWorkspace == nullptr) gWaveletWorkspace = gei_RenderMemory->LoadInfo(CRenderMemory::kDynamic)->mData;
    mBuffer = new uint8_t[kBufferSize];
}

CWaveletImp::~CWaveletImp() {
    if (mBuffer != nullptr) delete[] mBuffer;
}

int32_t CWaveletImp::AddRef() { return ++mReferences; }

int32_t CWaveletImp::GetRefCount() { return mReferences; }

int32_t CWaveletImp::Release() {
    const int32_t references = --mReferences;
    if (references < 1) delete this;
    return references;
}

// Wavelet.cpp, engine: DecompressColorFast 0x10039894 (3492), DecompressMonoFast
// 0x1003a67c (1168), ReverseChannelFast 0x1003ab50 (956), ConvertAndBlend 0x1003af4c (332).
// The fast format has a smooth half followed by a detail half in each coefficient row.

namespace {

uint32_t FastSize(uint32_t length) {
    uint32_t bits = 1;
    for (uint32_t rest = (length - 1) >> 1; rest != 0; rest >>= 1) ++bits;
    if (bits < 3) bits = 3;
    return ppc_slw(1, bits);
}

// One pair of the D4 inverse, with the wrap's different order of even terms.
void FastPair(float smooth, float nextSmooth, float detail, float nextDetail, float* out, bool wrap) {
    float even = gWaveletH0 * nextSmooth;
    even = ppc_fmadds(gWaveletH2, smooth, even);
    if (wrap) {
        even = ppc_fmadds(gWaveletH3, nextDetail, even);
        even = ppc_fmadds(gWaveletH1, detail, even);
    } else {
        even = ppc_fmadds(gWaveletH1, detail, even);
        even = ppc_fmadds(gWaveletH3, nextDetail, even);
    }
    out[0] = even;
    float odd = gWaveletH1 * nextSmooth;
    odd = ppc_fmadds(gWaveletH3, smooth, odd);
    odd = ppc_fnmsubs(gWaveletH0, detail, odd);
    odd = ppc_fnmsubs(gWaveletH2, nextDetail, odd);
    out[1] = odd;
}

const uint8_t* FastRows(const uint8_t* stream, float* row, uint32_t size, float scale) {
    const uint32_t half = size >> 1;
    float* out = gWaveletPlanes[kWaveletRows];
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            int32_t value = static_cast<int8_t>(*stream++);
            if (value == -128) {
                value = static_cast<int16_t>(stream[0] | stream[1] << 8);
                stream += 2;
            }
            row[x] = scale * static_cast<float>(value);
        }
        for (uint32_t x = 0; x + 1 < half; ++x) {
            FastPair(row[x], row[x + 1], row[half + x], row[half + x + 1], out + 2 * (x + 1), false);
        }
        FastPair(row[half - 1], row[0], row[size - 1], row[half], out, true);
        out += size;
    }
    return stream;
}

// Bottom to top and right to left, so the compact source survives the in-place doubling.
void FastUpsample(float* plane, uint32_t size) {
    const uint32_t half = size >> 1;
    for (int32_t y = static_cast<int32_t>(size - 1); y >= 0; --y) {
        for (int32_t x = static_cast<int32_t>(size - 1); x >= 0; --x) {
            plane[y * size + x] = plane[(y >> 1) * half + (x >> 1)];
        }
    }
}

uint8_t FastClamp(float value, int32_t limit) {
    const int32_t low = static_cast<int16_t>(ppc_fctiwz(value));
    return static_cast<uint8_t>(low < 0 ? 0 : (low > limit ? limit : low));
}

}  // namespace

void CWaveletImp::DecompressColorFast(uint32_t width, uint32_t height, const uint8_t* source, uint8_t* dest,
                                     float scaleY, float scaleCb, float scaleCr) {
    const uint32_t sx = FastSize(width);
    const uint32_t sy = FastSize(height);
    const uint32_t size = sx > sy ? sx : sy;
    const uint32_t half = size >> 1;
    for (int32_t plane : {kWaveletLuma, kWaveletCb, kWaveletCr, kWaveletRows}) {
        if (SamsUtilities::gMemory == nullptr) SamsUtilities::gMemory = new SamsUtilities::UAllocator;
        SamsUtilities::gMemory->Clear(gWaveletPlanes[plane], 0x40000);
    }
    LbZLib_MemDecompress(const_cast<uint8_t*>(source), gWaveletWorkspace, mBuffer);
    CIgArray<float, float> row;
    row.SetSize(static_cast<int32_t>(size), -1);
    const uint8_t* stream = FastRows(gWaveletWorkspace, row.GetData(), size, scaleY);
    ReverseChannelFast(gWaveletPlanes[kWaveletRows], gWaveletPlanes[kWaveletLuma], size, size);
    stream = FastRows(stream, row.GetData(), half, scaleCb);
    ReverseChannelFast(gWaveletPlanes[kWaveletRows], gWaveletPlanes[kWaveletCb], half, half);
    FastRows(stream, row.GetData(), half, scaleCr);
    ReverseChannelFast(gWaveletPlanes[kWaveletRows], gWaveletPlanes[kWaveletCr], half, half);
    FastUpsample(gWaveletPlanes[kWaveletCb], size);
    FastUpsample(gWaveletPlanes[kWaveletCr], size);
    ConvertAndBlend(width, height, size, dest);
}

void CWaveletImp::DecompressMonoFast(uint32_t width, uint32_t height, const uint8_t* source, uint8_t* dest,
                                    float scale) {
    const uint32_t sx = FastSize(width);
    const uint32_t sy = FastSize(height);
    const uint32_t size = sx > sy ? sx : sy;
    LbZLib_MemDecompress(const_cast<uint8_t*>(source), gWaveletWorkspace, mBuffer);
    CIgArray<float, float> row;
    row.SetSize(static_cast<int32_t>(size), -1);
    FastRows(gWaveletWorkspace, row.GetData(), size, scale);
    ReverseChannelFast(gWaveletPlanes[kWaveletRows], gWaveletPlanes[kWaveletLuma], size, size);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const float value = gWaveletPlanes[kWaveletLuma][y * size + x];
            *dest++ = !(value >= 0.0f) ? 0 : (!(value <= 255.0f) ? 255 : static_cast<uint8_t>(ppc_fctiwz(value)));
        }
    }
}

void CWaveletImp::ReverseChannelFast(float* source, float* dest, uint32_t stride, uint32_t count) {
    CIgArray<float, float> work;
    work.SetSize(static_cast<int32_t>(count << 3), -1);
    const int32_t half = static_cast<int32_t>(count) >> 1;
    const int32_t detail = half * stride;
    const int32_t last = (half - 1) * stride;
    for (uint32_t column = 0; column < count; column += 8) {
        for (uint32_t c = 0; c < 8; ++c) {
            const float* from = source + column + c;
            float* out = work.GetData() + c * count;
            int32_t k = 0;
            for (; k < last; k += stride) {
                FastPair(from[k], from[k + stride], from[k + detail], from[k + detail + stride],
                         out + 2 + 2 * (k / stride), false);
            }
            FastPair(from[k], from[0], from[k + detail], from[detail], out, true);
        }
        for (uint32_t y = 0; y < count; ++y) {
            for (uint32_t c = 0; c < 8; ++c) dest[y * stride + column + c] = work.GetData()[c * count + y];
        }
    }
}

// Three bytes per pixel: blue, green, red. The red ceiling really is 0xad.
// The original's leftover r3 is not a return value used by the caller.
void CWaveletImp::ConvertAndBlend(uint32_t width, uint32_t height, uint32_t stride, uint8_t* dest) {
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const uint32_t at = y * stride + x;
            const float luma = gWaveletPlanes[kWaveletLuma][at];
            const float cb = gWaveletPlanes[kWaveletCb][at];
            const float cr = gWaveletPlanes[kWaveletCr][at];
            *dest++ = FastClamp(ppc_fmadds(1.772f, cb, luma), 255);
            float green = ppc_fnmsubs(0.344136f, cb, luma);
            green = ppc_fnmsubs(0.714136f, cr, green);
            *dest++ = FastClamp(green, 255);
            *dest++ = FastClamp(ppc_fmadds(1.402f, cr, luma), 173);
        }
    }
}

// Wavelet.cpp, part 3 (engine library of SimTheme Park 2.04b3): the version 0x12 wavelet
// texture decoder, CWaveletImp::Decompress_v12 0x1003b0d4. Kept apart from
// src/Wavelet_decode.cpp (version 0x13) so that unit's test library can go on faking it.
//
// A version 0x12 file holds the same images as a version 0x13 one (the brightness at the
// transform's size, the blue and red differences at half size, the optional alpha at full
// size, one level of the Daubechies D4 transform each, rows first), with four differences:
// - each block is zlib-packed behind a 0x1c-byte "BILZ" header and unpacked into
//   gWaveletWorkspace by the bullfrog library's LbZLib_MemDecompress, with the decompressor's
//   own mBuffer as zlib's memory (not LzssDecompress: the call goes through cross-library
//   glue, which the call graph does not show);
// - every row of the stream is all its smooth coefficients, then all its details (version
//   0x13 interleaves them), and each coefficient is multiplied by the plane's scale as it is
//   read (version 0x13 folds the scale into the D4 coefficients instead);
// - the pixels go out upside down: image row y lands on destination row height - 1 - y
//   (CWaveLoad::IsNaturallyFlipped answers true for version 0x12);
// - without an alpha block the alpha nibble is GetAlphaMask() >> 4, which is 0 in 2.04b3
//   (version 0x13 ORs the mask itself in).

namespace {

// One coefficient of the unpacked stream: a signed byte, or the escape byte -128 followed by
// a little-endian 16-bit value.
inline int32_t ReadCoefficientV12(const uint8_t*& stream) {
    int32_t value = static_cast<int8_t>(*stream++);
    if (value == -0x80) {
        value = static_cast<int16_t>(stream[0] | stream[1] << 8);
        stream += 2;
    }
    return value;
}

// The horizontal pass of one image: `rows` rows of `count` coefficients, each times `scale`,
// into the row array, and each row inverse-transformed into gWaveletRows, `stride` floats
// apart. The original inlines the row transform: the same operations in the same order as
// ReverseTransformVert with a stride of 1 (the smooth half first), which this calls. Returns
// where the stream stops.
const uint8_t* RowPassV12(CWaveletImp* wavelet, const uint8_t* stream, CIgArray<float, float>& row, uint32_t rows,
                          int32_t count, uint32_t stride, float scale) {
    float* out = gWaveletPlanes[kWaveletRows];
    for (uint32_t y = 0; y < rows; ++y) {
        float* coefficients = row.mData;
        for (int32_t i = 0; i < count; ++i) coefficients[i] = scale * static_cast<float>(ReadCoefficientV12(stream));
        wavelet->ReverseTransformVert(row.mData, out, count, 1);
        out += stride;
    }
    return stream;
}

// The vertical pass over an n x n block of gWaveletRows into `out`: eight columns at a time
// into a work array of 8n floats (a column to a row of it), copied back transposed; the work
// array lives for this one image. As in version 0x13, n = 4 (an 8 x 8 image's colour
// differences) still takes eight columns, so it reads into the next rows and writes past the
// 4 x 4 block.
void ColumnPassV12(CWaveletImp* wavelet, float* out, uint32_t n) {
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
uint32_t TransformSizeV12(int32_t length) {
    uint32_t bits = 1;
    for (uint32_t rest = static_cast<uint32_t>(length - 1) >> 1; rest != 0; rest >>= 1) ++bits;
    if (bits < 3) bits = 3;
    return ppc_slw(1, static_cast<int32_t>(bits));
}

// A truncated colour component as the listing limits it: its low 16 bits as a signed short,
// clamped to 0..255.
inline int32_t ClampV12(int32_t value) {
    const int16_t low = static_cast<int16_t>(value);
    if (low < 0) return 0;
    if (low > 0xff) return 0xff;
    return low;
}

// The colour part of a 4444 pixel: red in the top four bits, then green and blue.
inline uint32_t PackColourV12(int32_t red, int32_t green, int32_t blue) {
    return static_cast<uint32_t>(ClampV12(red) & 0xf0) << 8 | static_cast<uint32_t>(ClampV12(green) & 0xf0) << 4 |
           static_cast<uint32_t>(ClampV12(blue) & 0xf0);
}

// YCbCr to RGB (engine constants 0x10050090..0x1005009c, shared with version 0x13).
constexpr float kRedCr = 1.402f;
constexpr float kGreenCb = 0.344136f;
constexpr float kGreenCr = 0.714136f;
constexpr float kBlueCb = 1.772f;

}  // namespace

// The arguments are those of Decompress_v13: the image's size, the destination's bytes per
// pixel, the two packed blocks, the destination (width x height pixels, no padding) and the
// four scales of the brightness, the blue and red differences and the alpha. Nothing is
// written for 4 bytes per pixel; anything else gets 16-bit RGBA 4444, upside down and two by
// two: an odd width writes one pixel past each row (over the next row's first pixel), an odd
// height writes the last square's second row one row BEFORE the destination. The original
// returns whatever is left in r3 (the row array's address; nobody reads it); this returns 0.
int32_t CWaveletImp::Decompress_v12(int32_t width, int32_t height, int32_t bytesPerPixel, void* data, void* alpha,
                                    void* dest, float lumaScale, float cbScale, float crScale, float alphaScale) {
    const uint32_t sizeX = TransformSizeV12(width);
    const uint32_t sizeY = TransformSizeV12(height);
    const uint32_t size = sizeX > sizeY ? sizeX : sizeY;
    const uint32_t half = size >> 1;

    LbZLib_MemDecompress(data, gWaveletWorkspace, mBuffer);   // its answer is not used
    // One row of coefficients at a time; the colour differences use the first half of it.
    CIgArray<float, float> row;
    row.SetSize(static_cast<int32_t>(size), -1);
    const uint8_t* stream = gWaveletWorkspace;
    stream = RowPassV12(this, stream, row, size, static_cast<int32_t>(size), size, lumaScale);
    ColumnPassV12(this, gWaveletPlanes[kWaveletLuma], size);
    stream = RowPassV12(this, stream, row, half, static_cast<int32_t>(half), half, cbScale);
    ColumnPassV12(this, gWaveletPlanes[kWaveletCb], half);
    RowPassV12(this, stream, row, half, static_cast<int32_t>(half), half, crScale);
    ColumnPassV12(this, gWaveletPlanes[kWaveletCr], half);
    if (alpha != nullptr) {
        LbZLib_MemDecompress(alpha, gWaveletWorkspace, mBuffer);
        RowPassV12(this, gWaveletWorkspace, row, size, static_cast<int32_t>(size), size, alphaScale);
        ColumnPassV12(this, gWaveletPlanes[kWaveletAlpha], size);
    }
    if (bytesPerPixel == 4) return 0;

    // Two image rows a pass, one colour difference sample per 2 x 2 square. `index` is the
    // square's second destination row (height - 2 - y); its first row (height - 1 - y) is the
    // one after it in memory, written first.
    const float* luma = gWaveletPlanes[kWaveletLuma];
    const float* alphas = gWaveletPlanes[kWaveletAlpha];
    uint16_t* pixels = static_cast<uint16_t*>(dest);
    if (alpha != nullptr) {
        gei_TextureSettings.GetAlphaMask();   // called, and the result not used
        int32_t chromaRow = 0;
        for (int32_t y = 0; y < height; y += 2) {
            uint32_t at = y * size;
            int32_t index = width * (height - 2 - y);
            const float* cb = gWaveletPlanes[kWaveletCb] + chromaRow * half;
            const float* cr = gWaveletPlanes[kWaveletCr] + chromaRow * half;
            if (width > 0) {
                for (uint32_t pairs = (static_cast<uint32_t>(width) + 1) >> 1; pairs != 0; --pairs) {
                    const float u = *cb;
                    const float v = *cr;
                    const float blue = kBlueCb * u;
                    const float red = kRedCr * v;
                    const float green = ppc_fnmsubs(kGreenCr, v, ppc_fneg(kGreenCb * u));
                    // The square's four pixels in the listing's order: the image row's two into
                    // the upper destination row, then the next image row's two below them.
                    const uint32_t square[4] = {at, at + 1, at + size, at + size + 1};
                    const int32_t target[4] = {index + width, index + width + 1, index, index + 1};
                    for (int32_t p = 0; p < 4; ++p) {
                        const float y0 = luma[square[p]];
                        const int32_t a = ppc_fctiwz(alphas[square[p]]);
                        const int32_t b = ppc_fctiwz(y0 + blue);
                        const int32_t g = ppc_fctiwz(y0 + green);
                        const int32_t r = ppc_fctiwz(y0 + red);
                        pixels[target[p]] = static_cast<uint16_t>(PackColourV12(r, g, b) | (ClampV12(a) >> 4 & 0xf));
                    }
                    at += 2;
                    index += 2;
                    ++cb;
                    ++cr;
                }
            }
            ++chromaRow;
        }
    } else {
        const uint32_t alphaNibble = gei_TextureSettings.GetAlphaMask() >> 4 & 0xf;
        int32_t chromaRow = 0;
        for (int32_t y = 0; y < height; y += 2) {
            uint32_t at = y * size;
            int32_t index = width * (height - 2 - y);
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
                    const int32_t target[4] = {index + width, index + width + 1, index, index + 1};
                    for (int32_t p = 0; p < 4; ++p) {
                        const float y0 = luma[square[p]];
                        const int32_t b = ppc_fctiwz(y0 + blue);
                        const int32_t r = ppc_fctiwz(y0 + red);
                        const int32_t g = ppc_fctiwz(y0 + green);
                        pixels[target[p]] = static_cast<uint16_t>(alphaNibble | PackColourV12(r, g, b));
                    }
                    at += 2;
                    index += 2;
                    ++cb;
                    ++cr;
                }
            }
            ++chromaRow;
        }
    }
    return 0;
}

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
TYPE* CIgArray<TYPE, ARG_TYPE>::GetData() {
    return mData;
}

CWavelet* CWavelet::Instance() {
    if (gWaveletInstance == nullptr) gWaveletInstance = new (std::nothrow) CWaveletImp;
    if (gWaveletInstance != nullptr) gWaveletInstance->AddRef();
    return gWaveletInstance;
}

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

template class CIgArray<float, float>;

void read_reset();

uint32_t read_code(uint32_t bits);

void LzssDecompress(void* source, void* dest);

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

}  // namespace

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
