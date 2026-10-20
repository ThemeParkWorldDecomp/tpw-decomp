// Test library for GenerateRideHeightMap (unit v5675; libtpw_unit_GenerateRideHeightMap.dylib, tests/test_GenerateRideHeightMap.py), main app:
//   src/RideDestroy_LoadFileIntoMemory_GenerateRideHeightMap.cpp   GenerateRideHeightMap and the global gRideHeightRows
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The
// globals other files define (the height map's limits and stride) are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "rsse.h"
#include "CBalanceTable.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
int32_t gRideHeightStride;
float gRideHeightLimitZ;
float gRideHeightLimitX;

namespace {

const void* gKnown[16];       // the objects the test names (slot + 1 is the label)
void* gAllocs[8];             // the blocks MReleaseAllocF handed out
int gAllocCount = 0;
uint32_t gCallCount = 0;      // CalculateMaxVertexHeights calls so far

int64_t I(uint64_t v) { return static_cast<int64_t>(v); }

int64_t Id(const void* p) {
    if (p == nullptr) return 0;
    for (int i = 0; i < 16; i++)
        if (gKnown[i] == p) return i + 1;
    return -1;
}

float FromBits(int64_t bits) {
    uint32_t b = static_cast<uint32_t>(bits);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

uint16_t* Halves(sRideHeightMap* map) { return reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(map) + 8); }

}  // namespace

// ---- the fakes
int32_t InstanceRideAnimSet(tag_sRideAnimSet* set, sHeapInfo* heap, uint32_t flags, tag_sRideAnimSet** output, uint32_t a) {
    ev("InstanceRideAnimSet", {Id(set), I(reinterpret_cast<uintptr_t>(heap)), I(flags), I(a)});
    int32_t ok = static_cast<int32_t>(ret("Inst"));
    if (ok != 0) *output = const_cast<tag_sRideAnimSet*>(static_cast<const tag_sRideAnimSet*>(gKnown[1]));
    return ok;
}

int32_t InitRideAnimSet(tag_sRideAnimSet* set, int32_t init, int32_t mode, int32_t keepNodes) {
    ev("InitRideAnimSet", {Id(set), I(static_cast<uint32_t>(init)), I(static_cast<uint32_t>(mode)), I(static_cast<uint32_t>(keepNodes))});
    return 0;
}

void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {I(size), I(flags)});
    void* p = tpw::ArenaAllocZeroed(size);
    if (gAllocCount < 8) gAllocs[gAllocCount++] = p;
    return p;
}

void GetAnimStats(tag_sRideAnimSet* set, uint32_t index, uint32_t* kind, uint32_t* place, uint32_t* other) {
    ev("GetAnimStats", {Id(set), I(index)});
    *kind = static_cast<uint32_t>(ret("gas_kind"));
    *place = static_cast<uint32_t>(ret("gas_idx"));
    *other = static_cast<uint32_t>(ret("gas_x"));
}

void SetRideAnimTime(uint32_t time) { ev("SetRideAnimTime", {I(time)}); }

void ResetAnimTimeStats(sAnimTimeControl* control, tag_sAnimHeader* animation, int32_t mode, float limit) {
    (void)control;
    ev("ResetAnimTimeStats", {Id(animation), I(static_cast<uint32_t>(mode)), I(fbits(limit))});
}

void GetAnimTimeStats(sAnimTimeControl* control, uint32_t* time) {
    (void)control;
    ev("GetAnimTimeStats", {I(*time)});
}

void SetNewAnim(tag_sRideAnimSet* set, uint32_t kind, uint32_t index, sAnimTimeControl* control, uint32_t flags, float speed) {
    ev("SetNewAnim", {Id(set), I(kind), I(index), I(flags), I(fbits(speed))});
    control->mF04 = kind;
    control->mF08 = index;
}

float GetAnimRemainingMilliseconds(sAnimTimeControl* control) {
    (void)control;
    return FromBits(ret("rem"));
}

int32_t RATick(sAnimTimeControl* control, tag_sMesh* mesh, tag_sAnimHeader* animation) {
    ev("RATick", {Id(mesh), Id(animation)});
    control->mF20 = FromBits(ret("tickf"));
    return 0;
}

void SetMatrixIdentity(sMatrix* matrix) {
    ev("SetMatrixIdentity", {});
    std::memset(matrix, 0, sizeof(*matrix));
    for (int i = 0; i < 4; i++) matrix->m[i][i] = 1.0f;
}

int32_t CalculateMaxVertexHeights(tag_sNode* node, tag_sM3D2Header* header, sMatrix matrix, sRideHeightMap* map, tag_sNode** badNode,
                                  tag_sAnimHeader* anim, int32_t unused, int32_t* flag) {
    uint32_t words[16];
    std::memcpy(words, &matrix, 64);
    ev("CMH", {Id(node), Id(header), I(map == gAllocs[0]), Id(anim), I(static_cast<uint32_t>(unused)), I(static_cast<uint32_t>(*flag)),
               I(badNode != nullptr)});
    ev("CMHM", {I(words[0]), I(words[1]), I(words[2]), I(words[3]), I(words[4]), I(words[5]), I(words[6]), I(words[7])});
    ev("CMHM", {I(words[8]), I(words[9]), I(words[10]), I(words[11]), I(words[12]), I(words[13]), I(words[14]), I(words[15])});
    uint32_t mm[6];
    std::memcpy(mm, map->mMin, 12);
    std::memcpy(mm + 3, map->mMax, 12);
    ev("CMHG", {I(fbits(gRideHeightLimitX)), I(fbits(gRideHeightLimitZ)), I(static_cast<uint32_t>(gRideHeightStride)),
                I(static_cast<uint32_t>(gRideHeightRows)), I(mm[0]), I(mm[1]), I(mm[2]), I(mm[3]), I(mm[4]), I(mm[5])});
    uint32_t seed = static_cast<uint32_t>(ret("cmh"));
    gCallCount++;
    uint32_t n = static_cast<uint32_t>(Halves(map)[0]) * Halves(map)[1] * 25;
    uint8_t* cells = static_cast<uint8_t*>(gAllocs[1]);
    for (uint32_t i = 0; i < n; i++) {
        uint8_t b = static_cast<uint8_t>(i * seed + gCallCount * 31 + (i >> 2));
        if (b > cells[i]) cells[i] = b;
    }
    return 1;
}

void QuickCopyUnmaskedMeshVertices(tag_sMesh* from, tag_sMesh* to, int32_t force) {
    ev("QuickCopyUnmaskedMeshVertices", {Id(from), Id(to), I(static_cast<uint32_t>(force))});
}

void ResetRideAnimationSingleChannel(tag_sRideAnimSet* set, tag_sAnimHeader* animation, tag_sAnimHeader* other) {
    ev("ResetRideAnimationSingleChannel", {Id(set), Id(animation), Id(other)});
}

void DestroyRideAnimSetInstance(tag_sRideAnimSet* set, sHeapInfo* heap, uint32_t flags) {
    ev("DestroyRideAnimSetInstance", {Id(set), I(reinterpret_cast<uintptr_t>(heap)), I(flags)});
}

extern "C" {

uint64_t tpw_gh_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"stride", &gRideHeightStride},
        {"limz", &gRideHeightLimitZ},
        {"limx", &gRideHeightLimitX},
        {"rows", &gRideHeightRows},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_gh_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_gh_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

void tpw_gh_known(int32_t slot, uint64_t p) { gKnown[slot] = reinterpret_cast<const void*>(static_cast<uintptr_t>(p)); }

void tpw_gh_reset() {
    gAllocCount = 0;
    gCallCount = 0;
    std::memset(gAllocs, 0, sizeof(gAllocs));
    std::memset(gKnown, 0, sizeof(gKnown));
}

uint64_t tpw_gh_alloc_get(int32_t i) { return i < gAllocCount ? static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gAllocs[i])) : 0; }

int32_t tpw_gh_alloc_count() { return gAllocCount; }

#define OFF(T, m) static_cast<int64_t>(__builtin_offsetof(T, m))
int32_t tpw_gh_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = OFF(tag_sRideAnimSet, mHeads);
    out[n++] = OFF(tag_sRideAnimSet, mGroups);
    out[n++] = sizeof(sRideAnimGroup);
    out[n++] = OFF(sRideAnimGroup, mCount);
    out[n++] = OFF(sRideAnimGroup, mEntries);
    out[n++] = OFF(tag_sRideAnimSet, mAnimationCount);
    out[n++] = OFF(tag_sRideAnimSet, mAnimations);
    out[n++] = sizeof(sRideAnimEntry);
    out[n++] = OFF(sRideAnimEntry, mAnimation);
    out[n++] = OFF(tag_sMesh, mHeader);
    out[n++] = OFF(tag_sMesh, mF08);
    out[n++] = sizeof(tag_sRideAnimSet);
    out[n++] = sizeof(tag_sMesh);
    out[n++] = sizeof(tag_sNode);
    out[n++] = OFF(tag_sM3D2Header, mNodeCount);
    out[n++] = OFF(tag_sM3D2Header, mNodes);
    out[n++] = OFF(tag_sM3D2Header, mF78);
    out[n++] = OFF(tag_sM3D2Header, mBox);
    out[n++] = sizeof(tag_sM3D2Header);
    out[n++] = sizeof(sAnimTimeControl);
    out[n++] = sizeof(RideShape);
    out[n++] = OFF(RideShape, mCells);
    out[n++] = sizeof(RideShapeCell);
    out[n++] = OFF(sRideHeightMap, mCells);
    out[n++] = OFF(sRideHeightMap, mF10);
    out[n++] = OFF(sRideHeightMap, mMin);
    out[n++] = OFF(sRideHeightMap, mMax);
    out[n++] = OFF(tag_sNode, mFlags);
    return n;
}

uint64_t tpw_gh_gen(uint64_t set, uint32_t w, uint32_t h, uint64_t shape, int32_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(GenerateRideHeightMap(
        reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(set)), w, h,
        reinterpret_cast<RideShape*>(static_cast<uintptr_t>(shape)), flag)));
}

}  // extern "C"
