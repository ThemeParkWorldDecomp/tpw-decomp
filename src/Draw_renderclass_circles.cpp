// Unit w239: Draw.cpp's eight CRenderClass circle rasterizers.
#include "DrawRenderClasses.h"

#include <cstddef>
#include <initializer_list>

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
