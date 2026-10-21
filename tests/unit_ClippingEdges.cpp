// Entry points for the clipper's edge functions (library unit_ClippingEdges:
// src/engine/Clipping.cpp). The unit calls nothing outside itself, so there are no fakes: a
// test hands each clipper two vertices and a stride and compares the vertex it makes and the
// five working globals with the original's (tests/test_ClippingEdges.py).
#include <cstring>

#include "tb_clipping.h"
#include "tb_drawprimitive.h"

extern "C" {

// One of the six clippers (0 Bottom, 1 Top, 2 Right, 3 Left, 4 Back, 5 Front) on the caller's vertices.
void tpw_ce_clip(int32_t which, void* out, void* outside, void* inside, int32_t stride) {
    SFVF_TLCTEX* o = static_cast<SFVF_TLCTEX*>(out);
    SFVF_TLCTEX* a = static_cast<SFVF_TLCTEX*>(outside);
    SFVF_TLCTEX* b = static_cast<SFVF_TLCTEX*>(inside);
    switch (which) {
    case 0: VertexClip_Bottom(o, a, b, stride); break;
    case 1: VertexClip_Top(o, a, b, stride); break;
    case 2: VertexClip_Right(o, a, b, stride); break;
    case 3: VertexClip_Left(o, a, b, stride); break;
    case 4: VertexClip_Back(o, a, b, stride); break;
    default: VertexClip_Front(o, a, b, stride); break;
    }
}

// The two integers and the three doubles the clippers leave behind.
void tpw_ce_statics(int32_t* ints, double* doubles) {
    ints[0] = gClipStep;
    ints[1] = gClipBytesLeft;
    doubles[0] = gClipFraction;
    doubles[1] = gClipInsideDistance;
    doubles[2] = gClipOutsideDistance;
}

uint32_t tpw_ce_colour(uint32_t from, uint32_t to, uint32_t t) { return InterpolateColour(from, to, t); }

}  // extern "C"
