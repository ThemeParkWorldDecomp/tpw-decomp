// extern "C" wrappers for tests/test_vector2d.py (ctypes). Pointers come in as raw
// buffers laid out exactly like the PowerPC side (two int32 fields), native endian.
#include "vector2d.h"
#include <new>

static int g_deleted = 0;

extern "C" {
uint32_t tpw_xaxis_circle_intersect(Vector2d* p, int32_t r) { return xaxis_circle_intersect(*p, r); }
int32_t tpw_line_circle_intersect(Vector2d* p1, Vector2d* d, Vector2d* c, int32_t r) { return line_circle_intersect(*p1, *d, *c, r); }
void tpw_v2_sub(Vector2d* out, const Vector2d* a, const Vector2d* b) { *out = *a - *b; }
void tpw_v2_add(Vector2d* out, const Vector2d* a, const Vector2d* b) { *out = *a + *b; }
void tpw_v2_neg(Vector2d* out, const Vector2d* a) { *out = -*a; }
void tpw_v2_mul(Vector2d* out, const Vector2d* a, int32_t s) { *out = *a * s; }
void tpw_v2_div(Vector2d* out, const Vector2d* a, int32_t s) { *out = *a / s; }
void tpw_v2_ctor0(Vector2d* out) { new (out) Vector2d(); }
void tpw_v2_ctor2(Vector2d* out, int32_t x, int32_t y) { new (out) Vector2d(x, y); }
int32_t tpw_fixmul(int32_t a, int32_t b) { return FixMul(a, b); }
int32_t tpw_fixdiv(int32_t a, int32_t b) { return FixDiv(a, b); }
uint32_t tpw_isqrt(uint32_t x) { return isqrt(x); }
void tpw_xaxis_global(Vector2d* out) { *out = g_vector2d_xaxis; }
}

// CodeWarrior destructor ABI: ~T(this, short flag) deletes the storage when flag > 0.
// The C++ equivalent is the implicit destructor plus `delete`; this wrapper only
// mirrors the convention so the harness can compare the "deleted" side effect.
extern "C" int32_t tpw_v2_dtor(Vector2d* p, int16_t flag) {
    g_deleted = (p != nullptr && flag > 0) ? 1 : 0;
    return g_deleted;
}
