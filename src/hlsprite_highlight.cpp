// HLSprite.cpp, main app 0x1007a934..0x1007ac40: effect 2 follows a button.
#include "HLSprite.h"
#include "Debug.h"
#include "effect_manager.h"
#include "ppc_math.h"
#include "rsse.h"
#include <algorithm>

float gHLBaseArea;
int8_t gHLBaseAreaSet;
float gHLAreaScale;
int8_t gHLAreaScaleSet;
int32_t gHLMinEmitters = 2;
int32_t gHLMaxEmitters = 5;

void BTN_HighLight_On(float x, float y, float width, float height) {
    if (gHLBaseAreaSet == 0) {
        gHLBaseArea = 0.0016f; // 0x10264bc8, 0x3ad1b717
        gHLBaseAreaSet = 1;
    }
    if (gHLAreaScaleSet == 0) {
        gHLAreaScale = 0.0051f; // 0x10264bc4, 0x3ba71de7
        gHLAreaScaleSet = 1;
    }
    const float area = width * height;
    const float scaled = (area - gHLBaseArea) / gHLAreaScale;
    const int32_t count = static_cast<int32_t>(static_cast<uint32_t>(ppc_fctiwz(3.0f * scaled)) + 2u);
    const int32_t clamped = std::max(gHLMinEmitters, std::min(count, gHLMaxEmitters));
    CTRACE(8, "%.3f %d", static_cast<double>(area), clamped);
    EM_SetEmitersOn(2, clamped, 0x23);
    EMEffect& effect = gEffects[2];
    effect.mX = x;
    effect.mY = y;
    effect.mTargetX = x;
    effect.mTargetY = y;
    // The conversion uses xoris, hence a signed clock word even though the clock returns unsigned.
    effect.mStart = static_cast<float>(static_cast<int32_t>(RealTimePauseAdjusted()));
    effect.mDuration = 400.0f; // 0x10264bbc
    effect.mOrbitX = effect.mOrbitTargetX = width / 3.0f;
    effect.mOrbitY = effect.mOrbitTargetY = height / 3.0f;
    effect.mOrbitTurns = 0.4f; // 0x10264bb8
    effect.mOrbitX = effect.mOrbitTargetX;
    effect.mOrbitY = effect.mOrbitTargetY;
    effect.mOrbitTargetX = width;
    effect.mOrbitTargetY = height;
    effect.mOrbitTurns = 0.4f;
}

void BTN_HighLight_Move(float x, float y) {
    gEffects[2].mX = x;
    gEffects[2].mY = y;
    gEffects[2].mTargetX = x;
    gEffects[2].mTargetY = y;
}

void BTN_HighLight_Kill() {
    for (int32_t i = 0; i < gEffects[2].mCount; ++i) {
        const uint32_t handle = gEffects[2].mEmitters[i];
        if (handle != 0) PAR_SetEmitterMask(handle, 1);
    }
    BTN_HighLight_Off();
}

void BTN_HighLight_Off() { EM_SetEmitersOn(2, 0, 0); }
