// Harness for the height field's view gamut (src/tb_hfield_gamut.cpp, src/tb_polygon.cpp):
// CGamut::CalculateGamut, CGamut::GetHFieldIntersection, the three CPolygon members and the two
// clip vectors, against the original (tests/test_HFieldGamut.py).
//
// The unit's callees are fakes that record through calllog:
//  - CMatrix::TransformVertex (the view matrix's rotation of a corner ray) records the matrix,
//    the type, where the input lies in the gamut and the input's words, and writes the next
//    three numbers of the case's "dir" queue as the direction, so a case chooses every ray;
//  - CMatrix::AdjustProjection records the projection matrix and the far plane;
//  - the assertion (the clip vectors') records its line, condition and file.
// The engine objects the unit reads (the display, the view and projection matrices, the field of
// view) are plain globals. The gamut itself is raw storage that the test fills from a byte
// image in the host's layout (the test packs it little-endian) and reads back the same way.
//
// NaNs are canonical (0x7fc00000) wherever a word is recorded: the two processors differ in the
// NaN they make.
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/msl.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_polygon.h"
#include "tb_resource_display.h"

using namespace calllog;

namespace {

enum : int32_t { kLabGamut = 1, kLabView = 2, kLabProjection = 3 };

alignas(16) unsigned char gGamutStorage[sizeof(CGamut)];
CGamut& Gamut() { return *reinterpret_cast<CGamut*>(gGamutStorage); }
CMatrix gView;   // the camera's matrix: only row 3 (the position) is read

uint32_t Word(const void* p) {
    uint32_t w;
    std::memcpy(&w, p, 4);
    return w;
}
uint32_t Canon(uint32_t word) {
    return (word & 0x7f800000u) == 0x7f800000u && (word & 0x007fffffu) != 0 ? 0x7fc00000u : word;
}
float FloatOf(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}
_D3DVECTOR VectorOf(const uint32_t* w) {
    _D3DVECTOR v;
    v.x = FloatOf(w[0]);
    v.y = FloatOf(w[1]);
    v.z = FloatOf(w[2]);
    return v;
}
void Put(uint32_t* out, const _D3DVECTOR& v) {
    out[0] = Word(&v.x);
    out[1] = Word(&v.y);
    out[2] = Word(&v.z);
}

}  // namespace

// ---- the engine objects the unit reads
CDisplay::CDisplay() {}
CDisplay gei_Display;
CMatrix* gei_ViewMatrix = &gView;
CMatrix gei_ProjectionMatrix;
float gProjectionFOV = 0.0f;

// ---- the fakes
void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("assert", {line}, {condition, file});
}

// The ray: three numbers from the queue "dir" (the case's words). The input is a vector that
// lies at gamut + offset (a corner of the gamut): its offset and words are recorded.
void CMatrix::TransformVertex(void* out, void* in, int32_t type) {
    const int64_t offset = static_cast<unsigned char*>(in) - gGamutStorage;
    ev("TransformVertex", {lab(this), type, offset, Canon(Word(static_cast<uint32_t*>(in))),
                           Canon(Word(static_cast<uint32_t*>(in) + 1)), Canon(Word(static_cast<uint32_t*>(in) + 2))});
    uint32_t direction[3];
    for (uint32_t& w : direction) w = static_cast<uint32_t>(ret("dir"));
    std::memcpy(out, direction, sizeof direction);
}

void CMatrix::AdjustProjection(float farPlane) { ev("AdjustProjection", {lab(this), fbits(farPlane)}); }

extern "C" {

// The objects the test names by label: 1 the gamut, 2 the view matrix, 3 the projection matrix.
void* tpw_hg_pointer(int32_t which) {
    switch (which) {
    case 0: return gGamutStorage;
    case 1: return &gView;
    default: return &gei_ProjectionMatrix;
    }
}

// Sets the globals of a case: the view matrix's 16 words, the field of view, the display's
// aspect (mF04) and first-person view distance.
void tpw_hg_globals(const uint32_t* view, uint32_t fov, uint32_t aspect, uint32_t distance) {
    std::memcpy(&gView, view, sizeof gView);
    gProjectionFOV = FloatOf(fov);
    gei_Display.mF04 = FloatOf(aspect);
    gei_Display.mFirstPersonViewDistance = FloatOf(distance);
}

// Reads the globals back (the unit must not change them): view words, fov, aspect, distance (19 words).
void tpw_hg_globals_back(uint32_t* out) {
    std::memcpy(out, &gView, sizeof gView);
    out[16] = Word(&gProjectionFOV);
    out[17] = Word(&gei_Display.mF04);
    out[18] = Word(&gei_Display.mFirstPersonViewDistance);
}

// The gamut as a byte image (sizeof(CGamut) bytes, host layout): in before a call, out after.
void tpw_hg_gamut_set(const unsigned char* image) { std::memcpy(gGamutStorage, image, sizeof(CGamut)); }
void tpw_hg_gamut_get(unsigned char* image) { std::memcpy(image, gGamutStorage, sizeof(CGamut)); }

// op 0: CalculateGamut (returns its answer); op 1: GetHFieldIntersection of corner `corner`
// (0..3; the vector goes to out[0..2]).
int32_t tpw_hg_gamut_run(int32_t op, int32_t corner, uint32_t* out) {
    if (op == 0) return Gamut().CalculateGamut();
    const _D3DVECTOR hit = Gamut().GetHFieldIntersection(reinterpret_cast<_D3DVECTOR*>(Gamut().mCorners[corner]));
    Put(out, hit);
    return 0;
}

// The clip vectors: axis 0 is GenerateClipXVector, 1 GenerateClipZVector.
void tpw_hg_clip(int32_t axis, const uint32_t* a, const uint32_t* b, uint32_t plane, uint32_t* out) {
    const _D3DVECTOR va = VectorOf(a);
    const _D3DVECTOR vb = VectorOf(b);
    const float p = FloatOf(plane);
    Put(out, axis == 0 ? GenerateClipXVector(va, vb, p) : GenerateClipZVector(va, vb, p));
}

// The polygon members on a polygon given as vertex words (48), status bytes (16) and the two
// counts; they come back changed. op 0: IsOutsideOfPlane(v, index) (returns the float's bits);
// op 1: GetNewVertexOutsideStatus(v); op 2: DumpData(nonzero ? a pointer : null).
uint32_t tpw_hg_polygon(int32_t op, uint32_t* vertices, int8_t* status, int32_t* counts, const uint32_t* v,
                        int32_t index) {
    CPolygon polygon;
    std::memcpy(polygon.mVertices, vertices, sizeof polygon.mVertices);
    std::memcpy(polygon.mOutside, status, sizeof polygon.mOutside);
    polygon.mCount = counts[0];
    polygon.mOutsideCount = counts[1];
    uint32_t result = 0;
    const _D3DVECTOR vector = VectorOf(v);
    if (op == 0) {
        const float side = polygon.IsOutsideOfPlane(vector, index);
        result = Canon(Word(&side));
    } else if (op == 1) {
        result = static_cast<uint32_t>(polygon.GetNewVertexOutsideStatus(vector));
    } else {
        _D3DVECTOR dummy = vector;
        polygon.DumpData(index != 0 ? &dummy : nullptr);
    }
    std::memcpy(vertices, polygon.mVertices, sizeof polygon.mVertices);
    std::memcpy(status, polygon.mOutside, sizeof polygon.mOutside);
    counts[0] = polygon.mCount;
    counts[1] = polygon.mOutsideCount;
    return result;
}

}  // extern "C"
