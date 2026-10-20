// Test library for the ride mesh helpers (unit v5338; libtpw_unit_CalculateRootNodeFaceNormals.dylib, tests/test_CalculateRootNodeFaceNormals.py), main app:
//   src/RideTypes_CalculateRootNodeFaceNormals.cpp   CalculateRootNodeFaceNormals, GetBlendedBaseTextures, GetNodeWorldVertPosnsIntoGeomBuffer
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues:
// FloatToIntGlue (0x10237cd8), CMesh::GenerateRideBase, ERRORBox, CombineMatricesNoWDest1st (dest += source, word by word).
// gScratchGeomBuffer and gHeightfieldHeader are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "rsse.h"
#include "tb_matrix.h"
#include "tb_mesh.h"

using namespace calllog;

// ---- globals of other files, defined here
void* gScratchGeomBuffer;
tag_sM3D2Header* gHeightfieldHeader;

int32_t FloatToIntGlue(float value);

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t HashMatrix(const float* m) {
    uint32_t h = 0;
    for (int i = 0; i < 16; ++i) h = h * 31u + static_cast<uint32_t>(CanonBits(m[i]));
    return h;
}

}  // namespace

// ---- the fakes
int32_t FloatToIntGlue(float value) {
    ev("Ftoi", {CanonBits(value)});
    return static_cast<int32_t>(ret("Ftoi"));
}

int32_t CMesh::GenerateRideBase(int32_t a, int32_t b, int32_t c, int32_t d) {
    ev("GenerateRideBase", {lab(this), a, b, c, d});
    return static_cast<int32_t>(ret("GenerateRideBase"));
}

void ERRORBox(const char* format, ...) {
    va_list args;
    va_start(args, format);
    const char* name = va_arg(args, const char*);
    va_end(args);
    ev("ERRORBox", {}, {format, name});
}

void CombineMatricesNoWDest1st(sMatrix* destination, sMatrix* source) {
    float* d = &destination->m[0][0];
    const float* s = &source->m[0][0];
    ev("Combine", {HashMatrix(s), HashMatrix(d)});
    for (int i = 0; i < 16; ++i) d[i] = d[i] + s[i];
}

extern "C" {

uint64_t tpw_rg_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"scratch", &gScratchGeomBuffer},
        {"hfheader", &gHeightfieldHeader},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_rg_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_rg_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

// The fixed (file layout) offsets first, then the host's native offsets.
int32_t tpw_rg_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = offsetof(tag_sNode, mF04);
    out[n++] = offsetof(tag_sNode, mMatrix);
    out[n++] = offsetof(tag_sNode, mVertexCount);
    out[n++] = offsetof(tag_sNode, mFaceSetCount);
    out[n++] = offsetof(tag_sNode, mVertices);
    out[n++] = offsetof(tag_sNode, mFaceSets);
    out[n++] = offsetof(tag_sNode, mFaces);
    out[n++] = offsetof(tag_sNode, mF94);
    out[n++] = offsetof(tag_sM3D2Header, mName);
    out[n++] = offsetof(tag_sM3D2Header, mHField);
    out[n++] = offsetof(tag_sM3D2Header, mF78);
    out[n++] = offsetof(sHField, mF08) + 8;
    out[n++] = offsetof(sHField, mF08) + 12;
    out[n++] = offsetof(sHField, mF18);
    out[n++] = offsetof(sHField, mQuads);
    // native
    out[n++] = offsetof(tag_sMesh, mHeader);
    out[n++] = offsetof(tag_sMesh, mF10);
    out[n++] = offsetof(tag_sMesh, mEngineMesh);
    out[n++] = offsetof(tag_sMesh, mSkirtVertices);
    out[n++] = offsetof(tag_sRideAnimSet, mSelf);
    out[n++] = offsetof(tag_sRideAnimSet, mHeads);
    out[n++] = offsetof(tag_sRideAnimSet, mFC0);
    out[n++] = offsetof(tag_sRideAnimSet, mFC4);
    out[n++] = offsetof(tag_sRideAnimSet, mHeightMap);
    out[n++] = sizeof(tag_sMesh);
    out[n++] = sizeof(tag_sRideAnimSet);
    return n;
}

void tpw_rg_normals(uint64_t node, int32_t rotation, int32_t* result) {
    CalculateRootNodeFaceNormals(reinterpret_cast<tag_sNode*>(node), rotation, *result);
}

void tpw_rg_blended(uint64_t set) { GetBlendedBaseTextures(reinterpret_cast<tag_sRideAnimSet*>(set)); }

void tpw_rg_worldverts(uint64_t node) { GetNodeWorldVertPosnsIntoGeomBuffer(reinterpret_cast<tag_sNode*>(node)); }

}  // extern "C"
