// Test library for GetNodePositionSimple, SetRideSpritePosnFocus and GetMeshPtrAtHFieldCell (unit v5258;
// libtpw_unit_GetNodePositionSimple.dylib, tests/test_GetNodePositionSimple.py), main app:
//   src/LevelSystems_GetNodePositionSimple.cpp
//
// The callees SetupMeshForPolySearchSimple and GetRideIDAtCell are fakes that record their arguments; the globals of other files the
// unit reads or writes are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "LevelSystems.h"
#include "Ptr32.h"
#include "TPEngine.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"

using namespace calllog;

void SetRideSpritePosnFocus(tag_sMesh* mesh);

// ---- globals of other files
tag_sM3D2Header* gHeightfieldHeader;
tag_sRideAnimSet* gRideAnimSets[64];
Ptr32<uint8_t> gRideCtxRef;
uint32_t gRideFlags;

// ---- the fakes
void SetupMeshForPolySearchSimple(tag_sMesh* mesh, int32_t flag) {
    ev("SetupMesh", {lab(mesh), flag});
}

int32_t GetRideIDAtCell(int32_t x, int32_t y) {
    ev("GetRideIDAtCell", {static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
    return static_cast<int32_t>(ret("GetRideIDAtCell"));
}

extern "C" {

uint64_t tpw_gn_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"hfhdr", &gHeightfieldHeader},
        {"sets", &gRideAnimSets[0]},
        {"ctx", &gRideCtxRef},
        {"flags", &gRideFlags},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_gn_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_gn_off(uint64_t p) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(p))); }

int32_t tpw_gn_ctx_is(uint64_t p) { return gRideCtxRef.get() == reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(p)) ? 1 : 0; }

int32_t tpw_gn_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF28));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mFA0));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mHField));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mF04));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mMatrix));
    out[n++] = static_cast<int64_t>(offsetof(sHField, mF18));
    out[n++] = static_cast<int64_t>(sizeof(s3DPoint));
    return n;
}

void tpw_gn_node(uint64_t node, uint64_t out, int32_t unused) {
    GetNodePositionSimple(reinterpret_cast<tag_sNode*>(static_cast<uintptr_t>(node)),
                          reinterpret_cast<s3DPoint*>(static_cast<uintptr_t>(out)), unused);
}

void tpw_gn_focus(uint64_t mesh) { SetRideSpritePosnFocus(reinterpret_cast<tag_sMesh*>(static_cast<uintptr_t>(mesh))); }

uint64_t tpw_gn_cell(uint32_t x, uint32_t y) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(GetMeshPtrAtHFieldCell(x, y))); }

}  // extern "C"
