// Test library for STATIC_WhiteOut (unit v5651; libtpw_unit_STATIC_WhiteOut.dylib, tests/test_STATIC_WhiteOut.py), main app.
// The GL calls and rand are fakes. The few state calls are logged; the 77k colours and vertices are folded into a hash and a count.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"

using namespace calllog;

namespace {
uint64_t gHash = 0;
uint64_t gCount = 0;
uint32_t gSeed = 0;

void Fold(uint64_t v) {
    gHash = (gHash * 1000003ull) ^ v;
    ++gCount;
}
uint64_t Bits(double d) {
    uint64_t b;
    std::memcpy(&b, &d, 8);
    return b;
}
}  // namespace

extern "C" {
void glPushAttrib(uint32_t mask) { ev("glPushAttrib", {mask}); }
void glPopAttrib() { ev("glPopAttrib", {}); }
void glDisable(uint32_t cap) { ev("glDisable", {cap}); }
void glBegin(uint32_t mode) { ev("glBegin", {mode}); }
void glEnd() { ev("glEnd", {}); }
void glColor4dv(const double* c) {
    for (int i = 0; i < 4; ++i) Fold(Bits(c[i]) ^ 0x1111);
}
void glVertex2d(double x, double y) {
    Fold(Bits(x) ^ 0x2222);
    Fold(Bits(y) ^ 0x3333);
}
}

int32_t Rand() {
    gSeed = (gSeed * 1103515245u + 12345u) & 0x7fffffffu;
    Fold(gSeed);
    return static_cast<int32_t>(gSeed);
}

extern "C" {

uint64_t tpw_ww_addr(const char* name) {
    if (std::strcmp(name, "colors") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gWhiteOutColors));
    return 0;
}

uint64_t tpw_ww_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ww_layout(int64_t* out) {
    out[0] = static_cast<int64_t>(sizeof(gWhiteOutColors));
    return 1;
}

void tpw_ww_run(uint32_t seed) {
    gSeed = seed;
    gHash = 0;
    gCount = 0;
    STATIC_WhiteOut();
}
uint64_t tpw_ww_hash() { return gHash; }
uint64_t tpw_ww_count() { return gCount; }

}  // extern "C"
