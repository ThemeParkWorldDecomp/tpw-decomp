// Unit w242: remaining legacy LbDraw dispatch and sprite helpers.
#include "LbDrawPrimitives.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>

#include "LbDraw.h"
#include "LbDrawText.h"
#include "LbPalette.h"
#include "LbRect.h"
#include "LbSurface.h"
#include "RenderContext8.h"
#include "RenderContextDepthCircles.h"

namespace {

bool Intersects(const TbRect& a, const TbRect& b) {
    return a.left <= b.right && b.left <= a.right &&
           a.top <= b.bottom && b.top <= a.bottom;
}

TbRect Normalised(TbRect rect) {
    LbRect_Normalise(&rect);
    return rect;
}

template <typename F>
void Context(F&& call) {
    const int32_t depth = _lbpRenderBase ? _lbpRenderBase->mDepth : 8;
    if (depth == 16) call(reinterpret_cast<RenderContext16*>(_lbpRenderBase));
    else if (depth == 24) call(reinterpret_cast<RenderContext24*>(_lbpRenderBase));
    else if (depth == 32) call(reinterpret_cast<RenderContext32*>(_lbpRenderBase));
    else call(reinterpret_cast<RenderContext8*>(_lbpRenderBase));
}

bool Visible(TbRect bounds) {
    bounds = Normalised(bounds);
    return Intersects(_lbCurrentClipRect, bounds);
}

TbColour Colour(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue,
                uint8_t index) {
    TbColour colour;
    colour.mAlpha = alpha;
    colour.mRed = red;
    colour.mGreen = green;
    colour.mBlue = blue;
    colour.mIndex = index;
    return colour;
}

} // namespace

void LbPalette_ToCompact(uint8_t* compact, const TbPalette* palette,
                         uint32_t count) {
    const auto* source = reinterpret_cast<const uint8_t*>(palette);
    for (uint32_t i = 0; i < count; ++i) {
        compact[i * 3] = source[i * 4];
        compact[i * 3 + 1] = source[i * 4 + 1];
        compact[i * 3 + 2] = source[i * 4 + 2];
    }
}

void LbDraw_OneColourSprite(int32_t x, int32_t y, const TbSprite* sprite,
                            TbColour colour) {
    Context([&](auto* c) { c->LbDraw_OneColourSprite(x, y, sprite, colour); });
}

void LbDraw_Sprite(int32_t x, int32_t y, const TbSprite* sprite) {
    Context([&](auto* c) { c->LbDraw_Sprite(x, y, sprite); });
}

void LbDraw_RemappedSprite(int32_t x, int32_t y, const TbSprite* sprite,
                           const uint8_t* remap) {
    const uint32_t width = static_cast<uint8_t>(sprite->mWidth);
    const uint32_t height = static_cast<uint8_t>(sprite->mHeight);
    if (x + static_cast<int32_t>(width) <= _lbCurrentClipRect.left ||
        x >= _lbCurrentClipRect.right ||
        y + static_cast<int32_t>(height) <= _lbCurrentClipRect.top ||
        y >= _lbCurrentClipRect.bottom) return;

    const bool flipX = (_lbCurrentDrawFlags & 1) != 0;
    const bool flipY = (_lbCurrentDrawFlags & 2) != 0;
    const bool blend = (_lbCurrentDrawFlags & 0x18) != 0;
    const bool inverseBlend = (_lbCurrentDrawFlags & 8) != 0;
    const uint8_t* stream = sprite->mData;
    for (uint32_t sourceY = 0; sourceY < height; ++sourceY) {
        uint32_t sourceX = 0;
        for (;;) {
            const int8_t run = static_cast<int8_t>(*stream++);
            if (run == 0) break;
            if (run < 0) {
                sourceX += static_cast<uint32_t>(-run);
                continue;
            }
            for (int32_t i = 0; i < run; ++i, ++sourceX) {
                const uint8_t mapped = remap[*stream++];
                const int32_t dx = x + static_cast<int32_t>(flipX ? width - 1 - sourceX : sourceX);
                const int32_t dy = y + static_cast<int32_t>(flipY ? height - 1 - sourceY : sourceY);
                if (dx < _lbCurrentClipRect.left || dx >= _lbCurrentClipRect.right ||
                    dy < _lbCurrentClipRect.top || dy >= _lbCurrentClipRect.bottom) continue;
                uint8_t* destination = _lbCurrentOriginPtr +
                    dy * _lbpCurrentRA->mStride + dx;
                if (!blend) *destination = mapped;
                else if (!inverseBlend)
                    *destination = _lbCurrentGhostTable[uint32_t(*destination) * 256 + mapped];
                else
                    *destination = _lbCurrentGhostTable[uint32_t(mapped) * 256 + *destination];
            }
        }
    }
}

void LbDraw_Pixel(int32_t x, int32_t y, TbColour colour) {
    Context([&](auto* c) { c->LbDraw_Pixel(x, y, colour); });
}

void LbDraw_Rectangle(const TbRect* rect, TbColour colour) {
    LbDraw_Rectangle(rect ? *rect : _lbCurrentClipRect, colour);
}

void LbDraw_Rectangle(const TbRect& rect, TbColour colour) {
    if (!Visible(rect)) return;
    Context([&](auto* c) { c->LbDraw_Rectangle(rect, colour); });
}

void LbDraw_RectangleOutline(const TbRect* rect, TbColour colour) {
    LbDraw_RectangleOutline(rect ? *rect : _lbCurrentClipRect, colour);
}

void LbDraw_RectangleOutline(const TbRect& rect, TbColour colour) {
    if (!Visible(rect)) return;
    Context([&](auto* c) { c->LbDraw_RectangleOutline(rect, colour); });
}

void LbDraw_Line(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                 TbColour colour) {
    if (!Visible({x0, y0, x1, y1})) return;
    Context([&](auto* c) { c->LbDraw_Line(x0, y0, x1, y1, colour); });
}

void LbDraw_HorizontalLine(int32_t x, int32_t y, int32_t length,
                           TbColour colour) {
    if (!Visible({x, y, x + length, y + 1})) return;
    Context([&](auto* c) { c->LbDraw_HorizontalLine(x, y, length, colour); });
}

void LbDraw_VerticalLine(int32_t x, int32_t y, int32_t length,
                         TbColour colour) {
    if (!Visible({x, y, x + 1, y + length})) return;
    Context([&](auto* c) { c->LbDraw_VerticalLine(x, y, length, colour); });
}

void LbDraw_Circle(int32_t x, int32_t y, uint32_t radius, TbColour colour) {
    if (!Visible({x - static_cast<int32_t>(radius), y - static_cast<int32_t>(radius),
                  x + static_cast<int32_t>(radius), y + static_cast<int32_t>(radius)})) return;
    Context([&](auto* c) { c->LbDraw_Circle(x, y, radius, colour); });
}

void LbDraw_CircleOutline(int32_t x, int32_t y, uint32_t radius,
                          TbColour colour) {
    if (!Visible({x - static_cast<int32_t>(radius), y - static_cast<int32_t>(radius),
                  x + static_cast<int32_t>(radius), y + static_cast<int32_t>(radius)})) return;
    Context([&](auto* c) { c->LbDraw_CircleOutline(x, y, radius, colour); });
}

void LbDraw_CircleFilled(int32_t x, int32_t y, uint32_t radius,
                         TbColour colour) {
    if (!Visible({x - static_cast<int32_t>(radius), y - static_cast<int32_t>(radius),
                  x + static_cast<int32_t>(radius), y + static_cast<int32_t>(radius)})) return;
    Context([&](auto* c) { c->LbDraw_CircleFilled(x, y, radius, colour); });
}

// 0x1002ab34: the global palette as 16 rows of 16 filled cells of width x height, with the draw flags off. With
// `labels`, each cell gets its index in decimal ("%d", 0x1006ca05), black on a cell whose red + green + blue is above
// 255 and white otherwise (both found in the palette first).
void LbDraw_Palette(int32_t x, int32_t y, uint32_t width, uint32_t height,
                    int32_t labels) {
    const uint32_t saved = _lbCurrentDrawFlags;
    _lbCurrentDrawFlags = 0;
    const auto* palette = reinterpret_cast<const uint8_t*>(&_lbGlobalPalette);
    const TbColour black = Colour(0xff, 0, 0, 0, LbPalette_FindColour(&_lbGlobalPalette, 0, 0, 0));
    const TbColour white = Colour(0xff, 0xff, 0xff, 0xff, LbPalette_FindColour(&_lbGlobalPalette, 255, 255, 255));
    uint8_t index = 0;
    uint32_t top = static_cast<uint32_t>(y);
    for (int row = 0; row < 16; ++row, top += height) {
        uint32_t left = static_cast<uint32_t>(x);
        for (int column = 0; column < 16; ++column, left += width, ++index) {
            const TbColour colour = Colour(0xff, palette[index * 4], palette[index * 4 + 1],
                                           palette[index * 4 + 2], index);
            const TbRect rect{static_cast<int32_t>(left), static_cast<int32_t>(top),
                              static_cast<int32_t>(left + width), static_cast<int32_t>(top + height)};
            LbDraw_RectangleFilled(&rect, colour);
            if (labels) {
                char text[13];
                std::snprintf(text, sizeof(text), "%d", index);
                const int32_t sum = colour.mRed + colour.mGreen + colour.mBlue;
                LbDraw_PropText(rect.left, rect.top, text, sum > 0xff ? black : white);
            }
        }
    }
    _lbCurrentDrawFlags = saved;
}

void LbDraw_Triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                     int32_t x2, int32_t y2, TbColour colour) {
    TbRect bounds{std::min({x0, x1, x2}), std::min({y0, y1, y2}),
                  std::max({x0, x1, x2}), std::max({y0, y1, y2})};
    if (!Visible(bounds)) return;
    Context([&](auto* c) { c->LbDraw_Triangle(x0, y0, x1, y1, x2, y2, colour); });
}

void LbDraw_ClearClipRect(TbColour colour) {
    Context([&](auto* c) { c->LbDraw_ClearClipWindow(colour); });
}

void LbDraw_RectangleFilled(const TbRect* rect, TbColour colour) {
    LbDraw_RectangleFilled(rect ? *rect : _lbCurrentClipRect, colour);
}

void LbDraw_RectangleFilled(const TbRect& rect, TbColour colour) {
    if (!Visible(rect)) return;
    Context([&](auto* c) { c->LbDraw_RectangleFilled(rect, colour); });
}

void LbDraw_PropText(int32_t x, int32_t y, const char* text, TbColour colour) {
    Context([&](auto* c) { c->LbDraw_PropText(x, y, text, colour); });
}

void LbDraw_UnicodePropText(int32_t x, int32_t y, const char16_t* text,
                            TbColour colour) {
    Context([&](auto* c) { c->LbDraw_UnicodePropText(x, y, text, colour); });
}

void LbDraw_ReleaseViewPort() { LbDraw_SetViewPort(nullptr); }
void LbDraw_ReleaseClipRect() { LbDraw_SetClipRect(nullptr); }
TbRect* LbDraw_GetViewPort() { return &_lbCurrentViewPort; }

void LbDraw_SetCurrent(TbSurface* surface) {
    if (!surface) {
        _lbpCurrentRA = nullptr;
        return;
    }
    _lbpCurrentRA = const_cast<TbRenderArea*>(&surface->GetRenderArea());
    _lbCurrentOriginPtr = _lbpCurrentRA->mData;
    _lbCurrentDrawFlags = surface->mDrawFlags;
    _lbCurrentGhostTable = surface->mGhostTable;
    _lbpCurrentFadeTable = surface->mFadeTable;
    _lbpCurrentFadeRow = surface->mFadeRow;
    LbDraw_SetViewPort(nullptr);
    if (_lbpCurrentRA->mDepth == 16) _lbpRenderBase = &_lbRenderContext16;
    else if (_lbpCurrentRA->mDepth == 24) _lbpRenderBase = &_lbRenderContext24;
    else if (_lbpCurrentRA->mDepth == 32) _lbpRenderBase = &_lbRenderContext32;
    else _lbpRenderBase = &_lbRenderContext8;
    _lbCurrentPixelCaps.Set(*reinterpret_cast<const TbPixFormat*>(_lbpCurrentRA));
}

void LbDraw_ScaledSprite(int32_t x, int32_t y, const TbSprite* sprite,
                         uint32_t width, uint32_t height) {
    LbDraw_SetSpriteScalingData(x, y, sprite->mWidth, sprite->mHeight, width, height);
}

void LbDraw_SetSpriteScalingData(int32_t, int32_t, uint32_t, uint32_t,
                                 uint32_t, uint32_t) {
    gLbSpriteDrawState[0] = _lbpCurrentRA->mStride;
    gLbSpriteDrawState[1] = LbRect_Width(&_lbCurrentClipRect);
    gLbSpriteDrawState[2] = LbRect_Height(&_lbCurrentClipRect);
    gLbSpriteDrawState[3] = static_cast<int32_t>(_lbCurrentDrawFlags);
    const uintptr_t origin = reinterpret_cast<uintptr_t>(_lbCurrentOriginPtr) +
        (static_cast<uint32_t>(_lbCurrentClipRect.left) *
         static_cast<uint32_t>(_lbpCurrentRA->mDepth) >> 3) +
        static_cast<uint32_t>(_lbCurrentClipRect.top) *
        static_cast<uint32_t>(_lbpCurrentRA->mStride);
    gLbSpriteDrawState[4] = static_cast<int32_t>(origin);
    if (_lbCurrentDrawFlags & 0x18)
        gLbSpriteDrawState[5] = static_cast<int32_t>(reinterpret_cast<uintptr_t>(_lbCurrentGhostTable));
    if (_lbCurrentDrawFlags & 0x20) {
        gLbSpriteDrawState[6] = static_cast<int32_t>(reinterpret_cast<uintptr_t>(_lbpCurrentFadeTable));
        gLbSpriteDrawState[7] = static_cast<int32_t>(reinterpret_cast<uintptr_t>(_lbpCurrentFadeRow));
    }
}
