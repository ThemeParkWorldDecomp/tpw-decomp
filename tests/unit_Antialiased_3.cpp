// Test library for the antialiased 3D line for 3 and 4 bytes per pixel (unit v5940; libtpw_unit_Antialiased_3.dylib,
// tests/test_Antialiased_3.py), main app:
//   src/TPString_Antialiased_3.cpp   Antialiased<CTraits<4>> 0x100058c0, Antialiased<CTraits<3>> 0x10005e30
//
// The callees outside the unit are recording fakes: CColour::GetFormattedValue and IntensifyPix<N>. sqrt is the real one on both sides.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"
#include "Light.h"
#include "DrawRenderClasses.h"
#include "TbGraphicFile.h"
#include "TbPixFmt.h"

using namespace calllog;

namespace {
const uint8_t* gData = nullptr;
}

// ---- the fakes
uint32_t bfl::draw::CColour::GetFormattedValue(const TbPixFormat& f) const {
    uint32_t c = (uint32_t(mRed) << 24) | (uint32_t(mGreen) << 16) | (uint32_t(mBlue) << 8) | uint32_t(mAlpha);
    ev("GetFormattedValue", {c, f.mRedMask, f.mGreenMask, f.mBlueMask, f.mAlphaMask, f.mBitsPerPixel, f.mPalettised});
    return static_cast<uint32_t>(ret("GetFormattedValue"));
}

namespace tpw_intensify {
template <int N>
void IntensifyPix(const CTraits<N>&, uint8_t* pix, double alpha, uint32_t colour, uint32_t mask1, uint32_t mask2, uint32_t mask3,
                  uint32_t keepMask, int32_t x, int32_t y, const TbRect& rect) {
    uint64_t bits;
    std::memcpy(&bits, &alpha, 8);
    int32_t off = static_cast<int32_t>(reinterpret_cast<uintptr_t>(pix) - reinterpret_cast<uintptr_t>(gData));
    ev("Pix", {off, static_cast<int64_t>(bits & 0xffffffffu), static_cast<int64_t>(bits >> 32), colour, mask1, mask2, mask3, keepMask, x, y});
    ev("PixClip", {rect.left, rect.top, rect.right, rect.bottom});
}
template void IntensifyPix<3>(const CTraits<3>&, uint8_t*, double, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t,
                              const TbRect&);
template void IntensifyPix<4>(const CTraits<4>&, uint8_t*, double, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t,
                              const TbRect&);
}  // namespace tpw_intensify

extern "C" {

uint64_t tpw_aa_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// fmt: m0 m1 m2 m3 depth palettised; clip: l t r b; pts: x0 y0 x1 y1
void tpw_aa_run(int32_t n, const uint32_t* fmt, uint64_t data, int32_t stride, const int32_t* clip, uint32_t colour, const int32_t* pts) {
    TbRenderArea area(TbRenderArea::kCleared);
    std::memcpy(&area, fmt, 16);
    area.mDepth = static_cast<int32_t>(fmt[4]);
    area.mPalettised = static_cast<int32_t>(fmt[5]);
    area.mData = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(data));
    area.mStride = stride;
    gData = area.mData;
    TbRect rect{clip[0], clip[1], clip[2], clip[3]};
    bfl::draw::CColour c;
    std::memset(&c, 0, sizeof(c));
    c.mRed = colour >> 24;
    c.mGreen = colour >> 16;
    c.mBlue = colour >> 8;
    c.mAlpha = colour;
    TbPoint from{pts[0], pts[1]};
    TbPoint to{pts[2], pts[3]};
    if (n == 4)
        tpw_antialias::Antialiased<4>(tpw_intensify::CTraits<4>(), area, rect, c, from, to);
    else
        tpw_antialias::Antialiased<3>(tpw_intensify::CTraits<3>(), area, rect, c, from, to);
}

}  // extern "C"
