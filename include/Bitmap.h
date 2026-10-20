// Bitmap.h: a plain pixel buffer (engine library; methods at 0x10046db4..0x100483c4, after
// the last static initialiser, so the original file name is unknown).
//
// Translated: clear, realloc, load, load_wavelet, the destructor and the pixel methods
// (colour_fill, copy_to, swizzle_for_gimex, alphablt, shadowblend, colourblt, descimate).
//
// Pixel order in memory, as the pixel methods treat it: byte 0 is the alpha, bytes 1 to 3 are
// the three colour channels (named red, green, blue below, in the order the arguments land;
// the binary does not say which channel is which). A 1-byte pixel is an alpha only.
#pragma once
#include <cstdint>

class CWavelet;   // Wavelet.h

class Bitmap {
public:
    // Inline in the original (the constructor stores zeros; the destructor's out-of-line
    // copy is at 0x10000b8c, in landscape.cpp).
    Bitmap() : mWidth(0), mHeight(0), mBytesPerPixel(0), mData(nullptr) {}
    ~Bitmap() {
        if (mData != nullptr) delete[] mData;
        mWidth = 0;
        mHeight = 0;
        mBytesPerPixel = 0;
        mData = nullptr;
    }

    void clear();
    // Reallocates when the size differs or there is no buffer (returns 0 if new fails),
    // then zeroes the buffer.
    int32_t realloc(uint32_t width, uint32_t height, uint32_t bytesPerPixel);
    // Reads width, height, bytes per pixel (little-endian 32-bit, swapped by
    // _USwapBlock32 on the Mac), reallocates as realloc does, then reads the pixels,
    // through the engine's file callbacks (Wavelet.h).
    int32_t load(void* file);
    // 0x10047294: the same header (width, height, depth), then the wavelet-packed planes
    // (depth 1: alpha only; 3: colour; 4: colour and alpha), unpacked by the wavelet
    // decompressor into mData. Answers 0 when a payload buffer cannot be allocated, else 1.
    int32_t load_wavelet(void* file, CWavelet* wavelet);

    // Pixel methods (the engine library, 0x10046e2c to 0x10047d24). None of them clips or
    // checks the destination's size against the source's: the caller sizes the buffers.
    // 0x10046e2c: no data, nothing. 4 bytes per pixel: every pixel becomes [alpha, red, green,
    // blue]. 1 byte per pixel: every byte becomes alpha. Other depths: nothing.
    void colour_fill(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
    // 0x10046ee4: with data, memcpy of mBytesPerPixel * mWidth * mHeight bytes into dest.mData.
    void copy_to(Bitmap& dest);
    // 0x10046f60: reverses the four bytes of each of the mWidth * mHeight pixels (no data
    // check, and 4 bytes per pixel whatever mBytesPerPixel says).
    void swizzle_for_gimex();
    // 0x10047910: blends this bitmap (by its depth: 1 = alpha only, 2 = [ff, b0, b1, 0],
    // else 4 bytes [alpha, c1, c2, c3]) into dest at (x, y). dest advances 4 bytes per pixel
    // whatever its depth; the row skip is dest.mBytesPerPixel * (dest.mWidth - mWidth).
    void alphablt(Bitmap& dest, uint32_t x, uint32_t y);
    // 0x10047afc: with data, dest byte = max(dest byte, byte) over mWidth x mHeight bytes
    // (this is an 8-bit mask); dest advances 1 byte per pixel.
    void shadowblend(Bitmap& dest);
    // 0x10047bb4: with data, this is an 8-bit mask: a = (mask * alpha) >> 8 raises dest byte 0
    // to at least a and blends bytes 1 to 3 towards (alpha * red, green, blue) >> 8 by a.
    void colourblt(Bitmap& dest, uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue);
    // 0x10047d24: box-filter downscale into dest (4 bytes per pixel both sides); answers 0
    // when an allocation fails (leaking what was allocated) and 1 otherwise.
    int32_t descimate(Bitmap& dest);

    uint32_t mWidth;          // 0x00
    uint32_t mHeight;         // 0x04
    uint32_t mBytesPerPixel;  // 0x08
    uint8_t* mData;           // 0x0c  new[] (4-byte pointer in the original)
};
