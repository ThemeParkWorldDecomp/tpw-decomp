// The Bullfrog library's rectangle, viewport, clip and palette leaves (bullfrog symbols shared 2.04b3): LbRect_Intersection
// 0x10029ad0, LbRect_Empty 0x10029cd4, LbRect_Normalise 0x10029d14 (the rectangle helpers), LbDraw_SetViewPort 0x1002b1c0,
// LbDraw_SetClipRect 0x1002b2c4, LbDraw_SetCurrent 0x1002b588 (the first overload; LbDraw.cpp) and LbPalette_FindColour
// 0x1001a5c8 (Lbgfsave.cpp). The drawing state they keep is declared in include/LbDraw.h.
#include <cstddef>

#include "LbDraw.h"
#include "LbPalette.h"
#include "LbRect.h"

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

// A render area starts with the six words of a pixel format (the masks, the depth and the palettised flag).
static_assert(offsetof(TbRenderArea, mDepth) == offsetof(TbPixFormat, mBitsPerPixel), "an area begins with its format");
static_assert(offsetof(TbRenderArea, mPalettised) == offsetof(TbPixFormat, mPalettised), "an area begins with its format");

// ---- the rectangle helpers

// 0x10029ad0: each side takes the stricter of the two, then the answer says whether anything is left. The rectangle is
// cut down even when the answer is 0.
uint8_t LbRect_Intersection(TbRect* rect, const TbRect* other) {
    rect->left = rect->left > other->left ? rect->left : other->left;
    rect->top = rect->top > other->top ? rect->top : other->top;
    rect->right = rect->right < other->right ? rect->right : other->right;
    rect->bottom = rect->bottom < other->bottom ? rect->bottom : other->bottom;
    return rect->left < rect->right && rect->top < rect->bottom ? 1 : 0;
}

// 0x10029cd4: nothing inside, at the top left corner.
void LbRect_Empty(TbRect* rect) {
    rect->right = rect->left;
    rect->bottom = rect->top;
}

// 0x10029d14
void LbRect_Normalise(TbRect* rect) {
    if (rect->left > rect->right) {
        const int32_t left = rect->left;
        rect->left = rect->right;
        rect->right = left;
    }
    if (rect->top > rect->bottom) {
        const int32_t top = rect->top;
        rect->top = rect->bottom;
        rect->bottom = top;
    }
}

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

// ---- LbPalette_FindColour (Lbgfsave.cpp)

namespace {

// |a - b| for the Manhattan distance (the original calls labs).
int32_t Distance(int32_t a, int32_t b) { return a > b ? a - b : b - a; }

}  // namespace

// 0x1001a5c8. A palette is 256 entries of four bytes, red, green, blue and one the search does not look at. The answer is
// the entry nearest to the colour by the squared distance; the first exact match wins at once. Several equally near
// entries are told apart by the sum of the absolute differences, and entries still tied by the weighted sum of squares
// (red and green twice, blue once) of the entry itself, the smallest of which wins; the first of equal ones.
uint8_t LbPalette_FindColour(const TbPalette* palette, uint8_t red, uint8_t green, uint8_t blue) {
    const uint8_t* entries = reinterpret_cast<const uint8_t*>(palette);
    int32_t nearest = 0x1000000;
    for (uint32_t i = 0; i < 256; ++i) {
        const uint8_t* entry = entries + i * 4;
        const int32_t dr = static_cast<int32_t>(red) - entry[0];
        const int32_t dg = static_cast<int32_t>(green) - entry[1];
        const int32_t db = static_cast<int32_t>(blue) - entry[2];
        const int32_t squared = dr * dr + dg * dg + db * db;
        if (squared < nearest) {
            nearest = squared;
            if (squared == 0) return static_cast<uint8_t>(i);
        }
    }
    // The entries that are as near as the nearest, in order.
    uint8_t tied[256];
    uint32_t tiedCount = 0;
    for (uint32_t i = 0; i < 256; ++i) {
        const uint8_t* entry = entries + i * 4;
        const int32_t dr = static_cast<int32_t>(red) - entry[0];
        const int32_t dg = static_cast<int32_t>(green) - entry[1];
        const int32_t db = static_cast<int32_t>(blue) - entry[2];
        if (dr * dr + dg * dg + db * db == nearest) tied[tiedCount++] = static_cast<uint8_t>(i);
    }
    if (tiedCount == 1) return tied[0];
    int32_t shortest = 0x1000000;
    for (uint32_t i = 0; i < tiedCount; ++i) {
        const uint8_t* entry = entries + tied[i] * 4;
        const int32_t sum = Distance(red, entry[0]) + Distance(green, entry[1]) + Distance(blue, entry[2]);
        if (sum < shortest) shortest = sum;
    }
    uint8_t shortTied[256];
    uint32_t shortCount = 0;
    for (uint32_t i = 0; i < tiedCount; ++i) {
        const uint8_t* entry = entries + tied[i] * 4;
        const int32_t sum = Distance(red, entry[0]) + Distance(green, entry[1]) + Distance(blue, entry[2]);
        if (sum == shortest) shortTied[shortCount++] = tied[i];
    }
    if (shortCount == 1) return shortTied[0];
    uint8_t answer = 0;
    uint32_t lightest = 0x1000000;
    for (uint32_t i = 0; i < shortCount; ++i) {
        const uint8_t* entry = shortTied[i] * 4 + entries;
        const uint32_t weight = static_cast<uint32_t>(entry[0]) * entry[0] * 2 + static_cast<uint32_t>(entry[1]) * entry[1] * 2 +
                                static_cast<uint32_t>(entry[2]) * entry[2];
        if (weight < lightest) {
            lightest = weight;
            answer = shortTied[i];
        }
    }
    return answer;
}

// unit w45b
// 0x1002b554 (8 bytes): the address of the clip rectangle (the TOC word 0x10074860). The 4-bit font render method (ltms) reads the
// clip through it.
TbRect* LbDraw_GetClipRect() { return &_lbCurrentClipRect; }
