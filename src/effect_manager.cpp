// The effect manager (main app of SimTheme Park 2.04b3, the HLSprite.cpp range of the inventory, 0x1007a3d0..0x1007a6b8): EM_Init,
// EM_SetEmitersOn, EM_SetEffectTranslation, EM_SetEffectOrbit and EM_Tick. The advisor's sparkles are effects: a few emitters of the
// particle library that circle a point which the advisor moves about the screen. See include/effect_manager.h.
#include "effect_manager.h"

#include <cmath>
#include <cstring>

#include "ppc_math.h"
#include "rsse.h"

EMEffect gEffects[kEffects];   // 0x102dab10

namespace {

// The constants of the main app's data (0x10264be0..0x10264bf0, 0x1029d304).
constexpr float kOne = 1.0f;               // 0x10264be0, and the limit EM_Tick stops the radii's progress at (0x1029d304)
constexpr float kSineScale = 0.0009765625f;   // 0x10264bd4: 2^-10, the table's sines are 1024 times the sine
constexpr float kScreenWidth = 50000.0f;   // 0x10264bd0
constexpr float kScreenDepth = 37500.0f;   // 0x10264bcc
constexpr double kTable = 512.0;           // 0x10264bd8: the table's entries make a turn

// The listing builds a float from a signed integer by subtracting the magic double 2^52 + 2^31 from the integer's bits under 0x43300000,
// then rounding to single: the nearest float to the integer.
inline float ToFloat(int32_t value) { return static_cast<float>(value); }

// The word of effect `effect`'s emitter list at `slot`: the original indexes past the five entries (into the next effect), so the
// table is addressed as bytes.
inline uint32_t& EmitterSlot(int32_t effect, int32_t slot) {
    return *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(&gEffects[0]) + effect * sizeof(EMEffect) +
                                        __builtin_offsetof(EMEffect, mEmitters) + slot * sizeof(uint32_t));
}

}  // namespace

// 0x1007a3d0
void EM_Init() {
    for (int32_t i = 0; i < kEffects; ++i) std::memset(&gEffects[i], 0, sizeof(EMEffect));
    BMP_GenerateSinTab();
}

// 0x1007a454. Emitters are started or ended one at a time, in the order of the list; a handle that is 0 (the effect never got one)
// is not ended. The count is stored last.
int32_t EM_SetEmitersOn(int32_t effect, int32_t count, int32_t emitter) {
    EMEffect& fx = *reinterpret_cast<EMEffect*>(reinterpret_cast<uint8_t*>(&gEffects[0]) + effect * sizeof(EMEffect));
    int32_t started = 0;
    if (fx.mCount < count) {
        for (int32_t slot = fx.mCount; slot < count; ++slot) {
            EmitterSlot(effect, slot) = PAR_InitEmitter(emitter, 0, 0, 0);
            ++started;
        }
    } else if (count < fx.mCount) {
        for (int32_t slot = count; slot < fx.mCount; ++slot) {
            uint32_t& handle = EmitterSlot(effect, slot);
            if (handle != 0) {
                PAR_KillEmitter(static_cast<int32_t>(handle));
                --started;
                handle = 0;
            }
        }
    }
    fx.mCount = count;
    return started;
}

// 0x1007a558. The move runs from the old target to the new one when `index` is 0, from the new one to itself otherwise (no
// move). Only a duration other than 0 starts the clock (the start and duration are left alone for 0).
void EM_SetEffectTranslation(int32_t effect, int32_t index, float x, float y, float z) {
    EMEffect& fx = gEffects[effect];
    if (index == 0) {
        fx.mX = fx.mTargetX;
        fx.mY = fx.mTargetY;
    } else {
        fx.mX = x;
        fx.mY = y;
    }
    fx.mTargetX = x;
    fx.mTargetY = y;
    if (z != 0.0f) {
        fx.mStart = ToFloat(static_cast<int32_t>(RealTimePauseAdjusted()));
        fx.mDuration = z;
    }
}

// 0x1007a63c
void EM_SetEffectOrbit(int32_t effect, int32_t index, float a, float b, float c) {
    EMEffect& fx = gEffects[effect];
    if (index == 0) {
        fx.mOrbitX = fx.mOrbitTargetX;
        fx.mOrbitY = fx.mOrbitTargetY;
    } else {
        fx.mOrbitX = a;
        fx.mOrbitY = b;
    }
    fx.mOrbitTargetX = a;
    fx.mOrbitTargetY = b;
    fx.mOrbitTurns = c;
}

// 0x1007a6b8. `progress` is the move's elapsed time over its duration, in single precision (a duration of 0 gives infinity or NaN,
// which the orbit's clamp turns into 1 and the centre's move into whatever the float arithmetic gives). The centre follows the
// unclamped progress, the radii the clamped one. Emitter j of n stands j / n of a turn round the orbit plus progress * turns (the
// fraction of the sum, as a place in the table of 512 sines); its place is the centre plus (sin * radius x, -cos * radius y), put on
// a screen of 100000 by 75000 units, its height 0.
void EM_Tick() {
    for (int32_t i = 0; i < kEffects; ++i) {
        EMEffect& fx = gEffects[i];
        if (fx.mCount == 0) continue;
        const float now = ToFloat(static_cast<int32_t>(RealTimePauseAdjusted()));
        const float progress = (now - fx.mStart) / fx.mDuration;
        const float clamped = progress < kOne ? progress : kOne;
        const float centreX = ppc_fmadds(progress, fx.mTargetX - fx.mX, fx.mX);
        const float centreY = ppc_fmadds(progress, fx.mTargetY - fx.mY, fx.mY);
        for (int32_t j = 0; j < fx.mCount; ++j) {
            const float share = kOne / ToFloat(fx.mCount);
            const float turns = ppc_fmadds(ToFloat(j), share, progress * fx.mOrbitTurns);
            double whole;
            const float fraction = static_cast<float>(std::modf(static_cast<double>(turns), &whole));
            const int32_t angle = ppc_fctiwz(kTable * static_cast<double>(fraction));
            const float sine = ToFloat(HOST_Sin(angle)) * kSineScale;
            const float cosine = ToFloat(HOST_Cos(angle)) * kSineScale;
            const float radiusX = ppc_fmadds(clamped, fx.mOrbitTargetX - fx.mOrbitX, fx.mOrbitX);
            const float radiusY = ppc_fmadds(clamped, fx.mOrbitTargetY - fx.mOrbitY, fx.mOrbitY);
            const float x = centreX + sine * radiusX;
            const float y = centreY - cosine * radiusY;
            PAR_SetEmitterPos(static_cast<int32_t>(fx.mEmitters[j]), ppc_fctiwz(kScreenWidth * (kOne + x)), 0,
                              ppc_fctiwz(kScreenDepth * (kOne + y)));
        }
    }
}
