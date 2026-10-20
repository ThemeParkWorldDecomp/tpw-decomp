#pragma once

#include <cstdint>

#include "TbColour.h"
#include "TbGraphicFile.h"

struct TbPalette;
struct TbSprite;

void LbPalette_ToCompact(uint8_t* compact, const TbPalette* palette, uint32_t count);
void LbDraw_RemappedSprite(int32_t x, int32_t y, const TbSprite* sprite,
                           const uint8_t* remap);
void LbDraw_Pixel(int32_t x, int32_t y, TbColour colour);
void LbDraw_Rectangle(const TbRect* rect, TbColour colour);
void LbDraw_Rectangle(const TbRect& rect, TbColour colour);
void LbDraw_RectangleOutline(const TbRect* rect, TbColour colour);
void LbDraw_RectangleOutline(const TbRect& rect, TbColour colour);
void LbDraw_Line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, TbColour colour);
void LbDraw_HorizontalLine(int32_t x, int32_t y, int32_t length, TbColour colour);
void LbDraw_VerticalLine(int32_t x, int32_t y, int32_t length, TbColour colour);
void LbDraw_Circle(int32_t x, int32_t y, uint32_t radius, TbColour colour);
void LbDraw_CircleOutline(int32_t x, int32_t y, uint32_t radius, TbColour colour);
void LbDraw_CircleFilled(int32_t x, int32_t y, uint32_t radius, TbColour colour);
void LbDraw_Palette(int32_t x, int32_t y, uint32_t width, uint32_t height,
                    int32_t labels);
void LbDraw_Triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                     int32_t x2, int32_t y2, TbColour colour);
void LbDraw_ClearClipRect(TbColour colour);
void LbDraw_RectangleFilled(const TbRect* rect, TbColour colour);
void LbDraw_RectangleFilled(const TbRect& rect, TbColour colour);
void LbDraw_PropText(int32_t x, int32_t y, const char* text, TbColour colour);
void LbDraw_UnicodePropText(int32_t x, int32_t y, const char16_t* text,
                            TbColour colour);
void LbDraw_ReleaseViewPort();
void LbDraw_ReleaseClipRect();
TbRect* LbDraw_GetViewPort();
void LbDraw_ScaledSprite(int32_t x, int32_t y, const TbSprite* sprite,
                         uint32_t width, uint32_t height);
void LbDraw_SetSpriteScalingData(int32_t x, int32_t y, uint32_t sourceWidth,
                                 uint32_t sourceHeight, uint32_t width,
                                 uint32_t height);
