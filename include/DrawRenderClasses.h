#pragma once

#include <cstdint>

#include "TbGraphicFile.h"

struct TbSprite;

namespace bfl {
namespace draw {

class TbMaskRotates {
public:
    uint32_t GetRightRotate2(uint32_t topBit, uint32_t mask) const;
    void Init(const TbPixFormat& source, const TbPixFormat& destination);

    uint32_t mRotate[4];
    uint32_t mMask[4];
};

// Partial original colour object. The first four bytes are the channels used by
// Draw.cpp; the remaining words are the original formatter's mutable cache.
struct CColour {
    uint8_t mRed;
    uint8_t mGreen;
    uint8_t mBlue;
    uint8_t mAlpha;
    mutable uint32_t mFormatCache[8];

    uint32_t GetFormattedValue(const TbPixFormat& format) const; // bullfrog 0x100193dc
    uint32_t GetFormattedValue(const TbMaskRotates& rotates) const; // bullfrog 0x1001a008
};
static_assert(sizeof(CColour) == 0x24, "CColour Mac size");

#define TPW_RENDER_CLASS(Name) \
class Name { \
public: \
    void Pixel(TbRenderArea* area, const TbRect& clip, uint32_t flags, \
               int32_t x, int32_t y, const CColour& colour) const; \
    void HorizontalLine(TbRenderArea* area, const TbRect& clip, uint32_t flags, \
                        int32_t x, int32_t y, int32_t length, const CColour& colour) const; \
    void VerticalLine(TbRenderArea* area, const TbRect& clip, uint32_t flags, \
                      int32_t x, int32_t y, int32_t length, const CColour& colour) const; \
    void Line(TbRenderArea* area, const TbRect& clip, uint32_t flags, \
              int32_t x0, int32_t y0, int32_t x1, int32_t y1, const CColour& colour) const; \
    void CircleOutline(TbRenderArea* area, const TbRect& clip, uint32_t flags, \
                       int32_t x, int32_t y, uint32_t radius, const CColour& colour) const; \
    void CircleFilled(TbRenderArea* area, const TbRect& clip, uint32_t flags, \
                      int32_t x, int32_t y, uint32_t radius, const CColour& colour) const; \
    void RectangleOutline(TbRenderArea* area, const TbRect& clip, uint32_t flags, \
                          const TbRect& rect, const CColour& colour) const; \
    void RectangleFilled(TbRenderArea* area, const TbRect& clip, uint32_t flags, \
                         const TbRect& rect, const CColour& colour) const; \
    void Triangle(TbRenderArea* area, const TbRect& clip, uint32_t flags, \
                  int32_t x0, int32_t y0, int32_t x1, int32_t y1, \
                  int32_t x2, int32_t y2, const CColour& colour) const; \
    void OneColourSprite(TbRenderArea* area, const TbRect& clip, uint32_t flags, \
                         int32_t x, int32_t y, const TbSprite& sprite, \
                         const CColour& colour) const; \
    void Sprite(TbRenderArea* area, const TbRect& clip, uint32_t flags, \
                const uint32_t* palette, int32_t x, int32_t y, \
                const TbSprite& sprite) const; \
    void PropText(TbRenderArea* area, const TbRect& clip, uint32_t flags, \
                  int32_t x, int32_t y, const char* text, const CColour& colour) const; \
}

// Unit w239, Draw.cpp's render-class circle rasterizers.
TPW_RENDER_CLASS(CRenderClass8);
TPW_RENDER_CLASS(CRenderClass16);
TPW_RENDER_CLASS(CRenderClass24);
TPW_RENDER_CLASS(CRenderClass32);

#undef TPW_RENDER_CLASS

} // namespace draw
} // namespace bfl
