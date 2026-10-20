// Test entry points for platform/upixels.cpp (library unit_UPixels): one conversion as the
// engine's image loader drives it, with every setter the converter has.
#include "platform/upixels.h"

using SamsUtilities::UPixels;

extern "C" {

// Builds a source and a destination over the caller's buffers, applies the settings and
// converts. `flags`: 1 flip the source, 2 set the source's transparent colour, 4 convert
// with `dither` (the game's way). Results: out[0] the alpha count, out[1..4] the averages
// (red, green, blue, alpha), out[5] whether both formats are translated.
void tpw_upixels_convert(int32_t sourceFormat, int32_t destFormat, void* source, void* dest, int32_t width,
                         int32_t height, int32_t destWidth, int32_t destHeight, int32_t sourcePitch,
                         int32_t destPitch, uint32_t alphaDefault, uint32_t alphaThreshold, uint32_t flags,
                         uint32_t transparent, int32_t* out) {
    UPixels src(sourceFormat, source, width, height, sourcePitch, nullptr);
    UPixels dst(destFormat, dest, destWidth, destHeight, destPitch, nullptr);
    out[5] = src.mImplementation != nullptr && dst.mImplementation != nullptr;
    src.SetAlphaDefault(static_cast<uint8_t>(alphaDefault));
    dst.SetAlphaThreshold(static_cast<uint8_t>(alphaThreshold));
    if (flags & 1) src.SetFlipped(1);
    if (flags & 2) src.SetTransparentColour(transparent);
    dst.Convert(src, (flags & 4) != 0);
    uint8_t red = 0, green = 0, blue = 0, alpha = 0;
    dst.GetAverages(&red, &green, &blue, &alpha);
    out[0] = dst.GetAlphaCount();
    out[1] = red;
    out[2] = green;
    out[3] = blue;
    out[4] = alpha;
}

}  // extern "C"
