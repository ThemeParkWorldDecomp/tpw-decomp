// Test library for DoMeshSkirtDeform (unit v5693; libtpw_unit_DoMeshSkirtDeform.dylib, tests/test_DoMeshSkirtDeform.py), main app:
//   src/Debug_blendflags_DoMeshSkirtDeform.cpp   DoMeshSkirtDeform
//
// DoOrSetupSkirtDeform is a recording fake (every argument; the by-value matrix in two events). SetMatrixIdentity is the real one.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "Debug.h"

using namespace calllog;

namespace {

int64_t Bits(float value) { return fbits(value); }

}  // namespace

void DoOrSetupSkirtDeform(tag_sNode* a, tag_sNode* b, uint16_t** list, sMatrix matrix, sHField* hf, int32_t setup, int32_t* count,
                          float lift, tag_sNode** bestNode, int32_t* bestCount, uint32_t flags, tag_sNode* only, int32_t mode) {
    ev("Skirt", {lab(a), lab(b), lab(*list), lab(hf), setup, lab(count), Bits(lift), lab(bestNode), lab(bestCount),
                 static_cast<int64_t>(flags), lab(only), mode});
    ev("SkirtM1", {Bits(matrix.m[0][0]), Bits(matrix.m[0][1]), Bits(matrix.m[0][2]), Bits(matrix.m[0][3]), Bits(matrix.m[1][0]),
                   Bits(matrix.m[1][1]), Bits(matrix.m[1][2]), Bits(matrix.m[1][3])});
    ev("SkirtM2", {Bits(matrix.m[2][0]), Bits(matrix.m[2][1]), Bits(matrix.m[2][2]), Bits(matrix.m[2][3]), Bits(matrix.m[3][0]),
                   Bits(matrix.m[3][1]), Bits(matrix.m[3][2]), Bits(matrix.m[3][3])});
}

extern "C" {

uint64_t tpw_dms_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset of a host address (what a Ptr32 holds).
uint32_t tpw_dms_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_dms_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF08));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mBlock14));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodeCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mHField));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodes));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mSelf));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mF98));
    out[n++] = static_cast<int64_t>(sizeof(tag_sNode));
    out[n++] = static_cast<int64_t>(offsetof(sTexture, mTexture));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));
    out[n++] = static_cast<int64_t>(sizeof(tag_sM3D2Header));
    return n;
}

void tpw_dms_run(uint64_t mesh, uint64_t header, float y, uint32_t a) {
    DoMeshSkirtDeform(reinterpret_cast<tag_sMesh*>(static_cast<uintptr_t>(mesh)),
                      reinterpret_cast<tag_sM3D2Header*>(static_cast<uintptr_t>(header)), y, a);
}

}  // extern "C"
