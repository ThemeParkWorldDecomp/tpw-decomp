// upixels.cpp: Sam's Utilities' pixel converter, translated from the Mac 2.04b3 shim
// library (sams utils shared, 0x1000d288..0x100112ac).
//
// Byte order. The Mac code reads and writes 16-bit and 32-bit pixels as whole words, so a
// buffer's bytes are those of big-endian words. Sources are file data (TGA pixels), which
// are bytes by nature, and are read as the original reads them. The 32-bit destination is
// bytes too (red, green, blue, alpha). The 16-bit RGBA destination (format 4) is the game's
// own texel format: the texture cache, the two wavelet decoders (Wavelet_decode*.cpp) and
// the OpenGL download (tb_opengl_texture_download.cpp) all handle it as HOST-ORDER 16-bit
// words, so this format reads and writes host words as well; its tests compare the buffers
// as words (test_UPixels.py swaps the original's bytes in pairs). Before 2026-10-11 it wrote
// big-endian bytes, which the download read swapped: every 16-bit texture that came through
// a TGA file (the lobby's sky cylinder among them) had its channels rotated, blue to yellow.
//
// Translated so far: the base class and the five formats the image loader uses (sources
// 16BGRA, 24BGR, 32BGRA; destinations 16RGBA, 32RGBA). The other seven formats select no
// implementation yet, and the per-format Convert(const Implementation&) (the path without
// `dither`, which the game never takes) is not translated. The port adds format 7, the
// postcard's (UPixels24RGB below).
#include "platform/upixels.h"

#include <cstdlib>
#include <cstring>

namespace SamsUtilities {

// 0x24 bytes on the Mac (0x28 for the two 8-bit formats). Mac offsets in comments.
class UPixelsImplementation {
public:
    // 0x1000d288
    UPixelsImplementation(uint8_t* data, uint32_t width, uint32_t height, uint32_t pitch, int32_t bytesPerPixel)
        : mWidth(width), mHeight(height), mPitch(pitch), mBytesPerPixel(bytesPerPixel) {
        mAlphaDefault = 0xff;   // SetAlphaDefault(0xff), 0x1000dc70
        mAlphaThreshold = 0;
        mFlipped = 0;
        mTransparentColour = 0;
        mHasTransparentColour = 0;
        if (pitch == 0xffffffffu) mPitch = width * static_cast<uint32_t>(bytesPerPixel);
        if (data == nullptr) {
            // MakeEmptyData, 0x1000d498: bytes per pixel * pitch * height (as written; the
            // original over-allocates by the factor of bytes per pixel).
            mData = static_cast<uint8_t*>(
                std::calloc(1, static_cast<size_t>(mBytesPerPixel) * mPitch * mHeight + 1));
            mOwnsData = 1;
        } else {
            mData = data;
            mOwnsData = 0;
        }
    }
    // 0x1000d380
    virtual ~UPixelsImplementation() {
        if (mOwnsData != 0 && mData != nullptr) {
            std::free(mData);
            mData = nullptr;
        }
    }
    // 0x1000d87c: vtable slot 0x10.
    int32_t Convert(const UPixelsImplementation& source, int32_t& alphaCount, uint8_t average[4]);
    virtual UPixelValue GetPixel(const uint8_t* p) const = 0;   // slot 0x18
    virtual void SetPixel(uint8_t* p, UPixelValue pixel) = 0;    // slot 0x1c

    uint8_t* mData;                    // 0x04
    uint32_t mWidth;                   // 0x08
    uint32_t mHeight;                  // 0x0c
    uint32_t mPitch;                   // 0x10
    int32_t mBytesPerPixel;            // 0x14
    uint8_t mOwnsData;                 // 0x18
    uint8_t mFlipped;                  // 0x19
    UPixelValue mTransparentColour;    // 0x1c
    uint8_t mHasTransparentColour;     // 0x20
    uint8_t mAlphaDefault;             // 0x21
    uint8_t mAlphaThreshold;           // 0x22
};

int32_t UPixelsImplementation::Convert(const UPixelsImplementation& source, int32_t& alphaCount,
                                       uint8_t average[4]) {
    uint8_t* dest = mData;
    const uint8_t* src = source.mData;
    if (dest == nullptr || src == nullptr || mHeight < source.mHeight || mWidth < source.mWidth) return 0;
    const uint32_t width = source.mWidth;
    int32_t alpha = 0, red = 0, green = 0, blue = 0;
    int32_t sourceRowStep;
    if (source.mFlipped == 0) {
        sourceRowStep = static_cast<int32_t>(source.mPitch) - source.mBytesPerPixel * static_cast<int32_t>(width);
    } else {
        sourceRowStep = -(static_cast<int32_t>(source.mPitch) + source.mBytesPerPixel * static_cast<int32_t>(width));
        src += static_cast<intptr_t>(static_cast<int32_t>((source.mHeight - 1) * source.mPitch));
    }
    const int32_t destRowStep = static_cast<int32_t>(mPitch) - mBytesPerPixel * static_cast<int32_t>(width);
    alphaCount = 0;
    for (uint32_t row = source.mHeight; row != 0; --row) {
        for (uint32_t column = width; column != 0; --column) {
            const UPixelValue pixel = source.GetPixel(src);
            if (static_cast<int32_t>(mAlphaThreshold) < static_cast<int32_t>(pixel >> 24)) ++alphaCount;
            alpha += static_cast<int32_t>(pixel >> 24);
            red += static_cast<int32_t>((pixel >> 16) & 0xff);
            green += static_cast<int32_t>((pixel >> 8) & 0xff);
            blue += static_cast<int32_t>(pixel & 0xff);
            SetPixel(dest, pixel);
            dest += mBytesPerPixel;
            src += source.mBytesPerPixel;
        }
        src += sourceRowStep;
        dest += destRowStep;
    }
    // Signed 32-bit divisions (divw). An empty source divides by zero, which the PowerPC
    // does without a trap and with an undefined quotient; the averages are zero here.
    const int32_t count = static_cast<int32_t>(source.mWidth * source.mHeight);
    average[0] = count != 0 ? static_cast<uint8_t>(alpha / count) : 0;
    average[1] = count != 0 ? static_cast<uint8_t>(red / count) : 0;
    average[2] = count != 0 ? static_cast<uint8_t>(green / count) : 0;
    average[3] = count != 0 ? static_cast<uint8_t>(blue / count) : 0;
    return 1;
}

namespace {
// The big-endian 16-bit word at p (what the Mac's `*(unsigned short*)p` reads).
uint32_t Word16(const uint8_t* p) { return static_cast<uint32_t>(p[0]) << 8 | p[1]; }
void SetWord16(uint8_t* p, uint32_t word) {
    p[0] = static_cast<uint8_t>(word >> 8);
    p[1] = static_cast<uint8_t>(word);
}

// The host's 16-bit word at p (the game's texel format: see the note at the top).
uint32_t HostWord16(const uint8_t* p) {
    uint16_t word;
    std::memcpy(&word, p, sizeof word);
    return word;
}
void SetHostWord16(uint8_t* p, uint32_t word) {
    const auto value = static_cast<uint16_t>(word);
    std::memcpy(p, &value, sizeof value);
}

// Format 4: four bits each of red, green, blue, alpha from the top of the word. Reading
// gives the high nibble of each channel (the low nibbles stay zero). Host-order words.
class UPixels16RGBA : public UPixelsImplementation {
public:
    using UPixelsImplementation::UPixelsImplementation;
    UPixelValue GetPixel(const uint8_t* p) const override {   // 0x1001091c
        const uint32_t w = HostWord16(p);
        return w << 28 | (w & 0xf0) | (w & 0xf000) << 8 | (w & 0xf00) << 4;
    }
    void SetPixel(uint8_t* p, UPixelValue pixel) override {   // 0x1001097c
        SetHostWord16(p, (pixel & 0xf0) | ((pixel >> 4) & 0xf00) | (pixel >> 28) | ((pixel >> 8) & 0xf000));
    }
};

// Format 5: blue, green, red, alpha from the top of the word.
class UPixels16BGRA : public UPixelsImplementation {
public:
    using UPixelsImplementation::UPixelsImplementation;
    UPixelValue GetPixel(const uint8_t* p) const override {   // 0x10010724
        const uint32_t w = Word16(p);
        return ((w >> 8) & 0xf0) | (w & 0xf00) << 4 | w << 28 | (w & 0xf0) << 16;
    }
    void SetPixel(uint8_t* p, UPixelValue pixel) override {   // 0x10010784
        const uint32_t high = (pixel >> 16) & 0xffff;
        SetWord16(p, ((pixel << 8) & 0xf000) | ((pixel >> 4) & 0xf00) | (high >> 12) | (high & 0xf0));
    }
};


// Format 8: three bytes blue, green, red (TGA order); the alpha is the default, or zero
// for the transparent colour when one is set. SetPixel writes red, green, blue: the
// reverse of what GetPixel reads, as in the original.
class UPixels24BGR : public UPixelsImplementation {
public:
    using UPixelsImplementation::UPixelsImplementation;
    UPixelValue GetPixel(const uint8_t* p) const override {   // 0x100102d4
        UPixelValue pixel = static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[2]) << 16 |
                            static_cast<uint32_t>(p[1]) << 8;
        if (mHasTransparentColour == 0 || pixel != mTransparentColour)
            pixel = static_cast<uint32_t>(mAlphaDefault) << 24 | (pixel & 0xffffff);
        return pixel;
    }
    void SetPixel(uint8_t* p, UPixelValue pixel) override {   // 0x10010364
        p[2] = static_cast<uint8_t>(pixel);
        p[1] = static_cast<uint8_t>(pixel >> 8);
        p[0] = static_cast<uint8_t>(pixel >> 16);
    }
};

// Format 10: bytes red, green, blue, alpha (what OpenGL takes as GL_RGBA, GL_UNSIGNED_BYTE).
class UPixels32RGBA : public UPixelsImplementation {
public:
    using UPixelsImplementation::UPixelsImplementation;
    UPixelValue GetPixel(const uint8_t* p) const override {   // 0x1000ff14
        return static_cast<uint32_t>(p[3]) << 24 | static_cast<uint32_t>(p[0]) << 16 |
               static_cast<uint32_t>(p[1]) << 8 | p[2];
    }
    void SetPixel(uint8_t* p, UPixelValue pixel) override {   // 0x1000ff6c
        p[0] = static_cast<uint8_t>(pixel >> 16);
        p[1] = static_cast<uint8_t>(pixel >> 8);
        p[2] = static_cast<uint8_t>(pixel);
        p[3] = static_cast<uint8_t>(pixel >> 24);
    }
};

// Format 11: bytes blue, green, red, alpha (TGA order).
class UPixels32BGRA : public UPixelsImplementation {
public:
    using UPixelsImplementation::UPixelsImplementation;
    UPixelValue GetPixel(const uint8_t* p) const override {   // 0x1000fd3c
        return static_cast<uint32_t>(p[3]) << 24 | static_cast<uint32_t>(p[2]) << 16 |
               static_cast<uint32_t>(p[1]) << 8 | p[0];
    }
    void SetPixel(uint8_t* p, UPixelValue pixel) override {   // 0x1000fd8c
        p[0] = static_cast<uint8_t>(pixel);
        p[1] = static_cast<uint8_t>(pixel >> 8);
        p[2] = static_cast<uint8_t>(pixel >> 16);
        p[3] = static_cast<uint8_t>(pixel >> 24);
    }
};

// The two formats the cursor pictures go through (InterfaceBFCursor::LoadData, src/game/ui/InterfaceBFCursor.cpp):
// NOT translations of the Mac's (0x28-byte 8-bit and 32ARGB implementations, untranslated and
// untested), only what reading those pictures needs. Format 9: bytes alpha, red, green, blue.
class UPixels32ARGB : public UPixelsImplementation {
public:
    using UPixelsImplementation::UPixelsImplementation;
    UPixelValue GetPixel(const uint8_t* p) const override {
        return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 |
               static_cast<uint32_t>(p[2]) << 8 | p[3];
    }
    void SetPixel(uint8_t* p, UPixelValue pixel) override {
        p[0] = static_cast<uint8_t>(pixel >> 24);
        p[1] = static_cast<uint8_t>(pixel >> 16);
        p[2] = static_cast<uint8_t>(pixel >> 8);
        p[3] = static_cast<uint8_t>(pixel);
    }
};

// Format 1: a byte an index into a palette of 256 format-9 pixels (the cursor loader converts the
// file's palette to that and hands the pointer as `palette`). GetPixel is the listing's (0x10011148): the
// palette's word, with the alpha default (0xff) put in the alpha byte unless the word is exactly the transparent
// colour, which is left as it is. The loader's palette has 0 for the fourth byte of each entry, so a magenta
// entry is exactly the cursor's transparent colour 0x00ff00ff and every other entry is made opaque. Nothing
// writes this format.
class UPixels8Indexed : public UPixelsImplementation {
public:
    UPixels8Indexed(uint8_t* data, uint32_t width, uint32_t height, uint32_t pitch, const uint8_t* palette)
        : UPixelsImplementation(data, width, height, pitch, 1), mPalette(palette) {}
    UPixelValue GetPixel(const uint8_t* p) const override {
        if (mPalette == nullptr) return static_cast<uint32_t>(mAlphaDefault) << 24;
        const uint8_t* entry = mPalette + static_cast<size_t>(*p) * 4;
        UPixelValue pixel = static_cast<uint32_t>(entry[0]) << 24 | static_cast<uint32_t>(entry[1]) << 16 |
                            static_cast<uint32_t>(entry[2]) << 8 | entry[3];
        if (mHasTransparentColour == 0 || pixel != mTransparentColour)
            pixel = static_cast<uint32_t>(mAlphaDefault) << 24 | (pixel & 0xffffff);
        return pixel;
    }
    void SetPixel(uint8_t*, UPixelValue) override {}
    const uint8_t* mPalette;
};

UPixelsImplementation* MakeImplementation(intptr_t format, uint8_t* bytes, uint32_t w, uint32_t h, uint32_t pitch,
                                          UPixel* palette) {
    switch (format) {
    case kPixels8Indexed: return new UPixels8Indexed(bytes, w, h, pitch, reinterpret_cast<const uint8_t*>(palette));
    case kPixels16RGBA: return new UPixels16RGBA(bytes, w, h, pitch, 2);
    case kPixels16BGRA: return new UPixels16BGRA(bytes, w, h, pitch, 2);
    case kPixels24BGR: return new UPixels24BGR(bytes, w, h, pitch, 3);
    case kPixels32ARGB: return new UPixels32ARGB(bytes, w, h, pitch, 4);
    case kPixels32RGBA: return new UPixels32RGBA(bytes, w, h, pitch, 4);
    case kPixels32BGRA: return new UPixels32BGRA(bytes, w, h, pitch, 4);
    default: return nullptr;   // above 11: none in the original either; 0, 2, 3, 6: not translated yet (7 too, without the port fixes)
    }
}
}  // namespace

// 0x1000eb88: the constructor is SetData (0x1000ee3c) on a fresh object.
UPixels::UPixels(intptr_t format, void* data, int32_t width, int32_t height, int32_t stride, UPixel* palette)
    : mImplementation(nullptr), mAlphaCount(0), mAverage{0, 0, 0, 0} {
    SetData(format, data, width, height, stride, palette);
}

UPixels::UPixels() : mImplementation(nullptr), mAlphaCount(0), mAverage{0, 0, 0, 0} {}

// A buffer of its own (the implementation makes one when the data is NULL), rows packed.
UPixels::UPixels(intptr_t format, int32_t width, int32_t height, UPixel* palette)
    : mImplementation(nullptr), mAlphaCount(0), mAverage{0, 0, 0, 0} {
    SetData(format, nullptr, width, height, -1, palette);
}

void UPixels::SetData(intptr_t format, void* data, int32_t width, int32_t height, int32_t stride, UPixel* palette) {
    delete mImplementation;
    mImplementation = MakeImplementation(format, static_cast<uint8_t*>(data), static_cast<uint32_t>(width),
                                         static_cast<uint32_t>(height), static_cast<uint32_t>(stride), palette);
}

void* UPixels::GetData() { return mImplementation != nullptr ? mImplementation->mData : nullptr; }

UPixels::~UPixels() { delete mImplementation; }

// The picture as rows of red, green, blue, alpha bytes, top row first, for the platform's cursor
// (platform/cursor.cpp): the implementation's own reading of each pixel, with the transparent colour
// (when one is set) made fully transparent for the formats whose reading does not do that itself.
void UPixelsReadRGBA(UPixels& pixels, uint8_t* out, int32_t width, int32_t height) {
    const UPixelsImplementation* source = pixels.mImplementation;
    for (int32_t row = 0; row < height; ++row) {
        for (int32_t column = 0; column < width; ++column) {
            UPixelValue pixel = 0;
            if (source != nullptr && source->mData != nullptr && static_cast<uint32_t>(row) < source->mHeight &&
                static_cast<uint32_t>(column) < source->mWidth) {
                const uint8_t* at = source->mData + static_cast<size_t>(row) * source->mPitch +
                                    static_cast<size_t>(column) * static_cast<size_t>(source->mBytesPerPixel);
                pixel = source->GetPixel(at);
                if (source->mHasTransparentColour != 0 && (pixel & 0xffffff) == (source->mTransparentColour & 0xffffff))
                    pixel &= 0xffffff;
            }
            out[0] = static_cast<uint8_t>(pixel >> 16);
            out[1] = static_cast<uint8_t>(pixel >> 8);
            out[2] = static_cast<uint8_t>(pixel);
            out[3] = static_cast<uint8_t>(pixel >> 24);
            out += 4;
        }
    }
}

// The original stores through the implementation pointer without a check (on the Mac a
// NULL one scribbles on low memory); the checks here only keep an unknown format harmless.
void UPixels::SetAlphaDefault(uint8_t alpha) {
    if (mImplementation != nullptr) mImplementation->mAlphaDefault = alpha;
}

void UPixels::SetAlphaThreshold(uint8_t alpha) {
    if (mImplementation != nullptr) mImplementation->mAlphaThreshold = alpha;
}

void UPixels::SetFlipped(uint8_t flipped) {
    if (mImplementation != nullptr) mImplementation->mFlipped = flipped;
}

void UPixels::SetTransparentColour(UPixelValue colour) {
    if (mImplementation == nullptr) return;
    mImplementation->mTransparentColour = colour;
    mImplementation->mHasTransparentColour = 1;
}

void UPixels::Convert(UPixels& source, bool dither) {
    if (mImplementation == nullptr || source.mImplementation == nullptr) return;
    // Without `dither` the original calls the format's own Convert (slot 0xc, untranslated), a
    // plain copy; the cursor loader's palette goes through it. Here it is the same copy with the
    // counts dropped.
    if (dither) {
        mImplementation->Convert(*source.mImplementation, mAlphaCount, mAverage);
    } else {
        int32_t count = 0;
        uint8_t average[4] = {0, 0, 0, 0};
        mImplementation->Convert(*source.mImplementation, count, average);
    }
}

void UPixels::GetAverages(uint8_t* red, uint8_t* green, uint8_t* blue, uint8_t* alpha) {
    *alpha = mAverage[0];
    *red = mAverage[1];
    *green = mAverage[2];
    *blue = mAverage[3];
}

int32_t UPixels::GetAlphaCount() { return mAlphaCount; }

}  // namespace SamsUtilities
