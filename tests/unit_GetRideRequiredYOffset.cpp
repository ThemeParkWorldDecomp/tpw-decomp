// Test library for the ride placement functions (unit v5886; libtpw_unit_GetRideRequiredYOffset.dylib, tests/test_GetRideRequiredYOffset.py), main app:
//   src/PublishPanelMsg_GetRideRequiredYOffset.cpp   GetRideMasterBoundArea, GetNodeWorldVertPosnsIntoGeomBuffer (node, matrix),
//   GetRideRequiredYOffset, CorrectRidePosnForRotation, SetRidePositionAndRotation, MoveRide
//
// The callees outside the unit (CellIsRide, GetRotatedHFieldCoord, YRotateLMatrixNoTrans, Disable/EnableMeshDraw) are recording fakes;
// the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CellBase.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "m3d2.h"
#include "rsse.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

// ---- globals of other files, defined here
tag_sM3D2Header* gHeightfieldHeader;
void* gScratchGeomBuffer;
tag_sRideAnimSet* gRideAnimSets[16];
// TPGlobals has no constructor here: raw zeroed storage under the program's symbol
alignas(16) char gTPGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

// ---- the fakes
int32_t CellIsRide(CELL_MODES mode) {
    ev("CellIsRide", {static_cast<int32_t>(mode)});
    return static_cast<int32_t>(ret("CellIsRide"));
}

void GetRotatedHFieldCoord(int32_t angle, float a1, float a2, float a3, float a4, float f5, float f6, uint32_t* outX,
                           uint32_t* outZ) {
    ev("Rot", {angle, CanonBits(a1), CanonBits(a2), CanonBits(a3), CanonBits(a4), CanonBits(f5), CanonBits(f6)});
    *outX = static_cast<uint32_t>(ret("RotX"));
    *outZ = static_cast<uint32_t>(ret("RotZ"));
}

void YRotateLMatrixNoTrans(sMatrix* matrix, float c, float s) {
    ev("YRot", {lab(reinterpret_cast<char*>(matrix) - 0x10), CanonBits(c), CanonBits(s)});
}

void DisableMeshDraw(tag_sMesh* mesh) { ev("Disable", {lab(mesh)}); }
void DisableMeshDraw(tag_sMesh* mesh, HFieldXZ a, HFieldXZ b) {
    ev("DisableR", {lab(mesh), a.mF00, a.mF02, b.mF00, b.mF02});
}
void EnableMeshDraw(tag_sMesh* mesh) { ev("Enable", {lab(mesh)}); }
void EnableMeshDraw(tag_sMesh* mesh, HFieldXZ a, HFieldXZ b) {
    ev("EnableR", {lab(mesh), a.mF00, a.mF02, b.mF00, b.mF02});
}

extern "C" {

uint64_t tpw_rd_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"hfh", &gHeightfieldHeader},
        {"scratch", &gScratchGeomBuffer},
        {"sets", &gRideAnimSets[0]},
        {"tpg", &gTPGlobalsStorage},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_rd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_rd_off(uint64_t p) { return static_cast<uint32_t>(tpw::ArenaOffset(reinterpret_cast<void*>(p))); }

int32_t tpw_rd_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFC0));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFC4));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeightMap));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFE8));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFEC));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF10));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mHField));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mFA8));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mMatrix));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mVertexCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mVertices));
    out[n++] = static_cast<int64_t>(offsetof(sHField, mF18));
    out[n++] = static_cast<int64_t>(offsetof(sHField, mHeights));
    return n;
}

uint32_t tpw_rd_yoff(int32_t x, int32_t z, uint64_t a, int32_t w, int32_t h, uint32_t flags, int32_t rot) {
    float v = GetRideRequiredYOffset(x, z, reinterpret_cast<void*>(a), w, h, flags, rot);
    uint32_t bits;
    std::memcpy(&bits, &v, 4);
    return bits;
}

int32_t tpw_rd_master(uint32_t ride, uint64_t box) {
    return GetRideMasterBoundArea(ride, reinterpret_cast<sAlignedBox*>(box));
}

void tpw_rd_node(uint64_t node, uint64_t matrix) {
    GetNodeWorldVertPosnsIntoGeomBuffer(reinterpret_cast<tag_sNode*>(node), reinterpret_cast<sMatrix*>(matrix));
}

void tpw_rd_correct(uint64_t x, uint64_t z, int32_t rot, uint64_t ox, uint64_t oz, int32_t w, int32_t h) {
    CorrectRidePosnForRotation(reinterpret_cast<int32_t*>(x), reinterpret_cast<int32_t*>(z), rot,
                               reinterpret_cast<int32_t*>(ox), reinterpret_cast<int32_t*>(oz), w, h);
}

void tpw_rd_setpos(uint64_t set, int32_t ox, int32_t oz, int32_t x, int32_t z, int32_t rot, float y, uint32_t flags,
                   int32_t w, int32_t h) {
    SetRidePositionAndRotation(reinterpret_cast<tag_sRideAnimSet*>(set), ox, oz, x, z, rot, y, flags, w, h);
}

void tpw_rd_move(uint64_t set, int32_t x, int32_t z, uint64_t a, int32_t w, int32_t h, uint32_t flags) {
    MoveRide(reinterpret_cast<tag_sRideAnimSet*>(set), x, z, reinterpret_cast<void*>(a), w, h, flags);
}

}  // extern "C"
