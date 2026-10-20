// tb_clipping.cpp: CClipping, the engine's polygon clipper (engine 0x1003f4d4..0x10040cf0, the
// file name is not known): the outside-code test of a run of vertices, the bounding box test and
// the three ClipPoly overloads. The edge clippers they call through a table (VertexClip_*, between
// the first two and the ClipPoly overloads in the original) are in tb_clipping_edges.cpp.
//
// Floating point. The dot product of a vertex with a plane is a multiply and three fused
// multiply-adds in the order of the listing; the outside test is a plain "below zero" (a NaN is
// inside). CPP_TestData does not compare floats at all but their bit patterns as signed integers,
// the way CodeWarrior wrote a float compare it did not want to emit.
//
// Not reproduced: the original adds and multiplies addresses in 32 bits, the host does not (the
// tests keep clear of products that carry an address across 4 GB); a negative vertex count makes
// the original loop 2^32 times (it is kept, as an unsigned count).
#include "tb_clipping.h"

#include <cmath>
#include <cstring>

#include "m3d2.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_drawprimitive.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"

namespace {

// The six planes (engine data 0x10052008): x, y, z, w.
const float kPlanes[6][4] = {
    {0.0f, -1.0f, 0.0f, 1.0f},    // 0: y <= w
    {0.0f, 1.0f, 0.0f, 1.0f},     // 1: y >= -w
    {-1.0f, 0.0f, 0.0f, 1.0f},    // 2: x <= w
    {1.0f, 0.0f, 0.0f, 1.0f},     // 3: x >= -w
    {0.0f, 0.0f, -1.0f, 1.0f},    // 4: z <= w
    {0.0f, 0.0f, 1.0f, 0.0f},     // 5: z >= 0
};

using VertexClipFunction = void (*)(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride);

// The edge clippers by plane (the table of function descriptors at engine 0x10052068).
const VertexClipFunction kEdgeClippers[6] = {VertexClip_Bottom, VertexClip_Top,  VertexClip_Right,
                                             VertexClip_Left,   VertexClip_Back, VertexClip_Front};

// Entries of the temporary vertex list ClipPoly(VertexClipList) keeps on its stack. The original
// has room for fourteen (0x1c bytes) before the list's own count; a polygon of more than that
// overwrites the count. This one is larger so an overflow stays harmless; the tests keep to 14.
constexpr int32_t kWorkEntries = 32;

// The vertex `index` places on from `base`, the product taken in 32 bits as mullw does.
SFVF_TLCTEX* VertexAt(void* base, int32_t index, int32_t stride) {
    const int32_t offset = static_cast<int32_t>(static_cast<uint32_t>(index) * static_cast<uint32_t>(stride));
    return reinterpret_cast<SFVF_TLCTEX*>(static_cast<uint8_t*>(base) + offset);
}

SFVF_TLCTEX* Advance(SFVF_TLCTEX* vertex, int32_t stride) {
    return reinterpret_cast<SFVF_TLCTEX*>(reinterpret_cast<uint8_t*>(vertex) + stride);
}

// Whether the vertex is on the negative side of the plane.
bool Outside(const SFVF_TLCTEX* vertex, const float* plane) {
    float distance = vertex->y * plane[1];
    distance = ppc_fmadds(vertex->x, plane[0], distance);
    distance = ppc_fmadds(vertex->z, plane[2], distance);
    distance = ppc_fmadds(vertex->rhw, plane[3], distance);
    return distance < 0.0f;
}

// 1 when the edge from `previous` to `current` leaves the inside (current outside), 2 when it
// enters, 3 when both are outside, 0 when both are inside: the plane test of `current` plus twice
// that of `previous`, current first as in the original.
int32_t EdgeKind(const SFVF_TLCTEX* current, const SFVF_TLCTEX* previous, const float* plane) {
    const int32_t now = Outside(current, plane) ? 1 : 0;
    const int32_t before = Outside(previous, plane) ? 1 : 0;
    return now + before * 2;
}

// a > b as signed integers (the original builds it from an xor, a subtraction and the carry).
uint32_t Greater(uint32_t a, uint32_t b) { return static_cast<int32_t>(a) > static_cast<int32_t>(b) ? 1u : 0u; }

uint32_t Bits(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

constexpr uint32_t kSign = 0x80000000u;

}  // namespace

// The test of a vertex in the original: with w below zero (ordered) the other coordinates are
// compared with |w| and z - 2w, otherwise with w; the NaN of an invalid subtraction is the
// PowerPC's default one (positive) on any host.
void CClipping::CPP_TestData(uint8_t* result, void* clipping, uint8_t* codes, const void* vertices, int32_t count,
                             int32_t stride, uint32_t mask) {
    uint32_t all = 0xff;
    uint32_t any = 0;
    const uint8_t* vertex = static_cast<const uint8_t*>(vertices);
    for (uint32_t left = static_cast<uint32_t>(count); left != 0; --left, vertex += stride) {
        float w;
        uint32_t x, y, z, wBits;
        std::memcpy(&w, vertex + 0xc, 4);
        std::memcpy(&x, vertex + 0, 4);
        std::memcpy(&y, vertex + 4, 4);
        std::memcpy(&z, vertex + 8, 4);
        std::memcpy(&wBits, vertex + 0xc, 4);
        uint32_t code;
        if (w < 0.0f) {
            const uint32_t magnitude = wBits & 0x7fffffffu;
            float zFloat;
            std::memcpy(&zFloat, vertex + 8, 4);
            const float shifted = zFloat - (w + w);
            uint32_t shiftedBits = Bits(shifted);
            if (std::isnan(shifted)) shiftedBits = std::isnan(zFloat) ? Bits(zFloat) | 0x00400000u : 0x7fc00000u;
            code = (Greater(magnitude, y ^ kSign) << 5) | (Greater(magnitude, y) << 4) |
                   (Greater(magnitude, x ^ kSign) << 3) | (Greater(magnitude, x) << 2) |
                   (Greater(shiftedBits, magnitude) << 1) | (z >> 31);
        } else {
            code = (Greater(y, wBits) << 5) | (Greater(y ^ kSign, wBits) << 4) | (Greater(x, wBits) << 3) |
                   (Greater(x ^ kSign, wBits) << 2) | (Greater(z, wBits) << 1) | (z >> 31);
        }
        code &= mask;
        *codes++ = static_cast<uint8_t>(code);
        all &= code;
        any |= code;
    }
    result[0] = static_cast<uint8_t>(all);
    result[1] = static_cast<uint8_t>(any);
}

// The box grown by 5% of its size on every side, as eight corners (x from the first four to the
// last four, y alternating in pairs, z alternating), through the math unit's strided transform
// (type 0: xyz in, xyzw out) and the outside codes. mMinZ is set after the transform: zero, then
// the smallest z of the eight transformed corners if one is below it. The assertion text is that
// of an inline in tb_matrix.h (line 152) which checks the math unit before calling it; the
// original goes on to call through the null pointer.
uint8_t CClipping::CheckBoundingBox(const sAlignedBox& box, CMatrix& matrix) {
    const float margin = 0.05f;
    const float dx = margin * (box.mMax[0] - box.mMin[0]);
    const float dy = margin * (box.mMax[1] - box.mMin[1]);
    const float dz = margin * (box.mMax[2] - box.mMin[2]);
    float corners[8][3];
    for (int32_t i = 0; i < 8; ++i) {
        corners[i][0] = i < 4 ? box.mMin[0] - dx : box.mMax[0] + dx;
        corners[i][1] = (i & 2) == 0 ? box.mMin[1] - dy : box.mMax[1] + dy;
        corners[i][2] = (i & 1) == 0 ? box.mMin[2] - dz : box.mMax[2] + dz;
    }
    float transformed[8][4];
    uint8_t codes[8];
    uint8_t result[4];
    if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x98);
    gei_MathUnit->Transform(&matrix, transformed, corners, 16, 12, 8, 0);
    mMinZ = 0.0f;
    for (int32_t i = 7; i >= 0; --i) {
        if (transformed[i][2] < mMinZ) mMinZ = transformed[i][2];
    }
    CPP_TestData(result, this, codes, transformed, 8, 16, 0xff);
    if (result[0] != 0) return 0;
    return static_cast<uint8_t>(result[1] | 0x40);
}

// One pass per set bit of `planes`, from plane 5 (bit 0) up to plane 0 (bit 5); the loop ends when
// no bit is left (a negative mask keeps its sign and runs all six). Each pass walks the edges of
// the polygon from the last vertex to the first, writing the kept vertices and the vertices the
// edge clippers make to `out`, then swaps the roles of `out` and `in`.
int32_t CClipping::ClipPoly(int32_t planes, SFVF_TLCTEX* out, SFVF_TLCTEX* in, int32_t& count, int32_t stride) {
    SFVF_TLCTEX* const first = out;
    int32_t mask = planes;
    for (int32_t step = 0; step < 6 && mask != 0; ++step, mask >>= 1) {
        if ((mask & 1) == 0) continue;
        const float* plane = kPlanes[5 - step];
        const VertexClipFunction clip = kEdgeClippers[5 - step];
        SFVF_TLCTEX* destination = out;
        SFVF_TLCTEX* current = in;
        SFVF_TLCTEX* previous = VertexAt(in, count - 1, stride);
        int32_t kept = 0;
        for (uint32_t left = static_cast<uint32_t>(count); left != 0; --left) {
            switch (EdgeKind(current, previous, plane)) {
            case 1:
                clip(destination, current, previous, stride);
                destination = Advance(destination, stride);
                ++kept;
                break;
            case 2:
                clip(destination, previous, current, stride);
                destination = Advance(destination, stride);
                ++kept;
                [[fallthrough]];
            case 0:
                std::memcpy(destination, current, static_cast<size_t>(stride));
                destination = Advance(destination, stride);
                ++kept;
                break;
            default:
                break;
            }
            previous = current;
            current = Advance(current, stride);
        }
        count = kept;
        if (count <= 2) return 0;
        SFVF_TLCTEX* const spare = out;
        out = in;
        in = spare;
    }
    if (first == out) std::memcpy(out, in, static_cast<uint32_t>(count) * static_cast<uint32_t>(stride));
    return 1;
}

// The same walk over a list of vertex numbers. New vertices go after the vertices already there
// (at vertices[count], count grows with each) and their numbers into the output list; the two
// lists alternate between the caller's list and a temporary one, and the result is copied back
// into the caller's when it ended in the temporary.
void CClipping::ClipPoly(int32_t planes, SFVF_TLCTEX* vertices, VertexClipList* list, int32_t& count,
                         int32_t stride) {
    uint16_t scratch[kWorkEntries];
    VertexClipList work;
    work.mCount = 0;
    work.mList = scratch;
    VertexClipList* from = list;
    VertexClipList* to = &work;
    int32_t mask = planes;
    for (int32_t step = 0; step < 6 && mask != 0; ++step, mask >>= 1) {
        if ((mask & 1) == 0) continue;
        const float* plane = kPlanes[5 - step];
        const VertexClipFunction clip = kEdgeClippers[5 - step];
        int32_t kept = 0;
        SFVF_TLCTEX* fresh = VertexAt(vertices, count, stride);
        SFVF_TLCTEX* previous = from->mCount > 0 ? VertexAt(vertices, from->mList[from->mCount - 1], stride) : nullptr;
        for (int32_t i = 0; i < from->mCount; ++i) {
            SFVF_TLCTEX* current = VertexAt(vertices, from->mList[i], stride);
            switch (EdgeKind(current, previous, plane)) {
            case 1:
                clip(fresh, current, previous, stride);
                fresh = Advance(fresh, stride);
                to->mList[kept++] = static_cast<uint16_t>(count);
                ++count;
                break;
            case 2:
                clip(fresh, previous, current, stride);
                fresh = Advance(fresh, stride);
                to->mList[kept++] = static_cast<uint16_t>(count);
                ++count;
                [[fallthrough]];
            case 0:
                to->mList[kept++] = from->mList[i];
                break;
            default:
                break;
            }
            previous = current;
        }
        to->mCount = kept;
        VertexClipList* const spare = from;
        from = to;
        to = spare;
    }
    if (list == to) {
        std::memcpy(to->mList, from->mList, static_cast<uint32_t>(from->mCount) << 1);
        to->mCount = from->mCount;
    }
}

// Through gei_Clipping, not this (the original loads the global and calls with it). The fan is
// written from the last triangle back, reading the polygon's entries as it goes (the writes
// land on entries already read).
uint16_t* CClipping::ClipPoly(int32_t planes, uint16_t* indices, SFVF_TLCTEX* vertices, int32_t& count,
                              int32_t stride) {
    VertexClipList list;
    list.mCount = 3;
    list.mList = indices - 3;
    gei_Clipping->ClipPoly(planes, vertices, &list, count, stride);
    const int32_t polygon = list.mCount;
    if (polygon <= 3) return indices - (3 - polygon);
    uint16_t* const first = list.mList;
    uint16_t* end = first + (polygon - 2) * 3;
    for (int32_t triangle = 0; triangle < polygon - 3; ++triangle) {
        end[-1] = first[polygon - 1 - triangle];
        end[-2] = first[polygon - 2 - triangle];
        end[-3] = first[0];
        end -= 3;
    }
    return first + (polygon - 2) * 3;
}
