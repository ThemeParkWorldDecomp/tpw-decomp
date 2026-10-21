// Test library for CalculateMaxVertexHeights (unit v5061; libtpw_unit_CalculateMaxVertexHeights.dylib, tests/test_CalculateMaxVertexHeights.py), main app:
//   src/game/park/TPHField.cpp   CalculateMaxVertexHeights and the four globals it uses
//
// The callees outside the unit are recording fakes that answer from the test's queues:
// CombineMatricesNoWDest2nd (changes two floats of the destination) and VectorThroughLMatrixNoW (replaces the point).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "m3d2.h"
#include "rsse.h"

using namespace calllog;

namespace {

int64_t Bits(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, 4);
    return bits;
}

float FromBits(int64_t bits) {
    uint32_t b = static_cast<uint32_t>(bits);
    float value;
    std::memcpy(&value, &b, 4);
    return value;
}

}  // namespace

void CombineMatricesNoWDest2nd(sMatrix* source, sMatrix* destination) {
    const float* s = reinterpret_cast<const float*>(source);
    float* d = &destination->m[0][0];
    ev("Combine", {Bits(s[0]), Bits(s[12]), Bits(d[0]), Bits(d[12]), Bits(d[13])});
    d[0] = FromBits(ret("cmb"));
    d[13] = s[0];
}

void VectorThroughLMatrixNoW(sMatrix* matrix, s3DPoint* point) {
    const float* m = &matrix->m[0][0];
    ev("Vector", {Bits(point->x), Bits(point->y), Bits(point->z), Bits(m[0]), Bits(m[1]), Bits(m[4]), Bits(m[5]), Bits(m[10]),
                  Bits(m[12]), Bits(m[13]), Bits(m[14])});
    point->x = FromBits(ret("vec"));
    point->y = FromBits(ret("vec"));
    point->z = FromBits(ret("vec"));
}

extern "C" {

uint64_t tpw_cmvh_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"scan", &gRideHeightScan[0][0]},
        {"stride", &gRideHeightStride},
        {"limz", &gRideHeightLimitZ},
        {"limx", &gRideHeightLimitX},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cmvh_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_cmvh_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_cmvh_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mF08));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mF0C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mMatrix));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mVertexCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mFaceSetCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mVertices));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mFaceSets));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mFaces));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mF94));
    out[n++] = static_cast<int64_t>(sizeof(sFace));
    out[n++] = static_cast<int64_t>(sizeof(sFaceSet));
    out[n++] = static_cast<int64_t>(offsetof(sRideHeightMap, mCells));
    out[n++] = static_cast<int64_t>(offsetof(sRideHeightMap, mMin));
    out[n++] = static_cast<int64_t>(offsetof(sRideHeightMap, mMax));
    return n;
}

int32_t tpw_cmvh_run(uint64_t node, const float* matrix, uint64_t map, uint64_t badNode, int32_t unused, uint64_t flag) {
    sMatrix m;
    std::memcpy(&m, matrix, sizeof(m));
    return CalculateMaxVertexHeights(reinterpret_cast<tag_sNode*>(node), nullptr, m, reinterpret_cast<sRideHeightMap*>(map),
                                     reinterpret_cast<tag_sNode**>(badNode), nullptr, unused, reinterpret_cast<int32_t*>(flag));
}

}  // extern "C"
