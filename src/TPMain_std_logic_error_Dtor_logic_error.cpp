#include "TPMain.h"
#include <string>
#include <cmath>
#include <cstring>
#include "ui_ime.h"
#include "ui_main.h"
#include "Light.h"
#include "DrawRenderClasses.h"
#include "TbGraphicFile.h"
#include "TbPixFmt.h"

// Unit v5949: the Metrowerks exception objects' destructors, the char reference-counted pointer and the 3D line blend.

namespace IME {

// std::_RefCountedPtr<char, std::_Array<char>>::~_RefCountedPtr 0x100025e0 (flag is the hidden delete flag, signed 16 bit).
MslRefCountedChars* MslRefCountedCharsDtor25e0(MslRefCountedChars* self, int16_t deleteFlag) {
    if (self != nullptr) {
        int32_t* count = self->mCount;
        if (count != nullptr) {
            int32_t v = *count - 1;
            *count = v;
            if (v == 0) {
                MslDeleteArray(self->mPtr);
                MslDelete(self->mCount);
            }
        }
        if (deleteFlag > 0) MslDelete(self);
    }
    return self;
}

// std::_RefCountedPtr<char, std::_Array<char>>::_RefCountedPtr(char*) 0x100026ac. (The exception cleanup path of the
// original, delete[] then throw, is only reached when operator new throws, which it never does.)
void MslRefCountedCharsCtor(MslRefCountedChars* self, char* p) {
    self->mPtr = p;
    self->mCount = nullptr;
    if (self->mPtr != nullptr) {
        int32_t* count = static_cast<int32_t*>(MslNew(4));
        if (count != nullptr) *count = 1;
        self->mCount = count;
    }
}

// std::logic_error::~logic_error 0x100020cc
MslExceptionObject* MslLogicErrorDtor(MslExceptionObject* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->mVtbl = gMslExcVtblB;
        MslRefCountedCharsDtor25e0(&self->mRc, -1);
        self->mVtbl = gMslExcVtblC;
        if (deleteFlag > 0) MslDelete(self);
    }
    return self;
}

// std::out_of_range::~out_of_range 0x10002008
MslExceptionObject* MslOutOfRangeDtor(MslExceptionObject* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->mVtbl = gMslExcVtblA;
        self->mVtbl = gMslExcVtblB;
        MslRefCountedCharsDtor25e0(&self->mRc, -1);
        self->mVtbl = gMslExcVtblC;
        if (deleteFlag > 0) MslDelete(self);
    }
    return self;
}

}  // namespace IME

// Antialiased<CTraits<__vec2x32float__>> 0x10006394: the line from `from` to `to`, three IntensifyPix per step.
template <>
void tpw_antialias::Antialiased<2>(const tpw_intensify::CTraits<2>& traits, const TbRenderArea& area, const TbRect& clip,
                                   const bfl::draw::CColour& colour, const TbPoint& from, const TbPoint& to) {
    int32_t x0 = from.x, y0 = from.y;
    int32_t x1 = to.x, y1 = to.y;
    int32_t dx = x1 - x0;
    int32_t dy = y1 - y0;
    int32_t adx = dx < 0 ? -dx : dx;
    int32_t ady = dy < 0 ? -dy : dy;
    int32_t sx = dx < 0 ? -1 : 1;
    int32_t sy = dy < 0 ? -1 : 1;
    uint32_t value = colour.GetFormattedValue(reinterpret_cast<const TbPixFormat&>(area));
    int32_t step = dx < 0 ? -2 : 2;                              // r22 / r0
    int32_t stride = dy < 0 ? -area.mStride : area.mStride;      // r25
    uint32_t m[4];
    std::memcpy(m, area.mF00, sizeof(m));
    const uint32_t m0 = m[0], m1 = m[1], m2 = m[2], m3 = m[3];

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
        int32_t err = 0;
        int32_t d = twoAdy - dx;
        double len = std::sqrt(static_cast<double>(static_cast<int32_t>(dx * dx + dy * dy)));
        double inv = 1.0 / (2.0 * len);
        double span = 2.0 * static_cast<double>(dx) * inv;
        int32_t remain = (ady - dx) * 2;
        int32_t cx = x0, cy = y0;
        uint8_t* p0 = area.mData + y0 * area.mStride + x0 * 2;
        uint8_t* p1 = p0 + stride;
        uint8_t* p2 = p0 - stride;
        while (cx != x1) {
            double a = static_cast<double>(err) * inv;
            tpw_intensify::IntensifyPix<2>(traits, p0, a, value, m0, m1, m2, m3, cx, cy, clip);
            tpw_intensify::IntensifyPix<2>(traits, p1, span - a, value, m0, m1, m2, m3, cx, cy + sy, clip);
            tpw_intensify::IntensifyPix<2>(traits, p2, span + a, value, m0, m1, m2, m3, cx, cy - sy, clip);
            if (d < 0) {
                err = d + dx;
                cx += sx;
                p0 += step; p1 += step; p2 += step;
                d += twoAdy;
            } else {
                int32_t both = step + stride;
                err = d - dx;
                d += remain;
                cx += sx;
                cy += sy;
                p0 += both; p1 += both; p2 += both;
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
        int32_t err = 0;
        int32_t d = twoAdx - dy;
        double len = std::sqrt(static_cast<double>(static_cast<int32_t>(dx * dx + dy * dy)));
        double inv = 1.0 / (2.0 * len);
        double span = 2.0 * static_cast<double>(dy) * inv;
        int32_t remain = (adx - dy) * 2;
        int32_t cy = y0, cx = x0;
        uint8_t* p0 = area.mData + y0 * area.mStride + x0 * 2;
        uint8_t* p1 = p0 + step;
        uint8_t* p2 = p0 - step;
        while (cy != y1) {
            double a = static_cast<double>(err) * inv;
            tpw_intensify::IntensifyPix<2>(traits, p0, a, value, m0, m1, m2, m3, cx, cy, clip);
            tpw_intensify::IntensifyPix<2>(traits, p1, span - a, value, m0, m1, m2, m3, cx + sx, cy, clip);
            tpw_intensify::IntensifyPix<2>(traits, p2, span + a, value, m0, m1, m2, m3, cx - sx, cy, clip);
            if (d < 0) {
                err = d + dy;
                d += twoAdx;
                cy += sy;
                p0 += stride; p1 += stride; p2 += stride;
            } else {
                int32_t both = step + stride;
                err = d - dy;
                d += remain;
                cy += sy;
                cx += sx;
                p0 += both; p1 += both; p2 += both;
            }
        }
    }
}
