// Test library for GenerateHoardingMeshes (unit v5625; libtpw_unit_GenerateHoardingMeshes.dylib, tests/test_GenerateHoardingMeshes.py),
// main app:
//   src/game/park/TPHoardings.cpp   GenerateHoardingMeshes, gHoardingTexturePathFormat and the three sort tables
//
// The callees outside the unit (MReleaseAllocF, HoardingCentreAngle, FloatSortByArray, DMCreateMesh, SetMatrixIdentity,
// DetermineInnerBaseInsets) are fakes that record what the real ones read and answer from the test's queues; MReleaseAllocF and
// DMCreateMesh hand out arena blocks and remember them for the module. gRideHoardingTextures (src/game/DynamicMeshSet.cpp) is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "CBalanceTable.h"
#include "LevelSystems.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"
#include "tb_matrix.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
sDTextureSet gRideHoardingTextures;

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float FromBits(int64_t value) {
    const uint32_t bits = static_cast<uint32_t>(value);
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

void* gAllocs[16];
int32_t gAllocCount = 0;
void* gDM[6];   // DMCreateMesh's blocks: vertex sets, points, UV sets, faces, matrix, face set

int64_t AllocIndex(const void* p) {
    for (int32_t i = 0; i < gAllocCount && i < 16; ++i)
        if (gAllocs[i] == p) return i;
    return -1;
}

void* Block(uint32_t size) { return tpw::ArenaAllocZeroed(size + 16); }

}  // namespace

// ---- the fakes
// The host passes its own sizes for the three structures (the Mac's 0x40, 0x30 and 0x10): the call in its place with the host size
// records the Mac's.
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    int64_t mac = size;
    if (gAllocCount == 0 && size == sizeof(tag_sDMeshSet)) mac = 0x40;
    if (gAllocCount == 1 && size == sizeof(sDMeshEntry)) mac = 0x30;
    if (gAllocCount == 3 && size == sizeof(sHoardingGeometry)) mac = 0x10;
    ev("MReleaseAllocF", {mac, static_cast<int64_t>(flags)});
    void* p = Block(size);
    if (gAllocCount < 16) gAllocs[gAllocCount] = p;
    ++gAllocCount;
    return p;
}

float HoardingCentreAngle(int32_t x, int32_t y, int32_t side, float halfWidth, float halfHeight) {
    ev("HoardingCentreAngle", {x, y, side, CanonBits(halfWidth), CanonBits(halfHeight)});
    return FromBits(ret("HoardingCentreAngle"));
}

void FloatSortByArray(int32_t count, void** source, void** scratch, int32_t depth, int32_t offset) {
    ev("FloatSortByArray", {count, source == gHoardingSortSource ? 1 : 0, scratch == gHoardingSortScratch ? 1 : 0, depth, offset});
    if (count <= 0) return;
    const intptr_t base = reinterpret_cast<intptr_t>(source[0]);
    for (int32_t i = 0; i < count; ++i) {
        const intptr_t p = reinterpret_cast<intptr_t>(source[i]);
        ev("FloatSortValue", {i, static_cast<int64_t>((p - base) / 4), CanonBits(*static_cast<float*>(source[i]))});
    }
    const int64_t mode = ret("FloatSortByArray");
    for (int32_t i = 0; i < count; ++i) {
        int32_t from;
        if (mode < 0)
            from = count - 1 - i;
        else
            from = static_cast<int32_t>((i + mode) % count);
        scratch[i] = source[from];
    }
}

int32_t DMCreateMesh(tag_sDMeshSet* set, sDTextureSet* textures, const char* name, uint32_t flags) {
    sDMeshEntry* entry = set->mEntries;
    const uint32_t n = entry->mPointCount;
    ev("DMCreateMesh", {AllocIndex(set), textures == &gRideHoardingTextures ? 1 : 0, static_cast<int64_t>(flags), AllocIndex(entry), n},
       {name, textures->mFolder != nullptr ? textures->mFolder : ""});
    const uint32_t seed = static_cast<uint32_t>(ret("FacePattern"));
    gDM[0] = set->mVertexSets = static_cast<sVertexPosnSet*>(Block(n * 0x30));
    gDM[1] = set->mPoints = static_cast<s3DPoint*>(Block(n * 5 * 12));
    gDM[2] = set->mUVSets = static_cast<sUVSet*>(Block(n * 0x20));
    auto* faces = static_cast<sFace*>(Block(n * 0x20));
    gDM[3] = set->mFaces = faces;
    auto* words = reinterpret_cast<uint16_t*>(faces);
    for (uint32_t k = 0; k < n * 16; ++k) words[k] = static_cast<uint16_t>(k * 0x9e37u + seed);
    gDM[4] = entry->mMatrix = static_cast<sMatrix*>(Block(0x40));
    auto* faceSet = static_cast<sFaceSet*>(Block(0x10));
    std::memset(reinterpret_cast<uint8_t*>(faceSet) + 4, 0x77, 0x0c);
    gDM[5] = entry->mFaceSets = faceSet;
    return static_cast<int32_t>(ret("DMCreateMesh"));
}

void SetMatrixIdentity(sMatrix* matrix) {
    ev("SetMatrixIdentity", {matrix == gDM[4] ? 1 : (matrix == nullptr ? 0 : -1)});
}

void DetermineInnerBaseInsets(s2DPoint* corners, s3DPoint* points, int32_t count, s2DPoint* insets) {
    ev("DetermineInnerBaseInsets", {count, CanonBits(corners[0].x), CanonBits(corners[0].y), CanonBits(corners[1].x),
                                    CanonBits(corners[1].y), CanonBits(corners[2].x), CanonBits(corners[2].y), CanonBits(corners[3].x),
                                    CanonBits(corners[3].y)});
    for (int32_t i = 0; i < count; ++i)
        ev("DIBIPoint", {i, CanonBits(points[i].x), CanonBits(points[i].y), CanonBits(points[i].z)});
    for (int32_t k = 0; k < 4; ++k) {
        insets[k].x = FromBits(ret("Insets"));
        insets[k].y = FromBits(ret("Insets"));
    }
}

extern "C" {

uint64_t tpw_ghm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gRideHoardingTextures", &gRideHoardingTextures},
        {"gHoardingSortScratch", gHoardingSortScratch},
        {"gHoardingSortSource", gHoardingSortSource},
        {"gHoardingSortOrder", gHoardingSortOrder},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ghm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets the module reads and writes by (the module's LAYOUT_KEYS order).
int32_t tpw_ghm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHoardingGeometry));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mDynamicMeshSet));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF0C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mRootNode));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));
    out[n++] = static_cast<int64_t>(offsetof(tag_sDMeshSet, mEntries));
    out[n++] = static_cast<int64_t>(offsetof(tag_sDMeshSet, mVertexSets));
    out[n++] = static_cast<int64_t>(offsetof(tag_sDMeshSet, mPoints));
    out[n++] = static_cast<int64_t>(offsetof(tag_sDMeshSet, mUVSets));
    out[n++] = static_cast<int64_t>(offsetof(tag_sDMeshSet, mFaces));
    out[n++] = static_cast<int64_t>(offsetof(sDMeshEntry, mMatrix));
    out[n++] = static_cast<int64_t>(offsetof(sDMeshEntry, mFaceSets));
    out[n++] = static_cast<int64_t>(offsetof(sHoardingGeometry, mBlock));
    out[n++] = static_cast<int64_t>(offsetof(sDTextureSet, mFolder));
    out[n++] = static_cast<int64_t>(offsetof(sDTextureSet, mTextures));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mMatrix));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mVertexCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mVertices));
    out[n++] = static_cast<int64_t>(offsetof(RideShape, mCells));
    out[n++] = static_cast<int64_t>(offsetof(sDMeshEntry, mBox));
    out[n++] = static_cast<int64_t>(offsetof(sDMeshEntry, mPointCount));
    return n;
}

void tpw_ghm_reset() {
    gAllocCount = 0;
    for (void*& p : gAllocs) p = nullptr;
    for (void*& p : gDM) p = nullptr;
}

// The blocks the fakes handed out: i in [0, 16) MReleaseAllocF's (i = -1: how many), 16 + j DMCreateMesh's j-th.
uint64_t tpw_ghm_block(int32_t i) {
    if (i < 0) return static_cast<uint64_t>(gAllocCount);
    if (i < 16) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gAllocs[i]));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gDM[i - 16]));
}

// A Ptr32 field: the word that points at `target`, and the host address a field at `field` points at.
uint32_t tpw_ghm_ptr32(uint64_t target) { return target == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<const void*>(target)); }
uint64_t tpw_ghm_ptr32_get(uint64_t field) {
    const auto* p = reinterpret_cast<const Ptr32<sTexture>*>(field);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p->get()));
}

void tpw_ghm_run(uint64_t theme, uint64_t set, uint64_t shape) {
    GenerateHoardingMeshes(reinterpret_cast<const char*>(theme), reinterpret_cast<tag_sRideAnimSet*>(set),
                           reinterpret_cast<void*>(shape));
}

}  // extern "C"
