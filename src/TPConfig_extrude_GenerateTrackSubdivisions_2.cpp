// GenerateTrackSubdivisions 0x1004e8c0, CoasterSetSpeed (instance overload) 0x1004b94c, GetBankAtBrace 0x1004b1f8,
// FlashEndPylons 0x10052158, DecCoasterConst1 0x100524a0.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ppc_math.h"
#include "Debug.h"
#include "keytable_actions.h"

// 0x1004e8c0. Fills `out` with the sample positions of the section: for each of `count` steps (the brace's spline count when a == b, else 11)
// the position t0 + span * i / (count - 1), then every key of the params array that lies in [position, next position) and differs from it.
// Answers how many floats it wrote. The params array is walked on (stride sizeof(sExtrusionParams)).
int32_t GenerateTrackSubdivisions(tag_sTrackBrace* a, tag_sTrackBrace* b, sExtrusionParams* params, float* out, float span) {
    int32_t written = 0;
    while (params->mF00 < gExtrudeTOffset) ++params;
    uint32_t count;
    if (a == b) count = b->mF90->mCount;
    else count = 11;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t last = count - 1;
        float t0 = gExtrudeTOffset;
        float start = ppc_fmadds(span, static_cast<float>(i) / static_cast<float>(last), t0);
        float end = ppc_fmadds(span, static_cast<float>(i + 1) / static_cast<float>(last), t0);
        out[written++] = start;
        while (i < last) {
            float key = params->mF00;
            if (!(key >= start)) break;
            if (!(key < end)) break;
            if (key != start) out[written++] = key;
            ++params;
        }
    }
    return written;
}

// 0x1004b94c. Sets the coaster's speed (a percentage): blends the type's two speed limits, flags the track for generation.
void CoasterSetSpeed(tag_sCoasterInstance* instance, uint32_t speed, int32_t flag) {
    if (flag == 0) LayoutRecordCoasterSpeed(instance, speed);
    const tag_sCoasterType* type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    float fraction = static_cast<float>(speed) / 100.0f;
    float one = 1.0f;
    float high = type->mF50;
    float low = type->mF4C;
    float inverse = one - fraction;
    float scaled = high * fraction;
    instance->mSpeedLimit = ppc_fmadds(low, inverse, scaled);
    std::memcpy(instance->mF30, &speed, 4);
    FlagGenerateTrack(instance, 1);
    type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    if ((type->mFlags & 8) != 0) FlagRecalcAllCoasterCollisions(instance);
}

// 0x1004b1f8. The bank change per unit of travel at the brace (0 when it has no bank or flag 0x100).
float GetBankAtBrace(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, float f) {
    float zero = 0.0f;
    if (brace->mF94w != zero && (brace->mFlags & 0x100) == 0) {
        float d1 = DistanceBetweenPoints(reinterpret_cast<const s3DPoint*>(&brace->mNext->mX4C), reinterpret_cast<const s3DPoint*>(&brace->mX4C));
        float d2 = DistanceBetweenPoints(reinterpret_cast<const s3DPoint*>(&brace->mPrev->mX4C), reinterpret_cast<const s3DPoint*>(&brace->mX4C));
        float bank = brace->mF94w;
        float sum = d1 + d2;
        float limit;
        if (bank >= zero) limit = 3.1415927410125732f;
        else limit = -3.1415927410125732f;
        float diff = limit - bank;
        const tag_sCoasterType* type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
        float scaled = f * diff;
        float per = scaled / type->mF10;
        return per / sum;
    }
    return 0.0f;
}

// 0x10052158. Stops the flashing of the current end pylon's set, then flashes the one the mode names (1 the first, 2 the last).
void FlashEndPylons(tag_sCoasterInstance* instance, uint32_t mode) {
    if (instance->mFlashBrace != nullptr) {
        tag_sRideAnimSet* set = gRideAnimSets[instance->mFlashBrace->mWord70];
        if ((set->mFlags & 0x4000000) != 0) StopFlashingObject(set);
        instance->mFlashBrace = nullptr;
    }
    tag_sTrackBrace* brace = nullptr;
    if ((mode & 1) != 0) {
        if ((instance->mBlock3C & 2) != 0) brace = instance->mPreviewBrace;
        else brace = instance->mBraces;
    } else if ((mode & 2) != 0) {
        if ((instance->mBlock3C & 2) != 0) brace = instance->mBrace64;
        else brace = instance->mLastBrace;
    }
    if (brace != nullptr) {
        tag_sRideAnimSet* set = gRideAnimSets[brace->mWord70];
        if ((set->mFlags & 0x4000000) == 0) StartFlashingObject(set);
        instance->mFlashBrace = brace;
    }
}

// 0x100524a0. Key action: divides the edited coaster type's curviness by 1.05, traces both values, flags the track.
int32_t DecCoasterConst1() {
    if (gCoasterEditInstance != nullptr) {
        tag_sCoasterType* type = reinterpret_cast<tag_sCoasterType*>(const_cast<uint32_t*>(gCoasterEditInstance->mType));
        if (type != nullptr) {
            type->mF08 = type->mF08 / 1.05f;
            type = reinterpret_cast<tag_sCoasterType*>(const_cast<uint32_t*>(gCoasterEditInstance->mType));
            CTRACE(0, "Curviness %.2f - Straight Divisor %.2f\n", static_cast<double>(type->mF08), static_cast<double>(type->mF0C));
            FlagGenerateTrack(gCoasterEditInstance, 1);
        }
    }
    return 1;
}
