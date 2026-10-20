// 0x1003bb44 (652 bytes, TPCoaster.cpp): SetupBraceAnimFrameData. Sets the five time controls (records of 0x38 bytes) of the
// brace's ride anim set: each value (the loft, the second value, the wobble, and two more) is scaled by a table of the
// pylon record's +0x18 block, clamped to a range and multiplied by the record's float at +0x1c, stored at +0x20.
#include <cmath>
#include <cstdint>

#include "TPCoaster.h"
#include "TPEngine.h"

namespace {

float Clamp(float v, float lo, float hi) {
    if (v < lo) v = lo;   // fcmpo / bge: a NaN branches over
    if (v > hi) v = hi;
    return v;
}

void Control(tag_sRideAnimSet* set, uint32_t offset, float value, const float* t, float add) {
    float v = value * t[2];
    v = v + t[3];
    v = v + add;
    v = Clamp(v, t[0], t[1]);
    float* rec = reinterpret_cast<float*>(static_cast<char*>(set->mTimeControls) + offset);
    rec[0x20 / 4] = v * rec[0x1c / 4];
}

}  // namespace

void SetupBraceAnimFrameData(sCoasterType* type, tag_sTrackBrace* brace) {
    (void)type;
    uint32_t index = brace->mWord70;
    const sTrackBraceData* data = brace->mData;
    if ((data->mFlags & 1) != 0) return;
    if ((brace->mFlags & 0x2000) != 0) return;
    const float* table = reinterpret_cast<const float*>(reinterpret_cast<const char*>(data->mF18Table) + 0xd0);
    float loft = brace->mFE8w;
    if (loft > 1.0f) brace->mFE8w = 1.0f;
    else if (loft < 0.0f) brace->mFE8w = 0.0f;
    double whole;
    float frac = static_cast<float>(std::modf(static_cast<double>(brace->mFF8), &whole));
    if (frac < 0.0f) frac = 1.0f + frac;
    brace->mFF8 = frac;
    tag_sRideAnimSet* set = gRideAnimSets[index];
    Control(set, 0x00, brace->mFE8w, table + 0, 0.0f);
    float second = brace->mFEC;
    if ((brace->mFlags & 0x10000000) != 0) {
        float sum = second + 0.5f;
        second = static_cast<float>(std::modf(static_cast<double>(sum), &whole));
    }
    Control(set, 0x38, second, table + 4, 0.0f);
    Control(set, 0xe0, brace->mFF8, table + 16, 0.0f);
    Control(set, 0x70, brace->mFF0, table + 8, 0.5f);
    Control(set, 0xa8, brace->mFF4, table + 12, 0.5f);
}
