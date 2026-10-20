#pragma once

#include <cstdint>

#include "TbColour.h"

struct TbRect;
struct TbSprite;

// Partial original drawing interfaces. These bodies do not read this or call
// sibling methods. The complete virtual drawing interface remains separate.
class RenderContext16 {
public:
    void LbDraw_Pixel(int32_t x, int32_t y, TbColour colour);
    void LbDraw_Line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, TbColour colour);
    void LbDraw_HorizontalLine(int32_t x, int32_t y, int32_t length, TbColour colour);
    void LbDraw_VerticalLine(int32_t x, int32_t y, int32_t length, TbColour colour);
    void LbDraw_Circle(int32_t x, int32_t y, uint32_t radius, TbColour colour);
    void LbDraw_Rectangle(const TbRect& rect, TbColour colour);
    void LbDraw_RectangleOutline(const TbRect& rect, TbColour colour);
    void LbDraw_Triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                         int32_t x2, int32_t y2, TbColour colour);
    void LbDraw_PropText(int32_t x, int32_t y, const char* text, TbColour colour);
    void LbDraw_UnicodePropText(int32_t x, int32_t y, const char16_t* text, TbColour colour);
    void LbDraw_Sprite(int32_t x, int32_t y, const TbSprite* sprite);
    void LbDraw_OneColourSprite(int32_t x, int32_t y, const TbSprite* sprite, TbColour colour);
    // Unit w234, bullfrog 0x1003c890, 13384 B, original slot +0x1c.
    void LbDraw_CircleOutline(int32_t x, int32_t y, uint32_t radius, TbColour colour);
    // Unit w235, bullfrog 0x1003fd20, 4344 B, original slot +0x20.
    void LbDraw_CircleFilled(int32_t x, int32_t y, uint32_t radius, TbColour colour);
    // Unit w235, bullfrog 0x100411d4, 384 B, original slot +0x30.
    void LbDraw_ClearClipWindow(TbColour colour);
    void LbDraw_RectangleFilled(const TbRect& rect, TbColour colour);
};

class RenderContext24 {
public:
    void LbDraw_Pixel(int32_t x, int32_t y, TbColour colour);
    void LbDraw_Line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, TbColour colour);
    void LbDraw_HorizontalLine(int32_t x, int32_t y, int32_t length, TbColour colour);
    void LbDraw_VerticalLine(int32_t x, int32_t y, int32_t length, TbColour colour);
    void LbDraw_Circle(int32_t x, int32_t y, uint32_t radius, TbColour colour);
    void LbDraw_Rectangle(const TbRect& rect, TbColour colour);
    void LbDraw_RectangleOutline(const TbRect& rect, TbColour colour);
    void LbDraw_Triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                         int32_t x2, int32_t y2, TbColour colour);
    void LbDraw_PropText(int32_t x, int32_t y, const char* text, TbColour colour);
    void LbDraw_UnicodePropText(int32_t x, int32_t y, const char16_t* text, TbColour colour);
    void LbDraw_Sprite(int32_t x, int32_t y, const TbSprite* sprite);
    void LbDraw_OneColourSprite(int32_t x, int32_t y, const TbSprite* sprite, TbColour colour);
    // Unit w234, bullfrog 0x10030a50, 10012 B, original slot +0x1c.
    void LbDraw_CircleOutline(int32_t x, int32_t y, uint32_t radius, TbColour colour);
    // Unit w235, bullfrog 0x100331b4, 3548 B, original slot +0x20.
    void LbDraw_CircleFilled(int32_t x, int32_t y, uint32_t radius, TbColour colour);
    void LbDraw_ClearClipWindow(TbColour colour);
    void LbDraw_RectangleFilled(const TbRect& rect, TbColour colour);
};

class RenderContext32 {
public:
    void LbDraw_Pixel(int32_t x, int32_t y, TbColour colour);
    void LbDraw_Line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, TbColour colour);
    void LbDraw_HorizontalLine(int32_t x, int32_t y, int32_t length, TbColour colour);
    void LbDraw_VerticalLine(int32_t x, int32_t y, int32_t length, TbColour colour);
    void LbDraw_Circle(int32_t x, int32_t y, uint32_t radius, TbColour colour);
    void LbDraw_Rectangle(const TbRect& rect, TbColour colour);
    void LbDraw_RectangleOutline(const TbRect& rect, TbColour colour);
    void LbDraw_Triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                         int32_t x2, int32_t y2, TbColour colour);
    void LbDraw_PropText(int32_t x, int32_t y, const char* text, TbColour colour);
    void LbDraw_UnicodePropText(int32_t x, int32_t y, const char16_t* text, TbColour colour);
    void LbDraw_Sprite(int32_t x, int32_t y, const TbSprite* sprite);
    void LbDraw_OneColourSprite(int32_t x, int32_t y, const TbSprite* sprite, TbColour colour);
    // Unit w234, bullfrog 0x10036f7c, 6744 B, original slot +0x1c.
    void LbDraw_CircleOutline(int32_t x, int32_t y, uint32_t radius, TbColour colour);
    // Unit w235, bullfrog 0x10038a1c, 2904 B, original slot +0x20.
    void LbDraw_CircleFilled(int32_t x, int32_t y, uint32_t radius, TbColour colour);
    void LbDraw_ClearClipWindow(TbColour colour);
    void LbDraw_RectangleFilled(const TbRect& rect, TbColour colour);
};
