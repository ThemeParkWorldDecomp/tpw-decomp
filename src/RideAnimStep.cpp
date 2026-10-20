// The ride mesh's frame callback and animation queries (SimTheme Park 2.04b3).
// TPRide.cpp: AnimateRideCallback 0x10069a98, ProcessRideAnim 0x1005b24c,
// HandleActiveHoardings 0x100619d4. rsse.cpp: GetCurrentAnimStats 0x100c7930.
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPEmbed.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"

// 0x10069a98, 80 bytes.
void AnimateRideCallback(tag_sM3D2Header* header) {
    tag_sRideAnimSet* set = header->mRideSet.get();
    if (set != nullptr) {
        ProcessRideAnim(set, 1);
        set->mFlags |= 0x200000;
    }
}

// 0x1005b24c, 228 bytes.
void ProcessRideAnim(tag_sRideAnimSet* set, int32_t animate) {
    if (animate != 0 && (set->mFlags & 0x800000) != 0) {
        if ((set->mFlags & 0x80000) == 0) ResetRideAnimationAllChannels(set);
        set->mFlags &= ~0x800000u;
    }
    if (set->mF14 != nullptr && (set->mFlags & 0x3000) == 0) {
        AnimateRideSet(set, animate, animate != 0 ? 0 : 8);
        if (animate != 0 && set->mHeads->mF28 != nullptr) TickRideHeads(set->mHeads);
    }
    if ((set->mFlags & 0x20) != 0) HandleActiveHoardings(set, gTPGlobals.mF4014);
}

// 0x100619d4, 216 bytes. Reaching an endpoint changes the flags on the next unchanged step.
void HandleActiveHoardings(tag_sRideAnimSet* set, float step) {
    if (set->mDynamicMeshSet == nullptr || set->mHoardingMesh == nullptr || (set->mFlags & 0xc0) == 0) return;
    const float old = set->mHoardingLevel;
    float level = ppc_fmadds(set->mHoardingSpeed, step, old);
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    set->mHoardingLevel = level;
    if (level == old) {
        if ((set->mFlags & 0x80) == 0) {
            if (level >= 1.0f) set->mFlags &= ~0x40u;
        } else if (level <= 0.0f) {
            set->mFlags &= ~0xa0u;
            EnableRideHoardings(set, 0);
        }
    } else {
        SetRideHoardingLevel(set, level);
    }
}

// 0x100c7930, 136 bytes. Runtime helper 0x10237cd8 is __cvt_fp2unsigned.
uint32_t GetCurrentAnimStats(tag_sRideAnimSet* set, uint32_t* kind, uint32_t* index,
                             uint32_t* playing, uint32_t control) {
    auto* record = static_cast<sAnimTimeControl*>(set->mTimeControls) + control;
    if (kind != nullptr) *kind = record->mF04;
    if (index != nullptr) *index = record->mF08;
    if (playing != nullptr) *playing = record->mFlags & 1;
    const float frames = record->mF1C - record->mF20;
    const float milliseconds = 0x1.0aaaaap+5f * frames;
    return cw_cvt_fp2unsigned(milliseconds);
}
