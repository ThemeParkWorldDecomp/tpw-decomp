#include <cstddef>

#include "RenderContextDepthCircles.h"
#include "RenderContext8.h"
#include "LbDraw.h"
#include "LbSurface.h"
#include "LbDrawText.h"

struct TbPalette { uint8_t bytes[1024]; };
TbPalette _lbGlobalPalette;
TbRenderArea* _lbpCurrentRA;
TbRect _lbCurrentClipRect;
uint8_t* _lbCurrentOriginPtr;
uint32_t _lbCurrentDrawFlags;
uint8_t* _lbCurrentGhostTable;
uint8_t* _lbpCurrentFadeRow;
TbPixelCaps _lbCurrentPixelCaps;

extern "C" {
uint64_t tpw_rdepthcircle_global(unsigned i) {
    const void* globals[] = {&_lbpCurrentRA, &_lbCurrentClipRect, &_lbCurrentOriginPtr,
        &_lbCurrentDrawFlags, &_lbCurrentGhostTable, &_lbpCurrentFadeRow,
        &_lbCurrentPixelCaps, &_lbGlobalPalette, &gLbSpriteDrawState};
    return reinterpret_cast<uintptr_t>(globals[i]);
}

uint32_t tpw_rdepthcircle_layout(unsigned i) {
    const uint32_t offsets[] = {sizeof(TbRenderArea), offsetof(TbRenderArea, mData),
        offsetof(TbRenderArea, mWidth), offsetof(TbRenderArea, mPictureHeight),
        offsetof(TbRenderArea, mStride)};
    return offsets[i];
}

void tpw_rdepthcircle_run(uint32_t operation, uint32_t depth, int32_t x, int32_t y,
                          int32_t a, int32_t b, int32_t c, int32_t d,
                          const TbRect* rect, const char* text, const char16_t* unicode,
                          const uint8_t* sprite_data, uint16_t sprite_width, uint16_t sprite_height,
                          const TbColour* colour) {
    const TbSprite sprite = {sprite_data, sprite_width, sprite_height};
    if (depth == 16) {
        RenderContext16 context;
        if (operation == 0) context.LbDraw_CircleOutline(x, y, uint32_t(a), *colour);
        else if (operation == 1) context.LbDraw_CircleFilled(x, y, uint32_t(a), *colour);
        else if (operation == 2) context.LbDraw_ClearClipWindow(*colour);
        else if (operation == 3) context.LbDraw_Pixel(x, y, *colour);
        else if (operation == 4) context.LbDraw_HorizontalLine(x, y, a, *colour);
        else if (operation == 5) context.LbDraw_VerticalLine(x, y, a, *colour);
        else if (operation == 6) context.LbDraw_Line(x, y, a, b, *colour);
        else if (operation == 7) context.LbDraw_Circle(x, y, uint32_t(a), *colour);
        else if (operation == 8) context.LbDraw_RectangleFilled(*rect, *colour);
        else if (operation == 9) context.LbDraw_Triangle(x, y, a, b, c, d, *colour);
        else if (operation == 10) context.LbDraw_Rectangle(*rect, *colour);
        else if (operation == 11) context.LbDraw_RectangleOutline(*rect, *colour);
        else if (operation == 12) context.LbDraw_PropText(x, y, text, *colour);
        else if (operation == 13) context.LbDraw_UnicodePropText(x, y, unicode, *colour);
        else if (operation == 14) context.LbDraw_Sprite(x, y, &sprite);
        else context.LbDraw_OneColourSprite(x, y, &sprite, *colour);
    } else if (depth == 24) {
        RenderContext24 context;
        if (operation == 0) context.LbDraw_CircleOutline(x, y, uint32_t(a), *colour);
        else if (operation == 1) context.LbDraw_CircleFilled(x, y, uint32_t(a), *colour);
        else if (operation == 2) context.LbDraw_ClearClipWindow(*colour);
        else if (operation == 3) context.LbDraw_Pixel(x, y, *colour);
        else if (operation == 4) context.LbDraw_HorizontalLine(x, y, a, *colour);
        else if (operation == 5) context.LbDraw_VerticalLine(x, y, a, *colour);
        else if (operation == 6) context.LbDraw_Line(x, y, a, b, *colour);
        else if (operation == 7) context.LbDraw_Circle(x, y, uint32_t(a), *colour);
        else if (operation == 8) context.LbDraw_RectangleFilled(*rect, *colour);
        else if (operation == 9) context.LbDraw_Triangle(x, y, a, b, c, d, *colour);
        else if (operation == 10) context.LbDraw_Rectangle(*rect, *colour);
        else if (operation == 11) context.LbDraw_RectangleOutline(*rect, *colour);
        else if (operation == 12) context.LbDraw_PropText(x, y, text, *colour);
        else if (operation == 13) context.LbDraw_UnicodePropText(x, y, unicode, *colour);
        else if (operation == 14) context.LbDraw_Sprite(x, y, &sprite);
        else context.LbDraw_OneColourSprite(x, y, &sprite, *colour);
    } else if (depth == 32) {
        RenderContext32 context;
        if (operation == 0) context.LbDraw_CircleOutline(x, y, uint32_t(a), *colour);
        else if (operation == 1) context.LbDraw_CircleFilled(x, y, uint32_t(a), *colour);
        else if (operation == 2) context.LbDraw_ClearClipWindow(*colour);
        else if (operation == 3) context.LbDraw_Pixel(x, y, *colour);
        else if (operation == 4) context.LbDraw_HorizontalLine(x, y, a, *colour);
        else if (operation == 5) context.LbDraw_VerticalLine(x, y, a, *colour);
        else if (operation == 6) context.LbDraw_Line(x, y, a, b, *colour);
        else if (operation == 7) context.LbDraw_Circle(x, y, uint32_t(a), *colour);
        else if (operation == 8) context.LbDraw_RectangleFilled(*rect, *colour);
        else if (operation == 9) context.LbDraw_Triangle(x, y, a, b, c, d, *colour);
        else if (operation == 10) context.LbDraw_Rectangle(*rect, *colour);
        else if (operation == 11) context.LbDraw_RectangleOutline(*rect, *colour);
        else if (operation == 12) context.LbDraw_PropText(x, y, text, *colour);
        else if (operation == 13) context.LbDraw_UnicodePropText(x, y, unicode, *colour);
        else if (operation == 14) context.LbDraw_Sprite(x, y, &sprite);
        else context.LbDraw_OneColourSprite(x, y, &sprite, *colour);
    } else {
        RenderContext8 context;
        if (operation == 0) context.LbDraw_CircleOutline(x, y, uint32_t(a), *colour);
        else if (operation == 1) context.LbDraw_CircleFilled(x, y, uint32_t(a), *colour);
        else if (operation == 2) context.LbDraw_ClearClipWindow(*colour);
        else if (operation == 3) context.LbDraw_Pixel(x, y, *colour);
        else if (operation == 4) context.LbDraw_HorizontalLine(x, y, a, *colour);
        else if (operation == 5) context.LbDraw_VerticalLine(x, y, a, *colour);
        else if (operation == 6) context.LbDraw_Line(x, y, a, b, *colour);
        else if (operation == 7) context.LbDraw_Circle(x, y, uint32_t(a), *colour);
        else if (operation == 8) context.LbDraw_RectangleFilled(*rect, *colour);
        else if (operation == 9) context.LbDraw_Triangle(x, y, a, b, c, d, *colour);
        else if (operation == 10) context.LbDraw_Rectangle(*rect, *colour);
        else if (operation == 11) context.LbDraw_RectangleOutline(*rect, *colour);
        else if (operation == 12) context.LbDraw_PropText(x, y, text, *colour);
        else if (operation == 13) context.LbDraw_UnicodePropText(x, y, unicode, *colour);
        else if (operation == 14) context.LbDraw_Sprite(x, y, &sprite);
        else context.LbDraw_OneColourSprite(x, y, &sprite, *colour);
    }
}
}
