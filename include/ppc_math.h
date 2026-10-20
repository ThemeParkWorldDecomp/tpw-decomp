// PowerPC idioms that plain C++ arithmetic does not reproduce: fused multiply-add and
// negation (below), double -> unsigned conversion, shifts by a register amount (at the end).
//
// CodeWarrior compiles a*b+c into a fused multiply-add (fmadds/fmsubs/fnmadds/fnmsubs),
// which rounds once; the build uses -ffp-contract=off so the host compiler never fuses on
// its own, and translations call these helpers exactly where the listing has a fused
// instruction. The decompiled C hides the fusion (it prints a*b+c), so read the listing.
//
// Operand order follows the instruction (frA * frC +- frB). The negated forms negate the
// ROUNDED result, so an exact zero comes out -0.0 (fnmsubs of 1*0 - 0 is -0.0). Host
// compilers fold "-fma(...)" into AArch64 FNMADD/FNMSUB or x86 VFNMADD, which negate the
// operands instead and give +0.0 there; ppc_fneg() hides the value from that folding.
// Use ppc_fneg() wherever the listing has an fneg of a fused or added result.
//
// NaN sign and payload are not reproduced bit for bit (the PowerPC leaves a NaN result's
// sign alone in fnmadd/fnmsub and propagates NaN operands in its own order); the
// differential tests compare NaNs as equal.
#pragma once
#include <cmath>
#include <cstdint>

// CodeWarrior's double -> unsigned long conversion (runtime helper __cvt_fp2unsigned,
// linked into each fragment without a name, e.g. engine 0x1004b9fc): below 0 -> 0,
// 2^32 and above or NaN -> 0xFFFFFFFF, otherwise truncation toward zero.
inline uint32_t cw_cvt_fp2unsigned(double x) {
    if (x < 0.0) return 0;
    if (!(x < 4294967296.0)) return 0xFFFFFFFFu;
    if (x < 2147483648.0) return static_cast<uint32_t>(static_cast<int32_t>(x));
    return static_cast<uint32_t>(static_cast<int32_t>(x - 2147483648.0)) + 0x80000000u;
}

// Negation that the optimiser cannot merge into the instruction that produced x.
inline float ppc_fneg(float x) {
    volatile float v = x;
    return -v;
}

inline double ppc_fneg(double x) {
    volatile double v = x;
    return -v;
}

// fmadds frD, frA, frC, frB: frA*frC + frB, one rounding to single.
inline float ppc_fmadds(float a, float c, float b) { return std::fmaf(a, c, b); }
// fmsubs: frA*frC - frB
inline float ppc_fmsubs(float a, float c, float b) { return std::fmaf(a, c, -b); }
// fnmadds: -(frA*frC + frB)
inline float ppc_fnmadds(float a, float c, float b) { return ppc_fneg(std::fmaf(a, c, b)); }
// fnmsubs: -(frA*frC - frB): the source's `x -= a*c`, with the sign of an exact zero
// result negative.
inline float ppc_fnmsubs(float a, float c, float b) { return ppc_fneg(std::fmaf(a, c, -b)); }

// Double-precision forms (fmadd, fmsub, ...).
inline double ppc_fmadd(double a, double c, double b) { return std::fma(a, c, b); }
inline double ppc_fmsub(double a, double c, double b) { return std::fma(a, c, -b); }
inline double ppc_fnmadd(double a, double c, double b) { return ppc_fneg(std::fma(a, c, b)); }
inline double ppc_fnmsub(double a, double c, double b) { return ppc_fneg(std::fma(a, c, -b)); }

// Shifts by a register amount (slw, srw, sraw) use the low 6 bits of the amount: 32..63
// shift everything out (sraw fills with the sign), so a negative or large amount gives 0
// where C++ would be undefined. Use these where a listing shifts by a computed amount.
inline uint32_t ppc_slw(uint32_t x, int32_t n) {
    uint32_t k = static_cast<uint32_t>(n) & 0x3f;
    return k >= 32 ? 0 : x << k;
}
inline uint32_t ppc_srw(uint32_t x, int32_t n) {
    uint32_t k = static_cast<uint32_t>(n) & 0x3f;
    return k >= 32 ? 0 : x >> k;
}
inline int32_t ppc_sraw(int32_t x, int32_t n) {
    uint32_t k = static_cast<uint32_t>(n) & 0x3f;
    return k >= 32 ? (x < 0 ? -1 : 0) : x >> k;
}

// fctiwz: double -> int32, truncating toward zero; NaN and values below INT32_MIN give
// INT32_MIN, values above INT32_MAX give INT32_MAX (a C++ cast would be undefined).
inline int32_t ppc_fctiwz(double x) {
    if (std::isnan(x)) return INT32_MIN;
    if (x >= 2147483648.0) return INT32_MAX;
    if (x <= -2147483649.0) return INT32_MIN;
    return static_cast<int32_t>(x);
}

// CodeWarrior's long long -> float conversion (runtime helper __cvt_sll_flt, linked into
// each fragment without a name, e.g. bullfrog 0x1006aedc): rounds to double first
// (nearest even), then to single (frsp), so a value above 2^53 can round differently
// from a direct conversion.
inline float cw_cvt_sll_flt(int64_t x) {
    return static_cast<float>(static_cast<double>(x));
}

// double -> long long (__cvt_dbl_sll, bullfrog 0x1006af8c): truncation toward zero;
// |x| >= 2^63, infinities and NaN saturate by the sign bit (a C++ cast is undefined).
inline int64_t cw_cvt_dbl_sll(double x) {
    if (std::isnan(x)) return std::signbit(x) ? INT64_MIN : INT64_MAX;
    if (x >= 9223372036854775808.0) return INT64_MAX;
    if (x <= -9223372036854775808.0) return INT64_MIN;
    return static_cast<int64_t>(x);
}
