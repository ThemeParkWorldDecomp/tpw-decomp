// Test library for HandleCamera and SetLMatrixOrientationHi (libtpw_unit_Camera.dylib; see tests/test_Camera.py): src/TPCamera_handle.cpp,
// src/rsse_orientation.cpp, src/TPCamera.cpp (the external branch), src/rsse_matrix.cpp and src/rsse_rotate.cpp, all real.
//
// Fakes that record through calllog: the clock (CClock::ReadFixed), the camera key table (CKeyTable::GetState: held when the id's bit is set
// in the case's key mask), DetermineHFieldHeight, GetNextGridsquare, RideRideIfRequired, MatrixDifferenceTiny, ApplyHeadsOnRidesBumpage
// (writes the scripted matrix) and the engine's view matrix (CMatrix::Load). The camera's globals of src/TPCamera_init.cpp are defined here
// (InitCamera is not linked); the test sets them through tpw_cam_run and reads them back.
#include <cstdint>
#include <cstring>

#include "Clock.h"
#include "TPCamera.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "calllog.h"
#include "keyboard.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"
#include "ui_main.h"

using calllog::ev;
using calllog::lab;
using calllog::ret;

// gTPGlobals and gPlayerSettings as raw storage under the symbols' names (nothing constructs them; the unit reads a few words).
#ifdef __APPLE__
extern unsigned char gGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");
extern unsigned char gSettingsStorage[sizeof(CPlayerSettings)] __asm__("_gPlayerSettings");
#else
extern unsigned char gGlobalsStorage[sizeof(TPGlobals)] __asm__("gTPGlobals");
extern unsigned char gSettingsStorage[sizeof(CPlayerSettings)] __asm__("gPlayerSettings");
#endif
alignas(16) unsigned char gGlobalsStorage[sizeof(TPGlobals)] = {};
alignas(16) unsigned char gSettingsStorage[sizeof(CPlayerSettings)] = {};

// The globals of TPCamera_init.cpp and the ones the camera reads.
float gCameraF68;
float gCameraStartPosition[3];
float gCameraPosition[3];
uint32_t gCameraClockStamp;
uint32_t gCameraModeA;
uint32_t gCameraModeB;
uint32_t gCameraF5AAC;
sMatrix gCameraMatrixA;
sMatrix gCameraMatrixB;
sMatrix gCameraMatrix;
float gCameraF5BB4[3];
float gCameraF5BF4;
float gCameraTarget[3];
s3DPoint gCameraViewPoint;
float gPointerScreenXY[2];
uint32_t gTPWFlags;
float gGameWindowCentreX;
float gGameWindowCentreY;
float gGameWindowHalfWidth;
float gGameWindowHalfHeight;
tag_sM3D2Header* gHeightfieldHeader;
alignas(16) static unsigned char gKeyStorage[64];
CKeyTable* gKeyTableCamera = reinterpret_cast<CKeyTable*>(gKeyStorage);
CFixedTime::CFixedTime() {}
CVariableTime::CVariableTime() {}
CClock gClock;
static CMatrix gView;
CMatrix* gei_ViewMatrix = &gView;
static sMatrix gRideMatrix;
static uint32_t gKeyMask;

namespace {
uint32_t Bits(float v) { return calllog::fbits(v); }
float FromBits(uint32_t w) {
    float f;
    std::memcpy(&f, &w, 4);
    return f;
}
}  // namespace

int32_t CClock::ReadFixed() {
    const int32_t value = static_cast<int32_t>(ret("ReadFixed"));
    ev("ReadFixed", {lab(this), value});
    return value;
}
int16_t CKeyTable::GetState(int16_t id) {
    const int16_t value = (id >= 0 && id < 32 && ((gKeyMask >> id) & 1)) ? 1 : 0;
    ev("GetState", {lab(this), id, value});
    return value;
}
float DetermineHFieldHeight(float x, float z, int32_t flag) {
    ev("DetermineHFieldHeight", {Bits(x), Bits(z), flag});
    return FromBits(static_cast<uint32_t>(ret("Height")));
}
int32_t GetNextGridsquare(s3DPoint* from, s3DPoint* step, s3DPoint* out) {
    // `out` is not recorded: the original leaves it unset on the first call (stack leftovers), the translation gives it 0.
    ev("GetNextGridsquare", {Bits(from->x), Bits(from->y), Bits(from->z), Bits(step->x), Bits(step->y), Bits(step->z)});
    out->x = FromBits(static_cast<uint32_t>(ret("GridOut")));
    out->y = FromBits(static_cast<uint32_t>(ret("GridOut")));
    out->z = FromBits(static_cast<uint32_t>(ret("GridOut")));
    return static_cast<int32_t>(ret("Grid"));
}
int32_t RideRideIfRequired(s3DPoint* point) {
    ev("RideRideIfRequired", {Bits(point->x), Bits(point->y), Bits(point->z)});
    return static_cast<int32_t>(ret("RideRide"));
}
int32_t MatrixDifferenceTiny(sMatrix* a, sMatrix* b) {
    ev("MatrixDifferenceTiny", {lab(a), lab(b)});
    return static_cast<int32_t>(ret("Tiny"));
}
void ApplyHeadsOnRidesBumpage(sMatrix* ride, sMatrix* previous, float dt, sMatrix* out) {
    ev("ApplyHeadsOnRidesBumpage", {lab(ride), lab(previous), Bits(dt), lab(out)});
    float* const m = reinterpret_cast<float*>(out);
    for (int32_t i = 0; i < 16; ++i) m[i] = FromBits(static_cast<uint32_t>(ret("Bump")));
}
void CMatrix::Load(sMatrix& source) {
    ev("Load", {lab(&source)});
    std::memcpy(this, &source, sizeof source);
}

extern "C" void tpw_cam_labels() {
    tpw_cl_label(1, &gCameraMatrix);
    tpw_cl_label(2, &gCameraMatrixA);
    tpw_cl_label(3, &gCameraMatrixB);
    tpw_cl_label(4, &gRideMatrix);
    tpw_cl_label(5, gKeyStorage);
    tpw_cl_label(6, &gClock);
    tpw_cl_label(8, &gExternalCameraMatrix);
}
extern "C" void tpw_cam_table(const uint32_t* words, int32_t n) {
    for (int32_t i = 0; i < n; ++i) reinterpret_cast<TPGlobals*>(gGlobalsStorage)->mSinTable[i] = FromBits(words[i]);
}
// A height field whose cells are sx by sz and w by d (heights and quads present as the flags say); mode 0 frees it.
extern "C" void tpw_cam_field(int32_t mode, uint32_t sx, uint32_t sz, uint32_t w, uint32_t d, int32_t heights, int32_t quads) {
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
    field->mF08[2] = FromBits(sx);
    field->mF08[3] = FromBits(sz);
    field->mF18[0] = w;
    field->mF18[1] = d;
    if (heights) field->mHeights = static_cast<float*>(tpw::ArenaAllocZeroed(64));
    if (quads) field->mQuads = static_cast<sHFieldQuad*>(tpw::ArenaAllocZeroed(64));
    gHeightfieldHeader->mHField = field;
}
// g: see test_Camera.py (GLOBALS); the in/out words are listed there as well.
extern "C" void tpw_cam_run(const uint32_t* g, uint32_t* out) {
    int32_t i = 0;
    auto u = [&]() { return g[i++]; };
    auto f = [&]() { return FromBits(g[i++]); };
    auto matrix = [&](sMatrix& m) {
        std::memcpy(&m, g + i, 64);
        i += 16;
    };
    reinterpret_cast<TPGlobals*>(gGlobalsStorage)->mSinTableMask = u();
    reinterpret_cast<TPGlobals*>(gGlobalsStorage)->mSinTableScale = f();
    gCameraClockStamp = u();
    gCameraModeA = u();
    gCameraModeB = u();
    gCameraF5AAC = u();
    gCameraF5BF4 = f();
    gCameraF68 = f();
    for (float& v : gCameraPosition) v = f();
    for (float& v : gCameraF5BB4) v = f();
    for (float& v : gCameraStartPosition) v = f();
    gCameraYaw = f();
    gCameraPitch = f();
    gCameraTilt[0] = f();
    gCameraTilt[1] = f();
    gCameraGroundHeight = f();
    gFirstPersonTurn = f();
    gFirstPersonPitch = f();
    gCameraPointerAnchor[0] = f();
    gCameraPointerAnchor[1] = f();
    gFirstPersonFlags = u();
    reinterpret_cast<CPlayerSettings*>(gSettingsStorage)->mCurrent.mIsometricOn = (u() & 1) != 0;
    reinterpret_cast<CPlayerSettings*>(gSettingsStorage)->mCurrent.mRMBScrollOn = (u() & 1) != 0;
    gTPWFlags = u();
    gPointerScreenXY[0] = f();
    gPointerScreenXY[1] = f();
    gGameWindowCentreX = f();
    gGameWindowCentreY = f();
    gGameWindowHalfWidth = f();
    gGameWindowHalfHeight = f();
    gCameraWideZoomOnce = static_cast<int8_t>(u());
    gCameraWideZoomMin = f();
    gCameraWideZoomMaxOnce = static_cast<int8_t>(u());
    gCameraWideZoomMax = f();
    gCameraWalkOnce = static_cast<int8_t>(u());
    gCameraF59FC = f();
    gKeyMask = u();
    for (float& v : gCameraTarget) v = f();
    gCameraViewPoint.x = f();
    gCameraViewPoint.y = f();
    gCameraViewPoint.z = f();
    const uint32_t rideSet = u();
    gFirstPersonRideMatrix = rideSet ? &gRideMatrix : nullptr;
    matrix(gRideMatrix);
    matrix(gCameraMatrix);
    matrix(gCameraMatrixA);
    matrix(gCameraMatrixB);
    matrix(gExternalCameraMatrix);
    std::memset(&gInverseCameraMatrix, 0xa5, sizeof(sMatrix));
    std::memset(&gView, 0xa5, sizeof(sMatrix));
    gCurrentCameraMatrix = nullptr;
    HandleCamera();
    int32_t o = 0;
    auto put = [&](uint32_t v) { out[o++] = v; };
    auto putf = [&](float v) { out[o++] = Bits(v); };
    auto putm = [&](const void* m) {
        std::memcpy(out + o, m, 64);
        o += 16;
    };
    put(gCameraClockStamp);
    put(gCameraModeA);
    put(gCameraModeB);
    put(gCameraF5AAC);
    putf(gCameraF5BF4);
    putf(gCameraF68);
    for (float v : gCameraPosition) putf(v);
    for (float v : gCameraF5BB4) putf(v);
    putf(gCameraYaw);
    putf(gCameraPitch);
    putf(gCameraTilt[0]);
    putf(gCameraTilt[1]);
    putf(gCameraGroundHeight);
    putf(gFirstPersonTurn);
    putf(gFirstPersonPitch);
    putf(gCameraPointerAnchor[0]);
    putf(gCameraPointerAnchor[1]);
    put(gFirstPersonFlags);
    put(static_cast<uint8_t>(gCameraWideZoomOnce));
    putf(gCameraWideZoomMin);
    put(static_cast<uint8_t>(gCameraWideZoomMaxOnce));
    putf(gCameraWideZoomMax);
    put(static_cast<uint8_t>(gCameraWalkOnce));
    putf(gCameraF59FC);
    for (float v : gCameraTarget) putf(v);
    putf(gCameraViewPoint.x);
    putf(gCameraViewPoint.y);
    putf(gCameraViewPoint.z);
    put(gCurrentCameraMatrix == &gCameraMatrix ? 1 : gCurrentCameraMatrix == &gExternalCameraMatrix ? 8 : gCurrentCameraMatrix ? 9 : 0);
    putm(&gCameraMatrix);
    putm(&gCameraMatrixA);
    putm(&gCameraMatrixB);
    putm(&gInverseCameraMatrix);
    putm(&gView);
    putm(&gRideMatrix);
}
// SetLMatrixOrientationHi on a matrix of 16 words.
extern "C" void tpw_cam_orient(const uint32_t* matrix, uint32_t pitch, uint32_t yaw, uint32_t* out) {
    sMatrix m;
    std::memcpy(&m, matrix, 64);
    SetLMatrixOrientationHi(&m, FromBits(pitch), FromBits(yaw));
    std::memcpy(out, &m, 64);
}
