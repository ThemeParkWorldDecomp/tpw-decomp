// tb_polygon.cpp (engine library of SimTheme Park 2.04b3; the original is tb_gamut.cpp, the
// file name the assertion in the clip vectors reports): the footprint polygon of CGamut's
// view volume and the two functions that cut an edge at a plane. CPolygon::DumpData
// 0x1000b748, GenerateClipXVector 0x1000b7dc, GenerateClipZVector 0x1000b930,
// CPolygon::GetNewVertexOutsideStatus 0x1000ba74, CPolygon::IsOutsideOfPlane 0x1000bb9c.
//
// Quirks kept: the clip vectors report a parameter outside -1..1 (a NaN too) through the MSL
// assertion and then use it all the same; the x and z a clip vector does not take from its
// plane come from a fused multiply-add of the parameter and the edge's extent.
#include "tb_polygon.h"

#include "platform/msl.h"
#include "ppc_math.h"

namespace {

// Which side of the edge from vertex index - 1 (the last vertex for index 0) to vertex `index`
// the point lies on, as the listing does it in both IsOutsideOfPlane and the loop of
// GetNewVertexOutsideStatus: the edge's x extent times the point's z offset, plus the negated z
// extent times the point's x offset, the second product fused.
float EdgeSide(const CPolygon& polygon, _D3DVECTOR v, int32_t index) {
    const int32_t before = index - 1 < 0 ? polygon.mCount - 1 : index - 1;
    const float* previous = polygon.mVertices[before];
    const float* current = polygon.mVertices[index];
    const float zOffset = v.z - previous[2];
    const float xExtent = current[0] - previous[0];
    const float zExtent = current[2] - previous[2];
    const float xOffset = v.x - previous[0];
    const float product = xExtent * zOffset;
    return ppc_fmadds(-zExtent, xOffset, product);
}

// The assertion every clip vector makes about the edge's parameter.
void CheckParameter(float t, int32_t line) {
    if (!(t <= 1.0f && t >= -1.0f)) msl_assertion_failed("(f<=1) && (f>=-1)", "tb_gamut.cpp", line);
}

}  // namespace

void CPolygon::DumpData(_D3DVECTOR* data) {
    // The Mac's body returns at once for a pointer; for a null one it counts mCount down in
    // two loops with empty bodies (a trace that was compiled out). Nothing is written.
    (void)data;
}

int32_t CPolygon::GetNewVertexOutsideStatus(_D3DVECTOR v) {
    int32_t any = 0;
    mOutsideCount = 0;
    for (int32_t index = mCount - 1; index >= 0; --index) {
        mOutside[index] = EdgeSide(*this, v, index) > 0.0f ? 1 : 0;
        mOutsideCount += mOutside[index];
        any |= mOutside[index];
    }
    return any;
}

float CPolygon::IsOutsideOfPlane(_D3DVECTOR v, int32_t index) {
    return EdgeSide(*this, v, index);
}

_D3DVECTOR GenerateClipXVector(_D3DVECTOR a, _D3DVECTOR b, float x) {
    const float t = (x - b.x) / (a.x - b.x);
    CheckParameter(t, 0xf6);
    _D3DVECTOR result;
    result.x = x;
    result.y = ppc_fmadds(t, a.y - b.y, b.y);
    result.z = ppc_fmadds(t, a.z - b.z, b.z);
    return result;
}

_D3DVECTOR GenerateClipZVector(_D3DVECTOR a, _D3DVECTOR b, float z) {
    const float t = (z - b.z) / (a.z - b.z);
    CheckParameter(t, 0x103);
    _D3DVECTOR result;
    result.x = ppc_fmadds(t, a.x - b.x, b.x);
    result.y = ppc_fmadds(t, a.y - b.y, b.y);
    result.z = z;
    return result;
}
