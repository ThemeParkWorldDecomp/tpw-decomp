// vector2d.cpp (decompiled from SimTheme Park 2.04b3, functions at 0x10130b40..0x10130e8c).
// Arithmetic is 32-bit two's complement wrapping, as the original PowerPC code: the
// project builds with -fwrapv so signed overflow is defined and matches.
#include "vector2d.h"

Vector2d g_vector2d_xaxis(0x10000, 0);

uint32_t xaxis_circle_intersect(Vector2d& p, Fixed r) {
    int32_t x = p.x;
    int32_t y = p.y;
    // (x >> 18)^2 + (y >> 18)^2 : arithmetic shifts, plain 32-bit multiplies
    int32_t a = (x >> 18) * (x >> 18) + (y >> 18) * (y >> 18);
    int32_t c = a - FixMul(r, r);
    int32_t minus2x = x * -2;
    int32_t disc = FixMul(minus2x, minus2x) - c * 4;
    if (disc <= 0) return 0x80000000u;
    uint32_t s = isqrt(static_cast<uint32_t>(disc));
    return static_cast<uint32_t>(x * 2 - static_cast<int32_t>(s)) >> 1;
}

int32_t line_circle_intersect(Vector2d& p1, Vector2d& d, Vector2d& centre, Fixed r) {
    Vector2d f = p1 - centre;
    int32_t a = FixMul(d.y, d.y) + FixMul(d.x, d.x);
    int32_t b = (FixMul(d.y, f.y) + FixMul(d.x, f.x)) * 2;
    int32_t c = (FixMul(f.y, f.y) + FixMul(f.x, f.x)) - FixMul(r, r);
    int32_t disc = FixMul(b, b) - FixMul(a, c) * 4;
    if (disc <= 0) return INT32_MIN;
    int32_t s = static_cast<int32_t>(isqrt(static_cast<uint32_t>(disc)));
    int32_t root1 = s - b;
    int32_t root2 = -(b + s);
    return ((root2 > root1) ? root1 : root2) >> 1;
}
