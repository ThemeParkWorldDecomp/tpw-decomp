// Test library for FLY_Draw (unit v5007; libtpw_unit_FLY_Draw.dylib, tests/test_FLY_Draw.py), main app:
//   src/game/rides/FlyingRide.cpp   FLY_Draw and its working globals
//
// The callees outside the unit (the matrix helpers, FindDataNodeIndexByID, PAR_SetEmitterPos and the float-to-angle function at 0x10237cd8)
// are fakes that record the arguments the real ones read and answer from the test's queues. gFlyBases and gRideAnimSets belong to other
// files and are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "HLSprite.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "rsse.h"

using namespace calllog;

void* gFlyBases[100];
tag_sRideAnimSet* gRideAnimSets[32];

namespace {

int64_t MatrixMarker(const sMatrix* m) {
    uint32_t bits;
    std::memcpy(&bits, &m->m[3][3], 4);
    return bits;
}

int64_t Bits(float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, 4);
    return bits;
}

}  // namespace

void SetLMatrixIdentityNoTrans(sMatrix* matrix) { ev("SetIdentity", {MatrixMarker(matrix)}); }

void RotateLMatrixXYZNoTrans(sMatrix* matrix, eRotateParams axis, float* angle) {
    ev("Rotate", {MatrixMarker(matrix), static_cast<int64_t>(axis), Bits(angle[0]), Bits(angle[1]), Bits(angle[2])});
}

uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    ev("Find", {static_cast<int64_t>(set->mFD4), static_cast<int64_t>(mask), static_cast<int64_t>(id)});
    return static_cast<uint32_t>(ret("Find"));
}

void PAR_SetEmitterPos(int32_t handle, int32_t x, int32_t y, int32_t z) { ev("Emitter", {handle, x, y, z}); }

int32_t FLY_FloatToInt(float value) {
    const int32_t answer = static_cast<int32_t>(static_cast<uint32_t>(Bits(value)) * 2654435761u);   // the module computes the same
    ev("F2I", {Bits(value), static_cast<int64_t>(static_cast<uint32_t>(answer))});
    return answer;
}

extern "C" {

uint64_t tpw_fd_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gFlyBases", gFlyBases},       {"gRideAnimSets", gRideAnimSets}, {"gFlyDrawPosX", &gFlyDrawPosX}, {"gFlyDrawPosY", &gFlyDrawPosY},
        {"gFlyDrawPosZ", &gFlyDrawPosZ}, {"gFlyDrawAngA", &gFlyDrawAngA},  {"gFlyDrawAngB", &gFlyDrawAngB}, {"gFlyDrawAngC", &gFlyDrawAngC},
        {"gFlyDrawOrigin", gFlyDrawOrigin},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The Ptr32 value of a host address.
uint32_t tpw_fd_off(uint64_t address) { return static_cast<uint32_t>(tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address)))); }

int32_t tpw_fd_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFD4));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF28));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mMatrix));
    out[n++] = static_cast<int64_t>(offsetof(sEmbeddedSet, mRecords));
    out[n++] = static_cast<int64_t>(sizeof(sEmbeddedRecord));
    out[n++] = static_cast<int64_t>(offsetof(sEmbeddedRecord, mBlock));
    out[n++] = static_cast<int64_t>(sizeof(void*));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));
    out[n++] = static_cast<int64_t>(sizeof(tag_sM3D2Header));
    out[n++] = static_cast<int64_t>(sizeof(tag_sNode));
    return n;
}

void tpw_fd_draw(float step) { FLY_Draw(step); }

}  // extern "C"
