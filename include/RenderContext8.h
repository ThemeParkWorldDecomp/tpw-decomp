#pragma once
#include <cstdint>
#include "TbColour.h"
struct TbRect;
struct TbSprite;
extern int32_t gLbSpriteDrawState[10];

// Partial original drawing interface. This body does not read this or dispatch
// siblings. The drawing-context globals retain their existing placeholder layout
// until the complete virtual interface is translated.
class RenderContext8 {
public:
    void LbDraw_Pixel(int32_t x, int32_t y, TbColour colour);
    void LbDraw_Line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, TbColour colour);
    void LbDraw_HorizontalLine(int32_t x, int32_t y, int32_t length, TbColour colour);
    void LbDraw_VerticalLine(int32_t x, int32_t y, int32_t length, TbColour colour);
    void LbDraw_Circle(int32_t x, int32_t y, uint32_t radius, TbColour colour);
    // Unit w233 (src/bullfrog/RenderContext.cpp), bullfrog 0x1002c7dc, 6892 B.
    // Original virtual slot 0x1c; directly callable while dispatch is unfinished.
    void LbDraw_CircleOutline(int32_t x, int32_t y, uint32_t radius, TbColour colour);
    void LbDraw_CircleFilled(int32_t x, int32_t y, uint32_t radius, TbColour colour);
    void LbDraw_Triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                         int32_t x2, int32_t y2, TbColour colour);
    void LbDraw_ClearClipWindow(TbColour colour);
    void LbDraw_PropText(int32_t x, int32_t y, const char* text, TbColour colour);
    void LbDraw_UnicodePropText(int32_t x, int32_t y, const char16_t* text, TbColour colour);
    void LbDraw_Sprite(int32_t x, int32_t y, const TbSprite* sprite);
    void LbDraw_OneColourSprite(int32_t x, int32_t y, const TbSprite* sprite, TbColour colour);
    void LbDraw_Rectangle(const TbRect& rect, TbColour colour);
    void LbDraw_RectangleOutline(const TbRect& rect, TbColour colour);
    void LbDraw_RectangleFilled(const TbRect& rect, TbColour colour);
};
