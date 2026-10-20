// vector2d.h: 2D vector in 16.16 fixed point (original: vector2d.h / vector2d.cpp).
// The inline members below were emitted out of line in vehicle.cpp in the original
// build; the two intersection helpers are vector2d.cpp's own code.
#pragma once
#include "fixmath.h"

struct Vector2d {
    Fixed x;
    Fixed y;

    Vector2d() : x(0), y(0) {}
    Vector2d(Fixed x_, Fixed y_) : x(x_), y(y_) {}

    Vector2d operator+(const Vector2d& o) const { return Vector2d(x + o.x, y + o.y); }
    Vector2d operator-(const Vector2d& o) const { return Vector2d(x - o.x, y - o.y); }
    Vector2d operator-() const { return Vector2d(-x, -y); }
    Vector2d operator*(Fixed s) const { return Vector2d(FixMul(x, s), FixMul(y, s)); }
    Vector2d operator/(Fixed s) const { return Vector2d(FixDiv(x, s), FixDiv(y, s)); }
};

// Unit x-axis vector initialised by the file's static initialiser (__sinit_vector2d_cpp).
extern Vector2d g_vector2d_xaxis;

// Intersection of a circle (radius r, centred at the origin) with a horizontal line
// through point p: returns the smaller root of the quadratic in 16.16, or INT32_MIN
// (0x80000000) when there is no intersection.
uint32_t xaxis_circle_intersect(Vector2d& p, Fixed r);

// Intersection of the line (p1 + t*d) with the circle (centre, r): returns the smaller
// root (t, as 16.16 halved) or INT32_MIN when the discriminant is not positive.
int32_t line_circle_intersect(Vector2d& p1, Vector2d& d, Vector2d& centre, Fixed r);
