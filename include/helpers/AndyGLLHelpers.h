// AndyGLLHelpers.h: file-local helpers that src/bullfrog/AndyGLL.cpp shares with Lbgfsave.cpp. Before the regroup, the
// functions using them shared a source file (2 files in all), which had one copy of each; that copy now lives here,
// static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include <cstdint>
#include <new>

static inline uint32_t Be16(const uint8_t* p) { return (uint32_t(p[0]) << 8) | p[1]; }

static inline uint32_t Be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }

static inline bool IffMagic(const uint8_t* h) {
    return Be32(h) == 0x4d524f46 && (Be32(h + 8) == 0x204d4250 || Be32(h + 8) == 0x4d424c49) && Be32(h + 12) == 0x44484d42;
}
