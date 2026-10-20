// Antialiased<CTraits<__int128>> from 3DFont.cpp (0x10006904, 1256 bytes): the antialiased 3D line for one byte per pixel.
// A Bresenham walk along the major axis; each step blends three pixels (the centre and the two beside it) with IntensifyPix.
#include <cmath>
#include <cstdint>

#include "DrawRenderClasses.h"
#include "Light.h"
#include "TbGraphicFile.h"
#include "ui_main.h"

namespace tpw_antialias {

template <int N>
void Antialiased(const tpw_intensify::CTraits<N>& traits, const TbRenderArea& area, const TbRect& clip, const bfl::draw::CColour& colour,
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
    int32_t sgn = dx < 0 ? -1 : 1;   // r22
    int32_t stepRow = dy < 0 ? -area.mStride : area.mStride;   // r25
    const uint32_t m0 = *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(&area) + 0);
    const uint32_t m1 = *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(&area) + 4);
    const uint32_t m2 = *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(&area) + 8);
    const uint32_t m3 = *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(&area) + 12);

    if (adx >= ady) {
        if (dx < 0) {
            int32_t t = x0; x0 = x1; x1 = t;
            t = y0; y0 = y1; y1 = t;
            dx = adx;
            dy = -dy;
            sx = -sx;
            sy = -sy;
            sgn = -sgn;
            stepRow = -stepRow;
        }
        int32_t twoAdy = ady * 2;
        int32_t d = twoAdy - dx;
        int32_t err = 0;
        double s = std::sqrt(static_cast<double>(dx * dx + dy * dy));
        double inv = 1.0 / (2.0 * s);
        int32_t step2 = (ady - dx) * 2;
        int32_t x = x0;
        int32_t y = y0;
        uint8_t* centre = area.mData + (area.mStride * y0 + x0);
        uint8_t* up = centre + stepRow;
        uint8_t* down = centre - stepRow;
        double sum = (2.0 * static_cast<double>(dx)) * inv;
        while (x != x1) {
            double a = static_cast<double>(err) * inv;
            tpw_intensify::IntensifyPix<N>(traits, centre, a, value, m0, m1, m2, m3, x, y, clip);
            tpw_intensify::IntensifyPix<N>(traits, up, sum - a, value, m0, m1, m2, m3, x, y + sy, clip);
            tpw_intensify::IntensifyPix<N>(traits, down, sum + a, value, m0, m1, m2, m3, x, y - sy, clip);
            if (d < 0) {
                err = d + dx;
                x += sx;
                centre += sgn;
                d += twoAdy;
                up += sgn;
                down += sgn;
            } else {
                int32_t move = sgn + stepRow;
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
            sgn = -sgn;
            stepRow = -stepRow;
        }
        int32_t twoAdx = adx * 2;
        int32_t d = twoAdx - dy;
        int32_t err = 0;
        double s = std::sqrt(static_cast<double>(dx * dx + dy * dy));
        double inv = 1.0 / (2.0 * s);
        int32_t step2 = (adx - dy) * 2;
        int32_t y = y0;
        int32_t x = x0;
        uint8_t* centre = area.mData + (area.mStride * y0 + x0);
        uint8_t* left = centre + sgn;
        uint8_t* right = centre - sgn;
        double sum = (2.0 * static_cast<double>(dy)) * inv;
        while (y != y1) {
            double a = static_cast<double>(err) * inv;
            tpw_intensify::IntensifyPix<N>(traits, centre, a, value, m0, m1, m2, m3, x, y, clip);
            tpw_intensify::IntensifyPix<N>(traits, left, sum - a, value, m0, m1, m2, m3, x + sx, y, clip);
            tpw_intensify::IntensifyPix<N>(traits, right, sum + a, value, m0, m1, m2, m3, x - sx, y, clip);
            if (d < 0) {
                err = d + dy;
                d += twoAdx;
                y += sy;
                centre += stepRow;
                left += stepRow;
                right += stepRow;
            } else {
                int32_t move = sgn + stepRow;
                err = d - dy;
                d += step2;
                y += sy;
                x += sx;
                centre += move;
                left += move;
                right += move;
            }
        }
    }
}

template void Antialiased<1>(const tpw_intensify::CTraits<1>&, const TbRenderArea&, const TbRect&, const bfl::draw::CColour&,
                             const TbPoint&, const TbPoint&);

}  // namespace tpw_antialias
