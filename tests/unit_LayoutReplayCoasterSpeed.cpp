// Test library for LayoutReplayCoasterSpeed (unit v5302; libtpw_unit_LayoutReplayCoasterSpeed.dylib), main app:
//   src/game/rides/CoasterEdit.cpp
//
// CoasterSetSpeed(uint32_t, uint32_t) is a recording fake; gObjectTable is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "rsse.h"

using namespace calllog;

void LayoutReplayCoasterSpeed(uint32_t instance, uint32_t speed);

void* gObjectTable[0x100];

void CoasterSetSpeed(uint32_t instance, uint32_t speed) {
    ev("CoasterSetSpeed", {static_cast<int64_t>(instance), static_cast<int64_t>(speed)});
}

extern "C" {

uint64_t tpw_lcs_addr(const char* name) {
    if (std::strcmp(name, "table") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gObjectTable));
    return 0;
}

uint64_t tpw_lcs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lcs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gObjectTable));
    return n;
}

void tpw_lcs_run(uint32_t instance, uint32_t speed) { LayoutReplayCoasterSpeed(instance, speed); }

}  // extern "C"
