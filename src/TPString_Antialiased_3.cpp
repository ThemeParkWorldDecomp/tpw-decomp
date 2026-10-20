// Antialiased<CTraits<4>> (0x100058c0, 1264 bytes) and Antialiased<CTraits<3>> (0x10005e30, 1252 bytes) from 3DFont.cpp: the antialiased
// 3D line for four and three bytes per pixel. A Bresenham walk along the major axis; each step blends three pixels with IntensifyPix.
#include <cmath>
#include <cstdint>

#include "TPString.h"
#include "LbFile.h"
#include "platform/macdoze.h"
#include "ui_main.h"
#include "Light.h"
#include "DrawRenderClasses.h"
#include "TbGraphicFile.h"
#include "TbPixFmt.h"

namespace {

template <int N>
void AntialiasedImpl(const tpw_intensify::CTraits<N>& traits, const TbRenderArea& area, const TbRect& clip, const bfl::draw::CColour& colour,
                     const TbPoint& from, const TbPoint& to) {
    int32_t x0 = from.x;
    int32_t x1 = to.x;
    int32_t y0 = from.y;
    int32_t y1 = to.y;
    int32_t dx = x1 - x0;
    int32_t dy = y1 - y0;
    int32_t adx = dx < 0 ? -dx : dx;
    int32_t ady = dy < 0 ? -dy : dy;
    int32_t sx = dx < 0 ? -1 : 1;
    int32_t sy = dy < 0 ? -1 : 1;
    uint32_t value = colour.GetFormattedValue(reinterpret_cast<const TbPixFormat&>(area));
    int32_t step = dx < 0 ? -N : N;
    int32_t stride = dy < 0 ? -area.mStride : area.mStride;
    const uint8_t* base = reinterpret_cast<const uint8_t*>(&area);
    const uint32_t m0 = *reinterpret_cast<const uint32_t*>(base + 0);
    const uint32_t m1 = *reinterpret_cast<const uint32_t*>(base + 4);
    const uint32_t m2 = *reinterpret_cast<const uint32_t*>(base + 8);
    const uint32_t m3 = *reinterpret_cast<const uint32_t*>(base + 12);

    if (adx >= ady) {
        if (dx < 0) {
            int32_t t = x0; x0 = x1; x1 = t;
            t = y0; y0 = y1; y1 = t;
            dx = adx;
            dy = -dy;
            sx = -sx;
            sy = -sy;
            step = -step;
            stride = -stride;
        }
        int32_t twoAdy = ady * 2;
        int32_t d = twoAdy - dx;
        int32_t err = 0;
        double s = std::sqrt(static_cast<double>(dx * dx + dy * dy));
        double inv = 1.0 / (2.0 * s);
        int32_t step2 = (ady - dx) * 2;
        int32_t x = x0;
        int32_t y = y0;
        uint8_t* centre = area.mData + (y0 * area.mStride + x0 * N);
        uint8_t* up = centre + stride;
        uint8_t* down = centre - stride;
        double sum = (2.0 * static_cast<double>(dx)) * inv;
        while (x != x1) {
            double a = static_cast<double>(err) * inv;
            tpw_intensify::IntensifyPix<N>(traits, centre, a, value, m0, m1, m2, m3, x, y, clip);
            tpw_intensify::IntensifyPix<N>(traits, up, sum - a, value, m0, m1, m2, m3, x, y + sy, clip);
            tpw_intensify::IntensifyPix<N>(traits, down, sum + a, value, m0, m1, m2, m3, x, y - sy, clip);
            if (d < 0) {
                err = d + dx;
                d += twoAdy;
                x += sx;
                centre += step;
                up += step;
                down += step;
            } else {
                int32_t move = step + stride;
                err = d - dx;
                d += step2;
                x += sx;
                y += sy;
                centre += move;
                up += move;
                down += move;
            }
        }
    } else {
        if (dy < 0) {
            int32_t t = x0; x0 = x1; x1 = t;
            t = y0; y0 = y1; y1 = t;
            dy = ady;
            dx = -dx;
            sy = -sy;
            sx = -sx;
            step = -step;
            stride = -stride;
        }
        int32_t twoAdx = adx * 2;
        int32_t d = twoAdx - dy;
        int32_t err = 0;
        double s = std::sqrt(static_cast<double>(dx * dx + dy * dy));
        double inv = 1.0 / (2.0 * s);
        int32_t step2 = (adx - dy) * 2;
        int32_t x = x0;
        int32_t y = y0;
        uint8_t* centre = area.mData + (y0 * area.mStride + x0 * N);
        uint8_t* right = centre + step;
        uint8_t* left = centre - step;
        double sum = (2.0 * static_cast<double>(dy)) * inv;
        while (y != y1) {
            double a = static_cast<double>(err) * inv;
            tpw_intensify::IntensifyPix<N>(traits, centre, a, value, m0, m1, m2, m3, x, y, clip);
            tpw_intensify::IntensifyPix<N>(traits, right, sum - a, value, m0, m1, m2, m3, x + sx, y, clip);
            tpw_intensify::IntensifyPix<N>(traits, left, sum + a, value, m0, m1, m2, m3, x - sx, y, clip);
            if (d < 0) {
                err = d + dy;
                d += twoAdx;
                y += sy;
                centre += stride;
                right += stride;
                left += stride;
            } else {
                int32_t move = step + stride;
                err = d - dy;
                d += step2;
                y += sy;
                x += sx;
                centre += move;
                right += move;
                left += move;
            }
        }
    }
}

}  // namespace

namespace tpw_antialias {

template <>
void Antialiased<4>(const tpw_intensify::CTraits<4>& traits, const TbRenderArea& area, const TbRect& clip, const bfl::draw::CColour& colour,
                    const TbPoint& from, const TbPoint& to) {
    AntialiasedImpl<4>(traits, area, clip, colour, from, to);
}

template <>
void Antialiased<3>(const tpw_intensify::CTraits<3>& traits, const TbRenderArea& area, const TbRect& clip, const bfl::draw::CColour& colour,
                    const TbPoint& from, const TbPoint& to) {
    AntialiasedImpl<3>(traits, area, clip, colour, from, to);
}

}  // namespace tpw_antialias
