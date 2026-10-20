#include <cstddef>
#include <cstdint>
#include <cstring>

#include "DrawRenderClasses.h"

using namespace bfl::draw;

namespace {
uint32_t gFormatted;
uint32_t gFormatCalls;
}

uint32_t CColour::GetFormattedValue(const TbPixFormat&) const {
    ++gFormatCalls;
    return gFormatted;
}

extern "C" {
uint32_t tpw_rclasscircle_layout(unsigned index) {
    const uint32_t values[] = {sizeof(TbRenderArea), offsetof(TbRenderArea, mData),
        offsetof(TbRenderArea, mStride), sizeof(CColour)};
    return values[index];
}

uint32_t tpw_rclasscircle_calls() { return gFormatCalls; }

void tpw_rclasscircle_run(uint32_t operation, uint32_t depth, uint8_t* pixels,
                          int32_t width, int32_t height, int32_t stride,
                          const uint32_t* format, const TbRect* clip, uint32_t flags,
                          int32_t x, int32_t y, uint32_t radius,
                          const CColour* colour, uint32_t formatted) {
    TbRenderArea area(TbRenderArea::kCleared);
    std::memcpy(area.mF00, format, 16);
    area.mDepth = static_cast<int32_t>(depth);
    area.mPalettised = depth == 8;
    area.mData = pixels;
    area.mWidth = width;
    area.mPictureHeight = height;
    area.mStride = stride;
    gFormatted = formatted;
    gFormatCalls = 0;

    if (depth == 8) {
        CRenderClass8 render;
        if (operation == 0) render.CircleOutline(&area, *clip, flags, x, y, radius, *colour);
        else render.CircleFilled(&area, *clip, flags, x, y, radius, *colour);
    } else if (depth == 16) {
        CRenderClass16 render;
        if (operation == 0) render.CircleOutline(&area, *clip, flags, x, y, radius, *colour);
        else render.CircleFilled(&area, *clip, flags, x, y, radius, *colour);
    } else if (depth == 24) {
        CRenderClass24 render;
        if (operation == 0) render.CircleOutline(&area, *clip, flags, x, y, radius, *colour);
        else render.CircleFilled(&area, *clip, flags, x, y, radius, *colour);
    } else {
        CRenderClass32 render;
        if (operation == 0) render.CircleOutline(&area, *clip, flags, x, y, radius, *colour);
        else render.CircleFilled(&area, *clip, flags, x, y, radius, *colour);
    }
}
}

