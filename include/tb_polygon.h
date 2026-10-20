// tb_polygon.h: the footprint polygon CGamut::CalculateGamut builds (engine library, original
// tb_gamut.cpp: the assertion in the clip vectors names that file). The type's name is the one
// the mangled names give; the member names are invented.
//
// The Mac keeps the polygon as a local of CalculateGamut: up to 16 vertices (x, y, z), one
// outside-status byte per vertex, the vertex count and the number of outside statuses the last
// GetNewVertexOutsideStatus found. Its fourth member function, the one that puts a vertex into
// the polygon (here AddVertex), is inlined at five places of CalculateGamut and has no
// out-of-line copy; the listing's inlined code is the source of AddVertex below.
#pragma once
#include <cstdint>
#include <cstring>

#include "platform/d3d_types.h"

class CPolygon {
public:
    enum : int32_t { kMaxVertices = 16 };

    // Does nothing but read mCount: with a null pointer the Mac loops over the count (the
    // body of a trace that was compiled out), with anything else it returns at once.
    void DumpData(_D3DVECTOR* data);   // 0x1000b748 (96 bytes)

    // Writes the outside status (0 or 1) of every vertex against the edge from its predecessor
    // for the point `v`, sets mOutsideCount to the number of ones and returns the OR of the
    // statuses.
    int32_t GetNewVertexOutsideStatus(_D3DVECTOR v);   // 0x1000ba74 (228 bytes)

    // The side of the edge from vertex index - 1 (the last vertex for index 0) to vertex
    // `index` that `v` lies on: above 0 is outside.
    float IsOutsideOfPlane(_D3DVECTOR v, int32_t index);   // 0x1000bb9c (104 bytes)

    // The inlined member of CalculateGamut (five copies, 0x10009f04..0x1000b6dc): adds a
    // point to the convex polygon of the footprint on the ground, keeping the polygon convex.
    void AddVertex(_D3DVECTOR v);

    float mVertices[kMaxVertices][3];   // 0x00
    int8_t mOutside[kMaxVertices];      // 0xc0  1 when the vertex lies outside the edge before it
    int32_t mCount;                     // 0xd0
    int32_t mOutsideCount;              // 0xd4
};
static_assert(sizeof(CPolygon) == 0xd8, "CPolygon layout");

// The point where the edge from `b` to `a` crosses the plane x = `x` (z = `z`): the point's
// other coordinate comes from the edge's parameter t = (x - b.x) / (a.x - b.x), which must lie
// in -1..1 (otherwise the assertion of tb_gamut.cpp line 0xf6 (Z: 0x103) is reported and the
// result is used anyway). The Mac returns the vector through a hidden pointer in r3.
_D3DVECTOR GenerateClipXVector(_D3DVECTOR a, _D3DVECTOR b, float x);   // 0x1000b7dc (272 bytes)
_D3DVECTOR GenerateClipZVector(_D3DVECTOR a, _D3DVECTOR b, float z);   // 0x1000b930 (256 bytes)

// ---- AddVertex ------------------------------------------------------------------------------
// The code of the five inlined copies is one: they differ only in which registers they use.
//  - 0 vertices: the point is the first one. 1 vertex: the point becomes the second one unless
//    its x and z are both those of the first. 2 vertices: IsOutsideOfPlane(point, 1) says which
//    side of the first edge it lies on; on one side it becomes the third vertex (after the old
//    second one, which it replaces as the second), on the other it is appended, and exactly on
//    the line (or when the answer is a NaN) it only replaces an end of the segment when it
//    lies beyond it (the parameter along x, or along z where the segment has no x extent).
//  - 3 or more: GetNewVertexOutsideStatus marks the edges the point lies outside of. With none
//    marked nothing happens. With one marked the point is inserted at the first marked
//    vertex. With two or more marked the vertices and the marks are rotated so that the run
//    of marked ones ends the list, and the polygon is rebuilt as the point, one vertex of the
//    rotated list (index marks - 1, see `pick`) and then every later unmarked vertex.
inline void CPolygon::AddVertex(_D3DVECTOR v) {
    auto put = [this](int32_t index, float x, float y, float z) {
        mVertices[index][0] = x;
        mVertices[index][1] = y;
        mVertices[index][2] = z;
    };
    if (mCount < 3) {
        if (mCount == 0) {
            mCount = 1;
            put(0, v.x, v.y, v.z);
        } else if (mCount == 1) {
            if (v.x != mVertices[0][0] || v.z != mVertices[0][2]) {
                put(1, v.x, v.y, v.z);
                mCount = 2;
            }
        } else {
            const float side = IsOutsideOfPlane(v, 1);
            if (side > 0.0f) {
                put(mCount, mVertices[1][0], mVertices[1][1], mVertices[1][2]);
                ++mCount;
                put(1, v.x, v.y, v.z);
            } else if (side < 0.0f) {
                put(mCount, v.x, v.y, v.z);
                ++mCount;
            } else {
                float t;
                if (0.0f == mVertices[1][0] - mVertices[0][0]) {
                    t = (v.z - mVertices[0][2]) / (mVertices[1][2] - mVertices[0][2]);
                } else {
                    t = (v.x - mVertices[0][0]) / (mVertices[1][0] - mVertices[0][0]);
                }
                if (t > 1.0f) {
                    put(1, v.x, v.y, v.z);
                } else if (t < 0.0f) {
                    put(0, v.x, v.y, v.z);
                }
            }
        }
        return;
    }

    if (GetNewVertexOutsideStatus(v) == 0) return;
    int32_t first = 0;
    while (mOutside[first] == 0) ++first;
    if (mOutsideCount < 2) {
        std::memmove(mVertices[first + 1], mVertices[first], static_cast<size_t>(mCount - first) * 12);
        put(first, v.x, v.y, v.z);
        ++mCount;
        return;
    }

    // Two or more edges outside: rotate vertices and statuses so that the run of outside
    // ones ends the list. `last` ends up on the vertex before the run (-1 when all are outside).
    int32_t last = mCount - 1;
    while (last >= 0 && mOutside[last] == 0) --last;
    while (last >= 0 && mOutside[last] != 0) --last;
    std::memcpy(&mOutside[mCount], &mOutside[0], static_cast<size_t>(last + 1));
    std::memmove(&mOutside[0], &mOutside[last + 1], static_cast<size_t>(mCount));
    std::memcpy(mVertices[mCount], mVertices[0], static_cast<size_t>(last + 1) * 12);
    std::memmove(mVertices[0], mVertices[last + 1], static_cast<size_t>(mCount) * 12);
    float rotated[kMaxVertices][3];
    std::memcpy(rotated, mVertices, static_cast<size_t>(mCount) * 12);

    // The vertex that follows the point: index (marks - 1), or the first one with a marked
    // successor when that is 0.
    int32_t pick = mOutsideCount - 1;
    if (pick > mCount) pick -= mCount;
    if (pick == 0) {
        while (mOutside[pick + 1] == 0) ++pick;
    }
    put(0, v.x, v.y, v.z);
    put(1, rotated[pick][0], rotated[pick][1], rotated[pick][2]);
    int32_t out = 2;
    int32_t i = mCount == 1 ? 0 : 1;
    while (i != 0) {
        if (mOutside[i] == 0) {
            put(out, rotated[i][0], rotated[i][1], rotated[i][2]);
            ++out;
        }
        ++i;
        if (i == mCount) i = 0;
    }
    mCount = out;
}
