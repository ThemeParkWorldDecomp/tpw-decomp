// Test library for src/RideAnimInterp.cpp (libtpw_unit_RideAnimInterp.dylib; see
// tests/test_RideAnimInterp.py): the node animation's fifteen interpolators.
//
// The functions call nothing of the game (only the C library's modf, fmod, sqrt and acos, which
// the PowerPC side stubs with Python's math and this side takes from libm), so there is nothing
// to fake. What is here: TPGlobals' sine table and flags, which RASphericalLinearInterpolate,
// RACalcOrientationBetweenControlPoints and the shape functions read, an arena for the records
// whose pointer fields are Ptr32 (shapes, texture coordinate animations, nodes), and one extern "C"
// entry point per function that takes plain pointers and numbers for the test to call.
#include <cstdint>
#include <cstring>
#include <vector>

#include "TPEngine.h"
#include "TPGlobals.h"
#include "fake_strings.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"

// ---- globals of other files that the unit owns here
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;

namespace {

// The arena blocks of the case being built, freed by the next tpw_rai_reset.
std::vector<void*> gBlocks;

float FromBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

}  // namespace

extern "C" {

// ---- the arena (as tpw_rat_* in unit_RideAnimTick.cpp)
uint64_t tpw_rai_base() { return reinterpret_cast<uint64_t>(tpw::ArenaBase()); }

void tpw_rai_reset() {
    for (void* block : gBlocks) tpw::ArenaFree(block);
    gBlocks.clear();
}

// A zeroed arena block that lives until the next tpw_rai_reset.
uint64_t tpw_rai_alloc(uint32_t bytes) {
    void* block = tpw::ArenaAllocZeroed(bytes);
    gBlocks.push_back(block);
    return reinterpret_cast<uint64_t>(block);
}

// ---- TPGlobals
// The sine table's size, mask and scale (as bits), the engine flags (0x400c) and the words 0x401c and
// 0x4020.
void tpw_rai_set_globals(uint32_t size, uint32_t mask, uint32_t scaleBits, uint32_t engineFlags, uint32_t f401c,
                         uint32_t f4020) {
    gTPGlobals.mSinTableSize = size;
    gTPGlobals.mSinTableMask = mask;
    gTPGlobals.mSinTableScale = FromBits(scaleBits);
    gTPGlobals.mEngineFlags = engineFlags;
    gTPGlobals.mF401C = f401c;
    gTPGlobals.mF4020 = f4020;
}

// The first `count` entries of the sine table, as bits.
void tpw_rai_set_table(const uint32_t* bits, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) gTPGlobals.mSinTable[i] = FromBits(bits[i]);
}

// What a case wrote, for the test to see that nothing changed it: the six words of
// tpw_rai_set_globals and the sum of the table's first `count` entries' bits.
void tpw_rai_get_globals(uint32_t count, uint32_t* out) {
    out[0] = gTPGlobals.mSinTableSize;
    out[1] = gTPGlobals.mSinTableMask;
    std::memcpy(&out[2], &gTPGlobals.mSinTableScale, sizeof(float));
    out[3] = gTPGlobals.mEngineFlags;
    out[4] = gTPGlobals.mF401C;
    out[5] = gTPGlobals.mF4020;
    uint32_t sum = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t bits;
        std::memcpy(&bits, &gTPGlobals.mSinTable[i], sizeof bits);
        sum += bits;
    }
    out[6] = sum;
}

// ---- the functions
void tpw_rai_posn(float* values, uint32_t count, float frame, float* out, int32_t wrap) {
    RAInterpPosnAtFrame(values, count, frame, out, wrap);
}

// kind: 0 RABezierInterFromShape, 1 RABezierTangentFromShape, 2 RALinearInterFromShape,
// 3 RALinearTangentFromShape, 4 RACatmulInterFromShape.
void tpw_rai_shape(int32_t kind, tag_sShape* shape, s3DPoint* points, uint32_t index, float fraction, s3DPoint* out) {
    switch (kind) {
    case 0: RABezierInterFromShape(shape, points, index, fraction, out); break;
    case 1: RABezierTangentFromShape(shape, points, index, fraction, out); break;
    case 2: RALinearInterFromShape(shape, points, index, fraction, out); break;
    case 3: RALinearTangentFromShape(shape, points, index, fraction, out); break;
    case 4: RACatmulInterFromShape(shape, points, index, fraction, out); break;
    }
}

void tpw_rai_orientation(sMatrix* matrix, s3DPoint* x, s3DPoint* y, s3DPoint* z) {
    SetLMatrixOrientation(matrix, x, y, z);
}

void tpw_rai_scale(sMatrix* matrix, float* x, float* y, float* z) { SetLMatrixScaleNoTrans(matrix, *x, *y, *z); }

void tpw_rai_xz(sMatrix* matrix, s3DPoint* direction) { SetLMatrixXZOrientation(matrix, direction); }

void tpw_rai_combine(s3DAxis* source, sMatrix* destination) { CombineMatricesNoWDest2nd(source, destination); }

void tpw_rai_quaternion_axis(sQuaternion* quaternion, s3DAxis* out) { RAQuaternionToLAxis(quaternion, out); }

void tpw_rai_slerp(sQuaternion* a, sQuaternion* b, float t, sQuaternion* out) {
    RASphericalLinearInterpolate(a, b, t, out);
}

void tpw_rai_calc_orientation(sQuatKey* keys, uint32_t index, float fraction, s3DAxis* out) {
    RACalcOrientationBetweenControlPoints(keys, index, fraction, out);
}

// The control and the frame list are references to pointers in the original's signature; the
// PowerPC side passes the addresses of cells holding them, this side the pointers themselves.
void tpw_rai_uv_frame(sAnimTimeControl* control, uint16_t* frames, uint32_t count, uint32_t* index, float* fraction) {
    RAGetUVFrameData(control, frames, count, *index, *fraction);
}

void tpw_rai_texture_coords(sAnimTimeControl* control, tag_sNodeAnim* nodeAnim, int32_t additive) {
    RAAnimateTextureCoords(control, nodeAnim, additive);
}

}  // extern "C"
