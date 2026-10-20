// The ride sign's image filters (main app, SimTheme Park 2.04b3, rsse.cpp range 0x100c8e40..0x100c9c6c; unit w84):
// erodebuffer_erode, make_gaussian_filter_2d, copy_offset_alpha, dilate, erode and blur. They work on include/Bitmap.h objects
// (one byte per pixel for the masks, though the loops stride by the bitmap's own bytes-per-pixel where the original does).
//
// Quirks kept: the line buffers' new[] is tested for NULL but never is (the runtime's new[] throws); blur leaks its kernel on that
// dead path; dilate, erode and blur walk the first `width` bytes of each row (and `height` of each column) of a pixel wider than one
// byte; copy_offset_alpha writes past the destination row when the move is a whole width or more.
#include <algorithm>
#include <cmath>
#include <cstring>

#include "Bitmap.h"
#include "ppc_math.h"
#include "rsse.h"

namespace {

// The original writes 6.2831853 (a double literal, not 2 pi).
constexpr double kTwoPi = 6.2831853;

// The window clamp copy_offset_alpha uses: `value` if it is in 0 .. max, 0 below, max above.
int32_t ClampToRange(int32_t value, int32_t max) {
    if (value <= max) return value < 0 ? 0 : value;
    return max;
}

}  // namespace

// 0x100c8e40 (596 bytes).
void erodebuffer_erode(Bitmap& work, Bitmap& mask, uint8_t value, Bitmap& out) {
    (void)mask;
    const int32_t width = static_cast<int32_t>(work.mWidth);
    const int32_t height = static_cast<int32_t>(work.mHeight);
    const int32_t rowStride = width * 2;
    uint8_t* pixel = work.mData;
    uint8_t* outPixel = out.mData;
    for (int32_t row = 0; row < height; ++row) {
        for (int32_t col = 0; col < width; ++col) {
            const uint8_t current = pixel[0];
            uint8_t smallest = current;
            if (0 < row) {
                if (0 < col) smallest = std::min(smallest, pixel[-rowStride - 2]);
                smallest = std::min(smallest, pixel[-rowStride]);
                if (col < width - 2) smallest = std::min(smallest, pixel[-rowStride + 2]);
            }
            if (0 < col) smallest = std::min(smallest, pixel[-2]);
            if (col < width - 2) smallest = std::min(smallest, pixel[2]);
            if (row < height - 2) {
                if (0 < col) smallest = std::min(smallest, pixel[rowStride - 2]);
                smallest = std::min(smallest, pixel[rowStride]);
                if (col < width - 2) smallest = std::min(smallest, pixel[rowStride + 2]);
            }
            pixel[1] = smallest;
            if (smallest != current) *outPixel = value;
            pixel += 2;
            ++outPixel;
        }
    }
    uint8_t* now = work.mData;
    const uint8_t* next = work.mData + 1;
    for (int32_t i = 0; i < width * height; ++i) {
        *now = *next;
        next += 2;
        now += 2;
    }
}

// 0x100c90d8 (288 bytes). exp is the C library's; the constants are -0.5 (0x10264f48) and 6.2831853 (0x10264f40).
void make_gaussian_filter_2d(double* filter, int32_t size, double sigma) {
    double sum = 0.0;
    double* entry = filter;
    const int32_t centre = (size - 1) / 2;
    for (int32_t row = 0; row < size; ++row) {
        for (int32_t col = 0; col < size; ++col) {
            const int32_t dx = col - centre;
            const int32_t dy = row - centre;
            const double distance = static_cast<double>(dx * dx + dy * dy);
            const double weight = std::exp((-0.5 * distance) / (sigma * sigma));
            *entry = weight / (kTwoPi * sigma * sigma);
            sum += *entry;
            ++entry;
        }
    }
    for (int32_t row = 0; row < size; ++row) {
        for (int32_t col = 0; col < size; ++col) {
            *filter = *filter / sum;
            ++filter;
        }
    }
}

// 0x100c922c (412 bytes).
int32_t copy_offset_alpha(Bitmap& dest, const Bitmap& src, int32_t dx, int32_t dy) {
    const int32_t height = static_cast<int32_t>(src.mHeight);
    const int32_t width = static_cast<int32_t>(src.mWidth);
    const int32_t firstRow = ClampToRange(-dy, height - 1);
    const int32_t lastRow = ClampToRange(height - dy - 1, height - 1);
    const int32_t firstCol = ClampToRange(-dx, width - 1);
    const int32_t lastCol = ClampToRange(width - dx - 1, width - 1);
    dest.realloc(src.mWidth, src.mHeight, 1);
    if (dy < 0) dy = 0;
    if (dx < 0) dx = 0;
    const int32_t destWidth = static_cast<int32_t>(dest.mWidth);
    const int32_t destStride = static_cast<int32_t>(dest.mBytesPerPixel);
    uint8_t* destRow = dest.mData + (destStride * (destWidth * dy) + dx);
    for (int32_t row = firstRow; row <= lastRow; ++row) {
        const int32_t srcStride = static_cast<int32_t>(src.mBytesPerPixel);
        const uint8_t* from = src.mData + srcStride * (firstCol + row * width);
        uint8_t* to = destRow;
        const int32_t nextRow = static_cast<int32_t>(dest.mWidth) * static_cast<int32_t>(dest.mBytesPerPixel);
        for (int32_t col = firstCol; col <= lastCol; ++col) {
            *to = *from;
            ++to;
            from += srcStride;
        }
        destRow += nextRow;
    }
    return 1;
}

// 0x100c9404 (548 bytes).
int32_t dilate(Bitmap& bitmap, int32_t radius) {
    const int32_t width = static_cast<int32_t>(bitmap.mWidth);
    const int32_t height = static_cast<int32_t>(bitmap.mHeight);
    const int32_t stride = static_cast<int32_t>(bitmap.mBytesPerPixel);
    uint8_t* line = new uint8_t[static_cast<uint32_t>(std::max(width, height))];
    if (line == nullptr) return 0;
    for (int32_t row = 0; row < height; ++row) {
        uint8_t* pixel = bitmap.mData + stride * row * width;
        std::memcpy(line, pixel, static_cast<uint32_t>(width));
        for (int32_t col = 0; col < width; ++col) {
            int32_t first = col - radius;
            if (first < 0) first = 0;
            int32_t last = col + radius;
            if (width - 1 < last) last = width - 1;
            const uint8_t* sample = line + first;
            uint8_t largest = 0;
            for (; first <= last; ++first) {
                if (largest < *sample) largest = *sample;
                ++sample;
            }
            *pixel = largest;
            ++pixel;
        }
    }
    for (int32_t col = 0; col < width; ++col) {
        uint8_t* pixel = bitmap.mData + col * stride;
        for (int32_t row = 0; row < height; ++row) line[row] = pixel[stride * row * width];
        for (int32_t row = 0; row < height; ++row) {
            int32_t first = row - radius;
            if (first < 0) first = 0;
            int32_t last = row + radius;
            if (height - 1 < last) last = height - 1;
            const uint8_t* sample = line + first;
            uint8_t largest = 0;
            for (; first <= last; ++first) {
                if (largest < *sample) largest = *sample;
                ++sample;
            }
            *pixel = largest;
            pixel += width * stride;
        }
    }
    delete[] line;
    return 1;
}

// 0x100c9650 (624 bytes).
int32_t erode(Bitmap& bitmap, int32_t radius) {
    const int32_t width = static_cast<int32_t>(bitmap.mWidth);
    const int32_t height = static_cast<int32_t>(bitmap.mHeight);
    const int32_t stride = static_cast<int32_t>(bitmap.mBytesPerPixel);
    uint8_t* line = new uint8_t[static_cast<uint32_t>(std::max(width, height))];
    if (line == nullptr) return 0;
    for (int32_t row = 0; row < height; ++row) {
        uint8_t* pixel = bitmap.mData + stride * row * width;
        std::memcpy(line, pixel, static_cast<uint32_t>(width));
        for (int32_t col = 0; col < width; ++col) {
            if (col < radius || width - radius - 1 < col) {
                *pixel = 0;
            } else {
                int32_t first = col - radius;
                if (first < 0) first = 0;
                int32_t last = col + radius;
                if (width - 1 < last) last = width - 1;
                const uint8_t* sample = line + first;
                uint8_t smallest = 0xff;
                for (; first <= last; ++first) {
                    if (*sample < smallest) smallest = *sample;
                    ++sample;
                }
                *pixel = smallest;
            }
            ++pixel;
        }
    }
    for (int32_t col = 0; col < width; ++col) {
        uint8_t* pixel = bitmap.mData + col * stride;
        for (int32_t row = 0; row < height; ++row) line[row] = pixel[stride * row * width];
        for (int32_t row = 0; row < height; ++row) {
            if (row < radius || height - radius - 1 < row) {
                *pixel = 0;
            } else {
                int32_t first = row - radius;
                if (first < 0) first = 0;
                int32_t last = row + radius;
                if (height - 1 < last) last = height - 1;
                const uint8_t* sample = line + first;
                uint8_t smallest = 0xff;
                for (; first <= last; ++first) {
                    if (*sample < smallest) smallest = *sample;
                    ++sample;
                }
                *pixel = smallest;
            }
            pixel += width * stride;
        }
    }
    delete[] line;
    return 1;
}

// 0x100c98e4 (904 bytes). The kernel is (int)(2.5 * sigma) + 3 taps; each sum is one fused multiply-add per tap (fmadd), stored as
// the low byte of the truncated integer.
int32_t blur(Bitmap& bitmap, double sigma) {
    const int32_t taps = ppc_fctiwz(2.5 * sigma) + 3;
    double* kernel = new double[static_cast<uint32_t>(taps)];
    double sum = 0.0;
    double* entry = kernel;
    const int32_t half = (taps - 1) / 2;
    for (int32_t i = 0; i < taps; ++i) {
        const int32_t distance = i - half;
        const double weight = std::exp((-0.5 * static_cast<double>(distance * distance)) / (sigma * sigma));
        *entry = weight / (kTwoPi * sigma * sigma);
        sum += *entry;
        ++entry;
    }
    entry = kernel;
    for (int32_t i = 0; i < taps; ++i) {
        *entry = *entry / sum;
        ++entry;
    }
    const int32_t width = static_cast<int32_t>(bitmap.mWidth);
    const int32_t height = static_cast<int32_t>(bitmap.mHeight);
    const int32_t stride = static_cast<int32_t>(bitmap.mBytesPerPixel);
    uint8_t* line = new uint8_t[static_cast<uint32_t>(std::max(width, height))];
    if (line == nullptr) return 0;
    for (int32_t row = 0; row < height; ++row) {
        uint8_t* pixel = bitmap.mData + stride * row * width;
        std::memcpy(line, pixel, static_cast<uint32_t>(width));
        for (int32_t col = 0; col < width; ++col) {
            const int32_t start = col - half;
            int32_t first = start < 0 ? 0 : start;
            int32_t last = start - 1 + taps;
            if (width - 1 < last) last = width - 1;
            const double* tap = kernel + (first - start);
            const uint8_t* sample = line + first;
            double total = 0.0;
            for (; first <= last; ++first) {
                total = ppc_fmadd(*tap, static_cast<double>(*sample), total);
                ++tap;
                ++sample;
            }
            *pixel = static_cast<uint8_t>(ppc_fctiwz(total));
            ++pixel;
        }
    }
    for (int32_t col = 0; col < width; ++col) {
        uint8_t* pixel = bitmap.mData + col * stride;
        for (int32_t row = 0; row < height; ++row) line[row] = pixel[stride * row * width];
        for (int32_t row = 0; row < height; ++row) {
            const int32_t start = row - half;
            int32_t first = start < 0 ? 0 : start;
            int32_t last = start - 1 + taps;
            if (height - 1 < last) last = height - 1;
            const double* tap = kernel + (first - start);
            const uint8_t* sample = line + first;
            double total = 0.0;
            for (; first <= last; ++first) {
                total = ppc_fmadd(*tap, static_cast<double>(*sample), total);
                ++tap;
                ++sample;
            }
            *pixel = static_cast<uint8_t>(ppc_fctiwz(total));
            pixel += width * stride;
        }
    }
    delete[] line;
    delete[] kernel;
    return 1;
}
