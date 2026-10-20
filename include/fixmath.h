// Fixed-point helpers used by the game code.
//
// FixMul and FixDiv are Mac OS Toolbox routines (InterfaceLib) on the Mac build and
// Bullfrog's own on the PC build. Their semantics are documented in Inside Macintosh
// ("Mathematical and Logical Utilities"): 16.16 fixed point, 64-bit intermediate,
// result rounded to nearest, overflow and division by zero saturate to 0x7FFFFFFF or
// 0x80000000 by sign. The differential tests use the same definition on both sides, so
// they do not verify these two functions themselves (see analysis/pilot-report.md).
#pragma once
#include "tpw_types.h"

Fixed FixMul(Fixed a, Fixed b);
Fixed FixDiv(Fixed a, Fixed b);

// Integer square root as the game computes it: (unsigned long)sqrt((double)x), with
// MSL's saturating double-to-unsigned conversion (irrelevant for x < 2^32).
uint32_t isqrt(uint32_t x);
