// Unit v5943: CalculateBraceCost 0x10058924, SetCoasterEdit(tag_sCoasterInstance*, int) 0x100585f4, CoasterAcceptLeftMouseClick 0x10057f80.
#include <cmath>
#include <cstdint>
#include <cstring>
#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "UIInterface.h"
#include "ppc_math.h"
#include "ui_main.h"

// Declared here, not in a header: the void* overload of the same name (LLCellSystem.h) is a different function and a nullptr argument would
// become ambiguous in the files that include both.
void SetCoasterEdit(tag_sCoasterInstance* instance, int32_t flag);
// 0x10067254 (12 bytes): the ride the pointer is over.
tag_sRideAnimSet* GetPointedAtRide();

// 0x10058924 (432 bytes): the cost of a brace: its pylon's base cost (when `a`), plus the distance to the next brace (or to the given point) and
// the height of its interval, each times the pylon control's price. The pylon control is the type's A one when the pylon's flags say so.
float CalculateBraceCost(tag_sCoasterInstance* instance, int32_t a, tag_sTrackBrace* brace, float* b, float* c, float* d, float* e) {
    if (brace == nullptr) return 0.0f;
    sLinkPylon* pylon = reinterpret_cast<sLinkPylon*>(const_cast<sTrackBraceData*>(brace->mData));
    if (pylon == nullptr) return 0.0f;
    sPylonControl* control = pylon->mControl;
    if (control == nullptr) return 0.0f;
    float x = d != nullptr ? *d : brace->mNext->mX4C;
    float z = e != nullptr ? *e : brace->mNext->mZ54;
    sPylonControl* other = pylon->mOther != nullptr ? pylon->mOther->mControl : nullptr;
    sPylonControl* typeA = const_cast<sPylonControl*>(&reinterpret_cast<const tag_sCoasterType*>(instance->mType)->mPylonsA);
    float cost = 0.0f;
    float height = GetHeightForInterval(brace->mFE8w, control) / 10.0f;
    float distance;
    if (b != nullptr) {
        float dz = *c - z;
        float dx = *b - x;
        float dz2 = dz * dz;
        distance = static_cast<float>(std::sqrt(ppc_fmadd(dx, dx, dz2))) / 10.0f;
    } else {
        float dz = brace->mZ54 - z;
        float dx = brace->mX4C - x;
        float dz2 = dz * dz;
        distance = static_cast<float>(std::sqrt(ppc_fmadd(dx, dx, dz2))) / 10.0f;
    }
    if (a != 0) cost = cost + control->mF5C;
    if ((control->mFlags & 0xc000) != 0) {
        if (other == nullptr || (other->mFlags & 0xc000) == 0) control = typeA;
        if ((control->mFlags & 0x4000) != 0 && (brace->mNext->mFlags & 0x80) == 0) control = typeA;
    }
    cost = ppc_fmadds(control->mF60, distance, cost);
    cost = ppc_fmadds(control->mF64, height, cost);
    return cost;
}

// 0x100585f4 (296 bytes): enters (instance and flag) or leaves the track editor for a coaster.
void SetCoasterEdit(tag_sCoasterInstance* instance, int32_t flag) {
    bool enter = instance != nullptr && flag != 0;
    gActionRecorder.RecordAction(0x44, static_cast<uint16_t>(enter ? instance->mObjectInstanceID : 0));
    if (enter) {
        if (gCoasterEditInstance == nullptr) {
            static_cast<CObject*>(instance->mCObject)->CloseObject();
            if ((instance->mBlock3C & 1) == 0 || (instance->mBlock3C & 2) != 0)
                SetCoasterState(instance, 1);
            else
                SetCoasterState(instance, 2);
        }
        SetCurrentEditCoaster(instance);
    } else if (gCoasterEditInstance != nullptr) {
        DisableCurrentPylonEditState();
        gCoasterEditEnabled = 0;
        gCoasterEditDown = 0;
        FlagGenerateTrack(gCoasterEditInstance, 0);
        tag_sCoasterInstance* edit = gCoasterEditInstance;
        SetCurrentEditCoaster(nullptr);
        if ((edit->mState & 2) != 0) static_cast<CObject*>(edit->mCObject)->Reopen();
    }
}

namespace {

// The brace of the editor's coaster whose ride is `ride` (the first single-brace-free pylon node's brace), or NULL. The edit coaster is not tested.
tag_sTrackBrace* FindBraceOfRide(tag_sRideAnimSet* ride) {
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    if (instance->mLastBrace != nullptr) {
        for (sCoasterPylon* node = instance->mPylons; node != nullptr; node = node->mNext) {
            if ((node->mFlags & 1) != 0) continue;
            for (uint32_t i = 0; i < node->mCount; i++) {
                tag_sTrackBrace* brace = node->mBraces[i];
                if (gRideAnimSets[brace->mWord70] == ride) return brace;
            }
        }
    }
    return nullptr;
}

}  // namespace

// 0x10057f80 (1,372 bytes): the left button of the track editor goes down (down != 0) or up, with the pylon tool in gCoasterWord16E0's low byte.
void CoasterAcceptLeftMouseClick(int32_t down) {
    if (gActionRecorder.mRecording != 0) return;
    uint32_t result = 0;
    tag_sRideAnimSet* ride = GetPointedAtRide();
    switch (gCoasterWord16E0 & 0xff) {
    case 2:
        if (down != 0 && gRideHighlightKeep == 0) {
            tag_sTrackBrace* brace = FindBraceOfRide(ride);
            gCoasterDragBrace = brace;
            result = StartMovePylon(brace) ? 1 : 0;
        }
        break;
    case 4:
        if (!((down != 0 && (gCoasterInputFlags & 4) != 0) || (down == 0 && (gCoasterInputFlags & 4) == 0))) {
            if (down != 0) gCoasterDragBrace = FindBraceOfRide(ride);
            tag_sCoasterInstance* instance = gCoasterEditInstance;
            if (instance != nullptr && instance->mF40 != 0 && gCoasterDragBrace != nullptr && (gCoasterDragBrace->mData->mFlags & 2) == 0) {
                if ((gCoasterDragBrace->mFlags & 0x40) != 0)
                    ToggleLoopTurnPylonState();
                else
                    ToggleBankInclinePylonState();
            }
        }
        break;
    case 8:
        if (down != 0 && (gCoasterInputFlags & 1) == 0) {
            gCoasterDragBrace = FindBraceOfRide(ride);
            ToggleLoftPylonState();
        } else if (down == 0 && (gCoasterInputFlags & 1) != 0) {
            ToggleLoftPylonState();
        }
        break;
    case 0x10:
        if (!((down != 0 && (gCoasterInputFlags & 2) != 0) || (down == 0 && (gCoasterInputFlags & 2) == 0))) {
            if (down != 0) gCoasterDragBrace = FindBraceOfRide(ride);
            ToggleRotatePylonState();
        }
        break;
    case 0x20:
        if (down == 0) {
            gCoasterDragBrace = FindBraceOfRide(ride);
            ToggleSwapUpPylonState();
        }
        break;
    case 0x40:
        if (down == 0) {
            gCoasterDragBrace = FindBraceOfRide(ride);
            ToggleSwapDownPylonState();
        }
        break;
    case 0x80:
        if (down == 0) {
            gCoasterDragBrace = FindBraceOfRide(ride);
            DoDeleteMultiPylons();
        }
        break;
    default:
        break;
    }
    (void)result;   // the original leaves StartMovePylon's answer in r3; the headers of the callers declare it void
}
