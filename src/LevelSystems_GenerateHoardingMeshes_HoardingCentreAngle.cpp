#include <cmath>
#include <cstdint>
#include "TPEngine.h"

// 0x1005f8c4 (380 bytes): the angle from (halfWidth, halfHeight) to the middle of one side (0 to 3) of cell (x, y),
// atan2 in double, plus pi and a quarter pi in single, wrapped by fmod at 2 pi. Sides outside 0..3 leave the registers
// unset in the original; here the point is (0, 0).
float HoardingCentreAngle(int32_t x, int32_t y, int32_t side, float halfWidth, float halfHeight) {
    const float fx = static_cast<float>(static_cast<double>(x));
    const float fy = static_cast<float>(static_cast<double>(y));
    float px = 0.0f;
    float py = 0.0f;
    switch (side) {
        case 0: px = 0.5f + fx; py = 1.0f + fy; break;
        case 1: px = 1.0f + fx; py = 0.5f + fy; break;
        case 2: px = 0.5f + fx; py = fy; break;
        case 3: px = fx; py = 0.5f + fy; break;
        default: break;
    }
    const float dy = py - halfHeight;
    const float dx = px - halfWidth;
    const float a = static_cast<float>(std::atan2(static_cast<double>(dy), static_cast<double>(dx)));
    const float s = 0.7853982f + (3.1415927f + a);
    return static_cast<float>(std::fmod(static_cast<double>(s), 6.2831854820251465));
}
