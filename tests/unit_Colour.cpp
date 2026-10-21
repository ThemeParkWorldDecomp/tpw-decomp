// Test library for __sinit_Colour_cpp (unit v5L21): the 13 records of src/bullfrog/Colour.cpp, six words each, in address order.
#include <cstdint>

#include "TbPixFmt.h"

extern "C" {
void tpw_colour_formats(uint32_t* out) {
    const TbPixFormat* formats[] = {&gColourFormatRGBA8888, &gColourFormatRGBA4444, &gColourFormatXBGR8888, &gColourFormatXRGB8888,
                                    &gColourFormatABGR8888, &gColourFormatARGB8888, &gColourFormatBGR888,   &gColourFormatRGB888,
                                    &gColourFormatARGB4444, &gColourFormatRGB565,   &gColourFormatXRGB1555, &gColourFormatARGB1555,
                                    &gColourFormatARGB8888Default};
    for (const TbPixFormat* f : formats) {
        *out++ = f->mRedMask;
        *out++ = f->mGreenMask;
        *out++ = f->mBlueMask;
        *out++ = f->mAlphaMask;
        *out++ = static_cast<uint32_t>(f->mBitsPerPixel);
        *out++ = static_cast<uint32_t>(f->mPalettised);
    }
}
}
