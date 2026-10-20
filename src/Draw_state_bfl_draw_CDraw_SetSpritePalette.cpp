#include "DrawState.h"
#include "DrawRenderClasses.h"
#include "TbPixFmt.h"

#include <cstring>

namespace bfl {
namespace draw {

namespace {
inline uint32_t Srw(uint32_t v, uint32_t n) { n &= 0x3f; return n >= 32 ? 0 : v >> n; }
inline uint32_t Slw(uint32_t v, uint32_t n) { n &= 0x3f; return n >= 32 ? 0 : v << n; }
inline uint32_t Rotl(uint32_t v, uint32_t n) { n &= 0x1f; return n ? (v << n) | (v >> (32 - n)) : v; }
}

// 0x10065a7c: the palette is kept at Mac +0x468; the table at Mac +0x68 holds each entry converted to the format
// (the one given, or the current one). A format of all zeros gives 0 / -1 alternating.
// The host object is laid out natively (pointers are 8 bytes), so members are used, not Mac offsets: the format is the
// first 24 bytes of mCurrent (Mac +8), the table is mF68[0..0x400), the palette copy mF68[0x400..0x800).
void CDraw::SetSpritePalette(const TbPalette& palette, const TbPixFormat* format) {
    uint8_t* self = mF68;
    const uint32_t* pal = reinterpret_cast<const uint32_t*>(&palette);
    std::memcpy(self + 0x400, pal, 0x400);

    const uint32_t* cur = reinterpret_cast<const uint32_t*>(&mCurrent);
    uint32_t fmt[6];
    std::memcpy(fmt, cur, sizeof(fmt));
    if (format) std::memcpy(fmt, format, sizeof(fmt));

    uint32_t* table = reinterpret_cast<uint32_t*>(self);
    bool zero = true;
    for (int i = 0; i < 6; ++i)
        if (fmt[i] != 0) zero = false;
    if (zero) {
        for (uint32_t i = 0; i < 256; ++i) table[i] = (i & 1) ? 0xffffffffu : 0;
        return;
    }

    TbPixFormat source(0xff, 0xff00, 0xff0000, 0, 0x20, 0);
    TbPixFormat destination(fmt[0], fmt[1], fmt[2], fmt[3], static_cast<int32_t>(fmt[4]), static_cast<int32_t>(fmt[5]));
    TbMaskRotates rot;
    rot.Init(source, destination);
    uint32_t r0 = rot.mRotate[0], r1 = rot.mRotate[1], r2 = rot.mRotate[2];
    const uint32_t* in = pal;
    const uint32_t red = cur[0];
    const uint32_t green = cur[1];
    const uint32_t blue = cur[2];
    const uint32_t alpha = cur[3];
    for (int i = 0; i < 128; ++i) {
        uint32_t v = in[0];
        uint32_t g = Srw(v, r1) | Slw(v, 32 - r1);
        uint32_t b = Srw(v, r2) | Slw(v, 32 - r2);
        table[2 * i] = alpha | (blue & b) | (red & Rotl(v, 32 - r0)) | (green & g);
        v = in[1];
        in += 2;
        uint32_t s = Srw(v, r1);
        g = s | Slw(v, 32 - r1);
        b = Srw(v, r2) | Slw(v, 32 - r2);
        // the original reuses the register holding the shifted value as the rotate count
        table[2 * i + 1] = alpha | (blue & b) | (red & Rotl(v, s)) | (green & g);
    }
}

} // namespace draw
} // namespace bfl
