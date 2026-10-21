// 3DFont.cpp: name proven by __sinit_3DFont_cpp in the main application.

#include "DrawRenderClasses.h"
#include "FS_System.h"
#include "Game.h"
#include "LbFile.h"
#include "Light.h"
#include "platform/macdoze.h"
#include "TbGraphicFile.h"
#include "TbPixFmt.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPResource.h"
#include "TPString.h"
#include "ui_ime.h"
#include "ui_main.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

// String helpers of the main app (decompiled from SimTheme Park 2.04b3:
// 0x10005354..0x1000585c). The original file is unknown (see TPString.h).

bool TPStrGtr(const TbDynamicStringTemplate<char>& a, const TbDynamicStringTemplate<char>& b) {
    return NS_MacDoze::_stricmp(a.mData, b.mData) > 0;
}

bool TPStrEq(const TbDynamicStringTemplate<char>& a, const TbDynamicStringTemplate<char>& b) {
    return NS_MacDoze::_stricmp(a.mData, b.mData) == 0;
}

bool TPUniStrEq(const TbDynamicStringTemplate<char16_t>& a, const TbDynamicStringTemplate<char16_t>& b) {
    return NS_MacDoze::_wcsicmp(a.mData, b.mData) == 0;
}

TbDynamicStringTemplate<char>& RemoveExtension(const TbDynamicStringTemplate<char>& extension,
                                                TbDynamicStringTemplate<char>& name) {
    const TbDynamicStringTemplate<char> tail(name.Right(static_cast<uint32_t>(extension.Length())));
    if (NS_MacDoze::_stricmp(tail.mData, extension.mData) == 0)
        name = name.Left(static_cast<uint32_t>(name.Length() - extension.Length()));
    return name;
}

int32_t UniStringToString(TbDynamicStringTemplate<char>& out, const TbDynamicStringTemplate<char16_t>& in) {
    int32_t written = 0;
    const int32_t size = NS_MacDoze::WideCharToMultiByte(0, 0, in.mData, -1, nullptr, 0, nullptr, nullptr);
    if (size > 0) {
        char* text = static_cast<char*>(LbMem_Alloc(static_cast<uint32_t>(size)));
        if (text != nullptr) {
            written = NS_MacDoze::WideCharToMultiByte(0, 0, in.mData, -1, text, size, nullptr, nullptr);
            out = text;
            LbMem_Free(text);
        }
    }
    return written;
}

int32_t StringToUniString(TbDynamicStringTemplate<char16_t>& out, const TbDynamicStringTemplate<char>& in) {
    int32_t written = 0;
    const int32_t size = NS_MacDoze::MultiByteToWideChar(0, 0, in.mData, -1, nullptr, 0);
    if (size > 0) {
        char16_t* text = static_cast<char16_t*>(LbMem_Alloc(static_cast<uint32_t>(size) * sizeof(char16_t)));
        if (text != nullptr) {
            written = NS_MacDoze::MultiByteToWideChar(0, 0, in.mData, -1, text, size);
            out = text;
            LbMem_Free(text);
        }
    }
    return written;
}

// 3DFont.cpp static initialiser: the font's global parallel light (unit v5109).

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940
void UIMain_DestroyLight(void* object);    // the light's destructor word 0x102657b8

alignas(16) unsigned char gSinit3DFontLight[0x100];   // Mac 0x102b34c8, 0x74 bytes
unsigned char gSinit3DFontNode[16];                   // Mac 0x102b353c

static_assert(sizeof(CLightParallel) <= sizeof(gSinit3DFontLight), "storage too small");

// 0x1000757c: constructs the global (the Mac inlines the CLightParallel constructor: vtable words, the shared
// CMapLinkBase), flags 2 (a direction), normal (0, 0, 1), colour all ones, then registers its destructor.
void __sinit_3DFont_cpp() {
    CLightParallel* light = new (gSinit3DFontLight) CLightParallel;
    light->mFlags = 2;
    light->SetNormal(0.0f, 0.0f, 1.0f);
    light->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
    __register_global_object(light, reinterpret_cast<void*>(&UIMain_DestroyLight), gSinit3DFontNode);
}

// Antialiased<CTraits<__int128>> from 3DFont.cpp (0x10006904, 1256 bytes): the antialiased 3D line for one byte per pixel.
// A Bresenham walk along the major axis; each step blends three pixels (the centre and the two beside it) with IntensifyPix.

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

// 0x10005288: the destructor is defined inline in FS_System.h (the Mac's NULL test and the delete flag belong to
// the delete-expression; the five strings are destroyed by the members), so every library that uses it links.

// Weak: a test library replaces it with a recording fake.
__attribute__((weak)) void CSYS_TPFileInfo::operator delete(void* object) { ::operator delete(object); }

// IntensifyPix<CTraits<N>> from 3DFont.cpp (anonymous namespace on the Mac), four instances of one template.

namespace tpw_intensify {

template <int N>
void IntensifyPix(const CTraits<N>&, uint8_t* pix, double alpha, uint32_t colour, uint32_t mask1, uint32_t mask2,
                  uint32_t mask3, uint32_t keepMask, int32_t x, int32_t y, const TbRect& rect) {
    bool inX = false, inXY = false, inside = false;
    alpha = std::fabs(alpha);
    if (x >= rect.left && x < rect.right) inX = true;
    if (inX && y >= rect.top) inXY = true;
    if (inXY && y < rect.bottom) inside = true;
    if (!inside || !(alpha <= 1.0)) return;
    uint32_t src = *reinterpret_cast<uint32_t*>(pix);
    uint32_t w = static_cast<uint32_t>(static_cast<int32_t>(255.0 * (1.0 - alpha))) & 0xff;
    uint32_t inv = 255u - w;
    uint32_t v3 = inv * (src & mask3) + w * (colour & mask3);
    uint32_t v2 = inv * (src & mask2) + w * (colour & mask2);
    uint32_t v1 = inv * (src & mask1) + w * (colour & mask1);
    uint32_t out = (src & keepMask) | (mask1 & ((v1 + (v1 >> 8) + 1) >> 8)) | (mask2 & ((v2 + (v2 >> 8) + 1) >> 8)) |
                   (mask3 & ((v3 + (v3 >> 8) + 1) >> 8));
    for (int i = 0; i < N; i++) {
        *pix++ = static_cast<uint8_t>(out >> ((i & 7) << 3));
    }
}

template void IntensifyPix<1>(const CTraits<1>&, uint8_t*, double, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t, const TbRect&);
template void IntensifyPix<2>(const CTraits<2>&, uint8_t*, double, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t, const TbRect&);
template void IntensifyPix<3>(const CTraits<3>&, uint8_t*, double, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t, const TbRect&);
template void IntensifyPix<4>(const CTraits<4>&, uint8_t*, double, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t, const TbRect&);

}  // namespace tpw_intensify

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

// Antialiased<CTraits<4>> (0x100058c0, 1264 bytes) and Antialiased<CTraits<3>> (0x10005e30, 1252 bytes) from 3DFont.cpp: the antialiased
// 3D line for four and three bytes per pixel. A Bresenham walk along the major axis; each step blends three pixels with IntensifyPix.

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
