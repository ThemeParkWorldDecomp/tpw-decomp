// tb_rotatevertex.cpp (engine library of SimTheme Park 2.04b3; the inventory files the range
// under Wavelet.cpp, next to GetCycleCount): the two turns of a quad of vertices.
//
//   0x10038cd0 RotateVertex_Y  (404 bytes)
//   0x10038e9c RotateVertex_Z  (428)
//
// Both turn the four vertices of a quad (0x10 bytes apart) in place about a centre. Y turns x
// and y by an angle given as a fraction of a turn through the engine's 256-entry sine table;
// Z turns x and z by an angle in radians through the C library's cosine and sine. The sky's
// added sprites, the particles and the sprite quads (SSpriteGameData::SetupVertices) use them.
//
// Each vertex is turned in the order 3, 2, 1, 0, in single precision with one fused
// multiply-add per coordinate, as the listing has them.
#include <cmath>

#include "Wavelet.h"
#include "ppc_math.h"
#include "tb_hfield.h"
#include "tb_matrix.h"

namespace {

// Turns the pair (a, b) of one vertex about (ca, cb): the vertex minus the centre is stored
// first, then read back; the new pair is the centre plus (a * c + s * b, -a * s + c * b).
void Turn(float& a, float& b, float ca, float cb, float c, float s) {
    a = a - ca;
    b = b - cb;
    const float x = a;
    const float y = b;
    const float negated = -x;
    a = ca + ppc_fmadds(x, c, s * y);
    b = cb + ppc_fmadds(negated, s, c * y);
}

}  // namespace

// The angle is a fraction of a turn: 256 * angle, cut to an integer, picks the sine (that
// entry of gSineTable) and the cosine (the entry a quarter turn on, 0x40 further). A NaN or
// an angle beyond 2^23 turns picks entry 0 or 0xff as the integer's low byte says. With no
// centre the middle of the quad is used: x halfway between vertices 0 and 1, y halfway
// between vertices 0 and 2.
void RotateVertex_Y(SVertex* vertices, float angle, SVertex* centre) {
    float cx;
    float cy;
    if (centre != nullptr) {
        cx = centre->x;
        cy = centre->y;
    } else {
        cx = 0.5f * (vertices[0].x + vertices[1].x);
        cy = 0.5f * (vertices[0].y + vertices[2].y);
    }
    const int32_t turn = ppc_fctiwz(static_cast<double>(256.0f * angle));
    const float cosine = gSineTable[static_cast<uint32_t>(turn + 0x40) & 0xff];
    const float sine = gSineTable[static_cast<uint32_t>(turn) & 0xff];
    for (int32_t i = 3; i >= 0; --i) Turn(vertices[i].x, vertices[i].y, cx, cy, cosine, sine);
}

// The cosine and sine are the library's (double precision, cut to single); the centre is the
// middle of the quad in x and z and is worked out before them.
void RotateVertex_Z(SVertex* vertices, float angle) {
    const float cx = 0.5f * (vertices[0].x + vertices[1].x);
    const float cz = 0.5f * (vertices[0].z + vertices[2].z);
    const float cosine = static_cast<float>(std::cos(static_cast<double>(angle)));
    const float sine = static_cast<float>(std::sin(static_cast<double>(angle)));
    for (int32_t i = 3; i >= 0; --i) Turn(vertices[i].x, vertices[i].z, cx, cz, cosine, sine);
}
