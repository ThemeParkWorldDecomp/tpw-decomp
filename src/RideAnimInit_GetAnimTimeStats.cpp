#include "TPEngine.h"
#include "TPGlobals.h"
#include "ppc_math.h"
#include "platform/samsutils.h"

// 0x100c5bcc, 92 bytes. With a time, both mF14 and mF18 take it; then mF20 = rate * ((30 * frames) / 1000)
// where frames is the unsigned mF14 - mF10.
void GetAnimTimeStats(sAnimTimeControl* control, uint32_t* time) {
    if (time != nullptr) {
        control->mF14 = static_cast<int32_t>(*time);
        control->mF18 = static_cast<int32_t>(*time);
    }
    const float frames = static_cast<float>(static_cast<uint32_t>(control->mF14 - control->mF10));
    const float scaled = 30.0f * frames;
    control->mF20 = control->mF0C * (scaled / 1000.0f);
}

// 0x100c5c68, 132 bytes. With an animation, mF1C is its frame range (unsigned) and mF20 is
// ((30 * elapsed frames) / 1000) / rate. Without, mF1C and mF20 are cleared, and mF10.
void RestoreAnimTimeStats(sAnimTimeControl* control, tag_sAnimHeader* animation) {
    if (animation == nullptr) {
        control->mF1C = 0.0f;
        control->mF20 = 0.0f;
        control->mF10 = 0;
        return;
    }
    control->mF1C = static_cast<float>(static_cast<uint32_t>(animation->mF08 - animation->mF04));
    const float frames = static_cast<float>(static_cast<uint32_t>(control->mF14 - control->mF10));
    const float scaled = 30.0f * frames;
    const float seconds = scaled / 1000.0f;
    control->mF20 = seconds / control->mF0C;
}

// 0x100c5fd4, 136 bytes. Counts the time controls whose kind (mF04) is below 0xc into mAnimatedCount,
// and sets flag 0x80000 of the mesh header's flags when there is any (clears it first).
void GetNumActiveChannels(tag_sRideAnimSet* set) {
    set->mHeads->mHeader->mFlags &= ~0x80000u;
    set->mAnimatedCount = 0;
    for (uint32_t i = 0; i < set->mTimeCount; i++) {
        const sAnimTimeControl* control = reinterpret_cast<const sAnimTimeControl*>(static_cast<char*>(set->mTimeControls) + i * 0x38);
        set->mAnimatedCount += (control->mF04 < 0xc) ? 1 : 0;
    }
    if (set->mAnimatedCount == 0) return;
    set->mHeads->mHeader->mFlags |= 0x80000u;
}
