// Wavelet.cpp, engine: DecompressColorFast 0x10039894 (3492), DecompressMonoFast
// 0x1003a67c (1168), ReverseChannelFast 0x1003ab50 (956), ConvertAndBlend 0x1003af4c (332).
// The fast format has a smooth half followed by a detail half in each coefficient row.
#include "Wavelet.h"

#include <initializer_list>

#include "IgArray.h"
#include "platform/samsutils.h"
#include "ppc_math.h"

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
