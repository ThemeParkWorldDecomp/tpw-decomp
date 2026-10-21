// Test library for CalculatePointerWorldPosn (libtpw_unit_PointerPick.dylib; see tests/test_PointerPick.py): src/game/TPPointer.cpp, real.
//
// The four callees are fakes that record through calllog: FindPointedRide (two events, the places and then the ray and the step; answers
// from a queue of ride labels and fills gPointedRidePosition from the RidePos queue when it answers a ride), SetPotentiallyHighlightedRide,
// VectorThroughLMatrixNoWNoTrans (replaces the point with the next three words of the Vec queue) and DistanceBetweenPoints (answers the
// Dist queue). The globals the function reads are defined here; the test sets them through tpw_pp_* and reads them back.
#include <cstdint>
#include <cstring>

#include "TPCamera.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "UI_Base.h"
#include "calllog.h"
#include "platform/arena.h"

using calllog::ev;
using calllog::lab;
using calllog::ret;

// gTPGlobals as raw storage under the symbol's name (nothing constructs it; the unit reads only the sine table words).
#ifdef __APPLE__
extern unsigned char gGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");
#else
extern unsigned char gGlobalsStorage[sizeof(TPGlobals)] __asm__("gTPGlobals");
#endif
alignas(16) unsigned char gGlobalsStorage[sizeof(TPGlobals)] = {};
sMatrix gCameraMatrix;
s3DPoint gCameraViewPoint;
float gPointerScreenXY[2];
float gCameraFov;
float gViewCentreX;
float gViewCentreY;
tag_sM3D2Header* gHeightfieldHeader;
tag_sRideAnimSet* gPointedRide;
s3DPoint gPointedRidePosition;

namespace {
alignas(8) char gRides[2][16];
tag_sRideAnimSet* Ride(int64_t index) { return index == 0 ? nullptr : reinterpret_cast<tag_sRideAnimSet*>(gRides[index - 1]); }
uint32_t Bits(float v) { return calllog::fbits(v); }
float FromBits(uint32_t w) {
    float f;
    std::memcpy(&f, &w, 4);
    return f;
}
}  // namespace

tag_sRideAnimSet* FindPointedRide(uint32_t cellX, uint32_t cellZ, int32_t axis, float x, float y, float z, float yStep, s3DPoint* direction,
                                  s3DPoint* camera, float step) {
    ev("FindPointedRide", {cellX, cellZ, axis, Bits(x), Bits(y), Bits(z), Bits(yStep)});
    ev("FindPointedRide2", {Bits(direction->x), Bits(direction->y), Bits(direction->z), lab(camera), Bits(step)});
    const int64_t answer = ret("FindPointedRide");
    if (answer != 0) {
        gPointedRidePosition.x = FromBits(static_cast<uint32_t>(ret("RidePos")));
        gPointedRidePosition.y = FromBits(static_cast<uint32_t>(ret("RidePos")));
        gPointedRidePosition.z = FromBits(static_cast<uint32_t>(ret("RidePos")));
    }
    return Ride(answer);
}
void SetPotentiallyHighlightedRide(tag_sRideAnimSet* ride) { ev("SetPotentiallyHighlightedRide", {lab(ride)}); }
void VectorThroughLMatrixNoWNoTrans(sMatrix* matrix, s3DPoint* point) {
    ev("VectorThroughLMatrixNoWNoTrans", {lab(matrix), Bits(point->x), Bits(point->y), Bits(point->z)});
    point->x = FromBits(static_cast<uint32_t>(ret("Vec")));
    point->y = FromBits(static_cast<uint32_t>(ret("Vec")));
    point->z = FromBits(static_cast<uint32_t>(ret("Vec")));
}
float DistanceBetweenPoints(const s3DPoint* a, const s3DPoint* b) {
    ev("DistanceBetweenPoints", {Bits(a->x), Bits(a->y), Bits(a->z), Bits(b->x), Bits(b->y), Bits(b->z)});
    return FromBits(static_cast<uint32_t>(ret("Dist")));
}

// The labels of the objects the fakes name: rides 1 and 2, the camera point 3, the matrix 4.
extern "C" void tpw_pp_labels() {
    tpw_cl_label(1, gRides[0]);
    tpw_cl_label(2, gRides[1]);
    tpw_cl_label(3, &gCameraViewPoint);
    tpw_cl_label(4, &gCameraMatrix);
}
// The unit space's width (the port's widescreen); 0 when the library is built without the port fixes.
extern "C" int32_t tpw_pp_unitwidth(int32_t width) {
    (void)width;
    return 0;
}
extern "C" void tpw_pp_table(const uint32_t* words, int32_t n) {
    for (int32_t i = 0; i < n; ++i) reinterpret_cast<TPGlobals*>(gGlobalsStorage)->mSinTable[i] = FromBits(words[i]);
}
// mode 0: no header; 1: a header with `flags` and no field; 2: a header and a field. f: origin x, origin z, size x, size z, lowest, highest
// (float bits); the field is width by depth quads; heights (width + 1) * (depth + 1) float words; quads width * depth flag words.
extern "C" void tpw_pp_field(int32_t mode, uint32_t flags, const uint32_t* f, uint32_t width, uint32_t depth, const uint32_t* heights,
                             const uint32_t* quads) {
    if (gHeightfieldHeader != nullptr) {
        if (gHeightfieldHeader->mHField != nullptr) {
            sHField* const old = gHeightfieldHeader->mHField;
            if (old->mQuads != nullptr) tpw::ArenaFree(old->mQuads.get());
            if (old->mHeights != nullptr) tpw::ArenaFree(old->mHeights.get());
            tpw::ArenaFree(old);
        }
        tpw::ArenaFree(gHeightfieldHeader);
        gHeightfieldHeader = nullptr;
    }
    if (mode == 0) return;
    gHeightfieldHeader = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    gHeightfieldHeader->mFlags = flags;
    if (mode == 1) return;
    sHField* const field = static_cast<sHField*>(tpw::ArenaAllocZeroed(sizeof(sHField)));
    field->mF08[0] = FromBits(f[0]);
    field->mF08[1] = FromBits(f[1]);
    field->mF08[2] = FromBits(f[2]);
    field->mF08[3] = FromBits(f[3]);
    field->mF20[0] = FromBits(f[4]);
    field->mF20[1] = FromBits(f[5]);
    field->mF18[0] = width;
    field->mF18[1] = depth;
    const uint32_t nh = (width + 1) * (depth + 1);
    float* const h = static_cast<float*>(tpw::ArenaAllocZeroed(sizeof(float) * (nh + 1)));
    for (uint32_t i = 0; i < nh; ++i) h[i] = FromBits(heights[i]);
    field->mHeights = h;
    const uint32_t nq = width * depth;
    sHFieldQuad* const q = static_cast<sHFieldQuad*>(tpw::ArenaAllocZeroed(sizeof(sHFieldQuad) * (nq + 1)));
    for (uint32_t i = 0; i < nq; ++i) q[i].mF00 = static_cast<uint16_t>(quads[i]);
    field->mQuads = q;
    gHeightfieldHeader->mHField = field;
}
// g: mask, scale, fov, view centre x, y, pointer x, y (float bits), once byte, factor, camera x y z, ride place x y z, the ride label gPointedRide
// starts with. nullMask: bit 0 position NULL, 1 pickup NULL, 2 result NULL. io: position 0..2, pickup 3..5, result 6 in; out the same, then
// 7 the ride label, 8 the once byte, 9 the factor, 10..12 gPointedRidePosition.
extern "C" void tpw_pp_run(const uint32_t* g, int32_t nullMask, uint32_t* io) {
    reinterpret_cast<TPGlobals*>(gGlobalsStorage)->mSinTableMask = g[0];
    reinterpret_cast<TPGlobals*>(gGlobalsStorage)->mSinTableScale = FromBits(g[1]);
    gCameraFov = FromBits(g[2]);
    gViewCentreX = FromBits(g[3]);
    gViewCentreY = FromBits(g[4]);
    gPointerScreenXY[0] = FromBits(g[5]);
    gPointerScreenXY[1] = FromBits(g[6]);
    gPointerFovOnce = static_cast<uint8_t>(g[7]);
    gPointerFovFactor = FromBits(g[8]);
    gCameraViewPoint.x = FromBits(g[9]);
    gCameraViewPoint.y = FromBits(g[10]);
    gCameraViewPoint.z = FromBits(g[11]);
    gPointedRidePosition.x = FromBits(g[12]);
    gPointedRidePosition.y = FromBits(g[13]);
    gPointedRidePosition.z = FromBits(g[14]);
    gPointedRide = Ride(g[15]);
    s3DPoint position, pickup;
    std::memcpy(&position, io, 12);
    std::memcpy(&pickup, io + 3, 12);
    int32_t result;
    std::memcpy(&result, io + 6, 4);
    CalculatePointerWorldPosn((nullMask & 1) ? nullptr : &position, 0.0f, (nullMask & 2) ? nullptr : &pickup, (nullMask & 4) ? nullptr : &result);
    std::memcpy(io, &position, 12);
    std::memcpy(io + 3, &pickup, 12);
    std::memcpy(io + 6, &result, 4);
    io[7] = static_cast<uint32_t>(lab(gPointedRide));
    io[8] = gPointerFovOnce;
    io[9] = Bits(gPointerFovFactor);
    io[10] = Bits(gPointedRidePosition.x);
    io[11] = Bits(gPointedRidePosition.y);
    io[12] = Bits(gPointedRidePosition.z);
}
