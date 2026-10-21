// Postcard.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of Postcard code, named after it.

#include "HFieldTextures.h"
#include "lobby.h"
#include "m3d2.h"
#include "platform/opengl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "Postcard.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_resource_display.h"
#include "TbGraphicFile.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPMain.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// The postcard composer (main app, rsse.cpp range 0x100bf7c4..0x100c0c20, 14 functions; Postcard::save at 0x100c0c20 and the
// postcard panel are not here). See include/Postcard.h for the classes.
//
// The program's TOC strings used here (data at 0x1023ed83..): the two assertion texts of copy_from_opengl at +0 and +0x28
// (the first, "couldn't reallocate buffer for postcard", is never reported: see copy_from_opengl; the second says "2nd
// buffer"), the empty path prefix at +0x54 and the five picture names at +0x55, +0x6f, +0x8a, +0xa2 and +0xba. Postcard::save's
// two (the file mode and the error text at +0xd2 and +0xd5) are not used.

namespace {

constexpr char kSecondBufferText[] = "couldn't reallocate 2nd buffer for postcard";          // 0x1023edab (+0x28)
constexpr char kPathPrefix[] = "";                                                            // 0x1023edd7 (+0x54)
constexpr char kLogoName[] = "data:postcard:tpwlogo.tga";                                    // 0x1023edd8
constexpr char kBullfrogName[] = "data:postcard:bullfrog.tga";                               // 0x1023edf2
constexpr char kStampName[] = "data:postcard:stamp.tga";                                     // 0x1023ee0d
constexpr char kFrankName[] = "data:postcard:frank.tga";                                     // 0x1023ee25
constexpr char kLegalName[] = "data:postcard:legal.tga";                                     // 0x1023ee3d

// The two result codes of the loader (this file's own copies of the library's constants, data at 0x1029ebd4 and 0x1029ebd8; -1
// each, the file image's value; ui_cursors.cpp has the same pair at 0x102b155c and 0x102b1560).
constexpr int32_t kReadHeaderFailed = -1;   // 0x1029ebd4
constexpr int32_t kLoadFailed = -1;         // 0x1029ebd8

// The test of the loader's answers (as src/ui_cursors.cpp): failed when the sign of the answer is the one of the constant's.
bool LoaderFailed(int32_t answer, int32_t constant) { return (answer >= 0) == (constant == 0); }

// operator new[] that answers NULL when the memory is not there (the Mac's does; the callers test for it).
uint8_t* AllocateBytes(uint32_t size) { return new (std::nothrow) uint8_t[size]; }

// OpenGL names (not in platform/opengl.h's list).
constexpr GLenum kGLFront = 0x404;
constexpr GLenum kGLBack = 0x405;

// The card and the grab (copy_from_opengl scales to this size).
constexpr uint32_t kCardWidth = 0x280;
constexpr uint32_t kCardHeight = 0xf0;
constexpr uint32_t kCardBytes = 0x96000;
constexpr int32_t kGrabWidth = 0x140;
constexpr int32_t kGrabHeight = 0xf0;
constexpr uint32_t kGrabBytes = 0x4b000;

// Unsigned divide (divwu). The PowerPC leaves the quotient undefined for a divisor of 0; the emulator the tests compare with
// answers the dividend, and so does this (the original only divides by 0 when it scales a bitmap up: a run of 0 source pixels).
uint32_t DivideUnsigned(uint32_t dividend, uint32_t divisor) { return divisor == 0 ? dividend : dividend / divisor; }

// The temporary allocator, made on first use (operator new(8) and its constructor, stored in the import gMemoryTemporary).
SamsUtilities::UAllocatorTemporary& TemporaryAllocator() {
    if (SamsUtilities::gMemoryTemporary == nullptr) SamsUtilities::gMemoryTemporary = new SamsUtilities::UAllocatorTemporary;
    return *SamsUtilities::gMemoryTemporary;
}

}  // namespace

// ============================================================ PostcardBitmap

// The product is bytes a pixel x width x height, in that order (32-bit wrap).
PostcardBitmap::PostcardBitmap(uint32_t width, uint32_t height, uint32_t bytesPerPixel)
    : mWidth(width), mHeight(height), mBytesPerPixel(bytesPerPixel) {
    mPixels = AllocateBytes(static_cast<uint32_t>(mBytesPerPixel * mWidth * mHeight));
}

PostcardBitmap::~PostcardBitmap() { clear(); }

void PostcardBitmap::clear() {
    mWidth = 0;
    mHeight = 0;
    mBytesPerPixel = 0;
    if (mPixels != nullptr) delete[] mPixels;
    mPixels = nullptr;
}

int32_t PostcardBitmap::load_from_tga(const char* path, const char* name) {
    if (path == nullptr) return 0;
    TbTGALoader loader;
    TbRenderArea area;
    char buffer[256];
    std::strcpy(buffer, path);
    std::strcat(buffer, name);
    TbResult result = loader.ReadHeader(buffer, nullptr, &area);
    if (LoaderFailed(result.mCode, kReadHeaderFailed)) return 0;
    area.AllocMemory();
    result = loader.Load(area, buffer, nullptr, nullptr);
    if (LoaderFailed(result.mCode, kLoadFailed)) {
        area.FreeMemory();
        return 0;
    }
    // The area's palettised word (+0x14) says 1 for an 8-bit picture; anything else is taken for 32 bits a pixel.
    const bool fullColour = area.mPalettised != 1;
    if (mPixels != nullptr) delete[] mPixels;
    const uint32_t width = static_cast<uint32_t>(area.mWidth);
    const uint32_t height = static_cast<uint32_t>(area.mPictureHeight);
    const uint32_t bytes = fullColour ? width * height * 4 : width * height;
    mPixels = AllocateBytes(bytes);
    uint8_t* dest = mPixels;
    if (dest == nullptr) return 0;   // the old width, height and bytes a pixel stay, and the area is not freed
    const uint8_t* source = area.mData;
    if (fullColour) {
        // The file's bytes come in the opposite order to the bitmap's: reversed.
        for (uint32_t row = 0; row < height; ++row) {
            for (uint32_t column = 0; column < width; ++column) {
                dest[0] = source[3];
                dest[1] = source[2];
                dest[2] = source[1];
                dest[3] = source[0];
                source += 4;
                dest += 4;
            }
        }
    } else {
        std::memcpy(dest, source, bytes);
    }
    mWidth = width;
    mHeight = height;
    mBytesPerPixel = fullColour ? 4 : 1;
    area.FreeMemory();
    return 1;
}

// The first check of the original (the allocation's answer, then a UDebug that needs the pointer to be non-NULL after it was
// found to be NULL) never reports anything, so a failed allocation goes on to read the screen into a NULL buffer.
int32_t PostcardBitmap::copy_from_opengl() {
    const int32_t width = gei_Display.mWidth;
    const int32_t height = gei_Display.mHeight;
    if (mPixels != nullptr) delete[] mPixels;
    mPixels = AllocateBytes(static_cast<uint32_t>(width * height * 4));
    if (mPixels != nullptr) {
        mWidth = static_cast<uint32_t>(width);
        mHeight = static_cast<uint32_t>(height);
        mBytesPerPixel = 4;
    }
    glReadBuffer(kGLFront);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, mPixels);
    glReadBuffer(kGLBack);
    glDrawBuffer(kGLBack);
    uint8_t* buffer = TemporaryAllocator().Allocate(kGrabBytes);
    if (buffer == nullptr) return 0;
    resize_into_buffer(buffer, kGrabWidth, kGrabHeight, 3);
    if (mPixels != nullptr) delete[] mPixels;
    mPixels = AllocateBytes(kGrabBytes);
    if (mPixels == nullptr) {
        SamsUtilities::UDebug(kSecondBufferText);
    } else {
        mWidth = kGrabWidth;
        mHeight = kGrabHeight;
        mBytesPerPixel = 4;
        // The grab is upside down (OpenGL's rows start at the bottom): the converter reads the source flipped.
        SamsUtilities::UPixels source(SamsUtilities::kPixels32ARGB, buffer, kGrabWidth, kGrabHeight, -1, nullptr);
        SamsUtilities::UPixels dest(SamsUtilities::kPixels32ARGB, mPixels, kGrabWidth, kGrabHeight, -1, nullptr);
        source.SetFlipped(1);
        dest.Convert(source, false);
    }
    TemporaryAllocator().Free(buffer);
    return 1;
}

// The destination pointer is worked out in 32-bit arithmetic (bytes a pixel x (x + width x y)), and the step to the next row
// (bytes a pixel x (dest width less this width)) may be negative: nothing is clipped or tested.
void PostcardBitmap::alphablt(PostcardBitmap& dest, uint32_t x, uint32_t y) {
    const uint8_t* source = mPixels;
    uint8_t* target = dest.mPixels + static_cast<uint32_t>(dest.mBytesPerPixel * (x + dest.mWidth * y));
    for (uint32_t row = 0; row < mHeight; ++row) {
        for (uint32_t column = 0; column < mWidth; ++column) {
            const uint32_t alpha = source[0];
            const uint32_t first = source[1];
            const uint32_t second = source[2];
            const uint32_t third = source[3];
            source += 4;
            target[0] = 0xff;
            // The product is taken as a signed word and shifted right (srawi): only the low byte is kept.
            target[1] = static_cast<uint8_t>(target[1] + (static_cast<int32_t>(alpha * (first - target[1])) >> 8));
            target[2] = static_cast<uint8_t>(target[2] + (static_cast<int32_t>(alpha * (second - target[2])) >> 8));
            target[3] = static_cast<uint8_t>(target[3] + (static_cast<int32_t>(alpha * (third - target[3])) >> 8));
            target += 4;
        }
        target += static_cast<int32_t>(dest.mBytesPerPixel * (dest.mWidth - mWidth));
    }
}

// A box filter. Each output pixel covers a run of source pixels across and a run of source rows down, the runs worked out as
// differences of 16.16 accumulators (a step of (size * 0x10000 + 0x8000) / output size) kept in one byte each (a run of 256
// or more wraps). Every output row averages its rows' runs across into a row buffer, then the buffer's rows down. The
// averages are (sum & 0xffff) / count per channel. The formats: 1 writes the bytes channel 1, 2, 3 and 0xff (the first
// channel is dropped), 2 a 4444 halfword of channels 1, 2 and 3 with 0xf, 3 the bytes 0xff and channels 0, 1 and 2 (the last
// channel is dropped); both words are written the way the Mac's stores lay them (high byte first). Any other format writes
// nothing and `out` does not move. A buffer that cannot be had answers 0 and keeps the ones already made.
int32_t PostcardBitmap::resize_into_buffer(uint8_t* out, int32_t width, int32_t height, int32_t format) {
    const int32_t sourceWidth = static_cast<int32_t>(mWidth);
    const uint8_t* source = mPixels;
    const uint32_t yStep = DivideUnsigned(static_cast<uint32_t>(static_cast<int32_t>(mHeight) * 0x10000 + 0x8000),
                                          static_cast<uint32_t>(height));
    const uint32_t xStep = DivideUnsigned(static_cast<uint32_t>(sourceWidth * 0x10000 + 0x8000), static_cast<uint32_t>(width));
    uint8_t* rowBuffer = AllocateBytes(((yStep >> 16) + 1) * static_cast<uint32_t>(width * 4));
    if (rowBuffer == nullptr) return 0;
    uint8_t* xRuns = AllocateBytes(static_cast<uint32_t>(width));
    uint8_t* yRuns = AllocateBytes(static_cast<uint32_t>(height));
    if (xRuns == nullptr || yRuns == nullptr) return 0;
    uint32_t accumulator = 0;
    for (int32_t i = 0; i < width; ++i) {
        const uint32_t next = accumulator + xStep;
        xRuns[i] = static_cast<uint8_t>((next >> 16) - (accumulator >> 16));
        accumulator = next;
    }
    accumulator = 0;
    for (int32_t i = 0; i < height; ++i) {
        const uint32_t next = accumulator + yStep;
        yRuns[i] = static_cast<uint8_t>((next >> 16) - (accumulator >> 16));
        accumulator = next;
    }
    for (int32_t y = 0; y < height; ++y) {
        const uint32_t rows = yRuns[y];
        uint8_t* line = rowBuffer;
        for (uint32_t row = 0; row < rows; ++row) {
            for (int32_t x = 0; x < width; ++x) {
                const uint32_t count = xRuns[x];
                uint32_t sum0 = 0;
                uint32_t sum1 = 0;
                uint32_t sum2 = 0;
                uint32_t sum3 = 0;
                for (uint32_t i = 0; i < count; ++i) {
                    sum0 += source[0];
                    sum1 += source[1];
                    sum2 += source[2];
                    sum3 += source[3];
                    source += 4;
                }
                line[0] = static_cast<uint8_t>(DivideUnsigned(sum0 & 0xffff, count));
                line[1] = static_cast<uint8_t>(DivideUnsigned(sum1 & 0xffff, count));
                line[2] = static_cast<uint8_t>(DivideUnsigned(sum2 & 0xffff, count));
                line[3] = static_cast<uint8_t>(DivideUnsigned(sum3 & 0xffff, count));
                line += 4;
            }
        }
        for (int32_t x = 0; x < width; ++x) {
            const uint8_t* column = rowBuffer + x * 4;
            uint32_t sum0 = 0;
            uint32_t sum1 = 0;
            uint32_t sum2 = 0;
            uint32_t sum3 = 0;
            for (uint32_t i = 0; i < rows; ++i) {
                sum0 += column[0];
                sum1 += column[1];
                sum2 += column[2];
                sum3 += column[3];
                column += width * 4;
            }
            const uint32_t average0 = DivideUnsigned(sum0 & 0xffff, rows) & 0xffff;
            const uint32_t average1 = DivideUnsigned(sum1 & 0xffff, rows) & 0xffff;
            const uint32_t average2 = DivideUnsigned(sum2 & 0xffff, rows) & 0xffff;
            const uint32_t average3 = DivideUnsigned(sum3 & 0xffff, rows) & 0xffff;
            if (format == 1) {
                const uint32_t word = (average1 << 24) | (average2 << 16) | ((average3 << 8) & 0x00ffff00u) | 0xff;
                out[0] = static_cast<uint8_t>(word >> 24);
                out[1] = static_cast<uint8_t>(word >> 16);
                out[2] = static_cast<uint8_t>(word >> 8);
                out[3] = static_cast<uint8_t>(word);
                out += 4;
            } else if (format == 2) {
                const uint32_t half = ((average2 << 4) & 0xf00) | ((average1 << 8) & 0xf000) | (average3 & 0xf0) | 0xf;
                out[0] = static_cast<uint8_t>(half >> 8);
                out[1] = static_cast<uint8_t>(half);
                out += 2;
            } else if (format == 3) {
                out[0] = 0xff;
                out[1] = static_cast<uint8_t>(average0);
                out[2] = static_cast<uint8_t>(average1);
                out[3] = static_cast<uint8_t>(average2);
                out += 4;
            }
        }
    }
    delete[] yRuns;
    delete[] xRuns;
    delete[] rowBuffer;
    return 1;
}

// ============================================================ Postcard

Postcard::Postcard() { clear(); }

Postcard::~Postcard() {}

void Postcard::clear() {
    mLogo.clear();
    mBullfrog.clear();
    mStamp.clear();
    mFrank.clear();
    mLegal.clear();
    mCard.clear();
    mGrab.clear();
}

int32_t Postcard::load_sources() {
    mLogo.load_from_tga(kPathPrefix, FindFilePath(kLogoName, kFileReturnName, nullptr));
    mBullfrog.load_from_tga(kPathPrefix, FindFilePath(kBullfrogName, kFileReturnName, nullptr));
    mStamp.load_from_tga(kPathPrefix, FindFilePath(kStampName, kFileReturnName, nullptr));
    mFrank.load_from_tga(kPathPrefix, FindFilePath(kFrankName, kFileReturnName, nullptr));
    mLegal.load_from_tga(kPathPrefix, FindFilePath(kLegalName, kFileReturnName, nullptr));
    if (mCard.mPixels != nullptr) delete[] mCard.mPixels;
    mCard.mPixels = AllocateBytes(kCardBytes);
    if (mCard.mPixels != nullptr) {   // otherwise the card keeps its old size words
        mCard.mWidth = kCardWidth;
        mCard.mHeight = kCardHeight;
        mCard.mBytesPerPixel = 4;
    }
    mGrab.clear();
    return 1;
}

int32_t Postcard::grab(_DDSURFACEDESC2&) {
    mGrab.copy_from_opengl();
    return 1;
}

namespace {
// The card's background (0xff, 0xfb, 0xf8, 0xc6 a pixel over its width x height) and the six pictures over it, the grab
// first. Generate and the preview's (dead) branch are the same code.
void Compose(Postcard& card, uint8_t* pixels) {
    for (uint32_t i = 0; i < card.mCard.mWidth * card.mCard.mHeight; ++i) {
        pixels[0] = 0xff;
        pixels[1] = 0xfb;
        pixels[2] = 0xf8;
        pixels[3] = 0xc6;
        pixels += 4;
    }
    card.mGrab.alphablt(card.mCard, 0, 0);
    card.mStamp.alphablt(card.mCard, 0x220, 0);
    card.mFrank.alphablt(card.mCard, 0x1dd, 6);
    card.mLegal.alphablt(card.mCard, 0x141, 0xda);
    card.mBullfrog.alphablt(card.mCard, 4, 0x9f);
    card.mLogo.alphablt(card.mCard, 0xb6, 0);
}
}  // namespace

void Postcard::generate() { Compose(*this, mCard.mPixels); }

// Quirk kept: the composing runs only when the card has NO pixels, and then starts from address 0 (a crash on the Mac and
// here); with pixels it only scales the card.
void Postcard::generate_preview_into_buffer(uint8_t* out, int32_t width, int32_t height, int32_t format) {
    if (mCard.mPixels == nullptr) Compose(*this, nullptr);
    mCard.resize_into_buffer(out, width, height, format);
}

// Unit v5906 (main app, file rsse.cpp): Postcard::save 0x100c0c20 (the card as a JPEG), SetWorldBoundingArea 0x100b9180 (a header's box
// copied to another and carried by the second's node matrix) and SetHeightfieldTexture 0x100b8080 (sets the texture and flags of one height
// field quad).

namespace {
const char kModeWrite[] = "wb";                    // 0x1023ee55
const char kCantOpen[] = "can't open %s\n";        // 0x1023ee58
}  // namespace

// 0x100c0c20. The card (format 9, 4 bytes a pixel) is converted to 24 bit RGB (format 7) and written as a JPEG of quality 75. A file
// that does not open is reported on stderr and the program exits (the original's stderr is entry 2 of the C library's file table, +0xa8).
int32_t Postcard::save(char* path) {
    SamsUtilities::UPixels source(SamsUtilities::kPixels32ARGB, mCard.mPixels, mCard.mWidth, mCard.mHeight, -1, nullptr);
    SamsUtilities::UPixels dest(SamsUtilities::kPixels24RGB, mCard.mWidth, mCard.mHeight, nullptr);
    dest.Convert(source, false);
    uint8_t* data = static_cast<uint8_t*>(dest.GetData());

    jpeg_error_mgr errorManager;
    jpeg_compress_struct cinfo;
    cinfo.mErr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(jpeg_std_error(&errorManager)));
    jpeg_CreateCompress(&cinfo, 0x3e, 0x174);
    std::FILE* file = std::fopen(path, kModeWrite);
    if (file == nullptr) {
        std::fprintf(stderr, kCantOpen, path);
        std::exit(1);
    }
    jpeg_stdio_dest(&cinfo, file);
    cinfo.mImageWidth = mCard.mWidth;
    cinfo.mImageHeight = mCard.mHeight;
    cinfo.mInputComponents = 3;
    cinfo.mInColorSpace = 2;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 0x4b, 1);
    jpeg_start_compress(&cinfo, 1);
    uint32_t stride = mCard.mWidth * 3;
    while (cinfo.mNextScanline < cinfo.mImageHeight) {
        uint8_t* row = data + static_cast<uint32_t>(cinfo.mNextScanline * stride);
        jpeg_write_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_compress(&cinfo);
    std::fclose(file);
    jpeg_destroy_compress(&cinfo);
    return 1;
}
