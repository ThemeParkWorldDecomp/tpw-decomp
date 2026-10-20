#include <cstddef>
#include <cstdint>
#include <cstring>

#include "DrawRenderClasses.h"
#include "LbDrawText.h"

using namespace bfl::draw;

namespace {
uint32_t gFormatted;
uint32_t gFormatCalls;
}

uint32_t CColour::GetFormattedValue(const TbPixFormat&) const {
    ++gFormatCalls;
    return gFormatted;
}

template<class Render>
void Run(Render& render, uint32_t operation, TbRenderArea* area, const TbRect& clip,
         uint32_t flags, const int32_t* p, const uint32_t* palette,
         const TbSprite& sprite, const char* text, const CColour& colour) {
    switch (operation) {
    case 0: render.Pixel(area, clip, flags, p[0], p[1], colour); break;
    case 1: render.HorizontalLine(area, clip, flags, p[0], p[1], p[2], colour); break;
    case 2: render.VerticalLine(area, clip, flags, p[0], p[1], p[2], colour); break;
    case 3: render.Line(area, clip, flags, p[0], p[1], p[2], p[3], colour); break;
    case 4: render.RectangleOutline(area, clip, flags,
                *reinterpret_cast<const TbRect*>(p), colour); break;
    case 5: render.RectangleFilled(area, clip, flags,
                *reinterpret_cast<const TbRect*>(p), colour); break;
    case 6: render.Triangle(area, clip, flags, p[0], p[1], p[2], p[3], p[4], p[5], colour); break;
    case 7: render.OneColourSprite(area, clip, flags, p[0], p[1], sprite, colour); break;
    case 8: render.Sprite(area, clip, flags, palette, p[0], p[1], sprite); break;
    case 9: render.PropText(area, clip, flags, p[0], p[1], text, colour); break;
    }
}

extern "C" {
uint32_t tpw_rclassprim_layout(unsigned index) {
    const uint32_t values[] = {sizeof(TbRenderArea), offsetof(TbRenderArea, mData),
        offsetof(TbRenderArea, mStride), sizeof(CColour), sizeof(TbSprite)};
    return values[index];
}

uint32_t tpw_rclassprim_calls() { return gFormatCalls; }

void tpw_rclassprim_run(uint32_t operation, uint32_t depth, uint8_t* pixels,
                        int32_t width, int32_t height, int32_t stride,
                        const uint32_t* format, const TbRect* clip, uint32_t flags,
                        const int32_t* params, const uint32_t* palette,
                        const uint8_t* sprite_data, uint16_t sprite_width,
                        uint16_t sprite_height, const char* text,
                        const CColour* colour, uint32_t formatted) {
    TbRenderArea area(TbRenderArea::kCleared);
    std::memcpy(area.mF00, format, 16);
    area.mDepth = static_cast<int32_t>(depth);
    area.mPalettised = depth == 8;
    area.mData = pixels;
    area.mWidth = width;
    area.mPictureHeight = height;
    area.mStride = stride;
    const TbSprite sprite = {sprite_data, sprite_width, sprite_height};
    gFormatted = formatted;
    gFormatCalls = 0;
    if (depth == 8) {
        CRenderClass8 render; Run(render, operation, &area, *clip, flags, params, palette, sprite, text, *colour);
    } else if (depth == 16) {
        CRenderClass16 render; Run(render, operation, &area, *clip, flags, params, palette, sprite, text, *colour);
    } else if (depth == 24) {
        CRenderClass24 render; Run(render, operation, &area, *clip, flags, params, palette, sprite, text, *colour);
    } else {
        CRenderClass32 render; Run(render, operation, &area, *clip, flags, params, palette, sprite, text, *colour);
    }
}
}
