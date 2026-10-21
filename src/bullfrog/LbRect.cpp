// LbRect.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "AudioPlaceHolder.h"
#include "bfl_Refpack.h"
#include "DrawState.h"
#include "LbDraw.h"
#include "LbDrawText.h"
#include "LbPalette.h"
#include "LbRect.h"
#include "LbSCBack.h"
#include "LbSurface.h"
#include "platform/macdoze.h"
#include "TbFPUPrecision.h"
#include "TbGraphicFile.h"
#include "WinScrn.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>
#include <type_traits>

// bullfrog TbSystemInfo.cpp, LbDraw.cpp, WinScrn.cpp, LbMaths.cpp, TbJPEGSaver.cpp, Draw.cpp, LbSCBack.cpp small functions (unit v51071).

void LbPoint_Set(TbPoint* point, int32_t x, int32_t y) {   // 0x10029760
    point->x = x;
    point->y = y;
}
void LbPoint_Offset(TbPoint* point, int32_t dx, int32_t dy) {   // 0x10029798
    point->x += dx;
    point->y += dy;
}

void LbRect_Set(TbRect* rect, int32_t left, int32_t top, int32_t right, int32_t bottom) {   // 0x100297e4
    rect->left = left;
    rect->top = top;
    rect->right = right;
    rect->bottom = bottom;
}

void LbRect_SetFromSize(TbRect* rect, const TbSize* size) {   // 0x10029824
    rect->left = 0;
    rect->top = 0;
    rect->right = size->width;
    rect->bottom = size->height;
}

void LbRect_SetWindow(TbRect* rect, int32_t x, int32_t y, int32_t width, int32_t height) {   // 0x10029880
    rect->left = x;
    rect->top = y;
    rect->right = x + width;
    rect->bottom = y + height;
}

TbRect* LbRect_Copy(TbRect* dest, const TbRect* source) {   // 0x100298d0 (r3 still holds dest)
    dest->left = source->left;
    dest->top = source->top;
    dest->right = source->right;
    dest->bottom = source->bottom;
    return dest;
}

void LbRect_Offset(TbRect* rect, int32_t dx, int32_t dy) {   // 0x10029928
    rect->left += dx;
    rect->top += dy;
    rect->right += dx;
    rect->bottom += dy;
}

// Bullfrog text drawing (Lbgfsave.cpp and the two LbDraw.cpp rectangle leaves), unit w224.

#define INSTANTIATE(T) \
    template void Draw_CheapText<T>(int32_t, int32_t, const T*, TbColour); \
    template int32_t Text_MeasureFormattedHeight<T>(uint32_t, const T*); \
    template void Draw_FormattedText<T>(const TbRect*, const T*, uint32_t, TbColour); \
    template void Draw_Text<T>(int32_t, int32_t, const T*, TbColour); \
    template void draw_text_chars<T>(int32_t, int32_t, const T*, uint32_t, TbColour); \
    template void draw_text_line<T>(int32_t, int32_t, const T*, uint32_t, TbColour, uint32_t, int32_t, uint32_t, uint32_t); \
    template uint32_t word_width<T>(const T*, uint32_t*); \
    template uint32_t Text_Measure<T>(const T*, TbSize*);
#undef INSTANTIATE

// LbDraw.cpp companions: 0x1002998c and 0x100299c8, 16 B each.
int32_t LbRect_Width(const TbRect* r) { return static_cast<uint32_t>(r->right) - r->left; }
int32_t LbRect_Height(const TbRect* r) { return static_cast<uint32_t>(r->bottom) - r->top; }

void LbRect_Bounding(TbRect* rect, const TbRect* other) {
    rect->left = rect->left < other->left ? rect->left : other->left;
    rect->top = rect->top < other->top ? rect->top : other->top;
    rect->right = rect->right > other->right ? rect->right : other->right;
    rect->bottom = rect->bottom > other->bottom ? rect->bottom : other->bottom;
}

// The Bullfrog library's rectangle, viewport, clip and palette leaves (bullfrog symbols shared 2.04b3): LbRect_Intersection
// 0x10029ad0, LbRect_Empty 0x10029cd4, LbRect_Normalise 0x10029d14 (the rectangle helpers), LbDraw_SetViewPort 0x1002b1c0,
// LbDraw_SetClipRect 0x1002b2c4, LbDraw_SetCurrent 0x1002b588 (the first overload; LbDraw.cpp) and LbPalette_FindColour
// 0x1001a5c8 (Lbgfsave.cpp). The drawing state they keep is declared in include/LbDraw.h.

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

// ---- LbDraw.cpp: LbRect_Contains 0x10029bcc, LbRect_Bounding 0x10029a04

bool LbRect_Contains(const TbRect* outer, const TbRect* inner) {
    bool a = false, b = false, c = false;
    if (outer->left <= inner->left) {
        if (outer->top <= inner->top) a = true;
    }
    if (a) {
        if (outer->right >= inner->right) b = true;
    }
    if (b) {
        if (outer->bottom >= inner->bottom) c = true;
    }
    return c;
}

bool LbRect_IsEmpty(const TbRect* rect) {   // 0x10029c74
    return rect->left == rect->right || rect->top == rect->bottom;
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

bool LbRect_IsNormal(const TbRect* rect) {   // 0x10029db8
    return rect->left <= rect->right && rect->top <= rect->bottom;
}

bool LbRect_Intersects(const TbRect* a, const TbRect* b) {   // 0x10029e18
    bool x = false;
    bool y = false;
    bool result = false;
    if (a->left <= b->right && b->left <= a->right) x = true;
    if (x && a->top <= b->bottom) y = true;
    if (y && b->top <= a->bottom) result = true;
    return result;
}

bool LbSize_IsNull(const TbSize* size) {   // 0x10029ec4
    return size->width == 0 || size->height == 0;
}

void LbSize_Set(TbSize* size, uint32_t width, uint32_t height) {   // 0x10029f18
    size->width = static_cast<int32_t>(width);
    size->height = static_cast<int32_t>(height);
}

// ---- LbDraw.cpp
int32_t LbSize_Area(const TbSize* size) { return size->width * size->height; }   // 0x10029f50

// The Bullfrog library's rectangle class (bullfrog symbols shared, LbDraw.cpp range): TbRect::Intersection 0x10029f8c, which
// the ltms font render method calls through the library's export (unit w45a). TbRect::Bounding (0x1002a110, unit w58) is below it.

// 0x10029f8c: each side takes the stricter of the two rectangles; a right or bottom that is then below its left or top is
// raised to it. The answer is never inverted, and unlike LbRect_Intersection (0x10029ad0) it cuts nothing in place and
// does not say whether anything is left (an empty answer has right == left or bottom == top).
TbRect TbRect::Intersection(const TbRect& other) const {
    const int32_t cutLeft = left > other.left ? left : other.left;
    const int32_t cutTop = top > other.top ? top : other.top;
    int32_t cutRight = right < other.right ? right : other.right;
    int32_t cutBottom = bottom < other.bottom ? bottom : other.bottom;
    if (cutRight < cutLeft) cutRight = cutLeft;
    if (cutBottom < cutTop) cutBottom = cutTop;
    return {cutLeft, cutTop, cutRight, cutBottom};
}

// 0x1002a06c: signed inclusive overlap test.
bool TbRect::Intersects(const TbRect& other) const {
    return left <= other.right && right >= other.left && top <= other.bottom && bottom >= other.top;
}

// 0x1002a110: the lower of the two lefts and of the two tops and the higher of the two rights and bottoms, whatever the
// rectangles are (an empty or inverted one counts by its numbers).
TbRect TbRect::Bounding(const TbRect& other) const {
    const int32_t boundLeft = left < other.left ? left : other.left;
    const int32_t boundTop = top < other.top ? top : other.top;
    const int32_t boundRight = right > other.right ? right : other.right;
    const int32_t boundBottom = bottom > other.bottom ? bottom : other.bottom;
    return {boundLeft, boundTop, boundRight, boundBottom};
}

// 0x1002a1d4 (unit v5L16): the Mac swaps each pair with three xors through memory, the same as a swap.
void TbRect::Normalise() {
    if (left > right) {
        left ^= right;
        right ^= left;
        left ^= right;
    }
    if (top > bottom) {
        top ^= bottom;
        bottom ^= top;
        top ^= bottom;
    }
}
