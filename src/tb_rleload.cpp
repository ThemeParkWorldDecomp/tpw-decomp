// CRLELoad's destructor, Close and the three members a texture entry unpacks through (engine
// library of SimTheme Park 2.04b3; the original file is tb_decode_rle.cpp by its assertions,
// 0x10005244..0x10005b90; the inventory files the range under tb_depthoffield.cpp). The
// constructor, SetSize, SetPalette and LoadSprite are in src/tb_spritefileio.cpp.
//
// The picture is run-length coded. After the file's header (src/tb_spritefileio.cpp, Load_RLE) the
// data holds one record per row of the picture:
//   s8 length          the bytes of the row's tokens that follow (a row is skipped with it)
//   tokens, until the row has `width` pixels:
//     s8 n, n > 0      n bytes follow, each the palette index of one pixel
//     s8 n, n <= 0     one byte follows, a palette index, repeated -n times (n = 0 is a token of no
//                      pixels, and -128 repeats 128 times)
// The palette holds the pixels in the texture format's order, 16 or 32 bits an entry, and the pixels
// are copied as they are.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "platform/msl.h"
#include "sprite_engine.h"
#include "tb_decode_wavelet.h"
#include "tb_loaders.h"
#include "tb_resource_texture.h"
#include "Wavelet.h"

namespace {

// The original expands a row that must be scaled into a buffer of 133 words on its stack, which a
// picture wider than that (the game's widest is 96) would overrun. A wider row gets a buffer of
// its own here; a token can run past the end of a row by up to 127 pixels.
constexpr int32_t kRowBufferPixels = 1024;
constexpr int32_t kTokenOverrun = 128;

// The destination and the palette need not be aligned, so every pixel goes through memcpy.
template <typename Pixel>
inline Pixel LoadPixel(const uint8_t* where) {
    Pixel pixel;
    std::memcpy(&pixel, where, sizeof pixel);
    return pixel;
}
template <typename Pixel>
inline void StorePixel(uint8_t* where, Pixel pixel) {
    std::memcpy(where, &pixel, sizeof pixel);
}

// Expands one row's tokens to `out` until `width` pixels are done, and returns where the next pixel
// would go. The last token may run past the width and writes all of its pixels; `in` is left after
// the row's last token.
template <typename Pixel>
uint8_t* ExpandRow(const uint8_t*& in, uint8_t* out, const uint8_t* palette, int32_t width) {
    int32_t done = 0;
    while (done < width) {
        const int32_t token = static_cast<int8_t>(*in++);
        if (token <= 0) {
            const Pixel pixel = LoadPixel<Pixel>(palette + static_cast<std::size_t>(*in++) * sizeof(Pixel));
            for (int32_t i = 0; i < -token; ++i) {
                StorePixel<Pixel>(out, pixel);
                out += sizeof(Pixel);
            }
            done -= token;
        } else {
            for (int32_t i = 0; i < token; ++i) {
                StorePixel<Pixel>(out, LoadPixel<Pixel>(palette + static_cast<std::size_t>(in[i]) * sizeof(Pixel)));
                out += sizeof(Pixel);
            }
            in += token;
            done += token;
        }
    }
    return out;
}

// Picks `count` pixels of the expanded row for a destination of another width: pixel k is the one at
// k * step in 16.16 fixed point, step being the source width over the destination's.
template <typename Pixel>
uint8_t* ScaleRow(const uint8_t* row, uint8_t* out, int32_t count, int32_t step) {
    int32_t position = 0;
    for (int32_t k = 0; k < count; ++k) {
        StorePixel<Pixel>(out, LoadPixel<Pixel>(row + (position >> 16) * static_cast<std::ptrdiff_t>(sizeof(Pixel))));
        out += sizeof(Pixel);
        position += step;
    }
    return out;
}

// The processor's divw: the answer for a zero divisor is undefined there, and the emulator the tests
// check against gives all ones for a negative dividend and zero otherwise. The same values here keep a
// destination of no width (the assertion, which the game stops at) from trapping the host.
inline int32_t Divide(int32_t dividend, int32_t divisor) {
    if (divisor == 0 || (dividend == INT32_MIN && divisor == -1)) return dividend < 0 ? -1 : 0;
    return dividend / divisor;
}

// A 16.16 fixed point value and a 32-bit product, both wrapping as the processor's slwi and mullw do.
inline int32_t Fixed16(int32_t value) { return static_cast<int32_t>(static_cast<uint32_t>(value) << 16); }
inline int32_t Product(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }

}  // namespace

// ---- CRLELoad -----------------------------------------------------------------------------------------

CRLELoad::~CRLELoad() { Close(); }

// Frees the RLE bytes (the block's pointer is cleared by the allocator's free) and forgets the size.
// The palette is not the loader's and is left alone.
int32_t CRLELoad::Close() {
    if (mData != nullptr) gFree(mData);
    mHeight = 0;
    mWidth = 0;
    return 1;
}

// Unpacks the picture into `dest`, which may differ in size from the picture. Each of the picture's
// `mHeight` rows takes one step, chosen by a vertical position (accum, 16.16, in the destination's rows;
// it advances by the picture's height over the destination's after a decode or a repeat):
//   decode      expand the row, scale it to the destination's width when that differs, and advance
//   repeat      copy the previous destination row (a destination taller than the picture)
//   skip        pass over the picture's row without writing (a destination shorter than the picture)
// After each step: below 2.0 the next step is a decode once the position has reached 1.0 and a repeat
// before that (the position keeps only its fraction); from 2.0 on it is a skip and the position drops 1.0.
// So the loop runs once per row of the PICTURE however many rows the destination has, and a destination
// taller than the picture is never filled to its height; the game's textures are the picture's own
// size or smaller.
// Quirks kept: the destination's rows advance by the pixels a row really wrote (a token that runs past
// the row's width shifts every later row of an unscaled picture) plus the pitch's slack, except
// the repeated rows, which advance by the pitch itself (so a pitch of 0 repeats a row in place).
// A destination of no width or height asserts (the game stops there) and the divisions go on.
int32_t CRLELoad::Decompress(SUnpackDestination* dest) {
    const int32_t bytes = gTextureBytesPerPixel[gei_TextureSettings.mFormat];   // the table at 0x10051600, the same values
    const bool sixteenBit = bytes == 2;
    const uint8_t* palette = static_cast<const uint8_t*>(mPalette);
    uint8_t* out = static_cast<uint8_t*>(dest->mData);
    const uint8_t* in = static_cast<const uint8_t*>(mData);
    const int32_t width = mWidth;
    const int32_t height = mHeight;
    if (!(dest->mWidth != 0 && dest->mHeight != 0))
        msl_assertion_failed("dest->i_Width && dest->i_Height", "tb_decode_rle.cpp", 0x6f);
    const int32_t columnStep = Divide(Fixed16(width), dest->mWidth);
    const int32_t rowStep = Divide(Fixed16(height), dest->mHeight);
    const bool scaled = dest->mWidth != width;
    const int32_t rowBytes = Product(dest->mWidth, bytes);
    const int32_t slack = dest->mPitch == 0 ? 0 : dest->mPitch - rowBytes;
    uint32_t rowBufferWords[kRowBufferPixels];
    rowBufferWords[0] = 0;   // a picture of no width scaled up reads one pixel of stack
    uint8_t* rowBuffer = reinterpret_cast<uint8_t*>(rowBufferWords);
    uint32_t* heapBuffer = nullptr;
    if (scaled && width > 0 && width + kTokenOverrun > kRowBufferPixels) {
        heapBuffer = new uint32_t[static_cast<std::size_t>(width) + kTokenOverrun];
        rowBuffer = reinterpret_cast<uint8_t*>(heapBuffer);
    }

    enum { kSkip = 0, kDecode = 1, kRepeat = 2 };
    int32_t step = kDecode;
    int32_t accum = 0;
    for (int32_t y = 0; y < height; ++y) {
        if (step == kSkip) {
            in += 1 + static_cast<int8_t>(*in);
        } else if (step == kRepeat) {
            std::memmove(out, out - dest->mPitch, static_cast<uint32_t>(rowBytes));
            accum += rowStep;
            out += dest->mPitch;
        } else {
            ++in;   // the row's length
            uint8_t* target = scaled ? rowBuffer : out;
            uint8_t* end;
            if (sixteenBit) {
                end = ExpandRow<uint16_t>(in, target, palette, width);
            } else {
                end = ExpandRow<uint32_t>(in, target, palette, width);
            }
            if (scaled) {
                if (sixteenBit) {
                    end = ScaleRow<uint16_t>(rowBuffer, out, dest->mWidth, columnStep);
                } else {
                    end = ScaleRow<uint32_t>(rowBuffer, out, dest->mWidth, columnStep);
                }
            }
            out = end + slack;
            accum += rowStep;
        }
        if (accum < 0x20000) {
            step = (accum & 0x10000) != 0 ? kDecode : kRepeat;
            accum &= 0xffff;
        } else {
            accum -= 0x10000;
            step = kSkip;
        }
    }
    delete[] heapBuffer;
    return 1;
}

// The picture's average colour is not kept: white with every channel full.
uint32_t CRLELoad::AverageColor() { return 0xffffffff; }

// The same as Decompress: the original calls its own virtual (the slot of Decompress in its
// vtable) with the destination and drops `flip`, so a flipped copy is not made.
int32_t CRLELoad::Blit(SUnpackDestination* dest, int32_t flip) { return Decompress(dest); }
