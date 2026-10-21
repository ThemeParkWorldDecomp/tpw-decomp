// Test library for the ride pointer pick (unit v51021; libtpw_unit_RayPolyCheckOnRide.dylib, tests/test_RayPolyCheckOnRide.py), main app:
//   src/game/TPPointer.cpp   RayPolyCheckOnRide, FindPointedRide, the ray globals
//
// The callees outside the unit are fakes that record what the PowerPC stubs record; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "MapCell.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files, defined here
int32_t gHeightfieldHeight;
int32_t gHeightfieldWidth;
s3DPoint gPointedRidePosition;
void* gScratchGeomBuffer;
tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances];

namespace {

uint32_t Hash(const void* words, int n) {
    const uint32_t* w = static_cast<const uint32_t*>(words);
    uint32_t h = 0;
    for (int i = 0; i < n; i++) h = h * 31u + w[i];
    return h;
}

uint32_t Bits(float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return b;
}

float FromBits(int64_t b) {
    uint32_t u = static_cast<uint32_t>(b);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

}  // namespace

// ---- the fakes
void CombineMatricesNoWDest2nd(sMatrix* source, sMatrix* destination) { ev("Combine", {Hash(source, 16), Hash(destination, 16)}); }

void GetNodeWorldVertPosnsIntoGeomBuffer(tag_sNode* node, sMatrix* matrix) {
    ev("NodeVerts", {node->mName.mOffset, Hash(matrix, 16)});
}

void VectorThroughLMatrixNoWNoTrans(sMatrix* matrix, s3DPoint* point) {
    ev("VecThrough", {Hash(matrix, 16), Bits(point->x), Bits(point->y), Bits(point->z)});
    float x = point->x, y = point->y, z = point->z;
    point->x = y;
    point->y = z;
    point->z = x;
}

float GetWorldGSquareHeight(int32_t x, int32_t y, uint32_t* extra) {
    ev("GSQHeight", {x, y, *extra});
    *extra = static_cast<uint32_t>(ret("GSQExtra"));
    return FromBits(ret("GSQHeight"));
}

sBraceMeshList* GetPylonFromBraceMesh(uint32_t id, uint32_t* out) {
    ev("GetPylon", {id});
    uint32_t p = static_cast<uint32_t>(ret("Pylon"));
    *out = static_cast<uint32_t>(ret("PylonCount"));
    return reinterpret_cast<sBraceMeshList*>(static_cast<uintptr_t>(p));
}

void* GetNthBraceMesh(sBraceMeshList* list, uint32_t n) {
    ev("GetNth", {static_cast<uint32_t>(reinterpret_cast<uintptr_t>(list)), n});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("NthId"))));
}

void RecalculateInvalidFaceNormals(tag_sM3D2Header* header) { ev("Recalc", {header->mF2C}); }

void SetMatrixIdentity(sMatrix* matrix) {
    ev("SetIdent", {});
    for (int i = 0; i < 16; i++) (&matrix->m[0][0])[i] = 1.0f + 0.25f * static_cast<float>(i);
}

extern "C" {

uint64_t tpw_rpc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"hitnormal", &gRayHitNormal},     {"hitpoint", &gRayHitPoint},          {"origin", &gRayOrigin},
        {"direction", &gRayDirection},     {"pointednormal", &gPointedRideNormal}, {"pointedpos", &gPointedRidePosition},
        {"geom", &gScratchGeomBuffer},     {"width", &gHeightfieldWidth},        {"height", &gHeightfieldHeight},
        {"ridesets", &gRideAnimSets[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_rpc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_rpc_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase())); }

int32_t tpw_rpc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(void*));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mMatrix));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mName));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mFaceSetCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mFaceSets));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mFaces));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mF94));
    out[n++] = static_cast<int64_t>(sizeof(sFaceSet));
    out[n++] = static_cast<int64_t>(sizeof(sFace));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF2C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mBox) + 4);
    return n;
}

uint32_t tpw_rpc_ray(uint64_t node, const uint32_t* m) {
    sMatrix matrix;
    std::memcpy(&matrix, m, sizeof(matrix));
    return static_cast<uint32_t>(RayPolyCheckOnRide(reinterpret_cast<tag_sNode*>(static_cast<uintptr_t>(node)), matrix));
}

uint64_t tpw_rpc_find(uint32_t cellX, uint32_t cellZ, int32_t axis, float x, float y, float z, float yStep, uint64_t direction,
                      uint64_t camera, float step) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        FindPointedRide(cellX, cellZ, axis, x, y, z, yStep, reinterpret_cast<s3DPoint*>(static_cast<uintptr_t>(direction)),
                        reinterpret_cast<s3DPoint*>(static_cast<uintptr_t>(camera)), step)));
}

}  // extern "C"
