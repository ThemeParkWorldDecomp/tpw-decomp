// OpenGLTexture.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of OpenGLTexture code, named after it.

#include "platform/indirectx.h"
#include "platform/opengl.h"
#include "ppc_math.h"
#include "tb_ddraw.h"
#include "tb_decode_wavelet.h"
#include "tb_error.h"
#include "tb_printf_opengl.h"
#include "tb_profiler.h"
#include "tb_resource_display.h"
#include "tb_resource_texture.h"
#include "tb_textureentry.h"
#include <cstdint>
#include <new>

// OpenGL start-up, Mac 2.04b3. Log contexts identify tb_init_opengl.cpp;
// these functions are filed with tb_printf_opengl.cpp in the inventory.

SamsUtilities::UMemoryAlignedBase* gOpenGLTextureMemory = nullptr;
void* gOpenGLTextureData = nullptr;
void COpenGLTexture::Initialise() {
    if (!gOpenGLTextureMemory) {
        // Allocate the block before creating the default allocator, as on the Mac.
        auto* storage = static_cast<SamsUtilities::UMemoryAlignedBase*>(
            ::operator new(sizeof(SamsUtilities::UMemoryAlignedBase)));
        if (!SamsUtilities::gMemory) SamsUtilities::gMemory = new SamsUtilities::UAllocator;
        new (storage) SamsUtilities::UMemoryAlignedBase(0x10000, 0x10, 4, SamsUtilities::gMemory);
        gOpenGLTextureMemory = storage;
        gOpenGLTextureData = storage->mData;
    }
}
void COpenGLTexture::Cleanup() {
    if (gOpenGLTextureMemory) {
        delete gOpenGLTextureMemory;
        gOpenGLTextureMemory = nullptr;
        gOpenGLTextureData = nullptr;
    }
}

// The OpenGL renderer's state keeping (engine library of SimTheme Park 2.04b3, in the
// inventory's tb_printf_opengl.cpp range): the rest of COpenGL's interface, the draw
// primitive's start-up and its blending, texture and fog setters, and the texture page.
// The constructors and start-up entry points are in tb_printf_opengl.cpp. PARTIAL: the
// draw primitive's drawing members (Execute, DrawIndexedPrimitive, the boxes and polys,
// GetPixel, GetPixelFillRate) and COpenGLTexture::Download are still to do.

// The destructor (0x10044ee8) is in src/tb_opengl_texture_download.cpp.

// ---- COpenGLTexture ---------------------------------------------------------------------
COpenGLTexture::COpenGLTexture() {
    mTexture = 0;
    mMipmaps = 0;
    mBytesPerPixel = 0;
    mPitch = 0;
    mWidth = 0;
    mHeight = 0;
    mMinFilter = GL_NEAREST;
    mState = 0;
}

COpenGLTexture::~COpenGLTexture() { Destroy(); }

// Sixteen lock and unlock rounds timed: bytes sent per clock tick.
double COpenGLTexture::TestUploadSpeed() {
    const int32_t start = gProfiler.GetClockCycleCount10h();
    const int32_t width = mWidth;
    const int32_t height = mHeight;
    for (int32_t i = 0; i < 16; ++i) {
        LockSurface();
        UnlockSurface();
    }
    const int32_t end = gProfiler.GetClockCycleCount10h();
    return static_cast<double>(static_cast<float>(width * height * 0x10) /
                               static_cast<float>(static_cast<uint32_t>((end - start) * 0x10)));
}

int32_t COpenGLTexture::Create(int32_t width, int32_t height, int32_t format, int32_t mipmaps) {
    const CTextureSettings& settings = gei_TextureSettings;
    if ((settings.mFlags & 2) == 0) mipmaps = 0;
    mWidth = width;
    mHeight = height;
    mMipmaps = mipmaps;
    mBytesPerPixel = 4;
    mPitch = width * mBytesPerPixel;
    glGenTextures(1, &mTexture);
    if (glGetError() != 0) return 0;
    glBindTexture(GL_TEXTURE_2D, mTexture);
    if (glGetError() != 0) {
        glDeleteTextures(1, &mTexture);
        mTexture = 0;
        return 0;
    }
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, 8448.0f);   // GL_MODULATE
    if (mipmaps < 1) mMinFilter = GL_LINEAR;
    else if ((settings.mFlags & 8) != 0) mMinFilter = GL_LINEAR_MIPMAP_LINEAR;
    else mMinFilter = GL_LINEAR_MIPMAP_NEAREST;
    mState = kStateNearest | kStateNoMipmaps;
    SetState(kStateRepeat);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, gOpenGLTextureData);
    return 1;
}

int32_t COpenGLTexture::Destroy() {
    if (mTexture != 0) {
        glDeleteTextures(1, &mTexture);
        mTexture = 0;
    }
    return glGetError() == 0;
}

void COpenGLTexture::Select(int32_t state) {
    glBindTexture(GL_TEXTURE_2D, mTexture);
    if (glGetError() == 0) SetState(state);
    else glDisable(GL_TEXTURE_2D);
}

// Only what differs from the state the texture was last given is sent.
void COpenGLTexture::SetState(int32_t state) {
    const uint32_t changed = static_cast<uint32_t>(mState ^ state);
    if (changed == 0) return;
    if ((changed & kStateNearest) != 0) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (state & kStateNearest) == 0 ? GL_LINEAR : GL_NEAREST);
    }
    if ((changed & (kStateNearest | kStateNoMipmaps)) != 0) {
        if ((state & kStateNearest) != 0) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        else if ((state & kStateNoMipmaps) != 0) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        else glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mMinFilter);
    }
    if ((changed & kStateRepeat) != 0) {
        const GLint wrap = (state & kStateRepeat) == 0 ? GL_CLAMP : GL_REPEAT;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    }
    mState = state;
}

int32_t COpenGLTexture::LockSurface() { return gOpenGLTextureData != nullptr; }

int32_t COpenGLTexture::UnlockSurface() {
    if (gOpenGLTextureData == nullptr || mTexture == 0) return 0;
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (mMipmaps == 0) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, gOpenGLTextureData);
    } else {
        gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA4, mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, gOpenGLTextureData);
    }
    return glGetError() == 0;
}

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

int32_t COpenGLTexture::MipmapsUsed() { return mMipmaps; }

int32_t COpenGLTexture::GetPixelCount() { return mWidth * mHeight; }

void COpenGLTexture::GetTexture() {}
