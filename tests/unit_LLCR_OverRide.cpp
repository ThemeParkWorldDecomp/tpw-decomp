// Test library for LLCR_OverRide (unit v5033; libtpw_unit_LLCR_OverRide.dylib, tests/test_LLCR_OverRide.py), main app:
//   src/LLCR_OverRide.cpp   LLCR_OverRide(uint16_t*, int32_t, int32_t)
//
// The callee at 0x1008905c is a fake that records the two ints it is given the addresses of and writes a queued halfword to the output.
// gMainBalance and gGameWorld are defined here (raw storage for the balance, whose constructor is not compiled).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LLCellSystem.h"
#include "MainBalance.h"
#include "World.h"

using namespace calllog;

char gMainBalanceStorage[sizeof(CMainBalance) + 16] __asm__("_gMainBalance");
CWorld* gGameWorld;

void LLCR_OverRide(uint16_t* out, int32_t* x, int32_t* y) {
    ev("Override", {*x, *y});
    *out = static_cast<uint16_t>(ret("Override"));
}

extern "C" {

uint64_t tpw_llcr_addr(const char* name) {
    if (std::strcmp(name, "balance") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gMainBalanceStorage));
    if (std::strcmp(name, "world") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gGameWorld));
    return 0;
}

uint64_t tpw_llcr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_llcr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CMainBalance, mValues));
    return n;
}

void tpw_llcr_a(uint64_t out, int32_t x, int32_t y) { LLCR_OverRide(reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(out)), x, y); }

}  // extern "C"
