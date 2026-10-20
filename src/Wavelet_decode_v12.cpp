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
#include "Wavelet.h"

#include "IgArray.h"
#include "ppc_math.h"
#include "tb_resource_texture.h"

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
