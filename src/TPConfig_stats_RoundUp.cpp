// TPConfig.cpp (main app), unit v5275: RoundUp 0x1004da1c (92 bytes) and GetCoasterTrackStats 0x1004d970 (116 bytes).
#include <cmath>
#include <cstdint>

#include "TPCoaster.h"
#include "ppc_math.h"

// 0x1004da1c: the integer part of the value (truncated), plus one when the fraction is not zero. A negative fraction counts too, so
// -1.5 gives -1 (the original's behaviour).
int32_t RoundUp(float value) {
    double whole;
    double fraction = std::modf(static_cast<double>(value), &whole);
    int32_t result = ppc_fctiwz(whole);
    if (0.0f != static_cast<float>(fraction)) result = result + 1;
    return result;
}

// 0x1004d970: copies the track length, the top height, the top speed and the marked-brace count of the coaster of the object.
void GetCoasterTrackStats(void* object, float* a, float* height, float* c, uint32_t* loops) {
    tag_sCoasterInstance* instance = GetCoasterInstanceFromCObject(object);
    *a = instance->mTrackLength;       // 0x10c
    *height = instance->mTopHeight;    // 0x110
    *c = instance->mTopSpeed;          // 0x114
    *loops = instance->mMarkedBraces;  // 0x108
}
