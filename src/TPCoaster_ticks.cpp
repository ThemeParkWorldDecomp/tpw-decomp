// The coasters' per-frame ticks and the track editor's pointer input (main app TPCoaster.cpp and the TPConfig.cpp range, unit w137):
// CoasterTickPreRender 0x10043028 (428 bytes), CoasterTickPostRender 0x10043200 (176) and CoasterUserInputTick 0x100554b4 (2,348).
// The globals' places are in include/TPCoaster.h (names invented); every callee outside the unit is untranslated.
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ppc_math.h"
#include "rsse.h"

tag_sCoasterInstance* gCoasterCurrent;
void* gCoasterCurrentData;
tag_sCoasterInstance* gCoasterEditInstance;
uint32_t gCoasterWord7310;
uint32_t gCoasterInputFlags;
tag_sTrackBrace* gCoasterDragBrace;
float gCoasterDragAnchor[2];
float gCoasterDragScaleX;
float gCoasterDragScaleY;
float gCoasterDragLastDX;
float gCoasterDragLastDY;
float gCoasterSavedVertical1[3];
float gCoasterSavedVertical2[3];
float gCoasterSavedHorizontal1[3];
float gCoasterSavedHorizontal2[3];
float* gCoasterVerticalTarget1;
float* gCoasterVerticalTarget2;
float* gCoasterHorizontalTarget1;
float* gCoasterHorizontalTarget2;
float gCoasterVerticalBase1;
float gCoasterVerticalMin1;
float gCoasterVerticalMax1;
float gCoasterVerticalBase2;
float gCoasterVerticalMin2;
float gCoasterVerticalMax2;
float gCoasterHorizontalBase1;
float gCoasterHorizontalMin1;
float gCoasterHorizontalMax1;
float gCoasterHorizontalBase2;
float gCoasterHorizontalMin2;
float gCoasterHorizontalMax2;
float gCoasterCostBefore;
int32_t gCoasterGridCellX;
int32_t gCoasterGridCellZ;
tag_sRideAnimSet* gCoasterPreviousPointedRide;
uint32_t gCoasterWord16E0;

namespace {

constexpr float kHalfPi = 1.5707964f;   // 0x3fc90fdb (TOC 0x102648e4)

constexpr uint32_t kTrainRunning = 2;   // the train's flags word

// The sine table at an angle in radians: the product with the scale in single precision, truncated (saturating), and-ed with the mask.
float TableAt(float scaled) {
    return gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(scaled)))];
}

// The sine and the cosine of an angle as the input tick reads them (the cosine at pi/2 plus the angle).
struct SinCos {
    float sine;
    float cosine;
};
SinCos SinCosAt(float angle) {
    const float scale = gTPGlobals.mSinTableScale;
    const float sine = TableAt(angle * scale);
    const float cosine = TableAt((kHalfPi + angle) * scale);
    return {sine, cosine};
}

// Turns the point about the y axis (x and z change): x' = x cos + z sin, z' = -x sin + z cos, each fused as the listing has it.
void TurnXZ(float* p, const SinCos& t) {
    const float x = p[0];
    p[0] = ppc_fmadds(x, t.cosine, p[2] * t.sine);
    p[2] = ppc_fmadds(ppc_fneg(x), t.sine, p[2] * t.cosine);
}

// About the x axis (y and z change): y' = y cos - z sin, z' = y sin + z cos.
void TurnYZ(float* p, const SinCos& t) {
    const float y = p[1];
    p[1] = ppc_fmsubs(y, t.cosine, p[2] * t.sine);
    p[2] = ppc_fmadds(y, t.sine, p[2] * t.cosine);
}

// The vertical drag of a target that is turned: the saved point copied in, then turned about y by the angle, about x by the delta
// and about y by the angle back.
void TurnSavedPoint(float* target, const float* saved, float angle, float delta) {
    std::memcpy(target, saved, 3 * sizeof(float));
    TurnXZ(target, SinCosAt(ppc_fneg(angle)));
    TurnYZ(target, SinCosAt(delta));
    TurnXZ(target, SinCosAt(angle));
}

// A clamp of a target's first float between the minimum and the maximum (the compares are fcmpo: a NaN takes the first branch).
// Stores the limit and returns 1 when the value went to the minimum, 2 when to the maximum, 0 when it was inside.
int Clamp(float* target, float minimum, float maximum) {
    const float value = *target;
    if (!(value >= minimum)) {
        *target = minimum;
        return 1;
    }
    if (!(value <= maximum)) {
        *target = maximum;
        return 2;
    }
    return 0;
}

}  // namespace

// 0x10043028. The first walk generates the flagged coasters' tracks (the preview brace is linked into the editor's coaster; the action
// 0x43 and the coaster's id are recorded the first time), the second ticks the running ones. GenerateTrack gets the global.
void CoasterTickPreRender() {
    for (tag_sCoasterInstance* instance = gCoasterInstances; instance != nullptr; instance = instance->mNext) {
        if ((instance->mFlags & 1) == 0) continue;
        gCoasterCurrent = instance;
        gCoasterCurrentData = &instance->mBlock3C;
        if (instance == gCoasterEditInstance && (instance->mFlags & 4) != 0) LinkPreviewBrace(instance);
        if ((instance->mFlags & 0x2000) == 0) gActionRecorder.RecordAction(0x43, static_cast<uint16_t>(instance->mObjectInstanceID));
        GenerateTrack(gCoasterCurrent);
        instance->mFlags = instance->mFlags | 0x2000;
    }
    gCoasterCurrent = nullptr;
    for (tag_sCoasterInstance* instance = gCoasterInstances; instance != nullptr; instance = instance->mNext) {
        if ((instance->mState & 0xe) == 0) continue;
        CoasterCarRideTrackTick(instance);
        if ((*instance->mType & 4) != 0) CoasterUVAnimTick(instance);
        sTrainInstance* train = instance->mTrains;
        const uint32_t state = instance->mState;
        if ((state & 0xc) != 0 && (state & 0x40) == 0) {
            for (uint32_t count = static_cast<uint32_t>(instance->mTrainCount); count != 0; --count) {
                if ((train->mFlags & kTrainRunning) != 0) {
                    HandleTrainLoading(instance, train);
                    TickTrainSounds(instance, train);
                }
                ++train;
            }
        } else {
            for (uint32_t count = static_cast<uint32_t>(instance->mTrainCount); count != 0; --count) {
                if ((train->mFlags & kTrainRunning) != 0) TickTrainSounds(instance, train);
                ++train;
            }
        }
    }
    gCoasterWord7310 = 0;
}

// 0x10043200. The flagged coasters lose flag 1 (through the global); the editor's one unlinks its preview brace first.
void CoasterTickPostRender() {
    for (tag_sCoasterInstance* instance = gCoasterInstances; instance != nullptr; instance = instance->mNext) {
        if ((instance->mFlags & 1) == 0) continue;
        gCoasterCurrent = instance;
        gCoasterCurrentData = &instance->mBlock3C;
        if (instance == gCoasterEditInstance && (instance->mFlags & 4) != 0) UnLinkPreviewBrace(instance);
        gCoasterCurrent->mFlags = gCoasterCurrent->mFlags & ~1u;
    }
    gCoasterCurrent = nullptr;
}

// 0x100554b4. Moves the brace the player drags in the track editor: the pointer's offset from the drag's anchor, scaled, is turned by
// the camera yaw (flag 0x2000), and drives the vertical targets (flags 0x400 move, 0x1000 turn saved points) and the horizontal ones
// (0x200, 0x800). The first of a pair also moves the anchor back when it clamps (without 0x2000). Then the multi-brace preview, the
// data change and the cost of the move (a refund adds -0xeeeeeee).
void CoasterUserInputTick() {
    bool changed = false;
    if (gCoasterEditInstance == nullptr || gCoasterEditInstance->mF40 == 0) return;
    float yaw;
    if (gCoasterDragBrace == nullptr) {
        yaw = gCameraF5BF4;
    } else {
        yaw = gCameraF5BF4 - gCoasterDragBrace->mAngle;
    }
    float dx = 0.0f;
    float dy = 0.0f;
    uint32_t flags = gCoasterInputFlags;
    if ((flags & 0x3f) != 0 && (flags & 0x1e00) != 0) {
        dx = gCoasterDragScaleX * (gCoasterDragAnchor[0] - gPointerScreenXY[0]);
        dy = gCoasterDragScaleY * (gCoasterDragAnchor[1] - gPointerScreenXY[1]);
        if ((flags & 0x2000) != 0) {
            const SinCos t = SinCosAt(yaw);
            const float newDX = ppc_fmsubs(dx, t.cosine, dy * t.sine);
            const float newDY = ppc_fmadds(dx, t.sine, dy * t.cosine);
            dx = newDX;
            dy = newDY;
        }
        // The vertical drags: the delta of y, or (with a horizontal drag) of x, changed since the last tick.
        if ((flags & 0x1400) != 0 &&
            (((flags & 0xa00) != 0 && dx != gCoasterDragLastDX) || dy != gCoasterDragLastDY)) {
            if ((flags & 0x400) != 0) {
                float* target = gCoasterVerticalTarget1;
                *target = gCoasterVerticalBase1 + dy;
                const int clamped = Clamp(target, gCoasterVerticalMin1, gCoasterVerticalMax1);
                if (clamped != 0 && (gCoasterInputFlags & 0x2000) == 0) {
                    gCoasterDragAnchor[1] = gPointerScreenXY[1] + (*target - gCoasterVerticalBase1) / gCoasterDragScaleY;
                }
                float* second = gCoasterVerticalTarget2;
                if (second != nullptr) {
                    *second = gCoasterVerticalBase2 + dy;
                    Clamp(second, gCoasterVerticalMin2, gCoasterVerticalMax2);
                }
            } else if ((flags & 0x1000) != 0) {
                float* target = gCoasterVerticalTarget1;
                std::memcpy(target, gCoasterSavedVertical1, 3 * sizeof(float));
                const float angle = GetVectorRotationAroundYAxisFromZ(*reinterpret_cast<s3DPoint*>(target));
                TurnXZ(target, SinCosAt(ppc_fneg(angle)));
                TurnYZ(target, SinCosAt(dy));
                TurnXZ(target, SinCosAt(angle));
                float* second = gCoasterVerticalTarget2;
                if (second != nullptr) TurnSavedPoint(second, gCoasterSavedVertical2, angle, dy);
            }
            changed = true;
            gCoasterDragLastDY = dy;
        }
        flags = gCoasterInputFlags;
        // The horizontal drags: the delta of x changed, or (with a vertical drag) the delta of y.
        if ((flags & 0xa00) != 0 &&
            (dx != gCoasterDragLastDX || ((flags & 0x1400) != 0 && dy != gCoasterDragLastDY))) {
            if ((flags & 0x200) != 0) {
                float* target = gCoasterHorizontalTarget1;
                *target = gCoasterHorizontalBase1 + dx;
                const int clamped = Clamp(target, gCoasterHorizontalMin1, gCoasterHorizontalMax1);
                if (clamped != 0 && (gCoasterInputFlags & 0x2000) == 0) {
                    gCoasterDragAnchor[0] = gPointerScreenXY[0] + (*target - gCoasterHorizontalBase1) / gCoasterDragScaleX;
                }
                float* second = gCoasterHorizontalTarget2;
                if (second != nullptr) {
                    *second = gCoasterHorizontalBase2 + dx;
                    Clamp(second, gCoasterHorizontalMin2, gCoasterHorizontalMax2);
                }
            } else if ((flags & 0x800) != 0) {
                float* target = gCoasterHorizontalTarget1;
                std::memcpy(target, gCoasterSavedHorizontal1, 3 * sizeof(float));
                TurnXZ(target, SinCosAt(dx));
                float* second = gCoasterHorizontalTarget2;
                if (second != nullptr) {
                    std::memcpy(second, gCoasterSavedHorizontal2, 3 * sizeof(float));
                    TurnXZ(second, SinCosAt(dx));
                }
            }
            changed = true;
            gCoasterDragLastDX = dx;
        }
    }
    flags = gCoasterInputFlags;
    if ((flags & 0x13f) != 0) {
        gCoasterGridCellX = ppc_fctiwz(static_cast<double>(gPointerWorldPosn.x / 10.0f));
        gCoasterGridCellZ = ppc_fctiwz(static_cast<double>(gPointerWorldPosn.z / 10.0f));
        if ((flags & 0x180) != 0) {
            HandleMultiBracePreviewChange(gCoasterEditInstance, gCoasterPreviousPointedRide, gPointedRide,
                                          gCoasterPreviousPointedRide != gPointedRide, static_cast<uint32_t>(gCoasterGridCellX),
                                          static_cast<uint32_t>(gCoasterGridCellZ));
        }
        gCoasterPreviousPointedRide = gPointedRide;
    }
    if (changed && gCoasterDragBrace != nullptr) {
        HandleCoasterDataChange(gCoasterInputFlags, gCoasterDragBrace, gCoasterEditInstance, 0, 0);
    }
    if ((gCoasterInputFlags & 0x3f) != 0 && gCoasterDragBrace != nullptr && (gCoasterDragBrace->mData->mFlags & 2) == 0 &&
        gActionRecorder.mRecording == 0) {
        const int32_t before = ppc_fctiwz(static_cast<double>(gCoasterCostBefore));
        const float cost = CalculateBraceCost(gCoasterEditInstance, 0, gCoasterDragBrace, nullptr, nullptr, nullptr, nullptr);
        const int32_t difference = static_cast<int32_t>(static_cast<uint32_t>(RoundUp(cost)) - static_cast<uint32_t>(before));
        if (difference < 0) {
            // A refund: the marker -0xeeeeeee is added (addis -0xeef, addi 0x1112).
            LLCC_SystemAddToSessionCost(static_cast<int32_t>(static_cast<uint32_t>(difference) - 0xeeeeeeeu));
        } else {
            LLCC_SystemAddToSessionCost(difference);
        }
    }
    if ((gCoasterWord16E0 & 1) != 0) FlagGenerateTrack(gCoasterEditInstance, 0);
}
