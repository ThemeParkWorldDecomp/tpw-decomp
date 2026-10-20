// tb_capture.cpp (decompiled from the engine library of SimTheme Park 2.04b3, functions at
// 0x10002ddc..0x10003424).
//
// Byte order: this is PC code that writes little-endian file data with native stores (the
// TGA header's 16-bit width and height, and the 32-bit words that ConvertCapture stores
// three bytes apart). The translation keeps the native stores, so a little-endian host
// writes a valid TGA (BGR pixels). The Mac build stored the same values big-endian, so its
// screenshots have byte-swapped header sizes and shifted channels; the differential test
// compares the stored values, not the Mac's bytes.
#include "tb_capture.h"

#include <cstdio>
#include <cstring>

#include "Wavelet.h"
#include "platform/indirectx.h"
#include "ppc_math.h"
#include "tb_ddraw.h"
#include "tb_error.h"
#include "tb_resource_display.h"

CCapture gei_Capture;

int32_t CCapture::SaveUncompressedTGA(char* name) {
    CDisplay& d = gei_Display;
    // Uncompressed true-colour image (type 2), 24 bits per pixel, bottom-left origin.
    uint8_t header[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 0};
    int16_t width = static_cast<int16_t>(d.mWidth);
    int16_t height = static_cast<int16_t>(d.mHeight);
    std::memcpy(header + 12, &width, 2);
    std::memcpy(header + 14, &height, 2);
    FILE* file = std::fopen(name, "w+b");
    if (file == nullptr) {
        gei_ErrorHandler.Push(2, kErrFileOpen);
        return 0;
    }
    std::fwrite(header, 1, 18, file);
    for (int32_t y = 0; y < d.mHeight; y++) {
        int32_t row = d.mWidth * ((d.mHeight - 1) - y) * 3;   // the bottom row first
        std::fwrite(static_cast<uint8_t*>(mRGB) + row, 3, static_cast<uint32_t>(d.mWidth), file);
    }
    std::fclose(file);
    return 1;
}

int32_t CCapture::ConvertCapture() {
    CDisplay& d = gei_Display;
    switch (d.mBitsPerPixel) {
    case 16: {
        // + 1: the last pixel's 4-byte store ends one byte past 3 * pixels.
        mRGB = gMalloc(static_cast<uint32_t>(mPixels * 3 + 1));
        if (mRGB == nullptr) {
            gFree(mScreen);
            gei_ErrorHandler.Push(2, kErrOutOfMemory);
            return 0;
        }
        // Each channel moves so that its top bit lands on bit 23 (red), 15 (green) or 7
        // (blue); the word is stored at 3 * i and the next pixel's store overwrites its
        // top byte.
        int32_t redShift = (24 - d.mRedBits.mShift) - d.mRedBits.mBits;
        int32_t greenShift = (16 - d.mGreenBits.mShift) - d.mGreenBits.mBits;
        int32_t blueShift = (8 - d.mBlueBits.mShift) - d.mBlueBits.mBits;
        uint32_t redMask = d.mRedBits.mMask;
        uint32_t greenMask = d.mGreenBits.mMask;
        uint32_t blueMask = d.mBlueBits.mMask;
        const uint16_t* src = static_cast<const uint16_t*>(mScreen);
        int32_t out = 0;
        for (int32_t i = 0; i < mPixels; i++) {
            uint32_t v = *src++;
            uint32_t word = ppc_slw(v & redMask, redShift) | ppc_slw(v & greenMask, greenShift) |
                            ppc_slw(v & blueMask, blueShift);
            std::memcpy(static_cast<uint8_t*>(mRGB) + out, &word, 4);
            out += 3;
        }
        gFree(mScreen);
        break;
    }
    case 32: {
        // In place: the first three bytes of each 4-byte pixel.
        int32_t in = 0, out = 0;
        for (int32_t i = 0; i < mPixels; i++) {
            uint8_t* p = static_cast<uint8_t*>(mScreen);
            p[out] = p[in];
            p[out + 1] = p[in + 1];
            p[out + 2] = p[in + 2];
            in += 4;
            out += 3;
        }
    }
        // fall through
    case 24:
        mRGB = mScreen;
        mScreen = nullptr;
        break;
    default:
        break;
    }
    return 1;
}

bool CCapture::GetCapture() {
    CDisplay& d = gei_Display;
    mPixels = d.mWidth * d.mHeight;
    // Bits, not bytes, per pixel: eight times what the copy below fills.
    mScreen = gMalloc(static_cast<uint32_t>(mPixels * d.mBitsPerPixel));
    if (mScreen == nullptr) {
        gei_ErrorHandler.Push(2, kErrOutOfMemory);
        return false;
    }
    IDirectDrawSurface4* surface = gei_DDraw->mBack;
    DDSURFACEDESC2 desc;
    std::memset(&desc, 0, sizeof desc);
    desc.dwSize = sizeof desc;
    gDDResult = surface->Lock(nullptr, &desc, DDLOCK_WAIT, nullptr);
    if (gDDResult != 0) return false;   // mScreen stays allocated
    int32_t bytesPerPixel = d.mBitsPerPixel >> 3;
    const uint8_t* src = static_cast<const uint8_t*>(desc.lpSurface);
    uint8_t* dst = static_cast<uint8_t*>(mScreen);
    for (int32_t y = 0; y < d.mHeight; y++) {
        std::memcpy(dst, src, static_cast<uint32_t>(d.mWidth * bytesPerPixel));
        src += desc.lPitch;
        dst += d.mWidth * bytesPerPixel;
    }
    gDDResult = surface->Unlock(nullptr);
    return gDDResult == 0;
}

int32_t CCapture::Capture() {
    if (mPending != nullptr) {
        if (!GetCapture()) return 0;
        if (ConvertCapture() == 0) return 0;
        if (SaveUncompressedTGA(mPending) == 0) return 0;
        gFree(mRGB);
        mPending = nullptr;
    }
    return 1;
}

int32_t CCapture::SetCapture(char* name) {
    if (name != nullptr) {
        std::strcpy(mName, name);
        mPending = mName;
    }
    return 1;
}
