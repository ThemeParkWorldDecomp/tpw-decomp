// Sam's Utilities' pixel converter (sams utils shared, UPixels.cpp): the engine's image
// loader converts every TGA through it (src/tb_loaders.cpp). Unlike the rest of the shim
// library this is pure computation, so platform/upixels.cpp is a translation of the
// original, tested differentially (tests/test_UPixels.py).
#pragma once
#include <cstdint>

namespace SamsUtilities {

// A pixel as the converter passes it around: 0xAARRGGBB (the Mac struct's bytes are alpha,
// red, green, blue).
using UPixelValue = uint32_t;
struct UPixel;                   // the game only ever passes a NULL palette
class UPixelsImplementation;     // one subclass per pixel format (upixels.cpp)

// Pixel formats (the numbers UPixels::SetData switches on, 0x1000ee3c).
enum : int32_t {
    kPixelsBitmap = 0,
    kPixels8Indexed = 1,
    kPixels16ARGB = 2,
    kPixels16ARGB1555 = 3,
    kPixels16RGBA = 4,
    kPixels16BGRA = 5,
    kPixels16BGR565 = 6,
    kPixels24RGB = 7,
    kPixels24BGR = 8,
    kPixels32ARGB = 9,
    kPixels32RGBA = 10,
    kPixels32BGRA = 11,
};

// 12 bytes on the Mac: implementation +0, alpha count +4, average colour +8.
class UPixels {
public:
    // 0x1000eb88. `stride` -1 means width * bytes per pixel. The format is an intptr_t
    // because the engine passes a data pointer for depths it does not know (tb_loaders.cpp);
    // any number above 11 selects no implementation, as in the original.
    UPixels(intptr_t format, void* data, int32_t width, int32_t height, int32_t stride, UPixel* palette);
    UPixels();                                                  // (sams utils shared; the one an array of them is made with)
    // The same with a buffer of its own for width x height pixels (sams utils shared).
    UPixels(intptr_t format, int32_t width, int32_t height, UPixel* palette);
    // Points this at other pixels (sams utils shared).
    void SetData(intptr_t format, void* data, int32_t width, int32_t height, int32_t stride, UPixel* palette);
    void* GetData();                                            // the pixels (sams utils shared)
    ~UPixels();                              // 0x1000ed94
    void SetAlphaDefault(uint8_t alpha);     // 0x1000fad0: the alpha of formats without one
    void SetAlphaThreshold(uint8_t alpha);   // 0x1000fb1c: pixels above it are counted by Convert
    void SetFlipped(uint8_t flipped);        // 0x1000f388: rows are read bottom-up as a source
    void SetTransparentColour(UPixelValue colour);   // 0x1000f3d0
    // 0x1000f450: copies `source` into this buffer, converting each pixel. With `dither`
    // set (the game always sets it) the copy also counts the pixels whose alpha is above
    // this object's threshold and averages each channel.
    void Convert(UPixels& source, bool dither);
    void GetAverages(uint8_t* red, uint8_t* green, uint8_t* blue, uint8_t* alpha);   // 0x1000fb6c
    int32_t GetAlphaCount();                 // 0x1000fbd8

    UPixelsImplementation* mImplementation;  // 0x00
    int32_t mAlphaCount;                     // 0x04
    uint8_t mAverage[4];                     // 0x08: alpha, red, green, blue
};

}  // namespace SamsUtilities
