// Unit v5053: the loft of a new brace (0x1003e108) and the link level of a pylon (0x1003e7dc), TPCoaster.cpp on the Mac.
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ppc_math.h"

namespace {

// "TrackMaster" + 0x1fa, 0x237, 0x278, 0x2cf, 0x303, 0x33f
const char kLinkingOnTop[] = "Loop is linking ontop of pylon at level %d with height %.3f\n";
const char kMoreThanEnough[] = "More than enough room inside pylon at level %d with height %.3f\n";
const char kJustSqueeze[] = "Can just squeeze inside pylon at level %d with height %.3f - not returning yet though\n";
const char kStickingOnTop[] = "Sticking brace on top at level %d with height %.3f\n";
const char kGoingToSqueeze[] = "Going to squeeze inside pylon at level %d with height %.3f\n";
const char kCantFit[] = "Can't fit anywhere without moving track, setting valid at top, level %d with height %.3f\n";

// Pointers read at a Mac offset of an opaque block (the host's block holds a native pointer there, unaligned for the 4 byte offset).
template <class T>
T* PointerAt(const void* block, size_t offset) {
    T* value;
    std::memcpy(&value, static_cast<const uint8_t*>(block) + offset, sizeof(value));
    return value;
}

}  // namespace

float GetNewBraceLoftInterval(sLinkPylon* pylon, sPylonControl* control, tag_sTrackBrace* brace, tag_sTrackBrace* other, int32_t a, int32_t b,
                              float* out) {
    float height;   // f31
    if (control == nullptr || (control->mFlags & 0x4000) == 0 || a != 0) {
        height = gCoasterEditInstance->mF118;
    } else {
        s3DPoint from;
        s3DPoint to;
        from.x = pylon->mX;
        from.y = pylon->mY;
        from.z = pylon->mZ;
        to.x = pylon->mOther->mX;
        to.y = pylon->mOther->mY;
        to.z = pylon->mOther->mZ;
        from.y = 0.0f;
        to.y = 0.0f;
        float f0 = 0.0f;
        height = 0.0f;   // the original keeps its caller's f31 when `other` is NULL
        if (other != nullptr) {
            const sCoasterPylon* data = reinterpret_cast<const sCoasterPylon*>(other->mData);
            if (data->mFlags & 1) {
                std::memcpy(&f0, &other->mF0C[0x50 - 0x0c], sizeof(float));
            } else {
                sPylonControl* otherControl = data->mControl;
                const void* mesh = gRideAnimSets[other->mWord70]->mHeads;
                const void* holder = static_cast<const tag_sMesh*>(mesh)->mF28;
                const uint8_t* table = PointerAt<const uint8_t>(holder, 4);
                const uint8_t* entry = PointerAt<const uint8_t>(table + otherControl->mF2C * 0x14, 4);
                if (entry != nullptr) std::memcpy(&f0, entry + 0x34, sizeof(float));
            }
            height = f0;
        }
        float distance = DistanceBetweenPoints(&from, &to);
        height = ppc_fmadds(distance, 0.5f, height);
        const sCoasterPylon* braceData = reinterpret_cast<const sCoasterPylon*>(brace->mData);
        float interval = GetIntervalForHeight(height, braceData->mControl);
        SetLoftPylon(interval, brace->mNumber, 1, 0);
    }
    if (out != nullptr) *out = height;
    float result = GetIntervalForHeight(height, control);
    if (pylon->mCount == 0) {
        if (!(result >= 0.0f)) result = 0.0f;
        if (!(result <= 1.0f)) result = 1.0f;
    }
    if (b != 0) result = height;
    return result;
}

int32_t DetermineLinkLevel(float height, sLinkPylon* pylon, uint32_t* level, float* out, int32_t quiet) {
    sPylonControl* control = reinterpret_cast<sPylonControl*>(pylon->mControl);
    float limit = height;
    if (control->mF6C < height) limit = control->mF6C;   // fVar1: the height capped at the control's top

    if ((gCoasterInputFlags & 0x100) == 0 && pylon->mCount != 0 && (pylon->mBraces[0]->mFlags & 0x40) != 0) {
        float top = CalculateBraceHeight(pylon->mBraces[pylon->mCount - 1]);
        float extra = pylon->mCount != 0 ? control->mFC4 : 0.0f;
        float sum = top + extra;
        float reach = control->mF68 + sum;
        if (reach < control->mF6C) {
            *level = pylon->mCount;
            extra = pylon->mCount != 0 ? control->mFC4 : 0.0f;
            float value = control->mF68 + (top + extra);
            *out = (value >= limit) ? value : limit;
            if (quiet == 0) CTRACE(5, kLinkingOnTop, *level, static_cast<double>(*out));
            return 1;
        }
    }

    bool found = false;
    uint32_t foundLevel = 0;
    float previous = 0.0f;   // f31
    for (uint32_t i = 0; i < pylon->mCount; i++) {
        tag_sTrackBrace* brace = pylon->mBraces[i];
        float step = i != 0 ? 1.0f : 0.0f;   // f29
        float current = CalculateBraceHeight(brace);
        float f4 = control->mFC4;
        float f5 = control->mF68;
        float need = ppc_fmadds(2.0f, f5, f4 * (1.0f + step));
        float room = 0.01f + (current - previous);
        if (room >= need) {
            float inside = 0.01f + ((current - f4) - f5);
            if (limit < inside) {
                uint32_t word;
                std::memcpy(&word, &brace->mF00[4], sizeof(word));
                *level = word;
                *out = limit;
                if (quiet == 0) CTRACE(5, kMoreThanEnough, *level, static_cast<double>(*out));
                return 1;
            }
            found = true;
            foundLevel = i;
            *out = f5 + ppc_fmadds(f4, step, previous);
            if (quiet == 0) CTRACE(5, kJustSqueeze, i, static_cast<double>(*out));
        }
        previous = current;
    }

    uint32_t count = pylon->mCount;
    float extra = count != 0 ? control->mFC4 : 0.0f;
    float reach = control->mF68 + (previous + extra);
    if (reach < control->mF6C) {
        *level = count;
        extra = pylon->mCount != 0 ? control->mFC4 : 0.0f;
        float value = control->mF68 + (previous + extra);
        *out = (value >= limit) ? value : limit;
        *out = (*out < control->mF6C) ? *out : control->mF6C;
        if (quiet == 0) CTRACE(5, kStickingOnTop, *level, static_cast<double>(*out));
        return 1;
    }
    if (found) {
        *level = foundLevel;
        if (quiet == 0) CTRACE(5, kGoingToSqueeze, *level, static_cast<double>(*out));
        return 1;
    }
    *level = count;
    *out = control->mF68 + (previous + control->mFC4);
    if (quiet == 0) CTRACE(5, kCantFit, *level, static_cast<double>(*out));
    return 1;
}
