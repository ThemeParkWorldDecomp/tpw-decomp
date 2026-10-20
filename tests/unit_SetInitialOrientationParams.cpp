// Test library for the small coaster, heightfield and mesh helpers (unit v51074; libtpw_unit_SetInitialOrientationParams.dylib,
// tests/test_SetInitialOrientationParams.py), main app:
//   src/DestroyCoasterPylons_SetupBraceBezierCtrlParams_SetInitialOrientationParams.cpp
//
// The callees outside the unit are fakes that record what the real one reads (BezierPosition, CTRACE, FlagGenerateTrack,
// FindCoasterInstanceFromEntranceID, DisableCurrentPylonEditState, SetPylonType, DeleteLastTrackBrace, RecordAction,
// RestoreHFieldDataFromCopy, ScaleLMatrixNoTrans); the globals of other files are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ActionRecorder.h"
#include "Debug.h"
#include "Light.h"
#include "LLCellSystem.h"
#include "SaveLoad.h"
#include "keytable_actions.h"
#include "m3d2.h"
#include "rsse.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
tag_sCoasterInstance* gCoasterEditInstance;
uint32_t gCoasterWord7310;
uint32_t gCoasterEditEnabled;
uint32_t gCoasterEditDown;
tag_sTrackBrace* gCoasterDragBrace;
uint32_t gCoasterInputFlags;
tag_sM3D2Header* gHeightfieldHeader;
sHField* gHeightfieldCopy;
tag_sRideAnimSet* gRideAnimSets[16];
CActionRecorder gActionRecorder;

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

// ---- the fakes
void BezierPosition(tag_sBezierControl* a, tag_sBezierControl* b, float t, s3DPoint* point) {
    ev("BezierPosition", {lab(a), lab(b), CanonBits(t), lab(point)});
}

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    double d1 = va_arg(args, double);
    double d2 = va_arg(args, double);
    va_end(args);
    uint64_t b1, b2;
    std::memcpy(&b1, &d1, 8);
    std::memcpy(&b2, &d2, 8);
    if ((b1 & 0x7ff0000000000000ull) == 0x7ff0000000000000ull && (b1 & 0xfffffffffffffull) != 0) b1 = 0x7ff8000000000000ull;
    if ((b2 & 0x7ff0000000000000ull) == 0x7ff0000000000000ull && (b2 & 0xfffffffffffffull) != 0) b2 = 0x7ff8000000000000ull;
    uint32_t sum = 0;
    for (const char* c = format; *c != 0; ++c) sum = sum * 31 + static_cast<uint8_t>(*c);
    ev("CTRACE", {static_cast<int64_t>(level), static_cast<int64_t>(sum), static_cast<int64_t>(b1 >> 32), static_cast<int64_t>(b1 & 0xffffffffu),
                  static_cast<int64_t>(b2 >> 32), static_cast<int64_t>(b2 & 0xffffffffu)});
}

void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) { ev("FlagGenerateTrack", {lab(instance), flag}); }

tag_sCoasterInstance* FindCoasterInstanceFromEntranceID(uint32_t entrance) {
    ev("FindCoasterInstanceFromEntranceID", {static_cast<int64_t>(entrance)});
    return reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(ret("FindCoasterInstanceFromEntranceID")));
}

void DisableCurrentPylonEditState() { ev("DisableCurrentPylonEditState", {}); }

void SetPylonType(uint32_t type) { ev("SetPylonType", {static_cast<int64_t>(type)}); }

void DeleteLastTrackBrace() { ev("DeleteLastTrackBrace", {}); }

void CActionRecorder::RecordAction(RecordedAction action, uint16_t a, uint16_t b) {
    ev("RecordAction", {lab(this), static_cast<int64_t>(action), static_cast<int64_t>(a), static_cast<int64_t>(b)});
}

void RestoreHFieldDataFromCopy(tag_sM3D2Header* header, sHField* copy) { ev("RestoreHFieldDataFromCopy", {lab(header), lab(copy)}); }

void ScaleLMatrixNoTrans(sMatrix* matrix, float x, float y, float z) {
    ev("ScaleLMatrixNoTrans", {lab(matrix), CanonBits(x), CanonBits(y), CanonBits(z)});
}

extern "C" {

uint64_t tpw_sip_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gCoasterEditInstance", &gCoasterEditInstance},
        {"gCoasterWord7310", &gCoasterWord7310},
        {"gCoasterEditEnabled", &gCoasterEditEnabled},
        {"gCoasterEditDown", &gCoasterEditDown},
        {"gCoasterDragBrace", &gCoasterDragBrace},
        {"gCoasterInputFlags", &gCoasterInputFlags},
        {"gHeightfieldHeader", &gHeightfieldHeader},
        {"gHeightfieldCopy", &gHeightfieldCopy},
        {"gRideAnimSets", &gRideAnimSets[0]},
        {"gActionRecorder", &gActionRecorder},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sip_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

uint64_t tpw_sip_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host's offsets and sizes of the structures the Mac lays out differently (native pointers); the Python keeps the Mac's.
int32_t tpw_sip_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mF90));                  // 0
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mPrev));                 // 1
    out[n++] = static_cast<int64_t>(offsetof(sTrackPosnControl, mBrace));              // 2
    out[n++] = static_cast<int64_t>(offsetof(sBraceSpline, mControlsA));               // 3
    out[n++] = static_cast<int64_t>(sizeof(tag_sBezierControl));                       // 4
    out[n++] = static_cast<int64_t>(offsetof(sCoasterTrackParams, mLastBrace));        // 5
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));               // 6
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHoardingGeometry));    // 7
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHoardingSpeed));       // 8
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));               // 9
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));                         // 10
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF28));                        // 11
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));                     // 12
    out[n++] = static_cast<int64_t>(offsetof(sEmbeddedSet, mRecords));                 // 13
    out[n++] = static_cast<int64_t>(sizeof(sEmbeddedRecord));                          // 14
    out[n++] = static_cast<int64_t>(offsetof(sEmbeddedRecord, mF08));                  // 15
    out[n++] = static_cast<int64_t>(sizeof(void*));                                    // 16
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mType));            // 17
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBlock3C));         // 18
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mObjectInstanceID));  // 19
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mHField));               // 20
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));                  // 21
    out[n++] = static_cast<int64_t>(offsetof(sHField, mF18));                          // 22
    out[n++] = static_cast<int64_t>(offsetof(tag_sMiniNode, mMatrix));                 // 23
    return n;
}

void tpw_sip_SetInitialOrientationParams(uint64_t o, uint64_t p, int32_t a) {
    SetInitialOrientationParams(reinterpret_cast<sCoasterOrientation*>(o), reinterpret_cast<sCoasterTrackParams*>(p), a);
}
void tpw_sip_SetInitialCoasterInterpParams(uint64_t p) { SetInitialCoasterInterpParams(reinterpret_cast<sCoasterTrackParams*>(p)); }
void tpw_sip_RecalcTrackParams(uint64_t i) { RecalcTrackParams(reinterpret_cast<tag_sCoasterInstance*>(i)); }
void tpw_sip_GetWrappedTrackPosition(uint64_t c, uint64_t b, float t, uint64_t p) {
    GetWrappedTrackPosition(reinterpret_cast<sTrackPosnControl*>(c), reinterpret_cast<tag_sTrackBrace*>(b), t, reinterpret_cast<s3DPoint*>(p));
}
uint64_t tpw_sip_GetCurSelectedTrackBrace() { return reinterpret_cast<uintptr_t>(GetCurSelectedTrackBrace()); }
void tpw_sip_SetCurSelectedTrackBrace(uint64_t b) { SetCurSelectedTrackBrace(reinterpret_cast<tag_sTrackBrace*>(b)); }
int32_t tpw_sip_IncCoasterConst2() { return IncCoasterConst2(); }
int32_t tpw_sip_IsTrackClosed(uint64_t i) { return IsTrackClosed(reinterpret_cast<tag_sCoasterInstance*>(i)); }
uint64_t tpw_sip_GetCoasterInstanceFromCObject(uint64_t o) { return reinterpret_cast<uintptr_t>(GetCoasterInstanceFromCObject(reinterpret_cast<void*>(o))); }
uint64_t tpw_sip_GetTrackBrace(uint64_t p, uint32_t id) { return reinterpret_cast<uintptr_t>(GetTrackBrace(reinterpret_cast<sCoasterTrackParams*>(p), id)); }
void tpw_sip_ClearCoasterButtons() { ClearCoasterButtons(); }
int32_t tpw_sip_OutputCoasterValues() { return OutputCoasterValues(); }
int32_t tpw_sip_IsCoasterLoftMode() { return IsCoasterLoftMode(); }
void tpw_sip_SetPlaceNormalPylon() { SetPlaceNormalPylon(); }
void tpw_sip_SetPlaceSpecialPylon() { SetPlaceSpecialPylon(); }
void tpw_sip_LayoutDeleteLastTrackBrace() { LayoutDeleteLastTrackBrace(); }
void tpw_sip_LayoutRecordCoasterSpeed(uint64_t i, uint32_t s) { LayoutRecordCoasterSpeed(reinterpret_cast<tag_sCoasterInstance*>(i), s); }
void tpw_sip_GetHFieldMaxVertexIndices(uint64_t w, uint64_t h) {
    GetHFieldMaxVertexIndices(reinterpret_cast<uint32_t*>(w), reinterpret_cast<uint32_t*>(h));
}
void tpw_sip_DeActivateHoardings(uint64_t s) { DeActivateHoardings(reinterpret_cast<tag_sRideAnimSet*>(s)); }
void tpw_sip_RestoreLevelHeightfield() { RestoreLevelHeightfield(); }
uint64_t tpw_sip_GetLightsourceHandle(uint32_t r, uint32_t i) { return reinterpret_cast<uintptr_t>(GetLightsourceHandle(r, i)); }
void tpw_sip_SetMeshPosition(uint64_t m, float x, float y, float z) { SetMeshPosition(reinterpret_cast<tag_sMesh*>(m), x, y, z); }
void tpw_sip_ScaleMesh(uint64_t m, uint64_t p) { ScaleMesh(reinterpret_cast<tag_sMesh*>(m), reinterpret_cast<s3DPoint*>(p)); }

}  // extern "C"
