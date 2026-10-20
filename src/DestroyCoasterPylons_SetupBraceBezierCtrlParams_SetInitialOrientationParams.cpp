#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>
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

// Unit v51074: small coaster, heightfield and mesh helpers (names of fields invented where the headers had none).

// 0x1005092c: the orientation block starts with zero angles and a maximal word at +0x1c (past the struct: the instance's next word).
void SetInitialOrientationParams(sCoasterOrientation* orient, sCoasterTrackParams* params, int32_t a) {
    (void)params;
    (void)a;
    uint8_t* p = reinterpret_cast<uint8_t*>(orient);
    const float zero = 0.0f;
    const float big = std::numeric_limits<float>::max();
    std::memset(p, 0, 4);
    std::memcpy(p + 0x18, &zero, 4);
    std::memcpy(p + 0x14, &zero, 4);
    std::memcpy(p + 0x10, &zero, 4);
    std::memcpy(p + 0x0c, &zero, 4);
    std::memcpy(p + 0x08, &zero, 4);
    std::memcpy(p + 0x04, &zero, 4);
    std::memcpy(p + 0x1c, &big, 4);
}

// 0x100509bc
void SetInitialCoasterInterpParams(sCoasterTrackParams* params) {
    uint8_t* p = reinterpret_cast<uint8_t*>(params);
    const float zero = 0.0f;
    const float big = std::numeric_limits<float>::max();
    std::memcpy(p + 0x38, &zero, 4);
    std::memset(p + 0x3c, 0, 4);
    std::memcpy(p + 0x54, &zero, 4);
    std::memcpy(p + 0x50, &zero, 4);
    std::memcpy(p + 0x4c, &zero, 4);
    std::memcpy(p + 0x48, &zero, 4);
    std::memcpy(p + 0x44, &zero, 4);
    std::memcpy(p + 0x40, &zero, 4);
    std::memcpy(p + 0x58, &big, 4);
}

// 0x10050f88
void RecalcTrackParams(tag_sCoasterInstance* instance) {
    (void)instance;
}

// 0x1005158c (the point stays in its register, r6, behind the float's slot r5)
void GetWrappedTrackPosition(sTrackPosnControl* control, tag_sTrackBrace* brace, float t, s3DPoint* point) {
    BezierPosition(control->mBrace->mF90->mControlsA + control->mIndex, brace->mF90->mControlsA, t, point);
}

// 0x10052290
tag_sTrackBrace* GetCurSelectedTrackBrace() { return gCoasterDragBrace; }

// 0x100522cc
void SetCurSelectedTrackBrace(tag_sTrackBrace* brace) { gCoasterDragBrace = brace; }

// 0x100526d4
int32_t IncCoasterConst2() {
    if (gCoasterEditInstance != nullptr && gCoasterEditInstance->mType != nullptr) {
        const float* type = reinterpret_cast<const float*>(gCoasterEditInstance->mType);
        CTRACE(0, "Curviness %.2f - Straight Divisor %.2f\n", static_cast<double>(type[2]), static_cast<double>(type[3]));
        FlagGenerateTrack(gCoasterEditInstance, 1);
    }
    return 1;
}

// 0x10052780 (the overload taking the instance; the one taking the coaster's object is 0x10052850)
int32_t IsTrackClosed(tag_sCoasterInstance* instance) {
    uint32_t flags = instance->mBlock3C;
    if ((flags & 1) && !(flags & 2)) return 1;
    return 0;
}

// 0x100527e0
tag_sCoasterInstance* GetCoasterInstanceFromCObject(void* object) {
    if (object == nullptr) return nullptr;
    uint32_t entrance;
    std::memcpy(&entrance, static_cast<uint8_t*>(object) + 0x1c, 4);
    return FindCoasterInstanceFromEntranceID(entrance);
}

// 0x1004de40: the id-th brace back from the block's last brace
tag_sTrackBrace* GetTrackBrace(sCoasterTrackParams* params, uint32_t id) {
    tag_sTrackBrace* brace = params->mLastBrace;
    while (id != 0) {
        brace = brace->mPrev;
        --id;
    }
    return brace;
}

// 0x10053d88
void ClearCoasterButtons() {
    DisableCurrentPylonEditState();
    gCoasterEditEnabled = 0;
    gCoasterEditDown = 0;
}

// 0x100545d4
int32_t OutputCoasterValues() {
    gCoasterWord7310 = 1;
    FlagGenerateTrack(gCoasterEditInstance, 0);
    return 1;
}

// 0x10058b30
int32_t IsCoasterLoftMode() { return static_cast<int32_t>(gCoasterInputFlags & 1); }

// 0x10058b68
void SetPlaceNormalPylon() { SetPylonType(1); }

// 0x10058bb8
void SetPlaceSpecialPylon() { SetPylonType(2); }

// 0x10058f4c (same machine code as Pilot::move)
void LayoutDeleteLastTrackBrace() { DeleteLastTrackBrace(); }

// 0x100590a0
void LayoutRecordCoasterSpeed(tag_sCoasterInstance* instance, uint32_t speed) {
    gActionRecorder.RecordAction(static_cast<RecordedAction>(0x45), static_cast<uint16_t>(instance->mObjectInstanceID),
                                 static_cast<uint16_t>(speed));
}

// 0x1005c250
void GetHFieldMaxVertexIndices(uint32_t* width, uint32_t* height) {
    *width = gHeightfieldHeader->mHField->mF18[0];
    *height = gHeightfieldHeader->mHField->mF18[1];
}

// 0x100617cc
void DeActivateHoardings(tag_sRideAnimSet* set) {
    if (set->mHoardingGeometry == nullptr) return;
    set->mHoardingSpeed = -0.30000001192092896f;
    if ((set->mFlags & 0x20) == 0) return;
    set->mFlags |= 0x80;
    set->mFlags &= ~0x40u;
}

// 0x10063cc0
void RestoreLevelHeightfield() { RestoreHFieldDataFromCopy(gHeightfieldHeader, gHeightfieldCopy); }

// 0x100647ec: the light handle of the index-th embedded record of the ride's mesh
sNodeLightParams* GetLightsourceHandle(uint32_t ride, uint32_t index) {
    sEmbeddedSet* embedded = static_cast<sEmbeddedSet*>(gRideAnimSets[ride]->mHeads->mF28);
    uintptr_t record = reinterpret_cast<uintptr_t>(embedded->mRecords) + index * sizeof(sEmbeddedRecord);
    if (record == 0) return nullptr;
    return reinterpret_cast<sNodeLightParams*>(static_cast<uintptr_t>(reinterpret_cast<sEmbeddedRecord*>(record)->mF08));
}

// 0x10065588
void SetMeshPosition(tag_sMesh* mesh, float x, float y, float z) {
    sMatrix* matrix = &mesh->mHeader->mF78.get()->mMatrix;
    matrix->m[3][0] = x;
    matrix->m[3][1] = y;
    matrix->m[3][2] = z;
}

// 0x100655d8
void ScaleMesh(tag_sMesh* mesh, s3DPoint* scale) {
    ScaleLMatrixNoTrans(&mesh->mHeader->mF78.get()->mMatrix, scale->x, scale->y, scale->z);
}
