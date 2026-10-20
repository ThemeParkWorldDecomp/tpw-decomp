// Test library for the ride preview (unit v5213; libtpw_unit_CreateRidePreview.dylib, tests/test_CreateRidePreview.py), main app:
//   src/CreateRidePreview.cpp   CreateRidePreview, DetermineRidePreviewScale and the preview block gRidePreviewBlock
//
// The callees outside the unit (InstanceRide, _stricmp, ScaleMesh, SetMatrixIdentity, SetRideMobile, SetMeshInUISpace, EnableMeshDraw,
// SetCurRideAnim, DistanceBetweenPoints) are fakes that record what the real ones read; gRideAnimSets and gRideTypes are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "MainWindowCallback.h"
#include "TPEngine.h"
#include "rsse.h"
#include "platform/macdoze.h"

#include <cstdio>
#define DBG(n) static_cast<void>(0)
using namespace calllog;

tag_sRideAnimSet* gRideAnimSets[16];   // 0x102c79a4
tag_sRideAnimSet* gRideTypes[16];      // 0x102d19b0

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

// A mesh or an animation set is told apart by a marker word the test writes at the start (mesh) or in mFlags (set).
int64_t MeshMarker(const tag_sMesh* mesh) { return mesh == nullptr ? 0 : static_cast<int64_t>(mesh->mF00); }
int64_t SetMarker(const tag_sRideAnimSet* set) { return set == nullptr ? 0 : static_cast<int64_t>(set->mFlags); }

}  // namespace

namespace NS_MacDoze {
int32_t _stricmp(const char* a, const char* b) {
    int64_t la = 0, sa = 0, lb = 0, sb = 0;
    for (const char* p = a; *p != 0; ++p) { ++la; sa += la * static_cast<uint8_t>(*p); }
    for (const char* p = b; *p != 0; ++p) { ++lb; sb += lb * static_cast<uint8_t>(*p); }
    DBG("stricmp");
    ev("stricmp", {la, sa, lb, sb});
    int32_t i = 0;
    for (;; ++i) {
        int ca = static_cast<uint8_t>(a[i]), cb = static_cast<uint8_t>(b[i]);
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return ca > cb ? 1 : -1;
        if (ca == 0) return 0;
    }
}
}  // namespace NS_MacDoze

uint32_t InstanceRide(uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, int32_t b, void* c, int32_t d, int32_t e,
                      uint32_t f, int32_t g) {
    DBG("InstanceRide");
    ev("InstanceRide", {type, flags, lab(params), a, static_cast<uint32_t>(b), lab(c), static_cast<uint32_t>(d), static_cast<uint32_t>(e), f,
                        static_cast<uint32_t>(g)});
    return static_cast<uint32_t>(ret("InstanceRide"));
}

void ScaleMesh(tag_sMesh* mesh, s3DPoint* scale) { DBG("ScaleMesh"); ev("ScaleMesh", {MeshMarker(mesh), CanonBits(scale->x), CanonBits(scale->y), CanonBits(scale->z)}); }

void SetMatrixIdentity(sMatrix* matrix) {
    ev("SetMatrixIdentity", {});
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) matrix->m[r][c] = r == c ? 1.0f : 0.0f;
}

void SetRideMobile(uint32_t& ride) { ev("SetRideMobile", {ride}); }

void SetMeshInUISpace(tag_sMesh*& mesh) { ev("SetMeshInUISpace", {MeshMarker(mesh)}); }

void EnableMeshDraw(tag_sMesh* mesh, sMatrix* matrix, int32_t flags) {
    int64_t w[16];
    for (int i = 0; i < 16; ++i) w[i] = CanonBits(matrix->m[i / 4][i % 4]);
    ev("EnableMeshDraw", {MeshMarker(mesh), static_cast<int64_t>(flags), w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]});
    ev("EnableMeshDraw2", {w[8], w[9], w[10], w[11], w[12], w[13], w[14], w[15]});
}

void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {SetMarker(set), a, anim, loop, CanonBits(speed), b});
}

float DistanceBetweenPoints(const s3DPoint* a, const s3DPoint* b) {
    DBG("Distance");
    ev("DistanceBetweenPoints", {CanonBits(a->x), CanonBits(a->y), CanonBits(a->z), CanonBits(b->x), CanonBits(b->y), CanonBits(b->z)});
    uint32_t bits = static_cast<uint32_t>(ret("Distance"));
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}

extern "C" {

uint64_t tpw_cr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"sets", gRideAnimSets},
        {"types", gRideTypes},
        {"preview", &gRidePreviewBlock},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_cr_base() {
    static_cast<void>(tpw::ArenaAllocZeroed(1));   // the arena sets its base at the first allocation
    Ptr32<char> one;
    one.mOffset = 1;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(one.get())) - 1;
}

int32_t tpw_cr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFD0));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeightMap));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));
    out[n++] = static_cast<int64_t>(sizeof(sRidePreviewInfo));
    out[n++] = static_cast<int64_t>(sizeof(sTextureInfo));
    out[n++] = static_cast<int64_t>(sizeof(sFaceSet));
    out[n++] = static_cast<int64_t>(sizeof(tag_sNode));
    out[n++] = static_cast<int64_t>(sizeof(sTexture));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mTextureCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodeCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mTextures));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mTextureInfos));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodes));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mFaceSetCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mFaceSets));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMiniNode, mMatrix));
    out[n++] = static_cast<int64_t>(offsetof(sTextureInfo, mName));
    out[n++] = static_cast<int64_t>(offsetof(sRidePreviewInfo, mTextures));
    return n;
}

uint64_t tpw_cr_create(float x, float y, float z, float w, float h, uint32_t id) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(CreateRidePreview(x, y, z, w, h, id)));
}

// out: scale, offsetX, offsetZ, rangeX, rangeZ, ratio, distance, distanceMid (floats)
void tpw_cr_determine(uint32_t ride, float w, float h, uint64_t out) {
    float* o = reinterpret_cast<float*>(static_cast<uintptr_t>(out));
    DetermineRidePreviewScale(ride, w, h, &o[0], nullptr, &o[1], &o[2], &o[3], &o[4], &o[5], &o[6], &o[7]);
}

}  // extern "C"
