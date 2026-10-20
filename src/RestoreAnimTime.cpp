// RestoreAnimTime 0x1006b6f4 (288 bytes): copies a saved time control back, re-finds the animation and restores its stats.
#include "TPEngine.h"
#include "rsse.h"

bool RestoreAnimTime(tag_sRideAnimSet* set, sAnimTimeControl* control, sAnimTimeControlSave* save) {
    control->mFlags = save->mF00;
    control->mF04 = save->mF04;
    control->mF08 = save->mF08;
    control->mF10 = static_cast<int32_t>(save->mF0C);
    control->mF14 = static_cast<int32_t>(save->mF10);
    control->mF18 = static_cast<int32_t>(save->mF14);
    control->mF0C = save->mF18;
    if (control->mFlags & 4) control->mFlags |= 0x10;
    control->mF24 = save->mF1C;
    control->mF28 = save->mF20;
    control->mF2C = save->mF24;
    control->mF30 = save->mF28;
    uint32_t b = control->mF08;
    uint32_t a = control->mF04;
    tag_sRideAnimSet* local = set;
    tag_sAnimHeader* animation = nullptr;
    if (IsValidAnimation(local, a, b) != 0) animation = local->mGroups[a].mEntries[b].mAnimation;
    if (animation != nullptr) RestoreAnimTimeStats(control, animation);
    return control->mF04 == 0;
}
