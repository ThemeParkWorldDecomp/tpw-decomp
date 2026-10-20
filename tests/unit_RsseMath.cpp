// Test library for rsse.cpp's matrix and vector helpers and FloatSortByArray (unit w170; libtpw_unit_RsseMath.dylib,
// tests/test_RsseMath.py), main app:
//   src/rsse_math.cpp   sixteen functions of 0x100c0eac to 0x100c3198
//
// The unit calls nothing outside the C library (atan2, sqrt are the host's libm), so there are no fakes and no globals. Every wrapper takes
// matrices as 16 words and points as 3 words (the bits of the floats), copies them in, calls the function and copies them back, so a
// wrapper never sees a pointer the test did not give it; floats come as bits too, so a NaN's payload is not changed on the way in.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "m3d2.h"
#include "TPEngine.h"
#include "rsse.h"
#include "tb_matrix.h"

namespace {

float F(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

uint32_t B(float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof bits);
    return bits;
}

}  // namespace

extern "C" {

// The offsets and sizes the module relies on (HOST_LAYOUT): the matrix is 16 floats, the point 3.
int32_t tpw_rm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(sMatrix));
    out[n++] = static_cast<int64_t>(offsetof(sMatrix, m[3][3]));
    out[n++] = static_cast<int64_t>(sizeof(s3DPoint));
    out[n++] = static_cast<int64_t>(offsetof(s3DPoint, z));
    return n;
}

void tpw_rm_SetLMatrixIdentityNoTrans(uint32_t* m) {
    sMatrix M;
    std::memcpy(&M, m, sizeof M);
    SetLMatrixIdentityNoTrans(&M);
    std::memcpy(m, &M, sizeof M);
}

uint32_t tpw_rm_GetLMatrixRotationAroundYAxis(uint32_t* m) {
    sMatrix M;
    std::memcpy(&M, m, sizeof M);
    return B(GetLMatrixRotationAroundYAxis(&M));
}

// The reference and pointer forms of the three angles: `ref` selects the s3DPoint& overload; `which` is 0 (x from z), 1 (y from z), 2 (z from y).
uint32_t tpw_rm_GetVectorRotation(int32_t which, int32_t ref, uint32_t* p) {
    s3DPoint P;
    std::memcpy(&P, p, sizeof P);
    float r = 0.0f;
    if (ref) {
        r = which == 0 ? GetVectorRotationAroundXAxisFromZ(P) : which == 1 ? GetVectorRotationAroundYAxisFromZ(P) : GetVectorRotationAroundZAxisFromY(P);
    } else {
        r = which == 0 ? GetVectorRotationAroundXAxisFromZ(&P) : which == 1 ? GetVectorRotationAroundYAxisFromZ(&P) : GetVectorRotationAroundZAxisFromY(&P);
    }
    return B(r);
}

// which: 0 XRotateLMatrix, 1 YRotateLMatrix, 2 ZRotateLMatrix.
void tpw_rm_RotateLMatrix(int32_t which, uint32_t* m, uint32_t c, uint32_t s) {
    sMatrix M;
    std::memcpy(&M, m, sizeof M);
    if (which == 0) XRotateLMatrix(&M, F(c), F(s));
    if (which == 1) YRotateLMatrix(&M, F(c), F(s));
    if (which == 2) ZRotateLMatrix(&M, F(c), F(s));
    std::memcpy(m, &M, sizeof M);
}

void tpw_rm_ScaleLMatrixNoTrans(uint32_t* m, uint32_t x, uint32_t y, uint32_t z) {
    sMatrix M;
    std::memcpy(&M, m, sizeof M);
    ScaleLMatrixNoTrans(&M, F(x), F(y), F(z));
    std::memcpy(m, &M, sizeof M);
}

void tpw_rm_VectorThroughLMatrixNoWNoTrans(uint32_t* m, uint32_t* p) {
    sMatrix M;
    s3DPoint P;
    std::memcpy(&M, m, sizeof M);
    std::memcpy(&P, p, sizeof P);
    VectorThroughLMatrixNoWNoTrans(&M, &P);
    std::memcpy(p, &P, sizeof P);
}

// same != 0: the one matrix `m` is both arguments (the second is then `m` itself, as the original's caller could pass it).
void tpw_rm_CombineMatricesNoWNoTransDest2nd(uint32_t* m, uint32_t* m2, int32_t same) {
    sMatrix M, M2;
    std::memcpy(&M, m, sizeof M);
    std::memcpy(&M2, m2, sizeof M2);
    if (same) {
        CombineMatricesNoWNoTransDest2nd(&M, &M);
    } else {
        CombineMatricesNoWNoTransDest2nd(&M, &M2);
    }
    std::memcpy(m, &M, sizeof M);
    std::memcpy(m2, &M2, sizeof M2);
}

uint32_t tpw_rm_DistanceBetweenPoints(uint32_t* p, uint32_t* q) {
    s3DPoint P, Q;
    std::memcpy(&P, p, sizeof P);
    std::memcpy(&Q, q, sizeof Q);
    return B(DistanceBetweenPoints(&P, &Q));
}

// The arrays are native pointers (64 bits): the test passes arrays of host addresses.
void tpw_rm_FloatSortByArray(int32_t count, uint64_t* source, uint64_t* scratch, int32_t depth, int32_t offset) {
    FloatSortByArray(count, reinterpret_cast<void**>(source), reinterpret_cast<void**>(scratch), depth, offset);
}

}  // extern "C"
