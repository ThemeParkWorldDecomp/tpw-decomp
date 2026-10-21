// RideSign.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "Bitmap.h"
#include "FS_System.h"
#include "PlasticShader.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "rsse.h"
#include "SignRenderer.h"
#include "TPGlobals.h"
#include "TrueTypeRender.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

// The ride sign's image filters (main app, SimTheme Park 2.04b3, rsse.cpp range 0x100c8e40..0x100c9c6c; unit w84):
// erodebuffer_erode, make_gaussian_filter_2d, copy_offset_alpha, dilate, erode and blur. They work on include/Bitmap.h objects
// (one byte per pixel for the masks, though the loops stride by the bitmap's own bytes-per-pixel where the original does).
//
// Quirks kept: the line buffers' new[] is tested for NULL but never is (the runtime's new[] throws); blur leaks its kernel on that
// dead path; dilate, erode and blur walk the first `width` bytes of each row (and `height` of each column) of a pixel wider than one
// byte; copy_offset_alpha writes past the destination row when the move is a whole width or more.

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

// TrueTypeRender (main app, rsse.cpp range): the constructor (0x100c9c90), the deleting-destructor shell (0x100c9d50),
// render_font_unicode (0x100c9dc0) and load_settings (0x100ca340).
//
// The NULL checks after new[] are the original's but dead in the game: the runtime's operator new[] throws.

TrueTypeRender::TrueTypeRender() {
    mLogFont.lfHeight = 0;
    mLogFont.lfWidth = 0;
    mLogFont.lfEscapement = 0;
    mLogFont.lfOrientation = 0;
    mLogFont.lfWeight = 400;
    mLogFont.lfItalic = 0;
    mLogFont.lfUnderline = 0;
    mLogFont.lfStrikeOut = 0;
    mLogFont.lfCharSet = 1;
    mLogFont.lfOutPrecision = 4;
    mLogFont.lfClipPrecision = 1;
    mLogFont.lfQuality = 4;
    mLogFont.lfPitchAndFamily = 0;
    std::strcpy(mLogFont.lfFaceName, "Arial Black");   // the string at 0x1023ee9b
    mF008 = 100;
    mTextY = 0;
}

TrueTypeRender::~TrueTypeRender() {}

int32_t TrueTypeRender::render_font_unicode(NS_MacDoze::InternalHandle* dc, const char16_t* text, Bitmap& out) {
    const uint32_t length = wcslen(text);
    if (length == 0) {
        mLeft = 0;
        mRight = 0;
        // bytes per pixel * (width * height), as the signed loop bound the listing computes
        const int32_t total = static_cast<int32_t>(out.mBytesPerPixel * (out.mWidth * out.mHeight));
        for (int32_t i = 0; i < total; ++i) out.mData[i] = 0;
        return 1;
    }

    char* narrow = new char[length + 1];
    if (narrow == nullptr) return 0;
    // snprintf(narrow, length + 1, "%ls", text) with MSL's 16-bit wchar_t (the host's is wider, so widen first)
    wchar_t* wide = static_cast<wchar_t*>(std::malloc((length + 1) * sizeof(wchar_t)));
    for (uint32_t i = 0; i <= length; ++i) wide[i] = text[i];
    std::snprintf(narrow, length + 1, "%ls", wide);
    std::free(wide);

    // The bitmap header and a 256-entry grey palette: a DIB twice as wide and high as the mask (top-down, 8 bits).
    uint8_t* raw = new uint8_t[0x42c];
    if (raw == nullptr) return 0;   // narrow leaks
    NS_MacDoze::tagBITMAPINFO* info = reinterpret_cast<NS_MacDoze::tagBITMAPINFO*>(raw);
    info->bmiHeader.biSize = 0x28;
    info->bmiHeader.biWidth = static_cast<int32_t>(out.mWidth) * 2;
    info->bmiHeader.biHeight = static_cast<int32_t>(out.mHeight) * -2;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 8;
    info->bmiHeader.biCompression = 0;
    info->bmiHeader.biSizeImage = 0;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed = 0x100;
    info->bmiHeader.biClrImportant = 0x100;
    info->bmiHeader.biSizeImage = 0x8000;
    NS_MacDoze::tagRGBQUAD* palette = reinterpret_cast<NS_MacDoze::tagRGBQUAD*>(raw + 0x28);
    for (int32_t i = 0; i < 0x100; ++i) {
        palette[i].rgbRed = static_cast<uint8_t>(i);
        palette[i].rgbGreen = static_cast<uint8_t>(i);
        palette[i].rgbBlue = static_cast<uint8_t>(i);   // rgbReserved is not written
    }
    void* bits = nullptr;
    NS_MacDoze::InternalHandle* dib = NS_MacDoze::CreateDIBSection(dc, info, 0, &bits, nullptr, 0);
    delete[] raw;
    if (dib == nullptr) return 0;   // narrow leaks

    NS_MacDoze::InternalHandle* memDC = NS_MacDoze::CreateCompatibleDC(dc);
    NS_MacDoze::InternalHandle* oldBitmap = NS_MacDoze::SelectObject(memDC, dib);
    NS_MacDoze::SetMapMode(memDC, 1);
    const int32_t savedWidth = mLogFont.lfWidth;
    NS_MacDoze::InternalHandle* font = NS_MacDoze::CreateFontIndirect(&mLogFont);
    NS_MacDoze::InternalHandle* oldFont = NS_MacDoze::SelectObject(memDC, font);
    NS_MacDoze::tagSIZE size;
    NS_MacDoze::GetTextExtentPoint(memDC, narrow, static_cast<int32_t>(length), &size);

    const int32_t limit = static_cast<int32_t>(out.mWidth) << 1;
    if (size.cx >= limit) {
        // The text is too wide: bisect lfWidth (lo..hi) until the extent fits, stopping with that font still selected.
        int32_t hi = mLogFont.lfWidth;
        if (hi == 0) hi = -mLogFont.lfHeight;
        int32_t lo = 1;
        int32_t mid = ((hi - 1) >> 1) + 1;
        bool done;
        do {
            mLogFont.lfWidth = mid;
            font = NS_MacDoze::CreateFontIndirect(&mLogFont);
            oldFont = NS_MacDoze::SelectObject(memDC, font);
            NS_MacDoze::GetTextExtentPoint(memDC, narrow, static_cast<int32_t>(length), &size);
            if (hi - lo < 3 && size.cx < limit) {
                done = true;
            } else {
                done = false;
                NS_MacDoze::SelectObject(memDC, oldFont);
                NS_MacDoze::DeleteObject(font);
                if (size.cx < limit) {
                    lo = mid;
                    mid = mid + ((hi - mid) >> 1);
                } else {
                    hi = mid;
                    mid = lo + ((mid - lo) >> 1);
                }
            }
        } while (!done);
    }
    mLogFont.lfWidth = savedWidth;
    NS_MacDoze::SetBkColor(memDC, 0);
    NS_MacDoze::SetTextColor(memDC, 0xffffff);
    NS_MacDoze::TextOut(memDC, 0x100 - size.cx / 2, mTextY, narrow, static_cast<int32_t>(length));
    delete[] narrow;
    NS_MacDoze::SelectObject(memDC, oldFont);
    NS_MacDoze::DeleteObject(font);

    // Box-filter the DIB (two rows of 2 * width bytes per mask row) down into the mask.
    out.realloc(out.mWidth, out.mHeight, 1);
    const uint8_t* top = static_cast<const uint8_t*>(bits);
    uint8_t* dst = out.mData;
    const int32_t width = static_cast<int32_t>(out.mWidth);
    for (int32_t y = 0; y < static_cast<int32_t>(out.mHeight); ++y) {
        const uint8_t* a = top;
        const uint8_t* b = top + width * 2;
        for (int32_t x = 0; x < static_cast<int32_t>(out.mWidth); ++x) {
            const int32_t sum = a[0] + a[1] + b[0] + b[1];
            *dst++ = static_cast<uint8_t>(sum >> 2);
            a += 2;
            b += 2;
        }
        top += static_cast<int32_t>(out.mWidth) * 4;
    }
    NS_MacDoze::SelectObject(memDC, oldBitmap);
    NS_MacDoze::DeleteObject(dib);
    NS_MacDoze::DeleteDC(memDC);

    // The first row with ink and the row after the second and later ones. The scan stops at the first non-zero byte, so
    // a row whose only ink is its last column does not count, and a single inked row leaves mRight at -1.
    mLeft = -1;
    mRight = -1;
    const uint8_t* row = out.mData;
    for (int32_t y = 0; y < static_cast<int32_t>(out.mHeight); ++y) {
        int32_t x = 0;
        uint8_t seen = 0;
        const uint8_t* p = row;
        while (x < static_cast<int32_t>(out.mWidth) && seen == 0) {
            seen |= *p++;
            ++x;
        }
        if (x < static_cast<int32_t>(out.mWidth)) {
            if (mLeft < 0) mLeft = y;
            else mRight = y + 1;
        }
        row += out.mWidth;
    }
    return 1;
}
int32_t TrueTypeRender::load_settings(bfl::FileSystem::CFile* file) {
    TPFile_Read(mF04C, 0x40, 1, file);
    TPFile_Read(mName, 0x104, 1, file);
    TPFile_Read(&mF008, 4, 1, file);
    SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(&mF008), 1);
    TPFile_Read(&mTextY, 4, 1, file);
    SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(&mTextY), 1);
    TPFile_Read(&mLogFont, 0x3c, 1, file);
    // Only the five int32 words are swapped (the inline byte swaps); the flag bytes and the face name stay as read.
    uint32_t* words = reinterpret_cast<uint32_t*>(&mLogFont);
    for (int32_t i = 0; i < 5; ++i) {
        const uint32_t v = words[i];
        words[i] = (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24);
    }
    return 1;
}

// The ride sign's shaders (main app, SimTheme Park 2.04b3, rsse.cpp range 0x100ca558..0x100cb804; unit w84): PlasticShader (clear,
// destructor, load_settings, bevel, blur_with_mask, plastic_shade) and ShadowGlow (shadow_glow, destructor). The filters they call
// are in rsse_sign_filters.cpp.
//
// The float arithmetic is the Mac's, single precision with fused multiply-adds where the listing has them (ppc_math.h); sin, cos,
// sqrt and pow are the C library's, called on the doubles the Mac converts to.

namespace {

// 0x10264f0c: degrees to radians as a float.
constexpr float kRadiansPerDegree = 0.017453292f;

// One setting of the file: four little-endian bytes, swapped into place (the Mac reads the words into the object and swaps them).
template <class T>
void ReadWord(T& field, bfl::FileSystem::CFile* file) {
    TPFile_Read(&field, 4, 1, file);
    SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(&field), 1);
}

// plastic_shade's clamp to 0 .. 255 of a float (the listing's compares: above 255 gives 255; otherwise the value, or 0 when it is
// below 0; a NaN passes through and fctiwz makes it 0x80000000).
float ClampColour(float value) {
    float result = 255.0f;
    if (!(value > 255.0f)) {
        result = value;
        if (value < 0.0f) result = 0.0f;
    }
    return result;
}

}  // namespace

// 0x100ca558 (284 bytes). clear() first; the four bitmaps are then destroyed (the members' inline destructors, last to first).
PlasticShader::~PlasticShader() { clear(); }

// 0x100ca6a0 (88 bytes).
void PlasticShader::clear() {
    mMask.clear();
    mFile.clear();
    mBevel.clear();
    mShaded.clear();
}

// 0x100ca724 (492 bytes).
int32_t PlasticShader::load_settings(bfl::FileSystem::CFile* file) {
    ReadWord(mBevelWidth, file);
    ReadWord(mHeightScale, file);
    ReadWord(mBlur, file);
    ReadWord(mAmbient, file);
    ReadWord(mDiffuse, file);
    ReadWord(mSpecular, file);
    ReadWord(mShininess, file);
    ReadWord(mLightYaw, file);
    ReadWord(mLightPitch, file);
    ReadWord(mPeriodY, file);
    ReadWord(mOffsetY, file);
    return 1;
}

// 0x100ca95c (468 bytes). The work buffer is a local Bitmap {width, height, 2, new[]}; erodebuffer_erode strips one layer of it per
// round and stamps (uchar)(256 * (2 t^3 - 3 t^2)) with t = 1 - i / width into `out`: negative for 0 < t < 1, so the byte is
// 256 less its magnitude (0 for the first round, t = 1).
void PlasticShader::bevel(Bitmap& mask, uint32_t width, Bitmap& out) {
    Bitmap work;
    work.mWidth = mask.mWidth;
    work.mHeight = mask.mHeight;
    work.mBytesPerPixel = 2;
    work.mData = new uint8_t[(mask.mWidth * mask.mHeight) << 1];
    const uint8_t* maskPixel = mask.mData;
    uint8_t* workPixel = work.mData;
    uint8_t* outPixel = out.mData;
    for (int32_t i = 0; i < static_cast<int32_t>(mask.mWidth) * static_cast<int32_t>(mask.mHeight); ++i) {
        workPixel[0] = *maskPixel == 0 ? 0 : 0xff;
        workPixel[1] = *maskPixel == 0 ? 0 : 0xff;
        workPixel += 2;
        ++maskPixel;
        *outPixel = 0;
        ++outPixel;
    }
    for (uint32_t i = 0; i < width; ++i) {
        const float t = 1.0f - static_cast<float>(i) / static_cast<float>(width);
        float twice = 2.0f * t;
        float thrice = 3.0f * t;
        twice = twice * t;
        thrice = thrice * t;
        const float curve = ppc_fmsubs(t, twice, thrice);
        const int32_t value = ppc_fctiwz(256.0f * curve);
        erodebuffer_erode(work, mask, static_cast<uint8_t>(value), out);
    }
    const uint8_t* core = work.mData;
    uint8_t* bevelOut = out.mData;
    for (int32_t i = 0; i < static_cast<int32_t>(work.mWidth) * static_cast<int32_t>(work.mHeight); ++i) {
        *bevelOut = *bevelOut | *core;
        core += 2;
        ++bevelOut;
    }
}

// 0x100cab6c (912 bytes). A size x size kernel (size = (int)(2.5 * sigma) + 3), a ring of `above` copies of the first row (the rows
// above the current one; the last is the current row's copy) and `below` pointers into the bitmap itself (the rows below, clamped to
// the last row: on the last rows they point at rows the pass has already written). The kernel is read straight through, one entry
// per sample, which is exactly size * size entries.
void PlasticShader::blur_with_mask(Bitmap& bitmap, Bitmap& mask, float sigma) {
    const int32_t n = ppc_fctiwz(2.5 * static_cast<double>(sigma));
    const int32_t size = n + 3;
    double* kernel = new double[static_cast<uint32_t>(size * size)];
    make_gaussian_filter_2d(kernel, size, static_cast<double>(sigma));
    const int32_t above = (size + 1) / 2;
    uint8_t** ring = new uint8_t*[static_cast<uint32_t>(above)];
    for (int32_t i = 0; i < above; ++i) {
        ring[i] = new uint8_t[bitmap.mWidth];
        std::memcpy(ring[i], bitmap.mData, bitmap.mWidth);
    }
    const int32_t belowCount = size - above;
    uint8_t** below = new uint8_t*[static_cast<uint32_t>(belowCount)];
    for (int32_t i = 0; i < belowCount; ++i) {
        const int32_t row = std::min(i + 1, static_cast<int32_t>(bitmap.mHeight) - 1);
        below[i] = bitmap.mData + static_cast<int32_t>(bitmap.mWidth) * row;
    }
    const int32_t centre = (size - 1) / 2;
    const int32_t end = (size + 2) / 2;
    uint8_t* pixel = bitmap.mData;
    const uint8_t* maskPixel = mask.mData;
    for (int32_t row = 0; row < static_cast<int32_t>(bitmap.mHeight); ++row) {
        uint8_t* nextRowPixel = pixel + bitmap.mWidth;
        const uint8_t* nextMaskRow = maskPixel + mask.mWidth;
        for (int32_t col = 0; col < static_cast<int32_t>(bitmap.mWidth); ++col) {
            if (*maskPixel != 0) {
                const double* tap = kernel;
                double total = 0.0;
                for (int32_t dy = -centre; dy < end; ++dy) {
                    const uint8_t* source = dy <= 0 ? ring[dy + centre] : below[dy - 1];
                    for (int32_t dx = -centre; dx < end; ++dx) {
                        int32_t x = static_cast<int32_t>(bitmap.mWidth) - 1;
                        if (col + dx <= x) {
                            x = col + dx;
                            if (x < 0) x = 0;
                        }
                        total = ppc_fmadd(*tap, static_cast<double>(source[x]), total);
                        ++tap;
                    }
                }
                *pixel = static_cast<uint8_t>(ppc_fctiwz(total));
            }
            ++maskPixel;
            ++pixel;
        }
        // The ring turns: the oldest copy becomes the newest and takes the first row below; the pointers below shift, the last one
        // taking the row size / 2 + 1 further on.
        uint8_t* oldest = ring[0];
        for (int32_t i = 0; i < above - 1; ++i) ring[i] = ring[i + 1];
        ring[above - 1] = oldest;
        std::memcpy(ring[above - 1], below[0], bitmap.mWidth);
        for (int32_t i = 0; i < belowCount - 1; ++i) below[i] = below[i + 1];
        const int32_t nextRow = std::min(row + size / 2 + 1, static_cast<int32_t>(bitmap.mHeight) - 1);
        below[belowCount - 1] = bitmap.mData + static_cast<int32_t>(bitmap.mWidth) * nextRow;
        maskPixel = nextMaskRow;
        pixel = nextRowPixel;
    }
    for (int32_t i = 0; i < above; ++i) delete[] ring[i];
    delete[] ring;
    delete[] below;
    delete[] kernel;
}

// 0x100caf40 (1260 bytes). The light is (0, 0, 1) turned by the yaw (negated) and the pitch; the half vector is (L + (0, 0, 1)) / 2,
// both normalised. Each lit pixel (mask byte set) takes the slope of `bevel` (the bytes below and to the right minus its own, read
// past the last row and column), makes the normal (nx, ny, 1)/|..|, and shades the tile's colour: ambient, plus diffuse * N.L when
// N.L is above 0, plus specular * pow(N.H, shininess) when N.H is above 0, times 255 and clamped. The output is the mask byte, then
// the three shaded bytes.
void PlasticShader::plastic_shade(Bitmap& bevelBitmap) {
    const int32_t periodY = mPeriodY;
    const uint8_t* bevelPixel = bevelBitmap.mData;
    const uint8_t* maskPixel = mMask.mData;
    uint8_t* shaded = mShaded.mData;
    if (periodY == 0) return;
    Bitmap tile;
    tile.mWidth = mFile.mWidth;
    tile.mHeight = static_cast<uint32_t>(periodY);
    tile.mBytesPerPixel = 4;
    tile.mData = new uint8_t[(tile.mWidth * static_cast<uint32_t>(periodY)) << 2];
    mFile.descimate(tile);

    const float yaw = kRadiansPerDegree * ppc_fneg(mLightYaw);
    const float yawSin = static_cast<float>(std::sin(static_cast<double>(yaw)));
    const float yawCos = static_cast<float>(std::cos(static_cast<double>(yaw)));
    const float pitch = kRadiansPerDegree * mLightPitch;
    const float turnedX = ppc_fmadds(yawCos, 1.0f, yawSin * 0.0f);
    const float turnedZ = ppc_fmadds(ppc_fneg(yawSin), 1.0f, yawCos * 0.0f);
    const float pitchSin = static_cast<float>(std::sin(static_cast<double>(pitch)));
    const float pitchCos = static_cast<float>(std::cos(static_cast<double>(pitch)));
    const float lightA = ppc_fmadds(pitchCos, turnedX, pitchSin * 0.0f);
    const float lightB = ppc_fmadds(ppc_fneg(pitchSin), turnedX, pitchCos * 0.0f);
    const float lightLength2 = ppc_fmadds(turnedZ, turnedZ, ppc_fmadds(lightA, lightA, lightB * lightB));
    const float lightScale = 1.0f / static_cast<float>(std::sqrt(static_cast<double>(lightLength2)));
    const float lightX = lightA * lightScale;
    const float lightY = lightB * lightScale;
    const float lightZ = turnedZ * lightScale;

    const float sumX = lightX + 0.0f;
    const float sumY = lightY + 0.0f;
    const float sumZ = lightZ + 1.0f;
    const float halfX = sumX * 0.5f;
    const float halfY = sumY * 0.5f;
    const float halfZ = sumZ * 0.5f;
    const float halfLength2 = ppc_fmadds(halfZ, halfZ, ppc_fmadds(halfX, halfX, halfY * halfY));
    const float halfScale = 1.0f / static_cast<float>(std::sqrt(static_cast<double>(halfLength2)));
    const float hx = halfX * halfScale;
    const float hy = halfY * halfScale;
    const float hz = halfZ * halfScale;

    const float ambient = mAmbient;
    const float diffuse = mDiffuse;
    const float specular = mSpecular;
    for (int32_t row = 0; row < static_cast<int32_t>(bevelBitmap.mHeight); ++row) {
        for (int32_t col = 0; col < static_cast<int32_t>(bevelBitmap.mWidth); ++col) {
            if (*maskPixel == 0) {
                shaded[0] = 0;
                shaded[1] = 0;
                shaded[2] = 0;
                shaded[3] = 0;
            } else {
                const int32_t down = static_cast<int32_t>(bevelPixel[bevelBitmap.mWidth]) - static_cast<int32_t>(bevelPixel[0]);
                const int32_t right = static_cast<int32_t>(bevelPixel[1]) - static_cast<int32_t>(bevelPixel[0]);
                const float slopeDown = mHeightScale * static_cast<float>(down);
                const float slopeRight = mHeightScale * static_cast<float>(right);
                const float gy = slopeDown / 255.0f;
                const float gx = slopeRight / 255.0f;
                const float negGy = -1.0f * gy;
                const float negGx = gx * -1.0f;
                const float nx = ppc_fmsubs(gx, 0.0f, negGy);
                const float ny = ppc_fmsubs(0.0f, gy, negGx);
                const float normal2 = 1.0f + ppc_fmadds(ny, ny, nx * nx);
                const float normalScale = 1.0f / static_cast<float>(std::sqrt(static_cast<double>(normal2)));
                const float unitX = nx * normalScale;
                const float unitY = ny * normalScale;
                const float unitZ = 1.0f * normalScale;
                const float nDotL = ppc_fmadds(unitZ, lightZ, ppc_fmadds(unitY, lightX, unitX * lightY));
                float nDotH = ppc_fmadds(unitZ, hz, ppc_fmadds(unitY, hx, unitX * hy));

                const int32_t tileWidth = static_cast<int32_t>(tile.mWidth);
                const int32_t tileHeight = static_cast<int32_t>(tile.mHeight);
                const int32_t tileRow = row - mOffsetY;
                const int32_t tileRowMod = tileRow - (tileRow / tileHeight) * tileHeight;
                const int32_t tileColMod = col - (col / tileWidth) * tileWidth;
                const int32_t texel = static_cast<int32_t>(tile.mBytesPerPixel) * (tileColMod + tileRowMod * tileWidth);
                const float tileR = static_cast<float>(tile.mData[texel + 1]);
                const float tileG = static_cast<float>(tile.mData[texel + 2]);
                const float tileB = static_cast<float>(tile.mData[texel + 3]);
                const float colourG = tileG / 255.0f;
                const float colourB = tileB / 255.0f;
                const float colourR = tileR / 255.0f;
                float outB = colourB * ambient;
                float outG = colourG * ambient;
                float outR = colourR * ambient;
                if (0.0 < static_cast<double>(nDotL)) {
                    const float lit = diffuse * nDotL;
                    outB = outB + colourB * lit;
                    outG = outG + colourG * lit;
                    outR = outR + colourR * lit;
                }
                if (0.0 < static_cast<double>(nDotH)) {
                    const float power = static_cast<float>(std::pow(static_cast<double>(nDotH), static_cast<double>(mShininess)));
                    const float shine = 1.0f * (specular * power);
                    outR = outR + shine;
                    outG = outG + shine;
                    outB = outB + shine;
                }
                outR = outR * 255.0f;
                outG = outG * 255.0f;
                outB = outB * 255.0f;
                shaded[0] = *maskPixel;
                shaded[1] = static_cast<uint8_t>(ppc_fctiwz(ClampColour(outR)));
                shaded[2] = static_cast<uint8_t>(ppc_fctiwz(ClampColour(outG)));
                shaded[3] = static_cast<uint8_t>(ppc_fctiwz(ClampColour(outB)));
            }
            shaded += 4;
            ++maskPixel;
            ++bevelPixel;
        }
    }
}

// 0x100cb468 (236 bytes). The failure branch (a step answering other than 1, then out.clear() and 0) is the original's but cannot
// be reached: copy_offset_alpha answers 1 and the others fail only when new[] answers NULL.
uint32_t ShadowGlow::shadow_glow(const Bitmap& src, Bitmap& out) const {
    if (src.mData == nullptr) return 0;
    out.realloc(src.mWidth, src.mHeight, 1);
    uint32_t answer = static_cast<uint32_t>(copy_offset_alpha(out, src, mOffsetX, mOffsetY));
    if ((answer & 0xff) == 1) {
        if (mGrow < 0) {
            answer = static_cast<uint32_t>(erode(out, -mGrow));
        } else if (0 < mGrow) {
            answer = static_cast<uint32_t>(dilate(out, mGrow));
        }
        if ((answer & 0xff) == 1) {
            if (0.0 != static_cast<double>(mBlur)) return static_cast<uint32_t>(blur(out, static_cast<double>(mBlur)));
            return answer;
        }
    }
    out.clear();
    return 0;
}

// SignRenderer shell (main app, rsse.cpp, unit w82; 4,160 bytes).

// 0x100cb594, 512 bytes. Settings words and unspecified shader words are untouched.
SignRenderer::SignRenderer() : mGlow1{}, mGlow2{} {
    mWavelet = CWavelet::Instance();
}

// 0x100cb7c0 (68 bytes).
ShadowGlow::~ShadowGlow() {}

// 0x100cb82c, 220 bytes. Implicit member destruction runs shader 2, shader 1,
// output, background. TrueTypeRender's destructor is the empty deleting-destructor shell (0x100c9d50).
SignRenderer::~SignRenderer() {
    mWavelet->Release();
}

// 0x100cb934, 1,016 bytes. The Mac never closes this file, including on failure.
int32_t SignRenderer::sign_load_settings(const char* path) {
    auto* file = TPFile_Open(path, "rb");
    if (file == nullptr) return 0;
    auto word = [file](void* dest) {
        TPFile_Read(dest, 4, 1, file);
        SamsUtilities::_USwapBlock32(static_cast<uint32_t*>(dest), 1);
    };
    word(&mVersion);
    if (mVersion < 100) return 0;
    word(&mF004);
    TPFile_Read(&mHasBackground, 1, 1, file);
    word(&mGlowMode1);
    word(&mGlowMode2);
    mFont1.load_settings(file);
    mShader1.load_settings(file);
    mFont2.load_settings(file);
    mShader2.load_settings(file);
    auto glow = [&](ShadowGlow& value) {
        TPFile_Read(&value.mRed, 1, 1, file);
        TPFile_Read(&value.mGreen, 1, 1, file);
        TPFile_Read(&value.mBlue, 1, 1, file);
        TPFile_Read(&value.mAlpha, 1, 1, file);
        word(&value.mGrow);
        word(&value.mBlur);
        word(&value.mOffsetX);
        word(&value.mOffsetY);
    };
    if (mGlowMode1 != 0) glow(mGlow1);
    if (mGlowMode2 != 0) glow(mGlow2);
    mShader1.mFile.load(file);
    mShader2.mFile.load(file);
    if (mHasBackground != 0) {
        if (mVersion == 100) mBackground.load(file);
        else mBackground.load_wavelet(file, mWavelet);
    }
    return 1;
}

// 0x100cbd68, 320 bytes.
int32_t SignRenderer::sign_install_fonts(const char* path) {
    TbDynamicStringTemplate<char> name;
    name = path;
    name += gTPGlobals.mSeparator;
    name += mFont1.mName;
    if (NS_MacDoze::AddFontResource(name.mData) == 0) return 0;
    if (std::strcmp(mFont1.mName, mFont2.mName) != 0) {
        name = path;
        name += gTPGlobals.mSeparator;
        name += mFont2.mName;
        if (NS_MacDoze::AddFontResource(name.mData) == 0) return 0;
    }
    return 1;
}

// 0x100cbee4, 2,092 bytes. Only the first height rows of the doubled output
// are split across the two textures, with destination rows traversed bottom-up.
int32_t SignRenderer::sign_render(NS_MacDoze::InternalHandle* dc, sign_renderer_options& options) {
    if (!options.mText1 || !options.mText2 || !options.mTexture1 || !options.mTexture2) return 0;
    const uint32_t width = static_cast<uint32_t>(options.mWidth) * 2;
    const uint32_t height = static_cast<uint32_t>(options.mHeight) * 2;
    mOutput.realloc(width, height, 4);
    mShader1.mMask.realloc(width, height, 1);
    mShader2.mMask.realloc(width, height, 1);
    Bitmap local1;
    local1.mWidth = width;
    local1.mHeight = height;
    local1.mBytesPerPixel = 1;
    local1.mData = new uint8_t[width * height];
    Bitmap local2;
    local2.mWidth = width;
    local2.mHeight = height;
    local2.mBytesPerPixel = 1;
    local2.mData = new uint8_t[width * height];
    mFont1.render_font_unicode(dc, options.mText1, mShader1.mMask);
    mFont2.render_font_unicode(dc, options.mText2, mShader2.mMask);
    if (mGlowMode1 == 0) local1.colour_fill(0, 0, 0, 0);
    else mGlow1.shadow_glow(mShader1.mMask, local1);
    if (mGlowMode2 == 0) local2.colour_fill(0, 0, 0, 0);
    else mGlow2.shadow_glow(mShader2.mMask, local2);
    auto shade = [](PlasticShader& shader, const TrueTypeRender& font) {
        shader.mPeriodY = font.mRight - font.mLeft;
        shader.mOffsetY = font.mLeft;
        if (shader.mMask.mData && shader.mFile.mData) {
            shader.mBevel.realloc(shader.mMask.mWidth, shader.mMask.mHeight, 1);
            shader.mShaded.realloc(shader.mMask.mWidth, shader.mMask.mHeight, 4);
            shader.bevel(shader.mMask, shader.mBevelWidth, shader.mBevel);
            if (shader.mBlur > 0.0f) shader.blur_with_mask(shader.mBevel, shader.mMask, shader.mBlur);
            shader.plastic_shade(shader.mBevel);
        }
    };
    shade(mShader1, mFont1);
    shade(mShader2, mFont2);
    mOutput.colour_fill(0, 0, 0, 0);
    if (mHasBackground) mBackground.alphablt(mOutput, 0, 0);
    auto blt = [&](Bitmap& bitmap, const ShadowGlow& glow) {
        bitmap.colourblt(mOutput, glow.mAlpha, glow.mRed, glow.mGreen, glow.mBlue);
    };
    if (mGlowMode1 == 2 && mGlowMode2 == 2) {
        Bitmap combined;
        combined.mWidth = local1.mWidth;
        combined.mHeight = local1.mHeight;
        combined.mBytesPerPixel = 1;
        combined.mData = new uint8_t[combined.mWidth * combined.mHeight];
        combined.colour_fill(0, 0, 0, 0);
        local1.copy_to(combined);
        local2.shadowblend(combined);
        blt(combined, mGlow1);
    } else if (mGlowMode1 == 2) blt(local1, mGlow1);
    else if (mGlowMode2 == 2) blt(local2, mGlow2);
    auto line1 = [&] {
        if (mGlowMode1 == 1) blt(local1, mGlow1);
        mShader1.mShaded.alphablt(mOutput, 0, 0);
    };
    auto line2 = [&] {
        if (mGlowMode2 == 1) blt(local2, mGlow2);
        mShader2.mShaded.alphablt(mOutput, 0, 0);
    };
    if (mF004 == 0) { line1(); line2(); }
    else { line2(); line1(); }
    uint8_t* source = mOutput.mData;
    if (options.mQuality == 1) mOutput.swizzle_for_gimex();
    if (options.mQuality == 1 || options.mQuality == 2) {
        const int32_t pixelSize = options.mQuality == 1 ? 4 : 2;
        for (int32_t y = 0; y < options.mHeight; ++y) {
            for (void* texture : {options.mTexture1, options.mTexture2}) {
                auto* dest = static_cast<uint8_t*>(texture) + (options.mHeight - 1 - y) * options.mWidth * pixelSize;
                for (int32_t x = 0; x < options.mWidth; ++x) {
                    if (pixelSize == 4) std::memcpy(dest, source, 4);
                    else {
                        uint16_t pixel = (source[0] >> 4) | (source[3] & 0xf0)
                            | ((source[1] & 0xf0) << 8) | ((source[2] & 0xf0) << 4);
                        std::memcpy(dest, &pixel, 2);
                    }
                    source += 4;
                    dest += pixelSize;
                }
            }
        }
    }
    return 1;
}
