// tb_clipping_edges.cpp: the six edge clippers CClipping::ClipPoly calls through a table
// (engine 0x1003f8b0..0x10040424) and the colour blend they share (0x1003fae4). Each makes the
// vertex where the edge from the inside vertex to the outside one meets its plane; they differ
// only in the plane (the distance of a vertex from it, and which coordinate of the new vertex is
// pinned to the plane instead of blended).
//
// The original keeps its working values in five globals (BSS 0x102b3618..0x102b3630) shared by the
// six; they are globals here too, and a test compares them. The blend is done in double
// precision: t = 0.9999 * d_in / (d_in - d_out) with the plane distances of the two vertices (a
// single-precision sum each), then every float is `inside + t * (outside - inside)` as a fused
// multiply-add in double rounded to single. The fraction stops just short of 1 so the new vertex
// lies a hair inside the plane.
#include <cstdint>

#include "ppc_math.h"
#include "tb_clipping.h"
#include "tb_drawprimitive.h"

int32_t gClipStep;               // 0x102b3618  the tail loop's counter
int32_t gClipBytesLeft;          // 0x102b361c  the tail's bytes still to blend
double gClipFraction;            // 0x102b3620  the blend fraction t
double gClipInsideDistance;      // 0x102b3628
double gClipOutsideDistance;     // 0x102b3630

namespace {

// Engine data 0x100500b8: 0.9999f as a double.
const double kEdgeBias = 0.9998999834060669;
// Engine data 0x100500b0: the scale of the fraction for the colour blend.
const double kColourScale = 255.0;

// inside + t * (outside - inside): the difference is a single-precision subtraction, the rest a
// fused multiply-add in double rounded to single (fmadd, frsp).
float Blend(float inside, float outside) {
    const float difference = outside - inside;
    return static_cast<float>(ppc_fmadd(gClipFraction, static_cast<double>(difference), static_cast<double>(inside)));
}

float* FloatAt(SFVF_TLCTEX* vertex, int32_t offset) {
    return reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(vertex) + offset);
}

const float* FloatAt(const SFVF_TLCTEX* vertex, int32_t offset) {
    return reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(vertex) + offset);
}

// Everything the six share except the plane's own coordinates: the fraction from the two
// distances, the new vertex's w, the words after the specular colour (texture coordinates and
// the like, two floats at a time), the colour and the specular colour. The caller sets x, y
// and z.
void MakeVertex(SFVF_TLCTEX* out, const SFVF_TLCTEX* outside, const SFVF_TLCTEX* inside, int32_t stride,
                float outsideDistance, float insideDistance) {
    gClipOutsideDistance = outsideDistance;
    gClipInsideDistance = insideDistance;
    gClipFraction = (kEdgeBias * gClipInsideDistance) / (gClipInsideDistance - gClipOutsideDistance);
    out->rhw = Blend(inside->rhw, outside->rhw);
    gClipBytesLeft = stride - 0x18;
    gClipStep = 0;
    while (gClipBytesLeft != 0) {
        int32_t offset = gClipStep * 8 + 0x18;
        *FloatAt(out, offset) = Blend(*FloatAt(inside, offset), *FloatAt(outside, offset));
        offset = gClipStep * 8 + 0x1c;
        *FloatAt(out, offset) = Blend(*FloatAt(inside, offset), *FloatAt(outside, offset));
        gClipBytesLeft -= 8;
        gClipStep += 1;
    }
    const uint32_t scaled = cw_cvt_fp2unsigned(kColourScale * gClipFraction);
    if (outside->colour != inside->colour) {
        out->colour = InterpolateColour(inside->colour, outside->colour, scaled);
    } else {
        out->colour = inside->colour;
    }
    // The specular colour keeps the inside vertex's top three bytes; its low byte becomes the
    // (inside - outside) difference of the low bytes times the fraction, shifted down 8 and not
    // added to anything (unsigned: a negative difference sets bits all over the word).
    if (outside->specular != inside->specular) {
        const uint32_t difference = (inside->specular & 0xffu) - (outside->specular & 0xffu);
        out->specular = (inside->specular & 0xffffff00u) | ((scaled * difference) >> 8);
    } else {
        out->specular = inside->specular;
    }
}

}  // namespace

// Per channel pair (the two masks 0x00ff00ff, shifted by 8 for the other pair):
// from + (t * (to - from) >> 8), the product and the shift unsigned (a negative difference
// shifts in zeros) and each pair masked back to its two bytes.
uint32_t InterpolateColour(uint32_t from, uint32_t to, uint32_t t) {
    const uint32_t mask = 0x00ff00ffu;
    const uint32_t fromHigh = (from >> 8) & mask;
    const uint32_t highStep = t * (((to >> 8) & mask) - fromHigh);
    const uint32_t fromLow = from & mask;
    const uint32_t lowStep = t * ((to & mask) - fromLow);
    const uint32_t high = ((fromHigh + (highStep >> 8)) & mask) << 8;
    const uint32_t low = (fromLow + (lowStep >> 8)) & mask;
    return low | high;
}

// Plane 0: y <= w, distance -y + w; y of the new vertex is w.
void VertexClip_Bottom(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride) {
    MakeVertex(out, outside, inside, stride, -outside->y + outside->rhw, -inside->y + inside->rhw);
    out->x = Blend(inside->x, outside->x);
    out->y = out->rhw;
    out->z = Blend(inside->z, outside->z);
}

// Plane 1: y >= -w, distance y + w; y of the new vertex is -w.
void VertexClip_Top(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride) {
    MakeVertex(out, outside, inside, stride, outside->y + outside->rhw, inside->y + inside->rhw);
    out->x = Blend(inside->x, outside->x);
    out->y = -out->rhw;
    out->z = Blend(inside->z, outside->z);
}

// Plane 2: x <= w, distance -x + w; x of the new vertex is w.
void VertexClip_Right(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride) {
    MakeVertex(out, outside, inside, stride, -outside->x + outside->rhw, -inside->x + inside->rhw);
    out->x = out->rhw;
    out->y = Blend(inside->y, outside->y);
    out->z = Blend(inside->z, outside->z);
}

// Plane 3: x >= -w, distance x + w; x of the new vertex is -w.
void VertexClip_Left(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride) {
    MakeVertex(out, outside, inside, stride, outside->x + outside->rhw, inside->x + inside->rhw);
    out->x = -out->rhw;
    out->y = Blend(inside->y, outside->y);
    out->z = Blend(inside->z, outside->z);
}

// Plane 4: z <= w, distance -z + w; z of the new vertex is w.
void VertexClip_Back(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride) {
    MakeVertex(out, outside, inside, stride, -outside->z + outside->rhw, -inside->z + inside->rhw);
    out->x = Blend(inside->x, outside->x);
    out->y = Blend(inside->y, outside->y);
    out->z = out->rhw;
}

// Plane 5: z >= 0, distance z (no sum); z of the new vertex is 0.
void VertexClip_Front(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride) {
    MakeVertex(out, outside, inside, stride, outside->z, inside->z);
    out->x = Blend(inside->x, outside->x);
    out->y = Blend(inside->y, outside->y);
    out->z = 0.0f;
}
