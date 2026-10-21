// Test library for the camera's small callees and the two height lookups (libtpw_unit_CameraCallees.dylib; see
// tests/test_CameraCallees.py): src/game/TPCamera.cpp and src/game/park/TPHField.cpp, real.
//
// Fakes that record through calllog: MAP_wall_in_way (answers the "Wall" queue), GetThingAtPosnIfEntrance (answers the case's object or
// null), CObject::GoFirstPerson and CRideDatabase::GetRideInfo (answers null). The camera globals the callees touch are defined here.
#include <cstdint>
#include <cstring>

#include "TPCamera.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "calllog.h"
#include "keytable_actions.h"
#include "m3d2.h"
#include "platform/arena.h"

using calllog::ev;
using calllog::lab;
using calllog::ret;

#ifdef __APPLE__
extern unsigned char gRideDatabaseStorage[sizeof(CRideDatabase)] __asm__("_gRideDatabase");
#else
extern unsigned char gRideDatabaseStorage[sizeof(CRideDatabase)] __asm__("gRideDatabase");
#endif
alignas(16) unsigned char gRideDatabaseStorage[sizeof(CRideDatabase)] = {};

float gCameraF68;
float gCameraPosition[3];
uint32_t gCameraModeA;
float gCameraF5BB4[3];
float gCameraF5BF4;
tag_sM3D2Header* gHeightfieldHeader;
alignas(16) static unsigned char gObjectStorage[sizeof(CObject)];
static bool gObjectPresent;

namespace {
uint32_t Bits(float v) { return calllog::fbits(v); }
float FromBits(uint32_t w) {
    float f;
    std::memcpy(&f, &w, 4);
    return f;
}
}  // namespace

int32_t MAP_wall_in_way(uint8_t x, uint8_t z, uint8_t dir, NavMode mode) {
    ev("MAP_wall_in_way", {x, z, dir, static_cast<int32_t>(mode)});
    return static_cast<int32_t>(ret("Wall"));
}
CObject* GetThingAtPosnIfEntrance(float x, float z) {
    ev("GetThingAtPosnIfEntrance", {Bits(x), Bits(z)});
    return gObjectPresent ? reinterpret_cast<CObject*>(gObjectStorage) : nullptr;
}
void CObject::GoFirstPerson(int32_t on) const { ev("GoFirstPerson", {lab(this), on}); }
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return nullptr;
}

extern "C" void tpw_cc_labels() {
    tpw_cl_label(1, gRideDatabaseStorage);
    tpw_cl_label(2, gObjectStorage);
}
// A height field: p = ox, oz, sx, sz (bits), w, d; heights and quads are copied (slack is added after them).
extern "C" void tpw_cc_field(int32_t mode, const uint32_t* p, const uint32_t* heights, int32_t nh, const uint32_t* quads, int32_t nq) {
    if (gHeightfieldHeader != nullptr) {
        sHField* const old = gHeightfieldHeader->mHField;
        if (old != nullptr) {
            if (old->mQuads != nullptr) tpw::ArenaFree(old->mQuads.get());
            if (old->mHeights != nullptr) tpw::ArenaFree(old->mHeights.get());
            tpw::ArenaFree(old);
        }
        tpw::ArenaFree(gHeightfieldHeader);
        gHeightfieldHeader = nullptr;
    }
    if (mode == 0) return;
    gHeightfieldHeader = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    sHField* const field = static_cast<sHField*>(tpw::ArenaAllocZeroed(sizeof(sHField)));
    for (int32_t i = 0; i < 4; ++i) field->mF08[i] = FromBits(p[i]);
    field->mF18[0] = p[4];
    field->mF18[1] = p[5];
    float* const h = static_cast<float*>(tpw::ArenaAllocZeroed(4 * nh + 512));
    std::memcpy(h, heights, 4 * nh);
    field->mHeights = h;
    sHFieldQuad* const q = static_cast<sHFieldQuad*>(tpw::ArenaAllocZeroed(4 * nq + 512));
    std::memcpy(q, quads, 4 * nq);
    field->mQuads = q;
    gHeightfieldHeader->mHField = field;
}
extern "C" void tpw_cc_height(const uint32_t* a, int32_t quick, uint32_t* out) {
    const float r = quick ? DetermineHFieldHeightQuick(FromBits(a[0]), FromBits(a[1]))
                          : DetermineHFieldHeight(FromBits(a[0]), FromBits(a[1]), static_cast<int32_t>(a[2]));
    out[0] = Bits(r);
}
// a: from (3), step (3); out: answer, point (3, prefilled 0xa5a5a5a5).
extern "C" void tpw_cc_grid(const uint32_t* a, uint32_t* out) {
    s3DPoint from, step, to;
    std::memcpy(&from, a, 12);
    std::memcpy(&step, a + 3, 12);
    std::memset(&to, 0xa5, 12);
    out[0] = static_cast<uint32_t>(GetNextGridsquare(&from, &step, &to));
    std::memcpy(out + 1, &to, 12);
}
extern "C" void tpw_cc_tiny(const uint32_t* a, uint32_t* out) {
    sMatrix x, y;
    std::memcpy(&x, a, 64);
    std::memcpy(&y, a + 16, 64);
    tpw_cl_label(3, &x);
    tpw_cl_label(4, &y);
    out[0] = static_cast<uint32_t>(MatrixDifferenceTiny(&x, &y));
}
// a: ride (16), previous (16), dt; out: 16 words (prefilled 0xa5a5a5a5).
extern "C" void tpw_cc_bump(const uint32_t* a, uint32_t* out) {
    sMatrix ride, prev, to;
    std::memcpy(&ride, a, 64);
    std::memcpy(&prev, a + 16, 64);
    std::memset(&to, 0xa5, 64);
    ApplyHeadsOnRidesBumpage(&ride, &prev, FromBits(a[32]), &to);
    std::memcpy(out, &to, 64);
}
extern "C" void tpw_cc_round(uint32_t yaw, uint32_t* out) {
    gCameraF5BF4 = FromBits(yaw);
    RoundRotationToNearest90Degrees();
    out[0] = Bits(gCameraF5BF4);
}
// a: mask of the present inputs (bit 0 x, 1 z, 2 zoom, 3 yaw, 4 value), the five input floats, then the start state:
// position (3), zoom, yaw, velocity (3), the POI value. out: the same state afterwards.
extern "C" void tpw_cc_poi(const uint32_t* a, uint32_t* out) {
    float in[5];
    for (int32_t i = 0; i < 5; ++i) in[i] = FromBits(a[1 + i]);
    for (int32_t i = 0; i < 3; ++i) gCameraPosition[i] = FromBits(a[6 + i]);
    gCameraF68 = FromBits(a[9]);
    gCameraF5BF4 = FromBits(a[10]);
    for (int32_t i = 0; i < 3; ++i) gCameraF5BB4[i] = FromBits(a[11 + i]);
    gCameraPOIValue = FromBits(a[14]);
    SetCameraPOI((a[0] & 1) ? &in[0] : nullptr, (a[0] & 2) ? &in[1] : nullptr, (a[0] & 4) ? &in[2] : nullptr, (a[0] & 8) ? &in[3] : nullptr,
                 (a[0] & 16) ? &in[4] : nullptr);
    for (int32_t i = 0; i < 3; ++i) out[i] = Bits(gCameraPosition[i]);
    out[3] = Bits(gCameraF68);
    out[4] = Bits(gCameraF5BF4);
    for (int32_t i = 0; i < 3; ++i) out[5 + i] = Bits(gCameraF5BB4[i]);
    out[8] = Bits(gCameraPOIValue);
}
extern "C" void tpw_cc_zoom(uint32_t mode, uint32_t* out) {
    gCameraModeA = mode;
    out[0] = static_cast<uint32_t>(AllowFullCameraZoomout());
    out[1] = gCameraModeA;
}
// a: x, z, y (bits), present, object type.
extern "C" void tpw_cc_ride(const uint32_t* a, uint32_t* out) {
    s3DPoint p;
    p.x = FromBits(a[0]);
    p.y = FromBits(a[2]);
    p.z = FromBits(a[1]);
    gObjectPresent = a[3] != 0;
    std::memset(gObjectStorage, 0, sizeof gObjectStorage);
    reinterpret_cast<CObject*>(gObjectStorage)->mObjectType = static_cast<uint16_t>(a[4]);
    out[0] = static_cast<uint32_t>(RideRideIfRequired(&p));
}
