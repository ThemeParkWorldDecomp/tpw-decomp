#include "fixmath.h"

#include <cmath>
#include <cstdlib>

namespace {
Fixed saturate(int64_t v) {
    if (v > INT32_MAX) return INT32_MAX;
    if (v < INT32_MIN) return INT32_MIN;
    return static_cast<Fixed>(v);
}
}  // namespace

Fixed FixMul(Fixed a, Fixed b) {
    int64_t p = static_cast<int64_t>(a) * static_cast<int64_t>(b);
    // round to nearest (half away from zero), then drop 16 fraction bits
    p += (p >= 0) ? 0x8000 : -0x8000;
    return saturate(p >> 16);
}

Fixed FixDiv(Fixed a, Fixed b) {
    if (b == 0) return (a >= 0) ? INT32_MAX : INT32_MIN;
    int64_t n = static_cast<int64_t>(a) << 16;
    int64_t q = n / b;
    int64_t r = n % b;
    // round to nearest, half away from zero
    if (std::llabs(r) * 2 >= std::llabs(static_cast<int64_t>(b))) q += ((n < 0) != (b < 0)) ? -1 : 1;
    return saturate(q);
}

uint32_t isqrt(uint32_t x) {
    double s = std::sqrt(static_cast<double>(x));
    if (s <= 0.0) return 0;
    if (s >= 4294967296.0) return 0xFFFFFFFFu;
    return static_cast<uint32_t>(s);   // truncation, as fctiwz
}
