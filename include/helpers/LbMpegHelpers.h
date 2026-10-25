// LbMpegHelpers.h: file-local helpers that src/sound/LbMpeg.cpp shares with FileAccess.cpp and MpegDecode.cpp. Before
// the regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now lives
// here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include <cstdint>
#include <new>

// divw: the quotient truncated towards zero; the dividend where the processor leaves it undefined (a zero divisor, the
// most negative number over -1), as the emulator answers.
static inline int32_t Div(int32_t a, int32_t b) {
    if (b == 0 || (a == INT32_MIN && b == -1)) return a;
    return a / b;
}

// mullw: the low 32 bits of the product.
static inline int32_t Mul(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }

static inline int32_t Add(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
