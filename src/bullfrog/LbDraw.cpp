// LbDraw.cpp: name proven by __sinit_LbDraw_cpp in the Bullfrog library.

#include "helpers/LbDrawHelpers.h"
#include "LbDraw.h"
#include "LbDrawPrimitives.h"
#include "LbDrawText.h"
#include "LbPalette.h"
#include "LbRect.h"
#include "LbSurface.h"
#include "RenderContext8.h"
#include "RenderContextDepthCircles.h"
#include "TpwPropFont.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Unit w242: remaining legacy LbDraw dispatch and sprite helpers.

namespace {

bool Intersects(const TbRect& a, const TbRect& b) {
    return a.left <= b.right && b.left <= a.right &&
           a.top <= b.bottom && b.top <= a.bottom;
}

TbRect Normalised(TbRect rect) {
    LbRect_Normalise(&rect);
    return rect;
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

// The Bullfrog library's rectangle, viewport, clip and palette leaves (bullfrog symbols shared 2.04b3): LbRect_Intersection
// 0x10029ad0, LbRect_Empty 0x10029cd4, LbRect_Normalise 0x10029d14 (the rectangle helpers), LbDraw_SetViewPort 0x1002b1c0,
// LbDraw_SetClipRect 0x1002b2c4, LbDraw_SetCurrent 0x1002b588 (the first overload; LbDraw.cpp) and LbPalette_FindColour
// 0x1001a5c8 (Lbgfsave.cpp). The drawing state they keep is declared in include/LbDraw.h.

// ---- the drawing state (LbDraw.cpp). __sinit_LbDraw_cpp (0x1002b7dc, untranslated) zeroes the rectangles and the pixel
// capabilities and gives each context its vtable; every other word starts as zero in the BSS.
TbRenderArea* _lbpCurrentRA = nullptr;
TbRect _lbCurrentViewPort = {0, 0, 0, 0};
TbRect _lbCurrentClipRect = {0, 0, 0, 0};
uint8_t* _lbCurrentOriginPtr = nullptr;
RenderContext* _lbpRenderBase = nullptr;
RenderContext _lbRenderContext8 = {8};
RenderContext _lbRenderContext16 = {16};
RenderContext _lbRenderContext24 = {24};
RenderContext _lbRenderContext32 = {32};
TbPixelCaps _lbCurrentPixelCaps = {};

uint8_t LbRect_Intersection(TbRect* rect, const TbRect* other);

void LbRect_Empty(TbRect* rect);

void LbRect_Normalise(TbRect* rect);

// ---- the viewport, the clip rectangle and the current area

// 0x1002b1c0. The origin pointer is the area's pixels plus left times the bytes of a pixel (the depth shifted right by 3,
// unsigned) plus top times the stride, the sum worked out in 32 bits as the Mac does.
void LbDraw_SetViewPort(const TbRect* rect) {
    if (rect != nullptr) {
        _lbCurrentViewPort.left = rect->left;
        _lbCurrentViewPort.top = rect->top;
        _lbCurrentViewPort.right = rect->right;
        _lbCurrentViewPort.bottom = rect->bottom;
    } else {
        _lbCurrentViewPort.left = 0;
        _lbCurrentViewPort.top = 0;
        _lbCurrentViewPort.right = _lbpCurrentRA->mWidth;
        _lbCurrentViewPort.bottom = _lbpCurrentRA->mPictureHeight;
    }
    LbRect_Normalise(&_lbCurrentViewPort);
    const uint32_t bytesOfPixel = static_cast<uint32_t>(_lbpCurrentRA->mDepth) >> 3;
    const uint32_t offset = static_cast<uint32_t>(_lbCurrentViewPort.left) * bytesOfPixel +
                            static_cast<uint32_t>(_lbCurrentViewPort.top) * static_cast<uint32_t>(_lbpCurrentRA->mStride);
    _lbCurrentOriginPtr = _lbpCurrentRA->mData + static_cast<int32_t>(offset);
    LbDraw_SetClipRect(nullptr);
}

// 0x1002b2c4. `wanted` is the clip rectangle asked for in viewport coordinates (the whole viewport for NULL, a rectangle
// cut down to the viewport otherwise; a rectangle that leaves nothing is made empty). The clip rectangle proper is that
// cut down again to the area, which in viewport coordinates runs from minus the viewport's corner to the area's size
// minus the corner; if nothing is left of that either, all four words are 0.
void LbDraw_SetClipRect(const TbRect* rect) {
    TbRect wanted = {0, 0, 0, 0};
    const int32_t viewWidth = _lbCurrentViewPort.right - _lbCurrentViewPort.left;
    const int32_t viewHeight = _lbCurrentViewPort.bottom - _lbCurrentViewPort.top;
    if (rect != nullptr) {
        wanted.left = rect->left;
        wanted.top = rect->top;
        wanted.right = rect->right;
        wanted.bottom = rect->bottom;
        LbRect_Normalise(&wanted);
        TbRect view = {0, 0, viewWidth, viewHeight};
        if (LbRect_Intersection(&wanted, &view) == 0) LbRect_Empty(&wanted);
    } else {
        wanted.left = 0;
        wanted.top = 0;
        wanted.right = viewWidth;
        wanted.bottom = viewHeight;
    }
    _lbCurrentClipRect.left = -_lbCurrentViewPort.left;
    _lbCurrentClipRect.top = -_lbCurrentViewPort.top;
    _lbCurrentClipRect.right = _lbpCurrentRA->mWidth - _lbCurrentViewPort.left;
    _lbCurrentClipRect.bottom = _lbpCurrentRA->mPictureHeight - _lbCurrentViewPort.top;
    if (LbRect_Intersection(&_lbCurrentClipRect, &wanted) == 0) {
        _lbCurrentClipRect.left = 0;
        _lbCurrentClipRect.top = 0;
        _lbCurrentClipRect.right = 0;
        _lbCurrentClipRect.bottom = 0;
    }
}

void LbDraw_ReleaseViewPort() { LbDraw_SetViewPort(nullptr); }
void LbDraw_ReleaseClipRect() { LbDraw_SetClipRect(nullptr); }
TbRect* LbDraw_GetViewPort() { return &_lbCurrentViewPort; }

// unit w45b
// 0x1002b554 (8 bytes): the address of the clip rectangle (the TOC word 0x10074860). The 4-bit font render method (ltms) reads the
// clip through it.
TbRect* LbDraw_GetClipRect() { return &_lbCurrentClipRect; }
// 0x1002b588. A depth of 8, 16, 24 or 32 bits picks its own context; every other depth, in or out of 8..32, the 8-bit one.
void LbDraw_SetCurrent(TbRenderArea* area) {
    if (area == nullptr) {
        _lbpCurrentRA = nullptr;
        return;
    }
    _lbpCurrentRA = area;
    _lbCurrentOriginPtr = area->mData;
    LbDraw_SetViewPort(nullptr);
    switch (_lbpCurrentRA->mDepth) {
    case 16: _lbpRenderBase = &_lbRenderContext16; break;
    case 24: _lbpRenderBase = &_lbRenderContext24; break;
    case 32: _lbpRenderBase = &_lbRenderContext32; break;
    default: _lbpRenderBase = &_lbRenderContext8; break;
    }
    _lbCurrentPixelCaps.Set(*reinterpret_cast<const TbPixFormat*>(_lbpCurrentRA));
}

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

// bullfrog 0x1002b7dc, 148 B: LbDraw.cpp's static initialiser. Zeroes the viewport (0x10074870), the clip rectangle
// (0x10074860) and the pixel capabilities (0x10074838, 32 bytes), then stores the vtable addresses of the four render
// contexts (0x10073720, 0x1007360c, 0x100736c4, 0x10073668) as the only word of _lbRenderContext8/16/24/32.
// On the host the contexts are placeholder objects holding the depth, and the vtables are not translated, so the
// context stores are not reproduced here (the placeholder's mDepth must stay intact).
void __sinit_LbDraw_cpp() {
    std::memset(&_lbCurrentViewPort, 0, sizeof(_lbCurrentViewPort));
    std::memset(&_lbCurrentClipRect, 0, sizeof(_lbCurrentClipRect));
    std::memset(&_lbCurrentPixelCaps, 0, sizeof(_lbCurrentPixelCaps));
}
