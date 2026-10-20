// COpenGLTexture::Download (engine library of SimTheme Park 2.04b3, the inventory's
// tb_printf_opengl.cpp range, 0x100465f8, 1,068 bytes) and COpenGLDrawPrimitive's destructor
// (0x10044ee8, 136 bytes).
//
// Download copies a texture entry's 16-bit image into the OpenGL page's upload buffer
// (gOpenGLTextureData, one shared 256 x 256 RGBA buffer) at the place the cache gave the entry;
// CTextureCache::UploadToVRAM calls it through the page's CTextureBase vtable, and
// CTextureLink::PageUpdated later sends the buffer to OpenGL (UnlockSurface). Each 4-bit
// channel of a pixel becomes the high nibble of a byte, so the buffer holds red, green, blue
// and alpha bytes in that order (the Mac stored the word 0xRRGGBBAA big-endian; this writes the
// bytes in the same order).
#include <cstdint>

#include "ppc_math.h"
#include "tb_decode_wavelet.h"
#include "tb_printf_opengl.h"
#include "tb_textureentry.h"

namespace {

// Flag the entry gets when its image is smaller than the room the cache gave it and was
// centred in it: UploadToVRAM then centres the texture coordinates the same way and clears it.
constexpr uint32_t kEntryCentred = 0x80000000;

// The pixel as four bytes of the upload buffer (the listing's rlwinm and rlwimi build the word
// 0xRRGGBBAA from the nibbles of 0xRGBA and store it big-endian).
void StorePixel(uint8_t* destination, uint16_t pixel) {
    const uint32_t word = ((pixel & 0xf000u) << 16) | ((pixel & 0x0f00u) << 12) | ((pixel & 0x00f0u) << 8) |
                          ((pixel & 0x000fu) << 4);
    destination[0] = static_cast<uint8_t>(word >> 24);
    destination[1] = static_cast<uint8_t>(word >> 16);
    destination[2] = static_cast<uint8_t>(word >> 8);
    destination[3] = static_cast<uint8_t>(word);
}

// A 32-bit signed product or sum (the Mac wraps) as a byte offset.
intptr_t Wrapped(uint32_t value) { return static_cast<intptr_t>(static_cast<int32_t>(value)); }

}  // namespace

int32_t COpenGLTexture::Download(CTextureEntry* entry, int32_t x, int32_t y) {
    int32_t cacheWidth = entry->mCacheWidth;
    int32_t cacheHeight = entry->mCacheHeight;
    const int32_t width = entry->mWidth;
    const int32_t height = entry->mHeight;

    // Stored upside down: by the entry's own flag, turned over once more by a compressed image
    // that unpacks upside down.
    bool flipped = (entry->mFlags & CTextureEntry::kFlipped) != 0;
    if ((entry->mFlags & CTextureEntry::kCompressed) != 0 &&
        static_cast<CCompBase*>(entry->mData)->IsNaturallyFlipped() != 0) {
        flipped = !flipped;
    }

    // An image smaller than its room in either direction is centred in the room, which is
    // cleared first.
    const bool narrower = width < cacheWidth;
    const bool shorter = height < cacheHeight;
    const bool clearFirst = narrower || shorter;

    const uint16_t* source = static_cast<const uint16_t*>(entry->GetData());

    // Pixels in a row of the buffer. The original divides without a check (bytes per pixel is
    // 4 once the page is created; a page that was never created has 0, which the PowerPC leaves
    // undefined); a 64-bit quotient also keeps the host clear of INT32_MIN / -1.
    const int32_t bytesPerPixel = mBytesPerPixel;
    const int32_t destinationStride =
        bytesPerPixel == 0 ? 0 : static_cast<int32_t>(static_cast<int64_t>(mPitch) / bytesPerPixel);
    uint8_t* destination = static_cast<uint8_t*>(gOpenGLTextureData) +
                           Wrapped(static_cast<uint32_t>(y) * static_cast<uint32_t>(mPitch) +
                                   static_cast<uint32_t>(x) * static_cast<uint32_t>(bytesPerPixel));

    // Pixels in a row of the image; a flipped image is read from its last row up.
    int32_t sourceStride = entry->mPitch;
    if (flipped) {
        source += Wrapped(static_cast<uint32_t>(height - 1) * static_cast<uint32_t>(sourceStride));
        sourceStride = -sourceStride;
    }
    int32_t sourceSkip = sourceStride - cacheWidth;
    int32_t destinationSkip = destinationStride - cacheWidth;

    if (clearFirst) {
        // The whole room, one word at a time (the original unrolls the row by eight; a width or
        // height below zero would make it write about 16 GB and is not reproduced).
        uint8_t* row = destination;
        for (int32_t rows = 0; rows < cacheHeight; ++rows) {
            for (int32_t column = 0; column < cacheWidth; ++column) {
                row[0] = row[1] = row[2] = row[3] = 0;
                row += 4;
            }
            row += Wrapped(static_cast<uint32_t>(destinationSkip) * 4u);
        }
    }

    if (narrower) {
        entry->mFlags |= kEntryCentred;
        destination += Wrapped(static_cast<uint32_t>((cacheWidth - width) / 2) * 4u);
        cacheWidth = width;
        destinationSkip = destinationStride - width;
        sourceSkip = sourceStride - width;
    }
    if (shorter) {
        entry->mFlags |= kEntryCentred;
        destination += Wrapped(static_cast<uint32_t>(destinationStride) *
                               static_cast<uint32_t>((cacheHeight - height) / 2) * 4u);
        cacheHeight = height;
    }

    if (width != cacheWidth || height != cacheHeight) {
        // Larger than its room: point-sampled down, the source position stepped in single
        // precision (width / room across, height / room down). The fractions carry from one row
        // to the next. Both steps are at least 1 whenever a row is drawn (a smaller image was
        // centred above, so its room is its own size), so the "not yet a whole pixel" paths
        // below cannot be taken; they are kept as the original has them.
        const float xStep = static_cast<float>(width) / static_cast<float>(cacheWidth);
        const float yStep = static_cast<float>(height) / static_cast<float>(cacheHeight);
        float xFraction = 0.0f;
        float yFraction = 0.0f;
        for (int32_t rows = 0; rows < cacheHeight; ++rows) {
            const uint16_t* rowStart = source;
            for (int32_t column = 0; column < cacheWidth; ++column) {
                xFraction = xFraction + xStep;
                StorePixel(destination, *source);
                destination += 4;
                if (xFraction >= 1.0f) {
                    const int32_t whole = ppc_fctiwz(xFraction);
                    source += whole;
                    xFraction = xFraction - static_cast<float>(whole);
                }
            }
            yFraction = yFraction + yStep;
            destination += Wrapped(static_cast<uint32_t>(destinationSkip) * 4u);
            if (yFraction >= 1.0f) {
                const int32_t whole = ppc_fctiwz(yFraction);
                source = rowStart + Wrapped(static_cast<uint32_t>(whole) * static_cast<uint32_t>(sourceStride));
                yFraction = yFraction - static_cast<float>(whole);
            }
        }
    } else {
        for (int32_t rows = 0; rows < cacheHeight; ++rows) {
            for (int32_t column = 0; column < cacheWidth; ++column) {
                StorePixel(destination, *source);
                ++source;
                destination += 4;
            }
            destination += Wrapped(static_cast<uint32_t>(destinationSkip) * 4u);
            source += sourceSkip;
        }
    }
    return 1;
}

// The original restores the vtable pointers of the object and of its virtual base, runs the
// base's destructor when it is the complete object (the flag in r4 is not zero) and frees the
// block when the flag is positive; the compiler does the same for this empty destructor.
COpenGLDrawPrimitive::~COpenGLDrawPrimitive() {}
