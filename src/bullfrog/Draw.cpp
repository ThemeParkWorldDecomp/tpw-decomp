// Draw.cpp: name proven by __sinit_Draw_cpp in the Bullfrog library.

#include "AudioPlaceHolder.h"
#include "bfl_Refpack.h"
#include "DrawRenderClasses.h"
#include "DrawState.h"
#include "LbDrawText.h"
#include "LbSCBack.h"
#include "LbSemaphore.h"
#include "LbSurface.h"
#include "lobby.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "TbFPUPrecision.h"
#include "TbGraphicFile.h"
#include "TbPixFmt.h"
#include "TbRaConvert.h"
#include "TpwPropFont.h"
#include "WinScrn.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <new>
#include <utility>

// Unit w239: Draw.cpp's eight CRenderClass circle rasterizers.

namespace bfl {
namespace draw {
namespace {

int32_t Add(int32_t a, int32_t b) {
    return static_cast<int32_t>(uint32_t(a) + uint32_t(b));
}

int32_t Subtract(int32_t a, int32_t b) {
    return static_cast<int32_t>(uint32_t(a) - uint32_t(b));
}

template<unsigned Bytes> uint32_t LoadPixel(const uint8_t* p) {
    uint32_t value = 0;
    for (unsigned i = 0; i < Bytes; ++i) value = (value << 8) | p[i];
    return value;
}

template<unsigned Bytes> void StorePixel(uint8_t* p, uint32_t value) {
    for (unsigned i = 0; i < Bytes; ++i)
        p[i] = static_cast<uint8_t>(value >> ((Bytes - 1 - i) * 8));
}

template<> void StorePixel<3>(uint8_t* p, uint32_t value) {
    // The original copies the first three bytes of its big-endian word.
    p[0] = static_cast<uint8_t>(value >> 24);
    p[1] = static_cast<uint8_t>(value >> 16);
    p[2] = static_cast<uint8_t>(value >> 8);
}

uint32_t BlendComponent(uint32_t destination, uint32_t source,
                        uint32_t mask, uint32_t alpha) {
    const uint32_t old_component = destination & mask;
    const uint32_t new_component = source & mask;
    return mask & (old_component + ((alpha * (new_component - old_component)) >> 8));
}

template<unsigned Bytes> class PixelWriter {
public:
    PixelWriter(TbRenderArea& area, uint32_t flags, const CColour& colour)
        : mArea(area), mFlags(flags), mFormatted(colour.GetFormattedValue(
              *reinterpret_cast<const TbPixFormat*>(&area))), mAlpha(colour.mAlpha) {
        if ((flags & 0x10) != 0) mAlpha = 0xffu - mAlpha;
    }

    bool Blended() const { return (mFlags & 0x18) != 0; }

    void operator()(uint8_t* destination) const {
        if (!Blended()) {
            StorePixel<Bytes>(destination, mFormatted);
            return;
        }
        uint32_t old_value = LoadPixel<Bytes>(destination);
        uint32_t value = old_value & Mask(3);
        value |= BlendComponent(old_value, mFormatted, Mask(0), mAlpha);
        value |= BlendComponent(old_value, mFormatted, Mask(1), mAlpha);
        value |= BlendComponent(old_value, mFormatted, Mask(2), mAlpha);
        StorePixel<Bytes>(destination, value);
    }

private:
    uint32_t Mask(unsigned index) const {
        uint32_t value;
        const auto* bytes = reinterpret_cast<const uint8_t*>(&mArea);
        const auto* p = bytes + index * 4;
        value = uint32_t(p[0]) | uint32_t(p[1]) << 8 |
                uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
        return value;
    }

    TbRenderArea& mArea;
    uint32_t mFlags;
    uint32_t mFormatted;
    uint32_t mAlpha;
};

template<> void PixelWriter<3>::operator()(uint8_t* destination) const {
    if (!Blended()) {
        StorePixel<3>(destination, mFormatted);
        return;
    }
    // The original performs an unaligned four-byte read but writes only the
    // three bytes belonging to the pixel.
    const uint32_t old_value = uint32_t(destination[0]) << 24 |
                               uint32_t(destination[1]) << 16 |
                               uint32_t(destination[2]) << 8 |
                               uint32_t(destination[3]);
    uint32_t value = old_value & Mask(3);
    value |= BlendComponent(old_value, mFormatted, Mask(0), mAlpha);
    value |= BlendComponent(old_value, mFormatted, Mask(1), mAlpha);
    value |= BlendComponent(old_value, mFormatted, Mask(2), mAlpha);
    destination[0] = static_cast<uint8_t>(value >> 24);
    destination[1] = static_cast<uint8_t>(value >> 16);
    destination[2] = static_cast<uint8_t>(value >> 8);
}

template<> void PixelWriter<4>::operator()(uint8_t* destination) const {
    if (!Blended()) {
        StorePixel<4>(destination, mFormatted);
        return;
    }
    const uint32_t old_value = LoadPixel<4>(destination);
    const uint32_t source = mFormatted & Mask(0);
    uint32_t value = old_value & 0xff;
    for (unsigned shift : {8u, 16u, 24u}) {
        const uint32_t old_component = old_value >> shift & 0xff;
        const uint32_t new_component = source >> shift & 0xff;
        value |= (old_component + ((mAlpha * (new_component - old_component)) >> 8))
                 << shift;
    }
    StorePixel<4>(destination, value);
}

template<unsigned Bytes, class Writer>
void Pixel(TbRenderArea& area, const TbRect& clip, int32_t x, int32_t y,
           const Writer& write) {
    if (x < clip.left || x >= clip.right || y < clip.top || y >= clip.bottom) return;
    write(area.mData + ptrdiff_t(y) * area.mStride + ptrdiff_t(x) * Bytes);
}

template<unsigned Bytes, class Writer>
void HorizontalLine(TbRenderArea& area, const TbRect& clip, int32_t x, int32_t y,
                    int32_t length, const Writer& write) {
    if (length < 0) {
        x = Add(x, length);
        length = -length;
    }
    if (y < clip.top || y >= clip.bottom || length <= 0) return;
    int32_t end = Add(x, length);
    if (x < clip.left) x = clip.left;
    if (end > clip.right) end = clip.right;
    if (x >= end) return;
    uint8_t* destination = area.mData + ptrdiff_t(y) * area.mStride + ptrdiff_t(x) * Bytes;
    if (Bytes == 3 && write.Blended()) {
        // The 24-bit filled-circle loop never advances its pixel pointer.
        // It repeatedly blends the first clipped pixel and leaves the rest
        // of the horizontal span untouched.
        while (x++ < end) write(destination);
        return;
    }
    if (Bytes == 4 && write.Blended()) {
        // The 32-bit specialization blends the first destination pixel once,
        // then replicates that result across the clipped span.
        write(destination);
        uint8_t value[Bytes];
        for (unsigned i = 0; i < Bytes; ++i) value[i] = destination[i];
        destination += Bytes;
        ++x;
        while (x++ < end) {
            for (unsigned i = 0; i < Bytes; ++i) destination[i] = value[i];
            destination += Bytes;
        }
        return;
    }
    while (x++ < end) {
        write(destination);
        destination += Bytes;
    }
}

template<unsigned Bytes, class Writer>
void OutlineDirect(TbRenderArea& area, const TbRect& clip, int32_t x, int32_t y,
                   uint32_t radius, const Writer& write) {
    const auto pixel = [&](int32_t px, int32_t py) { Pixel<Bytes>(area, clip, px, py, write); };
    if (radius == 0) { pixel(x, y); return; }
    if (radius == 1) {
        pixel(Subtract(x, 1), y); pixel(x, Subtract(y, 1));
        pixel(Add(x, 1), y); pixel(x, Add(y, 1)); pixel(x, y);
        return;
    }
    int32_t a = 0;
    int32_t b = static_cast<int32_t>(radius);
    int32_t decision = static_cast<int32_t>(3u - radius * 2u);
    int32_t step = 6;
    for (; a <= b; ++a) {
        pixel(Subtract(x, a), Subtract(y, b));
        pixel(Add(x, a), Subtract(y, b));
        pixel(Subtract(x, a), Add(y, b));
        pixel(Add(x, a), Add(y, b));
        pixel(Subtract(x, b), Subtract(y, a));
        pixel(Add(x, b), Subtract(y, a));
        pixel(Subtract(x, b), Add(y, a));
        pixel(Add(x, b), Add(y, a));
        if (decision < 0) decision = Add(decision, step);
        else {
            decision = Add(decision, static_cast<int32_t>((uint32_t(a) - uint32_t(b)) * 4u + 10u));
            --b;
        }
        step += 4;
    }
}

template<unsigned Bytes, class Writer>
void OutlineBlended(TbRenderArea& area, const TbRect& clip, int32_t x, int32_t y,
                    uint32_t radius, const Writer& write) {
    const auto pixel = [&](int32_t px, int32_t py) { Pixel<Bytes>(area, clip, px, py, write); };
    if (radius == 0) { pixel(x, y); return; }
    if (radius == 1) {
        pixel(Subtract(x, 1), y); pixel(x, Subtract(y, 1));
        pixel(Add(x, 1), y); pixel(x, Add(y, 1)); pixel(x, y);
        return;
    }
    int32_t b = static_cast<int32_t>(radius);
    int32_t decision = static_cast<int32_t>(3u - radius * 2u);
    pixel(x, Subtract(y, b)); pixel(x, Add(y, b));
    pixel(Subtract(x, b), y); pixel(Add(x, b), y);
    if (decision < 0) decision = static_cast<int32_t>(9u - radius * 2u);
    else {
        --b;
        decision = Add(decision, static_cast<int32_t>(10u - radius * 4u));
    }
    int32_t step = 10;
    int32_t a = 1;
    for (; a < b; ++a) {
        pixel(Subtract(x, a), Subtract(y, b)); pixel(Add(x, a), Subtract(y, b));
        pixel(Subtract(x, a), Add(y, b)); pixel(Add(x, a), Add(y, b));
        pixel(Subtract(x, b), Subtract(y, a)); pixel(Add(x, b), Subtract(y, a));
        pixel(Subtract(x, b), Add(y, a)); pixel(Add(x, b), Add(y, a));
        if (decision < 0) decision = Add(decision, step);
        else {
            decision = Add(decision, static_cast<int32_t>((uint32_t(a) - uint32_t(b)) * 4u + 10u));
            --b;
        }
        step += 4;
    }
    if (a == b) {
        pixel(Subtract(x, a), Subtract(y, b)); pixel(Add(x, a), Subtract(y, b));
        pixel(Subtract(x, a), Add(y, b)); pixel(Add(x, a), Add(y, b));
    }
}

template<unsigned Bytes, class Writer>
void Filled(TbRenderArea& area, const TbRect& clip, int32_t x, int32_t y,
            uint32_t radius, const Writer& write) {
    if (radius < 2) {
        OutlineBlended<Bytes>(area, clip, x, y, radius, write);
        return;
    }
    int32_t b = static_cast<int32_t>(radius);
    int32_t decision = static_cast<int32_t>(3u - radius * 2u);
    HorizontalLine<Bytes>(area, clip, Subtract(x, b), y, Add(b, b) + 1, write);
    if (decision < 0) decision = static_cast<int32_t>(9u - radius * 2u);
    else {
        HorizontalLine<Bytes>(area, clip, x, Subtract(y, b), 1, write);
        HorizontalLine<Bytes>(area, clip, x, Add(y, b), 1, write);
        --b;
        decision = Add(decision, static_cast<int32_t>(10u - radius * 4u));
    }
    int32_t span = Add(b, b) + 1;
    int32_t step = 4;
    int32_t a = 1;
    for (; a < b; ++a) {
        HorizontalLine<Bytes>(area, clip, Subtract(x, b), Subtract(y, a), span, write);
        HorizontalLine<Bytes>(area, clip, Subtract(x, b), Add(y, a), span, write);
        int32_t delta;
        if (decision < 0) delta = step + 6;
        else {
            const int32_t narrow = Add(a, a) + 1;
            HorizontalLine<Bytes>(area, clip, Subtract(x, a), Subtract(y, b), narrow, write);
            HorizontalLine<Bytes>(area, clip, Subtract(x, a), Add(y, b), narrow, write);
            delta = static_cast<int32_t>((uint32_t(a) - uint32_t(b)) * 4u + 10u);
            span -= 2;
            --b;
        }
        decision = Add(decision, delta);
        step += 4;
    }
    if (a == b) {
        const int32_t final_span = Add(b, b) + 1;
        HorizontalLine<Bytes>(area, clip, Subtract(x, b), Subtract(y, a), final_span, write);
        HorizontalLine<Bytes>(area, clip, Subtract(x, b), Add(y, a), final_span, write);
    }
}

template<unsigned Bytes>
void DrawCircle(TbRenderArea* area, const TbRect& clip, uint32_t flags,
                int32_t x, int32_t y, uint32_t radius, const CColour& colour,
                bool filled) {
    PixelWriter<Bytes> write(*area, flags, colour);
    if (filled) Filled<Bytes>(*area, clip, x, y, radius, write);
    else if (write.Blended()) OutlineBlended<Bytes>(*area, clip, x, y, radius, write);
    else OutlineDirect<Bytes>(*area, clip, x, y, radius, write);
}

} // namespace

#define TPW_DEFINE_RENDER_CLASS(Name, Bytes) \
void Name::CircleOutline(TbRenderArea* area, const TbRect& clip, uint32_t flags, \
                         int32_t x, int32_t y, uint32_t radius, const CColour& colour) const { \
    DrawCircle<Bytes>(area, clip, flags, x, y, radius, colour, false); \
} \
void Name::CircleFilled(TbRenderArea* area, const TbRect& clip, uint32_t flags, \
                        int32_t x, int32_t y, uint32_t radius, const CColour& colour) const { \
    DrawCircle<Bytes>(area, clip, flags, x, y, radius, colour, true); \
}

TPW_DEFINE_RENDER_CLASS(CRenderClass8, 1)
TPW_DEFINE_RENDER_CLASS(CRenderClass16, 2)
TPW_DEFINE_RENDER_CLASS(CRenderClass24, 3)
TPW_DEFINE_RENDER_CLASS(CRenderClass32, 4)

#undef TPW_DEFINE_RENDER_CLASS

} // namespace draw
} // namespace bfl

// Unit w240: Draw.cpp's remaining CRenderClass raster methods.

namespace bfl {
namespace draw {
namespace {

int32_t Fixed16(int32_t value) {
    return static_cast<int32_t>(uint32_t(value) << 16);
}

int32_t FixedSlope(int32_t from, int32_t to, int32_t height) {
    return static_cast<int32_t>(uint32_t(Subtract(to, from)) * 0x10000u) / height;
}

template<unsigned Bytes> class PixelWriter_2 {
public:
    PixelWriter_2(TbRenderArea& area, uint32_t flags, const CColour& colour,
                bool standalone_pixel = false)
        : mArea(area), mFlags(flags), mFormatted(colour.GetFormattedValue(
              *reinterpret_cast<const TbPixFormat*>(&area))), mAlpha(colour.mAlpha),
          mStandalonePixel(standalone_pixel) {
        if ((flags & 0x10) != 0) mAlpha = 0xffu - mAlpha;
    }

    void operator()(uint8_t* destination) const {
        if ((mFlags & 0x18) == 0) {
            StorePixel<Bytes>(destination, mFormatted);
            return;
        }
        const uint32_t old_value = LoadPixel<Bytes>(destination);
        uint32_t value = old_value & Mask(3);
        for (unsigned i = 0; i < 3; ++i) {
            const uint32_t mask = Mask(i);
            if (mStandalonePixel) {
                const uint32_t old_component = old_value & mask;
                value |= mask & (old_component +
                    ((mAlpha * (mFormatted & (mask - old_component))) >> 8));
            } else {
                value |= BlendComponent(old_value, mFormatted, mask, mAlpha);
            }
        }
        StorePixel<Bytes>(destination, value);
    }

private:
    uint32_t Mask(unsigned index) const {
        const auto* p = reinterpret_cast<const uint8_t*>(&mArea) + index * 4;
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
               uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    }

    TbRenderArea& mArea;
    uint32_t mFlags;
    uint32_t mFormatted;
    uint32_t mAlpha;
    bool mStandalonePixel;
};

template<> void PixelWriter_2<3>::operator()(uint8_t* destination) const {
    if ((mFlags & 0x18) == 0) {
        StorePixel<3>(destination, mFormatted);
        return;
    }
    const uint32_t old_value = uint32_t(destination[0]) << 24 |
                               uint32_t(destination[1]) << 16 |
                               uint32_t(destination[2]) << 8 |
                               uint32_t(destination[3]);
    uint32_t value = old_value & Mask(3);
    for (unsigned i = 0; i < 3; ++i) {
        const uint32_t mask = Mask(i);
        if (mStandalonePixel) {
            const uint32_t old_component = old_value & mask;
            value |= mask & (old_component +
                ((mAlpha * (mFormatted & (mask - old_component))) >> 8));
        } else {
            value |= BlendComponent(old_value, mFormatted, mask, mAlpha);
        }
    }
    destination[0] = static_cast<uint8_t>(value >> 24);
    destination[1] = static_cast<uint8_t>(value >> 16);
    destination[2] = static_cast<uint8_t>(value >> 8);
}

template<> void PixelWriter_2<4>::operator()(uint8_t* destination) const {
    if ((mFlags & 0x18) == 0) {
        StorePixel<4>(destination, mFormatted);
        return;
    }
    const uint32_t old_value = LoadPixel<4>(destination);
    const uint32_t source = mFormatted & Mask(0);
    uint32_t value = old_value & 0xff;
    for (unsigned shift : {8u, 16u, 24u}) {
        const uint32_t old_component = old_value >> shift & 0xff;
        const uint32_t new_component = source >> shift & 0xff;
        value |= (old_component + ((mAlpha * (new_component - old_component)) >> 8))
                 << shift;
    }
    StorePixel<4>(destination, value);
}

template<unsigned Bytes, class Writer>
void PutPixel(TbRenderArea& area, const TbRect& clip, int32_t x, int32_t y,
              const Writer& write) {
    if (x < clip.left || x >= clip.right || y < clip.top || y >= clip.bottom) return;
    write(area.mData + ptrdiff_t(y) * area.mStride + ptrdiff_t(x) * Bytes);
}

template<unsigned Bytes, class Writer>
void PutHorizontal(TbRenderArea& area, const TbRect& clip, int32_t x, int32_t y,
                   int32_t length, const Writer& write) {
    if (length < 0) {
        x = Add(x, length);
        length = -length;
    }
    if (y < clip.top || y >= clip.bottom) return;
    int32_t end = Add(x, length);
    if (end < clip.left || x >= clip.right) return;
    if (x < clip.left) x = clip.left;
    if (end > clip.right) end = clip.right;
    uint8_t* destination = area.mData + ptrdiff_t(y) * area.mStride + ptrdiff_t(x) * Bytes;
    while (x++ < end) {
        write(destination);
        destination += Bytes;
    }
}

template<unsigned Bytes, class Writer>
void PutHorizontalOriginal(TbRenderArea& area, const TbRect& clip, int32_t x, int32_t y,
                           int32_t length, bool blended, const Writer& write) {
    if (!blended || (Bytes != 3 && Bytes != 4)) {
        PutHorizontal<Bytes>(area, clip, x, y, length, write);
        return;
    }
    if (length < 0) {
        x = Add(x, length);
        length = -length;
    }
    if (y < clip.top || y >= clip.bottom) return;
    int32_t end = Add(x, length);
    if (end < clip.left || x >= clip.right) return;
    if (x < clip.left) x = clip.left;
    if (end > clip.right) end = clip.right;
    length = end - x;
    if (length <= 0) return;
    uint8_t* destination = area.mData + ptrdiff_t(y) * area.mStride + ptrdiff_t(x) * Bytes;
    if (Bytes == 3) {
        // The original's paired 24-bit loop never advances its word pointer.
        while (length-- > 0) write(destination);
    } else {
        // The 32-bit loop blends the first destination once and broadcasts it.
        write(destination);
        const uint32_t value = LoadPixel<4>(destination);
        while (--length > 0) {
            destination += 4;
            StorePixel<4>(destination, value);
        }
    }
}

template<unsigned Bytes, class Writer>
void PutVertical(TbRenderArea& area, const TbRect& clip, int32_t x, int32_t y,
                 int32_t length, const Writer& write) {
    if (length < 0) {
        y = Add(y, length);
        length = -length;
    }
    if (x < clip.left || x >= clip.right) return;
    int32_t end = Add(y, length);
    if (end < clip.top || y >= clip.bottom) return;
    if (y < clip.top) y = clip.top;
    if (end > clip.bottom) end = clip.bottom;
    uint8_t* destination = area.mData + ptrdiff_t(y) * area.mStride + ptrdiff_t(x) * Bytes;
    while (y++ < end) {
        write(destination);
        destination += area.mStride;
    }
}

template<unsigned Bytes, class Writer>
void PutDiagonal(TbRenderArea& area, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                 const Writer& write) {
    const int32_t dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    const int32_t dy = y1 >= y0 ? y1 - y0 : y0 - y1;
    const int32_t sx = x1 - x0 < 1 ? -1 : 1;
    const int32_t sy = y1 - y0 < 1 ? -1 : 1;
    uint8_t* destination = area.mData + ptrdiff_t(y0) * area.mStride + ptrdiff_t(x0) * Bytes;
    const ptrdiff_t row_step = ptrdiff_t(sy) * area.mStride;
    if (dy < dx) {
        int32_t error = -dx;
        for (;;) {
            error += dy * 2;
            write(destination);
            if (x0 == x1) break;
            if (error >= 0) {
                destination += row_step;
                error -= dx * 2;
            }
            x0 += sx;
            destination += ptrdiff_t(sx) * Bytes;
        }
    } else {
        int32_t error = -dy;
        for (;;) {
            error += dx * 2;
            write(destination);
            if (y0 == y1) break;
            if (error >= 0) {
                destination += ptrdiff_t(sx) * Bytes;
                error -= dy * 2;
            }
            y0 += sy;
            destination += row_step;
        }
    }
}

bool ClipLine(const TbRect& clip, int32_t& x0, int32_t& y0, int32_t& x1, int32_t& y1) {
    if (x0 < clip.left) {
        if (x1 < clip.left) return false;
        y0 -= ((y0 - y1) * (x0 - clip.left)) / (x0 - x1);
        x0 = clip.left;
        if (x1 >= clip.right) {
            y1 -= ((y1 - y0) * (x1 - (clip.right - 1))) / (x1 - clip.left);
            x1 = clip.right - 1;
        }
    } else if (x0 < clip.right) {
        if (x1 < clip.left) {
            y1 -= ((y1 - y0) * (x1 - clip.left)) / (x1 - x0);
            x1 = clip.left;
        } else if (x1 >= clip.right) {
            const int32_t old = x1;
            x1 = clip.right - 1;
            y1 -= ((y1 - y0) * (old - x1)) / (old - x0);
        }
    } else {
        if (x1 >= clip.right) return false;
        const int32_t edge = clip.right - 1;
        y0 -= ((y0 - y1) * (x0 - edge)) / (x0 - x1);
        x0 = edge;
        if (x1 < clip.left) {
            y1 -= ((y1 - y0) * (x1 - clip.left)) / (x1 - x0);
            x1 = clip.left;
        }
    }
    if (y0 < clip.top) {
        if (y1 < clip.top) return false;
        x0 -= ((x0 - x1) * (y0 - clip.top)) / (y0 - y1);
        y0 = clip.top;
        if (y1 >= clip.bottom) {
            x1 -= ((x1 - x0) * (y1 - (clip.bottom - 1))) / (y1 - clip.top);
            y1 = clip.bottom - 1;
        }
    } else if (y0 < clip.bottom) {
        if (y1 < clip.top) {
            x1 -= ((x1 - x0) * (y1 - clip.top)) / (y1 - y0);
            y1 = clip.top;
        } else if (y1 >= clip.bottom) {
            const int32_t old = y1;
            y1 = clip.bottom - 1;
            x1 -= ((x1 - x0) * (old - y1)) / (old - y0);
        }
    } else {
        if (y1 >= clip.bottom) return false;
        const int32_t edge = clip.bottom - 1;
        x0 -= ((x0 - x1) * (y0 - edge)) / (y0 - y1);
        y0 = edge;
        if (y1 < clip.top) {
            x1 -= ((x1 - x0) * (y1 - clip.top)) / (y1 - y0);
            y1 = clip.top;
        }
    }
    return true;
}

TbRect Normalised(TbRect rect) {
    if (rect.left > rect.right) std::swap(rect.left, rect.right);
    if (rect.top > rect.bottom) std::swap(rect.top, rect.bottom);
    return rect;
}

bool Intersects(const TbRect& a, const TbRect& b) {
    return a.left <= b.right && b.left <= a.right &&
           a.top <= b.bottom && b.top <= a.bottom;
}

TbRect Intersection(const TbRect& a, const TbRect& b) {
    TbRect out = {a.left > b.left ? a.left : b.left,
                  a.top > b.top ? a.top : b.top,
                  a.right < b.right ? a.right : b.right,
                  a.bottom < b.bottom ? a.bottom : b.bottom};
    if (out.right < out.left) out.right = out.left;
    if (out.bottom < out.top) out.bottom = out.top;
    return out;
}

template<unsigned Bytes, class Writer>
void PutRectangleFilled(TbRenderArea& area, const TbRect& rect, const Writer& write) {
    const int32_t width = rect.right - rect.left;
    int32_t height = rect.bottom - rect.top;
    uint8_t* row = area.mData + ptrdiff_t(rect.top) * area.mStride + ptrdiff_t(rect.left) * Bytes;
    while (height-- > 0) {
        uint8_t* destination = row;
        for (int32_t x = 0; x < width; ++x) {
            write(destination);
            destination += Bytes;
        }
        row += area.mStride;
    }
}

template<unsigned Bytes, class Horizontal, class Line>
void PutTriangle(TbRenderArea& area, const TbRect& clip, uint32_t flags,
                 int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                 int32_t x2, int32_t y2, Horizontal horizontal, Line line) {
    if ((flags & 4) != 0) {
        uint8_t saved[3][Bytes] = {};
        bool keep[3] = {};
        const int32_t xs[3] = {x0, x1, x2};
        const int32_t ys[3] = {y0, y1, y2};
        line(x0, y0, x1, y1);
        for (unsigned p = 0; p < 2; ++p) {
            keep[p] = xs[p] >= clip.left && xs[p] < clip.right &&
                      ys[p] >= clip.top && ys[p] < clip.bottom;
            if (keep[p]) {
                const uint8_t* src = area.mData + ptrdiff_t(ys[p]) * area.mStride + ptrdiff_t(xs[p]) * Bytes;
                for (unsigned i = 0; i < Bytes; ++i) saved[p][i] = src[i];
            }
        }
        line(x1, y1, x2, y2);
        keep[2] = x2 >= clip.left && x2 < clip.right && y2 >= clip.top && y2 < clip.bottom;
        if (keep[2]) {
            const uint8_t* src = area.mData + ptrdiff_t(y2) * area.mStride + ptrdiff_t(x2) * Bytes;
            for (unsigned i = 0; i < Bytes; ++i) saved[2][i] = src[i];
        }
        line(x2, y2, x0, y0);
        for (unsigned p = 0; p < 3; ++p) if (keep[p]) {
            uint8_t* dst = area.mData + ptrdiff_t(ys[p]) * area.mStride + ptrdiff_t(xs[p]) * Bytes;
            for (unsigned i = 0; i < Bytes; ++i) dst[i] = saved[p][i];
        }
        return;
    }
    if (y1 < y0) { std::swap(x0, x1); std::swap(y0, y1); }
    if (y2 < y1) { std::swap(x1, x2); std::swap(y1, y2); }
    if (y1 < y0) { std::swap(x0, x1); std::swap(y0, y1); }
    if (y0 == y1 && y1 == y2) return;
    int32_t long_step, long_x, short_x, short_step;
    if (y0 == y1) {
        if (x0 < x1) {
            long_step = FixedSlope(x0, x2, y2 - y0);
            long_x = Fixed16(x0); short_x = Fixed16(x1);
            short_step = FixedSlope(x1, x2, y2 - y1);
        } else {
            long_step = FixedSlope(x1, x2, y2 - y1);
            long_x = Fixed16(x1); short_x = Fixed16(x0);
            short_step = FixedSlope(x0, x2, y2 - y0);
        }
    } else {
        long_step = FixedSlope(x0, x2, y2 - y0);
        long_x = Fixed16(x0);
        const int32_t upper_step = FixedSlope(x0, x1, y1 - y0);
        short_x = long_x;
        if (upper_step <= long_step) {
            for (; y0 < y1; ++y0) {
                horizontal(short_x >> 16, y0, (long_x >> 16) - (short_x >> 16));
                long_x = Add(long_x, long_step); short_x = Add(short_x, upper_step);
            }
            if (y1 == y2) return;
            short_x = Fixed16(x1);
            short_step = FixedSlope(x1, x2, y2 - y1);
            for (; y0 < y2; ++y0) {
                horizontal(short_x >> 16, y0, (long_x >> 16) - (short_x >> 16));
                long_x = Add(long_x, long_step); short_x = Add(short_x, short_step);
            }
            return;
        }
        for (; y0 < y1; ++y0) {
            horizontal(long_x >> 16, y0, (short_x >> 16) - (long_x >> 16));
            long_x = Add(long_x, long_step); short_x = Add(short_x, upper_step);
        }
        if (y1 == y2) return;
        short_x = Fixed16(x1);
        short_step = FixedSlope(x1, x2, y2 - y1);
    }
    for (; y0 < y2; ++y0) {
        horizontal(long_x >> 16, y0, (short_x >> 16) - (long_x >> 16));
        long_x = Add(long_x, long_step); short_x = Add(short_x, short_step);
    }
}

template<unsigned Bytes, class Writer>
void PutPropText(TbRenderArea& area, const TbRect& clip, int32_t x, int32_t y,
                 const char* text, const Writer& write) {
    int32_t cursor = x;
    while (*text != 0) {
        const int32_t ch = static_cast<int8_t>(*text++);
        if (ch == 10) {
            y += 8;
            cursor = x;
            if (y + 6 > clip.bottom) return;
            continue;
        }
        if (ch == 9) {
            cursor = (cursor + 32) & ~31;
            continue;
        }
        uint32_t glyph = uint32_t(ch - 0x20);
        if (glyph > 0x88) glyph = 0;
        const uint8_t* rows = kTpwPropFont8 + glyph * 8;
        if (cursor >= clip.left && cursor + 6 <= clip.right) {
            for (int32_t row = 0; row < 6; ++row) {
                uint8_t* destination = area.mData + ptrdiff_t(y + row) * area.mStride + ptrdiff_t(cursor) * Bytes;
                for (int32_t column = 0; column < 5; ++column) {
                    if ((rows[row] & (0x40 >> column)) != 0) write(destination);
                    destination += Bytes == 3 ? 4 : Bytes;
                }
            }
        }
        cursor += rows[6];
    }
}

template<unsigned Bytes, class Writer>
void PutSprite(TbRenderArea& area, const TbRect& clip, uint32_t flags,
               int32_t x, int32_t y, const TbSprite& sprite, const Writer& write) {
    const bool flip_x = (flags & 1) != 0;
    const bool flip_y = (flags & 2) != 0;
    const uint8_t* data = sprite.mData;
    for (uint32_t row = 0; row < sprite.mHeight; ++row) {
        int32_t column = 0;
        for (;;) {
            const int32_t count = static_cast<int8_t>(*data++);
            if (count == 0) break;
            if (count < 0) {
                column -= count;
                continue;
            }
            for (int32_t i = 0; i < count; ++i, ++column) {
                const uint8_t index = *data++;
                const int32_t px = flip_x ? x + int32_t(sprite.mWidth) - 1 - column : x + column;
                const int32_t py = flip_y ? y + int32_t(sprite.mHeight) - 1 - int32_t(row) : y + int32_t(row);
                if (px >= clip.left && px < clip.right && py >= clip.top && py < clip.bottom)
                    write(area.mData + ptrdiff_t(py) * area.mStride + ptrdiff_t(px) * Bytes, index);
            }
        }
    }
}

template<unsigned Bytes>
void PutOneColourClippedFlip(TbRenderArea& area, const TbRect& clip, uint32_t flags,
                             int32_t x, int32_t y, const TbSprite& sprite,
                             uint32_t formatted) {
    const bool flip_y = (flags & 2) != 0;
    const int32_t visible_left = x < clip.left ? clip.left : x;
    const int32_t sprite_right = Add(x, sprite.mWidth);
    const int32_t visible_right = sprite_right > clip.right ? clip.right : sprite_right;
    const uint8_t* data = sprite.mData;
    for (uint32_t row = 0; row < sprite.mHeight; ++row) {
        int32_t column = 0;
        for (;;) {
            const int32_t count = static_cast<int8_t>(*data++);
            if (count == 0) break;
            if (count < 0) {
                column -= count;
                continue;
            }
            data += count;
            for (int32_t i = 0; i < count; ++i, ++column) {
                const int32_t px = x + int32_t(sprite.mWidth) - 1 - column;
                const int32_t py = flip_y ? y + int32_t(sprite.mHeight) - 1 - int32_t(row)
                                          : y + int32_t(row);
                if (px < visible_left || px >= visible_right ||
                    py < clip.top || py >= clip.bottom) continue;
                // In this clipped flip branch the original decrements the
                // byte pointer by one, irrespective of pixel depth.
                const ptrdiff_t offset = ptrdiff_t(visible_right - 1) * Bytes -
                                         ptrdiff_t((visible_right - 1) - px);
                StorePixel<Bytes>(area.mData + ptrdiff_t(py) * area.mStride + offset, formatted);
            }
        }
    }
}

void PutPaletteSprite24(TbRenderArea& area, const TbRect& clip, uint32_t flags,
                        int32_t x, int32_t y, const TbSprite& sprite,
                        const uint32_t* palette) {
    const bool flip_x = (flags & 1) != 0;
    const bool flip_y = (flags & 2) != 0;
    const int32_t visible_left = x < clip.left ? clip.left : x;
    const int32_t sprite_right = Add(x, sprite.mWidth);
    const int32_t visible_right = sprite_right > clip.right ? clip.right : sprite_right;
    const uint8_t* data = sprite.mData;
    for (uint32_t row = 0; row < sprite.mHeight; ++row) {
        int32_t column = 0;
        for (;;) {
            const int32_t count = static_cast<int8_t>(*data++);
            if (count == 0) break;
            if (count < 0) {
                column -= count;
                continue;
            }
            for (int32_t i = 0; i < count; ++i, ++column) {
                const uint8_t index = *data++;
                const int32_t px = flip_x ? x + int32_t(sprite.mWidth) - 1 - column : x + column;
                const int32_t py = flip_y ? y + int32_t(sprite.mHeight) - 1 - int32_t(row)
                                          : y + int32_t(row);
                if (px < visible_left || px >= visible_right ||
                    py < clip.top || py >= clip.bottom) continue;
                ptrdiff_t offset;
                if (flip_x) {
                    offset = ptrdiff_t(visible_right - 1) * 3 -
                             ptrdiff_t((visible_right - 1) - px) * 4;
                } else {
                    offset = ptrdiff_t(visible_left) * 3 + ptrdiff_t(px - visible_left) * 4;
                }
                StorePixel<3>(area.mData + ptrdiff_t(py) * area.mStride + offset, palette[index]);
            }
        }
    }
}

template<unsigned Bytes, class Render>
void PixelMethod(const Render&, TbRenderArea* area, const TbRect& clip, uint32_t flags,
                 int32_t x, int32_t y, const CColour& colour) {
    PixelWriter_2<Bytes> write(*area, flags, colour, true);
    PutPixel<Bytes>(*area, clip, x, y, write);
}

template<unsigned Bytes, class Render>
void HorizontalMethod(const Render&, TbRenderArea* area, const TbRect& clip, uint32_t flags,
                      int32_t x, int32_t y, int32_t length, const CColour& colour) {
    PixelWriter_2<Bytes> write(*area, flags, colour);
    PutHorizontalOriginal<Bytes>(*area, clip, x, y, length, (flags & 0x18) != 0, write);
}

template<unsigned Bytes, class Render>
void VerticalMethod(const Render&, TbRenderArea* area, const TbRect& clip, uint32_t flags,
                    int32_t x, int32_t y, int32_t length, const CColour& colour) {
    PixelWriter_2<Bytes> write(*area, flags, colour);
    PutVertical<Bytes>(*area, clip, x, y, length, write);
}

template<unsigned Bytes, class Render>
void LineMethod(const Render& render, TbRenderArea* area, const TbRect& clip, uint32_t flags,
                int32_t x0, int32_t y0, int32_t x1, int32_t y1, const CColour& colour) {
    if (x0 == x1) {
        render.VerticalLine(area, clip, flags, x0, y0, y1 - y0, colour);
        return;
    }
    if (y0 == y1) {
        render.HorizontalLine(area, clip, flags, x0, y0, x1 - x0, colour);
        return;
    }
    if (!ClipLine(clip, x0, y0, x1, y1)) return;
    PixelWriter_2<Bytes> write(*area, flags, colour);
    PutDiagonal<Bytes>(*area, x0, y0, x1, y1, write);
}

template<unsigned Bytes, class Render>
void RectangleOutlineMethod(const Render& render, TbRenderArea* area, const TbRect& clip,
                            uint32_t flags, TbRect rect, const CColour& colour) {
    rect = Normalised(rect);
    if (!Intersects(rect, clip)) return;
    const int32_t width = rect.right - rect.left;
    const int32_t height = rect.bottom - rect.top;
    render.HorizontalLine(area, clip, flags, rect.left, rect.top, width - 1, colour);
    render.VerticalLine(area, clip, flags, rect.right - 1, rect.top, height - 1, colour);
    render.HorizontalLine(area, clip, flags, rect.left + 1, rect.bottom - 1, width - 1, colour);
    render.VerticalLine(area, clip, flags, rect.left, rect.top + 1, height - 1, colour);
}

template<unsigned Bytes, class Render>
void RectangleFilledMethod(const Render&, TbRenderArea* area, const TbRect& clip,
                           uint32_t flags, TbRect rect, const CColour& colour) {
    rect = Normalised(rect);
    if (!Intersects(rect, clip)) return;
    rect = Intersection(rect, clip);
    PixelWriter_2<Bytes> write(*area, flags, colour);
    if ((flags & 0x18) != 0 && (Bytes == 3 || Bytes == 4)) {
        for (int32_t y = rect.top; y < rect.bottom; ++y)
            PutHorizontalOriginal<Bytes>(*area, clip, rect.left, y,
                                         rect.right - rect.left, true, write);
        return;
    }
    PutRectangleFilled<Bytes>(*area, rect, write);
}

template<unsigned Bytes, class Render>
void TriangleMethod(const Render& render, TbRenderArea* area, const TbRect& clip, uint32_t flags,
                    int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                    int32_t x2, int32_t y2, const CColour& colour) {
    PutTriangle<Bytes>(*area, clip, flags, x0, y0, x1, y1, x2, y2,
        [&](int32_t x, int32_t y, int32_t length) {
            render.HorizontalLine(area, clip, flags, x, y, length, colour);
        },
        [&](int32_t ax, int32_t ay, int32_t bx, int32_t by) {
            render.Line(area, clip, flags, ax, ay, bx, by, colour);
        });
}

template<unsigned Bytes, class Render>
void OneColourSpriteMethod(const Render&, TbRenderArea* area, const TbRect& clip, uint32_t flags,
                           int32_t x, int32_t y, const TbSprite& sprite, const CColour& colour) {
    if (sprite.mWidth == 0 || sprite.mHeight == 0) return;
    const int32_t right = Add(x, sprite.mWidth);
    const int32_t bottom = Add(y, sprite.mHeight);
    if (right <= clip.left || x >= clip.right || bottom <= clip.top || y >= clip.bottom) return;
    const uint32_t formatted = colour.GetFormattedValue(*reinterpret_cast<const TbPixFormat*>(area));

    if ((flags & 1) != 0 && (x < clip.left || right > clip.right)) {
        PutOneColourClippedFlip<Bytes>(*area, clip, flags, x, y, sprite, formatted);
        return;
    }

    // The un-clipped, horizontally flipped fast path in the original skips
    // one extra stream byte after a transparent run. Preserve that shipped
    // behaviour; clipped sprites use the general decoder below.
    if ((flags & 1) != 0 && x >= clip.left && right <= clip.right) {
        const bool flip_y = (flags & 2) != 0;
        const uint8_t* data = sprite.mData;
        for (uint32_t row = 0; row < sprite.mHeight; ++row) {
            const int32_t py = flip_y ? y + int32_t(sprite.mHeight) - 1 - int32_t(row)
                                      : y + int32_t(row);
            uint8_t* destination = area->mData + ptrdiff_t(py) * area->mStride +
                                   ptrdiff_t(x + int32_t(sprite.mWidth) - 1) * Bytes;
            if (py >= clip.top && py < clip.bottom) {
                for (;;) {
                    const int32_t count = static_cast<int8_t>(*data);
                    if (count == 0) { ++data; break; }
                    if (count < 0) {
                        destination += ptrdiff_t(-count) * Bytes;
                        data += 2;
                    } else {
                        data += count + 1;
                        for (int32_t i = 0; i < count; ++i) {
                            StorePixel<Bytes>(destination, formatted);
                            destination += Bytes;
                        }
                    }
                }
            } else {
                while (*data != 0) {
                    const int32_t count = static_cast<int8_t>(*data++);
                    if (count > 0) data += count;
                }
                ++data;
            }
        }
        return;
    }
    PutSprite<Bytes>(*area, clip, flags, x, y, sprite,
        [formatted](uint8_t* destination, uint8_t) { StorePixel<Bytes>(destination, formatted); });
}

template<unsigned Bytes, class Render>
void SpriteMethod(const Render&, TbRenderArea* area, const TbRect& clip, uint32_t flags,
                  const uint32_t* palette, int32_t x, int32_t y, const TbSprite& sprite) {
    if (sprite.mWidth == 0 || sprite.mHeight == 0) return;
    if (Bytes == 3) {
        PutPaletteSprite24(*area, clip, flags, x, y, sprite, palette);
        return;
    }
    PutSprite<Bytes>(*area, clip, flags, x, y, sprite,
        [palette](uint8_t* destination, uint8_t index) {
            StorePixel<Bytes>(destination, palette[index]);
        });
}

template<unsigned Bytes, class Render>
void PropTextMethod(const Render&, TbRenderArea* area, const TbRect& clip, uint32_t flags,
                    int32_t x, int32_t y, const char* text, const CColour& colour) {
    if (area == nullptr || y < clip.top || y + 6 > clip.bottom ||
        x + 6 > clip.right || text == nullptr || *text == 0) return;
    PixelWriter_2<Bytes> write(*area, flags, colour);
    PutPropText<Bytes>(*area, clip, x, y, text, write);
}

} // namespace

#define TPW_DEFINE_RENDER_PRIMITIVES(Name, Bytes) \
void Name::Pixel(TbRenderArea* a,const TbRect& c,uint32_t f,int32_t x,int32_t y,const CColour& q) const { PixelMethod<Bytes>(*this,a,c,f,x,y,q); } \
void Name::HorizontalLine(TbRenderArea* a,const TbRect& c,uint32_t f,int32_t x,int32_t y,int32_t n,const CColour& q) const { HorizontalMethod<Bytes>(*this,a,c,f,x,y,n,q); } \
void Name::VerticalLine(TbRenderArea* a,const TbRect& c,uint32_t f,int32_t x,int32_t y,int32_t n,const CColour& q) const { VerticalMethod<Bytes>(*this,a,c,f,x,y,n,q); } \
void Name::Line(TbRenderArea* a,const TbRect& c,uint32_t f,int32_t x0,int32_t y0,int32_t x1,int32_t y1,const CColour& q) const { LineMethod<Bytes>(*this,a,c,f,x0,y0,x1,y1,q); } \
void Name::RectangleOutline(TbRenderArea* a,const TbRect& c,uint32_t f,const TbRect& r,const CColour& q) const { RectangleOutlineMethod<Bytes>(*this,a,c,f,r,q); } \
void Name::RectangleFilled(TbRenderArea* a,const TbRect& c,uint32_t f,const TbRect& r,const CColour& q) const { RectangleFilledMethod<Bytes>(*this,a,c,f,r,q); } \
void Name::Triangle(TbRenderArea* a,const TbRect& c,uint32_t f,int32_t x0,int32_t y0,int32_t x1,int32_t y1,int32_t x2,int32_t y2,const CColour& q) const { TriangleMethod<Bytes>(*this,a,c,f,x0,y0,x1,y1,x2,y2,q); } \
void Name::OneColourSprite(TbRenderArea* a,const TbRect& c,uint32_t f,int32_t x,int32_t y,const TbSprite& s,const CColour& q) const { OneColourSpriteMethod<Bytes>(*this,a,c,f,x,y,s,q); } \
void Name::Sprite(TbRenderArea* a,const TbRect& c,uint32_t f,const uint32_t* p,int32_t x,int32_t y,const TbSprite& s) const { SpriteMethod<Bytes>(*this,a,c,f,p,x,y,s); } \
void Name::PropText(TbRenderArea* a,const TbRect& c,uint32_t f,int32_t x,int32_t y,const char* t,const CColour& q) const { PropTextMethod<Bytes>(*this,a,c,f,x,y,t,q); }

TPW_DEFINE_RENDER_PRIMITIVES(CRenderClass8, 1)
TPW_DEFINE_RENDER_PRIMITIVES(CRenderClass16, 2)
TPW_DEFINE_RENDER_PRIMITIVES(CRenderClass24, 3)
TPW_DEFINE_RENDER_PRIMITIVES(CRenderClass32, 4)

#undef TPW_DEFINE_RENDER_PRIMITIVES

} // namespace draw
} // namespace bfl

// Unit w242: bfl::draw::CDraw state management (bullfrog symbols shared, Draw.cpp, 0x100651b8..0x100659a4).
// SetSpritePalette (0x10065a7c) is not part of this unit.

namespace bfl {
namespace draw {

// The four render-context objects Draw.cpp's static initialiser (0x100661a8) builds: one word each, the vtable of
// the 8-, 16-, 24- and 32-bit renderer (0x100739a4, 0x10073958, 0x1007390c, 0x100738c0). CDraw only takes their
// addresses, so on the host they are opaque words.
CDrawContext gDrawContext8{};    // 0x100748dc
CDrawContext gDrawContext16{};   // 0x100748d8
CDrawContext gDrawContext24{};   // 0x100748d4
CDrawContext gDrawContext32{};   // 0x100748d0

namespace {

// SetCurrent's format test: the four mask words, the depth and whether the area is palettised (as a bool).
bool SameFormat(const TbRenderArea& a, const TbRenderArea& b) {
    return std::memcmp(a.mF00, b.mF00, sizeof(a.mF00)) == 0 &&
           a.mDepth == b.mDepth &&
           ((a.mPalettised == 0) == (b.mPalettised == 0));
}

}  // namespace

// 0x100651b8: both areas through the library's out-of-line constructor (0x10016630, which leaves the masks alone),
// then the clip, mCurrent's data pointer, the context, the surface and the dirty flag are zeroed.
CDraw::CDraw() {
    mClipRect = {0, 0, 0, 0};
    mCurrent.mData = nullptr;
    mRenderContext = nullptr;
    mSurface = nullptr;
    mPaletteDirty = 0;
}

// 0x1006524c: nothing to release.
CDraw::~CDraw() = default;

// 0x100652bc: a null area changes nothing. A format change marks the palette dirty; the area becomes both the
// current area and the view port, the clip covers it, and the context follows the depth (none when palettised).
void CDraw::SetCurrent(TbRenderArea* area) {
    if (!area) return;
    if (!SameFormat(mCurrent, *area)) mPaletteDirty = 1;
    mCurrent = *area;
    mViewPort = mCurrent;
    mClipRect = {0, 0, area->mWidth, area->mPictureHeight};
    if (mCurrent.mPalettised != 0) {
        mRenderContext = nullptr;
        return;
    }
    switch (mCurrent.mDepth) {
    case 8: mRenderContext = &gDrawContext8; break;
    case 16: mRenderContext = &gDrawContext16; break;
    case 24: mRenderContext = &gDrawContext24; break;
    case 32: mRenderContext = &gDrawContext32; break;
    default: mRenderContext = nullptr; break;
    }
}

// 0x1006552c: a null surface only clears the context; otherwise a copy of the surface's area goes to the other
// overload.
void CDraw::SetCurrent(TbSurface* surface) {
    if (!surface) {
        mRenderContext = nullptr;
        return;
    }
    TbRenderArea area = surface->GetRenderArea();
    SetCurrent(&area);
}

// 0x1006560c: the view port's rectangle cut by `rect`.
void CDraw::SetClipRect(const TbRect& rect) {
    ClearClipRect();
    mClipRect = mClipRect.Intersection(rect);
}

// 0x100656f0: `rect` cut to the current area becomes the view port, and the clip is the whole view port cut by the
// cut rectangle's size.
void CDraw::SetViewPort(const TbRect& rect) {
    const TbRect bounds{0, 0, mCurrent.mWidth, mCurrent.mPictureHeight};
    const TbRect clipped = bounds.Intersection(rect);
    mCurrent.SetupClientRA(mViewPort, clipped);
    const TbRect size{0, 0, clipped.right - clipped.left, clipped.bottom - clipped.top};
    ClearClipRect();
    mClipRect = mClipRect.Intersection(size);
}

const TbRenderArea& CDraw::GetCurrent() const { return mCurrent; }   // 0x10065868
const TbRect& CDraw::GetClipRect() const { return mClipRect; }       // 0x100658a4
const TbRenderArea& CDraw::GetViewPort() const { return mViewPort; } // 0x100658e0

// 0x1006591c: the clip covers the whole view port.
void CDraw::ClearClipRect() {
    mClipRect = {0, 0, mViewPort.mWidth, mViewPort.mPictureHeight};
}

// 0x100659a4: the view port goes back to the whole current area.
void CDraw::ClearViewPort() {
    mViewPort = mCurrent;
    ClearClipRect();
}

}  // namespace draw
}  // namespace bfl

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

// bullfrog TbSystemInfo.cpp, LbDraw.cpp, WinScrn.cpp, LbMaths.cpp, TbJPEGSaver.cpp, Draw.cpp, LbSCBack.cpp small functions (unit v51071).

// The four vtables of the draw contexts (initialised data, 0x4c bytes each, 0x100738c0..0x100739f0). The Mac's hold a
// header pointer, then twelve TVector pointers (CircleOutline, CircleFilled, HorizontalLine, VerticalLine, Line, Pixel,
// PropText, RectangleOutline, RectangleFilled, Triangle, OneColourSprite, Sprite of CRenderClass32/24/16/8, which the
// RenderContext8 translations stand for) and more code pointers. Nothing on the host dispatches through them (CDraw
// takes the address of the contexts and ui_2dmap only compares them), and a 32-bit TVector pointer cannot be stored as a
// host function pointer, so the arrays keep their size and stay zero.
alignas(8) uint8_t kDrawContextVtable8[0x4c];    // 0x100739a4
alignas(8) uint8_t kDrawContextVtable16[0x4c];   // 0x10073958
alignas(8) uint8_t kDrawContextVtable24[0x4c];   // 0x1007390c
alignas(8) uint8_t kDrawContextVtable32[0x4c];   // 0x100738c0

void __sinit_Draw_cpp() {   // 0x100661a8
    bfl::draw::gDrawContext8.mVtable = kDrawContextVtable8;
    bfl::draw::gDrawContext16.mVtable = kDrawContextVtable16;
    bfl::draw::gDrawContext24.mVtable = kDrawContextVtable24;
    bfl::draw::gDrawContext32.mVtable = kDrawContextVtable32;
}

// Unit v51127: small bullfrog functions (LbSCBack.cpp application globals, the _LbSEFunction list instances, CConverter and TbJPEGLoader
// destructors, SamsUtilities swap helpers, TbPoint, _LbSemaLock stand-ins). Mac addresses are in the comments.

void _LbSemaLock_Release(_LbSemaLock* self) { (void)self; }

// TbPoint::TbPoint(int, int) 0x100591c4.
void TbPoint_Construct(TbPoint* self, int32_t x, int32_t y) {
    self->x = x;
    self->y = y;
}

namespace SamsUtilities {
template<class T> void USwapBlock32(T* words, int32_t count) {
    _USwapBlock32(reinterpret_cast<uint32_t*>(words), count);
}
template<class T> void USwapInplace32(T& word) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    const uint32_t value = static_cast<uint32_t>(word);
    word = static_cast<T>((value >> 24) | ((value >> 8) & 0xff00) |
                          ((value << 8) & 0xff0000) | (value << 24));
#else
    (void)word;
#endif
}
template void USwapBlock32<uint32_t>(uint32_t*, int32_t);
template void USwapInplace32<uint32_t>(uint32_t&);
}
